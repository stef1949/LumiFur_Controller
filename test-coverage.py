"""
PlatformIO script to generate code coverage reports for Unity tests.
This script runs after test execution to collect and process coverage data.
"""

Import("env")
import os
import subprocess

def generate_coverage_report(*args, **kwargs):
    """
    Generate code coverage report using gcov/lcov after test execution.
    """
    print("=" * 60)
    print("Generating coverage report...")
    print("=" * 60)
    
    project_dir = env.get("PROJECT_DIR")
    build_dir = env.get("BUILD_DIR")
    
    # Check if we're in a native environment with coverage enabled
    if not env.get("PIOENV", "").startswith("native"):
        print("Coverage generation skipped - not a native environment")
        return
    
    # Check if gcovr or lcov is available
    try:
        # Try gcovr first (easier to use)
        result = subprocess.run(
            ["gcovr", "--version"],
            capture_output=True,
            text=True
        )
        if result.returncode == 0:
            print("Using gcovr for coverage generation")
            
            # Generate HTML report
            html_dir = os.path.join(project_dir, "coverage_report")
            os.makedirs(html_dir, exist_ok=True)
            
            subprocess.run([
                "gcovr",
                "--root", project_dir,
                "--filter", "src/.*",
                "--exclude", "test/.*",
                "--exclude", ".pio/.*",
                "--html-details", os.path.join(html_dir, "index.html"),
                "--print-summary"
            ])
            
            print(f"\nHTML coverage report generated: {html_dir}/index.html")
            
            # Generate lcov format for Coveralls
            subprocess.run([
                "gcovr",
                "--root", project_dir,
                "--filter", "src/.*",
                "--exclude", "test/.*",
                "--exclude", ".pio/.*",
                "--lcov", "coverage.info"
            ])
            
            print("LCOV coverage data generated: coverage.info")
        else:
            print("gcovr not available")
    except FileNotFoundError:
        print("gcovr not found, coverage report not generated")
        print("Install with: pip install gcovr")
    
    print("=" * 60)

# Register the callback to run after test
env.AddPostAction("checkprogsize", generate_coverage_report)
