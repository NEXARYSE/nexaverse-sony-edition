/* library.c - PORTHEON 6.1 (the user, 2026-11-08: "add any game to the home row: Steam, Epic, emulators, native ports,
 * any exe"). The player's own games ("Adicionar jogos"), beside the built-in ones but never rows of G[]: G[] is const and
 * sized NGAMES, and about 40 per-game arrays and every index macro (PS4G, UFC6G, EXTG...) assume a fixed index. So the
 * custom games have their own model here and their own selection, g_csel (-1: a built-in game, g_sel, is shown).
 * Included from portheon.c right after steam.c (it uses SteamFindPath, SteamFindUserDir, SteamListShortcuts, ReadAll,
 * RegStr, Backslashes and TrimSlash). Its screens are in ui_library.c (part of ui.c).
 *
 * Storage: %LOCALAPPDATA%\PORTHEON\library.ini (UTF-16 from the start, as zelda.ini/ufc3u.ini), kept by the uninstaller:
 *   [library] order=3,1,7  next_id=8
 *   [c<id>]   kind, name, target, args, dir, rom, core, steam_appid, steam_gameid, epic_app, epic_ns, epic_item,
 *             watch_dir, played_ms, last, added      (every string is written inside "...", so quotes in args survive)
 * Art: %LOCALAPPDATA%\PORTHEON\art\c<id>_icon.<ext> (the cover: Steam's 600x900 or the player's pick),
 *      c<id>_pic.<ext> (the background) and c<id>_exe.png (the program's own icon, 256 px). Copied at add time, so a
 *      cleanup of Steam's cache never takes them away. Only the player's own files: nothing is downloaded.
 * Starting: steam://rungameid/<id>; com.epicgames.launcher://apps/<ns>%3A<item>%3A<AppName>?action=launch&silent=true;
 *   CreateProcessW for programs and emulators (the emulator's arguments from a template, editable in library.ini);
 *   ShellExecuteExW for .lnk/.url/.bat. A watch thread finds the game's process by its install folder, so PORTHEON
 *   counts the time played, lets go of the controller and comes back when the game closes. Custom games never go
 *   through g_proc/StartPort/OnExited (the UFC crash texts, streaks and reports know nothing about them).
 * Tests: PORTHEON_STEAM_PATH (steam.c) and PORTHEON_EPIC_MANIFESTS point the lists at fake folders. The scan part
 *   (up to LIB_SCAN_ONLY's end) compiles on its own with steam.c, for a small console test.
 *
 * Hit ids (fixed block 800..875, documented in HOOKS-library-6.1.md; nothing else of PORTHEON uses 800 and up):
 *   H_CUSTOM 800..831 the custom tiles, H_LIBADD 832 the "+" tile, H_LIBSRC 833..837 the Add screen's sources,
 *   838..857 the Add screen's and the game page's buttons, H_LIBROW 860..875 the list rows.
 * Overlays OV_LIBADD 40 and OV_CGAME 41, messages WM_APP + 40 / + 41: values far from the enums the other 6.1 work grows. */

#define MAXCUSTOM 32
enum { CK_STEAM = 1, CK_EPIC, CK_EMU, CK_EXE, CK_LNK, CK_URL };
typedef struct {
    int id, kind;
    wchar_t name[80], target[MAX_PATH], args[512], dir[MAX_PATH], rom[MAX_PATH], core[MAX_PATH];
    unsigned appid; unsigned long long gameid;   /* Steam: the appid (art) and the id steam://rungameid takes (a non-Steam shortcut's is 64-bit) */
    wchar_t epicApp[128], epicNs[64], epicItem[64], watchDir[MAX_PATH];
    ULONGLONG playedMs; long long last, added;
} CGame;
/* one row of the Steam / Epic / non-Steam lists in the Add screen */
typedef struct { int kind; wchar_t name[80], dir[MAX_PATH], exe[MAX_PATH]; unsigned appid; unsigned long long gameid; wchar_t epicApp[128], epicNs[64], epicItem[64]; ULONGLONG size; } LibCand;
#define LIB_MAXCAND 256

/* ---------------------------------------------------------------- text helpers */
static void LibW(const char *s, wchar_t *out, int cap)   /* UTF-8 -> UTF-16, cut to cap */
{
    out[0] = 0; int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0); if (n <= 0) return;
    wchar_t *t = (wchar_t *)malloc((size_t)n * sizeof(wchar_t)); if (!t) return;
    MultiByteToWideChar(CP_UTF8, 0, s, -1, t, n); wcsncpy(out, t, cap - 1); out[cap - 1] = 0; free(t);
}
static int LibCandCmp(const void *a, const void *b) { return _wcsicmp(((const LibCand *)a)->name, ((const LibCand *)b)->name); }

/* ---------------------------------------------------------------- Steam's text VDF (libraryfolders.vdf, appmanifest_*.acf) */
/* the next token: 0 at the end, '{', '}', or 's' with the string (quotes gone, \\ \" \n \t unescaped, // comments skipped) */
static int LibVdfNext(const char **ps, const char *e, char *tok, int cap)
{
    const char *s = *ps;
    for (;;) {
        while (s < e && (unsigned char)*s <= ' ') s++;
        if (s >= e) { *ps = s; return 0; }
        if (*s == '/' && s + 1 < e && s[1] == '/') { while (s < e && *s != '\n') s++; continue; }
        break;
    }
    if (*s == '{' || *s == '}') { int c = *s++; *ps = s; return c; }
    int n = 0;
    if (*s == '"') {
        s++;
        while (s < e && *s != '"') {
            char c = *s++;
            if (c == '\\' && s < e) { char d = *s++; c = d == 'n' ? '\n' : d == 't' ? '\t' : d; }
            if (n < cap - 1) tok[n++] = c;
        }
        if (s < e) s++;
    } else while (s < e && (unsigned char)*s > ' ' && *s != '{' && *s != '}' && *s != '"') { if (n < cap - 1) tok[n++] = *s; s++; }
    tok[n] = 0; *ps = s; return 's';
}
/* the library folders: Steam's own, then every "path" of libraryfolders.vdf (and the pre-2021 "1" "D:\\Lib" form) */
static int LibSteamLibs(const wchar_t *steam, wchar_t libs[][MAX_PATH], int max)
{
    int n = 0; wcsncpy(libs[n], steam, MAX_PATH - 1); libs[n][MAX_PATH - 1] = 0; n++;
    wchar_t p[MAX_PATH]; _snwprintf(p, MAX_PATH, L"%s\\steamapps\\libraryfolders.vdf", steam); p[MAX_PATH - 1] = 0;
    unsigned char *b; DWORD len;
    if (!ReadAll(p, &b, &len)) { _snwprintf(p, MAX_PATH, L"%s\\config\\libraryfolders.vdf", steam); p[MAX_PATH - 1] = 0; if (!ReadAll(p, &b, &len)) return n; }
    const char *s = (const char *)b, *e = s + len; char key[1024], tok[1024]; int depth = 0, t;   /* 6.1: locals: the Add screen's scan thread and the UI thread both read these lists */ BOOL haveKey = FALSE;
    while ((t = LibVdfNext(&s, e, tok, (int)sizeof(tok))) != 0) {
        if (t == '{') { depth++; haveKey = FALSE; continue; }
        if (t == '}') { depth--; haveKey = FALSE; continue; }
        if (!haveKey) { strcpy(key, tok); haveKey = TRUE; continue; }
        haveKey = FALSE;
        BOOL isPath = (depth == 2 && !_stricmp(key, "path")) || (depth == 1 && key[0] >= '0' && key[0] <= '9' && strchr(tok, ':'));
        if (!isPath || n >= max) continue;
        wchar_t w[MAX_PATH]; LibW(tok, w, MAX_PATH); Backslashes(w); TrimSlash(w);
        BOOL dup = FALSE; for (int i = 0; i < n; i++) if (!_wcsicmp(libs[i], w)) dup = TRUE;
        if (!dup && w[0]) { wcscpy(libs[n], w); n++; }
    }
    free(b); return n;
}
/* one appmanifest_<id>.acf: "AppState" { "appid" "name" "installdir" "StateFlags" "SizeOnDisk" } */
static BOOL LibAcf(const wchar_t *path, unsigned *appid, char *name, int ncap, char *idir, int icap, unsigned *flags, ULONGLONG *size)
{
    unsigned char *b; DWORD len; *appid = 0; *flags = 0; *size = 0; name[0] = idir[0] = 0;
    if (!ReadAll(path, &b, &len)) return FALSE;
    const char *s = (const char *)b, *e = s + len; char key[256], tok[1024]; int depth = 0, t; BOOL haveKey = FALSE;
    while ((t = LibVdfNext(&s, e, tok, (int)sizeof(tok))) != 0) {
        if (t == '{') { depth++; haveKey = FALSE; continue; }
        if (t == '}') { depth--; haveKey = FALSE; continue; }
        if (!haveKey) { strncpy(key, tok, sizeof(key) - 1); key[sizeof(key) - 1] = 0; haveKey = TRUE; continue; }
        haveKey = FALSE; if (depth != 1) continue;
        if (!_stricmp(key, "appid")) *appid = (unsigned)strtoul(tok, NULL, 10);
        else if (!_stricmp(key, "name")) { strncpy(name, tok, ncap - 1); name[ncap - 1] = 0; }
        else if (!_stricmp(key, "installdir")) { strncpy(idir, tok, icap - 1); idir[icap - 1] = 0; }
        else if (!_stricmp(key, "StateFlags")) *flags = (unsigned)strtoul(tok, NULL, 10);
        else if (!_stricmp(key, "SizeOnDisk")) *size = _strtoui64(tok, NULL, 10);
    }
    free(b); return *appid != 0 && name[0];
}
/* the installed Steam games of every library, by name; tools (Steamworks, Proton, runtimes, SteamVR) left out */
static int LibSteamGames(LibCand *out, int max)
{
    wchar_t sp[MAX_PATH]; if (!SteamFindPath(sp, MAX_PATH)) return 0;
    wchar_t libs[16][MAX_PATH]; int nl = LibSteamLibs(sp, libs, 16), n = 0;
    for (int l = 0; l < nl; l++) {
        wchar_t pat[MAX_PATH]; _snwprintf(pat, MAX_PATH, L"%s\\steamapps\\appmanifest_*.acf", libs[l]); pat[MAX_PATH - 1] = 0;
        WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            wchar_t p[MAX_PATH]; _snwprintf(p, MAX_PATH, L"%s\\steamapps\\%s", libs[l], fd.cFileName); p[MAX_PATH - 1] = 0;
            unsigned id, fl; ULONGLONG sz; char nm[256], idir[512];
            if (!LibAcf(p, &id, nm, (int)sizeof(nm), idir, (int)sizeof(idir), &fl, &sz) || !(fl & 4) || !idir[0]) continue;   /* 4: fully installed */
            if (id == 228980 || !_strnicmp(nm, "Steamworks", 10) || !_strnicmp(nm, "Proton", 6) || !_strnicmp(nm, "Steam Linux Runtime", 19) || !_strnicmp(nm, "SteamVR", 7)) continue;
            BOOL dup = FALSE; for (int i = 0; i < n; i++) if (out[i].appid == id) dup = TRUE;
            if (dup || n >= max) continue;
            LibCand *c = &out[n]; memset(c, 0, sizeof(*c)); c->kind = CK_STEAM; c->appid = id; c->gameid = id; c->size = sz;
            LibW(nm, c->name, 80);
            wchar_t wd[MAX_PATH]; LibW(idir, wd, MAX_PATH);
            _snwprintf(c->dir, MAX_PATH, L"%s\\steamapps\\common\\%s", libs[l], wd); c->dir[MAX_PATH - 1] = 0;
            n++;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    qsort(out, (size_t)n, sizeof(LibCand), LibCandCmp);
    return n;
}
/* the player's non-Steam shortcuts (shortcuts.vdf), started through Steam too: ((uint64)appid << 32) | 0x02000000.
   PORTHEON's own "--play" shortcuts (Add to Steam) are left out: those games are on the home row already. */
typedef struct { LibCand *out; int max, n; } LibShortCtx;
static void LibUnquote(const char *s, wchar_t *out, int cap) { wchar_t t[MAX_PATH]; LibW(s, t, MAX_PATH); wchar_t *a = t; if (*a == L'"') a++; size_t k = wcslen(a); if (k && a[k - 1] == L'"') a[k - 1] = 0; wcsncpy(out, a, cap - 1); out[cap - 1] = 0; }
static void LibShortcutCb(int index, unsigned int appid, const char *appName, const char *exe, const char *startDir, const char *launchOptions, void *user)
{
    (void)index; LibShortCtx *x = (LibShortCtx *)user; if (x->n >= x->max || !appid) return;
    if (launchOptions && strstr(launchOptions, "--play") && exe && StrStrIA(exe, "PORTHEON")) return;
    LibCand *c = &x->out[x->n]; memset(c, 0, sizeof(*c)); c->kind = CK_STEAM; c->appid = appid; c->gameid = ((unsigned long long)appid << 32) | 0x02000000ull;
    LibW(appName ? appName : "", c->name, 80); if (!c->name[0]) return;
    LibUnquote(exe ? exe : "", c->exe, MAX_PATH); LibUnquote(startDir ? startDir : "", c->dir, MAX_PATH);
    if (!c->dir[0] && c->exe[0]) { wcscpy(c->dir, c->exe); PathRemoveFileSpecW(c->dir); }
    x->n++;
}
static int LibSteamShortcuts(LibCand *out, int max)
{
    LibShortCtx x = {out, max, 0}; wchar_t err[200];
    if (SteamListShortcuts(LibShortcutCb, &x, err, 200) < 0) return 0;
    qsort(out, (size_t)x.n, sizeof(LibCand), LibCandCmp);
    return x.n;
}

/* ---------------------------------------------------------------- Epic Games (Manifests\*.item, JSON) */
/* a top-level "Key": "value" (\\ \" \/ \n \uXXXX unescaped, to UTF-8) or a literal (true/false/number); FALSE when absent */
static BOOL LibJsonStr(const char *b, const char *key, char *out, int cap)
{
    char pat[96]; _snprintf(pat, sizeof(pat), "\"%s\"", key); pat[sizeof(pat) - 1] = 0;
    const char *p = b; out[0] = 0;
    while ((p = strstr(p, pat)) != NULL) {
        const char *q = p + strlen(pat); while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
        if (*q != ':') { p = q; continue; }
        q++; while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
        int n = 0;
        if (*q == '"') {
            q++;
            while (*q && *q != '"') {
                unsigned c = (unsigned char)*q++;
                if (c == '\\' && *q) {
                    char d = *q++;
                    if (d == 'u' && isxdigit((unsigned char)q[0]) && isxdigit((unsigned char)q[1]) && isxdigit((unsigned char)q[2]) && isxdigit((unsigned char)q[3])) {
                        char hx[5] = {q[0], q[1], q[2], q[3], 0}; unsigned u = (unsigned)strtoul(hx, NULL, 16); q += 4;
                        char u8[4]; int k = 0;
                        if (u >= 0xD800 && u <= 0xDFFF) u = '?';   /* a surrogate half: not worth more for a game's name */
                        if (u < 0x80) u8[k++] = (char)u; else if (u < 0x800) { u8[k++] = (char)(0xC0 | (u >> 6)); u8[k++] = (char)(0x80 | (u & 63)); }
                        else { u8[k++] = (char)(0xE0 | (u >> 12)); u8[k++] = (char)(0x80 | ((u >> 6) & 63)); u8[k++] = (char)(0x80 | (u & 63)); }
                        for (int j = 0; j < k; j++) if (n < cap - 1) out[n++] = u8[j];
                        continue;
                    }
                    c = d == 'n' ? '\n' : d == 't' ? '\t' : d == 'r' ? '\r' : (unsigned char)d;
                }
                if (n < cap - 1) out[n++] = (char)c;
            }
        } else while (*q && *q != ',' && *q != '}' && *q != '\r' && *q != '\n' && *q != ' ') { if (n < cap - 1) out[n++] = *q; q++; }
        out[n] = 0; return TRUE;
    }
    return FALSE;
}
static void LibEpicDir(wchar_t *out)
{
    DWORD n = GetEnvironmentVariableW(L"PORTHEON_EPIC_MANIFESTS", out, MAX_PATH);   /* tests: a folder of fake *.item files */
    if (n && n < MAX_PATH) { Backslashes(out); TrimSlash(out); return; }
    wchar_t ad[MAX_PATH];
    if (RegStr(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Epic Games\\EpicGamesLauncher", L"AppDataPath", ad, MAX_PATH)) {
        Backslashes(ad); TrimSlash(ad); _snwprintf(out, MAX_PATH, L"%s\\Manifests", ad); out[MAX_PATH - 1] = 0;
        if (GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES) return;
    }
    wchar_t pd[MAX_PATH] = L""; if (!GetEnvironmentVariableW(L"ProgramData", pd, MAX_PATH)) wcscpy(pd, L"C:\\ProgramData");
    _snwprintf(out, MAX_PATH, L"%s\\Epic\\EpicGamesLauncher\\Data\\Manifests", pd); out[MAX_PATH - 1] = 0;
}
/* the installed Epic games: complete installs whose AppName is the main game's (DLC left out), folder present */
static int LibEpicGames(LibCand *out, int max)
{
    wchar_t dir[MAX_PATH], pat[MAX_PATH]; LibEpicDir(dir); _snwprintf(pat, MAX_PATH, L"%s\\*.item", dir); pat[MAX_PATH - 1] = 0;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); int n = 0; if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.nFileSizeHigh || fd.nFileSizeLow > 256 * 1024 || n >= max) continue;
        wchar_t p[MAX_PATH]; _snwprintf(p, MAX_PATH, L"%s\\%s", dir, fd.cFileName); p[MAX_PATH - 1] = 0;
        unsigned char *b; DWORD len; if (!ReadAll(p, &b, &len)) continue;
        char nm[512], app[256], mainApp[256], ns[128], item[128], loc[1024], exe[1024], inc[16];
        const char *j = (const char *)b;
        LibJsonStr(j, "DisplayName", nm, (int)sizeof(nm)); LibJsonStr(j, "AppName", app, (int)sizeof(app));
        if (!LibJsonStr(j, "MainGameAppName", mainApp, (int)sizeof(mainApp))) strcpy(mainApp, app);
        LibJsonStr(j, "CatalogNamespace", ns, (int)sizeof(ns)); LibJsonStr(j, "CatalogItemId", item, (int)sizeof(item));
        LibJsonStr(j, "InstallLocation", loc, (int)sizeof(loc)); LibJsonStr(j, "LaunchExecutable", exe, (int)sizeof(exe));
        LibJsonStr(j, "bIsIncompleteInstall", inc, (int)sizeof(inc));
        free(b);
        if (!app[0] || !loc[0] || !_stricmp(inc, "true") || _stricmp(app, mainApp)) continue;
        LibCand *c = &out[n]; memset(c, 0, sizeof(*c)); c->kind = CK_EPIC;
        LibW(nm[0] ? nm : app, c->name, 80); LibW(app, c->epicApp, 128); LibW(ns, c->epicNs, 64); LibW(item, c->epicItem, 64);
        LibW(loc, c->dir, MAX_PATH); Backslashes(c->dir); TrimSlash(c->dir);
        if (GetFileAttributesW(c->dir) == INVALID_FILE_ATTRIBUTES) continue;
        if (exe[0]) { wchar_t we[MAX_PATH]; LibW(exe, we, MAX_PATH); Backslashes(we); _snwprintf(c->exe, MAX_PATH, L"%s\\%s", c->dir, we); c->exe[MAX_PATH - 1] = 0; }
        BOOL dup = FALSE; for (int i = 0; i < n; i++) if (!_wcsicmp(out[i].epicApp, c->epicApp)) dup = TRUE;
        if (!dup) n++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    qsort(out, (size_t)n, sizeof(LibCand), LibCandCmp);
    return n;
}

/* ---------------------------------------------------------------- Steam's pictures of a game */
/* kind 0: the cover (the player's own grid art, then library_600x900, then header), 1: the hero picture */
static BOOL LibSteamArtFind(unsigned appid, int kind, wchar_t *out)
{
    static const wchar_t *const ex[] = {L"png", L"jpg", L"jpeg"};
    wchar_t sp[MAX_PATH], ud[MAX_PATH], p[MAX_PATH];
    if (SteamFindUserDir(ud, MAX_PATH))
        for (int e = 0; e < 3; e++) { _snwprintf(p, MAX_PATH, L"%s\\config\\grid\\%u%s.%s", ud, appid, kind ? L"_hero" : L"p", ex[e]); p[MAX_PATH - 1] = 0; if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) { wcscpy(out, p); return TRUE; } }
    if (!SteamFindPath(sp, MAX_PATH)) return FALSE;
    static const wchar_t *const cover[] = {L"library_600x900.jpg", L"library_600x900_2x.jpg", L"library_600x900.png", L"header.jpg"}, *const hero[] = {L"library_hero.jpg", L"library_hero.png"};
    const wchar_t *const *names = kind ? hero : cover; int nn = kind ? 2 : 4;
    for (int i = 0; i < nn; i++) {
        _snwprintf(p, MAX_PATH, L"%s\\appcache\\librarycache\\%u_%s", sp, appid, names[i]); p[MAX_PATH - 1] = 0;   /* the old flat layout */
        if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) { wcscpy(out, p); return TRUE; }
        _snwprintf(p, MAX_PATH, L"%s\\appcache\\librarycache\\%u\\%s", sp, appid, names[i]); p[MAX_PATH - 1] = 0;   /* 2024+: a folder per game */
        if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) { wcscpy(out, p); return TRUE; }
        wchar_t pat[MAX_PATH]; _snwprintf(pat, MAX_PATH, L"%s\\appcache\\librarycache\\%u\\*", sp, appid); pat[MAX_PATH - 1] = 0;   /* ...and a hashed folder inside it */
        WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); BOOL got = FALSE;
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
                _snwprintf(p, MAX_PATH, L"%s\\appcache\\librarycache\\%u\\%s\\%s", sp, appid, fd.cFileName, names[i]); p[MAX_PATH - 1] = 0;
                if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) { wcscpy(out, p); got = TRUE; }
            } while (!got && FindNextFileW(h, &fd));
            FindClose(h);
        }
        if (got) return TRUE;
    }
    return FALSE;
}

/* ---------------------------------------------------------------- emulators */
/* match: the start of the emulator's exe name (any case); romKind 0 a file, 1 a folder, 2 EBOOT.BIN / a folder, 3 a
   RetroArch core + a file. The arguments are the emulators' documented command lines, NOT checked on a real install
   (none is on the PC where this was written): args stays editable in library.ini (the game page's "Editar opções"). */
typedef struct { const wchar_t *match, *name, *args; int romKind; } EmuPreset;
static const EmuPreset EMUS[] = {
    {L"rpcs3", L"RPCS3 (PS3)", L"--no-gui \"{rom}\"", 2},
    {L"pcsx2", L"PCSX2 (PS2)", L"-batch -fullscreen -- \"{rom}\"", 0},
    {L"duckstation", L"DuckStation (PS1)", L"-batch -fullscreen -- \"{rom}\"", 0},
    {L"dolphin", L"Dolphin (GameCube / Wii)", L"-b -e \"{rom}\"", 0},
    {L"xenia", L"Xenia (Xbox 360)", L"\"{rom}\" --fullscreen", 0},
    {L"shadps4", L"shadPS4 (PS4)", L"-g \"{rom}\"", 2},
    {L"retroarch", L"RetroArch", L"-f -L \"{core}\" \"{rom}\"", 3},
    {L"eden", L"Eden (Switch)", L"-f -g \"{rom}\"", 0}, {L"citron", L"Citron (Switch)", L"-f -g \"{rom}\"", 0},
    {L"suyu", L"Suyu (Switch)", L"-f -g \"{rom}\"", 0}, {L"sudachi", L"Sudachi (Switch)", L"-f -g \"{rom}\"", 0},
    {L"yuzu", L"yuzu (Switch)", L"-f -g \"{rom}\"", 0},
    {L"ryujinx", L"Ryujinx (Switch)", L"\"{rom}\" --fullscreen", 0},
    {L"ppsspp", L"PPSSPP (PSP)", L"\"{rom}\" --fullscreen", 0},
    {L"cemu", L"Cemu (Wii U)", L"-f -g \"{rom}\"", 0},
};
static const EmuPreset EMU_ANY = {NULL, NULL, L"\"{rom}\"", 0};
static const EmuPreset *EmuFor(const wchar_t *exe)
{
    const wchar_t *f = PathFindFileNameW(exe);
    for (int i = 0; i < (int)(sizeof(EMUS) / sizeof(EMUS[0])); i++) if (!_wcsnicmp(f, EMUS[i].match, wcslen(EMUS[i].match))) return &EMUS[i];
    return &EMU_ANY;
}
/* the arguments with {rom} {core} {romdir} {exe} filled in; FALSE when a path holds a '"' (it would break the quoting) */
static BOOL LibBuildArgs(const CGame *c, wchar_t *out, int n)
{
    out[0] = 0;
    if (wcschr(c->target, L'"') || wcschr(c->rom, L'"') || wcschr(c->core, L'"')) return FALSE;
    wchar_t romdir[MAX_PATH]; wcscpy(romdir, c->rom); if (!DirExists(romdir)) PathRemoveFileSpecW(romdir);
    int k = 0;
    for (const wchar_t *s = c->args; *s && k < n - 1; ) {
        const wchar_t *v = NULL; size_t skip = 0;
        if (!wcsncmp(s, L"{rom}", 5)) { v = c->rom; skip = 5; } else if (!wcsncmp(s, L"{core}", 6)) { v = c->core; skip = 6; }
        else if (!wcsncmp(s, L"{romdir}", 8)) { v = romdir; skip = 8; } else if (!wcsncmp(s, L"{exe}", 5)) { v = c->target; skip = 5; }
        if (v) { for (; *v && k < n - 1; v++) out[k++] = *v; s += skip; } else out[k++] = *s++;
    }
    out[k] = 0; return TRUE;
}
static BOOL LibBuildCmd(const CGame *c, wchar_t *cmd, int n)   /* "<exe>" <args> */
{
    wchar_t a[1600]; if (!LibBuildArgs(c, a, 1600)) return FALSE;
    _snwprintf(cmd, n, a[0] ? L"\"%s\" %s" : L"\"%s\"", c->target, a); cmd[n - 1] = 0; return TRUE;
}

/* ---------------------------------------------------------------- names */
static void LibStripExt(wchar_t *s) { wchar_t *e = PathFindExtensionW(s); if (e && *e) *e = 0; }
/* a program's name: the file's, or its folder's when the file has a generic name (game.exe, Launcher.exe, bin\...) */
static void LibNiceName(const wchar_t *path, wchar_t *out, int n)
{
    wchar_t f[MAX_PATH]; wcsncpy(f, PathFindFileNameW(path), MAX_PATH - 1); f[MAX_PATH - 1] = 0; LibStripExt(f);
    size_t fl = wcslen(f); if (fl > 15 && !_wcsicmp(f + fl - 15, L"-Win64-Shipping")) f[fl - 15] = 0;
    static const wchar_t *const gen[] = {L"game", L"launcher", L"start", L"play", L"bin", L"win64", L"shipping", L"main", L"app", L"client", L"run", L"setup"};
    BOOL generic = !f[0]; for (int i = 0; i < (int)(sizeof(gen) / sizeof(gen[0])); i++) if (!_wcsicmp(f, gen[i])) generic = TRUE;
    if (generic) {
        wchar_t d[MAX_PATH]; wcsncpy(d, path, MAX_PATH - 1); d[MAX_PATH - 1] = 0;
        for (int up = 0; up < 4; up++) {
            PathRemoveFileSpecW(d); const wchar_t *dn = PathFindFileNameW(d);
            if (!dn[0] || PathIsRootW(d)) break;
            if (!_wcsicmp(dn, L"Binaries") || !_wcsicmp(dn, L"Win64") || !_wcsicmp(dn, L"Win32") || !_wcsicmp(dn, L"bin") || !_wcsicmp(dn, L"x64") || !_wcsicmp(dn, L"x86")) continue;
            wcsncpy(f, dn, MAX_PATH - 1); f[MAX_PATH - 1] = 0; break;
        }
    }
    for (wchar_t *p = f; *p; p++) if (*p == L'_') *p = L' ';
    wcsncpy(out, f, n - 1); out[n - 1] = 0;
}
/* a game file's name: "Game (USA) [!].iso" -> "Game"; an EBOOT.BIN takes its game's folder name (PS3_GAME\USRDIR) */
static void LibRomName(const wchar_t *rom, wchar_t *out, int n)
{
    wchar_t f[MAX_PATH], d[MAX_PATH]; wcsncpy(d, rom, MAX_PATH - 1); d[MAX_PATH - 1] = 0;
    if (DirExists(rom)) wcscpy(f, PathFindFileNameW(d));
    else if (!_wcsicmp(PathFindFileNameW(rom), L"EBOOT.BIN") || !_wcsicmp(PathFindFileNameW(rom), L"eboot.bin")) {
        PathRemoveFileSpecW(d);
        while (d[0] && (!_wcsicmp(PathFindFileNameW(d), L"USRDIR") || !_wcsicmp(PathFindFileNameW(d), L"PS3_GAME"))) PathRemoveFileSpecW(d);
        wcscpy(f, PathFindFileNameW(d));
    } else { wcscpy(f, PathFindFileNameW(rom)); LibStripExt(f); }
    for (;;) {   /* the trailing (region) and [tag] groups */
        size_t k = wcslen(f); while (k && f[k - 1] == L' ') f[--k] = 0;
        if (k && (f[k - 1] == L')' || f[k - 1] == L']')) { wchar_t o = f[k - 1] == L')' ? L'(' : L'['; wchar_t *p = wcsrchr(f, o); if (p && p > f) { *p = 0; continue; } }
        break;
    }
    for (wchar_t *p = f; *p; p++) if (*p == L'_') *p = L' ';
    if (!f[0]) wcscpy(f, L"?");
    wcsncpy(out, f, n - 1); out[n - 1] = 0;
}
#ifndef LIB_SCAN_ONLY
/* ================================================================ the rest needs PORTHEON itself */
static CGame g_cg[MAXCUSTOM]; static int g_ncg;   /* in display order; g_csel / g_crun are declared beside Play (portheon.c) */
#define OV_LIBADD 40   /* the Add screen (ui_library.c) */
#define OV_CGAME 41    /* a custom game's page */
#define WM_APP_LIBWATCH (WM_APP + 40)   /* the watch thread: wParam = event (1 the game is running, 2 it ended) | id << 8; lParam = seconds played */
#define WM_APP_LIBSCAN (WM_APP + 41)    /* the Add screen's list was read: wParam = source, lParam = rows */
enum { H_CUSTOM = 800 /* 32 tiles */, H_LIBADD = 832, H_LIBSRC = 833 /* 5 sources */, H_LIBPREV = 838, H_LIBNEXT, H_LIBPICKEXE, H_LIBPICKROM, H_LIBPICKDIR,
       H_LIBPICKCORE, H_LIBOK, H_LIBCLOSE, H_LIBBACK, H_CPLAY, H_COPEN, H_CCOVER, H_CBG, H_CLEFT, H_CRIGHT, H_CINI, H_CREMOVE, H_CCLOSE,
       H_CCARD /* 2 home cards */, H_CPAD = H_CCARD + 2, H_CKEYS, H_LIBROW = 860 /* 16 list rows */, H_LIB_END = 876 };   /* 6.2: H_CPAD, H_CKEYS */
_Static_assert(H_CCARD + 2 <= H_LIBROW, "library hit ids overlap");
_Static_assert(H_CUSTOM + MAXCUSTOM <= H_LIBADD, "custom tiles overlap");

static long long LibNow(void) { FILETIME ft; GetSystemTimeAsFileTime(&ft); ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime; return (long long)(u.QuadPart / 10000000ull - 11644473600ull); }
static int LibIndex(int id) { for (int i = 0; i < g_ncg; i++) if (g_cg[i].id == id) return i; return -1; }
static void LibIni(wchar_t *out)
{
    wchar_t d[MAX_PATH]; LocalData(d, L"PORTHEON"); CreateDirectoryW(d, NULL); Join(out, d, L"library.ini");
    if (!FileExists(out)) { HANDLE h = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL); if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, "\xFF\xFE", 2, &w, NULL); CloseHandle(h); } }
}
static void LibArtDir(wchar_t *out) { wchar_t d[MAX_PATH]; LocalData(d, L"PORTHEON"); Join(out, d, L"art"); CreateDirectoryW(out, NULL); }
static void LibArtPath(int id, const wchar_t *kind, const wchar_t *ext, wchar_t *out) { wchar_t d[MAX_PATH], f[64]; LibArtDir(d); _snwprintf(f, 64, L"c%d_%s.%s", id, kind, ext); Join(out, d, f); }
static const wchar_t *const LIB_EXTS[] = {L"png", L"jpg", L"jpeg", L"bmp", L"gif"};
static BOOL LibArtFind(int id, const wchar_t *kind, wchar_t *out) { for (int e = 0; e < 5; e++) { LibArtPath(id, kind, LIB_EXTS[e], out); if (FileExists(out)) return TRUE; } out[0] = 0; return FALSE; }
static void LibArtDelete(int id, const wchar_t *kind) { wchar_t p[MAX_PATH]; for (int e = 0; e < 5; e++) { LibArtPath(id, kind, LIB_EXTS[e], p); DeleteFileW(p); } }
static BOOL LibArtCopy(int id, const wchar_t *kind, const wchar_t *src)   /* the picture keeps its own format (PNG, JPG...) */
{
    const wchar_t *ext = PathFindExtensionW(src); if (*ext == L'.') ext++;
    wchar_t e[8]; wcsncpy(e, ext, 7); e[7] = 0; _wcslwr(e);
    BOOL known = FALSE; for (int k = 0; k < 5; k++) if (!wcscmp(e, LIB_EXTS[k])) known = TRUE;
    if (!known) return FALSE;
    wchar_t dst[MAX_PATH]; LibArtDelete(id, kind); LibArtPath(id, kind, e, dst);
    return CopyFileW(src, dst, FALSE);
}

/* ---------------------------------------------------------------- the pictures in memory (ui_library.c draws them) */
static GpBitmap *g_cTile[MAXCUSTOM]; static int g_cTileS[MAXCUSTOM];   /* each tile at the home row's biggest size */
static GpBitmap *g_cBg; static int g_cBgId = -1, g_cBgW, g_cBgH;      /* ONE full-window background: the shown game's */
/* 6.1: LibArtRetry already looked this session. 6.1 review: by the game's id, and kept through LibArtFree (a scale or
   list change no longer makes every tile look again in its paint): at most once per session per game */
static int g_cArtRetryId[MAXCUSTOM * 4], g_cArtRetryN;
static volatile LONG g_cArtAdded;   /* 6.1 review: LibConsoleArt copied something: music.c looks again for the chosen game's music */
static void LibBmpFree(GpBitmap **b) { if (*b) { GdipDisposeImage((GpImage *)*b); *b = NULL; } }
static void LibArtFree(void)   /* the scale, the color or the list changed (ScaleChanged), or PORTHEON lets go of memory */
{
    for (int i = 0; i < MAXCUSTOM; i++) { LibBmpFree(&g_cTile[i]); g_cTileS[i] = 0; }
    LibBmpFree(&g_cBg); g_cBgId = -1;
}

/* ---------------------------------------------------------------- library.ini */
static void LibPutS(const wchar_t *ini, const wchar_t *sec, const wchar_t *k, const wchar_t *v)
{
    /* GetPrivateProfileString drops one pair of quotes around a value: every value goes inside "...", so an argument
       line such as "{rom}" keeps its own quotes */
    if (!v || !v[0]) { WritePrivateProfileStringW(sec, k, NULL, ini); return; }
    size_t n = wcslen(v) + 3; wchar_t *q = (wchar_t *)malloc(n * sizeof(wchar_t)); if (!q) return;
    _snwprintf(q, n, L"\"%s\"", v); q[n - 1] = 0; WritePrivateProfileStringW(sec, k, q, ini); free(q);
}
static void LibPutU(const wchar_t *ini, const wchar_t *sec, const wchar_t *k, unsigned long long v) { wchar_t b[32]; _snwprintf(b, 32, L"%llu", v); WritePrivateProfileStringW(sec, k, v ? b : NULL, ini); }
static void LibGetS(const wchar_t *ini, const wchar_t *sec, const wchar_t *k, wchar_t *out, int n) { GetPrivateProfileStringW(sec, k, L"", out, n, ini); }
static unsigned long long LibGetU(const wchar_t *ini, const wchar_t *sec, const wchar_t *k) { wchar_t b[32]; GetPrivateProfileStringW(sec, k, L"0", b, 32, ini); return _wcstoui64(b, NULL, 10); }
static void LibSaveOne(int i)
{
    if (i < 0 || i >= g_ncg) return;
    CGame *c = &g_cg[i]; wchar_t ini[MAX_PATH], sec[16]; LibIni(ini); _snwprintf(sec, 16, L"c%d", c->id);
    LibPutU(ini, sec, L"kind", (unsigned long long)c->kind); LibPutS(ini, sec, L"name", c->name); LibPutS(ini, sec, L"target", c->target);
    LibPutS(ini, sec, L"args", c->args); LibPutS(ini, sec, L"dir", c->dir); LibPutS(ini, sec, L"rom", c->rom); LibPutS(ini, sec, L"core", c->core);
    LibPutU(ini, sec, L"steam_appid", c->appid); LibPutU(ini, sec, L"steam_gameid", c->gameid);
    LibPutS(ini, sec, L"epic_app", c->epicApp); LibPutS(ini, sec, L"epic_ns", c->epicNs); LibPutS(ini, sec, L"epic_item", c->epicItem);
    LibPutS(ini, sec, L"watch_dir", c->watchDir); LibPutU(ini, sec, L"played_ms", c->playedMs);
    LibPutU(ini, sec, L"last", (unsigned long long)c->last); LibPutU(ini, sec, L"added", (unsigned long long)c->added);
}
static BOOL LibReadOne(const wchar_t *ini, int id, CGame *c)
{
    wchar_t sec[16]; _snwprintf(sec, 16, L"c%d", id); memset(c, 0, sizeof(*c)); c->id = id;
    c->kind = GetPrivateProfileIntW(sec, L"kind", 0, ini); if (c->kind < CK_STEAM || c->kind > CK_URL) return FALSE;
    LibGetS(ini, sec, L"name", c->name, 80); LibGetS(ini, sec, L"target", c->target, MAX_PATH); LibGetS(ini, sec, L"args", c->args, 512);
    LibGetS(ini, sec, L"dir", c->dir, MAX_PATH); LibGetS(ini, sec, L"rom", c->rom, MAX_PATH); LibGetS(ini, sec, L"core", c->core, MAX_PATH);
    c->appid = (unsigned)LibGetU(ini, sec, L"steam_appid"); c->gameid = LibGetU(ini, sec, L"steam_gameid"); if (!c->gameid) c->gameid = c->appid;
    LibGetS(ini, sec, L"epic_app", c->epicApp, 128); LibGetS(ini, sec, L"epic_ns", c->epicNs, 64); LibGetS(ini, sec, L"epic_item", c->epicItem, 64);
    LibGetS(ini, sec, L"watch_dir", c->watchDir, MAX_PATH); c->playedMs = LibGetU(ini, sec, L"played_ms");
    c->last = (long long)LibGetU(ini, sec, L"last"); c->added = (long long)LibGetU(ini, sec, L"added");
    if (!c->name[0]) wcscpy(c->name, L"?");
    return TRUE;
}
static void LibSaveOrder(void)
{
    wchar_t ini[MAX_PATH], ord[400] = L""; LibIni(ini);
    for (int i = 0; i < g_ncg; i++) { size_t k = wcslen(ord); _snwprintf(ord + k, 400 - k, L"%s%d", k ? L"," : L"", g_cg[i].id); }
    ord[399] = 0; WritePrivateProfileStringW(L"library", L"order", ord, ini);
}
static FILETIME g_libStamp;   /* library.ini's last write as read: an edit by hand (Editar opções) is read again */
static void LibStampNow(void) { wchar_t ini[MAX_PATH]; LibIni(ini); WIN32_FILE_ATTRIBUTE_DATA a; if (GetFileAttributesExW(ini, GetFileExInfoStandard, &a)) g_libStamp = a.ftLastWriteTime; }
static void LibLoad(void)   /* WinMain, after g_pIni */
{
    wchar_t ini[MAX_PATH], ord[400]; LibIni(ini);
    GetPrivateProfileStringW(L"library", L"order", L"", ord, 400, ini);
    g_ncg = 0; wchar_t *ctx = NULL;
    for (wchar_t *t = wcstok(ord, L",", &ctx); t && g_ncg < MAXCUSTOM; t = wcstok(NULL, L",", &ctx)) {
        int id = _wtoi(t); if (id <= 0 || LibIndex(id) >= 0) continue;
        CGame c; if (LibReadOne(ini, id, &c)) g_cg[g_ncg++] = c;
    }
    int sid = PIni(L"selected_custom", 0); g_csel = sid > 0 ? LibIndex(sid) : -1;
#ifdef PORTHEON_TEST   /* UI shots: PORTHEON_TEST_CSEL=<index> shows that custom game on the home screen */
    { wchar_t v[16]; if (GetEnvironmentVariableW(L"PORTHEON_TEST_CSEL", v, 16)) { int k = _wtoi(v); g_csel = k >= 0 && k < g_ncg ? k : -1; } }
#endif
    LibStampNow(); LibArtFree();
}
static void LibReload(void)   /* WM_ACTIVATE: library.ini changed outside (notepad): read again, keep the shown game */
{
    wchar_t ini[MAX_PATH]; LibIni(ini); WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExW(ini, GetFileExInfoStandard, &a) || !CompareFileTime(&a.ftLastWriteTime, &g_libStamp)) return;
    int keep = g_csel >= 0 && g_csel < g_ncg ? g_cg[g_csel].id : 0, run = g_crun;
    LibLoad(); g_csel = keep ? LibIndex(keep) : -1; g_crun = run;
}
static int LibNextId(void)
{
    wchar_t ini[MAX_PATH]; LibIni(ini); int id = GetPrivateProfileIntW(L"library", L"next_id", 1, ini);
    for (int i = 0; i < g_ncg; i++) if (g_cg[i].id >= id) id = g_cg[i].id + 1;
    if (id < 1) id = 1;
    wchar_t b[16]; _snwprintf(b, 16, L"%d", id + 1); WritePrivateProfileStringW(L"library", L"next_id", b, ini);
    return id;
}
/* the index of the new game; -1 the library is full, -2 the game is in it already (its index in *dupAt) */
static int LibAdd(const CGame *src, int *dupAt)
{
    if (dupAt) *dupAt = -1;
    for (int i = 0; i < g_ncg; i++) {
        const CGame *c = &g_cg[i]; BOOL same = FALSE;
        if (src->kind == CK_STEAM && c->kind == CK_STEAM) same = c->gameid == src->gameid;
        else if (src->kind == CK_EPIC && c->kind == CK_EPIC) same = !_wcsicmp(c->epicApp, src->epicApp);
        else if (src->kind == c->kind) same = !_wcsicmp(c->target, src->target) && !_wcsicmp(c->rom, src->rom);
        if (same) { if (dupAt) *dupAt = i; return -2; }
    }
    if (g_ncg >= MAXCUSTOM) return -1;
    CGame c = *src; c.id = LibNextId(); c.added = LibNow(); c.playedMs = 0; c.last = 0;
    g_cg[g_ncg++] = c; LibSaveOne(g_ncg - 1); LibSaveOrder(); LibStampNow();
    return g_ncg - 1;
}
static void LibRemove(int i)
{
    if (i < 0 || i >= g_ncg) return;
    int id = g_cg[i].id; wchar_t ini[MAX_PATH], sec[16]; LibIni(ini); _snwprintf(sec, 16, L"c%d", id);
    WritePrivateProfileStringW(sec, NULL, NULL, ini);   /* the whole [c<id>] section */
    LibArtDelete(id, L"icon"); LibArtDelete(id, L"pic"); LibArtDelete(id, L"exe");
    { wchar_t s0[MAX_PATH]; LibArtPath(id, L"snd0", L"at9", s0); DeleteFileW(s0); }   /* 6.1: its music copy (LibConsoleArt) */
    memmove(&g_cg[i], &g_cg[i + 1], (size_t)(g_ncg - i - 1) * sizeof(CGame)); g_ncg--;
    if (g_csel == i) g_csel = -1; else if (g_csel > i) g_csel--;
    LibArtFree(); LibSaveOrder(); LibStampNow();
}
static float g_cScale[MAXCUSTOM]; static int g_cTileArt[MAXCUSTOM];   /* ui_library.c's (tentative definitions of the same objects) */
static void LibMove(int i, int d)   /* one place left (-1) or right (+1) on the home row */
{
    int j = i + d; if (i < 0 || i >= g_ncg || j < 0 || j >= g_ncg) return;
    CGame t = g_cg[i]; g_cg[i] = g_cg[j]; g_cg[j] = t;
    GpBitmap *b = g_cTile[i]; g_cTile[i] = g_cTile[j]; g_cTile[j] = b; int s = g_cTileS[i]; g_cTileS[i] = g_cTileS[j]; g_cTileS[j] = s;
    s = g_cTileArt[i]; g_cTileArt[i] = g_cTileArt[j]; g_cTileArt[j] = s;   /* 6.1 review: the cached tile's kind goes with it */
    float f = g_cScale[i]; g_cScale[i] = g_cScale[j]; g_cScale[j] = f;
    if (g_csel == i) g_csel = j; else if (g_csel == j) g_csel = i;
    LibSaveOrder(); LibStampNow();
}

/* ---------------------------------------------------------------- icons, files */
static const wchar_t *EmuName(const wchar_t *exe) { const EmuPreset *e = EmuFor(exe); return e->name ? e->name : L2(L"Emulador", L"Emulator"); }
/* a program's own icon (256 px when it has one) as a PNG with its transparency */
static BOOL LibIconPng(const wchar_t *src, int index, const wchar_t *png)
{
    HICON hi = NULL; UINT iid = 0;
    if (PrivateExtractIconsW(src, index, 256, 256, &hi, &iid, 1, 0) != 1 || !hi) { hi = NULL; if (ExtractIconExW(src, index, &hi, NULL, 1) != 1 || !hi) return FALSE; }
    ICONINFO ii; if (!GetIconInfo(hi, &ii)) { DestroyIcon(hi); return FALSE; }
    BOOL ok = FALSE;
    if (ii.hbmColor) {
        BITMAP bm; memset(&bm, 0, sizeof(bm)); GetObjectW(ii.hbmColor, sizeof(bm), &bm);
        int w = bm.bmWidth, h = bm.bmHeight;
        if (w > 0 && h > 0 && w <= 1024 && h <= 1024) {
            BITMAPINFO bi; memset(&bi, 0, sizeof(bi)); bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
            bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
            DWORD *px = (DWORD *)malloc((size_t)w * h * 4), *mk = (DWORD *)malloc((size_t)w * h * 4); HDC dc = GetDC(NULL);
            if (px && mk && GetDIBits(dc, ii.hbmColor, 0, (UINT)h, px, &bi, DIB_RGB_COLORS) == h) {
                BOOL alpha = FALSE; for (int i = 0; i < w * h && !alpha; i++) if (px[i] >> 24) alpha = TRUE;
                if (!alpha) {   /* an old icon: the mask says what is see-through (white in the mask) */
                    BOOL m = ii.hbmMask && GetDIBits(dc, ii.hbmMask, 0, (UINT)h, mk, &bi, DIB_RGB_COLORS) == h;
                    for (int i = 0; i < w * h; i++) px[i] = (px[i] & 0xFFFFFF) | ((m && (mk[i] & 0xFFFFFF)) ? 0 : 0xFF000000u);
                }
                GpBitmap *b = NULL;
                if (!GdipCreateBitmapFromScan0(w, h, w * 4, 0x26200A /* PixelFormat32bppARGB */, (BYTE *)px, &b) && b) {
                    static const CLSID pngc = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
                    ok = !GdipSaveImageToFile((GpImage *)b, png, &pngc, NULL); GdipDisposeImage((GpImage *)b);
                }
            }
            ReleaseDC(NULL, dc); free(px); free(mk);
        }
        DeleteObject(ii.hbmColor);
    }
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    DestroyIcon(hi); return ok;
}
/* a .lnk's target, arguments' folder and icon (empty for an "advertised" installer shortcut) */
static void LibLnkRead(const wchar_t *lnk, wchar_t *target, wchar_t *wdir, wchar_t *icon, int *iconIdx)
{
    target[0] = wdir[0] = icon[0] = 0; *iconIdx = 0;
    IShellLinkW *sl = NULL; IPersistFile *pf = NULL;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl)) || !sl) return;
    if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf)) && pf) {
        if (SUCCEEDED(pf->lpVtbl->Load(pf, lnk, STGM_READ))) {
            sl->lpVtbl->GetPath(sl, target, MAX_PATH, NULL, 0); sl->lpVtbl->GetWorkingDirectory(sl, wdir, MAX_PATH);
            int ix = 0; if (SUCCEEDED(sl->lpVtbl->GetIconLocation(sl, icon, MAX_PATH, &ix))) *iconIdx = ix;
            wchar_t ex[MAX_PATH]; if (icon[0] && ExpandEnvironmentStringsW(icon, ex, MAX_PATH)) wcscpy(icon, ex);
        }
        pf->lpVtbl->Release(pf);
    }
    sl->lpVtbl->Release(sl);
}
/* where a Steam / Epic game is installed (for the watch), from the store's own lists */
static BOOL LibSteamDirOf(unsigned appid, wchar_t *out)
{
    LibCand *k = (LibCand *)malloc(sizeof(LibCand) * LIB_MAXCAND); if (!k) return FALSE;
    int n = LibSteamGames(k, LIB_MAXCAND); BOOL ok = FALSE;
    for (int i = 0; i < n && !ok; i++) if (k[i].appid == appid) { wcscpy(out, k[i].dir); ok = TRUE; }
    free(k); return ok;
}
static BOOL LibEpicDirOf(const wchar_t *app, wchar_t *out, wchar_t *exe)
{
    LibCand *k = (LibCand *)malloc(sizeof(LibCand) * LIB_MAXCAND); if (!k) return FALSE;
    int n = LibEpicGames(k, LIB_MAXCAND); BOOL ok = FALSE;
    for (int i = 0; i < n && !ok; i++) if (!_wcsicmp(k[i].epicApp, app)) { wcscpy(out, k[i].dir); if (exe) wcscpy(exe, k[i].exe); ok = TRUE; }
    free(k); return ok;
}
/* the biggest .exe straight inside a folder (a Steam game's icon when it has no cover) */
static BOOL LibBiggestExe(const wchar_t *dir, wchar_t *out)
{
    wchar_t pat[MAX_PATH]; Join(pat, dir, L"*.exe"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); ULONGLONG best = 0;
    out[0] = 0; if (h == INVALID_HANDLE_VALUE) return FALSE;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (StrStrIW(fd.cFileName, L"crash") || StrStrIW(fd.cFileName, L"unins") || StrStrIW(fd.cFileName, L"redist") || StrStrIW(fd.cFileName, L"setup")) continue;
        ULONGLONG sz = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow; if (sz > best) { best = sz; Join(out, dir, fd.cFileName); }
    } while (FindNextFileW(h, &fd));
    FindClose(h); return out[0] != 0;
}
/* an Epic launch link: apps/<ns>%3A<item>%3A<AppName> (or apps/<AppName>) -> the three parts */
static void LibEpicFromUrl(const wchar_t *url, CGame *c)
{
    const wchar_t *a = StrStrIW(url, L"/apps/"); if (!a) return; a += 6;
    wchar_t s[400]; int k = 0; while (*a && *a != L'?' && k < 399) s[k++] = *a++; s[k] = 0;
    wchar_t *parts[3] = {s, NULL, NULL}; int np = 1;
    for (wchar_t *p = s; *p && np < 3; ) { if (!_wcsnicmp(p, L"%3A", 3) || *p == L':') { int sk = *p == L':' ? 1 : 3; *p = 0; parts[np++] = p + sk; p += sk; } else p++; }
    if (np == 3) { wcsncpy(c->epicNs, parts[0], 63); wcsncpy(c->epicItem, parts[1], 63); wcsncpy(c->epicApp, parts[2], 127); }
    else wcsncpy(c->epicApp, parts[np - 1], 127);
}
/* a picked file (.exe, .lnk, .url, .bat, .cmd) as a game; *iconSrc / *iconIdx: where its icon comes from */
static BOOL LibFromFile(const wchar_t *path, CGame *c, wchar_t *iconSrc, int *iconIdx)
{
    memset(c, 0, sizeof(*c)); iconSrc[0] = 0; *iconIdx = 0;
    const wchar_t *ext = PathFindExtensionW(path);
    if (!_wcsicmp(ext, L".exe")) {
        c->kind = CK_EXE; wcsncpy(c->target, path, MAX_PATH - 1); wcscpy(c->dir, path); PathRemoveFileSpecW(c->dir); wcscpy(c->watchDir, c->dir);
        LibNiceName(path, c->name, 80); wcscpy(iconSrc, path); return TRUE;
    }
    if (!_wcsicmp(ext, L".lnk") || !_wcsicmp(ext, L".bat") || !_wcsicmp(ext, L".cmd")) {
        c->kind = CK_LNK; wcsncpy(c->target, path, MAX_PATH - 1);
        wcsncpy(c->name, PathFindFileNameW(path), 79); LibStripExt(c->name);
        if (!_wcsicmp(ext, L".lnk")) {
            wchar_t t[MAX_PATH], wd[MAX_PATH], ic[MAX_PATH]; int ix; LibLnkRead(path, t, wd, ic, &ix);
            if (t[0]) { wcscpy(c->watchDir, t); PathRemoveFileSpecW(c->watchDir); if (!_wcsicmp(PathFindExtensionW(t), L".exe")) { wcscpy(iconSrc, t); *iconIdx = 0; } }
            if (ic[0] && FileExists(ic)) { wcscpy(iconSrc, ic); *iconIdx = ix; }
            if (wd[0]) wcscpy(c->dir, wd);
        } else { wcscpy(c->watchDir, path); PathRemoveFileSpecW(c->watchDir); wcscpy(c->dir, c->watchDir); }
        return TRUE;
    }
    if (!_wcsicmp(ext, L".url")) {
        wchar_t url[1024]; GetPrivateProfileStringW(L"InternetShortcut", L"URL", L"", url, 1024, path);
        wcsncpy(c->name, PathFindFileNameW(path), 79); LibStripExt(c->name);
        { wchar_t ic[MAX_PATH]; GetPrivateProfileStringW(L"InternetShortcut", L"IconFile", L"", ic, MAX_PATH, path);
          if (ic[0] && FileExists(ic)) { wcscpy(iconSrc, ic); *iconIdx = GetPrivateProfileIntW(L"InternetShortcut", L"IconIndex", 0, path); } }
        if (!_wcsnicmp(url, L"steam://rungameid/", 18)) {   /* a Steam desktop shortcut: the game itself, with its covers */
            c->kind = CK_STEAM; c->gameid = _wcstoui64(url + 18, NULL, 10); c->appid = c->gameid >> 32 ? (unsigned)(c->gameid >> 32) : (unsigned)c->gameid;
            if (!c->gameid) return FALSE;
            if (!(c->gameid >> 32)) LibSteamDirOf(c->appid, c->watchDir);
            return TRUE;
        }
        if (!_wcsnicmp(url, L"com.epicgames.launcher://", 25)) {   /* an Epic desktop shortcut */
            c->kind = CK_EPIC; LibEpicFromUrl(url, c); if (!c->epicApp[0]) return FALSE;
            wchar_t exe[MAX_PATH] = L""; if (LibEpicDirOf(c->epicApp, c->watchDir, exe) && exe[0] && !iconSrc[0]) wcscpy(iconSrc, exe);
            return TRUE;
        }
        c->kind = CK_URL; wcsncpy(c->target, path, MAX_PATH - 1); return url[0] != 0;
    }
    return FALSE;
}
/* the pictures of a new game: Steam's cover and hero picture; the program's icon for everything else */
static BOOL LibConsoleArt(int i);   /* 6.1: a console dump's own pictures and music (below LibWatchDirOk) */
static void LibArtImport(int i, const wchar_t *iconSrc, int iconIdx)
{
    if (i < 0 || i >= g_ncg) return;
    CGame *c = &g_cg[i]; wchar_t src[MAX_PATH], dst[MAX_PATH]; BOOL cover = FALSE;
    if (c->kind == CK_STEAM && c->appid) {
        if (LibSteamArtFind(c->appid, 0, src)) cover = LibArtCopy(c->id, L"icon", src);
        if (LibSteamArtFind(c->appid, 1, src)) LibArtCopy(c->id, L"pic", src);
    }
    LibConsoleArt(i);   /* 6.1: an emulator's PS4/PS3 dump, or a port's folder with one */
    wchar_t ex[MAX_PATH] = L""; int ix = iconIdx;
    if (iconSrc && iconSrc[0]) wcscpy(ex, iconSrc);
    else if (c->kind == CK_STEAM && !cover && c->watchDir[0]) { LibBiggestExe(c->watchDir, ex); ix = 0; }
    if (ex[0]) { LibArtPath(c->id, L"exe", L"png", dst); LibIconPng(ex, ix, dst); }
    LibBmpFree(&g_cTile[i]); g_cTileS[i] = 0; if (g_cBgId == c->id) { LibBmpFree(&g_cBg); g_cBgId = -1; }
}
/* a row of the Steam / Epic / non-Steam lists as a game */
static void LibFromCand(const LibCand *k, CGame *c)
{
    memset(c, 0, sizeof(*c)); c->kind = k->kind; wcscpy(c->name, k->name); wcscpy(c->watchDir, k->dir);
    c->appid = k->appid; c->gameid = k->gameid;
    wcscpy(c->epicApp, k->epicApp); wcscpy(c->epicNs, k->epicNs); wcscpy(c->epicItem, k->epicItem);
    if (k->kind == CK_EPIC) wcscpy(c->target, k->exe);   /* only for the icon and "Abrir pasta": Epic starts it */
}
/* a candidate already in the library (the list says "Na biblioteca") */
static BOOL LibHasCand(const LibCand *k)
{
    for (int i = 0; i < g_ncg; i++) {
        if (k->kind == CK_STEAM && g_cg[i].kind == CK_STEAM && g_cg[i].gameid == k->gameid) return TRUE;
        if (k->kind == CK_EPIC && g_cg[i].kind == CK_EPIC && !_wcsicmp(g_cg[i].epicApp, k->epicApp)) return TRUE;
    }
    return FALSE;
}

/* ---------------------------------------------------------------- starting a game and watching it */
/* 6.1 review: PORTHEON closes only processes it started itself. The process PLAY started (CreateProcess, or the shell for
   a shortcut, a .bat or an elevated program) and everything that one starts are held by handles in g_cTr (PTree,
   portheon.c: a child only when created after its parent, ids kept valid by the handles). A game that a store or a
   launcher outside that tree starts is only watched (play time), by its folder, and only when the folder is narrow:
   STOP is not offered for it ("Aberto": the player closes it from the game). */
static CRITICAL_SECTION g_cTrCs; static BOOL g_cTrInit; static PTree g_cTr;
static volatile LONG g_cCanStop;   /* a live process of g_cTr that PORTHEON may close: STOP is offered */
static BOOL g_cMin;                /* PORTHEON minimized itself for the game: it comes back when the game ends */
typedef struct { int id; HANDLE h; BOOL stoppable; ULONGLONG start; wchar_t dir[MAX_PATH]; } LibWatchArg;
static BOOL LibUnder(const wchar_t *child, const wchar_t *parent)   /* child is parent or inside it (whole names only) */
{
    size_t pl = wcslen(parent); while (pl && (parent[pl - 1] == L'\\' || parent[pl - 1] == L'/')) pl--;
    if (!pl || _wcsnicmp(child, parent, pl)) return FALSE;
    return child[pl] == 0 || child[pl] == L'\\' || child[pl] == L'/';
}
static BOOL LibSameDir(const wchar_t *a, const wchar_t *b) { return LibUnder(a, b) && LibUnder(b, a); }
static BOOL LibDirIsChildOf(const wchar_t *d, const wchar_t *parent)   /* d is straight inside parent (one level) */
{
    if (!LibUnder(d, parent) || LibSameDir(d, parent)) return FALSE;
    size_t pl = wcslen(parent); while (pl && (parent[pl - 1] == L'\\' || parent[pl - 1] == L'/')) pl--;
    const wchar_t *r = d + pl + 1; const wchar_t *s = wcschr(r, L'\\'); return !s || !s[1];
}
/* a folder narrow enough to find the game by: not a drive, not Windows, not Program Files, the user folders (Desktop,
   Documents, Downloads, C:\Users, a profile's own folders) or the stores' library roots, not PORTHEON's folder */
/* 6.1 review: the stores' roots (each Steam library, its steamapps and steamapps\common; the folder above each Epic game)
   read once and kept for 5 minutes: reading Steam's libraryfolders and every Epic manifest at each call stalled the tile
   paint (LibArtRetry -> LibConsoleArt). Shared by the UI and the watch threads (an SRW lock) */
#define LIB_ROOTS_MAX (16 * 3 + LIB_MAXCAND)
static SRWLOCK g_libRootsLock = SRWLOCK_INIT;
static wchar_t (*g_libRoots)[MAX_PATH]; static int g_libRootsN; static ULONGLONG g_libRootsAt;
static BOOL LibStoreRoot(const wchar_t *d)
{
    AcquireSRWLockExclusive(&g_libRootsLock);
    ULONGLONG now = GetTickCount64();
    if (!g_libRoots) g_libRoots = (wchar_t (*)[MAX_PATH])calloc(LIB_ROOTS_MAX, sizeof(*g_libRoots));
    if (g_libRoots && (!g_libRootsAt || now - g_libRootsAt > 5 * 60 * 1000)) {
        int n = 0; wchar_t sp[MAX_PATH], libs[16][MAX_PATH];
        if (SteamFindPath(sp, MAX_PATH)) {
            int nl = LibSteamLibs(sp, libs, 16);
            for (int l = 0; l < nl && n + 3 <= LIB_ROOTS_MAX; l++) {
                wcscpy(g_libRoots[n], libs[l]); Join(g_libRoots[n + 1], libs[l], L"steamapps"); Join(g_libRoots[n + 2], g_libRoots[n + 1], L"common"); n += 3;
            }
        }
        LibCand *k = (LibCand *)malloc(sizeof(LibCand) * LIB_MAXCAND);
        if (k) {
            int ne = LibEpicGames(k, LIB_MAXCAND);
            for (int i = 0; i < ne && n < LIB_ROOTS_MAX; i++) { wcscpy(g_libRoots[n], k[i].dir); PathRemoveFileSpecW(g_libRoots[n]); if (g_libRoots[n][0]) n++; }
            free(k);
        }
        g_libRootsN = n; g_libRootsAt = now ? now : 1;
    }
    BOOL hit = FALSE;
    for (int i = 0; g_libRoots && i < g_libRootsN && !hit; i++) hit = LibSameDir(d, g_libRoots[i]);
    ReleaseSRWLockExclusive(&g_libRootsLock);
    return hit;
}
static BOOL LibWatchDirOk(const wchar_t *d0)
{
    wchar_t d[MAX_PATH]; wcsncpy(d, d0, MAX_PATH - 1); d[MAX_PATH - 1] = 0; StripSlash(d);
    if (!d[0] || wcslen(d) <= 3 || PathIsRootW(d) || !DirExists(d)) return FALSE;
    wchar_t w[MAX_PATH]; if (GetWindowsDirectoryW(w, MAX_PATH) && LibUnder(d, w)) return FALSE;
    static const wchar_t *const env[] = {L"ProgramFiles", L"ProgramFiles(x86)", L"ProgramW6432", L"ProgramData", L"USERPROFILE", L"APPDATA", L"LOCALAPPDATA", L"PUBLIC", L"TEMP", L"TMP"};
    for (int i = 0; i < (int)(sizeof(env) / sizeof(env[0])); i++) { wchar_t v[MAX_PATH]; DWORD n = GetEnvironmentVariableW(env[i], v, MAX_PATH); if (n && n < MAX_PATH && LibSameDir(d, v)) return FALSE; }
    {   /* C:\Users, every profile in it, and every folder straight inside the player's profile (Desktop, Documents...) */
        wchar_t up[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"USERPROFILE", up, MAX_PATH);
        if (n && n < MAX_PATH) {
            StripSlash(up); if (LibDirIsChildOf(d, up)) return FALSE;
            wchar_t users[MAX_PATH]; wcscpy(users, up); PathRemoveFileSpecW(users);
            if (users[0] && (LibSameDir(d, users) || LibDirIsChildOf(d, users))) return FALSE;
            wchar_t ad[MAX_PATH]; Join(ad, up, L"AppData\\Local\\Programs"); if (LibSameDir(d, ad)) return FALSE;
        }
        static const KNOWNFOLDERID *const kf[] = {&FOLDERID_Desktop, &FOLDERID_Documents, &FOLDERID_Downloads, &FOLDERID_Music, &FOLDERID_Pictures, &FOLDERID_Videos, &FOLDERID_PublicDesktop, &FOLDERID_SavedGames, &FOLDERID_Profile};
        for (int i = 0; i < (int)(sizeof(kf) / sizeof(kf[0])); i++) { PWSTR k = NULL; if (SHGetKnownFolderPath(kf[i], 0, NULL, &k) == S_OK && k) { BOOL bad = LibSameDir(d, k); CoTaskMemFree(k); if (bad) return FALSE; } }
    }
    {   /* the stores' roots: Steam, each Steam library, its steamapps and steamapps\common; the folder of Epic's games */
        const wchar_t *leaf = PathFindFileNameW(d);
        if (!_wcsicmp(leaf, L"steamapps") || !_wcsicmp(leaf, L"common") || !_wcsicmp(leaf, L"SteamLibrary") || !_wcsicmp(leaf, L"Epic Games") || !_wcsicmp(leaf, L"Steam")) return FALSE;
        if (LibStoreRoot(d)) return FALSE;   /* 6.1 review: cached (LibStoreRoot) */
    }
    wchar_t e[MAX_PATH]; wcscpy(e, g_exeDir); StripSlash(e);
    if (LibUnder(e, d) || LibUnder(d, e)) return FALSE;   /* PORTHEON's own folder, one above it or one inside it */
    return TRUE;
}
/* 6.1 (the user: "the real pictures and the music of each game added, automatically"): a game given to an emulator
   (or a native port's folder) that is the player's own console dump carries its own pictures and theme, as the UFCs':
   PS4 sce_sys\icon0.png (cover), pic1.png/pic0.png (background), snd0.at9 (music, music.c MUS_CUSTOM), PS3
   PS3_GAME\ICON0.PNG / PIC1.PNG (RPCS3's EBOOT.BIN sits in PS3_GAME\USRDIR, two folders below). Copied to
   %LOCALAPPDATA%\PORTHEON\art like Steam's. Only what is missing is filled: a picture the player chose stays. Looked for
   in the game's folder and one folder down, never in a drive root, a profile or a store folder (LibWatchDirOk). */
static BOOL LibFindIn(const wchar_t *d, const wchar_t *const *names, int n, wchar_t *out)
{
    wchar_t p[MAX_PATH];
    for (int i = 0; i < n; i++) { Join(p, d, names[i]); if (FileExists(p)) { wcscpy(out, p); return TRUE; } }
    wchar_t pat[MAX_PATH]; Join(pat, d, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); BOOL got = FALSE; int seen = 0;
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
        wchar_t sub[MAX_PATH]; Join(sub, d, fd.cFileName);
        for (int i = 0; i < n && !got; i++) { Join(p, sub, names[i]); if (FileExists(p)) { wcscpy(out, p); got = TRUE; } }
    } while (!got && ++seen < 96 && FindNextFileW(h, &fd));
    FindClose(h); return got;
}
static BOOL LibConsoleArt(int i)   /* TRUE when a picture or the music was added */
{
    if (i < 0 || i >= g_ncg) return FALSE;
    CGame *c = &g_cg[i]; wchar_t base[MAX_PATH] = L"";
    if (c->kind == CK_EMU && c->rom[0]) { wcscpy(base, c->rom); if (!DirExists(base)) PathRemoveFileSpecW(base); }
    else if (c->kind == CK_EXE && c->target[0]) { wcscpy(base, c->target); PathRemoveFileSpecW(base); }
    else return FALSE;
    if (c->kind == CK_EMU && !_wcsicmp(PathFindFileNameW(c->rom), L"EBOOT.BIN")) {   /* PS3: ...\PS3_GAME\USRDIR\EBOOT.BIN */
        wchar_t up[MAX_PATH]; wcscpy(up, base);
        if (!_wcsicmp(PathFindFileNameW(up), L"USRDIR")) { PathRemoveFileSpecW(up); if (!_wcsicmp(PathFindFileNameW(up), L"PS3_GAME")) PathRemoveFileSpecW(up); wcscpy(base, up); }
    }
    if (!LibWatchDirOk(base)) return FALSE;
    static const wchar_t *const ico[] = {L"sce_sys\\icon0.png", L"PS3_GAME\\ICON0.PNG", L"ICON0.PNG"},
                         *const pic[] = {L"sce_sys\\pic1.png", L"sce_sys\\pic0.png", L"PS3_GAME\\PIC1.PNG", L"PS3_GAME\\PIC0.PNG", L"PIC1.PNG"},
                         *const snd[] = {L"sce_sys\\snd0.at9"};
    wchar_t have[MAX_PATH], src[MAX_PATH]; BOOL added = FALSE;
    if (!LibArtFind(c->id, L"icon", have) && LibFindIn(base, ico, 3, src) && LibArtCopy(c->id, L"icon", src)) added = TRUE;
    if (!LibArtFind(c->id, L"pic", have) && LibFindIn(base, pic, 5, src) && LibArtCopy(c->id, L"pic", src)) added = TRUE;
    LibArtPath(c->id, L"snd0", L"at9", have);
    if (!FileExists(have) && LibFindIn(base, snd, 1, src) && CopyFileW(src, have, FALSE)) added = TRUE;
    if (added) InterlockedExchange(&g_cArtAdded, 1);   /* 6.1 review: MusicFollow decides again (the music may be new) */
    return added;
}
/* the pictures Steam had not downloaded yet when the game was added, and a dump's own art: looked for again once per
   session for a game still missing its cover or its background (CTileBmp and CustomBg call it) */
static void LibArtRetry(int i)
{
    if (i < 0 || i >= g_ncg) return;
    CGame *c = &g_cg[i];
    for (int k = 0; k < g_cArtRetryN; k++) if (g_cArtRetryId[k] == c->id) return;   /* 6.1 review: once per session per game */
    if (g_cArtRetryN < (int)(sizeof(g_cArtRetryId) / sizeof(g_cArtRetryId[0]))) g_cArtRetryId[g_cArtRetryN++] = c->id;
    else return;   /* the list is full (games added and removed many times in one session): no more looks */
    wchar_t have[MAX_PATH], src[MAX_PATH];
    if (c->kind == CK_STEAM && c->appid) {
        if (!LibArtFind(c->id, L"icon", have) && LibSteamArtFind(c->appid, 0, src)) LibArtCopy(c->id, L"icon", src);
        if (!LibArtFind(c->id, L"pic", have) && LibSteamArtFind(c->appid, 1, src)) LibArtCopy(c->id, L"pic", src);
    } else LibConsoleArt(i);
}
/* a process started after PLAY (born >= start) whose program is inside dir (not PORTHEON, not a store client): a handle
   to wait on (watch only) */
static HANDLE LibFindUnder(const wchar_t *dir, ULONGLONG start)
{
    if (wcslen(dir) < 4) return NULL;
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (sn == INVALID_HANDLE_VALUE) return NULL;
    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe); HANDLE found = NULL; DWORD self = GetCurrentProcessId();
    if (Process32FirstW(sn, &pe)) do {
        if (!pe.th32ProcessID || pe.th32ProcessID == self || ProcShared(pe.szExeFile)) continue;
        HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!h) h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!h) continue;
        wchar_t img[MAX_PATH * 2]; DWORD n = MAX_PATH * 2;
        if (QueryFullProcessImageNameW(h, 0, img, &n) && LibUnder(img, dir) && !LibSameDir(img, dir) && ProcBornH(h) >= start) found = h;
        else CloseHandle(h);
    } while (!found && Process32NextW(sn, &pe));
    CloseHandle(sn); return found;
}
static void LibWaitOne(HANDLE h)   /* a handle without SYNCHRONIZE (an elevated game): looked at every 2 s, held */
{
    if (WaitForSingleObject(h, INFINITE) != WAIT_FAILED) return;
    DWORD code; while (GetExitCodeProcess(h, &code) && code == STILL_ACTIVE) Sleep(2000);
}
static ULONGLONG LibNowFt(void) { FILETIME f; GetSystemTimeAsFileTime(&f); return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; }
/* waits for the game: PORTHEON's own processes (g_cTr) while one lives, looking for their new children every second;
   then, for a launcher or a store that hands over to the game, a process found inside the game's folder (narrow folders
   only): up to 120 s at the start, 6 s after each end */
static DWORD WINAPI LibWatchThread(void *p)
{
    LibWatchArg *a = (LibWatchArg *)p; BOOL seen = FALSE, dirOk = a->dir[0] && LibWatchDirOk(a->dir); ULONGLONG t0 = 0; LONG canStop = 0;
    for (;;) {
        HANDLE live[MAXIMUM_WAIT_OBJECTS]; BOOL anyKill = FALSE; int nl;
        EnterCriticalSection(&g_cTrCs); PTreeScan(&g_cTr); nl = PTreeAlive(&g_cTr, live, MAXIMUM_WAIT_OBJECTS, &anyKill); LeaveCriticalSection(&g_cTrCs);
        LONG cs = nl && anyKill && a->stoppable;
        if (cs != canStop) { canStop = cs; InterlockedExchange(&g_cCanStop, cs); PostMessageW(g_wnd, WM_APP_LIBWATCH, (WPARAM)(3 | (a->id << 8)), 0); }
        if (!seen && nl) { seen = TRUE; t0 = GetTickCount64(); PostMessageW(g_wnd, WM_APP_LIBWATCH, (WPARAM)(1 | (a->id << 8)), 0); }
        if (nl) { WaitForMultipleObjects((DWORD)nl, live, FALSE, 1000); continue; }   /* the handles stay g_cTr's: only this thread frees them */
        HANDLE w = NULL;
        if (dirOk) {
            ULONGLONG s0 = GetTickCount64(), limit = seen ? 6000 : 120000;
            while (!(w = LibFindUnder(a->dir, a->start)) && GetTickCount64() - s0 < limit) Sleep(seen ? 1000 : 2000);
        }
        if (!w) break;
        if (!seen) { seen = TRUE; t0 = GetTickCount64(); PostMessageW(g_wnd, WM_APP_LIBWATCH, (WPARAM)(1 | (a->id << 8)), 0); }
        LibWaitOne(w); CloseHandle(w);
    }
    EnterCriticalSection(&g_cTrCs); PTreeFree(&g_cTr); LeaveCriticalSection(&g_cTrCs);
    InterlockedExchange(&g_cCanStop, 0);
    ULONGLONG played = seen ? GetTickCount64() - t0 : 0;
    PostMessageW(g_wnd, WM_APP_LIBWATCH, (WPARAM)(2 | (a->id << 8) | (seen ? 0x80 : 0)), (LPARAM)(played / 1000));
    free(a); return 0;
}
/* the kinds PORTHEON starts as a process of its own (a store or a link's program is the store's) */
static BOOL LibStoppable(const CGame *c) { return c->kind == CK_EXE || c->kind == CK_EMU || c->kind == CK_LNK; }
/* STOP is offered now: the game shown runs and a process of PORTHEON's own tree that it may close is alive */
static BOOL LibCanStop(const CGame *c) { return g_crun == c->id && LibStoppable(c) && g_cCanStop; }
static void PadMapEnvC(int id); static void CkStart(const CGame *c);   /* 6.2: ui_padmap62.c, ui_cinput.c */
static void LibPlay(int i)
{
    if (i < 0 || i >= g_ncg) return;
    CGame *c = &g_cg[i];
    if (g_proc) { Msg(C_ORANGE, L"%s", L2(L"Feche o jogo que está aberto antes de abrir outro.", L"Close the game that is running before starting another.")); return; }
    if (g_crun) { LibBusyMsg(); return; }
    HANDLE hp = NULL; BOOL ok = FALSE; DWORD err = 0; wchar_t watch[MAX_PATH]; wcscpy(watch, c->watchDir);
    ULONGLONG start = LibNowFt();   /* a process found by its folder counts only when created after this */
    MusicStop(TRUE); PadPause(TRUE);   /* the controller is the game's (Steam Input, the emulator), as for the built-in games */
    if (c->kind != CK_STEAM && c->kind != CK_EPIC) PadMapEnvC(c->id);   /* 6.2: the player's buttons, for what is started from here and reads the pad through SDL */
    switch (c->kind) {
    case CK_STEAM: { wchar_t u[96]; _snwprintf(u, 96, L"steam://rungameid/%llu", c->gameid ? c->gameid : (unsigned long long)c->appid);
                     ok = (INT_PTR)ShellExecuteW(g_wnd, L"open", u, NULL, NULL, SW_SHOWNORMAL) > 32; if (!ok) err = GetLastError(); break; }
    case CK_EPIC: { wchar_t u[512];
                    if (c->epicNs[0] && c->epicItem[0]) _snwprintf(u, 512, L"com.epicgames.launcher://apps/%s%%3A%s%%3A%s?action=launch&silent=true", c->epicNs, c->epicItem, c->epicApp);
                    else _snwprintf(u, 512, L"com.epicgames.launcher://apps/%s?action=launch&silent=true", c->epicApp);
                    u[511] = 0; ok = (INT_PTR)ShellExecuteW(g_wnd, L"open", u, NULL, NULL, SW_SHOWNORMAL) > 32; if (!ok) err = GetLastError(); break; }
    case CK_EXE: case CK_EMU: {
        wchar_t cmd[2048], args[1600], d[MAX_PATH];
        if (!FileExists(c->target)) { err = ERROR_FILE_NOT_FOUND; break; }
        if (c->kind == CK_EMU && c->rom[0] && !FileExists(c->rom) && !DirExists(c->rom)) { err = ERROR_PATH_NOT_FOUND; break; }
        if (!LibBuildArgs(c, args, 1600) || !LibBuildCmd(c, cmd, 2048)) { err = ERROR_BAD_PATHNAME; break; }
        if (c->dir[0] && DirExists(c->dir)) wcscpy(d, c->dir); else { wcscpy(d, c->target); PathRemoveFileSpecW(d); }
        STARTUPINFOW si; memset(&si, 0, sizeof(si)); si.cb = sizeof(si); PROCESS_INFORMATION pi;
        if (CreateProcessW(c->target, cmd, NULL, NULL, FALSE, 0, NULL, d, &si, &pi)) { CloseHandle(pi.hThread); hp = pi.hProcess; ok = TRUE; }
        else if ((err = GetLastError()) == ERROR_ELEVATION_REQUIRED) {   /* "run as administrator" programs: through the shell (UAC) */
            SHELLEXECUTEINFOW se; memset(&se, 0, sizeof(se)); se.cbSize = sizeof(se); se.fMask = SEE_MASK_NOCLOSEPROCESS; se.hwnd = g_wnd;
            se.lpFile = c->target; se.lpParameters = args[0] ? args : NULL; se.lpDirectory = d; se.nShow = SW_SHOWNORMAL;
            ok = ShellExecuteExW(&se); hp = ok ? se.hProcess : NULL; if (!ok) err = GetLastError();
        }
        if (!watch[0]) { wcscpy(watch, c->target); PathRemoveFileSpecW(watch); }
        break; }
    default: {   /* .lnk, .bat, .url: as a double-click on them */
        SHELLEXECUTEINFOW se; memset(&se, 0, sizeof(se)); se.cbSize = sizeof(se); se.fMask = SEE_MASK_NOCLOSEPROCESS; se.hwnd = g_wnd;
        se.lpFile = c->target; se.lpDirectory = c->dir[0] && DirExists(c->dir) ? c->dir : NULL; se.nShow = SW_SHOWNORMAL;
        if (!FileExists(c->target)) { err = ERROR_FILE_NOT_FOUND; break; }
        ok = ShellExecuteExW(&se); hp = ok ? se.hProcess : NULL; if (!ok) err = GetLastError();
        break; }
    }
    EnvPopAll();
    if (!ok) {
        PadPause(FALSE);
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) Msg(C_BAD, L2(L"O %s não está mais onde estava (%s). Remova e adicione de novo.", L"%s is no longer where it was (%s). Remove it and add it again."), c->name, c->kind == CK_EMU && c->rom[0] && FileExists(c->target) ? c->rom : c->target);
        else if (err == ERROR_BAD_PATHNAME) Msg(C_BAD, L"%s", L2(L"Um caminho deste jogo tem aspas (\"): o PORTHEON não consegue montar o comando. Mude o nome da pasta.", L"A path of this game holds a quote (\"): PORTHEON cannot build the command. Rename the folder."));
        else if (c->kind == CK_STEAM) Msg(C_BAD, L"%s", L2(L"A Steam não abriu: ela está instalada neste PC?", L"Steam did not open: is it installed on this PC?"));
        else if (c->kind == CK_EPIC) Msg(C_BAD, L"%s", L2(L"O Epic Games Launcher não abriu: ele está instalado neste PC?", L"The Epic Games Launcher did not open: is it installed on this PC?"));
        else Msg(C_BAD, T(S_CANTSTART), c->name, err);
        return;
    }
    c->last = LibNow(); LibSaveOne(i); LibStampNow();
    g_crun = c->id; g_cMin = FALSE; InterlockedExchange(&g_cCanStop, 0);
    CkStart(c);   /* 6.2: the player's keys */
    if (!g_cTrInit) { InitializeCriticalSection(&g_cTrCs); g_cTrInit = TRUE; }
    if (hp) {   /* a store's client or the shell (a shortcut to steam.exe, a folder) is not the game: never held, never closed */
        wchar_t img[MAX_PATH * 2]; DWORD n = MAX_PATH * 2;
        if (QueryFullProcessImageNameW(hp, 0, img, &n) && ProcShared(img)) { CloseHandle(hp); hp = NULL; }
    }
    LibWatchArg *a = (LibWatchArg *)calloc(1, sizeof(LibWatchArg));
    if (a) {
        a->id = c->id; a->h = NULL; a->stoppable = LibStoppable(c); a->start = start; wcscpy(a->dir, watch);
        EnterCriticalSection(&g_cTrCs); PTreeFree(&g_cTr); if (hp) PTreeAddH(&g_cTr, hp, a->stoppable); LeaveCriticalSection(&g_cTrCs);
        hp = NULL;   /* g_cTr's now */
        HANDLE th = CreateThread(NULL, 0, LibWatchThread, a, 0, NULL);
        if (th) CloseHandle(th);
        else { EnterCriticalSection(&g_cTrCs); PTreeFree(&g_cTr); LeaveCriticalSection(&g_cTrCs); free(a); g_crun = 0; PadPause(FALSE); }
    }
    else { if (hp) CloseHandle(hp); g_crun = 0; PadPause(FALSE); }
    Msg(C_GOOD, c->kind == CK_STEAM ? L2(L"Abrindo o %s pela Steam...", L"Starting %s through Steam...") : c->kind == CK_EPIC ? L2(L"Abrindo o %s pela Epic Games...", L"Starting %s through Epic Games...") : T(S_STARTING), c->name);
}
static void LibStop(int i)
{
    if (i < 0 || i >= g_ncg || g_crun != g_cg[i].id) return;
    if (!LibCanStop(&g_cg[i])) { Msg(C_SUB, L2(L"O %s está aberto: feche pelo próprio jogo (ou pela loja).", L"%s is open: close it from the game itself (or its store)."), g_cg[i].name); return; }
    EnterCriticalSection(&g_cTrCs); PTreeKill(&g_cTr); LeaveCriticalSection(&g_cTrCs);   /* only the processes PORTHEON started */
}
/* PLAY with a custom game shown (Play, portheon.c): start it, or stop it while it runs */
static void LibPlayToggle(void)
{
    if (g_csel < 0 || g_csel >= g_ncg) return;
    if (g_crun == g_cg[g_csel].id) LibStop(g_csel); else LibPlay(g_csel);
}
static BOOL LibBusyMsg(void)   /* Play of a built-in game while a custom one runs */
{
    if (!g_crun) return FALSE;
    int r = LibIndex(g_crun);
    if (r >= 0) Msg(C_ORANGE, L2(L"O %s já está aberto: feche esse jogo antes de abrir outro.", L"%s is already running: close it before starting another game."), g_cg[r].name);
    else Msg(C_ORANGE, L"%s", L2(L"Um jogo seu já está aberto: feche esse jogo antes de abrir outro.", L"One of your games is already running: close it before starting another game."));
    return TRUE;
}
static void LibOnWatch(WPARAM wp, LPARAM lp)   /* WM_APP_LIBWATCH */
{
    int ev = (int)(wp & 0x7f), id = (int)(wp >> 8), i = LibIndex(id); BOOL seen = (wp & 0x80) != 0;
    if (ev == 3) { InvalidateRect(g_wnd, NULL, FALSE); return; }   /* STOP is offered, or not any more */
    if (ev == 1) {   /* the game is running: PORTHEON goes quiet (no animation timer, no update check) and steps aside */
        if (g_crun != id) return;
        KillTimer(g_wnd, 7); KillTimer(g_wnd, 9);
        if (PIni(L"lib_minimize", 1) && !IsIconic(g_wnd)) { g_cMin = TRUE; ShowWindow(g_wnd, SW_MINIMIZE); }
        return;
    }
    if (i >= 0 && lp > 0) { g_cg[i].playedMs += (ULONGLONG)lp * 1000; LibSaveOne(i); LibStampNow(); }
    if (g_crun == id) g_crun = 0;
    InterlockedExchange(&g_cCanStop, 0);
    PadPause(FALSE); SetTimer(g_wnd, 7, 30, NULL); SetTimer(g_wnd, 9, 6 * 3600 * 1000, NULL);
    if (g_cMin) { g_cMin = FALSE; ShowWindow(g_wnd, SW_RESTORE); SetForegroundWindow(g_wnd); }
    const wchar_t *nm = i >= 0 ? g_cg[i].name : L2(L"O jogo", L"The game");
    if (seen && lp >= 60) Msg(C_SUB, L2(L"%s fechado: %d min jogados.", L"%s closed: %d min played."), nm, (int)(lp / 60));
    else if (seen) Msg(C_SUB, L2(L"%s fechado.", L"%s closed."), nm);
    else Msg(C_SUB, L2(L"O PORTHEON não viu o %s rodando (o tempo de jogo não foi contado). O controle voltou para o PORTHEON.", L"PORTHEON did not see %s running (the play time was not counted). The controller is PORTHEON's again."), nm);
    InvalidateRect(g_wnd, NULL, FALSE);
}

/* ---------------------------------------------------------------- the Add screen's lists, read on a thread */
/* 6.1 review: each scan reads into its own buffer and carries a generation number; a result that is not the newest
   scan's (the player picked another source meanwhile) is dropped */
static LibCand g_lbCand[LIB_MAXCAND]; static int g_lbN = -1, g_lbSrc = -1; static volatile LONG g_lbScanning, g_lbGen;
typedef struct { LONG gen; int src, n; LibCand k[LIB_MAXCAND]; } LibScanRes;
static DWORD WINAPI LibScanThread(void *p)
{
    LibScanRes *r = (LibScanRes *)p;
    r->n = r->src == 0 ? LibSteamGames(r->k, LIB_MAXCAND) : r->src == 1 ? LibEpicGames(r->k, LIB_MAXCAND) : LibSteamShortcuts(r->k, LIB_MAXCAND);
    if (!PostMessageW(g_wnd, WM_APP_LIBSCAN, (WPARAM)r->gen, (LPARAM)r)) free(r);
    return 0;
}
static void LibScanStart(int src)   /* 0 Steam, 1 Epic, 2 non-Steam shortcuts */
{
    LONG gen = InterlockedIncrement(&g_lbGen);
    g_lbSrc = src; g_lbN = -1; InterlockedExchange(&g_lbScanning, 1);
    LibScanRes *r = (LibScanRes *)calloc(1, sizeof(LibScanRes));
    if (!r) { InterlockedExchange(&g_lbScanning, 0); g_lbN = 0; return; }
    r->gen = gen; r->src = src;
    HANDLE th = CreateThread(NULL, 0, LibScanThread, r, 0, NULL);
    if (th) CloseHandle(th); else { free(r); InterlockedExchange(&g_lbScanning, 0); g_lbN = 0; }
}
static void LibOnScan(WPARAM wp, LPARAM lp)   /* WM_APP_LIBSCAN */
{
    LibScanRes *r = (LibScanRes *)lp; if (!r) return;
    if ((LONG)wp == g_lbGen && r->src == g_lbSrc) {
        int n = r->n < 0 ? 0 : r->n > LIB_MAXCAND ? LIB_MAXCAND : r->n;
        memcpy(g_lbCand, r->k, sizeof(LibCand) * (size_t)n); g_lbN = n; InterlockedExchange(&g_lbScanning, 0);
    }
    free(r); InvalidateRect(g_wnd, NULL, FALSE);
}

/* ---------------------------------------------------------------- ui_library.c (part of ui.c, included later) */
static GpBitmap *CustomBg(int i, int w, int h);
static int LibHomeTiles(HDC dc, GpGraphics *g, int x, int cy, int base, int grow, int gap);
static void PaintHomeCustom(HDC dc, GpGraphics *g);
static BOOL LibAnimate(void);
static BOOL PickFolder(wchar_t *path, const wchar_t *title);   /* portheon.c, after ui.c */
#endif /* LIB_SCAN_ONLY */
