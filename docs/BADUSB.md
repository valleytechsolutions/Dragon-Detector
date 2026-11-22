# BadUSB USB keyboard scripts

Native-USB S3 builds include a USB keyboard script runner alongside the existing
USB serial connection. This is a basic DuckyScript subset, using a **US keyboard
layout**. The host's layout must match. Scripts only run when you press **Run** on
the detector. Selecting, uploading, restarting, or connecting USB never starts one.

## Boards

| Profiles | USB keyboard |
| --- | --- |
| Waveshare 1.75C, 1.75, 1.75-G, 1.85B, 1.43C | Native USB port |
| Spotpear / Guition JC3636W518, including Guition beta | Native USB port |
| M5Stack StopWatch, M5Stack Dial | Native USB port |
| Seeed XIAO S3 + Round Display, LILYGO AMOLED 1.75 | Controller's native USB port |
| Elecrow CrowPanel 2.1 Rotary | Native USB port |
| Waveshare 1.28 Lite | Unavailable through its CH343 USB-to-serial bridge |
| SenseCAP Watcher | Unavailable through its CH342 USB-to-serial bridge |

An S3 chip alone is insufficient: the board's connector must reach its native
USB data pins. The Watcher's GPIO19/20 are also used by its optional GPS interface.
The bridge-only profiles keep their existing USB configuration.

This feature still needs physical keyboard, serial transfer, clock-sync and
reflashing checks on each supported board. A successful build does not verify
the USB connection on hardware.

## Run a script

BadUSB is a separate **keyboard mode**. Normally the detector stays on its
ordinary USB serial port, so flashing, the web flasher and serial tools work as
usual. Keyboard mode lasts until the next restart.

On full touch profiles, open **Utilities → BadUSB**. On Dial, use
**Menu → Tools → BadUSB**. Choose a script and review its preview, then:

1. Press **KEYBOARD** (Dial: **Keyboard mode**). The detector restarts as a USB
   keyboard and reopens BadUSB with the same script selected. Nothing is typed.
2. Press **RUN** to type the script.
3. Press **EXIT** (Dial: **Exit keyboard**) to restart back to USB serial. Any
   reset or power cycle also returns to USB serial.

BadUSB and the wardriving node modes never share the device. While **BISCUIT NODE**
or **JCMK Node** runs, BadUSB refuses to select a script, enter keyboard mode or
run (`Stop the node first`); stop the node first. In keyboard mode, and while a
script runs, the node modes refuse to start, and the restart into keyboard mode
does not resume a node that was remembered; it resumes after you leave keyboard
mode. See [BISCUIT NODE and JCMK Node](NODE-MODES.md).

Use **Stop** or leave the BadUSB page to stop a running script. Every key or chord is released
after sending; disconnect, suspend, send failures and the ten-minute limit also
stop execution. Reconnecting never resumes a stopped script.

For a first check, open an empty text editor on your own computer and select
the built-in **demo.txt**. Enter keyboard mode, then press Run; after three seconds it types
`Dragon Detector USB keyboard test.` and Enter into the focused application.
Use scripts only on computers you own or have permission to test.

## Add scripts

On boards with a mounted card, put `.txt` files in **/dragon/badusb** and refresh
the picker. Other boards store scripts in **/badusb** in internal flash. Storage
is never formatted by this feature. The built-in demo works without writable
storage. The picker shows the demo plus up to 23 saved scripts.

USB upload works with either medium. Close the web Studio's serial connection,
then run:

```sh
python3 -m pip install pyserial
python3 tools/badusb_upload.py /dev/cu.usbmodemYOURPORT hello.txt
# Windows: python tools/badusb_upload.py COM5 hello.txt
```

Names use 1–28 letters, digits, underscores or hyphens followed by `.txt`.
`demo.txt` is reserved. Uploads refuse existing names; use a new name or remove
the old file from the card. Each script is limited to 8 KiB, with at most 256
bytes per line. Validation covers the whole script before any key is sent.
Unsupported commands or invalid text produce a line-numbered error.

Example `hello.txt`:

```text
REM Open an empty text editor before running.
DELAY 3000
STRING Hello from Dragon Detector!
ENTER
```

## Commands

| Command | Behavior |
| --- | --- |
| `REM text` | Comment |
| `STRING text` | Type printable ASCII, preserving spaces after the first separator |
| `STRINGLN text` | Type text, then Enter |
| `DELAY 500` | Wait in milliseconds, from 0 to 60000 |
| `DEFAULT_DELAY 100` / `DEFAULTDELAY 100` | Delay after output commands; added to explicit DELAY |
| `ENTER`, `TAB`, `ESC`, `SPACE`, `BACKSPACE`, `DELETE` | Named key |
| `UP`, `DOWN`, `LEFT`, `RIGHT`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN` | Navigation key; arrow names also accept `ARROW` suffix |
| `F1` through `F12` | Function key |
| `CTRL`, `SHIFT`, `ALT`, `GUI` plus keys | Chord, e.g. `CTRL a`; `CONTROL`, `WINDOWS` and `COMMAND` are aliases |
| `CAPSLOCK`, `NUMLOCK`, `SCROLLLOCK`, `PRINTSCREEN`, `PAUSE`, `INSERT`, `MENU` | Other named keys |

Commands are uppercase. Chords support at most six keys, plus modifiers. A
capital letter adds Shift, so use `CTRL a` for Ctrl+A without Shift. CRLF and LF
line endings are accepted. Unicode, alternate layouts, mouse actions, variables,
loops and `REPEAT` are not supported. Keys are spaced by at least 10 ms, and
execution remains cooperative so the UI can handle Stop during waits and text.

## USB serial and recovery

Normal boots use the S3's hardware USB Serial/JTAG port (`ARDUINO_USB_MODE=1`),
exactly as before BadUSB. Flashing with automatic reset, the web flasher, clock
setup and every serial command work unchanged. Uploading, listing and selecting
scripts happen in this mode.

Keyboard mode is requested by a software restart and stored only in RTC memory.
The detector then starts TinyUSB as a keyboard only: there is no serial port
while it is active. The request is cleared on every boot, so EXIT, the reset
button, a crash or a power cycle all return to USB serial. To flash a detector
that is in keyboard mode, press EXIT or power-cycle it first.

For integrations, serial commands are `badusb status` (reports `mode=serial`),
`badusb list`, `badusb select NAME.txt`, `badusb keyboard` (restart into keyboard
mode with the current selection; it never starts typing), `badusb stop`, and
`badusb cancel`. While a node runs, every command except `badusb status`,
`badusb stop` and `badusb cancel` answers `BADUSB_ERROR Stop the node first`. Uploads use
`badusb upload NAME.txt BYTES`, sequential `badusb chunk OFFSET HEX` messages
(at most 256 decoded bytes each), then `badusb save`. Each successful chunk
acknowledges the new byte offset. Idle uploads expire after 30 seconds. There is
no serial command to start execution: `badusb run` directs the user to the device.

USB references: [Espressif USB API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/usb.html),
[Waveshare Lite hardware](https://docs.waveshare.com/ESP32-S3-LCD-1.28),
[M5Stack Dial](https://docs.m5stack.com/en/core/Dial), and
[Watcher schematic](https://github.com/Seeed-Studio/OSHW-SenseCAP-Watcher/blob/main/Hardware/SenseCAP_Watcher_v1.0_SCH.pdf).
