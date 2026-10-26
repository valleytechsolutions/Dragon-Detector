# GPS + SD upload investigation — 2026-10-23

Historical design for the **0.13.28 resource beta**. That beta did not read
credentials or send uploads. This proposal is retained as research history;
its `config.txt` schema and automatic-queue policy are not the current interface.
The **0.13.39 GPS Waveshare development build** implements explicit uploads with
Piglet-compatible `/wardriver.cfg` fields. See the [current upload guide](WARDRIVE-UPLOADS.md)
and [validation record](VALIDATION-0.13.39.md). Live authenticated service
validation remains pending. No real or synthetic observations were uploaded
to production services during this work.

## Verified official contracts

| Service | Official endpoint | Authentication | Body |
|---|---|---|---|
| WiGLE v2 | `https://api.wigle.net/api/v2/file/upload` | HTTP Basic API Name + API Token | multipart `file`, including filename; omit `donate` unless separately opted in |
| WiGLE v3 | `https://api.wigle.net/api/v3/upload` | HTTP Basic API Name + API Token | multipart `file`, optional `fileSize` |
| WDGWars | `https://wdgwars.pl/api/upload-csv` | `X-API-Key`, 64-character key from profile | multipart `file`, WiGLE/Bruce CSV/LOG |

Source: [WiGLE official Swagger](https://api.wigle.net/swagger.json), [WiGLE CSV format](https://api.wigle.net/csvFormat.html), [WDGWars developer section](https://www.wdgwars.pl/press). WiGLE v2 explicitly documents WiGLE CSV and a 180 MiB maximum; v3 documents 429 daily upload limits and requires Basic auth. Prefer the documented v2 file contract for the existing WiGLE-1.6 logger; verify account support before any future production rollout. WDGWars' public page says 40 MB for CSV; third-party reports disagree on operational caps/routes. Do not silently switch to undocumented paths or assume those limits are permanently reliable.

Review [WDGWars terms](https://wdgwars.pl/terms), version 1.2 effective 2026-10-16: only personally collected broadcast observations; no fabricated/copied data or GPS spoofing; uploads grant processing/presentation rights. The [WiGLE EULA](https://wigle.net/eula.html) governs non-commercial database use and forbids credential sharing. The firmware must not accept terms on a user's behalf. Link these pages beside an opt-in and let the account holder decide. Commercial donation is a separate choice and defaults off. No upload fixture should ever be sent to these production services.

## Proposed private SD-root config.txt

Only a profile with `BOARD_HAS_SD && BOARD_HAS_GPS` may load this file. Missing/invalid/duplicate/unknown keys fail closed; all destinations default off. Parse a maximum of 2 KiB into bounded fixed fields. The reference policy in `upload_policy.h` is host-tested and deliberately not wired to the firmware startup or a network sender.

```ini
config_version=1
uploads_enabled=false
terms_reviewed=false
wigle_enabled=false
wdgwars_enabled=false
wigle_api_name=
wigle_api_token=
wdgwars_api_key=
```

Do not put credentials in the downloadable pack, web page, source tree, console commands, diagnostic dumps, or request URLs. Production integration must block `/config.txt` from the generic USB `get` path and all file browser/export paths, suppress framework HTTP debug output, and never print response bodies that might echo credentials. Use an explicit private network configuration flow; do not reuse arbitrary captured credentials. Separate destination opt-ins are required. File permissions on removable FAT do not provide confidentiality; users must keep their card private.

## Queue and radio design

Use only finalized WiGLE CSV segments in `/dragon/wardrive`, never `.part`, captures, arbitrary paths or another user's logs. Persist one job per immutable file hash and destination, with separate accepted receipts. Enqueue new sessions after opt-in; existing logs require explicit selection. Keep originals and separate pending/accepted/rejected/uncertain states. Power loss after server acceptance but before receipt persistence is an uncertain outcome, not proof of failure. Do not blindly resend ambiguous jobs; WiGLE returns a transaction identifier whose processing result should be tracked.

Automatic upload may run only while the user has paused scanning, no recording is finalizing and the network task has exclusive radio ownership. Association and TLS on ESP32 share resources with scanning; merely launching a FreeRTOS task does not guarantee noninterference. Resuming radar must cancel upload, close TLS, release SD/network ownership and restore scanning before reporting success. Stream bounded chunks with backpressure and deadlines. Require certificate/hostname verification and valid time; never use `setInsecure()`.

The tested policy yields: 401/403 => blocked until config changes; malformed/permanent 4xx => rejected; 429 => bounded retry respecting numeric Retry-After; 5xx => exponential backoff; network loss after transmission => uncertain/manual reconciliation. A successful HTTP status alone is insufficient: require the service's success field and retain its transaction/receipt before marking accepted. Cap retries and daily attempts; preserve the queue across reboot. Integrate and test filesystem journals, transport, cancellation, certificate failures and radio ownership before changing the status above.
