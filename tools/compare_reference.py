"""Compare diagnostic CSV with labeled timeSeconds,frequency,voiced reference CSV.

Standard library only. Nearest reference timestamp within --tolerance (seconds).
No automatic latency alignment: errors include causal tracking response.
"""
import argparse
import bisect
import csv
import json
import math
from pathlib import Path


def percentile(values, fraction):
    if not values:
        return None
    return sorted(values)[math.ceil(fraction * (len(values) - 1))]


def compare(analysis, reference, tolerance):
    with Path(reference).open(newline="", encoding="utf-8-sig") as stream:
        refs = list(csv.DictReader(stream))
    times = [float(row["timeSeconds"]) for row in refs]
    if not times or any(not math.isfinite(t) for t in times) or any(a >= b for a, b in zip(times, times[1:])):
        raise ValueError("Reference timestamps must be finite and strictly increasing")
    for row in refs:
        hz = float(row["frequency"])
        if row["voiced"] not in ("0", "1") or not math.isfinite(hz) or hz < 0 or (row["voiced"] == "1" and hz == 0):
            raise ValueError("Reference requires voiced=0/1 and a finite, positive frequency when voiced")
    errors = []
    tp = fp = tn = fn = skipped = voiced_without_pitch = 0
    with Path(analysis).open(newline="", encoding="utf-8-sig") as stream:
        for row in csv.DictReader(stream):
            t = float(row["timeSeconds"])
            if not math.isfinite(t):
                raise ValueError("Analysis timestamps must be finite")
            index = bisect.bisect_left(times, t)
            choices = [i for i in (index - 1, index) if 0 <= i < len(times)]
            index = min(choices, key=lambda i: abs(times[i] - t))
            if abs(times[index] - t) > tolerance:
                skipped += 1
                continue
            ref = refs[index]
            actual = bool(int(ref["voiced"]))
            detected = bool(int(row["voiced"]))
            tp += actual and detected
            fp += not actual and detected
            tn += not actual and not detected
            fn += actual and not detected
            hz = float(row["trackedFrequency"])
            if not math.isfinite(hz) or hz < 0 or row["voiced"] not in ("0", "1") or row["trackedValid"] not in ("0", "1"):
                raise ValueError("Analysis flags or tracked frequency are invalid")
            ref_hz = float(ref["frequency"])
            trustworthy = int(row["trackedValid"]) and row["trackingState"] == "tracking"
            if actual and ref_hz <= 0:
                raise ValueError("Voiced reference frequency must be positive")
            if actual and trustworthy and hz > 0:
                errors.append(abs(1200 * math.log2(hz / ref_hz)))
            elif actual:
                voiced_without_pitch += 1
    return dict(matched_frames=tp + fp + tn + fn, skipped_frames=skipped,
                pitch_frames=len(errors), voiced_without_tracked_pitch=voiced_without_pitch,
                median_absolute_cents=percentile(errors, .5), p95_absolute_cents=percentile(errors, .95),
                max_absolute_cents=percentile(errors, 1), octave_errors=sum(abs(e - 1200) < 100 for e in errors),
                gross_errors_over_600_cents=sum(e > 600 for e in errors),
                true_positive=tp, false_positive=fp, true_negative=tn, false_negative=fn,
                voiced_precision=tp / (tp + fp) if tp + fp else None,
                voiced_recall=tp / (tp + fn) if tp + fn else None)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis")
    parser.add_argument("reference")
    parser.add_argument("--tolerance", type=float, default=.006)
    args = parser.parse_args()
    if not math.isfinite(args.tolerance) or args.tolerance < 0:
        parser.error("tolerance must be finite and non-negative")
    print(json.dumps(compare(args.analysis, args.reference, args.tolerance), indent=2, allow_nan=False))
