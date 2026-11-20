/* detect.c - part of portheon.c (included by it): which game a folder holds, and a scan of this PC for game folders (3.5).
 * A PS4 dump has sce_sys\param.sfo (TITLE_ID, APP_VER, TITLE), a PS5 dump sce_sys\param.json, the UFC Undisputed Reborn
 * folder has Xenia Canary. Only names and versions are read: nothing is copied, moved or changed. */

static volatile LONG g_scanning;
/* param.sfo: "\0PSF", the key and data table offsets, then 16-byte entries (key offset, format, length, max, offset) */
static BOOL ReadSfo(const wchar_t *path, char *tid, char *ver, char *title)
{
    tid[0] = ver[0] = title[0] = 0;
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    DWORD sz = GetFileSize(h, NULL), got = 0;
    if (sz < 20 || sz > (1u << 20)) { CloseHandle(h); return FALSE; }
    BYTE *b = (BYTE *)malloc(sz + 1); if (!b) { CloseHandle(h); return FALSE; }
    ReadFile(h, b, sz, &got, NULL); CloseHandle(h); b[sz] = 0;
    if (got == sz && !memcmp(b, "\0PSF", 4)) {
        uint32_t kt = *(uint32_t *)(b + 8), dt = *(uint32_t *)(b + 12), n = *(uint32_t *)(b + 16);
        for (uint32_t i = 0; i < n && 20 + (i + 1) * 16 <= sz; i++) {
            BYTE *e = b + 20 + i * 16; uint16_t ko = *(uint16_t *)e, fmt = *(uint16_t *)(e + 2); uint32_t len = *(uint32_t *)(e + 4), off = *(uint32_t *)(e + 12);
            if ((uint64_t)kt + ko >= sz || (uint64_t)dt + off + len > sz || fmt != 0x0204) continue;   /* 0x0204: UTF-8 text */
            const char *key = (const char *)b + kt + ko, *val = (const char *)b + dt + off;
            char *dst = !strcmp(key, "TITLE_ID") ? tid : !strcmp(key, "APP_VER") ? ver : !strcmp(key, "TITLE") ? title : NULL;
            int cap = dst == title ? 128 : 16;
            if (dst) { size_t l = strnlen(val, len); if (l >= (size_t)cap) l = cap - 1; memcpy(dst, val, l); dst[l] = 0; }
        }
    }
    free(b); return tid[0] != 0;
}
/* the UFC a PS4 title is: the known title IDs, else the name ("EA SPORTS™ UFC® 4" -> UFC 4; no number -> UFC 1) */
static int GameFromPs4(const char *tid, const char *title)
{
    static const struct { const char *id; int g; } K[] = {{"CUSA00222", 0}, {"CUSA00264", 0}, {"CUSA00267", 0}, {"CUSA01936", 1}, {"CUSA01968", 1}, {"CUSA01990", 1},
                                                          {"CUSA06534", 2}, {"CUSA06536", 2}, {"CUSA14204", 3}, {"CUSA14209", 3}};
    for (int i = 0; i < (int)(sizeof(K) / sizeof(K[0])); i++) if (!strcmp(tid, K[i].id)) return K[i].g;
    if (!strcmp(tid, "CUSA01623") || StrStrIA(title, "God of War III")) return 8;   /* 6.0: God of War III Remastered (extgames.c) */
    const char *u = strstr(title, "UFC"); if (!u || StrStrIA(title, "Undisputed")) return -1;
    for (const char *p = u + 3; *p && p < u + 14; p++) if (*p >= '2' && *p <= '5') return *p == '5' ? -1 : *p - '1';
    return 0;
}
/* a PS5 dump: sce_sys\param.json naming one of the title IDs (4.2: UFC 6; 4.3: UFC 5 again, its four regions) */
static BOOL JsonHasTitle(const wchar_t *path, const char *const *ids, int nids)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    char *b = (char *)malloc(262144); DWORD got = 0;
    if (!b) { CloseHandle(h); return FALSE; }
    ReadFile(h, b, 262143, &got, NULL); CloseHandle(h); b[got] = 0;
    BOOL yes = FALSE; for (int i = 0; i < nids && !yes; i++) yes = strstr(b, ids[i]) != NULL;
    free(b); return yes;
}
static BOOL IsUfc6Json(const wchar_t *path) { static const char *const id[] = {"PPSA23566"}; return JsonHasTitle(path, id, 1); }
static BOOL IsUfc5Json(const wchar_t *path) { static const char *const id[] = {"PPSA03541", "PPSA03540", "PPSA03539", "PPSA03542"}; return JsonHasTitle(path, id, 4); }
/* what one folder is: 0-3 a PS4 UFC (ver = APP_VER), 4 UFC 6, 5 UFC 5, -1 none (4.0: UFC Undisputed 3 left PORTHEON) */
static int DumpAt(const wchar_t *d, char *ver)
{
    wchar_t p[MAX_PATH]; ver[0] = 0;
    Join(p, d, L"sce_sys\\param.sfo");
    if (FileExists(p)) { char tid[16], title[128]; if (ReadSfo(p, tid, ver, title)) return GameFromPs4(tid, title); return -1; }
    Join(p, d, L"sce_sys\\param.json");
    if (FileExists(p)) return IsUfc6Json(p) ? 4 : IsUfc5Json(p) ? 5 : -1;
    /* 4.0.3: a PKG extracted as Image0 (the game, no sce_sys) beside Sc0 (param.sfo): the ports make Image0\sce_sys from
       Sc0 at PLAY (merge_update.py) */
    if (HasEboot(d)) {
        wchar_t up[MAX_PATH]; wcsncpy(up, d, MAX_PATH - 1); up[MAX_PATH - 1] = 0; wchar_t *s = wcsrchr(up, L'\\');
        if (s) { *s = 0; Join(p, up, L"Sc0\\param.sfo"); if (FileExists(p)) { char tid[16], title[128]; if (ReadSfo(p, tid, ver, title)) return GameFromPs4(tid, title); } }
    }
    return -1;
}
static BOOL VersionOk(int g, const char *ver)   /* the update each PS4 port runs (game_check.py of each port) */
{
    static const char *const need[4] = {"01.08", "01.13", "01.14", "28.00"};
    if (g < 0 || g > 3) return TRUE;
    return !strcmp(ver, need[g]);
}
/* which game folder d holds (d itself, or the folders right under it: the extractor's output folder, or a game and its
   update side by side, which the ports merge). why: what is missing (empty when all is right). */
static int DetectFolder(const wchar_t *d, wchar_t *outDir, wchar_t *why, int wn)
{
    if (why) why[0] = 0;
    if (!d || !d[0] || !DirExists(d)) return -1;
    char ver[16]; int g = DumpAt(d, ver);
    char best[16] = ""; int found = g; if (g >= 0 && ver[0]) strcpy(best, ver);
    if (g < 0) {
        wchar_t pat[MAX_PATH]; Join(pat, d, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
                wchar_t sub[MAX_PATH]; Join(sub, d, fd.cFileName);
                char v[16]; int sg = DumpAt(sub, v);
                if (sg < 0) continue;
                if (found < 0) found = sg;
                if (sg == found && strcmp(v, best) > 0) strcpy(best, v);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    if (found < 0) return -1;
    if (outDir) wcscpy(outDir, d);
    if (why && found <= 3 && best[0] && !VersionOk(found, best)) {
        wchar_t have[16]; MultiByteToWideChar(CP_UTF8, 0, best, -1, have, 16);
        _snwprintf(why, wn, L2(L"Encontrei o %s na versão %s, mas o port precisa do update %s. Copie o update para dentro da pasta do jogo (ou deixe as duas pastas lado a lado e escolha a pasta de fora). Veja \"Como instalar\".",
                               L"Found %s at version %s, but the port needs update %s. Copy the update into the game folder (or keep both folders side by side and pick the outer folder). See \"How to install\"."),
                   G[found].name, have, G[found].upd);
        why[wn - 1] = 0;
    }
    if (why && found <= 3 && !GameOk(d))
        _snwprintf(why, wn, L"%s", L2(L"A pasta tem o sce_sys, mas não tem o eboot.bin: o jogo não está completo ou ainda está dentro de um .pkg.", L"The folder has sce_sys but no eboot.bin: the game is incomplete or still inside a .pkg."));
    return found;
}

/* ---------------------------------------------------------------- the scan
   The player's Desktop, Downloads, Documents and every local drive, a few folders deep, skipping Windows and program
   folders, links and system folders. At most 30 s; the folders it finds go to the games that have none yet. */
typedef struct { wchar_t dir[MAX_PATH]; char ver[16]; BOOL ok; } ScanHit;
static ScanHit g_scanHit[NGAMES]; static volatile LONG g_scanDirs; static BOOL g_scanManual;
static BOOL SkipName(const wchar_t *n, BOOL atRoot)
{
    static const wchar_t *const skip[] = {L"Windows", L"Program Files", L"Program Files (x86)", L"ProgramData", L"$Recycle.Bin", L"System Volume Information",
        L"Recovery", L"PerfLogs", L"AppData", L"$WinREAgent", L"MSOCache", L"Config.Msi", L"node_modules", L".git", L"WindowsApps", L"WpSystem",
        L"XboxGames", L"Windows.old", L"$SysReset", L"OneDriveTemp", L"$GetCurrent", L"$Windows.~BT", L"$Windows.~WS", L"Intel", L"AMD", L"NVIDIA",
        L"_ShaderCache", L"_PipelineCache", L"steamapps"};
    for (int i = 0; i < (int)(sizeof(skip) / sizeof(skip[0])); i++) if (!_wcsicmp(n, skip[i])) return TRUE;
    if (atRoot && !_wcsicmp(n, L"Users")) return TRUE;   /* the user's own folders are scanned on their own */
    return n[0] == L'.';
}
typedef struct { wchar_t p[MAX_PATH]; int depth; } ScanItem;
static void ScanFrom(const wchar_t *root, int maxDepth, ULONGLONG t0)
{
    int cap = 4096, n = 0; ScanItem *q = (ScanItem *)malloc(sizeof(ScanItem) * cap); if (!q) return;
    wcscpy(q[0].p, root); q[0].depth = 0; n = 1;
    for (int head = 0; head < n; head++) {
        if (GetTickCount64() - t0 > 30000 || g_scanDirs > 120000) break;
        InterlockedIncrement(&g_scanDirs);
        ScanItem it = q[head];
        char ver[16]; int g = DumpAt(it.p, ver);
        if (g >= 0) {
            BOOL ok = g > 3 || VersionOk(g, ver);
            ScanHit *s = &g_scanHit[g];
            if (!s->dir[0] || (ok && !s->ok) || (ok == s->ok && strcmp(ver, s->ver) > 0)) { wcscpy(s->dir, it.p); strcpy(s->ver, ver); s->ok = ok; }
            continue;
        }
        if (it.depth >= maxDepth) continue;
        wchar_t pat[MAX_PATH]; Join(pat, it.p, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileExW(pat, FindExInfoBasic, &fd, FindExSearchLimitToDirectories, NULL, FIND_FIRST_EX_LARGE_FETCH);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
            if ((fd.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)) && it.depth == 0 && wcslen(it.p) <= 3) continue;
            if (SkipName(fd.cFileName, wcslen(it.p) <= 3)) continue;
            if (n >= cap) { ScanItem *q2 = (ScanItem *)realloc(q, sizeof(ScanItem) * cap * 2); if (!q2) break; q = q2; cap *= 2; }
            Join(q[n].p, it.p, fd.cFileName); q[n].depth = it.depth + 1; n++;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    free(q);
}
static DWORD WINAPI ScanThread(void *p)
{
    (void)p; SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    memset(g_scanHit, 0, sizeof(g_scanHit)); g_scanDirs = 0;
    ULONGLONG t0 = GetTickCount64();
    const KNOWNFOLDERID *kf[] = {&FOLDERID_Desktop, &FOLDERID_Downloads, &FOLDERID_Documents, &FOLDERID_Profile};
    for (int i = 0; i < 4; i++) { PWSTR d = NULL; if (SHGetKnownFolderPath(kf[i], 0, NULL, &d) == S_OK) { ScanFrom(d, i == 3 ? 2 : 5, t0); CoTaskMemFree(d); } }
    DWORD drives = GetLogicalDrives();
    for (int i = 2; i < 26; i++) {   /* C: to Z:, local disks and USB drives */
        if (!(drives & (1u << i))) continue;
        wchar_t root[4] = {(wchar_t)(L'A' + i), L':', L'\\', 0}; UINT t = GetDriveTypeW(root);
        if (t != DRIVE_FIXED && t != DRIVE_REMOVABLE) continue;
        ScanFrom(root, 5, t0);
    }
    int found = 0; for (int g = 0; g < NGAMES; g++) found += g_scanHit[g].dir[0] != 0 && !SOON(g);
    InterlockedExchange(&g_scanning, 0);
    PostMessageW(g_wnd, WM_APP_SCAN, (WPARAM)found, 0);
    return 0;
}
static void StartScan(BOOL manual)
{
    if (InterlockedCompareExchange(&g_scanning, 1, 0)) return;
    g_scanManual = manual;
    if (manual) Msg(C_SUB, L"%s", L2(L"Procurando jogos neste PC (área de trabalho, downloads, documentos e discos)...", L"Looking for games on this PC (desktop, downloads, documents and drives)..."));
    HANDLE th = CreateThread(NULL, 0, ScanThread, NULL, 0, NULL); if (th) CloseHandle(th); else InterlockedExchange(&g_scanning, 0);
}
static void SetGameFolder(int g, const wchar_t *p);
static void OnScanDone(int found)
{
    PIniSet(L"scan_done", 1);
    wchar_t list[400] = L"", warn[400] = L""; int set = 0;
    for (int g = 0; g < NGAMES; g++) {
        if (!g_scanHit[g].dir[0] || SOON(g)) continue;   /* 3.5.1: UFC 5 is "Em breve" */
        int st = Status(g);
        if (st == S_PICK) {
            PutStr(g, L"game", g_scanHit[g].dir); ForgetArt(g); set++;
            if (g == g_sel) SetWindowTextW(g_editGame, g_scanHit[g].dir);
            if (list[0]) wcsncat(list, L", ", 399 - wcslen(list));
            wcsncat(list, G[g].name, 399 - wcslen(list));
            if (!g_scanHit[g].ok && !warn[0]) { wchar_t v[16]; MultiByteToWideChar(CP_UTF8, 0, g_scanHit[g].ver, -1, v, 16);
                _snwprintf(warn, 400, L2(L" (o %s está na versão %s: falta o update %s)", L" (%s is at version %s: update %s is missing)"), G[g].name, v, G[g].upd); }
        }
    }
    if (set) Msg(warn[0] ? C_ORANGE : C_GOOD, L2(L"Encontrei e configurei: %s.%s", L"Found and set up: %s.%s"), list, warn);
    else if (g_scanManual) Msg(found ? C_SUB : C_ORANGE, L"%s", found ? L2(L"A procura terminou: os jogos encontrados já estavam configurados.", L"The search ended: the games it found were already set up.")
                                                                     : L2(L"Não encontrei jogos extraídos neste PC. Veja \"Como instalar\" ou arraste a pasta do jogo para a janela.", L"No extracted games found on this PC. See \"How to install\" or drop the game folder on the window."));
    InvalidateRect(g_wnd, NULL, FALSE);
}
