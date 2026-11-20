/* home62.c - part of ui.c (6.2): what makes the home screen feel alive.
 *  - The VITRINE: a 16:9 glass panel on the right of the chosen game (7.0: 576x324 at x 664, y 244 for every game, the
 *    name and PLAY on its left; the mouse or the focus is marked by a white line outside it; Released, the home's two pages
 *    (ui.c HOME_PAGES): y 340..664, the same rectangle on both pages, never moved or hidden by the page change). It plays the player's own video for that game
 *    (video.c: <PORTHEON>\videos\<ID>\ or %LOCALAPPDATA%\PORTHEON\videos\<ID>\, muted, looping), else a LIVE EDIT that
 *    PORTHEON cuts by itself from the game's own art (the player's dump, the art cache, PORTHEON's art, Steam's art for a
 *    custom game): Ken Burns pans and zooms on a beat, punch zooms and flashes on the cuts, a slight colour split, a light
 *    sweep and the game's name as a sting. The pictures are scaled once, in a worker thread; a frame is one or two
 *    bitmap draws. Nothing runs while a game runs, while PORTHEON is minimized or behind another window, or in the
 *    light effects tier (a still frame then). Settings > Interface: "Vídeos e edits dos jogos" (pini ui_vitrine).
 *  - Live cards: Seu PC (CPU load, free RAM), Controles (the pads, their battery, the button pressed), Atividade (the
 *    last session and the logs). Read only while the home screen shows and PORTHEON is in front (pini ui_live).
 *  - Status chips under the game's name, quick options (glass pop-overs, no Windows combo boxes),
 *    a Logs pill in the footer (a dot after a crash), Square / X = logs. */

#define VIT_X 664          /* the vitrine, in frame units (1280x800): x 664..1240, y 244..568 (16:9); 6.2: 422x238 at 812, 236 */
#define VIT_Y HPV(340, 244)   /* Released (the two pages, ui.c HOME_PAGES): lower on the right, y 340..664, the same place on both pages */
#define VIT_W 576
#define VIT_H 324
#define VIT_RAD 20         /* its corners (the web view's window region and the page's own edge follow it) */
#define VIT_TIMER 21
#define VIT_NSRC 4
#define WM_APP_VITOPEN (WM_APP + 0x71)
#define WM_APP_VITART (WM_APP + 0x72)
#define OV_QUICK 50        /* a quick option's pop-over */
#define OV_VITFULL 51      /* the vitrine full screen */
enum { H_VITRINE = 900, H_HOMELOGS = 901, H_VITCLOSE = 902, H_HOMEMORE = 903 /* Released: the home's other page */, H_VITMODE = 904 /* 3 */, H_QCHIP = 910 /* 6 */, H_QROW = 920 /* 32 */, H_YTPASTE = 960, H_YTCLEAR = 961, H_YTSNDPICK = 964 /* 2: Settings > Interface > Som dos vídeos */ };
#define HOME_CHIP_Y 358    /* the status chips */
#define HOME_QUICK_Y 476   /* the quick options (510 when the sentence about PLAY shows above them) */
static const Field FS_GSKIN = {S_COUNT, L"pini:game_skins", NULL, 0, 1};   /* 6.2: Settings > Interface: each game's own look (skin.c) */
static const Field FS_LIVE = {S_COUNT, L"pini:ui_live", NULL, 0, 1};   /* Settings > Interface: show the live panels */
/* Released: the home's two pages (ui.c, HOME_PAGES). The change is a short slide that Animate repaints; the focus goes to
   the new page's first stop once it is drawn (PaintHome); what the other page showed takes no click and no focus */
static void HomePageSet(int p)
{
    if (!HOME_PAGES || g_view != V_HOME || g_overlay || (p != 0) == (g_homePage != 0)) return;
    g_homePage = p != 0; g_pageT0 = g_proc ? 0 : GetTickCount() /* review: while a game runs Animate does not repaint (ui.c): no slide, the new page whole in this one paint */; g_focus = -1; g_hot = -1; g_pageFocus = TRUE; g_lastInput = GetTickCount();
    InvalidateRect(g_wnd, NULL, FALSE);
}
static RECT HomeCardRect(RECT r, int k, int n);   /* ui_screens.c: where card k stands (page 2: two by two, left of the video) */

typedef struct { BYTE *px; int w, h, stride; } VitPix;   /* a picture as plain pixels (BGRX): the edit scales them itself */
int WINAPI GdipCreateLineBrush(const GpPointF *, const GpPointF *, UINT32, UINT32, int, GpBrush **);
int WINAPI GdipDrawImageRectRect(GpGraphics *, GpImage *, float, float, float, float, float, float, float, float, int, GpImageAttributes *, void *, void *);

/* 0 YouTube + the local videos + the edits, 1 the local videos and edits only, 2 off. 6.2 review: both settings are kept in
   memory (PIniSet forgets them when they change): the vitrine asked portheon.ini several times a frame */
static int VitMode(void) { if (g_uiVitC < 0) { int m = PIni(L"ui_vitrine", 0); g_uiVitC = m < 0 || m > 2 ? 0 : m; } return g_uiVitC; }
static BOOL LiveOn(void) { if (g_uiLiveC < 0) g_uiLiveC = PIni(L"ui_live", 1) != 0; return g_uiLiveC; }
/* yt.c (included after this file): YouTube's embed player over the vitrine */
static void YtTick(int key, BOOL allowed, BOOL visible, RECT r, int rad, BOOL big); static BOOL YtShowing(void); static BOOL YtCredit(wchar_t *out, int n);
static BOOL YtCreditParts(wchar_t *head, int hn, wchar_t *tail, int tn); static BOOL YtCovers(void);
static BOOL YtSoundPref(void); static void YtSetSound(BOOL on, BOOL say);   /* the videos' sound (portheon.ini yt_sound) */
/* YouTube's credit in r: the channel always whole; a title with no room first loses what follows its " - " (the song: the
   fighter's name stays whole), then letters, ending in "…" */
static __attribute__((unused)) void YtCreditDraw(HDC dc, HFONT f, COLORREF c, RECT r)
{
    wchar_t hd[300], tl[160]; if (!YtCreditParts(hd, 300, tl, 160)) return;
    int avail = r.right - r.left, tw = TextW(dc, f, tl); if (tw > avail * 6 / 10) tw = avail * 6 / 10;
    int room = avail - tw;
    if (TextW(dc, f, hd) > room) {
        static const wchar_t *const cuts[] = {L" - ", L" – ", L" | ", L" — ", L" x "};
        for (int k = 0; k < 5; k++) { wchar_t *d = wcsstr(hd, cuts[k]); if (d && d - hd >= 4) { *d = 0; break; } }
        if (TextW(dc, f, hd) > room) {
            size_t n = wcslen(hd); wchar_t t[304];
            while (n > 1) { hd[--n] = 0; while (n > 1 && hd[n - 1] == L' ') hd[--n] = 0; _snwprintf(t, 304, L"%s…", hd); t[303] = 0; if (TextW(dc, f, t) <= room) break; }
            wcscpy(hd, t);
        }
    }
    int hw = TextW(dc, f, hd);
    RECT a = {r.left, r.top, r.left + hw + 2, r.bottom}; TextR(dc, f, c, a, hd, DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    RECT b = {a.right, r.top, r.right, r.bottom}; TextR(dc, f, c, b, tl, DT_LEFT | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}
static int YtCount(int g, int *own, int *trailers, int *edits); static void YtPasteLink(int g); static void YtClearLinks(int g);
static int YtOwnLinks(int g, wchar_t ids[][12], int max); static void YtDestroy(void);
#ifdef PORTHEON_TEST
static void YtTestShot(void); static void YtTestAfterPaint(HBITMAP back, int w, int h);
#endif
static void OpenGame(void);
static void VitDirty(void); static void VitFreeV(void); static void VitCompose(int W, int H, BOOL big); static float VitAlpha(DWORD now);
static COLORREF LibHue(const wchar_t *s, float v);   /* ui_library.c: a custom game's own colors */

/* ---------------------------------------------------------------- the vitrine's state */
typedef struct {
    int key;                   /* the game shown: 0..NGAMES-1, or 1000 + a custom game's id; -1 none */
    DWORD selAt, readyAt;      /* chosen at; its pictures (or its video's first frame) ready at */
    VPlayer *vp; int vidGen; BOOL vidTried; wchar_t vidName[80];
    VitPix src[VIT_NSRC]; int kind[VIT_NSRC], nsrc, artGen, srcW, srcH; BOOL artAsked;
    VitPix big[VIT_NSRC]; int bigKind[VIT_NSRC], nbig, bigW, bigH; BOOL bigAsked;   /* the full-screen set */
    DWORD editT0, stillT; int timerMs; BOOL lastLive; DWORD fullT0;   /* stillT: the edit's time while paused (it goes on from there) */
    DWORD dipT0; BOOL lastVid, ytWas;   /* 6.2: the source changed (edit / video / YouTube): a short dip through the glass's dark */
} Vit;
static Vit g_vit = {-1, .stillT = 900};
static volatile LONG g_vitOpenGen;   /* 6.2 review: g_vit.vidGen for the opening thread: VitStop (a new game, PLAY) cancels an open in progress */
static BOOL GamePlayed(int g);
#ifdef PORTHEON_TEST   /* the UI tests' window is off the screen, so Windows hands every paint an empty rectangle: the speed test
                         paints what the vitrine asked for, as on a monitor */
static RECT g_vitTestInval; static DWORD g_vitTestInvalAt;
#endif
static BOOL g_vitDirty = TRUE; static float g_vitVa;   /* the composed picture needs drawing again; its fade when composed */
static HDC g_vitDC; static HBITMAP g_vitHB; static HGDIOBJ g_vitOld; static void *g_vitBits; static GpBitmap *g_vitV; static int g_vitVW, g_vitVH;   /* the composed picture (VitCompose) */
static GpBitmap *g_vitLogo; static int g_vitLogoS;

static int VitKey(void) { return g_csel >= 0 && g_csel < g_ncg ? 1000 + g_cg[g_csel].id : g_sel; }
static BOOL VitFront(void)
{
#ifdef PORTHEON_TEST   /* UI tests run off screen and never in front: PORTHEON_TEST_VITRUN=1 lets the vitrine move */
    if (GetEnvironmentVariableW(L"PORTHEON_TEST_VITBACK", NULL, 0)) return FALSE;   /* as behind another window */
    if (GetEnvironmentVariableW(L"PORTHEON_TEST_VITRUN", NULL, 0)) return TRUE;
#endif
    /* 6.2 fix: PORTHEON is in front when the foreground window is PORTHEON's or one inside it (a click on the video gives
       the web view's own windows the focus, and some Windows builds report those, not PORTHEON's, as the foreground) */
    HWND fg = GetForegroundWindow();
    return fg && (fg == g_wnd || GetAncestor(fg, GA_ROOT) == g_wnd || IsChild(g_wnd, fg));
}
static BOOL VitWanted(void) { return (g_view == V_HOME || g_overlay == OV_VITFULL) && VitMode() != 2 && g_wnd; }
static BOOL VitOnScreen(void)   /* the vitrine is seen and may move: home (or its full screen), nothing over it, PORTHEON in front */
{
    return VitWanted() && (!g_overlay || g_overlay == OV_VITFULL) && !g_proc && !IsIconic(g_wnd) && IsWindowVisible(g_wnd) && VitFront() && !g_fxLight;
}
/* 6.2 fix: YouTube's clip: as VitOnScreen, but the light effects tier that Windows' battery saver or a remote desktop
   turns on by itself does not hide it (the clip is decoded by the graphics card; only light chosen in Settings or a very
   weak PC does): on a laptop that went on battery the vitrine stopped and only the game's music played */
static BOOL VitYtOnScreen(void)
{
    return VitWanted() && (!g_overlay || g_overlay == OV_VITFULL) && !g_proc && !IsIconic(g_wnd) && IsWindowVisible(g_wnd) && VitFront() && !g_fxLightHard;
}
static BOOL VitHasVideo(void);
static BOOL VitLive(void) { return VitOnScreen() && !YtShowing() && VitHasVideo(); }   /* 6.2: no live edit any more (the owner: only the clip) */   /* (YouTube over it: the edit and the local video rest) */
#define HERO_L (SkinSide() ? MX + 12 + VIT_W + 34 : MX + 12)   /* 6.2: the name's column: a game's look can put the video on the left */
#define HERO_R (SkinSide() ? BASE_W - MX - 18 : VIT_X - 20)
static int VitLeft(void) { return SkinSide() ? MX + 12 : VIT_X; }
static RECT VitRect(void) { RECT r = {SC(VitLeft()), SC(VIT_Y), SC(VitLeft() + VIT_W), SC(VIT_Y + VIT_H)}; return r; }   /* frame coordinates */
static RECT VitFullRect(void)
{
    RECT c; GetClientRect(g_wnd, &c); int W = c.right, H = c.bottom, h = W * 9 / 16; if (h > H) { h = H; W = h * 16 / 9; }
    RECT r = {(c.right - W) / 2 - g_ox, (c.bottom - h) / 2 - g_oy, (c.right - W) / 2 - g_ox + W, (c.bottom - h) / 2 - g_oy + h}; return r;
}
static void VitInvalidate(void)
{
    if (!g_wnd) return;
    VitDirty();
    if (g_overlay == OV_VITFULL) { RECT f = VitFullRect(); OffsetRect(&f, g_ox, g_oy); InvalidateRect(g_wnd, &f, FALSE); return; }   /* (only the picture: the rest is black) */
    RECT r = VitRect(); OffsetRect(&r, g_ox, g_oy); InvalidateRect(g_wnd, &r, FALSE);
#ifdef PORTHEON_TEST
    g_vitTestInval = r; g_vitTestInvalAt = GetTickCount();
#endif
}
static void VitFreeSet(VitPix *b, int *n) { for (int i = 0; i < VIT_NSRC; i++) { free(b[i].px); b[i].px = NULL; } *n = 0; }
static void VitStop(void)
{
    if (g_wnd) KillTimer(g_wnd, VIT_TIMER);
    g_vit.timerMs = 0;
    if (g_vit.vp) { VideoClose(g_vit.vp); g_vit.vp = NULL; }
    VitFreeSet(g_vit.src, &g_vit.nsrc); VitFreeSet(g_vit.big, &g_vit.nbig);
    g_vit.vidGen++; InterlockedExchange(&g_vitOpenGen, g_vit.vidGen); g_vit.artGen++; g_vit.key = -1; g_vit.stillT = 900; g_vit.lastLive = FALSE; g_vit.vidTried = g_vit.artAsked = g_vit.bigAsked = FALSE; g_vit.readyAt = 0;
    FreeBmp(&g_vitLogo); g_vitLogoS = 0; VitFreeV();
}
static void VitReset(void) { int k = g_vit.key; DWORD s = g_vit.selAt; VitStop(); if (k >= 0) { g_vit.key = k; g_vit.selAt = s; g_vit.editT0 = GetTickCount(); } }   /* the scale changed */

/* ---------------------------------------------------------------- the pictures (a worker thread scales them once) */
typedef struct { int gen, w, h, n; BOOL big; wchar_t path[VIT_NSRC][MAX_PATH]; int kind[VIT_NSRC]; VitPix out[VIT_NSRC]; } VitArtJob;
static GpBitmap *VitScale(GpImage *im, int kind, int w, int h)
{
    GpGraphics *g = NULL; GpBitmap *b = NewBitmap(w, h, &g); if (!b || !g) { if (g) GdipDeleteGraphics(g); FreeBmp(&b); return NULL; }
    if (kind == 0) DrawCover(g, im, 0, 0, w, h);
    else {   /* a cover (portrait or square): a poster on its own picture, blurred and darker, with a soft shadow */
        GpGraphics *sg = NULL; GpBitmap *sm = NewBitmap(w / 18 + 2, h / 18 + 2, &sg);
        if (sm && sg) { DrawCover(sg, im, 0, 0, w / 18 + 2, h / 18 + 2); GdipDeleteGraphics(sg); GdipSetInterpolationMode(g, 7); GdipDrawImageRectI(g, (GpImage *)sm, -w / 16, -h / 16, w + w / 8, h + h / 8); }
        else if (sg) GdipDeleteGraphics(sg);
        FreeBmp(&sm); Fill(g, 0, 0, w, h, A(RGB0(0, 0, 0), 120));
        UINT iw = 0, ih = 0; GdipGetImageWidth(im, &iw); GdipGetImageHeight(im, &ih);
        if (iw && ih) {
            int ph = h * 80 / 100, pw = (int)((double)ph * iw / ih); if (pw > w * 86 / 100) { pw = w * 86 / 100; ph = (int)((double)pw * ih / iw); }
            int px = (w - pw) / 2, py = (h - ph) / 2;
            for (int k = 3; k >= 1; k--) { RECT s = {px - k * 6, py - k * 6 + 10, px + pw + k * 6, py + ph + k * 6 + 10}; Box(g, s, 10 + k * 6, A(RGB0(0, 0, 0), 46), 0, 0); }
            GpPath *p = RoundPath(px, py, pw - 1, ph - 1, w / 60 + 4); GdipSetClipPath(g, p, 0);
            GdipSetInterpolationMode(g, 7); GdipDrawImageRectI(g, im, px, py, pw, ph); GdipResetClip(g);
            GpPen *pen = NULL; GdipCreatePen1(A(RGB0(255, 255, 255), 60), 1.5f, 2, &pen); if (pen) { GdipDrawPath(g, pen, p); GdipDeletePen(pen); }
            GdipDeletePath(p);
        }
    }
    GdipDeleteGraphics(g); return b;
}
static VitPix VitToPix(GpBitmap *b)   /* the scaled picture's pixels, copied out of GDI+ (opaque BGRX) */
{
    VitPix p = {0}; UINT w = 0, h = 0; if (!b) return p; GdipGetImageWidth((GpImage *)b, &w); GdipGetImageHeight((GpImage *)b, &h);
    GpRectI r = {0, 0, (INT)w, (INT)h}; GpBitmapData bd; memset(&bd, 0, sizeof(bd));
    if (!w || !h || GdipBitmapLockBits(b, &r, 1 /* read */, 0x00022009 /* 32bppRGB */, &bd)) return p;
    p.px = (BYTE *)malloc((size_t)w * h * 4);
    if (p.px) { for (UINT y = 0; y < h; y++) memcpy(p.px + (size_t)y * w * 4, (BYTE *)bd.Scan0 + (size_t)y * bd.Stride, (size_t)w * 4); p.w = (int)w; p.h = (int)h; p.stride = (int)w * 4; }
    GdipBitmapUnlockBits(b, &bd); return p;
}
static void VitArtRun(VitArtJob *j)
{
    for (int i = 0; i < j->n; i++) { GpImage *im = LoadImageFile(j->path[i]); if (!im) continue; GpBitmap *b = VitScale(im, j->kind[i], j->w, j->h); GdipDisposeImage(im); j->out[i] = VitToPix(b); FreeBmp(&b); }
}
static DWORD WINAPI VitArtThread(LPVOID p)
{
    VitArtJob *j = (VitArtJob *)p; SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    VitArtRun(j);
    if (!g_wnd || !PostMessageW(g_wnd, WM_APP_VITART, (WPARAM)j->gen, (LPARAM)j)) { for (int i = 0; i < VIT_NSRC; i++) free(j->out[i].px); free(j); }
    return 0;
}
/* the picture files of the game shown: wide ones first (pic1, pic0, the art cache, PORTHEON's own background), then one
   cover; the same file twice (the cache is a copy) is kept once, by its size */
static int VitAddPath(VitArtJob *j, const wchar_t *p, int kind, ULONGLONG *sizes)
{
    if (j->n >= VIT_NSRC || !p[0]) return 0;
    WIN32_FILE_ATTRIBUTE_DATA fa; if (!GetFileAttributesExW(p, GetFileExInfoStandard, &fa) || (fa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return 0;
    ULONGLONG sz = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow; if (!sz) return 0;
    for (int i = 0; i < j->n; i++) if (sizes[i] == sz) return 0;
    sizes[j->n] = sz; wcsncpy(j->path[j->n], p, MAX_PATH - 1); j->kind[j->n] = kind; j->n++; return 1;
}
static void VitPaths(VitArtJob *j)
{
    ULONGLONG sizes[VIT_NSRC] = {0}; wchar_t p[MAX_PATH], d[MAX_PATH], pd[MAX_PATH], own[MAX_PATH];
    int key = g_vit.key, wide = 0;
    if (key >= 1000) {   /* a custom game: its own picture and cover (library.c: Steam's hero and grid, a dump's art, the player's pick) */
        int id = key - 1000;
        if (LibArtFind(id, L"pic", p)) VitAddPath(j, p, 0, sizes);
        if (LibArtFind(id, L"icon", p)) VitAddPath(j, p, 1, sizes);
        return;
    }
    int gi = key; if (gi < 0 || gi >= NGAMES) return;
    GameDir(gi, d); PortDir(gi, pd); Join(own, g_exeDir, L"games"); Join(own, own, G[gi].id);
    if (d[0]) { static const wchar_t *const pics[] = {L"sce_sys\\pic1.png", L"sce_sys\\pic0.png", L"PS3_GAME\\PIC1.PNG", L"PS3_GAME\\PIC0.PNG"};
                for (int i = 0; i < 4 && wide < 3; i++) { Join(p, d, pics[i]); wide += VitAddPath(j, p, 0, sizes); } }
    if (wide < 3) { ArtCachePath(gi, L"pic", p); wide += VitAddPath(j, p, 0, sizes); }
    if (X360G(gi)) { static const wchar_t *const st[] = {L"steam\\hero.png", L"steam\\wide.png"}; for (int i = 0; i < 2 && wide < 3; i++) { Join(p, pd, st[i]); wide += VitAddPath(j, p, 0, sizes); } }
    if (wide < 3) { Join(p, pd, L"art\\background.jpg"); wide += VitAddPath(j, p, 0, sizes); }
    if (wide < 3 && _wcsicmp(own, pd)) { Join(p, own, L"art\\background.jpg"); wide += VitAddPath(j, p, 0, sizes); }
    int cov = 0;
    if (d[0]) { Join(p, d, L"sce_sys\\icon0.png"); cov += VitAddPath(j, p, 1, sizes); }
    if (!cov) { ArtCachePath(gi, L"icon", p); cov += VitAddPath(j, p, 1, sizes); }
    if (!cov) { Join(p, pd, L"art\\cover.jpg"); cov += VitAddPath(j, p, 1, sizes); }
    if (!cov && _wcsicmp(own, pd)) { Join(p, own, L"art\\cover.jpg"); cov += VitAddPath(j, p, 1, sizes); }
    if (!cov && X360G(gi)) { Join(p, pd, L"steam\\grid.png"); VitAddPath(j, p, 1, sizes); }
}
static VitArtJob *VitArtJobNew(BOOL big)
{
    VitArtJob *j = (VitArtJob *)calloc(1, sizeof(VitArtJob)); if (!j) return NULL;
    j->gen = g_vit.artGen; j->big = big;
    if (big) { RECT f = VitFullRect(); j->w = (f.right - f.left) * 108 / 100; j->h = (f.bottom - f.top) * 108 / 100; }
    else { j->w = SC(VIT_W) * 130 / 100; j->h = SC(VIT_H) * 130 / 100; }
    VitPaths(j);
    if (big && j->n > 3) j->n = 3;   /* about 10 MB each at 1080p: two pictures and the cover, freed when it closes */
    return j;
}
static void VitOnArt(WPARAM gen, LPARAM lp)
{
    VitArtJob *j = (VitArtJob *)lp; if (!j) return;
    if ((int)gen != g_vit.artGen) { for (int i = 0; i < VIT_NSRC; i++) free(j->out[i].px); free(j); return; }
    VitPix *dst = j->big ? g_vit.big : g_vit.src; int *kd = j->big ? g_vit.bigKind : g_vit.kind, *n = j->big ? &g_vit.nbig : &g_vit.nsrc;
    VitFreeSet(dst, n);
    for (int i = 0; i < j->n; i++) if (j->out[i].px) { kd[*n] = j->kind[i]; dst[(*n)++] = j->out[i]; j->out[i].px = NULL; }
    if (j->big) { g_vit.bigW = j->w; g_vit.bigH = j->h; } else { g_vit.srcW = j->w; g_vit.srcH = j->h; }
    BOOL big = j->big; free(j);
    if (!big && !g_vit.readyAt) { g_vit.readyAt = GetTickCount(); g_vit.editT0 = g_vit.readyAt; }
    VitInvalidate();   /* (its fade runs on the vitrine's own timer: no whole-window motion) */
}
static void VitAskArt(BOOL big)
{
    VitArtJob *j = VitArtJobNew(big); if (!j) return;
    if (big) g_vit.bigAsked = TRUE; else g_vit.artAsked = TRUE;
    if (!j->n) { free(j); if (!big && !g_vit.readyAt) { g_vit.readyAt = GetTickCount(); g_vit.editT0 = g_vit.readyAt; VitInvalidate(); } return; }   /* no art: the edit runs on the game's colors */
    HANDLE t = CreateThread(NULL, 0, VitArtThread, j, 0, NULL);
    if (t) CloseHandle(t); else { VitArtRun(j); VitOnArt((WPARAM)j->gen, (LPARAM)j); }
}

/* ---------------------------------------------------------------- the player's own video (video.c, opened in a thread) */
typedef struct { int gen; wchar_t path[MAX_PATH]; } VitOpenJob;
static DWORD WINAPI VitOpenThread(LPVOID p)
{
    VitOpenJob *j = (VitOpenJob *)p; VPlayer *v = g_vitOpenGen == j->gen && !g_proc ? VideoOpenEx(j->path, TRUE, &g_vitOpenGen, j->gen) : NULL;
    if (!g_wnd || !PostMessageW(g_wnd, WM_APP_VITOPEN, (WPARAM)j->gen, (LPARAM)v)) { if (v) VideoClose(v); }
    free(j); return 0;
}
static void VitOnOpened(WPARAM gen, LPARAM lp)
{
    VPlayer *v = (VPlayer *)lp;
    if ((int)gen != g_vit.vidGen || !VitWanted()) { if (v) VideoClose(v); return; }
    if (g_vit.vp) VideoClose(g_vit.vp);
    g_vit.vp = v; if (v) VideoPause(v, !VitLive());
    VitInvalidate();
}
static void VitAskVideo(void)
{
    g_vit.vidTried = TRUE; g_vit.vidName[0] = 0;
    if (VitMode() == 2 || g_vit.key < 0 || g_vit.key >= NGAMES) return;   /* 6.2: a custom game has no ID folder (yet) */
    wchar_t p[MAX_PATH]; if (!VideoFind(g_vit.key, G[g_vit.key].id, p, MAX_PATH)) return;
    _snwprintf(g_vit.vidName, 80, L"%s", PathFindFileNameW(p)); g_vit.vidName[79] = 0;
    static BOOL wset; if (!wset) { wset = TRUE; VideoSetWindow(g_wnd); }
    VitOpenJob *j = (VitOpenJob *)calloc(1, sizeof(VitOpenJob)); if (!j) return;
    j->gen = g_vit.vidGen; wcsncpy(j->path, p, MAX_PATH - 1);
    HANDLE t = CreateThread(NULL, 0, VitOpenThread, j, 0, NULL); if (t) CloseHandle(t); else free(j);
}
static BOOL VitHasVideo(void) { return g_vit.vp && VideoFrame(g_vit.vp, NULL, NULL, NULL, NULL, NULL); }
static void VitOnVideoFrame(void)
{
    if (!g_vit.vp) return;
    if (!VitLive()) { VideoPause(g_vit.vp, TRUE); return; }
    if (!g_vit.readyAt) g_vit.readyAt = GetTickCount();
    if (g_overlay == OV_VITFULL && MotionT(g_vit.fullT0, 320) < 1.0f) return;   /* the zoom shows the picture composed at its start */
    VitInvalidate(); if (g_overlay != OV_VITFULL) { RECT R = VitRect(); VitCompose(R.right - R.left, R.bottom - R.top, FALSE); }
}

/* ---------------------------------------------------------------- what runs, every 33 ms (Animate) */
static void VitSetTimer(int ms)
{
    if (ms == g_vit.timerMs) return;
    g_vit.timerMs = ms;
    if (ms) SetTimer(g_wnd, VIT_TIMER, (UINT)ms, NULL); else KillTimer(g_wnd, VIT_TIMER);
}
static void VitAnimate(void)
{
    if (!g_wnd) return;
#ifdef PORTHEON_TEST
    YtTestShot();
#endif
    {   /* YouTube (yt.c): over the vitrine while it is on screen and the choice settled; full screen keeps a bar for the credit */
        BOOL full = g_overlay == OV_VITFULL, settled = GetTickCount() - g_vit.selAt > 350 && VitKey() == g_vit.key, zoom = full && MotionT(g_vit.fullT0, 320) < 1.0f;
        BOOL vis = VitYtOnScreen() && settled && !zoom && !g_motionOn && MotionT(g_viewT0, 280) >= 1.0f;
        RECT r; int rad = 0;
        if (full) { RECT f = VitFullRect(); int bar = 0, h = f.bottom - f.top - bar, w = h * 16 / 9; if (w > f.right - f.left) { w = f.right - f.left; h = w * 9 / 16; }
                    r.left = f.left + (f.right - f.left - w) / 2; r.top = f.top + (f.bottom - bar - f.top - h) / 2; r.right = r.left + w; r.bottom = r.top + h; }
        else { r = VitRect(); InflateRect(&r, -1, -1); rad = SC(VIT_RAD) - 1; }
        OffsetRect(&r, g_ox, g_oy);
        if (g_crun) YtDestroy();   /* a custom game runs: no web view (its processes end) */
        YtTick(VitKey() < NGAMES ? VitKey() : -1, VitMode() == 0 && VitWanted() && !g_proc && !g_crun, vis, r, rad, full);
    }
    if (!VitWanted()) { if (g_vit.vp) VideoPause(g_vit.vp, TRUE); VitSetTimer(0); if (g_view != V_HOME && g_view != V_GAME && g_vit.key >= 0 && g_view != V_SETTINGS) VitStop(); return; }
    int key = VitKey();
    if (key != g_vit.key) { VitStop(); g_vit.key = key; g_vit.selAt = GetTickCount(); g_vit.editT0 = g_vit.selAt; }
    DWORD now = GetTickCount();
    if (g_proc || IsIconic(g_wnd)) { if (g_vit.vp) VideoPause(g_vit.vp, TRUE); VitSetTimer(0); return; }
    {   BOOL ys = YtShowing(); if (g_vit.ytWas && !ys && g_vit.readyAt) { g_vit.dipT0 = now; VitInvalidate(); } g_vit.ytWas = ys; }   /* YouTube went: our picture comes back through the dark */
    if (YtShowing()) { if (g_vit.vp) VideoPause(g_vit.vp, TRUE); VitSetTimer(0); g_vit.lastLive = FALSE; return; }   /* YouTube covers it: nothing of ours runs */
    if (now - g_vit.selAt > 350) {   /* the choice settled: the pictures and the video (scrolling the row loads nothing) */
        /* perf 11-13: the pictures' thread scales with GDI+, and GDI+ draws one thing at a time in the whole process: started
           in the middle of a game change it froze one frame of the motion for 60 to 270 ms. It waits until nothing moves
           (the motion, the crossfade, the page change, the tiles' growth); the full-screen request below is not held */
        if (!g_vit.artAsked && ((!g_motionOn && g_fadeFrom < 0 && !g_pageT0 && !g_tilesMove) || g_overlay == OV_VITFULL || now - g_vit.selAt > 4000)) VitAskArt(FALSE);
        if (!g_vit.vidTried) VitAskVideo();
        if (g_overlay == OV_VITFULL && !g_vit.bigAsked) VitAskArt(TRUE);
    }
    BOOL live = VitLive(), vid = VitHasVideo();
    if (g_vit.vp) VideoPause(g_vit.vp, !live);
    /* the edit's frames: 60 a second while someone uses PORTHEON, 30 when nobody touched it for 8 s; a video paces itself */
    BOOL fading = (g_vit.readyAt && now - g_vit.readyAt < 700) || (g_vit.dipT0 && now - g_vit.dipT0 < 520);
    VitSetTimer(live && (!vid || fading || (g_overlay == OV_VITFULL && now - g_vit.fullT0 < 400)) ? (g_calm && !fading ? 33 : 16) : 0);
    if (!live && g_vit.readyAt && g_vitVa < 1.0f && VitAlpha(now) >= 1.0f && g_view == V_HOME) VitInvalidate();
    if (g_vitDirty && g_view == V_HOME && !g_overlay && VitMode() != 2) { RECT R = VitRect(); VitCompose(R.right - R.left, R.bottom - R.top, FALSE); }
    if (live != g_vit.lastLive) {   /* paused: one still frame, and the edit goes on from it */
        g_vit.lastLive = live; if (live) g_vit.editT0 = now - g_vit.stillT; else g_vit.stillT = now - g_vit.editT0; VitInvalidate();
    }
}
static void VitTimer(void)
{
    if (!VitLive()) { VitSetTimer(0); return; }
    if (g_overlay == OV_VITFULL && MotionT(g_vit.fullT0, 320) < 1.0f) return;   /* 6.2 review: the zoom scales the picture composed once */
    VitInvalidate(); if (g_overlay != OV_VITFULL) { RECT R = VitRect(); VitCompose(R.right - R.left, R.bottom - R.top, FALSE); }
}

/* ---------------------------------------------------------------- drawing the edit */
static float Clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }   /* (was in ui_screens.c) */
static float Hash01(unsigned k) { k ^= k >> 16; k *= 0x7feb352du; k ^= k >> 15; k *= 0x846ca68bu; k ^= k >> 16; return (float)(k & 0xFFFFFF) / 16777216.0f; }
static float Smooth(float p) { if (p <= 0) return 0; if (p >= 1) return 1; return p * p * (3.0f - 2.0f * p); }
#define BEATS 5
static const int BEAT_MS[BEATS] = {1800, 1800, 900, 900, 2000};   /* a phrase: two long shots, two quick cuts, the name sting (7.4 s) */
static GpImage *VitLogo(int s)
{
    if (!g_imgLogo || s <= 0) return NULL;
    if (g_vitLogo && g_vitLogoS == s) return (GpImage *)g_vitLogo;
    FreeBmp(&g_vitLogo); GpGraphics *lg = NULL; GpBitmap *b = NewBitmap(s, s, &lg);
    if (!b || !lg) { if (lg) GdipDeleteGraphics(lg); FreeBmp(&b); return NULL; }
    UINT iw = 0, ih = 0; GdipGetImageWidth(g_imgLogo, &iw); GdipGetImageHeight(g_imgLogo, &ih);
    GpImageAttributes *ta = ThemeImageAttr(1.0f); GdipDrawImageRectRectI(lg, g_imgLogo, 0, 0, s, s, 0, 0, (INT)iw, (INT)ih, 2, ta, NULL, NULL); if (ta) GdipDisposeImageAttributes(ta);
    GdipDeleteGraphics(lg); g_vitLogo = b; g_vitLogoS = s; return (GpImage *)b;
}
static const wchar_t *VitName(void) { return g_vit.key >= 1000 ? (g_csel >= 0 && g_csel < g_ncg ? g_cg[g_csel].name : L"") : g_vit.key >= 0 && g_vit.key < NGAMES ? G[g_vit.key].name : L""; }
/* the edit's own scaler: bilinear, straight into the composed bitmap (GDI+'s DrawImage took about 7 ms a frame for this;
   this takes about 1). chroma > 0 takes red from the right and blue from the left: the colour split of a cut, for free */
static int *g_vitCols; static int g_vitColsN;
static void VitBlit(const VitPix *s, float sx, float sy, float sw, float sh, float dx, float dy, float chroma)
{
    BYTE *dst = (BYTE *)g_vitBits; int W = g_vitVW, H = g_vitVH;
    if (!dst || !s || !s->px || W <= 0 || H <= 0 || s->w < 2 || s->h < 2) return;
    if (g_vitColsN < W * 3) { int *nc = (int *)realloc(g_vitCols, sizeof(int) * W * 3); if (!nc) return; g_vitCols = nc; g_vitColsN = W * 3; }
    int *xg = g_vitCols, *xr = g_vitCols + W, *xb = g_vitCols + 2 * W; float kx = sw / W, ky = sh / H, mx = (float)(s->w - 1) - 0.004f, my = (float)(s->h - 1) - 0.004f;
    for (int x = 0; x < W; x++) {
        float u = sx + (x + 0.5f - dx) * kx - 0.5f, ur = u + chroma * kx, ub = u - chroma * kx;
        if (u < 0) u = 0; if (u > mx) u = mx; if (ur < 0) ur = 0; if (ur > mx) ur = mx; if (ub < 0) ub = 0; if (ub > mx) ub = mx;
        xg[x] = (int)(u * 256.0f); xr[x] = (int)(ur * 256.0f); xb[x] = (int)(ub * 256.0f);
    }
    for (int y = 0; y < H; y++) {
        float v = sy + (y + 0.5f - dy) * ky - 0.5f; if (v < 0) v = 0; if (v > my) v = my;
        int vi = (int)(v * 256.0f), y0 = vi >> 8, fy = vi & 255;
        const BYTE *r0 = s->px + (size_t)y0 * s->stride, *r1 = r0 + s->stride;
        BYTE *d = dst + (size_t)y * W * 4;
        if (chroma <= 0.0f) {
            for (int x = 0; x < W; x++, d += 4) {
                int u = xg[x], f = u & 255; const BYTE *a = r0 + (u >> 8) * 4, *b = r1 + (u >> 8) * 4;
                for (int c = 0; c < 3; c++) { int t = a[c] * (256 - f) + a[c + 4] * f, bt = b[c] * (256 - f) + b[c + 4] * f; d[c] = (BYTE)((t * (256 - fy) + bt * fy) >> 16); }
                d[3] = 255;
            }
        } else {
            for (int x = 0; x < W; x++, d += 4) {
                const int *cols[3] = {xb, xg, xr};   /* B G R */
                for (int c = 0; c < 3; c++) { int u = cols[c][x], f = u & 255; const BYTE *a = r0 + (u >> 8) * 4 + c, *b = r1 + (u >> 8) * 4 + c;
                    int t = a[0] * (256 - f) + a[4] * f, bt = b[0] * (256 - f) + b[4] * f; d[c] = (BYTE)((t * (256 - fy) + bt * fy) >> 16); }
                d[3] = 255;
            }
        }
    }
}
/* one source drawn at zoom z around (fx, fy) of it, over the whole composed bitmap (sub-pixel, so a slow pan never steps) */
static void VitShot(const VitPix *b, RECT R, float z, float fx, float fy, float dx, float dy, float chroma)
{
    if (z < 1.0f) z = 1.0f; int sw = b->w, sh = b->h;
    float w = sw / z, h = w * (R.bottom - R.top) / (float)(R.right - R.left); if (h > sh / z) { h = sh / z; w = h * (R.right - R.left) / (float)(R.bottom - R.top); }
    float x = fx * sw - w / 2, y = fy * sh - h / 2;
    if (x < 0) x = 0; if (y < 0) y = 0; if (x + w > sw) x = sw - w; if (y + h > sh) y = sh - h;
    VitBlit(b, x, y, w, h, dx, dy, chroma);
}
static void VitSweep(GpGraphics *g, RECT R, float p, BYTE a)   /* a slanted band of light crossing R (p 0..1) */
{
    if (p <= 0 || p >= 1) return;
    int w = R.right - R.left, h = R.bottom - R.top; float bx = R.left - w * 0.35f + p * w * 1.7f, hw = w * 0.11f, sl = h * 0.45f;
    GpPointF q[4] = {{bx - hw + sl, (float)R.top}, {bx + hw + sl, (float)R.top}, {bx + hw - sl, (float)R.bottom}, {bx - hw - sl, (float)R.bottom}};
    GpPath *pa = NULL; GdipCreatePath(0, &pa); if (!pa) return; GdipAddPathPolygon(pa, q, 4);
    /* the gradient across the band, square to its slant (a line brush repeats past its ends: it must span the band exactly) */
    float nx = (float)h, ny = 2.0f * sl, nl = sqrtf(nx * nx + ny * ny), bw = 2.0f * hw * h / nl; nx /= nl; ny /= nl;
    GpPointF l = {bx - hw, R.top + h / 2.0f}, r2 = {l.X + nx * bw, l.Y + ny * bw}; GpBrush *lb = NULL; GdipCreateLineBrush(&l, &r2, A(RGB0(255, 255, 255), 0), A(RGB0(255, 255, 255), 0), 0, &lb);
    if (lb) { UINT32 c[3] = {A(RGB0(255, 255, 255), 0), A(RGB0(255, 255, 255), a), A(RGB0(255, 255, 255), 0)}; float ps[3] = {0, 0.5f, 1}; GdipSetLinePresetBlend(lb, c, ps, 3); GdipFillPath(g, lb, pa); GdipDeleteBrush(lb); }
    GdipDeletePath(pa);
}
static HFONT VitFont(int px, int weight)
{
    static HFONT f[2]; static int fs[2], fw[2]; int k = weight >= FW_BOLD ? 1 : 0;
    if (f[k] && fs[k] == px && fw[k] == weight) return f[k];
    if (f[k]) DeleteObject(f[k]);
    f[k] = CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Bahnschrift"); fs[k] = px; fw[k] = weight; return f[k];
}
/* the edit at time t (ms) into R. Returns the phrase's progress 0..1 (the bar) */
static float VitEdit(HDC dc, GpGraphics *g, RECT R, DWORD t, BOOL big)
{
    VitPix *src = big && g_vit.nbig ? g_vit.big : g_vit.src; int *kd = big && g_vit.nbig ? g_vit.bigKind : g_vit.kind;
    int n = big && g_vit.nbig ? g_vit.nbig : g_vit.nsrc;
    int W = R.right - R.left, H = R.bottom - R.top, cycle = 0; for (int i = 0; i < BEATS; i++) cycle += BEAT_MS[i];
    unsigned c = t / (unsigned)cycle; int r = (int)(t % (unsigned)cycle), bi = 0; while (r >= BEAT_MS[bi]) r -= BEAT_MS[bi++];
    int lt = r, dur = BEAT_MS[bi]; unsigned j = c * (BEATS - 1) + (unsigned)bi;   /* the shot's number (the sting not counted) */
    float p = (float)lt / dur, punch = 1.0f - EaseOut3(lt / 260.0f);
    COLORREF c1 = g_vit.key >= 0 && g_vit.key < NGAMES ? G[g_vit.key].c1 : LibHue(VitName(), 0.70f), c2 = g_vit.key >= 0 && g_vit.key < NGAMES ? G[g_vit.key].c2 : LibHue(VitName(), 0.16f);
    if (bi == BEATS - 1) {   /* the sting: the name over the art, darker, its letters closing in, PORTHEON's mark */
        if (n) VitShot(&src[0], R, 1.30f + 0.06f * p, 0.5f + 0.05f * p, 0.45f, 0, 0, 0);
        else { POINT a = {R.left, R.top}, z = {R.right, R.bottom}; GpBrush *lb = NULL; GdipCreateLineBrushI(&a, &z, A(c1, 255), A(c2, 255), 0, &lb); if (lb) { GdipFillRectangleI(g, lb, R.left, R.top, W, H); GdipDeleteBrush(lb); } }
        Fill(g, R.left, R.top, W, H, A(RGB0(0, 0, 0), 168));
        VitSweep(g, R, (p - 0.30f) / 0.55f, 44);
        float in = Clamp01(lt / 300.0f), e = EaseOut3(lt / 900.0f);
        int fh = H * 17 / 100; HFONT f = VitFont(fh, FW_BOLD); const wchar_t *nm = VitName();
        int sp = (int)(fh * (0.42f - 0.30f * e)); SetTextCharacterExtra(dc, LangCjk() ? 0 : sp);
        int tw = TextW(dc, f, nm); if (tw > W * 88 / 100) { SetTextCharacterExtra(dc, 0); tw = TextW(dc, f, nm); sp = 0; }
        RECT tr = {R.left + (W - tw) / 2 - fh, R.top + H * 22 / 100, R.left + (W + tw) / 2 + fh, R.top + H * 22 / 100 + fh * 13 / 10};   /* above the caption */
        TextR(dc, f, Mix(RGB0(0, 0, 0), RGB0(255, 255, 255), in), tr, nm, DT_CENTER | DT_VCENTER); SetTextCharacterExtra(dc, 0);
        int lw = (int)(W * 0.30f * EaseOut3(Clamp01((lt - 150) / 500.0f))); Fill(g, R.left + (W - lw) / 2, tr.bottom + H / 40, lw, H / 120 + 1, A(RGB0(255, 255, 255), (BYTE)(200 * in)));
        int ls = H / 9; GpImage *lg = VitLogo(ls);
        HFONT sf = VitFont(H / 15, FW_NORMAL); const wchar_t *by = L"PORTHEON  ·  EDIT"; SetTextCharacterExtra(dc, LangCjk() ? 0 : H / 70); int bw = TextW(dc, sf, by);
        int bx = R.left + (W - (ls + H / 30 + bw)) / 2, byy = tr.bottom + H / 16;
        float a2 = Clamp01((lt - 350) / 400.0f);
        if (lg) DrawAlpha(g, lg, bx, byy, ls, ls, a2);
        RECT br = {bx + ls + H / 30, byy, bx + ls + H / 30 + bw + 4, byy + ls}; TextR(dc, sf, Mix(RGB0(0, 0, 0), RGB0(214, 214, 222), a2), br, by, DT_LEFT | DT_VCENTER); SetTextCharacterExtra(dc, 0);
    } else if (n) {   /* a shot: Ken Burns on a beat */
        int s = (int)(j % (unsigned)n), v = (int)(j % 3u); float h1 = Hash01(j * 3 + 1), h2 = Hash01(j * 3 + 2);
        float z0, z1, fx0, fx1, fy0, fy1;
        if (kd[s] == 1) { z0 = 1.02f; z1 = 1.14f; fx0 = fx1 = 0.5f; fy0 = 0.5f; fy1 = 0.46f; }   /* the poster: a slow push */
        else if (bi >= 2) { z0 = 1.55f + 0.2f * h1; z1 = z0 + 0.10f; fx0 = 0.35f + 0.4f * h2; fx1 = fx0 + (h1 > 0.5f ? 0.04f : -0.04f); fy0 = 0.40f; fy1 = 0.44f; }   /* the quick cuts: close */
        else if (v == 0) { z0 = 1.06f; z1 = 1.20f; fx0 = h1 > 0.5f ? 0.38f : 0.62f; fx1 = 1.0f - fx0; fy0 = fy1 = 0.5f; }   /* a wide pan */
        else if (v == 1) { z0 = 1.30f; z1 = 1.62f; fx0 = fx1 = 0.55f + 0.2f * (h2 - 0.5f); fy0 = 0.48f; fy1 = 0.40f; }   /* a push in */
        else { z0 = 1.55f; z1 = 1.08f; fx0 = 0.5f + 0.25f * (h1 - 0.5f); fx1 = 0.5f; fy0 = 0.42f; fy1 = 0.5f; }   /* a pull out */
        float e = Smooth(p) * 0.35f + p * 0.65f, z = (z0 + (z1 - z0) * e) * (1.0f + 0.10f * punch);
        float fx = fx0 + (fx1 - fx0) * e, fy = fy0 + (fy1 - fy0) * e, sk = lt < 180 && bi >= 2 ? (1.0f - lt / 180.0f) * (float)SC(3) : 0.0f;
        float dx = sk * (Hash01(j * 7 + lt / 30) - 0.5f) * 2.0f, dy = sk * (Hash01(j * 11 + lt / 30) - 0.5f) * 2.0f;
        float split = lt < 170 && !g_fxLight ? (1.0f - lt / 170.0f) * (float)SC(big ? 9 : 4) : 0.0f;   /* the colour split on the cut */
        VitShot(&src[s], R, z, fx, fy, dx, dy, split);
        if (bi == 0 && kd[s] == 0) VitSweep(g, R, (p - 0.35f) / 0.5f, 30);
    } else {   /* no art at all: the game's colors, moving */
        POINT a = {R.left - (int)(W * p / 2), R.top}, z = {R.right + (int)(W * (1 - p) / 2), R.bottom}; GpBrush *lb = NULL; GdipCreateLineBrushI(&a, &z, A(c1, 255), A(c2, 255), 0, &lb);
        if (lb) { GdipFillRectangleI(g, lb, R.left, R.top, W, H); GdipDeleteBrush(lb); }
        VitSweep(g, R, p, 40);
    }
    if (lt < 140 && !g_fxLight) Fill(g, R.left, R.top, W, H, A(RGB0(255, 255, 255), (BYTE)(86 * (1.0f - lt / 140.0f))));   /* the flash on the cut */
    return (float)((t % (unsigned)cycle)) / cycle;
}
static void VitVideo(GpGraphics *g, RECT R, BOOL big)
{
    const BYTE *px = NULL; int w = 0, h = 0, st = 0; UINT ser = 0; (void)g; (void)big;
    /* 6.2 review: read with the player locked (a size change mid-file frees the buffers); the copy takes about 1 ms */
    if (VideoFrameLock(g_vit.vp, &px, &w, &h, &st, &ser) && px && w > 0 && h > 0) {
        int W = R.right - R.left, H = R.bottom - R.top; double s = (double)W / w > (double)H / h ? (double)W / w : (double)H / h;
        float cw = (float)(W / s), ch = (float)(H / s); VitPix f = {(BYTE *)px, w, h, st};
        VitBlit(&f, (w - cw) / 2, (h - ch) / 2, cw, ch, 0, 0, 0);
    }
    VideoFrameUnlock(g_vit.vp);
}
/* the panel's picture, composed once per frame into its own bitmap (a DIB, so GDI's text and GDI+ mix as in the window's
   back buffer): every paint of the home screen then copies it in one draw. Composed by the vitrine's timer, the video's
   frame message and Animate (outside WM_PAINT), or by the paint when nothing did yet. */
static BOOL g_vitVBig;   /* (g_vitDirty, g_vitVa: at the top) */
static GpBitmap *g_vitOv; static int g_vitOvW, g_vitOvH;   /* the glass over the picture (its veils and its reflection), made once per size */
static void VitDirty(void) { g_vitDirty = TRUE; }
static GpBitmap *VitOverlay(int W, int H, BOOL big)
{
    if (g_vitOv && g_vitOvW == W && g_vitOvH == H + (big ? 1 : 0)) return g_vitOv;
    FreeBmp(&g_vitOv); GpGraphics *g = NULL; GpBitmap *b = NewBitmap(W, H, &g); if (!b || !g) { if (g) GdipDeleteGraphics(g); FreeBmp(&b); return NULL; }
    VGrad(g, 0, H * 45 / 100, W, H - H * 45 / 100, A(RGB0(0, 0, 0), 0), A(RGB0(0, 0, 0), 215));
    VGrad(g, 0, 0, W, H / 5, A(RGB0(0, 0, 0), 90), A(RGB0(0, 0, 0), 0));
    if (!big) { GpPointF q[3] = {{0, 0}, {W * 0.62f, 0}, {0, H * 0.70f}};
        GpPath *sp = NULL; GdipCreatePath(0, &sp); if (sp) { GdipAddPathPolygon(sp, q, 3); float ea = W * 0.62f, eb = H * 0.70f, ed = ea * ea + eb * eb; GpPointF l0 = {0, 0}, l1 = {ea * eb * eb / ed, ea * ea * eb / ed}; GpBrush *lb = NULL;   /* ends on the edge */
            GdipCreateLineBrush(&l0, &l1, A(RGB0(255, 255, 255), 20), A(RGB0(255, 255, 255), 0), 0, &lb); if (lb) { GdipFillPath(g, lb, sp); GdipDeleteBrush(lb); } GdipDeletePath(sp); } }
    GdipDeleteGraphics(g); g_vitOv = b; g_vitOvW = W; g_vitOvH = H + (big ? 1 : 0); return b;
}
static void VitFreeV(void)
{
    FreeBmp(&g_vitV); FreeBmp(&g_vitOv);
    if (g_vitDC) { SelectObject(g_vitDC, g_vitOld); DeleteDC(g_vitDC); g_vitDC = NULL; }
    if (g_vitHB) { DeleteObject(g_vitHB); g_vitHB = NULL; }
    g_vitBits = NULL; g_vitVW = g_vitVH = 0; g_vitDirty = TRUE;
}
static float VitAlpha(DWORD now)
{
    DWORD showAt = g_vit.selAt + 600; if (g_vit.readyAt > showAt) showAt = g_vit.readyAt;
    return (!g_vit.readyAt || (int)(now - showAt) < 0) ? 0.0f : g_fxLight ? 1.0f : Clamp01((now - showAt) / 450.0f);
}
static void VitCompose(int W, int H, BOOL big)
{
    if (W < 8 || H < 8) return;
    if (!g_vitDC || W != g_vitVW || H != g_vitVH) {
        VitFreeV();
        BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), W, -H, 1, 32, BI_RGB}}; HDC sdc = GetDC(NULL);
        g_vitHB = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &g_vitBits, NULL, 0); ReleaseDC(NULL, sdc);
        if (!g_vitHB || !g_vitBits) { VitFreeV(); return; }
        g_vitDC = CreateCompatibleDC(NULL); g_vitOld = SelectObject(g_vitDC, g_vitHB);
        GdipCreateBitmapFromScan0(W, H, W * 4, 0x00022009 /* 32bppRGB */, (BYTE *)g_vitBits, &g_vitV);
        if (!g_vitV) { VitFreeV(); return; }
        g_vitVW = W; g_vitVH = H;
    }
#ifdef PORTHEON_TEST
    LARGE_INTEGER qf, q0, q1, q2; QueryPerformanceFrequency(&qf); QueryPerformanceCounter(&q0);
#endif
    /* GDI+ draws straight into the bitmap (much faster than through the DC); GDI's text goes through the DC on the same
       memory, flushed before GDI+ draws over it */
    HDC dc = g_vitDC; GpGraphics *g = NULL; GdipGetImageGraphicsContext((GpImage *)g_vitV, &g); if (!g) return;
    GdipSetSmoothingMode(g, 4); GdipSetPixelOffsetMode(g, 4); SetBkMode(dc, TRANSPARENT);
    int ox = g_ox, oy = g_oy; g_ox = g_oy = 0;   /* the text helpers move by the full-screen offset: none in this bitmap */
    RECT R = {0, 0, W, H}; DWORD now = GetTickCount();
    BOOL vid = VitHasVideo(); float a = VitAlpha(now), prog = -1.0f;
    if (a < 1.0f || (!vid && !g_vit.nsrc)) Fill(g, 0, 0, W, H, A(RGB0(10, 10, 14), 255));   /* the picture covers it all otherwise */
    if (a > 0.0f) {
        if (vid) VitVideo(g, R, big);
        else { DWORD t = g_fxLight ? 900 : VitLive() ? now - g_vit.editT0 : g_vit.stillT; prog = VitEdit(dc, g, R, t, big); GdiFlush(); }
        if (a < 1.0f) Fill(g, 0, 0, W, H, A(RGB0(10, 10, 14), (BYTE)(255 * (1.0f - a))));
        if (vid != g_vit.lastVid) { if (g_vit.readyAt && now - g_vit.readyAt > 700) g_vit.dipT0 = now; g_vit.lastVid = vid; }   /* the edit gave way to the video (or back) */
        if (g_vit.dipT0 && !g_fxLight && now - g_vit.dipT0 < 480) Fill(g, 0, 0, W, H, A(RGB0(10, 10, 14), (BYTE)(230 * (1.0f - Smooth((now - g_vit.dipT0) / 480.0f)))));
    } else VitSweep(g, R, (float)((now - g_vit.selAt) % 1400) / 1400.0f, 14);   /* waiting: smoked glass, a soft light passing */
    /* the glass: darker toward the caption, a faint sheen on the upper left (a reflection), the bar */
#ifdef PORTHEON_TEST
    QueryPerformanceCounter(&q1);
#endif
    {   GpBitmap *ov = VitOverlay(W, H, big); if (ov) { GdipSetInterpolationMode(g, 5); GdipDrawImageRectI(g, (GpImage *)ov, 0, 0, W, H); } }
    if (0 && prog >= 0.0f && a > 0.0f) { Fill(g, 0, H - SC(2), W, SC(2), A(RGB0(255, 255, 255), 40)); Fill(g, 0, H - SC(2), (int)(W * prog), SC(2), A(RGB0(255, 255, 255), 160)); }
    /* the caption: a tag at the top left, the game's name and what plays at the bottom left */
    int pad = big ? SC(40) : SC(16);
    if (0) {   /* 6.2: no tag and no caption over the clip (the owner's call) */
        const wchar_t *tag = vid ? L2(L"SEU VÍDEO", L"YOUR VIDEO") : L2(L"EDIT AO VIVO", L"LIVE EDIT");
        int tw = SpacedW(dc, g_fTag, tag, 1); RECT tg = {pad, pad * 3 / 4, pad + tw + SC(30), pad * 3 / 4 + SC(20)};
        Box(g, tg, SC(10), A(RGB0(10, 10, 14), 170), A(RGB0(255, 255, 255), 40), 1.0f);
        float pu = g_calm || !VitLive() ? 1.0f : 0.55f + 0.45f * sinf((float)(now % 1600) / 1600.0f * 6.2831853f);
        Dot(g, tg.left + SC(11), (tg.top + tg.bottom) / 2, SC(3), A(vid ? RGB0(255, 255, 255) : RGB0(255, 72, 88), (BYTE)(255 * pu)));
        Spaced(dc, g_fTag, RGB0(236, 236, 240), tg.left + SC(20), tg.top + SC(4), tag, 1); }
    if (0) {   HFONT nf = big ? g_fH2 : g_fTile; int nh = big ? SC(34) : SC(24), sh2 = big ? SC(22) : SC(18);
        const wchar_t *sub = vid ? g_vit.vidName : (big ? g_vit.nbig || g_vit.nsrc : g_vit.nsrc) ? L2(L"Feito pelo PORTHEON com a arte do seu jogo", L"Cut by PORTHEON from your game's own art")
                                                 : L2(L"Feito pelo PORTHEON nas cores do jogo", L"Made by PORTHEON in the game's colours");   /* 6.2 review: no art, no claim of art */
        RECT nr = {pad, H - pad - nh - sh2, W - pad - SC(70), H - pad - sh2}; TextR(dc, nf, RGB0(255, 255, 255), nr, VitName(), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        RECT sr = {pad, H - pad - sh2, W - pad - SC(70), H - pad}; TextR(dc, g_fSmall, RGB0(190, 190, 200), sr, sub, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS); }
    if (0 && !vid && g_vit.nsrc > 1 && a > 0.0f) {   /* the pictures in the edit: a dot each, the one on screen white */
        int cycle = 0; for (int i = 0; i < BEATS; i++) cycle += BEAT_MS[i];
        DWORD t = g_fxLight ? 900 : VitLive() ? now - g_vit.editT0 : g_vit.stillT; unsigned c = t / (unsigned)cycle; int r = (int)(t % (unsigned)cycle), bi = 0; while (r >= BEAT_MS[bi]) r -= BEAT_MS[bi++];
        int cur = bi == BEATS - 1 ? -1 : (int)((c * (BEATS - 1) + (unsigned)bi) % (unsigned)g_vit.nsrc);
        for (int i = 0; i < g_vit.nsrc; i++) Dot(g, W - pad - (g_vit.nsrc - 1 - i) * SC(12) - SC(3), H - pad - SC(8), SC(3), A(RGB0(255, 255, 255), i == cur ? 255 : 80));
    }
    if (big) {   /* full screen: how to leave */
        const wchar_t *h = L2(L"Esc  Voltar", L"Esc  Back"); int hw = TextW(dc, g_fSmall, h) + SC(28);
        RECT hr = {W - pad - hw, pad * 3 / 4, W - pad, pad * 3 / 4 + SC(28)}; Box(g, hr, SC(14), A(RGB0(10, 10, 14), 170), A(RGB0(255, 255, 255), 50), 1.0f);
        TextR(dc, g_fSmall, RGB0(236, 236, 240), hr, h, DT_CENTER | DT_VCENTER);
    }
    g_ox = ox; g_oy = oy; SetViewportOrgEx(dc, 0, 0, NULL); SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));   /* no font of ours stays selected (MakeFonts deletes them) */
    GdipDeleteGraphics(g); GdiFlush();
#ifdef PORTHEON_TEST
    {   QueryPerformanceCounter(&q2); static wchar_t lp[MAX_PATH]; static int has = -1; if (has < 0) has = GetEnvironmentVariableW(L"PORTHEON_TEST_PAINTLOG", lp, MAX_PATH) > 0;
        if (has) { FILE *o = _wfopen(lp, L"a"); if (o) { fwprintf(o, L"c %.2f %.2f\n", (q1.QuadPart - q0.QuadPart) * 1000.0 / qf.QuadPart, (q2.QuadPart - q1.QuadPart) * 1000.0 / qf.QuadPart); fclose(o); } } }
#endif
    g_vitDirty = FALSE; g_vitVa = a; g_vitVBig = big;
}
/* the whole panel: its shadow, the picture (one copy of the composed bitmap), the edge */
static void VitPaint(HDC dc, GpGraphics *g, RECT R, BOOL big, BOOL hot)
{
    int W = R.right - R.left, H = R.bottom - R.top, rad = big ? 0 : SC(VIT_RAD);
    if (g_vitDirty || !g_vitV || W != g_vitVW || H != g_vitVH || big != g_vitVBig) VitCompose(W, H, big);
    if (!big) for (int i = 5; i >= 0; i--) { int k = SC(2) * i; RECT s = {R.left - k, R.top + SC(5) - k, R.right + k, R.bottom + SC(5) + k}; Box(g, s, rad + k, A(RGB0(0, 0, 0), 26), 0, 0); }   /* 6.2: a soft shadow (it was one hard-edged block) */
    GpPath *p = RoundPath(R.left, R.top, W - 1, H - 1, rad);
    if (!big) CLIPPATH(g, p);
    if (g_vitV) { GdipSetInterpolationMode(g, 5); GdipDrawImageRectI(g, (GpImage *)g_vitV, R.left, R.top, W, H); }
    if (!big) ClipBase(g);
    if (!big && hot) { RECT o = R; InflateRect(&o, SC(3), SC(3)); Box(g, o, rad + SC(3), 0, A(RGB0(255, 255, 255), 255), 2.0f); }   /* 7.0: the mouse or the focus is marked outside the clip (the web view covers what is inside) */
    if (!big) {   /* the edge: dark under, light on top (lighter at the top, as GlassPanel) */
        GpPen *dk = NULL; GdipCreatePen1(A(RGB0(0, 0, 0), 120), 2.0f, 2, &dk); if (dk) { GdipDrawPath(g, dk, p); GdipDeletePen(dk); }
        POINT a0 = {0, R.top}, a1 = {0, R.bottom}; GpBrush *lb = NULL; GdipCreateLineBrushI(&a0, &a1, A(RGB0(255, 255, 255), 70), A(RGB0(255, 255, 255), 20), 0, &lb);
        if (lb) { GpPen *pen = NULL; GdipCreatePen2(lb, 1.0f, 2, &pen); if (pen) { GdipDrawPath(g, pen, p); GdipDeletePen(pen); } GdipDeleteBrush(lb); }
    }
    GdipDeletePath(p);
    if (big) { int pad = SC(40), hw = TextW(dc, g_fSmall, L2(L"Esc  Voltar", L"Esc  Back")) + SC(28); AddHit(R.right - pad - hw, R.top + pad * 3 / 4, R.right - pad, R.top + pad * 3 / 4 + SC(28), H_VITCLOSE); }
}
/* PaintHome: the vitrine in its place (a click or Cross opens it full screen). hits: FALSE in the paint the vitrine alone
   asked for (6.2 review: the hit list of the last whole paint stays as it is, it filled with copies of H_VITRINE) */
static void VitHomeEx(HDC dc, GpGraphics *g, BOOL hits)
{
    if (VitMode() == 2) return;
#ifdef PORTHEON_TEST   /* the UI shots have no web view: PORTHEON_TEST_VITBOX=1 draws a dark box (and the hit) where the clip plays */
    {   static int box = -1; if (box < 0) box = GetEnvironmentVariableW(L"PORTHEON_TEST_VITBOX", NULL, 0) > 0;
        if (box && !YtShowing() && !YtCovers() && !VitHasVideo()) {
            RECT B = VitRect(); int br = SC(VIT_RAD);
            for (int i = 5; i >= 0; i--) { int k = SC(2) * i; RECT s = {B.left - k, B.top + SC(5) - k, B.right + k, B.bottom + SC(5) + k}; Box(g, s, br + k, A(RGB0(0, 0, 0), 26), 0, 0); }
            if (g_hot == H_VITRINE) { RECT o = B; InflateRect(&o, SC(3), SC(3)); Box(g, o, br + SC(3), 0, A(RGB0(255, 255, 255), 255), 2.0f); }
            Box(g, B, br, A(RGB0(10, 10, 14), 255), A(RGB0(255, 255, 255), 70), 1.0f);
            if (hits) AddHit(B.left, B.top, B.right, B.bottom, H_VITRINE);
            return;
        } }
#endif
    if (!YtShowing() && !YtCovers() && !VitHasVideo()) return;   /* 6.2: only a clip shows here (or YouTube's page waiting for its clip); with none (offline, no list) the panel is not drawn */
    RECT R = VitRect(); BOOL hot = g_hot == H_VITRINE;
    if (Seen((RECT){R.left - SC(12), R.top - SC(12), R.right + SC(12), R.bottom + SC(24)})) VitPaint(dc, g, R, FALSE, hot);
    /* 6.2: no title/credit line under the panel (the owner's call; the creators are listed in videos\youtube.txt and the README) */
    if (hits) AddHit(R.left, R.top, R.right, R.bottom, H_VITRINE);
}
static void VitHome(HDC dc, GpGraphics *g) { VitHomeEx(dc, g, TRUE); }
/* the paint that only the vitrine asked for: the background under it and the vitrine (no other part of the home is there) */
static BOOL VitFastOk(const RECT *pr)
{
    if (g_view != V_HOME || g_overlay || VitMode() == 2 || g_motionOn || MotionT(g_viewT0, 280) < 1.0f) return FALSE;
    RECT r = VitRect(); OffsetRect(&r, g_ox, g_oy);
    return pr->left >= r.left && pr->top >= r.top && pr->right <= r.right && pr->bottom <= r.bottom;
}
static void VitOpenFull(void)
{
    if (VitMode() == 2) return;
    if (!YtShowing() && !YtCovers() && !VitHasVideo()) return;   /* 6.2: nothing to open without a clip (its page waiting for it counts) */
    g_overlay = OV_VITFULL; g_vit.fullT0 = GetTickCount(); g_focus = -1; Motion(320);
    if (!g_vit.bigAsked && g_vit.artAsked) VitAskArt(TRUE);
    InvalidateRect(g_wnd, NULL, FALSE);
}
static void PaintVitFull(HDC dc, GpGraphics *g, RECT rc)
{
    Fill(g, -g_ox, -g_oy, rc.right, rc.bottom, 0xFF000000);
    RECT F = VitFullRect(), S = VitRect(); float e = EaseOut3(MotionT(g_vit.fullT0, 320));
    RECT R = {S.left + (int)((F.left - S.left) * e), S.top + (int)((F.top - S.top) * e), S.right + (int)((F.right - S.right) * e), S.bottom + (int)((F.bottom - S.bottom) * e)};
    if ((YtShowing() || YtCovers()) && e >= 0.98f) {   /* YouTube full screen (6.2 fix: its waiting still too): the web view over the picture, a bar under it with the credit and the way back */
        return;   /* 6.2 (the owner: "only the video"): nothing over it or under it; M / R3 and Esc / Circle still answer */
    }
    /* 6.2 review: the picture is composed once at the full-screen size and only scaled during the zoom (it was composed
       again at every size of the zoom: new bitmaps and glass each frame, 8-10 ms) */
    int FW = F.right - F.left, FH = F.bottom - F.top, W = R.right - R.left, H = R.bottom - R.top;
    if (e >= 0.98f) VitPaint(dc, g, F, TRUE, FALSE);
    else {
        if (g_vitDirty || !g_vitV || FW != g_vitVW || FH != g_vitVH || !g_vitVBig) VitCompose(FW, FH, TRUE);
        int rad = (int)(SC(VIT_RAD) * (1.0f - e)); GpPath *p = RoundPath(R.left, R.top, W - 1, H - 1, rad > 0 ? rad : 1); CLIPPATH(g, p);
        if (g_vitV) { GdipSetInterpolationMode(g, 5 /* nearest: a scaled copy, about 1 ms */); GdipDrawImageRectI(g, (GpImage *)g_vitV, R.left, R.top, W, H); }
        ClipBase(g); GdipDeletePath(p);
    }
    if (e < 0.98f) AddHit(R.left, R.top, R.right, R.bottom, H_VITCLOSE);
    else AddHit(F.left, F.top, F.right, F.bottom - SC(1), H_VITCLOSE);
}

/* ---------------------------------------------------------------- the status chips and the quick options */
static int StatusChip(HDC dc, GpGraphics *g, int x, int y, COLORREF dot, const wchar_t *t, BOOL strong)
{
    int h = SC(26), w = TextW(dc, g_fSmall, t) + SC(24) + (dot ? SC(14) : 0);
    RECT r = {x, y, x + w, y + h};
    Box(g, r, h / 2, A(RGB0(14, 14, 20), strong ? 200 : 150), A(RGB0(255, 255, 255), strong ? 70 : 34), 1.0f);
    int tx = x + SC(12); if (dot) { Dot(g, tx + SC(4), y + h / 2, SC(4), A(dot, 255)); tx += SC(14); }
    RECT tr = {tx, y, r.right - SC(8), r.bottom}; TextR(dc, g_fSmall, strong ? RGB0(246, 246, 250) : RGB0(214, 214, 222), tr, t, DT_LEFT | DT_VCENTER);
    return w;
}
/* the chips of a built-in game: its state, the port's version, the game's version, the time played */
static void HomeChips(HDC dc, GpGraphics *g, int gsel, int st, int x, int y, int maxRight, float he)
{
    (void)he;
    wchar_t c[4][96]; COLORREF d[4] = {0, 0, 0, 0}; int n = 0;
    _snwprintf(c[n], 96, L"%s", SOON(gsel) ? L2(L"Em breve", L"Coming soon") : StatusText(st)); d[n++] = st == S_READY ? C_GOOD : st == S_RUNNING ? C_GOOD : st == S_PICK || st == S_NOPORT ? C_ORANGE : C_BAD;
    if (g_ver[gsel][0]) _snwprintf(c[n++], 96, L"%s", g_ver[gsel]);
    {   const GameVer *hv = st == S_READY ? GameVerOf(gsel) : NULL; if (hv && hv->tag[0]) _snwprintf(c[n++], 96, L"%s %s", L2(L"Jogo", L"Game"), hv->tag); }
    {   wchar_t pt[128]; FmtPlayTime(gsel, pt, 128); wchar_t k[48]; _snwprintf(k, 48, L"playtime_%s", G[gsel].id);
        if (PIni(k, 0) >= 60) _snwprintf(c[n++], 96, L"%s", pt);
        else if (GamePlayed(gsel)) _snwprintf(c[n++], 96, L"%s", L2(L"Jogado por menos de 1 min", L"Played for under 1 min")); }   /* 6.2 review: as the Atividade card; 6.2 owner: no "Novo" chip */
    for (int i = 0; i < n; i++) {
        int w = TextW(dc, g_fSmall, c[i]) + SC(24) + (d[i] ? SC(14) : 0);
        if (x + w > maxRight) {   /* 7.0: the column ends at the video: "47 min jogados  ·  última vez: ontem" keeps its first half (the Atividade card says when) */
            wchar_t *cut = wcsstr(c[i], L"  ·  "); if (!cut) break;
            *cut = 0; w = TextW(dc, g_fSmall, c[i]) + SC(24) + (d[i] ? SC(14) : 0); if (x + w > maxRight) break;
        }
        x += StatusChip(dc, g, x, y, d[i], c[i], i == 0) + SC(8);
    }
}
/* a quick option: label and value in a glass pill with a small chevron; kinds: 0 a list (pop-over), 1 a switch, 2 a hub tab */
typedef struct { const Field *f; int kind, tab; const wchar_t *label; wchar_t value[64]; } QChip;
static QChip g_qc[6]; static int g_nqc; static RECT g_qcR[6];
#define QPOP_MAX 16        /* the rows a pop-over shows (and takes) */
static int g_qOpen = -1, g_qN, g_qGame = -1; static DWORD g_qT0;   /* the open pop-over: its chip, its rows, the game the chips were built for */
static void QuickBuild(int gsel)
{
    g_nqc = 0; g_qGame = gsel; if (g_csel >= 0) return;
    #define QL(F, L) do { QChip *q = &g_qc[g_nqc++]; q->f = &(F); q->kind = 0; q->label = (L); int i = OptIndex(q->f, GetInt(gsel, q->f->key, q->f->def)); _snwprintf(q->value, 64, L"%s", i >= 0 ? OptName(&q->f->opts[i]) : L"-"); } while (0)
    #define QS(F, L) do { QChip *q = &g_qc[g_nqc++]; q->f = &(F); q->kind = 1; q->label = (L); _snwprintf(q->value, 64, L"%s", GetInt(gsel, q->f->key, q->f->def) ? L2(L"Ligada", L"On") : L2(L"Desligada", L"Off")); } while (0)
    #define QT(TAB, L, V) do { QChip *q = &g_qc[g_nqc++]; q->f = NULL; q->kind = 2; q->tab = (TAB); q->label = (L); _snwprintf(q->value, 64, L"%s", (V)); } while (0)
    if (St(gsel) != S_READY) return;
    if (G[gsel].ps5) {
        QL(F_GFX, L2(L"Qualidade", L"Quality"));
        if (UFC6G(gsel)) QL(F6_FPS, L"FPS"); else QL(F5_FPS, L"FPS");
        QS(F56_CAM3P, L2(L"Câmera de TV", L"TV camera"));
    } else if (PS4G(gsel)) {
        QL(F4_RES, L2(L"Resolução", L"Resolution"));
        QS(F4_CAM3P, gsel == 1 ? L2(L"Câmera do ombro", L"Shoulder camera") : L2(L"Câmera de TV", L"TV camera"));
        CheatsLoad(gsel); { wchar_t v[32]; _snwprintf(v, 32, L2(L"%d ligadas", L"%d on"), CheatsOn(gsel)); if (g_ncheat > 0) QT(TAB_CHEATS, L2(L"Trapaças", L"Cheats"), v); }
        QT(TAB_SAVES, L"Saves", L2(L"Abrir", L"Open"));
    } else QT(TAB_OVERVIEW, L2(L"Opções do jogo", L"Game options"), L2(L"Abrir", L"Open"));
    #undef QL
    #undef QS
    #undef QT
    for (int i = 0; i < g_nqc; i++) { wchar_t *b = wcsstr(g_qc[i].value, L" ("); if (b && b != g_qc[i].value) *b = 0; }   /* "Automático (o melhor...)" -> "Automático" */
}
/* one pill (7.0 fix: its own function: the open pop-over draws its pill again over the dimmed column) */
static void QuickPill(HDC dc, GpGraphics *g, int i, RECT r, BOOL lit)
{
    QChip *q = &g_qc[i]; wchar_t t[160]; _snwprintf(t, 160, L"%s:  ", q->label);
    int x = r.left, y = r.top, h = r.bottom - r.top, lw = TextW(dc, g_fSmall, t), vw = TextW(dc, g_fBtn, q->value);
    Box(g, r, h / 2, A(RGB0(14, 14, 20), lit ? 226 : 176), A(RGB0(255, 255, 255), lit ? 230 : 40), lit ? 1.6f : 1.0f);
    RECT lr = {x + SC(18), y, x + SC(18) + lw, r.bottom}; TextR(dc, g_fSmall, RGB0(170, 170, 182), lr, t, DT_LEFT | DT_VCENTER);
    RECT vr = {lr.right, y, lr.right + vw + SC(2), r.bottom}; TextR(dc, g_fBtn, RGB0(255, 255, 255), vr, q->value, DT_LEFT | DT_VCENTER);
    Chevron(g, r.right - SC(16), y + h / 2 + (q->kind == 2 ? 0 : SC(1)), SC(4), q->kind == 2 ? 1 : 0, A(RGB0(220, 220, 228), 255));
}
static void QuickRow(HDC dc, GpGraphics *g, int gsel, int x, int y, int maxRight)
{
    if (SOON(gsel)) { g_nqc = 0; return; }   /* 6.2 */
    QuickBuild(gsel);
    int x0 = x, row2 = 0;   /* 7.0: the pills wrap once to a second row (the column left of the video is narrower) */
    for (int i = 0; i < g_nqc; i++) {
        QChip *q = &g_qc[i]; wchar_t t[160]; _snwprintf(t, 160, L"%s:  ", q->label);
        int h = SC(34), lw = TextW(dc, g_fSmall, t), vw = TextW(dc, g_fBtn, q->value), w = SC(18) + lw + vw + SC(34);
        if (x + w > maxRight && i > 0 && !row2 && x0 + w <= maxRight) { row2 = 1; x = x0; y += SC(42); }
        if (x + w > maxRight) { g_nqc = i; break; }
        RECT r = {x, y, x + w, y + h}; BOOL hot = g_hot == H_QCHIP + i, open = g_overlay == OV_QUICK && g_qOpen == i; g_qcR[i] = r;
        if (hot) Glow(g, (r.left + r.right) / 2, (r.top + r.bottom) / 2, w * 6 / 10, h, A(RGB0(255, 255, 255), 26));
        QuickPill(dc, g, i, r, hot || open);
        AddHit(r.left, r.top, r.right, r.bottom, H_QCHIP + i);
        x += w + SC(10);
    }
}
/* Released (page 2 is laid out upward from the cards): how many rows QuickRow will use (0, 1 or 2), by the same rule */
static __attribute__((unused)) int QuickRows(HDC dc, int gsel, int x, int maxRight)   /* (unused with HOME_PAGES 0) */
{
    if (SOON(gsel)) return 0;
    QuickBuild(gsel);
    int x0 = x, rows = 0, row2 = 0;
    for (int i = 0; i < g_nqc; i++) {
        QChip *q = &g_qc[i]; wchar_t t[160]; _snwprintf(t, 160, L"%s:  ", q->label);
        int w = SC(18) + TextW(dc, g_fSmall, t) + TextW(dc, g_fBtn, q->value) + SC(34);
        if (x + w > maxRight && i > 0 && !row2 && x0 + w <= maxRight) { row2 = 1; x = x0; }
        if (x + w > maxRight) break;
        rows = 1 + row2; x += w + SC(10);
    }
    return rows;
}
static void QuickOpen(int i)
{
    if (i < 0 || i >= g_nqc || g_qGame != g_sel || g_csel >= 0) return;   /* 6.2 review: the chips are the shown game's (ToggleSwitch and DropdownApply act on g_sel) */
    QChip *q = &g_qc[i];
    if (q->kind == 1) { ToggleSwitch(q->f); Msg(C_GOOD, L"%s: %s", q->label, GetInt(g_qGame, q->f->key, q->f->def) ? L2(L"ligada. Salvo.", L"on. Saved.") : L2(L"desligada. Salvo.", L"off. Saved.")); InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (q->kind == 2) { SetView(V_GAME); g_tab = q->tab; LoadEdits(); return; }
    g_qOpen = i; g_qN = q->f->nopts > QPOP_MAX ? QPOP_MAX : q->f->nopts; g_qT0 = GetTickCount(); g_overlay = OV_QUICK; Motion(200);
    int cur = OptIndex(q->f, GetInt(g_qGame, q->f->key, q->f->def)); g_focus = H_QROW + (cur >= 0 && cur < g_qN ? cur : 0); g_hot = g_kbNav ? g_focus : -1;
    InvalidateRect(g_wnd, NULL, FALSE);
}
static void PaintQuickPop(HDC dc, GpGraphics *g, RECT rc)
{
    if (g_qOpen < 0 || g_qOpen >= g_nqc || !g_qc[g_qOpen].f || g_qGame != g_sel) { g_overlay = OV_NONE; g_qOpen = -1; return; }
    QChip *q = &g_qc[g_qOpen]; const Field *f = q->f; RECT a = g_qcR[g_qOpen];
    int rowH = SC(34), n = g_qN, w = SC(300), h = SC(44) + n * rowH + SC(10);   /* (g_qN: QPOP_MAX rows at most, drawn and taken alike) */
    for (int i = 0; i < n; i++) { int tw = TextW(dc, g_fBtn, OptName(&f->opts[i])) + SC(70); if (tw > w) w = tw; }
    if (w > SC(480)) w = SC(480);
    float e = EaseOut3(MotionT(g_qT0, 160)); int drop = (int)(SC(8) * (1.0f - e));
    int x = a.left, y = a.top - SC(8) - h - drop;   /* above the chip: the cards are under it */
    if (y < SC(230) && SC(230) + h <= SC(592)) y = SC(230) + drop;   /* 7.0: a long list stays under the row of tiles (it then covers its own pill, as a menu does); fix: 230 (222 sat under the chosen tile's outline) */
    if (y < SC(96)) y = a.bottom + SC(8) + drop;
    if (x + w > SC(BASE_W - MX)) x = SC(BASE_W - MX) - w;
    if (HOME_PAGES) {   /* Released (page 2: the pills stand above the cards): the list opens under its pill, over the cards; one too long for that goes up over the pill, as a menu does */
        y = a.bottom + SC(8) + drop; if (y + h > SC(790)) y = SC(790) - h; if (y < SC(76)) y = SC(76);
    }
    RECT r = {x, y, x + w, y + h};
    {   /* 7.0 fix: the name's column goes dim under the list (half-covered chips and pills stuck out around it); never over the video */
        int va = (int)(196 * e), vr = SC(VIT_X - 16), fade = SC(48), vt = SC(HPV(84, 232)), vb = SC(HPV(744, 594)) /* the cards start at 600 */, vf = SC(14), x0 = -g_ox, x1 = vr - fade;
        if (HOME_PAGES) {   /* Released review: page 2 has no single column under the list: everything under the top bar goes dim, evenly (no slab, no edge through "Voltar", which PaintHome leaves out meanwhile), and the video is drawn again on top, as it was */
            UINT32 dcol = A(RGB0(0, 0, 0), (BYTE)(150 * e)); int dt = SC(64), df = SC(32);
            VGrad(g, x0, dt, rc.right, df, A(RGB0(0, 0, 0), 0), dcol); Fill(g, x0, dt + df, rc.right, rc.bottom - g_oy - dt - df, dcol);
            VitHomeEx(dc, g, FALSE);
        } else
        for (int s = 0; s <= 16; s++) {   /* the solid part, then 16 slices that fade toward the video (each with its own soft top and bottom: no notch at the corners) */
            UINT32 vc = A(RGB0(0, 0, 0), (BYTE)(s ? va * (33 - 2 * s) / 32 : va)); int sx = s ? x1 + (s - 1) * fade / 16 : x0, sw = s ? x1 + s * fade / 16 - sx : x1 - x0;
            Fill(g, sx, vt + vf, sw, vb - vt - 2 * vf, vc); VGrad(g, sx, vt, sw, vf, A(RGB0(0, 0, 0), 0), vc); VGrad(g, sx, vb - vf, sw, vf, vc, A(RGB0(0, 0, 0), 0));
        }
        QuickPill(dc, g, g_qOpen, a, TRUE); }
    for (int k = 3; k >= 1; k--) { RECT s = {r.left - SC(2 * k), r.top + SC(6), r.right + SC(2 * k), r.bottom + SC(4 * k) + SC(4)}; Box(g, s, SC(16) + SC(4 * k), A(RGB0(0, 0, 0), (BYTE)(40 * e)), 0, 0); }
    GlassPanel(g, r, SC(16), 228);
    RECT tr = {x + SC(18), y + SC(10), r.right - SC(16), y + SC(36)}; TextR(dc, g_fSection, RGB0(170, 170, 182), tr, q->label, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    int cur = OptIndex(f, GetInt(g_qGame, f->key, f->def));
    for (int i = 0; i < n; i++) {
        RECT rr = {x + SC(8), y + SC(40) + i * rowH, r.right - SC(8), y + SC(40) + (i + 1) * rowH}; BOOL hot = g_hot == H_QROW + i || g_focus == H_QROW + i;
        if (hot) Box(g, rr, SC(10), A(RGB0(255, 255, 255), 30), A(RGB0(255, 255, 255), 120), 1.0f);
        RECT t = {rr.left + SC(12), rr.top, rr.right - SC(34), rr.bottom}; TextR(dc, i == cur ? g_fBtn : g_fText, i == cur || hot ? RGB0(255, 255, 255) : RGB0(214, 214, 222), t, OptName(&f->opts[i]), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        if (i == cur) IconCheck(g, rr.right - SC(28), rr.top + (rowH - SC(14)) / 2, SC(14), A(RGB0(255, 255, 255), 255));
        AddHit(rr.left, rr.top, rr.right, rr.bottom, H_QROW + i);
    }
}

/* ---------------------------------------------------------------- the live cards */
static int g_cpuPct = -1, g_cpuHist[32], g_cpuN; static ULONGLONG g_ramFree, g_ramTotal;
static PadLive g_pads[4]; static int g_npads = -1; static DWORD g_padsHeldShown;
static RECT g_liveR[4]; static int g_cardKind[4];   /* what each home card is (0 the game's own, 1 Seu PC, 2 Controles, 3 Atividade) */
static void LiveDirty(int kind) { for (int k = 0; k < 4; k++) if (g_cardKind[k] == kind && g_liveR[k].right > g_liveR[k].left) { RECT r = g_liveR[k]; OffsetRect(&r, g_ox, g_oy); InvalidateRect(g_wnd, &r, FALSE); } }
static void LiveSample(void)
{
    static ULONGLONG li, lk, lu; FILETIME fi, fk, fu;
    if (GetSystemTimes(&fi, &fk, &fu)) {
        ULONGLONG i = ((ULONGLONG)fi.dwHighDateTime << 32) | fi.dwLowDateTime, k = ((ULONGLONG)fk.dwHighDateTime << 32) | fk.dwLowDateTime, u = ((ULONGLONG)fu.dwHighDateTime << 32) | fu.dwLowDateTime;
        if (lk) { ULONGLONG di = i - li, dt = (k - lk) + (u - lu); if (dt) { g_cpuPct = (int)(100 - di * 100 / dt); if (g_cpuPct < 0) g_cpuPct = 0; if (g_cpuPct > 100) g_cpuPct = 100;
            if (g_cpuN < 32) g_cpuHist[g_cpuN++] = g_cpuPct; else { memmove(g_cpuHist, g_cpuHist + 1, 31 * sizeof(int)); g_cpuHist[31] = g_cpuPct; } } }
        li = i; lk = k; lu = u;
    }
    MEMORYSTATUSEX ms = {sizeof(ms)}; if (GlobalMemoryStatusEx(&ms)) { g_ramFree = ms.ullAvailPhys; g_ramTotal = ms.ullTotalPhys; }
}
/* Animate: the PC every 2 s and the pads every second, only on the home screen with PORTHEON in front, never in a game */
static void LiveTick(void)
{
    {   static BOOL primed; if (!primed) { primed = TRUE; LiveSample(); } }   /* 6.2 review: the CPU's baseline at start, so the first paint of Seu PC has a value */
    if (g_view != V_HOME || g_overlay || g_proc || !LiveOn() || g_csel >= 0) return;
    if (!VitFront()) return;
    DWORD now = GetTickCount(); static DWORD pcAt, padAt;
    if (now - pcAt >= 2000 || !pcAt || (g_cpuPct < 0 && now - pcAt >= 300)) { pcAt = now; LiveSample(); LiveDirty(1); }   /* (the first value 0.3 s after the baseline) */
    if (now - padAt >= 1000 || g_npads < 0) { padAt = now; PadLive p[4]; int n = PadList(p, 4); if (n != g_npads || memcmp(p, g_pads, sizeof(PadLive) * (n > 0 ? n : 0))) { memcpy(g_pads, p, sizeof(p)); g_npads = n; LiveDirty(2); } }
    if ((g_padHeld & 0x7FFF) != g_padsHeldShown) { g_padsHeldShown = g_padHeld & 0x7FFF; LiveDirty(2); }
}
static void LiveCardBox(HDC dc, GpGraphics *g, RECT r, int k, int icon, const wchar_t *title, const wchar_t *right)
{
    BOOL hot = g_hot == H_HOMECARD + k;
    if (Seen(r)) {
        if (hot) Glow(g, (r.left + r.right) / 2, (r.top + r.bottom) / 2, (r.right - r.left) * 6 / 10, (r.bottom - r.top), A(RGB0(255, 255, 255), 30));
        SkinCard(g, r, hot);
    }
    int is = SC(20), x = r.left + SC(20), y = r.top + SC(18); UINT32 ic = A(RGB0(255, 255, 255), 255);
    switch (icon) { case 5: IconChip(g, x, y, is, ic); break; case 6: IconPad(g, x, y, is, ic); break; case 7: IconInfo(g, x, y, is, ic); break; default: IconMonitor(g, x, y, is, ic); }
    int rw = right && right[0] ? TextW(dc, g_fSmall, right) : 0;
    RECT tt = {x + is + SC(12), y - SC(4), r.right - SC(20) - rw - SC(8), y + is + SC(4)}; TextR(dc, g_fBtn, RGB0(255, 255, 255), tt, title, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    if (rw) { RECT rr = {r.right - SC(18) - rw, y - SC(4), r.right - SC(18), y + is + SC(4)}; TextR(dc, g_fSmall, RGB0(170, 170, 182), rr, right, DT_RIGHT | DT_VCENTER); }
    AddHit(r.left, r.top, r.right, r.bottom, H_HOMECARD + k);
}
static void ShortHw(wchar_t *c, wchar_t *v)   /* "AMD Ryzen 5 5500" -> "Ryzen 5 5500", "NVIDIA GeForce RTX 4060" -> "RTX 4060" */
{
    static const wchar_t *const cut[] = {L"AMD ", L"Intel(R) Core(TM) ", L"Intel(R) ", L"NVIDIA GeForce ", L"NVIDIA ", L"AMD Radeon(TM) ", L"Intel(R) Arc(TM) "};
    _snwprintf(c, 80, L"%s", g_cpuName[0] ? g_cpuName : L"CPU"); c[79] = 0; _snwprintf(v, 80, L"%s", g_haveGpu ? g_gpu.name : L""); v[79] = 0;
    for (int i = 0; i < (int)(sizeof(cut) / sizeof(cut[0])); i++) { size_t n = wcslen(cut[i]);
        if (!wcsncmp(c, cut[i], n)) memmove(c, c + n, (wcslen(c + n) + 1) * sizeof(wchar_t));
        if (!wcsncmp(v, cut[i], n)) memmove(v, v + n, (wcslen(v + n) + 1) * sizeof(wchar_t)); }
    wchar_t *at = wcsstr(c, L" with "); if (at) *at = 0; at = wcsstr(c, L" CPU @"); if (at) *at = 0;
    for (size_t n = wcslen(c); n && c[n - 1] == L' '; ) c[--n] = 0; for (size_t n = wcslen(v); n && v[n - 1] == L' '; ) v[--n] = 0;
}
static void LivePcCard(HDC dc, GpGraphics *g, RECT r, int k)
{
    if (g_cpuPct < 0) LiveSample();   /* (a value when the baseline is older than this paint) */
    wchar_t cpu[24] = L""; if (g_cpuPct >= 0) _snwprintf(cpu, 24, L"CPU %d%%", g_cpuPct);
    int tier = PcTier(); const wchar_t *vt = tier == 2 ? L2(L"Pronto no máximo", L"Ready at max") : tier == 1 ? L2(L"Pronto, ajustado", L"Ready, tuned") : L2(L"Modo leve", L"Light mode");
    LiveCardBox(dc, g, r, k, 5, L2(L"Seu PC", L"Your PC"), vt);
    {   int vw = TextW(dc, g_fSmall, vt); Dot(g, r.right - SC(18) - vw - SC(10), r.top + SC(28), SC(3), A(tier ? C_GOOD : C_ORANGE, 255)); }
    int x = r.left + SC(20), w = r.right - r.left - SC(40), y = r.top + SC(48);
    {   /* the CPU's last minute, a thin line on the right of the processor's line */
        int sw = SC(56), sh = SC(14), sx = r.right - SC(20) - sw, sy = y + SC(23);
        if (g_cpuN > 1) { GpPointF pt[32];   /* (no line until there are two values: an empty baseline read as "no data") */ for (int i = 0; i < g_cpuN; i++) { pt[i].X = sx + (float)sw * i / 31.0f + (float)sw * (32 - g_cpuN) / 31.0f; pt[i].Y = sy + sh - (float)sh * g_cpuHist[i] / 100.0f; }
            GpPen *pn = NULL; GdipCreatePen1(A(RGB0(255, 255, 255), 170), 1.4f, 2, &pn); if (pn) { for (int i = 1; i < g_cpuN; i++) GdipDrawLineI(g, pn, (int)pt[i - 1].X, (int)pt[i - 1].Y, (int)pt[i].X, (int)pt[i].Y); GdipDeletePen(pn); } }
        if (g_cpuN > 1) Fill(g, sx, sy + sh, sw, 1, A(RGB0(255, 255, 255), 40));
        if (cpu[0]) { RECT cr = {sx - SC(30), y, r.right - SC(20), y + SC(20)}; TextR(dc, g_fSmall, RGB0(236, 236, 240), cr, cpu, DT_RIGHT | DT_VCENTER); }
    }
    w -= SC(70);   /* the two lines leave room for the CPU's load */
    wchar_t c[80], v[80], l1[160], l2[160]; ShortHw(c, v);
    if (v[0]) _snwprintf(l1, 160, L"%s  ·  %llu GB", v, (unsigned long long)((g_gpu.vram + (512ull << 20)) >> 30)); else _snwprintf(l1, 160, L"%s", L2(L"Placa de vídeo não identificada", L"Graphics card not identified"));
    _snwprintf(l2, 160, L"%s  ·  %d threads", c, g_threads);
    RECT a = {x, y, x + w, y + SC(20)}; TextR(dc, g_fSmall, RGB0(236, 236, 240), a, l1, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    RECT b = {x, y + SC(20), x + w, y + SC(40)}; TextR(dc, g_fSmall, RGB0(170, 170, 182), b, l2, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    {   /* free memory: a thin meter (white; orange under 4 GB free, red under 2 GB) */
        double fg = g_ramFree / 1073741824.0, tg = g_ramTotal / 1073741824.0; float used = g_ramTotal ? 1.0f - (float)(fg / tg) : 0;
        COLORREF mc = fg < 2.0 ? C_BAD : fg < 4.0 ? C_ORANGE : RGB0(255, 255, 255);
        w += SC(70); int my = y + SC(50); RECT tr = {x, my, x + w, my + SC(6)}; Box(g, tr, SC(3), A(RGB0(255, 255, 255), 34), 0, 0);
        RECT fr = {x, my, x + (int)(w * used), my + SC(6)}; if (fr.right > fr.left + SC(6)) Box(g, fr, SC(3), A(mc, 230), 0, 0);
        wchar_t m[96]; _snwprintf(m, 96, L2(L"RAM livre  %.1f de %.0f GB", L"Free RAM  %.1f of %.0f GB"), fg, tg);
        if (g_lang == LANG_PT) for (wchar_t *q = m; *q; q++) if (*q == L'.') *q = L',';
        RECT mr = {x, my + SC(10), x + w, my + SC(30)}; TextR(dc, g_fSmall, RGB0(170, 170, 182), mr, m, DT_LEFT | DT_VCENTER);
    }
}
static const wchar_t *PadKind(int t) { return t == 3 ? L"DualSense" : t == 2 ? L"DualShock 4" : t == 1 ? L"Xbox" : t == 4 ? L"Switch Pro" : t == 5 ? L"DualShock 3" : L2(L"Controle", L"Controller"); }
static const wchar_t *PadButtonName(int type, DWORD held)
{
    BOOL ps = type == 2 || type == 3 || type == 5;
    static const struct { int pb; const wchar_t *ps, *xb, *pe; } B[] = {{PB_A, L"X", L"A", L"Cross"}, {PB_B, L"Bola", L"B", L"Circle"}, {PB_X, L"Quadrado", L"X", L"Square"}, {PB_Y, L"Triângulo", L"Y", L"Triangle"} /* review: words, as the Controller screen (ui_padmap62.c): no maker's symbols */, {PB_LB, L"L1", L"LB"}, {PB_RB, L"R1", L"RB"},
        {PB_START, L"Options", L"Menu"}, {PB_BACK, L"Create", L"View"}, {PB_UP, L"↑", L"↑"}, {PB_DOWN, L"↓", L"↓"}, {PB_LEFT, L"←", L"←"}, {PB_RIGHT, L"→", L"→"}, {PB_LS, L"L3", L"LS"}, {PB_RS, L"R3", L"RS"}};
    for (int i = 0; i < (int)(sizeof(B) / sizeof(B[0])); i++) if (held & (1u << B[i].pb)) {
        if (ps && B[i].pb == PB_START && type != 3) return type == 5 ? L"Start" : L"Options";   /* 6.2 review: DualShock 3 Start, DualShock 4 Options */
        if (ps && B[i].pb == PB_BACK) return type == 3 ? L"Create" : type == 2 ? L"Share" : L"Select";   /* DualSense Create, DualShock 4 Share, DualShock 3 Select */
        return ps ? (B[i].pe ? L2(B[i].ps, B[i].pe) : B[i].ps) : B[i].xb;
    }
    return NULL;
}
static void LivePadsCard(HDC dc, GpGraphics *g, RECT r, int k)
{
    if (g_npads < 0) { g_npads = PadList(g_pads, 4); }
    wchar_t right[48] = L""; if (g_npads > 0) _snwprintf(right, 48, g_npads == 1 ? L2(L"1 conectado", L"1 connected") : L2(L"%d conectados", L"%d connected"), g_npads);
    LiveCardBox(dc, g, r, k, 6, L2(L"Controles", L"Controllers"), right);
    int x = r.left + SC(20), w = r.right - r.left - SC(40), y = r.top + SC(48);
    if (g_npads <= 0) {
        RECT a = {x, y, x + w, y + SC(20)}; TextR(dc, g_fSmall, RGB0(236, 236, 240), a, L2(L"Nenhum controle conectado", L"No controller connected"), DT_LEFT | DT_VCENTER);
        RECT b = {x, y + SC(20), x + w, y + SC(40)}; TextR(dc, g_fSmall, RGB0(150, 150, 162), b, L2(L"USB ou Bluetooth: DualSense, DualShock 4, Xbox", L"USB or Bluetooth: DualSense, DualShock 4, Xbox"), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        for (int i = 0; i < 4; i++) { int cx = x + SC(10) + i * SC(30), cy = y + SC(62); Ring(g, cx, cy, SC(9), A(RGB0(255, 255, 255), 50), 1.0f); wchar_t s[4]; _snwprintf(s, 4, L"%d", i + 1); RECT nr = {cx - SC(9), cy - SC(9), cx + SC(9), cy + SC(9)}; TextR(dc, g_fTag, RGB0(130, 130, 142), nr, s, DT_CENTER | DT_VCENTER); }
        return;
    }
    int rows = g_npads > 3 ? 3 : g_npads;
    for (int i = 0; i < rows; i++) {
        PadLive *p = &g_pads[i]; int ry = y + i * SC(24); RECT row = {x - SC(8), ry - SC(2), x + w + SC(8), ry + SC(22)};
        const wchar_t *btn = i == 0 ? PadButtonName(p->type, g_padsHeldShown) : NULL;
        if (btn) Box(g, row, SC(8), A(RGB0(255, 255, 255), 30), 0, 0);
        int cx = x + SC(9), cy = ry + SC(10); Dot(g, cx, cy, SC(9), A(RGB0(255, 255, 255), i == 0 ? 230 : 40));
        wchar_t s[4]; _snwprintf(s, 4, L"%d", i + 1); RECT nr = {cx - SC(9), cy - SC(9), cx + SC(9), cy + SC(9)}; TextR(dc, g_fTag, i == 0 ? RGB0(16, 16, 20) : RGB0(230, 230, 236), nr, s, DT_CENTER | DT_VCENTER);
        wchar_t nm[96]; _snwprintf(nm, 96, L"%s%s", PadKind(p->type), p->wired == 1 ? L"  ·  USB" : p->wired == 0 ? L"  ·  Bluetooth" : L"");
        int bw = SC(46); if (btn && TextW(dc, g_fBtn, btn) + SC(2) > bw) bw = TextW(dc, g_fBtn, btn) + SC(2);   /* (a word: "Triângulo") */
        RECT tr = {x + SC(26), ry, x + w - bw - SC(6), ry + SC(20)}; TextR(dc, g_fSmall, RGB0(236, 236, 240), tr, nm, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        RECT br = {x + w - bw, ry + SC(4), x + w, ry + SC(16)};
        if (btn) { RECT bt = {br.left - SC(6), ry, x + w, ry + SC(20)}; TextR(dc, g_fBtn, RGB0(255, 255, 255), bt, btn, DT_RIGHT | DT_VCENTER); }
        else if (p->batt >= 0) {   /* four segments */
            RECT body = {br.left + SC(10), br.top, br.right - SC(3), br.bottom}; Box(g, body, SC(2), 0, A(RGB0(255, 255, 255), 150), 1.0f); Fill(g, body.right, br.top + SC(4), SC(2), SC(4), A(RGB0(255, 255, 255), 150));
            int seg = (p->batt + 24) / 25, sw = (body.right - body.left - SC(4)) / 4;
            for (int q = 0; q < 4; q++) Fill(g, body.left + SC(2) + q * sw, body.top + SC(2), sw - SC(1), body.bottom - body.top - SC(4), A(q < seg ? (seg <= 1 ? C_BAD : RGB0(255, 255, 255)) : RGB0(255, 255, 255), q < seg ? 230 : 24));
        } else if (p->wired == 1) { RECT bt = {br.left - SC(10), ry, x + w, ry + SC(20)}; TextR(dc, g_fTag, RGB0(150, 150, 162), bt, L2(L"CABO", L"WIRED"), DT_RIGHT | DT_VCENTER); }
    }
    if (g_npads > 3) { RECT mr = {x, y + 3 * SC(24), x + w, y + 3 * SC(24) + SC(18)}; wchar_t m[48]; _snwprintf(m, 48, L2(L"+%d controle(s)", L"+%d controller(s)"), g_npads - 3); TextR(dc, g_fSmall, RGB0(150, 150, 162), mr, m, DT_LEFT | DT_VCENTER); }
    else { RECT hr = {x, r.bottom - SC(26), x + w, r.bottom - SC(8)}; TextR(dc, g_fSmall, RGB0(130, 130, 142), hr, L2(L"Aperte o cartão: o controle 1 vibra", L"Press the card: controller 1 buzzes"), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS); }
}
/* the last session (OnExited/LateExited -> SessionRecord): when, how long, how it ended; the logs one press away.
   6.2 review: one rule for "ended with an error", the one StreakAfter uses: 0, 1 and Ctrl+C/close (0xC000013A) are normal
   ends; for UFC Undisputed 3 only an NTSTATUS error (0xC0000000 and up) is a crash (UFC3.exe passes the game's own code,
   and a player's own close can end with another one); for UFC 5/6 the code is the engine's own, which the launcher
   leaves in its ini as last_code (OnExited gets the launcher's) */
static DWORD SessionCode(int g, DWORD code) { return g >= 0 && g < NGAMES && G[g].ps5 ? (DWORD)GetInt(g, L"last_code", (int)code) : code; }
static BOOL SessionCrash(int g, DWORD code)
{
    if (code == 0 || code == 1 || code == 0xC000013Au) return FALSE;
    if (X360G(g) && code < 0xC0000000u) return FALSE;
    return TRUE;
}
static void CrashUnreadKey(int g, wchar_t *k) { _snwprintf(k, 48, L"crash_unread_%s", G[g].id); }
static void SessionRecord(int g, ULONGLONG ms, DWORD code)
{
    if (g < 0 || g >= NGAMES) return;
    code = SessionCode(g, code);
    if (G[g].ps5) { wchar_t why[32]; GetStr(g, L"last_reason", why, 32, L""); if (!wcscmp(why, L"bench") || !wcscmp(why, L"warmup")) code = 0; }   /* the performance test / "Preparar" end by themselves */
    wchar_t k[64]; _snwprintf(k, 64, L"lastsess_ms_%s", G[g].id); PIniSet(k, (int)(ms / 1000));
    _snwprintf(k, 64, L"lastsess_code_%s", G[g].id); PIniSet(k, (int)code);
    if (SessionCrash(g, code)) { CrashUnreadKey(g, k); PIniSet(k, 1); }   /* the red dot on the Logs pill, for that game only */
}
static BOOL GamePlayed(int g) { wchar_t k[64]; _snwprintf(k, 64, L"lastplay_%s", G[g].id); if (PIni(k, 0) > 0) return TRUE; _snwprintf(k, 64, L"playtime_%s", G[g].id); return PIni(k, 0) >= 60; }
static BOOL ActivityHas(int g) { wchar_t k[64]; _snwprintf(k, 64, L"lastplay_%s", G[g].id); return PIni(k, 0) > 0 && !SOON(g); }
static void ActivityCard(HDC dc, GpGraphics *g, RECT r, int k, int gi)
{
    LiveCardBox(dc, g, r, k, 7, L2(L"Atividade", L"Activity"), L"");
    int x = r.left + SC(20), w = r.right - r.left - SC(40), y = r.top + SC(48);
    wchar_t kk[64]; _snwprintf(kk, 64, L"lastplay_%s", G[gi].id); long long last = PIni(kk, 0);
    _snwprintf(kk, 64, L"lastsess_ms_%s", G[gi].id); int secs = PIni(kk, -1);
    _snwprintf(kk, 64, L"lastsess_code_%s", G[gi].id); DWORD code = (DWORD)PIni(kk, 0);
    wchar_t when[96] = L"", l1[160];
    {   FILETIME ft; ULARGE_INTEGER u; u.QuadPart = ((ULONGLONG)last + 11644473600ull) * 10000000ull; ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart;
        FILETIME lf; SYSTEMTIME s, n; FileTimeToLocalFileTime(&ft, &lf); FileTimeToSystemTime(&lf, &s); GetLocalTime(&n);
        FILETIME nf; SystemTimeToFileTime(&n, &nf); ULARGE_INTEGER a, b; a.LowPart = lf.dwLowDateTime; a.HighPart = lf.dwHighDateTime; b.LowPart = nf.dwLowDateTime; b.HighPart = nf.dwHighDateTime;
        int days = (int)((b.QuadPart / 864000000000ull) - (a.QuadPart / 864000000000ull));
        if (days <= 0) _snwprintf(when, 96, L2(L"Hoje %02d:%02d", L"Today %02d:%02d"), s.wHour, s.wMinute);
        else if (days == 1) _snwprintf(when, 96, L2(L"Ontem %02d:%02d", L"Yesterday %02d:%02d"), s.wHour, s.wMinute);
        else _snwprintf(when, 96, L"%02d/%02d %02d:%02d", s.wDay, s.wMonth, s.wHour, s.wMinute); }
    if (secs >= 0) _snwprintf(l1, 160, L"%s  ·  %d min", when, secs < 60 ? 1 : secs / 60); else _snwprintf(l1, 160, L"%s", when);
    RECT a = {x, y, x + w, y + SC(20)}; TextR(dc, g_fSmall, RGB0(236, 236, 240), a, l1, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    BOOL ok = secs < 0 || !SessionCrash(gi, code); wchar_t l2[160];
    if (ok) _snwprintf(l2, 160, L"%s", secs < 0 ? L2(L"Sessão registrada", L"Session recorded") : L2(L"Terminou normalmente", L"Ended normally"));
    else _snwprintf(l2, 160, L2(L"Fechou com erro (código 0x%08lX)", L"Closed with an error (code 0x%08lX)"), (unsigned long)code);   /* 6.2 review: NTSTATUS in hex */
    Dot(g, x + SC(4), y + SC(30), SC(4), A(ok ? C_GOOD : C_BAD, 255));
    RECT b = {x + SC(16), y + SC(20), x + w, y + SC(40)}; TextR(dc, g_fSmall, RGB0(214, 214, 222), b, l2, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    {   const wchar_t *t = L2(L"Abrir logs", L"Open logs"); int tw = TextW(dc, g_fSmall, t), bw = tw + SC(48);
        RECT pl = {x, r.bottom - SC(40), x + bw, r.bottom - SC(14)}; Box(g, pl, SC(13), A(RGB0(255, 255, 255), g_hot == H_HOMECARD + k ? 40 : 14), A(RGB0(255, 255, 255), 60), 1.0f);
        RECT sq = {pl.left + SC(11), pl.top + SC(8), pl.left + SC(21), pl.top + SC(18)}; Box(g, sq, SC(2), 0, A(RGB0(255, 255, 255), 200), 1.2f);
        RECT tr = {pl.left + SC(30), pl.top, pl.right - SC(8), pl.bottom}; TextR(dc, g_fSmall, RGB0(236, 236, 240), tr, t, DT_LEFT | DT_VCENTER); }
}
/* PaintHome: a card in slot k: the game's own (HomeCard), or a live one */
static void HomeCard(HDC dc, GpGraphics *g, RECT r, int k, int icon, const wchar_t *title, const wchar_t *l1, const wchar_t *l2, COLORREF accent);
static void HomeSlot(HDC dc, GpGraphics *g, RECT r, int k, int kind, int gi, int icon, const wchar_t *title, const wchar_t *l1, const wchar_t *l2)
{
    r = HomeCardRect(r, k, 2);   /* Released: page 2's place */
    if (kind && !LiveOn() && kind != 3) kind = 0;
    if (kind == 3 && !ActivityHas(gi)) kind = 0;
    g_cardKind[k] = kind; RECT inner = r; inner.left += SC(7); inner.right -= SC(7); g_liveR[k] = inner;
    if (kind == 1) LivePcCard(dc, g, inner, k);
    else if (kind == 2) LivePadsCard(dc, g, inner, k);
    else if (kind == 3) ActivityCard(dc, g, inner, k, gi);
    else HomeCard(dc, g, r, k, icon, title, l1, l2, 0);
}
/* 6.2 review: the logs of the game on screen (a custom game has none here: no pill, no Square), and only its own red dot goes */
static void HomeLogs(void)
{
    if (g_csel >= 0 || g_sel < 0 || g_sel >= NGAMES) return;
    wchar_t k[48]; CrashUnreadKey(g_sel, k); WritePrivateProfileStringW(L"PORTHEON", k, NULL, g_pIni); OpenLogs(); InvalidateRect(g_wnd, NULL, FALSE);
}
static void SpecsCopy(void)
{
    wchar_t c[80], v[80], s[600]; ShortHw(c, v);
    OSVERSIONINFOW ov = {sizeof(ov)}; typedef LONG (WINAPI *RtlGV)(OSVERSIONINFOW *); RtlGV rg = (RtlGV)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"); if (rg) rg(&ov);
    _snwprintf(s, 600, L2(L"Portheon %s\r\nCPU: %s (%d threads)\r\nGPU: %s (%llu GB)\r\nRAM: %d GB (%.1f GB livre)\r\nWindows %lu.%lu.%lu\r\n", L"Portheon %s\r\nCPU: %s (%d threads)\r\nGPU: %s (%llu GB)\r\nRAM: %d GB (%.1f GB free)\r\nWindows %lu.%lu.%lu\r\n"), APP_SHOW, g_cpuName[0] ? g_cpuName : L"?", g_threads,
               g_haveGpu ? g_gpu.name : L"?", (unsigned long long)((g_gpu.vram + (512ull << 20)) >> 30), g_ramGb, g_ramFree / 1073741824.0, ov.dwMajorVersion, ov.dwMinorVersion, ov.dwBuildNumber);
    s[599] = 0;
    if (OpenClipboard(g_wnd)) {
        EmptyClipboard(); size_t n = (wcslen(s) + 1) * sizeof(wchar_t); HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n);
        if (h) { void *p = GlobalLock(h); if (p) { memcpy(p, s, n); GlobalUnlock(h); if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h); } else GlobalFree(h); }
        CloseClipboard(); Msg(C_GOOD, L"%s", L2(L"Especificações do PC copiadas: cole no Discord quando pedir ajuda.", L"PC specs copied: paste them on Discord when you ask for help."));
    }
}

/* ---------------------------------------------------------------- the footer's Logs pill */
static void HomeLogsPill(HDC dc, GpGraphics *g, int y)
{
    if (g_csel >= 0 || g_sel < 0 || g_sel >= NGAMES) return;   /* 6.2 review: a custom game's logs are not PORTHEON's to open */
    wchar_t uk[48]; CrashUnreadKey(g_sel, uk);
    int h = SC(30), top = SC(y), x = SC(MX + 12); BOOL hot = g_hot == H_HOMELOGS, unread = PIni(uk, 0) != 0;
    const wchar_t *t = L2(L"Logs do sistema", L"System logs"); int tw = TextW(dc, g_fSmall, t), w = SC(12) + SC(12) + SC(8) + tw + SC(14) + (unread ? SC(14) : 0);
    RECT r = {x, top, x + w, top + h};
    Box(g, r, h / 2, A(RGB0(255, 255, 255), hot ? 30 : 9), A(RGB0(255, 255, 255), hot ? 230 : 34), hot ? 1.4f : 1.0f);
    RECT sq = {x + SC(13), top + SC(10), x + SC(23), top + SC(20)}; Box(g, sq, SC(2), 0, A(hot ? RGB0(255, 255, 255) : C_SUB, 255), 1.2f);
    RECT tr = {x + SC(32), top, x + SC(32) + tw + SC(2), top + h}; TextR(dc, g_fSmall, hot ? RGB0(255, 255, 255) : C_TEXT, tr, t, DT_LEFT | DT_VCENTER);
    if (unread) Dot(g, r.right - SC(14), top + h / 2, SC(4), A(C_BAD, 255));
    AddHit(r.left, r.top, r.right, r.bottom, H_HOMELOGS);
}
/* ---------------------------------------------------------------- the clicks */
static BOOL Home62Click(int h)
{
    if (h == H_VITRINE) { VitOpenFull(); return TRUE; }
    if (h == H_HOMEMORE) { HomePageSet(!g_homePage); return TRUE; }   /* Released: "Mais" / "Voltar" */
    if (h == H_HOMELOGS) { HomeLogs(); return TRUE; }
    if (h >= H_QCHIP && h < H_QCHIP + 6) { QuickOpen(h - H_QCHIP); return TRUE; }
    if (h == H_YTPASTE) { YtPasteLink(g_sel); InvalidateRect(g_wnd, NULL, FALSE); return TRUE; }
    if (h == H_YTCLEAR) { YtClearLinks(g_sel); InvalidateRect(g_wnd, NULL, FALSE); return TRUE; }
    if (h >= H_YTSNDPICK && h < H_YTSNDPICK + 2) { YtSetSound(h == H_YTSNDPICK, TRUE); return TRUE; }
    if (h >= H_VITMODE && h < H_VITMODE + 3) {
        PIniSet(L"ui_vitrine", h - H_VITMODE); VitStop();
        static const wchar_t *const pt[3] = {L"YouTube, vídeos e edits ligados", L"só vídeos e edits locais", L"desligado"}, *const en[3] = {L"YouTube, videos and edits on", L"local videos and edits only", L"off"};
        Msg(C_GOOD, L2(L"Vitrine dos jogos: %s.", L"Game showcase: %s."), TR(pt[h - H_VITMODE], en[h - H_VITMODE])); InvalidateRect(g_wnd, NULL, FALSE); return TRUE;
    }
    if (g_view == V_HOME && g_csel < 0 && h >= H_HOMECARD && h < H_HOMECARD + 4) {
        int kind = g_cardKind[h - H_HOMECARD];
        if (kind == 1) { SpecsCopy(); return TRUE; }
        if (kind == 2) { if (g_npads > 0) { PadIdentify(); Msg(C_GOOD, L2(L"Controle 1: %s.", L"Controller 1: %s."), PadKind(g_pads[0].type)); } else Msg(C_SUB, L"%s", L2(L"Nenhum controle conectado. Ligue um pelo USB ou Bluetooth.", L"No controller connected. Plug one in by USB or Bluetooth.")); return TRUE; }
        if (kind == 3) { HomeLogs(); return TRUE; }
    }
    return FALSE;
}
static void Home62OverlayClick(int h)
{
    if (g_overlay == OV_VITFULL) { g_overlay = OV_NONE; VitFreeSet(g_vit.big, &g_vit.nbig); g_vit.bigAsked = FALSE; g_focus = H_VITRINE; Snd(SND_BACK); InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (g_overlay == OV_QUICK) {
        if (h >= H_QROW && h < H_QROW + g_qN && g_qOpen >= 0 && g_qOpen < g_nqc && g_qc[g_qOpen].f) {
            const Field *f = g_qc[g_qOpen].f; int i = h - H_QROW; wchar_t before[1024]; wcscpy(before, g_msg); DWORD at = g_msgAt;
            DropdownApply(f, f->opts[i].value);
            if (g_msgAt == at && !wcscmp(before, g_msg)) Msg(C_GOOD, L"%s: %s. %s", g_qc[g_qOpen].label, OptName(&f->opts[i]), L2(L"Salvo", L"Saved"));
            Snd(SND_OK);
        } else Snd(SND_BACK);
        g_focus = H_QCHIP + (g_qOpen >= 0 ? g_qOpen : 0); g_overlay = OV_NONE; g_qOpen = -1; InvalidateRect(g_wnd, NULL, FALSE);
    }
}
static void Home62Back(void) { if (g_overlay == OV_VITFULL || g_overlay == OV_QUICK) Home62OverlayClick(-1); }

/* ---------------------------------------------------------------- the hub's Vitrine section (••• > Vitrine): the player's YouTube links */
static void PanelTitle(HDC dc, GpGraphics *g, RECT pr, const wchar_t *t, const wchar_t *sub);
static void PaintVitrineTab(HDC dc, GpGraphics *g, RECT pr)
{
    int gsel = g_sel, own = 0, tr = 0, ed = 0; YtCount(gsel, &own, &tr, &ed);
    PanelTitle(dc, g, pr, L2(L"Vitrine", L"Showcase"), NULL);
    int y = pr.top + SC(70), x = pr.left + SC(24), w = pr.right - pr.left - SC(48);
    const wchar_t *a = L2(L"O quadro ao lado do jogo, na tela inicial, toca vídeos do YouTube pelo player oficial do YouTube, com som e sem baixar nada: primeiro os seus links, depois os vídeos escolhidos pelo PORTHEON (nos UFCs, edits de lutas reais com lutadores da época do jogo; nos outros jogos, um trailer). O título e o canal de cada vídeo aparecem embaixo do quadro. Sem internet, ou se nenhum vídeo puder tocar fora do YouTube, ele toca o seu vídeo da pasta videos\\<jogo> ou o edit ao vivo do PORTHEON. M / R3 tiram o som.",
                          L"The panel beside the game on the home screen plays YouTube videos through YouTube's own player, with sound and nothing downloaded: your links first, then the videos PORTHEON picked (for the UFC games, edits of real fights with fighters of the game's era; for the others, a trailer). Each video's title and channel show under the panel. Without internet, or when no video may play outside YouTube, it plays your video from the videos\\<game> folder or PORTHEON's live edit. M / R3 mutes it.");
    RECT r1 = {0, 0, w - SC(48), 0}; int h1 = Wrap(dc, g_fSmall, C_SUB, r1, a, FALSE) + SC(20); NoteBox(dc, g, x, y, w, h1, a, FALSE); y += h1 + SC(14);
    if (VitMode() != 0) {
        const wchar_t *b = L2(L"O YouTube está desligado em Configurações > Interface > Vídeos e edits dos jogos (escolha Ligado).", L"YouTube is off in Settings > Interface > Game videos and edits (pick On).");
        int h2 = Wrap(dc, g_fSmall, C_SUB, r1, b, FALSE) + SC(20); NoteBox(dc, g, x, y, w, h2, b, TRUE); y += h2 + SC(14);
    }
    wchar_t c[240]; _snwprintf(c, 240, L2(L"Na lista do %s: %d link(s) seu(s) e %d vídeo(s) escolhido(s) pelo PORTHEON.", L"On the %s list: %d link(s) of yours and %d video(s) picked by PORTHEON."), G[gsel].name, own, tr + ed);
    Text(dc, g_fText, C_TEXT, x, y, c); y += SC(36);
    RECT b1 = {x, y, x + SC(300), y + SC(46)}; IconButton(dc, g, b1, L2(L"Colar link do YouTube", L"Paste a YouTube link"), H_YTPASTE, 1, 7);
    if (own) { RECT b2 = {b1.right + SC(12), y, b1.right + SC(12) + SC(240), y + SC(46)}; Button(dc, g, b2, L2(L"Apagar os meus links", L"Remove my links"), H_YTCLEAR, 2); }
    y += SC(58);
    Text(dc, g_fSmall, C_SUB, x, y, L2(L"Copie o link no YouTube (Compartilhar > Copiar) e clique em Colar. O seu link toca primeiro.", L"Copy the link on YouTube (Share > Copy), then click Paste. Your link plays first.")); y += SC(30);
    if (own) {
        wchar_t ids[12][12]; int n = YtOwnLinks(gsel, ids, 12);
        for (int i = 0; i < n; i++) { wchar_t l[64]; _snwprintf(l, 64, L"%d.  youtu.be/%s", i + 1, ids[i]); Dot(g, x + SC(4), y + SC(10), SC(3), A(RGB0(255, 48, 64), 255)); Text(dc, g_fSmall, C_TEXT, x + SC(16), y + SC(1), l); y += SC(24); if (y > pr.bottom - SC(40)) break; }
    }
}
#ifdef PORTHEON_TEST
/* the .go of the UI shots: the vitrine of the scene at once (its pictures and the video's first frame), aged past its fade */
static void VitTestSettle(void)
{
    VitStop(); g_npads = -1; if (VitMode() == 2 || g_view != V_HOME) return;
    g_vit.key = VitKey(); g_vit.selAt = GetTickCount() - 5000;
    VitArtJob *j = VitArtJobNew(FALSE); if (j) { VitArtRun(j); VitOnArt((WPARAM)j->gen, (LPARAM)j); }
    g_vit.artAsked = TRUE; g_vit.vidTried = TRUE;
    wchar_t p[MAX_PATH]; if (VitMode() != 2 && g_vit.key >= 0 && g_vit.key < NGAMES && VideoFind(g_vit.key, G[g_vit.key].id, p, MAX_PATH)) {
        VideoSetWindow(g_wnd); _snwprintf(g_vit.vidName, 80, L"%s", PathFindFileNameW(p)); g_vit.vp = VideoOpen(p, TRUE);
        for (int i = 0; i < 150 && g_vit.vp && !VitHasVideo(); i++) Sleep(20);
        if (g_vit.vp) { Sleep(1500); VitHasVideo(); }   /* past the clip's first fade */
    }
    wchar_t v[16]; DWORD t = GetEnvironmentVariableW(L"PORTHEON_TEST_VITT", v, 16) ? (DWORD)_wtoi(v) : 400;
    g_vit.readyAt = GetTickCount() - 3000; g_vit.editT0 = GetTickCount() - t; g_vit.stillT = t; g_vit.lastLive = FALSE;
    if (g_overlay == OV_QUICK) { QuickBuild(g_sel); g_qOpen = 0; g_qN = g_nqc && g_qc[0].f ? (g_qc[0].f->nopts > QPOP_MAX ? QPOP_MAX : g_qc[0].f->nopts) : 0; g_qT0 = GetTickCount() - 2000; }
    if (g_overlay == OV_VITFULL) { g_vit.fullT0 = GetTickCount() - 2000; VitArtJob *b = VitArtJobNew(TRUE); if (b) { VitArtRun(b); VitOnArt((WPARAM)b->gen, (LPARAM)b); } g_vit.bigAsked = TRUE; }
}
#endif
