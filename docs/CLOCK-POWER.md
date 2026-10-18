# Clock and timer power saving

From 0.13.16, all five board profiles pause background Wi-Fi and Bluetooth scanning while the clock, stopwatch or Locksport Timer is displayed. Scanning stays paused for lap history and the Time, Date and Sync settings pages. Lite applies the same behavior to its clock and timer pages, including BOOT-button navigation.

The clock, timer, animations, buttons, touch and USB commands continue working. Running, paused and completed timers all keep scanning paused while their timer page is open. Leaving for the radar or another tool resumes the selected radio mode; a running timer keeps counting.

Entering these pages stops active radio sessions and captures without discarding their collected data. Fox advertising stops too and is not automatically re-enabled. New radio sessions must be started from another screen. Saved themes, clock settings and detection history are retained.

USB time synchronization remains available. If saved-Wi-Fi clock recovery was already enabled, its bounded connection may still run to obtain the time, then disconnect. It does not resume radar scanning while the clock or timer is displayed. This update does not enable Wi-Fi recovery or save a network automatically.

There are no live detections or alerts from new packets during the pause. Old contacts age normally; the radar reacquires nearby devices after scanning resumes. The display and processor remain active, so battery life still depends on brightness, animations and the board. No percentage improvement is claimed without current measurements.

For diagnostics, `scan status` reports `standby=1 enabled=0` while background scanning is paused. The Wi-Fi/BLE listening counters stop advancing. `standby=0 enabled=1` indicates normal scanning has resumed. Run `tools/test_clock_power.py` against an idle connected board to check transitions, USB time readback, timer controls and scan-counter behavior.
