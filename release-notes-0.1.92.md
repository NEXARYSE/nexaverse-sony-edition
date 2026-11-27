# NEXAVERSE 0.1.92 — launcher startup hotfix

- Fixed the missing updater dependencies that prevented 0.1.91 from opening. A clean production dependency tree and package lock are now used when building.
- An updater initialization failure now reports an update error while allowing the launcher window to open.
- Preserves the 0.1.91 GTA runtime, graphics settings and save-folder shortcut. No GTA core/shader changes in this hotfix. Saves, caches and account settings are preserved.
- Validated all updater imports inside the actual packaged archive and tested missing-dependency failure handling. The repaired 0.1.91 installation opens to the welcome screen.

If 0.1.91 cannot open, run NEXAVERSE-Startup-Repair.exe or install the full NEXAVERSE-Setup.exe. The small repair restores updater dependencies in the default per-user installation and restarts the launcher; it leaves game data untouched.

GTA VI remains experimental. Stable 30 FPS, complete rendering, long-session stability and 16 GB compatibility are not established. Optional NVIDIA libraries remain separately installed.
