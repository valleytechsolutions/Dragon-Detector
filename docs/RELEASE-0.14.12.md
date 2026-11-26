# Dragon Detector 0.14.12 · Waveshare 1.85B HOME touch

The Waveshare ESP32-S3-Touch-LCD-1.85B now has a wider HOME control and a larger
touch area on regular screens and the locksport timer. The owner flashed a
preview, checked both locations with a finger and reported that HOME is easy to
tap and the layout looks clean. The 1.85B has no global swipe-back navigation,
so HOME remains available. Other hardware profiles keep their layout.

The full [0.14.11 feature set](RELEASE-0.14.11.md) remains included: the larger
1.85B radar TARGETS and TOOLS buttons, scanned-network selection on full
interface boards, reviewed radio updates, existing security tools, wardrive
and node modes, and the native rectangular Waveshare AMOLED 2.06 port. The
compact Dial and Waveshare Lite retain their compact menus. Update only the
exact matching board profile to preserve settings and files.

The official release flasher and the companion site's stable-board installer
use the verified GitHub Release packages. The companion site checks the
official catalog when a stable board is selected, so it can load this release
without another firmware upload. The P4 Knob and Guition beta remain separate.

The HOME fix was physically checked only on the 1.85B preview. The 2.06 port
and the newer radio behavior still need physical acceptance checks; compiling
other board profiles does not establish their peripheral behavior.

Validation: 194 JavaScript tests passed; the 1.85B touch and UI host checks
passed; all 15 local profiles built and packaged; and the package and browser
manifest validators accepted every board. The live companion installer was
also checked against all 14 stable v0.14.11 factory and update packages before
this release.
