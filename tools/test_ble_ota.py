#!/usr/bin/env python3
"""Run the production BLE OTA callbacks against deterministic host flash/BLE stubs."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
tests = root / "test" / "host_ble_ota"
with tempfile.TemporaryDirectory(prefix="lumifur-ota-") as temporary:
    binary = Path(temporary) / "ota_test"
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
        "-I", str(tests / "stubs"), "-I", str(root / "src"), "-I", str(root / "include"),
        str(tests / "ota_test.cpp"), str(root / "src" / "ble" / "ble_ota.cpp"),
        "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
