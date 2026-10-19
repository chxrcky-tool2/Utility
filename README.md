## [1.0.2-RELEASE]

### Added
*   **Real-Time Temporal Step Decoupling:** The TPS override system now dynamically synchronizes with the engine in real-time. Modifying the target tick rate (e.g., adjusting to 1440) will instantly apply the new physics calculation rate without requiring a level restart.
*   **StartPos Toggle Integration:** Added a new configuration toggle for the Checkpoint State Allocator directly within the `Display` configuration tab for quicker access.
*   **Open-Source Compliance (GPL-3.0 License):** Integrated the GNU General Public License v3.0 documentation directly into the `Settings` tab for full transparency and developer compliance.
*   **Enhanced UI Navigation:** Enlarged the directional arrows within the StartPos switcher interface to improve touch-target accuracy and overall mobile usability.
*   **UI Element Boundary Rendering:** Added complete bounding outlines to UI toggle elements (such as the Noclip configuration) across all interface tabs to ensure visual consistency and a polished layout.
*   **Dynamic Content Viewport:** Implemented a scrollable interface module within the `Global` configuration tab. This allows for seamless vertical navigation of extensive parameter lists without overflowing the screen bounds on mobile devices.
*   **Armeabi-v7a Architecture Support:** Expanded platform compatibility to include native 32-bit Android environments. This utility is now fully operational on both Android64 and Android32 architectures.
*   **Multi-Slot Macro Save System:** Implemented a robust multi-slot storage framework. Users can now manage, save, and load multiple distinct `.cky` input sequences simultaneously without overwriting previous data, providing a comprehensive archive for gameplay analysis.
*   **Customized Directory Allocation:** Implemented a dedicated file path routing system for macro storage. All `.cky` sequence files are now securely archived in the standard Android directory: `Android/Com.geode.launch/.game/macros` for streamlined file management and accessibility.
*   **Macro Buffer Purge Utility:** Introduced a dedicated interface button designed to instantly flush and clear all active input sequences from the recording buffer, allowing for immediate fresh recordings without manually deleting data.
*   **Checkpoint State Telemetry Toggle:** Added an optional visual diagnostic notification that triggers upon checkpoint death. This toggle displays the exact frame sequence regression (e.g., "restarted from 136 to 113") for debugging purposes, and can be easily enabled or disabled within the menu settings.

### Fixed
*   **Checkpoint State Desync (Critical):** Resolved a major engine bug where recording from a checkpoint past input index 114 would cause the sequence cursor to incorrectly regress to 113, resulting in severe macro corruption. Input logging above index 114 is now fully stable.
*   **Audio Playback Interruption (Critical):** Resolved an engine anomaly where utilizing the Checkpoint State Allocator (StartPos switcher) would inadvertently halt the background music track. The audio engine now correctly resumes and maintains playback upon state transition.
