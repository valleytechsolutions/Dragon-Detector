# Publishing

The current source is built from the repository root. See [BUILDING.md](BUILDING.md) and [FLASHING.md](FLASHING.md).

The firmware workflow runs native and browser checks, then builds/packages all five boards. Each artifact contains split images, a merged factory image, factory/update ESP Web Tools manifests, build metadata and checksums. Successful `main` pushes trigger the Pages workflow, which uses artifacts and source from that same run. It packages guides and images alongside the flasher so setup instructions do not depend on repository rendering.

Enable **GitHub Actions** as the Pages source in repository Settings → Pages. The intended URL is [Dragon Detector](https://valleytechsolutions.github.io/Dragon-Detector/). Verify deployment status before describing a local preview as published.

## Independent hosting

The working flasher is [valleytech-dragon-detector.pages.dev](https://valleytech-dragon-detector.pages.dev/). See [hosting status](HOSTING-STATUS.md) for the current GitHub outage. GitHub Actions permissions alone cannot override an account-level restriction. A Gitea repository copy does not deploy GitHub Pages or configure replacement runners.

Use the five artifacts from one verified firmware run under `web/firmware/<board>/`. Do not rebuild or rename firmware just to repair hosting. Package and inspect a new, empty output directory:

```sh
python -m pip install -r tools/requirements-site.txt
python tools/validate_release.py
node tools/check_web_manifests.mjs
node --test tests/*.test.js
python tools/build_static_site.py --output .build/site
python -m http.server 8765 --bind 127.0.0.1 --directory .build/site
```

The packager verifies hashes, keeps factory/update manifests and their split images, generates local HTML guides and a download index, and rejects files larger than the static host's 25 MiB limit. Only the C152 merged factory image is included for M5Burner; the web installer uses split images on every board. Source archives and other merged images remain release assets.

After checking all five board selectors, both installation modes, the Custom theme tab, guides and images, publish from an authenticated Cloudflare account:

```sh
npx wrangler@4.139.0 pages deploy .build/site --project-name valleytech-dragon-detector --branch main
```

Verify the public manifests and downloaded image hashes after deployment. A website-only update can reuse the existing firmware version; it must not claim that device firmware changed. Keep Cloudflare and GitHub hosting synchronized during future releases. The visit counter allows both production origins and retains a single shared total; its deployment is documented in [VISIT-COUNTER.md](VISIT-COUNTER.md).

Do not upload `.work/`, `.pio/`, device flash backups, scan contacts, raw packet captures, credentials or local serial logs. Generated current firmware lives in CI artifacts / Pages, not under a misleading older version filename. Earlier board releases remain under `hardware/` with original licenses.
