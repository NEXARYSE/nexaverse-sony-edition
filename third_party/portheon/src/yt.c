/* yt.c - part of ui.c (6.2): YouTube in the vitrine, through YouTube's own embed player (youtube-nocookie.com, IFrame
 * Player API) in a WebView2 child window laid exactly over the vitrine. Nothing is downloaded or stored: the player
 * streams the video as on any web page that embeds it, and the creator is credited under the panel (title and channel as
 * YouTube's player gives them).
 *  - The list of a game: the player's own links (••• > Vitrine > "Colar link do YouTube", portheon.ini yt_<ID>), then
 *    PORTHEON's videos\youtube.txt (GAME|id|title|channel; '*' = every UFC). Played in order, looping; a video whose
 *    owner turned embedding off (onError 101/150), or that does not start in 12 s, is skipped.
 *  - Sound (the owner: "the edits WITH their music"): on by default (portheon.ini yt_sound), at 60 %, and PORTHEON's own
 *    music pauses while it plays. M / R3 / Settings > Interface > "Som dos vídeos" turn it off.
 *    The browser runs with --autoplay-policy=no-user-gesture-required, so an embed may start with its sound.
 *  - The web view is made, navigated and shown only while the vitrine is on screen, in front and the choice settled
 *    (scrolling the row loads nothing). It shows (6.2 fix) as soon as our page loaded, with its own dark waiting panel
 *    until the video plays: an empty window region made Chromium count it as hidden, and a hidden page never started its
 *    video (only PORTHEON's music went on). A watchdog plays again / skips / falls back when nothing plays. Hidden (another screen, an overlay, a game, PORTHEON
 *    minimized or behind another window, the light effects tier): the video pauses, the page is suspended (TrySuspend),
 *    no sound. Closed (its processes end) when a game starts and after 3 minutes hidden.
 *  - WebView2Loader.dll is loaded at run time: no runtime, no loader, no internet, every video refused -> the vitrine plays
 *    the local video or the live edit, silently. After a failure YouTube is tried again when the internet comes back, or
 *    after 1, 2, 4... minutes (10 at most).
 *  - Never blocks the UI thread: WebView2 works by callbacks on this thread's message loop; the internet check runs in a
 *    worker thread. */
#ifndef DECLSPEC_XFGVIRT
#define DECLSPEC_XFGVIRT(base, func)
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#include "webview2/WebView2.h"
#pragma clang diagnostic pop
#include <netlistmgr.h>

#define WM_APP_YT (WM_APP + 0x74)   /* wp: 1 page message (lp: a malloc'd string), 2 focus back to PORTHEON, 3 restart after a crash */
#define YT_MAXLIST 48
#define YT_MAXCUR 200
typedef HRESULT (STDAPICALLTYPE *YtCreateEnv_t)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions *, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);
typedef struct { int game; wchar_t id[12], title[128], chan[64]; } YtEntry;   /* game: -2 = '*' (every UFC) */
static YtEntry *g_ytCur; static int g_ytNcur = -1;   /* videos\youtube.txt, read once */

static struct {
    HMODULE dll; YtCreateEnv_t create;
    int state;                  /* 0 nothing, 1 the environment is being made, 2 the controller, 3 ready, -1 not available (no runtime) */
    ICoreWebView2Environment *env; ICoreWebView2Controller *ctl; ICoreWebView2 *wv; HWND host;
    int key, wantKey;           /* the game whose list is loaded / wanted (-1 none) */
    int nav;                    /* the page's number: messages of an older page are ignored */
    wchar_t ids[YT_MAXLIST][12]; int n;
    BOOL playing;               /* a video plays (the page said so) */
    wchar_t title[160], chan[96]; /* the credit */
    BOOL shown, revealed, parked, suspended, big, sound;   /* shown: visible to the page; revealed: its pixels show; parked: held + suspended */
    RECT at; int rad, u;        /* where it is (client coordinates), its corner, the UI scale the page has (SC(1000)) */
    int failKey, failN; DWORD failAt;   /* the game whose list did not play: the vitrine falls back (tried again later) */
    volatile LONG net; LONG netSeen; DWORD netAt; volatile LONG netBusy;   /* 1 online, 0 offline, -1 not known yet */
    DWORD hiddenAt, offSince, sndAt; int crashes;
    int sndPref;                /* portheon.ini yt_sound (-1 not read yet) */
    /* 6.2 fix (the owner: "sometimes only the game's music plays and the video never shows"):
       pageOk: the page of g_yt.nav loaded (it shows its own dark waiting panel, so the view is revealed from then on: a
       web view with an empty window region counts as hidden for Chromium and the video never started);
       suspending: a TrySuspend is in flight (its result, not the call, says whether the page sleeps);
       navAt / waitAt / wdStage / wdSkips: the watchdog (no 'ready' 15 s after the page is asked for, or no 'play' 8 s after
       it shows: playVideo again, then the next video, then the fallback); startFails / startAt: a web view that could not
       be made (its folder still held by the browser of a PORTHEON that just closed...) is tried again a few times */
    BOOL pageOk, suspending; DWORD navAt, waitAt; int wdStage, wdSkips; int startFails; DWORD startAt;
} g_yt = {.key = -1, .wantKey = -1, .failKey = -1, .net = -1, .netSeen = -1, .sndPref = -1};
static UINT64 g_ytNavId; static long g_ytNavN = -1;   /* WebView2's id of the navigation to our page g_ytNavN (stale results are told apart) */

static void YtDestroy(void); static void YtStartFailed(HRESULT hr);
static void YtOnMessage(const wchar_t *m);
#ifdef PORTHEON_TEST   /* PORTHEON_TEST_YTLOG=<file>: what the web view does (tests) */
static void YtLog(const wchar_t *fmt, ...)
{
    static wchar_t lp[MAX_PATH]; static int has = -1; if (has < 0) has = GetEnvironmentVariableW(L"PORTHEON_TEST_YTLOG", lp, MAX_PATH) > 0;
    if (!has) return; FILE *f = _wfopen(lp, L"a"); if (!f) return;
    va_list ap; va_start(ap, fmt); fwprintf(f, L"%lu ", GetTickCount()); vfwprintf(f, fmt, ap); fwprintf(f, L"\n"); va_end(ap); fclose(f);
}
#else
#define YtLog(...) ((void)0)
#endif
static void VitOpenFull(void); static void Home62Back(void); static int GameById(const wchar_t *id);
static BOOL YtSoundPref(void) { if (g_yt.sndPref < 0) { if (!PIni(L"yt_sound_v62", 0)) { PIniSet(L"yt_sound_v62", 1); PIniSet(L"yt_sound", 1); } g_yt.sndPref = PIni(L"yt_sound", 1) != 0; } return g_yt.sndPref; }   /* 6.2: once, a mute left by a stray key goes */

/* ---------------------------------------------------------------- the lists */
static BOOL YtIdOk(const wchar_t *s, int n)
{
    if (n != 11) return FALSE;
    for (int i = 0; i < 11; i++) { wchar_t c = s[i]; if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || c == L'-' || c == L'_')) return FALSE; }
    return TRUE;
}
static void YtLoadCurated(void)
{
    if (g_ytNcur >= 0) return;
    g_ytNcur = 0; g_ytCur = (YtEntry *)calloc(YT_MAXCUR, sizeof(YtEntry)); if (!g_ytCur) return;
    wchar_t p[MAX_PATH]; Join(p, g_exeDir, L"videos\\youtube.txt");
    HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); if (h == INVALID_HANDLE_VALUE) return;
    DWORD sz = GetFileSize(h, NULL), got = 0; char *b = sz && sz < (1u << 20) ? (char *)malloc(sz + 1) : NULL;
    if (b && ReadFile(h, b, sz, &got, NULL)) b[got] = 0; else { free(b); b = NULL; }
    CloseHandle(h); if (!b) return;
    int wn = MultiByteToWideChar(CP_UTF8, 0, b, -1, NULL, 0); wchar_t *w = wn > 0 ? (wchar_t *)malloc(sizeof(wchar_t) * wn) : NULL;
    if (w) MultiByteToWideChar(CP_UTF8, 0, b, -1, w, wn);
    free(b); if (!w) return;
    for (wchar_t *ln = w, *nx; ln && *ln && g_ytNcur < YT_MAXCUR; ln = nx) {
        nx = wcschr(ln, L'\n'); if (nx) *nx++ = 0;
        size_t L = wcslen(ln); while (L && (ln[L - 1] == L'\r' || ln[L - 1] == L' ')) ln[--L] = 0;
        if (ln[0] == 0xFEFF) ln++;
        if (!ln[0] || ln[0] == L'#') continue;
        wchar_t *f[4] = {ln, NULL, NULL, NULL}; int k = 1;
        for (wchar_t *q = ln; *q && k < 4; q++) if (*q == L'|') { *q = 0; f[k++] = q + 1; }
        if (k < 2 || !YtIdOk(f[1], (int)wcslen(f[1]))) continue;
        YtEntry *e = &g_ytCur[g_ytNcur]; e->game = !wcscmp(f[0], L"*") ? -2 : GameById(f[0]); if (e->game == -1) continue;
        wcsncpy(e->id, f[1], 11); e->id[11] = 0;
        if (f[2]) { wcsncpy(e->title, f[2], 127); e->title[127] = 0; } if (f[3]) { wcsncpy(e->chan, f[3], 63); e->chan[63] = 0; }
        g_ytNcur++;
    }
    free(w);
}
static BOOL YtUfc(int g) { return g >= 0 && g <= 6; }   /* UFC 1-6 and UFC Undisputed 3: the '*' edits */
static int YtOwnLinks(int g, wchar_t ids[][12], int max)   /* portheon.ini yt_<ID> = id,id,... */
{
    if (g < 0 || g >= NGAMES) return 0;
    wchar_t k[48], v[600]; _snwprintf(k, 48, L"yt_%s", G[g].id); GetPrivateProfileStringW(L"PORTHEON", k, L"", v, 600, g_pIni);
    int n = 0; wchar_t *ctx = NULL; for (wchar_t *t = wcstok(v, L", ;", &ctx); t && n < max; t = wcstok(NULL, L", ;", &ctx)) if (YtIdOk(t, (int)wcslen(t))) { wcsncpy(ids[n], t, 11); ids[n][11] = 0; n++; }
    return n;
}
static int YtList(int g, wchar_t ids[][12], int max)   /* the player's links, the game's trailers, the '*' edits (no id twice) */
{
    if (g < 0 || g >= NGAMES) return 0;
    YtLoadCurated();
    int n = YtOwnLinks(g, ids, max);
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < g_ytNcur && n < max; i++) {
            YtEntry *e = &g_ytCur[i]; if (pass == 0 ? e->game != g : !(e->game == -2 && YtUfc(g))) continue;
            BOOL dup = FALSE; for (int j = 0; j < n; j++) if (!wcscmp(ids[j], e->id)) dup = TRUE;
            if (!dup) { wcscpy(ids[n], e->id); n++; }
        }
    return n;
}
static int YtCount(int g, int *own, int *trailers, int *edits)
{
    wchar_t tmp[YT_MAXLIST][12]; *own = YtOwnLinks(g, tmp, YT_MAXLIST); *trailers = *edits = 0; YtLoadCurated();
    for (int i = 0; i < g_ytNcur; i++) { if (g_ytCur[i].game == g) (*trailers)++; else if (g_ytCur[i].game == -2 && YtUfc(g)) (*edits)++; }
    return *own + *trailers + *edits;
}


/* ---------------------------------------------------------------- the internet (a worker thread: Windows' network list) */
static const GUID YT_CLSID_NLM = {0xdcb00c01, 0x570f, 0x4a9b, {0x8d, 0x69, 0x19, 0x9f, 0xdb, 0xa5, 0x72, 0x3b}};
static const GUID YT_IID_NLM = {0xdcb00000, 0x570f, 0x4a9b, {0x8d, 0x69, 0x19, 0x9f, 0xdb, 0xa5, 0x72, 0x3b}};
static DWORD WINAPI YtNetThread(LPVOID unused)
{
    (void)unused; LONG on = 1;   /* not known: try (the page itself falls back when nothing plays) */
    if (SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
        INetworkListManager *m = NULL;
        if (SUCCEEDED(CoCreateInstance(&YT_CLSID_NLM, NULL, CLSCTX_ALL, &YT_IID_NLM, (void **)&m)) && m) {
            /* 6.2 fix: IsConnectedToInternet says FALSE on many PCs that are online (Windows' internet probe blocked, a
               VPN, a proxy, a DNS of its own: "no internet" / "unknown" in the tray). Only no connection at all is offline:
               with a local link the page is tried and falls back by itself when nothing loads */
            NLM_CONNECTIVITY c = NLM_CONNECTIVITY_DISCONNECTED;
            if (SUCCEEDED(m->lpVtbl->GetConnectivity(m, &c))) on = c != NLM_CONNECTIVITY_DISCONNECTED ? 1 : 0;
            m->lpVtbl->Release(m);
        }
        CoUninitialize();
    }
    InterlockedExchange(&g_yt.net, on); InterlockedExchange(&g_yt.netBusy, 0);
    return 0;
}
static void YtNetCheck(void)   /* at most every 15 s, while YouTube is wanted */
{
    DWORD now = GetTickCount(); if (g_yt.netAt && now - g_yt.netAt < 15000) return;
    if (InterlockedCompareExchange(&g_yt.netBusy, 1, 0)) return;
    g_yt.netAt = now; HANDLE t = CreateThread(NULL, 0, YtNetThread, NULL, 0, NULL); if (t) CloseHandle(t); else { InterlockedExchange(&g_yt.net, 1); InterlockedExchange(&g_yt.netBusy, 0); }
}

/* ---------------------------------------------------------------- the COM callbacks (C objects: a vtable, a count, their IID) */
typedef struct { void *lpVtbl; volatile LONG ref; const IID *iid; } YtCb;
static HRESULT STDMETHODCALLTYPE YtQI(void *This, REFIID riid, void **ppv)
{
    if (!ppv) return E_POINTER;
    YtCb *c = (YtCb *)This;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, c->iid)) { *ppv = This; InterlockedIncrement(&c->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;   /* IMarshal, IAgileObject...: not these objects */
}
static ULONG STDMETHODCALLTYPE YtAddRef(void *This) { return (ULONG)InterlockedIncrement(&((YtCb *)This)->ref); }
static ULONG STDMETHODCALLTYPE YtRelease(void *This) { LONG r = InterlockedDecrement(&((YtCb *)This)->ref); if (r == 0) free(This); return (ULONG)(r < 0 ? 0 : r); }
static void *YtNewCb(void *vtbl, const IID *iid) { YtCb *c = (YtCb *)calloc(1, sizeof(YtCb)); if (c) { c->lpVtbl = vtbl; c->ref = 1; c->iid = iid; } return c; }
#define YT_CB(T, args, ...) \
    static HRESULT STDMETHODCALLTYPE T##_Invoke args __VA_ARGS__ \
    static T##Vtbl T##_vt = {(void *)YtQI, (void *)YtAddRef, (void *)YtRelease, T##_Invoke};
#define YT_NEW(T) YtNewCb(&T##_vt, &IID_##T)

/* the page is served from memory under a made-up https address (RFC 2606 .example; every request to it is answered here,
   so it is never looked up): YouTube's player refuses to play without a referring page (error 153), which a page given
   by NavigateToString does not have */
#define YT_ORIGIN L"https://portheon.example/"
static BOOL YtOurs(const wchar_t *u) { return u && !wcsncmp(u, YT_ORIGIN, wcslen(YT_ORIGIN)); }
static char *g_ytPage; static int g_ytPageLen;   /* the page, UTF-8 */

static void YtControllerMade(HRESULT hr, ICoreWebView2Controller *c);
static void YtEnvMade(HRESULT hr, ICoreWebView2Environment *e);
YT_CB(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This, HRESULT hr, ICoreWebView2Environment *e), { (void)This; YtEnvMade(hr, e); return S_OK; })
YT_CB(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This, HRESULT hr, ICoreWebView2Controller *c), { (void)This; YtControllerMade(hr, c); return S_OK; })
YT_CB(ICoreWebView2WebMessageReceivedEventHandler, (ICoreWebView2WebMessageReceivedEventHandler *This, ICoreWebView2 *s, ICoreWebView2WebMessageReceivedEventArgs *a), {
    (void)This; (void)s; LPWSTR m = NULL, src = NULL;
    if (a && SUCCEEDED(a->lpVtbl->get_Source(a, &src)) && YtOurs(src)   /* only PORTHEON's own page talks to PORTHEON */
        && SUCCEEDED(a->lpVtbl->TryGetWebMessageAsString(a, &m)) && m) { wchar_t *c = _wcsdup(m); if (c && !PostMessageW(g_wnd, WM_APP_YT, 1, (LPARAM)c)) free(c); }
    if (m) CoTaskMemFree(m); if (src) CoTaskMemFree(src);
    return S_OK; })
YT_CB(ICoreWebView2NavigationStartingEventHandler, (ICoreWebView2NavigationStartingEventHandler *This, ICoreWebView2 *s, ICoreWebView2NavigationStartingEventArgs *a), {
    (void)This; (void)s; LPWSTR u = NULL;   /* the top frame stays PORTHEON's page (a _top link of the embed goes nowhere) */
    if (a && SUCCEEDED(a->lpVtbl->get_Uri(a, &u)) && !YtOurs(u)) { a->lpVtbl->put_Cancel(a, TRUE); YtLog(L"navigation away cancelled"); }
    else if (u) {   /* 6.2 fix: which of our pages this navigation is (its result is told apart from an older page's) */
        const wchar_t *q = wcsstr(u, L"?n="); UINT64 id = 0;
        if (q && SUCCEEDED(a->lpVtbl->get_NavigationId(a, &id))) { g_ytNavId = id; g_ytNavN = wcstol(q + 3, NULL, 10); }
    }
    if (u) CoTaskMemFree(u);
    return S_OK; })
YT_CB(ICoreWebView2FocusChangedEventHandler, (ICoreWebView2FocusChangedEventHandler *This, ICoreWebView2Controller *s, IUnknown *a), { (void)This; (void)s; (void)a; PostMessageW(g_wnd, WM_APP_YT, 2, 0); return S_OK; })
YT_CB(ICoreWebView2NewWindowRequestedEventHandler, (ICoreWebView2NewWindowRequestedEventHandler *This, ICoreWebView2 *s, ICoreWebView2NewWindowRequestedEventArgs *a), { (void)This; (void)s; if (a) a->lpVtbl->put_Handled(a, TRUE); return S_OK; })
YT_CB(ICoreWebView2NavigationCompletedEventHandler, (ICoreWebView2NavigationCompletedEventHandler *This, ICoreWebView2 *s, ICoreWebView2NavigationCompletedEventArgs *a), {
    (void)This; (void)s; BOOL ok = FALSE; COREWEBVIEW2_WEB_ERROR_STATUS es = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN; UINT64 id = 0;
    if (a) { a->lpVtbl->get_IsSuccess(a, &ok); if (!ok) a->lpVtbl->get_WebErrorStatus(a, &es); if (FAILED(a->lpVtbl->get_NavigationId(a, &id))) id = 0; }
    long n = id && id == g_ytNavId ? g_ytNavN : -1;   /* 6.2 fix: -1 = not the page asked for last (a stale result changes nothing) */
    YtLog(L"nav done ok=%d status=%d n=%ld", ok, (int)es, n);
    if (n >= 0) {
        if (ok) PostMessageW(g_wnd, WM_APP_YT, 5, (LPARAM)n);   /* our page loaded: it may show (its dark waiting panel) */
        else if (es != COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED) PostMessageW(g_wnd, WM_APP_YT, 4, (LPARAM)n);   /* it did not load: as 'fail' */
    }
    return S_OK; })
YT_CB(ICoreWebView2ProcessFailedEventHandler, (ICoreWebView2ProcessFailedEventHandler *This, ICoreWebView2 *s, ICoreWebView2ProcessFailedEventArgs *a), {
    (void)This; (void)s; COREWEBVIEW2_PROCESS_FAILED_KIND k = COREWEBVIEW2_PROCESS_FAILED_KIND_UNKNOWN_PROCESS_EXITED;
    if (a) a->lpVtbl->get_ProcessFailedKind(a, &k);
    YtLog(L"process failed kind=%d", (int)k);
    /* only the browser or the page's own renderer gone needs a new web view (a GPU or iframe process WebView2 brings back itself) */
    if (k == COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED || k == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED) PostMessageW(g_wnd, WM_APP_YT, 3, 0);
    return S_OK; })
YT_CB(ICoreWebView2WebResourceRequestedEventHandler, (ICoreWebView2WebResourceRequestedEventHandler *This, ICoreWebView2 *s, ICoreWebView2WebResourceRequestedEventArgs *a), {
    (void)This; (void)s; ICoreWebView2WebResourceRequest *rq = NULL; LPWSTR uri = NULL;
    if (a && g_yt.env && SUCCEEDED(a->lpVtbl->get_Request(a, &rq)) && rq && SUCCEEDED(rq->lpVtbl->get_Uri(rq, &uri)) && YtOurs(uri)) {
        BOOL page = g_ytPage && !wcsncmp(uri + wcslen(YT_ORIGIN), L"vitrine", 7);   /* anything else (favicon.ico...): 404, from here too */
        IStream *st = page ? SHCreateMemStream((const BYTE *)g_ytPage, (UINT)g_ytPageLen) : NULL; ICoreWebView2WebResourceResponse *rs = NULL;
        if (SUCCEEDED(g_yt.env->lpVtbl->CreateWebResourceResponse(g_yt.env, st, page ? 200 : 404, page ? L"OK" : L"Not Found",
                page ? L"Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store" : L"Cache-Control: no-store", &rs)) && rs) { a->lpVtbl->put_Response(a, rs); rs->lpVtbl->Release(rs); }
        if (st) st->lpVtbl->Release(st);
    }
    if (uri) CoTaskMemFree(uri); if (rq) rq->lpVtbl->Release(rq);
    return S_OK; })
static void YtSuspendDone(HRESULT hr, BOOL ok);
YT_CB(ICoreWebView2TrySuspendCompletedHandler, (ICoreWebView2TrySuspendCompletedHandler *This, HRESULT hr, BOOL ok), { (void)This; YtSuspendDone(hr, ok); return S_OK; })

/* ---------------------------------------------------------------- making it (all asynchronous) */
static LRESULT CALLBACK YtHostProc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) { PAINTSTRUCT ps; HDC dc = BeginPaint(w, &ps); HBRUSH b = CreateSolidBrush(RGB0(10, 10, 14)); FillRect(dc, &ps.rcPaint, b); DeleteObject(b); EndPaint(w, &ps); return 0; }
    if (m == WM_SIZE && g_yt.ctl) { RECT r = {0, 0, LOWORD(lp), HIWORD(lp)}; g_yt.ctl->lpVtbl->put_Bounds(g_yt.ctl, r); }
    return DefWindowProcW(w, m, wp, lp);
}
static void YtStart(void)
{
    if (g_yt.state) return;
    if (!g_yt.dll) {
        wchar_t p[MAX_PATH]; Join(p, g_exeDir, L"WebView2Loader.dll");
        g_yt.dll = LoadLibraryExW(p, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (g_yt.dll) g_yt.create = (YtCreateEnv_t)(void *)GetProcAddress(g_yt.dll, "CreateCoreWebView2EnvironmentWithOptions");
    }
    if (!g_yt.create) { g_yt.state = -1; return; }
    {   /* the edits play with their music: an embed may start with sound (no click needed). WebView2 reads this variable
           as its AdditionalBrowserArguments (the C API has no options object; the loader's own variable does the same) */
        static const wchar_t ap[] = L"--autoplay-policy=no-user-gesture-required"; wchar_t a[1024] = L"";
        GetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", a, 900); a[899] = 0;
        BOOL ch = FALSE;
        if (!wcsstr(a, L"--autoplay-policy")) { if (a[0]) wcscat(a, L" "); wcscat(a, ap); ch = TRUE; }
        /* 6.2 fix: Chromium's own guess of whether the window is covered (it counts a child window clipped by its region,
           or one under a layered sibling, as hidden, and a hidden page does not start its video). PORTHEON says itself
           when the view is visible (put_IsVisible) and parks it when it is not, so the guess is turned off. (One
           --disable-features only: a second one would replace the first, so one the PC already has is left alone.) */
        if (!wcsstr(a, L"--disable-features")) { if (a[0]) wcscat(a, L" "); wcscat(a, L"--disable-features=CalculateNativeWinOcclusion"); ch = TRUE; }
        if (ch) SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", a);
    }
    wchar_t udf[MAX_PATH]; LocalData(udf, L"PORTHEON\\webview2");
    void *cb = YT_NEW(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler); if (!cb) { g_yt.state = -1; return; }
    g_yt.state = 1;
    HRESULT hr = g_yt.create(NULL, udf, NULL, (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)cb);
    YtRelease(cb); YtLog(L"create env hr=0x%08lX udf=%s", (unsigned long)hr, udf);
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) g_yt.state = -1;   /* no WebView2 runtime on this PC: the vitrine stays as it was */
    else if (FAILED(hr)) YtStartFailed(hr);
}
/* 6.2 fix: the web view could not be made (the folder still held by the browser of the PORTHEON that closed for the game,
   a busy runtime update...): tried again after 5, 10, 20 s; after 4 failures not in this session */
static void YtStartFailed(HRESULT hr)
{
    YtDestroy(); g_yt.startFails++; g_yt.startAt = GetTickCount(); YtLog(L"start failed hr=0x%08lX (%d)", (unsigned long)hr, g_yt.startFails);
    g_yt.state = g_yt.startFails >= 4 ? -1 : 0;
}
static void YtEnvMade(HRESULT hr, ICoreWebView2Environment *e)
{
    YtLog(L"env made hr=0x%08lX state=%d", (unsigned long)hr, g_yt.state);
    if (g_yt.state != 1) return;   /* closed meanwhile */
    if (FAILED(hr) || !e) { YtStartFailed(hr); return; }
    g_yt.env = e; e->lpVtbl->AddRef(e);
    static BOOL reg; if (!reg) { WNDCLASSW wc = {0}; wc.lpfnWndProc = YtHostProc; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"PortheonYtHost"; wc.hCursor = LoadCursor(NULL, IDC_HAND); reg = RegisterClassW(&wc) != 0; }
    g_yt.host = CreateWindowExW(0, L"PortheonYtHost", L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 16, 9, g_wnd, NULL, GetModuleHandleW(NULL), NULL);
    if (g_yt.host) SetWindowRgn(g_yt.host, CreateRectRgn(0, 0, 0, 0), FALSE);   /* nothing of it shows until a video plays */
    g_yt.revealed = FALSE; SetRectEmpty(&g_yt.at);
    void *cb = g_yt.host ? YT_NEW(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) : NULL;
    if (!cb || FAILED(e->lpVtbl->CreateCoreWebView2Controller(e, g_yt.host, (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *)cb))) { if (cb) YtRelease(cb); YtStartFailed(E_FAIL); return; }
    YtRelease(cb); g_yt.state = 2;
}
static void YtControllerMade(HRESULT hr, ICoreWebView2Controller *c)
{
    YtLog(L"controller made hr=0x%08lX state=%d", (unsigned long)hr, g_yt.state);
    if (g_yt.state != 2) return;
    if (FAILED(hr) || !c) { YtStartFailed(hr); return; }
    g_yt.ctl = c; c->lpVtbl->AddRef(c);
    if (FAILED(c->lpVtbl->get_CoreWebView2(c, &g_yt.wv)) || !g_yt.wv) { YtStartFailed(E_FAIL); return; }
    g_yt.startFails = 0;
    ICoreWebView2 *wv = g_yt.wv; EventRegistrationToken tk;
    ICoreWebView2Settings *st = NULL;
    if (SUCCEEDED(wv->lpVtbl->get_Settings(wv, &st)) && st) {
        st->lpVtbl->put_AreDefaultContextMenusEnabled(st, FALSE); st->lpVtbl->put_AreDevToolsEnabled(st, FALSE); st->lpVtbl->put_IsStatusBarEnabled(st, FALSE);
        st->lpVtbl->put_IsZoomControlEnabled(st, FALSE); st->lpVtbl->put_AreDefaultScriptDialogsEnabled(st, FALSE); st->lpVtbl->put_IsBuiltInErrorPageEnabled(st, FALSE);
        ICoreWebView2Settings3 *s3 = NULL; if (SUCCEEDED(st->lpVtbl->QueryInterface(st, &IID_ICoreWebView2Settings3, (void **)&s3)) && s3) { s3->lpVtbl->put_AreBrowserAcceleratorKeysEnabled(s3, FALSE); s3->lpVtbl->Release(s3); }
        st->lpVtbl->Release(st);
    }
    {   ICoreWebView2Controller2 *c2 = NULL; if (SUCCEEDED(c->lpVtbl->QueryInterface(c, &IID_ICoreWebView2Controller2, (void **)&c2)) && c2) { COREWEBVIEW2_COLOR k = {255, 10, 10, 14}; c2->lpVtbl->put_DefaultBackgroundColor(c2, k); c2->lpVtbl->Release(c2); } }
    {   ICoreWebView2Controller3 *c3 = NULL;   /* CSS pixels = the screen's pixels: the page's sizes come from PORTHEON's own scale */
        if (SUCCEEDED(c->lpVtbl->QueryInterface(c, &IID_ICoreWebView2Controller3, (void **)&c3)) && c3) { c3->lpVtbl->put_ShouldDetectMonitorScaleChanges(c3, FALSE); c3->lpVtbl->put_RasterizationScale(c3, 1.0); c3->lpVtbl->Release(c3); } }
    void *cb;
    if ((cb = YT_NEW(ICoreWebView2WebMessageReceivedEventHandler))) { wv->lpVtbl->add_WebMessageReceived(wv, (ICoreWebView2WebMessageReceivedEventHandler *)cb, &tk); YtRelease(cb); }
    if ((cb = YT_NEW(ICoreWebView2NavigationStartingEventHandler))) { wv->lpVtbl->add_NavigationStarting(wv, (ICoreWebView2NavigationStartingEventHandler *)cb, &tk); YtRelease(cb); }
    if ((cb = YT_NEW(ICoreWebView2NewWindowRequestedEventHandler))) { wv->lpVtbl->add_NewWindowRequested(wv, (ICoreWebView2NewWindowRequestedEventHandler *)cb, &tk); YtRelease(cb); }
    if ((cb = YT_NEW(ICoreWebView2NavigationCompletedEventHandler))) { wv->lpVtbl->add_NavigationCompleted(wv, (ICoreWebView2NavigationCompletedEventHandler *)cb, &tk); YtRelease(cb); }
    if ((cb = YT_NEW(ICoreWebView2ProcessFailedEventHandler))) { wv->lpVtbl->add_ProcessFailed(wv, (ICoreWebView2ProcessFailedEventHandler *)cb, &tk); YtRelease(cb); }
    if ((cb = YT_NEW(ICoreWebView2FocusChangedEventHandler))) { c->lpVtbl->add_GotFocus(c, (ICoreWebView2FocusChangedEventHandler *)cb, &tk); YtRelease(cb); }
    wv->lpVtbl->AddWebResourceRequestedFilter(wv, YT_ORIGIN L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);   /* every request to it, not only the page */
    if ((cb = YT_NEW(ICoreWebView2WebResourceRequestedEventHandler))) { wv->lpVtbl->add_WebResourceRequested(wv, (ICoreWebView2WebResourceRequestedEventHandler *)cb, &tk); YtRelease(cb); }
#ifdef PORTHEON_TEST   /* tests while the person at the PC works: the page's sound logic runs (the log shows muted=0 vol=60), the speakers stay silent */
    if (TestQuiet()) { ICoreWebView2_8 *w8 = NULL; if (SUCCEEDED(wv->lpVtbl->QueryInterface(wv, &IID_ICoreWebView2_8, (void **)&w8)) && w8) { w8->lpVtbl->put_IsMuted(w8, TRUE); w8->lpVtbl->Release(w8); } }
#endif
    {   /* 6.2 (the owner: no play/pause icon in the middle of the video): YouTube draws its icons inside its own frame, over the
           running video, where the page's cover cannot help. This runs in every child frame: nothing there is visible but the <video>
           itself, whatever YouTube names its elements (a list of class names missed one). */
        static wchar_t js[4000]; const wchar_t *top = L"", *dump = L"", *mark = L"", *css = L"html,body{background:#000!important}body *{visibility:hidden!important}video{visibility:visible!important}video::-webkit-media-controls{display:none!important}";
#ifdef PORTHEON_TEST   /* PORTHEON_TEST_YTMARK: the video's colours turn. PORTHEON_TEST_YTDUMP: what is visible at the player's centre goes to the log. PORTHEON_TEST_YTNOHIDE: nothing hidden */
        if (GetEnvironmentVariableW(L"PORTHEON_TEST_YTMARK", NULL, 0)) mark = L"video{filter:hue-rotate(180deg)}";
        if (GetEnvironmentVariableW(L"PORTHEON_TEST_YTNOHIDE", NULL, 0)) css = L"";
        if (GetEnvironmentVariableW(L"PORTHEON_TEST_YTDUMP", NULL, 0)) {
            top = L"if(top===self)addEventListener('message',function(e){if((''+e.data).slice(0,6)=='ytdbg ')try{chrome.webview.postMessage(N+' '+e.data)}catch(x){}});";
            dump = L"setInterval(function(){var o=[],cx=innerWidth/2,cy=innerHeight/2;document.querySelectorAll('*').forEach(function(e){if(e.tagName=='VIDEO'||e==document.body||e==document.documentElement)return;var s=getComputedStyle(e);if(s.display=='none'||s.visibility!='visible'||+s.opacity==0)return;var r=e.getBoundingClientRect();if(r.width>4&&r.height>4&&r.width<300&&Math.abs(r.left+r.width/2-cx)<70&&Math.abs(r.top+r.height/2-cy)<70)o.push(e.tagName+'.'+(e.getAttribute('class')||'')+'/'+s.opacity)});var m=o.join(' | ');if(m!=window.__l){window.__l=m;parent.postMessage('ytdbg '+location.hostname+' center: '+m,'*')}},150);";
        }
#endif
        _snwprintf(js, 4000, L"(function(){%s if(top===self)return;var c='%s%s';function a(){var r=document.documentElement;if(!r){setTimeout(a,0);return}var s=document.createElement('style');s.textContent=c;(document.head||r).appendChild(s)}a();%s})()", top, css, mark, dump); js[3999] = 0;
        wv->lpVtbl->AddScriptToExecuteOnDocumentCreated(wv, js, NULL);
    }
    c->lpVtbl->put_IsVisible(c, FALSE);
    g_yt.state = 3; g_yt.key = -1; g_yt.shown = g_yt.suspended = g_yt.suspending = g_yt.revealed = g_yt.pageOk = FALSE; g_yt.parked = TRUE;
}
static void YtDestroy(void)
{
    if (g_yt.ctl) { g_yt.ctl->lpVtbl->Close(g_yt.ctl); }
    if (g_yt.wv) { g_yt.wv->lpVtbl->Release(g_yt.wv); g_yt.wv = NULL; }
    if (g_yt.ctl) { g_yt.ctl->lpVtbl->Release(g_yt.ctl); g_yt.ctl = NULL; }
    if (g_yt.env) { g_yt.env->lpVtbl->Release(g_yt.env); g_yt.env = NULL; }
    if (g_yt.host) { DestroyWindow(g_yt.host); g_yt.host = NULL; }
    if (g_yt.state > 0) g_yt.state = 0;
    BOOL had = g_yt.playing && g_yt.revealed;
    g_yt.key = -1; g_yt.playing = g_yt.shown = g_yt.suspended = g_yt.suspending = g_yt.revealed = g_yt.parked = g_yt.pageOk = FALSE; g_yt.title[0] = g_yt.chan[0] = 0; SetRectEmpty(&g_yt.at);
    g_yt.waitAt = 0; g_yt.wdStage = g_yt.wdSkips = 0; g_ytNavN = -1;
    g_yt.sound = FALSE; g_ytSound = FALSE; g_yt.sndAt = 0;
    if (had && g_wnd) InvalidateRect(g_wnd, NULL, FALSE);
}

/* ---------------------------------------------------------------- the page (built in memory) */
static void YtPost(const wchar_t *m) { if (g_yt.wv) g_yt.wv->lpVtbl->PostWebMessageAsString(g_yt.wv, m); }
static void YtPostR(void) { wchar_t m[24]; _snwprintf(m, 24, L"r %d", g_yt.rad > 0 ? g_yt.rad - 1 : 0); YtPost(m); }
static void YtPostU(void) { wchar_t m[32]; g_yt.u = SC(1000); _snwprintf(m, 32, L"u %.3f", g_yt.u / 1000.0); YtPost(m); }
static void YtCat(wchar_t **o, size_t *left, const wchar_t *s) { size_t n = wcslen(s); if (n >= *left) n = *left - 1; wmemcpy(*o, s, n); *o += n; *left -= n; **o = 0; }
static void YtResume(void)   /* 6.2 fix: always (it is a no-op on a page that is awake): the old flag was set when TrySuspend
                                 was only asked, so a suspend that finished after a Resume left the page asleep for good */
{
    g_yt.suspended = FALSE;
    if (!g_yt.wv) return;
    ICoreWebView2_3 *w3 = NULL; if (SUCCEEDED(g_yt.wv->lpVtbl->QueryInterface(g_yt.wv, &IID_ICoreWebView2_3, (void **)&w3)) && w3) { w3->lpVtbl->Resume(w3); w3->lpVtbl->Release(w3); }
}
static void YtSuspendDone(HRESULT hr, BOOL ok)   /* TrySuspend's result: the page sleeps only now; shown again meanwhile = awake */
{
    g_yt.suspending = FALSE; g_yt.suspended = SUCCEEDED(hr) && ok && g_yt.wv; YtLog(L"suspend done hr=0x%08lX ok=%d shown=%d", (unsigned long)hr, ok, g_yt.shown);
    if (g_yt.suspended && (g_yt.shown || !g_yt.parked)) YtResume();
}
static void YtNavigate(void)
{
    static wchar_t page[20000]; wchar_t *o = page, num[160]; size_t left = 20000; page[0] = 0;
    YtResume(); g_yt.parked = FALSE;   /* a suspended web view would not finish the navigation */
    g_yt.nav++; g_yt.pageOk = FALSE; g_yt.navAt = GetTickCount(); g_yt.waitAt = 0; g_yt.wdStage = g_yt.wdSkips = 0;
    YtCat(&o, &left, L"<!doctype html><html><head><meta charset=utf-8><meta name=referrer content=strict-origin-when-cross-origin><style>"
        L"html,body{margin:0;width:100%;height:100%;overflow:hidden;background:#0a0a0e}"
        /* 6.2 (the owner: no video title on top): the player is 50% taller than the panel and centred, so the video (16:9,
           as wide as the panel) fills it while YouTube's title bar above it and its logo below it fall outside */
        L"#f{position:absolute;left:0;top:-25%;width:100%;height:150%;border:0;pointer-events:none}"
        /* 6.2 (the owner: no YouTube play/start button, more professional): #w, the clip's own still (blurred, dark), covers the
           player whenever a video is not really running: loading, buffering, paused, between two clips. YouTube's big play
           button, spinner, pause screen and end screen only ever exist under it. It leaves with a fade, once frames advance. */
        L"#w{position:absolute;inset:0;overflow:hidden;background:#0a0a0e}"
        L"body.on #w{opacity:0;transition:opacity .45s ease-out}body.fade #w{transition:opacity .3s ease-in}"
        L"#b{position:absolute;inset:-14px;background:center/cover no-repeat;filter:blur(7px) brightness(.58) saturate(.92);opacity:0;transition:opacity .35s}#b.ok{opacity:1}"
        /* the panel's edge, drawn here so it is anti-aliased: outside the arc a dark ring hides the window region's stepped corner */
        L"#fr{position:absolute;inset:1px;border-radius:calc(var(--r)*1px);pointer-events:none;box-shadow:0 0 0 14px #08080b,inset 0 0 0 1px rgba(255,255,255,.13),inset 0 0 70px rgba(0,0,0,.30)}"
        L"#c{position:absolute;inset:0;cursor:pointer}"
        L"</style>");   /* 6.2 (the owner: "only the video"): no speaker on the picture; M / R3 and Settings > Interface */
    _snwprintf(num, 160, L"<style>:root{--u:%.3f;--r:%d}</style></head><body class=%s>", SC(1000) / 1000.0, g_yt.rad > 0 ? g_yt.rad - 1 : 0, YtSoundPref() ? L"''" : L"m"); YtCat(&o, &left, num);
    g_yt.u = SC(1000);
    YtCat(&o, &left, L"<iframe id=f allow=\"autoplay; encrypted-media\" src=\"https://www.youtube-nocookie.com/embed/");
    YtCat(&o, &left, g_yt.ids[0]);
    YtCat(&o, &left, L"?autoplay=1&mute=1&controls=0&playsinline=1&rel=0&modestbranding=1&enablejsapi=1&iv_load_policy=3&disablekb=1&fs=0&origin=https%3A%2F%2Fportheon.example\"></iframe>"
        L"<div id=w><div id=b></div></div><div id=fr></div><div id=c></div><script>");   /* 6.2: nothing on the video (the owner's call) */
    _snwprintf(num, 160, L"var N=%d,snd=%d,V=[", g_yt.nav, YtSoundPref() ? 1 : 0); YtCat(&o, &left, num);
    for (int i = 0; i < g_yt.n; i++) { YtCat(&o, &left, i ? L",'" : L"'"); YtCat(&o, &left, g_yt.ids[i]); YtCat(&o, &left, L"'"); }
    YtCat(&o, &left, L"],i=0,p=null,bad=0,wd=0,held=0,on=0,live=0,t0=-1,lock=0,bt=0,ut=0,cp='',B=document.body,W=document.getElementById('b');"
        L"function post(m){try{window.chrome.webview.postMessage(N+' '+m)}catch(e){}}"
        L"function aps(){B.classList.toggle('m',!snd);if(p&&p.mute){if(snd){p.unMute();p.setVolume(60)}else p.mute()}}"
        L"function arm(){clearTimeout(wd);if(!held)wd=setTimeout(function(){skip('timeout')},12000)}"
        L"function pic(){var u='https://i.ytimg.com/vi/'+V[i]+'/hqdefault.jpg';if(cp==u)return;cp=u;var m=new Image();m.onload=function(){if(cp==u){W.style.backgroundImage='url('+u+')';W.classList.add('ok')}};m.src=u}"
        L"function cover(){on=0;live=0;t0=-1;clearTimeout(bt);B.classList.remove('on')}"
        L"function skip(w){post('err '+i+' '+w);bad++;if(bad>=V.length){post('fail');clearTimeout(wd);return}nx()}"
        L"function nx(){i=(i+1)%V.length;cover();pic();arm();p.loadVideoById(V[i])}"
        L"function show(){on=1;clearTimeout(wd);bad=0;B.classList.add('on');var d=p.getVideoData?p.getVideoData():{};post('play '+i+'\t'+(d.title||'')+'\t'+(d.author||''))}"
        L"function st(e){var s=e.data;if(s==1){clearTimeout(bt);if(held){p.pauseVideo();return}aps();live=1}"
        L"else if(s==0){if(!lock){cover();post('next');nx()}}"
        L"else if(s==2){live=0;if(held)return;if(on)cover();arm();p.playVideo()}"
        L"else if(s==3){live=0;if(on){clearTimeout(bt);bt=setTimeout(function(){cover();arm()},700)}}}"
        /* frames really advance: the cover leaves. 1.4 s before the end the next clip takes over: YouTube's end screen is never reached */
        L"setInterval(function(){if(!p||!live||held||lock||!p.getCurrentTime)return;var c=p.getCurrentTime(),d=p.getDuration();"
        L"if(!on){if(t0<0)t0=c;else if(c-t0>=.3)show()}"
        L"else if(d>5&&d-c<1.4){lock=1;on=0;live=0;B.classList.add('fade');B.classList.remove('on');setTimeout(function(){B.classList.remove('fade');lock=0;post('next');nx()},320)}},100);"
        L"setInterval(function(){if(p&&p.isMuted&&live&&!held&&snd&&(p.isMuted()||p.getVolume()<60)){p.unMute();p.setVolume(60)}},800);"   /* 6.2: YouTube can bring back the embed's mute=1 (a new video, a hold): the sound is asked for again */
        L"pic();window.onYouTubeIframeAPIReady=function(){p=new YT.Player('f',{events:{onReady:function(){aps();if(!held)p.playVideo()},onStateChange:st,"
        L"onError:function(e){clearTimeout(wd);skip(e.data)}}});arm()};"
        L"var s=document.createElement('script');s.src='https://www.youtube.com/iframe_api';s.onerror=function(){post('fail')};document.head.appendChild(s);"
        L"setTimeout(function(){if(!p&&!held)post('fail')},15000);"
        L"document.getElementById('c').onclick=function(){post('click')};"
        L"document.addEventListener('keydown',function(e){post('key '+e.key);e.preventDefault()});"
        L"window.chrome.webview.addEventListener('message',function(e){var d=''+e.data;"
        L"if(d=='hold'){held=1;clearTimeout(wd);cover();if(p&&p.pauseVideo)p.pauseVideo()}"
        L"else if(d=='go'){held=0;if(p&&p.playVideo){p.playVideo();if(!on)arm()}}"
        L"else if(d=='skip'){if(p&&p.loadVideoById){clearTimeout(wd);skip('watchdog')}else post('fail')}"
        L"else if(d=='snd1'||d=='snd0'){snd=d=='snd1'?1:0;aps()}"
        L"else if(d.slice(0,2)=='u '){document.documentElement.style.setProperty('--u',d.slice(2))}"
        L"else if(d.slice(0,2)=='r '){document.documentElement.style.setProperty('--r',d.slice(2))}"
        L"else if(d=='stat'){post('stat '+(p&&p.getPlayerState?p.getPlayerState():-9)+' '+(p&&p.getCurrentTime?Math.round(p.getCurrentTime()):-1)+' muted='+(p&&p.isMuted?p.isMuted():-1)+' vol='+(p&&p.getVolume?p.getVolume():-1)+' '+innerWidth+'x'+innerHeight)}});"
        L"post('ready')</script></body></html>");
    g_yt.playing = FALSE; g_yt.title[0] = g_yt.chan[0] = 0;
    {   int n = WideCharToMultiByte(CP_UTF8, 0, page, -1, NULL, 0, NULL, NULL); char *u = n > 0 ? (char *)malloc(n) : NULL;
        if (u) { WideCharToMultiByte(CP_UTF8, 0, page, -1, u, n, NULL, NULL); free(g_ytPage); g_ytPage = u; g_ytPageLen = n - 1; } }
    wchar_t url[96]; _snwprintf(url, 96, YT_ORIGIN L"vitrine?n=%d", g_yt.nav);
    HRESULT hr = g_ytPage ? g_yt.wv->lpVtbl->Navigate(g_yt.wv, url) : E_OUTOFMEMORY; YtLog(L"navigate key=%d n=%d len=%d hr=0x%08lX first=%s last=%s", g_yt.key, g_yt.n, g_ytPageLen, (unsigned long)hr, g_yt.ids[0], g_yt.ids[g_yt.n - 1]);
    if (FAILED(hr)) PostMessageW(g_wnd, WM_APP_YT, 4, 0);
}

/* ---------------------------------------------------------------- showing and hiding */
static void YtCreditInval(void) { if (g_wnd && !IsRectEmpty(&g_yt.at)) { RECT r = g_yt.at; InflateRect(&r, SC(8), SC(44)); InvalidateRect(g_wnd, &r, FALSE); } }
static void YtRegion(void)   /* the window's pixels: none until our page loaded, then the rounded panel (6.2 fix: the page's own
                                 dark waiting panel shows until the video plays; an empty region kept Chromium from starting it) */
{
    if (!g_yt.host) return;
    int w = g_yt.at.right - g_yt.at.left, h = g_yt.at.bottom - g_yt.at.top;
    if (!g_yt.revealed || w <= 0) SetWindowRgn(g_yt.host, CreateRectRgn(0, 0, 0, 0), TRUE);
    else if (g_yt.rad > 0) SetWindowRgn(g_yt.host, CreateRoundRectRgn(0, 0, w + 1, h + 1, g_yt.rad * 2, g_yt.rad * 2), TRUE);
    else SetWindowRgn(g_yt.host, NULL, TRUE);
}
static void YtReveal(BOOL on)
{
    on = on && g_yt.shown && (g_yt.playing || g_yt.pageOk);
    if (on == g_yt.revealed) return;
    g_yt.revealed = on; YtRegion(); YtLog(L"reveal %d", on);
    if (g_wnd) { InvalidateRect(g_wnd, &g_yt.at, FALSE); YtCreditInval(); }
}
static void YtSyncSound(void)   /* the page's sound, and PORTHEON's music (paused while a video plays with its sound) */
{
    BOOL want = YtSoundPref() && g_yt.shown && g_yt.state == 3;
    if (want != g_yt.sound) { g_yt.sound = want; YtPost(want ? L"snd1" : L"snd0"); }
    DWORD now = GetTickCount();
    if (want && g_yt.playing) g_yt.sndAt = now;
    /* the music stays off between two videos of the list (3 s) and through a short hide (the full-screen zoom: 1.2 s) */
    BOOL mus = YtSoundPref() && g_yt.sndAt && now - g_yt.sndAt < (want ? 3000u : 1200u);
    if (mus != g_ytSound) { g_ytSound = mus; YtLog(L"music %s", mus ? L"paused" : L"back"); }
}
static void YtPark(void)   /* the page holds its video and sleeps (its renderer is frozen) */
{
    YtPost(L"hold");
    if (g_yt.ctl) g_yt.ctl->lpVtbl->put_IsVisible(g_yt.ctl, FALSE);
    ICoreWebView2_3 *w3 = NULL;
    if (!g_yt.suspended && !g_yt.suspending && g_yt.wv && SUCCEEDED(g_yt.wv->lpVtbl->QueryInterface(g_yt.wv, &IID_ICoreWebView2_3, (void **)&w3)) && w3) {
        void *cb = YT_NEW(ICoreWebView2TrySuspendCompletedHandler);   /* 6.2 fix: asked, not done: YtSuspendDone says */
        if (cb) { if (SUCCEEDED(w3->lpVtbl->TrySuspend(w3, (ICoreWebView2TrySuspendCompletedHandler *)cb))) g_yt.suspending = TRUE; YtRelease(cb); }
        w3->lpVtbl->Release(w3);
    }
    g_yt.parked = TRUE;
}
static void YtHide(void)
{
    if (!g_yt.shown && (g_yt.parked || !g_yt.wv)) return;
    g_yt.shown = FALSE; g_yt.playing = FALSE; g_yt.hiddenAt = GetTickCount(); YtLog(L"hide");   /* 6.2 fix: not playing until the page says so again: the watchdog also covers the resume */
    YtReveal(FALSE); YtSyncSound();
    if (g_yt.host) ShowWindow(g_yt.host, SW_HIDE);
    YtPark();
}
static void YtPlace(RECT r, int rad)
{
    if (EqualRect(&r, &g_yt.at) && rad == g_yt.rad) return;
    if (rad != g_yt.rad) { g_yt.rad = rad; YtPostR(); }   /* 6.2: the page draws the panel's edge */
    g_yt.at = r;
    SetWindowPos(g_yt.host, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE);
    YtRegion();
    if (g_yt.ctl) { RECT b = {0, 0, r.right - r.left, r.bottom - r.top}; g_yt.ctl->lpVtbl->put_Bounds(g_yt.ctl, b); }
}
static void YtShow(RECT r, int rad)
{
    YtPlace(r, rad);
    if (g_yt.u != SC(1000)) YtPostU();   /* the UI scale changed: the page's tag and button follow */
    if (g_yt.shown) return;
    g_yt.parked = FALSE;
    YtLog(L"show %ld,%ld %ldx%ld page=%d", r.left, r.top, r.right - r.left, r.bottom - r.top, g_yt.pageOk);
    ShowWindow(g_yt.host, SW_SHOWNA);
    if (g_yt.ctl) g_yt.ctl->lpVtbl->put_IsVisible(g_yt.ctl, TRUE);
    YtResume();   /* 6.2 fix: visible first, then awake, then 'go' (TrySuspend refuses a visible view; Resume after it) */
    g_yt.shown = TRUE; g_yt.waitAt = 0; g_yt.wdStage = 0; if (!g_yt.pageOk) g_yt.navAt = GetTickCount();   /* the watchdog counts from now */
    YtPost(L"go"); YtSyncSound(); YtReveal(TRUE);
}
static void YtFail(void)   /* nothing of the list plays (offline, every video refused): the vitrine falls back, tried again later */
{
    g_yt.failKey = g_yt.key >= 0 ? g_yt.key : g_yt.wantKey; g_yt.failAt = GetTickCount(); if (g_yt.failN < 8) g_yt.failN++;
    g_yt.playing = FALSE; g_yt.title[0] = g_yt.chan[0] = 0; YtLog(L"fail key=%d n=%d", g_yt.failKey, g_yt.failN);
    YtHide(); g_yt.key = -1;   /* the next try navigates again */
    if (g_wnd) InvalidateRect(g_wnd, NULL, FALSE);
}
/* 6.2 fix: shown and nothing plays. The page never said 'ready' 15 s after it was asked for (a renderer that hung, a page
   left asleep): the fallback. Ready but no 'play' 8 s after it showed: playVideo again (with the view visible and awake);
   8 s more: the next video of the list; twice (or the whole list): the fallback (hidden, PORTHEON's music goes on) */
static void YtWatchdog(DWORD now)
{
    if (!g_yt.shown || g_yt.playing || g_yt.state != 3) return;
    if (!g_yt.pageOk) { if (g_yt.navAt && (LONG)(now - g_yt.navAt) > 15000) {   /* 6.2 fix: signed: navAt can be a tick NEWER than now, and the unsigned difference read as 49 days made the clip give up at once (only the music played) */
             YtLog(L"watchdog: page not ready"); YtFail(); } return; }
    if (!g_yt.waitAt) { g_yt.waitAt = now; g_yt.wdStage = 0; return; }
    LONG el = (LONG)(now - g_yt.waitAt);
    if (g_yt.wdStage == 0 && el > 8000) {
        YtLog(L"watchdog: play again"); g_yt.wdStage = 1;
        if (g_yt.ctl) g_yt.ctl->lpVtbl->put_IsVisible(g_yt.ctl, TRUE);
        YtResume(); YtRegion(); YtPost(L"go");
    } else if (g_yt.wdStage == 1 && el > 16000) {
        if (++g_yt.wdSkips >= 2 || g_yt.wdSkips >= g_yt.n) { YtLog(L"watchdog: fall back"); YtFail(); return; }
        YtLog(L"watchdog: next video"); YtPost(L"skip"); g_yt.waitAt = now; g_yt.wdStage = 0;
    }
}
/* what the vitrine asks, every tick of Animate: the game shown, whether YouTube may play it now, where */
static BOOL YtWanted(int key) { return key >= 0 && key < NGAMES && key != g_yt.failKey && g_yt.state >= 0; }   /* 6.2: Windows' "no internet" can be wrong (VPNs): the page itself says when nothing loads */
static BOOL YtShowing(void) { return g_yt.shown && g_yt.playing && g_yt.revealed; }
static BOOL YtCovers(void) { return g_yt.shown && g_yt.revealed; }   /* 6.2 fix: the web view is over the panel (a video or its waiting panel) */
static void YtTick(int key, BOOL allowed, BOOL visible, RECT r, int rad, BOOL big)
{
    YtSyncSound();   /* (the music's grace times out here too) */
    DWORD now = GetTickCount();
#ifdef PORTHEON_TEST
    {   static int lk = -9, la = -9, lv = -9, ls = -9, ln = -9, lnet = -9; if (lk != key || la != allowed || lv != visible || ls != g_yt.state || ln != g_yt.n || lnet != g_yt.net) {
            lk = key; la = allowed; lv = visible; ls = g_yt.state; ln = g_yt.n; lnet = g_yt.net; YtLog(L"tick key=%d allowed=%d visible=%d state=%d n=%d net=%ld fail=%d", key, allowed, visible, g_yt.state, g_yt.n, g_yt.net, g_yt.failKey); } }
#endif
    BOOL on = allowed && visible && key >= 0 && key < NGAMES;   /* 3 minutes off the screen: its processes go (made again when needed) */
    if (on) g_yt.offSince = 0; else if (!g_yt.offSince) g_yt.offSince = now; else if (g_yt.state > 0 && now - g_yt.offSince > 180000) YtDestroy();
    {   LONG net = g_yt.net;   /* the internet came back: what failed is tried again at once */
        if (net != g_yt.netSeen) { if (g_yt.netSeen == 0 && net == 1 && g_yt.failKey >= 0) { YtLog(L"online again: retry"); g_yt.failKey = -1; g_yt.failN = 0; g_yt.key = -1; } g_yt.netSeen = net; } }
    if (g_yt.failKey >= 0) {   /* or after a back-off: 15 s after a first failure (6.2 fix: most are passing:
                                                   a slow start, the network waking up), then 1, 2, 4, 8 minutes, then every 10 */
        DWORD wait = g_yt.failN <= 3 ? 15000u : 45000u;   /* 6.2: the video is the home screen's only sound now: it comes back soon */
        if ((LONG)(now - g_yt.failAt) > (LONG)wait) { YtLog(L"retry after %lu ms", wait); g_yt.failKey = -1; g_yt.key = -1; }
    }
    if (!allowed || key < 0 || key >= NGAMES) { YtHide(); return; }   /* not wanted at all (another mode, a custom game, a game running) */
    if (key != g_yt.wantKey) {   /* another game: its own list (and another chance) */
        g_yt.wantKey = key; if (g_yt.failKey != key) g_yt.failKey = -1;
        g_yt.n = YtList(key, g_yt.ids, YT_MAXLIST);
    }
    if (g_yt.key != key && (g_yt.shown || g_yt.playing)) {   /* the old game's video goes at once (the new one loads once the choice settles) */
        g_yt.playing = FALSE; g_yt.title[0] = g_yt.chan[0] = 0; YtHide(); if (g_wnd) InvalidateRect(g_wnd, NULL, FALSE);
    }
    if (!g_yt.n) { YtHide(); return; }
    YtNetCheck();
    if (!YtWanted(key)) { YtHide(); return; }
    if (!visible) { YtHide(); return; }   /* nothing is made, loaded or played off the screen */
    if (g_yt.state == 0) { if (!g_yt.startFails || now - g_yt.startAt > (5000u << (g_yt.startFails - 1))) YtStart(); return; }
    if (g_yt.state != 3) return;
    if (g_yt.key != key) { g_yt.key = key; YtNavigate(); }   /* only now: on screen, in front, the choice settled */
    if (big != g_yt.big) g_yt.big = big;
    YtShow(r, rad);
    YtSyncSound();
    YtWatchdog(GetTickCount());
}
static void YtSetSound(BOOL on, BOOL say)   /* M / R3, Settings > Interface > Som dos vídeos */
{
    g_yt.sndPref = on ? 1 : 0; if (!g_ytNoSave) PIniSet(L"yt_sound", on ? 1 : 0); YtSyncSound();
    if (say) Msg(C_SUB, L"%s", on ? L2(L"Som dos vídeos ligado. M / R3 desliga.", L"Video sound on. M / R3 turns it off.")
                                  : L2(L"Vídeos sem som. M / R3 liga de novo.", L"Videos muted. M / R3 turns the sound on again."));
    if (g_wnd) InvalidateRect(g_wnd, NULL, FALSE);
}
static void YtOnMessage(const wchar_t *msg)
{
    wchar_t *rest = NULL; long n = wcstol(msg, &rest, 10);
    if (!rest || *rest != L' ' || n != g_yt.nav) { YtLog(L"old page: %s", msg); return; }   /* a message of a page that is gone */
    const wchar_t *m = rest + 1;
    YtLog(L"page: %s", m);
    if (!wcscmp(m, L"ready")) {   /* the page loaded: it learns the scale and the sound; a page that loaded hidden holds and sleeps */
        YtPostU(); YtPostR(); YtPost(YtSoundPref() && g_yt.shown ? L"snd1" : L"snd0"); g_yt.sound = YtSoundPref() && g_yt.shown;
        g_yt.pageOk = TRUE; g_yt.waitAt = 0; g_yt.wdStage = 0;
        if (!g_yt.shown) { g_yt.parked = FALSE; YtPark(); }
        else { YtReveal(TRUE); YtPost(L"go"); }   /* 6.2 fix: shown: its pixels show (its waiting panel) and it plays (a 'go' sent
                                                     before it loaded went to the page before it) */
    } else if (!wcsncmp(m, L"play ", 5)) {
        const wchar_t *a = wcschr(m, L'\t'), *b = a ? wcschr(a + 1, L'\t') : NULL; int i = _wtoi(m + 5);
        g_yt.title[0] = g_yt.chan[0] = 0;
        if (a && b) { int k = (int)(b - a - 1); if (k > 159) k = 159; wmemcpy(g_yt.title, a + 1, k); g_yt.title[k] = 0; _snwprintf(g_yt.chan, 96, L"%s", b + 1); g_yt.chan[95] = 0; }
        if (i >= 0 && i < g_yt.n && (!g_yt.title[0] || !g_yt.chan[0])) {   /* the list's own words when the player gives none */
            YtLoadCurated(); for (int k = 0; k < g_ytNcur; k++) if (!wcscmp(g_ytCur[k].id, g_yt.ids[i])) {
                if (!g_yt.title[0]) wcscpy(g_yt.title, g_ytCur[k].title); if (!g_yt.chan[0]) wcscpy(g_yt.chan, g_ytCur[k].chan); break; }
        }
        g_yt.playing = TRUE; g_yt.failN = 0; g_yt.waitAt = 0; g_yt.wdStage = g_yt.wdSkips = 0;
        YtReveal(TRUE); YtSyncSound();
        if (g_wnd) InvalidateRect(g_wnd, NULL, FALSE);
    } else if (!wcsncmp(m, L"err ", 4) || !wcscmp(m, L"next")) {   /* a video skipped or ended: PORTHEON's own picture until the next one plays */
        g_yt.playing = FALSE; g_yt.title[0] = g_yt.chan[0] = 0; g_yt.waitAt = 0; g_yt.wdStage = 0;
        YtReveal(TRUE); YtSyncSound();   /* 6.2 fix: stays revealed (the page's waiting panel): hidden, the next one would not start */
        if (g_wnd) InvalidateRect(g_wnd, NULL, FALSE);
    } else if (!wcscmp(m, L"fail")) {
        YtFail();
    } else if (!wcscmp(m, L"click")) {
        if (g_overlay == OV_VITFULL) { /* 6.2 (the owner): a click no longer turns the sound off by accident */ }   /* full screen: a click on the video turns its sound on or off */
        else if (g_view == V_HOME && !g_overlay) { Snd(SND_OK); VitOpenFull(); }
    } else if (!wcsncmp(m, L"key ", 4)) {
        if (!wcscmp(m + 4, L"Escape") || !wcscmp(m + 4, L"Backspace")) Home62Back();
        else if (!_wcsicmp(m + 4, L"m")) { g_ytNoSave = TRUE; YtSetSound(!YtSoundPref(), TRUE); g_ytNoSave = FALSE; }
        SetFocus(g_wnd);
    }
}
static void YtWndMsg(WPARAM wp, LPARAM lp)
{
    if (wp == 1) { wchar_t *m = (wchar_t *)lp; if (m) { YtOnMessage(m); free(m); } }
    else if (wp == 2) { if (g_wnd && GetFocus() != g_wnd) SetFocus(g_wnd); }   /* the keyboard and the pad stay PORTHEON's */
    else if (wp == 3) { YtDestroy(); if (++g_yt.crashes >= 3) g_yt.state = -1; }   /* the browser or the page's renderer died: made again later (3 times at most) */
    else if (wp == 4) { if (g_yt.state == 3 && (long)lp == g_yt.nav) YtFail(); }   /* our own page did not load (6.2 fix: this page, not an older one) */
    else if (wp == 5) { if (g_yt.state == 3 && (long)lp == g_yt.nav && !g_yt.pageOk) { g_yt.pageOk = TRUE; g_yt.waitAt = 0; if (g_yt.shown) { YtReveal(TRUE); YtPost(L"go"); } } }   /* 6.2 fix: it loaded */
}
/* the credit in two parts: "<title>" (shortened when it must be) and " — <channel> · YouTube" (always whole) */
static BOOL YtCreditParts(wchar_t *head, int hn, wchar_t *tail, int tn)
{
    if (!YtShowing() || !g_yt.title[0]) return FALSE;
    _snwprintf(head, hn, L"%s", g_yt.title); head[hn - 1] = 0;
    if (g_yt.chan[0]) _snwprintf(tail, tn, L"  —  %s · YouTube", g_yt.chan); else _snwprintf(tail, tn, L"  ·  YouTube");
    tail[tn - 1] = 0; return TRUE;
}
/* the credit as one line: "<title> — <channel> · YouTube" */
static __attribute__((unused)) BOOL YtCredit(wchar_t *out, int n)
{
    wchar_t hd[200], tl[140]; if (!YtCreditParts(hd, 200, tl, 140)) return FALSE;
    _snwprintf(out, n, L"%s%s", hd, tl); out[n - 1] = 0; return TRUE;
}

/* ---------------------------------------------------------------- the player's own links (••• > Vitrine) */
static BOOL YtParse(const wchar_t *s, wchar_t id[12])
{
    while (*s == L' ' || *s == L'\t' || *s == L'\r' || *s == L'\n') s++;
    static const wchar_t *const keys[] = {L"v=", L"youtu.be/", L"/shorts/", L"/embed/", L"/live/", L"/v/"};
    for (int k = 0; k < 6; k++) { const wchar_t *p = wcsstr(s, keys[k]); if (p) { p += wcslen(keys[k]); if (wcslen(p) >= 11 && YtIdOk(p, 11) && !(p[11] && (iswalnum(p[11]) || p[11] == L'-' || p[11] == L'_'))) { wcsncpy(id, p, 11); id[11] = 0; return TRUE; } } }
    size_t n = wcslen(s); while (n && (s[n - 1] == L' ' || s[n - 1] == L'\r' || s[n - 1] == L'\n' || s[n - 1] == L'\t')) n--;
    if (YtIdOk(s, (int)n)) { wcsncpy(id, s, 11); id[11] = 0; return TRUE; }
    return FALSE;
}
static void YtPasteLink(int g)
{
    if (g < 0 || g >= NGAMES) return;
    wchar_t txt[1024] = L"";
    if (OpenClipboard(g_wnd)) { HANDLE h = GetClipboardData(CF_UNICODETEXT); const wchar_t *p = h ? (const wchar_t *)GlobalLock(h) : NULL; if (p) { wcsncpy(txt, p, 1023); txt[1023] = 0; GlobalUnlock(h); } CloseClipboard(); }
    wchar_t id[12];
    if (!txt[0]) { Msg(C_ORANGE, L"%s", L2(L"Copie primeiro o link do vídeo no YouTube (Compartilhar > Copiar) e clique aqui de novo.", L"First copy the video's link on YouTube (Share > Copy), then click here again.")); return; }
    if (!YtParse(txt, id)) { Msg(C_BAD, L"%s", L2(L"Isso não parece um link do YouTube. Copie o link do vídeo (youtube.com/watch?v=... ou youtu.be/...) e tente de novo.", L"That does not look like a YouTube link. Copy the video's link (youtube.com/watch?v=... or youtu.be/...) and try again.")); return; }
    wchar_t ids[YT_MAXLIST][12]; int n = YtOwnLinks(g, ids, YT_MAXLIST);
    for (int i = 0; i < n; i++) if (!wcscmp(ids[i], id)) { Msg(C_SUB, L"%s", L2(L"Esse vídeo já está na sua lista.", L"That video is already on your list.")); return; }
    wchar_t v[600]; _snwprintf(v, 600, L"%s", id);
    for (int i = 0; i < n && i < 11; i++) { size_t L = wcslen(v); _snwprintf(v + L, 600 - L, L",%s", ids[i]); }   /* the newest first, 12 at most */
    wchar_t k[48]; _snwprintf(k, 48, L"yt_%s", G[g].id); WritePrivateProfileStringW(L"PORTHEON", k, v, g_pIni);
    g_yt.wantKey = -1; g_yt.key = -1; g_yt.failKey = -1;   /* the list is read again and plays from the new link */
    Msg(C_GOOD, L2(L"Link salvo: o vídeo %s toca primeiro na vitrine do %s (pelo player do YouTube, sem baixar nada).", L"Link saved: video %s plays first in the %s showcase (through YouTube's player, nothing downloaded)."), id, G[g].name);
}
static void YtClearLinks(int g)
{
    if (g < 0 || g >= NGAMES) return;
    wchar_t k[48]; _snwprintf(k, 48, L"yt_%s", G[g].id); WritePrivateProfileStringW(L"PORTHEON", k, NULL, g_pIni);
    g_yt.wantKey = -1; g_yt.key = -1; g_yt.failKey = -1;
    Msg(C_SUB, L"%s", L2(L"Seus links do YouTube deste jogo foram apagados.", L"Your YouTube links for this game were removed."));
}
#ifdef PORTHEON_TEST
/* PORTHEON_TEST_YTSHOT=<png>: 8 s into a YouTube video, the web view as Windows composes it (PrintWindow with its own
   content) is laid over PORTHEON's own next frame, and that goes to <png>; with PORTHEON_TEST_YTFULL=1 the vitrine then
   opens full screen and 6 s later the same goes to <png without .png>-full.png. The test window is off the screen, so
   PrintWindow gives the web view only (PORTHEON's frame is the back buffer) */
static HBITMAP g_ytCap; static int g_ytCapStage; static wchar_t g_ytCapOut[MAX_PATH];
static void YtTestShot(void)
{
    static wchar_t out[MAX_PATH]; static int has = -1, full = 0, stageSeen = 0; static DWORD playAt;
    if (has < 0) { has = GetEnvironmentVariableW(L"PORTHEON_TEST_YTSHOT", out, MAX_PATH) > 0; full = GetEnvironmentVariableW(L"PORTHEON_TEST_YTFULL", NULL, 0) > 0; }
    if (!has || g_ytCap || g_ytCapStage >= (full ? 2 : 1)) return;
    if (stageSeen != g_ytCapStage) { stageSeen = g_ytCapStage; playAt = 0; return; }   /* the next shot waits its own time */
    static int cyc = -1; static DWORD cycAt; BOOL force = FALSE; if (cyc < 0) cyc = GetEnvironmentVariableW(L"PORTHEON_TEST_YTCYCLE", NULL, 0) > 0;   /* PORTHEON_TEST_YTCYCLE=1: the shot is the instant after a hide + show */
    if (cyc == 1 && YtShowing()) { if (!playAt) playAt = GetTickCount(); if (GetTickCount() - playAt > 5000) { YtHide(); cyc = 2; cycAt = GetTickCount(); playAt = 0; } return; }
    if (cyc == 2) { if (!g_yt.shown || GetTickCount() - cycAt < 180) return; force = TRUE; cyc = 0; YtLog(L"cycle shot playing=%d", g_yt.playing); }
    if (!force) {
    if (!YtShowing()) { playAt = 0; return; }
    if (!playAt) { playAt = GetTickCount(); return; }
    {   static DWORD statAt; if (GetTickCount() - statAt > 2000) { statAt = GetTickCount(); YtPost(L"stat"); } }   /* the player's state, into the log */
    if (GetTickCount() - playAt < (g_ytCapStage ? 6000u : 8000u)) return;
    }
    RECT c; GetClientRect(g_wnd, &c); HDC wdc = GetDC(g_wnd), mdc = CreateCompatibleDC(wdc); HBITMAP bm = CreateCompatibleBitmap(wdc, c.right, c.bottom); HGDIOBJ ob = SelectObject(mdc, bm);
    BOOL ok = PrintWindow(g_wnd, mdc, 3 /* PW_CLIENTONLY | PW_RENDERFULLCONTENT */);
    SelectObject(mdc, ob); DeleteDC(mdc); ReleaseDC(g_wnd, wdc);
    if (!ok) { DeleteObject(bm); g_ytCapStage = 9; return; }
    g_ytCap = bm; wcscpy(g_ytCapOut, out);
    if (g_ytCapStage == 1) { wchar_t *d = wcsrchr(g_ytCapOut, L'.'); if (d) *d = 0; wcscat(g_ytCapOut, L"-full.png"); }
    InvalidateRect(g_wnd, NULL, FALSE);   /* the next paint (YtTestAfterPaint) lays it over PORTHEON's frame */
}
static void YtTestAfterPaint(HBITMAP back, int w, int h)
{
    if (!g_ytCap || !back) return;
    HDC sdc = GetDC(NULL), a = CreateCompatibleDC(sdc), b = CreateCompatibleDC(sdc); HBITMAP cp = CreateCompatibleBitmap(sdc, w, h);
    HGDIOBJ oa = SelectObject(a, cp), ob = SelectObject(b, back); BitBlt(a, 0, 0, w, h, b, 0, 0, SRCCOPY);
    SelectObject(b, g_ytCap); RECT r = g_yt.at; BitBlt(a, r.left, r.top, r.right - r.left, r.bottom - r.top, b, r.left, r.top, SRCCOPY);
    if (g_yt.rad > 0) {   /* the web view's corners are the window region's: PORTHEON's own pixels show there */
        HRGN in = CreateRoundRectRgn(r.left, r.top, r.right + 1, r.bottom + 1, g_yt.rad * 2, g_yt.rad * 2), box = CreateRectRgn(r.left, r.top, r.right, r.bottom);
        CombineRgn(box, box, in, RGN_DIFF); SelectObject(b, back); SelectClipRgn(a, box); BitBlt(a, r.left, r.top, r.right - r.left, r.bottom - r.top, b, r.left, r.top, SRCCOPY); SelectClipRgn(a, NULL);
        DeleteObject(in); DeleteObject(box);
    }
    SelectObject(a, oa); SelectObject(b, ob); DeleteDC(a); DeleteDC(b); ReleaseDC(NULL, sdc);
    GpImage *im = NULL; static const CLSID pc = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
    if (!GdipCreateBitmapFromHBITMAP(cp, NULL, &im) && im) { GdipSaveImageToFile(im, g_ytCapOut, &pc, NULL); GdipDisposeImage(im); }
    DeleteObject(cp); DeleteObject(g_ytCap); g_ytCap = NULL; g_ytCapStage++;
    BOOL more = g_ytCapStage == 1 && GetEnvironmentVariableW(L"PORTHEON_TEST_YTFULL", NULL, 0) > 0;
    if (more) VitOpenFull();
    else {
        wchar_t out[MAX_PATH], inf[MAX_PATH + 8]; GetEnvironmentVariableW(L"PORTHEON_TEST_YTSHOT", out, MAX_PATH); _snwprintf(inf, MAX_PATH + 8, L"%s.txt", out);
        FILE *f = _wfopen(inf, L"w, ccs=UTF-8"); if (f) { wchar_t cr[400] = L""; YtCredit(cr, 400); fwprintf(f, L"shots=%d\ncredit=%s\nlist=%d\nsound=%d\n", g_ytCapStage, cr, g_yt.n, g_yt.sound); fclose(f); }
    }
}
#endif
