# OpenDroneID core

- Upstream: https://github.com/opendroneid/opendroneid-core-c
- Commit: `849c03adbe0cdd68997a462fafab397edcfe53f3`
- Copied without modification: `libopendroneid/opendroneid.c`, `libopendroneid/opendroneid.h`, `LICENSE`.
- License: [Apache-2.0](LICENSE). Original copyright and SPDX notices remain in both source files. Intel Corporation, OpenDroneID contributors and other named authors retain their rights.
- Dragon Detector supplies its own bounded BLE/Wi-Fi transport framing in `firmware/include/remote_id.h`. Upstream's Wi-Fi transmitter is not compiled.
- `library.json` is the local PlatformIO packaging descriptor. `ODID_DISABLE_PRINTF` excludes optional upstream debug printers.

This dependency retains Apache-2.0; the application's MIT license does not replace it.
