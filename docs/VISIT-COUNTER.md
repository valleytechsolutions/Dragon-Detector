# Historical flasher visits

The former counter measured page loads beginning October 16, 2026. On October 26
it was replaced by a separate [completed-install counter](INSTALL-COUNTER.md).
The visit total remains preserved and frozen; it was not converted into installs.

The existing Worker, Durable Object binding, class and named instance are retained
to preserve that history. `/visits` is a compatibility endpoint returning the
frozen total. Current pages use `/installs` and count successful firmware writes.
