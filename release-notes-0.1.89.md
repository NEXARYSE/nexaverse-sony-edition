# NEXAVERSE 0.1.89

- Replaced the integrated experimental GTA VI core with the checkpointed VIctory-7900XTX-based PC candidate (2a9910e), including the tested shader/binding and texture lifetime fixes.
- Preserved shader caches and save storage; automatic preparation resumes previously used pipelines before launch.
- Default: Series S, 720p internal rendering with the title's temporal upscaler to 1080p. The launcher now offers 720p or native 1080p next to memory setup.
- Retained the GTA logo, fullscreen borderless presentation, F3 menu and keyboard/controller input. Experimental texture-worker and scratch-substitution options stay off.

Known limitations: this remains experimental. The current 32 GB test completed four game minutes without a native crash; late scene samples were roughly 6-9 FPS. Intro stalls, lighting/material defects and high RAM usage remain. Stable 30 FPS, full rendering, long-session stability and 16 GB compatibility are not established. Frame generation is not a substitute for these unfinished performance fixes.

Requires the user's own supported local Xbox executable and assets. No game assets, derived shaders, private caches, save games or credentials are distributed. Matching modified runtime source, launcher source, build instructions and license notices accompany this release.
