#!/bin/bash
# Benchmarks psacklinger (PSAC-parallel compiler) against duckc (the real compiler)
# on full compiles, and measures psacklinger's thread scaling.
#
# Usage:
#   ./bench_compiler.sh [input.dmf ...]
#
# Without arguments the large generated samples from the benchmark repo are used.
# Tunables via environment:
#   THREADS="1 2 4 8 16"   thread counts for psacklinger -j
#   RUNS=5 WARMUP=1        hyperfine repetitions
#
# Notes:
# - duckc needs `import core.builtins.*;` which the generated samples omit
#   (psacklinger registers the builtin implicitly); a patched copy is created.
# - duckc must run from the dev repo root (the standard library path is relative)
#   and keeps an incremental cache in ./duck_build — it is wiped before every
#   timed run (--prepare) so every measurement is a cold, full compile.
# - duckc is measured two ways:
#     * `duckc` — compile_module, single-threaded, object files only (no link);
#     * `duckc-wN` — compile_packages with N query-framework workers; this also
#       compiles the std/core packages and links a real executable, so it is the
#       closest equivalent of what psacklinger does (which also links).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Adjust accordingly
REPO_ROOT="/home/mateusz/code/duckling"
DEV_ROOT="$REPO_ROOT/dev"
BUILD_DIR="${BUILD_DIR:-$DEV_ROOT/build-release}"  # benchmark optimized builds by default
BENCH_DATA="/home/mateusz/code/benchmark/data/input"

DUCKC="$BUILD_DIR/bin/duckc"
PSACKLINGER="$BUILD_DIR/bin/psacklinger"

THREADS="${THREADS:-1 2 4 8 16}"
RUNS="${RUNS:-5}"
WARMUP="${WARMUP:-1}"

if [ "$#" -gt 0 ]; then
    INPUTS=("$@")
else
    INPUTS=(
        "$BENCH_DATA/simple1/duck1.dmf"
        "$BENCH_DATA/simple2/duck2.dmf"
    )
fi

echo "=== Building compilers ==="
cmake --build "$BUILD_DIR" --target duckc psacklinger -j"$(nproc)"

command -v hyperfine >/dev/null || { echo "Error: hyperfine not installed"; exit 1; }

WORK="$(mktemp -d /tmp/psacklinger_bench.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
echo "Work dir: $WORK"

# duckc resolves the standard library relative to the CWD.
cd "$DEV_ROOT"

for INPUT in "${INPUTS[@]}"; do
    if [ ! -f "$INPUT" ]; then
        echo "Error: input not found: $INPUT"
        exit 1
    fi
    NAME="$(basename "$INPUT" .dmf)"
    echo ""
    echo "=== Benchmarking $NAME ($(wc -l < "$INPUT") lines) ==="

    # duckc needs the builtins import; psacklinger compiles the original.
    DUCKC_INPUT="$WORK/${NAME}_duckc.dmf"
    { echo "import core.builtins.*;"; echo; cat "$INPUT"; } > "$DUCKC_INPUT"

    # Package + manifest for the multi-worker duckc runs (compile_packages).
    PKG="${NAME}pkg"
    PKG_DIR="$WORK/$PKG"
    ART_DIR="$WORK/${NAME}_artifacts"
    mkdir -p "$PKG_DIR"
    cp "$DUCKC_INPUT" "$PKG_DIR/$PKG.dmf"
    cat > "$WORK/${NAME}_manifest.json" <<EOF
{
    "packages": [
        {
            "name": "$PKG",
            "id": "$PKG",
            "version": "0.1.0",
            "dependencies": [],
            "path": "$PKG_DIR"
        }
    ],
    "tasks": [
        {
            "package": "$PKG",
            "strategy": "native",
            "output_file": "$PKG"
        }
    ]
}
EOF

    COMMANDS=(
        -n "duckc"
        "$DUCKC compile_module $DUCKC_INPUT --no-incremental"
        -n "psacklinger-seq"
        "$PSACKLINGER $INPUT --sequential -o $WORK/$NAME.out"
    )
    for T in $THREADS; do
        COMMANDS+=(
            -n "duckc-w$T"
            "$DUCKC compile_packages $WORK/${NAME}_manifest.json -w $T -a $ART_DIR --no-incremental"
            -n "psacklinger-j$T"
            "$PSACKLINGER $INPUT -j $T -o $WORK/$NAME.out"
        )
    done

    hyperfine \
        --warmup "$WARMUP" \
        --runs "$RUNS" \
        --prepare "rm -rf $DEV_ROOT/duck_build $ART_DIR" \
        --export-json "$SCRIPT_DIR/results_compiler_${NAME}.json" \
        --export-markdown "$SCRIPT_DIR/results_compiler_${NAME}.md" \
        "${COMMANDS[@]}"
done

echo ""
echo "Done! Results saved next to this script as results_compiler_<name>.{json,md}"
