# Historical flasher visits

The historical counter measures page loads beginning October 16, 2026. The
updated flasher uses a separate [completed-install counter](INSTALL-COUNTER.md)
starting October 26. The visit total has not been converted into installs.

The existing Worker, Durable Object binding, class and named instance are untouched.
Cached old pages can still call `/visits`. Updated pages use `/installs` instead.
