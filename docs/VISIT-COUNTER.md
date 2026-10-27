# Historical flasher visits

The historical counter measures page loads beginning October 16, 2026. The
updated flasher uses a separate [completed-install counter](INSTALL-COUNTER.md)
starting October 26. On October 27, the owner requested a carried-forward starting
total of 2,156. That baseline is stored separately from new measured installs;
the original visit data is preserved.

The existing Worker, Durable Object binding, class and named instance are untouched.
Cached old pages can still call `/visits`. Updated pages use `/installs` instead.
