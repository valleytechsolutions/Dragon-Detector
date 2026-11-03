# Dragon Detector 0.14.2

Adds **High, Medium and Silent** sound levels across supported audio profiles,
adjusts the Waveshare 1.85B speaker output, and removes Hypnotic while preserving
all other saved theme IDs.

## Sound levels

Medium enables audio at a saved application gain of 50; High enables it at 100.
Silent disables audio and sets gain to zero, including previews and hunt sounds.
The full-board Settings and Audio screens, plus the Dial's sound control, use the
three levels. Hunt feedback and automatic family focus remain separate settings.
Devices without configured speaker hardware keep their visual controls. Fresh
1.85B installations start at Medium; other audio profiles keep their previous
High default. Existing saved values are preserved until a level is selected.

The web tools replace the gain slider and separate alert switch with those three
choices. Reading a device never changes its saved gain; an older non-preset gain
stays intact until an explicit save. The established configuration command keeps
older firmware compatible, and the page checks exact saved fields before
reporting success. Preview reads the saved state and rejects Silent, including
older firmware where a muted alert switch alone did not stop previews. Compact
profiles remain outside the web audio controls.

USB adds `tone level silent`, `tone level medium` and `tone level high`.
`tone settings` retains its existing fields and appends the effective `level`.
The existing configuration and tone commands remain supported on their profiles.

## Waveshare 1.85B speaker

The initial no-sound report matched saved settings with alerts disabled and
application gain 10. After enabling audio, Medium and High test tones at the
previous codec setting were audible but too quiet. The 1.85B now uses ES8311
codec setting 75, its unity-gain DAC setting, while application gain controls
Medium and High. This codec adjustment applies only to the 1.85B.

On November 3, 2026, the owner listened to the Medium and High test beeps and
approved the resulting output. The 0.14.2 application was then flashed to that
1.85B while preserving its saved files and settings. Silent readback confirmed
`enabled=0 volume=0 level=silent`, and a preview request returned
`TONE_ERROR output`. Medium was selected afterward and remained enabled at gain
50 after restart. The SD card and both lookup indexes remained ready, and the RTC
was valid. The Audio screen capture was checked for legibility. Retired radar and
clock commands were rejected while the existing Valleytech selection stayed
unchanged.

## Theme changes

Hypnotic is removed from the radar and clock choices, USB theme commands and web
tools. The remaining themes retain their saved numeric IDs. Radar slot 40 and
watch slot 43 stay reserved so later selections do not move to a different design.
Saved selections for the retired theme fall back to a supported default on updated
firmware.

The active library now contains **44 built-in radar themes plus an optional
personal design**. The existing theme gallery is unchanged. Older firmware can
still connect to the web tools when it reports the retired radar ID: Green appears
as an unsaved choice, and the device changes only after an explicit save. A failed
save cannot be reported as successful by substituting a default during readback.

## Validation and release checks

Shared firmware tests cover the three sound levels, true silence, legacy settings,
no-speaker behavior and theme retirement with stable IDs. All **166 browser tests**
passed locally, including invalid-command rejection before USB writes, exact
sound-setting readback, legacy preview guards and explicit theme migration.

Physical audio acceptance applies to the one 1.85B described above. The other
boards were not physically retested for this release. Publication requires the
complete twelve-profile CI build, clean-source package validation and live file
verification; a host test or compile is not a listening test on another board.
The 1.85B's first-supported compatibility gates remain at 0.14.1, and the existing
SD resource pack and partition layouts are unchanged.
