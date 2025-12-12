# Web Firmware Updater on GitHub Pages with Cloudflare

The Web Bluetooth firmware updater lives in `docs/firmware-updater/` and is served directly from GitHub Pages.

## Verify Web Bluetooth Requirements
- GitHub Pages and Cloudflare both serve the site over HTTPS, satisfying Web Bluetooth's secure context requirement.
- When testing locally, continue using `http://localhost` or `https://<hostname>`—`file://` is blocked by Web Bluetooth.
