# Spotpear clock setup · firmware 0.13.9+

Spotpear JC3636W518 uses the ESP32-S3 software clock. In 0.13.9 it retains time through a **powered software restart**. It cannot measure time while completely unpowered. Saved Wi-Fi can recover the correct time after power returns; USB sync also works without Wi-Fi.

## Set it from your computer

1. Open the [Dragon Detector web flasher and Studio](https://valleytechsolutions.github.io/Dragon-Detector/) in desktop **Chrome or Edge**. Use a USB data cable and close other serial monitors.
2. Select **Spotpear / Guition JC3636W518**. If updating, use **Update matching Dragon Detector** for an already compatible installation to preserve settings and files.
3. Leave **Set my local date & time after installation** enabled. With 0.13.10+ the installer closes automatically after flashing and starts clock verification. Keep USB connected until the verified result appears. Older installers require closing the dialog manually. If USB reconnects under a new port, use **Finish clock setup** and choose the running Spotpear.
4. Alternatively, use **Connect detector** in Studio. Connection automatically copies the computer's current date, time and UTC offset. **Sync my local time** repeats this later.
5. Open **Tools → Utilities → Clock** on the device. Studio's clock status should show a valid time and the expected UTC offset. `rtc=0` is normal for this Spotpear profile.

Windows COM numbers vary. Choose the connected Spotpear, not a different ESP32 board. A successful upload alone does not prove the clock was set: wait for the time-readback confirmation.

## Recover automatically using a saved Wi-Fi network

This feature is **off by default**. Firmware does not retrieve your computer's saved Wi-Fi passwords.

1. Connect Spotpear through Studio, then expand **Connected network / IP tools**.
2. Choose one of the eight profile slots. Enter the Wi-Fi name and password, then click **Save profile**. Use a compatible **2.4 GHz** network; an Internet connection and access to NTP are needed. Captive portals are not handled automatically.
3. Click **Use selected slot for clock**. The selected saved profile is now remembered for time recovery. You do not need to click Join first.
4. Click **Clock recovery status**. Allow up to about 40 seconds for the join and time request. A successful attempt reports `result=synced` and a nonzero `last` epoch. **Sync clock now** requests another attempt immediately when radio tools are idle.
5. Disconnect Studio before another app opens the same USB port.

When enabled, the device briefly joins at boot and every six hours. A failed join or time request waits 15 minutes before retrying. Detection pauses during an owned Wi-Fi attempt and resumes afterward. An existing manual network connection stays connected after time sync; a connection opened only for time recovery is closed. Active radio sessions are not interrupted to start automatic recovery.

The firmware uses `time.cloudflare.com` and `pool.ntp.org`. Profiles stay in the device's settings and are not uploaded to the flasher server. Use **Disable clock recovery** to stop automatic attempts; deleting its selected profile also disables recovery.

## Time zones and power loss

| Event | Expected behavior |
|---|---|
| Powered software restart (`system restart`) | Time continues using the ESP32 RTC timer; UTC offset remains saved |
| Complete power loss | Software time is unavailable until Wi-Fi or USB sync succeeds |
| Saved Wi-Fi is unavailable | Device retries later; use USB sync if you need time immediately |
| Travel or daylight-saving change | Click **Sync my local time** again; the saved UTC offset is fixed, not a geographic time-zone rule |
| Pause, OPEN, or clock correction | Stopwatch/locksport durations use a separate monotonic timer; wall-clock correction does not change elapsed attempt time |
| Reboot during an attempt | Elapsed attempts and splits are RAM-only and reset |

In 0.13.11+, open **Settings → Time** for hour/minute/AM-PM wheels, or **Settings → Date** for month/day/year wheels. Date-only edits preserve the ticking time. **Sync → Time zone** changes the UTC offset. With **Keep my clock synchronized while connected** enabled in the flasher, an idle USB connection checks time and timezone once a minute. History timestamps remain UTC.

## Troubleshooting and USB diagnostics

- **Cannot connect:** close other serial tools, choose the correct running device, and use a data cable. Do not leave the board in ROM download mode after flashing.
- **Time is wrong by whole hours:** resync the computer's local time/offset or correct Zone. Check the computer's clock first.
- **`result=failed` / `cancelled`:** verify the saved network, 2.4 GHz availability, Internet/NTP access, and that other radio tools are stopped. Recovery returns to the previous radar state; it does not retry continuously.
- **No Wi-Fi saved yet:** USB sync remains fully usable. Saving a profile and enabling recovery are separate steps.

Read-only commands are `clock status` and `net time status`. `net time profile 0` selects the first saved slot, `net time sync` requests sync, and `net time off` disables recovery. Profile indices in USB commands are **0–7**, corresponding to UI slots **1–8**. `system restart` refuses while a timer or exclusive radio/storage session is active.

Validation: a Spotpear running 0.13.9 passed clock set/readback and powered software-restart retention tests. The owner's Wi-Fi provisioning was deferred, so Internet NTP success and recovery after a physical power cycle have **not** been verified on that device. The software clock has no guaranteed accuracy while unpowered. [ESP-IDF system-time behavior](https://docs.espressif.com/projects/esp-idf/en/v5.3.3/esp32s3/api-reference/system/system_time.html).
