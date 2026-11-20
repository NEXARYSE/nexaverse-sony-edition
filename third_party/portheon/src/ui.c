/* ui.c - part of portheon.c (included by it): everything PORTHEON draws (4.0, the console interface).
 * Like a game console: a boot animation with the PORTHEON logo, "who is playing?" (the profiles), the home screen (the
 * games as tiles, the chosen one's art filling the window, PLAY, its cards), a hub per game (Overview, Video,
 * Gameplay, Online, Saves, Cheats, DLC, Extras, Game files), the system settings (language, profiles, interface,
 * updates, about), the install helper and the online center. Every clickable thing is also a place the keyboard
 * and the controller can move to (NavKey, ui_input.c). The layout is in logical units of a 1280x800 window; SC()
 * scales them to the window's real size, and full screen centres that frame on the monitor (g_ox, g_oy).
 * Parts: ui.c (drawing primitives, pictures, widgets), ui_items.c (the settings lists), ui_screens.c (every
 * screen), ui_input.c (the paint, the mouse, the keyboard and the controller). */

static int OptIndex(const Field *f, int v) { for (int i = 0; i < f->nopts; i++) if (f->opts[i].value == v) return i; return -1; }
static const wchar_t *OptName(const Opt *o) { return TR(o->pt, o->en); }
static void Select(int g);
static void Browse(void);
static void OpenLogs(void);
static void MakeReport(int g); static void StartBench(int g); static void StartWarmup(int g);   /* 4.3.5; 4.3.7 */
static void OpenGameFolder(void);
static void OpenUrl(const wchar_t *url);
static void DropdownMenu(const Field *f, int hitId); static void DropdownApply(const Field *f, int val);   /* 6.2 */
static void ToggleSwitch(const Field *f);
static void AdvMenu(void);
static void BestSettings(void);
static int PopupAt(HMENU m, int hitId);
static void SetFullscreen(BOOL on);
static void BootDone(void);
static void HomePrewarmTile(void);   /* perf 11-13 (ui_screens.c) */
static void LaunchTick(void);   /* 4.4.0 (ui_screens.c) */
static BOOL g_full;
static void VitReset(void); static void VitAnimate(void); static void LiveTick(void);   /* 6.2 (home62.c) */
static BOOL YtShowing(void);   /* 6.2 (yt.c) */
#ifdef PORTHEON_TEST
static void VitTestSettle(void);
#endif

/* Released (11-13, the owner pointed at a tour of the console's home: "the interface must look like it, keeping the videos
   already there in a place that looks great"): the home screen is TWO PAGES.
   Page 1 (g_homePage 0): the top bar, a row of small tiles hugging the top left (the chosen one bigger, its name in small
   letters under the next tiles), the game's art with the lightest veil, and low on the left the game's name, ONE line
   (its state and the time played), PLAY in dark glass (white under the mouse or the focus) and "..."; the video low on the
   right; a small "Mais" at the bottom centre.
   Page 2 (g_homePage 1; Down from the PLAY row, the wheel, or a click on "Mais"): the name small, the chips, the quick
   options, the four cards (two by two, left of the video), the logs pill and the credits. Up, the wheel, Esc / B go back.
   The video NEVER moves or hides between the pages (it is a child window: nothing can be drawn over it and moving it with
   an animation would tear): both pages are laid out around the same rectangle, so a clip keeps playing.
   HOME_PAGES 0 = the single page of 11-12 (every number below falls back to it through HPV). */
#define HOME_PAGES 1
#define HPV(pages, classic) (HOME_PAGES ? (pages) : (classic))
#define HP_SLIDE_MS 260     /* the page change: the old page leaves upward and fades, the new one arrives (Animate repaints; never Motion(): it hides the clip) */
#define HP_SLIDE 56         /* how far a page travels, in frame units */
#define HP_TITLE_Y 474      /* page 1: the name (64 high), its line, the buttons (54 high, their foot on the video's foot at 664) */
#define HP_STAT_Y 566
#define HP_BTN_Y 610
#define HP_CARDS_Y 378      /* page 2: the cards, two rows of TAPE_H with 14 between (378..514, 528..664) */
static int g_homePage, g_hpDraw; static DWORD g_pageT0; static BOOL g_pageFocus;   /* the page shown; the page being drawn (the other one while it leaves); the change's start; the focus goes to the new page's first stop */
#define HP_ON(p) (!HOME_PAGES || g_hpDraw == (p))   /* is this part of the home drawn now? (always with HOME_PAGES 0) */

/* ---------------------------------------------------------------- drawing primitives (GDI+, anti-aliased) */
static COLORREF Mix(COLORREF a, COLORREF b, float t)
{
    return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t), (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t), (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}
/* 7.0 (the console's look): every box has truly round corners (5.0 to 6.2 cut them at 45 degrees, as the logo's octagon).
   r is the corner's radius, held to half the box (a box as tall as twice its radius is a pill: the chips and the switches) */
static GpPath *RoundPath(int x, int y, int w, int h, int r)
{
    GpPath *p = NULL; GdipCreatePath(0, &p);
    int d = 2 * r; if (d > w) d = w; if (d > h) d = h;
    if (d < 2) { GdipAddPathLineI(p, x, y, x + w, y); GdipAddPathLineI(p, x + w, y, x + w, y + h); GdipAddPathLineI(p, x + w, y + h, x, y + h); }
    else {   /* four quarter circles; the path joins them with the straight sides */
        GdipAddPathArcI(p, x, y, d, d, 180.0f, 90.0f); GdipAddPathArcI(p, x + w - d, y, d, d, 270.0f, 90.0f);
        GdipAddPathArcI(p, x + w - d, y + h - d, d, d, 0.0f, 90.0f); GdipAddPathArcI(p, x, y + h - d, d, d, 90.0f, 90.0f);
    }
    GdipClosePathFigure(p); return p;
}
/* a regular octagon around (cx, cy) reaching r at its sides (the avatars, the top bar's icons) */
static GpPath *OctagonPath(int cx, int cy, int r)
{
    GpPath *p = NULL; GdipCreatePath(0, &p); if (p) GdipAddPathEllipseI(p, cx - r, cy - r, 2 * r, 2 * r); return p;   /* 7.0 fix: a circle (the console's look has no cut corners): the pad drawing's sticks and buttons, the achievement badges, the status marks */
}
static void Oct(GpGraphics *g, int cx, int cy, int r, UINT32 fill, UINT32 border, float bw)
{
    GpPath *p = OctagonPath(cx, cy, r);
    if (fill) { GpBrush *b = NULL; GdipCreateSolidFill(fill, &b); GdipFillPath(g, b, p); GdipDeleteBrush(b); }
    if (border) { GpPen *pen = NULL; GdipCreatePen1(border, bw, 2, &pen); GdipDrawPath(g, pen, p); GdipDeletePen(pen); }
    GdipDeletePath(p);
}
static void Box(GpGraphics *g, RECT r, int rad, UINT32 fill, UINT32 border, float bw)
{
    GpPath *p = RoundPath(r.left, r.top, r.right - r.left - 1, r.bottom - r.top - 1, rad);
    if (fill) { GpBrush *b = NULL; GdipCreateSolidFill(fill, &b); GdipFillPath(g, b, p); GdipDeleteBrush(b); }
    if (border) { GpPen *pen = NULL; GdipCreatePen1(border, bw, 2, &pen); GdipDrawPath(g, pen, p); GdipDeletePen(pen); }
    GdipDeletePath(p);
}
static void GradBox(GpGraphics *g, RECT r, int rad, UINT32 c1, UINT32 c2, BOOL fill, float bw)
{
    POINT a = {r.left, r.top}, z = {r.right, r.top}; GpBrush *lb = NULL; GdipCreateLineBrushI(&a, &z, c1, c2, 0, &lb);
    GpPath *p = RoundPath(r.left, r.top, r.right - r.left - 1, r.bottom - r.top - 1, rad);
    if (fill) GdipFillPath(g, lb, p); else { GpPen *pen = NULL; GdipCreatePen2(lb, bw, 2, &pen); GdipDrawPath(g, pen, p); GdipDeletePen(pen); }
    GdipDeletePath(p); GdipDeleteBrush(lb);
}
/* 6.0 final: what is on or chosen (a switch, a step, an effects chip): the logo's black and white fills it in white
   with dark marks on it, as the PLAY button; the other colors keep their gradient with white marks. Returns the
   marks' color. */
static COLORREF AccentBox(GpGraphics *g, RECT r, int rad)
{
    if (g_themeMono) { Box(g, r, rad, A(RGB0(236, 236, 238), 255), 0, 0); return RGB0(16, 16, 20); }
    GradBox(g, r, rad, A(C_CYAN, 255), A(C_PURPLE, 255), TRUE, 0); return RGB0(255, 255, 255);
}
static void VGrad(GpGraphics *g, int x, int y, int w, int h, UINT32 top, UINT32 bottom)
{
    POINT a = {0, y}, z = {0, y + h}; GpBrush *lb = NULL; GdipCreateLineBrushI(&a, &z, top, bottom, 0, &lb);
    if (lb) { GdipFillRectangleI(g, lb, x, y, w, h); GdipDeleteBrush(lb); }
}
static void HGrad(GpGraphics *g, int x, int y, int w, int h, UINT32 left, UINT32 right)
{
    POINT a = {x, 0}, z = {x + w, 0}; GpBrush *lb = NULL; GdipCreateLineBrushI(&a, &z, left, right, 0, &lb);
    if (lb) { GdipFillRectangleI(g, lb, x, y, w, h); GdipDeleteBrush(lb); }
}
static void Fill(GpGraphics *g, int x, int y, int w, int h, UINT32 c) { GpBrush *b = NULL; GdipCreateSolidFill(c, &b); if (b) { GdipFillRectangleI(g, b, x, y, w, h); GdipDeleteBrush(b); } }
/* 4.3: the neon look. PORTHEON's own colors, the logo's cyan to purple, mark what is chosen, focused or the main action
   (the PS5's white stays for text). NeonBrush: an angled gradient cyan -> purple -> cyan over r, turned by deg; the
   turn animates the chosen tile's edge and the focus (NeonDeg). */
static GpBrush *NeonBrush(RECT r, float deg, BYTE a)
{
    GpRectI rr = {r.left, r.top, r.right - r.left > 0 ? r.right - r.left : 1, r.bottom - r.top > 0 ? r.bottom - r.top : 1};
    GpBrush *b = NULL; GdipCreateLineBrushFromRectWithAngleI(&rr, A(C_CYAN, a), A(C_PURPLE, a), deg, TRUE, 0, &b);
    if (b) { UINT32 c[3] = {A(C_CYAN, a), A(C_PURPLE, a), A(C_CYAN, a)}; float p[3] = {0.0f, 0.5f, 1.0f}; GdipSetLinePresetBlend(b, c, p, 3); }
    return b;
}
/* a fill goes once from cyan to purple (the loop above suits an edge that turns) */
static GpBrush *NeonBrush2(RECT r, float deg, BYTE a)
{
    GpRectI rr = {r.left, r.top, r.right - r.left > 0 ? r.right - r.left : 1, r.bottom - r.top > 0 ? r.bottom - r.top : 1};
    GpBrush *b = NULL; GdipCreateLineBrushFromRectWithAngleI(&rr, A(RGB(170, 112, 255), a), A(RGB(98, 38, 214), a), deg, TRUE, 0, &b);   /* 5.0: light violet to deep purple */
    return b;
}
/* an edge in the neon gradient with a soft glow of `glow` rings around it (0: none) */
static void NeonEdge(GpGraphics *g, RECT r, int rad, float w, float deg, int glow)
{
    if (g_fxLight) glow = 0;   /* 5.0: the light tier draws the edge alone */
    GpPath *p = RoundPath(r.left, r.top, r.right - r.left - 1, r.bottom - r.top - 1, rad);
    for (int k = glow; k >= 0; k--) {
        GpBrush *b = NeonBrush(r, deg, k ? (BYTE)(54 / k) : 255); GpPen *pen = NULL;
        if (b) { GdipCreatePen2(b, w + (float)SC(3 * k), 2, &pen); if (pen) { GdipDrawPath(g, pen, p); GdipDeletePen(pen); } GdipDeleteBrush(b); }
    }
    GdipDeletePath(p);
}
/* a shape filled with the neon gradient */
static void NeonFill(GpGraphics *g, RECT r, int rad, float deg, BYTE a)
{
    GpPath *p = RoundPath(r.left, r.top, r.right - r.left - 1, r.bottom - r.top - 1, rad);
    GpBrush *b = NeonBrush2(r, deg, a); if (b) { GdipFillPath(g, b, p); GdipDeleteBrush(b); }
    GdipDeletePath(p);
}
/* soft light: c in the middle, fading to nothing at the ellipse's edge (under PLAY and the chosen tile) */
static void GlowRaw(GpGraphics *g, int cx, int cy, int rx, int ry, UINT32 c);
static void Glow(GpGraphics *g, int cx, int cy, int rx, int ry, UINT32 c)
{
    if (g_fxLight) return;   /* 5.0: no soft lights drawn live in the light tier (the cached backgrounds keep theirs) */
    GlowRaw(g, cx, cy, rx, ry, c);
}
static void GlowRaw(GpGraphics *g, int cx, int cy, int rx, int ry, UINT32 c)
{
    if (rx <= 0 || ry <= 0) return;
    GpPath *p = NULL; GdipCreatePath(0, &p); GdipAddPathEllipseI(p, cx - rx, cy - ry, 2 * rx, 2 * ry);
    GpBrush *b = NULL; GdipCreatePathGradientFromPath(p, &b);
    if (b) { UINT32 edge = c & 0x00FFFFFF; INT n = 1; GdipSetPathGradientCenterColor(b, c); GdipSetPathGradientSurroundColorsWithCount(b, &edge, &n); GdipFillPath(g, b, p); GdipDeleteBrush(b); }
    GdipDeletePath(p);
}
static void Dot(GpGraphics *g, int cx, int cy, int r, UINT32 c) { GpBrush *b = NULL; GdipCreateSolidFill(c, &b); GdipFillEllipseI(g, b, cx - r, cy - r, 2 * r, 2 * r); GdipDeleteBrush(b); }
static void Ring(GpGraphics *g, int cx, int cy, int r, UINT32 c, float w) { GpPen *p = NULL; GdipCreatePen1(c, w, 2, &p); GdipDrawEllipseI(g, p, cx - r, cy - r, 2 * r, 2 * r); GdipDeletePen(p); }
static void Line(GpGraphics *g, int x1, int y1, int x2, int y2, UINT32 c, float w) { GpPen *p = NULL; GdipCreatePen1(c, w, 2, &p); GdipDrawLineI(g, p, x1, y1, x2, y2); GdipDeletePen(p); }
/* 5.0: the cage brackets: what is chosen or focused is held by four corners, like the posts of the octagon (the left
   ones in light violet, the right ones in purple: the brand's gradient without a gradient brush), `out` outside r */
static __attribute__((unused)) void Brackets(GpGraphics *g, RECT r, int out, int len, float w, BYTE a)   /* 7.0: no caller left (the focus is a white line, ui_input.c) */
{
    InflateRect(&r, out, out); int L = len, cut = SC(3);
    if (L > (r.right - r.left) / 3) L = (r.right - r.left) / 3; if (L > (r.bottom - r.top) / 3) L = (r.bottom - r.top) / 3;
    UINT32 cl = A(C_CYAN, a), cr = A(RGB(150, 84, 255), a);
    GpPen *pl = NULL, *pr = NULL; GdipCreatePen1(cl, w, 2, &pl); GdipCreatePen1(cr, w, 2, &pr); if (!pl || !pr) { if (pl) GdipDeletePen(pl); if (pr) GdipDeletePen(pr); return; }
    GdipSetPenStartCap(pl, 2); GdipSetPenEndCap(pl, 2); GdipSetPenStartCap(pr, 2); GdipSetPenEndCap(pr, 2);
    int x0 = r.left, y0 = r.top, x1 = r.right, y1 = r.bottom;
    GdipDrawLineI(g, pl, x0, y0 + L, x0, y0 + cut); GdipDrawLineI(g, pl, x0, y0 + cut, x0 + cut, y0); GdipDrawLineI(g, pl, x0 + cut, y0, x0 + L, y0);
    GdipDrawLineI(g, pl, x0, y1 - L, x0, y1 - cut); GdipDrawLineI(g, pl, x0, y1 - cut, x0 + cut, y1); GdipDrawLineI(g, pl, x0 + cut, y1, x0 + L, y1);
    GdipDrawLineI(g, pr, x1, y0 + L, x1, y0 + cut); GdipDrawLineI(g, pr, x1, y0 + cut, x1 - cut, y0); GdipDrawLineI(g, pr, x1 - cut, y0, x1 - L, y0);
    GdipDrawLineI(g, pr, x1, y1 - L, x1, y1 - cut); GdipDrawLineI(g, pr, x1, y1 - cut, x1 - cut, y1); GdipDrawLineI(g, pr, x1 - cut, y1, x1 - L, y1);
    GdipDeletePen(pl); GdipDeletePen(pr);
}
/* 5.0: cinematic motion runs only after something happens (a new game, a new screen, the mouse on PLAY) and stops by
   itself: g_motionUntil is when the last one ends (Animate repaints until then); 0..1 progress of a motion begun at t0 */
static DWORD g_motionUntil, g_heroT0, g_sheenT0; static BOOL g_motionOn;
static void Motion(DWORD ms) { if (g_fxLight) return; DWORD u = GetTickCount() + ms; if (!g_motionOn || (int)(u - g_motionUntil) > 0) g_motionUntil = u; g_motionOn = TRUE; }
static float MotionT(DWORD t0, DWORD ms) { if (g_fxLight || !t0) return 1.0f; DWORD t = GetTickCount() - t0; return t >= ms ? 1.0f : (float)t / ms; }
static float EaseOut3(float k) { if (k <= 0) return 0; if (k >= 1) return 1; return 1.0f - (1.0f - k) * (1.0f - k) * (1.0f - k); }
/* 4.0.1: GDI text in the frame's coordinates. GDI+ resets the DC's viewport origin when it draws, so each text sets
   the full-screen offset itself and puts it back to 0 for GDI+ (which has its own world transform) */
static void GdiText(HDC dc, BOOL on) { SetViewportOrgEx(dc, on ? g_ox : 0, on ? g_oy : 0, NULL); }
static void Text(HDC dc, HFONT f, COLORREF c, int x, int y, const wchar_t *s) { SelectObject(dc, f); SetTextColor(dc, c); GdiText(dc, TRUE); TextOutW(dc, x, y, s, (int)wcslen(s)); GdiText(dc, FALSE); }
static void TextR(HDC dc, HFONT f, COLORREF c, RECT r, const wchar_t *s, UINT fl) { SelectObject(dc, f); SetTextColor(dc, c); GdiText(dc, TRUE); DrawTextW(dc, s, -1, &r, fl | DT_SINGLELINE | DT_NOPREFIX); GdiText(dc, FALSE); }
static int TextW(HDC dc, HFONT f, const wchar_t *s) { SelectObject(dc, f); SIZE z; GetTextExtentPoint32W(dc, s, (int)wcslen(s), &z); return z.cx; }
static int Wrap(HDC dc, HFONT f, COLORREF c, RECT r, const wchar_t *s, BOOL draw)   /* word-wrapped text; returns its height */
{
    SelectObject(dc, f); SetTextColor(dc, c);
    RECT m = r; DrawTextW(dc, s, -1, &m, DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
    if (draw) { GdiText(dc, TRUE); DrawTextW(dc, s, -1, &r, DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS); GdiText(dc, FALSE); }
    return m.bottom - m.top;
}
static int Sp(int sp) { return LangCjk() ? 0 : SC(sp); }   /* letter spacing: none in Japanese, Korean and Chinese */
static void Spaced(HDC dc, HFONT f, COLORREF c, int x, int y, const wchar_t *s, int sp) { SetTextCharacterExtra(dc, Sp(sp)); Text(dc, f, c, x, y, s); SetTextCharacterExtra(dc, 0); }
static int SpacedW(HDC dc, HFONT f, const wchar_t *s, int sp) { SetTextCharacterExtra(dc, Sp(sp)); int w = TextW(dc, f, s); SetTextCharacterExtra(dc, 0); return w; }
/* 4.0.4: "JOGAR" -> "Jogar", as the PS5 writes its sections and its Play (CharLowerBuff knows every script's case) */
static const wchar_t *Sentence(const wchar_t *s, wchar_t *out, int n)
{
    wcsncpy(out, s, n - 1); out[n - 1] = 0; int l = (int)wcslen(out);
    if (l > 1) CharLowerBuffW(out + 1, (DWORD)(l - 1));
    return out;
}
/* 4.0.4: a button's text fits its button: the letter spacing goes first, then one size smaller (then "...") */
static HFONT FitBtnFont(HDC dc, const wchar_t *t, int avail, int *sp)
{
    *sp = 0; if (TextW(dc, g_fBtn, t) <= avail) return g_fBtn;   /* 5.0: no wide letter spacing on buttons (cleaner) */
    return g_fSection;
}
static void PlayTriangle(GpGraphics *g, int x, int cy, int s, UINT32 c)
{
    GpPath *p = NULL; GdipCreatePath(0, &p); GpBrush *b = NULL; GdipCreateSolidFill(c, &b);
    GdipAddPathLineI(p, x, cy - s / 2, x + s * 9 / 10, cy); GdipAddPathLineI(p, x + s * 9 / 10, cy, x, cy + s / 2);
    GdipClosePathFigure(p); GdipFillPath(g, b, p); GdipDeleteBrush(b); GdipDeletePath(p);
}
static void Chevron(GpGraphics *g, int cx, int cy, int s, int dir, UINT32 c)   /* dir 0 down, 1 right, 2 left, 3 up */
{
    GpPen *pen = NULL; GdipCreatePen1(c, (float)SC(2), 2, &pen);
    if (dir == 0) { GdipDrawLineI(g, pen, cx - s, cy - s / 2, cx, cy + s / 2); GdipDrawLineI(g, pen, cx, cy + s / 2, cx + s, cy - s / 2); }
    else if (dir == 1) { GdipDrawLineI(g, pen, cx - s / 2, cy - s, cx + s / 2, cy); GdipDrawLineI(g, pen, cx + s / 2, cy, cx - s / 2, cy + s); }
    else if (dir == 2) { GdipDrawLineI(g, pen, cx + s / 2, cy - s, cx - s / 2, cy); GdipDrawLineI(g, pen, cx - s / 2, cy, cx + s / 2, cy + s); }
    else { GdipDrawLineI(g, pen, cx - s, cy + s / 2, cx, cy - s / 2); GdipDrawLineI(g, pen, cx, cy - s / 2, cx + s, cy + s / 2); }
    GdipDeletePen(pen);
}
static float PW(int s, int div) { return (float)(s / div > 1 ? s / div : 1); }
static void IconFolder(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    RECT tab = {x, y + s / 8, x + s * 45 / 100, y + s / 3}; Box(g, tab, s / 10, c, 0, 0);
    RECT body = {x, y + s / 4, x + s, y + s * 9 / 10}; Box(g, body, s / 8, c, 0, 0);
}
static void IconSearch(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    GpPen *pen = NULL; GdipCreatePen1(c, PW(s, 8), 2, &pen);
    GdipDrawEllipseI(g, pen, x, y, s * 6 / 10, s * 6 / 10); GdipDrawLineI(g, pen, x + s * 55 / 100, y + s * 55 / 100, x + s, y + s); GdipDeletePen(pen);
}
static void IconCheck(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    GpPen *pen = NULL; GdipCreatePen1(c, PW(s, 7), 2, &pen);
    GdipDrawLineI(g, pen, x, y + s / 2, x + s * 4 / 10, y + s * 85 / 100); GdipDrawLineI(g, pen, x + s * 4 / 10, y + s * 85 / 100, x + s, y + s / 8); GdipDeletePen(pen);
}
static void IconCross(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    GpPen *pen = NULL; GdipCreatePen1(c, PW(s, 7), 2, &pen);
    GdipDrawLineI(g, pen, x, y, x + s, y + s); GdipDrawLineI(g, pen, x + s, y, x, y + s); GdipDeletePen(pen);
}
static void IconDownload(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    GpPen *pen = NULL; GdipCreatePen1(c, PW(s, 8), 2, &pen);
    GdipDrawLineI(g, pen, x + s / 2, y, x + s / 2, y + s * 7 / 10);
    GdipDrawLineI(g, pen, x + s / 5, y + s * 4 / 10, x + s / 2, y + s * 7 / 10); GdipDrawLineI(g, pen, x + s / 2, y + s * 7 / 10, x + s * 8 / 10, y + s * 4 / 10);
    GdipDrawLineI(g, pen, x, y + s, x + s, y + s); GdipDeletePen(pen);
}
static void IconPlus(GpGraphics *g, int x, int y, int s, UINT32 c) { Line(g, x + s / 2, y, x + s / 2, y + s, c, PW(s, 7)); Line(g, x, y + s / 2, x + s, y + s / 2, c, PW(s, 7)); }
static void IconWindow(GpGraphics *g, int x, int y, int s, UINT32 c)   /* 4.0.4: "window mode": a window with its title bar */
{
    RECT b = {x, y + s / 8, x + s, y + s * 7 / 8}; Box(g, b, s / 8, 0, c, PW(s, 9));
    Line(g, x, y + s * 3 / 8, x + s, y + s * 3 / 8, c, PW(s, 10));
}
static void IconExpand(GpGraphics *g, int x, int y, int s, UINT32 c)   /* 4.0.4: "full screen": the four corners */
{
    int k = s * 35 / 100; float w = PW(s, 8);
    Line(g, x, y, x + k, y, c, w); Line(g, x, y, x, y + k, c, w); Line(g, x + s, y, x + s - k, y, c, w); Line(g, x + s, y, x + s, y + k, c, w);
    Line(g, x, y + s, x + k, y + s, c, w); Line(g, x, y + s, x, y + s - k, c, w); Line(g, x + s, y + s, x + s - k, y + s, c, w); Line(g, x + s, y + s, x + s, y + s - k, c, w);
}
static void IconGear(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    int cx = x + s / 2, cy = y + s / 2;
    for (int k = 0; k < 8; k++) { float a = k * 0.785398f; Line(g, cx + (int)(cosf(a) * s * 0.30f), cy + (int)(sinf(a) * s * 0.30f), cx + (int)(cosf(a) * s * 0.50f), cy + (int)(sinf(a) * s * 0.50f), c, PW(s, 6)); }
    Ring(g, cx, cy, s * 32 / 100, c, PW(s, 7)); Ring(g, cx, cy, s / 9 + 1, c, PW(s, 9));
}
static void IconGlobe(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    int cx = x + s / 2, cy = y + s / 2, r = s / 2; float w = PW(s, 9);
    Ring(g, cx, cy, r, c, w);
    GpPen *p = NULL; GdipCreatePen1(c, w, 2, &p); GdipDrawEllipseI(g, p, cx - r / 2, cy - r, r, 2 * r); GdipDeletePen(p);
    Line(g, x, cy, x + s, cy, c, w); Line(g, cx, y, cx, y + s, c, w);
}
static void IconPad(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    RECT b = {x, y + s / 5, x + s, y + s * 8 / 10}; Box(g, b, s / 4, 0, c, PW(s, 9));
    Line(g, x + s / 4, y + s / 2 - s / 9, x + s / 4, y + s / 2 + s / 9, c, PW(s, 10)); Line(g, x + s / 4 - s / 9, y + s / 2, x + s / 4 + s / 9, y + s / 2, c, PW(s, 10));
    Dot(g, x + s * 72 / 100, y + s * 42 / 100, s / 16 + 1, c); Dot(g, x + s * 82 / 100, y + s * 55 / 100, s / 16 + 1, c);
}
static void IconSave(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    RECT b = {x, y, x + s, y + s}; Box(g, b, s / 8, 0, c, PW(s, 9));
    RECT t = {x + s / 4, y, x + s * 3 / 4, y + s * 3 / 10}; Box(g, t, 1, 0, c, PW(s, 10));
    RECT l = {x + s / 5, y + s / 2, x + s * 4 / 5, y + s}; Box(g, l, 1, 0, c, PW(s, 10));
}
static void IconBolt(GpGraphics *g, int x, int y, int s, UINT32 c)   /* cheats */
{
    GpPath *p = NULL; GdipCreatePath(0, &p); GpBrush *b = NULL; GdipCreateSolidFill(c, &b);
    GdipAddPathLineI(p, x + s * 6 / 10, y, x + s / 5, y + s * 55 / 100); GdipAddPathLineI(p, x + s / 5, y + s * 55 / 100, x + s / 2, y + s * 55 / 100);
    GdipAddPathLineI(p, x + s / 2, y + s * 55 / 100, x + s * 4 / 10, y + s); GdipAddPathLineI(p, x + s * 4 / 10, y + s, x + s * 8 / 10, y + s * 42 / 100);
    GdipAddPathLineI(p, x + s * 8 / 10, y + s * 42 / 100, x + s / 2, y + s * 42 / 100); GdipClosePathFigure(p);
    GdipFillPath(g, b, p); GdipDeleteBrush(b); GdipDeletePath(p);
}
static void IconBox(GpGraphics *g, int x, int y, int s, UINT32 c)   /* DLC: a package */
{
    RECT b = {x, y + s / 4, x + s, y + s}; Box(g, b, s / 10, 0, c, PW(s, 9));
    Line(g, x, y + s / 4, x + s / 5, y, c, PW(s, 9)); Line(g, x + s, y + s / 4, x + s * 4 / 5, y, c, PW(s, 9)); Line(g, x + s / 5, y, x + s * 4 / 5, y, c, PW(s, 9));
    Line(g, x + s / 2, y + s / 4, x + s / 2, y + s, c, PW(s, 10));
}
static void IconMonitor(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    RECT b = {x, y + s / 10, x + s, y + s * 72 / 100}; Box(g, b, s / 10, 0, c, PW(s, 9));
    Line(g, x + s / 2, y + s * 72 / 100, x + s / 2, y + s * 9 / 10, c, PW(s, 9)); Line(g, x + s / 4, y + s * 9 / 10, x + s * 3 / 4, y + s * 9 / 10, c, PW(s, 9));
}
static void IconSliders(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    for (int k = 0; k < 3; k++) { int yy = y + s / 6 + k * s / 3; Line(g, x, yy, x + s, yy, c, PW(s, 10)); Dot(g, x + (k == 1 ? s * 7 / 10 : s * 3 / 10), yy, s / 8 + 1, c); }
}
static void IconChip(GpGraphics *g, int x, int y, int s, UINT32 c)   /* 4.3: the processor */
{
    RECT b = {x + s / 5, y + s / 5, x + s * 4 / 5, y + s * 4 / 5}; Box(g, b, s / 10, 0, c, PW(s, 9));
    RECT core = {x + s * 37 / 100, y + s * 37 / 100, x + s * 63 / 100, y + s * 63 / 100}; Box(g, core, s / 20 + 1, c, 0, 0);
    for (int k = 0; k < 3; k++) { int p = s * (35 + k * 15) / 100;
        Line(g, x + p, y, x + p, y + s / 5, c, PW(s, 12)); Line(g, x + p, y + s * 4 / 5, x + p, y + s, c, PW(s, 12));
        Line(g, x, y + p, x + s / 5, y + p, c, PW(s, 12)); Line(g, x + s * 4 / 5, y + p, x + s, y + p, c, PW(s, 12)); }
}
static void IconGpu(GpGraphics *g, int x, int y, int s, UINT32 c)   /* 4.3: the graphics card, a board with its fan */
{
    RECT b = {x, y + s / 5, x + s, y + s * 8 / 10}; Box(g, b, s / 10, 0, c, PW(s, 9));
    Ring(g, x + s * 36 / 100, y + s / 2, s / 6, c, PW(s, 10)); Dot(g, x + s * 36 / 100, y + s / 2, s / 16 + 1, c);
    Line(g, x + s * 64 / 100, y + s * 4 / 10, x + s * 84 / 100, y + s * 4 / 10, c, PW(s, 12)); Line(g, x + s * 64 / 100, y + s * 6 / 10, x + s * 84 / 100, y + s * 6 / 10, c, PW(s, 12));
    Line(g, x + s / 8, y + s * 8 / 10, x + s / 8, y + s * 95 / 100, c, PW(s, 10));
}
static void IconInfo(GpGraphics *g, int x, int y, int s, UINT32 c) { Ring(g, x + s / 2, y + s / 2, s / 2, c, PW(s, 9)); Line(g, x + s / 2, y + s * 45 / 100, x + s / 2, y + s * 75 / 100, c, PW(s, 8)); Dot(g, x + s / 2, y + s * 28 / 100, s / 14 + 1, c); }
static void IconPower(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    GpPen *p = NULL; GdipCreatePen1(c, PW(s, 8), 2, &p); GdipDrawArcI(g, p, x, y + s / 10, s, s * 9 / 10, -60, 300); GdipDeletePen(p);
    Line(g, x + s / 2, y, x + s / 2, y + s * 45 / 100, c, PW(s, 8));
}
static void IconUser(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    Ring(g, x + s / 2, y + s * 3 / 10, s * 22 / 100, c, PW(s, 9));
    GpPen *p = NULL; GdipCreatePen1(c, PW(s, 9), 2, &p); GdipDrawArcI(g, p, x + s / 8, y + s * 6 / 10, s * 3 / 4, s * 7 / 10, 180, 180); GdipDeletePen(p);
}
static void IconChat(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    RECT b = {x, y, x + s, y + s * 3 / 4}; Box(g, b, s / 4, 0, c, PW(s, 9));
    GpPen *pen = NULL; GdipCreatePen1(c, PW(s, 9), 2, &pen);
    GdipDrawLineI(g, pen, x + s / 4, y + s * 3 / 4 - 1, x + s / 6, y + s); GdipDrawLineI(g, pen, x + s / 6, y + s, x + s / 2, y + s * 3 / 4 - 1);
    GdipDeletePen(pen);
    int dr = s / 14 > 1 ? s / 14 : 1; Dot(g, x + s * 3 / 10, y + s * 3 / 8, dr, c); Dot(g, x + s / 2, y + s * 3 / 8, dr, c); Dot(g, x + s * 7 / 10, y + s * 3 / 8, dr, c);
}
static void IconCopy(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    RECT back = {x + s / 3, y, x + s, y + s * 2 / 3}; Box(g, back, s / 6, 0, c, PW(s, 9));
    RECT front = {x, y + s / 3, x + s * 2 / 3, y + s}; Box(g, front, s / 6, A(C_SURF, 255), c, PW(s, 9));
}
static void IconLink(GpGraphics *g, int x, int y, int s, UINT32 c)
{
    GpPen *p = NULL; GdipCreatePen1(c, PW(s, 8), 2, &p);
    GdipDrawArcI(g, p, x, y + s / 4, s / 2, s / 2, 90, 180); GdipDrawArcI(g, p, x + s / 2, y + s / 4, s / 2, s / 2, -90, 180); GdipDeletePen(p);
    Line(g, x + s / 4, y + s / 4, x + s * 3 / 4, y + s / 4, c, PW(s, 8)); Line(g, x + s / 4, y + s * 3 / 4, x + s * 3 / 4, y + s * 3 / 4, c, PW(s, 8));
}
/* perf 11-13: PORTHEON's logo at the small sizes it is drawn (the top bar of every screen, the credits): the 1024 px
   picture was scaled again on every paint (4.7 ms; 6.8 through a color matrix). A copy per size, made with the same
   scaling the paint itself would use (the graphics' own interpolation and pixel offset modes) and with the theme's color
   already in it, is then copied 1:1. Freed by ScaleChanged (the scale, the color). */
int WINAPI GdipGetInterpolationMode(GpGraphics *, int *);
int WINAPI GdipGetPixelOffsetMode(GpGraphics *, int *);
#define NLOGOS 4
static struct { GpBitmap *b; int s, im, po, tint; float delta; } g_logoS[NLOGOS]; static int g_logoNext;
static void LogoSmallFree(void) { for (int i = 0; i < NLOGOS; i++) { if (g_logoS[i].b) GdipDisposeImage((GpImage *)g_logoS[i].b); g_logoS[i].b = NULL; g_logoS[i].s = 0; } }
static GpImage *LogoSmall(GpGraphics *g, int s)
{
    int im = 5, po = 0, tint = g_theme && !g_themeMono && !g_skinOn; GdipGetInterpolationMode(g, &im); GdipGetPixelOffsetMode(g, &po);
    for (int i = 0; i < NLOGOS; i++) if (g_logoS[i].b && g_logoS[i].s == s && g_logoS[i].im == im && g_logoS[i].po == po && g_logoS[i].tint == tint && (!tint || g_logoS[i].delta == g_themeDelta)) return (GpImage *)g_logoS[i].b;
    GpBitmap *b = NULL; GpGraphics *bg = NULL;
    if (GdipCreateBitmapFromScan0(s, s, 0, 0x000E200B /* PixelFormat32bppPARGB */, NULL, &b) != 0 || !b) return NULL;
    GdipGetImageGraphicsContext((GpImage *)b, &bg); if (!bg) { GdipDisposeImage((GpImage *)b); return NULL; }
    GdipSetInterpolationMode(bg, im); GdipSetPixelOffsetMode(bg, po);
    UINT iw = 0, ih = 0; GdipGetImageWidth(g_imgLogo, &iw); GdipGetImageHeight(g_imgLogo, &ih);
    GpImageAttributes *ta = ThemeImageAttr(1.0f);   /* the theme's turn, once (NULL for the logo's black and white) */
    GdipDrawImageRectRectI(bg, g_imgLogo, 0, 0, s, s, 0, 0, (INT)iw, (INT)ih, 2, ta, NULL, NULL); if (ta) GdipDisposeImageAttributes(ta);
    GdipDeleteGraphics(bg);
    int k = g_logoNext; g_logoNext = (g_logoNext + 1) % NLOGOS;
    if (g_logoS[k].b) GdipDisposeImage((GpImage *)g_logoS[k].b);
    g_logoS[k].b = b; g_logoS[k].s = s; g_logoS[k].im = im; g_logoS[k].po = po; g_logoS[k].tint = tint; g_logoS[k].delta = g_themeDelta;
    return (GpImage *)b;
}
/* an image drawn at a given opacity (0..1) */
static void DrawAlpha(GpGraphics *g, GpImage *img, int x, int y, int w, int h, float a)
{
    if (a <= 0.01f) return;
    if (img == g_imgLogo && w == h && w > 0 && w <= 256) { GpImage *sm = LogoSmall(g, w); if (sm) img = sm; }   /* perf 11-13 */
    UINT iw = 0, ih = 0; GdipGetImageWidth(img, &iw); GdipGetImageHeight(img, &ih);
    if (img == g_imgBg || img == g_imgLogo) {   /* 6.0: PORTHEON's own pictures take its color (theme.c) */
        GpImageAttributes *ta = ThemeImageAttr(a);
        GdipDrawImageRectRectI(g, img, x, y, w, h, 0, 0, (INT)iw, (INT)ih, 2, ta, NULL, NULL); if (ta) GdipDisposeImageAttributes(ta); return;
    }
    if (a >= 0.99f) { GdipDrawImageRectRectI(g, img, x, y, w, h, 0, 0, (INT)iw, (INT)ih, 2, NULL, NULL, NULL); return; }
    GpImageAttributes *at = NULL; GdipCreateImageAttributes(&at);
    GpColorMatrix cm; memset(&cm, 0, sizeof(cm)); cm.m[0][0] = cm.m[1][1] = cm.m[2][2] = cm.m[4][4] = 1.0f; cm.m[3][3] = a;
    if (at) GdipSetImageAttributesColorMatrix(at, 0, TRUE, &cm, NULL, 0);
    GdipDrawImageRectRectI(g, img, x, y, w, h, 0, 0, (INT)iw, (INT)ih, 2, at, NULL, NULL);
    if (at) GdipDisposeImageAttributes(at);
}
static void DrawCover(GpGraphics *g, GpImage *img, int x, int y, int w, int h)   /* cover crop, no stretching */
{
    UINT iw = 0, ih = 0; GdipGetImageWidth(img, &iw); GdipGetImageHeight(img, &ih); if (!iw || !ih) return;
    double s = (double)w / iw > (double)h / ih ? (double)w / iw : (double)h / ih;
    int sw = (int)(w / s), sh = (int)(h / s);
    GdipSetInterpolationMode(g, s > 1.0 ? 6 : 7);   /* 4.0.1: enlarging (the art in 2K/4K) needs no bicubic: 3x faster */
    GpImageAttributes *ta = img == g_imgBg ? ThemeImageAttr(1.0f) : NULL;   /* 6.0: PORTHEON's background takes its color */
    GdipDrawImageRectRectI(g, img, x, y, w, h, (int)(iw - sw) / 2, (int)(ih - sh) / 3, sw, sh, 2, ta, NULL, NULL);
    if (ta) GdipDisposeImageAttributes(ta);
    GdipSetInterpolationMode(g, 5);
}
/* the part of the window being repainted: what is drawn is clipped to it. Everything is drawn in the frame's own
   coordinates (the world transform moves the frame by g_ox, g_oy in full screen). */
static RECT g_paintRect; static HRGN g_paintRgn;
static void ClipBase(GpGraphics *g)
{
    if (g_paintRgn) GdipSetClipHrgn(g, g_paintRgn, 0);
    else GdipSetClipRectI(g, g_paintRect.left - g_ox, g_paintRect.top - g_oy, g_paintRect.right - g_paintRect.left, g_paintRect.bottom - g_paintRect.top, 0);
}
static BOOL Seen(RECT r) { OffsetRect(&r, g_ox, g_oy); return g_paintRgn ? RectInRegion(g_paintRgn, &r) : (r.left < g_paintRect.right && r.right > g_paintRect.left && r.top < g_paintRect.bottom && r.bottom > g_paintRect.top); }
#define CLIPPATH(g, p) do { ClipBase(g); GdipSetClipPath((g), (p), 1 /* intersect */); } while (0)

/* ---------------------------------------------------------------- cached pictures (rebuilt when the scale or the art changes) */
static GpBitmap *g_bgArt[NGAMES], *g_tileBmp[3][NGAMES], *g_neutralBg;
static int g_bgArtW, g_bgArtH, g_tileS[3][NGAMES], g_neutralW, g_neutralH;
static void FreeBmp(GpBitmap **b) { if (*b) { GdipDisposeImage((GpImage *)*b); *b = NULL; } }
#define NGLASS (2 * NGAMES + 1)
static GpBitmap *g_glassC[NGLASS]; static int g_glassW[NGLASS], g_glassH[NGLASS];   /* 5.0: frosted copies of the backgrounds (GlassPanel): each game's, each game's dimmed hub, the neutral one */
static GpBitmap *g_bgDim; static int g_bgDimGame;
/* perf 11-13: the crossfade's old background as a plain picture GDI can blend (PaintBackground, ui_screens.c), kept for
   the 520 ms of the fade */
int WINAPI GdipFlush(GpGraphics *, int);
static HBITMAP g_fadeKeep; static int g_fadeKeepW, g_fadeKeepH, g_fadeKeepGame = -1; static GpBitmap *g_fadeKeepOf;
static void FadeKeepFree(void) { if (g_fadeKeep) { DeleteObject(g_fadeKeep); g_fadeKeep = NULL; } g_fadeKeepGame = -1; g_fadeKeepOf = NULL; }
static void ArtChanged(int g) { if (g_fadeKeepGame == g) FadeKeepFree(); if (g_bgDimGame == g) { FreeBmp(&g_bgDim); g_bgDimGame = -1; } FreeBmp(&g_bgArt[g]); for (int k = 0; k < 3; k++) FreeBmp(&g_tileBmp[k][g]); FreeBmp(&g_glassC[g]); FreeBmp(&g_glassC[NGAMES + g]); }
static void ScaleChanged(void) { for (int i = 0; i < NGAMES; i++) ArtChanged(i); FreeBmp(&g_bgDim); g_bgDimGame = -1; FreeBmp(&g_neutralBg); for (int k = 0; k < NGLASS; k++) FreeBmp(&g_glassC[k]); LibArtFree(); VitReset(); LogoSmallFree(); FadeKeepFree(); }   /* 6.1: LibArtFree; perf 11-13: LogoSmallFree, FadeKeepFree */
static GpBitmap *NewBitmap(int w, int h, GpGraphics **g)
{
    GpBitmap *b = NULL; *g = NULL;
    if (GdipCreateBitmapFromScan0(w, h, 0, 0x000E200B /* PixelFormat32bppPARGB */, NULL, &b) != 0 || !b) return NULL;
    GdipGetImageGraphicsContext((GpImage *)b, g);
    if (*g) { GdipSetInterpolationMode(*g, 7); GdipSetSmoothingMode(*g, 4); GdipSetPixelOffsetMode(*g, 4); }
    return b;
}
/* 5.0: the cinema frame: the corners and the edges fall to black */
static void Vignette(GpGraphics *g, int w, int h)
{
    VGrad(g, 0, 0, w, h / 6, A(RGB(0, 0, 0), 110), A(RGB(0, 0, 0), 0));
    VGrad(g, 0, h - h / 5, w, h / 5, A(RGB(0, 0, 0), 0), A(RGB(0, 0, 0), 150));
    HGrad(g, w - w / 7, 0, w / 7, h, A(RGB(0, 0, 0), 0), A(RGB(0, 0, 0), 120));
}
/* the console's own background (no game art): 6.0, a deep night-blue gradient (the console's look) */
static GpBitmap *NeutralBg(int w, int h)
{
    if (g_neutralBg && g_neutralW == w && g_neutralH == h) return g_neutralBg;
    FreeBmp(&g_neutralBg); g_neutralW = w; g_neutralH = h;
    GpGraphics *g = NULL; GpBitmap *b = NewBitmap(w, h, &g); if (!b || !g) return b;
    /* 6.0 (the user: "exactly like the PS5"): the console's own background: a deep night-blue gradient with two soft
       lights, nothing drawn on it */
    VGrad(g, 0, 0, w, h, A(RGB(16, 20, 34), 255), A(RGB(3, 4, 8), 255));
    GlowRaw(g, w * 3 / 4, -h / 8, w * 7 / 10, h * 7 / 10, A(RGB(64, 84, 150), 54));
    GlowRaw(g, w / 8, h, w * 6 / 10, h * 6 / 10, A(RGB(40, 54, 110), 40));
    Vignette(g, w, h);
    GdipDeleteGraphics(g); g_neutralBg = b; return b;
}
#include "skin.c"       /* 6.2: each game's own look */
/* a game's background: its own art (pic1) filling the window, darkened toward the bottom and the left where the text
   goes; without art, its colors over the circuit */
static GpBitmap *GameBg(int gi, int w, int h)
{
    if (g_bgArtW != w || g_bgArtH != h) { for (int i = 0; i < NGAMES; i++) FreeBmp(&g_bgArt[i]); g_bgArtW = w; g_bgArtH = h; }
    if (g_bgArt[gi]) return g_bgArt[gi];
    LoadArt(gi);
    const GameSkin *sk = SkinOf(gi);
    GpGraphics *g = NULL; GpBitmap *bmp = NewBitmap(w, h, &g); if (!bmp || !g) return bmp;
    Fill(g, 0, 0, w, h, A(C_BG, 255));
    if (g_art[gi]) SkinDrawArt(g, g_art[gi], sk, w, h);
    else {
        POINT a = {0, 0}, z = {w, h}; GpBrush *lb = NULL; GdipCreateLineBrushI(&a, &z, A(G[gi].c2, 255), A(Mix(G[gi].c1, C_BG, 0.35f), 255), 0, &lb);
        if (lb) { GdipFillRectangleI(g, lb, 0, 0, w, h); GdipDeleteBrush(lb); }
        if (g_imgBg) DrawAlpha(g, g_imgBg, 0, 0, w, h, 0.22f);
    }
    if (sk) SkinGrade(bmp, sk, w, h);   /* 6.2: the game's own colors over its art */
    /* 6.0 (the user: "exactly like the PS5"): the art shows clean, as on the console's home; only black gradients keep
       the words readable: under the top bar and the row, from the left behind the name, and toward the bottom cards */
    {   /* a bright, busy picture (UFC 4's) gets a light black veil, so the text always reads: its mean light, from an 8x8 copy */
        GpGraphics *sg = NULL; GpBitmap *sm = NewBitmap(8, 8, &sg); int lum = 0;
        if (sm && sg) { GdipSetInterpolationMode(sg, 6); GdipDrawImageRectI(sg, (GpImage *)bmp, 0, 0, 8, 8); GdipDeleteGraphics(sg);
            for (int yy = 0; yy < 8; yy++) for (int xx = 0; xx < 8; xx++) { UINT32 c = 0; GdipBitmapGetPixel(sm, xx, yy, &c); lum += (int)(((c >> 16) & 255) * 30 + ((c >> 8) & 255) * 59 + (c & 255) * 11) / 100; }
            lum /= 64; }
        FreeBmp(&sm);
        if (lum > 70) Fill(g, 0, 0, w, h, A(RGB(0, 0, 0), (BYTE)((lum > 150 ? 110 : (lum - 70) * 110 / 80) * HPV(7, 10) / 10)));
    }
    /* Released (the two pages): the lightest veil that keeps the words readable: the art shows nearly clean, as on the console */
    VGrad(g, 0, 0, w, h * 34 / 100, A(RGB(0, 0, 0), HPV(140, 170)), A(RGB(0, 0, 0), 0));
    VGrad(g, 0, h * 50 / 100, w, h - h * 50 / 100, A(RGB(0, 0, 0), 0), A(RGB(0, 0, 0), HPV(196, 226)));
    if (sk && SKIN_SIDE(sk)) HGrad(g, w * 36 / 100, 0, w * 64 / 100, h, A(RGB(0, 0, 0), 0), A(RGB(0, 0, 0), 190));   /* 6.2: the name stands on the right */
    else HGrad(g, 0, 0, w * HPV(54, 64) / 100, h, A(RGB(0, 0, 0), HPV(156, 190)), A(RGB(0, 0, 0), 0));
    if (sk) {   /* 6.2: a busy art gets a veil behind the name */
        int veil = sk->motif == SKM_SPLIT ? 132 : sk->motif == SKM_DIAGONALS ? 112 : sk->motif == SKM_BLOCKS ? 64 : sk->motif == SKM_OCTAGON ? 50 : 0;
        if (HOME_PAGES) veil = veil * 6 / 10;
        if (veil && SKIN_SIDE(sk)) { HGrad(g, w * 24 / 100, 0, w * 14 / 100 + 1, h, A(RGB(0, 0, 0), 0), A(RGB(0, 0, 0), (BYTE)veil)); Fill(g, w * 38 / 100, 0, w - w * 38 / 100, h, A(RGB(0, 0, 0), (BYTE)veil)); }
        else if (veil) { Fill(g, 0, 0, w * 62 / 100, h, A(RGB(0, 0, 0), (BYTE)veil)); HGrad(g, w * 62 / 100, 0, w * 14 / 100 + 1, h, A(RGB(0, 0, 0), (BYTE)veil), A(RGB(0, 0, 0), 0)); }   /* 7.0: the name stands on the left */
        VGrad(g, 0, h * 86 / 100, w, h - h * 86 / 100, A(RGB(0, 0, 0), 0), A(RGB(0, 0, 0), HPV(200, 235))); SkinMotif(g, sk, w, h);
    }   /* 6.2: the art's small print goes, the game's drawing comes */
    Vignette(g, w, h);
    GdipDeleteGraphics(g);
    g_bgArt[gi] = bmp; return bmp;
}
/* 5.0: a game's hub shows its background darker: one cached copy (it was a full-window fill at every paint) */
static GpBitmap *g_bgDim; static int g_bgDimGame = -1, g_bgDimW, g_bgDimH;
static GpBitmap *GameBgDim(int gi, int w, int h)
{
    if (g_bgDim && g_bgDimGame == gi && g_bgDimW == w && g_bgDimH == h) return g_bgDim;
    FreeBmp(&g_bgDim); GpBitmap *src = GameBg(gi, w, h); if (!src) return NULL;
    GpGraphics *g = NULL; g_bgDim = NewBitmap(w, h, &g); if (!g_bgDim || !g) return src;
    GdipSetCompositingMode(g, 1); GdipDrawImageRectI(g, (GpImage *)src, 0, 0, w, h); GdipSetCompositingMode(g, 0);
    Fill(g, 0, 0, w, h, A(C_BG, 150)); GdipDeleteGraphics(g);
    g_bgDimGame = gi; g_bgDimW = w; g_bgDimH = h; return g_bgDim;
}
/* 5.0: frosted glass. The background under a panel, blurred (drawn down to 1/16 and back up to 1/4 of its size: a
   soft blur for the price of two small copies, made once per background) and tinted: the art glows through the
   panels without stealing the text. The panel draws only its own part of that quarter-size picture, stretched. */
static void BgFrame(RECT rc, int *X, int *Y, int *W, int *H) { (void)rc; *X = 0; *Y = 0; *W = SC(BASE_W); *H = SC(BASE_H); if (g_ox || g_oy) { RECT c; GetClientRect(g_wnd, &c); *X = -g_ox; *Y = -g_oy; *W = c.right; *H = c.bottom; } }
static GpBitmap *GlassBmp(void)
{
    int X, Y, W, H; RECT rc = {0}; BgFrame(rc, &X, &Y, &W, &H);
    int key = g_view == V_HOME ? g_sel : g_view == V_GAME ? NGAMES + g_sel : 2 * NGAMES;   /* the hub's glass shows its dimmed background */
    if (g_glassC[key] && g_glassW[key] == W && g_glassH[key] == H) return g_glassC[key];
    FreeBmp(&g_glassC[key]); g_glassW[key] = W; g_glassH[key] = H;
    GpBitmap *src = key < NGAMES ? GameBg(key, W, H) : key < 2 * NGAMES ? GameBgDim(key - NGAMES, W, H) : NeutralBg(W, H); if (!src || W < 64 || H < 64) return NULL;
    /* down to 1/4 and then 1/16 with a cheap filter (the blur hides it), back up to 1/4 smooth: about 2 ms at 1080p */
    GpGraphics *g0 = NULL, *g1 = NULL, *g2 = NULL; GpBitmap *q = NewBitmap(W / 4, H / 4, &g0), *tiny = NewBitmap(W / 16, H / 16, &g1);
    if (!q || !g0 || !tiny || !g1) { if (g0) GdipDeleteGraphics(g0); if (g1) GdipDeleteGraphics(g1); FreeBmp(&q); FreeBmp(&tiny); return NULL; }
    GdipSetInterpolationMode(g0, 6); GdipDrawImageRectI(g0, (GpImage *)src, 0, 0, W / 4, H / 4); GdipDeleteGraphics(g0);
    GdipSetInterpolationMode(g1, 6); GdipDrawImageRectI(g1, (GpImage *)q, 0, 0, W / 16, H / 16); GdipDeleteGraphics(g1);
    FreeBmp(&q); g_glassC[key] = NewBitmap(W / 4, H / 4, &g2);
    if (g_glassC[key] && g2) { GdipSetInterpolationMode(g2, 7); GdipDrawImageRectI(g2, (GpImage *)tiny, -2, -2, W / 4 + 4, H / 4 + 4); GdipDeleteGraphics(g2); }
    FreeBmp(&tiny); return g_glassC[key];
}
/* a glass panel: the frosted background, a dark violet tint (alpha `tint`), a light sheen on its upper part and a thin
   edge that is lighter at the top, as light catching the glass */
static void GlassPanel(GpGraphics *g, RECT r, int rad, BYTE tint)
{
    GpPath *p = RoundPath(r.left, r.top, r.right - r.left - 1, r.bottom - r.top - 1, rad);
    GpBitmap *gl = GlassBmp(); int X, Y, W, H; RECT rc = {0}; BgFrame(rc, &X, &Y, &W, &H);
    CLIPPATH(g, p);
    if (gl) {
        GdipSetInterpolationMode(g, g_fxLight ? 3 : 6);
        int sx = (r.left - X) / 4, sy = (r.top - Y) / 4, sw = (r.right - r.left) / 4 + 1, sh = (r.bottom - r.top) / 4 + 1;
        GdipDrawImageRectRectI(g, (GpImage *)gl, r.left, r.top, sw * 4, sh * 4, sx, sy, sw, sh, 2, NULL, NULL, NULL);
        GdipSetInterpolationMode(g, 5);
    }
    /* 6.0 (the console's look): smoked glass with no colour cast, a faint white sheen and a white edge */
    { GpBrush *b = NULL; GdipCreateSolidFill(A(RGB0(14, 15, 20), gl ? tint : (BYTE)(tint + (255 - tint) / 2)), &b); if (b) { GdipFillPath(g, b, p); GdipDeleteBrush(b); } }
    VGrad(g, r.left, r.top, r.right - r.left, (r.bottom - r.top) / 2, A(RGB0(255, 255, 255), 10), A(RGB0(255, 255, 255), 0));
    ClipBase(g);
    POINT a = {0, r.top}, z = {0, r.bottom}; GpBrush *lb = NULL; GdipCreateLineBrushI(&a, &z, A(RGB0(255, 255, 255), 62), A(RGB0(255, 255, 255), 14), 0, &lb);
    if (lb) { GpPen *pen = NULL; GdipCreatePen2(lb, 1.0f, 2, &pen); if (pen) { GdipDrawPath(g, pen, p); GdipDeletePen(pen); } GdipDeleteBrush(lb); }
    GdipDeletePath(p);
}
#define TILE_RAD(s) ((s) * 14 / 100)   /* 7.0: a tile's round corner, for its picture, its outline and what is clipped to it */
/* a game's square picture at one size (3 cached sizes: tiles, the hub, small): its icon, else its colors and name */
static GpBitmap *TileBmp(int gi, int s, int slot)
{
    /* 4.0.1: the size is kept per game: the home row has two sizes (the chosen game is bigger), and one size for the
       whole slot rebuilt every tile at every paint (about 40 ms each time) */
    if (g_tileS[slot][gi] != s) { FreeBmp(&g_tileBmp[slot][gi]); g_tileS[slot][gi] = s; }
    if (g_tileBmp[slot][gi]) return g_tileBmp[slot][gi];
    LoadArt(gi);
    GpGraphics *g = NULL; GpBitmap *bmp = NewBitmap(s, s, &g); if (!bmp || !g) return bmp;
    GpPath *p = RoundPath(0, 0, s - 1, s - 1, TILE_RAD(s)); GdipSetClipPath(g, p, 0);
    if (g_icon[gi]) DrawCover(g, g_icon[gi], 0, 0, s, s);
    else if (g_art[gi]) DrawCover(g, g_art[gi], 0, 0, s, s);
    else {
        POINT a = {0, 0}, z = {s, s}; GpBrush *lb = NULL; GdipCreateLineBrushI(&a, &z, A(G[gi].c1, 255), A(G[gi].c2, 255), 0, &lb);
        if (lb) { GdipFillRectangleI(g, lb, 0, 0, s, s); GdipDeleteBrush(lb); }
        GpPen *pen = NULL; GdipCreatePen1(A(RGB(255, 255, 255), 16), (float)(s / 14 > 1 ? s / 14 : 1), 2, &pen);
        for (int k = -s; k < s; k += s / 5 > 0 ? s / 5 : 1) GdipDrawLineI(g, pen, k, s, k + s, 0);
        GdipDeletePen(pen);
    }
    GdipDeletePath(p); GdipDeleteGraphics(g);
    g_tileBmp[slot][gi] = bmp; return bmp;
}
static void GameTile(HDC dc, GpGraphics *g, int gi, int x, int y, int s, int slot)   /* the picture, and the name over it when there is no icon */
{
    /* the home row's tiles grow and shrink (PaintHome: 128 to 128 + 46): one picture at the biggest size, scaled */
    GpBitmap *b = TileBmp(gi, slot == 0 ? SC(128) + SC(46) : s, slot); if (b) GdipDrawImageRectI(g, (GpImage *)b, x, y, s, s);
    if (!g_icon[gi] && !g_art[gi]) {
        /* 6.0: Zelda and God of War III ship no art: their name in two lines (a title and a subtitle) */
        const wchar_t *top = ZELDAG(gi) ? L"ZELDA" : GOW3G(gi) ? L"GOD OF WAR" : BBG(gi) ? L"BLOODBORNE" : ISSG(gi) ? L"inFAMOUS" : FNCG(gi) ? L"FIGHT NIGHT" : NULL,
                      *sub = ZELDAG(gi) ? L"Ocarina of Time" : GOW3G(gi) ? L"III" : BBG(gi) ? L"" : ISSG(gi) ? L"Second Son" : FNCG(gi) ? L"Champion" : NULL;   /* 6.1 (sub is never NULL with top) */
        if (top) {
            HFONT f1 = CreateFontW(-(s * (wcslen(top) > 6 ? 15 : 24) / 100), 0, 0, 0, FW_BLACK, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift"),
                  f2 = CreateFontW(-(s * (wcslen(sub) > 6 ? 12 : 22) / 100), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
            RECT r1 = {x + s / 20, y + s * 24 / 100, x + s - s / 20, y + s * 54 / 100}; TextR(dc, f1, RGB(255, 255, 255), r1, top, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
            RECT r2 = {x + s / 20, y + s * 54 / 100, x + s - s / 20, y + s * 80 / 100}; TextR(dc, f2, RGB(236, 236, 236), r2, sub, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
            DeleteObject(f1); DeleteObject(f2);
        } else {
            HFONT f = CreateFontW(-(s * 30 / 100), 0, 0, 0, FW_BLACK, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
            RECT tr = {x, y, x + s, y + s}; TextR(dc, f, RGB(255, 255, 255), tr, G[gi].name, DT_CENTER | DT_VCENTER); DeleteObject(f);
        }
    }
}

/* ---------------------------------------------------------------- animation state */
static float g_tileScale[NGAMES + 4]; static int g_fadeFrom = -1; static DWORD g_fadeT0, g_bootT0, g_viewT0;
static float g_easeK = 0.30f;   /* perf 11-13: the tiles' easing step of this tick (Animate; LibAnimate) */
static BOOL g_tilesMove;   /* perf 11-13: the tiles were still growing or shrinking at the last tick (Animate; read by VitAnimate, home62.c) */
#define BOOT_MS 2500   /* 4.4.0: the opening's length (the user: "a spectacular entrance, like a console"); any key or click skips it */
static int g_idleTick, g_focus = -1; static BOOL g_kbNav;
static float FadeT(void) { DWORD t = GetTickCount() - g_fadeT0; return g_fadeFrom < 0 || g_fxLight ? 1.0f : t >= 520 ? 1.0f : t / 520.0f; }
static void FeatureChanged(int old) { g_fadeFrom = old; g_fadeT0 = GetTickCount(); g_heroT0 = GetTickCount(); Motion(960); if (HOME_PAGES) { g_homePage = 0; g_pageT0 = 0; } }   /* Released: another game is shown on page 1 */   /* 6.2: 960: the light over the chosen tile (ui_screens.c) */
static void Dirty(int x, int y, int w, int h) { RECT r = {SC(x) + g_ox, SC(y) + g_oy, SC(x + w) + g_ox, SC(y + h) + g_oy}; InvalidateRect(g_wnd, &r, FALSE); }
static void DirtyHit(int id, int grow) { RECT r; if (HitRect(id, &r)) { InflateRect(&r, SC(grow), SC(grow)); OffsetRect(&r, g_ox, g_oy); InvalidateRect(g_wnd, &r, FALSE); } }
static void SetView(int v)
{
    if (v != g_view) {
        if ((v == V_SETTINGS || v == V_INSTALL || v == V_ONLINE) && (g_view == V_HOME || g_view == V_GAME)) g_viewBack = g_view;
        g_view = v; g_viewT0 = GetTickCount(); g_focus = -1; Motion(300);   /* 5.0: the cut from black (PaintInner) */
    }
    InvalidateRect(g_wnd, NULL, FALSE);
}
static void PadPoll(void);
/* 5.0: the mouse moved onto or off something: only those two places are drawn again (a whole 1080p frame costs about
   24 ms, a 4K one four times that; every hover repainted all of it). The top bar's icons show a label under them, so a
   change there redraws the bar; the screens whose hover moves things elsewhere still redraw whole. */
static BOOL TopBarHit(int h) { return (h >= H_NAV && h < H_NAV + 4) || h == H_ARENATOP || h == H_POWER || h == H_FULLSCREEN || h == H_PROFILE || h == H_LANGBTN || h == H_UPDATE; }
static void HotChanged(int old, int now)
{
    if (old == now) return;
    if (now == H_PLAY) { g_sheenT0 = GetTickCount(); }   /* 5.0: the blade of light crosses PLAY once */
    if (g_overlay || (g_view != V_HOME && g_view != V_GAME && g_view != V_SETTINGS)) { InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (TopBarHit(old) || TopBarHit(now)) Dirty(0, 0, BASE_W, 92);
    if (old >= 0 && !TopBarHit(old)) DirtyHit(old, 40);
    if (now >= 0 && !TopBarHit(now)) DirtyHit(now, 40);
}
/* 5.0: the effects tier. ui_effects: 0 automatic, 1 full, 2 light. Automatic is light on a very weak PC, in a remote
   desktop session and with Windows' battery saver on: no looping glow, no glow rings, no crossfades */
static void FxTier(void)
{
    int m = PIni(L"ui_effects", 0); BOOL light = m == 2;
    if (m == 0) { SYSTEM_POWER_STATUS ps; light = PcTier() == 0 || GetSystemMetrics(SM_REMOTESESSION) || (GetSystemPowerStatus(&ps) && ps.SystemStatusFlag == 1); }
    g_fxLightHard = m == 2 || (m == 0 && PcTier() == 0);   /* 6.2 fix: battery saver turning on (a laptop unplugged) no longer takes the YouTube clip away */
    if (light != g_fxLight) { g_fxLight = light; InvalidateRect(g_wnd, NULL, FALSE); }
}
/* 4.0.4: where the notification goes (its right edge and top, in frame units): on the home screen beside the chosen
   game's name, in a game's hub to the left of PLAY (it covered PLAY), elsewhere at the top right */
static void ToastPlace(int *right, int *top)
{
    if (g_view == V_HOME) { *right = BASE_W - MX; *top = 76; }   /* 7.0: at the top right, above the video (the web view would cover a notification drawn in its place) */
    else if (g_view == V_GAME) { *right = MX + CW - 292; *top = 96; }
    else { *right = BASE_W - MX; *top = 76; }
}
#ifdef PORTHEON_TEST   /* Released: keys for the UI shots (the .go hook below); Paint counts the frames */
static int g_tkey[16], g_ntkey, g_tkeyAt; static DWORD g_tkeyPaint, g_testPaints;
static void NavKey(int vk); static BOOL HomeWheel(int delta);
enum { H_HOMEMORE_T = 903 };   /* (home62.c's H_HOMEMORE, declared after this file's start) */
#endif
/* every 33 ms: the controller, and the parts that move */
static void Animate(void)
{
    PadPoll();
    MusicFollow();   /* 6.0: the chosen game's music starts, follows the choice and stops in the settings or behind other windows */
#ifdef PORTHEON_TEST   /* 5.0 UI shots: <PORTHEON_TEST_DUMP>.go holds "view game tab overlay settings-section hot":
                          the screen is set at once (no animation) and the next frame goes to <base>.png */
    {   static wchar_t dp[MAX_PATH]; static int has = -1; if (has < 0) has = GetEnvironmentVariableW(L"PORTHEON_TEST_DUMP", dp, MAX_PATH - 8) > 0;
        wchar_t go[MAX_PATH]; if (has) { _snwprintf(go, MAX_PATH, L"%s.go", dp); FILE *f = _wfopen(go, L"r");
            if (f) { int v = -1, s = -1, t = -1, o = -1, e = -1, h = -1, th = -1, pg = 0, sl = 0; fwscanf(f, L"%d %d %d %d %d %d %d %d %d", &v, &s, &t, &o, &e, &h, &th, &pg, &sl);
                g_ntkey = 0; while (g_ntkey < 16 && fwscanf(f, L"%d", &g_tkey[g_ntkey]) == 1) g_ntkey++;   /* Released: then keys (below) */
                g_tkeyAt = 0; g_tkeyPaint = g_testPaints; fclose(f); DeleteFileW(go);
                g_homePage = HOME_PAGES && pg == 1; g_pageT0 = sl > 0 ? GetTickCount() - (DWORD)sl : 0; g_pageFocus = FALSE;   /* Released: an 8th number = the home's page (1 = the second); a 9th = that many ms into the change to that page */
                if (s >= 0 && s < NGAMES) { g_sel = s; g_stTick = 0; }
                if (v >= 0) { g_view = v; g_viewT0 = GetTickCount() - 5000; if (v == V_LAUNCH) { g_launchGame = g_sel; g_launchT0 = GetTickCount() - 5000; } if (v == V_BOOT) g_bootT0 = GetTickCount() - 1500; }
                if (th >= 0 && th < NTHEMES) { ThemeSet(th); ScaleChanged(); }   /* 6.0: a 7th number = the color */
                if (t >= 0) g_tab = t; if (o >= 0) g_overlay = o; if (e >= 0) g_setSec = e; g_hot = h; g_focus = -1; g_fadeFrom = -1;
                if (GetEnvironmentVariableW(L"PORTHEON_TEST_KBNAV", NULL, 0)) { g_kbNav = TRUE; g_focus = h; }   /* 7.0: the focus ring in the shots */
                if (o == OV_KEYS) { void KeysOpenT(int); KeysOpenT(g_sel); }   /* 6.0: the keyboard screen with the game's keys */
                if (o == 61) { void AchLoadT(void); AchLoadT(); }   /* 6.2: OV_ACHV */
                if (o == 62) { void PmOpenT(int); PmOpenT(g_sel); }   /* 6.2: OV_PADMAP */
                if (o == 63) { void CkOpenT(void); CkOpenT(); }   /* 6.2: OV_CKEYS */
                if (o == 41) { void CpT(void); CpT(); }   /* 6.2: a custom game's page */
                for (int i = 0; i < NGAMES + 4; i++) g_tileScale[i] = i < NGAMES ? (i == g_sel && g_csel < 0 ? 1.0f : g_hot == H_CARD + i ? 0.3f : 0.0f) : (g_hot == H_SYSTILE + (i - NGAMES) ? 0.6f : 0.0f);
                g_msg[0] = 0; InvalidateRect(g_wnd, NULL, FALSE);
                VitTestSettle();   /* 6.2: the vitrine ready in the shot */
                if (sl > 0) g_pageT0 = GetTickCount() - (DWORD)sl;   /* (again: the vitrine's pictures took their time) */
                if (!g_ntkey) { _snwprintf(go, MAX_PATH, L"%s.req", dp); FILE *r = _wfopen(go, L"w"); if (r) fclose(r); } } }
        /* Released (the home's two pages): numbers after the 9th are keys, pressed one per painted frame before the shot:
           the arrows (37-40) and Esc (27) through NavKey, 1001 / 1002 = the wheel down / up, 13 = Enter ONLY on "Mais" /
           "Voltar" (a test never presses PLAY); Left / Right are skipped on a tile (they would choose another game) */
        if (has && g_ntkey && g_testPaints != g_tkeyPaint && !g_pageT0) {
            g_tkeyPaint = g_testPaints;
            if (g_tkeyAt < g_ntkey) {
                int k = g_tkey[g_tkeyAt++]; BOOL tile = (g_focus >= H_CARD && g_focus < H_CARD + NGAMES) || (g_focus >= H_CUSTOM && g_focus < H_CUSTOM + MAXCUSTOM);
                if (k == 1001) HomeWheel(-120); else if (k == 1002) HomeWheel(120);
                else if (k == VK_RETURN) { if (g_focus == H_HOMEMORE_T) NavKey(k); }
                else if (k == VK_UP || k == VK_DOWN || k == VK_ESCAPE || ((k == VK_LEFT || k == VK_RIGHT) && !tile)) NavKey(k);
                InvalidateRect(g_wnd, NULL, FALSE);
            } else { g_ntkey = 0;
                _snwprintf(go, MAX_PATH, L"%s.keylog", dp); FILE *kl = _wfopen(go, L"a"); if (kl) { fwprintf(kl, L"page=%d focus=%d hot=%d sel=%d view=%d overlay=%d\n", g_homePage, g_focus, g_hot, g_sel, g_view, g_overlay); fclose(kl); }   /* (what the keys led to) */
                _snwprintf(go, MAX_PATH, L"%s.req", dp); FILE *r = _wfopen(go, L"w"); if (r) fclose(r); InvalidateRect(g_wnd, NULL, FALSE); }
        } }
#endif
    VitAnimate();   /* 6.2: the vitrine pauses itself (minimized, behind, a game) and runs its own timer */
    if (IsIconic(g_wnd) || g_proc || !IsWindowVisible(g_wnd)) return;
    {   /* perf 11-13: the tick is asked every 30 ms now (with 33 it came every 47: 21 frames a second); the tiles' easing
           keeps the speed it had (0.30 of the way per 47 ms) whatever the tick's real length */
        DWORD nowA = GetTickCount(), dt = g_anim ? nowA - g_anim : 47; if (dt < 8) dt = 8; if (dt > 94) dt = 94;
        g_easeK = 1.0f - powf(0.70f, (float)dt / 47.0f); g_anim = nowA; }
    g_idleTick++;
    LiveTick();
    if (g_idleTick % 60 == 1) FxTier();
    {   /* 5.0: the looping effects run while someone uses PORTHEON (8 s after the last input) and it is in front */
        BOOL calm = g_fxLight || GetForegroundWindow() != g_wnd || g_anim - g_lastInput > 8000;
        if (calm != g_calm) { g_calm = calm; if (g_updState >= 1) Dirty(0, 0, BASE_W, 70); }
    }
    if (g_view == V_LAUNCH) { LaunchTick(); return; }   /* 4.4.0: the game's picture (also in the window that hands over to the PLAY instance) */
    if (g_view == V_BOOT) { if (g_anim - g_bootT0 > BOOT_MS) BootDone(); else HomePrewarmTile(); /* perf 11-13 (ui_screens.c) */ InvalidateRect(g_wnd, NULL, FALSE); return; }   /* 4.4.0: the new opening (ui_screens.c, PaintBoot) */
    BOOL front = GetForegroundWindow() == g_wnd, tiles = FALSE;
    for (int i = 0; i < NGAMES + 4; i++) {
        float target = i < NGAMES ? (i == g_sel && g_csel < 0 ? 1.0f : g_hot == H_CARD + i ? 0.3f : 0.0f) : (g_hot == H_SYSTILE + (i - NGAMES) ? 0.6f : 0.0f);   /* 6.1: g_csel */
        if (fabsf(g_tileScale[i] - target) > 0.005f) { g_tileScale[i] += (target - g_tileScale[i]) * g_easeK; tiles = TRUE; } else if (g_tileScale[i] != target) { g_tileScale[i] = target; tiles = TRUE; }
    }
    if (LibAnimate()) tiles = TRUE;   /* 6.1: the custom tiles and "+ ADICIONAR" (ui_library.c) */
    g_tilesMove = tiles;
    if (tiles && g_view == V_HOME) Dirty(0, 60, BASE_W, 178);   /* the row of tiles (7.0: y 60..238, it ends above the video at 244) */
    {   /* 5.0: the cinematic motions (a new game, a new screen) repaint until they end, then once more to settle */
        BOOL now = g_motionOn && (int)(g_motionUntil - g_anim) > 0;
        if (g_motionOn) InvalidateRect(g_wnd, NULL, FALSE);   /* while it runs, and once more after it ends */
        if (!now) g_motionOn = FALSE;
        static BOOL sw; BOOL sn = MotionT(g_sheenT0, 520) < 1.0f;
        if (sn || sw) DirtyHit(H_PLAY, 4); sw = sn;
    }
    if (g_fadeFrom >= 0) { if (FadeT() >= 1.0f) g_fadeFrom = -1; InvalidateRect(g_wnd, NULL, FALSE); }   /* the background's crossfade */
    if (HOME_PAGES && g_pageT0) { if (MotionT(g_pageT0, HP_SLIDE_MS) >= 1.0f) g_pageT0 = 0; if (g_view == V_HOME) InvalidateRect(g_wnd, NULL, FALSE); }   /* Released: the home's page change (while it runs, and once more to settle; not a Motion(): the clip stays) */
    if (!g_calm && front && !g_overlay && g_updState >= 1 && g_idleTick % 3 == 0) Dirty(0, 0, BASE_W, 70);   /* 5.0: only the update pill still breathes */
    DWORD age = GetTickCount() - g_msgAt;
    if (g_msg[0] && age < 9000 && g_idleTick % 2 == 0) { int tr, tt; ToastPlace(&tr, &tt); Dirty(tr - 500, tt - 6, 560, 140); }   /* the notification */
    if (g_msg[0] && age >= 8000 && age < 9000 && g_view != V_HOME && g_idleTick % 6 == 0) Dirty(MX, 750, CW, 40);   /* 6.2: the status line goes back to calm facts */
    /* 4.4.0: the welcome and "who is playing?" fade in whole, then only their moving part is drawn again (a whole 1080p
       frame of them costs about 33 ms: every other tick that was half a core) */
    if ((g_view == V_WELCOME || g_view == V_PROFILES) && !g_overlay) {
        if (GetTickCount() - g_viewT0 < 650) InvalidateRect(g_wnd, NULL, FALSE);
        else if (!g_calm && g_idleTick % 2 == 0) { if (g_view == V_WELCOME) Dirty(BASE_W / 2 - 140, 368 - 140, 280, 280); else Dirty(0, 270, BASE_W, 210); }   /* 5.0: only while someone is using it */
    }
    else if (g_overlay == OV_UPDATE || g_scanning || (g_view == V_INSTALL && GetTickCount() - g_viewT0 < 1200)) { if (g_idleTick % 2 == 0) InvalidateRect(g_wnd, NULL, FALSE); }   /* 5.0: the installer no longer repaints whole forever */
    {   static WORD lastMin = 0xFFFF; SYSTEMTIME t; GetLocalTime(&t); if (t.wMinute != lastMin) { lastMin = t.wMinute; Dirty(BASE_W - 330, 0, 330, 70); } }   /* the clock: only when its minute turns */
}

/* ---------------------------------------------------------------- the games' status, read at most once a second while painting */
static int g_stCache[NGAMES];
static void StatusDirty(void) { g_stTick = 0; }
static int St(int g)
{
    DWORD now = GetTickCount();
    if (!g_stTick || now - g_stTick > 1000) { for (int i = 0; i < NGAMES; i++) g_stCache[i] = Status(i); g_stTick = now ? now : 1; }
    return g_stCache[g];
}

/* ---------------------------------------------------------------- widgets */
static COLORREF StatusColor(int s) { return s == S_READY ? C_CYAN : s == S_RUNNING ? C_GOOD : s == S_PICK ? C_ORANGE : C_BAD; }
static const wchar_t *StatusText(int st) { return st == S_PICK ? L2(L"Jogo não instalado", L"Game not installed") : st == S_NOCPU ? L2(L"Processador sem suporte", L"Processor not supported") : T(st); }
/* 4.0.4: the PS5 look. The main action (kind 1) is white with dark text; the others are dark glass; the one under the
   mouse or the focus gets a white edge. The text fits the button (FitBtnFont). */
/* 4.3: the main action (kind 1) is the neon gradient with white text and a glow under the mouse or the focus; the others
   stay dark glass, with a neon edge when hot */
#define BTN_FILL RGB0(245, 245, 247)   /* 7.0: the main action is white with dark words in every game (6.2: the game's color); its glow keeps the game's color */
#define BTN_INK RGB0(14, 14, 18)
#define HP_GLASS A(RGB0(34, 36, 44), 182)          /* Released: the home's main button at rest: dark glass a shade lighter than "..." and the second buttons, with a clearer edge */
#define HP_GLASS_EDGE A(RGB0(255, 255, 255), 84)
static void BtnBox(GpGraphics *g, RECT r, int rad, int kind, BOOL hot)
{
    /* 6.0 (the user: "exactly like the PS5's interface"): the main action is white with dark words, as on the console;
       the others are smoked glass; the one under the mouse or the focus gets a white edge and a soft light */
    if (hot) Glow(g, (r.left + r.right) / 2, (r.top + r.bottom) / 2 + SC(2), (r.right - r.left) * 6 / 10, (r.bottom - r.top), A(kind == 1 ? SkinAcc() : RGB(255, 255, 255), kind == 1 ? 44 : 26));
    if (kind == 1) ShapeBox(g, r, rad, A(hot ? RGB0(255, 255, 255) : BTN_FILL, 255), hot ? A(RGB0(255, 255, 255), 255) : 0, 2.0f);
    else if (kind == 2) ShapeBox(g, r, rad, A(RGB(255, 255, 255), hot ? 30 : 8), A(RGB(255, 255, 255), hot ? 230 : 50), hot ? 2.0f : 1.0f);
    else ShapeBox(g, r, rad, A(hot ? RGB(54, 54, 62) : RGB(20, 20, 26), 226), A(RGB(255, 255, 255), hot ? 230 : 34), hot ? 2.0f : 1.0f);
}
static COLORREF BtnInk(int kind, BOOL hot) { return kind == 1 ? BTN_INK : hot ? RGB(255, 255, 255) : C_TEXT; }   /* 6.0: dark words on the white button */
static void Button(HDC dc, GpGraphics *g, RECT r, const wchar_t *t, int id, int kind)   /* kind 0 normal, 1 primary, 2 ghost */
{
    BOOL hot = g_hot == id; BtnBox(g, r, SC(12), kind, hot);
    int sp; HFONT f = FitBtnFont(dc, t, r.right - r.left - SC(20), &sp);
    SetTextCharacterExtra(dc, Sp(sp)); TextR(dc, f, BtnInk(kind, hot), r, t, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS); SetTextCharacterExtra(dc, 0);
    AddHit(r.left, r.top, r.right, r.bottom, id);
}
/* a button with an icon on its left: icon 0 folder, 1 save, 2 search, 3 check, 4 download, 5 plus, 6 gear, 7 link, 8 copy, 9 bolt */
static void IconButton(HDC dc, GpGraphics *g, RECT r, const wchar_t *t, int id, int kind, int icon)
{
    BOOL hot = g_hot == id; BtnBox(g, r, SC(12), kind, hot);
    int is = SC(16), x = r.left + SC(16), y = (r.top + r.bottom - is) / 2; UINT32 ic = A(kind == 1 ? BTN_INK : hot ? RGB(255, 255, 255) : C_SUB, 255);
    switch (icon) { case 0: IconFolder(g, x, y, is, ic); break; case 1: IconSave(g, x, y, is, ic); break; case 2: IconSearch(g, x, y, is, ic); break; case 3: IconCheck(g, x, y, is, ic); break;
                    case 4: IconDownload(g, x, y, is, ic); break; case 5: IconPlus(g, x, y, is, ic); break; case 6: IconGear(g, x, y, is, ic); break; case 7: IconLink(g, x, y, is, ic); break;
                    case 8: IconCopy(g, x, y, is, ic); break; default: IconBolt(g, x, y, is, ic); }
    RECT tr = {x + is + SC(10), r.top, r.right - SC(10), r.bottom};
    int sp; HFONT f = FitBtnFont(dc, t, tr.right - tr.left, &sp); (void)sp;
    TextR(dc, f, BtnInk(kind, hot), tr, t, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    AddHit(r.left, r.top, r.right, r.bottom, id);
}
/* JOGAR / PARAR. 4.0.4: like the PS5's Play, with a soft light around it while it can be pressed; red while the game
   runs (STOP). 4.3: the neon gradient with white text, its light breathing in the logo's colors */
static void PlayButton(HDC dc, GpGraphics *g, RECT r, const wchar_t *label)
{
    int st = St(g_sel); BOOL hot = g_hot == H_PLAY, can = st == S_READY || g_proc;
    int rad = SC(12);
    BOOL glass = HOME_PAGES && g_view == V_HOME && !hot;   /* Released: on the home screen PLAY is dark glass, as the console's; white under the mouse or the focus */
    /* 6.0 (the user: "exactly like the PS5"): the console's Play: a white button with dark words, a white edge and a soft
       light under the mouse or the focus; red while the game runs (STOP) */
    if (can) {
        int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, w = r.right - r.left, h = r.bottom - r.top;
        if (hot || (g_skin && !g_proc && !glass)) Glow(g, cx, cy + SC(4), w * 7 / 10, h + SC(8), A(SkinAcc(), hot ? 92 : 52));
        if (g_proc) ShapeBox(g, r, rad, A(RGB(226, 56, 72), hot ? 255 : 242), hot ? A(RGB(255, 255, 255), 230) : 0, 2.0f);
        else if (glass) ShapeBox(g, r, rad, HP_GLASS, HP_GLASS_EDGE, 1.0f);
        else {
            ShapeBox(g, r, rad, A(hot ? RGB0(255, 255, 255) : BTN_FILL, 255), hot ? A(RGB0(255, 255, 255), 255) : 0, 2.0f);
            float sh = MotionT(g_sheenT0, 520);   /* a soft light that crosses it once when the mouse or the focus arrives */
            if (sh < 1.0f) { GpPath *bp = SkinPath(r, rad); CLIPPATH(g, bp);
                int sx = r.left - w / 3 + (int)((w + w / 3) * EaseOut3(sh)); GpPath *band = NULL; GdipCreatePath(0, &band);
                GpPointF q[4] = {{(float)sx, (float)r.top}, {(float)(sx + SC(46)), (float)r.top}, {(float)(sx + SC(46) - h / 2), (float)r.bottom}, {(float)(sx - h / 2), (float)r.bottom}};
                GdipAddPathPolygon(band, q, 4); GpBrush *wb = NULL; GdipCreateSolidFill(A(RGB(190, 196, 210), (BYTE)(110 * (1.0f - sh))), &wb);
                if (wb) { GdipFillPath(g, wb, band); GdipDeleteBrush(wb); } GdipDeletePath(band); ClipBase(g); GdipDeletePath(bp); }
        }
    } else { ShapeBox(g, r, rad, A(RGB(255, 255, 255), 16), A(RGB(255, 255, 255), 50), 1.0f); }
    wchar_t sc[48]; const wchar_t *t = Sentence(g_proc ? T(S_STOP) : label ? label : T(S_PLAY), sc, 48);
    int tw = TextW(dc, g_fPlay, t), ic = SC(16), gap = SC(12);
    int x0 = (r.left + r.right - (ic + gap + tw)) / 2, cy = (r.top + r.bottom) / 2;
    COLORREF tc = !can ? C_DIM : g_proc || glass ? RGB(255, 255, 255) : BTN_INK;
    if (g_proc) { RECT sq = {x0 + SC(1), cy - SC(7), x0 + SC(15), cy + SC(7)}; Box(g, sq, SC(3), A(tc, 255), 0, 0); }
    else PlayTriangle(g, x0 + SC(1), cy, SC(18), A(tc, 255));
    RECT tr = {x0 + ic + gap, r.top, r.right, r.bottom}; TextR(dc, g_fPlay, tc, tr, t, DT_LEFT | DT_VCENTER);
    AddHit(r.left, r.top, r.right, r.bottom, H_PLAY);
}
static int StatusPill(HDC dc, GpGraphics *g, int x, int top, int st)   /* returns its width */
{
    const wchar_t *s = StatusText(st); COLORREF c = StatusColor(st);
    int pw = TextW(dc, g_fSmall, s) + SC(44); RECT p = {x, top, x + pw, top + SC(28)};   /* 5.0: a hexagon tag */
    Box(g, p, SC(14), A(RGB(8, 6, 16), 186), A(c, 190), 1.0f); Box(g, p, SC(14), A(c, 26), 0, 0);
    { GpPath *o = OctagonPath(p.left + SC(19), (p.top + p.bottom) / 2, SC(4)); GpBrush *b = NULL; GdipCreateSolidFill(A(c, 255), &b); if (b) { GdipFillPath(g, b, o); GdipDeleteBrush(b); } GdipDeletePath(o); }
    RECT t = {p.left + SC(30), p.top, p.right - SC(8), p.bottom}; TextR(dc, g_fSmall, c, t, s, DT_LEFT | DT_VCENTER);
    return pw;
}
static void Section(HDC dc, GpGraphics *g, int x, int y, const wchar_t *t)
{
    { GpPath *o = OctagonPath(x + SC(4), y + SC(8), SC(4)); GpBrush *b = NULL; GdipCreateSolidFill(A(C_CYAN, 255), &b); if (b) { GdipFillPath(g, b, o); GdipDeleteBrush(b); } GdipDeletePath(o); }   /* 5.0: an octagon mark */
    Spaced(dc, g_fSection, C_SUB, x + SC(16), y, t, 2);
}
static void Panel(GpGraphics *g, RECT r) { GlassPanel(g, r, SkinRad(SC(18)), 176); }   /* 5.0: frosted glass */
static void FmtPlayTime(int g, wchar_t *out, int n)
{
    wchar_t k[48]; _snwprintf(k, 48, L"playtime_%s", G[g].id); int secs = PIni(k, 0);
    _snwprintf(k, 48, L"lastplay_%s", G[g].id); int last = PIni(k, 0);
    if (secs < 60) { _snwprintf(out, n, L"%s", L2(L"Ainda não jogado pelo PORTHEON", L"Not played through PORTHEON yet")); return; }
    wchar_t t[48]; int h = secs / 3600, m = (secs % 3600) / 60;
    if (h) _snwprintf(t, 48, L"%d h %02d min", h, m); else _snwprintf(t, 48, L"%d min", m);
    FILETIME ft; GetSystemTimeAsFileTime(&ft); ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
    int now = (int)(u.QuadPart / 10000000ull - 11644473600ull), days = last ? (now - last) / 86400 : -1;
    const wchar_t *when = days < 0 ? L"" : days == 0 ? L2(L"hoje", L"today") : days == 1 ? L2(L"ontem", L"yesterday") : NULL;
    wchar_t wd[32]; if (!when) { _snwprintf(wd, 32, L2(L"há %d dias", L"%d days ago"), days); when = wd; }
    _snwprintf(out, n, L2(L"%s jogados  ·  última vez: %s", L"%s played  ·  last time: %s"), t, when);
    out[n - 1] = 0;
}
/* 6.0 (the user: "exactly like the PS5"): the profile pictures are round, as on the console; the chosen one gets a
   white ring with a soft light */
static GpPath *CirclePath(int cx, int cy, int r) { GpPath *p = NULL; GdipCreatePath(0, &p); if (p) GdipAddPathEllipseI(p, cx - r, cy - r, 2 * r, 2 * r); return p; }
static void AvatarRing(GpGraphics *g, int cx, int cy, int r)
{
    int R = r + SC(6);
    if (!g_fxLight) Ring(g, cx, cy, R, A(RGB(255, 255, 255), 46), (float)SC(7));
    Ring(g, cx, cy, R, A(RGB(255, 255, 255), 250), (float)SC(3));
}
/* a profile's picture: a disc in its color with one of the symbols, or its initial */
static void Avatar(HDC dc, GpGraphics *g, int cx, int cy, int r, const Profile *p, BOOL ring)
{
    COLORREF c = PROF_COLORS[p->color % 12];
    RECT d = {cx - r, cy - r, cx + r, cy + r};
    GpImage *ph = PhotoGet(p);
    if (ph) {   /* 4.4.0: the player's photo, round, with a thin light edge */
        GpPath *cp = CirclePath(cx, cy, r);   /* 6.0: round, as on the console */
        CLIPPATH(g, cp); GdipDrawImageRectI(g, ph, d.left, d.top, 2 * r, 2 * r); ClipBase(g); GdipDeletePath(cp);
        Ring(g, cx, cy, r, A(RGB(255, 255, 255), 52), 1.2f);
        if (ring) AvatarRing(g, cx, cy, r);
        return;
    }
    POINT a = {d.left, d.top}, z = {d.right, d.bottom}; GpBrush *lb = NULL;
    GdipCreateLineBrushI(&a, &z, A(Mix(c, RGB(255, 255, 255), 0.18f), 255), A(Mix(c, RGB(0, 0, 0), 0.45f), 255), 0, &lb);
    GpPath *pp = CirclePath(cx, cy, r); if (lb) GdipFillPath(g, lb, pp); GdipDeletePath(pp); if (lb) GdipDeleteBrush(lb);
    UINT32 w = 0xF0FFFFFF; int s = r, x = cx - s / 2, y = cy - s / 2;
    switch (p->avatar % NAVATARS) {
    case 1: { RECT b = {x - s / 6, y + s / 3, x + s + s / 6, y + s * 2 / 3}; Box(g, b, s / 8, w, 0, 0);
              RECT m = {cx - s / 5, y + s / 4, cx + s / 5, y + s * 3 / 4}; Box(g, m, s / 10, A(Mix(c, RGB(255, 215, 0), 0.65f), 255), 0, 0); break; }   /* a belt */
    case 2: { GpPath *o = NULL; GdipCreatePath(0, &o);   /* the octagon */
              for (int k = 0; k < 8; k++) { float a1 = k * 0.785398f + 0.3927f, a2 = a1 + 0.785398f; GdipAddPathLineI(o, cx + (int)(cosf(a1) * s * 0.58f), cy + (int)(sinf(a1) * s * 0.58f), cx + (int)(cosf(a2) * s * 0.58f), cy + (int)(sinf(a2) * s * 0.58f)); }
              GdipClosePathFigure(o); GpPen *pn = NULL; GdipCreatePen1(w, PW(s, 7), 2, &pn); GdipDrawPath(g, pn, o); GdipDeletePen(pn); GdipDeletePath(o); break; }
    case 3: { RECT f = {x, y + s / 6, x + s, y + s * 5 / 6}; Box(g, f, s / 4, w, 0, 0);   /* a glove */
              for (int k = 0; k < 3; k++) Line(g, x + s * (k + 1) / 4, y + s / 6, x + s * (k + 1) / 4, y + s / 2, A(Mix(c, RGB(0, 0, 0), 0.3f), 255), PW(s, 10)); break; }
    case 4: { GpPath *st = NULL; GdipCreatePath(0, &st);   /* a star */
              for (int k = 0; k < 10; k++) { float a1 = -1.5708f + k * 0.6283f, r1 = k % 2 ? s * 0.26f : s * 0.62f, a2 = a1 + 0.6283f, r2 = (k + 1) % 2 ? s * 0.26f : s * 0.62f;
                                             GdipAddPathLineI(st, cx + (int)(cosf(a1) * r1), cy + (int)(sinf(a1) * r1), cx + (int)(cosf(a2) * r2), cy + (int)(sinf(a2) * r2)); }
              GdipClosePathFigure(st); GpBrush *b = NULL; GdipCreateSolidFill(w, &b); GdipFillPath(g, b, st); GdipDeleteBrush(b); GdipDeletePath(st); break; }
    case 5: IconBolt(g, x, y, s, w); break;
    case 6: { GpPath *cr = NULL; GdipCreatePath(0, &cr);   /* a crown */
              GdipAddPathLineI(cr, x, y + s * 3 / 4, x, y + s / 4); GdipAddPathLineI(cr, x, y + s / 4, x + s / 4, y + s / 2); GdipAddPathLineI(cr, x + s / 4, y + s / 2, cx, y + s / 6);
              GdipAddPathLineI(cr, cx, y + s / 6, x + s * 3 / 4, y + s / 2); GdipAddPathLineI(cr, x + s * 3 / 4, y + s / 2, x + s, y + s / 4); GdipAddPathLineI(cr, x + s, y + s / 4, x + s, y + s * 3 / 4); GdipClosePathFigure(cr);
              GpBrush *b = NULL; GdipCreateSolidFill(w, &b); GdipFillPath(g, b, cr); GdipDeleteBrush(b); GdipDeletePath(cr); break; }
    case 7: IconPad(g, x - s / 8, y, s + s / 4, w); break;
    default: {   /* the initial */
        wchar_t ini[2] = {p->name[0] ? (wchar_t)towupper(p->name[0]) : L'P', 0};
        HFONT f = CreateFontW(-(r * 11 / 10), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, FaceHead());
        TextR(dc, f, RGB(255, 255, 255), d, ini, DT_CENTER | DT_VCENTER); DeleteObject(f);
    }
    }
    if (ring) AvatarRing(g, cx, cy, r);
}
/* 4.4.0: the status dot on a profile's picture, as on a console: green online, red with a bar busy, a grey ring offline */
static void StatusBadge(GpGraphics *g, int cx, int cy, int r, int st)
{
    Dot(g, cx, cy, r + SC(3), A(RGB(10, 8, 18), 255));
    COLORREF c = ProfStatusColor(st);
    if (st == PST_OFFLINE) { Dot(g, cx, cy, r, A(c, 255)); Dot(g, cx, cy, r * 5 / 10, A(RGB(10, 8, 18), 255)); return; }
    Dot(g, cx, cy, r, A(c, 255));
    if (st == PST_BUSY) Line(g, cx - r / 2, cy, cx + r / 2, cy, A(RGB(255, 255, 255), 255), (float)(r / 3 > 1 ? r / 3 : 1));
}
/* the picture with its status dot at the lower right */
static void AvatarSt(HDC dc, GpGraphics *g, int cx, int cy, int r, const Profile *p, BOOL ring)
{
    Avatar(dc, g, cx, cy, r, p, ring);
    int br = r * 22 / 100; if (br < SC(5)) br = SC(5);
    StatusBadge(g, cx + r * 70 / 100, cy + r * 70 / 100, br, p->status);
}

#include "ui_items.c"
#include "home62.c"      /* 6.2: the vitrine, the live cards, the chips and quick options */
#include "yt.c"          /* 6.2: YouTube's embed player over the vitrine (WebView2) */
#include "ui_screens.c"
#include "ui_extra62.c"   /* 6.2: the Help center and the Achievements */
#include "ui_padmap62.c"  /* 6.2: the Controller screen */
#include "ui_keys.c"      /* 6.0: the keyboard screen */
#include "ui_cinput.c"    /* 6.2: the keyboard of the games the player added */
#include "ui_library.c"   /* 6.1: the player's own games: tiles, the Add screen, the game page */
#include "ui_input.c"
