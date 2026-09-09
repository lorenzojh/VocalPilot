"""Make anonymous, RMS-matched listening packs. Reveal only after scoring.

Uses standard-library PCM WAV I/O, no plugin dependency. Identity map lives
outside the listening directory; this is a blinded rating test, not proof of
ABX discrimination or a perceptual loudness normalization standard.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import random
import struct
import wave

RATINGS = ("naturalness", "timbre_preservation", "absence_of_metallic_character",
           "absence_of_warble", "absence_of_graininess", "consonant_clarity",
           "transient_clarity", "note_transition_quality", "pitch_effectiveness")


def read(path):
    with wave.open(str(path), "rb") as wav:
        if wav.getsampwidth() != 3:
            raise ValueError("Use 24-bit PCM from vocalpilot_render")
        raw = wav.readframes(wav.getnframes())
        values = [int.from_bytes(raw[i:i+3], "little", signed=True) / 8388608 for i in range(0,len(raw),3)]
        return wav.getframerate(), wav.getnchannels(), values


def write(path, rate, channels, values, gain):
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(channels); wav.setsampwidth(3); wav.setframerate(rate)
        wav.writeframes(b"".join(int(round(x*gain*8388607)).to_bytes(3,"little",signed=True) for x in values))


def prepare(args):
    destination = Path(args.output)
    if destination.exists():
        raise ValueError("Use a new listening directory")
    sources = [Path(args.renders)/f"{name}.wav" for name in ("dry","legacy","synchronous","spectral")]
    decoded = [read(path) for path in sources]
    if len({(rate,channels,len(values)) for rate,channels,values in decoded}) != 1:
        raise ValueError("Renders must have identical rate, channels and length")
    rms = [(sum(x*x for x in values)/len(values))**.5 for _,_,values in decoded]
    if min(rms) < 1e-9:
        raise ValueError("Silent render cannot be loudness matched")
    target = min(.08, min(rms))
    gains = [target/r for r in rms]
    peak = max(max(abs(x) for x in item[2])*gain for item,gain in zip(decoded,gains))
    if peak > .89:
        gains = [g*.89/peak for g in gains]
    randomizer = random.Random(args.seed) if args.seed is not None else random.SystemRandom()
    order = list(range(4)); randomizer.shuffle(order)
    destination.mkdir(parents=True)
    mapping = {}
    for label,index in zip("ABCD",order):
        rate,channels,values = decoded[index]
        write(destination/f"{label}.wav",rate,channels,values,gains[index])
        mapping[label] = {"engine":sources[index].stem,"gain":gains[index],"source_sha256":hashlib.sha256(sources[index].read_bytes()).hexdigest()}
    # A labelled dry reference is useful for judging identity, and is not scored.
    rate,channels,values = decoded[0]
    write(destination/"reference-dry.wav",rate,channels,values,gains[0])
    with (destination/"scores.csv").open("w",newline="") as out:
        writer=csv.writer(out); writer.writerow(["listener","label",*RATINGS,"notes"])
        for label in "ABCD": writer.writerow(["",label,*([""]*len(RATINGS)),""])
    (destination/"INSTRUCTIONS.txt").write_text("Compare A/B/C/D in random order against reference-dry.wav. Rate each column 1 (worst) to 5 (best). Enter N/A for absent events. All absence-of-artifact columns: 5 = no artifact. Judge identity, vowels, attacks, consonants, transitions and correction separately. Complete every score before revealing. RMS matching is approximate perceived loudness matching, not LUFS. Dry is intentionally included among anonymous candidates; successful pitch correction can differ from the reference. Do not inspect the reveal file before scoring.\n")
    reveal = destination.parent / (destination.name+"-reveal.json")
    if reveal.exists(): raise ValueError("Reveal file exists")
    reveal.write_text(json.dumps(mapping,indent=2))
    print(f"Listening pack: {destination}. Identity map stored separately; use reveal after scoring.")


def reveal(args):
    scores = Path(args.output)/"scores.csv"
    with scores.open(newline="") as source: rows=list(csv.DictReader(source))
    if len(rows)!=4 or {r["label"] for r in rows}!=set("ABCD"):
        raise ValueError("Expected four scored labels")
    for row in rows:
        if not row["listener"].strip() or any(row[field] not in ("1","2","3","4","5","N/A") for field in RATINGS):
            raise ValueError("Complete listener and all ratings (1..5 or N/A) before revealing")
    mapping=json.loads(Path(args.reveal_file).read_text())
    print(json.dumps(mapping,indent=2))


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest="command",required=True)
    build=sub.add_parser("prepare"); build.add_argument("renders"); build.add_argument("output"); build.add_argument("--seed",type=int)
    show=sub.add_parser("reveal"); show.add_argument("output"); show.add_argument("reveal_file")
    args=parser.parse_args()
    (prepare if args.command=="prepare" else reveal)(args)
