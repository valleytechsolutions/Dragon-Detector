# Publishing

The current source is built from the repository root. See [BUILDING.md](BUILDING.md) and [FLASHING.md](FLASHING.md).

The firmware workflow tests both the parser/tracker and badge protocol, then builds/packages all three boards. Each artifact contains split images, a merged factory image, factory/update ESP Web Tools manifests, build metadata and checksums. Successful `main` pushes trigger the Pages workflow, which uses artifacts and source from that same run.

Enable **GitHub Actions** as the Pages source in repository Settings → Pages. The intended URL is [Dragon Detector](https://valleytechsolutions.github.io/Dragon-Detector/). Verify deployment status before describing a local preview as published.

Do not upload `.work/`, `.pio/`, device flash backups, scan contacts, raw packet captures, credentials or local serial logs. Generated current firmware lives in CI artifacts / Pages, not under a misleading older version filename. Earlier board releases remain under `hardware/` with original licenses.
