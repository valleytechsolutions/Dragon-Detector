# Completed web-flasher installs

The top-right **INSTALLS** counter starts October 26, 2026. It counts successful
firmware writes reported by the web installer on the two production origins.
Updates and reinstalls count. Page loads, firmware downloads, cancelled/failed
writes, USB clock-only setup and personal-theme-only uploads do not.

The former page-visit total is preserved separately and frozen. It is not used
as an install baseline: historical installs cannot be inferred from page visits.
Command-line, M5Burner, Tuya-tool and other external installs are not observed.

## Completion and storage

The pinned ESP Web Tools dialog reports its `finished` write state. Completion
is reported once per flash attempt, including when the subsequent clock setup
is disabled or the USB reopen fails. Closing the dialog cannot count it again.
The counter never blocks flashing, clock verification or personal-theme setup.

The browser creates one random ID per completion and retries a transient delivery
failure once using that same ID. A SQLite insertion and trigger update the total
atomically; duplicate delivery cannot increment twice. Random retry IDs expire
after seven days. No visitor ID, device serial, IP address, board identity or
cookie is collected by this application. Worker request logging stays disabled.
Cloudflare still processes requests as the infrastructure provider.

This is a count of browser-reported completions, not cryptographic proof of
unique devices. CORS restricts browser writes to the production sites, but an
automated client can imitate requests. Network outages or closing the page
before delivery can undercount. Unknown/unavailable totals display a dash.

## Endpoints and deployment

Worker: `services/visit-counter/`; public endpoint:
`https://dragon-detector-visits.valleytechsolutions-dd.workers.dev/installs`.
GET reads the count without incrementing. POST accepts a JSON UUID-v4 `id` from
`https://valleytechsolutions.github.io` or
`https://valleytech-dragon-detector.pages.dev`. Local and preview sites only read.
The old `/visits` endpoint returns its frozen historical total, including for
cached old pages still sending POST requests.

Preserve the existing Worker name, `VisitCounter` class, `VISITS` binding,
migration history and `dragon-detector-flasher` instance. New install tables
are additive; do not rename/drop the old visits table or seed installs from it.
Deploy the Worker before the updated website. No firmware rebuild is required.

`tests/install-counter.test.js` exercises real SQLite, concurrent completions,
lost-response deduplication, persistence, legacy visits, origin checks, bad
requests, stale reads and failure handling. `tests/clock-verification.test.js`
covers success-only reporting and independent USB/clock handoff behavior.
