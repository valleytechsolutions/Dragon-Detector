# Dragon Detector 0.14.12 · Waveshare P4 Knob Beta 2

This prerelease adds the Waveshare ESP32-P4 Knob AMOLED 1.75X as an official Dragon Detector beta hardware option. It uses version 0.14.12 on the device and has its own P4 package, separate from the stable v0.14.12 release.

- Browse the animated launcher and dial ring, with dedicated artwork for Biscuit and JCMK.
- Play MP3 and WAV files from microSD and adjust volume with the knob.
- Record WAV files to microSD and monitor the microphone level.
- Transfer microSD files in USB Disk mode.
- Run passive Wi-Fi and Bluetooth radar or send observations to a Biscuit or JCMK core in the matching node mode.

Portal and Room Motion are pending for the P4. The board has no onboard GPS and this beta has no local GPS wardriving mode. Biscuit record delivery was confirmed on a development preview. Both node modes are included, with independent JCMK record delivery still awaiting a logged check.

Use [the official P4 flasher](https://valleytechsolutions.github.io/Dragon-Detector-Flasher/?board=waveshare_p4_knob) on an ESP32-P4 Knob revision 3.x. Select **Update** on a running Dragon Detector knob with a verified layout. Use **Fresh installation** to replace factory firmware. Fresh installation erases internal storage. [Board guide](WAVESHARE-P4-KNOB-175.md)
