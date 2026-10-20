# Dragon Detector 0.13.22

This release improves radar selection and Find My Beeper. Detection signatures,
Axon priority, important-device alerts and quiet tracker behavior are unchanged.

## Radar controls

- Press a dot to select its device immediately. A white ring marks the selection;
  the summary shows its family, address suffix and RSSI.
- Tap the summary or double-tap the same dot for details, BLE tools and Hunt.
- Selection captures the device identity at finger-down. Release movement and
  contact-cache reuse cannot substitute a different device.
- The selected dot holds its position for the ten-second summary window; its
  signal value continues updating. Radar angles remain decorative.
- Each retained device receives a distinct RGB565 shade within its type's color
  range. Dots and their leader labels use the same shade on every radar theme.
  Colors remain stable as RSSI and ordering change. A rotated address is a new
  observed identity, not proof of a different physical tracker.
- AirTags and other trackers remain small, quiet dots until selected. Axon retains
  the primary automatic label even when a tracker is selected.

## Find My Beeper

Open **Attacks → BLE attacks → Find My Beeper**, or the entry in **BLE Lab**.

1. Select a fresh tracker and choose **Start Repeating Beeps**, or choose **Beep
   All** in the tracker picker.
2. Beep All snapshots the fresh Find My / Find Hub candidates observed when it
   starts, including candidates beyond the first twenty picker rows. It does not
   add newly arriving addresses to an already-running session.
3. Requests run sequentially through the existing AirTag, Find My accessory and
   DULT services. Scanning resumes for at least three seconds between connections.
   An acknowledged tracker waits at least ten seconds before its next request.
4. The screen shows attempts, acknowledged writes and per-device results.
   Unsupported services and explicit DULT rejections are skipped for the rest of
   that session. Temporary connection/write failures back off for 15, 30, then
   60 seconds. Addresses not heard within thirty seconds wait for fresh reception.
5. **Stop** or **Home** ends repetition and restores scanning. Stop is available
   during a connection attempt and during the scan intervals. A request already
   delivered can still make its tracker sound.

The screen renders up to twenty result rows; USB **Beeper results** returns the
entire session. The target set is bounded by the 384-contact cache. Repetition
does not pair, save bonds or bypass tracker permissions. An acknowledged write
does not establish audible sound; listen to confirm it.

The web BLE Lab provides **Repeat selected tracker**, **Beep All · repeat**,
**Beeper results**, and **Stop / resume radar** on 0.13.22 full profiles.
Older versions retain the single-request button.

USB commands:

```text
lab select MAC TYPE
lab sound repeat
lab sound all
lab beeper status
lab stop
```

`lab sound` still sends one request for compatibility. Repeating sessions reject
other BLE Lab operations until stopped. Starting another radio tool stops the
beeper instead of letting two jobs compete for the radio.

## Hardware

The shared controls apply to all full touch profiles. Waveshare Lite receives
the color updates and retains its button controls; it has no touch or active BLE
Lab. The non-C Waveshare 1.75 and GPS profiles retain their beta status and
[SD/GPS setup instructions](WAVESHARE-AMOLED-175.md). They boot without a
mandatory SD asset pack.

See [validation](VALIDATION-0.13.22.md) for the exact checks performed.
