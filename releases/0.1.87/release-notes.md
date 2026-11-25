# NEXAVERSE 0.1.87

- Updated the experimental NEXA GTA Xbox runtime foundation for the GTA VI entry, including streamed texture/mip handling, resource bindings, large compute dispatches and supported depth-bounds state.
- Prepare translated shaders and compatible graphics pipelines before startup, with persistent caches for later launches.
- Added a left-to-right shader progress bar with centered stage percentages and the GTA VI logo's purple, pink and orange gradient.
- Preserved NEXA GTA branding and the GTA VI icon, fullscreen borderless launch, tracked keyboard/mouse controls, and F3 access to hidden File/Options/Help settings. Debug text and development/status footers stay hidden.
- Removed the legacy GTA VI renderer selection and software-rendering checkbox. Old saved renderer choices cannot silently switch the hardware runtime.
- Play requires a linked, supported local Xbox executable and a complete runtime package. Diagnostic shader/test flags are cleared before normal launches.

GTA VI remains experimental: loading/menu screens and world scenes have been reached in local tests, but world rendering is incomplete and frame rates remain around 1–2 FPS on the tested PC. Four compute pipelines are still refused there. This update does not make GTA VI fully playable or reproduce complete indoor lighting. First preparation may take several minutes.

Requires the user's own supported Xbox executable and game assets. No game files, game-derived shaders, private caches, saves or credentials are distributed. Runtime source and required license notices accompany this release. Other game runtimes and the existing Discord sign-in recovery remain unchanged.
