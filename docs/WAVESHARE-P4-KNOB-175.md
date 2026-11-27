# Waveshare P4 Knob AMOLED 1.75X

The Waveshare P4 Knob is an official Dragon Detector hardware option on its own beta release channel. Install **0.14.12 Beta 2** using the [official flasher](https://valleytechsolutions.github.io/Dragon-Detector-Flasher/?board=waveshare_p4_knob). It is for the **ESP32-P4 revision 3.x** knob with the 466 × 466 AMOLED, 32 MB flash, microSD and onboard ESP32 wireless coprocessor. [Waveshare hardware documentation](https://docs.waveshare.com/ESP32-P4-Knob-Touch-AMOLED-1.75X)

- **Launcher.** Turn the knob to move through animated cards. Tap a card to open it. Swipe up from the bottom of the radar for the launcher. The same gesture goes back from other screens.
- **Music.** Copy your MP3 or WAV files to `/music` on a FAT32 microSD card. Select a track from Music or SD Files. Turn the knob during playback to adjust volume.
- **Voice Recorder.** Record WAV files to `/recordings` and watch the live input level. Stop recording before turning off the board or transferring files.
- **USB Disk.** Use the native USB orientation to transfer files to and from microSD. Eject the drive on the computer before choosing Done on the knob. The board remounts the card afterward.
- **Radio and nodes.** The onboard ESP32 provides passive Wi-Fi and Bluetooth radar. Biscuit Node and JCMK Node send observations to their respective cores. Use the matching mode for your core. The knob has no local GPS wardriving mode.

**Portal and Room Motion are pending.** The beta uses Espressif’s runtime loaded into the radio coprocessor’s RAM at boot. The separate beta has its own feature list and validation record. [Beta 2 release notes](RELEASE-P4-0.14.12-BETA-2.md)

Use native USB for Update. If the computer only shows a CH343 serial bridge, reverse the USB-C plug at the knob. Update verifies the running board and app slot, then writes only that app slot. Fresh installation erases internal settings, saved artwork and files. The microSD is not part of the flash image.
