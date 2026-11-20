/* steam.c - PORTHEON 5.0: non-Steam shortcuts with artwork. C, Win32, llvm-mingw, no C++. See steam.h.
 *
 * shortcuts.vdf (binary VDF, "KeyValues" binary):
 *   node  = 0x00 key\0 { node* } 0x08      map
 *         | 0x01 key\0 value\0              string (UTF-8)
 *         | 0x02 key\0 int32-LE             int
 *         ( 0x03 float, 0x07 uint64 exist in KeyValues but Steam does not write them here; kept raw )
 *   file  = 0x00 "shortcuts"\0 { 0x00 "0"\0 { appid, AppName, Exe, StartDir, icon, ShortcutPath, LaunchOptions,
 *           IsHidden, AllowDesktopConfig, AllowOverlay, OpenVR, Devkit, DevkitGameID, DevkitOverrideAppID,
 *           LastPlayTime, FlatpakAppID, 0x00 "tags"\0 { 0x01 "0"\0 "favorite"\0 ... } 0x08 } 0x08  ... } 0x08 0x08
 *   (the last 0x08 ends the file's root list: an empty list is 00 "shortcuts" 00 08 08, a list whose last entry has
 *   tags ends 08 08 08 08)
 * Keys are case-insensitive for Steam; entries are numbered "0".."n-1" in order.
 * 7.1 (players: "it says added, but nothing shows in Steam"): 5.0 to 7.0 wrote the file without the root list's 0x08,
 * one byte short. Steam's binary KeyValues reader only ends a list at an 0x08, so it refused the whole file (its own
 * appcache\appinfo.vdf blobs, 471 of 471, end 08 08). The file is now written whole and checked before it is written;
 * an existing file is kept only when it reads back byte for byte (or is a 5.0-7.0 file, one 0x08 short, which is
 * repaired); anything else is left alone. The first backup (shortcuts.vdf.portheon.bak) is kept, the one before each
 * write is shortcuts.vdf.portheon.last.bak. SteamLastTrace says what the last write did (steam.txt in reports).
 * 7.1 review: both backups are written so that Steam can read them (a one-short file gets its 0x08, a .bak 5.0-7.0 left
 * one short is repaired in place); SteamLastBakKind says whether the first backup is the list from before PORTHEON's
 * first click; the restore from it runs only before 7.1's first write to that list (no .portheon.last.bak yet).
 */
#include "steam.h"
#include <tlhelp32.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------------------------------------------------------------- small helpers */
static void SetErr(wchar_t *out, int cap, const wchar_t *msg) { if (out && cap > 0) { wcsncpy(out, msg, cap - 1); out[cap - 1] = 0; } }
/* 7.1: what the last SteamAddShortcut / SteamRemoveShortcut did (SteamLastTrace, SteamLastState, SteamLastRestored):
   the account choice, the state of the file it found, the sizes and the last bytes. No path and no full account id. */
static wchar_t g_stHow[48] = L"-";      /* how the account was chosen */
static int g_stAccounts = -1;           /* numeric userdata folders */
static uint32_t g_stId3;                /* the chosen account (only its last 3 digits go to the trace) */
static int g_stState = -2;              /* the file found: 0 none (or empty), 1 valid, 2 repaired (one 0x08 short), -1 refused, -2 not read */
static DWORD g_stInSize, g_stOutSize;
static unsigned char g_stOutTail[4]; static int g_stOutTailN;
static int g_stRestored;                /* entries taken back from the first backup */
static int g_stBakKind;                 /* 7.1 review: the first backup after the write: 0 none, 1 the list from before PORTHEON, 2 later */
static wchar_t g_stTrace[400];
static char *Utf8(const wchar_t *w)   /* malloc'd UTF-8 copy ("" for NULL) */
{
    if (!w) w = L"";
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL); if (n <= 0) n = 1;
    char *s = (char *)malloc(n); if (!s) return NULL;
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL) <= 0) s[0] = 0;
    return s;
}
static BOOL ReadAll(const wchar_t *path, unsigned char **out, DWORD *len)   /* FALSE when the file does not exist */
{
    *out = NULL; *len = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    DWORD sz = GetFileSize(f, NULL), got = 0; BOOL ok = FALSE;
    if (sz != INVALID_FILE_SIZE && sz < (64u << 20)) {
        unsigned char *b = (unsigned char *)malloc(sz + 1);
        if (b && (sz == 0 || (ReadFile(f, b, sz, &got, NULL) && got == sz))) { b[sz] = 0; *out = b; *len = sz; ok = TRUE; }
        else free(b);
    }
    CloseHandle(f); return ok;
}
static BOOL WriteAll(const wchar_t *path, const unsigned char *b, DWORD n)
{
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    DWORD wr = 0; BOOL ok = (n == 0) || (WriteFile(f, b, n, &wr, NULL) && wr == n);
    ok = FlushFileBuffers(f) && ok; CloseHandle(f); return ok;
}
static void Backslashes(wchar_t *s) { for (; *s; s++) if (*s == L'/') *s = L'\\'; }
static void TrimSlash(wchar_t *s) { size_t n = wcslen(s); while (n > 3 && (s[n - 1] == L'\\' || s[n - 1] == L'/')) s[--n] = 0; }

/* ---------------------------------------------------------------- crc32 (zlib polynomial), for the appid */
static uint32_t Crc32(const unsigned char *p, size_t n)
{
    static uint32_t tab[256]; static int made = 0;
    if (!made) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; tab[i] = c; } made = 1; }
    uint32_t c = 0xFFFFFFFFu; for (size_t i = 0; i < n; i++) c = tab[(c ^ p[i]) & 0xFF] ^ (c >> 8); return c ^ 0xFFFFFFFFu;
}
static char *QuotedUtf8(const wchar_t *path)   /* "\"C:\\...\"" as Steam writes Exe and StartDir */
{
    char *u = Utf8(path); if (!u) return NULL;
    size_t n = strlen(u); char *q = (char *)malloc(n + 3); if (!q) { free(u); return NULL; }
    q[0] = '"'; memcpy(q + 1, u, n); q[n + 1] = '"'; q[n + 2] = 0; free(u); return q;
}
static unsigned int AppIdOf(const char *exeQuoted, const char *appName)
{
    size_t a = strlen(exeQuoted), b = strlen(appName); unsigned char *s = (unsigned char *)malloc(a + b + 1);
    if (!s) return 0;
    memcpy(s, exeQuoted, a); memcpy(s + a, appName, b + 1);
    unsigned int id = Crc32(s, a + b) | 0x80000000u; free(s); return id;
}
unsigned int SteamShortcutAppId(const wchar_t *appName, const wchar_t *exe)
{
    char *q = QuotedUtf8(exe), *n = Utf8(appName); unsigned int id = (q && n) ? AppIdOf(q, n) : 0; free(q); free(n); return id;
}

/* ---------------------------------------------------------------- the binary VDF tree */
typedef struct Node { int type; char *key; char *str; int32_t i; unsigned char raw[8]; struct Node *child, *next; } Node;
static Node *NewNode(int type, const char *key) { Node *n = (Node *)calloc(1, sizeof *n); if (n) { n->type = type; n->key = _strdup(key ? key : ""); } return n; }
static void FreeNodes(Node *n) { while (n) { Node *nx = n->next; FreeNodes(n->child); free(n->key); free(n->str); free(n); n = nx; } }
static Node *AddStr(const char *key, const char *v) { Node *n = NewNode(1, key); if (n) { n->str = _strdup(v ? v : ""); } return n; }
static Node *AddInt(const char *key, int32_t v) { Node *n = NewNode(2, key); if (n) n->i = v; return n; }
static void Append(Node *map, Node *n) { if (!n) return; Node **p = &map->child; while (*p) p = &(*p)->next; *p = n; }
static Node *Find(Node *map, const char *key) { for (Node *n = map ? map->child : NULL; n; n = n->next) if (!_stricmp(n->key, key)) return n; return NULL; }
static const char *Str(Node *map, const char *key) { Node *n = Find(map, key); return n && n->type == 1 && n->str ? n->str : ""; }
static int32_t Int(Node *map, const char *key, int32_t def) { Node *n = Find(map, key); return n && n->type == 2 ? n->i : def; }

/* parse: returns the list of nodes until 0x08 (or the end of the data); *pos moves */
static Node *Parse(const unsigned char *b, DWORD len, DWORD *pos, int depth)
{
    Node *head = NULL, **tail = &head;
    while (*pos < len) {
        int t = b[(*pos)++];
        if (t == 0x08) break;
        if (t > 0x07 || depth > 32) break;   /* corrupt: keep what was read */
        const char *key = (const char *)b + *pos; size_t kl = strnlen(key, len - *pos); if (*pos + kl >= len) break;
        *pos += (DWORD)kl + 1;
        Node *n = NewNode(t, key); if (!n) break;
        if (t == 0x00) n->child = Parse(b, len, pos, depth + 1);
        else if (t == 0x01) { const char *v = (const char *)b + *pos; size_t vl = strnlen(v, len - *pos); if (*pos + vl >= len) { free(n->key); free(n); break; } n->str = _strdup(v); *pos += (DWORD)vl + 1; }
        else { int w = (t == 0x07 || t == 0x0A) ? 8 : 4; if (*pos + w > len) { free(n->key); free(n); break; } memcpy(n->raw, b + *pos, w); if (w == 4) memcpy(&n->i, n->raw, 4); *pos += w; }
        *tail = n; tail = &n->next;
    }
    return head;
}
/* serialize into a growing buffer */
typedef struct { unsigned char *p; size_t n, cap; } Buf;
static BOOL Put(Buf *b, const void *d, size_t n)
{
    if (b->n + n > b->cap) { size_t c = b->cap ? b->cap * 2 : 4096; while (c < b->n + n) c *= 2; unsigned char *q = (unsigned char *)realloc(b->p, c); if (!q) return FALSE; b->p = q; b->cap = c; }
    memcpy(b->p + b->n, d, n); b->n += n; return TRUE;
}
static BOOL Emit(Buf *b, const Node *n)
{
    for (; n; n = n->next) {
        unsigned char t = (unsigned char)n->type;
        if (!Put(b, &t, 1) || !Put(b, n->key, strlen(n->key) + 1)) return FALSE;
        if (n->type == 0x00) { unsigned char e = 0x08; if (!Emit(b, n->child) || !Put(b, &e, 1)) return FALSE; }
        else if (n->type == 0x01) { const char *s = n->str ? n->str : ""; if (!Put(b, s, strlen(s) + 1)) return FALSE; }
        else if (n->type == 0x02) { if (!Put(b, &n->i, 4)) return FALSE; }
        else { if (!Put(b, n->raw, (n->type == 0x07 || n->type == 0x0A) ? 8 : 4)) return FALSE; }
    }
    return TRUE;
}
/* 7.1: the whole file: the root list and its own 0x08 */
static BOOL EmitFile(Buf *b, const Node *root) { unsigned char rootEnd = 0x08; return Emit(b, root) && Put(b, &rootEnd, 1); }
/* 7.1: Steam's reading of the file, strict: every list (the root's too) ends with its 0x08 and nothing comes after it */
static BOOL StrictList(const unsigned char *b, DWORD len, DWORD *pos, int depth)
{
    if (depth > 32) return FALSE;
    for (;;) {
        if (*pos >= len) return FALSE;                                   /* the data ended before this list's 0x08 */
        int t = b[(*pos)++];
        if (t == 0x08) return TRUE;
        size_t kl = strnlen((const char *)b + *pos, len - *pos); if (*pos + kl >= len) return FALSE; *pos += (DWORD)kl + 1;
        if (t == 0x00) { if (!StrictList(b, len, pos, depth + 1)) return FALSE; }
        else if (t == 0x01) { size_t vl = strnlen((const char *)b + *pos, len - *pos); if (*pos + vl >= len) return FALSE; *pos += (DWORD)vl + 1; }
        else if (t == 0x02 || t == 0x03 || t == 0x04 || t == 0x06) { if (*pos + 4 > len) return FALSE; *pos += 4; }
        else if (t == 0x07) { if (*pos + 8 > len) return FALSE; *pos += 8; }
        else return FALSE;
    }
}
static BOOL SteamFormatOk(const unsigned char *b, DWORD n) { DWORD pos = 0; return n && StrictList(b, n, &pos, 0) && pos == n; }
/* 7.1: reads a shortcuts.vdf and says whether it may be written back: 1 it reads back byte for byte, 2 it is a
   5.0-7.0 file one 0x08 short (repaired by the next write), 0 empty, -1 anything else (a type PORTHEON does not know,
   cut-off data, bytes after the end: writing it back would drop what follows). *rootOut gets the tree (lenient). */
static int ReadShortcutsFile(const unsigned char *b, DWORD n, Node **rootOut)
{
    DWORD pos = 0; *rootOut = n ? Parse(b, n, &pos, 0) : NULL;
    if (!n) return 0;
    Buf re = {0}; int st = -1;
    if (EmitFile(&re, *rootOut)) {
        if (re.n == n && !memcmp(re.p, b, n)) st = 1;
        else if (re.n == (size_t)n + 1 && !memcmp(re.p, b, n) && re.p[n] == 0x08) st = 2;
    }
    free(re.p); return st;
}

/* ---------------------------------------------------------------- Steam's folders */
BOOL SteamIsRunning(void)
{
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (s == INVALID_HANDLE_VALUE) return FALSE;
    PROCESSENTRY32W e; e.dwSize = sizeof e; BOOL found = FALSE;
    if (Process32FirstW(s, &e)) do { if (!_wcsicmp(e.szExeFile, L"steam.exe")) { found = TRUE; break; } } while (Process32NextW(s, &e));
    CloseHandle(s); return found;
}
static BOOL RegStr(HKEY root, const wchar_t *sub, const wchar_t *name, wchar_t *out, int cap)
{
    DWORD sz = (DWORD)(cap * sizeof(wchar_t)), type = 0;
    if (RegGetValueW(root, sub, name, RRF_RT_REG_SZ | 0x00020000 /* RRF_SUBKEY_WOW6432KEY */, &type, out, &sz) != ERROR_SUCCESS)
        if (RegGetValueW(root, sub, name, RRF_RT_REG_SZ, &type, out, &sz) != ERROR_SUCCESS) return FALSE;
    out[cap - 1] = 0; return out[0] != 0;
}
BOOL SteamFindPath(wchar_t *out, int cap)
{
    DWORD ov = GetEnvironmentVariableW(L"PORTHEON_STEAM_PATH", out, cap);   /* tests: a folder that stands in for Steam's */
    if (ov && ov < (DWORD)cap) { Backslashes(out); TrimSlash(out); return GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES; }
    /* 7.1: HKLM's InstallPath also when HKCU's SteamPath names a folder that is gone (Steam moved or reinstalled) */
    if (RegStr(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", out, cap)) {
        Backslashes(out); TrimSlash(out);
        if (GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES) return TRUE;
    }
    if (!RegStr(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath", out, cap)) return FALSE;
    Backslashes(out); TrimSlash(out);
    return GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES;
}
/* loginusers.vdf (text VDF): "users" { "<steamid64>" { "AccountName" ".." "MostRecent" "1" "Timestamp" ".." } }:
   the account with MostRecent 1, else the newest Timestamp. id3 = steamid64 - 76561197960265728. Steam of 2026 writes
   no MostRecent and a lowercase "timestamp" (_stricmp matches it). *how: 1 MostRecent, 2 newest timestamp. */
static uint32_t MostRecentId3(const wchar_t *steamDir, int *how)
{
    if (how) *how = 0;
    wchar_t p[MAX_PATH]; _snwprintf(p, MAX_PATH, L"%s\\config\\loginusers.vdf", steamDir); p[MAX_PATH - 1] = 0;
    unsigned char *b; DWORD n; if (!ReadAll(p, &b, &n)) return 0;
    uint64_t best = 0, cur = 0; long long bestTs = -1; int depth = 0, by = 0; char tok[2][256]; int ti = 0;
    const char *s = (const char *)b, *e = s + n;
    while (s < e) {   /* the token stream: quoted strings and braces */
        if (*s == '{') { depth++; s++; ti = 0; continue; }
        if (*s == '}') { depth--; s++; ti = 0; continue; }
        if (*s == '/' && s + 1 < e && s[1] == '/') { while (s < e && *s != '\n') s++; continue; }
        if (*s != '"') { s++; continue; }
        /* 7.1: Steam writes a quote inside a value as \" (and a backslash as \\): a persona name with a quote put keys
           and values out of step, and the file went to another account (steam-2) */
        const char *q = ++s; while (s < e && *s != '"') { if (*s == '\\' && s + 1 < e) s++; s++; }
        size_t l = (size_t)(s - q); if (l > 255) l = 255; memcpy(tok[ti], q, l); tok[ti][l] = 0; if (s < e) s++;
        if (depth == 1 && ti == 0) { cur = _strtoui64(tok[0], NULL, 10); ti = 0; continue; }   /* the steamid64 key */
        if (depth == 2) {
            if (ti == 0) { ti = 1; continue; }
            ti = 0;
            if (!_stricmp(tok[0], "MostRecent") && atoi(tok[1]) == 1 && cur) { best = cur; bestTs = 0x7FFFFFFFFFFFFFFFll; by = 1; }
            else if (!_stricmp(tok[0], "Timestamp") && cur) { long long t = _atoi64(tok[1]); if (t > bestTs) { bestTs = t; best = cur; by = 2; } }
        }
    }
    free(b);
    if (how) *how = best ? by : 0;
    return best ? (uint32_t)(best - 76561197960265728ull) : 0;
}
BOOL SteamFindUserDir(wchar_t *out, int cap)
{
    wchar_t sd[MAX_PATH]; if (!SteamFindPath(sd, MAX_PATH)) return FALSE;
    int how = 0; uint32_t id3 = MostRecentId3(sd, &how);
    /* the numeric userdata folders (counted for the trace, and the only one is the fallback) */
    wchar_t pat[MAX_PATH]; _snwprintf(pat, MAX_PATH, L"%s\\userdata\\*", sd); pat[MAX_PATH - 1] = 0;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); int count = 0; wchar_t one[64] = L"";
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] < L'1' || fd.cFileName[0] > L'9') continue;
            if (wcscmp(fd.cFileName, L"0") == 0) continue;
            count++; wcsncpy(one, fd.cFileName, 63); one[63] = 0;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    g_stAccounts = count;
    if (id3) {
        _snwprintf(out, cap, L"%s\\userdata\\%u", sd, id3); out[cap - 1] = 0;
        if (GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES) { wcscpy(g_stHow, how == 1 ? L"MostRecent" : L"newest timestamp"); g_stId3 = id3; return TRUE; }
    }
    /* no loginusers.vdf (or its account has no folder): the only numeric userdata folder */
    if (count != 1) { wcscpy(g_stHow, id3 ? L"login account has no folder" : L"none"); g_stId3 = 0; return FALSE; }
    _snwprintf(out, cap, L"%s\\userdata\\%s", sd, one); out[cap - 1] = 0;
    wcscpy(g_stHow, L"only folder"); g_stId3 = (uint32_t)wcstoul(one, NULL, 10); return TRUE;
}

/* ---------------------------------------------------------------- shortcuts.vdf in and out */
/* root list; "shortcuts" map made when missing. 7.1: forWrite (Add, Remove) refuses a file that ReadShortcutsFile says
   cannot be written back (NULL, g_stState -1): the player's other non-Steam games stay as they are. The list of the
   library page (SteamListShortcuts) reads any file. */
static Node *LoadShortcuts(const wchar_t *userDir, wchar_t *vdfPath, wchar_t *err, int errCap, BOOL forWrite)
{
    _snwprintf(vdfPath, MAX_PATH, L"%s\\config\\shortcuts.vdf", userDir); vdfPath[MAX_PATH - 1] = 0;
    unsigned char *b = NULL; DWORD n = 0; Node *root = NULL; int st = 0;
    if (ReadAll(vdfPath, &b, &n)) { st = ReadShortcutsFile(b, n, &root); free(b); }
    if (forWrite) { g_stState = st; g_stInSize = n; }
    if (forWrite && st < 0) {
        FreeNodes(root);
        SetErr(err, errCap, L"the existing shortcuts.vdf is in a form PORTHEON does not know (it does not read back byte for byte): nothing was changed");
        return NULL;
    }
    Node *sc = NULL; for (Node *x = root; x; x = x->next) if (x->type == 0 && !_stricmp(x->key, "shortcuts")) { sc = x; break; }
    if (!sc) { sc = NewNode(0, "shortcuts"); if (!sc) { FreeNodes(root); SetErr(err, errCap, L"out of memory"); return NULL; } sc->next = root; root = sc; }
    return root;
}
static Node *ShortcutsMap(Node *root) { for (; root; root = root->next) if (root->type == 0 && !_stricmp(root->key, "shortcuts")) return root; return NULL; }
static BOOL SameExe(const char *a, const char *b)   /* quoted or not, case-insensitive */
{
    size_t la = strlen(a), lb = strlen(b);
    if (la >= 2 && a[0] == '"' && a[la - 1] == '"') { a++; la -= 2; }
    if (lb >= 2 && b[0] == '"' && b[lb - 1] == '"') { b++; lb -= 2; }
    return la == lb && _strnicmp(a, b, la) == 0;
}
static Node *FindEntry(Node *sc, const char *appName, const char *exeQuoted, Node ***linkOut)
{
    for (Node **p = &sc->child; *p; p = &(*p)->next) {
        Node *e = *p; if (e->type != 0) continue;
        if (!_stricmp(Str(e, "AppName"), appName) && SameExe(Str(e, "Exe"), exeQuoted)) { if (linkOut) *linkOut = p; return e; }
    }
    return NULL;
}
static void Renumber(Node *sc) { int i = 0; for (Node *e = sc->child; e; e = e->next) if (e->type == 0) { char k[16]; sprintf(k, "%d", i++); free(e->key); e->key = _strdup(k); } }
/* 7.1 review: an entry of PORTHEON's own: this PORTHEON.exe, or library.c's rule for any PORTHEON (Exe names PORTHEON
   and LaunchOptions holds --play) */
static BOOL PortheonEntry(Node *e, const char *ourExeQ)
{
    const char *exe = Str(e, "Exe");
    return SameExe(exe, ourExeQ) || (StrStrIA(exe, "PORTHEON") && strstr(Str(e, "LaunchOptions"), "--play"));
}
/* 7.1 review (correctness#3, player#3, safety#1): copies src to dst so that Steam can read the copy: a 5.0-7.0 file (one
   0x08 short, ReadShortcutsFile state 2) gets its 0x08, anything else is copied byte for byte. keep: an existing dst
   stays. src == dst repairs a one-short file in place (and leaves any other file as it is). */
static void CopyRepaired(const wchar_t *src, const wchar_t *dst, BOOL keep)
{
    BOOL same = !_wcsicmp(src, dst);
    if (keep && !same && GetFileAttributesW(dst) != INVALID_FILE_ATTRIBUTES) return;
    unsigned char *b = NULL; DWORD n = 0; Node *r = NULL;
    if (!ReadAll(src, &b, &n)) return;
    int st = ReadShortcutsFile(b, n, &r); FreeNodes(r);
    if (st == 2) {   /* ReadAll leaves room for one more byte */
        b[n] = 0x08;
        wchar_t tmp[MAX_PATH]; _snwprintf(tmp, MAX_PATH, L"%s.tmp", dst); tmp[MAX_PATH - 1] = 0;
        if (!WriteAll(tmp, b, n + 1) || !MoveFileExW(tmp, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) DeleteFileW(tmp);
    } else if (!same) CopyFileW(src, dst, keep);
    free(b);
}
/* 7.1 review: what the first backup holds, for AddToSteam's text. 0: none. 1: a list from before any PORTHEON entry (it
   reads back byte for byte, holds at least one game and no PORTHEON entry): "the list as it was before PORTHEON's first
   click" is true only then. 2: anything else (a PORTHEON entry in it, no game, or another form): it was written after a
   5.0-7.0 click (5.0-7.0 overwrote the backup at every click) or is Steam's emptied list. */
static int BackupKind(const wchar_t *vdfPath, const char *ourExeQ)
{
    wchar_t bak[MAX_PATH]; _snwprintf(bak, MAX_PATH, L"%s.portheon.bak", vdfPath); bak[MAX_PATH - 1] = 0;
    unsigned char *b = NULL; DWORD n = 0; if (!ReadAll(bak, &b, &n)) return 0;
    Node *r = NULL; int st = ReadShortcutsFile(b, n, &r); free(b);
    Node *sc = st == 1 ? ShortcutsMap(r) : NULL; int games = 0, ours = 0;
    for (Node *e = sc ? sc->child : NULL; e; e = e->next) if (e->type == 0) { if (PortheonEntry(e, ourExeQ)) ours++; else games++; }
    FreeNodes(r);
    return st == 1 && games && !ours ? 1 : 2;
}
/* 7.1: a player hit by the 5.0-7.0 file: Steam read the one-byte-short list as broken, and the list it writes back can
   be empty, without the player's other non-Steam games. When the current list holds no entry but PORTHEON's own (Exe =
   this PORTHEON.exe) and the first backup (shortcuts.vdf.portheon.bak, the file as it was before PORTHEON's first
   write) reads well, the backup's entries that are not in the list are put back. Returns how many.
   7.1 review (correctness#8, safety#2): only before 7.1's first write to this list (no shortcuts.vdf.portheon.last.bak,
   which 5.0-7.0 never wrote and 7.1 writes at every write over an existing file): after that Steam reads the whole file,
   so a game missing from it was removed by the player and must not come back at the next click. */
static int RestoreFromBackup(Node *sc, const wchar_t *vdfPath, const char *ourExeQ)
{
    for (Node *e = sc->child; e; e = e->next) if (e->type == 0 && !SameExe(Str(e, "Exe"), ourExeQ)) return 0;   /* the player's own games are there */
    wchar_t last[MAX_PATH]; _snwprintf(last, MAX_PATH, L"%s.portheon.last.bak", vdfPath); last[MAX_PATH - 1] = 0;
    if (GetFileAttributesW(last) != INVALID_FILE_ATTRIBUTES) return 0;   /* 7.1 wrote this list before */
    wchar_t bak[MAX_PATH]; _snwprintf(bak, MAX_PATH, L"%s.portheon.bak", vdfPath); bak[MAX_PATH - 1] = 0;
    unsigned char *b = NULL; DWORD n = 0; if (!ReadAll(bak, &b, &n)) return 0;
    Node *broot = NULL; int st = ReadShortcutsFile(b, n, &broot); free(b);
    Node *bsc = st > 0 ? ShortcutsMap(broot) : NULL; int moved = 0;
    if (bsc) {
        BOOL other = FALSE; for (Node *e = bsc->child; e; e = e->next) if (e->type == 0 && !SameExe(Str(e, "Exe"), ourExeQ)) other = TRUE;
        for (Node **p = &bsc->child; other && *p;) {
            Node *e = *p;
            if (e->type != 0 || FindEntry(sc, Str(e, "AppName"), Str(e, "Exe"), NULL)) { p = &e->next; continue; }
            if (!SameExe(Str(e, "Exe"), ourExeQ)) moved++;   /* PORTHEON's own entries come back too, uncounted (one is replaced below) */
            *p = e->next; e->next = NULL; Append(sc, e);
        }
    }
    FreeNodes(broot); return moved;
}
static BOOL SaveShortcuts(const Node *root, const wchar_t *vdfPath, wchar_t *err, int errCap)
{
    Buf b = {0}; if (!EmitFile(&b, root)) { free(b.p); SetErr(err, errCap, L"out of memory"); return FALSE; }   /* 7.1: with the root's 0x08 */
    if (!SteamFormatOk(b.p, (DWORD)b.n)) { free(b.p); SetErr(err, errCap, L"the new shortcuts.vdf would not be valid for Steam: nothing was changed"); return FALSE; }
    g_stOutSize = (DWORD)b.n; g_stOutTailN = b.n < 4 ? (int)b.n : 4; memcpy(g_stOutTail, b.p + b.n - g_stOutTailN, g_stOutTailN);
    wchar_t bak[MAX_PATH], last[MAX_PATH], tmp[MAX_PATH];
    _snwprintf(bak, MAX_PATH, L"%s.portheon.bak", vdfPath); bak[MAX_PATH - 1] = 0;
    _snwprintf(last, MAX_PATH, L"%s.portheon.last.bak", vdfPath); last[MAX_PATH - 1] = 0;
    _snwprintf(tmp, MAX_PATH, L"%s.portheon.tmp", vdfPath); tmp[MAX_PATH - 1] = 0;
    /* 7.1: the first backup is never overwritten (5.0-7.0 overwrote it at every click, so after two clicks it held
       PORTHEON's own file and the player's list was gone); the file before this write goes to .portheon.last.bak.
       7.1 review: a first backup that 5.0-7.0 left one 0x08 short is repaired in place, and both copies of a one-short
       file are written with its 0x08, so either one, renamed to shortcuts.vdf, is a list Steam reads */
    CopyRepaired(bak, bak, FALSE);
    if (GetFileAttributesW(vdfPath) != INVALID_FILE_ATTRIBUTES) { CopyRepaired(vdfPath, bak, TRUE); CopyRepaired(vdfPath, last, FALSE); }
    { wchar_t cfg[MAX_PATH]; wcsncpy(cfg, vdfPath, MAX_PATH); cfg[MAX_PATH - 1] = 0; wchar_t *s = wcsrchr(cfg, L'\\'); if (s) { *s = 0; CreateDirectoryW(cfg, NULL); } }
    BOOL ok = WriteAll(tmp, b.p, (DWORD)b.n); free(b.p);
    if (!ok) { DeleteFileW(tmp); SetErr(err, errCap, L"could not write shortcuts.vdf"); return FALSE; }
    if (!MoveFileExW(tmp, vdfPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { DeleteFileW(tmp); SetErr(err, errCap, L"could not replace shortcuts.vdf"); return FALSE; }
    return TRUE;
}
/* 7.1: one line for steam.txt (portheon.c AddToSteam) and the reports: no path, the account id cut to its last 3 digits */
static void TraceMake(BOOL ok, const wchar_t *err)
{
    static const wchar_t *const sn[] = {L"not read", L"refused (not written)", L"none or empty (new)", L"valid", L"repaired (was one 0x08 short)"};
    static const wchar_t *const bk[] = {L"none or not checked", L"the list from before PORTHEON", L"written after a PORTHEON click"};   /* 7.1 review */
    wchar_t tail[16] = L"-"; if (g_stOutTailN) { tail[0] = 0; for (int i = 0; i < g_stOutTailN; i++) { wchar_t h[4]; _snwprintf(h, 4, L"%02x", g_stOutTail[i]); wcscat(tail, h); } }
    _snwprintf(g_stTrace, 400, L"%s; accounts %d, chosen by %s (id ...%03u); file found: %s, %lu bytes; written %lu bytes, ends %s; restored from backup %d; first backup: %s%s%s",
               ok ? L"ok" : L"failed", g_stAccounts, g_stHow, g_stId3 % 1000, sn[g_stState + 2 >= 0 && g_stState + 2 <= 4 ? g_stState + 2 : 0], g_stInSize,
               g_stOutSize, tail, g_stRestored, bk[g_stBakKind >= 0 && g_stBakKind <= 2 ? g_stBakKind : 0], err && err[0] && !ok ? L"; error: " : L"", err && !ok ? err : L"");
    g_stTrace[399] = 0;
}
static void TraceReset(void) { wcscpy(g_stHow, L"-"); g_stAccounts = -1; g_stId3 = 0; g_stState = -2; g_stInSize = g_stOutSize = 0; g_stOutTailN = 0; g_stRestored = 0; g_stBakKind = 0; g_stTrace[0] = 0; }
const wchar_t *SteamLastTrace(void) { return g_stTrace; }
int SteamLastState(void) { return g_stState; }
int SteamLastRestored(void) { return g_stRestored; }
int SteamLastBakKind(void) { return g_stBakKind; }

/* ---------------------------------------------------------------- grid pictures */
static const wchar_t *Ext(const wchar_t *p) { const wchar_t *d = p ? wcsrchr(p, L'.') : NULL; return d && !wcschr(d, L'\\') ? d : L""; }
static void GridFile(const wchar_t *userDir, unsigned int appid, const wchar_t *suffix, const wchar_t *ext, wchar_t *out)
{
    _snwprintf(out, MAX_PATH, L"%s\\config\\grid\\%u%s%s", userDir, appid, suffix, ext); out[MAX_PATH - 1] = 0;
}
static void GridDrop(const wchar_t *userDir, unsigned int appid, const wchar_t *suffix)   /* every extension of one kind */
{
    /* 7.1: .exe too: 5.0-7.0 copied the whole PORTHEON.exe (about 5 MB) as <appid>_icon.exe when the game had no icon
       picture, and never removed it */
    static const wchar_t *const exts[] = {L".png", L".jpg", L".jpeg", L".ico", L".webp", L".exe"};
    wchar_t p[MAX_PATH]; for (int i = 0; i < 6; i++) { GridFile(userDir, appid, suffix, exts[i], p); DeleteFileW(p); }
}
static BOOL GridPut(const wchar_t *userDir, unsigned int appid, const wchar_t *suffix, const wchar_t *src, wchar_t *dstOut)
{
    if (dstOut) dstOut[0] = 0;
    if (!src || !src[0]) return TRUE;
    if (GetFileAttributesW(src) == INVALID_FILE_ATTRIBUTES) return FALSE;
    wchar_t ext[16]; wcsncpy(ext, Ext(src), 15); ext[15] = 0; for (wchar_t *c = ext; *c; c++) *c = (wchar_t)towlower(*c);
    if (!ext[0]) return FALSE;
    GridDrop(userDir, appid, suffix);
    /* 7.1: a program's own icon is read from the program where it is (the entry's "icon" field names it), not copied */
    if (!wcscmp(ext, L".exe")) { if (dstOut) { wcsncpy(dstOut, src, MAX_PATH - 1); dstOut[MAX_PATH - 1] = 0; } return TRUE; }
    wchar_t dst[MAX_PATH]; GridFile(userDir, appid, suffix, ext, dst);
    if (!CopyFileW(src, dst, FALSE)) return FALSE;
    if (dstOut) wcscpy(dstOut, dst);
    return TRUE;
}

/* ---------------------------------------------------------------- the API */
BOOL SteamAddShortcut(const wchar_t *appName, const wchar_t *exe, const wchar_t *startDir, const wchar_t *launchOptions,
                      const wchar_t *gridPng, const wchar_t *heroPng, const wchar_t *logoPng, const wchar_t *widePng,
                      const wchar_t *iconPath, wchar_t *errOut, int errCap)
{
    SetErr(errOut, errCap, L""); TraceReset();
    if (!appName || !appName[0] || !exe || !exe[0]) { SetErr(errOut, errCap, L"name and exe are required"); TraceMake(FALSE, errOut); return FALSE; }
    if (SteamIsRunning()) { SetErr(errOut, errCap, L"Steam is running: close it first (it overwrites shortcuts.vdf when it exits)"); TraceMake(FALSE, errOut); return FALSE; }
    wchar_t userDir[MAX_PATH], vdf[MAX_PATH], sdir[MAX_PATH], gridDir[MAX_PATH];
    if (!SteamFindPath(sdir, MAX_PATH)) { SetErr(errOut, errCap, L"Steam is not installed (no SteamPath in the registry)"); TraceMake(FALSE, errOut); return FALSE; }
    if (!SteamFindUserDir(userDir, MAX_PATH)) { SetErr(errOut, errCap, L"no Steam account on this PC (userdata is empty: sign in to Steam once)"); TraceMake(FALSE, errOut); return FALSE; }

    if (!startDir || !startDir[0]) { wcsncpy(sdir, exe, MAX_PATH); sdir[MAX_PATH - 1] = 0; wchar_t *s = wcsrchr(sdir, L'\\'); if (s) *s = 0; }
    else { wcsncpy(sdir, startDir, MAX_PATH); sdir[MAX_PATH - 1] = 0; }
    TrimSlash(sdir);
    char *name = Utf8(appName), *exeQ = QuotedUtf8(exe), *dirQ = QuotedUtf8(sdir), *opts = Utf8(launchOptions);
    if (!name || !exeQ || !dirQ || !opts) { free(name); free(exeQ); free(dirQ); free(opts); SetErr(errOut, errCap, L"out of memory"); TraceMake(FALSE, errOut); return FALSE; }
    unsigned int appid = AppIdOf(exeQ, name);

    /* 7.1: the list is read (and refused, untouched, when it cannot be written back) before any picture is copied */
    Node *root = LoadShortcuts(userDir, vdf, errOut, errCap, TRUE); if (!root) { free(name); free(exeQ); free(dirQ); free(opts); TraceMake(FALSE, errOut); return FALSE; }
    _snwprintf(gridDir, MAX_PATH, L"%s\\config", userDir); CreateDirectoryW(gridDir, NULL);
    _snwprintf(gridDir, MAX_PATH, L"%s\\config\\grid", userDir); CreateDirectoryW(gridDir, NULL);
    Node *sc = ShortcutsMap(root), **link = NULL, *old;
    g_stRestored = RestoreFromBackup(sc, vdf, exeQ);   /* before FindEntry: it appends to the list */
    old = FindEntry(sc, name, exeQ, &link);
    int32_t lastPlay = old ? Int(old, "LastPlayTime", 0) : 0;
    Node *oldTags = old ? Find(old, "tags") : NULL;

    /* the pictures first (their names need the appid): an old entry with another appid is cleaned below */
    wchar_t iconDst[MAX_PATH] = L"", tmpDst[MAX_PATH]; const wchar_t *bad = NULL;
    if (old) { unsigned int oid = (unsigned int)Int(old, "appid", 0); if (oid && oid != appid) { GridDrop(userDir, oid, L"p"); GridDrop(userDir, oid, L"_hero"); GridDrop(userDir, oid, L"_logo"); GridDrop(userDir, oid, L""); GridDrop(userDir, oid, L"_icon"); } }
    if (!GridPut(userDir, appid, L"p", gridPng, tmpDst)) bad = L"grid (portrait) picture";
    else if (!GridPut(userDir, appid, L"_hero", heroPng, tmpDst)) bad = L"hero picture";
    else if (!GridPut(userDir, appid, L"_logo", logoPng, tmpDst)) bad = L"logo picture";
    else if (!GridPut(userDir, appid, L"", widePng, tmpDst)) bad = L"wide picture";
    else if (!GridPut(userDir, appid, L"_icon", iconPath, iconDst)) bad = L"icon";
    if (bad) { wchar_t m[200]; _snwprintf(m, 200, L"could not copy the %s into config\\grid", bad); SetErr(errOut, errCap, m); }

    Node *e = NewNode(0, "0");
    if (!e) { FreeNodes(root); free(name); free(exeQ); free(dirQ); free(opts); SetErr(errOut, errCap, L"out of memory"); return FALSE; }
    char *iconU = Utf8(iconDst[0] ? iconDst : (iconPath ? iconPath : L""));
    Append(e, AddInt("appid", (int32_t)appid));
    Append(e, AddStr("AppName", name));
    Append(e, AddStr("Exe", exeQ));
    Append(e, AddStr("StartDir", dirQ));
    Append(e, AddStr("icon", iconU ? iconU : ""));
    Append(e, AddStr("ShortcutPath", ""));
    Append(e, AddStr("LaunchOptions", opts));
    Append(e, AddInt("IsHidden", 0));
    Append(e, AddInt("AllowDesktopConfig", 1));
    Append(e, AddInt("AllowOverlay", 1));
    Append(e, AddInt("OpenVR", 0));
    Append(e, AddInt("Devkit", 0));
    Append(e, AddStr("DevkitGameID", ""));
    Append(e, AddInt("DevkitOverrideAppID", 0));
    Append(e, AddInt("LastPlayTime", lastPlay));
    Append(e, AddStr("FlatpakAppID", ""));
    { Node *tags = NewNode(0, "tags");
      if (tags && oldTags) { int i = 0; for (Node *t = oldTags->child; t; t = t->next) if (t->type == 1) { char k[16]; sprintf(k, "%d", i++); Append(tags, AddStr(k, t->str)); } }
      Append(e, tags); }
    free(iconU);
    if (old) { *link = old->next; old->next = NULL; FreeNodes(old); }
    Append(sc, e); Renumber(sc);
    BOOL ok = SaveShortcuts(root, vdf, errOut, errCap);
    g_stBakKind = ok ? BackupKind(vdf, exeQ) : 0;   /* 7.1 review: AddToSteam's backup sentence */
    FreeNodes(root); free(name); free(exeQ); free(dirQ); free(opts);
    if (!ok) g_stRestored = 0;   /* nothing was written */
    TraceMake(ok && !bad, errOut);
    if (ok && !bad) { wchar_t m[32]; _snwprintf(m, 32, L"%u", appid); SetErr(errOut, errCap, m); }
    return ok && !bad;
}

BOOL SteamRemoveShortcut(const wchar_t *appName, const wchar_t *exe)   /* no caller in PORTHEON 7.1 (kept for the API) */
{
    TraceReset();
    if (SteamIsRunning()) return FALSE;
    wchar_t userDir[MAX_PATH], vdf[MAX_PATH], err[200];
    if (!SteamFindUserDir(userDir, MAX_PATH)) return FALSE;
    char *name = Utf8(appName), *exeQ = QuotedUtf8(exe); if (!name || !exeQ) { free(name); free(exeQ); return FALSE; }
    Node *root = LoadShortcuts(userDir, vdf, err, 200, TRUE); if (!root) { free(name); free(exeQ); return FALSE; }
    Node *sc = ShortcutsMap(root), **link = NULL, *old = FindEntry(sc, name, exeQ, &link); BOOL ok = TRUE;
    if (old) {
        unsigned int oid = (unsigned int)Int(old, "appid", 0); if (!oid) oid = AppIdOf(exeQ, name);
        GridDrop(userDir, oid, L"p"); GridDrop(userDir, oid, L"_hero"); GridDrop(userDir, oid, L"_logo"); GridDrop(userDir, oid, L""); GridDrop(userDir, oid, L"_icon");
        *link = old->next; old->next = NULL; FreeNodes(old); Renumber(sc);
        ok = SaveShortcuts(root, vdf, err, 200);
    }
    FreeNodes(root); free(name); free(exeQ); return ok;
}

int SteamListShortcuts(SteamShortcutFn cb, void *user, wchar_t *errOut, int errCap)
{
    wchar_t userDir[MAX_PATH], vdf[MAX_PATH];
    if (!SteamFindPath(userDir, MAX_PATH)) { SetErr(errOut, errCap, L"Steam is not installed (no SteamPath in the registry)"); return -1; }
    if (!SteamFindUserDir(userDir, MAX_PATH)) { SetErr(errOut, errCap, L"no Steam account on this PC"); return -1; }
    Node *root = LoadShortcuts(userDir, vdf, errOut, errCap, FALSE); if (!root) return -1;
    Node *sc = ShortcutsMap(root); int n = 0;
    for (Node *e = sc->child; e; e = e->next) {
        if (e->type != 0) continue;
        unsigned int id = (unsigned int)Int(e, "appid", 0); if (!id) id = AppIdOf(Str(e, "Exe"), Str(e, "AppName"));
        if (cb) cb(n, id, Str(e, "AppName"), Str(e, "Exe"), Str(e, "StartDir"), Str(e, "LaunchOptions"), user);
        n++;
    }
    FreeNodes(root); return n;
}
