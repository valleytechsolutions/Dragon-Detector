# Security

Use the latest published release and verify downloads against its SHA-256 checksums. Install only the image for your exact board.

## Reporting

Use the repository’s **Security → Report a vulnerability** option when available. Otherwise, open an issue with a brief impact summary and request a private contact channel before sharing exploit details, credentials, or captures.

## Data handling

- Wi-Fi profiles are stored in the device’s unencrypted NVS. Anyone with physical access may be able to recover them. Remove saved profiles before transferring a device to someone else.
- Captures, encounter history, badges, and profiles can contain private data. Review them before sharing logs or screenshots.
- USB tools require the browser’s device-access permission. Firmware installation replaces code on the selected device; fresh installation also replaces saved files.
- The flasher uses a shared page-visit counter. [Counter behavior](docs/VISIT-COUNTER.md)

## Repository checks

Release packaging rejects private workstation paths in firmware images. Private working files, local environments, backups, and captures are excluded from source control. Secret scanning complements code review; a passing scan does not establish the absence of every vulnerability.
