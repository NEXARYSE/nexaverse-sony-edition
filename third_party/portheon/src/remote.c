/* remote.c - part of portheon.c (included after checks.c): the remote fix list (4.3.8).
 * The user: "fix every error of every UFC, so they run on everyone's PC". A fix found in a player's log reaches every
 * PORTHEON at its next start, without a new release: PORTHEON reads portheon-fixes.txt from the project's GitHub
 * (raw.githubusercontent.com/XAndromedaXR/PORTHEON/HEAD/portheon-fixes.txt, the default branch), keeps the last good
 * copy in %LOCALAPPDATA%\PORTHEON, and applies at PLAY only what names this PC's graphics card. The file is data only,
 * never code: lines PORTHEON reads and checks, anything else is ignored.
 *
 *   # PORTHEON fixes 1                                    the first line (any other file is refused)
 *   skip|UFC6|rtx 50|61c57b5821694347,dfb275b3c29a65b2   shaders to skip (16 hex digits each), UFC1-UFC6 (UFC 5: none)
 *   warn|*|rx 5700|Texto em português|English text       a warning shown at PLAY and in "Este PC roda?"
 *
 * The card filter is "*", or words joined by "+" that must all match: "nvidia", "amd" or "intel" (the vendor), or a
 * piece of the card's name ("rtx 50", "rx 5700 xt", "1060"), case does not matter.
 * 5.2: or the driver's version (checks.c, g_vkc.drvNum: NVIDIA "581.57", AMD "26.10.1", Intel "101.6083"), compared
 * number by number: "drv<581.57", "drv<=", "drv>", "drv>=", and "drv=621" (equal in the parts given: 621.xx matches).
 * A PC whose driver could not be read matches no drv token. So a known-bad driver is named without a release:
 *   warn|UFC6|nvidia+rtx 50+drv=621|Texto|Text
 * remote_fixes=0 in portheon.ini turns it off. */
#define FIXES_URL L"https://raw.githubusercontent.com/XAndromedaXR/PORTHEON/HEAD/portheon-fixes.txt"
#define FIXES_HEAD "# PORTHEON fixes 1"
static void FixesPath(wchar_t *out) { wchar_t d[MAX_PATH]; LocalData(d, L"PORTHEON"); Join(out, d, L"portheon-fixes.txt"); }
static DWORD WINAPI FixesFetchThread(void *p)
{
    (void)p; Sleep(4000);   /* after the window and the update check */
    DWORD status = 0, len = 0; char *b = HttpGet(FIXES_URL, TRUE, &status, NULL, 0, &len);
    if (b && status == 200 && len < (256u << 10) && !strncmp(b, FIXES_HEAD, strlen(FIXES_HEAD))) {
        wchar_t path[MAX_PATH], tmp[MAX_PATH]; FixesPath(path); _snwprintf(tmp, MAX_PATH, L"%s.new", path); tmp[MAX_PATH - 1] = 0;
        HANDLE f = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) { DWORD w = 0; BOOL ok = WriteFile(f, b, len, &w, NULL) && w == len; CloseHandle(f);
                                         if (ok) MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING); else DeleteFileW(tmp); }
    }
    free(b);
    return 0;
}
static void StartFixesFetch(void)   /* g_playInstance: portheon.c (this one was started with --play) */
{
    if (!PIni(L"remote_fixes", 1)) return;
    if (g_playInstance) {   /* 6.0 final: the instance that starts a game downloads nothing while the game loads, unless the list is older than 12 h */
        wchar_t p[MAX_PATH]; FixesPath(p); WIN32_FILE_ATTRIBUTE_DATA a; FILETIME now; GetSystemTimeAsFileTime(&now);
        if (GetFileAttributesExW(p, GetFileExInfoStandard, &a)) {
            ULONGLONG t = ((ULONGLONG)a.ftLastWriteTime.dwHighDateTime << 32) | a.ftLastWriteTime.dwLowDateTime, n = ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime;
            if (n > t && n - t < 12ull * 3600 * 10000000) return;
        }
    }
    HANDLE th = CreateThread(NULL, 0, FixesFetchThread, NULL, 0, NULL); if (th) CloseHandle(th);
}
/* 5.2: two versions number by number (581.57 > 581.8: the parts are numbers, not decimals); parts compares only the
   parts b has (drv=621 equals 621.05); <0, 0, >0 */
static unsigned long VerPart(const char **p)   /* one number, and past its dot; at anything else the string ends */
{
    char *e; unsigned long v = strtoul(*p, &e, 10);
    if (e == *p) { *p += strlen(*p); return 0; }
    *p = *e == '.' ? e + 1 : e + strlen(e); return v;
}
static int DrvVerCmp(const char *a, const char *b, BOOL parts)
{
    while (*a || *b) {
        if (!*b && parts) return 0;
        unsigned long x = *a ? VerPart(&a) : 0, y = *b ? VerPart(&b) : 0;   /* a missing part counts as 0 */
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}
static BOOL DrvTokenMatch(const char *tok)   /* tok after "drv": "<581.57", ">=26.10.1", "=621" */
{
    if (!g_vkc.drvNum[0]) return FALSE;
    int op = 0;   /* 1 <, 2 <=, 3 >, 4 >=, 5 = */
    if (!strncmp(tok, "<=", 2)) { op = 2; tok += 2; } else if (!strncmp(tok, ">=", 2)) { op = 4; tok += 2; }
    else if (*tok == '<') { op = 1; tok++; } else if (*tok == '>') { op = 3; tok++; } else if (*tok == '=') { op = 5; tok++; }
    while (*tok == ' ') tok++;
    if (!op || !(*tok >= '0' && *tok <= '9')) return FALSE;
    int c = DrvVerCmp(g_vkc.drvNum, tok, op == 5);
    return op == 1 ? c < 0 : op == 2 ? c <= 0 : op == 3 ? c > 0 : op == 4 ? c >= 0 : c == 0;
}
/* the card filter (see above) against this PC's card */
static BOOL FixCardMatch(const char *filter)
{
    while (*filter == ' ') filter++;
    if (!*filter || !strcmp(filter, "*")) return TRUE;
    if (strstr(filter, "drv") || !g_haveGpu) VulkanCheck();   /* 6.0 final: the name and maker come from DetectGpu; only a driver version needs the Vulkan probe */
    char name[256] = ""; WideCharToMultiByte(CP_UTF8, 0, g_vkc.name[0] ? g_vkc.name : g_gpu.name, -1, name, 256, NULL, NULL);
    for (char *c = name; *c; c++) *c = (char)tolower((unsigned char)*c);
    uint32_t vendor = g_vkc.vendor ? g_vkc.vendor : g_gpu.vendor;
    char f[160]; strncpy(f, filter, 159); f[159] = 0;
    for (char *tok = strtok(f, "+"); tok; tok = strtok(NULL, "+")) {
        while (*tok == ' ') tok++; size_t l = strlen(tok); while (l && tok[l - 1] == ' ') tok[--l] = 0;
        for (char *c = tok; *c; c++) *c = (char)tolower((unsigned char)*c);
        if (!*tok) continue;
        if (!strcmp(tok, "nvidia")) { if (vendor != 0x10de) return FALSE; }
        else if (!strcmp(tok, "amd")) { if (vendor != 0x1002) return FALSE; }
        else if (!strcmp(tok, "intel")) { if (vendor != 0x8086) return FALSE; }
        else if (!strcmp(tok, "rdna1")) { if (!GpuIsRdna1()) return FALSE; }   /* 6.0: every Radeon RX 5000 (older versions read it as a name and match none) */
        else if (!strncmp(tok, "drv", 3) && (tok[3] == '<' || tok[3] == '>' || tok[3] == '=')) { if (!DrvTokenMatch(tok + 3)) return FALSE; }   /* 5.2 */
        else if (!strstr(name, tok)) return FALSE;
    }
    return TRUE;
}
/* every line of a kind ("skip", "warn") for game g that matches this PC: fields after the card filter, '|'-separated */
typedef void (*FixFn)(int g, char *rest, void *ctx);
static void ForFixes(int g, const char *kind, FixFn fn, void *ctx)
{
    if (X360G(g) || EXTG(g) || !PIni(L"remote_fixes", 1)) return;   /* 5.0: the fix list is for the PS4/PS5 engines (UFC Undisputed 3 never matches) */
    wchar_t path[MAX_PATH]; FixesPath(path);
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); if (f == INVALID_HANDLE_VALUE) return;
    DWORD sz = GetFileSize(f, NULL); if (sz == INVALID_FILE_SIZE || sz > (256u << 10)) { CloseHandle(f); return; }
    char *b = (char *)malloc(sz + 1); DWORD got = 0; if (!b) { CloseHandle(f); return; }
    ReadFile(f, b, sz, &got, NULL); CloseHandle(f); b[got] = 0;
    if (strncmp(b, FIXES_HEAD, strlen(FIXES_HEAD))) { free(b); return; }
    char gid[16]; WideCharToMultiByte(CP_UTF8, 0, G[g].id, -1, gid, 16, NULL, NULL);
    for (char *line = b, *next; line && *line; line = next) {
        next = strpbrk(line, "\r\n"); if (next) { *next++ = 0; while (*next == '\r' || *next == '\n') next++; }
        if (*line == '#' || !*line) continue;
        char *p1 = strchr(line, '|'); if (!p1) continue; *p1++ = 0;
        char *p2 = strchr(p1, '|'); if (!p2) continue; *p2++ = 0;
        char *p3 = strchr(p2, '|'); if (!p3) continue; *p3++ = 0;
        if (_stricmp(line, kind)) continue;
        if (strcmp(p1, "*") && _stricmp(p1, gid)) continue;
        if (!FixCardMatch(p2)) continue;
        fn(g, p3, ctx);
    }
    free(b);
}
/* skip lines: 16-hex-digit hashes, comma-separated (anything else is left out) */
typedef struct { char list[2048]; int n; } SkipAcc;
static void FixSkipCb(int g, char *rest, void *ctx)
{
    (void)g; SkipAcc *a = (SkipAcc *)ctx;
    for (char *h = strtok(rest, ","); h; h = strtok(NULL, ",")) {
        while (*h == ' ') h++; int k = 0; while (isxdigit((unsigned char)h[k])) k++;
        if (k != 16 || (h[16] && h[16] != ' ')) continue;
        char hex[17]; for (int i = 0; i < 16; i++) hex[i] = (char)tolower((unsigned char)h[i]); hex[16] = 0;
        if (strstr(a->list, hex) || strlen(a->list) + 18 >= sizeof(a->list)) continue;
        if (a->n++) strcat(a->list, ","); strcat(a->list, hex);
    }
}
static void FixWarnCb(int g, char *rest, void *ctx)
{
    (void)g; wchar_t *out = (wchar_t *)ctx; char *en = strchr(rest, '|'); if (en) *en++ = 0;
    const char *t = g_lang == LANG_PT || !en ? rest : en;
    wchar_t w[400]; MultiByteToWideChar(CP_UTF8, 0, t, -1, w, 400); w[399] = 0;
    size_t l = wcslen(out); if (l + wcslen(w) + 4 < 1200) { if (l) wcscat(out, L"\n"); wcscat(out, w); }
}
static void FixWarnings(int g, wchar_t out[1200]) { out[0] = 0; ForFixes(g, "warn", FixWarnCb, out); }
/* at PLAY: UFC 1-4 get the hashes in their skip file (BB_SKIP_FILE), UFC 6 in games\UFC6\bin\portheon-remote-skip.txt,
   which UFC6.exe adds to KYTY_DEBUG_SKIP_SHADERS (1.13.0). The warnings are said after the start message. */
static void FixesApply(int g)
{
    SkipAcc a; memset(&a, 0, sizeof(a)); ForFixes(g, "skip", FixSkipCb, &a);
    if (PS4G(g) && G[g].data && a.n) {
        wchar_t sf[MAX_PATH]; SkipFilePath(g, sf); char t[16384] = ""; DWORD n = 0;
        HANDLE r = CreateFileW(sf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (r != INVALID_HANDLE_VALUE) { ReadFile(r, t, sizeof(t) - 1, &n, NULL); CloseHandle(r); t[n] = 0; }
        wchar_t dd[MAX_PATH]; wcscpy(dd, sf); wchar_t *sl = wcsrchr(dd, L'\\'); if (sl) { *sl = 0; CreateDirectoryW(dd, NULL); }
        HANDLE w = CreateFileW(sf, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (w != INVALID_HANDLE_VALUE) {
            char l2[2048]; strcpy(l2, a.list);
            for (char *h = strtok(l2, ","); h; h = strtok(NULL, ",")) if (!strstr(t, h)) { DWORD wr; WriteFile(w, h, 16, &wr, NULL); WriteFile(w, "\r\n", 2, &wr, NULL); }
            CloseHandle(w);
        }
    }
    if (UFC6G(g)) {
        wchar_t d[MAX_PATH], f[MAX_PATH]; PortDir(g, d); Join(f, d, L"bin\\portheon-remote-skip.txt");
        if (!a.n) DeleteFileW(f);
        else { HANDLE w = CreateFileW(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); if (w != INVALID_HANDLE_VALUE) { DWORD wr; WriteFile(w, a.list, (DWORD)strlen(a.list), &wr, NULL); CloseHandle(w); } }
    }
}
