# Shared flasher visits

The top-right counter shows page loads across all visitors, beginning October 16,
2026. Reloading counts again; navigating between page sections does not. It is
not a count of unique people, firmware installs, or visits before this release.

The static GitHub Pages flasher calls the small Cloudflare Worker in
`services/visit-counter/`. A SQLite-backed Durable Object stores one integer and
increments it in a single atomic statement. No visitor identifiers, IP addresses
or cookies are stored by the application. Worker request logging is disabled;
Cloudflare still handles requests as the infrastructure provider.

Public endpoint: <https://dragon-detector-visits.valleytechsolutions-dd.workers.dev/visits>.
GET reads without incrementing; POST increments and is restricted to the
production site's browser origin. CORS prevents other browser sites from using
the write endpoint, but this is a decorative page counter, not an abuse-proof
analytics system. Automated clients can imitate page requests.

`web/visit-counter.js` makes one request per document load. Local previews only
read the current count. Requests time out after five seconds; failures display a
dash and never block the flasher, firmware downloads or USB tools.

To redeploy using an authorized Cloudflare account, run `npx wrangler deploy`
from `services/visit-counter/`. Preserve the Worker name, Durable Object class,
binding, migration history and named instance to retain the count. No token or
account credentials are included in the public website or repository.

The Node tests use actual SQLite for concurrent increment and persistence
checks, and exercise CORS, routing, client formatting and failure handling.
