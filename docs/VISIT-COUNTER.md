# Historical flasher visits

The public counter measures page loads beginning October 16, 2026. A separate
[completed-install counter](INSTALL-COUNTER.md) is prepared as of October 26,
but deployment is waiting for Cloudflare Workers access. The visit total has
not been converted into installs.

The existing Worker, Durable Object binding, class and named instance are untouched.
The public page and cached clients can still call `/visits`. Once the new service
and website are deployed, updated pages will use `/installs` instead.
