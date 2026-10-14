# Publish the StopWatch firmware to M5Burner

This directory supplies a cover image and instructions. The BIN is stored once in the matching hardware release folder; this is not a proprietary import archive.

1. Open M5Burner and sign in to your M5Stack account.
2. Open **USER CUSTOM → Publish**, following the [official publishing guide](https://docs.m5stack.com/en/uiflow/m5burner/publish).
3. Enter the fields below, select the BIN and cover, review the description and submit. If the exact StopWatch device category is unavailable, check current M5Stack guidance rather than selecting a different board.

| Field | Value |
|---|---|
| Name | Dragon Detector - M5Stack StopWatch |
| Version | 0.2.0 |
| Device | M5Stack StopWatch C152 |
| GitHub | https://github.com/valleytechsolutions/Dragon-Detector |
| Firmware | [StopWatch factory BIN](../../hardware/m5stack-stopwatch/release/Dragon-Detector-StopWatch-v0.2.0-factory-0x0.bin) |
| Flash address | 0x0 |
| Flash size | 16 MB |
| Cover | [cover.png](cover.png) |

Suggested description:

> Passive Wi-Fi/BLE radar for M5Stack StopWatch C152, with candidate-device evidence, anomaly observations, channel monitoring, capture/logging, custom badges and optional tone/vibration feedback. Yellow returns to radar; blue opens/cycles tools. No music player or microphone recorder. RSSI is not measured distance or bearing; candidate signatures do not prove identity or intent. Inspired by ESP32 Marauder by JustCallMeKoko, with acknowledgment to CodeHedge's related community work. Source and licenses are available at the GitHub link. Factory installation replaces stored files/settings.

Use the clean factory image, not a dump of a used device. The Spotpear BIN does not belong in this StopWatch submission. This guide does not mean the firmware has been published, reviewed, or approved by M5Stack.
