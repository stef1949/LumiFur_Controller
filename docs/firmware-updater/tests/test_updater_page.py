from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FirmwareUpdaterPageTest(unittest.TestCase):
    def setUp(self) -> None:
        self.index_html = (ROOT / "index.html").read_text(encoding="utf-8")
        self.app_js = (ROOT / "app.js").read_text(encoding="utf-8")

    def test_index_contains_core_elements(self):
        """The updater UI should present primary controls."""
        for marker in ["connect-btn", "firmware-file", "upload-btn", "progress-bar", "log"]:
            with self.subTest(marker=marker):
                self.assertIn(marker, self.index_html)

    def test_app_logs_key_states(self):
        """Ensure the OTA workflow logs the main connection and upload states."""
        for marker in [
          "Connected to device and ready for OTA.",
          "Connect to the device before uploading.",
          "Beginning OTA upload",
          "End command sent. Waiting for device to reboot",
        ]:
            with self.subTest(marker=marker):
                self.assertIn(marker, self.app_js)

    def test_app_has_disconnect_guard(self):
        self.assertRegex(
            self.app_js,
            r"gattserverdisconnected",
            msg="Disconnect cleanup listener missing",
        )


if __name__ == "__main__":
    unittest.main()
