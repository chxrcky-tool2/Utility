# Changelog

All notable changes to the ChxrCky Utility project will be documented in this file.

## [1.0.2] - Initial Release
------------------------------------
### Added
*   **Real-Time Temporal Step Decoupling:** The tick rate override system dynamically synchronizes with the engine in real-time. Modifying the target tick rate instantly applies the new physics calculation rate without requiring a level restart.
*   **Checkpoint State Toggle Integration:** Added a new configuration toggle for the Checkpoint State Allocator directly within the Display configuration tab for quicker access.
*   **Open-Source Compliance:** Integrated the GNU General Public License v3.0 documentation directly into the Settings tab for full transparency and developer compliance.
*   **Enhanced Interface Navigation:** Enlarged the directional arrows within the Checkpoint State Allocator interface to improve touch-target accuracy and overall mobile usability.
*   **Interface Element Boundary Rendering:** Added complete bounding outlines to interface toggle elements across all tabs to ensure visual consistency and a polished layout.
*   **Dynamic Content Viewport:** Implemented a scrollable interface module within the Global configuration tab. This allows for seamless vertical navigation of extensive parameter lists without overflowing the screen bounds on mobile devices.
*   **Armeabi-v7a Architecture Support:** Expanded platform compatibility to include native 32-bit Android environments. This utility is now fully operational on both Android64 and Android32 architectures.
*   **Multi-Slot Storage Framework:** Implemented a robust multi-slot storage system. Users can now manage, save, and load multiple distinct input sequences simultaneously without overwriting previous data.
*   **Customized Directory Allocation:** Implemented a dedicated file path routing system for sequence storage. All local files are now securely archived in the standard Android directory for streamlined file management and accessibility.
*   **Sequence Buffer Purge Utility:** Introduced a dedicated interface button designed to instantly flush and clear all active input sequences from the recording buffer, allowing for immediate fresh recordings without manually deleting data.
*   **Checkpoint State Telemetry Toggle:** Added an optional visual diagnostic notification that triggers upon checkpoint death. This toggle displays the exact frame sequence regression for debugging purposes and can be easily enabled or disabled within the menu settings.

### Fixed
*   **Checkpoint State Desync Critical:** Resolved a major engine bug where recording from a checkpoint past input index 114 would cause the sequence cursor to incorrectly regress to 113, resulting in severe sequence corruption. Input logging above index 114 is now fully stable.
*   **Audio Playback Interruption Critical:** Resolved an engine anomaly where utilizing the Checkpoint State Allocator would inadvertently halt the background music track. The audio engine now correctly resumes and maintains playback upon state transition.

## [1.0.0] - Initial Release
------------------------------------
### Added
*   Initial public release of the ChxrCky Utility framework.
*   Input Sequence Recorder and Re-enactment.
*   Temporal Step Decoupling.
*   Time-Scale Modifier and Audio Sync.
*   Render Loop Rate Decoupling.
*   Progress Integrity Guard.
*   Discrete Physics Frame Advancer.
