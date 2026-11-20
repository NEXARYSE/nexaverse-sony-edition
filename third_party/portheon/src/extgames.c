/* extgames.c - part of portheon.c (included by it, after gamever.c). 6.0 (the user, 2026-11-07: "put Zelda inside
 * PORTHEON too, and God of War III; the best emulator for Zelda with its patches and optional settings ... I won't have
 * the ISOs of the ported Zelda and God of War 3, but leave it ready for everyone"; "and the recomp program: any game it
 * installs goes into PORTHEON by itself"). Two games that are not UFC ports:
 *  - ZELDA (7): The Legend of Zelda: Ocarina of Time for Switch 2 (title 0400486024DEC000). Played on Lichii, a Switch 2
 *    emulator built on Eden/yuzu: its "Fork MDias v4" build (games\ZELDA\emu\mdias: PT-BR, AMD freeze fixes, the game's
 *    fog switch, Dynamic FPS and Max Resolution built in) by default, "Frizti v3" (games\ZELDA\emu\frizti) as the other
 *    build; or natively, as the PC program Ocarina Recomp builds from the player's own copy of version 1.1.1
 *    (games\ZELDA\recomp is its setup, run from PORTHEON; a finished build, <drive>:\OcarinaRecomp\build\oot111\
 *    ocarina.exe, is found by itself and played directly). The recomp is made for this one game: it cannot rebuild other
 *    games. The Dynamic FPS patch (the user's Dynamic-fps.rar, exefs IPS for build E0F3CCFA...) is byte for byte the one
 *    both Lichii builds already carry ("Dynamic FPS by Dark (built-in)"), so nothing is installed for it.
 *  - GOW3 (8): God of War III Remastered (PS4 CUSA01623, update 01.02) through the native PC port gow3_pc (games\GOW3,
 *    GPL-2.0, a bloodborne_pc fork with shadPS4's video core); its own launcher "God of War III.exe" prepares the game,
 *    keeps its settings and starts bin\gow3-probe.exe.
 * Nothing of the games ships: the player brings their own copy (and for Switch .nsp/.xci files their own prod.keys). The
 * "Instalador de Shader - Zelda OOT" cache (1.3 GB of the game's own shaders) is not shipped either; the emulator runs on
 * its normal user folder (%APPDATA%\eden), where that installer puts it, so a player who has it gets it. None of these
 * programs was run on the PC this was written on (third-party downloads): what PORTHEON passes them comes from their own
 * option tables and scripts, read statically (_build-tools\inbox-1107\analysis). */

static const Opt ZENG[] = {{L"Automático (o nativo, se você já recompilou)", L"Automatic (the native one, if you recompiled)", 0},
                           {L"Emulador Lichii MDias v4 (recomendado, PT-BR, AMD)", L"Lichii MDias v4 emulator (recommended)", 1},
                           {L"Emulador Lichii Frizti v3", L"Lichii Frizti v3 emulator", 2}, {L"PC nativo (Ocarina Recomp)", L"Native PC (Ocarina Recomp)", 3}};
static const Opt ZFOG[] = {{L"Reduzida (mais perto do console)", L"Reduced (closer to the console)", 0}, {L"Ligada (a do jogo, mais forte)", L"On (the game's, thicker)", 1},
                           {L"Desligada", L"Off", 2}, {L"Original (sem mudanças)", L"Original (no changes)", 3}};
static const Field FZ_ENGINE = {S_COUNT, L"engine", ZENG, 4, 0}, FZ_FOG = {S_COUNT, L"fog", ZFOG, 4, 0},
    FZ_FPS30 = {S_COUNT, L"fps30", NULL, 0, 0}, FZ_PRELOAD = {S_COUNT, L"preload", NULL, 0, 1}, FZ_AMDGFX = {S_COUNT, L"amd_safe_graphics", NULL, 0, 0},
    FZ_CRUMBS = {S_COUNT, L"gpu_breadcrumbs", NULL, 0, 0}, FZ_FULL = {S_COUNT, L"fullscreen", NULL, 0, 1};
static const Field FG_FULL = {S_COUNT, L"fullscreen", NULL, 0, 1};
static const wchar_t *ExtFieldName(const Field *f)
{
    if (f == &FZ_ENGINE) return L2(L"Como jogar", L"How to play");
    if (f == &FZ_FOG) return L2(L"Neblina do jogo", L"The game's fog");
    if (f == &FZ_FPS30) return L2(L"Travar em 30 FPS (ritmo regular)", L"Lock at 30 FPS (steady pacing)");
    if (f == &FZ_PRELOAD) return L2(L"Montar os shaders ao abrir (menos engasgos)", L"Build the shaders at start (less stutter)");
    if (f == &FZ_AMDGFX) return L2(L"AMD: compilação segura (se fechar no Deku Tree)", L"AMD: safe compiling (if it closes at the Deku Tree)");
    if (f == &FZ_CRUMBS) return L2(L"Diagnóstico de travamento da placa (log)", L"GPU hang diagnostics (log)");
    if (f == &FZ_FULL || f == &FG_FULL) return L2(L"Tela cheia", L"Full screen");
    return NULL;
}

/* ---------------------------------------------------------------- Zelda */
static void ZeldaEmuPath(int which, wchar_t *exe)   /* which 1 MDias v4, 2 Frizti v3 */
{
    wchar_t d[MAX_PATH]; PortDir(7, d); Join(exe, d, which == 2 ? L"emu\\frizti\\lichii.exe" : L"emu\\mdias\\lichii.exe");
}
static BOOL ZeldaEmuOk(int which) { wchar_t e[MAX_PATH]; ZeldaEmuPath(which, e); return FileExists(e); }
/* a finished Ocarina Recomp build under root: the setup's own check (build\romfs\index.json, build\exefs\main) and the
   program itself */
static BOOL ZeldaRecompAt(const wchar_t *root)
{
    if (!root[0]) return FALSE;
    wchar_t p[MAX_PATH]; _snwprintf(p, MAX_PATH, L"%s\\build\\oot111\\ocarina.exe", root); p[MAX_PATH - 1] = 0; if (!FileExists(p)) return FALSE;
    _snwprintf(p, MAX_PATH, L"%s\\build\\exefs\\main", root); p[MAX_PATH - 1] = 0; if (!FileExists(p)) return FALSE;
    _snwprintf(p, MAX_PATH, L"%s\\build\\romfs\\index.json", root); p[MAX_PATH - 1] = 0; return FileExists(p);
}
/* the recomp's root: the one PORTHEON started (recomp_root), else <drive>:\OcarinaRecomp on any local drive (the setup's
   default and its "drive with the most room"); read again at most every 10 s */
static BOOL ZeldaRecompRoot(wchar_t *root)
{
    static wchar_t cached[MAX_PATH]; static DWORD at; static BOOL have;
    if (at && GetTickCount() - at < 10000) { wcscpy(root, cached); return have; }
    at = GetTickCount(); if (!at) at = 1; have = FALSE; cached[0] = 0;
    wchar_t r[MAX_PATH]; GetStr(7, L"recomp_root", r, MAX_PATH, L"");
    if (ZeldaRecompAt(r)) { wcscpy(cached, r); have = TRUE; }
    else if (GetInt(7, L"engine", 0) == 3 || GetInt(7, L"recomp_scan_day", -1) != (int)(UnixNow() / 86400)) {
        /* 6.0 final: the other drives are looked through at most once a day (or when "Native PC" is picked): a Zelda PLAY
           with no native build no longer wakes every disk */
        PutInt(7, L"recomp_scan_day", (int)(UnixNow() / 86400));
        DWORD drives = GetLogicalDrives();
        for (int i = 2; i < 26 && !have; i++) {
            if (!(drives & (1u << i))) continue;
            wchar_t rt[4] = {(wchar_t)(L'A' + i), L':', L'\\', 0}; if (GetDriveTypeW(rt) != DRIVE_FIXED) continue;
            _snwprintf(r, MAX_PATH, L"%c:\\OcarinaRecomp", L'A' + i);
            if (ZeldaRecompAt(r)) { wcscpy(cached, r); have = TRUE; PutStr(7, L"recomp_root", r); }
        }
    }
    wcscpy(root, cached); return have;
}
/* what PLAY runs: 1 MDias, 2 Frizti, 3 the native build (0 = none of them is here) */
static int ZeldaEngine(void)
{
    int e = GetInt(7, L"engine", 0); wchar_t r[MAX_PATH];
    if (e == 3) return ZeldaRecompRoot(r) ? 3 : ZeldaEmuOk(1) ? 1 : ZeldaEmuOk(2) ? 2 : 0;
    if (e == 1 || e == 2) return ZeldaEmuOk(e) ? e : ZeldaEmuOk(3 - e) ? 3 - e : 0;
    if (ZeldaRecompRoot(r)) return 3;
    return ZeldaEmuOk(1) ? 1 : ZeldaEmuOk(2) ? 2 : 0;
}
static BOOL ZeldaExt(const wchar_t *p)
{
    const wchar_t *x = PathFindExtensionW(p);
    return !_wcsicmp(x, L".nup") || !_wcsicmp(x, L".nsp") || !_wcsicmp(x, L".xci") || !_wcsicmp(x, L".nsz") || !_wcsicmp(x, L".xcz");
}
/* the file the emulator boots (-g): the picked .nup/.nsp/.xci itself, the first such file in a picked folder, or an
   extracted dump's exefs\main (romfs beside it) */
static BOOL ZeldaGameFile(const wchar_t *p, wchar_t *out)
{
    out[0] = 0; if (!p[0]) return FALSE;
    if (FileExists(p)) { if (!ZeldaExt(p)) return FALSE; wcsncpy(out, p, MAX_PATH - 1); out[MAX_PATH - 1] = 0; return TRUE; }
    if (!DirExists(p)) return FALSE;
    wchar_t m[MAX_PATH]; Join(m, p, L"exefs\\main"); if (FileExists(m)) { wcscpy(out, m); return TRUE; }
    static const wchar_t *const pats[] = {L"*.nup", L"*.nsp", L"*.xci", L"*.nsz", L"*.xcz"};
    for (int i = 0; i < 5; i++) {
        wchar_t pat[MAX_PATH]; Join(pat, p, pats[i]); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
        if (h != INVALID_HANDLE_VALUE) { FindClose(h); Join(out, p, fd.cFileName); return TRUE; }
    }
    return FALSE;
}
static BOOL ZeldaNeedsKeys(const wchar_t *file)   /* .nsp/.xci need the player's prod.keys; .nup and dumps do not */
{
    const wchar_t *x = PathFindExtensionW(file); if (!_wcsicmp(x, L".nup") || !_wcsicmp(x, L"")) return FALSE;
    wchar_t ad[MAX_PATH] = L"", k[MAX_PATH]; GetEnvironmentVariableW(L"APPDATA", ad, MAX_PATH); Join(k, ad, L"eden\\keys\\prod.keys");
    return !FileExists(k);
}
/* the "Instalador de Shader - Zelda OOT" cache in the emulator's user folder (the emulator builds it by itself too) */
static int ZeldaShaderCacheMb(void)
{
    wchar_t ad[MAX_PATH] = L"", d[MAX_PATH], pat[MAX_PATH]; GetEnvironmentVariableW(L"APPDATA", ad, MAX_PATH); Join(d, ad, L"eden\\cache\\shader\\0400486024dec000");
    Join(pat, d, L"vulkan*.bin"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); ULONGLONG sum = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    do sum += ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow; while (FindNextFileW(h, &fd));
    FindClose(h); return (int)(sum >> 20);
}
/* once: the emulator's own update check off, so it never replaces this build with upstream Lichii (its qt-config.ini,
   the yuzu/Eden format: a value and its "\default" flag) */
static void ZeldaConfigOnce(void)
{
    if (GetInt(7, L"cfg_done", 0)) return;
    wchar_t ad[MAX_PATH] = L"", d[MAX_PATH], f[MAX_PATH]; GetEnvironmentVariableW(L"APPDATA", ad, MAX_PATH);
    Join(d, ad, L"eden"); CreateDirectoryW(d, NULL); Join(f, d, L"config"); CreateDirectoryW(f, NULL); Join(d, f, L"qt-config.ini");
    WritePrivateProfileStringW(L"UI", L"check_for_updates\\default", L"false", d);
    WritePrivateProfileStringW(L"UI", L"check_for_updates", L"false", d);
    PutInt(7, L"cfg_done", 1);
}

/* ---------------------------------------------------------------- God of War III */
/* the player's PS4 dump: eboot.bin and sce_sys\param.sfo of CUSA01623 */
static BOOL Gow3Folder(const wchar_t *d, char *ver)
{
    if (ver) ver[0] = 0;
    if (!d[0] || !DirExists(d)) return FALSE;
    wchar_t p[MAX_PATH]; Join(p, d, L"eboot.bin"); if (!FileExists(p)) return FALSE;
    Join(p, d, L"sce_sys\\param.sfo"); char tid[16], v[16], title[128];
    if (!ReadSfo(p, tid, v, title)) return FALSE;
    if (ver) strcpy(ver, v);
    return !strcmp(tid, "CUSA01623") || !strcmp(tid, "CUSA01663") || StrStrIA(title, "God of War III") != NULL;
}

/* ---------------------------------------------------------------- 6.1: Bloodborne and inFAMOUS Second Son
 * (the user, 2026-11-08: Bloodborne (bbport-windows, Supermedo, GPL) and the inFAMOUS Second Son PC port as games,
 * "install from inside" like Zelda/GoW3, the player picks their own dump). Neither port ships with PORTHEON: games\
 * BLOODBORNE and games\INFAMOUS carry only PORTHEON's own art. "Install the port" (ExtInstallMenu) takes the player's
 * downloaded .zip (both), the already extracted folder (both, used in place through port_<ID>= in portheon.ini), or, for
 * Bloodborne only, the official release (github.com/Supermedo/bloodborne_pc/releases/latest) after the player confirms
 * its name and size. The inFAMOUS package names no download page and no source address, so nothing is downloaded for it.
 *  - BLOODBORNE (9): "Bloodborne.exe --play" runs the port with its saved settings; its launcher rebuilds BB_GAME_DIR
 *    from %APPDATA%\bbport-launcher\settings.json, so PORTHEON writes the dump folder there (game_dir only, the other keys
 *    untouched). The game process is bin\bb-probe.exe (a grandchild), logs in user\last_run.log.
 *  - INFAMOUS (10): its launcher has no direct-play switch: PORTHEON writes game_dir= in user\launcher.ini and opens it;
 *    the player presses PLAY there. Single instance (window class IssLauncher). Logs in user\logs.
 * Read statically from the ports' packages (_build-tools\new61); neither port was run on the PC this was written on. */
#define WM_APP_EXTINST (WM_APP + 0x61B)   /* the install worker's news: wParam 0 note, 1 release found, 2 done, 3 failed */
static char *HttpGet(const wchar_t *url, BOOL follow, DWORD *status, wchar_t *location, int locN, DWORD *len);   /* update.c */
static BOOL DownloadTo(const wchar_t *url, const wchar_t *path);
static BOOL Sha256File(const wchar_t *path, char hex[65]);
static BOOL DirWritable(const wchar_t *d);
static void DelTree(const wchar_t *d);
static int CopyTree(const wchar_t *src, const wchar_t *dst, wchar_t *bad);
static BOOL JsonStr(const char *from, const char *key, char *out, int n);
static BOOL PickFolder(wchar_t *path, const wchar_t *title);   /* portheon.c */
static BOOL VulkanApiBelow(uint32_t need);                      /* checks.c (6.1): a Vulkan card was read and its API is older */
static wchar_t g_updErr[300];                                   /* update.c's (tentative definitions of the same objects) */
static volatile LONG g_updState, g_dlCancel; static volatile LONGLONG g_dlGot, g_dlTotal;
/* 6.1 review: DownloadTo (update.c) and its progress/cancel variables serve one download at a time: PORTHEON's own
   update or the Bloodborne port's. Whoever takes g_dlBusy (0 -> 1) downloads; the other is refused with a message */
static volatile LONG g_dlBusy;

static const wchar_t *const BB_MARK[] = {L"Bloodborne.exe", L"bin\\bb-probe.exe", L"_internal\\python312.dll"};
static const wchar_t *const FNC_MARK[] = {L"dist\\FNCReborn.exe"};   /* 6.2 */
static const wchar_t *const ISS_MARK[] = {L"inFAMOUS Second Son Launcher.exe", L"bin\\iss-probe.exe", L"bin\\iss-gpu.dll", L"shaders\\recipes\\recipes.bin"};
#define BB_RELEASES L"https://api.github.com/repos/Supermedo/bloodborne_pc/releases/latest"
#define BB_RELPAGE L"https://github.com/Supermedo/bloodborne_pc/releases/latest"

/* the player's PS4 dump: Bloodborne CUSA03173 (the port's), the other regions accepted with a warning (BbOtherRegion) */
static BOOL BbFolder(const wchar_t *d, char *ver, char *tidOut)
{
    if (ver) ver[0] = 0; if (tidOut) tidOut[0] = 0;
    if (!d[0] || !DirExists(d)) return FALSE;
    wchar_t p[MAX_PATH]; Join(p, d, L"eboot.bin"); if (!FileExists(p)) return FALSE;
    Join(p, d, L"sce_module"); if (!DirExists(p)) return FALSE;
    Join(p, d, L"dvdroot_ps4"); if (!DirExists(p)) return FALSE;
    Join(p, d, L"sce_sys\\param.sfo"); char tid[16], v[16], title[128];
    if (!ReadSfo(p, tid, v, title)) return FALSE;
    if (ver) strcpy(ver, v); if (tidOut) strcpy(tidOut, tid);
    return !strcmp(tid, "CUSA03173") || !strcmp(tid, "CUSA00207") || !strcmp(tid, "CUSA00900") || !strcmp(tid, "CUSA00299") || StrStrIA(title, "Bloodborne") != NULL;
}
/* the decrypted retail inFAMOUS Second Son (the port checks CUSA00004 and these three files itself) */
static BOOL IssFolder(const wchar_t *d)
{
    if (!d[0] || !DirExists(d)) return FALSE;
    wchar_t p[MAX_PATH]; Join(p, d, L"eboot.bin"); if (!FileExists(p)) return FALSE;
    Join(p, d, L"sce_module\\libc.prx"); if (!FileExists(p)) return FALSE;
    Join(p, d, L"sce_sys\\param.sfo"); char tid[16], v[16], title[128];
    return ReadSfo(p, tid, v, title) && !strcmp(tid, "CUSA00004");
}
static BOOL NgMarkersAt(int g, const wchar_t *d)
{
    const wchar_t *const *m = FNCG(g) ? FNC_MARK : BBG(g) ? BB_MARK : ISS_MARK; int n = FNCG(g) ? 1 : BBG(g) ? 3 : 4;
    for (int i = 0; i < n; i++) { wchar_t p[MAX_PATH]; Join(p, d, m[i]); if (!FileExists(p)) return FALSE; }
    return TRUE;
}
/* the folder under root (itself or up to 3 levels down) that holds the port: a zip often wraps it in one or two folders */
static BOOL NgFindPortRoot(int g, const wchar_t *root, wchar_t *out, int depth)
{
    if (NgMarkersAt(g, root)) { wcsncpy(out, root, MAX_PATH - 1); out[MAX_PATH - 1] = 0; return TRUE; }
    if (depth >= 3) return FALSE;
    wchar_t q[MAX_PATH]; Join(q, root, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(q, &fd); BOOL ok = FALSE;
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) || !wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        wchar_t s[MAX_PATH]; Join(s, root, fd.cFileName); ok = NgFindPortRoot(g, s, out, depth + 1);
    } while (!ok && FindNextFileW(h, &fd));
    FindClose(h); return ok;
}
static BOOL UnderProgramFiles(const wchar_t *p)
{
    wchar_t pf[MAX_PATH]; static const wchar_t *const vars[] = {L"ProgramFiles", L"ProgramFiles(x86)", L"ProgramW6432"};
    for (int i = 0; i < 3; i++) { DWORD n = GetEnvironmentVariableW(vars[i], pf, MAX_PATH); if (n && n < MAX_PATH && !_wcsnicmp(p, pf, n) && (p[n] == L'\\' || !p[n])) return TRUE; }
    return FALSE;
}
static BOOL NonAscii(const wchar_t *p) { for (; *p; p++) if (*p > 126) return TRUE; return FALSE; }

/* ---- small file helpers (whole file in, atomic file out) */
static char *NgReadAll(const wchar_t *path, DWORD *len)
{
    *len = 0; HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(h, NULL), got = 0; char *b = NULL;
    if (sz != INVALID_FILE_SIZE && sz < (8u << 20) && (b = (char *)malloc(sz + 3)) != NULL) { if (!ReadFile(h, b, sz, &got, NULL) || got != sz) { free(b); b = NULL; } else { b[sz] = b[sz + 1] = b[sz + 2] = 0; *len = sz; } }
    CloseHandle(h); return b;
}
static BOOL NgWriteAtomic(const wchar_t *path, const void *data, DWORD len)
{
    wchar_t tmp[MAX_PATH + 16]; _snwprintf(tmp, MAX_PATH + 16, L"%s.portheon-tmp", path); tmp[MAX_PATH + 15] = 0;
    HANDLE h = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); DWORD w = 0;
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    BOOL ok = WriteFile(h, data, len, &w, NULL) && w == len; FlushFileBuffers(h); CloseHandle(h);
    if (ok) ok = MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileW(tmp);
    return ok;
}

/* ---- Bloodborne: game_dir in the port launcher's settings.json */
/* the path as a JSON string body: UTF-8, \ and " escaped (control characters cannot be in a Windows path) */
static void JsonEscPath(const wchar_t *w, char *out, int n)
{
    char u[MAX_PATH * 4]; WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), NULL, NULL); u[sizeof(u) - 1] = 0;
    int o = 0; for (const char *p = u; *p && o < n - 3; p++) { if (*p == '\\' || *p == '"') out[o++] = '\\'; out[o++] = *p; }
    out[o] = 0;
}
/* the merged file in *out (malloc'ed): 1 changed, 0 already so, -1 not a JSON object or not read to the end (left alone).
   6.1 review: only the object's own (top-level) "game_dir" key: one inside a nested object is not it; strings are
   walked with their escapes, so a "{" or a "game_dir" inside a value does not count */
static int BbJsonMerge(const char *in, const char *esc, char **out)
{
    *out = NULL;
    const char *s = in; if ((BYTE)s[0] == 0xEF && (BYTE)s[1] == 0xBB && (BYTE)s[2] == 0xBF) s += 3;
    const char *b = s; while (*b == ' ' || *b == '\t' || *b == '\r' || *b == '\n') b++;
    if (*b != '{') return -1;
    const char *v0 = NULL, *v1 = NULL; int depth = 0; BOOL closed = FALSE;
    for (const char *p = b; *p; ) {
        if (*p == '"') {   /* a string: a key when it is at depth 1 and a colon follows */
            const char *q = p + 1; while (*q && *q != '"') { if (*q == '\\' && q[1]) q++; q++; }
            if (!*q) return -1;   /* an unterminated string: not touched */
            const char *after = q + 1;
            if (depth == 1 && !v0 && q - p - 1 == 8 && !strncmp(p + 1, "game_dir", 8)) {
                const char *c = after; while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') c++;
                if (*c == ':') {
                    c++; while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') c++;
                    if (*c == '"') {
                        const char *e = c + 1; while (*e && *e != '"') { if (*e == '\\' && e[1]) e++; e++; }
                        if (!*e) return -1;
                        if ((size_t)(e - c - 1) == strlen(esc) && !strncmp(c + 1, esc, e - c - 1)) return 0;
                        v0 = c; v1 = e + 1; after = e + 1;
                    } else if (*c == '{' || *c == '[') return -1;   /* not a string value: left alone */
                    else { const char *e = c; while (*e && *e != ',' && *e != '}' && *e != '\r' && *e != '\n') e++; while (e > c && (e[-1] == ' ' || e[-1] == '\t')) e--; v0 = c; v1 = e; after = e; }   /* null, a number */
                }
            }
            p = after; continue;
        }
        if (*p == '{' || *p == '[') depth++;
        else if (*p == '}' || *p == ']') { if (--depth == 0) { closed = TRUE; break; } }
        p++;
    }
    if (!closed) return -1;   /* the object does not end: not touched */
    size_t need = strlen(s) + strlen(esc) + 64; char *r = (char *)malloc(need); if (!r) return -1;
    if (v0) { snprintf(r, need, "%.*s\"%s\"%s", (int)(v0 - s), s, esc, v1); *out = r; return 1; }
    const char *a = b + 1; while (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n') a++;
    if (*a == '}') snprintf(r, need, "%.*s\n  \"game_dir\": \"%s\"\n%s", (int)(b + 1 - s), s, esc, a);
    else snprintf(r, need, "%.*s\n  \"game_dir\": \"%s\",%s", (int)(b + 1 - s), s, esc, b + 1);
    *out = r; return 1;
}
/* one settings.json: 1 written, 0 already so, -1 not written, -2 there but not readable (in use, too big), -3 there but
   not a JSON object PORTHEON can merge into. 6.1 review: a new file only when GetFileAttributes says there is none; the
   player's first file is kept once as settings.json.portheon-bak before any change */
static int BbWriteOne(const wchar_t *dir, const char *esc, BOOL *created)
{
    wchar_t f[MAX_PATH], bak[MAX_PATH]; CreateDirectoryW(dir, NULL); Join(f, dir, L"settings.json"); Join(bak, dir, L"settings.json.portheon-bak");
    DWORD at = GetFileAttributesW(f), err = at == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0; char *out = NULL; int rc;
    if (at == INVALID_FILE_ATTRIBUTES && (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)) {
        size_t n = strlen(esc) + 40; out = (char *)malloc(n); if (!out) return -1;
        snprintf(out, n, "{\n  \"game_dir\": \"%s\"\n}\n", esc); rc = 1; if (created) *created = TRUE;
    } else {
        if (at == INVALID_FILE_ATTRIBUTES || (at & FILE_ATTRIBUTE_DIRECTORY)) return -2;
        DWORD len = 0; char *in = NgReadAll(f, &len);
        if (!in) return -2;
        if (!len) {   /* an empty file: the new one, as for none (the empty one is kept as the copy) */
            free(in); size_t n = strlen(esc) + 40; out = (char *)malloc(n); if (!out) return -1;
            snprintf(out, n, "{\n  \"game_dir\": \"%s\"\n}\n", esc); rc = 1; if (created) *created = TRUE;
        } else { rc = BbJsonMerge(in, esc, &out); free(in); }
        if (rc < 0) return -3;
        if (rc > 0 && !CopyFileW(f, bak, TRUE) && GetFileAttributesW(bak) == INVALID_FILE_ATTRIBUTES) { free(out); return -1; }   /* no copy kept: no change */
    }
    if (rc > 0 && !NgWriteAtomic(f, out, (DWORD)strlen(out))) rc = -1;
    free(out); return rc;
}
/* the dump folder into %APPDATA%\bbport-launcher\settings.json (and the co-op build's, when it is there); only when the
   file holds another folder: the launcher re-reads it at every start, and PORTHEON writes before starting it.
   The main file's result (BbWriteOne) */
static int BbWriteGameDir(const wchar_t *dump, BOOL *firstTime)
{
    wchar_t ad[MAX_PATH] = L"", d[MAX_PATH]; char esc[MAX_PATH * 8]; if (firstTime) *firstTime = FALSE;
    if (!GetEnvironmentVariableW(L"APPDATA", ad, MAX_PATH)) return -1;
    JsonEscPath(dump, esc, sizeof(esc));
    Join(d, ad, L"bbport-launcher"); int rc = BbWriteOne(d, esc, firstTime);
    Join(d, ad, L"bbport-launcher-coop"); if (DirExists(d)) BbWriteOne(d, esc, NULL);
    return rc;
}
/* the saves folder the port uses: user_dir from settings.json, else <port>\user */
static void BbUserDir(wchar_t *out)
{
    wchar_t pd[MAX_PATH], ad[MAX_PATH] = L"", f[MAX_PATH]; PortDir(9, pd); Join(out, pd, L"user");
    GetEnvironmentVariableW(L"APPDATA", ad, MAX_PATH); Join(f, ad, L"bbport-launcher\\settings.json");
    DWORD len; char *b = NgReadAll(f, &len), v[MAX_PATH * 2];
    if (b && JsonStr(b, "user_dir", v, sizeof(v)) && v[0]) {   /* raw JSON: \\ back to \ */
        char u[MAX_PATH * 2]; int o = 0; for (const char *p = v; *p && o < (int)sizeof(u) - 1; p++) { if (*p == '\\' && p[1]) p++; u[o++] = *p; } u[o] = 0;
        wchar_t w[MAX_PATH]; if (MultiByteToWideChar(CP_UTF8, 0, u, -1, w, MAX_PATH) && DirExists(w)) wcscpy(out, w);
    }
    free(b);
}
/* bbport.ini's output_res above 1080p asks for ~10 GB of commit instead of ~6 (the port's README) */
static BOOL BbBigOutput(void)
{
    wchar_t pd[MAX_PATH], f[MAX_PATH]; PortDir(9, pd); Join(f, pd, L"bbport.ini");
    DWORD len; char *b = NgReadAll(f, &len); BOOL big = FALSE;
    if (b) { char *p = strstr(b, "output_res"); int w = 0, h = 0;
             if (p && (p = strchr(p, '=')) != NULL && sscanf(p + 1, " %dx%d", &w, &h) == 2) big = w > 1920 || h > 1080;
             free(b); }
    return big;
}

/* ---- inFAMOUS: game_dir= in <port>\user\launcher.ini */
/* the line replaced or added, every other line kept byte for byte; 1 written, 0 already so, -1 failed, -2 the file is
   there but could not be read (in use, too big): not touched. 6.1 review: a new file only when GetFileAttributes says
   there is none; the player's first file is kept once as launcher.ini.portheon-bak before any change */
static int IssWriteGameDir(const wchar_t *dump)
{
    wchar_t pd[MAX_PATH], ud[MAX_PATH], f[MAX_PATH], bak[MAX_PATH]; PortDir(10, pd); Join(ud, pd, L"user"); CreateDirectoryW(ud, NULL); Join(f, ud, L"launcher.ini"); Join(bak, ud, L"launcher.ini.portheon-bak");
    char val[MAX_PATH * 4]; WideCharToMultiByte(CP_UTF8, 0, dump, -1, val, sizeof(val), NULL, NULL); val[sizeof(val) - 1] = 0;
    DWORD at = GetFileAttributesW(f), aerr = at == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0;
    BOOL exists = !(at == INVALID_FILE_ATTRIBUTES && (aerr == ERROR_FILE_NOT_FOUND || aerr == ERROR_PATH_NOT_FOUND));
    if (exists && (at == INVALID_FILE_ATTRIBUTES || (at & FILE_ATTRIBUTE_DIRECTORY))) return -2;
    DWORD len = 0; char *in = exists ? NgReadAll(f, &len) : NULL;
    if (exists && !in) return -2;
    BOOL wide = in && len >= 2 && (BYTE)in[0] == 0xFF && (BYTE)in[1] == 0xFE;   /* a UTF-16 file stays UTF-16 */
    if (wide) {
        int n = WideCharToMultiByte(CP_UTF8, 0, (wchar_t *)(in + 2), (int)(len - 2) / 2, NULL, 0, NULL, NULL); char *u = (char *)malloc(n + 1);
        if (!u) { free(in); return -1; }
        WideCharToMultiByte(CP_UTF8, 0, (wchar_t *)(in + 2), (int)(len - 2) / 2, u, n, NULL, NULL); u[n] = 0; free(in); in = u; len = (DWORD)n;
    }
    size_t cap = len + strlen(val) + 32; char *out = (char *)malloc(cap); if (!out) { free(in); return -1; }
    size_t o = 0; BOOL found = FALSE, same = FALSE; const char *eol = "\r\n";
    for (const char *p = in ? in : ""; *p; ) {
        const char *e = strchr(p, '\n'); size_t ll = e ? (size_t)(e - p + 1) : strlen(p);
        const char *k = p; if (p == in && (BYTE)k[0] == 0xEF && (BYTE)k[1] == 0xBB && (BYTE)k[2] == 0xBF) k += 3;   /* a UTF-8 mark */
        while (*k == ' ' || *k == '\t') k++;
        if (!found && !_strnicmp(k, "game_dir", 8)) {
            const char *q = k + 8; while (*q == ' ' || *q == '\t') q++;
            if (*q == '=') {
                found = TRUE; q++; const char *qe = p + ll; while (qe > q && (qe[-1] == '\n' || qe[-1] == '\r' || qe[-1] == ' ' || qe[-1] == '\t')) qe--;
                same = (size_t)(qe - q) == strlen(val) && !strncmp(q, val, qe - q);
                if (e && e > p && e[-1] != '\r') eol = "\n";
                o += snprintf(out + o, cap - o, "game_dir=%s%s", val, e ? eol : "");
                p += ll; continue;
            }
        }
        if (e && e > p && e[-1] != '\r') eol = "\n";
        memcpy(out + o, p, ll); o += ll; p += ll;
    }
    if (!found) { if (o && out[o - 1] != '\n') o += snprintf(out + o, cap - o, "%s", eol); o += snprintf(out + o, cap - o, "game_dir=%s%s", val, eol); }
    free(in);
    if (found && same) { free(out); return 0; }
    if (exists && !CopyFileW(f, bak, TRUE) && GetFileAttributesW(bak) == INVALID_FILE_ATTRIBUTES) { free(out); return -1; }   /* no copy kept: no change */
    BOOL ok;
    if (wide) {
        int n = MultiByteToWideChar(CP_UTF8, 0, out, (int)o, NULL, 0); wchar_t *w = (wchar_t *)malloc((n + 1) * sizeof(wchar_t));
        ok = w != NULL; if (ok) { w[0] = 0xFEFF; MultiByteToWideChar(CP_UTF8, 0, out, (int)o, w + 1, n); ok = NgWriteAtomic(f, w, (DWORD)(n + 1) * sizeof(wchar_t)); free(w); }
    } else ok = NgWriteAtomic(f, out, (DWORD)o);
    free(out); return ok ? 1 : -1;
}

/* ---- the texts the screens show for these two (ui_*.c) */
static const wchar_t *NgPickText(int g)   /* no game picked yet, or the folder picked is not it */
{
    if (FNCG(g)) return L2(L"O Fight Night Champion é escolhido no instalador do FNC Reborn: a SUA .iso de Xbox 360 ou a pasta com default.xex e databigs. O PORTHEON não vem com o jogo.", L"Fight Night Champion is chosen in FNC Reborn's installer: YOUR Xbox 360 .iso or the folder with default.xex and databigs. PORTHEON ships no game.");
    return BBG(g) ? L2(L"Escolha a pasta do SEU Bloodborne de PS4 já extraído (CUSA03173, com eboot.bin, sce_module e dvdroot_ps4), de preferência na versão 01.09. O PORTHEON não vem com o jogo.",
                       L"Choose the folder of YOUR extracted PS4 Bloodborne (CUSA03173, with eboot.bin, sce_module and dvdroot_ps4), ideally at version 01.09. PORTHEON ships no game.")
                  : L2(L"Escolha a pasta do SEU inFAMOUS Second Son de PS4 já extraído e decriptado (CUSA00004, com eboot.bin, sce_sys e sce_module). O PORTHEON não vem com o jogo.",
                       L"Choose the folder of YOUR extracted, decrypted PS4 inFAMOUS Second Son (CUSA00004, with eboot.bin, sce_sys and sce_module). PORTHEON ships no game.");
}
static const wchar_t *NgNoPortText(int g)   /* S_NOPORT: the port is installed from inside */
{
    if (FNCG(g)) return L2(L"Aperte \"Instalar o port\": o instalador do FNC Reborn abre e transforma a SUA .iso de Xbox 360 num programa de PC (baixa ~2,5 GB, leva de 15 a 60 minutos, ~25 GB livres). O PORTHEON acha o jogo sozinho quando terminar.", L"Press \"Install the port\": FNC Reborn's installer opens and turns YOUR Xbox 360 .iso into a PC program (downloads ~2.5 GB, takes 15 to 60 minutes, ~25 GB free). PORTHEON finds the game by itself when it ends.");
    return BBG(g) ? L2(L"O port do Bloodborne (bbport-windows, GPL) não vem no PORTHEON: aperte \"Instalar o port\" para baixar a versão oficial do GitHub (com a sua confirmação), escolher o .zip que você baixou ou a pasta já extraída.",
                       L"The Bloodborne port (bbport-windows, GPL) does not come with PORTHEON: press \"Install the port\" to download the official release from GitHub (after you confirm), pick the .zip you downloaded or the already extracted folder.")
                  : L2(L"O port do inFAMOUS Second Son não vem no PORTHEON: aperte \"Instalar o port\" e escolha o .zip que você baixou ou a pasta já extraída (o pacote do port não informa uma página oficial de download).",
                       L"The inFAMOUS Second Son port does not come with PORTHEON: press \"Install the port\" and pick the .zip you downloaded or the already extracted folder (the port's package names no official download page).");
}
static const wchar_t *NgPlayWhat(int g)   /* the home screen's line under the name, for a ready game */
{
    if (FNCG(g)) return L2(L"Jogar abre o Fight Night Champion nativo no PC (FNC Reborn). Tela e controles ficam no menu do próprio jogo.", L"Play opens the native PC Fight Night Champion (FNC Reborn). Display and controls are in the game's own menu.");
    return BBG(g) ? L2(L"Jogar abre o Bloodborne pelo port nativo para PC (Bloodborne.exe --play). Vídeo, controles, mods e online ficam no launcher do port.",
                       L"Play opens Bloodborne through the native PC port (Bloodborne.exe --play). Video, controls, mods and online are in the port's launcher.")
                  : L2(L"Jogar abre o launcher do port com a sua pasta já escolhida: aperte PLAY nele.", L"Play opens the port's launcher with your folder already chosen: press PLAY in it.");
}
static const wchar_t *NgCredits(int g)
{
    if (FNCG(g)) return L2(L"Fight Night Champion roda pelo port nativo FNC Reborn by Whompay (github.com/whompay/FNCReborn, Apache License 2.0), feito sobre o ReXGlue SDK. O PORTHEON inclui o instalador oficial e não inclui o jogo: o port é gerado no seu PC a partir do SEU disco de Xbox 360. Não afiliado à Electronic Arts.", L"Fight Night Champion runs through the native FNC Reborn by Whompay port (github.com/whompay/FNCReborn, Apache License 2.0), built on the ReXGlue SDK. PORTHEON includes the official installer and not the game: the port is generated on your PC from YOUR Xbox 360 disc. Not affiliated with Electronic Arts.");
    return BBG(g) ? L2(L"Bloodborne roda pelo port nativo bbport-windows, de Supermedo (Mohammed Albarghouthi), feito sobre o bbport (o port para Linux) de deadinside28 e o renderizador do shadPS4. GPL-2.0-or-later. Código: github.com/Supermedo/bloodborne_pc e github.com/deadinside28/bloodborne_pc. Online: código do shadPS4/shadNet, Awaken (BloodBorneButPc) e bbhost de droogie (GPL-3.0-or-later). DLSS, FSR e o online são recursos do port, não do PORTHEON. O PORTHEON não inclui o port nem o jogo: o port é baixado da página oficial ou do zip do jogador; o jogo é o dump do próprio jogador (CUSA03173 v1.09). Discord do port: discord.gg/yTMG8c4Bqm. Não afiliado à Sony Interactive Entertainment nem à FromSoftware.",
                       L"Bloodborne runs through the native bbport-windows port by Supermedo (Mohammed Albarghouthi), built on deadinside28's bbport (the Linux port) and shadPS4's renderer. GPL-2.0-or-later. Source: github.com/Supermedo/bloodborne_pc and github.com/deadinside28/bloodborne_pc. Online: code from shadPS4/shadNet, Awaken (BloodBorneButPc) and droogie's bbhost (GPL-3.0-or-later). DLSS, FSR and online are the port's features, not PORTHEON's. PORTHEON includes neither the port nor the game: the port comes from its official page or the player's zip; the game is the player's own dump (CUSA03173 v1.09). The port's Discord: discord.gg/yTMG8c4Bqm. Not affiliated with Sony Interactive Entertainment or FromSoftware.")
                  : L2(L"inFAMOUS Second Son roda pelo port nativo para PC (GPL-2.0-or-later; feito sobre o shadPS4, github.com/shadps4-emu/shadPS4; leitura de SELF adaptada do bloodborne_pc de deadinside28; SDL3; fonte Oswald OFL). O pacote do port não informa o autor nem o endereço do código-fonte do próprio port. O PORTHEON não inclui o port nem o jogo (CUSA00004 do próprio jogador). Não afiliado à Sony Interactive Entertainment nem à Sucker Punch Productions.",
                       L"inFAMOUS Second Son runs through its native PC port (GPL-2.0-or-later; built on shadPS4, github.com/shadps4-emu/shadPS4; SELF reading adapted from deadinside28's bloodborne_pc; SDL3; the Oswald font, OFL). The port's package names neither its author nor the port's own source code address. PORTHEON includes neither the port nor the game (the player's own CUSA00004). Not affiliated with Sony Interactive Entertainment or Sucker Punch Productions.");
}
/* the home screen's four cards: how it plays, your game, the port, the first time */
static void NgCards(int g, wchar_t *a1, wchar_t *b1, wchar_t *a2, wchar_t *b2, wchar_t *a3, wchar_t *b3, wchar_t *a4, wchar_t *b4)
{
    const GameVer *gv = GameVerOf(g); BOOL port = PortOk(g);
    if (FNCG(g)) {   /* 6.2 */
        _snwprintf(a1, 160, L"%s", L2(L"Port nativo (FNC Reborn)", L"Native port (FNC Reborn)")); _snwprintf(b1, 160, L"%s", port ? L2(L"Roda direto no PC", L"Runs straight on the PC") : L2(L"Ainda não instalado", L"Not installed yet"));
        _snwprintf(a2, 160, L"%s", L"Xbox 360"); _snwprintf(b2, 160, L"%s", L2(L"Sua .iso, escolhida no instalador", L"Your .iso, picked in the installer"));
        _snwprintf(a3, 160, L"%s", port ? L2(L"Atualizar o port", L"Update the port") : L2(L"Instalador incluído", L"Installer included")); _snwprintf(b3, 160, L"%s", port ? L2(L"Pelo instalador do FNC Reborn", L"Through FNC Reborn's installer") : L2(L"Baixa ~2,5 GB, ~25 GB livres", L"Downloads ~2.5 GB, ~25 GB free"));
        _snwprintf(a4, 160, L"%s", L2(L"Compila uma vez", L"Builds once")); _snwprintf(b4, 160, L"%s", L2(L"De 15 a 60 minutos", L"15 to 60 minutes"));
        (void)gv; return;
    }
    if (BBG(g)) {
        _snwprintf(a1, 160, L"%s", L2(L"Port nativo (bbport-windows)", L"Native port (bbport-windows)")); _snwprintf(b1, 160, L"%s", port ? (g_ver[g][0] ? g_ver[g] : L"Bloodborne") : L2(L"Ainda não instalado", L"Not installed yet"));
        _snwprintf(a2, 160, L"%s", gv->tag[0] ? gv->tag : L2(L"Escolha a pasta do jogo", L"Choose the game folder")); _snwprintf(b2, 160, L"%s", L2(L"PS4 CUSA03173, versão 01.09", L"PS4 CUSA03173, version 01.09"));
        _snwprintf(a3, 160, L"%s", port ? L2(L"Launcher do port", L"The port's launcher") : L2(L"Baixar ou escolher", L"Download or choose")); _snwprintf(b3, 160, L"%s", port ? L2(L"Vídeo, DLSS/FSR, controles, mods", L"Video, DLSS/FSR, controls, mods") : L2(L"GitHub oficial, seu .zip ou pasta", L"Official GitHub, your .zip or folder"));
        _snwprintf(a4, 160, L"%s", L2(L"Prepara o jogo uma vez", L"Prepares the game once")); _snwprintf(b4, 160, L"%s", L2(L"Precisa de ~6 GB de memória livre", L"Needs ~6 GB of free memory"));
    } else {
        _snwprintf(a1, 160, L"%s", L2(L"Port nativo para PC", L"Native PC port")); _snwprintf(b1, 160, L"%s", port ? L2(L"Aperte PLAY no launcher dele", L"Press PLAY in its launcher") : L2(L"Ainda não instalado", L"Not installed yet"));
        _snwprintf(a2, 160, L"%s", gv->tag[0] ? gv->tag : L2(L"Escolha a pasta do jogo", L"Choose the game folder")); _snwprintf(b2, 160, L"%s", L2(L"PS4 CUSA00004 decriptado", L"Decrypted PS4 CUSA00004"));
        _snwprintf(a3, 160, L"%s", port ? L2(L"Launcher do port", L"The port's launcher") : L2(L"Você escolhe o pacote", L"You pick the package")); _snwprintf(b3, 160, L"%s", port ? L2(L"Tela, controles e DLC", L"Display, controls and DLC") : L2(L"Seu .zip ou a pasta extraída", L"Your .zip or the extracted folder"));
        _snwprintf(a4, 160, L"%s", L2(L"Prepara jogo e shaders", L"Prepares game and shaders")); _snwprintf(b4, 160, L"%s", L2(L"Uns 2 minutos na primeira vez", L"About 2 minutes the first time"));
    }
}
static const wchar_t *NgVideoNote(int g)
{
    if (FNCG(g)) return L2(L"O Fight Night Champion tem tela e opções no menu do próprio jogo (aba DISPLAY) e em dist\\FNCReborn.toml, na pasta do port. O PORTHEON não muda nada disso.", L"Fight Night Champion has its display and options in the game's own menu (DISPLAY tab) and in dist\\FNCReborn.toml in the port folder. PORTHEON changes none of them.");
    return BBG(g) ? L2(L"O Bloodborne tem resolução, upscaler (FSR, DLSS), FPS, geração de quadros, controles e mods no launcher do próprio port (botão acima). As opções do jogo ficam no bbport.ini da pasta do port. O PORTHEON não muda nada disso.",
                       L"Bloodborne has its resolution, upscaler (FSR, DLSS), FPS, frame generation, controls and mods in the port's own launcher (button above). The game's options are in bbport.ini in the port folder. PORTHEON changes none of them.")
                  : L2(L"O inFAMOUS Second Son tem tela, limite de FPS, VSync, controles e DLC no launcher do próprio port (botão acima). Na primeira vez ele prepara o jogo e os shaders (uns 2 minutos).",
                       L"inFAMOUS Second Son has its display, FPS limit, VSync, controls and DLC in the port's own launcher (button above). The first time it prepares the game and the shaders (about 2 minutes).");
}
/* the setup guide's six steps (ui_screens.c InstallSteps): NULL keeps the general text */
static const wchar_t *NgGuide(int g, int k, const wchar_t **title)
{
    if (title) *title = NULL;
    if (FNCG(g)) switch (k) {   /* 6.2 */
    case 0: return L2(L"O PORTHEON abre o Fight Night Champion de Xbox 360 direto no PC pelo port nativo FNC Reborn. Você precisa do SEU disco do jogo em .iso (ou a pasta com default.xex e databigs). O PORTHEON traz o instalador do port, não traz o jogo e não indica sites de download.", L"PORTHEON opens the Xbox 360 Fight Night Champion directly on the PC through the native FNC Reborn port. You need YOUR game disc as an .iso (or the folder with default.xex and databigs). PORTHEON brings the port's installer, not the game, and points to no download sites.");
    case 1: if (title) *title = L2(L"Abra o instalador", L"Open the installer");
            return L2(L"Na Visão geral do jogo, \"Instalar o port\": o instalador do FNC Reborn abre. Se o Windows avisar que o programa não é assinado, escolha Mais informações > Executar assim mesmo.", L"On the game's Overview, \"Install the port\": FNC Reborn's installer opens. If Windows warns that the program is unsigned, choose More info > Run anyway.");
    case 2: if (title) *title = L2(L"Escolha o seu jogo", L"Pick your game");
            return L2(L"No instalador, escolha a sua .iso de Xbox 360 ou a pasta do jogo, e aceite a licença da Microsoft que ele mostra.", L"In the installer, pick your Xbox 360 .iso or game folder, and accept the Microsoft license it shows.");
    case 3: if (title) *title = L2(L"Espere compilar", L"Wait for the build");
            return L2(L"Ele baixa cerca de 2,5 GB de ferramentas e leva de 15 a 60 minutos. São precisos uns 25 GB livres e 16 GB de memória.", L"It downloads about 2.5 GB of tools and takes 15 to 60 minutes. It needs about 25 GB free and 16 GB of memory.");
    case 4: return L2(L"Quando o instalador terminar, o PORTHEON acha o jogo sozinho. Se ele não achar, use \"Instalar o port\" > \"Já instalei\" e mostre a pasta.", L"When the installer ends, PORTHEON finds the game by itself. If it does not, use \"Install the port\" > \"Already installed\" and show the folder.");
    case 5: return L2(L"Aperte JOGAR. Tela e controles ficam no menu do próprio jogo.", L"Press PLAY. Display and controls are in the game's own menu.");
    default: return NULL;
    }
    switch (k) {
    case 0: return BBG(g) ? L2(L"O PORTHEON roda o Bloodborne de PS4 (CUSA03173) direto no PC pelo port nativo bbport-windows (GPL), que você instala de dentro do PORTHEON. Você precisa do SEU jogo de PS4 já extraído numa pasta, de preferência com o update 01.09. O PORTHEON não vem com o port nem com o jogo e não indica sites de download do jogo.",
                               L"PORTHEON runs the PS4 Bloodborne (CUSA03173) directly on the PC through the native bbport-windows port (GPL), which you install from inside PORTHEON. You need YOUR PS4 game already extracted into a folder, ideally with update 01.09. PORTHEON ships neither the port nor the game and points to no game download sites.")
                          : L2(L"O PORTHEON abre o inFAMOUS Second Son de PS4 (CUSA00004) pelo port nativo para PC, que você instala de dentro do PORTHEON. Você precisa do SEU jogo de PS4 já extraído e decriptado numa pasta. O PORTHEON não vem com o port nem com o jogo e não indica sites de download.",
                               L"PORTHEON opens the PS4 inFAMOUS Second Son (CUSA00004) through its native PC port, which you install from inside PORTHEON. You need YOUR PS4 game already extracted and decrypted into a folder. PORTHEON ships neither the port nor the game and points to no download sites.");
    case 1: if (title) *title = L2(L"Instale o port", L"Install the port");
            return BBG(g) ? L2(L"Na Visão geral do jogo, \"Instalar o port\": baixe a versão oficial (github.com/Supermedo/bloodborne_pc; o PORTHEON mostra o nome e o tamanho antes), escolha o .zip que você baixou, ou a pasta onde ele já está extraído. O port ocupa uns 400 MB.",
                               L"On the game's Overview, \"Install the port\": download the official release (github.com/Supermedo/bloodborne_pc; PORTHEON shows the name and size first), pick the .zip you downloaded, or the folder it is already extracted in. The port takes about 400 MB.")
                          : L2(L"Na Visão geral do jogo, \"Instalar o port\": escolha o .zip do port que você baixou (por exemplo inFAMOUS-Second-Son-PC-Port-20261019-v2.zip) ou a pasta onde ele já está extraído. Não deixe o port em Arquivos de Programas.",
                               L"On the game's Overview, \"Install the port\": pick the port's .zip you downloaded (for example inFAMOUS-Second-Son-PC-Port-20261019-v2.zip) or the folder it is already extracted in. Do not keep the port in Program Files.");
    case 2: if (title) *title = BBG(g) ? L2(L"A versão 01.09", L"Version 01.09") : L2(L"O jogo decriptado", L"The decrypted game");
            return BBG(g) ? L2(L"O port foi feito para a versão 01.09 (o update junto com o jogo). Em outra versão ele roda a 30 FPS, sem os patches da comunidade. Outras regiões (CUSA00207, CUSA00900, CUSA00299) não são as que o port testou.",
                               L"The port is made for version 01.09 (the update merged into the game). On another version it runs at 30 FPS, without the community patches. Other regions (CUSA00207, CUSA00900, CUSA00299) are not the ones the port tested.")
                          : L2(L"O port precisa do jogo de varejo decriptado (CUSA00004), com sce_module\\libc.prx. Ele mesmo confere o eboot.bin ao preparar o jogo.",
                               L"The port needs the decrypted retail game (CUSA00004), with sce_module\\libc.prx. It checks eboot.bin by itself while preparing the game.");
    case 3: return L2(L"Clique em \"Escolher a pasta\" e selecione a pasta do jogo (a que tem eboot.bin, sce_sys e sce_module), ou arraste a pasta para a janela do PORTHEON.",
                      L"Click \"Choose the folder\" and pick the game folder (the one with eboot.bin, sce_sys and sce_module), or drop the folder on PORTHEON's window.");
    case 4: return BBG(g) ? L2(L"O PORTHEON confere se a pasta é do Bloodborne e qual versão ela tem. Ao jogar, ele grava a pasta no settings.json do launcher do port (só o game_dir).",
                               L"PORTHEON checks that the folder is Bloodborne and which version it has. When you play, it writes the folder into the port launcher's settings.json (game_dir only).")
                          : L2(L"O PORTHEON confere se a pasta é do inFAMOUS Second Son. Ao jogar, ele grava a pasta no user\\launcher.ini do port (só a linha game_dir).",
                               L"PORTHEON checks that the folder is inFAMOUS Second Son. When you play, it writes the folder into the port's user\\launcher.ini (only the game_dir line).");
    case 5: return BBG(g) ? L2(L"Aperte JOGAR. A primeira vez demora mais: o port prepara o jogo e os shaders. Se o launcher do port abrir em vez do jogo, termine o guia de 5 passos dele uma vez. Vídeo, DLSS/FSR, controles, mods e online ficam no launcher do port.",
                               L"Press PLAY. The first time takes longer: the port prepares the game and the shaders. If the port's launcher opens instead of the game, finish its 5-step guide once. Video, DLSS/FSR, controls, mods and online are in the port's launcher.")
                          : L2(L"Aperte JOGAR: o launcher do port abre com a sua pasta já escolhida; aperte PLAY nele. A primeira vez prepara o jogo e os shaders (uns 2 minutos). O PORTHEON espera o launcher fechar.",
                               L"Press PLAY: the port's launcher opens with your folder already chosen; press PLAY in it. The first time prepares the game and the shaders (about 2 minutes). PORTHEON waits for the launcher to close.");
    }
    return NULL;
}

/* ---- install from inside */
typedef struct { COLORREF c; wchar_t t[700]; } NgNote;
typedef struct { int g, mode; wchar_t zip[MAX_PATH], url[1024], name[260], tag[64]; char sha[65]; LONGLONG size; } NgJob;   /* mode 1 ask GitHub, 2 download + install, 3 install a zip */
static volatile LONG g_ngBusy; static NgJob g_ngJob;
static void NgPost(int code, COLORREF c, const wchar_t *fmt, ...)
{
    NgNote *n = NULL;
    if (fmt) { n = (NgNote *)malloc(sizeof(NgNote)); if (n) { va_list ap; va_start(ap, fmt); _vsnwprintf(n->t, 700, fmt, ap); va_end(ap); n->t[699] = 0; n->c = c; } }
    if (!PostMessageW(g_wnd, WM_APP_EXTINST, (WPARAM)code, (LPARAM)n)) free(n);
}
/* where the port goes: games\<ID> beside PORTHEON.exe, or %LOCALAPPDATA%\PORTHEON\ports\<ID> when PORTHEON's folder is
   not writable (Program Files) */
static BOOL NgInstallDest(int g, wchar_t *out, BOOL *local)
{
    wchar_t gd[MAX_PATH]; Join(gd, g_exeDir, L"games"); *local = FALSE;
    if (DirExists(gd) && DirWritable(gd) && !(ISSG(g) && UnderProgramFiles(gd))) { Join(out, gd, G[g].id); return TRUE; }
    wchar_t d[MAX_PATH]; LocalData(d, L"PORTHEON"); CreateDirectoryW(d, NULL); Join(d, d, L"ports"); CreateDirectoryW(d, NULL); Join(out, d, G[g].id); *local = TRUE;
    return TRUE;
}
/* the found port's files into dest, beside PORTHEON's art\ (kept). A reinstall keeps the player's user\ (saves, cache,
   settings) and replaces the rest; the old program folders go to <name>.portheon-old first and are removed at the end */
static BOOL NgMoveInto(const wchar_t *src, const wchar_t *dest, wchar_t *err, int nerr)
{
    CreateDirectoryW(dest, NULL);
    wchar_t q[MAX_PATH]; Join(q, src, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(q, &fd); BOOL ok = TRUE;
    if (h == INVALID_HANDLE_VALUE) { _snwprintf(err, nerr, L"%s", L2(L"a pasta extraída está vazia", L"the extracted folder is empty")); return FALSE; }
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        wchar_t s[MAX_PATH], t[MAX_PATH], old[MAX_PATH]; Join(s, src, fd.cFileName); Join(t, dest, fd.cFileName); _snwprintf(old, MAX_PATH, L"%s.portheon-old", t); old[MAX_PATH - 1] = 0;
        BOOL dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (dir && (!_wcsicmp(fd.cFileName, L"user") || !_wcsicmp(fd.cFileName, L"art")) && DirExists(t)) continue;   /* the player's saves; PORTHEON's art */
        if (dir && DirExists(t)) {
            /* 6.1 review: a destination that is a link (junction, symbolic link) is never emptied: DelTree would delete
               what it points to */
            DWORD ta = GetFileAttributesW(t), oa = GetFileAttributesW(old);
            if ((ta != INVALID_FILE_ATTRIBUTES && (ta & FILE_ATTRIBUTE_REPARSE_POINT)) || (oa != INVALID_FILE_ATTRIBUTES && (oa & FILE_ATTRIBUTE_REPARSE_POINT))) {
                ok = FALSE; _snwprintf(err, nerr, L2(L"%s é um link para outra pasta (junção): o PORTHEON não mexe nele. Apague o link ou instale em outra pasta", L"%s is a link to another folder (a junction): PORTHEON leaves it alone. Remove the link or install elsewhere"), (ta & FILE_ATTRIBUTE_REPARSE_POINT) ? t : old); break;
            }
            DelTree(old); if (!MoveFileExW(t, old, 0)) DelTree(t);
        }
        if (MoveFileExW(s, t, MOVEFILE_REPLACE_EXISTING)) continue;
        wchar_t bad[MAX_PATH] = L"";
        if (dir ? CopyTree(s, t, bad) != 0 : !CopyFileW(s, t, FALSE)) { ok = FALSE; _snwprintf(err, nerr, L2(L"não consegui copiar %s (arquivo em uso ou antivírus)", L"could not copy %s (file in use or an antivirus)"), bad[0] ? bad : t); break; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    /* the old program folders that were set aside */
    Join(q, dest, L"*.portheon-old"); h = FindFirstFileW(q, &fd);
    if (h != INVALID_HANDLE_VALUE) { do { wchar_t p[MAX_PATH]; Join(p, dest, fd.cFileName); DelTree(p); } while (FindNextFileW(h, &fd)); FindClose(h); }
    return ok;
}
/* tar.exe unpacks the zip next to the destination (the same disk, so the files are moved, not copied), the port's folder
   is found inside, and moved in. Runs on the worker thread. */
static BOOL NgInstallZip(int g, const wchar_t *zip, wchar_t *err, int nerr)
{
    err[0] = 0; wchar_t dest[MAX_PATH], tmp[MAX_PATH], root[MAX_PATH]; BOOL local;
    NgInstallDest(g, dest, &local);
    WIN32_FILE_ATTRIBUTE_DATA fa; ULONGLONG zsz = GetFileAttributesExW(zip, GetFileExInfoStandard, &fa) ? ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow : 0;
    ULONGLONG need = BBG(g) ? (1200ull << 20) : (512ull << 20); if (zsz * 2 > need) need = zsz * 2;
    wchar_t vol[MAX_PATH]; wcscpy(vol, dest); PathStripToRootW(vol); ULARGE_INTEGER fb;
    if (GetDiskFreeSpaceExW(vol, &fb, NULL, NULL) && fb.QuadPart < need) { _snwprintf(err, nerr, L2(L"pouco espaço em %s: precisa de %llu MB livres", L"not enough room on %s: it needs %llu MB free"), vol, need >> 20); return FALSE; }
    wchar_t tar[MAX_PATH], cmd[3 * MAX_PATH + 32]; GetSystemDirectoryW(tar, MAX_PATH); wcscat(tar, L"\\tar.exe");
    if (!FileExists(tar)) { _snwprintf(err, nerr, L"%s", L2(L"este Windows não tem o tar.exe (Windows 10 de 2018 ou mais novo): extraia o .zip você mesmo e use \"Escolher a pasta já extraída\"", L"this Windows has no tar.exe (Windows 10 from 2018 or newer): unpack the .zip yourself and use \"Choose the already extracted folder\"")); return FALSE; }
    /* paths stay under MAX_PATH: the port's own longest path is ~70 characters (Bloodborne), and a zip may wrap it in two
       folders of ~40 each (inFAMOUS's does), so the unpacking goes to a short folder when dest's is long */
    if (wcslen(dest) > 180) { _snwprintf(err, nerr, L2(L"o caminho da pasta do PORTHEON é longo demais (%s): mova o PORTHEON para uma pasta mais curta, por exemplo C:\\PORTHEON", L"PORTHEON's folder path is too long (%s): move PORTHEON to a shorter folder, for example C:\\PORTHEON"), dest); return FALSE; }
    _snwprintf(tmp, MAX_PATH, L"%s.portheon-tmp", dest); tmp[MAX_PATH - 1] = 0;
    if (wcslen(tmp) > 100) { LocalData(tmp, L"PORTHEON"); CreateDirectoryW(tmp, NULL); Join(tmp, tmp, BBG(g) ? L"tmp-bb" : L"tmp-iss"); }   /* another disk maybe: NgMoveInto copies then */
    DelTree(tmp);
    { wchar_t parent[MAX_PATH]; wcscpy(parent, dest); PathRemoveFileSpecW(parent); CreateDirectoryW(parent, NULL); }
    if (DirExists(tmp) || !CreateDirectoryW(tmp, NULL)) { _snwprintf(err, nerr, L2(L"não consegui criar %s (um antivírus pode estar examinando os arquivos da tentativa anterior: tente de novo em um minuto)", L"could not create %s (an antivirus may be scanning the previous attempt's files: try again in a minute)"), tmp); return FALSE; }
    NgPost(0, C_SUB, L2(L"Descompactando %s...", L"Unpacking %s..."), PathFindFileNameW(zip));
    _snwprintf(cmd, 3 * MAX_PATH + 32, L"\"%s\" -xf \"%s\" -C \"%s\"", tar, zip, tmp); cmd[3 * MAX_PATH + 31] = 0;
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi; DWORD code = 1;
    if (CreateProcessW(tar, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, tmp, &si, &pi)) {
        if (WaitForSingleObject(pi.hProcess, 20 * 60000) == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
        GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    BOOL ok = FALSE;
    if (code) _snwprintf(err, nerr, L2(L"não consegui descompactar o .zip (tar %lu): ele está inteiro?", L"could not unpack the .zip (tar %lu): is it complete?"), code);
    else if (!NgFindPortRoot(g, tmp, root, 0))
        _snwprintf(err, nerr, L2(L"esse .zip não tem o port do %s (procurei %s). Se o antivírus apagou algum arquivo, restaure-o da quarentena.", L"that .zip does not hold the %s port (looked for %s). If an antivirus removed a file, restore it from quarantine."),
                   G[g].name, BBG(g) ? L"Bloodborne.exe, bin\\bb-probe.exe" : L"inFAMOUS Second Son Launcher.exe, bin\\iss-probe.exe");
    else {
        NgPost(0, C_SUB, L2(L"Instalando o port do %s em %s...", L"Installing the %s port into %s..."), G[g].name, dest);
        ok = NgMoveInto(root, dest, err, nerr);
    }
    DelTree(tmp);
    if (ok && !NgMarkersAt(g, dest)) { ok = FALSE; _snwprintf(err, nerr, L"%s", L2(L"os arquivos do port sumiram depois de copiados: um antivírus pode ter apagado algum (restaure da quarentena e marque como permitido)", L"the port's files vanished after the copy: an antivirus may have removed one (restore it from quarantine and allow it)")); }
    if (ok) {
        wchar_t k[32]; _snwprintf(k, 32, L"port_%s", G[g].id);
        /* 6.1 review: a reinstall from a zip over a port the player kept in its own folder ("the already extracted
           folder"): that folder's user\ (saves, settings) comes along when the new place has none; the old folder stays */
        wchar_t prev[MAX_PATH] = L""; GetPrivateProfileStringW(L"PORTHEON", k, L"", prev, MAX_PATH, g_pIni); StripSlash(prev);
        if (prev[0] && _wcsicmp(prev, dest) && DirExists(prev)) {
            wchar_t ou[MAX_PATH], nu[MAX_PATH], bad[MAX_PATH] = L""; Join(ou, prev, L"user"); Join(nu, dest, L"user");
            if (DirExists(ou) && !DirExists(nu) && CopyTree(ou, nu, bad) != 0)
                NgPost(0, C_ORANGE, L2(L"Os saves do port em %s não foram copiados por inteiro (%s): copie a pasta user de lá para %s.", L"The port's saves in %s were not all copied (%s): copy its user folder to %s."), prev, bad[0] ? bad : ou, dest);
        }
        WritePrivateProfileStringW(L"PORTHEON", k, local ? dest : NULL, g_pIni);   /* the default place needs no key */
    }
    return ok;
}
/* inFAMOUS: the three binaries against the build PORTHEON was written for (v2, 2026-10-19): a note, never a block */
static void IssKnownBuild(void)
{
    static const wchar_t *const f[3] = {L"inFAMOUS Second Son Launcher.exe", L"bin\\iss-probe.exe", L"bin\\iss-gpu.dll"};
    static const char *const want[3] = {"06511cf388eb8cbcefd7e714dc649faf6667649df9dd64de7970582c2a91352a", "5b27d2d004139d2f3af61c373d5a8daf46a42ed5896e2b115eef6779ae94b16d",
                                        "aaab02cabec9ca1ac4569a3455a66c1a2a2711beeb73df29a5e549cfa7f3dc65"};
    wchar_t pd[MAX_PATH], p[MAX_PATH]; PortDir(10, pd); int known = 1;
    for (int i = 0; i < 3 && known; i++) { char h[65]; Join(p, pd, f[i]); if (!Sha256File(p, h) || strcmp(h, want[i])) known = 0; }
    PutInt(10, L"known_build", known);
}
/* the release's Windows .zip (2026-11-08: "Bloodborne-Windows.zip" beside a "...-source.zip"): a .zip named "windows" or
   "bbport" first, never a source archive; its size and its "sha256:" digest when GitHub gives one */
static BOOL BbPickAsset(const char *json, char *bestUrl, int nurl, LONGLONG *bestSize, char bestSha[65])
{
    const char *assets = strstr(json, "\"assets\""); int bestRank = 0; bestUrl[0] = 0; bestSha[0] = 0; *bestSize = 0;
    for (const char *q = assets, *prev = assets; q && (q = strstr(q, "\"browser_download_url\"")) != NULL; prev = q, q++) {
        char url[1024]; if (!JsonStr(q, "browser_download_url", url, sizeof(url))) continue;
        const char *nm = strrchr(url, '/'); nm = nm ? nm + 1 : url;
        size_t nl = strlen(nm); if (nl < 4 || _stricmp(nm + nl - 4, ".zip") || StrStrIA(nm, "source") || StrStrIA(nm, "-src")) continue;
        int rank = (StrStrIA(nm, "windows") || StrStrIA(nm, "bbport")) ? 2 : 1; if (rank <= bestRank) continue;
        /* this asset's own fields come before its url, after the previous asset's url */
        LONGLONG size = 0; char sha[80] = "";
        for (const char *s = strstr(prev, "\"size\""); s && s < q; s = strstr(s + 1, "\"size\"")) { const char *c = s + 6; while (*c == ' ' || *c == ':') c++; size = _atoi64(c); }
        for (const char *s = strstr(prev, "\"digest\""); s && s < q; s = strstr(s + 1, "\"digest\"")) JsonStr(s, "digest", sha, sizeof(sha));
        bestRank = rank; snprintf(bestUrl, nurl, "%s", url); *bestSize = size; bestSha[0] = 0;
        if (!_strnicmp(sha, "sha256:", 7) && strlen(sha + 7) == 64) strcpy(bestSha, sha + 7);
    }
    return bestUrl[0] != 0;
}
static DWORD WINAPI NgDownloadOnly(LPVOID p) { NgJob *j = (NgJob *)p; wchar_t *path = j->zip; return DownloadTo(j->url, path) ? 0 : 1; }
static DWORD WINAPI NgWorker(LPVOID unused)
{
    (void)unused; NgJob *j = &g_ngJob; int g = j->g; wchar_t err[500] = L"";
    if (j->mode == 1) {   /* the latest official release: its zip asset, its size, its SHA-256 when GitHub gives one */
        DWORD st = 0, len = 0; char *b = HttpGet(BB_RELEASES, TRUE, &st, NULL, 0, &len);
        if (!b || st != 200) { free(b); NgPost(3, C_BAD, L2(L"Não consegui ler a versão mais nova no GitHub (resposta %lu). Tente mais tarde ou baixe em %s e use \"Escolher o .zip que eu baixei\".", L"Could not read the latest release on GitHub (answer %lu). Try later, or download it at %s and use \"Choose the .zip I downloaded\"."), st, BB_RELPAGE); return 0; }
        char tag[64] = ""; JsonStr(b, "tag_name", tag, 64);
        char bestUrl[1024] = "", bestSha[65] = ""; LONGLONG bestSize = 0;
        BbPickAsset(b, bestUrl, sizeof(bestUrl), &bestSize, bestSha);
        free(b);
        if (!bestUrl[0]) { NgPost(3, C_BAD, L2(L"A versão mais nova do port no GitHub não tem um .zip para Windows. Baixe pela página %s e use \"Escolher o .zip que eu baixei\".", L"The port's latest GitHub release has no Windows .zip. Download it from %s and use \"Choose the .zip I downloaded\"."), BB_RELPAGE); return 0; }
        MultiByteToWideChar(CP_UTF8, 0, bestUrl, -1, j->url, 1024); j->url[1023] = 0;
        const wchar_t *wn = wcsrchr(j->url, L'/'); _snwprintf(j->name, 260, L"%s", wn ? wn + 1 : L"bloodborne.zip"); j->name[259] = 0;
        for (wchar_t *c = j->name; *c; c++) if (wcschr(L"\\/:*?\"<>|%", *c)) *c = L'_';   /* a file name, whatever the url says */
        MultiByteToWideChar(CP_UTF8, 0, tag, -1, j->tag, 64); j->tag[63] = 0; j->size = bestSize; strcpy(j->sha, bestSha);
        NgPost(1, 0, NULL); return 0;   /* the UI thread asks the player (ExtInstallMsg) */
    }
    if (j->mode == 2) {   /* download (resumable, cancellable from the update window's variables) then install */
        wchar_t d[MAX_PATH]; LocalData(d, L"PORTHEON"); CreateDirectoryW(d, NULL); Join(d, d, L"downloads"); CreateDirectoryW(d, NULL); Join(j->zip, d, j->name);
        if (InterlockedCompareExchange(&g_dlBusy, 1, 0)) { NgPost(3, C_ORANGE, L"%s", L2(L"O PORTHEON está baixando a própria atualização: espere terminar e instale o port depois.", L"PORTHEON is downloading its own update: wait for it to end, then install the port.")); return 0; }
        InterlockedExchange(&g_dlCancel, 0); g_dlGot = 0; g_dlTotal = 0; g_updErr[0] = 0;
        HANDLE th = CreateThread(NULL, 0, NgDownloadOnly, j, 0, NULL); DWORD rc = 1;
        if (!th) { InterlockedExchange(&g_dlBusy, 0); NgPost(3, C_BAD, L"%s", L2(L"O PORTHEON não conseguiu começar o download.", L"PORTHEON could not start the download.")); return 0; }
        LONGLONG last = -1;
        while (WaitForSingleObject(th, 1000) == WAIT_TIMEOUT) {
            LONGLONG got = g_dlGot, tot = g_dlTotal ? g_dlTotal : j->size;
            if (got != last) { last = got; NgPost(0, C_SUB, L2(L"Baixando o port do Bloodborne: %lld de %lld MB...", L"Downloading the Bloodborne port: %lld of %lld MB..."), got >> 20, tot >> 20); }
        }
        GetExitCodeThread(th, &rc); CloseHandle(th);
        wchar_t derr[300]; wcscpy(derr, g_updErr);   /* this download's error, before the lock goes */
        InterlockedExchange(&g_dlBusy, 0);
        if (rc) { NgPost(3, C_BAD, L2(L"O download parou (%s). Aperte \"Instalar o port\" de novo para continuar de onde parou.", L"The download stopped (%s). Press \"Install the port\" again to resume where it stopped."), derr[0] ? derr : L2(L"motivo desconhecido", L"unknown reason")); return 0; }
        if (j->sha[0]) {
            char h[65]; NgPost(0, C_SUB, L"%s", L2(L"Conferindo o SHA-256 do download...", L"Checking the download's SHA-256..."));
            if (!Sha256File(j->zip, h) || _stricmp(h, j->sha)) { DeleteFileW(j->zip); NgPost(3, C_BAD, L"%s", L2(L"O arquivo baixado não confere com o SHA-256 que o GitHub informa: apaguei. Tente de novo.", L"The downloaded file does not match the SHA-256 GitHub gives: deleted. Try again.")); return 0; }
        }
        if (!NgInstallZip(g, j->zip, err, 500)) { NgPost(3, C_BAD, L2(L"Não deu para instalar o port do %s: %s.", L"Could not install the %s port: %s."), G[g].name, err); return 0; }
        DeleteFileW(j->zip);   /* the port is unpacked: the 400 MB zip goes */
        NgPost(2, C_GOOD, L2(L"Port do %s %s instalado. Agora escolha a pasta do seu jogo (Arquivos do jogo).", L"%s port %s installed. Now choose your game folder (Game files)."), G[g].name, j->tag);
        return 0;
    }
    /* mode 3: the player's own zip (never deleted) */
    if (!NgInstallZip(g, j->zip, err, 500)) { NgPost(3, C_BAD, L2(L"Não deu para instalar o port do %s: %s.", L"Could not install the %s port: %s."), G[g].name, err); return 0; }
    if (ISSG(g)) IssKnownBuild();
    NgPost(2, C_GOOD, L2(L"Port do %s instalado. Agora escolha a pasta do seu jogo (Arquivos do jogo).", L"%s port installed. Now choose your game folder (Game files)."), G[g].name);
    return 0;
}
static void NgStart(int mode)
{
    g_ngJob.mode = mode;
    HANDLE h = CreateThread(NULL, 0, NgWorker, NULL, 0, NULL);
    if (h) CloseHandle(h); else { InterlockedExchange(&g_ngBusy, 0); Msg(C_BAD, L"%s", L2(L"Não consegui começar a instalação.", L"Could not start the install.")); }
}
/* the worker's news, on the UI thread (portheon.c's window procedure) */
static void ExtInstallMsg(WPARAM wp, LPARAM lp)
{
    NgNote *n = (NgNote *)lp; int g = g_ngJob.g;
    if (n) { Msg(n->c, L"%s", n->t); free(n); }
    if (wp == 1) {   /* the official release: name, version and size, then the player decides */
        wchar_t q[1200], sz[32];
        if (g_ngJob.size > 0) _snwprintf(sz, 32, L"%.0f MB", g_ngJob.size / 1048576.0); else wcscpy(sz, L"?");
        _snwprintf(q, 1200, L2(L"Baixar o port do Bloodborne da página oficial?\n\nArquivo: %s\nVersão: %s\nTamanho: %s\nDe: github.com/Supermedo/bloodborne_pc (GPL-2.0-or-later)\n%s\nO port é descompactado em games\\BLOODBORNE e o .zip é apagado depois. O jogo continua sendo o SEU (CUSA03173).",
                                L"Download the Bloodborne port from its official page?\n\nFile: %s\nVersion: %s\nSize: %s\nFrom: github.com/Supermedo/bloodborne_pc (GPL-2.0-or-later)\n%s\nThe port is unpacked into games\\BLOODBORNE and the .zip deleted afterwards. The game is still YOURS (CUSA03173)."),
                   g_ngJob.name, g_ngJob.tag[0] ? g_ngJob.tag : L"?", sz, g_ngJob.sha[0] ? L2(L"O PORTHEON confere o SHA-256 que o GitHub informa.\n", L"PORTHEON checks the SHA-256 GitHub gives.\n") : L"");
        q[1199] = 0;
        if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONQUESTION) == IDYES) NgStart(2);
        else { InterlockedExchange(&g_ngBusy, 0); Msg(C_SUB, L"%s", L2(L"Download cancelado.", L"Download cancelled.")); }
        return;
    }
    if (wp == 2 || wp == 3) {
        InterlockedExchange(&g_ngBusy, 0);
        ReadVersion(g); ForgetArt(g); g_kvGame = -1; g_stTick = 0;
        if (g_wnd) InvalidateRect(g_wnd, NULL, FALSE);
    }
}
/* the "Install the port" choice: TaskDialog command links (comctl32 6, from the manifest), a message box without it.
   1 download (Bloodborne), 2 a zip, 3 an extracted folder, 0 cancel */
typedef HRESULT (WINAPI *NgTaskDlg)(const TASKDIALOGCONFIG *, int *, int *, BOOL *);
/* 6.2: FNC Reborn: 4 = run its official installer (shipped in games\\FNCREBORN), 3 = an install that already exists, 0 cancel */
static int NgAskFnc(NgTaskDlg td)
{
    const wchar_t *head = L2(L"Instalar o FNC Reborn (Fight Night Champion)", L"Install FNC Reborn (Fight Night Champion)");
    const wchar_t *body = L2(L"O instalador oficial transforma o SEU Fight Night Champion de Xbox 360 num programa de PC: você escolhe a .iso nele, ele baixa cerca de 2,5 GB de ferramentas e compila por 15 a 60 minutos (~25 GB livres, 16 GB de memória). O PORTHEON não vem com o jogo.",
                             L"The official installer turns YOUR Xbox 360 Fight Night Champion into a PC program: you pick the .iso in it, it downloads about 2.5 GB of tools and builds for 15 to 60 minutes (~25 GB free, 16 GB of memory). PORTHEON ships no game.");
    if (td) {
        TASKDIALOG_BUTTON b[2]; b[0].nButtonID = 104; b[0].pszButtonText = L2(L"Abrir o instalador\nVocê escolhe a sua .iso e aceita a licença da Microsoft nele", L"Open the installer\nYou pick your .iso and accept the Microsoft license in it");
        b[1].nButtonID = 103; b[1].pszButtonText = L2(L"Já instalei: escolher a pasta\nA pasta do FNC Reborn (a que tem dist\\FNCReborn.exe)", L"Already installed: choose the folder\nFNC Reborn's folder (the one with dist\\FNCReborn.exe)");
        TASKDIALOGCONFIG c; memset(&c, 0, sizeof(c)); c.cbSize = sizeof(c); c.hwndParent = g_wnd; c.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
        c.dwCommonButtons = TDCBF_CANCEL_BUTTON; c.pszWindowTitle = L"PORTHEON"; c.pszMainInstruction = head; c.pszContent = body; c.pButtons = b; c.cButtons = 2;
        c.pszFooter = L"FNC Reborn by Whompay, Apache License 2.0. github.com/whompay/FNCReborn";
        int pressed = 0; if (SUCCEEDED(td(&c, &pressed, NULL, NULL))) return pressed == 104 ? 4 : pressed == 103 ? 3 : 0;
    }
    wchar_t q[900]; _snwprintf(q, 900, L"%s\n\n%s", head, body); q[899] = 0;
    return MessageBoxW(g_wnd, q, L"PORTHEON", MB_OKCANCEL | MB_ICONQUESTION) == IDOK ? 4 : 0;
}
static int NgAskSource(int g)
{
    HMODULE cc = GetModuleHandleW(L"comctl32.dll"); if (!cc) cc = LoadLibraryW(L"comctl32.dll");
    NgTaskDlg td = cc ? (NgTaskDlg)(void *)GetProcAddress(cc, "TaskDialogIndirect") : NULL;
    if (FNCG(g)) return NgAskFnc(td);   /* 6.2 */
    wchar_t head[160]; _snwprintf(head, 160, L2(L"Instalar o port do %s", L"Install the %s port"), G[g].name);
    const wchar_t *body = BBG(g) ? L2(L"O PORTHEON não vem com o port nem com o jogo. O port (bbport-windows, GPL, ~400 MB) vai para games\\BLOODBORNE; depois você escolhe a pasta do SEU jogo (CUSA03173).",
                                      L"PORTHEON ships neither the port nor the game. The port (bbport-windows, GPL, ~400 MB) goes into games\\BLOODBORNE; then you pick YOUR game folder (CUSA03173).")
                                 : L2(L"O PORTHEON não vem com o port nem com o jogo, e o pacote do port não informa uma página oficial de download: use o .zip que você baixou. Depois você escolhe a pasta do SEU jogo (CUSA00004).",
                                      L"PORTHEON ships neither the port nor the game, and the port's package names no official download page: use the .zip you downloaded. Then you pick YOUR game folder (CUSA00004).");
    if (td) {
        TASKDIALOG_BUTTON b[3]; int nb = 0;
        if (BBG(g)) { b[nb].nButtonID = 101; b[nb++].pszButtonText = L2(L"Baixar do GitHub oficial\nA versão mais nova de github.com/Supermedo/bloodborne_pc (o nome e o tamanho aparecem antes)", L"Download from the official GitHub\nThe latest release of github.com/Supermedo/bloodborne_pc (its name and size show first)"); }
        b[nb].nButtonID = 102; b[nb++].pszButtonText = L2(L"Escolher o .zip que eu baixei\nO PORTHEON descompacta e instala", L"Choose the .zip I downloaded\nPORTHEON unpacks and installs it");
        b[nb].nButtonID = 103; b[nb++].pszButtonText = L2(L"Escolher a pasta já extraída\nO port fica onde está (nada é copiado)", L"Choose the already extracted folder\nThe port stays where it is (nothing is copied)");
        TASKDIALOGCONFIG c; memset(&c, 0, sizeof(c)); c.cbSize = sizeof(c); c.hwndParent = g_wnd; c.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
        c.dwCommonButtons = TDCBF_CANCEL_BUTTON; c.pszWindowTitle = L"PORTHEON"; c.pszMainInstruction = head; c.pszContent = body; c.pButtons = b; c.cButtons = (UINT)nb;
        c.pszFooter = BBG(g) ? L2(L"Port bbport-windows: Supermedo (Mohammed Albarghouthi), sobre o bbport de deadinside28, GPL-2.0-or-later. Não afiliado à Sony nem à FromSoftware.", L"bbport-windows port: Supermedo (Mohammed Albarghouthi), on deadinside28's bbport, GPL-2.0-or-later. Not affiliated with Sony or FromSoftware.")
                             : L2(L"Port: GPL-2.0-or-later, feito sobre o shadPS4 (github.com/shadps4-emu/shadPS4). Não afiliado à Sony nem à Sucker Punch.", L"Port: GPL-2.0-or-later, built on shadPS4 (github.com/shadps4-emu/shadPS4). Not affiliated with Sony or Sucker Punch.");
        int pressed = 0;
        if (SUCCEEDED(td(&c, &pressed, NULL, NULL))) return pressed == 101 ? 1 : pressed == 102 ? 2 : pressed == 103 ? 3 : 0;
    }
    wchar_t q[900];
    if (BBG(g)) {
        _snwprintf(q, 900, L"%s\n\n%s\n\n%s", head, body, L2(L"Sim = baixar do GitHub oficial\nNão = escolher o .zip que eu baixei (Cancelar nele = a pasta já extraída)", L"Yes = download from the official GitHub\nNo = choose the .zip I downloaded (Cancel there = the already extracted folder)")); q[899] = 0;
        int r = MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNOCANCEL | MB_ICONQUESTION); return r == IDYES ? 1 : r == IDNO ? 2 : 0;
    }
    _snwprintf(q, 900, L"%s\n\n%s\n\n%s", head, body, L2(L"OK = escolher o .zip (Cancelar nele = a pasta já extraída)", L"OK = choose the .zip (Cancel there = the already extracted folder)")); q[899] = 0;
    return MessageBoxW(g_wnd, q, L"PORTHEON", MB_OKCANCEL | MB_ICONQUESTION) == IDOK ? 2 : 0;
}
/* the tile's "Install the port" (S_NOPORT), and Game files' "Install / reinstall the port" */
static void ExtInstallMenu(int g)
{
    if (!NGPORTG(g)) return;
    if (g_proc) { Msg(C_ORANGE, L"%s", L2(L"Feche o jogo antes de instalar o port.", L"Close the game before installing the port.")); return; }
    if (g_ngBusy) { Msg(C_ORANGE, L"%s", L2(L"Uma instalação de port já está em andamento.", L"A port install is already running.")); return; }
    if (g_updState >= 2 && g_updState <= 4) { Msg(C_ORANGE, L"%s", L2(L"O PORTHEON está baixando a própria atualização: espere terminar.", L"PORTHEON is downloading its own update: wait for it to end.")); return; }
    int how = NgAskSource(g); if (!how) return;
    if (how == 4) {   /* 6.2: its own window does everything; FncDetect finds the result */
        wchar_t dir[MAX_PATH], exe[MAX_PATH]; Join(dir, g_exeDir, L"games\\FNCREBORN"); Join(exe, dir, L"FNCReborn-Setup.exe");
        if (!FileExists(exe)) { Msg(C_BAD, L"%s", L2(L"O instalador do FNC Reborn não está em games\\FNCREBORN. Extraia o pacote do PORTHEON inteiro de novo.", L"FNC Reborn's installer is not in games\\FNCREBORN. Extract the whole PORTHEON package again.")); return; }
        if ((INT_PTR)ShellExecuteW(g_wnd, L"open", exe, NULL, dir, SW_SHOWNORMAL) <= 32) { Msg(C_BAD, L"%s", L2(L"Não consegui abrir o instalador do FNC Reborn (o antivírus ou o Windows bloqueou?).", L"Could not open FNC Reborn's installer (blocked by the antivirus or Windows?).")); return; }
        Msg(C_GOOD, L"%s", L2(L"Instalador do FNC Reborn aberto: acompanhe na janela dele. Quando terminar, o PORTHEON acha o jogo sozinho.", L"FNC Reborn's installer is open: follow it in its window. When it ends, PORTHEON finds the game by itself.")); return;
    }
    if (how == 2) {
        wchar_t f[MAX_PATH] = L""; OPENFILENAMEW of = {sizeof(of)}; of.hwndOwner = g_wnd; of.lpstrFilter = L"ZIP (*.zip)\0*.zip\0"; of.lpstrFile = f; of.nMaxFile = MAX_PATH;
        wchar_t t[160]; _snwprintf(t, 160, L2(L"Escolha o .zip do port do %s (Cancelar = escolher a pasta já extraída)", L"Choose the %s port's .zip (Cancel = pick the already extracted folder)"), G[g].name); of.lpstrTitle = t;
        of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&of)) {
            if (InterlockedCompareExchange(&g_ngBusy, 1, 0)) return;
            memset(&g_ngJob, 0, sizeof(g_ngJob)); g_ngJob.g = g; wcscpy(g_ngJob.zip, f); NgStart(3); return;
        }
        how = 3;
    }
    if (how == 3) {
        wchar_t p[MAX_PATH] = L"", root[MAX_PATH], t[160]; _snwprintf(t, 160, L2(L"Escolha a pasta do port do %s (a que tem %s)", L"Choose the %s port's folder (the one with %s)"), G[g].name, G[g].exe);
        if (!PickFolder(p, t)) return;
        StripSlash(p);
        if (!NgFindPortRoot(g, p, root, 0)) { Msg(C_BAD, L2(L"O port do %s não está nessa pasta (procurei %s e a pasta bin).", L"The %s port is not in that folder (looked for %s and its bin folder)."), G[g].name, G[g].exe); return; }
        if (ISSG(g) && UnderProgramFiles(root)) { Msg(C_BAD, L"%s", L2(L"O port do inFAMOUS não pode ficar em Arquivos de Programas (ele grava saves e cache na própria pasta): mova para outra pasta, por exemplo C:\\Jogos.", L"The inFAMOUS port cannot stay in Program Files (it writes saves and cache into its own folder): move it elsewhere, for example C:\\Games.")); return; }
        wchar_t k[32]; _snwprintf(k, 32, L"port_%s", G[g].id); WritePrivateProfileStringW(L"PORTHEON", k, root, g_pIni);
        if (ISSG(g)) IssKnownBuild();
        ReadVersion(g); ForgetArt(g); g_kvGame = -1; g_stTick = 0;
        if (FNCG(g)) Msg(C_GOOD, L2(L"Port do %s em %s: aperte JOGAR.", L"%s port in %s: press PLAY."), G[g].name, root); else Msg(C_GOOD, L2(L"Port do %s em %s. Agora escolha a pasta do seu jogo (Arquivos do jogo).", L"%s port in %s. Now choose your game folder (Game files)."), G[g].name, root);
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    if (InterlockedCompareExchange(&g_ngBusy, 1, 0)) return;
    memset(&g_ngJob, 0, sizeof(g_ngJob)); g_ngJob.g = g;
    Msg(C_SUB, L"%s", L2(L"Procurando a versão mais nova do port no GitHub...", L"Looking for the port's latest release on GitHub..."));
    NgStart(1);
}
/* before a PLAY of these two (StartPort): Vulkan 1.3, free commit memory for Bloodborne, a writable port folder */
static BOOL ExtPreflight(int g)
{
    if (!BBG(g) && !ISSG(g)) return TRUE;
    if (VulkanApiBelow((1u << 22) | (3u << 12))) { Msg(C_BAD, L"%s", L2(L"A placa precisa de Vulkan 1.3 (atualize o driver).", L"The card needs Vulkan 1.3 (update the driver).")); return FALSE; }
    wchar_t pd[MAX_PATH]; PortDir(g, pd);
    if (ISSG(g) && UnderProgramFiles(pd)) { Msg(C_BAD, L"%s", L2(L"O port do inFAMOUS está em Arquivos de Programas: ele não roda daí. Instale de novo noutra pasta (\"Instalar o port\").", L"The inFAMOUS port is in Program Files: it does not run from there. Install it again elsewhere (\"Install the port\").")); return FALSE; }
    if (!DirWritable(pd)) { Msg(C_BAD, L2(L"Não dá para gravar na pasta do port (%s): o port guarda saves e cache nela. Instale noutra pasta.", L"Cannot write in the port folder (%s): the port keeps saves and cache there. Install it elsewhere."), pd); return FALSE; }
    if (BBG(g)) {
        MEMORYSTATUSEX ms = {sizeof(ms)}; BOOL big = BbBigOutput();
        if (GlobalMemoryStatusEx(&ms) && ms.ullAvailPageFile < (big ? (10ull << 30) : (6ull << 30)))
            PlayWarn(L2(L"Pouca memória livre (RAM + arquivo de paginação): o Bloodborne pede ~6 GB (10 GB em 1440p/4K). Feche outros programas ou aumente o arquivo de paginação.",
                        L"Little free memory (RAM + page file): Bloodborne asks ~6 GB (10 GB at 1440p/4K). Close other programs or enlarge the page file."));
        wchar_t miss[64] = L""; if (CpuLacksPs4(miss, 64) && !wcsstr(miss, L"AVX"))
            PlayWarn(L2(L"Intel 2ª/3ª geração: o port traduz as instruções que faltam, melhor esforço.", L"Intel 2nd/3rd gen: the port translates the missing instructions, best effort."));
    }
    if (ISSG(g)) { wchar_t miss[64] = L""; if (CpuLacksPs4(miss, 64)) PlayWarn(L2(L"Este processador não tem todas as instruções do PS4 (AVX, BMI1, MOVBE, LZCNT): o inFAMOUS pode não abrir.", L"This processor lacks some PS4 instructions (AVX, BMI1, MOVBE, LZCNT): inFAMOUS may not start.")); }
    return TRUE;
}

/* ---------------------------------------------------------------- shared */
static BOOL ExtGameOk(int g, const wchar_t *d)
{
    if (ZELDAG(g)) { wchar_t f[MAX_PATH]; return ZeldaGameFile(d, f); }
    if (GOW3G(g)) return Gow3Folder(d, NULL);
    if (BBG(g)) return BbFolder(d, NULL, NULL);   /* 6.1 */
    if (ISSG(g)) return IssFolder(d);
    if (FNCG(g)) return PortOk(g);   /* 6.2: the game is built into the port */
    return FALSE;
}
/* 6.2: where FNC Reborn's installer put it: its registry value, else its default folder (read from the installer, which
   was not run here). Found = remembered as the port's folder, so PLAY works with nothing to pick */
static void FncDetect(int g)
{
    wchar_t d[MAX_PATH] = L"", e[MAX_PATH]; DWORD sz = sizeof(d);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\FNCReborn", L"InstallDir", RRF_RT_REG_SZ, NULL, d, &sz) != ERROR_SUCCESS) d[0] = 0;
    Join(e, d, G[g].exe);
    if (!d[0] || !FileExists(e)) { wchar_t sd[8] = L"C:"; GetEnvironmentVariableW(L"SystemDrive", sd, 8); _snwprintf(d, MAX_PATH, L"%s\\Games\\FNCReborn", sd); d[MAX_PATH - 1] = 0; Join(e, d, G[g].exe); if (!FileExists(e)) return; }
    wchar_t k[32]; _snwprintf(k, 32, L"port_%s", G[g].id); WritePrivateProfileStringW(L"PORTHEON", k, d, g_pIni); ReadVersion(g);
}
static int ExtStatus(int g)
{
    wchar_t d[MAX_PATH]; GameDir(g, d);
    if (ZELDAG(g)) {
        int e = ZeldaEngine(); if (!e) return S_NOPORT;
        if (e == 3) return S_READY;   /* the native build carries the game itself */
        return ExtGameOk(g, d) ? S_READY : S_PICK;
    }
    if (FNCG(g) && !PortOk(g)) FncDetect(g);   /* 6.2 */
    if (!PortOk(g)) return S_NOPORT;   /* 6.1: for Bloodborne and inFAMOUS this is "Install the port" (ExtInstallMenu) */
    if (GOW3G(g) && CpuLacksPs4(NULL, 0)) return S_NOCPU;   /* the game's own x86-64 PS4 code runs directly */
    /* 6.1: Bloodborne's port translates BMI1/MOVBE/LZCNT on Sandy/Ivy Bridge (legacy_cpu, best effort) but needs AVX;
       inFAMOUS states no CPU rule (ExtPreflight warns) */
    if (BBG(g)) { wchar_t miss[64] = L""; if (CpuLacksPs4(miss, 64) && wcsstr(miss, L"AVX")) return S_NOCPU; }
    return ExtGameOk(g, d) ? S_READY : S_PICK;
}
/* the command line, folder and environment of a PLAY (play) or of the port's own window (!play); FALSE with a message */
static BOOL ExtCommand(int g, BOOL play, wchar_t *exe, wchar_t *dir, wchar_t *cmd, int ncmd)
{
    wchar_t d[MAX_PATH]; GameDir(g, d);
    if (ZELDAG(g)) {
        int e = ZeldaEngine();
        if (e == 3 && play) {   /* the native build: its own folder, its profile beside it (the setup's jogar-ocarina.ps1) */
            wchar_t root[MAX_PATH]; ZeldaRecompRoot(root);
            _snwprintf(dir, MAX_PATH, L"%s\\build\\oot111", root); dir[MAX_PATH - 1] = 0; Join(exe, dir, L"ocarina.exe");
            _snwprintf(cmd, ncmd, L"\"%s\"", exe); cmd[ncmd - 1] = 0;
            wchar_t prof[MAX_PATH]; _snwprintf(prof, MAX_PATH, L"%s\\perfil", root); prof[MAX_PATH - 1] = 0; EnvPush(L"TOTK_PROFILE_DIR", prof);
            if (g_haveGpu && g_gpu.vendor == 0x10de) EnvPush(L"TOTK_GPU_NAME", L"NVIDIA");
            return TRUE;
        }
        if (e == 3) e = ZeldaEmuOk(1) ? 1 : 2;   /* the emulator's window for "Configure" */
        ZeldaEmuPath(e, exe); wcscpy(dir, exe); *wcsrchr(dir, L'\\') = 0;   /* MDias reads .\languages from the working folder */
        if (!FileExists(exe)) { Msg(C_BAD, T(S_NOTFOUND), L"lichii.exe"); return FALSE; }
        ZeldaConfigOnce();
        wchar_t f[MAX_PATH] = L"";
        if (play && !ZeldaGameFile(d, f)) { Msg(C_ORANGE, L"%s", L2(L"Escolha o arquivo do Zelda (.nup, .nsp ou .xci) em Arquivos do jogo.", L"Choose the Zelda file (.nup, .nsp or .xci) in Game files.")); return FALSE; }
        if (play && ZeldaNeedsKeys(f))
            PlayWarn(L2(L"Arquivos .nsp e .xci precisam das SUAS chaves do console (prod.keys e title.keys) em %APPDATA%\\eden\\keys. Sem elas o emulador não abre o jogo; um .nup não precisa delas.",
                        L".nsp and .xci files need YOUR console keys (prod.keys and title.keys) in %APPDATA%\\eden\\keys. Without them the emulator cannot open the game; a .nup does not need them."));
        if (play) _snwprintf(cmd, ncmd, L"\"%s\"%s -g \"%s\"", exe, GetInt(g, L"fullscreen", 1) ? L" -f" : L"", f);
        else _snwprintf(cmd, ncmd, L"\"%s\"", exe);
        cmd[ncmd - 1] = 0;
        /* the options, as the emulator's own switches (an environment variable wins over nx2_switches.txt) */
        static const wchar_t *const fog[4] = {L"reduced", L"on", L"off", L"original"}; int fv = GetInt(g, L"fog", 0); if (fv < 0 || fv > 3) fv = 0;
        EnvPush(L"EDEN_NX2_OOT_FOG", fog[fv]);
        EnvPush(L"EDEN_NX2_PRELOAD_ALL_PIPELINES", GetInt(g, L"preload", 1) ? L"1" : L"0");
        if (GetInt(g, L"fps30", 0)) EnvPush(L"EDEN_NX2_SWAP_INTERVAL", L"2");
        if (GetInt(g, L"amd_safe_graphics", 0)) EnvPush(L"EDEN_NX2_AMD_SAFE_COMPILE_GRAPHICS", L"1");
        if (GetInt(g, L"gpu_breadcrumbs", 0)) EnvPush(L"EDEN_NX2_GPU_BREADCRUMBS", L"1");
        /* 6.0 final: MDias writes two log lines per shader compile by default (when the game already stutters): only with
           the diagnostics switch on */
        EnvPush(L"EDEN_NX2_LOG_PIPELINE_COMPILES", GetInt(g, L"gpu_breadcrumbs", 0) ? L"1" : L"0");
        return TRUE;
    }
    if (GOW3G(g)) {   /* the port's launcher: --play runs it with its saved settings (no window), GOW3_GAME_DIR = the dump */
        PortExe(g, exe); PortDir(g, dir);
        if (play && !Gow3Folder(d, NULL)) { Msg(C_ORANGE, L"%s", L2(L"Escolha a pasta do God of War III (CUSA01623, com eboot.bin e sce_sys) em Arquivos do jogo.", L"Choose the God of War III folder (CUSA01623, with eboot.bin and sce_sys) in Game files.")); return FALSE; }
        _snwprintf(cmd, ncmd, play ? L"\"%s\" --play" : L"\"%s\"", exe); cmd[ncmd - 1] = 0;
        if (d[0]) EnvPush(L"GOW3_GAME_DIR", d);
        return TRUE;
    }
    if (BBG(g)) {   /* 6.1: the port's launcher: --play with its saved settings; the game folder goes into its settings.json */
        PortExe(g, exe); PortDir(g, dir);
        if (!FileExists(exe)) { Msg(C_ORANGE, L"%s", NgNoPortText(g)); return FALSE; }
        BOOL have = BbFolder(d, NULL, NULL);
        if (play && !have) { Msg(C_ORANGE, L"%s", L2(L"Escolha a pasta do Bloodborne (CUSA03173, com eboot.bin, sce_module e dvdroot_ps4) em Arquivos do jogo.", L"Choose the Bloodborne folder (CUSA03173, with eboot.bin, sce_module and dvdroot_ps4) in Game files.")); return FALSE; }
        if (have) {
            BOOL first = FALSE;
            int w = BbWriteGameDir(d, &first);
            if (w == -2) { Msg(C_BAD, L"%s", L2(L"O PORTHEON não conseguiu ler o %APPDATA%\\bbport-launcher\\settings.json do port (aberto em outro programa?) e não mexeu nele. Feche o launcher do port e aperte JOGAR de novo.", L"PORTHEON could not read the port's %APPDATA%\\bbport-launcher\\settings.json (open in another program?) and left it alone. Close the port's launcher and press PLAY again.")); return FALSE; }
            if (w == -3) { Msg(C_BAD, L"%s", L2(L"O %APPDATA%\\bbport-launcher\\settings.json do port não está num formato que o PORTHEON entende, e ele não mexeu no arquivo. Escolha a pasta do jogo no launcher do port.", L"The port's %APPDATA%\\bbport-launcher\\settings.json is not in a form PORTHEON understands, so it left the file alone. Choose the game folder in the port's launcher.")); return FALSE; }
            if (w < 0) { Msg(C_BAD, L"%s", L2(L"O PORTHEON não conseguiu gravar a pasta do jogo no %APPDATA%\\bbport-launcher\\settings.json do port.", L"PORTHEON could not write the game folder into the port's %APPDATA%\\bbport-launcher\\settings.json.")); return FALSE; }
            if (play && first) PlayWarn(L2(L"Primeira vez: se o launcher do port abrir em vez do jogo, termine o guia dele uma vez e aperte JOGAR de novo.", L"First time: if the port's launcher opens instead of the game, finish its guide once and press PLAY again."));
            EnvPush(L"BB_GAME_DIR", d);   /* harmless: the launcher sets its own from settings.json */
            char tid[16]; if (play && BbFolder(d, NULL, tid) && strcmp(tid, "CUSA03173")) PlayWarn(L2(L"Este dump não é o CUSA03173, a região que o port testou.", L"This dump is not CUSA03173, the region the port tested."));
        }
        _snwprintf(cmd, ncmd, play ? L"\"%s\" --play" : L"\"%s\"", exe); cmd[ncmd - 1] = 0;   /* not "Play Bloodborne.exe": one process less */
        return TRUE;
    }
    if (FNCG(g)) {   /* 6.2: FNC Reborn: the game itself, run from its dist folder */
        wchar_t pd[MAX_PATH]; PortDir(g, pd); PortExe(g, exe); Join(dir, pd, L"dist");
        if (!FileExists(exe)) { Msg(C_ORANGE, L"%s", NgNoPortText(g)); return FALSE; }
        _snwprintf(cmd, ncmd, L"\"%s\"", exe); cmd[ncmd - 1] = 0;
        return TRUE;
    }
    if (ISSG(g)) {   /* 6.1: the port's launcher has no direct play: it opens with game_dir= already written, PLAY is pressed in it */
        PortExe(g, exe); PortDir(g, dir);
        if (!FileExists(exe)) { Msg(C_ORANGE, L"%s", NgNoPortText(g)); return FALSE; }
        HWND lw = FindWindowW(L"IssLauncher", NULL);   /* single instance (Local\IssLauncherSingleInstance) */
        if (lw) { if (IsIconic(lw)) ShowWindow(lw, SW_RESTORE); SetForegroundWindow(lw); Msg(C_ORANGE, L"%s", L2(L"O launcher do inFAMOUS já está aberto: use a janela dele.", L"The inFAMOUS launcher is already open: use its window.")); return FALSE; }
        BOOL have = IssFolder(d);
        if (play && !have) { Msg(C_ORANGE, L"%s", L2(L"Escolha a pasta do inFAMOUS Second Son (CUSA00004, com eboot.bin, sce_sys e sce_module) em Arquivos do jogo.", L"Choose the inFAMOUS Second Son folder (CUSA00004, with eboot.bin, sce_sys and sce_module) in Game files.")); return FALSE; }
        if (have) {
            int w = IssWriteGameDir(d);   /* only when it changed: the launcher prepares the game again after a change */
            if (w == -2) { Msg(C_BAD, L"%s", L2(L"O PORTHEON não conseguiu ler o user\\launcher.ini do port (aberto em outro programa?) e não mexeu nele. Feche o launcher do inFAMOUS e aperte JOGAR de novo.", L"PORTHEON could not read the port's user\\launcher.ini (open in another program?) and left it alone. Close the inFAMOUS launcher and press PLAY again.")); return FALSE; }
            if (w < 0) { Msg(C_BAD, L"%s", L2(L"O PORTHEON não conseguiu gravar a pasta do jogo no user\\launcher.ini do port.", L"PORTHEON could not write the game folder into the port's user\\launcher.ini.")); return FALSE; }
            if (play && w > 0 && NonAscii(d)) PlayWarn(L2(L"A pasta do jogo tem acentos ou caracteres especiais: se o launcher não achar o jogo, use uma pasta só com letras simples (por exemplo C:\\Jogos\\CUSA00004).", L"The game folder has accented or special characters: if the launcher does not find the game, use a plain-letter folder (for example C:\\Games\\CUSA00004)."));
        }
        if (play && !GetInt(g, L"told_play", 0)) {
            PutInt(g, L"told_play", 1);
            PlayWarn(L2(L"O launcher do inFAMOUS abre com a sua pasta já escolhida: aperte PLAY nele. A primeira vez prepara o jogo e os shaders (uns 2 minutos).", L"The inFAMOUS launcher opens with your folder already chosen: press PLAY in it. The first time it prepares the game and the shaders (about 2 minutes)."));
        }
        _snwprintf(cmd, ncmd, L"\"%s\"", exe); cmd[ncmd - 1] = 0;   /* it takes no arguments */
        return TRUE;
    }
    return FALSE;
}
/* 6.0: Ocarina Recomp from PORTHEON. Unattended when the player's game is known (the setup's own OCARINA_* variables),
   else its window asks. The build goes to <the fixed drive with the most room>:\OcarinaRecomp (no spaces or accents, as
   the setup requires); when the setup ends, ZeldaRecompRoot finds the build and PLAY runs it natively. */
static void ZeldaRecompile(void)
{
    wchar_t pd[MAX_PATH], setup[MAX_PATH], ps1[MAX_PATH]; PortDir(7, pd); Join(setup, pd, L"recomp"); Join(ps1, setup, L"instalador\\Instalar-Ocarina.ps1");
    if (!FileExists(ps1)) { Msg(C_BAD, L"%s", L2(L"O Ocarina Setup não está em games\\ZELDA\\recomp. Extraia o PORTHEON inteiro de novo.", L"Ocarina Setup is not in games\\ZELDA\\recomp. Extract the whole PORTHEON package again.")); return; }
    wchar_t root[MAX_PATH]; if (ZeldaRecompRoot(root)) { Msg(C_GOOD, L2(L"O Ocarina of Time nativo já está pronto em %s: aperte JOGAR.", L"The native Ocarina of Time is already built in %s: press PLAY."), root); return; }
    /* the drive with the most free space, at least 45 GB (the setup needs ~40 GB) */
    ULONGLONG best = 0; wchar_t drive = 0; DWORD drives = GetLogicalDrives();
    for (int i = 2; i < 26; i++) {
        if (!(drives & (1u << i))) continue;
        wchar_t rt[4] = {(wchar_t)(L'A' + i), L':', L'\\', 0}; if (GetDriveTypeW(rt) != DRIVE_FIXED) continue;
        ULARGE_INTEGER fb; if (GetDiskFreeSpaceExW(rt, &fb, NULL, NULL) && fb.QuadPart > best) { best = fb.QuadPart; drive = (wchar_t)(L'A' + i); }
    }
    if (!drive || best < (45ull << 30)) { Msg(C_BAD, L2(L"A recompilação precisa de uns 40 GB livres num disco (o maior livre aqui tem %llu GB).", L"Recompiling needs about 40 GB free on one drive (the biggest free one here has %llu GB)."), best >> 30); return; }
    _snwprintf(root, MAX_PATH, L"%c:\\OcarinaRecomp", drive);
    wchar_t game[MAX_PATH], gf[MAX_PATH]; GameDir(7, game);
    BOOL haveGame = game[0] && (ZeldaGameFile(game, gf) && (!_wcsicmp(PathFindExtensionW(gf), L".nup") || DirExists(game)));
    wchar_t q[900]; _snwprintf(q, 900, L2(L"Ocarina Setup 0.2.1: recompilar o Zelda: Ocarina of Time para PC (nativo, sem emulador)\n\nO Ocarina Setup (já dentro do PORTHEON) transforma o SEU jogo (versão 1.1.1, como .nup ou como pasta com exefs e romfs) num programa de PC. Leva de 30 minutos a 2 horas e usa uns 40 GB em %s. Funciona só com este jogo e só com a versão 1.1.1.\n\n%s\n\nComeçar agora?",
                                          L"Ocarina Setup 0.2.1: recompile Zelda: Ocarina of Time for PC (native, no emulator)\n\nOcarina Setup (already inside PORTHEON) turns YOUR game (version 1.1.1, as a .nup or as a folder with exefs and romfs) into a PC program. It takes 30 minutes to 2 hours and uses about 40 GB in %s. It works only with this game and only with version 1.1.1.\n\n%s\n\nStart now?"),
                 root, haveGame ? L2(L"O PORTHEON passa o jogo que você escolheu; a janela do recompilador mostra o progresso.", L"PORTHEON passes the game you picked; the recompiler's window shows the progress.")
                                : L2(L"A janela do recompilador vai pedir o arquivo do jogo.", L"The recompiler's window will ask for the game file."));
    q[899] = 0;
    if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    PutStr(7, L"recomp_root", root);
    SetEnvironmentVariableW(L"OCARINA_SETUP", setup); SetEnvironmentVariableW(L"OCARINA_PASTA", root);
    if (haveGame) { SetEnvironmentVariableW(L"OCARINA_JOGO", game); SetEnvironmentVariableW(L"OCARINA_AUTO", L"1"); }
    /* 6.0 final: started the way the setup's own Instalar-Ocarina.exe starts it (projeto\instalador\launcher.c): PowerShell
       without a console window (CREATE_NO_WINDOW, -WindowStyle Hidden), so only the setup's own window shows */
    wchar_t ps[MAX_PATH], cl[MAX_PATH * 3 + 128]; GetSystemDirectoryW(ps, MAX_PATH); wcsncat(ps, L"\\WindowsPowerShell\\v1.0\\powershell.exe", MAX_PATH - wcslen(ps) - 1);
    _snwprintf(cl, MAX_PATH * 3 + 128, L"\"%s\" -NoProfile -Sta -WindowStyle Hidden -ExecutionPolicy Bypass -File \"%s\"", ps, ps1); cl[MAX_PATH * 3 + 127] = 0;
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi = {0};
    BOOL started = CreateProcessW(ps, cl, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, setup, &si, &pi);
    SetEnvironmentVariableW(L"OCARINA_SETUP", NULL); SetEnvironmentVariableW(L"OCARINA_PASTA", NULL); SetEnvironmentVariableW(L"OCARINA_JOGO", NULL); SetEnvironmentVariableW(L"OCARINA_AUTO", NULL);
    if (!started) { Msg(C_BAD, L"%s", L2(L"Não consegui abrir o Ocarina Setup (PowerShell).", L"Could not open Ocarina Setup (PowerShell).")); return; }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    PutInt(7, L"engine", 0);   /* Automatic: the native build as soon as it is there */
    Msg(C_GOOD, L2(L"Recompilando em %s: acompanhe na janela do recompilador. Quando terminar, o PORTHEON acha o jogo nativo sozinho e o JOGAR abre ele.", L"Recompiling into %s: follow it in the recompiler's window. When it ends, PORTHEON finds the native game by itself and PLAY opens it."), root);
}
/* where a game's logs are */
static BOOL ExtLogsDir(int g, wchar_t *out)
{
    out[0] = 0;
    if (ZELDAG(g)) {
        /* 7.1 (critic 2D): the recomp's runtime writes totk.log (and totk.1-4.log, the older ones) in a "logs" folder
           beside ocarina.exe when its output is not sent to a file (runtime\main.c log_open of Ocarina Setup 0.2.1);
           the recomp's root holds the repository (.gitignore, AGENTS.md...) and no log, so it is only the fallback */
        wchar_t r[MAX_PATH]; if (ZeldaEngine() == 3 && ZeldaRecompRoot(r)) {
            _snwprintf(out, MAX_PATH, L"%s\\build\\oot111\\logs", r); out[MAX_PATH - 1] = 0;
            if (!DirExists(out)) wcscpy(out, r);
            return TRUE;
        }
        wchar_t ad[MAX_PATH] = L""; GetEnvironmentVariableW(L"APPDATA", ad, MAX_PATH); Join(out, ad, L"eden\\log"); return DirExists(out);
    }
    if (GOW3G(g)) { wchar_t d[MAX_PATH]; PortDir(g, d); Join(out, d, L"user"); return DirExists(out); }
    if (BBG(g)) { BbUserDir(out); return DirExists(out); }   /* 6.1: last_run.log, trophies.log */
    if (FNCG(g)) { wchar_t d[MAX_PATH]; PortDir(g, d); Join(out, d, L"dist"); return DirExists(out); }   /* 6.2 */
    if (ISSG(g)) { wchar_t d[MAX_PATH]; PortDir(g, d); Join(out, d, L"user\\logs"); return DirExists(out); }   /* 6.1: stdout.log, stderr.log, a folder per session */
    return FALSE;
}
/* 7.1: Zelda's other logs for a report (MakeReport), k = 0, 1, ...: the recomp's own start script (jogar-ocarina.ps1)
   sends the game's output to ultima-partida.log / ultima-partida-erros.log in the recomp's root, and Ocarina Setup
   writes %TEMP%\ocarina-instalador.log while it recompiles. FALSE after the last one (out may not exist: ReportAdd skips it). */
static BOOL ZeldaExtraLog(int k, wchar_t *out)
{
    out[0] = 0; wchar_t r[MAX_PATH];
    if (k == 0 || k == 1) { if (ZeldaRecompRoot(r)) { _snwprintf(out, MAX_PATH, L"%s\\%s", r, k ? L"ultima-partida-erros.log" : L"ultima-partida.log"); out[MAX_PATH - 1] = 0; } return TRUE; }
    if (k == 2) { wchar_t t[MAX_PATH]; DWORD n = GetTempPathW(MAX_PATH, t); if (n && n < MAX_PATH) Join(out, t, L"ocarina-instalador.log"); return TRUE; }
    return FALSE;
}
/* the version line for the game page (gamever.c) */
static void ExtVersion(int g, GameVer *v)
{
    wchar_t d[MAX_PATH]; GameDir(g, d);
    if (ZELDAG(g)) {
        int e = ZeldaEngine(); wchar_t r[MAX_PATH];
        if (e == 3 && ZeldaRecompRoot(r)) { v->ok = 1; _snwprintf(v->tag, 64, L"%s", L2(L"1.1.1 · nativo", L"1.1.1 · native")); _snwprintf(v->line, 400, L2(L"Jogo: a versão nativa para PC feita pelo Ocarina Recomp (da versão 1.1.1), em %s.", L"Game: the native PC version made by Ocarina Recomp (from version 1.1.1), in %s."), r); return; }
        wchar_t f[MAX_PATH];
        if (!ZeldaGameFile(d, f)) { _snwprintf(v->line, 400, L"%s", L2(L"Jogo: escolha o arquivo do Zelda (.nup, .nsp ou .xci) ou a pasta extraída.", L"Game: choose the Zelda file (.nup, .nsp or .xci) or the extracted folder.")); return; }
        v->ok = 1; _snwprintf(v->tag, 64, L"%s", L"Switch 2");
        int mb = ZeldaShaderCacheMb();
        _snwprintf(v->line, 400, L2(L"Jogo: %s (no emulador %s). A recompilação nativa precisa da versão 1.1.1.%s", L"Game: %s (on the %s emulator). The native recompile needs version 1.1.1.%s"), PathFindFileNameW(f), e == 2 ? L"Lichii Frizti v3" : L"Lichii MDias v4",
                   mb > 50 ? L2(L" Cache de shaders instalado: o jogo já abre sem engasgos.", L" Shader cache installed: the game opens without stutter.") : L"");
        return;
    }
    if (GOW3G(g)) {
        char ver[16];
        if (!Gow3Folder(d, ver)) { _snwprintf(v->line, 400, L"%s", L2(L"Versão do jogo: escolha a pasta do God of War III (CUSA01623).", L"Game version: choose the God of War III folder (CUSA01623).")); return; }
        MultiByteToWideChar(CP_UTF8, 0, ver, -1, v->ver, 24);
        BOOL ok = !strcmp(ver, "01.02"); v->ok = ok ? 1 : -1;
        _snwprintf(v->tag, 64, ok ? L2(L"v%s · atualizado", L"v%s · updated") : L2(L"v%s · falta o update", L"v%s · update missing"), v->ver);
        _snwprintf(v->line, 400, ok ? L2(L"Versão do jogo: %s, a que o port precisa. Tudo certo.", L"Game version: %s, the one the port needs. All set.")
                                    : L2(L"Versão do jogo: %s. O port precisa do update 01.02 junto com o jogo (eboot.bin da 01.02).", L"Game version: %s. The port needs update 01.02 merged into the game (01.02's eboot.bin)."), v->ver);
    }
    if (BBG(g)) {   /* 6.1 */
        char ver[16], tid[16];
        if (!BbFolder(d, ver, tid)) { _snwprintf(v->line, 400, L"%s", L2(L"Versão do jogo: escolha a pasta do Bloodborne (CUSA03173).", L"Game version: choose the Bloodborne folder (CUSA03173).")); return; }
        MultiByteToWideChar(CP_UTF8, 0, ver, -1, v->ver, 24);
        BOOL ok = !strcmp(ver, "01.09"); v->ok = ok && !strcmp(tid, "CUSA03173") ? 1 : -1;
        _snwprintf(v->tag, 64, ok ? L2(L"v%s · atualizado", L"v%s · updated") : L2(L"v%s · sem os patches de 60 FPS", L"v%s · no 60 FPS patches"), v->ver);
        wchar_t wt[16]; MultiByteToWideChar(CP_UTF8, 0, tid, -1, wt, 16);
        _snwprintf(v->line, 400, ok ? L2(L"Versão do jogo: %s (%s), a que o port usa. O próprio launcher do port avisa de versões novas do port (Avançado > Verificar atualizações).", L"Game version: %s (%s), the one the port uses. The port's own launcher announces new port versions (Advanced > Check for updates).")
                                    : L2(L"Versão do jogo: %s (%s). O port roda a 30 FPS sem os patches da comunidade: ele precisa da 1.09 (o update junto com o jogo).", L"Game version: %s (%s). The port runs at 30 FPS without the community patches: it needs 1.09 (the update merged into the game)."), v->ver, wt);
    }
    if (FNCG(g)) {   /* 6.2 */
        if (PortOk(g)) { v->ok = 1; _snwprintf(v->tag, 64, L"%s", L2(L"Xbox 360 · nativo", L"Xbox 360 · native")); _snwprintf(v->line, 400, L"%s", L2(L"Jogo: compilado no seu PC a partir do seu disco de Xbox 360. Para atualizar, use \"Reinstalar / atualizar o port\".", L"Game: built on your PC from your Xbox 360 disc. To update it, use \"Reinstall / update the port\".")); }
        else _snwprintf(v->line, 400, L"%s", NgNoPortText(g));
        return;
    }
    if (ISSG(g)) {   /* 6.1: the port checks the eboot itself while preparing */
        if (!IssFolder(d)) { _snwprintf(v->line, 400, L"%s", L2(L"Jogo: escolha a pasta do inFAMOUS Second Son (CUSA00004).", L"Game: choose the inFAMOUS Second Son folder (CUSA00004).")); return; }
        v->ok = 1; _snwprintf(v->tag, 64, L"%s", L"CUSA00004 · retail");
        int known = GetInt(g, L"known_build", -1);
        _snwprintf(v->line, 400, L2(L"Jogo: CUSA00004. O port confere o eboot.bin ao preparar.%s", L"Game: CUSA00004. The port checks eboot.bin while preparing.%s"),
                   known == 1 ? L2(L" Port: versão conhecida (v2, 2026-10-19).", L" Port: known version (v2, 2026-10-19).") : known == 0 ? L2(L" Port: versão diferente da testada pelo PORTHEON.", L" Port: not the version PORTHEON knows.") : L"");
    }
}
/* 6.0: a launcher that starts the game as its child and may leave first (God of War III's): PORTHEON waits for the whole
   process tree, so the session ends when the game does; the exit code is the last process's */
static DWORD WINAPI WaitTreeThread(LPVOID p)
{
    /* 6.1 review: the tree is held by handles from the start (PTree, portheon.c) and looked at every second while it
       lives, so a game whose launcher in between leaves first (Bloodborne's "--run" before bin\bb-probe.exe) is still
       waited for; a child counts only when created after its parent */
    HANDLE root = (HANDLE)p, dup = NULL; DWORD code = 0;
    PTree *t = (PTree *)calloc(1, sizeof(PTree));
    if (!t || !DuplicateHandle(GetCurrentProcess(), root, GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        free(t); WaitForSingleObject(root, INFINITE); GetExitCodeProcess(root, &code); PostMessageW(g_wnd, WM_APP_EXITED, code, 0); return 0;
    }
    PTreeAddH(t, dup, FALSE);
    for (;;) {
        HANDLE live[MAXIMUM_WAIT_OBJECTS]; PTreeScan(t); int nl = PTreeAlive(t, live, MAXIMUM_WAIT_OBJECTS, NULL);
        if (!nl) { PTreeScan(t); if (!PTreeAlive(t, live, MAXIMUM_WAIT_OBJECTS, NULL)) break; continue; }   /* one last look for a child started at the end */
        WaitForMultipleObjects((DWORD)nl, live, FALSE, 1000);
    }
    {   /* the exit code is the last process's to end */
        ULONGLONG lastEnd = 0;
        for (int i = 0; i < t->n; i++) {
            FILETIME c, e, k, u; DWORD x = 0;
            if (GetProcessTimes(t->h[i], &c, &e, &k, &u) && GetExitCodeProcess(t->h[i], &x)) {
                ULONGLONG te = ((ULONGLONG)e.dwHighDateTime << 32) | e.dwLowDateTime;
                if (te >= lastEnd) { lastEnd = te; code = x; }
            }
        }
    }
    PTreeFree(t); free(t);
    PostMessageW(g_wnd, WM_APP_EXITED, code, 0); return 0;
}
