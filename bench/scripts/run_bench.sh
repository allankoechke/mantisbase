#!/usr/bin/env bash
# Repeated benchmark runs with JSON output.
#
# Runs mantisbase_bench N times (fresh server per run) and writes one
# timestamped JSON file per run, then summarizes medians across runs.
#
# Usage:
#   bench/scripts/run_bench.sh [options]
#
# Options:
#   --runs N          Full binary runs (default: 3)
#   --repetitions N   Google Benchmark repetitions per case (default: 5)
#   --filter REGEX    --benchmark_filter passthrough (default: all)
#   --min-time SEC    --benchmark_min_time passthrough (default: unset)
#   --tag NAME        Filename tag (default: bench)
#   --out-dir DIR     Output dir, created if missing (default: <repo>/bench-results)
#   --build-dir DIR   CMake build dir holding bin/mantisbase_bench
#                     (default: first existing of cmake-build-relwithdebinfo,
#                     cmake-build-debug, build)
#   --build           Build mantisbase_bench first (cmake --build --target)
#   -h, --help        This message
#
# Example:
#   bench/scripts/run_bench.sh --runs 5 --repetitions 5 --build
#   bench/scripts/run_bench.sh --runs 2 --repetitions 1 --filter 'BM_SSE.*' --tag sse
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

RUNS=3
REPETITIONS=5
FILTER=""
MIN_TIME=""
TAG="bench"
OUT_DIR="${REPO_ROOT}/bench-results"
BUILD_DIR=""
DO_BUILD=0

usage() {
    sed -n '2,/^#$/p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --runs) RUNS="$2"; shift 2 ;;
        --repetitions) REPETITIONS="$2"; shift 2 ;;
        --filter) FILTER="$2"; shift 2 ;;
        --min-time) MIN_TIME="$2"; shift 2 ;;
        --tag) TAG="$2"; shift 2 ;;
        --out-dir) OUT_DIR="$2"; shift 2 ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --build) DO_BUILD=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ -z "${BUILD_DIR}" ]]; then
    for cand in "${REPO_ROOT}/cmake-build-relwithdebinfo" \
                "${REPO_ROOT}/cmake-build-release" \
                "${REPO_ROOT}/cmake-build-debug" \
                "${REPO_ROOT}/build"; do
        if [[ -x "${cand}/bin/mantisbase_bench" ]]; then
            BUILD_DIR="${cand}"
            break
        fi
    done
fi

if [[ "${DO_BUILD}" == "1" ]]; then
    if [[ -z "${BUILD_DIR}" ]]; then
        echo "error: --build needs --build-dir (no configured build dir found)" >&2
        exit 2
    fi
    cmake --build "${BUILD_DIR}" --target mantisbase_bench -j"$(nproc)"
fi

BIN="${BUILD_DIR:-<unset>}/bin/mantisbase_bench"
if [[ ! -x "${BIN}" ]]; then
    echo "error: mantisbase_bench not found at ${BIN}" >&2
    echo "hint: configure with -DMB_BUILD_BENCHMARKS=ON then rerun with --build" >&2
    exit 2
fi

mkdir -p "${OUT_DIR}"

# --- file descriptor guard -------------------------------------------------
# The bench runs an in-process drogon server plus up to ~100 concurrent
# client loops; each holds several fds, so the 1024 default dies fast with
# EMFILE (errno=24, trantor LOG_FATAL). Raise the soft limit when we can.
MIN_FD=16384
fd_soft="$(ulimit -Sn)"
if [[ "${fd_soft}" != "unlimited" && "${fd_soft}" -lt "${MIN_FD}" ]]; then
    fd_hard="$(ulimit -Hn)"
    if ! ulimit -Sn "${MIN_FD}" 2>/dev/null; then
        ulimit -Sn "${fd_hard}" 2>/dev/null || true
    fi
    echo "fd limit: ${fd_soft} -> $(ulimit -Sn) (hard ${fd_hard})"
    if [[ "$(ulimit -Sn)" != "unlimited" && "$(ulimit -Sn)" -lt "${MIN_FD}" ]]; then
        echo "warning: fd limit below ${MIN_FD}; high-concurrency cases may hit EMFILE (errno=24)" >&2
    fi
fi

TS="$(date +%Y%m%d_%H%M%S)"

ARGS=(--benchmark_repetitions="${REPETITIONS}"
      --benchmark_counters_tabular=true)
# Aggregates of a single repetition are meaningless (and aggregate-only
# output can come back empty), so only request them for repetitions > 1.
if [[ "${REPETITIONS}" -gt 1 ]]; then
    ARGS+=(--benchmark_report_aggregates_only=true)
fi
[[ -n "${FILTER}" ]] && ARGS+=(--benchmark_filter="${FILTER}")
[[ -n "${MIN_TIME}" ]] && ARGS+=(--benchmark_min_time="${MIN_TIME}")

echo "binary : ${BIN}"
echo "runs   : ${RUNS} x --benchmark_repetitions=${REPETITIONS}"
echo "out-dir: ${OUT_DIR}"
[[ -n "${FILTER}" ]] && echo "filter : ${FILTER}"

FILES=()
for ((i = 1; i <= RUNS; i++)); do
    OUT="${OUT_DIR}/${TAG}_${TS}_run${i}.json"
    echo "--- run ${i}/${RUNS} -> $(basename "${OUT}")"
    "${BIN}" "${ARGS[@]}" \
        --benchmark_out="${OUT}" \
        --benchmark_out_format=json > "${OUT%.json}.log" 2>&1
    tail -n +1 "${OUT%.json}.log" | grep -E "^(Benchmark|BM_)" || true
    FILES+=("${OUT}")
done

echo
echo "wrote ${#FILES[@]} JSON file(s) to ${OUT_DIR}"
if command -v python3 >/dev/null 2>&1; then
    echo
    python3 "${SCRIPT_DIR}/summarize_bench.py" "${FILES[@]}"
else
    echo "(python3 not found: skipping summary; JSON files above hold the data)"
fi
