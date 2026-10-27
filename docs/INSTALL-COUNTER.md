# Completed web-flasher installs

The separate install-counter Worker is deployed. Wrangler's missing Workers
permissions were restored through OAuth reauthorization on October 26, 2026.
Its production endpoint below was verified with a read-only request before
publishing the updated flasher.

The top-right **INSTALLS** counter starts October 26, 2026. It counts successful
firmware writes reported by the web installer on the two production origins.
Updates and reinstalls count. Page loads, firmware downloads, cancelled/failed
writes, USB clock-only setup and personal-theme-only uploads do not.

The existing page-visit total remains in its separate service. It is not used
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

Worker: `services/install-counter/`; production endpoint:
`https://dragon-detector-installs.valleytechsolutions-dd.workers.dev/installs`.
GET reads the count without incrementing. POST accepts a JSON UUID-v4 `id` from
`https://valleytechsolutions.github.io` or
`https://valleytech-dragon-detector.pages.dev`. Local and preview sites only read.
The old visit service is untouched and may still receive page-load requests
from cached old pages. Current source uses only the new install service.

The new Worker is `dragon-detector-installs`, with class `InstallCounter`, binding
`INSTALLS`, migration `v1` and named instance `dragon-detector-flasher-installs`.
Preserve these after first deployment to retain the total. Do not seed installs
from the old visit count. Deploy the Worker before the updated website. No
firmware rebuild is required. This website update retains every published
0.13.38 firmware package; only counter-related assets and guides change.

`tests/install-counter.test.js` exercises real SQLite, concurrent completions,
lost-response deduplication, persistence, independent visit totals, origin checks, bad
requests, stale reads and failure handling. `tests/clock-verification.test.js`
covers success-only reporting and independent USB/clock handoff behavior.
