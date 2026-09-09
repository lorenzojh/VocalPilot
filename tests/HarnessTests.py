"""End-to-end WAV/CSV tool tests; run with Python 3 and the built analyser path."""
import csv
import importlib.util
import math
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path


def main():
    exe = Path(sys.argv[1]).resolve()
    spec = importlib.util.spec_from_file_location("compare", Path(__file__).resolve().parents[1] / "tools/compare_reference.py")
    comparison = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(comparison)
    with tempfile.TemporaryDirectory(prefix="vocalpilot-harness-") as temporary:
        folder = Path(temporary)
        source = folder / "input.wav"
        output = folder / "analysis.csv"
        for rate in (44100, 48000, 96000):
            with wave.open(str(source), "wb") as wav:
                wav.setparams((2, 2, rate, 0, "NONE", "not compressed"))
                # Anti-phase stereo verifies that analysis uses left, not L+R.
                samples = [round(6000 * math.sin(2 * math.pi * 432 * i / rate)) for i in range(rate // 2)]
                wav.writeframes(b"".join(struct.pack("<hh", x, -x) for x in samples))
            for strength in (0, 50, 100):
                subprocess.run([str(exe), str(source), str(output), "0", "major", str(strength)], check=True, capture_output=True)
                with output.open(newline="") as stream:
                    rows = list(csv.DictReader(stream))
                assert len(rows) >= 40
                times = [float(r["timeSeconds"]) for r in rows]
                assert all(a < b for a, b in zip(times, times[1:]))
                # The last row is the final completed hop, not an invented end-of-file frame.
                assert abs(times[0] - .05) < .001 and 0 <= .5 - times[-1] < .011
                assert all(.009 < b - a < .011 for a, b in zip(times, times[1:]))
                stable = [r for r in rows if float(r["timeSeconds"]) > .15]
                for row in stable:
                    assert row["voiced"] == "1" and row["valid"] == "1"
                    assert abs(float(row["rawFrequency"]) - 432) < 2
                    assert abs(float(row["trackedFrequency"]) - 432) < 2
                    assert int(row["targetMidi"]) == 69 and row["targetNote"] == "A4"
                    assert .9 < float(row["confidence"]) <= 1
                    expected = 1200 * math.log2(440 / 432) * strength / 100
                    assert abs(float(row["requestedCorrectionCents"]) - expected) < 1
                    assert all(math.isfinite(float(row[field])) for field in ("inputRms", "smoothedCorrectionCents", "reliability", "shiftMix"))
                reference = folder / "reference.csv"
                with reference.open("w", newline="") as stream:
                    writer = csv.writer(stream)
                    writer.writerow(("timeSeconds", "frequency", "voiced"))
                    writer.writerows((row["timeSeconds"], 432, 1) for row in stable)
                metrics = comparison.compare(output, reference, .001)
                assert metrics["pitch_frames"] == len(stable)
                assert metrics["p95_absolute_cents"] < 5
                assert metrics["voiced_recall"] == 1 and metrics["octave_errors"] == 0
                reference.write_text("timeSeconds,frequency,voiced\n0.2,nan,1\n")
                try:
                    comparison.compare(output, reference, .001)
                except ValueError:
                    pass
                else:
                    raise AssertionError("Non-finite reference label was accepted")
        before = source.read_bytes()
        assert subprocess.run([str(exe), str(source), str(source)], capture_output=True).returncode != 0
        assert source.read_bytes() == before
        for bad in (["12"], ["2", "dorian"], ["2", "major", "nan"]):
            assert subprocess.run([str(exe), str(source), str(output), *bad], capture_output=True).returncode != 0
        assert subprocess.run([str(exe), str(folder / "absent.wav"), str(output)], capture_output=True).returncode != 0
    print("Harness tests passed: 3 sample rates, stereo selection, strengths, CSV semantics, reference scoring, invalid arguments, input preservation")


if __name__ == "__main__":
    main()
