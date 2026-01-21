#!/bin/bash
# Simple test runner for Unity tests when PlatformIO is not available
# This script compiles and runs Unity tests using GCC

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Unity framework location
UNITY_DIR="/tmp/unity-framework"

echo -e "${YELLOW}Unity Test Runner for LumiFur Controller${NC}"
echo "==========================================="
echo ""

# Check if Unity is available, if not clone it
if [ ! -d "$UNITY_DIR" ]; then
    echo "Unity framework not found. Cloning from GitHub..."
    git clone https://github.com/ThrowTheSwitch/Unity.git "$UNITY_DIR"
    echo "Unity framework cloned successfully."
    echo ""
fi

# Create test runner main function
cat > /tmp/unity_test_runner.cpp << 'EOF'
void setup(void);
void loop(void);

int main(int argc, char** argv) {
  setup();
  return 0;
}
EOF

# Function to compile and run a test
run_test() {
    local test_name=$1
    local test_file=$2
    local source_files=$3
    
    echo -e "${YELLOW}Running $test_name...${NC}"
    
    # Compile
    g++ -std=c++11 \
        -I"$UNITY_DIR/src" \
        -I"$PROJECT_DIR/src" \
        -o "/tmp/$test_name" \
        "$UNITY_DIR/src/unity.c" \
        $source_files \
        "$test_file" \
        /tmp/unity_test_runner.cpp
    
    # Run
    if "/tmp/$test_name"; then
        echo -e "${GREEN}✓ $test_name passed${NC}"
        echo ""
        return 0
    else
        echo -e "${RED}✗ $test_name failed${NC}"
        echo ""
        return 1
    fi
}

# Run tests
cd "$PROJECT_DIR"

total_tests=0
passed_tests=0

# AnimationState tests
if run_test "test_animation_state" \
    "$PROJECT_DIR/test/test_animation_state/test_animation_state.cpp" \
    "$PROJECT_DIR/src/core/AnimationState.cpp"; then
    ((passed_tests++))
fi
((total_tests++))

# ScrollState tests
if run_test "test_scroll_state" \
    "$PROJECT_DIR/test/test_scroll_state/test_scroll_state.cpp" \
    "$PROJECT_DIR/src/core/ScrollState.cpp"; then
    ((passed_tests++))
fi
((total_tests++))

# Easing functions tests (need wrapper)
cat > /tmp/easing_wrapper.cpp << 'EOFEASE'
float easeInQuad(float t)
{
  if (t < 0.0f)
    return 0.0f;
  if (t > 1.0f)
    return 1.0f;
  return t * t;
}

float easeOutQuad(float t)
{
  if (t < 0.0f)
    return 0.0f;
  if (t > 1.0f)
    return 1.0f;
  return 1.0f - (1.0f - t) * (1.0f - t);
}

float easeInOutQuad(float t)
{
  return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
}
EOFEASE

if run_test "test_easing_functions" \
    "$PROJECT_DIR/test/test_easing_functions/test_easing_functions.cpp" \
    "/tmp/easing_wrapper.cpp"; then
    ((passed_tests++))
fi
((total_tests++))

# BLE tests
if run_test "test_ble" \
    "$PROJECT_DIR/test/test_ble/test_ble.cpp" \
    ""; then
    ((passed_tests++))
fi
((total_tests++))

# Print summary
echo ""
echo "==========================================="
if [ $passed_tests -eq $total_tests ]; then
    echo -e "${GREEN}All $total_tests test suites passed!${NC}"
    exit 0
else
    echo -e "${RED}$passed_tests/$total_tests test suites passed${NC}"
    exit 1
fi
