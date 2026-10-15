# Axon detection and priority alerts · 0.9.3

Use **Tools → Detectors → Axon priority: ON** (the default). Keep the Axon / TASER family enabled. A first received registered Axon identifier creates the usual radar contact, local history entry and unique Axon alert. Priority allows medium-confidence Axon vendor evidence to alert even when the general alert threshold is set to strong-only. Quiet mode, sound settings, ignored/snoozed addresses, family filters and per-contact cooldowns still apply. Turning priority off restores the general confidence threshold and two-observation / 400 ms alert rule.

The signal identifies **Axon/TASER equipment**, which can include body cameras, Fleet and other products. It does not identify the exact model, owner or recording state.

## Evidence

| Received evidence | Interpretation |
|---|---|
| BLE manufacturer company `034D` | Medium-confidence TASER/Axon vendor evidence, including random addresses |
| Service `FC81`, `FE6B` or `FE6C` | Medium-confidence assigned service, in 16-bit, zero-extended 32-bit or exact Bluetooth Base UUID form; lists and service data are supported |
| Company plus assigned service | Stronger combined signature |
| Public BLE address or Wi-Fi probe/beacon source beginning `00:25:DF` | Medium-confidence Axon vendor evidence |
| Random BLE address with that prefix, or arbitrary UUID containing those digits in AXEOFF's tested position | Weak compatibility clue only; no registered vendor attribution and no priority-alert exemption |

This last distinction matters when comparing with [Colonel Panic's AXEOFF implementation](https://github.com/colonelpanichacks/oui-spy-unified-blue/blob/6e5e2829540925ff1ab5fa6d0d365378818ed56e/src/raw/axeoff.cpp). Its broad UUID substring and address checks can also match unregistered custom UUIDs or random addresses. Dragon Detector displays these as weak clues, rather than silently dropping them or claiming a confirmed camera. Name-only and shared radio-vendor clues also remain weak.

Registered address evidence does not depend on an advertised name or nonempty BLE payload. A complete, bounded manufacturer/service field remains usable if an unrelated trailing field is malformed. A valid Wi-Fi source header remains usable when its SSID/security elements cannot be decoded. Incomplete fields themselves never supply an identifier; rejected payloads remain counted in diagnostics.

## Listening and diagnostics

Normal radar now starts with BLE and alternates 2.2-second BLE and Wi-Fi phases. Wi-Fi's planned 200 ms dwell covers channels 1–11 per slice. BLE requests an 85/100 ms passive window. There are still radio gaps; a short burst can fall entirely outside its listening period. Dedicated Hunt continues to favor its selected radio. This release does not enable simultaneous Wi-Fi/BLE scanning.

Registered Axon observations have reserved packet capacity and are processed before ordinary traffic. USB `status` reports overall drops. `axon status` reports enabled/priority settings, signature counters, alert count, reserved-queue drops and maximum Axon processing lag. These counters reset at reboot; local history persists. `recovered` counts observations whose independent Axon evidence remained usable despite an empty or invalid full payload. `weak` counts broad compatibility matches, not confirmed cameras. No queue can guarantee reception under unlimited traffic.

USB commands:

```
axon status
axon priority on
axon priority off
```

For a field comparison, leave both detectors listening normally and record AXEOFF's match type/identifier and Dragon Detector's `axon status` plus history. A beep alone cannot establish that the devices received the same packet. No real Axon body camera was available for controlled validation of 0.9.3; see [the validation record](VALIDATION.md).
