# Publishing

The current source is built from the repository root. See [BUILDING.md](BUILDING.md) and [FLASHING.md](FLASHING.md).

The firmware workflow runs native and browser checks, then builds/packages all
eleven board profiles. Each artifact contains split images, a merged factory
image, factory/update ESP Web Tools manifests, build metadata and checksums.
These are CI artifacts; pushing source does not publish a new firmware release.

Enable **GitHub Actions** as the Pages source in repository Settings → Pages.
The Pages workflow publishes `hosting/github-pages/index.html` as both the entry
page and the custom 404 page. It forwards the
[old GitHub Pages address](https://valleytechsolutions.github.io/Dragon-Detector/)
to the primary flasher, preserving tabs, query parameters and guide paths.
It runs on changes to that page, its tests or the workflow on `main`, and can also
be dispatched manually. Firmware builds do not overwrite this forwarding page.

## Independent hosting

The primary flasher is [valleytech-dragon-detector.pages.dev](https://valleytech-dragon-detector.pages.dev/).
GitHub access was verified restored on October 30, 2026. Keep the repository
website field and README install links pointed at this primary URL. See
[hosting status](HOSTING-STATUS.md). A Gitea copy does not deploy either host.

For a firmware release, use the eleven artifacts from one verified firmware run
under `web/firmware/<board>/`. For website-only changes, preserve the existing
published firmware packages. Package and inspect a new, empty output directory:

```sh
python -m pip install -r tools/requirements-site.txt
python tools/validate_release.py
node tools/check_web_manifests.mjs
node --test tests/*.test.js
python tools/build_static_site.py --output .build/site
python -m http.server 8765 --bind 127.0.0.1 --directory .build/site
```

The packager verifies hashes, keeps factory/update manifests and their split images, generates local HTML guides and a download index, and rejects files larger than the static host's 25 MiB limit. Only the C152 merged factory image is included for M5Burner; the web installer uses split images on every board. Source archives and other merged images remain release assets.

After checking all eleven board selectors, both installation modes, the Custom theme tab, guides and images, publish from an authenticated Cloudflare account:

```sh
npx wrangler@4.139.0 pages deploy .build/site --project-name valleytech-dragon-detector --branch main
```

Verify the public manifests and downloaded image hashes after deployment. A website-only update can reuse the existing firmware version; it must not claim that device firmware changed. GitHub Pages continues forwarding to Cloudflare during future releases. Preserve the install counter's carried-forward baseline and recorded completions; its deployment is documented in [INSTALL-COUNTER.md](INSTALL-COUNTER.md).

Do not upload `.work/`, `.pio/`, device flash backups, scan contacts, raw packet captures, credentials or local serial logs. Generated current firmware lives in CI artifacts / Pages, not under a misleading older version filename. Earlier board releases remain under `hardware/` with original licenses.
