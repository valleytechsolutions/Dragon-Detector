# Checking a Flipper or Pineapple Pager

Detection needs a recognizable transmission that reaches the detector during its listening window. These ESP32-S3 boards receive legacy BLE advertisements and 2.4 GHz Wi-Fi. They cannot receive 5/6 GHz Wi-Fi, Bluetooth Classic or silent devices. Balanced mode alternates radios; Hunt prioritizes its selected radio.

## Flipper Zero

Enable Bluetooth and leave the Flipper advertising. A connection, firmware configuration or disabled Bluetooth may change its advertising behavior. Detection uses service IDs 3080–3083, company 0E29 and appearance 8600 as applicable; it does not require the visible name to contain “Flipper.” A name-only match is a weak clue. The owner confirmed a live Flipper on the attached Waveshare; this is not a test of every Flipper firmware or connection state.

Open TARGETS, choose the Flipper, and select Hunt signal. Compare signal trend while changing position; body shielding and antenna orientation also change RSSI. Quiet mode keeps reception active but mutes hunt feedback. The speaker button toggles it.

## Wi-Fi Pineapple Pager

The [Pager has 2.4/5/6 GHz radios](https://hak5.org/products/pager). To check its management network, use a 2.4 GHz network and confirm the SSID/channel in its own settings. Passive recon alone may emit no recognizable identifying frames. A custom or hidden SSID can remove the name clue; randomized addresses can remove an address clue.

The detector recognizes observed Pineapple/Hak5 name clues and documented local-address heuristics. These are explicitly weak evidence: a matching name or address is spoofable and does not establish a Pager model. There is no universal “Pineapple manufacturer MAC” added to the firmware. [Official Pager documentation](https://documentation.hak5.org/wifi-pineapple-pager).

If the management network is visible in **Tools → Wi-Fi** but unclassified, open its contact to inspect its broadcast evidence. Once you identify your own network, Watch keeps that address on the radar and Hunt listens on its observed channel. This marks a known contact without inventing a universal device signature. If it changes address, select the new contact.

An intermittent Pineapple local-address clue was observed during Waveshare validation; it was not authenticated as the owner's Pager. Recent-signal retention keeps such fleeting matches visible for up to two minutes with an explicit age. A constant nearby beacon may still be unclassified if it carries no recognized signature.

An absent match is not proof the device is absent. Testing should use ordinary advertisements and management beacons; attack transmissions are unnecessary for this check.
