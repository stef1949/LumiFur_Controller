#!/usr/bin/env bash
# Fallback runner for the same production core modules as PlatformIO native2.
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNITY_DIR="${UNITY_DIR:-/tmp/unity-framework}"
if [[ ! -f "$UNITY_DIR/src/unity.c" ]]; then
    git clone --depth 1 https://github.com/ThrowTheSwitch/Unity.git "$UNITY_DIR"
fi
RUN_DIR="$(mktemp -d)"
cleanup() {
    for binary in "$RUN_DIR"/*; do
        if [[ -f "$binary" ]]; then rm -- "$binary"; fi
    done
    rmdir -- "$RUN_DIR"
}
trap cleanup EXIT
sources=(
    "$PROJECT_DIR/src/core/AnimationState.cpp"
    "$PROJECT_DIR/src/core/ColorParser.cpp"
    "$PROJECT_DIR/src/core/ScrollState.cpp"
    "$PROJECT_DIR/src/core/TemperatureHistory.cpp"
    "$PROJECT_DIR/src/core/video/MonoVideoCodec.cpp"
    "$PROJECT_DIR/src/core/mic/mic_math.cpp"
    "$PROJECT_DIR/src/core/mouth/MouthMorph.cpp"
)
failed=0
for test_file in "$PROJECT_DIR"/test/test_*/*.cpp; do
    test_name="$(basename "$(dirname "$test_file")")"
    binary="$RUN_DIR/$test_name"
    g++ -std=c++17 -I"$UNITY_DIR/src" -I"$PROJECT_DIR/src" -I"$PROJECT_DIR/include" \
        "$UNITY_DIR/src/unity.c" "${sources[@]}" "$test_file" -o "$binary"
    if output="$("$binary")"; then
        printf '%s\n' "$output"
        # Older suites return zero even when Unity reports an assertion failure.
        if [[ "$output" =~ [1-9][0-9]*[[:space:]]Failures ]]; then failed=1; fi
    else
        printf '%s\n' "$output"
        failed=1
    fi
done
exit "$failed"
