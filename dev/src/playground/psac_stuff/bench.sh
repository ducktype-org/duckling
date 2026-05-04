#!/bin/bash

# Adjust accordingly
REPO_ROOT="/home/mateusz/code/duckling"
BUILD_DIR="$REPO_ROOT/dev/build-bench-psac"
BIN_DIR="$BUILD_DIR/bin"

PSAC_ARRAY_TARGET="psac_array_sum"
QUERY_ARRAY_TARGET="query_array_sum"
PSAC_DAG_TARGET="psac_dag"
QUERY_DAG_TARGET="query_dag"

PSAC_ARRAY="$BIN_DIR/psac_array_sum"
QUERY_ARRAY="$BIN_DIR/query_array_sum"
PSAC_DAG="$BIN_DIR/psac_dag"
QUERY_DAG="$BIN_DIR/query_dag"

echo "=== Building Binaries ==="

TARGETS=(
    "$PSAC_ARRAY_TARGET"
    "$QUERY_ARRAY_TARGET"
    "$PSAC_DAG_TARGET"
    "$QUERY_DAG_TARGET"
)

if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory $BUILD_DIR does not exist!"
    exit 1
fi

for TARGET in "${TARGETS[@]}"; do
    echo "Building target: $TARGET..."
    cmake --build "$BUILD_DIR" --target "$TARGET" -j$(nproc)

    if [ $? -ne 0 ]; then
        echo "Error: Build failed for target $TARGET"
        exit 1
    fi
done

echo "Build successful!"
echo ""

# Common
THREADS="1,4,8,16"
WORKLOADS="0,50,100"
UP_PERCENT="1,10,50,100"
MODES="calc,update"

# ArraySum
SIZES="65536,262144"

# DAG
DAG_LAYERS="50"
DAG_VERTS="100"
DAG_EDGES="10000"

echo "=== Running ARRAY SUM Benchmarks ==="
hyperfine --warmup 3 \
  --parameter-list size "$SIZES" \
  --parameter-list threads "$THREADS" \
  --parameter-list workload "$WORKLOADS" \
  --parameter-list up "$UP_PERCENT" \
  --parameter-list mode "$MODES" \
  --export-json results_array.json \
  "$PSAC_ARRAY {size} {threads} {workload} {up} {mode}" \
  "$QUERY_ARRAY {size} {threads} {workload} {up} {mode}"

echo ""
echo "=== Running DAG Benchmarks ==="

hyperfine --warmup 3 \
  --parameter-list threads "$THREADS" \
  --parameter-list workload "$WORKLOADS" \
  --parameter-list up "$UP_PERCENT" \
  --parameter-list mode "$MODES" \
  --export-json results_dag.json \
  "$PSAC_DAG $DAG_LAYERS $DAG_VERTS $DAG_EDGES {threads} {workload} {up} {mode}" \
  "$QUERY_DAG $DAG_LAYERS $DAG_VERTS $DAG_EDGES {threads} {workload} {up} {mode}"

echo "Done! Results saved to results_array.json and results_dag.json"
