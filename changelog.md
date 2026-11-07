# Changelog
All notable changes to the ChxrCky Macro Bot project will be documented in this file.

## [1.0.5]
### Added
- **Hitbox trail:** Leaves the player's outer and inner hitboxes behind at every physics step. Adjustable length and RGB + opacity colors for both boxes, in the Cocos menu (Display tab, gear next to the toggle) and in ImGui. Rewinds with the back step. (untested)
- **ImGui User Interface:** Complete UI overhaul with a toggleable ImGui framework alongside the native Cocos UI for zero-lag playback.
- **Theme Engine:** Added multiple UI themes including Mono, Forest, Neon, and Sunset, plus Cat, Vintage, Sky, Lavender and Rose. Ocean got a richer aqua-to-deep-blue palette.
- **Cky Icon:** Added a customizable, draggable bot icon with adjustable sizing.
- **License Info:** Added GPL-3.0 license information directly to the settings menu.
- **Show trajectory:** My actual best update. Draws the future path. Has a slider for the trajectory to become longer.
- **Frame fix velocity:** Frame fixes now restore vertical velocity (both players), not just position and rotation. (untested)
- **Frame fix interval:** New setting to record a fix every N steps (counted in 240-TPS steps). A fix is also always recorded on every input frame. (untested)
- **Fix order:** Fixes are applied before the step's inputs during playback, matching when they were recorded. (untested)
- **CKY4 format:** Introduced the CKY4 format (CKY3 plus velocity). CKY1-CKY3 macros still load (CKY3 fixes skip the velocity part). (untested)
- **Hitbox fallback + diagnostic log:** Added an extra draw call for hitboxes and a `[cky] hitbox diag:` log line. (untested)
- **Custom TPS Slider:** Added a TPS slider ranging from 1 to 6000. (Note: Real-time performance depends on the device; high TPS may cause extreme lag on mobile).
- **HUD Displayer Settings:** Tweaked and improved the on-screen HUD settings for better customizability and display accuracy.

### Fixed
- **Back step:** it now works right after pausing. While playing, a few light "anchor" snapshots are kept per second (no TPS lag), so Back jumps to the last one; after that, stepping forward makes exact one-frame back steps again.
- **TPS slowdown:** The rewind buffer while playing is now off by default (it copied the whole level state every step and capped real TPS), and it is skipped above 240 TPS. (untested)

## [1.0.4]
### Added
- **Click Packs:** Integrated custom audio click packs (e.g., Viper 8K) with an adjustable volume slider (up to 200%).
- **Import GDR:** Added a direct importer for `.gdr` files, allowing seamless migration of macros recorded in other bots.
- **Input Offset Slider:** Added a dedicated slider to manually adjust input frame offsets for perfect frame-window alignment.
- **Checkpoint Telemetry:** Added an experimental telemetry toggle for debugging frame desyncs.
- **New CKY3 format:** Added a new format (Not literal named .CKY3, it is just a label.). You now can't transfer the old CKY1 and CKY2 into v1.0.4. For CKY1 (1.0.1) and CKY2 (1.0.2) to work, turn frame fixes off.

## [1.0.3]
### Added
- **Public Release:** First stable build of the ChxrCky Macro Bot.
- **Macro slots:** Added 20 slots of macro saves, so everytime you save, you just gotta slide the slider.

## [1.0.2]
### Added
- **Frame Stepper:** Implemented a basic frame-stepper with backstep functionality.
- **Macro Management:** Added save, load, and clear macro buttons.
- **License Info:** Added GPL-3.0 license to the source code.
- **CKY2 format:** Tho not literal, CKY2 is here because the space that CKY1 takes is 5 Bytes.
- **Noclip:** Allows for the player to pass through solid blocks and spikes without restarting.

## [1.0.1]
### Added
- **Speedhack:** Implemented manual speedhack text input to allow for ultra-precise values (e.g., 0.03x) instead of standard sliders.
- **UI Tweaks:** Basic layout improvements.
- **CKY1 format:** New format for squeezed releases and inputs, taking 5 Bytes per input and release.

## [1.0.0]
### Added
- **Initial Prototype:** Basic framework for recording and playing back player inputs.
- **Geode Integration:** Successfully hooked into the Geometry Dash physics engine.
- **Tested Environment:** Verified working on GD 2.2081 / Geode 5.11.2 (Android).
- **Macro bot:** To record a level, go to practice mode, then after finishing, playback it.
- **CKY format:** 8 Bytes per input (Releases included).
