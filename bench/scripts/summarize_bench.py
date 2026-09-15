#!/usr/bin/env python3
"""Summarize Google Benchmark JSON files across repeated runs.

Usage:
    summarize_bench.py run1.json [run2.json ...]

For each benchmark case it takes the per-file median aggregate (or the
median of raw iterations when run without repetitions) and reports the
median of those across files, plus median CPU time and numeric counters.
Entries with "error_occurred": true are reported and skipped.
"""
import json
import sys
from statistics import median


def base_name(entry):
    # Aggregates are named "<case>_mean|_median|_stddev"; run_name is the case.
    if entry.get("run_name"):
        return entry["run_name"]
    name = entry.get("name", "")
    for suffix in ("_mean", "_median", "_stddev", "_cv"):
        if name.endswith(suffix):
            return name[: -len(suffix)]
    return name


def fmt_time(ns):
    for unit, scale in (("s", 1e9), ("ms", 1e6), ("us", 1e3), ("ns", 1)):
        if ns >= scale or unit == "ns":
            return f"{ns / scale:,.1f} {unit}"
    return f"{ns} ns"


# Fixed per-run fields in google benchmark JSON; anything else numeric at
# the top level of a run object is a user counter (the reporter flattens
# run.counters into the object — there is no nested "counters" dict).
KNOWN_FIELDS = {
    "name", "family_index", "per_family_instance_index", "run_name",
    "run_type", "repetitions", "repetition_index", "threads",
    "aggregate_name", "aggregate_unit", "iterations", "real_time",
    "cpu_time", "time_unit", "error_occurred", "error_message",
    "skipped", "skip_message", "cpu_coefficient", "real_coefficient",
    "big_o", "rms", "allocs_per_iter", "max_bytes_used",
    "total_allocated_bytes", "net_heap_growth",
}


def extract_counters(entry):
    return {
        k: float(v)
        for k, v in entry.items()
        if k not in KNOWN_FIELDS and isinstance(v, (int, float))
    }


def main(paths):
    # per file -> {case: (real_time, cpu_time, counters)}
    per_file = []
    errors = []
    for path in paths:
        with open(path) as f:
            data = json.load(f)
        cases = {}
        raw = {}  # case -> [(real, cpu)] for non-aggregate runs
        for e in data.get("benchmarks", []):
            if e.get("error_occurred"):
                errors.append((path, base_name(e)))
                continue
            name = base_name(e)
            if e.get("run_type") == "aggregate":
                if e.get("aggregate_name") != "median":
                    continue
                cases[name] = (
                    float(e["real_time"]),
                    float(e["cpu_time"]),
                    extract_counters(e),
                )
            elif e.get("run_type") == "iteration":
                raw.setdefault(name, []).append(
                    (float(e["real_time"]), float(e["cpu_time"]),
                     extract_counters(e))
                )
        for name, samples in raw.items():
            if name not in cases and samples:
                keys = {k for s in samples for k in s[2]}
                cases[name] = (
                    median(s[0] for s in samples),
                    median(s[1] for s in samples),
                    {k: median(s[2][k] for s in samples if k in s[2])
                     for k in keys},
                )
        per_file.append(cases)

    names = sorted({n for cases in per_file for n in cases})
    if not names:
        print("no benchmark data found")
        return 1

    rows = []
    for name in names:
        reals = [c[name][0] for c in per_file if name in c]
        cpus = [c[name][1] for c in per_file if name in c]
        counter_vals = {}
        for c in per_file:
            if name in c:
                for k, v in c[name][2].items():
                    if isinstance(v, (int, float)):
                        counter_vals.setdefault(k, []).append(float(v))
        counters = " ".join(
            f"{k}={median(v):,.3g}" for k, v in sorted(counter_vals.items())
        )
        rows.append((name, len(reals), median(reals), median(cpus), counters))

    width = max(len(r[0]) for r in rows)
    print(f"{'benchmark':<{width}}  {'runs':>4}  {'real p50':>12}  {'cpu p50':>12}  counters")
    for name, n, real, cpu, counters in rows:
        print(f"{name:<{width}}  {n:>4}  {fmt_time(real):>12}  {fmt_time(cpu):>12}  {counters}")
    if errors:
        print("\nskipped entries with errors:")
        for path, name in errors:
            print(f"  {path}: {name}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1:]))
