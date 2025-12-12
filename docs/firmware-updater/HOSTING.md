# Hosting the Web Firmware Updater on GitHub Pages with Cloudflare

The Web Bluetooth firmware updater lives in `docs/firmware-updater/` and can be served directly from GitHub Pages. This guide explains how to publish the static site and front it with Cloudflare for HTTPS and a custom domain.

## 1. Deploy to GitHub Pages
1. Ensure the repository has GitHub Pages enabled for the `GitHub Actions` source.
2. The workflow `.github/workflows/deploy-web-updater.yml` uploads `docs/firmware-updater/` as the site artifact on pushes to `main`.
3. After the workflow runs successfully, GitHub publishes the site to the `github-pages` environment. The URL reported by the deployment step is the public endpoint (e.g., `https://<user>.github.io/LumiFur_Controller/firmware-updater/`).
4. To trigger a deployment manually (for example after updating assets), run the **Deploy Web Firmware Updater to GitHub Pages** workflow via **Actions → Deploy Web Firmware Updater to GitHub Pages → Run workflow**.

## 2. Configure Cloudflare for a Custom Domain
1. Add your domain to Cloudflare and set the DNS records:
   - Create a CNAME pointing the desired hostname (e.g., `update.example.com`) to `<user>.github.io`.
   - Keep the proxy enabled so Cloudflare manages TLS and caching.
2. Under **SSL/TLS → Overview**, set the mode to **Full** so Cloudflare establishes HTTPS to GitHub Pages.
3. (Optional but recommended) Enable **Always Use HTTPS** and **HTTP Strict Transport Security (HSTS)** in **SSL/TLS → Edge Certificates**.
4. If you want Cloudflare to redirect a path to the updater, create a Page Rule: `https://update.example.com/*` → **Forwarding URL (302)** to the Pages URL from the workflow output.

## 3. Verify Web Bluetooth Requirements
- GitHub Pages and Cloudflare both serve the site over HTTPS, satisfying Web Bluetooth's secure context requirement.
- When testing locally, continue using `http://localhost` or `https://<hostname>`—`file://` is blocked by Web Bluetooth.

Once deployed, you can share the Cloudflare-backed URL with users to access the Web Firmware Updater without running a local server.
