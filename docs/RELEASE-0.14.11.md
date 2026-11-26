# Dragon Detector 0.14.11 · Waveshare 1.85B radar touch

On the Waveshare ESP32-S3-Touch-LCD-1.85B, the radar's TARGETS and TOOLS
buttons are taller and sit higher on the round display. The contact summary and
tracking text remain clear of the controls. The owner flashed a 0.14.10 preview
with this change and confirmed both buttons respond reliably, including near
their lower edges, and that the layout looks clean.

This release includes the full [0.14.10 feature set](RELEASE-0.14.10.md):
scanned Wi-Fi network selection on full-interface boards, reviewed Marauder
radio updates, framed serial file download on SD-capable boards and the 410 ×
502 Waveshare AMOLED 2.06 port. Existing security tools, wardrive and node
modes, themes, clocks and saved-file workflows remain in the firmware. Update
the exact matching board profile to preserve settings and files.

The 1.85B button layout is the only board-specific code change. All 14 stable
profiles and the separate Guition beta are built from the current source. The
compact Dial and Waveshare Lite keep their own menus. The 2.06 port and the
new radio behavior still need physical acceptance checks. A successful build
of another board does not establish that its peripherals work on hardware.

The companion site's stable-board installer now checks the official release
catalog when a board is selected and loads the matching published manifest and
firmware files. This lets the companion installer follow future GitHub Releases
after the companion site receives this code update. The P4 Knob and Guition
beta retain their separate package paths.

Validation: 194 JavaScript tests passed; the 1.85B touch and UI host checks
passed; all 15 local firmware profiles built and packaged; the package and
browser manifest validators accepted every board. The 1.85B layout was checked
by the owner on a flashed preview. Other boards were build checked, not
physically retested for this release.
