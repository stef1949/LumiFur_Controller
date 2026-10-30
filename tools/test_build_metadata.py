"""Check the build helper and preprocessor fallback against actual Git state."""
from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import os
import subprocess
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = spec_from_file_location("build_metadata", ROOT / "tools/platformio/custom_compile.py")
metadata = module_from_spec(spec)
spec.loader.exec_module(metadata)


class Environment(dict):
    def StringifyMacro(self, value):
        return '"' + value + '"'

    def Append(self, **values):
        self.update(values)

class BuildMetadataTest(unittest.TestCase):
    def test_git_hash_is_injected_without_replacing_firmware_version(self):
        env = Environment(PROJECT_DIR=str(ROOT))
        metadata.inject_git_info(env)
        definitions = dict(env["CPPDEFINES"])
        expected = subprocess.check_output(["git", "rev-parse", "--short=12", "HEAD"], cwd=ROOT, text=True).strip()
        self.assertEqual(definitions["GIT_COMMIT"], '"' + expected + '"')
        self.assertNotIn("FIRMWARE_VERSION", definitions)

    def test_source_date_epoch_is_utc(self):
        env = Environment(PROJECT_DIR=str(ROOT))
        with patch.dict(os.environ, {"SOURCE_DATE_EPOCH": "0"}):
            metadata.inject_git_info(env)
        definitions = dict(env["CPPDEFINES"])
        self.assertEqual(definitions["BUILD_DATE"], '"1970-01-01"')
        self.assertEqual(definitions["BUILD_TIME"], '"00:00:00 UTC"')
        self.assertEqual(env["BUILD_UNFLAGS"], ["-DBUILD_DATE=__DATE__", "-DBUILD_TIME=__TIME__"])

    def test_release_fallback_is_not_a_fake_commit(self):
        result = subprocess.check_output(
            ["g++", "-E", "-P", "-x", "c++", "-DDEBUG_MODE=0", "-include",
             str(ROOT / "src/ble/ble_constants.h"), "-"],
            input="GIT_COMMIT\nGIT_BRANCH\n", text=True,
        )
        self.assertEqual(result.split(), ['"unknown"', '"main"'])


if __name__ == "__main__":
    unittest.main()
