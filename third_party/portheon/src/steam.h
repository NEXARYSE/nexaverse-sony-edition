/* steam.h - PORTHEON 5.0: "Add to Steam" (non-Steam shortcuts with artwork). C, Win32, llvm-mingw.
 *
 * Steam keeps the shortcuts in userdata\<id3>\config\shortcuts.vdf (binary VDF) and the library pictures in
 * userdata\<id3>\config\grid\<appid>[p|_hero|_logo|_icon].png|jpg. Steam rewrites shortcuts.vdf from memory when it
 * closes, so the file must be changed while Steam is NOT running (SteamIsRunning): otherwise the entry is lost at exit.
 *
 * appid of a non-Steam shortcut = crc32(Exe-as-written (with its quotes) + AppName) | 0x80000000 (unsigned 32-bit);
 * the 64-bit id Big Picture / steam://rungameid uses is ((uint64)appid << 32) | 0x02000000.
 */
#ifndef PORTHEON_STEAM_H
#define PORTHEON_STEAM_H
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TRUE when a steam.exe process exists (the file may not be edited then). */
BOOL SteamIsRunning(void);

/* The appid the shortcut gets for this exe (absolute path, unquoted) and name: the same formula Steam uses. */
unsigned int SteamShortcutAppId(const wchar_t *appName, const wchar_t *exe);

/* Where Steam is (HKCU\Software\Valve\Steam\SteamPath, then HKLM ...\Valve\Steam\InstallPath). Backslashes. */
BOOL SteamFindPath(wchar_t *out, int cap);

/* The user folder to write: userdata\<id3> of the MostRecent account in config\loginusers.vdf, else the only
   userdata folder. FALSE with no account. */
BOOL SteamFindUserDir(wchar_t *out, int cap);

/* Adds (or replaces, matched by AppName + Exe) a non-Steam shortcut and copies the pictures into config\grid:
 *   gridPng  -> <appid>p.png|jpg   (portrait cover, 600x900)
 *   heroPng  -> <appid>_hero.png|jpg (1920x620 or 3840x1240)
 *   logoPng  -> <appid>_logo.png   (transparent)
 *   widePng  -> <appid>.png|jpg    (wide "Main Image", 920x430 or 460x215)
 *   iconPath -> <appid>_icon.ico|png and the entry's "icon" field (an .ico, .png or .exe path).
 * Any picture may be NULL/empty. Nothing is converted: give PNG/JPG files (the extension is kept).
 * exe: absolute unquoted path (quoted in the file); startDir: folder (NULL = the exe's folder);
 * launchOptions: arguments, e.g. L"--play UFC6" (NULL = none).
 * shortcuts.vdf is rewritten atomically (temp file + rename). 7.1: the first copy, shortcuts.vdf.portheon.bak, is
 * kept for good; the file before each write is copied to shortcuts.vdf.portheon.last.bak. The file is written whole
 * (it ends with the root list's 0x08: 5.0-7.0 left it out and Steam refused the file) and checked before it is
 * written; an existing file that does not read back byte for byte is left untouched (FALSE), a 5.0-7.0 file (one
 * 0x08 short) is repaired. When the list holds nothing but PORTHEON's own entries and the first backup holds other
 * games, those come back (SteamLastRestored), only before 7.1's first write to that list (no .portheon.last.bak yet:
 * 7.1 review). Both backups are written whole (a one-short file gets its 0x08; a .bak 5.0-7.0 left short is repaired).
 * Returns FALSE with the reason in errOut (UTF-16); on success errOut holds the appid in decimal. */
BOOL SteamAddShortcut(const wchar_t *appName, const wchar_t *exe, const wchar_t *startDir, const wchar_t *launchOptions,
                      const wchar_t *gridPng, const wchar_t *heroPng, const wchar_t *logoPng, const wchar_t *widePng,
                      const wchar_t *iconPath, wchar_t *errOut, int errCap);

/* Removes the entry matched by AppName + Exe (and its grid pictures). TRUE also when there was none. */
BOOL SteamRemoveShortcut(const wchar_t *appName, const wchar_t *exe);

/* Lists the entries: cb(index, appid, AppName, Exe, StartDir, LaunchOptions, user) for each; returns the count or -1. */
typedef void (*SteamShortcutFn)(int index, unsigned int appid, const char *appName, const char *exe, const char *startDir,
                                const char *launchOptions, void *user);
int SteamListShortcuts(SteamShortcutFn cb, void *user, wchar_t *errOut, int errCap);

/* 7.1: what the last SteamAddShortcut / SteamRemoveShortcut did, for a log line: one line of text without paths (the
   account id cut to its last 3 digits); the state of the file it found (0 none or empty, 1 valid, 2 repaired: one 0x08
   short, -1 refused and left untouched, -2 not read); the entries put back from the first backup. */
const wchar_t *SteamLastTrace(void);
int SteamLastState(void);
int SteamLastRestored(void);
/* 7.1 review: after a successful SteamAddShortcut, what shortcuts.vdf.portheon.bak holds: 0 none, 1 the list from before
   PORTHEON's first click (reads back whole, at least one game, no PORTHEON entry), 2 anything else (written after a
   5.0-7.0 click, or an emptied list): only 1 may be called "the list from before PORTHEON's first click". */
int SteamLastBakKind(void);

#ifdef __cplusplus
}
#endif
#endif
