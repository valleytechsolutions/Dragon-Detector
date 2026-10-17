# Dragon Detector 0.13.11 — simple clock settings and themed pop-ups

Time and Date now have separate tabs on all four touch-board profiles. Large hour/minute/AM-PM wheels set the time; month/day/year wheels set the date. Date-only saves preserve the live ticking time, time-only saves preserve the current date, and shorter months/leap years constrain the day. A new unset clock requires confirmation of both tabs before saving. Cancel leaves the clock unchanged.

The Sync tab shows the running clock, USB instructions and a separate UTC-offset editor. An unset-clock prompt appears after startup and closes when USB sync succeeds. Detection notices, confirmation dialogs and errors use the active theme's background, text and accents; light themes and ASCII styling are included. Radar family colors remain unchanged.

The web flasher now checks the computer's current time and UTC offset once a minute while connected and idle. It corrects missing time, drift and timezone changes without interrupting transfers. Users can turn these checks off; manual web clock edits pause them. The existing automatic post-flash sync and direct RTC verification remain in place. Lite's direct clock diagnostic is now permitted by its compact command filter.

USB-only operation does not recover elapsed time after total power loss on a board without an independent powered RTC. Keep the device powered, reconnect the flasher, or set both fields manually. USB power alone is not a time source, and browser sleep can delay periodic checks. Wi-Fi clock recovery remains opt-in and is not enabled on the owner's AMOLED.

All five release profiles retain the 21% reserve guard. The connected AMOLED is updated and visually checked; other profiles are build/package-tested for this release. See [clock setup](CLOCK-SETUP.md) and [validation](VALIDATION.md).
