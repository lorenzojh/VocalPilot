"""Independent NumPy measurements of actual C++ WAV renders, not the detector.

Synthetic source/filter fixtures use known poles and harmonic excitation. The
formant estimator fits those pole frequencies to measured harmonic amplitudes;
bandwidth and excitation slope are known fixture priors, NOT a real-voice model.
No desired result is used as an assertion except safety/format invariants.
"""
import argparse
import csv
import json
from pathlib import Path
import subprocess
import tempfile
import wave
import numpy as np

VOWELS = {"ah": (730, 1090, 2440), "ee": (270, 2290, 3010), "oo": (300, 870, 2240)}
BANDWIDTHS = np.array([90., 110., 160.])
ENGINES = ("legacy", "synchronous", "spectral")


def write_wav(path, rate, audio):
    audio = np.asarray(audio)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1 if audio.ndim == 1 else audio.shape[1])
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(np.round(np.clip(audio, -.999, .999) * 32767).astype("<i2").tobytes())


def read_wav(path):
    with wave.open(str(path), "rb") as source:
        raw = source.readframes(source.getnframes())
        if source.getsampwidth() == 3:
            b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
            data = b[:, 0] | b[:, 1] << 8 | b[:, 2] << 16
            data = np.where(data & 0x800000, data - 0x1000000, data) / 8388608.
        elif source.getsampwidth() == 2:
            data = np.frombuffer(raw, "<i2") / 32768.
        else:
            raise ValueError("Expected PCM16/24 WAV")
        return source.getframerate(), data.reshape(-1, source.getnchannels())


def response(frequencies, formants, rate):
    z = np.exp(-2j * np.pi * np.asarray(frequencies) / rate)
    h = np.ones(z.shape, dtype=complex)
    for frequency, bandwidth in zip(formants, BANDWIDTHS):
        radius = np.exp(-np.pi * bandwidth / rate)
        h /= 1 - 2 * radius * np.cos(2 * np.pi * frequency / rate) * z + radius**2 * z**2
    return h


def vowel(rate, f0, formants, seconds=1.0):
    t = np.arange(round(rate * seconds)) / rate
    harmonics = np.arange(1, int(.47 * rate / f0) + 1)
    h = response(harmonics * f0, formants, rate) / np.sqrt(harmonics)
    y = np.zeros(len(t))
    for k, coefficient in zip(harmonics, h):
        y += np.real(coefficient * np.exp(2j * np.pi * k * f0 * t))
    y *= .16 / np.max(np.abs(y))
    fade = np.minimum(1., t / .02) * np.minimum(1., (seconds - t) / .02)
    return y * fade


def spectrum(y, rate):
    y = y[round(rate * .25):round(rate * .85)]
    nfft = 1 << (len(y) * 4 - 1).bit_length()
    s = np.abs(np.fft.rfft(y * np.hanning(len(y)), nfft))
    return np.fft.rfftfreq(nfft, 1 / rate), s


def fit_formants(f, amplitudes, rate, known):
    # Fits three independently variable poles (no fixed original-formant
    # output). Multi-start includes shifted seeds; coordinate search +/-50%.
    log_amp = 20 * np.log10(np.maximum(amplitudes * np.sqrt(f / f[0]), 1e-12))
    best = None
    for factor in (.7, 1., 1.4):
        poles = np.array(known, float) * factor
        def loss(candidate):
            predicted = 20 * np.log10(np.maximum(np.abs(response(f, candidate, rate)), 1e-20))
            residual = log_amp - predicted
            return float(np.mean((residual - np.mean(residual))**2))
        for step in (80., 30., 10., 3.):
            for _ in range(2):
                for j in range(3):
                    trials = []
                    for delta in np.arange(-3, 4) * step:
                        candidate = poles.copy()
                        candidate[j] = np.clip(poles[j] + delta, known[j] * .45, known[j] * 1.8)
                        if np.all(np.diff(candidate) > 50):
                            trials.append((loss(candidate), candidate))
                    if trials:
                        poles = min(trials, key=lambda v: v[0])[1]
        score = loss(poles)
        if best is None or score < best[0]:
            best = (score, poles)
    return best[1], best[0]**.5


def metrics(y, dry, rate, f0, cents, formants):
    target = f0 * 2**(cents / 1200)
    f, mag = spectrum(y, rate)
    # Narrow fundamental neighborhood deliberately reports gross failure if
    # no strong target-near component exists; harmonic mask is independent.
    vicinity = np.flatnonzero((f > target * .8) & (f < target * 1.2))
    p = vicinity[np.argmax(mag[vicinity])]
    logs = np.log(np.maximum(mag[p-1:p+2], 1e-20))
    fraction = .5 * (logs[0] - logs[2]) / (logs[0] - 2 * logs[1] + logs[2])
    estimated = (p + fraction) * (f[1] - f[0])
    harmonic = np.abs(f / target - np.round(f / target)) * target < 8
    total = np.sum(mag**2) + 1e-30
    nonharmonic = 10 * np.log10(max(1e-15, np.sum(mag[~harmonic]**2) / total))
    hf = np.arange(1, int(min(5000, rate * .42) / target) + 1) * target
    amplitudes = []
    for frequency in hf:
        region = (f >= frequency - 8) & (f <= frequency + 8)
        amplitudes.append(np.max(mag[region]))
    amplitudes = np.asarray(amplitudes)
    poles, fit_error = fit_formants(hf, amplitudes, rate, formants)
    expected = np.abs(response(hf, formants, rate)) / np.sqrt(hf / target)
    residual = 20 * np.log10(np.maximum(amplitudes, 1e-10) / np.maximum(expected, 1e-10))
    envelope_db = float(np.sqrt(np.mean((residual - np.mean(residual))**2)))
    segment = y[round(.25 * rate):round(.85 * rate)]
    rms = np.sqrt(np.mean(segment**2))
    dry_rms = np.sqrt(np.mean(dry[round(.25*rate):round(.85*rate)]**2))
    hop = max(1, round(.020 * rate))
    energy = [np.sqrt(np.mean(segment[i:i+hop]**2)) for i in range(0, len(segment)-hop, hop)]
    row = dict(pitch_error_cents=float(1200*np.log2(estimated/target)), inharmonic_db=float(nonharmonic),
               envelope_rmse_db=envelope_db, formant_fit_residual_db=fit_error,
               rms_gain_db=float(20*np.log10(max(rms,1e-15)/dry_rms)),
               rms_modulation_db=float(20*np.log10(max(energy)/max(min(energy),1e-15))),
               peak=float(np.max(np.abs(y))), dc=float(np.mean(segment)), max_step=float(np.max(np.abs(np.diff(y)))))
    for j in range(3):
        row[f"F{j+1}_hz"] = float(poles[j])
        row[f"F{j+1}_error_hz"] = float(poles[j]-formants[j])
        row[f"F{j+1}_error_percent"] = float(100*(poles[j]/formants[j]-1))
    return row


def run(args):
    destination = Path(args.output)
    destination.mkdir(parents=True, exist_ok=True)
    rows = []
    shifts = [0] + [sign * value for value in (.1,1,5,10,25,50,100,200,300,500,700,1200) for sign in (-1,1)]
    cases = [(48000, f0, name, cents) for f0 in (90,130,220,440) for name in VOWELS for cents in shifts]
    cases += [(rate,f0,name,cents) for rate in (44100,96000) for f0 in (90,220,660) for name in VOWELS for cents in (-700,-100,25,700)]
    if args.quick:
        cases = [(48000,130,name,cents) for name in VOWELS for cents in (-200,25,200)]
    with tempfile.TemporaryDirectory(prefix="vocalpilot-quality-") as temp:
        temp = Path(temp)
        for index, (rate,f0,name,cents) in enumerate(cases):
            input_file = temp / "source.wav"
            write_wav(input_file, rate, vowel(rate, f0, VOWELS[name]))
            output = temp / f"case-{index}"
            subprocess.run([args.renderer, str(input_file), str(output), "--f0",str(f0),"--cents",str(cents)] + (["--no-refine"] if args.no_refine else []), check=True, capture_output=True)
            _, dry = read_wav(output / "dry.wav")
            for engine in ENGINES:
                _, audio = read_wav(output / f"{engine}.wav")
                assert audio.shape == dry.shape and np.isfinite(audio).all()
                if cents == 0:
                    assert np.array_equal(audio, dry), "PCM unity identity failed"
                row = dict(rate=rate,f0=f0,vowel=name,cents=cents,engine=engine)
                row.update(metrics(audio[:,0], dry[:,0],rate,f0,cents,VOWELS[name]))
                rows.append(row)
            # Temp results are reproducible and huge; retain only measurements.
            import shutil
            shutil.rmtree(output)
            if index % 12 == 0:
                print(f"Measured {index+1}/{len(cases)} fixtures", flush=True)
    with (destination / "transformation-quality.csv").open("w",newline="") as out:
        writer = csv.DictWriter(out,fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
    summary = {}
    for engine in ENGINES:
        selected = [r for r in rows if r["engine"] == engine and 10 <= abs(r["cents"]) <= 200]
        summary[engine] = {key: float(np.median([abs(r[key]) for r in selected])) for key in
                           ("pitch_error_cents","envelope_rmse_db","F1_error_percent","F2_error_percent","F3_error_percent","rms_modulation_db")}
    (destination / "transformation-quality-summary.json").write_text(json.dumps(summary,indent=2))
    print(json.dumps(summary,indent=2))
    if args.verify:
        for engine in ("synchronous", "spectral"):
            assert summary[engine]["pitch_error_cents"] < 5, "Candidate pitch regression"
            assert summary[engine]["envelope_rmse_db"] < .7 * summary["legacy"]["envelope_rmse_db"], "Candidate envelope regression"
        assert summary["synchronous"]["F2_error_percent"] < 3, "Pitch-synchronous F2 regression"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("renderer")
    parser.add_argument("output")
    parser.add_argument("--quick",action="store_true")
    parser.add_argument("--verify",action="store_true")
    parser.add_argument("--no-refine",action="store_true",help="Ablate waveform epoch refinement")
    run(parser.parse_args())
