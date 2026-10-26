# Upload saved wardrives from the GPS Waveshare

Local development version **0.13.39**, `waveshare_175_gps` only. Finish the wardrive
and wait for the menu before uploading. Open **Radio tools → GPS wardrive → UPLOADS**.
Uploads use the Wi-Fi radio exclusively, so recording and uploading cannot run together.

## SD configuration

At boot, a FAT32 card gets a blank **`/wardriver.cfg`** if that file and its staging
file are absent. Existing files are never overwritten. Shut down before removing
the card, then edit the file with a text editor on your computer. A commented
[example](../examples/wardriver.cfg.example) is included. Do not post the filled
file, tokens or Wi-Fi password in a chat, issue, screenshot or repository.

```ini
homeSsid=Your WiFi name
homePsk=Your WiFi password
uploadNetworkProfile=
wigleBasicToken=
wdgwarsApiKey=
```

- `homeSsid` / `homePsk`: Wi-Fi used for uploads. A blank password is for an open
  network. Supported names are 1–32 printable ASCII bytes; passwords are 8–63.
- `uploadNetworkProfile`: optionally choose a previously saved network slot
  **0–7**, which takes precedence over the two fields above.
- `wigleBasicToken`: copy **Encoded for Use** from your [WiGLE API account](https://wigle.net/account).
  Paste just the encoded value, without `Basic `.
- `wdgwarsApiKey`: the API key from your [WDGoWars profile](https://wdgwars.pl/).

These key names match Piglet. Unknown configuration options are ignored; options
for automatic upload in another firmware do not enable it here. Configuration is
limited to 16 KiB and 511 bytes per line. Only readiness flags appear in status
output. Generic USB file downloads refuse this configuration file. Credentials
remain plaintext on your card; anyone reading the card can
read them. Temporary credentials are not saved to the device's network profiles.

Set a valid device clock first so HTTPS certificate validity can be checked.
WiGLE and WDGoWars use the SDK certificate trust bundle; certificate checking
is enabled and HTTP redirects are disabled.

## Buttons and results

**WiGLE** or **WDGoWars** joins the configured Wi-Fi and streams eligible saved
`wigle-*.csv` files from `/dragon/wardrive`, one at a time. Header-only files are
skipped. Captures remain on SD regardless of upload result. **CANCEL UPLOAD**
stops between bounded network operations; a TLS operation may take up to its
eight-second timeout before cancellation finishes. A Wi-Fi join has a 25-second
deadline. The normal radio state resumes after the operation.

| Display result | Meaning |
|---|---|
| Accepted | WiGLE returned HTTP 200 with `success: true`, or a previously queued WDGoWars job now reports `done`. A WiGLE acceptance is not proof that its background import completed. |
| Queued | WDGoWars returned HTTP 202, `ok: true` and a valid job ID, or its existing job is still pending. |
| Skipped | Header-only, already confirmed for this destination with the same contents, or not a queued job when checking jobs. |
| Failed | File/SD/network/service error or an earlier upload whose result needs review. Read the message. |

**CHECK JOBS** asks WDGoWars about previously saved job IDs. A queued file is
never reposted just because its import is unfinished. Authentication rejection
(401/403) or rate limiting (429) stops the batch. Correct configuration or wait
before trying again.

Each destination has its own receipt under `/dragon/wardrive-uploads`. Receipts
include file size, CRC, state and any job ID. Receipt writes use staging and a
recoverable previous file. A receipt is marked uncertain **before** starting a
POST, so power loss or a lost reply cannot trigger an automatic duplicate POST.
Corrupted receipts are also treated as uncertain. Do not delete receipts to
force a retry unless you have checked the destination's upload history.

**RETRY UNCLEAR** requires confirmation and a destination selection. Use it only
after checking that the earlier file did not arrive: the server can receive a
file even when the device loses the response. This action can create a duplicate
when the earlier upload actually succeeded.

## Validation and reference

Native tests execute the production HTTP streaming/response parser and batch
worker with fake HTTP/SD and the SDK's cJSON 1.7.18. They cover partial writes,
bounded JSON, rejected credentials, rate limits, pending jobs, independent
receipts, interrupted receipt replacement, cancellation and storage failures.
The connected device's real located WiGLE file passed byte/CRC, column, time and
coordinate checks. **Authenticated uploads to either live service have not been
verified yet; they require the owner's configured Wi-Fi and service credentials.**

The behavior reference is [Hamspiced/Piglet](https://github.com/Hamspiced/piglet/tree/fb71eded9c7bdec6b646f14dff3711ff4bd1bf5a).
This is an independent implementation. It keeps strict fresh-fix checks,
certificate verification, original CSVs and per-destination receipts.
[WDGoWars developer documentation](https://wdgwars.pl/press) describes API keys,
CSV requirements, limits and asynchronous jobs. The existing 32 MiB CSV segments
fit its documented 40 MB CSV limit as checked October 26, 2026.

USB commands: `screen wardrive-upload`, `wardrive upload status`,
`wardrive upload wigle`, `wardrive upload wdgwars`, `wardrive upload check`,
`wardrive upload cancel`. Bulk USB file transfers are blocked during an upload.
