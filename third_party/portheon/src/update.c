/* update.c - part of portheon.c (included by it): the updater (3.5).
 * At start (and every 6 hours while open) PORTHEON asks GitHub for the newest release of XAndromedaXR/PORTHEON. When the
 * API does not answer (its hourly limit, a proxy), the release page's redirect gives the tag and the package's name.
 * A newer release opens a window with its notes. "Update now" downloads the Windows package inside PORTHEON (progress,
 * speed, time left; three retries that resume where they stopped), checks its SHA-256 against the release's
 * SHA256SUMS.txt, and only then starts PORTHEON.exe again as its own installer (--apply-update, a small window that
 * says what it does): it waits for PORTHEON and the games to close, checks the SHA-256 once more, unpacks the package
 * (Windows' tar.exe), keeps the player's community patch files, copies it over this folder, puts the old PORTHEON.exe
 * back if the copy fails, and opens PORTHEON again. A folder that needs administrator rights (Program Files) gets the
 * installer elevated once. Until 3.5 a hidden PowerShell script with the execution policy bypassed did the install:
 * antivirus programs flag that pattern, and some block PowerShell started by other programs. auto_update=1 in
 * portheon.ini installs without asking; update_check=0 turns the check off. Under Wine (Linux, Steam Deck) it only
 * says that there is a new version.
 * 6.1: a player on 6.0 read "Could not look for updates now (no Internet?)" while online. The real cause: GitHub's 6.0
 * release has no PORTHEON-6.0-Windows-x64.zip asset (only Linux tar.gz, Setup.exe and SHA256SUMS.txt; the zip URL
 * answers 404), and the check threw the tag away when the zip was missing, which then read as a failed check. Now the
 * version is compared anyway (6.0 against 6.0: "you have the newest"; a newer release without the zip: its page), a
 * failed check says why (no connection, the secure connection failed, GitHub's hourly limit, an answer it could not
 * read), the API's own SHA-256 of the asset ("digest") is used next to SHA256SUMS.txt, a package already downloaded
 * and correct is not downloaded again, and with "Look for PORTHEON updates by itself" on (update_check) the update
 * downloads in the background and installs by itself, never while a game, a port install or the UFC 2 room runs
 * (auto_update=0 keeps the old window; a version whose automatic install failed once goes back to the window). */
#define UPD_API_HOST L"api.github.com"
#define UPD_API_PATH L"/repos/XAndromedaXR/PORTHEON/releases/latest"
#define UPD_PAGE L"https://github.com/XAndromedaXR/PORTHEON/releases/latest"
#ifndef UPD_API_URL   /* 6.1: the addresses asked (a test build points them at a local server) */
#define UPD_API_URL L"https://" UPD_API_HOST UPD_API_PATH
#endif
#ifndef UPD_PAGE_URL
#define UPD_PAGE_URL UPD_PAGE
#endif
#ifndef UPD_DL_BASE
#define UPD_DL_BASE L"https://github.com/XAndromedaXR/PORTHEON/releases/download/"
#endif
static wchar_t g_updTag[32], g_updZip[600], g_updSums[600], g_updName[128], g_updErr[300], g_dlPath[MAX_PATH], g_updSha[65];
static volatile LONG g_dlHard;   /* 6.1 review: the package or the release itself is bad (hash), not a passing network failure */
static wchar_t g_updWant[65], g_updWhy[200];   /* 6.1: the asset's SHA-256 from the API ("digest"); why the last check failed */
static wchar_t *g_updNotes;
static volatile LONG g_updBusy, g_updState, g_dlCancel;   /* state: 0 nothing, 1 available, 2 downloading, 3 checking, 4 installing, 5 failed */
static volatile LONGLONG g_dlGot, g_dlTotal; static ULONGLONG g_dlT0;
static BOOL g_updAuto, g_updReady;   /* 6.1: this download started by itself; the package is downloaded and checked, waiting for the game to close */
static BOOL g_updCancelled;          /* 6.1 review: the player pressed Cancel on this download (UpdCancel) */
/* 6.1 review: a release the check thread found, handed over to the UI thread (UpdTakeFound) */
typedef struct { wchar_t tag[32], zip[600], sums[600], name[128], want[65]; wchar_t *notes; } UpdFound;
static UpdFound *volatile g_updFound;
#define UPD_AUTO_TIMER 0x6161   /* 6.1: the automatic install's own timer (UpdAutoWait) */
/* 6.1: what the last HttpGet of this thread met: the WinHTTP error when no answer came, GitHub's hourly API budget */
static _Thread_local DWORD t_httpErr; static _Thread_local int t_rlLeft; static _Thread_local LONGLONG t_rlReset;
static int VerCmp(const wchar_t *a, const wchar_t *b)   /* "3.2.1" against "v3.2.2": <0, 0, >0 */
{
    while (*a && !iswdigit(*a)) a++; while (*b && !iswdigit(*b)) b++;
    for (int i = 0; i < 4; i++) {
        int x = *a ? _wtoi(a) : 0, y = *b ? _wtoi(b) : 0;
        if (x != y) return x < y ? -1 : 1;
        while (*a && *a != L'.') a++; if (*a) a++;
        while (*b && *b != L'.') b++; if (*b) b++;
    }
    return 0;
}
static BOOL UnderWine(void) { return GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version") != NULL; }
/* 4.0.7: only PORTHEON's own releases count: a tag that is a version ("4.0.7", "v4.0.7") and an asset named
   "PORTHEON-<version>-Windows-x64.zip". A release of the separate PORTHEON UFC 6 app ("ufc6-v1.0.0") would otherwise read
   as version 6 here and replace PORTHEON with it. */
static BOOL PortheonTag(const char *t) { if (*t == 'v' || *t == 'V') t++; return *t >= '0' && *t <= '9'; }
static BOOL PortheonAsset(const char *name) { return !_strnicmp(name, "PORTHEON-", 9) && name[9] >= '0' && name[9] <= '9'; }
/* the first "key":"value" after from (raw, no unescaping) */
static BOOL JsonStr(const char *from, const char *key, char *out, int n)
{
    char k[64]; snprintf(k, 64, "\"%s\"", key); const char *p = strstr(from, k); if (!p) return FALSE;
    p += strlen(k); while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p++ != ':') return FALSE; while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p++ != '"') return FALSE;
    int i = 0; while (*p && *p != '"' && i < n - 1) out[i++] = *p++; out[i] = 0; return i > 0;
}
/* a JSON string value, unescaped, as a new wide string (the release notes) */
static wchar_t *JsonText(const char *from, const char *key)
{
    char k[64]; snprintf(k, 64, "\"%s\"", key); const char *p = strstr(from, k); if (!p) return NULL;
    p += strlen(k); while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p++ != '"') return NULL;
    size_t cap = strlen(p) + 1; char *u = (char *)malloc(cap); if (!u) return NULL; size_t o = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
            case 'n': u[o++] = '\n'; break; case 'r': break; case 't': u[o++] = ' '; break;
            case 'u': { unsigned c = (unsigned)strtoul((char[5]){p[1], p[2], p[3], p[4], 0}, NULL, 16); p += 4;
                        if (c < 0x80) u[o++] = (char)c; else if (c < 0x800) { u[o++] = (char)(0xC0 | (c >> 6)); u[o++] = (char)(0x80 | (c & 63)); }
                        else { u[o++] = (char)(0xE0 | (c >> 12)); u[o++] = (char)(0x80 | ((c >> 6) & 63)); u[o++] = (char)(0x80 | (c & 63)); } break; }
            default: u[o++] = *p;
            }
            p++;
        } else u[o++] = *p++;
        if (o + 4 >= cap) break;
    }
    u[o] = 0;
    /* light Markdown clean-up: headings and bold marks out, bullets as dots. 3.5.2: room for twice the text, since a
       "- " bullet becomes the 3 bytes of "· " (notes with more bullets than bold marks wrote past the old o + 1) */
    char *c = (char *)malloc(o * 2 + 8); size_t q = 0; BOOL lineStart = TRUE;
    for (size_t i = 0; c && i < o; i++) {
        if (lineStart) { int hashes = 0; while (u[i] == '#') { hashes++; i++; } if (hashes) while (u[i] == ' ') i++;
                         if ((u[i] == '-' || u[i] == '*') && u[i + 1] == ' ') { c[q++] = (char)0xC2; c[q++] = (char)0xB7; i++; } }
        if (u[i] == '*' && u[i + 1] == '*') { i++; lineStart = FALSE; continue; }
        if (u[i] == '`') { lineStart = FALSE; continue; }
        c[q++] = u[i]; lineStart = u[i] == '\n';
    }
    wchar_t *w = NULL;
    if (c) { c[q] = 0; int n = MultiByteToWideChar(CP_UTF8, 0, c, -1, NULL, 0); w = (wchar_t *)malloc(sizeof(wchar_t) * (n + 1)); if (w) MultiByteToWideChar(CP_UTF8, 0, c, -1, w, n); }
    free(c); free(u); return w;
}
/* 6.1: TLS 1.2 (and 1.3 where WinHTTP has it) besides the system's defaults: GitHub refuses TLS 1.0/1.1, and older
   Windows builds leave 1.2 out of WinHTTP's default list */
static void HttpTls(HINTERNET s)
{
    DWORD p = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | 0x00002000 /* TLS 1.3 */;
    if (!WinHttpSetOption(s, WINHTTP_OPTION_SECURE_PROTOCOLS, &p, sizeof(p))) { p &= ~0x00002000u; WinHttpSetOption(s, WINHTTP_OPTION_SECURE_PROTOCOLS, &p, sizeof(p)); }
}
/* 6.1: a session; the proxy the system names (automatic, then the old default), or none (the second try) */
static HINTERNET HttpSession(BOOL noProxy, DWORD t1, DWORD t2, DWORD t3, DWORD t4)
{
    HINTERNET s = noProxy ? WinHttpOpen(L"PORTHEON/" APP_VER, WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0)
                          : WinHttpOpen(L"PORTHEON/" APP_VER, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, 0);
    if (!s && !noProxy) s = WinHttpOpen(L"PORTHEON/" APP_VER, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (s) { WinHttpSetTimeouts(s, t1, t2, t3, t4); HttpTls(s); }
    return s;
}
/* 6.1: a WinHTTP error that is the network itself (no DNS, no route, timeout, a proxy that does not answer) */
static BOOL HttpNetErr(DWORD e)
{
    return e == ERROR_WINHTTP_NAME_NOT_RESOLVED || e == ERROR_WINHTTP_CANNOT_CONNECT || e == ERROR_WINHTTP_TIMEOUT ||
           e == ERROR_WINHTTP_CONNECTION_ERROR || e == 12180 /* ERROR_WINHTTP_AUTODETECTION_FAILED */ ? TRUE : FALSE;   /* 12007 12029 12002 12030 */
}
static BOOL HttpTlsErr(DWORD e) { return e == ERROR_WINHTTP_SECURE_FAILURE || e == ERROR_WINHTTP_SECURE_CHANNEL_ERROR || e == ERROR_WINHTTP_SECURE_INVALID_CA || e == ERROR_WINHTTP_SECURE_CERT_DATE_INVALID; }
static char *HttpGetOnce(const wchar_t *url, BOOL follow, DWORD *status, wchar_t *location, int locN, DWORD *len, BOOL noProxy)
{
    URL_COMPONENTS uc = {sizeof(uc)}; wchar_t host[256], path[2048];
    uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    *status = 0; if (len) *len = 0; if (location) location[0] = 0;
    t_httpErr = 0; t_rlLeft = -1; t_rlReset = 0;
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) { t_httpErr = GetLastError(); return NULL; }
    HINTERNET s = HttpSession(noProxy, 10000, 10000, 15000, 30000), c = NULL, r = NULL;
    char *buf = NULL; DWORD got = 0;
    if (s) c = WinHttpConnect(s, host, uc.nPort, 0);
    if (c) r = WinHttpOpenRequest(c, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (r && !follow) { DWORD opt = WINHTTP_DISABLE_REDIRECTS; WinHttpSetOption(r, WINHTTP_OPTION_DISABLE_FEATURE, &opt, sizeof(opt)); }
    if (r && WinHttpSendRequest(r, L"Accept: application/vnd.github+json\r\n", (DWORD)-1L, NULL, 0, 0, 0) && WinHttpReceiveResponse(r, NULL)) {
        DWORD sz = sizeof(DWORD); WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, status, &sz, NULL);
        if (location) { DWORD ln = (DWORD)(locN * sizeof(wchar_t)); if (!WinHttpQueryHeaders(r, WINHTTP_QUERY_LOCATION, NULL, location, &ln, NULL)) location[0] = 0; }
        {   /* 6.1: GitHub's hourly budget (60 API calls per address without an account) */
            wchar_t h[32]; DWORD hn = sizeof(h);
            if (WinHttpQueryHeaders(r, WINHTTP_QUERY_CUSTOM, L"X-RateLimit-Remaining", h, &hn, NULL)) t_rlLeft = _wtoi(h);
            hn = sizeof(h); if (WinHttpQueryHeaders(r, WINHTTP_QUERY_CUSTOM, L"X-RateLimit-Reset", h, &hn, NULL)) t_rlReset = _wtoi64(h);
        }
        buf = (char *)malloc(2u << 20);
        DWORD n = 0;
        while (buf && got < (2u << 20) - 8192 && WinHttpReadData(r, buf + got, 8192, &n) && n) got += n;
        if (buf) buf[got] = 0;
    } else t_httpErr = GetLastError();
    if (r) WinHttpCloseHandle(r); if (c) WinHttpCloseHandle(c); if (s) WinHttpCloseHandle(s);
    if (len) *len = got;
    return buf;
}
/* one HTTPS request; body (malloc'ed, NUL-terminated) and status; follow=FALSE keeps a redirect's Location.
   6.1: no answer through the system's proxy (a dead WPAD/PAC script, an old proxy setting) is tried once without it */
static char *HttpGet(const wchar_t *url, BOOL follow, DWORD *status, wchar_t *location, int locN, DWORD *len)
{
    char *b = HttpGetOnce(url, follow, status, location, locN, len, FALSE);
    if (!b && !*status && t_httpErr != ERROR_WINHTTP_NAME_NOT_RESOLVED) {
        DWORD e = t_httpErr; b = HttpGetOnce(url, follow, status, location, locN, len, TRUE);
        if (!b && !*status && !HttpNetErr(t_httpErr) && !HttpTlsErr(t_httpErr)) t_httpErr = e;   /* the first, more telling error */
    }
    return b;
}
/* 4.3.8, the test-versions channel (Settings > Updates, beta_updates=1, for the players who help test): the newest of the
   last releases, test versions (GitHub "pre-release") included, instead of /releases/latest. A test version is tagged
   with a fourth number (4.3.8.1, 4.3.8.2...), so the final 4.3.9 still comes after it. The release objects follow one
   another in the list: each one's fields lie between its "tag_name" and the next one's.
   6.1: "draft"/"prerelease" read with GitHub's spacing ("draft": false); pre=FALSE leaves the test versions out (the
   stable channel when the newest release is not PORTHEON's own). */
static BOOL JsonTrue(const char *from, const char *key)   /* 6.1: "key": true */
{
    char k[64]; snprintf(k, 64, "\"%s\"", key); const char *p = strstr(from, k); if (!p) return FALSE;
    p += strlen(k); while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return !strncmp(p, "true", 4);
}
static char *BetaReleaseEx(char *list, BOOL pre)
{
    char *best = NULL, bestTag[32] = "";
    for (char *q = strstr(list, "\"tag_name\""); q; ) {
        char *next = strstr(q + 10, "\"tag_name\""); char tag[32] = "";
        if (next) *next = 0;
        if (JsonStr(q, "tag_name", tag, 32) && PortheonTag(tag) && strstr(q, "-Windows-x64.zip\"") && !JsonTrue(q, "draft") && (pre || !JsonTrue(q, "prerelease"))) {
            wchar_t a[32], b[32]; MultiByteToWideChar(CP_UTF8, 0, tag, -1, a, 32); MultiByteToWideChar(CP_UTF8, 0, bestTag, -1, b, 32);
            if (!best || VerCmp(a, b) > 0) { best = q; strcpy(bestTag, tag); }
        }
        if (next) *next = '"';
        q = next;
    }
    if (best) { char *end = strstr(best + 10, "\"tag_name\""); if (end) *end = 0; }
    return best;
}
static char *BetaRelease(char *list) { return BetaReleaseEx(list, TRUE); }
/* 6.1: one release's fields: its tag (empty when it is not PORTHEON's own), the Windows package's address, name and
   SHA-256 (the asset's "digest", which comes before its "browser_download_url"), SHA256SUMS.txt's address */
static BOOL UpdParseRelease(const char *rel, char tag[32], wchar_t *zip, wchar_t *sums, wchar_t *name, wchar_t *want)
{
    static const char BDU[] = "\"browser_download_url\"";
    char url[600]; zip[0] = sums[0] = name[0] = want[0] = 0;
    if (!JsonStr(rel, "tag_name", tag, 32)) return FALSE;
    if (!PortheonTag(tag)) { tag[0] = 0; return TRUE; }   /* 4.0.7: not a PORTHEON release */
    const char *prev = strstr(rel, "\"assets\""); if (!prev) prev = rel;
    for (const char *q = strstr(prev, BDU); q; prev = q + 1, q = strstr(q + 1, BDU)) {
        if (!JsonStr(q, "browser_download_url", url, 600)) continue;
        size_t l = strlen(url); const char *base = strrchr(url, '/'); base = base ? base + 1 : url;
        if (l > 16 && !_stricmp(url + l - 16, "-Windows-x64.zip") && PortheonAsset(base)) {
            MultiByteToWideChar(CP_UTF8, 0, url, -1, zip, 600); MultiByteToWideChar(CP_UTF8, 0, base, -1, name, 128);
            char d[96] = ""; for (const char *s = strstr(prev, "\"digest\""); s && s < q; s = strstr(s + 1, "\"digest\"")) { d[0] = 0; JsonStr(s, "digest", d, sizeof(d)); }
            want[0] = 0;
            if (!_strnicmp(d, "sha256:", 7) && strlen(d + 7) == 64) { for (int i = 0; i < 64; i++) want[i] = (wchar_t)tolower((unsigned char)d[7 + i]); want[64] = 0; }
        }
        if (l > 14 && !_stricmp(url + l - 14, "SHA256SUMS.txt")) MultiByteToWideChar(CP_UTF8, 0, url, -1, sums, 600);
    }
    return TRUE;
}
/* 6.1: why a check found nothing, in the player's words; the code OnUpdateFound gets when the player asked:
   4 no connection, 5 GitHub's hourly limit, 6 the secure connection failed, 7 an answer it could not read */
static int UpdWhy(DWORD apiSt, DWORD apiErr, int apiLeft, LONGLONG apiReset, DWORD pgSt, DWORD pgErr)
{
    DWORD e = apiErr ? apiErr : pgErr;
    if ((apiSt == 403 && apiLeft == 0) || apiSt == 429) {
        int min = apiReset > 0 ? (int)((apiReset - UnixNow() + 59) / 60) : 60; if (min < 1) min = 1; if (min > 60) min = 60;
        _snwprintf(g_updWhy, 200, L2(L"o GitHub limitou as consultas desta internet por agora (60 por hora); tente de novo em %d min", L"GitHub limited the checks from this Internet connection for now (60 an hour); try again in %d min"), min);
        return 5;
    }
    if (!apiSt && !pgSt && (HttpTlsErr(apiErr) || HttpTlsErr(pgErr))) {
        _snwprintf(g_updWhy, 200, L2(L"a conexão segura com o GitHub falhou (erro %lu): confira a data e a hora do PC e o antivírus que examina HTTPS", L"the secure connection to GitHub failed (error %lu): check the PC's date and time and any antivirus that scans HTTPS"), HttpTlsErr(apiErr) ? apiErr : pgErr);
        return 6;
    }
    if (!apiSt && !pgSt) {
        _snwprintf(g_updWhy, 200, L2(L"sem conexão com o GitHub (erro %lu): confira a internet, o proxy ou o firewall", L"no connection to GitHub (error %lu): check the Internet, the proxy or the firewall"), e);
        return 4;
    }
    _snwprintf(g_updWhy, 200, L2(L"o GitHub respondeu de um jeito que não entendi (API %lu, página %lu)", L"GitHub answered in a way PORTHEON could not read (API %lu, page %lu)"), apiSt, pgSt);
    return 7;
}
static DWORD WINAPI UpdCheckThread(void *p)
{
    BOOL manual = p != NULL; if (!manual) Sleep(2500);
    DWORD status = 0, len = 0; wchar_t loc[1024];
    BOOL beta = PIni(L"beta_updates", 0);
    char *buf = HttpGet(beta ? L"https://" UPD_API_HOST L"/repos/XAndromedaXR/PORTHEON/releases?per_page=15" : UPD_API_URL, TRUE, &status, NULL, 0, &len);
    DWORD apiSt = status, apiErr = t_httpErr; int apiLeft = t_rlLeft; LONGLONG apiReset = t_rlReset;
    char *rel = buf && status == 200 ? (beta ? BetaRelease(buf) : buf) : NULL;
    char tag[32] = "";
    wchar_t zip[600] = L"", sums[600] = L"", name[128] = L"", want[65] = L"", *notes = NULL;
    BOOL got = rel && UpdParseRelease(rel, tag, zip, sums, name, want);
    if (got && !tag[0] && !beta) {   /* 6.1: the newest release is not PORTHEON's own (the UFC 6 app's): the newest stable one that is */
        free(buf); buf = HttpGet(L"https://" UPD_API_HOST L"/repos/XAndromedaXR/PORTHEON/releases?per_page=15", TRUE, &status, NULL, 0, &len);
        rel = buf && status == 200 ? BetaReleaseEx(buf, FALSE) : NULL;
        if (rel) UpdParseRelease(rel, tag, zip, sums, name, want);
    }
    if (got && rel) notes = JsonText(rel, "body");
    if (!got) {   /* the API did not answer (its hourly limit, a proxy) or could not be read: the release page redirects to .../releases/tag/<tag> */
        free(buf); buf = HttpGet(UPD_PAGE_URL, FALSE, &status, loc, 1024, &len);
        DWORD pgSt = status, pgErr = t_httpErr;
        const wchar_t *t = status >= 300 && status < 400 ? wcsstr(loc, L"/tag/") : NULL;
        if (t) {
            wchar_t wt[32]; wcsncpy(wt, t + 5, 31); wt[31] = 0; wt[wcscspn(wt, L"/?#")] = 0;
            WideCharToMultiByte(CP_UTF8, 0, wt, -1, tag, 32, NULL, NULL);
            got = TRUE;
            if (!PortheonTag(tag)) tag[0] = 0;   /* 4.0.7: not a PORTHEON release */
            else {
                const wchar_t *ver = wt; while (*ver && !iswdigit(*ver)) ver++;
                _snwprintf(name, 128, L"PORTHEON-%s-Windows-x64.zip", ver);
                _snwprintf(zip, 600, L"%s%s/%s", UPD_DL_BASE, wt, name);
                _snwprintf(sums, 600, L"%s%s/SHA256SUMS.txt", UPD_DL_BASE, wt);
                if (VerCmp(APP_VER, wt) < 0) {   /* 6.1: the package really is there (6.0's release went out without it: 404) */
                    DWORD zs = 0, zl = 0; char *zb = HttpGet(zip, FALSE, &zs, NULL, 0, &zl); free(zb);
                    if (zs == 404) zip[0] = 0;
                }
            }
        }
        if (!got) { WPARAM why = UpdWhy(apiSt, apiErr, apiLeft, apiReset, pgSt, pgErr); free(buf); InterlockedExchange(&g_updBusy, 0); if (manual) PostMessageW(g_wnd, WM_APP_UPDATE, why, 0); return 0; }
    }
    free(buf);
    WPARAM res = manual ? 3 : 0;   /* 6.1: a release was read: newer, or this one is the newest (no PORTHEON release: nothing newer) */
    if (tag[0]) {
        wchar_t wt[32]; MultiByteToWideChar(CP_UTF8, 0, tag, -1, wt, 32);
        if (VerCmp(APP_VER, wt) < 0) {
            /* 6.1 review: the release is handed to the UI thread (UpdTakeFound), which alone writes g_updTag/g_updZip/...
               and never while a download, a check of the package or an install reads them (g_updState 2..4) */
            UpdFound *f = (UpdFound *)calloc(1, sizeof(UpdFound));
            if (f) {
                wcscpy(f->tag, wt); wcscpy(f->zip, zip); wcscpy(f->sums, sums); wcscpy(f->name, name); wcscpy(f->want, want);
                f->notes = notes; notes = NULL;
                UpdFound *old = (UpdFound *)InterlockedExchangePointer((PVOID volatile *)&g_updFound, f);
                if (old) { free(old->notes); free(old); }
                res = manual ? 2 : 1;
            }
        }
    }
    free(notes);
    InterlockedExchange(&g_updBusy, 0);
    if (res) PostMessageW(g_wnd, WM_APP_UPDATE, res, 0);
    return 0;
}
/* 6.1 review: on the UI thread (OnUpdateFound): the release the check thread found goes into the globals, unless a
   download, a package check or an install is under way (their thread reads them): then the current one stays */
static BOOL UpdTakeFound(void)
{
    UpdFound *f = (UpdFound *)InterlockedExchangePointer((PVOID volatile *)&g_updFound, NULL);
    if (!f) return g_updTag[0] != 0;
    if (g_updState >= 2 && g_updState <= 4) { free(f->notes); free(f); return FALSE; }
    if (wcscmp(g_updTag, f->tag)) g_updReady = FALSE;   /* 6.1: a package ready for an older tag is not this one */
    wcscpy(g_updTag, f->tag); wcscpy(g_updZip, f->zip); wcscpy(g_updSums, f->sums); wcscpy(g_updName, f->name); wcscpy(g_updWant, f->want);
    wchar_t *old = g_updNotes; g_updNotes = f->notes; free(old); free(f);
    return TRUE;
}
static void StartUpdateCheck(BOOL manual)
{
#ifdef UPD_FAKE   /* test builds only: an update is "found" without asking GitHub, to look at the update window */
    wcscpy(g_updTag, L"v3.5.1"); wcscpy(g_updName, L"PORTHEON-3.5.1-Windows-x64.zip");
    wcscpy(g_updZip, L"http://127.0.0.1:8765/PORTHEON-3.5.1-Windows-x64.zip"); wcscpy(g_updSums, L"http://127.0.0.1:8765/SHA256SUMS.txt");   /* a local test server */
    g_updNotes = JsonText("{\"body\":\"## Novidades\\n- **UFC 4**: teste da janela de atualização.\\n- Segunda linha das notas, longa o bastante para quebrar em mais de uma linha dentro da caixa de notas da atualização.\"}", "body");
    PostMessageW(g_wnd, WM_APP_UPDATE, 2, 0); (void)manual; return;
#endif
    if (g_updState >= 2 && g_updState <= 4) return;
    if (!manual && !PIni(L"update_check", 1)) return;   /* 6.1: the 6-hour timer obeys "Look for PORTHEON updates by itself" too */
    if (InterlockedCompareExchange(&g_updBusy, 1, 0)) return;
    HANDLE th = CreateThread(NULL, 0, UpdCheckThread, manual ? (void *)1 : NULL, 0, NULL); if (th) CloseHandle(th); else InterlockedExchange(&g_updBusy, 0);
}

/* ---------------------------------------------------------------- download and check */
static BOOL Sha256File(const wchar_t *path, char hex[65])
{
    BCRYPT_ALG_HANDLE alg = NULL; BCRYPT_HASH_HANDLE h = NULL; BYTE dig[32]; BOOL ok = FALSE;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) return FALSE;
    if (BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0) == 0) {
        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            BYTE *buf = (BYTE *)malloc(1 << 20); DWORD n = 0;
            while (buf && ReadFile(f, buf, 1 << 20, &n, NULL) && n) BCryptHashData(h, buf, n, 0);
            free(buf); CloseHandle(f);
            ok = BCryptFinishHash(h, dig, 32, 0) == 0;
        }
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    if (ok) for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", dig[i]);
    return ok;
}
/* 6.1: an automatic download steps aside while the player plays (a game, a game of their own library, the UFC 2 room):
   it stops, keeps the part it has and goes on from there when the game closes, so a fight (online above all) never
   shares the connection or the disk with it. Read from the download thread: a handle/flag is only compared with 0. */
static volatile LONG g_dlYield, g_dlPaused;
static BOOL UpdPlaying(void) { return *(HANDLE volatile *)&g_proc || *(volatile int *)&g_crun || *(HANDLE volatile *)&g_u2srv; }
/* the package into path, resuming a partial file; FALSE with g_updErr set */
static BOOL DownloadTo(const wchar_t *url, const wchar_t *path)
{
    URL_COMPONENTS uc = {sizeof(uc)}; wchar_t host[256], upath[2048];
    uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = upath; uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) { _snwprintf(g_updErr, 300, L"%s", L2(L"endereço do pacote inválido", L"invalid package address")); return FALSE; }
    for (int attempt = 0; attempt < 4; attempt++) {
        if (g_dlCancel || (g_dlYield && g_dlPaused)) break;
        if (attempt) Sleep(1500 * attempt);
        LONGLONG have = 0; WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(path, GetFileExInfoStandard, &fa)) have = ((LONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
        /* 6.1: TLS 1.2/1.3 as HttpGet; the second half of the tries goes without the system's proxy */
        HINTERNET s = HttpSession(attempt >= 2, 15000, 15000, 30000, 60000), c = NULL, r = NULL;
        if (s) c = WinHttpConnect(s, host, uc.nPort, 0);
        if (c) r = WinHttpOpenRequest(c, L"GET", upath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
        wchar_t range[64] = L""; if (have > 0) _snwprintf(range, 64, L"Range: bytes=%lld-\r\n", have);
        BOOL ok = FALSE, done = FALSE;
        if (r && WinHttpSendRequest(r, range[0] ? range : WINHTTP_NO_ADDITIONAL_HEADERS, range[0] ? (DWORD)-1L : 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(r, NULL)) {
            DWORD st = 0, sz = sizeof(st); WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &st, &sz, NULL);
            wchar_t cl[32] = L""; DWORD cln = sizeof(cl); LONGLONG clen = -1;
            if (WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH, NULL, cl, &cln, NULL)) clen = _wtoi64(cl);
            if (st == 416 && have > 0) { DeleteFileW(path); }                       /* the partial file is bigger than the package */
            else if (st == 200 || st == 206) {
                if (st == 200) have = 0;
                g_dlTotal = clen >= 0 ? have + clen : 0; g_dlGot = have;
                HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, st == 206 ? OPEN_ALWAYS : CREATE_ALWAYS, 0, NULL);
                if (f != INVALID_HANDLE_VALUE) {
                    if (st == 206) SetFilePointer(f, 0, NULL, FILE_END);
                    BYTE *b = (BYTE *)malloc(1 << 16); DWORD n = 0, wr; ok = b != NULL;   /* 6.1 review: this call's own buffer */
                    while (b && !g_dlCancel) {
                        if (g_dlYield && UpdPlaying()) { InterlockedExchange(&g_dlPaused, 1); ok = FALSE; break; }   /* 6.1 */
                        if (!WinHttpReadData(r, b, 1 << 16, &n)) { ok = FALSE; break; }
                        if (!n) { done = TRUE; break; }
                        if (!WriteFile(f, b, n, &wr, NULL) || wr != n) { ok = FALSE; _snwprintf(g_updErr, 300, L"%s", L2(L"o disco está cheio ou bloqueado", L"the disk is full or locked")); break; }
                        g_dlGot += n;
                    }
                    free(b); CloseHandle(f);
                }
            }
            else _snwprintf(g_updErr, 300, L2(L"o servidor respondeu %lu", L"the server answered %lu"), st);
        }
        else _snwprintf(g_updErr, 300, L2(L"sem conexão (erro %lu)", L"no connection (error %lu)"), GetLastError());
        if (r) WinHttpCloseHandle(r); if (c) WinHttpCloseHandle(c); if (s) WinHttpCloseHandle(s);
        if (ok && done && (!g_dlTotal || g_dlGot >= g_dlTotal)) return TRUE;
    }
    if (g_dlYield && g_dlPaused) _snwprintf(g_updErr, 300, L"%s", L2(L"pausado enquanto você joga", L"paused while you play"));
    else if (g_dlCancel) _snwprintf(g_updErr, 300, L"%s", L2(L"cancelado", L"cancelled"));
    else if (!g_updErr[0]) _snwprintf(g_updErr, 300, L"%s", L2(L"o download parou no meio", L"the download stopped halfway"));
    return FALSE;
}
/* 6.1: the SHA-256 the package must have: GitHub's own (the asset's "digest", g_updWant) and SHA256SUMS.txt's line;
   both present and different is refused; FALSE with g_updErr set */
static BOOL UpdExpected(char want[65])
{
    char name[128]; WideCharToMultiByte(CP_UTF8, 0, g_updName, -1, name, 128, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, g_updWant, -1, want, 65, NULL, NULL); want[64] = 0; if (strlen(want) != 64) want[0] = 0;
    char sw[65] = ""; BOOL read = FALSE;
    if (g_updSums[0]) {
        DWORD st = 0, len = 0; char *sums = HttpGet(g_updSums, TRUE, &st, NULL, 0, &len);
        if (sums && st == 200) { read = TRUE; for (char *line = sums; line && *line; ) {
            char *e = strchr(line, '\n'); if (e) *e = 0;
            if (strstr(line, name)) { int k = 0; while (k < 64 && isxdigit((unsigned char)line[k])) { sw[k] = (char)tolower((unsigned char)line[k]); k++; } sw[k] = 0; break; }
            line = e ? e + 1 : NULL;
        } }
        free(sums);
        if (strlen(sw) != 64) sw[0] = 0;
    }
    if (want[0] && sw[0] && strcmp(want, sw)) { InterlockedExchange(&g_dlHard, 1); _snwprintf(g_updErr, 300, L"%s", L2(L"o SHA256SUMS.txt da release e o SHA-256 que o GitHub informa não batem (release com problema: baixe pela página)", L"the release's SHA256SUMS.txt and the SHA-256 GitHub gives differ (a broken release: get it from the page)")); return FALSE; }
    if (!want[0]) strcpy(want, sw);
    if (!want[0]) { _snwprintf(g_updErr, 300, L"%s", read || !g_updSums[0] ? L2(L"a release não tem o SHA256SUMS.txt com este pacote", L"the release has no SHA256SUMS.txt for this package")
                                                                      : L2(L"não consegui ler o SHA256SUMS.txt da release (sem conexão?)", L"could not read the release's SHA256SUMS.txt (no connection?)")); return FALSE; }
    return TRUE;
}
static DWORD WINAPI DownloadThread(void *p)
{
    (void)p; g_updErr[0] = 0;
    if (g_dlYield) SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);   /* 6.1: the automatic one, behind everything else */
    wchar_t tmp[MAX_PATH], dir[MAX_PATH]; GetTempPathW(MAX_PATH, tmp); _snwprintf(dir, MAX_PATH, L"%sPORTHEON-update", tmp); CreateDirectoryW(dir, NULL);
    Join(g_dlPath, dir, g_updName[0] ? g_updName : L"PORTHEON-Windows-x64.zip");
    char want[65] = "", have[65] = "";
    BOOL ok = UpdExpected(want);   /* 6.1: first, so a package already here can be used as it is */
    if (ok) {
        if (FileExists(g_dlPath)) InterlockedExchange(&g_updState, 3);
        if (FileExists(g_dlPath) && Sha256File(g_dlPath, have) && !strcmp(have, want)) {   /* 6.1: downloaded before (a failed install, a pause) and whole */
            WIN32_FILE_ATTRIBUTE_DATA fa; if (GetFileAttributesExW(g_dlPath, GetFileExInfoStandard, &fa)) g_dlTotal = g_dlGot = ((LONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
        } else {
            InterlockedExchange(&g_updState, 2);
            ok = DownloadTo(g_updZip, g_dlPath);
            if (ok) {
                InterlockedExchange(&g_updState, 3);
                if (!Sha256File(g_dlPath, have) || strcmp(want, have)) { ok = FALSE; DeleteFileW(g_dlPath); InterlockedExchange(&g_dlHard, 1); _snwprintf(g_updErr, 300, L"%s", L2(L"o pacote baixado não confere com o SHA-256 da release (baixe de novo)", L"the downloaded package does not match the release's SHA-256 (download again)")); }
            }
        }
        if (ok) MultiByteToWideChar(CP_ACP, 0, want, -1, g_updSha, 65);
    }
    InterlockedExchange(&g_dlYield, 0);   /* 6.1: the Bloodborne download that may come next never pauses (g_dlPaused stays for OnDownloadDone) */
    InterlockedExchange(&g_dlBusy, 0);   /* 6.1 review: DownloadTo is free again (StartDownload took it) */
    PostMessageW(g_wnd, WM_APP_DLDONE, ok, 0);
    return 0;
}
/* 6.1: quiet = the automatic download (no window over the screen, steps aside while a game runs); FALSE when it did not start */
static BOOL StartDownloadEx(BOOL quiet)
{
    if (!g_updZip[0] || (!g_updSums[0] && !g_updWant[0])) { if (!quiet) ShellExecuteW(g_wnd, L"open", UPD_PAGE, NULL, NULL, SW_SHOWNORMAL); return FALSE; }
    if (g_proc) { if (!quiet) Msg(C_ORANGE, L"%s", L2(L"Feche o jogo antes de atualizar.", L"Close the game before updating.")); return FALSE; }
    /* 6.1 review: one download at a time (g_dlBusy, extgames.c): not during the Bloodborne port's install */
    if (g_ngBusy || InterlockedCompareExchange(&g_dlBusy, 1, 0)) { if (!quiet) Msg(C_ORANGE, L"%s", L2(L"O PORTHEON está instalando um port de jogo: espere terminar e atualize depois (botão no topo).", L"PORTHEON is installing a game port: wait for it to end, then update (button at the top).")); return FALSE; }
    InterlockedExchange(&g_dlCancel, 0); InterlockedExchange(&g_dlPaused, 0); InterlockedExchange(&g_dlYield, quiet ? 1 : 0);
    if (!quiet) KillTimer(g_wnd, UPD_AUTO_TIMER);   /* 6.1 review: the player's own download: the automatic one stops waiting */
    g_updCancelled = FALSE;
    g_updAuto = quiet; g_dlGot = 0; g_dlTotal = 0; g_dlT0 = GetTickCount64();
    InterlockedExchange(&g_updState, 2); if (!quiet) g_overlay = OV_UPDATE;
    HANDLE th = CreateThread(NULL, 0, DownloadThread, NULL, 0, NULL);
    if (th) CloseHandle(th); else { InterlockedExchange(&g_dlBusy, 0); InterlockedExchange(&g_dlYield, 0); InterlockedExchange(&g_updState, 5); InvalidateRect(g_wnd, NULL, FALSE); return FALSE; }
    InvalidateRect(g_wnd, NULL, FALSE);
    return TRUE;
}
static void StartDownload(void) { StartDownloadEx(FALSE); }
/* 6.1 review: the player's Cancel (H_UPDCANCEL): the download stops, the automatic install stops waiting (its timer would
   otherwise start the download again 30 s later) and this version is never downloaded by itself again (update_auto_skip;
   the top button and a check the player asks for still offer it) */
static void UpdCancel(void)
{
    InterlockedExchange(&g_dlCancel, 1);
    KillTimer(g_wnd, UPD_AUTO_TIMER); g_updReady = FALSE; g_updCancelled = TRUE;
    if (g_updTag[0]) WritePrivateProfileStringW(L"PORTHEON", L"update_auto_skip", g_updTag, g_pIni);
}

/* ---------------------------------------------------------------- install (after PORTHEON closes) */
static BOOL DirWritable(const wchar_t *d)
{
    wchar_t t[MAX_PATH]; Join(t, d, L".portheon-write-test");
    HANDLE h = CreateFileW(t, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    CloseHandle(h); return TRUE;
}
static BOOL DotName(const wchar_t *n) { return !wcscmp(n, L".") || !wcscmp(n, L".."); }
static void DelTree(const wchar_t *d)
{
    wchar_t q[MAX_PATH]; Join(q, d, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd);
    if (f != INVALID_HANDLE_VALUE) {
        do {
            if (DotName(fd.cFileName)) continue;
            wchar_t p[MAX_PATH]; Join(p, d, fd.cFileName);
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) DelTree(p);
            else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveDirectoryW(p);
            else { SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL); DeleteFileW(p); }
        } while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    RemoveDirectoryW(d);
}
/* every file of src over dst (folders made), a locked file retried like robocopy /R:5 /W:2; bad gets the first that failed */
static int CopyTree(const wchar_t *src, const wchar_t *dst, wchar_t *bad)
{
    int fails = 0; CreateDirectoryW(dst, NULL);
    wchar_t q[MAX_PATH]; Join(q, src, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd);
    if (f == INVALID_HANDLE_VALUE) return 1;
    do {
        if (DotName(fd.cFileName)) continue;
        wchar_t s[MAX_PATH], t[MAX_PATH]; Join(s, src, fd.cFileName); Join(t, dst, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { fails += CopyTree(s, t, bad); continue; }
        BOOL ok = FALSE;
        for (int i = 0; i < 6 && !ok; i++) {
            if (i) Sleep(2000);
            DWORD a = GetFileAttributesW(t); if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_READONLY)) SetFileAttributesW(t, a & ~FILE_ATTRIBUTE_READONLY);
            ok = CopyFileW(s, t, FALSE);
        }
        if (!ok) { if (!bad[0]) _snwprintf(bad, MAX_PATH, L"%s", t); fails++; }
    } while (FindNextFileW(f, &fd));
    FindClose(f);
    return fails;
}
/* a program running from PORTHEON's folder (a game started by itself, its engine), this installer apart */
static BOOL BusyIn(const wchar_t *dir)
{
    BOOL busy = FALSE; size_t n = wcslen(dir); DWORD self = GetCurrentProcessId();
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32W pe = {sizeof(pe)};
    if (sn == INVALID_HANDLE_VALUE) return FALSE;
    if (Process32FirstW(sn, &pe)) do {
        if (pe.th32ProcessID == self) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID); if (!h) continue;
        wchar_t p[MAX_PATH]; DWORD len = MAX_PATH;
        if (QueryFullProcessImageNameW(h, 0, p, &len) && len > n && !_wcsnicmp(p, dir, n) && p[n] == L'\\') busy = TRUE;
        CloseHandle(h);
    } while (!busy && Process32NextW(sn, &pe));
    CloseHandle(sn); return busy;
}
static void LeftoversDrop(void);
static DWORD WINAPI DropOldExe(void *p)   /* the PORTHEON.exe the last update replaced (renamed while it ran) */
{
    (void)p; wchar_t o[MAX_PATH]; Join(o, g_exeDir, L"PORTHEON.exe.old");
    for (int i = 0; i < 20 && FileExists(o) && !DeleteFileW(o); i++) Sleep(500);
    Sleep(1500); LeftoversDrop();   /* 5.1: PORTHEON.exe.portheon-old and the others the last install left */
    return 0;
}

/* ---------------------------------------------------------------- 5.1: the install as one step
   Players' reports (2026-11-05): after an update some installs were half new, half old and needed a new download. The
   old install copied file by file over the folder and, when one failed (a game or an antivirus holding it), put back
   only PORTHEON.exe. Now: (1) every file the package replaces is first renamed to <name>.portheon-old (a rename also
   works on a running program), and if one cannot be renamed nothing has changed yet; (2) the package is copied; (3) if
   a copy fails, the new files go and every old one is renamed back: the folder is exactly the old version again.
   Only after all of it the .portheon-old files are deleted (the ones in use: at the next start, update-leftovers.txt).
   The player's data is never in the package and never touched: saves (_SaveData), caches, UFC5.ini / UFC6.ini, the
   learned skip lists; files a player tunes (-community.xml, UFC 5's AeonSR.ini / ReShadePreset.ini) stay when present. */
static BOOL g_installElevated;   /* the installer runs as administrator (ApplyUpdate) */
typedef struct { wchar_t (*rel)[MAX_PATH]; int n, cap; } RelList;
static void ListTree(const wchar_t *root, const wchar_t *rel, RelList *L)
{
    wchar_t dir[MAX_PATH], q[MAX_PATH]; if (rel[0]) Join(dir, root, rel); else _snwprintf(dir, MAX_PATH, L"%s", root);
    Join(q, dir, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd);
    if (f == INVALID_HANDLE_VALUE) return;
    do {
        if (DotName(fd.cFileName)) continue;
        wchar_t r[MAX_PATH]; if (rel[0]) Join(r, rel, fd.cFileName); else _snwprintf(r, MAX_PATH, L"%s", fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { ListTree(root, r, L); continue; }
        if (L->n == L->cap) { int c = L->cap ? L->cap * 2 : 1024; void *m = realloc(L->rel, (size_t)c * sizeof(*L->rel)); if (!m) continue; L->rel = m; L->cap = c; }
        wcscpy(L->rel[L->n++], r);
    } while (FindNextFileW(f, &fd));
    FindClose(f);
}
static void MakeParents(const wchar_t *file)   /* every folder above file */
{
    wchar_t p[MAX_PATH]; _snwprintf(p, MAX_PATH, L"%s", file); p[MAX_PATH - 1] = 0;
    for (wchar_t *s = p + 3; *s; s++) if (*s == L'\\') { *s = 0; CreateDirectoryW(p, NULL); *s = L'\\'; }
}
static BOOL KeepPlayers(const wchar_t *rel, const wchar_t *target)   /* the player's own copy of this file stays */
{
    static const wchar_t *const tuned[] = {L"games\\UFC5\\bin\\AeonSR.ini", L"games\\UFC5\\bin\\ReShadePreset.ini",
                                           L"games\\UFC5\\UFC5.ini", L"games\\UFC6\\UFC6.ini"};   /* 6.1: the games' settings too, should a package ever carry them */
    if (StrStrIW(rel, L"\\_SaveData\\") || StrStrIW(rel, L"\\_PipelineCache\\") || StrStrIW(rel, L"\\_ShaderCache\\")) return TRUE;   /* never from a package */
    if (!FileExists(target)) return FALSE;
    size_t n = wcslen(rel); if (n > 14 && !_wcsicmp(rel + n - 14, L"-community.xml")) return TRUE;
    for (int i = 0; i < (int)(sizeof(tuned) / sizeof(tuned[0])); i++) if (!_wcsicmp(rel, tuned[i])) return TRUE;
    return FALSE;
}
static void LeftoverAdd(const wchar_t *data, const wchar_t *path)
{
    wchar_t f[MAX_PATH]; Join(f, data, L"update-leftovers.txt"); char u[3 * MAX_PATH + 4]; DWORD wr;
    int n = WideCharToMultiByte(CP_UTF8, 0, path, -1, u, sizeof(u) - 2, NULL, NULL); if (n < 2) return;
    u[n - 1] = '\n'; HANDLE h = CreateFileW(f, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { WriteFile(h, u, (DWORD)n, &wr, NULL); CloseHandle(h); }
}
static void LeftoversDrop(void)   /* the .portheon-old files the last update could not delete (in use then) */
{
    wchar_t pd[MAX_PATH], f[MAX_PATH]; LocalData(pd, L"PORTHEON"); Join(f, pd, L"update-leftovers.txt");
    HANDLE h = CreateFileW(f, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); if (h == INVALID_HANDLE_VALUE) return;
    DWORD sz = GetFileSize(h, NULL), got = 0; char *b = sz < (1u << 20) ? (char *)malloc(sz + 1) : NULL;
    if (b && ReadFile(h, b, sz, &got, NULL)) { b[got] = 0; } else got = 0;
    CloseHandle(h);
    BOOL left = FALSE;
    for (char *l = b; b && l && *l; ) {
        char *e = strchr(l, '\n'); if (e) *e = 0;
        wchar_t w[MAX_PATH]; if (MultiByteToWideChar(CP_UTF8, 0, l, -1, w, MAX_PATH) && wcsstr(w, L".portheon-old")) {
            for (int i = 0; i < 10 && FileExists(w); i++) { SetFileAttributesW(w, FILE_ATTRIBUTE_NORMAL); if (DeleteFileW(w)) break; Sleep(300); }
            if (FileExists(w)) left = TRUE;
        }
        l = e ? e + 1 : NULL;
    }
    free(b); if (!left) DeleteFileW(f);
}
static int InstallTree(const wchar_t *top, const wchar_t *dst, const wchar_t *data, wchar_t *err, int errN)
{
    RelList L = {0}; ListTree(top, L"", &L);
    if (!L.n) { _snwprintf(err, errN, L"%s", L2(L"o pacote está vazio", L"the package is empty")); return 1; }
    BYTE *st = (BYTE *)calloc((size_t)L.n, 1);   /* 1 the old file renamed, 2 the new one copied, 4 left as the player's */
    if (!st) { free(L.rel); _snwprintf(err, errN, L"%s", L2(L"sem memória", L"out of memory")); return 1; }
    int fail = 0; wchar_t s[MAX_PATH], t[MAX_PATH], o[MAX_PATH + 16];
    for (int i = 0; i < L.n && !fail; i++) {   /* 1: the files being replaced step aside */
        Join(t, dst, L.rel[i]);
        if (KeepPlayers(L.rel[i], t)) { st[i] = 4; continue; }
        if (!FileExists(t)) continue;
        _snwprintf(o, MAX_PATH + 16, L"%s.portheon-old", t);
        SetFileAttributesW(o, FILE_ATTRIBUTE_NORMAL); DeleteFileW(o);
        DWORD a = GetFileAttributesW(t); if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_READONLY)) SetFileAttributesW(t, a & ~FILE_ATTRIBUTE_READONLY);
        BOOL ok = FALSE; for (int k = 0; k < 6 && !ok; k++) { if (k) Sleep(2000); ok = MoveFileExW(t, o, MOVEFILE_REPLACE_EXISTING); }
        if (ok) st[i] = 1; else { fail = 1; _snwprintf(err, errN, L2(L"um arquivo está em uso por outro programa (%s): feche os jogos e o antivírus e atualize de novo", L"a file is in use by another program (%s): close the games and the antivirus and update again"), L.rel[i]); }
    }
    for (int i = 0; i < L.n && !fail; i++) {   /* 2: the package goes in */
        if (st[i] == 4) continue;
        Join(s, top, L.rel[i]); Join(t, dst, L.rel[i]); MakeParents(t);
        BOOL ok = FALSE; for (int k = 0; k < 4 && !ok; k++) { if (k) Sleep(1500); ok = CopyFileW(s, t, FALSE); }
        if (ok) st[i] |= 2; else { fail = 1; _snwprintf(err, errN, L2(L"não consegui copiar %s (erro %lu; disco cheio?)", L"could not copy %s (error %lu; disk full?)"), L.rel[i], GetLastError()); }
    }
    for (int i = 0; i < L.n; i++) {   /* 3: undone on a failure, tidied on success */
        Join(t, dst, L.rel[i]); _snwprintf(o, MAX_PATH + 16, L"%s.portheon-old", t);
        if (fail) {
            BOOL back = TRUE;
            if (st[i] & 2) { SetFileAttributesW(t, FILE_ATTRIBUTE_NORMAL); for (int k = 0; k < 6 && !DeleteFileW(t) && FileExists(t); k++) Sleep(2000); }
            if (st[i] & 1) { back = FALSE; for (int k = 0; k < 6 && !back; k++) { if (k) Sleep(2000); back = MoveFileExW(o, t, MOVEFILE_REPLACE_EXISTING); } }
            if (!back) { size_t m = wcslen(err); _snwprintf(err + m, errN - (int)m, L2(L"; não consegui restaurar %s (a cópia antiga está em %s.portheon-old)", L"; could not restore %s (the old copy is at %s.portheon-old)"), L.rel[i], L.rel[i]); err[errN - 1] = 0; }
        } else if (st[i] & 1) {   /* an elevated install schedules what it cannot delete for the next restart (its folder is protected) */
            SetFileAttributesW(o, FILE_ATTRIBUTE_NORMAL);
            if (!DeleteFileW(o) && !(g_installElevated && MoveFileExW(o, NULL, MOVEFILE_DELAY_UNTIL_REBOOT))) LeftoverAdd(data, o);
        }
    }
    free(st); free(L.rel); return fail;
}

/* PORTHEON.exe --apply-update "<zip>" <pid> "<tag>" <elevated> "<PORTHEON data folder>" <sha256> */
typedef struct { wchar_t zip[MAX_PATH], tag[32], data[MAX_PATH], sha[65]; DWORD pid; int elevated; } ApplyArgs;
static ApplyArgs g_ap; static HWND g_apWnd; static wchar_t g_apText[200];
static void ApplyStep(const wchar_t *t) { _snwprintf(g_apText, 200, L"%s", t); g_apText[199] = 0; if (g_apWnd) InvalidateRect(g_apWnd, NULL, TRUE); }
static DWORD WINAPI ApplyThread(void *p)
{
    (void)p; ApplyArgs *a = &g_ap;
    wchar_t err[400] = L"", bad[MAX_PATH] = L"", exe[MAX_PATH], old[MAX_PATH], wd[MAX_PATH], x[MAX_PATH], top[MAX_PATH] = L"";
    Join(exe, g_exeDir, L"PORTHEON.exe"); Join(old, g_exeDir, L"PORTHEON.exe.old");
    wcscpy(wd, a->zip); { wchar_t *s = wcsrchr(wd, L'\\'); if (s) *s = 0; } Join(x, wd, L"x");
    HANDLE pp = OpenProcess(SYNCHRONIZE, FALSE, a->pid); if (pp) { WaitForSingleObject(pp, 60000); CloseHandle(pp); }
    if (BusyIn(g_exeDir)) ApplyStep(L2(L"Esperando o jogo fechar...", L"Waiting for the game to close..."));
    for (int i = 0; i < 180 && BusyIn(g_exeDir); i++) Sleep(1000);
    if (BusyIn(g_exeDir)) _snwprintf(err, 400, L"%s", L2(L"um jogo do PORTHEON ainda estava aberto: feche-o e atualize de novo (nada foi mudado)", L"a PORTHEON game was still open: close it and update again (nothing was changed)"));   /* 5.1 */
    else {

    ApplyStep(L2(L"Conferindo e abrindo o pacote...", L"Checking and unpacking the package..."));
    char have[65] = ""; wchar_t hw[65] = L"";
    if (Sha256File(a->zip, have)) MultiByteToWideChar(CP_ACP, 0, have, -1, hw, 65);
    if (wcslen(a->sha) != 64 || _wcsicmp(hw, a->sha)) _snwprintf(err, 400, L"%s", L2(L"o pacote não confere com o SHA-256 da release", L"the package does not match the release's SHA-256"));
    else {
        DelTree(x); CreateDirectoryW(x, NULL);
        wchar_t tar[MAX_PATH], cmd[3 * MAX_PATH]; GetSystemDirectoryW(tar, MAX_PATH); wcscat(tar, L"\\tar.exe");
        DWORD code = 1;
        if (FileExists(tar)) {
            _snwprintf(cmd, 3 * MAX_PATH, L"\"%s\" -xf \"%s\" -C \"%s\"", tar, a->zip, x); cmd[3 * MAX_PATH - 1] = 0;
            STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi;
            if (CreateProcessW(tar, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, x, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, 600000); GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
            }
        }
        if (code == 0) {
            wchar_t q[MAX_PATH]; Join(q, x, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd);
            if (f != INVALID_HANDLE_VALUE) {
                do {
                    wchar_t d[MAX_PATH], e[MAX_PATH];
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || DotName(fd.cFileName)) continue;
                    Join(d, x, fd.cFileName); Join(e, d, L"PORTHEON.exe"); if (FileExists(e)) { wcscpy(top, d); break; }
                } while (FindNextFileW(f, &fd));
                FindClose(f);
            }
        }
        if (!FileExists(tar)) _snwprintf(err, 400, L"%s", L2(L"este Windows não tem o tar.exe (Windows 10 de 2018 ou mais novo)", L"this Windows has no tar.exe (Windows 10 from 2018 or newer)"));
        else if (code) _snwprintf(err, 400, L2(L"não consegui abrir o pacote (tar %lu)", L"could not unpack the package (tar %lu)"), code);
        else if (!top[0]) _snwprintf(err, 400, L"%s", L2(L"o pacote não é do PORTHEON", L"not a PORTHEON package"));
    }
    }   /* 5.1: no game open */
    if (!err[0]) {
        ApplyStep(L2(L"Instalando (tudo ou nada: se algo falhar, a versão atual fica como estava)...", L"Installing (all or nothing: if something fails, the current version stays as it was)..."));
        wchar_t bk[MAX_PATH], t[MAX_PATH]; Join(t, a->data, L"rollback"); CreateDirectoryW(t, NULL); Join(bk, t, APP_VER); CreateDirectoryW(bk, NULL);
        Join(t, bk, L"PORTHEON.exe"); CopyFileW(exe, t, FALSE);
        DeleteFileW(old);
        /* 5.1: one step (InstallTree); a running program's file can be renamed, so PORTHEON.exe steps aside like the rest */
        if (InstallTree(top, g_exeDir, a->data, err, 400) == 0 && !FileExists(exe)) _snwprintf(err, 400, L"%s", L2(L"o PORTHEON.exe novo não ficou na pasta", L"the new PORTHEON.exe is not in the folder"));
        (void)bad;
    }
    err[399] = 0;
    {   /* what UpdateResult shows on the next start */
        wchar_t res[460], flag[MAX_PATH]; char u[1400]; DWORD wr;
        _snwprintf(res, 460, err[0] ? L"fail %s" : L"ok %s", err[0] ? err : a->tag); res[459] = 0;
        int n = WideCharToMultiByte(CP_UTF8, 0, res, -1, u, sizeof(u), NULL, NULL);
        Join(flag, a->data, L"update-result.txt");
        HANDLE h = CreateFileW(flag, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) { if (n > 1) WriteFile(h, u, (DWORD)(n - 1), &wr, NULL); CloseHandle(h); }
    }
    if (!err[0]) DeleteFileW(a->zip);
    DelTree(x);
    /* open PORTHEON again; from an elevated installer through Explorer, so it runs as the player and not as administrator */
    wchar_t q[MAX_PATH + 2]; _snwprintf(q, MAX_PATH + 2, L"\"%s\"", exe); q[MAX_PATH + 1] = 0;
    if (a->elevated) ShellExecuteW(NULL, L"open", L"explorer.exe", q, NULL, SW_SHOWNORMAL);
    else { STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi; if (CreateProcessW(exe, q, NULL, NULL, FALSE, 0, NULL, g_exeDir, &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); } }
    PostMessageW(g_apWnd, WM_APP, 0, 0);
    return 0;
}
static LRESULT CALLBACK ApplyWndProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    static HFONT font;
    switch (m) {
    case WM_CREATE: font = CreateFontW(-MulDiv(10, GetDpiForWindow(h), 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI"); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT r; GetClientRect(h, &r);
        HBRUSH b = CreateSolidBrush(C_BG); FillRect(dc, &r, b); DeleteObject(b);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, C_TEXT); HGDIOBJ o = SelectObject(dc, font);
        DrawTextW(dc, g_apText, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, o); EndPaint(h, &ps); return 0; }
    case WM_CLOSE: return 0;   /* not in the middle of the copy */
    case WM_APP: DestroyWindow(h); return 0;
    case WM_DESTROY: if (font) DeleteObject(font); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}
static int ApplyUpdate(HINSTANCE inst)
{
    int argc = 0; wchar_t **av = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!av || argc < 8 || _wcsicmp(av[1], L"--apply-update")) { if (av) LocalFree(av); return 1; }
    _snwprintf(g_ap.zip, MAX_PATH, L"%s", av[2]); g_ap.pid = wcstoul(av[3], NULL, 10); _snwprintf(g_ap.tag, 32, L"%s", av[4]);
    g_ap.elevated = _wtoi(av[5]); g_installElevated = g_ap.elevated != 0; _snwprintf(g_ap.data, MAX_PATH, L"%s", av[6]); _snwprintf(g_ap.sha, 65, L"%s", av[7]);
    g_ap.zip[MAX_PATH - 1] = g_ap.data[MAX_PATH - 1] = g_ap.tag[31] = g_ap.sha[64] = 0;
    LocalFree(av);
    GetModuleFileNameW(NULL, g_exeDir, MAX_PATH); { wchar_t *s = wcsrchr(g_exeDir, L'\\'); if (s) *s = 0; }
    Join(g_pIni, g_ap.data, L"portheon.ini"); g_lang = LangLoad(g_pIni);   /* 6.1: the player's language (ui_lang), not only PT/EN */
    _snwprintf(g_apText, 200, L2(L"Instalando o PORTHEON %s...", L"Installing PORTHEON %s..."), g_ap.tag);
    WNDCLASSEXW wc = {sizeof(wc)}; wc.lpfnWndProc = ApplyWndProc; wc.hInstance = inst; wc.lpszClassName = L"PortheonUpdate";
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_APPSTARTING); wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    RegisterClassExW(&wc);
    UINT dpi = GetDpiForSystem(); DWORD style = WS_OVERLAPPED | WS_CAPTION;
    RECT r = {0, 0, MulDiv(460, dpi, 96), MulDiv(90, dpi, 96)}; AdjustWindowRectExForDpi(&r, style, FALSE, 0, dpi);
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int ww = r.right - r.left, wh = r.bottom - r.top;
    g_apWnd = CreateWindowExW(0, wc.lpszClassName, L2(L"PORTHEON - atualização", L"PORTHEON - update"), style,
                              wa.left + (wa.right - wa.left - ww) / 2, wa.top + (wa.bottom - wa.top - wh) / 2, ww, wh, NULL, NULL, inst, NULL);
    BOOL dark = TRUE; DwmSetWindowAttribute(g_apWnd, 20, &dark, sizeof(dark));
    ShowWindow(g_apWnd, SW_SHOWNORMAL); UpdateWindow(g_apWnd);
    HANDLE th = CreateThread(NULL, 0, ApplyThread, NULL, 0, NULL);
    if (!th) { ApplyThread(NULL); } else CloseHandle(th);
    MSG msg; while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}


/* ---------------------------------------------------------------- 5.1: saves from an older PORTHEON folder
   UFC 5 and 6 keep their saves, caches and settings inside PORTHEON's folder (games\UFCn\bin\_SaveData, _PipelineCache,
   _ShaderCache, games\UFCn\UFCn.ini). A player who extracts a new version into a new folder "lost" them (players'
   reports, 2026-11-05). At start, for each of the two games whose saves are missing here, the newest saves of another
   PORTHEON folder are copied in: the last folder this PORTHEON ran from (install_dir), the folders next to this one and
   the ones a level deeper ("PORTHEON 5.0\PORTHEON-5.0"). Only copied, never moved; nothing here is overwritten. */
static BOOL DirHasFiles(const wchar_t *d)
{
    wchar_t q[MAX_PATH]; Join(q, d, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd); BOOL any = FALSE;
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    do { if (!DotName(fd.cFileName)) any = TRUE; } while (!any && FindNextFileW(f, &fd));
    FindClose(f); return any;
}
static ULONGLONG NewestIn(const wchar_t *d, int depth)
{
    ULONGLONG best = 0; wchar_t q[MAX_PATH]; Join(q, d, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd);
    if (f == INVALID_HANDLE_VALUE) return 0;
    do {
        if (DotName(fd.cFileName)) continue;
        ULONGLONG t = ((ULONGLONG)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && depth < 6) { wchar_t s[MAX_PATH]; Join(s, d, fd.cFileName); ULONGLONG u = NewestIn(s, depth + 1); if (u > t) t = u; }
        if (t > best) best = t;
    } while (FindNextFileW(f, &fd));
    FindClose(f); return best;
}
static int g_migN; static wchar_t g_migDirs[64][MAX_PATH];
static void MigAdd(const wchar_t *d)
{
    wchar_t e[MAX_PATH]; Join(e, d, L"PORTHEON.exe");
    if (g_migN >= 64 || !_wcsicmp(d, g_exeDir) || !FileExists(e)) return;
    for (int i = 0; i < g_migN; i++) if (!_wcsicmp(g_migDirs[i], d)) return;
    wcscpy(g_migDirs[g_migN++], d);
}
static void MigScan(const wchar_t *parent, int depth)
{
    wchar_t q[MAX_PATH]; Join(q, parent, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd);
    if (f == INVALID_HANDLE_VALUE) return;
    do {
        if (DotName(fd.cFileName) || !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        wchar_t d[MAX_PATH]; Join(d, parent, fd.cFileName); MigAdd(d);
        if (depth < 1) MigScan(d, depth + 1);
    } while (FindNextFileW(f, &fd));
    FindClose(f);
}
static void MigrateOldInstall(void)
{
    wchar_t prev[MAX_PATH] = L""; GetPrivateProfileStringW(L"PORTHEON", L"install_dir", L"", prev, MAX_PATH, g_pIni);
    if (!_wcsicmp(prev, g_exeDir)) return;   /* only the first start in a folder looks around (install_dir: written once it worked) */
    static const wchar_t *const games[2] = {L"UFC5", L"UFC6"};
    BOOL need = FALSE;
    for (int k = 0; k < 2; k++) { wchar_t m[MAX_PATH]; _snwprintf(m, MAX_PATH, L"%s\\games\\%s\\bin\\_SaveData", g_exeDir, games[k]); if (!DirHasFiles(m)) need = TRUE; }
    if (!need) { WritePrivateProfileStringW(L"PORTHEON", L"install_dir", g_exeDir, g_pIni); return; }
    g_migN = 0; if (prev[0]) MigAdd(prev);
    { wchar_t parent[MAX_PATH]; wcscpy(parent, g_exeDir); wchar_t *s = wcsrchr(parent, L'\\'); if (s && s > parent + 2) { *s = 0; MigScan(parent, 0);
        wchar_t *s2 = wcsrchr(parent, L'\\'); if (s2 && s2 > parent + 2) { *s2 = 0; MigScan(parent, 0); } } }   /* next to it, and next to its parent folder */
    wchar_t done[200] = L""; int allFails = 0;
    for (int k = 0; k < 2; k++) {
        wchar_t mine[MAX_PATH]; _snwprintf(mine, MAX_PATH, L"%s\\games\\%s\\bin\\_SaveData", g_exeDir, games[k]);
        if (DirHasFiles(mine)) continue;
        int best = -1; ULONGLONG bt = 0;
        for (int i = 0; i < g_migN; i++) {
            wchar_t sd[MAX_PATH]; _snwprintf(sd, MAX_PATH, L"%s\\games\\%s\\bin\\_SaveData", g_migDirs[i], games[k]);
            if (!DirHasFiles(sd)) continue;
            ULONGLONG t = NewestIn(sd, 0); if (best < 0 || t > bt) { best = i; bt = t; }
        }
        if (best < 0) continue;
        static const wchar_t *const sub[] = {L"bin\\_SaveData", L"bin\\_PipelineCache", L"bin\\_ShaderCache"};
        wchar_t bad[MAX_PATH] = L""; int fails = 0;
        for (int j = 0; j < 3; j++) {
            wchar_t s[MAX_PATH], d[MAX_PATH];
            _snwprintf(s, MAX_PATH, L"%s\\games\\%s\\%s", g_migDirs[best], games[k], sub[j]); _snwprintf(d, MAX_PATH, L"%s\\games\\%s\\%s", g_exeDir, games[k], sub[j]);
            if (!DirExists(s) || (j > 0 && DirHasFiles(d))) continue;
            fails += CopyTree(s, d, bad);
        }
        {   /* the game's settings, when this folder has none yet; UFC 5's engine path is found again in this folder */
            wchar_t s[MAX_PATH], d[MAX_PATH];
            _snwprintf(s, MAX_PATH, L"%s\\games\\%s\\%s.ini", g_migDirs[best], games[k], games[k]); _snwprintf(d, MAX_PATH, L"%s\\games\\%s\\%s.ini", g_exeDir, games[k], games[k]);
            if (FileExists(s) && !FileExists(d) && CopyFileW(s, d, TRUE) && k == 0) WritePrivateProfileStringW(L"UFC5", L"emulator", L"", d);
        }
        if (!fails) { if (done[0]) wcscat(done, L" e "); wcscat(done, k ? L"UFC 6" : L"UFC 5"); }
        allFails += fails;
    }
    if (!allFails) WritePrivateProfileStringW(L"PORTHEON", L"install_dir", g_exeDir, g_pIni);   /* a copy that failed is tried again next start */
    if (done[0]) Msg(C_GOOD, L2(L"Saves e configurações do %s trazidos da pasta antiga do PORTHEON.", L"%s saves and settings brought over from the older PORTHEON folder."), done);
}

/* 6.1: the automatic install waits for the player: no game (built-in or of their own library), no port install, no
   download, no UFC 2 room, nothing running from PORTHEON's folder; free for two looks in a row (15 to 30 s after the
   game closed, so a quick "play again" is not cut). A timer of its own (TIMERPROC: no line in the window procedure). */
static int g_updFree;
static BOOL UpdBusyNow(void)
{
    return g_proc || g_crun || g_u2srv || g_ngBusy || g_dlBusy || BusyIn(g_exeDir);
}
/* 6.1 review: the install closes PORTHEON and opens it again, so it may only happen while the player is at PORTHEON:
   PORTHEON's window is the foreground one, or the player has left the PC alone for 5 minutes and the foreground window
   is not a full-screen one (a game started outside PORTHEON's watch, a store's launcher that handed it on, a video; a
   controller's input does not count as input for GetLastInputInfo, so a full-screen window always waits) */
static BOOL UpdFullscreenFg(HWND fg)
{
    RECT r; MONITORINFO mi = {sizeof(mi)};
    if (!fg || !GetWindowRect(fg, &r)) return FALSE;
    if (fg == GetDesktopWindow() || fg == GetShellWindow()) return FALSE;
    HMONITOR mon = MonitorFromWindow(fg, MONITOR_DEFAULTTONULL);
    if (!mon || !GetMonitorInfoW(mon, &mi)) return FALSE;
    return r.left <= mi.rcMonitor.left && r.top <= mi.rcMonitor.top && r.right >= mi.rcMonitor.right && r.bottom >= mi.rcMonitor.bottom;
}
static BOOL UpdPlayerAtPortheon(void)
{
    HWND fg = GetForegroundWindow(); DWORD pid = 0;
    if (fg) GetWindowThreadProcessId(fg, &pid);
    if (fg && pid == GetCurrentProcessId()) return TRUE;
    LASTINPUTINFO li = {sizeof(li)};
    if (!GetLastInputInfo(&li) || GetTickCount() - li.dwTime < 5 * 60 * 1000) return FALSE;
    return !UpdFullscreenFg(fg);
}
static void UpdApplyNow(void);
static BOOL UpdAutoOn(void);
static void CALLBACK UpdAutoTimer(HWND h, UINT m, UINT_PTR id, DWORD t)
{
    (void)h; (void)m; (void)id; (void)t;
    /* 6.1 review: an automatic one cancelled, failed or switched off meanwhile (a package the player asked for still installs) */
    if (g_updAuto && !UpdAutoOn()) { KillTimer(g_wnd, UPD_AUTO_TIMER); g_updFree = 0; return; }
    if ((g_updState >= 2 && g_updState <= 4) || UpdBusyNow()) { g_updFree = 0; return; }   /* a download or an install under way, or the player busy */
    if (g_updReady && !UpdPlayerAtPortheon()) { g_updFree = 0; return; }   /* 6.1 review: the install waits for the player at PORTHEON */
    if (++g_updFree < 2) return;
    KillTimer(g_wnd, UPD_AUTO_TIMER); g_updFree = 0;
    if (g_updReady) UpdApplyNow(); else if (UpdAutoOn()) StartDownloadEx(TRUE);
}
static void UpdAutoWait(void) { g_updFree = 0; SetTimer(g_wnd, UPD_AUTO_TIMER, 15000, UpdAutoTimer); }
/* 6.1: "Look for PORTHEON updates by itself" (update_check) also installs them (auto_update=0 keeps the window); a
   version whose automatic install failed once (update_auto_failed) goes back to the window, so it never loops.
   6.1 review: also a version the player cancelled (update_auto_skip), and a folder that needs administrator rights
   (Program Files): an automatic install never raises the administrator prompt by itself, the window asks instead */
static BOOL UpdAutoOn(void)
{
    if (UnderWine() || !PIni(L"update_check", 1) || !PIni(L"auto_update", 1) || !g_updTag[0]) return FALSE;
    wchar_t bad[32]; GetPrivateProfileStringW(L"PORTHEON", L"update_auto_failed", L"", bad, 32, g_pIni);
    if (!wcscmp(bad, g_updTag)) return FALSE;
    GetPrivateProfileStringW(L"PORTHEON", L"update_auto_skip", L"", bad, 32, g_pIni);
    if (!wcscmp(bad, g_updTag)) return FALSE;
    return DirWritable(g_exeDir);
}
/* 6.1 review: an automatic download or SHA-256 check that failed, counted per version: at the second the version goes
   back to the window that asks (update_auto_failed), so a proxy or antivirus that breaks the package does not make
   every start download ~500 MB again */
static void UpdAutoDlFailed(void)
{
    if (!InterlockedExchange(&g_dlHard, 0)) return;   /* offline or a broken connection: the next check simply tries again */
    wchar_t t[32]; GetPrivateProfileStringW(L"PORTHEON", L"update_auto_dlfail_tag", L"", t, 32, g_pIni);
    int n = wcscmp(t, g_updTag) ? 1 : (int)GetPrivateProfileIntW(L"PORTHEON", L"update_auto_dlfail", 0, g_pIni) + 1;
    wchar_t nb[16]; _snwprintf(nb, 16, L"%d", n);
    WritePrivateProfileStringW(L"PORTHEON", L"update_auto_dlfail_tag", g_updTag, g_pIni);
    WritePrivateProfileStringW(L"PORTHEON", L"update_auto_dlfail", nb, g_pIni);
    if (n >= 2) WritePrivateProfileStringW(L"PORTHEON", L"update_auto_failed", g_updTag, g_pIni);
}
static void UpdApplyNow(void)
{
    if (g_updAuto && !DirWritable(g_exeDir)) {   /* 6.1 review: never the administrator prompt by itself: the window asks */
        g_updAuto = FALSE; g_updReady = TRUE; InterlockedExchange(&g_updState, 1); g_overlay = OV_UPDATE;
        Msg(C_SUB, L2(L"PORTHEON %s baixado e conferido: a pasta precisa da permissão de administrador, clique em atualizar para instalar.", L"PORTHEON %s downloaded and checked: the folder needs administrator rights, press update to install it."), g_updTag);
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    g_updReady = FALSE;
    InterlockedExchange(&g_updState, 4);
    g_overlay = OV_UPDATE;   /* 6.1: the player sees why PORTHEON closes */
    if (g_updAuto) WritePrivateProfileStringW(L"PORTHEON", L"update_auto_try", g_updTag, g_pIni);
    InvalidateRect(g_wnd, NULL, FALSE); UpdateWindow(g_wnd);
    wchar_t self[MAX_PATH], pd[MAX_PATH]; GetModuleFileNameW(NULL, self, MAX_PATH); LocalData(pd, L"PORTHEON");
    BOOL elevate = !DirWritable(g_exeDir);
    static wchar_t args[2048];
    _snwprintf(args, 2048, L"--apply-update \"%s\" %lu \"%s\" %d \"%s\" %s", g_dlPath, GetCurrentProcessId(), g_updTag, elevate ? 1 : 0, pd, g_updSha);
    args[2047] = 0;
    SHELLEXECUTEINFOW ei = {sizeof(ei)}; ei.fMask = SEE_MASK_NOASYNC; ei.hwnd = g_wnd; ei.lpVerb = elevate ? L"runas" : L"open";
    ei.lpFile = self; ei.lpParameters = args; ei.lpDirectory = g_exeDir; ei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&ei)) {
        InterlockedExchange(&g_updState, 5);
        if (g_updAuto) WritePrivateProfileStringW(L"PORTHEON", L"update_auto_failed", g_updTag, g_pIni);   /* 6.1: the administrator prompt said no: ask from now on */
        WritePrivateProfileStringW(L"PORTHEON", L"update_auto_try", NULL, g_pIni);
        _snwprintf(g_updErr, 300, L"%s", elevate ? L2(L"a instalação precisa da permissão de administrador (a pasta do PORTHEON é protegida)", L"the install needs administrator rights (PORTHEON's folder is protected)")
                                                 : L2(L"o instalador não abriu", L"the installer did not start"));
        g_updAuto = FALSE;
        Msg(C_BAD, L"%s", g_updErr); InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    DestroyWindow(g_wnd);
}
static void OnDownloadDone(BOOL ok)
{
    if (g_updCancelled) {   /* 6.1 review: the player's Cancel is not a failure, nothing starts it again by itself, and it wins even over a finished download */
        InterlockedExchange(&g_dlPaused, 0);
        InterlockedExchange(&g_updState, 5); g_updAuto = FALSE; g_updCancelled = FALSE; KillTimer(g_wnd, UPD_AUTO_TIMER);
        Msg(C_SUB, L2(L"Download da atualização cancelado. O botão no topo baixa a %s quando você quiser.", L"Update download cancelled. The button at the top gets %s whenever you want."), g_updTag);
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    if (!ok && g_dlPaused) {   /* 6.1: the automatic download stepped aside for a game: it goes on when the game closes */
        InterlockedExchange(&g_dlPaused, 0); InterlockedExchange(&g_updState, 1); UpdAutoWait(); InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    if (!ok) {
        InterlockedExchange(&g_updState, 5);
        if (g_updAuto) UpdAutoDlFailed();   /* 6.1 review: at the second failure of this version, back to asking */
        if (g_updAuto) Msg(C_BAD, L2(L"A atualização automática para a %s não baixou (%s). O botão no topo tenta de novo; ou baixe em %s", L"The automatic update to %s did not download (%s). The button at the top tries again; or get it at %s"), g_updTag, g_updErr, UPD_PAGE);
        else Msg(C_BAD, L2(L"A atualização não baixou (%s). Tente de novo ou baixe em %s", L"The update did not download (%s). Try again or get it at %s"), g_updErr, UPD_PAGE);
        g_updAuto = FALSE;
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    /* 6.1: never while a game runs: the package waits, checked, and installs when the game closes. 6.1 review: an
       automatic one also waits for the player at PORTHEON (UpdPlayerAtPortheon); the player's own click is that */
    if (UpdBusyNow() || (g_updAuto && (!UpdPlayerAtPortheon() || !UpdAutoOn()))) {   /* 6.1 review: automatic turned off meanwhile: keep it ready and ask */
        g_updReady = TRUE; InterlockedExchange(&g_updState, 1); UpdAutoWait();
        Msg(C_SUB, L2(L"PORTHEON %s baixado e conferido: ele instala quando o jogo fechar.", L"PORTHEON %s downloaded and checked: it installs once the game closes."), g_updTag);
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    UpdApplyNow();
}
/* code: 1 newer release (at start), 2 newer (asked), 3 up to date (asked), 4..7 the check failed (asked; g_updWhy) */
static void OnUpdateFound(int code)
{
    if (code == 3) { Msg(C_GOOD, L2(L"Você já tem a versão mais nova (%s).", L"You already have the newest version (%s)."), L"Portheon " APP_SHOW); return; }
    if (code >= 4 && code <= 7) {   /* 6.1: why, instead of "no Internet?" for everything */
        Msg(C_ORANGE, L2(L"Não consegui procurar atualizações agora: %s. Página: %s", L"Could not look for updates now: %s. Page: %s"), g_updWhy[0] ? g_updWhy : L"?", UPD_PAGE); return;
    }
    if (code != 1 && code != 2) return;
    if (!UpdTakeFound()) return;   /* 6.1 review: the release goes into the globals here, on the UI thread */
    if (g_updState >= 2 && g_updState <= 4) return;   /* 6.1: a download or an install already under way */
    InterlockedExchange(&g_updState, 1);
    if (UnderWine() || !g_updZip[0] || (!g_updSums[0] && !g_updWant[0])) { Msg(C_ORANGE, L2(L"Nova versão %s disponível: %s", L"New version %s available: %s"), g_updTag, UPD_PAGE); return; }
    if (code == 1 && UpdAutoOn()) {   /* 6.1: by itself, in the background */
        g_updAuto = TRUE;   /* 6.1 review: the timer below is the automatic one */
        if (g_updReady && !UpdBusyNow() && UpdPlayerAtPortheon()) { UpdApplyNow(); return; }
        if (g_updReady || UpdBusyNow()) { UpdAutoWait(); Msg(C_SUB, L2(L"PORTHEON %s disponível: ele baixa e instala sozinho quando o jogo fechar.", L"PORTHEON %s available: it downloads and installs by itself once the game closes."), g_updTag); return; }
        if (StartDownloadEx(TRUE)) { Msg(C_SUB, L2(L"Baixando o PORTHEON %s: ele instala sozinho e abre de novo (pausa se você jogar).", L"Downloading PORTHEON %s: it installs by itself and opens again (pauses if you play)."), g_updTag); return; }
        g_updAuto = FALSE;   /* 6.1 review: it did not start: the window below asks */
    }
    if (g_proc) { Msg(C_ORANGE, L2(L"Nova versão %s disponível: ela instala quando o jogo fechar (botão no topo).", L"New version %s available: it installs once the game closes (button at the top)."), g_updTag); return; }
    wchar_t skip[32]; GetPrivateProfileStringW(L"PORTHEON", L"update_skip", L"", skip, 32, g_pIni);
    if (code == 1 && !wcscmp(skip, g_updTag)) { Msg(C_SUB, L2(L"A versão %s está disponível: o botão no topo atualiza.", L"Version %s is available: the button at the top updates."), g_updTag); return; }
    g_overlay = OV_UPDATE;
}
static void UpdateResult(void)   /* what the installer left after the last update */
{
    wchar_t f[MAX_PATH], pd[MAX_PATH]; LocalData(pd, L"PORTHEON"); Join(f, pd, L"update-result.txt");
    wchar_t tried[32]; GetPrivateProfileStringW(L"PORTHEON", L"update_auto_try", L"", tried, 32, g_pIni);   /* 6.1: an automatic install */
    HANDLE h = CreateFileW(f, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); if (h == INVALID_HANDLE_VALUE) return;
    char b[400] = ""; DWORD got = 0; ReadFile(h, b, sizeof(b) - 1, &got, NULL); CloseHandle(h); b[got] = 0; DeleteFileW(f);
    for (int i = (int)got - 1; i >= 0 && (b[i] == '\r' || b[i] == '\n' || b[i] == ' '); i--) b[i] = 0;
    wchar_t w[400]; MultiByteToWideChar(CP_UTF8, 0, b, -1, w, 400);
    WritePrivateProfileStringW(L"PORTHEON", L"update_auto_try", NULL, g_pIni);
    if (!strncmp(b, "ok ", 3)) { WritePrivateProfileStringW(L"PORTHEON", L"update_auto_failed", NULL, g_pIni);
                                 WritePrivateProfileStringW(L"PORTHEON", L"update_auto_dlfail", NULL, g_pIni); WritePrivateProfileStringW(L"PORTHEON", L"update_auto_dlfail_tag", NULL, g_pIni);   /* 6.1 review */
                                 Msg(C_GOOD, L2(L"Atualizado para a %s.", L"Updated to %s."), w + 3); g_overlay = OV_NEWS; }
    else if (!strncmp(b, "fail ", 5)) {
        if (tried[0]) WritePrivateProfileStringW(L"PORTHEON", L"update_auto_failed", tried, g_pIni);   /* 6.1: that version asks from now on */
        Msg(C_BAD, L2(L"A atualização não instalou (%s). Baixe em %s", L"The update did not install (%s). Download it from %s"), w + 5, UPD_PAGE);
    }
}