# PORTHEON — Ports & Mods

**PORTHEON** is a launcher for the PC ports of **EA SPORTS UFC 1, 2, 3, 4 (PS4), UFC 5 and UFC 6 (PS5)** and **UFC Undisputed 3 (Xbox 360)**, from 6.0 also **Zelda: Ocarina of Time (Switch 2)** and **God of War III Remastered (PS4)**, from 6.1 **Bloodborne** and **inFAMOUS Second Son (PS4)** and any game of yours, created and maintained by **Marcelo Miyake (XAndromedaXR)**. It finds the game, reads your PC and picks the best settings for each game by itself.

> **PORTHEON is free. I do not sell copies of PORTHEON — I only accept donations** from anyone who wants to support the project.
>
> **No game files are included.** You need your own copy of each game.
> This project is not affiliated with or endorsed by Electronic Arts, the UFC, THQ, Sony Interactive Entertainment, Nintendo or Microsoft. *EA SPORTS UFC*, *PlayStation*, *Zelda*, *God of War*, *Bloodborne*, *inFAMOUS* and *Xbox* are trademarks of their owners and are named only to identify the games.

⬇️ **Download:** [latest release](../../releases/latest)

💬 **Discord:** https://discord.gg/FEQ752rUz

*(Versão em português logo abaixo.)*

---

## What's new in PORTHEON Released

- **UFC 4 on the Native Plus engine:** Automatic now runs UFC 4 on the engine of UFC4 Native Plus 1.2.0 (by Abdullah Tech, GPL-2.0), except on an Intel processor, an Intel graphics card or in Parsec mode. If the game closes with an error on it, PORTHEON goes back to the engine from before by itself; Extras > Video engine picks any of them. On an RTX 4060 it ran fights at the same FPS as the engine before.
- **UFC 4 crowd, reduced:** a new "Reduced (25%)" choice between On and Off (Native Plus's crowd patch).
- **UFC 6 new engine and PS5 Pro mode (experimental, off by default):** UFC 6 > Video > "New engine" runs KytyPS5 09eaab2 built by PORTHEON; "PS5 Pro mode" then turns on the game's PS5 Pro profile (on an RTX 4060 at 960x540: 58 FPS in a fight against 44 to 48, with the Pro's shadows). Not in Parsec mode nor with the actions menu. Same save as the usual engine; its shader cache is its own. Tested on one RTX 4060 only.

Everything below came with 6.2, which was never published on its own.

## What's new in 6.2

- **The game showcase (vitrine):** a 16:9 glass panel beside the chosen game, alive like a console's store page. For the UFC games it plays **edits of real fights with their music**, with fighters of each game's era; for the other games, the game's trailer. It is only the clip: no title, no buttons on the picture.
- **With their music:** the videos play with sound at a comfortable volume. **M** or **R3** mutes them, and Settings > Interface > "Video sound" (On / Off) remembers it. Never any sound while a game runs, or while PORTHEON is minimized or behind another window: the video pauses then.
- **New game: Fight Night Champion** (Xbox 360) through the native PC port **FNC Reborn by Whompay** (Apache License 2.0, github.com/whompay/FNCReborn). "Install the port" opens its official installer: you pick YOUR disc image (.iso), it downloads about 2.5 GB of tools and builds the game on your PC in 15 to 60 minutes (about 25 GB free and 16 GB of memory).
- **UFC 6: in-game actions menu (optional, off by default):** Gameplay > "In-game actions menu". Hold LB + RB + Start, or press F10, for screenshot, restart game, quit, restart or shut down the PC.
- **Live cards on the home screen:** **Your PC** (graphics card, processor, the CPU's load over the last minute, a free-memory meter; a click copies your specs for support), **Controllers** (each pad, its type, cable or Bluetooth, battery, and the button you press; Cross makes controller 1 buzz) and **Activity** (your last session: when, how long, whether it ended normally, and the logs one press away). Read only while the home screen is in front, never while a game runs. Settings > Interface can hide them.
- **Status chips** under the game's name (ready, port version, game version, time played), **quick options** under Play (quality, FPS, TV camera, cheats, saves) in glass pop-overs that the controller drives, and a **System logs** pill in the footer (with a dot after a crash). **Square / X** opens the chosen game's logs.
- A light passes over the chosen tile, the showcase changes picture with a short fade, and an error no longer stays in the status line for good: it shows as a notice, then the line goes back to calm facts.
- **About the videos:** PORTHEON does not host, download or ship any video. `videos\youtube.txt` only names public YouTube videos whose owners allow embedding; they play in YouTube's own player under YouTube's Terms of Service, with the creator's title and channel shown, and a video that is removed or no longer embeddable is skipped. No video of anyone ships in the package. The showcase uses Microsoft Edge **WebView2** (the runtime included with Windows 10 and 11); `WebView2Loader.dll` is redistributed under Microsoft's WebView2 SDK licence (`licenses\WebView2-LICENSE.txt`). Without the runtime, the showcase plays a local video if there is one; with no video it does not show.

- **Texture quality** (Low, Medium 4x, High 8x, Ultra 16x) in every UFC, tied to the graphics presets. **Ultrawide (3440x1440)** preset and a 3440x1440 window in UFC 1 to 4 (the engine enlarges the game's picture). **Experimental HDR** in UFC 1 to 4 and 6.
- **UFC 6 on NVIDIA:** on drivers newer than 617.42 PORTHEON applies a fix by itself against the game closing shortly after the start. With 12 GB or more of video memory, the 3D resolution list also has the game's own 4K scale (3200x1800 in fights, much heavier).
- **New covers and GTA VI:** Zelda, God of War III, Bloodborne, inFAMOUS, Fight Night and UFC Undisputed 3 have new covers. GTA VI is in the library with its trailer, marked COMING SOON (nothing to play yet). Each game's background music is gone: the sound of the home screen is the video.
- **First-person camera, through the fighter's eyes,** in UFC 2 and 4 (Gameplay tab; Select or the touchpad cycles it in a fight).
- **Controller and keyboard your way:** a screen with a drawing of the controller to change the buttons per UFC, and a drawn keyboard with your keys.
- **Help center (F1)**, **Achievements with points** and a **ready invitation** on the Online tab's Copy button, to paste in Discord, Steam or any chat.
- UFC 6: a global ReShade no longer loads into the game (it can close the engine at start) and the `icon0.png` error has a clear message. More FPS limits: 75, 100, 165, 180, 280 and 360.
- **Every game has its own look:** the same layout for all, with its own color grading of the art, light, lettering and drawing (Settings > Interface turns it off). **Effects and shadows** (Low to Ultra), **the monitor's rate while the game runs** and **tuning after each session** (UFC 1 to 4) are in the Video tab, with more FPS limits, and **Find all my games** is on the Install screen. **3440x1440 is in UFC 1 to 4's Resolution list** (the engine enlarges the picture), the controller screen **draws the pad you have connected**, and the **games you add** get their own controller and keyboard screens (the keyboard works with any game in front, with PORTHEON open; the controller, in games that read it through SDL and start from PORTHEON).
- **PORTHEON's new online mode in UFC 2 and UFC 3 (in testing):** one PC hosts on port 42230, the other joins by address, and both search for a fight in the game's Online. © 2026 Marcelo Miyake (XAndromedaXR).

## What's new in 6.1

- **New games:** **Bloodborne** (PS4) through its native PC port **bbport-windows** by Supermedo (Mohammed Albarghouthi), built on deadinside28's bbport, the earlier Linux port (GPL-2.0-or-later; source: github.com/Supermedo/bloodborne_pc), and **inFAMOUS Second Son** (PS4) through its native PC port (GPL-2.0-or-later, built on shadPS4: github.com/shadps4-emu/shadPS4; the port's package names neither its author nor its own source address). Both are installed from inside PORTHEON ("Install the port" on the game's tile); bring your own copy of the game. No port, game or game art ships with PORTHEON. Credits and licenses: `games\BLOODBORNE\PORTHEON-README.txt` and `games\INFAMOUS\PORTHEON-README.txt`.
- **Your own games on the home screen:** "ADD GAME" puts any game of yours next to the UFCs: Steam and Epic games, non-Steam shortcuts, emulators (RPCS3, PCSX2, Dolphin, RetroArch and others, with the ROM you pick) and any program or shortcut. PORTHEON starts it, steps out of the way and counts the play time.
- **UFC 1, 3 and 4: live broadcast camera like UFC 6's** (experimental, not yet tested in a fight): in a stand-up fight Select switches between the normal camera and the TV one (UFC 3/4: still cameras high at the cage side with cuts; UFC 1: one TV camera that follows the fight). UFC 3: with the game's Classic camera style; UFC 4: with the Medium, Wide and Tight camera styles (Classic stays normal). Gameplay tab, off by default.
- **UFC 2 online:** a fix for the online mode closing at the login (it now takes its memory as the game maps it; in testing). **More FPS for UFC 2 online** (deferred GPU readbacks). UFC 2 copies of the other regions are accepted, and you get a clear warning when the game's config file is damaged.
- **Fixes from the players' reports (Nov 7-8):** UFC6.exe 1.22.0, UFC5.exe 1.35.0, and PORTHEON's crash handler no longer closes PORTHEON when a program names a thread.
## What's new in 6.0

- **New games:** **Zelda: Ocarina of Time** (Switch 2) on the Lichii emulator (the MDias v4 build, with AMD fixes and Dynamic FPS built in; Frizti v3 as the other choice) or **natively on the PC** through **Ocarina Recomp** (experimental; "Ocarina Setup: recompile for PC" in its page turns your own 1.1.1 copy, a .nup or the extracted exefs and romfs, into a PC program, and PORTHEON opens the finished build by itself; it needs an AVX2 processor and a Vulkan 1.3 card with mesh shaders); and **God of War III Remastered** (PS4, CUSA01623 with update 01.02) through its native PC port gow3_pc. Bring your own game; no game, console key, firmware or shader cache ships.
- **Keyboard:** UFC 1 to UFC 6's controls can be set to any keys (Gameplay > Keyboard), for players without a controller. UFC 5/6 take mouse buttons too (not yet tested in a UFC 1-4 game).
- **Weak PCs:** a 3D resolution of 480p and 360p in UFC 5 (through FSR 2's ratio: the UI stays sharp) and UFC 6, and UFC 6's 720p is now exact (1280x720).
- **Your game's version:** for UFC 1 to UFC 6 and God of War III, the game's page shows the version of your copy and whether its update is in (and, for UFC 5/6, how many of PORTHEON's fixes went in at the last PLAY).
- **Borderless full screen:** every UFC's full screen already was borderless (the game covers the monitor without changing its resolution, and Alt+Tab is instant); the switch now says so (Video tab). UFC Undisputed 3 has it in Configure the port.
- **PORTHEON's colors:** by default the new logo's black and white; purple, octagon red, electric blue, neon green, belt gold, pink and cyan for whoever wants another (Settings > Interface).
- **UFC 6, new options (Gameplay tab):** no sparks, sweat and smoke (it turns off every particle effect, not only the blows': the arena's fireworks, smoke and haze go too); infinite stamina; no damage and no knockout (both for both fighters). All three are off by default; the last two are cheats.
- **Fixes from the players' reports (Nov 3-6):** UFC 4 on every Radeon RX 5000 starts with the effect that stopped the card (f74dbf6a) skipped, and a big draw stuck twice on the same shader is switched off; UFC 6 on RTX 50: pipeline parts off from the first start (NVIDIA 621's compiler crash moved to the PipelinePrefetch thread) and no useless learning; AMD's image-copy crash recognised on every driver; incomplete copies (missing sce_module\libc.prx) and a dedicated card Windows does not show anymore are told clearly, without resetting the settings; the fight's real-time pace, the audio watchdog fix and no frame queued ahead are back on for everyone once.
- **The console look and the new logo:** the home works like a console's: one row of games, the chosen one bigger with its name, its art filling the screen, a white PLAY; an opening like the console's when it turns on, with the new logo and a light crossing it; the game's picture with a small ring while it loads; the new logo on the shortcuts and the taskbar even over an older install (PORTHEON tells Windows to drop the icon it kept). The chosen game plays its own music (from your copy; Zelda and UFC Undisputed 3 have none) and the controller vibrates its own way for the UFCs, Zelda and God of War III (both can be turned off in Settings > Interface).
- **Faster to the game:** PLAY no longer repeats the PC check, file scans and backups every time; with the UFCs PORTHEON closes and only the game stays open; with Zelda and God of War III it stays minimized and idle (nothing drawn, low priority, its memory given back) until the game closes.
- **UFC 2-4:** a 3D resolution of 720p down to 360p for weak PCs (Video tab; not with the game at 2K/4K; not yet tested in these games); UFC 2's new online server (Ranked hub, invites; in testing); new experimental cheats: infinite stamina in UFC 3, no damage in UFC 4.
- **Zelda and God of War III** show their covers even before they are installed; "Ocarina Setup: recompile for PC" opens Ocarina Setup 0.2.1, which comes inside PORTHEON.
- **Everything from 5.2** (never published on its own): the installer, UFC 5's smooth mode, UFC 5 01.032 without fast fights, the compatibility checks and the fuller Discord report.
## What's new in 5.2

- **Installer:** PORTHEON-Setup.exe checks the PC before installing (processor, graphics driver, Vulkan, RAM, disk) and shows which UFCs run on it. It picks the drive with the most free space, resumes if interrupted, makes the shortcuts, and its uninstaller keeps the saves. The zip is still published for players who prefer it.
- **UFC 5 smooth mode by default:** VSync with 60 Hz holds 60 FPS in the menus and a steady 30 in the fight, at the right speed. The maximum-FPS mode is one choice away (UFC 5 > Video), and UFC 5's FPS limit works again (the launcher used to force 120 Hz at every PLAY).
- **UFC 5 01.032:** the game fixes (clock, dynamic resolution, audio watchdog) are found in the player's own eboot; without the clock fix the frame rate is capped at 60, so the fight no longer runs fast.
- **Compatibility:** full CPU checks with clear texts (PS5 games need AVX2/FMA/BMI2), graphics driver shown in "Does this PC run it?", Vulkan 1.3 check, disk and memory warnings, battery/power-saver warnings, compatible mode after two crashes in a row, more rungs in UFC 5's and UFC 6's automatic repair, UFC 3/4 engine rules for Intel CPUs.
- **Reports:** crash dumps of the UFC 5/6 engines (small, user name removed) and the driver, Windows and memory data go into the report for Discord.
- **Smoother:** Steam's overlay and Fossilize Vulkan layers are off in the games; the PC stays awake while playing; priority for the whole session.
- **UFC 5 on AMD:** the referee-entrance compute shader that lost the device (KytyPS5 issue 1885) is skipped.

## What's new in 5.1.1

Crash fixes from the players' reports of November 5-6 (UFC6.exe 1.17.0).

- **UFC 6 on AMD RX 6700 XT / 6750 XT:** the AMD driver closing while it copies a game image (amdvlk64.dll+0x491b5b) was read as a shader compile crash, and PORTHEON could learn the wrong effects. Now nothing is learned, the message says what happened, and the automatic repair goes straight to its strongest step that keeps the picture (no submit queue, no draws prepared ahead).
- **UFC 6 on RTX 50:** effects are now prepared one at a time. With two at once, an RTX 5070 Ti on driver 621.14 still closed while preparing the game's giant compute effects. The first start is a little slower.
- **UFC 6 on Intel Arc:** effects are prepared two at a time instead of four (a B580 closed 3 of 3 times in the Intel driver with four), and the log names the effects, so the next crash can be fixed remotely.
- **UFC 6, mixed game copies:** a copy whose sce_sys folder is one version and whose eboot.bin is another now gets a clear message (use a whole copy of one version).
- **UFC 6, memory error in the game itself** in the middle of play now has its own message instead of a generic error.
- **UFC 5:** "Make a report for Discord" also takes the newest engine log (*_motor.txt), which shows why the card stopped.

## What's new in 5.1

**Graphics that really change**
- **UFC 6, 3D resolution with FSR-style modes:** Video > 3D resolution picks the size the game draws its 3D picture at before it scales it to your screen: Performance 50% (960x540), Balanced 59%, Quality 67%, High 75%, Native 100% (1920x1080), 2K 133% (2560x1440) and 4K 200% (3840x2160). 2K and 4K make the game draw bigger and scale down, for the sharpest picture; they need about 8 and 12 GB of video memory (UFC6.exe steps down by itself on smaller cards). Works on 01.004, 01.005 and 01.006.
- **Presets in every UFC:** Low, Medium, High (1080p), Ultra (2K) and the new Max (4K). On UFC 6 each step is a real 3D size; on UFC 1-4 the game's render size; on UFC 5 the window's size (full screen uses the monitor's).
- **UFC 5 with UFC 6's options** in Video: FPS counter (off, left or right), no shadows, no empty seats behind the cage, and the F9 mode (without the game's FSR 2) as an option.
- **UFC 5 with the game's FSR 2 on again, as on the PS5:** the F9 mode's stretch left a band on one side of the screen flickering black or in colours whenever the game narrowed its picture, and the picture could shake (a player's video: the main menu's 3D picture only on the left part of the screen, the rest flickering black). F9 mode gives more FPS in fights (39 against 33 on a Ryzen 5 5500 + RTX 4060): turn it on in Video if you prefer FPS.
- **UFC Undisputed 3:** its video (resolution, FSR 1, FPS, VSync) is set in its own port; the Video tab now opens it instead of a preset that did nothing.

**Fewer crashes, fixed by themselves**
- **Automatic repair in UFC 6:** when the NVIDIA or AMD driver brings the game down (driver crash, lost device, compiler crash), the next PLAY turns on one more safeguard, up to four steps: the card gets its commands directly and the crash is recorded; safety barriers between compute effects; no submit queue or draws prepared ahead; the lightest picture. A new driver starts over; "Best settings for this PC" resets it. The message after the crash says which step is on.
- **RTX 50 (driver 621) and RX 6000:** two more fighter effects that brought the driver down are skipped; a crash while the game rebuilt its saved effects at start no longer teaches PORTHEON the wrong effects (that kept it closing): the saved effects are rebuilt instead.
- **UFC 5:** the Vulkan Configurator's validation layer is kept out of the game (it left it at 1-4 FPS); a copy that is not 01.031 gets a clear message; the FPS counter also shows when PORTHEON starts the game.

**Saves and updates**
- **Safe update:** the update installs everything or changes nothing. If a file is in use or a copy fails, the folder stays exactly the version you had. It waits up to 3 minutes for a game to close and never touches saves, caches or your tuned files.
- **Saves from an older folder:** UFC 5 and 6 keep their saves inside PORTHEON's folder. The first time a new PORTHEON folder opens, it brings the saves, caches and settings of UFC 5 and 6 from your older PORTHEON folder next to it (copied, never moved).
- **Starter saves that skip the intro:** new for UFC 4 (a profile past the intro and a created fighter, at the first PLAY of each user). UFC 5 and 6 now also put theirs in when the game had left an empty save behind (the old one is kept aside), on 01.005 and 01.006 too.
- **Tip:** UFC 5's walkouts are its heaviest scene; hold X for 1-2 seconds to skip them.

## What's new in 5.0

**A new interface, like a game console**

- **Your account:** the first time, PORTHEON asks you to create your account, with your name in the games, a photo and a status. Each user keeps their own saves.
- **Profile photo:** pick any photo on your PC (PNG, JPG, BMP or GIF). It shows at the top, in "Who is playing?" and on the game's loading screen. The photo stays on your PC.
- **Status:** Online, Busy or Offline, shown as a dot on your photo. Busy is "do not disturb": only error notices show.
- **Profile card:** click your photo at the top to change your status, edit your profile, switch users or add another one.
- **A new opening:** lights rushing in, the logo rising inside a neon octagon and the PORTHEON name. Any key skips it.
- **The game's screen while it loads:** when you press PLAY, the game's own picture shows right away, as on a console, until the game opens. If it takes long, Esc goes back to PORTHEON. You can turn it off in Settings > Interface.
- **More room for the game:** once the game opens, PORTHEON frees the memory it used and steps aside. MFG SUPER MOTION no longer starts at low priority.
- **Cleaner credits:** just the name, the Discord and the GitHub, in a corner.

**A look that is PORTHEON's own**

- **Black and purple, glass and octagons:** frosted glass panels, every corner cut like the octagon of the cage and the logo, the chosen game held by cage brackets, and the game's number huge behind its name, as on a fight poster.
- **Organized as you open it:** Ready to play, To install and System, with every game's cover (installed or not), a sentence that says what Play will do and the controller keys at the bottom. PORTHEON opens on a game you can play.
- **Cinematic, and light:** a new game slides in, screens come up from black, the loading screen closes in with cinema bars. Nothing moves by itself while you are not using it: idle, PORTHEON uses no processor (it used 10% of a core before). Weak PCs, remote desktop and battery saver get the light effects by themselves (Settings > Interface > Visual effects).
- **Its own icon:** PORTHEON shows its logo on the taskbar and gets its own desktop shortcut; each game keeps its own window and icon.

**The games**

- **UFC Undisputed 3 joins PORTHEON:** a new tile with its own native PC port. Choose the folder with the game (default.xex) or with its ISO and press PLAY. "Configure the port" opens its video, controls, saves and mods. The first start prepares the shaders for a few minutes.
- **AMD FSR 1 in every UFC:** Video > AMD FSR 1, on AMD, NVIDIA and Intel cards. UFC 5 and 6 get FSR 1's sharpening (a crisper picture at no noticeable FPS cost: UFC 6 fights at 50-55 FPS with it on); UFC 1-4 also get the upscale (77% to 50%) when the game accepts drawing smaller. DLSS needs the game's own code and cannot be added to these games.
- **Add to the Steam library:** one button puts any game in Steam with its cover, background, logo and icon. It shows in the Library and in Big Picture and starts straight through PORTHEON. Close Steam first.
- **UFC 4 shadows:** a new option turns off the fighters' shadows, the octagon's or all of them, for more FPS on weak cards.

**UFC 6: fewer crashes on every card**

- **RTX 50 (NVIDIA 621 drivers):** the crash while the game prepared its effects came from the driver compiling the game's giant effects many at a time. UFC 6 now prepares them two at a time on RTX 50 cards.
- **AMD RX 6000 (driver 2.0.353):** three more fighter effects that crashed AMD's compiler are skipped, and PORTHEON now learns new ones from a crash log by itself.
- **Every AMD card** now writes in its log which effect was running when the card stopped, so the next fix comes faster.
- **Your PC's memory, used right:** a low, medium or high profile is picked from your RAM and your card's memory. PCs with little memory keep more room for the game; strong PCs let the engine keep everything.
- **New messages that say what happened:** Windows' Smart App Control blocking the game, an effect the engine cannot translate, a fault in the game's own code, memory running out, and sudden closes (the next session then writes a full log).
- **Nothing in the background:** after PLAY, PORTHEON closes and UFC 6's launcher waits at the lowest priority, using almost no memory.

**UFC 5**

- **Fight speed (new):** Video > Fight speed. 100% is the PS5's own pace (the round clock runs 1.7x faster than real time on the console too); the new default is 90%, a little slower, as players asked.
- **A new graphics driver no longer surprises:** the first session after a driver update rebuilds the effects; UFC 5 now notices it, keeps the picture whole while it builds and says so in the log.
- **Memory profile:** PCs with less than 15 GB of RAM translate effects on fewer threads, so the game keeps its memory.
- The game's window shows the UFC 5 cover on the taskbar.
- The black band on the right no longer appears on game versions other than 01.031 (the FSR2 bypass only runs when the resolution fix went in).
- The log now ends with the exit code, and the engine's depth-texture stop is explained in plain words.

## What's new in 4.3.10

**UFC 6**

- **No more slow motion from a wrong version label:** PORTHEON picked its fixes by the version written in the game folder. When that label did not match the game's code, no fix went in and the fight ran in slow motion, with no message. If the fixes for that version do not fit, PORTHEON now tries those of the other supported version.

## What's new in 4.3.9

**UFC 6**

- **Straight to the menu:** a new player's starter save was out of date (agreement screen, then the opening); now the game opens at the title and goes to the main menu.
- **Menus at about 60 FPS instead of 30** (measured 58.8; the fight's speed and FPS do not change). `vblank_hz=60` in UFC6.ini puts the old limit back.
- **Ready to play:** the live broadcast camera (touchpad in a fight) and the fighters and skins with a lock now come on for everyone, once. They stay switches in UFC 6's options. Paid content stays locked.
- **More quality on strong cards:** the first setup of an NVIDIA or current-driver AMD card with 12 GB or more starts at the High preset (1440x810 + FXAA). Everyone else starts where they did. Textures are never lowered by PORTHEON: the presets change the render size.
- **AMD cards on any driver:** a lost device or a driver crash now turns on the full AMD protection on that PC by itself (`amd_safe`), from a player's RX 7600 report. The message says to just play again.

**Every game**

- Everything from 4.3.2 to 4.3.8 stays, including the camera modes and the fix list over the Internet.

## What's new in 4.3.8

**Every game: so the UFCs run on more PCs**

- **Fixes over the Internet:** PORTHEON reads a fix list from its GitHub at each start (`portheon-fixes.txt`). When an error from a player's log is fixed (a shader to skip on one card, a warning about a driver), every PORTHEON gets it at its next start, without a new version, and only what matches its graphics card is applied. The list only carries data (shader codes and texts), never programs. Settings > Updates turns it off.
- **Does this PC run it?** Before a game's first PLAY, and any time in its Game files tab, PORTHEON says in plain words what will run, what will not and why: the processor, the graphics card and its Vulkan, the feature UFC 5 and UFC 6 need (RX 5000 and GTX 10 cards lack it), RAM and video memory, plus the fix list's warnings for this card.
- **Report with one click:** when a game closes with an error, PORTHEON offers to make the report (a .zip on the desktop with the logs and a summary of the PC, without your Windows account's name) and opens PORTHEON's Discord. "Cancel" stops asking.
- **Test versions:** Settings > Updates > "Get test versions". Whoever helps testing gets the test versions (tagged 4.3.8.1, 4.3.8.2...) before everyone else; the next final version still comes after them.

## What's new in 4.3.7

**UFC 6**

- **Prepare UFC 6** (Game files tab): one session that plays itself. The game goes to a CPU-vs-CPU "Fight now" fight, runs it and closes after about 8 minutes; the visual effects (shaders) it built stay saved, so your own first menu and first fight no longer stutter. No need to press anything, and you can use another program meanwhile.
- **A rare stop while the game loads** (an integer division by zero in the game's start, seen once on an RTX 5060 Ti) now starts the game again by itself, once, instead of ending on an error code.
- **Guide to preparing 01.005 / 01.006** in `games\UFC6\COMO-PREPARAR-O-JOGO.txt` (Portuguese, English, Russian): PS5 PKG Tool for the game, PS5 Dump Forge for the 01.006 update. Nothing needs converting: PORTHEON reads the pkg's own files.
- **Compatibility engine (RX 5000 / GTX 10):** when it closes, its log now names the Windows or driver file that failed and the spot inside it (both RX 5700 XT logs stopped inside a driver file at the same spot). Send that log: it is what is needed to make it run on those cards.

**UFC 2**

- **Online without touching the router:** when you host, PORTHEON asks the router to open port 42230 by itself (UPnP) and the Online tab shows the Internet address to give your friend; the port closes when the room ends. A router without UPnP still has the VPN or a port opened by hand.

## What's new in 4.3.6

**PORTHEON**

- **PORTHEON closes while the game runs:** once the game's window has the focus, PORTHEON exits and only the game (with its small launcher) stays open. When you open PORTHEON again it tells how the game ended (an error's explanation, the performance test's result), and if the game is still running it follows it as before (STOP works). To keep PORTHEON open during the game: Settings > Interface.

**UFC 6**

- **Live broadcast camera remade like the UFC Perth broadcast:** the cameras stand high outside the fence and look down on the fight, side-on and on the diagonal (never behind a fighter's back), both fighters head to feet; each one stays put and turns with the fight for 5 to 7 s, then cuts to the next. Same use as before: "Camera style" on Classic, "Impact moments" on No, touchpad in a stand-up fight.

**UFC 2**

- **Online between friends** (Online tab: Offline / Host / Join) (a test build): login, Ultimate Team (fighters and coins), online fights, Ranked Championships and Online Rivals. One player hosts (PORTHEON starts the server on that PC, port 42230) and the friend joins the host's address; the fight's traffic goes through the host's server, so only the host needs the port (or both on the same VPN: Radmin, ZeroTier, Tailscale). The name online is your PORTHEON profile's. Ultimate Team packs, store and leaderboards are not there yet.

**UFC 6** (fixes from the players' logs)

- **RTX 50 cards on NVIDIA's 621 drivers:** the crash moved to the next effects in line; 4 more are skipped (8 in all), and PORTHEON now notes by itself any new effect that crashes the shader compiler (RTX 50, and AMD's 2.0.353 driver), so the next PLAY goes past it. Tested: the menu and the fight run normally with the 8 skipped.
- **Out of memory:** when Windows cannot reserve UFC 6's memory (~14 GB of RAM plus page file, seen on a 16 GB PC), PORTHEON says so and offers to enlarge the page file (it applies after a restart).
- **Compatibility engine (RX 5000 / GTX 10):** when it also closes on a card, the message says so clearly instead of a bare code 321. It stays experimental.

**UFC 5**

- The message for cards without fragment shader barycentrics now names the right Radeons (RX 470/480/570/580 or RX 6000 and newer); the RX 5000 series does not run UFC 5 or UFC 6.

## What's new in 4.3.5

**UFC 6**

- **Versions 01.005 and 01.006 supported**, with every fix of 01.004: the fight at the PS5's pace, no code 321 (dynamic resolution off), Fast and 75% resolution, no blur, no crowd, every fighter and skin unlocked, no slow motion and the live broadcast camera. 01.006's game code is 01.005's; PORTHEON picks the right set of fixes from the game's version. 01.004 keeps working as before.
- **Fight pace, a little closer to real time:** the community's "Real-time game speed v3" now runs together with PORTHEON's pace fix (same switch): when a frame takes longer than the game's step, the round clock no longer loses that time. Same fight, 4 alternating pairs on 01.006: 0.97x -> 1.01x of real time, the same FPS. Both 01.004 and 01.005/01.006.
- **Live broadcast camera remade like a real UFC broadcast:** the main camera stands outside the cage above the fence and looks down on both fighters (~12 s), with cuts to a shot over each fighter's shoulder (~4 s); the cameras stand still and turn to the fight, and the camera no longer drops. Set the game's "Camera style" to Classic and "Impact moments" to No (pause > Settings > Gameplay), then press the touchpad in a stand-up fight.
- **Two players on one PC:** UFC 6's Parsec mode (Online tab) already has a Player 2: the next controller that connects after yours, a friend's through Parsec or a second controller on this PC. The Online tab now says so.
- **No contextual slow motion** (Gameplay tab, on by default): the fight no longer slows down at dodges and counters, even with the game's "Contextual time dilation" on. A one-instruction patch in the game's time-dilation check; same FPS.
- **RTX 50 cards on NVIDIA's 621 drivers:** PORTHEON now skips the 4 main-menu effects that crashed the card's shader compiler in every RTX 50 crash log (RTX 5050 to 5080); the driver never compiles them. Other cards are unchanged.
- **Test this PC's performance** (Game files tab): play a fight for about 2 minutes, close the game during the fight, and PORTHEON shows the fight's FPS and the right preset for your PC.

**Every game**

- **VSync off (Immediate) by default** in every game and every preset, for the most FPS; Mailbox and VSync stay a choice in Video. UFC 5 and 6 switch to it once; a choice made later stays.
- **A clearer, lighter PORTHEON:** the home cards of UFC 5 and 6 say in plain words what matters (the preset, whether your PC is ready, VSync, whether the first fight will stutter), and the opening takes 1.5 s instead of 2.7. PLAY keeps starting the game the light way, as the desktop shortcut does.
- **"Custom" preset:** change by hand a setting the graphics preset sets (resolution, crowd, blur, FXAA) and the preset shows Custom instead of a name that is no longer true. The other settings stay as they are (Ultra + crowd off by hand keeps 1080p).
- **Make a report for Discord** (Game files tab): one .zip on the desktop with the game's newest logs, its settings and a summary of the PC, with your Windows account's name taken out.

## What's new in 4.3.4

**UFC 6**

- **Live broadcast camera like a TV broadcast:** fixed cameras around the cage (side-on, over each fighter's shoulder, wide on the diagonal) that only turn to follow the fight, above the fighters' shoulders, cutting every 3 to 4.6 seconds and always staying on one side of the fight (touchpad, Gameplay tab).
- **Unlock every fighter and skin** (Gameplay tab, off by default): fighters through a community patch, skins through one of PORTHEON's, for UFC 6 01.004.
- **Crowd off** (Extras tab) is now a game patch: the stands are empty and whole, without the old black blocks, and fights run 7-12 FPS faster than with the crowd.
- **VSync** is back as a choice in the present mode, and an **FPS limit** (30, 60, 90, 120, 144, 240 or unlimited, the engine's own limiter). Both start off.
- **Graphics presets one step apart:** Low 960x540 without the crowd, Medium 960x540 (the default on an RTX 4060), High 1440x810 + FXAA, Ultra 1920x1080 + depth of field + FXAA. Measured on the same fight against 4.3.3: every preset equal or slightly faster.
- **Cards without fragment shader barycentrics (AMD RX 5000, NVIDIA GTX 10):** instead of refusing the card, PORTHEON switches by itself to a compatibility engine (`games\UFC6\compat`: the official KytyPS5 a06e356 with PORTHEON's fallback, GPL-2.0, source patch included). Experimental: slower, and it may stop in some scenes.
- Crash fixes from the players' logs: a compute shader the engine cannot translate (late in fights) is skipped, AMD's 2.0.353 driver also skips the shader pair it crashed on, and the engine's threads get 8 MB stacks (an overflow under Wine/Proton).

**UFC 5**

- **VSync** and an **FPS limit** (the engine's virtual vblank: 60, 120, 144 or 200) in the Video tab; every profile starts without VSync.

**UFC 1-4**

- **VSync** is back as a choice in the present mode (Immediate stays the default).

## What's new in 4.3.3

**UFC 6**

- **Parsec mode with 2 players:** PORTHEON's own 2-player engine. With Parsec mode on (Online tab), a second PS5 user ("Player 2") logs in and the next controller that connects is Player 2's: in Parsec, your friend's controller. UFC6.exe builds it on your PC from the game's engine (`bin\UFC 6 Parsec.exe`, every byte it changes is checked first), so the download has no second engine. The hooks are those of Danteh3-lab's kytyps5-2player-patch (GPL-2.0), ported to PORTHEON's engine with the code and its data in separate read+execute and read+write sections. Connect your own controller first.
- **Less delay in Parsec mode:** Parsec mode now presents frames at once (Immediate) instead of queuing one (Mailbox). Without Parsec mode nothing changes.
- **Live broadcast camera, as on TV:** a press of the touchpad switches between the normal camera and the live broadcast camera: cameras around the octagon a little above the fighters, cutting every few seconds, not tied to a fighter (turn it on in the Gameplay tab).
- Crash fixes from the players' logs (UFC6.exe 1.6.1).

**UFC 5**

- **Starts in F9 mode:** the game's FSR2 is off from the start (what the engine's F9 key does), applied once to every install; safe now that the game's dynamic resolution is off (no black band in the fighter select).
- **Fight cameras** (Gameplay tab): each touchpad press moves to the next camera: normal, 3rd person (over the shoulder), live broadcast (a TV director with four shots around the cage and cuts every few seconds) and 1st person.

**Every game**

- **Graphics presets** (Video tab): Low, Medium, High and Ultra set the options each game already has. Automatic, the default, stays PORTHEON's setup for your PC, with the same performance as 4.3.2 (UFC 6 checked: the same engine, command line and game patches).
- **Frame generator (optional):** a switch in every game's Extras tab starts [MFG SUPER MOTION](https://github.com/NODIX-TECH/MFG-SUPER-MOTION/releases/tag/1.0.4) (a separate program by NODIX TECH, not included) with the game, with the player's own MFG settings; without it installed, the switch opens its download page. Generated frames look smoother, but can mark the HUD and fast motion and add a little input delay.
- **A lighter PORTHEON:** while a game runs, PORTHEON takes a below-normal priority and Windows' power-saving mode for itself, so the processor is the game's; and it no longer closes at start on PCs with a faulty Vulkan layer (an overlay's): its graphics-card check runs apart and without layers.

**UFC 1-4**

- **Fight cameras:** UFC 1 has the live broadcast camera; UFC 3 and 4 the four cameras of UFC 5 (switch in the Extras tab; UFC 3 with the Classic camera style); UFC 2 the same four cameras as a cheat (Cheats tab). Select / View / touchpad changes the camera in a stand-up fight.

## What's new in 4.3.2

**UFC 6**

- **Code 321 found and fixed:** players saw it only through PORTHEON. The game's dynamic resolution kept changing its render size in 8-pixel steps, the engine rebuilt its render targets at every step, and on scene changes that lost the graphics card: on the main menu, switching tabs with L1/R1, at the 4th switch every time (RTX 4060, four runs, with any video memory setting); "Quality" made it rarer, but it still happened on the way to a fight. PORTHEON now turns the game's dynamic resolution off, as it does in UFC 5: the picture keeps one size, the render-target rebuilds fell from about 1000 to 23 per session, and the same test went through 40 switches. "Fast" is a fixed 960x540 (the size fights used to drop to), "Quality" the game's own size.
- **Closer to the emulator:** UFC 6 also starts with the engine's own defaults where PORTHEON's choice had no measured gain: no frame built ahead of the graphics card (also less input lag), Mailbox presentation, and the engine's AMD-CPU mode off (it changed nothing on Ryzen). The video memory budget is never above what the engine would take by itself (4.3.1's was above it on every card in the players' logs), and on 8 GB cards it is 70% of the card, so textures are freed before the card fills. These are applied once to existing installs.
- **Less memory:** on PCs with less than 24 GB of RAM or cards of 9 GB or less, the engine keeps less idle scratch memory (about 800 MB less RAM).
- **Lighter menus and walkouts:** the depth-of-field blur is off by default (in the kit selection the graphics card's time per frame went from 20.0 to 14.1 ms and its memory from 6.5 to 4.3 GB). It can be turned back on in the Video tab.
- **New options:** anti-aliasing (FXAA, the engine's own, off by default) and 3D resolution at 0.75x (more FPS in menus and walkouts, softer picture) in the Video tab; "Faster controller" in the Gameplay tab (on by default; off builds one frame ahead, which measured 0-8% more FPS).

**UFC 5**

- **Anti-aliasing (FXAA):** a new switch in the Video tab. The UFC 5 engine has no anti-aliasing of its own, so PORTHEON draws the same FXAA as UFC 6 over every frame, with its own small Vulkan layer that only the UFC 5 engine loads. No measurable FPS cost.
- **No black band on the right:** the game's dynamic resolution narrowed the picture to about 90% and, on this engine, the rest of the screen stayed black in the fighter select, the kit select and the walkouts. PORTHEON now turns the game's dynamic resolution off, so the picture is always drawn whole.
- **Closer to the console:** the crowd, the seats behind the cage and the shadows are drawn again on strong PCs (12+ threads, 7.5+ GB of video memory, 16 GB of RAM). In the 1080p mode of 4.3.1 they no longer cost FPS (28 against 27 FPS in the same fight), and without them the arena behind the cage was black. Weaker PCs keep them off; the crowd can still be changed in the Extras tab.

**PORTHEON**

- **It steps aside while you play:** every game, UFC 5 and UFC 6 included, opens with the focus and the controller; PORTHEON stops its own timers and, while minimized, frees its pictures and memory, and comes back when the game closes.
- **Windows' power throttling (EcoQoS) really stays off for the games:** it was asked for in a way Windows 10 refuses, and a second call on Windows 11 undid the first. The UFC 5 launcher no longer takes F10 from every program when it runs without its own window.

**UFC 6 "code 321"** (from the logs players sent on 2026-10-30):

- **AMD cards:** RX 6000 cards (AMD's driver 2.0.353, the newest AMD makes for them) stopped at the main menu. PORTHEON now turns on the engine's fixes for AMD by itself: on every AMD card the GPU thread makes its own Vulkan calls, and with driver 2.0.353 or older the barriers between compute dispatches come back and the engine's shader loop guard is on. If an AMD card still stops, the next session records in its log what the card was doing.
- **No trophy toast:** it asked for memory in the middle of fights and closed the game on 6 and 8 GB cards (an RTX 3050 6 GB and an RX 7600). Trophies are still earned; `trophy_toasts=1` in `games\UFC6\UFC6.ini` brings the toast back.
- **Clearer messages:** a driver crash, running out of video memory and a game version other than 01.004 now each get their own message instead of the bare "code 321".

## What's new in 4.3.1

Everything that changed from 4.2.0 to 4.3.1 (versions 4.3.0 and 4.3.1):

**UFC 5**

- **UFC 5 is back**, next to UFC 6, each with its own game folder.
- **No more slow motion:** UFC 5 moved its world one fixed step per drawn frame, made for 60 FPS. PORTHEON now changes the game's clock at start, so every frame moves by the real time that passed: fights move at the PS5's pace at any FPS, with no lag, like UFC 6.
- **Much lighter on 1080p and 1440p screens:** the engine told the game it was on a 4K TV, so UFC 5 drew every frame for 4K. Now the game is told the size of your screen, and below 4K it renders its 1080p mode, like a PS5 on a Full HD TV: on a Ryzen 5 5500 + RTX 4060, in a 1280x720 window, the fighter select went from 12-20 to 39-53 FPS and the fight from 24-31 to 31-45, with less video memory used. 4K screens keep the 4K mode.
- **No black band in the fighter and kit selection:** the game's FSR2 is on again by default; the plain stretch that replaced it left the right tenth of the screen black when the game lowered its resolution.
- **The engine runs at High priority**, and the session summary's fight FPS leaves out loadings and cinematics.

**UFC 6**

- **No more slow motion:** fights run at the PS5's pace, 60 steps a second apart from the screen's FPS, and with more FPS (34-40 instead of 29-31 in a fight on a Ryzen 5 5500 + RTX 4060, in a 1280x720 window).
- **New options:** process priority (normal, above normal or high), AMD CPU compatibility and presentation mode (Immediate or Mailbox).
- **Video memory profiles for 4, 5 and 6 GB cards** (part of the data stays in RAM instead of the game closing), a warning on PCs with less than 12 GB of RAM, and, if UFC 6 closes for lack of RAM, PORTHEON offers to set up the page file, as it does for UFC 1-4.

**UFC 5 and UFC 6**

- **They start past the tutorial:** the first start gets a save that is already past the opening tutorial, on any PC; a save you already have is never touched. The first session builds the shaders for your graphics card (it may stutter there; from the second one on it runs smoothly). To install PORTHEON in a new folder without building them again, `games\UFC5\cache-inicial` and `games\UFC6\cache-inicial` take your own caches (see `LEIA-ME.txt` there).
- **The audio watchdog thread**, which kept a whole processor core busy, is off.
- **Parsec mode** (Online section) keeps the controller working with Parsec's window in front, in Mailbox at 60 Hz; in UFC 5 and 6 it is one player at a time: your friend plays in your place.

**UFC 1 to 4**

- **UFC 3 and UFC 4 on the new engine on every card:** in UFC 4, 11% more FPS in the opening scene and fewer stutters, with the fixes for codes 20 and 23 and for the Career. On NVIDIA cards, if the game closes right at the start with it, PORTHEON goes back to the port's own engine by itself.
- UFC 1 to 4 start in PORTHEON's language the first time.

**PORTHEON**

- **New neon interface** and a **performance center** in the style of the PS5: one card each for the processor, the graphics card and the shaders of every game, with what PORTHEON does with them. UFC 5 and UFC 6 are shown as PC ports, like UFC 1-4.
- **No more restarting by itself:** for a moment after any game ends, Enter, Space, the controller's A and Start and the PLAY button do nothing, so a key you were still typing cannot start the game again; and when UFC 1-4 close while you are in another program, PORTHEON stays behind it.
- **A clearer driver message (UFC 1-4):** if the graphics driver lacks a Vulkan extension the engine needs, PORTHEON names it and asks for a driver update, instead of a generic error.
- The new Discord invite.

## Games

| Game | Platform | What you need | Port |
|---|---|---|---|
| EA SPORTS UFC (2014) | PS4 | `CUSA00222`, update 1.08 | native PC port |
| EA SPORTS UFC 2 | PS4 | `CUSA01968`, update 1.13 | native PC port |
| EA SPORTS UFC 3 | PS4 | `CUSA06534`, update 1.14 | native PC port |
| EA SPORTS UFC 4 | PS4 | `CUSA14209` or `CUSA14204`, update 28.00 for firmware 5.05 (the "backport", not the original 6.72+ one) | native PC port |
| EA SPORTS UFC 5 | PS5 | `PPSA03541`, version 01.031 | PC port (KytyPS5 engine) |
| EA SPORTS UFC 6 | PS5 | `PPSA23566`, version 01.004, 01.005 or 01.006 | PC port (KytyPS5 engine) |
| The Legend of Zelda: Ocarina of Time | Switch 2 | `0400486024DEC000` (.nup, or .nsp/.xci with your own keys); version 1.1.1 to recompile it natively | Lichii emulator, or native PC through Ocarina Recomp |
| God of War III Remastered | PS4 | `CUSA01623`, update 01.02 | native PC port (gow3_pc) |
| Bloodborne | PS4 | `CUSA03173` extracted, ideally version 01.09 | native PC port bbport-windows (installed from inside PORTHEON) |
| inFAMOUS Second Son | PS4 | `CUSA00004` extracted and decrypted | native PC port (installed from inside PORTHEON) |

## How to install

1. Download `PORTHEON-x.x.x-Windows-x64.zip` from the [latest release](../../releases/latest).
2. Extract it into a **new, empty folder** (never over an older version, and not inside OneDrive).
3. Open `PORTHEON.exe`. On Linux / Steam Deck, `portheon.sh` opens PORTHEON through Proton or Wine (experimental): UFC 1-4 do not run on Wine/Proton yet, and UFC 5 and 6 have not been tested there.
4. PORTHEON looks for your games on the PC. If it does not find one, open the game, choose **Game files** and pick the folder of your extracted game.
5. Click **PLAY**.

**Already have PORTHEON?** Use the update button at the top of PORTHEON: it installs the new version in the same folder and keeps your saves, caches and settings. If you install by hand in a new folder instead, copy these from the old folder before the first PLAY: `games\UFC5\bin\_SaveData`, `_ShaderCache` and `_PipelineCache`, and `games\UFC6\bin\_SaveData` and `_PipelineCache` (UFC 1-4 keep their saves outside PORTHEON's folder).

**Requirements:** Windows 10 (1903 or newer) or 11, 64-bit; a graphics card with Vulkan 1.3 (NVIDIA, AMD or Intel Arc) and an up-to-date driver. UFC 1-4: an Intel Core 4th generation (Haswell) or AMD Ryzen processor or newer; Intel Core 2nd/3rd generation and AMD FX run them through PORTHEON's compatibility mode (UFC 3 is the most tested on them). UFC 5 and 6: a processor with AVX2, 16 GB of RAM recommended (8 GB may run with a 16-32 GB page file, not tested) and a card newer than the GTX 10 series (the GTX 10 and older cannot run them); for UFC 6, 8 GB of video memory recommended (profiles for 4, 5 and 6 GB cards). UFC 5 also needs the Microsoft Visual C++ 2015-2022 Redistributable (x64); if it is missing, PORTHEON offers to open Microsoft's download page.

## What PORTHEON does

- One window for every UFC: video, gameplay and controller settings written straight into each port.
- Automatic setup for your PC (processor, graphics card, video memory, RAM and monitor), redone when the PC changes.
- Fixes known crashes by itself: after a graphics card stop (code 23) or a game fault (code 3) it changes what caused it for the next PLAY and explains it in words.
- Controllers: PS5, PS4, PS3, Xbox and generic; two controllers are two players in UFC 1-4; Parsec mode for playing with a friend.
- Profiles, automatic save backups, cheats (UFC 4), desktop shortcuts, 16 languages.

## License

PORTHEON's own code is free software under the GNU GPL, version 2 or later. The bundled ports and engines keep their own licenses (GPL-2.0, some GPL-2.0-only parts, GPL-3.0 for the UFC 4 cheats). PORTHEON's source code is published with each release; the sources of PORTHEON's builds of the UFC 1-4 ports, of the Intel Arc engine and of the UFC 6 engine are still being gathered for publication. The `licenses` folder of the package has every license, and `NOTICE.txt`; Zelda's and God of War III's programs (6.0) keep theirs in `games\ZELDA\licenses` and `games\GOW3\licenses`. Bloodborne's and inFAMOUS Second Son's ports (6.1) are not shipped: the player installs them from inside PORTHEON, and their credits, licenses (GPL-2.0-or-later) and source addresses are in `games\BLOODBORNE\PORTHEON-README.txt` and `games\INFAMOUS\PORTHEON-README.txt` (Bloodborne: bbport-windows by Supermedo (Mohammed Albarghouthi), github.com/Supermedo/bloodborne_pc, built on deadinside28's bbport; inFAMOUS: built on shadPS4, github.com/shadps4-emu/shadPS4). UFC 4's Native Plus engine is the unchanged runtime of UFC4 Native Plus 1.2.0 by Abdullah Tech (github.com/abdullah-tech7/UFC4-Native-Plus, GPL-2.0; built on schans-arg's UFC4-PC-Port and GNSB's UFC 3 renderer): its license, notices and source address are in `games\engines\native-plus`.

---

# PORTHEON — Ports & Mods (português)

O **PORTHEON** é um launcher dos ports para PC do **EA SPORTS UFC 1, 2, 3, 4 (PS4), UFC 5 e UFC 6 (PS5)** e do **UFC Undisputed 3 (Xbox 360)**, desde a 6.0 também do **Zelda: Ocarina of Time (Switch 2)** e do **God of War III Remastered (PS4)**, desde a 6.1 do **Bloodborne** e do **inFAMOUS Second Son (PS4)** e de qualquer jogo seu, criado e mantido por **Marcelo Miyake (XAndromedaXR)**. Ele acha o jogo, lê o seu PC e escolhe sozinho as melhores configurações de cada jogo.

> **O PORTHEON é gratuito. Eu não vendo cópias do PORTHEON — aceito apenas doações** de quem quiser apoiar o projeto.
>
> **Nenhum arquivo dos jogos vem junto.** Você precisa da sua própria cópia de cada jogo.
> Este projeto não é afiliado nem endossado pela Electronic Arts, pelo UFC, pela THQ, pela Sony Interactive Entertainment, pela Nintendo ou pela Microsoft. *EA SPORTS UFC*, *PlayStation*, *Zelda*, *God of War*, *Bloodborne*, *inFAMOUS* e *Xbox* são marcas dos seus donos e aparecem só para identificar os jogos.

⬇️ **Download:** [versão mais recente](../../releases/latest)

💬 **Discord:** https://discord.gg/FEQ752rUz

## Novidades do PORTHEON Released

- **UFC 4 no motor Native Plus:** o Automático agora roda o UFC 4 no motor do UFC4 Native Plus 1.2.0 (de Abdullah Tech, GPL-2.0), menos em processador Intel, placa de vídeo Intel e no modo Parsec. Se o jogo fechar com erro nele, o PORTHEON volta sozinho ao motor de antes; em Extras > Motor de vídeo dá para escolher qualquer um. Numa RTX 4060 as lutas rodaram com o mesmo FPS do motor de antes.
- **Torcida reduzida no UFC 4:** nova opção "Reduzida (25%)", entre Ligada e Desligada (o patch de torcida do Native Plus).
- **Motor novo e modo PS5 Pro no UFC 6 (experimentais, desligados por padrão):** UFC 6 > Vídeo > "Motor novo" roda o KytyPS5 09eaab2 compilado pelo PORTHEON; com ele, "Modo PS5 Pro" liga o perfil do PS5 Pro do jogo (numa RTX 4060 em 960x540: 58 FPS na luta contra 44 a 48, com as sombras do Pro). Não vale no modo Parsec nem com o menu de ações. O save é o mesmo do motor de sempre; o cache de shaders é próprio. Testado só numa RTX 4060.

Tudo abaixo veio com a 6.2, que nunca foi publicada sozinha.

## Novidades da 6.2

- **A vitrine dos jogos:** um painel de vidro 16:9 ao lado do jogo escolhido, vivo como a página de um jogo no console. Nos UFCs ela toca **edits de lutas reais com música**, com lutadores da época de cada jogo; nos outros jogos, o trailer do jogo. É só o clipe: sem título e sem botões na imagem.
- **Com a música:** os vídeos tocam com som, num volume agradável. **M** ou **R3** tiram o som, e Configurações > Interface > "Som dos vídeos" (Ligado / Desligado) guarda a escolha. Nunca sai som com um jogo aberto, nem com o PORTHEON minimizado ou atrás de outra janela: o vídeo pausa.
- **Jogo novo: Fight Night Champion** (Xbox 360) pelo port nativo para PC **FNC Reborn by Whompay** (Apache License 2.0, github.com/whompay/FNCReborn). "Instalar o port" abre o instalador oficial: você escolhe a SUA imagem de disco (.iso), ele baixa cerca de 2,5 GB de ferramentas e monta o jogo no seu PC em 15 a 60 minutos (cerca de 25 GB livres e 16 GB de memória).
- **UFC 6: menu de ações no jogo (opcional, vem desligado):** Jogabilidade > "Menu de ações no jogo". Segure LB + RB + Start, ou aperte F10, para captura de tela, reiniciar o jogo, sair, reiniciar ou desligar o PC.
- **Cartões ao vivo na tela inicial:** **Seu PC** (placa de vídeo, processador, o uso da CPU no último minuto, um medidor de memória livre; um clique copia as especificações para pedir ajuda), **Controles** (cada controle, o tipo, cabo ou Bluetooth, bateria e o botão que você aperta; X faz o controle 1 vibrar) e **Atividade** (a sua última sessão: quando, quanto tempo, se terminou normal, e os logs a um toque). Lidos só com a tela inicial na frente, nunca durante um jogo. Dá para esconder em Configurações > Interface.
- **Chips de status** abaixo do nome do jogo (pronto, versão do port, versão do jogo, tempo jogado), **opções rápidas** abaixo do Jogar (qualidade, FPS, câmera de TV, trapaças, saves) em janelinhas de vidro que o controle navega e o botão **Logs do sistema** no rodapé (com um ponto depois de uma falha). **Quadrado / X** abre os logs do jogo escolhido.
- Um brilho passa sobre o jogo escolhido, a vitrine troca de imagem com um fade curto, e um erro não fica mais parado na linha de status: ele aparece como aviso e depois a linha volta a mostrar só informações calmas.
- **Sobre os vídeos:** o PORTHEON não hospeda, não baixa e não distribui nenhum vídeo. O `videos\youtube.txt` só indica vídeos públicos do YouTube cujos donos permitem a incorporação; eles tocam no player do próprio YouTube, nos Termos de Serviço do YouTube, e um vídeo removido ou que deixou de permitir a incorporação é pulado. Nenhum vídeo de ninguém vem no pacote. A vitrine usa o **WebView2** do Microsoft Edge (o runtime que já vem no Windows 10 e 11); o `WebView2Loader.dll` é redistribuído sob a licença do SDK do WebView2 da Microsoft (`licenses\WebView2-LICENSE.txt`). Sem o runtime, a vitrine toca o vídeo local, se houver; sem nenhum vídeo, ela não aparece.

- **Qualidade das texturas** (Baixo, Médio 4x, Alto 8x, Ultra 16x) em todos os UFCs, ligada aos presets gráficos. Preset **Ultrawide (3440x1440)** e janela 3440x1440 nos UFC 1 a 4 (o motor amplia a imagem do jogo). **HDR experimental** nos UFC 1 a 4 e 6.
- **UFC 6 na NVIDIA:** nos drivers acima do 617.42 o PORTHEON aplica sozinho uma correção contra o jogo fechar logo depois de abrir. Com 12 GB ou mais de memória de vídeo, a lista de resolução 3D também tem a escala 4K do próprio jogo (3200x1800 nas lutas, bem mais pesada).
- **Capas novas e GTA VI:** Zelda, God of War III, Bloodborne, inFAMOUS, Fight Night e UFC Undisputed 3 têm capas novas. O GTA VI está na biblioteca com o trailer, marcado EM BREVE (ainda não dá para jogar). A música de fundo de cada jogo saiu: o som da tela inicial agora é o do vídeo.
- **Câmera em primeira pessoa, nos olhos do lutador,** nos UFC 2 e 4 (aba Jogabilidade; Select ou touchpad alterna na luta).
- **Controle e teclado do seu jeito:** tela com o desenho do controle para mudar os botões em cada UFC e teclado desenhado com as suas teclas.
- **Central de ajuda (F1)**, **Conquistas com pontos** e **convite pronto** no botão Copiar da aba Online, para colar no Discord, na Steam ou em qualquer conversa.
- UFC 6: a ReShade global não entra mais no jogo (pode fechar o motor ao abrir) e o erro do `icon0.png` agora tem mensagem clara. Mais limites de FPS: 75, 100, 165, 180, 280 e 360.
- **Cada jogo tem o seu visual:** o mesmo layout para todos, com tratamento de cor da arte, luz, letras e desenho próprios (Configurações > Interface desliga). **Efeitos e sombras** (Baixo a Ultra), **taxa do monitor enquanto o jogo roda** e **ajuste a cada partida** (UFC 1 a 4) ficam na aba Vídeo, com mais limites de FPS, e **Encontrar todos os meus jogos** fica na tela Instalar. **3440x1440 na lista de Resolução dos UFC 1 a 4** (o motor amplia a imagem), a tela do controle **desenha o controle que está conectado** e os **jogos que você adicionar** têm tela de controle e de teclado (o teclado vale em qualquer jogo na frente, com o PORTHEON aberto; o controle, em jogos que leem pelo SDL e abrem pelo PORTHEON).
- **Novo modo online do PORTHEON no UFC 2 e no UFC 3 (em teste):** um PC hospeda na porta 42230, o outro entra pelo endereço, e os dois procuram luta no Online do jogo. © 2026 Marcelo Miyake (XAndromedaXR).

## Novidades da 6.1

- **Jogos novos:** **Bloodborne** (PS4) pelo port nativo para PC **bbport-windows**, de Supermedo (Mohammed Albarghouthi), feito sobre o bbport de deadinside28, o port para Linux que veio antes (GPL-2.0-or-later; código: github.com/Supermedo/bloodborne_pc), e **inFAMOUS Second Son** (PS4) pelo port nativo para PC dele (GPL-2.0-or-later, feito sobre o shadPS4: github.com/shadps4-emu/shadPS4; o pacote do port não informa o autor nem o endereço do código-fonte do próprio port). Os dois são instalados por dentro do PORTHEON ("Instalar o port" no quadro do jogo); traga a sua própria cópia do jogo. Nenhum port, jogo ou arte dos jogos vem com o PORTHEON. Créditos e licenças: `games\BLOODBORNE\PORTHEON-README.txt` e `games\INFAMOUS\PORTHEON-README.txt`.
- **Os seus jogos na tela inicial:** "ADICIONAR" põe qualquer jogo seu ao lado dos UFCs: jogos da Steam e da Epic, atalhos fora da Steam, emuladores (RPCS3, PCSX2, Dolphin, RetroArch e outros, com a ROM que você escolher) e qualquer programa ou atalho. O PORTHEON abre o jogo, sai da frente e conta o tempo de jogo.
- **UFC 1, 3 e 4: câmera de transmissão ao vivo como a do UFC 6** (experimental, ainda não testada numa luta): na luta em pé o Select troca entre a câmera normal e a de TV (UFC 3/4: câmeras paradas no alto, na beira da grade, com cortes; UFC 1: uma câmera de TV que acompanha a luta). No UFC 3, com o estilo de câmera Clássico do jogo; no UFC 4, com os estilos Médio, Aberto e Fechado (o Clássico fica normal). Aba Jogabilidade, vem desligada.
- **UFC 2 online:** correção do fechamento no login do online (ele agora reserva a memória do jeito que o jogo mapeia; em teste). **Mais FPS no UFC 2 online** (leituras da GPU adiadas). Cópias do UFC 2 de outras regiões são aceitas, e há um aviso claro quando o arquivo de configuração do jogo está danificado.
- **Correções dos relatórios dos jogadores (7 e 8/11):** UFC6.exe 1.22.0, UFC5.exe 1.35.0, e o tratamento de falhas do PORTHEON não fecha mais o PORTHEON quando um programa dá nome a uma thread.
## Novidades da 6.0

- **Jogos novos:** **Zelda: Ocarina of Time** (Switch 2) no emulador Lichii (a versão MDias v4, com correções para AMD e o Dynamic FPS embutido; a Frizti v3 como outra opção) ou **nativo no PC** pelo **Ocarina Recomp** (experimental; "Ocarina Setup: recompilar para PC" na página do jogo transforma a sua cópia 1.1.1, um .nup ou a pasta extraída com exefs e romfs, num programa de PC, e o PORTHEON abre o jogo pronto sozinho; precisa de processador com AVX2 e placa de vídeo com Vulkan 1.3 e mesh shaders); e **God of War III Remastered** (PS4, CUSA01623 com o update 01.02) pelo port nativo gow3_pc. Traga o seu jogo; nenhum jogo, chave de console, firmware ou cache de shaders vem junto.
- **Teclado:** os controles do UFC 1 ao UFC 6 podem ir para qualquer tecla (Jogabilidade > Teclado), para quem joga sem controle. No UFC 5 e 6 vale botão do mouse também (no UFC 1 a 4 ainda não foi testado num jogo).
- **PC fraco:** resolução 3D de 480p e 360p no UFC 5 (pela razão do FSR 2: a interface continua nítida) e no UFC 6, e o 720p do UFC 6 agora é exato (1280x720).
- **Versão do seu jogo:** do UFC 1 ao UFC 6 e no God of War III, a página do jogo mostra a versão da sua cópia e se a atualização está nela (e, no UFC 5/6, quantas correções do PORTHEON entraram no último JOGAR).
- **Tela cheia sem bordas:** a tela cheia de todos os UFCs já era sem bordas (o jogo cobre o monitor sem trocar a resolução, e o Alt+Tab é na hora); agora a chave diz isso (aba Vídeo). No UFC Undisputed 3 ela fica em Configurar o port.
- **Cores do PORTHEON:** de padrão, o preto e branco do logo novo; roxo, vermelho octógono, azul elétrico, verde neon, dourado cinturão, rosa e ciano para quem quiser outra (Configurações > Interface).
- **UFC 6, opções novas (aba Jogabilidade):** sem faíscas, suor e fumaça (desliga todos os efeitos de partícula, não só os dos golpes: fogos, fumaça e névoa da arena também somem); stamina infinita; sem dano e sem nocaute (as duas valem para os dois lutadores). As três vêm desligadas; as duas últimas são trapaças.
- **Correções dos relatórios dos jogadores (3 a 6/11):** o UFC 4 em toda Radeon RX 5000 começa com o efeito que travava a placa (f74dbf6a) pulado, e um desenho grande que trava duas vezes no mesmo shader é desligado; UFC 6 nas RTX 50: partes de pipeline desligadas desde a primeira vez (a falha do compilador do NVIDIA 621 passou para a thread PipelinePrefetch) e sem aprendizado inútil; o crash de cópia de imagem da AMD é reconhecido em qualquer driver; cópia incompleta (sem sce_module\libc.prx) e placa dedicada que o Windows não mostra mais são avisadas com clareza, sem zerar as configurações; o ritmo real da luta, a correção do watchdog de áudio e nenhum quadro na fila voltam ligados para todos, uma vez.
- **Visual de console e o logo novo:** a tela inicial funciona como a de um console: uma fileira de jogos, o escolhido maior com o nome ao lado, a arte dele na tela inteira, JOGAR em branco; uma abertura como a do console ao ligar, com o logo novo e uma luz passando por ele; a imagem do jogo com um anel pequeno enquanto ele carrega; o logo novo nos atalhos e na barra de tarefas mesmo por cima de uma versão antiga (o PORTHEON avisa o Windows para trocar o ícone que ele guardou). O jogo escolhido toca a própria música (da sua cópia; o Zelda e o UFC Undisputed 3 não têm) e o controle vibra de um jeito para os UFCs, outro para o Zelda e outro para o God of War III (os dois desligam em Configurações > Interface).
- **Mais rápido até o jogo:** o JOGAR não repete mais a checagem do PC, varreduras de arquivos e backups toda vez; nos UFCs o PORTHEON fecha e fica só o jogo aberto; no Zelda e no God of War III ele fica minimizado e parado (sem desenhar, com prioridade baixa e a memória devolvida ao jogo) até o jogo fechar.
- **UFC 2 a 4:** resolução 3D de 720p até 360p para PC fraco (aba Vídeo; não vale com o jogo em 2K/4K; ainda não testada nesses jogos); o servidor online novo do UFC 2 (painel do Ranqueado, convites; em teste); trapaças novas experimentais: stamina infinita no UFC 3, sem dano no UFC 4.
- **Zelda e God of War III** mostram a capa mesmo antes de instalar; "Ocarina Setup: recompilar para PC" abre o Ocarina Setup 0.2.1, que já vem dentro do PORTHEON.
- **Tudo da 5.2** (que não chegou a ser publicada sozinha): o instalador, o modo liso do UFC 5, o UFC 5 01.032 sem luta acelerada, as checagens de compatibilidade e o relatório completo para o Discord.
## Novidades da 5.2

- **Instalador:** o PORTHEON-Setup.exe confere o PC antes de instalar (processador, driver de vídeo, Vulkan, RAM, disco) e mostra quais UFCs rodam nele. Escolhe o disco com mais espaço, continua de onde parou se for interrompido, cria os atalhos, e o desinstalador guarda os saves. O zip continua publicado para quem preferir.
- **UFC 5 no modo liso por padrão:** VSync com 60 Hz segura 60 FPS nos menus e 30 estáveis na luta, na velocidade certa. O modo de máximo FPS está a uma escolha (UFC 5 > Vídeo), e o Limite de FPS do UFC 5 voltou a funcionar (o lançador forçava 120 Hz a cada JOGAR).
- **UFC 5 01.032:** as correções do jogo (relógio, resolução dinâmica, watchdog de áudio) são encontradas no eboot do próprio jogador; sem a correção do relógio o FPS fica limitado a 60, e a luta não corre mais acelerada.
- **Compatibilidade:** checagem completa do processador com textos claros (jogos de PS5 precisam de AVX2/FMA/BMI2), driver de vídeo no "Este PC roda?", checagem do Vulkan 1.3, avisos de disco e memória, avisos de bateria/economia de energia, modo compatível depois de dois crashes seguidos, mais degraus na correção automática do UFC 5 e do UFC 6, regras de motor do UFC 3/4 para processadores Intel.
- **Relatórios:** dumps de crash dos motores do UFC 5/6 (pequenos, sem o nome do usuário) e os dados de driver, Windows e memória vão no relatório para o Discord.
- **Mais liso:** as camadas Vulkan do overlay e do Fossilize da Steam ficam desligadas nos jogos; o PC não dorme durante o jogo; prioridade durante a sessão inteira.
- **UFC 5 em AMD:** o shader da entrada do árbitro que derrubava a placa (issue 1885 do KytyPS5) é pulado.

## Novidades da 5.1.1

Correções de crashes dos relatórios dos jogadores de 5 e 6 de novembro (UFC6.exe 1.17.0).

- **UFC 6 em AMD RX 6700 XT / 6750 XT:** o driver da AMD fechando ao copiar uma imagem do jogo (amdvlk64.dll+0x491b5b) era lido como erro ao compilar efeitos, e o PORTHEON podia aprender os efeitos errados. Agora nada é aprendido, a mensagem diz o que aconteceu e a correção automática vai direto para o passo mais forte que mantém a imagem (sem fila de envio e sem desenhos preparados antes).
- **UFC 6 em RTX 50:** os efeitos agora são preparados um de cada vez. Com dois ao mesmo tempo, uma RTX 5070 Ti no driver 621.14 ainda fechava ao preparar os efeitos de cálculo gigantes do jogo. A primeira abertura fica um pouco mais lenta.
- **UFC 6 em Intel Arc:** os efeitos são preparados de dois em dois em vez de quatro (uma B580 fechava 3 de 3 vezes no driver da Intel com quatro), e o log passa a nomear os efeitos, para o próximo travamento ser corrigido pela internet.
- **UFC 6, cópia misturada:** uma cópia com a pasta sce_sys de uma versão e o eboot.bin de outra agora recebe um aviso claro (use uma cópia inteira de uma versão só).
- **UFC 6, erro de memória do próprio jogo** no meio da partida agora tem mensagem própria em vez de erro genérico.
- **UFC 5:** o "Gerar relatório para o Discord" leva também o log mais novo do motor (*_motor.txt), que mostra por que a placa parou.

## Novidades da 5.1

**Gráficos que mudam de verdade**
- **UFC 6, Resolução 3D com modos tipo FSR:** Vídeo > Resolução 3D escolhe o tamanho em que o jogo desenha a imagem 3D antes de ampliar para a sua tela: Desempenho 50% (960x540), Equilíbrio 59%, Qualidade 67%, Alta 75%, Nativa 100% (1920x1080), 2K 133% (2560x1440) e 4K 200% (3840x2160). O 2K e o 4K fazem o jogo desenhar maior e reduzir, para a imagem mais nítida; precisam de cerca de 8 e 12 GB de memória de vídeo (o UFC6.exe desce sozinho em placas menores). Vale na 01.004, 01.005 e 01.006.
- **Presets em todos os UFC:** Baixo, Médio, Alto (1080p), Ultra (2K) e o novo Máximo (4K). No UFC 6 cada passo é um tamanho 3D de verdade; no UFC 1 a 4 o tamanho de desenho do jogo; no UFC 5 o tamanho da janela (em tela cheia vale o do monitor).
- **UFC 5 com as opções do UFC 6** em Vídeo: contador de FPS (desligado, esquerda ou direita), sem sombras, sem cadeiras vazias atrás da grade, e o modo F9 (sem o FSR 2 do jogo) como opção.
- **UFC 5 com o FSR 2 do jogo ligado de novo, como no PS5:** o esticar do modo F9 deixava uma faixa de um lado da tela piscando em preto ou colorido sempre que o jogo estreitava a imagem, e a imagem podia tremer (vídeo de um jogador: a imagem 3D do menu só na parte esquerda da tela, o resto piscando em preto). O modo F9 dá mais FPS na luta (39 contra 33 num Ryzen 5 5500 + RTX 4060): ligue em Vídeo se preferir FPS.
- **UFC Undisputed 3:** o vídeo dele (resolução, FSR 1, FPS, VSync) fica no próprio port; a aba Vídeo agora abre o port em vez de um preset que não fazia nada.

**Menos crashes, corrigidos sozinhos**
- **Correção automática no UFC 6:** quando o driver da NVIDIA ou da AMD derruba o jogo (erro do driver, placa que parou, erro ao compilar efeitos), o próximo JOGAR liga uma proteção a mais, em até quatro passos: a placa recebe os comandos direto e o travamento fica registrado; barreiras de segurança entre os efeitos de cálculo; sem fila de envio e sem desenhos preparados antes; a imagem mais leve. Driver novo começa do zero; "Melhores configurações para este PC" zera também. A mensagem depois do travamento diz qual passo ligou.
- **RTX 50 (driver 621) e RX 6000:** mais dois efeitos dos lutadores que derrubavam o driver são pulados; um travamento enquanto o jogo remontava os efeitos guardados na abertura não ensina mais os efeitos errados ao PORTHEON (era o que fazia o jogo continuar fechando): os efeitos guardados são refeitos.
- **UFC 5:** a camada de validação do Vulkan Configurator fica fora do jogo (deixava a 1-4 FPS); uma cópia que não é a 01.031 recebe um aviso claro; o contador de FPS aparece também quando o PORTHEON abre o jogo.

**Saves e atualizações**
- **Atualização segura:** a atualização instala tudo ou não muda nada. Se um arquivo estiver em uso ou uma cópia falhar, a pasta fica exatamente na versão que você tinha. Ela espera até 3 minutos o jogo fechar e nunca mexe em saves, caches ou arquivos que você ajustou.
- **Saves de uma pasta antiga:** o UFC 5 e o 6 guardam os saves dentro da pasta do PORTHEON. Na primeira vez que uma pasta nova do PORTHEON abre, ela traz os saves, caches e configurações do UFC 5 e 6 da sua pasta antiga do PORTHEON ao lado (copiados, nunca movidos).
- **Save inicial que pula a intro:** novo no UFC 4 (um perfil depois da intro e um lutador criado, no primeiro JOGAR de cada usuário). O UFC 5 e o 6 agora também colocam o deles quando o jogo deixou um save vazio (o antigo fica guardado ao lado), também na 01.005 e 01.006.
- **Dica:** a entrada dos lutadores é a parte mais pesada do UFC 5; segure o X por 1 a 2 segundos para pular.

## Novidades da 5.0

**Uma interface nova, como um videogame**

- **Sua conta:** na primeira vez, o PORTHEON pede para você criar a sua conta, com o seu nome nos jogos, uma foto e um status. Cada usuário continua com os próprios saves.
- **Foto de perfil:** escolha qualquer foto do PC (PNG, JPG, BMP ou GIF). Ela aparece no topo, em "Quem está jogando?" e na tela de carregamento do jogo. A foto fica só no seu PC.
- **Status:** Online, Ocupado ou Offline, com uma bolinha na sua foto. Ocupado é "não perturbe": só os avisos de erro aparecem.
- **Cartão do perfil:** clique na sua foto no canto de cima para trocar o status, editar o perfil, trocar de usuário ou adicionar outro.
- **Abertura nova:** luzes que se encontram no centro, o logo surgindo dentro de um octógono em neon e o nome do PORTHEON. Qualquer tecla pula.
- **A tela do jogo enquanto ele carrega:** ao apertar JOGAR, a imagem do próprio jogo aparece na hora, como num console, até o jogo abrir. Se demorar, Esc volta para o PORTHEON. Dá para desligar em Configurações > Interface.
- **Mais espaço para o jogo:** quando o jogo abre, o PORTHEON libera a memória que usava e sai da frente. O MFG SUPER MOTION não abre mais com prioridade baixa.
- **Créditos mais limpos:** só o nome, o Discord e o GitHub, num canto.

**Um visual só do PORTHEON**

- **Preto e roxo, vidro e octógonos:** painéis de vidro, todos os cantos cortados como o octógono da jaula e do logo, o jogo escolhido preso pelas cantoneiras da jaula e o número do jogo gigante atrás do nome, como num pôster de luta.
- **Organizado quando abre:** Prontos para jogar, Para instalar e Sistema, com a capa de todos os jogos (instalados ou não), uma frase dizendo o que o Jogar vai fazer e as teclas do controle no rodapé. O PORTHEON abre num jogo que dá para jogar.
- **Cinematográfico, e leve:** o jogo novo entra deslizando, as telas sobem do preto, a tela de carregamento fecha com faixas de cinema. Nada se mexe sozinho quando você não está usando: parado, o PORTHEON não gasta processador (antes gastava 10% de um núcleo). PCs fracos, área de trabalho remota e economia de bateria ficam com os efeitos leves sozinhos (Configurações > Interface > Efeitos visuais).
- **Ícone próprio:** o PORTHEON aparece com o logo na barra de tarefas e ganha um atalho na área de trabalho; cada jogo fica com a própria janela e o próprio ícone.

**Os jogos**

- **O UFC Undisputed 3 chega ao PORTHEON:** um bloco novo com o port nativo de PC. Escolha a pasta com o jogo (default.xex) ou com a ISO dele e aperte JOGAR. "Configurar o port" abre vídeo, controles, saves e mods. A primeira vez prepara os shaders por alguns minutos.
- **FSR 1 da AMD em todos os UFC:** Vídeo > FSR 1 da AMD, em placas AMD, NVIDIA e Intel. O UFC 5 e o 6 ganham a nitidez do FSR 1 (imagem mais definida sem custo de FPS que se perceba: luta do UFC 6 a 50-55 FPS com ela ligada); o UFC 1 a 4 ganha também a ampliação (77% a 50%) quando o jogo aceita desenhar menor. O DLSS precisa do código do próprio jogo e não dá para colocar nestes jogos.
- **Adicionar à biblioteca da Steam:** um botão coloca qualquer jogo na Steam com capa, fundo, logo e ícone. Ele aparece na Biblioteca e no Big Picture e abre direto pelo PORTHEON. Feche a Steam antes.
- **Sombras no UFC 4:** uma opção nova desliga a sombra dos lutadores, a do octógono ou todas, para mais FPS em placas fracas.

**UFC 6: menos travamentos em todas as placas**

- **RTX 50 (drivers NVIDIA 621):** o fechamento ao preparar os efeitos vinha do driver compilando os efeitos gigantes do jogo vários de uma vez. Agora o UFC 6 prepara de dois em dois nas RTX 50.
- **AMD RX 6000 (driver 2.0.353):** mais três efeitos dos lutadores que derrubavam o compilador da AMD são pulados, e o PORTHEON passa a aprender sozinho os novos a partir do log de um travamento.
- **Todas as placas AMD** agora gravam no log qual efeito rodava quando a placa parou, para a próxima correção vir mais rápido.
- **A memória do seu PC bem usada:** um perfil baixo, médio ou alto é escolhido pela sua RAM e pela memória da placa. PCs com pouca memória deixam mais espaço para o jogo; PCs fortes deixam o motor guardar tudo.
- **Mensagens novas que dizem o que aconteceu:** o Smart App Control do Windows bloqueando o jogo, um efeito que o motor não consegue traduzir, uma falha no código do próprio jogo, falta de memória e fechamentos de repente (a próxima sessão grava o log completo).
- **Nada em segundo plano:** depois do JOGAR, o PORTHEON fecha e o iniciador do UFC 6 espera na prioridade mais baixa, quase sem usar memória.

**UFC 5**

- **Velocidade da luta (novo):** Vídeo > Velocidade da luta. 100% é o ritmo do próprio PS5 (o relógio do round anda 1,7x mais rápido que o tempo real no console também); o novo padrão é 90%, um pouco mais lento, como os jogadores pediram.
- **Driver de vídeo novo não pega mais de surpresa:** a primeira sessão depois de atualizar o driver remonta os efeitos; o UFC 5 agora percebe isso, mantém a imagem inteira enquanto monta e avisa no log.
- **Perfil de memória:** PCs com menos de 15 GB de RAM traduzem os efeitos com menos threads, e o jogo fica com a memória.
- A janela do jogo mostra a capa do UFC 5 na barra de tarefas.
- A faixa preta à direita não aparece mais em versões do jogo diferentes da 01.031 (o desvio do FSR2 só roda quando a correção de resolução entrou).
- O log agora termina com o código de saída, e a parada do motor por textura de profundidade é explicada em palavras simples.

## Novidades da 4.3.10

**UFC 6**

- **Fim da câmera lenta por versão trocada:** o PORTHEON escolhia as correções pela versão escrita na pasta do jogo. Quando ela não batia com o código do jogo, nenhuma correção entrava e a luta ficava em câmera lenta, sem nenhum aviso. Se as correções daquela versão não servirem, o PORTHEON agora tenta as da outra versão suportada.

## Novidades da 4.3.9

**UFC 6**

- **Direto no menu:** o save inicial do jogador novo estava desatualizado (tela do contrato e depois a abertura); agora o jogo abre no título e vai para o menu principal.
- **Menus a cerca de 60 FPS em vez de 30** (58,8 medidos; a velocidade e o FPS da luta não mudam). `vblank_hz=60` no UFC6.ini volta ao limite antigo.
- **Pronto para jogar:** a câmera de transmissão ao vivo (touchpad na luta) e os lutadores e skins com cadeado agora já vêm ligados para todo mundo, uma vez. Continuam sendo opções do UFC 6. Conteúdo pago continua trancado.
- **Mais qualidade nas placas fortes:** o primeiro ajuste de uma placa NVIDIA, ou AMD com driver atual, de 12 GB ou mais começa no preset Alto (1440x810 + FXAA). Os outros começam como antes. O PORTHEON nunca reduz as texturas: os presets mudam a resolução.
- **Placas AMD em qualquer driver:** um device lost ou um travamento do driver agora liga sozinho a proteção completa para AMD naquele PC (`amd_safe`), a partir do relatório de um jogador com RX 7600. A mensagem diz para só jogar de novo.

**Todos os jogos**

- Tudo da 4.3.2 à 4.3.8 continua, incluindo os modos de câmera e a lista de correções pela internet.

## Novidades da 4.3.8

**Todos os jogos: para os UFC rodarem em mais PCs**

- **Correções pela internet:** o PORTHEON lê uma lista de correções do GitHub dele a cada vez que abre (`portheon-fixes.txt`). Quando um erro do log de um jogador é corrigido (um efeito a pular numa placa, um aviso sobre um driver), todo PORTHEON recebe a correção ao abrir, sem versão nova, e só o que serve para a placa de vídeo dele é aplicado. A lista só traz dados (códigos de efeitos e textos), nunca programas. Dá para desligar em Configurações > Atualizações.
- **Este PC roda?** Antes do primeiro JOGAR de cada jogo, e a qualquer hora na aba Arquivos do jogo, o PORTHEON diz em palavras simples o que vai rodar, o que não vai e por quê: o processador, a placa de vídeo e o Vulkan dela, o recurso que o UFC 5 e o UFC 6 precisam (as RX 5000 e GTX 10 não têm), RAM e memória de vídeo, e os avisos da lista de correções para esta placa.
- **Relatório com um clique:** quando um jogo fecha com erro, o PORTHEON oferece gerar o relatório (um .zip na área de trabalho com os logs e o resumo do PC, sem o nome da sua conta do Windows) e abre o Discord do PORTHEON. "Cancelar" para de perguntar.
- **Versões de teste:** Configurações > Atualizações > "Receber versões de teste". Quem ajuda a testar recebe as versões de teste (marcadas 4.3.8.1, 4.3.8.2...) antes de todo mundo; a próxima versão final continua vindo depois delas.

## Novidades da 4.3.7

**UFC 6**

- **Preparar o UFC 6** (aba Arquivos do jogo): uma sessão que joga sozinha. O jogo vai até uma luta CPU contra CPU em "Lutar agora", roda a luta e fecha em uns 8 minutos; os efeitos visuais (shaders) montados ficam salvos, então o seu primeiro menu e a sua primeira luta não travam mais. Não precisa apertar nada, e dá para usar outro programa enquanto isso.
- **Uma parada rara enquanto o jogo carrega** (divisão inteira por zero no início do jogo, vista uma vez numa RTX 5060 Ti) agora abre o jogo de novo sozinho, uma vez, em vez de terminar num código de erro.
- **Guia para preparar a 01.005 / 01.006** em `games\UFC6\COMO-PREPARAR-O-JOGO.txt` (português, inglês e russo): PS5 PKG Tool para o jogo, PS5 Dump Forge para o update 01.006. Não precisa converter nada: o PORTHEON lê os próprios arquivos do pkg.
- **Motor de compatibilidade (RX 5000 / GTX 10):** quando ele fecha, o log agora diz qual arquivo do Windows ou do driver falhou e em que ponto (os dois logs de RX 5700 XT pararam dentro de um arquivo do driver, no mesmo ponto). Mande esse log: é o que falta para ele rodar nessas placas.

**UFC 2**

- **Online sem mexer no roteador:** ao hospedar, o PORTHEON pede ao roteador para abrir a porta 42230 sozinho (UPnP) e a aba Online mostra o endereço da internet para passar ao amigo; a porta fecha quando a sala acaba. Roteador sem UPnP continua com a VPN ou a porta aberta à mão.

## Novidades da 4.3.6

**PORTHEON**

- **O PORTHEON fecha enquanto o jogo roda:** quando a janela do jogo recebe o foco, o PORTHEON sai e só o jogo (com o seu pequeno inicializador) fica aberto. Quando você abre o PORTHEON de novo, ele conta como o jogo terminou (a explicação de um erro, o resultado do teste de desempenho), e se o jogo ainda estiver aberto ele o acompanha como antes (PARAR funciona). Para deixar o PORTHEON aberto durante o jogo: Configurações > Interface.

**UFC 6**

- **Câmera de transmissão ao vivo refeita como a transmissão do UFC Perth:** as câmeras ficam altas, fora da grade, olhando a luta de cima, de lado e na diagonal (nunca atrás das costas de um lutador), com os dois de corpo inteiro; cada uma fica parada e gira com a luta por 5 a 7 s, depois corta para a próxima. Do mesmo jeito de antes: "Estilo da câmera" em Clássica, "Momentos impactantes" em Não e o touchpad na luta em pé.

**UFC 2**

- **Online entre amigos** (aba Online: Offline / Hospedar / Entrar) (versão de teste): login, Ultimate Team (lutadores e moedas), luta online, Campeonatos Ranqueados e Online Rivals. Um jogador hospeda (o PORTHEON liga o servidor no PC dele, porta 42230) e o amigo entra com o endereço do anfitrião; a luta passa pelo servidor do anfitrião, então só ele precisa da porta (ou os dois na mesma VPN: Radmin, ZeroTier, Tailscale). O nome no online é o do seu perfil do PORTHEON. Pacotes, loja e placares do Ultimate Team ainda não existem.

**UFC 6** (correções dos logs dos jogadores)

- **RTX 50 com drivers NVIDIA 621:** o travamento passou para os próximos efeitos da fila; mais 4 são pulados (8 no total), e o PORTHEON agora anota sozinho qualquer efeito novo que trave o compilador de shaders (RTX 50 e o driver AMD 2.0.353), para o próximo JOGAR passar por ele. Testado: o menu e a luta rodam normalmente com os 8 pulados.
- **Falta de memória:** quando o Windows não deixa o UFC 6 reservar a memória dele (~14 GB de RAM mais arquivo de paginação, visto num PC de 16 GB), o PORTHEON explica e oferece aumentar o arquivo de paginação (vale depois de reiniciar).
- **Motor de compatibilidade (RX 5000 / GTX 10):** quando ele também fecha numa placa, a mensagem explica isso em vez de só "erro 321". Continua experimental.

**UFC 5**

- A mensagem das placas sem baricêntricas no fragment shader agora diz as Radeon certas (RX 470/480/570/580 ou RX 6000 e mais novas); a série RX 5000 não roda o UFC 5 nem o UFC 6.

## Novidades da 4.3.5

**UFC 6**

- **Versões 01.005 e 01.006 suportadas**, com todas as correções da 01.004: luta na velocidade do PS5, sem erro 321 (resolução dinâmica desligada), resolução Rápido e 75%, sem desfoque, sem torcida, todos os lutadores e skins liberados, sem câmera lenta e a câmera de transmissão ao vivo. O código da 01.006 é o mesmo da 01.005; o PORTHEON escolhe as correções certas pela versão do jogo. A 01.004 continua funcionando como antes.
- **Velocidade da luta mais perto do tempo real:** o "Real-time game speed v3" da comunidade agora roda junto com a correção de velocidade do PORTHEON (mesma opção): quando um quadro demora mais que o passo do jogo, o relógio do round não perde mais esse tempo. Mesma luta, 4 pares alternados na 01.006: 0,97x -> 1,01x do tempo real, com o mesmo FPS. Na 01.004 e na 01.005/01.006.
- **Câmera de transmissão ao vivo refeita como numa transmissão do UFC:** a câmera principal fica fora do octógono, acima da grade, olhando de cima os dois lutadores (~12 s), com cortes para um plano por cima do ombro de cada lutador (~4 s); as câmeras ficam paradas e viram para a luta, e a câmera não abaixa mais. Deixe o "Estilo da câmera" do jogo em Clássica e "Momentos impactantes" em Não (pausa > Configurações > Jogabilidade) e aperte o touchpad na luta em pé.
- **Dois jogadores no mesmo PC:** o Modo Parsec do UFC 6 (aba Online) já tem o Jogador 2: o próximo controle que se conectar depois do seu, o do amigo pelo Parsec ou um segundo controle neste PC. A aba Online agora explica isso.
- **Sem câmera lenta contextual** (aba Jogabilidade, vem ligado): a luta não desacelera mais nas esquivas e contra-ataques, mesmo com a "Dilatação temporal contextual" ligada no jogo. Um patch de uma instrução na verificação do próprio jogo; o FPS é o mesmo.
- **RTX 50 com drivers NVIDIA 621:** o PORTHEON agora pula os 4 efeitos do menu que travavam o compilador de shaders da placa em todos os logs de RTX 50 (da 5050 à 5080); o driver nunca chega a compilar esses efeitos. As outras placas não mudam.
- **Testar o desempenho do PC** (aba Arquivos do jogo): jogue uma luta por uns 2 minutos, feche o jogo durante a luta e o PORTHEON mostra o FPS da luta e o preset certo para o seu PC.

**Todos os jogos**

- **VSync desligado (Imediato) por padrão** em todos os jogos e em todos os presets, para o máximo de FPS; Mailbox e VSync continuam como escolha em Vídeo. O UFC 5 e o UFC 6 mudam para ele uma vez; o que você escolher depois fica.
- **PORTHEON mais claro e mais leve:** os cartões da tela inicial do UFC 5 e do UFC 6 dizem em palavras simples o que importa (o preset, se o seu PC está pronto, o VSync, se a primeira luta vai engasgar), e a abertura leva 1,5 s em vez de 2,7. O JOGAR continua abrindo o jogo do jeito leve, como o atalho da área de trabalho.
- **Preset "Personalizado":** se você muda à mão uma opção que o preset gráfico ajusta (resolução, torcida, desfoque, FXAA), o preset mostra Personalizado em vez de um nome que não vale mais. As outras opções ficam como estão (Ultra com a torcida desligada à mão continua em 1080p).
- **Gerar relatório para o Discord** (aba Arquivos do jogo): um .zip na área de trabalho com os logs mais novos do jogo, as configurações e o resumo do PC, sem o nome da sua conta do Windows.

## Novidades da 4.3.4

**UFC 6**

- **Câmera de transmissão ao vivo como na TV:** câmeras paradas em volta da grade (de lado, por cima do ombro de cada lutador e aberta na diagonal), que só giram para acompanhar a luta, acima do ombro dos lutadores, com cortes a cada 3 a 4,6 segundos e sempre do mesmo lado da luta (touchpad, aba Jogabilidade).
- **Liberar todos os lutadores e skins** (aba Jogabilidade, vem desligado): os lutadores com um patch da comunidade, as skins com um do PORTHEON, para o UFC 6 01.004.
- **Torcida desligada** (aba Extras) agora é um patch no jogo: a arquibancada fica vazia e inteira, sem os blocos pretos de antes, e a luta roda de 7 a 12 FPS mais rápido que com torcida.
- **VSync** volta como opção no modo de apresentação, e há **limite de FPS** (30, 60, 90, 120, 144, 240 ou ilimitado, com o limitador do próprio motor). Os dois vêm desligados.
- **Presets gráficos escalonados:** Baixo 960x540 sem torcida, Médio 960x540 (o padrão numa RTX 4060), Alto 1440x810 + FXAA, Ultra 1920x1080 + desfoque de profundidade + FXAA. Medidos na mesma luta contra a 4.3.3: todos iguais ou um pouco mais rápidos.
- **Placas sem baricêntricas (AMD RX 5000, NVIDIA GTX 10):** em vez de recusar a placa, o PORTHEON passa sozinho para um motor de compatibilidade (`games\UFC6\compat`: o KytyPS5 oficial a06e356 com a adaptação do PORTHEON, GPL-2.0, com o patch do código-fonte). Experimental: mais lento e pode travar em algumas cenas.
- Correções dos logs dos jogadores: um shader de compute que o motor não traduz (no fim das lutas) é pulado, o driver 2.0.353 da AMD também pula o par de shaders em que travava, e as threads do motor ganham pilha de 8 MB (estouro no Wine/Proton).

**UFC 5**

- **VSync** e **limite de FPS** (o vblank virtual do motor: 60, 120, 144 ou 200) na aba Vídeo; todos os perfis começam sem VSync.

**UFC 1-4**

- **VSync** volta como opção no modo de apresentação (Imediato continua o padrão).

## Novidades da 4.3.3

**UFC 6**

- **Modo Parsec com 2 jogadores:** o motor de 2 jogadores do próprio PORTHEON. Com o modo Parsec ligado (aba Online), um segundo usuário do PS5 ("Player 2") entra e o próximo controle que conectar é o do Jogador 2: no Parsec, o controle do seu amigo. O UFC6.exe monta esse motor no seu PC a partir do motor do jogo (`bin\UFC 6 Parsec.exe`, conferindo antes cada byte que muda), então o download não traz um segundo motor. Os ganchos são os do kytyps5-2player-patch do Danteh3-lab (GPL-2.0), adaptados ao motor do PORTHEON, com o código e os dados em seções separadas (só execução e só escrita). Ligue o seu controle primeiro.
- **Menos atraso no modo Parsec:** o modo Parsec agora mostra cada quadro na hora (Immediate) em vez de deixar um na fila (Mailbox). Sem o modo Parsec nada muda.
- **Câmera de transmissão ao vivo, como na TV:** um toque no touchpad alterna entre a câmera normal e a de transmissão ao vivo: câmeras em volta do octógono, um pouco acima dos lutadores, com cortes a cada poucos segundos, sem ficar presa a um lutador (ligue na aba Jogabilidade).
- Correções de travamentos dos logs dos jogadores (UFC6.exe 1.6.1).

**UFC 5**

- **Abre no modo F9:** o FSR2 do jogo vem desligado desde o início (o que a tecla F9 do motor faz), aplicado uma vez em toda instalação; seguro agora que a resolução dinâmica do jogo está desligada (sem faixa preta na seleção de lutador).
- **Câmeras da luta** (aba Jogabilidade): cada toque no touchpad passa para a próxima: normal, 3ª pessoa (por cima do ombro), transmissão ao vivo (um diretor de TV com quatro câmeras em volta do octógono e cortes a cada poucos segundos) e 1ª pessoa.

**Todos os jogos**

- **Presets gráficos** (aba Vídeo): Baixo, Médio, Alto e Ultra mudam as opções que cada jogo já tem. Automático, o padrão, continua sendo o ajuste do PORTHEON para o seu PC, com o mesmo desempenho da 4.3.2 (UFC 6 conferido: o mesmo motor, a mesma linha de comando e os mesmos patches do jogo).
- **Gerador de quadros (opcional):** uma chave na aba Extras de cada jogo abre o [MFG SUPER MOTION](https://github.com/NODIX-TECH/MFG-SUPER-MOTION/releases/tag/1.0.4) (programa à parte, da NODIX TECH, não incluído) junto com o jogo, com as configurações que a pessoa fez no próprio MFG; sem ele instalado, a chave abre a página de download. Gerar quadros deixa mais fluido, mas pode marcar o HUD e movimentos rápidos e soma um pouco de atraso no controle.
- **PORTHEON mais leve:** enquanto um jogo roda, o PORTHEON fica com prioridade abaixo do normal e no modo de economia do Windows, deixando o processador para o jogo; e ele não fecha mais ao abrir em PCs com uma camada de Vulkan com defeito (de algum overlay): a leitura da placa de vídeo roda à parte e sem camadas.

**UFC 1-4**

- **Câmeras da luta:** o UFC 1 tem a câmera de transmissão ao vivo; o UFC 3 e o 4, as quatro câmeras do UFC 5 (chave na aba Extras; o UFC 3 com o estilo de câmera Classic); o UFC 2, as mesmas quatro câmeras como cheat (aba Cheats). Select / View / touchpad troca a câmera na luta em pé.

## Novidades da 4.3.2

**UFC 6**

- **Código 321 achado e corrigido:** os jogadores só viam pelo PORTHEON. A resolução dinâmica do jogo trocava o tamanho da imagem em passos de 8 pixels, o motor recriava as imagens de renderização a cada passo, e numa troca de cena isso derrubava a placa de vídeo: no menu principal, trocando de aba com L1/R1, na 4ª troca, todas as vezes (RTX 4060, quatro rodadas, com qualquer ajuste de memória de vídeo); "Qualidade" deixava mais raro, mas ainda acontecia no caminho até a luta. O PORTHEON agora desliga a resolução dinâmica do jogo, como já faz no UFC 5: a imagem fica num tamanho só, as recriações caíram de umas 1000 para 23 por sessão, e o mesmo teste passou de 40 trocas. "Rápido" é 960x540 fixo (o tamanho para o qual a luta descia), "Qualidade" o tamanho do próprio jogo.
- **Mais perto do emulador:** o UFC 6 também abre com os padrões do próprio motor onde a escolha do PORTHEON não tinha ganho medido: sem quadro adiantado para a placa de vídeo (também menos atraso no controle), apresentação em Mailbox e o modo de CPU AMD do motor desligado (não mudava nada no Ryzen). O orçamento de memória de vídeo nunca passa do que o motor usaria sozinho (o da 4.3.1 passava em todas as placas dos logs dos jogadores), e nas placas de 8 GB fica em 70% da placa, para as texturas serem liberadas antes de a placa encher. Isso vale uma vez para as instalações que já existem.
- **Menos memória:** em PCs com menos de 24 GB de RAM ou placas de 9 GB ou menos, o motor guarda menos memória de rascunho parada (uns 800 MB a menos de RAM).
- **Menus e entradas mais leves:** o desfoque de profundidade vem desligado (na escolha do kit o tempo da placa de vídeo por quadro caiu de 20,0 para 14,1 ms e a memória dela de 6,5 para 4,3 GB). Dá para ligar de novo na aba Vídeo.
- **Opções novas:** anti-aliasing (o FXAA do próprio motor, desligado por padrão) e resolução 3D em 0,75x (mais FPS em menus e entradas, imagem mais suave) na aba Vídeo; "Controle mais rápido" na aba Jogabilidade (ligado por padrão; desligado, monta um quadro adiantado, o que mediu 0 a 8% mais FPS).

**UFC 5**

- **Anti-aliasing (FXAA):** uma chave nova na aba Vídeo. O motor do UFC 5 não tem anti-aliasing próprio, então o PORTHEON desenha sobre cada quadro o mesmo FXAA do UFC 6, com uma camada Vulkan própria e pequena que só o motor do UFC 5 carrega. Sem custo de FPS mensurável.
- **Sem a faixa preta da direita:** a resolução dinâmica do jogo estreitava a imagem para uns 90% e, neste motor, o resto da tela ficava preto na seleção de lutador, na de kit e nas entradas. O PORTHEON agora desliga a resolução dinâmica do jogo, e a imagem sai sempre inteira.
- **Mais parecido com o console:** a torcida, as cadeiras atrás da grade e as sombras voltam a ser desenhadas nos PCs fortes (12 threads ou mais, 7,5 GB de memória de vídeo ou mais, 16 GB de RAM). No modo 1080p da 4.3.1 elas já não custam FPS (28 contra 27 FPS na mesma luta), e sem elas a arena ficava preta atrás da grade. PCs mais fracos continuam com elas desligadas; a torcida ainda pode ser trocada na aba Extras.

**PORTHEON**

- **Sai da frente enquanto você joga:** todo jogo, UFC 5 e UFC 6 inclusive, abre com o foco e o controle; o PORTHEON para os próprios timers e, minimizado, libera as imagens e a memória, e volta quando o jogo fecha.
- **A economia de energia do Windows (EcoQoS) fica mesmo desligada nos jogos:** ela era pedida de um jeito que o Windows 10 recusa, e no Windows 11 uma segunda chamada desfazia a primeira. O launcher do UFC 5 não toma mais o F10 de todos os programas quando roda sem janela própria.

**"Código 321" no UFC 6** (a partir dos logs que os jogadores mandaram em 30/10/2026):

- **Placas AMD:** as RX 6000 (driver da AMD 2.0.353, o mais novo que a AMD faz para elas) travavam no menu principal. O PORTHEON agora liga sozinho as correções do motor para AMD: em toda placa AMD a linha da GPU faz as próprias chamadas Vulkan, e com o driver 2.0.353 ou mais antigo voltam as esperas entre as tarefas de cálculo e liga a trava de laços dos shaders do motor. Se uma placa AMD ainda travar, a sessão seguinte grava no log o que a placa fazia.
- **Sem o aviso de troféu:** ele pedia memória no meio da luta e fechava o jogo em placas de 6 e 8 GB (uma RTX 3050 de 6 GB e uma RX 7600). Os troféus continuam sendo ganhos; `trophy_toasts=1` em `games\UFC6\UFC6.ini` traz o aviso de volta.
- **Mensagens mais claras:** travamento do driver, falta de memória de vídeo e versão do jogo diferente da 01.004 agora têm mensagem própria, em vez de só "código 321".

## Novidades da 4.3.1

Tudo o que mudou da 4.2.0 até a 4.3.1 (versões 4.3.0 e 4.3.1):

**UFC 5**

- **O UFC 5 voltou**, ao lado do UFC 6, cada um com a sua pasta de jogo.
- **Sem câmera lenta:** o UFC 5 andava um passo fixo a cada quadro desenhado, feito para 60 FPS. Agora o PORTHEON muda o relógio do jogo ao abrir, e cada quadro avança o tempo real que passou: a luta anda na velocidade do PS5 com qualquer FPS, sem atraso, como no UFC 6.
- **Bem mais leve em telas 1080p e 1440p:** o motor dizia ao jogo que ele estava numa TV 4K, e o UFC 5 desenhava cada quadro para 4K. Agora o jogo recebe o tamanho da sua tela e, abaixo de 4K, renderiza no modo 1080p, como um PS5 numa TV Full HD: num Ryzen 5 5500 + RTX 4060, numa janela de 1280x720, a seleção de lutador foi de 12-20 para 39-53 FPS e a luta de 24-31 para 31-45, com menos memória de vídeo. Telas 4K continuam no modo 4K.
- **Sem faixa preta na seleção de lutador e de kit:** o FSR2 do jogo voltou ligado por padrão; o esticamento simples que o substituía deixava preto o décimo da direita da tela quando o jogo baixava a resolução.
- **O motor roda com prioridade Alta**, e o FPS "na luta" do resumo da sessão deixa de fora carregamentos e cinemáticas.

**UFC 6**

- **Sem câmera lenta:** a luta roda na velocidade do PS5, a 60 passos por segundo, separados do FPS da tela, e com mais FPS (34-40 em vez de 29-31 numa luta, num Ryzen 5 5500 + RTX 4060, numa janela de 1280x720).
- **Opções novas:** prioridade do processo (normal, acima do normal ou alta), compatibilidade com processador AMD e modo de apresentação (Imediato ou Mailbox).
- **Perfis de memória de vídeo para placas de 4, 5 e 6 GB** (parte dos dados fica na RAM em vez de o jogo fechar), aviso em PCs com menos de 12 GB de RAM e, se o UFC 6 fechar por falta de RAM, o PORTHEON oferece configurar o arquivo de paginação, como já fazia no UFC 1 a 4.

**UFC 5 e UFC 6**

- **Abrem já sem o tutorial:** na primeira vez entra um save que já passou do tutorial de início, em qualquer PC; um save que você já tem nunca é tocado. A primeira sessão monta os shaders da sua placa de vídeo (pode engasgar nela; da segunda em diante fica liso). Para instalar o PORTHEON numa pasta nova sem montar tudo de novo, `games\UFC5\cache-inicial` e `games\UFC6\cache-inicial` recebem os seus próprios caches (veja o `LEIA-ME.txt` lá).
- **A thread vigia de áudio**, que ocupava um núcleo inteiro do processador, foi desligada.
- **Modo Parsec** (aba Online): mantém o controle funcionando com a janela do Parsec na frente, em Mailbox a 60 Hz; no UFC 5 e 6 é um jogador por vez: o seu amigo joga no seu lugar.

**UFC 1 a 4**

- **UFC 3 e UFC 4 no motor novo em todas as placas:** no UFC 4, 11% mais FPS na cena de abertura e menos travadinhas, com as correções dos códigos 20 e 23 e da Carreira. Nas placas NVIDIA, se o jogo fechar logo no começo com ele, o PORTHEON volta sozinho para o motor do port.
- UFC 1 a 4 abrem no idioma do PORTHEON na primeira vez.

**PORTHEON**

- **Interface neon nova** e uma **central de desempenho** no estilo do PS5: um cartão para o processador, um para a placa de vídeo e um para os shaders de cada jogo, com o que o PORTHEON faz com eles. O UFC 5 e o UFC 6 aparecem como ports para PC, como o UFC 1 a 4.
- **O jogo não reabre mais sozinho:** por um instante depois que qualquer jogo fecha, Enter, Espaço, o A e o Start do controle e o botão JOGAR não fazem nada, então uma tecla que você ainda estava digitando não abre o jogo de novo; e quando o UFC 1 a 4 fecha enquanto você está em outro programa, o PORTHEON fica atrás dele.
- **Mensagem mais clara do driver (UFC 1 a 4):** se o driver de vídeo não tiver uma extensão Vulkan que o motor precisa, o PORTHEON diz qual é e pede para atualizar o driver, em vez de um erro genérico.
- O link novo do Discord.

## Jogos

| Jogo | Plataforma | O que você precisa | Port |
|---|---|---|---|
| EA SPORTS UFC (2014) | PS4 | `CUSA00222`, update 1.08 | port nativo para PC |
| EA SPORTS UFC 2 | PS4 | `CUSA01968`, update 1.13 | port nativo para PC |
| EA SPORTS UFC 3 | PS4 | `CUSA06534`, update 1.14 | port nativo para PC |
| EA SPORTS UFC 4 | PS4 | `CUSA14209` ou `CUSA14204`, update 28.00 para firmware 5.05 (o "backport", não o original para 6.72+) | port nativo para PC |
| EA SPORTS UFC 5 | PS5 | `PPSA03541`, versão 01.031 | port para PC (motor KytyPS5) |
| EA SPORTS UFC 6 | PS5 | `PPSA23566`, versão 01.004, 01.005 ou 01.006 | port para PC (motor KytyPS5) |
| The Legend of Zelda: Ocarina of Time | Switch 2 | `0400486024DEC000` (.nup, ou .nsp/.xci com as suas próprias chaves); versão 1.1.1 para recompilar como nativo | emulador Lichii, ou PC nativo pelo Ocarina Recomp |
| God of War III Remastered | PS4 | `CUSA01623`, update 01.02 | port nativo para PC (gow3_pc) |
| Bloodborne | PS4 | `CUSA03173` extraído, de preferência na versão 01.09 | port nativo para PC bbport-windows (instalado por dentro do PORTHEON) |
| inFAMOUS Second Son | PS4 | `CUSA00004` extraído e decriptado | port nativo para PC (instalado por dentro do PORTHEON) |

## Como instalar

1. Baixe o `PORTHEON-x.x.x-Windows-x64.zip` da [versão mais recente](../../releases/latest).
2. Extraia numa **pasta nova e vazia** (nunca por cima de uma versão antiga, e fora do OneDrive).
3. Abra o `PORTHEON.exe`. No Linux / Steam Deck, o `portheon.sh` abre o PORTHEON pelo Proton ou pelo Wine (experimental): o UFC 1 a 4 ainda não roda no Wine/Proton, e o UFC 5 e 6 não foram testados lá.
4. O PORTHEON procura os seus jogos no PC. Se não achar algum, abra o jogo, escolha **Arquivos do jogo** e indique a pasta do seu jogo extraído.
5. Clique em **JOGAR**.

**Já tem o PORTHEON?** Use o botão de atualizar no topo do PORTHEON: ele instala a versão nova na mesma pasta e mantém os seus saves, caches e configurações. Se preferir instalar à mão numa pasta nova, copie da pasta antiga, antes do primeiro JOGAR: `games\UFC5\bin\_SaveData`, `_ShaderCache` e `_PipelineCache`, e `games\UFC6\bin\_SaveData` e `_PipelineCache` (o UFC 1 a 4 guarda os saves fora da pasta do PORTHEON).

**Requisitos:** Windows 10 (1903 ou mais novo) ou 11, 64 bits; placa de vídeo com Vulkan 1.3 (NVIDIA, AMD ou Intel Arc) e driver atualizado. UFC 1 a 4: processador Intel Core de 4ª geração (Haswell) ou AMD Ryzen, ou mais novo; Intel Core de 2ª e 3ª geração e AMD FX rodam pelo modo de compatibilidade do PORTHEON (o UFC 3 é o mais testado neles). UFC 5 e 6: processador com AVX2, 16 GB de RAM recomendados (com 8 GB pode rodar com um arquivo de paginação de 16 a 32 GB, sem teste) e uma placa mais nova que a série GTX 10 (as GTX 10 e anteriores não rodam); no UFC 6, 8 GB de memória de vídeo recomendados (perfis para placas de 4, 5 e 6 GB). O UFC 5 precisa também do Microsoft Visual C++ 2015-2022 Redistributable (x64); se faltar, o PORTHEON oferece abrir a página de download da Microsoft.

## O que o PORTHEON faz

- Uma janela para todos os UFC: vídeo, jogabilidade e controles gravados direto em cada port.
- Configuração automática para o seu PC (processador, placa de vídeo, memória de vídeo, RAM e monitor), refeita quando o PC muda.
- Resolve sozinho os travamentos conhecidos: depois de a placa de vídeo parar (código 23) ou de uma falha no jogo (código 3), ele muda o que causou isso para o próximo JOGAR e explica em palavras.
- Controles: PS5, PS4, PS3, Xbox e genéricos; dois controles são dois jogadores nos UFC 1 a 4; Modo Parsec para jogar com um amigo.
- Perfis, backup automático dos saves, trapaças (UFC 4), atalhos na área de trabalho, 16 idiomas.

## Licença

O código do próprio PORTHEON é software livre sob a GNU GPL, versão 2 ou mais nova. Os ports e motores que vêm junto mantêm as suas próprias licenças (GPL-2.0, algumas partes GPL-2.0-only, GPL-3.0 nas trapaças do UFC 4). O código-fonte do PORTHEON é publicado com cada versão; os códigos-fonte das builds do PORTHEON dos ports do UFC 1 a 4, do motor Intel Arc e do motor do UFC 6 ainda estão sendo reunidos para publicação. A pasta `licenses` do pacote tem todas as licenças, e o `NOTICE.txt`; os programas do Zelda e do God of War III (6.0) têm as deles em `games\ZELDA\licenses` e `games\GOW3\licenses`. Os ports do Bloodborne e do inFAMOUS Second Son (6.1) não vêm no pacote: o jogador instala por dentro do PORTHEON, e os créditos, as licenças (GPL-2.0-or-later) e os endereços do código-fonte estão em `games\BLOODBORNE\PORTHEON-README.txt` e `games\INFAMOUS\PORTHEON-README.txt` (Bloodborne: bbport-windows de Supermedo (Mohammed Albarghouthi), github.com/Supermedo/bloodborne_pc, feito sobre o bbport de deadinside28; inFAMOUS: feito sobre o shadPS4, github.com/shadps4-emu/shadPS4). O motor Native Plus do UFC 4 é o runtime do UFC4 Native Plus 1.2.0 de Abdullah Tech, sem alteração (github.com/abdullah-tech7/UFC4-Native-Plus, GPL-2.0; feito sobre o UFC4-PC-Port de schans-arg e o renderizador do UFC 3 do GNSB): a licença, os avisos e o endereço do código-fonte estão em `games\engines\native-plus`.
