# GUI tour

These are CRC-verified logical display captures from the firmware on the connected Waveshare AMOLED. They show the pixels sent to the display, not photographs of the physical panel. The storage screens below are from **0.13.2**; unchanged radar and Attacks examples retain their original version labels.

| Utilities · 0.13.2 | Settings · 0.13.2 | Capture studio · 0.13.2 |
|---|---|---|
| ![Utilities with Saved files](../media/waveshare-utilities-0132.png) | ![Settings with internal capture controls](../media/waveshare-settings-0132.png) | ![Capture studio](../media/waveshare-captures-0132.png) |

- **Tools → Utilities → Saved files** opens the internal-flash capture library on Waveshare AMOLED and M5Stack. There is no SD card menu or card prompt on either board.
- **Settings → Capture studio** opens Wi-Fi/BLE recording controls. **Settings → Saved captures** opens the same capture library. Download saved captures through USB at [Radio tools → Capture library](https://valleytechsolutions.github.io/Dragon-Detector/#radio-tools).
- GATT profiles and BLE event logs remain in BLE Lab; detection history has its own USB CSV export. Saved files is a capture library, not a browser for every private system file.
- Spotpear and XIAO keep **Utilities → SD files** and optional SD CSV/PCAP settings for their actual card slots. They also have the internal-flash capture controls.
- Lite stays on its green radar. It has no utilities, settings, badge or interactive lab pages.

| Valleytech radar · 0.13.1 | Attacks menu · 0.13.0 |
|---|---|
| ![Valleytech radar](../media/radar-valleytech-0131.png) | ![Wi-Fi and BLE Attacks menu](../media/waveshare-attacks-013.png) |

The four full boards provide **Tools → Recon → Attacks**, BLE Lab, notebook, modes and target hunting. Normal menus and badge mode keep background detection running. Explicit lab operations can take ownership of the shared radio; Stop/Home restores listening. RSSI indicates signal strength; radar angle is decorative.

**[Flash your board](FLASHING.md)** · [Hardware feature comparison](HARDWARE.md) · [Six theme previews](THEMES.md) · [Controls](USER-GUIDE.md) · [Validation](VALIDATION.md)
