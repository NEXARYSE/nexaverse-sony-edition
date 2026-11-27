# NEXAVERSE 0.1.91

- Updated the GTA VI runtime to checkpoint ad94a19, retaining the VIctory-7900XTX renderer fixes, keyboard/controller controls, borderless fullscreen, F3 settings and bottom shader preparation bar.
- Added Open save folder to GTA VI’s three-dot settings; follows the configured save drive.
- Added grouped graphics controls in GTA VI's three-dot settings, including filtering, view distance, shadows, reflections and effects. Rendering choices: native 720p, 720p to 1080p (default), native 1080p, and native 2560x1440.
- Added optional NVIDIA DLSS and two-times Frame Generation controls, gated by installed NVIDIA libraries and detected support. Frame generation defaults off. NVIDIA libraries are not bundled; setup instructions are in resources/nexa-gta/streamline/NVIDIA-SETUP.txt. Native modes use DLAA when DLSS is selected.
- Retained saved shader caches, saves, paging-drive settings, hardware/build logging and the logs shortcut. Unsupported VSync/HDR controls remain disabled.

Experimental: the RTX 4060 Ti / 32 GB test reached gameplay and executed DLSS successfully, with recent samples around 13–14 actual FPS. Some generated frames were observed, but steady two-times output and stable 30 FPS are not verified. High RAM use, material/lighting defects and stutters remain; 16 GB compatibility and long-session stability are not established. This release does not claim those issues are fixed.
