# Changelog
All notable changes to the ChxrCky Macro Bot project will be documented in this file.

## [1.0.5]
### Added
- **ImGui User Interface:** Complete UI overhaul with a toggleable ImGui framework alongside the native Cocos UI for zero-lag playback.
- **Theme Engine:** Added multiple UI themes including Mono, Forest, Neon, and Sunset.
- **Cky Icon:** Added a customizable, draggable bot icon with adjustable sizing.
- **License Info:** Added GPL-3.0 license information directly to the settings menu.
- **New CKY3 format:** Added a new format ( Not literal named .CKY3, it is just a label.), you now can't transfer the old CKY1 and CKY2 into v1.0.5. For CKY1 (1.0.1) and CKY2 (1.0.2) to work, turn frame fixes off.
- **Show trajectory:** My actually best update. Draws the future path. Has a slider for the trajectory to become longer.

## [1.0.4]
### Added
- **Click Packs:** Integrated custom audio click packs (e.g., Viper 8K) with an adjustable volume slider (up to 200%).
- **Import GDR:** Added a direct importer for `.gdr` files, allowing seamless migration of macros recorded in other bots.
- **Input Offset Slider:** Added a dedicated slider to manually adjust input frame offsets for perfect frame-window alignment.
- **Checkpoint Telemetry:** Added an experimental telemetry toggle for debugging frame desyncs.

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
- **Tested Environmental:** Verified working on GD 2.2081 / Geode 5.11.2 (Android).
- **Macro bot:** To record a level, go to practice mode, then after finishing, playback it.
- **CKY format:** 8 Bytes per input ( Releases included )
