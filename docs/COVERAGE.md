# Code Coverage with Coveralls

This project uses [Coveralls](https://coveralls.io/) to track code coverage for Unity framework tests.

## Coverage Badge

The coverage status is displayed in the main README:

[![Coverage Status](https://coveralls.io/repos/github/stef1949/LumiFur_Controller/badge.svg?branch=main)](https://coveralls.io/github/stef1949/LumiFur_Controller?branch=main)

## How Coverage Works

### Test Environment

The `native2` environment in `platformio.ini` is configured for code coverage:

```ini
[env:native2]
platform = native
test_framework = unity
build_flags = 
    -lgcov
    --coverage
    -DUNITY_CONFIG_DIR=./unity
extra_scripts = test-coverage.py
```

Key flags:
- `-lgcov`: Links against the gcov library
- `--coverage`: Enables both compilation and linking flags for coverage
- `extra_scripts`: Runs `test-coverage.py` after tests to generate reports

### GitHub Actions Workflow

The `.github/workflows/coveralls.yaml` workflow:

1. **Runs Unity tests** on the `native2` environment
2. **Collects coverage data** using `lcov` and `gcovr`
3. **Filters coverage** to only include `src/` files
4. **Uploads to Coveralls** using the official GitHub Action

### Local Coverage Generation

To generate coverage reports locally:

```bash
# Install coverage tools
sudo apt-get install lcov gcovr  # Linux
brew install lcov gcovr          # macOS

# Run tests with coverage
pio test -e native2

# Generate HTML report
gcovr --root . \
      --filter 'src/.*' \
      --exclude 'test/.*' \
      --exclude '.pio/.*' \
      --html-details coverage_report/index.html

# View report
open coverage_report/index.html  # macOS
xdg-open coverage_report/index.html  # Linux
```

### What Gets Covered

The coverage tracks:
- ✅ All source files in `src/` directory
- ✅ Core modules: AnimationState, ScrollState, ColorParser
- ✅ BLE functionality
- ✅ Easing functions

Excluded from coverage:
- ❌ Test files (`test/`)
- ❌ Build artifacts (`.pio/`)
- ❌ Library dependencies (`lib/`)
- ❌ Header-only template code

## Coverage Reports

### Coveralls Dashboard

Visit the [Coveralls dashboard](https://coveralls.io/github/stef1949/LumiFur_Controller) to see:
- Overall coverage percentage
- Line-by-line coverage
- Coverage trends over time
- Per-file coverage breakdown

### Local HTML Reports

After running tests locally, view the detailed HTML report at:
- `coverage_report/index.html` - Overview and file listing
- Individual file reports with line-by-line coverage highlighting

## Improving Coverage

To improve test coverage:

1. **Identify uncovered code** in the Coveralls dashboard
2. **Write new Unity tests** for uncovered functions
3. **Run tests locally** to verify coverage increases
4. **Push changes** - GitHub Actions will update Coveralls automatically

## Test Suites with Coverage

Current test suites contributing to coverage:

- **test_animation_state** (7 tests) - `src/core/AnimationState.cpp`
- **test_scroll_state** (6 tests) - `src/core/ScrollState.cpp`
- **test_easing_functions** (7 tests) - Easing functions in `src/main.h`
- **test_ble** (13 tests) - BLE helper functions
- **test_accelerometer** (19 tests) - Motion detection logic
- **test_microphone** (25 tests) - Audio processing functions

Total: **77 tests** tracking coverage across core embedded functionality.

## Troubleshooting

### Coverage not updating on Coveralls

1. Check that the workflow ran successfully in GitHub Actions
2. Verify the `GITHUB_TOKEN` has proper permissions
3. Check that coverage data was generated (`coverage_filtered.info`)
4. Ensure the repository is linked to Coveralls account

### Low coverage percentage

This is expected for embedded projects because:
- Hardware-dependent code can't be tested in native environment
- Some code requires ESP32-specific libraries
- Integration tests run on hardware, not in CI

Focus on testing:
- Pure logic functions (calculations, state management)
- Data processing algorithms
- Helper functions and utilities

### Coverage data not generated locally

Ensure gcov is properly linked:
```bash
# Check for .gcda files after test run
find .pio/build/native2 -name "*.gcda"

# If no files found, ensure coverage flags are in platformio.ini
pio test -e native2 -v
```

## References

- [Coveralls Documentation](https://docs.coveralls.io/)
- [gcov Documentation](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html)
- [lcov Documentation](http://ltp.sourceforge.net/coverage/lcov.php)
- [PlatformIO Unit Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/)
