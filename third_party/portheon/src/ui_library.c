/* ui_library.c - part of ui.c (6.1, "Adicionar jogos"): the player's own games on the home row (library.c holds the
 * model). Their tiles after the built-in ones, then "+ ADICIONAR", then INSTALAR; the shown custom game's name, PLAY
 * and "..." in the console's look; the Add screen (OV_LIBADD: Steam, Epic, emulator + game, program or shortcut, and
 * the non-Steam shortcuts) and a simple page per game (OV_CGAME: Play, Open folder, Change cover / background, Move,
 * Edit options, Remove). Every control is a hit rect, so the controller and the keyboard reach all of it (FocusMove). */

/* ---------------------------------------------------------------- pictures */
static float g_cScale[MAXCUSTOM], g_addScale;   /* the tiles' growth (Animate -> LibAnimate) */
static int g_cTileArt[MAXCUSTOM];               /* what the cached tile shows: 0 colors only, 1 a cover, 2 the program's icon */
/* a color of its own for a game without art, from its name (through RGB, so PORTHEON's mono color turns it grey) */
static COLORREF LibHue(const wchar_t *s, float v)
{
    unsigned h = 2166136261u; for (; *s; s++) h = (h ^ (unsigned)towlower(*s)) * 16777619u;
    float hue = (float)(h % 360) / 60.0f, f = hue - (int)hue, r = 0, gg = 0, b = 0;
    switch ((int)hue % 6) { case 0: r = 1; gg = f; break; case 1: r = 1 - f; gg = 1; break; case 2: gg = 1; b = f; break; case 3: gg = 1 - f; b = 1; break; case 4: r = f; b = 1; break; default: r = 1; b = 1 - f; }
    r = v * (0.30f + 0.70f * r); gg = v * (0.30f + 0.70f * gg); b = v * (0.30f + 0.70f * b);
    return RGB((int)(255 * r), (int)(255 * gg), (int)(255 * b));
}
static GpBitmap *CTileBmp(int i, int s)
{
    if (g_cTileS[i] != s) { LibBmpFree(&g_cTile[i]); g_cTileS[i] = s; }
    if (g_cTile[i]) return g_cTile[i];
    CGame *c = &g_cg[i]; wchar_t p[MAX_PATH];
    if (!LibArtFind(c->id, L"icon", p) || !LibArtFind(c->id, L"pic", p)) LibArtRetry(i);   /* 6.1: Steam's late pictures, a dump's own art */
    GpImage *cov = LibArtFind(c->id, L"icon", p) ? LoadImageFile(p) : NULL, *ico = NULL;
    if (!cov && LibArtFind(c->id, L"exe", p)) ico = LoadImageFile(p);
    g_cTileArt[i] = cov ? 1 : ico ? 2 : 0;
    GpGraphics *g = NULL; GpBitmap *bmp = NewBitmap(s, s, &g);
    if (bmp && g) {
        GpPath *rp = RoundPath(0, 0, s - 1, s - 1, TILE_RAD(s)); GdipSetClipPath(g, rp, 0);
        if (cov) DrawCover(g, cov, 0, 0, s, s);   /* a 600x900 cover: its upper part (DrawCover keeps the top third) */
        else {
            POINT a = {0, 0}, z = {s, s}; GpBrush *lb = NULL; GdipCreateLineBrushI(&a, &z, A(LibHue(c->name, 0.80f), 255), A(LibHue(c->name, 0.20f), 255), 0, &lb);
            if (lb) { GdipFillRectangleI(g, lb, 0, 0, s, s); GdipDeleteBrush(lb); }
            GpPen *pen = NULL; GdipCreatePen1(A(RGB(255, 255, 255), 14), (float)(s / 14 > 1 ? s / 14 : 1), 2, &pen);
            if (pen) { for (int k = -s; k < s; k += s / 5 > 0 ? s / 5 : 1) GdipDrawLineI(g, pen, k, s, k + s, 0); GdipDeletePen(pen); }
            if (ico) { int is = s * 46 / 100; GdipSetInterpolationMode(g, 7); GdipDrawImageRectI(g, ico, (s - is) / 2, s * 14 / 100, is, is); }
        }
        GdipDeletePath(rp); GdipDeleteGraphics(g);
    }
    if (cov) GdipDisposeImage(cov); if (ico) GdipDisposeImage(ico);   /* only the tile stays in memory */
    g_cTile[i] = bmp; return bmp;
}
static void LibTextBox(HDC dc, HFONT f, COLORREF c, RECT r, const wchar_t *s)   /* centred, up to the lines that fit */
{
    SelectObject(dc, f); SetTextColor(dc, c); GdiText(dc, TRUE);
    RECT m = r; DrawTextW(dc, s, -1, &m, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
    int hh = m.bottom - m.top; if (hh < r.bottom - r.top) { r.top += (r.bottom - r.top - hh) / 2; r.bottom = r.top + hh; }
    DrawTextW(dc, s, -1, &r, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);
    GdiText(dc, FALSE);
}
static void CTile(HDC dc, GpGraphics *g, int i, int x, int y, int s)
{
    GpBitmap *b = CTileBmp(i, SC(128) + SC(46)); if (b) GdipDrawImageRectI(g, (GpImage *)b, x, y, s, s);
    if (g_cTileArt[i] == 1) return;
    BOOL ico = g_cTileArt[i] == 2;
    HFONT f = CreateFontW(-(s * (ico ? 11 : 15) / 100), 0, 0, 0, FW_BLACK, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
    RECT tr = ico ? (RECT){x + s / 14, y + s * 64 / 100, x + s - s / 14, y + s * 94 / 100} : (RECT){x + s / 12, y + s / 10, x + s - s / 12, y + s - s / 10};
    LibTextBox(dc, f, RGB(255, 255, 255), tr, g_cg[i].name); DeleteObject(f);
}
/* the shown custom game's background: its own picture, else its cover blurred and darkened, else its colors; ONE
   cached full-window bitmap (about 8 MB at 1080p), never one per game */
static GpBitmap *CustomBg(int i, int w, int h)
{
    CGame *c = &g_cg[i];
    if (g_cBg && g_cBgId == c->id && g_cBgW == w && g_cBgH == h) return g_cBg;
    LibBmpFree(&g_cBg); g_cBgId = c->id; g_cBgW = w; g_cBgH = h;
    GpGraphics *g = NULL; GpBitmap *bmp = NewBitmap(w, h, &g); if (!bmp || !g) { if (g) GdipDeleteGraphics(g); return bmp; }
    Fill(g, 0, 0, w, h, A(C_BG, 255));
    wchar_t p[MAX_PATH]; GpImage *art = LibArtFind(c->id, L"pic", p) ? LoadImageFile(p) : NULL; BOOL blur = FALSE;
    if (!art && LibArtFind(c->id, L"icon", p)) { art = LoadImageFile(p); blur = TRUE; }
    if (art && !blur) DrawCover(g, art, 0, 0, w, h);
    else if (art) {   /* a cover blown up to the window would be soft and busy: drawn small, then back up (a blur), darker */
        GpGraphics *sg = NULL; GpBitmap *sm = NewBitmap(w / 24 + 1, h / 24 + 1, &sg);
        if (sm && sg) { DrawCover(sg, art, 0, 0, w / 24 + 1, h / 24 + 1); GdipDeleteGraphics(sg); GdipSetInterpolationMode(g, 7); GdipDrawImageRectI(g, (GpImage *)sm, -w / 20, -h / 20, w + w / 10, h + h / 10); }
        else if (sg) GdipDeleteGraphics(sg);
        LibBmpFree(&sm); Fill(g, 0, 0, w, h, A(RGB(0, 0, 0), 96));
    } else {
        POINT a = {0, 0}, z = {w, h}; GpBrush *lb = NULL; GdipCreateLineBrushI(&a, &z, A(LibHue(c->name, 0.20f), 255), A(Mix(LibHue(c->name, 0.55f), C_BG, 0.45f), 255), 0, &lb);
        if (lb) { GdipFillRectangleI(g, lb, 0, 0, w, h); GdipDeleteBrush(lb); }
        GlowRaw(g, w * 3 / 4, -h / 8, w * 7 / 10, h * 7 / 10, A(LibHue(c->name, 0.9f), 40));
    }
    if (art) GdipDisposeImage(art);
    VGrad(g, 0, 0, w, h * 34 / 100, A(RGB(0, 0, 0), 170), A(RGB(0, 0, 0), 0));   /* the same black gradients as GameBg: the words stay readable */
    VGrad(g, 0, h * 50 / 100, w, h - h * 50 / 100, A(RGB(0, 0, 0), 0), A(RGB(0, 0, 0), 226));
    HGrad(g, 0, 0, w * 64 / 100, h, A(RGB(0, 0, 0), 190), A(RGB(0, 0, 0), 0));
    Vignette(g, w, h);
    GdipDeleteGraphics(g); g_cBg = bmp; return bmp;
}

/* ---------------------------------------------------------------- the home row */
static BOOL LibAnimate(void)   /* Animate, every 33 ms: TRUE when a tile moved */
{
    BOOL moved = FALSE;
    for (int i = 0; i < g_ncg; i++) {
        float target = i == g_csel ? 1.0f : g_hot == H_CUSTOM + i ? 0.3f : 0.0f;
        if (fabsf(g_cScale[i] - target) > 0.005f) { g_cScale[i] += (target - g_cScale[i]) * g_easeK; moved = TRUE; } else if (g_cScale[i] != target) { g_cScale[i] = target; moved = TRUE; }
    }
    float at = g_hot == H_LIBADD ? 1.0f : 0.0f;
    if (fabsf(g_addScale - at) > 0.01f) { g_addScale += (at - g_addScale) * g_easeK; moved = TRUE; } else g_addScale = at;
    return moved;
}
/* PaintHome, after the built-in tiles: the custom tiles and "+ ADICIONAR"; returns where the next tile goes */
static int LibHomeTiles(HDC dc, GpGraphics *g, int x, int cy, int base, int grow, int gap)
{
    for (int i = 0; i < g_ncg; i++) {
        BOOL sel = i == g_csel; float t = g_cScale[i]; int sz = base + (int)(grow * t), y = TILE_TOP(cy, sz, base, grow);   /* Released: as the built-in tiles (ui_screens.c) */
        RECT r = {x, y, x + sz, y + sz};
        if (x >= SC(MX + 12) - SC(4) && Seen((RECT){x - SC(24), y - SC(24), x + sz + SC(24), y + sz + SC(24)})) {
            if (sel) Glow(g, x + sz / 2, y + sz / 2 + SC(6), sz * 7 / 10, sz * 7 / 10, A(RGB(0, 0, 0), 150));
            CTile(dc, g, i, x, y, sz);
            if (g_crun == g_cg[i].id) { int br = SC(7); Dot(g, x + sz - br - SC(6), y + br + SC(6), br + SC(2), A(RGB(0, 0, 0), 190)); Dot(g, x + sz - br - SC(6), y + br + SC(6), br, A(C_GOOD, 255)); }   /* running */
            if (sel) { float e = EaseOut3(MotionT(g_heroT0, 260)); RECT o = r; InflateRect(&o, SC(4), SC(4)); Box(g, o, TILE_RAD(sz) + SC(4), 0, A(RGB0(255, 255, 255), (BYTE)(140 + 115 * e)), (float)SC(2) + 0.5f); }   /* 7.0: as the built-in tiles: a white line a gap apart */
            else if (g_hot == H_CUSTOM + i) Box(g, r, TILE_RAD(sz), 0, A(RGB(255, 255, 255), 190), 1.5f);
        }
        AddHit(x, y, x + sz, y + sz, H_CUSTOM + i);
        x += sz + gap;
        if (sel && HOME_PAGES) {   /* Released: in small letters under the next tiles */
            float e = EaseOut3(MotionT(g_heroT0, 300));
            Text(dc, g_fText, Mix(RGB(0, 0, 0), RGB(255, 255, 255), e), x + SC(2), TILE_TOP(cy, sz, base, grow) + base + SC(13), g_cg[i].name);
        } else
        if (sel) {   /* its name beside it, as the built-in ones */
            float e = EaseOut3(MotionT(g_heroT0, 300)); int nw = TextW(dc, g_fCard, g_cg[i].name);
            Text(dc, g_fCard, Mix(RGB(0, 0, 0), RGB(255, 255, 255), e), x + SC(4), cy - SC(13), g_cg[i].name);
            x += nw + SC(30);
        }
    }
    if (g_ncg < MAXCUSTOM) {   /* "+ ADICIONAR": add a game of your own */
        int sz = base, y = TILE_TOP(cy, sz, base, grow); BOOL hot = g_hot == H_LIBADD; RECT r = {x, y, x + sz, y + sz};
        if (x >= SC(MX + 12) - SC(4) && Seen(r)) {
            if (HOME_PAGES) Box(g, r, TILE_RAD(sz), A(RGB0(14, 15, 20), 150), 0, 0);   /* Released: dark glass under it, as "INSTALAR" */
            Box(g, r, TILE_RAD(sz), A(RGB(255, 255, 255), hot ? 46 : 20), A(RGB(255, 255, 255), hot ? 235 : 60), hot ? 2.0f : 1.0f);
            int is = sz * 30 / 100; IconPlus(g, x + (sz - is) / 2, y + sz * 24 / 100, is, A(RGB(255, 255, 255), 255));
            RECT lr = {x, y + sz * 60 / 100, x + sz, y + sz * 86 / 100}; TextR(dc, g_fTag, RGB(236, 236, 240), lr, L2(L"ADICIONAR", L"ADD GAME"), DT_CENTER | DT_VCENTER);
        }
        AddHit(x, y, x + sz, y + sz, H_LIBADD);
        x += sz + gap;
    }
    return x;
}
static void CSelect(int i)
{
    if (i < 0 || i >= g_ncg || i == g_csel) return;
    SaveEdits(); FeatureChanged(-1); g_csel = i; g_msg[0] = 0;
    PIniSet(L"selected_custom", g_cg[i].id);
    MusicFollow();   /* 6.1: its music only when a dump's snd0.at9 was copied for it (LibConsoleArt); else none */
    InvalidateRect(g_wnd, NULL, FALSE);
}
/* L1/R1 on the home screen: the built-in games in their order, then the custom ones */
static void LibHomeCycle(int d)
{
    HomeOrder(); int n = NGAMES + g_ncg, k = 0;
    if (g_csel >= 0) k = NGAMES + g_csel; else while (k < NGAMES && g_homeOrd[k] != g_sel) k++;
    k = ((k + d) % n + n) % n;
    if (k < NGAMES) { Select(g_homeOrd[k]); g_focus = H_CARD + g_sel; } else { CSelect(k - NGAMES); g_focus = H_CUSTOM + g_csel; }
    Snd(SND_MOVE);
}

/* ---------------------------------------------------------------- the shown custom game */
static void LibKindLine(const CGame *c, wchar_t *out, int n)
{
    const wchar_t *mine = L2(L"SEUS JOGOS", L"YOUR GAMES");
    switch (c->kind) {
    case CK_STEAM: _snwprintf(out, n, L"STEAM  ·  %s  ·  %s", c->gameid >> 32 ? L2(L"Atalho não-Steam", L"Non-Steam shortcut") : L2(L"Jogo da sua biblioteca", L"From your library"), mine); break;
    case CK_EPIC: _snwprintf(out, n, L"EPIC GAMES  ·  %s  ·  %s", L2(L"Jogo da sua biblioteca", L"From your library"), mine); break;
    case CK_EMU: _snwprintf(out, n, L"%s  ·  %s  ·  %s", EmuName(c->target), PathFindFileNameW(c->rom), mine); break;
    case CK_EXE: _snwprintf(out, n, L"%s  ·  %s  ·  %s", L2(L"PROGRAMA DO PC", L"PC PROGRAM"), PathFindFileNameW(c->target), mine); break;
    default: _snwprintf(out, n, L"%s  ·  %s  ·  %s", L2(L"ATALHO", L"SHORTCUT"), PathFindFileNameW(c->target), mine);
    }
    out[n - 1] = 0;
}
static void LibPlayed(const CGame *c, wchar_t *played, wchar_t *last, int n)
{
    int secs = (int)(c->playedMs / 1000);
    if (secs < 60) _snwprintf(played, n, L"%s", L2(L"Ainda não jogado pelo PORTHEON", L"Not played through PORTHEON yet"));
    else { int h = secs / 3600, m = (secs % 3600) / 60; if (h) _snwprintf(played, n, L2(L"%d h %02d min jogados", L"%d h %02d min played"), h, m); else _snwprintf(played, n, L2(L"%d min jogados", L"%d min played"), m); }
    if (!c->last) { _snwprintf(last, n, L"%s", L2(L"Nunca aberto pelo PORTHEON", L"Never started through PORTHEON")); return; }
    int days = (int)((LibNow() - c->last) / 86400);
    if (days <= 0) _snwprintf(last, n, L"%s", L2(L"Última vez: hoje", L"Last time: today"));
    else if (days == 1) _snwprintf(last, n, L"%s", L2(L"Última vez: ontem", L"Last time: yesterday"));
    else _snwprintf(last, n, L2(L"Última vez: há %d dias", L"Last time: %d days ago"), days);
    played[n - 1] = last[n - 1] = 0;
}
static void LibFolderOf(const CGame *c, wchar_t *out)   /* where the game is, for "Abrir pasta" and the card */
{
    out[0] = 0;
    if (c->watchDir[0] && DirExists(c->watchDir)) { wcscpy(out, c->watchDir); return; }
    if (c->kind == CK_EMU && c->rom[0]) { wcscpy(out, c->rom); if (!DirExists(out)) PathRemoveFileSpecW(out); return; }
    if (c->target[0]) { wcscpy(out, c->target); PathRemoveFileSpecW(out); }
}
static void LibOpenFolder(int i)
{
    if (i < 0 || i >= g_ncg) return;
    CGame *c = &g_cg[i]; const wchar_t *f = c->kind == CK_EMU && c->rom[0] ? c->rom : c->kind == CK_STEAM || c->kind == CK_EPIC ? NULL : c->target;
    if (f && f[0] && (FileExists(f) || DirExists(f))) { wchar_t sel[MAX_PATH + 16]; _snwprintf(sel, MAX_PATH + 16, L"/select,\"%s\"", f); sel[MAX_PATH + 15] = 0; ShellExecuteW(g_wnd, L"open", L"explorer.exe", sel, NULL, SW_SHOWNORMAL); return; }
    wchar_t d[MAX_PATH]; LibFolderOf(c, d);
    if (d[0] && DirExists(d)) ShellExecuteW(g_wnd, L"open", d, NULL, NULL, SW_SHOWNORMAL);
    else Msg(C_ORANGE, L2(L"O PORTHEON não sabe onde o %s está instalado: a loja dele abre o jogo direto.", L"PORTHEON does not know where %s is installed: its store starts it directly."), c->name);
}
/* a card at the bottom, as HomeCard but with the library's own ids (H_CCARD + k) */
static void LibCard(HDC dc, GpGraphics *g, RECT r, int k, int icon, const wchar_t *title, const wchar_t *l1, const wchar_t *l2)
{
    BOOL hot = g_hot == H_CCARD + k;
    r = HomeCardRect(r, k, 1);   /* Released: page 2's place (one over the other, left of the cover) */
    r.left += SC(7); r.right -= SC(7);
    if (Seen(r)) {
        if (hot) Glow(g, (r.left + r.right) / 2, (r.top + r.bottom) / 2, (r.right - r.left) * 6 / 10, (r.bottom - r.top), A(RGB(255, 255, 255), 30));
        Box(g, r, SC(16), A(RGB(0, 0, 0), hot ? 176 : 132), A(RGB(255, 255, 255), hot ? 220 : 30), hot ? 2.0f : 1.0f);
    }
    int is = SC(20), x = r.left + SC(20), y = r.top + SC(18); UINT32 ic = A(RGB(255, 255, 255), 255);   /* 7.0 fix: the built-in cards' own places (HomeCard): the title at 18, the lines at 30 and 50, where it leads at the foot */
    if (icon == 0) IconFolder(g, x, y, is, ic); else IconBolt(g, x, y, is, ic);
    RECT tt = {x + is + SC(12), y - SC(4), r.right - SC(16), y + is + SC(4)}; TextR(dc, g_fBtn, RGB(255, 255, 255), tt, title, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    RECT a = {x, y + SC(30), r.right - SC(16), y + SC(50)}; TextR(dc, g_fSmall, RGB(228, 228, 234), a, l1, DT_LEFT | DT_VCENTER | DT_PATH_ELLIPSIS);
    RECT b = {x, y + SC(50), r.right - SC(16), y + SC(70)}; TextR(dc, g_fSmall, RGB(150, 150, 162), b, l2, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    { RECT f = {x, r.bottom - SC(34), r.right - SC(16), r.bottom - SC(14)}; TextR(dc, g_fSmall, RGB(120, 120, 132), f, L2(L"Abrir  ›", L"Open  ›"), DT_LEFT | DT_VCENTER); }
    AddHit(r.left, r.top, r.right, r.bottom, H_CCARD + k);
}
/* PLAY for a custom game: white with the triangle; red STOP while a program or an emulator runs; "Aberto" (open) for a
   store game that runs (its store closes it) */
static void LibPlayBtn(HDC dc, GpGraphics *g, RECT r, int id, BOOL run, BOOL stop)
{
    BOOL hot = g_hot == id; int w = r.right - r.left, h = r.bottom - r.top, rad = h / 2;   /* 7.0: a pill */
    BOOL glass = HOME_PAGES && !run && !hot;   /* Released: dark glass at rest, white under the mouse or the focus (as PLAY, ui.c) */
    if (hot) Glow(g, (r.left + r.right) / 2, (r.top + r.bottom) / 2 + SC(2), w * 6 / 10, h, A(RGB(255, 255, 255), 50));
    if (run && stop) Box(g, r, rad, A(RGB(226, 56, 72), hot ? 255 : 242), hot ? A(RGB(255, 255, 255), 230) : 0, 2.0f);
    else if (run) Box(g, r, rad, A(RGB(255, 255, 255), hot ? 44 : 22), A(RGB(255, 255, 255), hot ? 230 : 60), hot ? 2.0f : 1.0f);
    else if (glass) Box(g, r, rad, HP_GLASS, HP_GLASS_EDGE, 1.0f);
    else Box(g, r, rad, A(RGB(255, 255, 255), hot ? 255 : 238), hot ? A(RGB(255, 255, 255), 255) : 0, 2.0f);
    wchar_t sc[48]; const wchar_t *t = Sentence(run ? (stop ? T(S_STOP) : L2(L"Aberto", L"Open")) : T(S_PLAY), sc, 48);
    int tw = TextW(dc, g_fPlay, t), ic = SC(16), gap = SC(12), x0 = (r.left + r.right - (ic + gap + tw)) / 2, cy = (r.top + r.bottom) / 2;
    COLORREF tc = run || glass ? RGB(255, 255, 255) : RGB(14, 14, 18);
    if (run && stop) { RECT sq = {x0 + SC(1), cy - SC(7), x0 + SC(15), cy + SC(7)}; Box(g, sq, SC(3), A(tc, 255), 0, 0); }
    else if (run) Dot(g, x0 + SC(8), cy, SC(6), A(C_GOOD, 255));
    else PlayTriangle(g, x0 + SC(1), cy, SC(18), A(tc, 255));
    RECT tr = {x0 + ic + gap, r.top, r.right, r.bottom}; TextR(dc, g_fPlay, tc, tr, t, DT_LEFT | DT_VCENTER);
    AddHit(r.left, r.top, r.right, r.bottom, id);
}
#if HOME_PAGES
/* Released (the two pages, ui.c HOME_PAGES): a custom game has no clip: its cover (or its colors and name) stands in a glass
   panel where the video is on the built-in games, on both pages (PaintHome draws it, still, after the page) */
static void LibHomeCover(HDC dc, GpGraphics *g)
{
    if (g_csel < 0 || g_csel >= g_ncg || VitMode() == 2) return;
    CGame *c = &g_cg[g_csel]; RECT V = VitRect(); int ts = SC(174), tcx = (V.left + V.right) / 2, tcy = (V.top + V.bottom) / 2;
    if (!Seen((RECT){V.left - SC(12), V.top - SC(12), V.right + SC(12), V.bottom + SC(24)})) return;
    for (int k2 = 3; k2 >= 0; k2--) { int k = SC(2) * k2; RECT s = {V.left - k, V.top + SC(5) - k, V.right + k, V.bottom + SC(5) + k}; Box(g, s, SC(VIT_RAD) + k, A(RGB0(0, 0, 0), 26), 0, 0); }
    Box(g, V, SC(VIT_RAD), A(RGB0(10, 10, 14), 150), A(RGB0(255, 255, 255), 34), 1.0f);
    Glow(g, tcx, tcy, ts, ts * 8 / 10, A(LibHue(c->name, 0.8f), 60));
    Glow(g, tcx, tcy + SC(10), ts * 7 / 10, ts * 7 / 10, A(RGB0(0, 0, 0), 150));
    CTile(dc, g, g_csel, tcx - ts / 2, tcy - ts / 2, ts);
    RECT o = {tcx - ts / 2, tcy - ts / 2, tcx + ts / 2, tcy + ts / 2}; Box(g, o, TILE_RAD(ts), 0, A(RGB0(255, 255, 255), 60), 1.0f);
}
/* a custom game's home, one page of it (g_hpDraw). Page 1: its name, one line, PLAY and "...". Page 2: the name small, its
   kind, the chips, what PLAY does, the two cards one over the other */
static void PaintHomeCustomP(HDC dc, GpGraphics *g)
{
    CGame *c = &g_cg[g_csel]; BOOL run = g_crun == c->id, stop = LibCanStop(c);
    float he = EaseOut3(MotionT(g_heroT0, 420)); int tx = SC(MX + 12) - (int)(SC(18) * (1.0f - he)), right = VitMode() == 2 ? SC(BASE_W - MX) : SC(VIT_X - 20);
    COLORREF wc = Mix(RGB(0, 0, 0), RGB(255, 255, 255), he), sc2 = Mix(RGB(0, 0, 0), RGB(200, 200, 210), he), tc2 = Mix(RGB(0, 0, 0), RGB(232, 232, 238), he);
    wchar_t pl[96], ls[96], what[300]; LibPlayed(c, pl, ls, 96);
    const wchar_t *ready = run ? T(S_RUNNING) : L2(L"Pronto para jogar", L"Ready to play");
    if (run) _snwprintf(what, 300, stop ? L2(L"%s está aberto. Parar fecha o jogo.", L"%s is open. Stop closes the game.") : L2(L"%s está aberto. Feche pelo próprio jogo.", L"%s is open. Close it from the game itself."), c->name);
    else if (c->kind == CK_STEAM) _snwprintf(what, 300, L2(L"Jogar abre o %s pela Steam.", L"Play opens %s through Steam."), c->name);
    else if (c->kind == CK_EPIC) _snwprintf(what, 300, L2(L"Jogar abre o %s pelo Epic Games Launcher.", L"Play opens %s through the Epic Games Launcher."), c->name);
    else if (c->kind == CK_EMU) _snwprintf(what, 300, L2(L"Jogar abre o %s no %s.", L"Play opens %s on %s."), c->name, EmuName(c->target));
    else if (c->kind == CK_EXE) _snwprintf(what, 300, L2(L"Jogar abre o %s direto, como um clique duplo nele.", L"Play opens %s directly, as a double-click on it."), c->name);
    else _snwprintf(what, 300, L2(L"Jogar abre o atalho do %s.", L"Play opens the %s shortcut."), c->name);
    what[299] = 0;
    if (g_hpDraw == 0) {
        HFONT big = CreateFontW(-SC(64), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Bahnschrift");
        RECT nr = {tx - SC(3), SC(HP_TITLE_Y) - SC(4), right, SC(HP_TITLE_Y) + SC(80)}; TextR(dc, big, wc, nr, c->name, DT_LEFT | DT_TOP | DT_END_ELLIPSIS); DeleteObject(big);
        wchar_t line[400]; if (run) _snwprintf(line, 400, L"%s", what); else if (c->playedMs >= 60000) _snwprintf(line, 400, L"%s  ·  %s", ready, pl); else _snwprintf(line, 400, L"%s", ready);
        line[399] = 0;
        RECT wr = {tx + SC(18), SC(HP_STAT_Y), right, SC(HP_STAT_Y + 24)}; Dot(g, tx + SC(5), (wr.top + wr.bottom) / 2 + SC(1), SC(4), A(C_GOOD, (BYTE)(255 * he)));
        TextR(dc, g_fText, tc2, wr, line, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        tx = SC(MX + 12); int by = SC(HP_BTN_Y), bh = SC(54);
        RECT b1 = {tx, by, tx + SC(232), by + bh}; LibPlayBtn(dc, g, b1, H_PLAY, run, stop);
        RECT b2 = {b1.right + SC(14), by, b1.right + SC(14) + bh, by + bh}; BOOL oh = g_hot == H_OPTIONS;
        int ocx = (b2.left + b2.right) / 2, ocy = (b2.top + b2.bottom) / 2;
        if (oh) Glow(g, ocx, ocy, SC(44), SC(44), A(RGB0(255, 255, 255), 30));
        Dot(g, ocx, ocy, bh / 2, A(RGB0(14, 15, 20), oh ? 196 : 158)); Ring(g, ocx, ocy, bh / 2 - 1, A(RGB0(255, 255, 255), oh ? 230 : 40), oh ? 2.0f : 1.0f);
        for (int q = -1; q <= 1; q++) Dot(g, ocx + q * SC(9), ocy, SC(3), A(RGB0(255, 255, 255), 255));
        AddHit(b2.left, b2.top, b2.right, b2.bottom, H_OPTIONS);   /* OpenGame opens the game's page (the hook in ui_input.c) */
        return;
    }
    int sy = SC(HP_CARDS_Y - 20 - 24), chipY = sy - SC(10 + 26);
    {   wchar_t lab[200]; LibKindLine(c, lab, 200); Text(dc, g_fSmall, sc2, tx, chipY - SC(30), lab);
        HFONT nf = CreateFontW(-SC(36), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Bahnschrift");
        RECT nr = {tx - SC(2), chipY - SC(30 + 48), right, chipY - SC(30 + 2)}; TextR(dc, nf, wc, nr, c->name, DT_LEFT | DT_TOP | DT_END_ELLIPSIS); DeleteObject(nf); }
    {   int cx = tx + StatusChip(dc, g, tx, chipY, C_GOOD, ready, TRUE) + SC(8); StatusChip(dc, g, cx, chipY, 0, pl, FALSE); }
    {   RECT wr = {tx, sy, right, sy + SC(24)}; TextR(dc, g_fText, tc2, wr, what, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS); }
    wchar_t where[MAX_PATH], wl2[120]; LibFolderOf(c, where);
    if (!where[0]) _snwprintf(where, MAX_PATH, L"%s", c->kind == CK_STEAM ? L2(L"Na biblioteca da Steam", L"In your Steam library") : c->kind == CK_EPIC ? L2(L"Na biblioteca da Epic", L"In your Epic library") : c->kind == CK_URL ? L2(L"Um link (atalho de internet)", L"A link (Internet shortcut)") : L2(L"Pasta não encontrada", L"Folder not found"));
    _snwprintf(wl2, 120, L"%s", L2(L"Abrir a pasta", L"Open the folder"));
    RECT r0 = {0, 0, 0, 0};   /* (LibCard places them: HomeCardRect) */
    LibCard(dc, g, r0, 0, 0, L2(L"Onde está", L"Where it is"), where, wl2);
    LibCard(dc, g, r0, 1, 1, L2(L"Tempo de jogo", L"Play time"), pl, ls);
}
#endif
/* PaintHome with a custom game shown: its name big, one line of facts, PLAY and "...", two cards */
static void PaintHomeCustom(HDC dc, GpGraphics *g)
{
    if (g_csel < 0 || g_csel >= g_ncg) return;
#if HOME_PAGES
    PaintHomeCustomP(dc, g); return;
#endif
    CGame *c = &g_cg[g_csel]; BOOL run = g_crun == c->id, stop = LibCanStop(c);
    float he = EaseOut3(MotionT(g_heroT0, 420)); int tx = SC(MX + 12) - (int)(SC(18) * (1.0f - he)), ty = SC(262);   /* 7.0: as the built-in games: the facts at 262, the name at 280, the chips at 358, PLAY at 400 */
    COLORREF wc = Mix(RGB(0, 0, 0), RGB(255, 255, 255), he), sc2 = Mix(RGB(0, 0, 0), RGB(200, 200, 210), he);
    wchar_t lab[200]; LibKindLine(c, lab, 200); Text(dc, g_fSmall, sc2, tx, ty, lab);
    HFONT big = CreateFontW(-SC(64), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Bahnschrift");
    RECT nr = {tx - SC(3), ty + SC(14), VitMode() == 2 ? SC(BASE_W - MX) : SC(VIT_X - 20), ty + SC(94)};   /* 6.2: the vitrine is on the right */ TextR(dc, big, wc, nr, c->name, DT_LEFT | DT_TOP | DT_END_ELLIPSIS); DeleteObject(big);
    wchar_t pl[96], ls[96], info[260]; LibPlayed(c, pl, ls, 96);
    _snwprintf(info, 260, L"%s  ·  %s", run ? T(S_RUNNING) : L2(L"Pronto para jogar", L"Ready to play"), pl); info[259] = 0;
    (void)info;   /* 6.2: the same facts as chips, as the built-in games */
    {   int cx = tx + StatusChip(dc, g, tx, SC(HOME_CHIP_Y), C_GOOD, run ? T(S_RUNNING) : L2(L"Pronto para jogar", L"Ready to play"), TRUE) + SC(8);
        StatusChip(dc, g, cx, SC(HOME_CHIP_Y), 0, pl, FALSE); }
    {   /* what PLAY does now, in one plain sentence */
        wchar_t what[300];
        if (run) _snwprintf(what, 300, stop ? L2(L"%s está aberto. Parar fecha o jogo.", L"%s is open. Stop closes the game.") : L2(L"%s está aberto. Feche pelo próprio jogo.", L"%s is open. Close it from the game itself."), c->name);
        else if (c->kind == CK_STEAM) _snwprintf(what, 300, L2(L"Jogar abre o %s pela Steam.", L"Play opens %s through Steam."), c->name);
        else if (c->kind == CK_EPIC) _snwprintf(what, 300, L2(L"Jogar abre o %s pelo Epic Games Launcher.", L"Play opens %s through the Epic Games Launcher."), c->name);
        else if (c->kind == CK_EMU) _snwprintf(what, 300, L2(L"Jogar abre o %s no %s.", L"Play opens %s on %s."), c->name, EmuName(c->target));
        else if (c->kind == CK_EXE) _snwprintf(what, 300, L2(L"Jogar abre o %s direto, como um clique duplo nele.", L"Play opens %s directly, as a double-click on it."), c->name);
        else _snwprintf(what, 300, L2(L"Jogar abre o atalho do %s.", L"Play opens the %s shortcut."), c->name);
        what[299] = 0;
        RECT wr = {tx, SC(476), SC(VIT_X - 20), SC(500)}; TextR(dc, g_fText, Mix(RGB(0, 0, 0), RGB(232, 232, 238), he), wr, what, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    }
    tx = SC(MX + 12); int by = SC(400), bh = SC(60);   /* 7.0: pills 60 high, PLAY 248 wide, "..." a glass circle */
    RECT b1 = {tx, by, tx + SC(248), by + bh}; LibPlayBtn(dc, g, b1, H_PLAY, run, stop);
    RECT b2 = {b1.right + SC(14), by, b1.right + SC(14) + bh, by + bh}; BOOL oh = g_hot == H_OPTIONS;
    int ocx = (b2.left + b2.right) / 2, ocy = (b2.top + b2.bottom) / 2;
    if (oh) Glow(g, ocx, ocy, SC(44), SC(44), A(RGB0(255, 255, 255), 30));
    Dot(g, ocx, ocy, bh / 2, A(RGB0(14, 15, 20), oh ? 196 : 158)); Ring(g, ocx, ocy, bh / 2 - 1, A(RGB0(255, 255, 255), oh ? 230 : 40), oh ? 2.0f : 1.0f);
    for (int q = -1; q <= 1; q++) Dot(g, ocx + q * SC(9), ocy, SC(3), A(RGB0(255, 255, 255), 255));
    AddHit(b2.left, b2.top, b2.right, b2.bottom, H_OPTIONS);   /* OpenGame opens the game's page (the hook in ui_input.c) */
    /* two cards only: where it is, and the time played */
    if (VitMode() != 2) {   /* 7.0 fix: a custom game has no clip: its cover (or its colors and name) stands where the video is on the built-in games; a video would be drawn over it (VitHome comes after) */
        RECT V = VitRect(); int ts = SC(174), tcx = (V.left + V.right) / 2, tcy = (V.top + V.bottom) / 2;
        for (int k2 = 3; k2 >= 0; k2--) { int k = SC(2) * k2; RECT s = {V.left - k, V.top + SC(5) - k, V.right + k, V.bottom + SC(5) + k}; Box(g, s, SC(VIT_RAD) + k, A(RGB0(0, 0, 0), 26), 0, 0); }
        Box(g, V, SC(VIT_RAD), A(RGB0(10, 10, 14), 150), A(RGB0(255, 255, 255), 34), 1.0f);
        Glow(g, tcx, tcy, ts, ts * 8 / 10, A(LibHue(c->name, 0.8f), 60));
        Glow(g, tcx, tcy + SC(10), ts * 7 / 10, ts * 7 / 10, A(RGB0(0, 0, 0), 150));
        CTile(dc, g, g_csel, tcx - ts / 2, tcy - ts / 2, ts);
        RECT o = {tcx - ts / 2, tcy - ts / 2, tcx + ts / 2, tcy + ts / 2}; Box(g, o, TILE_RAD(ts), 0, A(RGB0(255, 255, 255), 60), 1.0f);
    }
    int cyTop = SC(TAPE_Y), x0 = SC(MX + 12) - SC(7), cw = 2 * ((SC(CW - 24) + SC(14) + 3) / 4);   /* 7.0 fix: the two cards share the whole row (they stood in its left half) */
    wchar_t where[MAX_PATH], wl2[120]; LibFolderOf(c, where);
    if (!where[0]) _snwprintf(where, MAX_PATH, L"%s", c->kind == CK_STEAM ? L2(L"Na biblioteca da Steam", L"In your Steam library") : c->kind == CK_EPIC ? L2(L"Na biblioteca da Epic", L"In your Epic library") : c->kind == CK_URL ? L2(L"Um link (atalho de internet)", L"A link (Internet shortcut)") : L2(L"Pasta não encontrada", L"Folder not found"));
    _snwprintf(wl2, 120, L"%s", L2(L"Abrir a pasta", L"Open the folder"));
    RECT r0 = {x0, cyTop, x0 + cw, cyTop + SC(TAPE_H)}; LibCard(dc, g, r0, 0, 0, L2(L"Onde está", L"Where it is"), where, wl2);
    RECT r1 = {r0.right, cyTop, r0.right + cw, cyTop + SC(TAPE_H)}; LibCard(dc, g, r1, 1, 1, L2(L"Tempo de jogo", L"Play time"), pl, ls);
}

/* ---------------------------------------------------------------- the Add screen (OV_LIBADD) */
static int g_lbStep, g_lbPage, g_lbSteamOk = -1;   /* step 0: the sources, 1: a store's list, 2: emulator + game */
static CGame g_lbEmu;                              /* the emulator game being put together */
static GpImage *g_lbThumb[8]; static unsigned g_lbThumbKey[8]; static int g_lbThumbPage = -1, g_lbThumbSrc = -1;
#define LB_ROWS 8
static void LibThumbsFree(void) { for (int k = 0; k < 8; k++) { if (g_lbThumb[k]) GdipDisposeImage(g_lbThumb[k]); g_lbThumb[k] = NULL; g_lbThumbKey[k] = 0; } g_lbThumbPage = -1; }
static void LibAddOpen(void)
{
    if (g_ncg >= MAXCUSTOM) { Msg(C_ORANGE, L"%s", L2(L"A biblioteca está cheia (32 jogos). Remova um para adicionar outro.", L"The library is full (32 games). Remove one to add another.")); Snd(SND_ERR); return; }
    wchar_t sp[MAX_PATH]; g_lbSteamOk = SteamFindPath(sp, MAX_PATH);
    g_lbStep = 0; g_lbPage = 0; LibThumbsFree(); g_overlay = OV_LIBADD; g_focus = -1; InvalidateRect(g_wnd, NULL, FALSE);
}
static void LibAddClose(void) { g_overlay = OV_NONE; LibThumbsFree(); g_focus = -1; InvalidateRect(g_wnd, NULL, FALSE); }
static BOOL LibAddBack(void)   /* Back() first: B / Esc in a list goes back to the sources */
{
    if (g_overlay != OV_LIBADD || g_lbStep == 0) return FALSE;
    g_lbStep = 0; g_focus = -1; LibThumbsFree(); Snd(SND_BACK); InvalidateRect(g_wnd, NULL, FALSE); return TRUE;
}
static void LibAddedMsg(int idx, int dup)
{
    if (idx == -1) { Msg(C_ORANGE, L"%s", L2(L"A biblioteca está cheia (32 jogos). Remova um para adicionar outro.", L"The library is full (32 games). Remove one to add another.")); Snd(SND_ERR); return; }
    if (idx == -2) { Msg(C_SUB, L2(L"O %s já está na biblioteca.", L"%s is in the library already."), dup >= 0 ? g_cg[dup].name : L"?"); if (dup >= 0) CSelect(dup); return; }
    Msg(C_GOOD, L2(L"Adicionado: %s. Ele está na fila da tela inicial.", L"Added: %s. It is on the home row."), g_cg[idx].name); Snd(SND_OK);
    CSelect(idx);
}
static BOOL LibPickFile(wchar_t *file, const wchar_t *filter, const wchar_t *title, const wchar_t *startDir, BOOL noDeref)
{
    OPENFILENAMEW of; memset(&of, 0, sizeof(of)); of.lStructSize = sizeof(of); of.hwndOwner = g_wnd; of.lpstrFile = file; of.nMaxFile = MAX_PATH;
    of.lpstrFilter = filter; of.lpstrTitle = title; of.lpstrInitialDir = startDir && startDir[0] ? startDir : NULL;
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (noDeref ? OFN_NODEREFERENCELINKS : 0);
    BOOL ok = GetOpenFileNameW(&of); SetFocus(g_wnd); return ok;
}
static void LibAddProgram(void)   /* "Programa ou atalho": one pick, then it is added straight away */
{
    wchar_t f[MAX_PATH] = L"", desk[MAX_PATH] = L""; PWSTR dk = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Desktop, 0, NULL, &dk)) && dk) { wcsncpy(desk, dk, MAX_PATH - 1); CoTaskMemFree(dk); }
    if (!LibPickFile(f, L"*.exe; *.lnk; *.url; *.bat\0*.exe;*.lnk;*.url;*.bat;*.cmd\0*.*\0*.*\0", L2(L"Escolha o jogo (programa ou atalho)", L"Choose the game (program or shortcut)"), desk, TRUE)) return;
    CGame c; wchar_t ic[MAX_PATH]; int ix;
    if (!LibFromFile(f, &c, ic, &ix)) { Msg(C_ORANGE, L"%s", L2(L"Esse arquivo não abre um jogo: escolha um .exe, um atalho .lnk / .url ou um .bat.", L"That file does not start a game: choose an .exe, an .lnk / .url shortcut or a .bat.")); Snd(SND_ERR); return; }
    int dup, idx = LibAdd(&c, &dup); if (idx >= 0) LibArtImport(idx, ic, ix);
    LibAddedMsg(idx, dup); if (idx >= 0) LibAddClose();
}
static void LibAddCand(int k)   /* a row of a store's list */
{
    if (k < 0 || k >= g_lbN) return;
    const LibCand *cd = &g_lbCand[k];
    if (LibHasCand(cd)) { Msg(C_SUB, L2(L"O %s já está na biblioteca.", L"%s is in the library already."), cd->name); return; }
    CGame c; LibFromCand(cd, &c); int dup, idx = LibAdd(&c, &dup);
    if (idx >= 0) LibArtImport(idx, cd->exe[0] ? cd->exe : NULL, 0);
    LibAddedMsg(idx, dup);   /* the list stays open: more can be added */
}
static void LibEmuPick(int what)   /* 0 the emulator, 1 the game file, 2 the game folder, 3 RetroArch's core */
{
    wchar_t f[MAX_PATH] = L"", start[MAX_PATH] = L"";
    if (what == 0) {
        GetEnvironmentVariableW(L"USERPROFILE", start, MAX_PATH);
        if (!LibPickFile(f, L"*.exe\0*.exe\0", L2(L"Escolha o emulador (.exe)", L"Choose the emulator (.exe)"), start, FALSE)) return;
        if (_wcsicmp(PathFindExtensionW(f), L".exe")) return;
        wcscpy(g_lbEmu.target, f); wcscpy(g_lbEmu.dir, f); PathRemoveFileSpecW(g_lbEmu.dir); wcscpy(g_lbEmu.watchDir, g_lbEmu.dir);
        wcsncpy(g_lbEmu.args, EmuFor(f)->args, 511); g_lbEmu.core[0] = 0;
        Snd(SND_OK); return;
    }
    if (what == 3) {
        Join(start, g_lbEmu.dir, L"cores");
        if (!LibPickFile(f, L"*_libretro.dll\0*_libretro.dll\0*.dll\0*.dll\0", L2(L"Escolha o core do RetroArch", L"Choose the RetroArch core"), start, FALSE)) return;
        wcscpy(g_lbEmu.core, f); Snd(SND_OK); return;
    }
    if (g_lbEmu.rom[0]) { wcscpy(start, g_lbEmu.rom); if (!DirExists(start)) PathRemoveFileSpecW(start); }
    if (what == 1) { if (!LibPickFile(f, L"*.*\0*.*\0", L2(L"Escolha o jogo (ROM, ISO, EBOOT.BIN...)", L"Choose the game (ROM, ISO, EBOOT.BIN...)"), start, FALSE)) return; }
    else { wcscpy(f, start); if (!PickFolder(f, L2(L"Escolha a pasta do jogo", L"Choose the game folder"))) { SetFocus(g_wnd); return; } SetFocus(g_wnd); }
    wcscpy(g_lbEmu.rom, f); LibRomName(f, g_lbEmu.name, 80); Snd(SND_OK);
}
static BOOL LibEmuReady(void) { return g_lbEmu.target[0] && g_lbEmu.rom[0] && (EmuFor(g_lbEmu.target)->romKind != 3 || g_lbEmu.core[0]); }
static void LibEmuAdd(void)
{
    if (!LibEmuReady()) { Msg(C_ORANGE, L"%s", L2(L"Escolha o emulador e o jogo primeiro.", L"Choose the emulator and the game first.")); Snd(SND_ERR); return; }
    wchar_t t[16]; if (!LibBuildArgs(&g_lbEmu, t, 16)) { Msg(C_BAD, L"%s", L2(L"O caminho do jogo tem aspas (\"): mude o nome da pasta ou do arquivo.", L"The game's path holds a quote (\"): rename the folder or the file.")); return; }
    g_lbEmu.kind = CK_EMU; int dup, idx = LibAdd(&g_lbEmu, &dup); if (idx >= 0) LibArtImport(idx, g_lbEmu.target, 0);
    LibAddedMsg(idx, dup); if (idx >= 0) LibAddClose();
}
static int LibPages(void) { return g_lbN > 0 ? (g_lbN + LB_ROWS - 1) / LB_ROWS : 1; }
static void LibOverlayPage(int d)   /* L1/R1 in a list: the next page */
{
    if (g_overlay != OV_LIBADD || g_lbStep != 1) return;
    int np = LibPages(), p = ((g_lbPage + d) % np + np) % np; if (p != g_lbPage) { g_lbPage = p; g_focus = -1; Snd(SND_MOVE); InvalidateRect(g_wnd, NULL, FALSE); }
}
static void LibAddClick(int h)
{
    if (h == H_LIBCLOSE) { LibAddClose(); Snd(SND_BACK); return; }
    if (h == H_LIBBACK) { LibAddBack(); return; }
    if (g_lbStep == 0 && h >= H_LIBSRC && h < H_LIBSRC + 5) {
        int s = h - H_LIBSRC; Snd(SND_OK);
        if (s == 0 || s == 1 || s == 4) {
            if ((s == 0 || s == 4) && !g_lbSteamOk) { Msg(C_ORANGE, L"%s", L2(L"A Steam não foi encontrada neste PC.", L"Steam was not found on this PC.")); Snd(SND_ERR); return; }
            g_lbStep = 1; g_lbPage = 0; g_focus = -1; LibThumbsFree(); LibScanStart(s == 0 ? 0 : s == 1 ? 1 : 2);
        } else if (s == 2) { memset(&g_lbEmu, 0, sizeof(g_lbEmu)); g_lbStep = 2; g_focus = -1; }
        else LibAddProgram();
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    if (g_lbStep == 1) {
        if (h == H_LIBPREV) LibOverlayPage(-1); else if (h == H_LIBNEXT) LibOverlayPage(1);
        else if (h >= H_LIBROW && h < H_LIBROW + LB_ROWS && !g_lbScanning) LibAddCand(g_lbPage * LB_ROWS + (h - H_LIBROW));
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    if (g_lbStep == 2) {
        if (h == H_LIBPICKEXE) LibEmuPick(0); else if (h == H_LIBPICKROM) LibEmuPick(1); else if (h == H_LIBPICKDIR) LibEmuPick(2);
        else if (h == H_LIBPICKCORE) LibEmuPick(3); else if (h == H_LIBOK) LibEmuAdd();
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
}
static void LibOverlayMsg(HDC dc, RECT c)   /* the last notification inside the card (the toast is hidden under overlays) */
{
    if (!g_msg[0] || GetTickCount() - g_msgAt > 8000) return;
    RECT m = {c.left + SC(34), c.bottom - SC(118), c.right - SC(34), c.bottom - SC(80)};
    Wrap(dc, g_fSmall, g_msgColor, m, g_msg, TRUE);
}
/* one big choice: an icon, a title and a line under it */
static void LibSrcRow(HDC dc, GpGraphics *g, RECT r, int id, int icon, const wchar_t *t, const wchar_t *sub, BOOL dim)
{
    BOOL hot = g_hot == id || g_focus == id;
    if (hot) Glow(g, (r.left + r.right) / 2, (r.top + r.bottom) / 2, (r.right - r.left) * 6 / 10, r.bottom - r.top, A(RGB(255, 255, 255), 22));
    Box(g, r, SC(14), A(RGB(255, 255, 255), hot ? 30 : 10), A(RGB(255, 255, 255), hot ? 220 : 36), hot ? 2.0f : 1.0f);
    int is = SC(26), x = r.left + SC(22), y = (r.top + r.bottom - is) / 2; UINT32 ic = A(dim ? C_DIM : RGB(255, 255, 255), 255);
    switch (icon) { case 0: IconGlobe(g, x, y, is, ic); break; case 1: IconDownload(g, x, y, is, ic); break; case 2: IconPad(g, x, y, is, ic); break; case 3: IconFolder(g, x, y, is, ic); break; default: IconLink(g, x, y, is, ic); }
    RECT tr = {x + is + SC(20), r.top + SC(10), r.right - SC(20), r.top + SC(40)}; TextR(dc, g_fBtn, dim ? C_DIM : RGB(255, 255, 255), tr, t, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    RECT sr = {tr.left, r.top + SC(40), r.right - SC(20), r.bottom - SC(8)}; TextR(dc, g_fSmall, dim ? C_DIM : C_SUB, sr, sub, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    AddHit(r.left, r.top, r.right, r.bottom, id);
}
static void LibThumbLoad(int k, unsigned appid)   /* a Steam cover for a list row, kept small (68x100) */
{
    if (g_lbThumbKey[k] == appid && g_lbThumb[k]) return;
    if (g_lbThumb[k]) { GdipDisposeImage(g_lbThumb[k]); g_lbThumb[k] = NULL; }
    g_lbThumbKey[k] = appid; wchar_t p[MAX_PATH]; if (!appid || !LibSteamArtFind(appid, 0, p)) return;
    GpImage *src = LoadImageFile(p); if (!src) return;
    GpGraphics *tg = NULL; GpBitmap *b = NewBitmap(68, 100, &tg);
    if (b && tg) { DrawCover(tg, src, 0, 0, 68, 100); GdipDeleteGraphics(tg); g_lbThumb[k] = (GpImage *)b; } else { if (tg) GdipDeleteGraphics(tg); LibBmpFree(&b); }
    GdipDisposeImage(src);
}
static void PaintLibAdd(HDC dc, GpGraphics *g, RECT rc)
{
#ifdef PORTHEON_TEST   /* UI shots: PORTHEON_TEST_LIBSRC=0/1/2 opens that list (the scan runs), 9 the emulator step */
    { static int once; wchar_t v[8]; if (!once && GetEnvironmentVariableW(L"PORTHEON_TEST_LIBSRC", v, 8)) { once = 1; int s = _wtoi(v);
          if (s == 9) { memset(&g_lbEmu, 0, sizeof(g_lbEmu)); wcscpy(g_lbEmu.target, L"C:\\Emu\\retroarch.exe"); wcscpy(g_lbEmu.dir, L"C:\\Emu"); wcscpy(g_lbEmu.args, EmuFor(g_lbEmu.target)->args);
                        wcscpy(g_lbEmu.rom, L"D:\\Roms\\Jogo (USA).sfc"); LibRomName(g_lbEmu.rom, g_lbEmu.name, 80); g_lbStep = 2; }
          else { g_lbStep = 1; g_lbPage = 0; LibScanStart(s); } } }
#endif
    Dim(g, rc);
    RECT c = Card(g, 210, 50, 860, 700);
    int L = c.left + SC(34), R = c.right - SC(34);
    if (g_lbStep == 0) {
        Text(dc, g_fH2, RGB(255, 255, 255), L, c.top + SC(22), L2(L"Adicionar jogo", L"Add a game"));
        RECT st = {L, c.top + SC(62), R - SC(50), c.top + SC(104)};
        Wrap(dc, g_fSmall, C_SUB, st, L2(L"Escolha de onde vem o jogo. Ele entra na fila da tela inicial, com capa e tempo de jogo, e abre pelo PORTHEON. Nada muda nos arquivos dele.",
                                         L"Choose where the game comes from. It joins the home row, with its cover and play time, and starts from PORTHEON. Nothing changes in its files."), TRUE);
        int y = c.top + SC(116), rh = SC(70), gap = SC(10);
        const wchar_t *steamSub = g_lbSteamOk ? L2(L"Os jogos instalados da sua conta Steam, com as capas da Steam", L"The installed games of your Steam account, with Steam's covers") : L2(L"Steam não encontrada neste PC", L"Steam was not found on this PC");
        RECT r0 = {L, y, R, y + rh}; LibSrcRow(dc, g, r0, H_LIBSRC + 0, 0, L"Steam", steamSub, !g_lbSteamOk); y += rh + gap;
        RECT r1 = {L, y, R, y + rh}; LibSrcRow(dc, g, r1, H_LIBSRC + 1, 1, L"Epic Games", L2(L"Os jogos instalados pelo Epic Games Launcher", L"The games installed through the Epic Games Launcher"), FALSE); y += rh + gap;
        RECT r2 = {L, y, R, y + rh}; LibSrcRow(dc, g, r2, H_LIBSRC + 2, 2, L2(L"Emulador + jogo (ROM / ISO)", L"Emulator + game (ROM / ISO)"), L"RPCS3, PCSX2, DuckStation, Dolphin, Xenia, shadPS4, RetroArch, Ryujinx, PPSSPP, Cemu...", FALSE); y += rh + gap;
        RECT r3 = {L, y, R, y + rh}; LibSrcRow(dc, g, r3, H_LIBSRC + 3, 3, L2(L"Programa ou atalho (.exe, .lnk, .url)", L"Program or shortcut (.exe, .lnk, .url)"), L2(L"Qualquer jogo do PC, um port nativo, um atalho da área de trabalho ou um .bat", L"Any PC game, a native port, a desktop shortcut or a .bat"), FALSE); y += rh + gap;
        RECT r4 = {L, y, R, y + rh}; LibSrcRow(dc, g, r4, H_LIBSRC + 4, 4, L2(L"Atalhos não-Steam", L"Non-Steam shortcuts"), g_lbSteamOk ? L2(L"Os jogos que você já pôs na Steam como \"jogo não-Steam\"", L"The games you already put in Steam as a \"non-Steam game\"") : L2(L"Steam não encontrada neste PC", L"Steam was not found on this PC"), !g_lbSteamOk);
        wchar_t cnt[120]; _snwprintf(cnt, 120, L2(L"%d de %d jogos na sua biblioteca", L"%d of %d games in your library"), g_ncg, MAXCUSTOM);
        LibOverlayMsg(dc, c);
        Text(dc, g_fSmall, C_DIM, L, c.bottom - SC(56), cnt);
        CloseX(g, c, H_LIBCLOSE);
        return;
    }
    if (g_lbStep == 1) {
        const wchar_t *tt = g_lbSrc == 0 ? L2(L"Steam: seus jogos instalados", L"Steam: your installed games") : g_lbSrc == 1 ? L2(L"Epic Games: seus jogos instalados", L"Epic Games: your installed games") : L2(L"Steam: seus atalhos não-Steam", L"Steam: your non-Steam shortcuts");
        Text(dc, g_fH2, RGB(255, 255, 255), L, c.top + SC(22), tt);
        int y = c.top + SC(78), rh = SC(58);
        if (g_lbScanning || g_lbN < 0) { Text(dc, g_fText, C_SUB, L, y + SC(10), L2(L"Procurando os jogos...", L"Looking for the games...")); }
        else if (g_lbN == 0) {
            RECT er = {L, y, R, y + SC(120)};
            Wrap(dc, g_fText, C_SUB, er, g_lbSrc == 0 ? L2(L"Nenhum jogo instalado foi encontrado nas bibliotecas da Steam (steamapps\\appmanifest_*.acf).", L"No installed game was found in Steam's libraries (steamapps\\appmanifest_*.acf).")
                                      : g_lbSrc == 1 ? L2(L"Nenhum jogo instalado foi encontrado no Epic Games Launcher (a pasta Manifests dele).", L"No installed game was found in the Epic Games Launcher (its Manifests folder).")
                                                     : L2(L"Nenhum atalho não-Steam foi encontrado na sua conta Steam.", L"No non-Steam shortcut was found in your Steam account."), TRUE);
        } else {
            if (g_lbPage >= LibPages()) g_lbPage = 0;
            if (g_lbThumbPage != g_lbPage || g_lbThumbSrc != g_lbSrc) { LibThumbsFree(); g_lbThumbPage = g_lbPage; g_lbThumbSrc = g_lbSrc; }
            for (int k = 0; k < LB_ROWS; k++) {
                int idx = g_lbPage * LB_ROWS + k; if (idx >= g_lbN) break;
                const LibCand *cd = &g_lbCand[idx]; BOOL has = LibHasCand(cd), hot = g_hot == H_LIBROW + k || g_focus == H_LIBROW + k;
                RECT r = {L, y, R, y + rh - SC(6)};
                if (hot) Box(g, r, SC(10), A(RGB(255, 255, 255), 22), A(RGB(255, 255, 255), 200), 1.5f); else Box(g, r, SC(10), A(RGB(255, 255, 255), 7), 0, 0);
                int tx = r.left + SC(14);
                if (cd->kind == CK_STEAM) {   /* the Steam cover, small */
                    LibThumbLoad(k, cd->appid); int th = r.bottom - r.top - SC(8), tw = th * 68 / 100;
                    if (g_lbThumb[k]) GdipDrawImageRectI(g, g_lbThumb[k], tx, r.top + SC(4), tw, th);
                    else { RECT pr = {tx, r.top + SC(4), tx + tw, r.top + SC(4) + th}; Box(g, pr, SC(4), A(LibHue(cd->name, 0.5f), 255), 0, 0); }
                    tx += tw + SC(14);
                }
                RECT nr = {tx, r.top + SC(4), R - SC(190), r.top + SC(28)}; TextR(dc, g_fText, RGB(255, 255, 255), nr, cd->name, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
                wchar_t sub[MAX_PATH + 40];
                if (cd->size > 0) _snwprintf(sub, MAX_PATH + 40, L"%.1f GB  ·  %s", cd->size / 1073741824.0, cd->dir); else _snwprintf(sub, MAX_PATH + 40, L"%s", cd->dir[0] ? cd->dir : cd->exe);
                sub[MAX_PATH + 39] = 0;
                RECT dr = {tx, r.top + SC(28), R - SC(190), r.bottom - SC(4)}; TextR(dc, g_fSmall, C_DIM, dr, sub, DT_LEFT | DT_VCENTER | DT_PATH_ELLIPSIS);
                RECT pr = {R - SC(170), r.top + SC(10), R - SC(14), r.bottom - SC(10)};
                if (has) TextR(dc, g_fSmall, C_GOOD, pr, L2(L"Na biblioteca", L"In the library"), DT_RIGHT | DT_VCENTER);
                else { Box(g, pr, SC(10), A(RGB(255, 255, 255), hot ? 240 : 26), A(RGB(255, 255, 255), hot ? 255 : 80), 1.0f); TextR(dc, g_fBtn, hot ? RGB(14, 14, 18) : RGB(255, 255, 255), pr, L2(L"Adicionar", L"Add"), DT_CENTER | DT_VCENTER); }
                AddHit(r.left, r.top, r.right, r.bottom, H_LIBROW + k);
                y += rh;
            }
        }
        LibOverlayMsg(dc, c);
        int fy = c.bottom - SC(70);
        RECT bk = {L, fy, L + SC(150), fy + SC(46)}; Button(dc, g, bk, L2(L"Voltar", L"Back"), H_LIBBACK, 2);
        if (g_lbN > LB_ROWS) {
            RECT nx = {R - SC(120), fy, R, fy + SC(46)}; Button(dc, g, nx, L2(L"Próxima", L"Next"), H_LIBNEXT, 0);
            RECT pv = {nx.left - SC(12) - SC(120), fy, nx.left - SC(12), fy + SC(46)}; Button(dc, g, pv, L2(L"Anterior", L"Previous"), H_LIBPREV, 0);
            wchar_t pg[64]; _snwprintf(pg, 64, L2(L"Página %d de %d  ·  L1 / R1", L"Page %d of %d  ·  L1 / R1"), g_lbPage + 1, LibPages());
            RECT pgr = {bk.right + SC(16), fy, pv.left - SC(16), fy + SC(46)}; TextR(dc, g_fSmall, C_DIM, pgr, pg, DT_CENTER | DT_VCENTER);
        }
        CloseX(g, c, H_LIBCLOSE);
        return;
    }
    /* step 2: emulator + game */
    Text(dc, g_fH2, RGB(255, 255, 255), L, c.top + SC(22), L2(L"Emulador + jogo", L"Emulator + game"));
    RECT st = {L, c.top + SC(62), R - SC(50), c.top + SC(104)};
    Wrap(dc, g_fSmall, C_SUB, st, L2(L"Escolha o programa do emulador e depois o arquivo (ou a pasta) do jogo. O PORTHEON abre o emulador já com o jogo.", L"Choose the emulator's program, then the game's file (or folder). PORTHEON starts the emulator with the game."), TRUE);
    const EmuPreset *ep = g_lbEmu.target[0] ? EmuFor(g_lbEmu.target) : NULL;
    int y = c.top + SC(120), bw = SC(230), bh = SC(46);
    Spaced(dc, g_fTag, C_CYAN, L, y, L2(L"1. EMULADOR", L"1. EMULATOR"), 2); y += SC(24);
    { RECT b = {L, y, L + bw, y + bh}; IconButton(dc, g, b, g_lbEmu.target[0] ? L2(L"Trocar o emulador", L"Change the emulator") : L2(L"Escolher o emulador", L"Choose the emulator"), H_LIBPICKEXE, g_lbEmu.target[0] ? 0 : 1, 0);
      wchar_t t[MAX_PATH + 80]; if (ep) _snwprintf(t, MAX_PATH + 80, L"%s  ·  %s", EmuName(g_lbEmu.target), g_lbEmu.target); else _snwprintf(t, MAX_PATH + 80, L"%s", L2(L"Nenhum escolhido (o .exe do emulador)", L"None chosen (the emulator's .exe)"));
      t[MAX_PATH + 79] = 0; RECT tr = {b.right + SC(16), y, R, y + bh}; TextR(dc, g_fSmall, ep ? C_TEXT : C_DIM, tr, t, DT_LEFT | DT_VCENTER | DT_PATH_ELLIPSIS); }
    y += bh + SC(22);
    Spaced(dc, g_fTag, C_CYAN, L, y, L2(L"2. JOGO", L"2. GAME"), 2); y += SC(24);
    { RECT b = {L, y, L + bw, y + bh}; IconButton(dc, g, b, L2(L"Escolher o arquivo", L"Choose the file"), H_LIBPICKROM, g_lbEmu.target[0] && !g_lbEmu.rom[0] ? 1 : 0, 2);
      RECT b2 = {b.right + SC(12), y, b.right + SC(12) + bw, y + bh}; IconButton(dc, g, b2, L2(L"Escolher a pasta", L"Choose the folder"), H_LIBPICKDIR, 0, 0);
      wchar_t t[MAX_PATH + 80]; if (g_lbEmu.rom[0]) _snwprintf(t, MAX_PATH + 80, L"%s  ·  %s", g_lbEmu.name, g_lbEmu.rom); else _snwprintf(t, MAX_PATH + 80, L"%s", ep && ep->romKind == 2 ? L2(L"O EBOOT.BIN (ou a pasta) do jogo", L"The game's EBOOT.BIN (or its folder)") : L2(L"A ROM, a ISO ou a pasta do jogo", L"The ROM, the ISO or the game's folder"));
      t[MAX_PATH + 79] = 0; RECT tr = {L, y + bh + SC(6), R, y + bh + SC(30)}; TextR(dc, g_fSmall, g_lbEmu.rom[0] ? C_TEXT : C_DIM, tr, t, DT_LEFT | DT_VCENTER | DT_PATH_ELLIPSIS); }
    y += bh + SC(44);
    if (ep && ep->romKind == 3) {
        Spaced(dc, g_fTag, C_CYAN, L, y, L2(L"3. CORE DO RETROARCH", L"3. RETROARCH CORE"), 2); y += SC(24);
        RECT b = {L, y, L + bw, y + bh}; IconButton(dc, g, b, L2(L"Escolher o core", L"Choose the core"), H_LIBPICKCORE, g_lbEmu.rom[0] && !g_lbEmu.core[0] ? 1 : 0, 2);
        RECT tr = {b.right + SC(16), y, R, y + bh}; TextR(dc, g_fSmall, g_lbEmu.core[0] ? C_TEXT : C_DIM, tr, g_lbEmu.core[0] ? g_lbEmu.core : L2(L"Um *_libretro.dll da pasta cores", L"A *_libretro.dll from the cores folder"), DT_LEFT | DT_VCENTER | DT_PATH_ELLIPSIS);
        y += bh + SC(22);
    }
    if (g_lbEmu.target[0]) {   /* the command line it will use, and where to change it */
        wchar_t cmd[2048]; if (!LibBuildCmd(&g_lbEmu, cmd, 2048)) _snwprintf(cmd, 2048, L"%s", L2(L"(um caminho tem aspas: não dá para montar o comando)", L"(a path holds a quote: the command cannot be built)"));
        RECT cr = {L, y, R, y + SC(44)}; Wrap(dc, g_fSmall, C_DIM, cr, cmd, TRUE); y += SC(48);
        RECT nr = {L, y, R, y + SC(44)};
        Wrap(dc, g_fSmall, C_DIM, nr, L2(L"As opções vêm do modelo deste emulador e não foram testadas com cada versão dele: se ele pedir outras, mude em ... > Editar opções (library.ini).", L"The options come from this emulator's template and were not tested with each of its versions: if it needs others, change them in ... > Edit options (library.ini)."), TRUE);
    }
    LibOverlayMsg(dc, c);
    int fy = c.bottom - SC(70);
    RECT bk = {L, fy, L + SC(150), fy + SC(46)}; Button(dc, g, bk, L2(L"Voltar", L"Back"), H_LIBBACK, 2);
    RECT ok = {R - SC(220), fy, R, fy + SC(46)}; Button(dc, g, ok, L2(L"Adicionar", L"Add"), H_LIBOK, LibEmuReady() ? 1 : 0);
    CloseX(g, c, H_LIBCLOSE);
}

/* ---------------------------------------------------------------- a custom game's page (OV_CGAME) */
static int g_cpIdx = -1;
static void CPageOpen(int i) { if (i < 0 || i >= g_ncg) return; g_cpIdx = i; g_overlay = OV_CGAME; g_focus = -1; Snd(SND_OK); InvalidateRect(g_wnd, NULL, FALSE); }
static void LibPickPicture(int i, BOOL bg)   /* "Trocar capa" / "Trocar fundo": the player's picture, copied to art\ */
{
    wchar_t f[MAX_PATH] = L"", start[MAX_PATH] = L""; PWSTR pics = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Pictures, 0, NULL, &pics)) && pics) { wcsncpy(start, pics, MAX_PATH - 1); CoTaskMemFree(pics); }
    if (!LibPickFile(f, L"PNG, JPG, BMP, GIF\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0", bg ? L2(L"Escolha o fundo do jogo", L"Choose the game's background") : L2(L"Escolha a capa do jogo", L"Choose the game's cover"), start, FALSE)) return;
    GpImage *test = LoadImageFile(f); UINT w = 0; if (test) { GdipGetImageWidth(test, &w); GdipDisposeImage(test); }
    if (!w) { Msg(C_ORANGE, L"%s", L2(L"Essa imagem não abriu: use PNG, JPG, BMP ou GIF de até 24 MB.", L"That picture did not open: use a PNG, JPG, BMP or GIF of up to 24 MB.")); Snd(SND_ERR); return; }
    if (!LibArtCopy(g_cg[i].id, bg ? L"pic" : L"icon", f)) { Msg(C_BAD, L"%s", L2(L"Não consegui copiar a imagem para %LOCALAPPDATA%\\PORTHEON\\art.", L"Could not copy the picture to %LOCALAPPDATA%\\PORTHEON\\art.")); return; }
    LibBmpFree(&g_cTile[i]); g_cTileS[i] = 0; LibBmpFree(&g_cBg); g_cBgId = -1;
    Msg(C_GOOD, bg ? L2(L"Fundo do %s trocado.", L"%s's background changed.") : L2(L"Capa do %s trocada.", L"%s's cover changed."), g_cg[i].name); Snd(SND_OK);
}
static void CPageClick(int h)
{
    int i = g_cpIdx; if (i < 0 || i >= g_ncg) { g_overlay = OV_NONE; InvalidateRect(g_wnd, NULL, FALSE); return; }
    switch (h) {
    case H_CCLOSE: g_overlay = OV_NONE; Snd(SND_BACK); break;
    case H_CPLAY: g_overlay = OV_NONE; if (i != g_csel) CSelect(i); Snd(SND_PLAY); Play(); break;   /* Play's custom branch (portheon.c) */
    case H_COPEN: LibOpenFolder(i); break;
    case H_CPAD: PmOpenC(g_cg[i].id, g_cg[i].name); return;   /* 6.2 */
    case H_CKEYS: CkOpen(g_cg[i].id, g_cg[i].name); return;   /* 6.2 */
    case H_CCOVER: LibPickPicture(i, FALSE); break;
    case H_CBG: LibPickPicture(i, TRUE); break;
    case H_CLEFT: if (i > 0) { LibMove(i, -1); g_cpIdx = i - 1; g_focus = H_CLEFT; Snd(SND_MOVE); } break;
    case H_CRIGHT: if (i < g_ncg - 1) { LibMove(i, 1); g_cpIdx = i + 1; g_focus = H_CRIGHT; Snd(SND_MOVE); } break;
    case H_CINI: { wchar_t ini[MAX_PATH], q[MAX_PATH + 4]; LibIni(ini); _snwprintf(q, MAX_PATH + 4, L"\"%s\"", ini); q[MAX_PATH + 3] = 0;
                   ShellExecuteW(g_wnd, L"open", L"notepad.exe", q, NULL, SW_SHOWNORMAL);
                   Msg(C_SUB, L2(L"O %s está na seção [c%d] do library.ini (args, dir, rom...). Salve e volte ao PORTHEON: ele lê de novo.", L"%s is in section [c%d] of library.ini (args, dir, rom...). Save and come back to PORTHEON: it reads it again."), g_cg[i].name, g_cg[i].id); break; }
    case H_CREMOVE: {
        if (g_crun == g_cg[i].id) { Msg(C_ORANGE, L"%s", L2(L"Feche o jogo antes de remover.", L"Close the game before removing it.")); Snd(SND_ERR); break; }
        wchar_t q[400]; _snwprintf(q, 400, L2(L"Remover %s do PORTHEON?\n\nO jogo continua instalado: só a capa e o atalho daqui saem.", L"Remove %s from PORTHEON?\n\nThe game stays installed: only its cover and its place here go."), g_cg[i].name);
        if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
        wchar_t nm[80]; wcscpy(nm, g_cg[i].name); BOOL shown = g_csel == i;
        LibRemove(i); g_overlay = OV_NONE; g_cpIdx = -1;
        if (shown) { PIniSet(L"selected_custom", 0); FeatureChanged(-1); g_focus = H_CARD + g_sel; }
        Msg(C_GOOD, L2(L"%s saiu do PORTHEON.", L"%s left PORTHEON."), nm); Snd(SND_OK);
        break; }
    }
    InvalidateRect(g_wnd, NULL, FALSE);
}
static void PaintCPage(HDC dc, GpGraphics *g, RECT rc)
{
    Dim(g, rc);
    if (g_cpIdx < 0 || g_cpIdx >= g_ncg) g_cpIdx = g_csel;   /* (a UI shot sets the overlay straight away) */
    int i = g_cpIdx; if (i < 0 || i >= g_ncg) return;
    CGame *c = &g_cg[i]; BOOL run = g_crun == c->id, stop = LibCanStop(c);
    RECT cd = Card(g, 330, 56, 620, 690);
    int L = cd.left + SC(34), R = cd.right - SC(34), y = cd.top + SC(22);
    RECT tr = {L, y, R - SC(50), y + SC(36)}; TextR(dc, g_fH2, RGB(255, 255, 255), tr, c->name, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    wchar_t lab[200]; LibKindLine(c, lab, 200); RECT lr = {L, y + SC(42), R, y + SC(64)}; TextR(dc, g_fSmall, C_SUB, lr, lab, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    y = cd.top + SC(100); int bh = SC(48), gap = SC(10), half = (R - L - gap) / 2;
    RECT b0 = {L, y, R, y + bh}; Button(dc, g, b0, run ? (stop ? T(S_STOP) : L2(L"Aberto (feche pelo jogo)", L"Open (close it from the game)")) : T(S_PLAY), H_CPLAY, run && !stop ? 0 : 1); y += bh + gap;
    RECT b1 = {L, y, R, y + bh}; Button(dc, g, b1, L2(L"Abrir pasta", L"Open folder"), H_COPEN, 0); y += bh + gap;
    RECT kp = {L, y, L + half, y + bh}; Button(dc, g, kp, L2(L"Controle", L"Controller"), H_CPAD, 0);   /* 6.2 */
    RECT kk = {L + half + gap, y, R, y + bh}; Button(dc, g, kk, L2(L"Teclado", L"Keyboard"), H_CKEYS, 0); y += bh + gap;
    RECT b2 = {L, y, L + half, y + bh}; Button(dc, g, b2, L2(L"Trocar capa", L"Change cover"), H_CCOVER, 0);
    RECT b3 = {L + half + gap, y, R, y + bh}; Button(dc, g, b3, L2(L"Trocar fundo", L"Change background"), H_CBG, 0); y += bh + gap;
    RECT b4 = {L, y, L + half, y + bh}; Button(dc, g, b4, L2(L"Mover para a esquerda", L"Move left"), H_CLEFT, 0);
    RECT b5 = {L + half + gap, y, R, y + bh}; Button(dc, g, b5, L2(L"Mover para a direita", L"Move right"), H_CRIGHT, 0); y += bh + gap;
    RECT b6 = {L, y, R, y + bh}; Button(dc, g, b6, L2(L"Editar opções (library.ini)", L"Edit options (library.ini)"), H_CINI, 0); y += bh + gap;
    RECT b7 = {L, y, R, y + bh}; Button(dc, g, b7, L2(L"Remover do PORTHEON", L"Remove from PORTHEON"), H_CREMOVE, 2); y += bh + SC(22);
    wchar_t pl[96], ls[96]; LibPlayed(c, pl, ls, 96);
    wchar_t info[300]; _snwprintf(info, 300, L"%s  ·  %s", pl, ls); info[299] = 0;
    Text(dc, g_fSmall, C_SUB, L, y, info); y += SC(26);
    wchar_t where[MAX_PATH * 2 + 8];
    if (c->kind == CK_EMU) _snwprintf(where, MAX_PATH * 2 + 8, L"%s\n%s", c->target, c->rom);
    else if (c->kind == CK_STEAM) _snwprintf(where, MAX_PATH * 2 + 8, L"steam://rungameid/%llu%s%s", c->gameid ? c->gameid : (unsigned long long)c->appid, c->watchDir[0] ? L"\n" : L"", c->watchDir);
    else if (c->kind == CK_EPIC) _snwprintf(where, MAX_PATH * 2 + 8, L"Epic: %s%s%s", c->epicApp, c->watchDir[0] ? L"\n" : L"", c->watchDir);
    else _snwprintf(where, MAX_PATH * 2 + 8, L"%s", c->target);
    where[MAX_PATH * 2 + 7] = 0;
    for (wchar_t *ln = where, *nx; ln && *ln; ln = nx) {   /* each path on its own line, cut in the middle when long */
        nx = wcschr(ln, L'\n'); if (nx) *nx++ = 0;
        RECT wr = {L, y, R, y + SC(22)}; TextR(dc, g_fSmall, C_DIM, wr, ln, DT_LEFT | DT_VCENTER | DT_PATH_ELLIPSIS); y += SC(22);
    }
    LibOverlayMsg(dc, cd);
    CloseX(g, cd, H_CCLOSE);
}

#ifdef PORTHEON_TEST
void CpT(void) { if (g_ncg > 0) g_cpIdx = 0; }
#endif
/* ---------------------------------------------------------------- the hooks' entry points (ui_input.c) */
static BOOL LibClick(int h)   /* OnClick on the home screen: TRUE when it was the library's */
{
    if (h >= H_CUSTOM && h < H_CUSTOM + g_ncg) { int i = h - H_CUSTOM; if (i == g_csel && g_view == V_HOME) CPageOpen(i); else CSelect(i); return TRUE; }
    if (h == H_LIBADD) { LibAddOpen(); return TRUE; }
    if (h == H_CCARD) { LibOpenFolder(g_csel); return TRUE; }
    if (h == H_CCARD + 1) { CPageOpen(g_csel); return TRUE; }
    return FALSE;
}
static BOOL LibOverlayClick(int h)   /* OnClick while one of the library's overlays is up */
{
    if (g_overlay == OV_LIBADD) { LibAddClick(h); return TRUE; }
    if (g_overlay == OV_CGAME) { CPageClick(h); return TRUE; }
    return FALSE;
}
static BOOL PaintLibOverlay(HDC dc, GpGraphics *g, RECT rc)
{
    if (g_overlay == OV_LIBADD) { PaintLibAdd(dc, g, rc); return TRUE; }
    if (g_overlay == OV_CGAME) { PaintCPage(dc, g, rc); return TRUE; }
    return FALSE;
}
