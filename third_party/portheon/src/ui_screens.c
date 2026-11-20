/* ui_screens.c - part of ui.c (4.0): every screen of the console interface. */

/* ---------------------------------------------------------------- the background of each screen */
/* perf 11-13: the crossfade to another game's background. The old background was drawn whole through a GDI+ color matrix
   on every frame of the fade (13 ms at 1080p); now it is copied once into a plain picture and GDI's AlphaBlend lays it
   over the new one (5 ms), as the home's page change does. FALSE = it could not: the caller draws it the old way. */
static BOOL FadeBlend(HDC dc, GpGraphics *g, GpBitmap *o, int game, int W, int H, float a)
{
    int sca = (int)(255.0f * a); if (sca <= 2) return TRUE; if (sca > 255) sca = 255;   /* (DrawAlpha draws nothing under 0.01) */
    HDC kdc = CreateCompatibleDC(dc); if (!kdc) return FALSE;
    HGDIOBJ kold = NULL;
    if (!g_fadeKeep || g_fadeKeepGame != game || g_fadeKeepOf != o || g_fadeKeepW != W || g_fadeKeepH != H) {
        FadeKeepFree();
        BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), W, -H, 1, 32, BI_RGB}}; void *bits = NULL;
        g_fadeKeep = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!g_fadeKeep) { DeleteDC(kdc); return FALSE; }
        kold = SelectObject(kdc, g_fadeKeep);
        GpGraphics *kg = NULL; GdipCreateFromHDC(kdc, &kg);
        if (!kg) { SelectObject(kdc, kold); DeleteDC(kdc); FadeKeepFree(); return FALSE; }
        GdipSetCompositingMode(kg, 1 /* SourceCopy */); GdipDrawImageRectI(kg, (GpImage *)o, 0, 0, W, H); GdipDeleteGraphics(kg);
        g_fadeKeepGame = game; g_fadeKeepOf = o; g_fadeKeepW = W; g_fadeKeepH = H;
    } else kold = SelectObject(kdc, g_fadeKeep);
    GdipFlush(g, 1);   /* what GDI+ drew so far is in the picture before GDI writes over it */
    POINT vo = {0, 0}; SetViewportOrgEx(dc, 0, 0, &vo);
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, (BYTE)sca, 0}; BOOL ok = AlphaBlend(dc, 0, 0, W, H, kdc, 0, 0, W, H, bf);
    SetViewportOrgEx(dc, vo.x, vo.y, NULL);
    SelectObject(kdc, kold); DeleteDC(kdc);
    return ok;
}
static void PaintBackground(HDC dc, GpGraphics *g, RECT rc)
{
    int X = 0, Y = 0, W = SC(BASE_W), H = SC(BASE_H);
    /* full screen: the background fills the whole monitor, not only the frame (4.0.1: no dark bars at the sides) */
    if (g_ox || g_oy) { X = -g_ox; Y = -g_oy; W = rc.right; H = rc.bottom; }
    /* 4.0.1: the cached background is opaque: copied over (no blending), 4 to 5 times faster in 4K */
    GpBitmap *b = g_view == V_GAME ? GameBgDim(g_sel, W, H) : g_view == V_HOME ? (g_csel >= 0 ? CustomBg(g_csel, W, H) : GameBg(g_sel, W, H)) : NeutralBg(W, H);   /* 6.1: CustomBg */
    if (b) { GdipSetCompositingMode(g, 1 /* SourceCopy */); GdipDrawImageRectI(g, (GpImage *)b, X, Y, W, H); GdipSetCompositingMode(g, 0); }
    else Fill(g, X, Y, W, H, A(C_BG, 255));
    if (g_view == V_HOME || g_view == V_GAME) {
        float ft = FadeT();
        if (g_fadeFrom >= 0 && g_fadeFrom != g_sel && ft < 1.0f) { GpBitmap *o = g_view == V_GAME ? NULL : GameBg(g_fadeFrom, W, H); if (o && !FadeBlend(dc, g, o, g_fadeFrom, W, H, 1.0f - ft)) DrawAlpha(g, (GpImage *)o, X, Y, W, H, 1.0f - ft); }
        (void)0;   /* 5.0: V_GAME's darker background is cached (GameBgDim) */
    }
    if (g_fadeKeep && (g_view != V_HOME || g_fadeFrom < 0 || g_fadeFrom == g_sel || FadeT() >= 1.0f)) FadeKeepFree();   /* perf 11-13: the fade is over */
}

/* ---------------------------------------------------------------- the top bar
   4.0.4: as on a PS5: the sections on the left in plain words (the one open in white), and on the right the system's
   round icons (window mode, settings, the profile), the clock and the power button. An icon says what it is (a label
   under it) while the mouse or the focus is on it. */
static void TopIcon(HDC dc, GpGraphics *g, int cx, int id, int icon, BOOL on, const wchar_t *label)
{
    BOOL hot = g_hot == id; int r = SC(18), cy = SC(34), s = SC(18);
    if (hot || on) { Dot(g, cx, cy, r, A(RGB(255, 255, 255), hot ? 44 : 22)); if (hot) Ring(g, cx, cy, r, A(RGB(255, 255, 255), 230), 1.6f); }   /* 6.0: round plates, as on the console */
    UINT32 c = A(hot || on ? RGB(255, 255, 255) : C_TEXT, 255);
    switch (icon) {
    case 0: IconGear(g, cx - s / 2, cy - s / 2, s, c); break;
    case 1: IconWindow(g, cx - s / 2, cy - s / 2, s, c); break;
    case 2: IconExpand(g, cx - s * 4 / 10, cy - s * 4 / 10, s * 8 / 10, c); break;
    default: IconPower(g, cx - SC(8), cy - SC(8), SC(16), c);
    }
    if (hot && label) {
        int w = TextW(dc, g_fSmall, label) + SC(20), l = cx - w / 2; if (l + w > SC(BASE_W - 6)) l = SC(BASE_W - 6) - w;
        RECT lr = {l, SC(58), l + w, SC(80)}; Box(g, lr, SC(11), A(RGB(16, 12, 28), 236), A(RGB(255, 255, 255), 40), 1.0f);
        TextR(dc, g_fSmall, RGB(255, 255, 255), lr, label, DT_CENTER | DT_VCENTER);
    }
    AddHit(cx - r, cy - r, cx + r, cy + r, id);
}
static void PaintTopBar(HDC dc, GpGraphics *g)
{
    /* 6.0 (the user: "PORTHEON exactly like the PS5's interface, with PORTHEON's logo"): PORTHEON's mark, then the two
       sections in plain words (the open one in white, the other grey, no underline); on the right only what the console
       shows: settings, the profile, the clock and the power button. Install a game is the last tile of the games' row,
       as the console's library; the language and full screen are in Settings > Interface (F11 still switches it). */
    VGrad(g, -g_ox, -g_oy, SC(BASE_W) + 2 * g_ox, SC(84) + g_oy, A(RGB(0, 0, 0), 120), A(RGB(0, 0, 0), 0));   /* the whole width in full screen */
    int lx = SC(MX + 12), my = SC(34);
    if (g_imgLogo) DrawAlpha(g, g_imgLogo, lx, my - SC(16), SC(32), SC(32), 1.0f);
    struct { const wchar_t *t; int view; } nav[2] = {{L2(L"Jogos", L"Games"), V_HOME}, {TR(L"Online", L"Online"), V_ONLINE}};
    int x = lx + SC(56);
    for (int k = 0; k < 2; k++) {
        BOOL on = g_view == nav[k].view || (k == 0 && g_view == V_GAME), h2 = g_hot == H_NAV + k;
        int w = TextW(dc, g_fCard, nav[k].t);
        Text(dc, g_fCard, on ? RGB(255, 255, 255) : h2 ? RGB(228, 228, 234) : RGB(146, 146, 156), x, SC(20), nav[k].t);
        AddHit(x - SC(10), SC(12), x + w + SC(10), SC(56), H_NAV + k);
        x += w + SC(34);
    }
    {   /* 7.3 (the owner: "coloque o Portheon Arena lá em cima ao lado de jogos e online"): the third word opens the
           ARENA (its own window) for the game shown; it is never "the open section", so it stays grey until hot */
        const wchar_t *t = L"PORTHEON ARENA"; BOOL h2 = g_hot == H_ARENATOP; int w = TextW(dc, g_fCard, t);
        Text(dc, g_fCard, h2 ? RGB(255, 255, 255) : RGB(146, 146, 156), x, SC(20), t);
        AddHit(x - SC(10), SC(12), x + w + SC(10), SC(56), H_ARENATOP);
    }
    int rx = SC(BASE_W - MX);
    TopIcon(dc, g, rx - SC(18), H_POWER, 3, FALSE, L2(L"Energia", L"Power")); rx -= SC(36) + SC(16);
    {   /* the clock */
        SYSTEMTIME t; GetLocalTime(&t); wchar_t c[16]; _snwprintf(c, 16, L"%02d:%02d", t.wHour, t.wMinute);
        int w = TextW(dc, g_fCard, c); Text(dc, g_fCard, RGB(255, 255, 255), rx - w, SC(20), c); rx -= w + SC(18);
    }
    {   /* the profile: its picture with its status dot; its name while the mouse is on it. 4.4.0: a click opens the
           profile card (status, edit, switch, add) */
        Profile *p = CurProf(); BOOL hot = g_hot == H_PROFILE && g_overlay != OV_PROFMENU; int cx = rx - SC(19);
        AvatarSt(dc, g, cx, SC(34), SC(17), p, g_overlay == OV_PROFMENU);
        if (hot) {
            Ring(g, cx, SC(34), SC(21), A(RGB(255, 255, 255), 240), 2.0f);
            int w = TextW(dc, g_fSmall, p->name) + SC(20); RECT lr = {cx - w / 2, SC(58), cx + w / 2, SC(80)};
            Box(g, lr, SC(11), A(RGB(18, 18, 24), 236), A(RGB(255, 255, 255), 40), 1.0f); TextR(dc, g_fSmall, RGB(255, 255, 255), lr, p->name, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
        }
        AddHit(cx - SC(21), SC(13), cx + SC(21), SC(55), H_PROFILE); rx -= SC(38) + SC(14);
    }
    TopIcon(dc, g, rx - SC(18), H_NAV + 3, 0, g_view == V_SETTINGS, L2(L"Configurações", L"Settings")); rx -= SC(36) + SC(16);
    if (g_updState >= 1) {   /* the update waiting: a white pill, as the console's notices */
        wchar_t v[64];
        if (g_updState == 2) _snwprintf(v, 64, L2(L"Baixando %d%%", L"Downloading %d%%"), g_dlTotal ? (int)(g_dlGot * 100 / g_dlTotal) : 0);
        else _snwprintf(v, 64, L2(L"Atualizar para %s", L"Update to %s"), g_updTag);
        int w = TextW(dc, g_fSmall, v) + SC(48); RECT r = {rx - w, SC(18), rx, SC(50)}; BOOL uh = g_hot == H_UPDATE;
        Box(g, r, SC(16), A(RGB(255, 255, 255), uh ? 255 : 228), 0, 0);
        IconDownload(g, r.left + SC(14), r.top + SC(10), SC(12), A(RGB(16, 16, 22), 255));
        RECT t = {r.left + SC(34), r.top, r.right - SC(12), r.bottom}; TextR(dc, g_fSmall, RGB(16, 16, 22), t, v, DT_LEFT | DT_VCENTER);
        AddHit(r.left, r.top, r.right, r.bottom, H_UPDATE);
    }
}

/* ---------------------------------------------------------------- the author's corner and the status line
   4.4.0 (the user: "only my Discord, my GitHub name and the original name, in a cleaner corner"): no strip across the
   screen any more, no PIX and no phone: a small line at the lower right with the name and two round links */
static int CredPill(HDC dc, GpGraphics *g, int right, int top, int h, int icon, const wchar_t *label, int id)
{
    BOOL hot = g_hot == id; int is = SC(13), pad = SC(12), gap = SC(7), lw = TextW(dc, g_fSmall, label);
    RECT r = {right - (pad + is + gap + lw + pad), top, right, top + h};
    Box(g, r, h / 2, A(RGB(255, 255, 255), hot ? 30 : 9), hot ? 0 : A(RGB(255, 255, 255), 34), 1.0f);
    if (hot) NeonEdge(g, r, h / 2, 1.4f, 0.0f, 0);
    UINT32 ic = A(hot ? RGB(255, 255, 255) : C_SUB, 255); int ix = r.left + pad, iy = top + (h - is) / 2;
    if (icon == 0) IconChat(g, ix, iy, is, ic); else IconLink(g, ix, iy, is, ic);
    RECT t = {ix + is + gap, top, r.right - pad + SC(2), top + h}; TextR(dc, g_fSmall, hot ? RGB(255, 255, 255) : C_TEXT, t, label, DT_LEFT | DT_VCENTER);
    AddHit(r.left, r.top, r.right, r.bottom, id);
    return r.left;
}
static void PaintCredits(HDC dc, GpGraphics *g, int y)
{
    int h = SC(30), top = SC(y), rx = SC(MX + CW);
    wchar_t nick[48]; _snwprintf(nick, 48, L"@%s", AUTHOR_NICK);
    rx = CredPill(dc, g, rx, top, h, 1, nick, H_CRED_AUTHOR) - SC(8);
    rx = CredPill(dc, g, rx, top, h, 0, L"Discord", H_CRED_DISCORD) - SC(14);
    wchar_t by[96]; _snwprintf(by, 96, L"Portheon %s  ·  © 2026 %s", APP_SHOW, AUTHOR_NAME);   /* 11-13, the user: "write Portheon Released, only the P and the R in capitals" (the version's name; the brand's own logo stays as it is). 7.1: "Portheon Update 7.1" */
    int bw = TextW(dc, g_fSmall, by); RECT t = {rx - bw, top, rx + SC(2), top + h}, ts = t; OffsetRect(&ts, 1, 1);
    TextR(dc, g_fSmall, RGB(0, 0, 0), ts, by, DT_LEFT | DT_VCENTER); TextR(dc, g_fSmall, C_SUB, t, by, DT_LEFT | DT_VCENTER);
    if (g_imgLogo) DrawAlpha(g, g_imgLogo, t.left - SC(28), top + (h - SC(20)) / 2, SC(20), SC(20), 0.9f);
}
static void PaintStatus(HDC dc, GpGraphics *g)
{
    RECT sb = {SC(MX), SC(756), SC(MX + CW), SC(786)};
    Box(g, sb, SC(15), A(C_SURF, 220), A(C_LINE, 230), 1.0f);
    /* 6.2 (the user's circle on 6.0's Updates page: an error that stayed in the status line for good): a message is the
       toast's for its 8 s; the line keeps only calm facts */
    BOOL fresh = g_msg[0] && GetTickCount() - g_msgAt < 8000;
    int st = St(g_sel); COLORREF mc = fresh ? g_msgColor : StatusColor(st);
    Dot(g, sb.left + SC(18), (sb.top + sb.bottom) / 2, SC(4), A(mc, 255));
    wchar_t pc[200] = L"";
    if (g_haveGpu) _snwprintf(pc, 200, L"%s  ·  %d threads  ·  %d GB", g_gpu.name, g_threads, g_ramGb);
    int pw = TextW(dc, g_fSmall, pc);
    RECT pr = {sb.right - SC(16) - pw, sb.top, sb.right - SC(16), sb.bottom}; TextR(dc, g_fSmall, C_DIM, pr, pc, DT_RIGHT | DT_VCENTER);
    wchar_t def[200]; _snwprintf(def, 200, L"%s  ·  %s  ·  %s", CurProf()->name, G[g_sel].name, StatusText(st));
    RECT mr = {sb.left + SC(32), sb.top, pr.left - SC(16), sb.bottom};
    TextR(dc, g_fSmall, fresh ? g_msgColor : C_SUB, mr, fresh ? g_msg : def, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    if (fresh && TextW(dc, g_fSmall, g_msg) > mr.right - mr.left) AddHit(mr.left, mr.top, mr.right, mr.bottom, H_MSG);
}
/* the notification: what just happened, top right, for 8 s (a click shows all of it) */
static void PaintToast(HDC dc, GpGraphics *g)
{
    DWORD age = GetTickCount() - g_msgAt;
    if (!g_msg[0] || age >= 8000 || g_view == V_BOOT) return;
    /* 4.4.0: "Busy" is do-not-disturb, and the game's loading picture stays clean: only warnings and errors show there */
    BOOL serious = g_msgColor == C_BAD || g_msgColor == C_ORANGE;
    if (!serious && (CurProf()->status == PST_BUSY || g_view == V_LAUNCH || g_view == V_WELCOME)) return;
    float in = age < 260 ? age / 260.0f : 1.0f, out = age > 7400 ? (8000 - age) / 600.0f : 1.0f, a = in < out ? in : out;
    int right, top; ToastPlace(&right, &top);
    int w = SC(440), x = SC(right) - w + (int)((1.0f - in) * SC(40)), y = SC(top);
    RECT tr = {x + SC(46), y + SC(12), x + w - SC(16), y + SC(400)};
    int th = Wrap(dc, g_fSmall, C_TEXT, tr, g_msg, FALSE); BOOL cut = th > SC(60); if (cut) th = SC(60);
    RECT r = {x, y, x + w, y + th + SC(26)};
    RECT sh = {r.left + SC(4), r.top + SC(8), r.right + SC(4), r.bottom + SC(8)}; Box(g, sh, SC(16), A(RGB(0, 0, 0), (BYTE)(110 * a)), 0, 0);
    Box(g, r, SC(16), A(RGB(16, 12, 30), (BYTE)(240 * a)), A(g_msgColor, (BYTE)(200 * a)), 1.2f);
    Dot(g, x + SC(24), y + SC(22), SC(6), A(g_msgColor, (BYTE)(255 * a)));
    tr.bottom = r.bottom - SC(10);
    SelectObject(dc, g_fSmall); SetTextColor(dc, Mix(C_BG, C_TEXT, a)); GdiText(dc, TRUE); DrawTextW(dc, g_msg, -1, &tr, DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS); GdiText(dc, FALSE);
    if (g_view != V_HOME || cut) AddHit(r.left, r.top, r.right, r.bottom, H_TOAST);   /* 7.0 fix: on the home screen it lies over the row of tiles: it takes the click only when its words were cut (as H_MSG) */
}

/* ---------------------------------------------------------------- the opening (6.0)
   The user (2026-11-07): "exactly like the PS5's interface, with PORTHEON's logo" and "work on top of this new design"
   (the new logo: a P in a diamond of two linked bands). As the console starts: black, then PORTHEON's logo comes up in
   white out of a soft light, with a slight zoom, and a band of light crosses it; the name follows under it, then
   everything goes back to black. 2.5 s;
   any key or click skips it. Every frame is drawn from the time alone, so a start that waits on Windows lands mid-way. */
/* EaseOut3: ui.c (5.0) */
/* Clamp01: home62.c (6.2) */
/* 6.0 final: the logo already at the opening's size, made once (about 0.3 MB, freed when the opening ends): the logo
   and the light's five clipped draws copy this small picture instead of scaling the 1024 px logo every frame, which
   took a frame of the opening from about 20 to 120 ms while the light crossed it */
static GpBitmap *g_bootLogo; static int g_bootLogoS;
static void BootLogoFree(void) { FreeBmp(&g_bootLogo); g_bootLogoS = 0; }
static GpImage *BootLogo(int s)
{
    if (!g_imgLogo || s <= 0) return g_imgLogo;
    if (g_bootLogo && g_bootLogoS == s) return (GpImage *)g_bootLogo;
    BootLogoFree();
    GpGraphics *bg = NULL; GpBitmap *b = NewBitmap(s, s, &bg);
    if (!b || !bg) { if (bg) GdipDeleteGraphics(bg); if (b) GdipDisposeImage((GpImage *)b); return g_imgLogo; }
    UINT iw = 0, ih = 0; GdipGetImageWidth(g_imgLogo, &iw); GdipGetImageHeight(g_imgLogo, &ih);
    GpImageAttributes *ta = ThemeImageAttr(1.0f);   /* the theme's turn, once (NULL for the logo's black and white) */
    GdipDrawImageRectRectI(bg, g_imgLogo, 0, 0, s, s, 0, 0, (INT)iw, (INT)ih, 2, ta, NULL, NULL); if (ta) GdipDisposeImageAttributes(ta);
    GdipDeleteGraphics(bg); g_bootLogo = b; g_bootLogoS = s; return (GpImage *)b;
}
static void PaintBoot(HDC dc, GpGraphics *g, RECT rc)
{
    DWORD t = GetTickCount() - g_bootT0;
    Fill(g, -g_ox, -g_oy, rc.right, rc.bottom, 0xFF000000);
    int W = SC(BASE_W), H = SC(BASE_H), cx = W / 2, cy = H / 2 - SC(40);
    float out = t > BOOT_MS - 520 ? Clamp01((BOOT_MS - (float)t) / 520.0f) : 1.0f;
    float k = Clamp01(((float)t - 120.0f) / 1150.0f), e = EaseOut3(k), a = Clamp01(k * 1.35f) * out;
    /* the light behind it: wide and faint, a cooler core */
    Glow(g, cx, cy, SC(340), SC(300), A(RGB(255, 255, 255), (BYTE)(20 * a)));
    Glow(g, cx, cy, SC(190), SC(170), A(RGB(176, 192, 255), (BYTE)(28 * a)));
    int ls = (int)(SC(200) * (0.90f + 0.10f * e)), lx = cx - ls / 2, ly = cy - ls / 2;
    /* 6.0 final (the user: "on entering, the new logo, like the PS5's entrance when it turns on"): the logo comes up a
       little grey, a band of light crosses it (drawn as the logo again inside a slanted band, so the light takes the
       logo's own shape) with a soft light passing behind, and it stays full white */
    float sw = Clamp01(((float)t - 1180.0f) / 640.0f), full = Clamp01(((float)t - 1560.0f) / 380.0f);
    GpImage *logo = BootLogo(SC(200));
    if (logo && ls > 0) {
        DrawAlpha(g, logo, lx, ly, ls, ls, a * (0.74f + 0.26f * full));
        if (sw > 0.0f && sw < 1.0f) {
            float bx = lx - ls * 0.45f + sw * ls * 1.9f, slant = ls * 0.35f;
            Glow(g, (int)bx, cy, ls / 3, ls / 2, A(RGB(255, 255, 255), (BYTE)(26 * a)));
            for (int i = 0; i < 5; i++) {   /* five nested bands: the light is soft at its edges */
                float hw = ls * (0.16f - 0.03f * i);
                GpPointF pt[4] = {{bx - hw + slant, (float)(ly - 4)}, {bx + hw + slant, (float)(ly - 4)}, {bx + hw - slant, (float)(ly + ls + 4)}, {bx - hw - slant, (float)(ly + ls + 4)}};
                GpPath *p = NULL; GdipCreatePath(0, &p); if (!p) break;
                GdipAddPathPolygon(p, pt, 4); GdipClosePathFigure(p);
                CLIPPATH(g, p); DrawAlpha(g, logo, lx, ly, ls, ls, a * 0.38f); ClipBase(g); GdipDeletePath(p);
            }
        }
    }
    if (t > 820) {   /* the name, its letters closing in */
        float b = Clamp01((t - 820) / 700.0f), eb = EaseOut3(b); b *= out;
        HFONT f = CreateFontW(-SC(30), 0, 0, 0, FW_LIGHT, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Bahnschrift");
        int sp = 14 + (int)(10 * (1.0f - eb)), w = SpacedW(dc, f, L"PORTHEON", sp);   /* Spaced scales it */
        Spaced(dc, f, Mix(RGB(0, 0, 0), RGB(255, 255, 255), b), cx - w / 2, cy + SC(132), L"PORTHEON", sp);
        DeleteObject(f);
    }
    if (t > 1500) {
        float c = Clamp01((t - 1500) / 450.0f) * out;
        wchar_t s[160]; _snwprintf(s, 160, L"© 2026 %s (%s)  ·  Portheon %s", AUTHOR_NAME, AUTHOR_NICK, APP_SHOW);   /* 7.1: the whole shown name (7.0 showed "Released" alone here) */
        RECT r = {0, H - SC(56), W, H - SC(28)}; TextR(dc, g_fSmall, Mix(RGB(0, 0, 0), RGB(120, 120, 130), c), r, s, DT_CENTER | DT_VCENTER);
    }
    AddHit(0, 0, W, H, H_SKIPBOOT);
}
/* ---------------------------------------------------------------- 4.4.0: create your account (once, after the opening) */
static void PaintWelcome(HDC dc, GpGraphics *g)
{
    int W = SC(BASE_W), cx = W / 2; DWORD t = GetTickCount() - g_viewT0; float a = Clamp01(t / 500.0f), e = EaseOut3(a);
    Glow(g, cx, SC(330), SC(520), SC(330), A(C_PURPLE, (BYTE)(46 * a)));
    Glow(g, cx, SC(330), SC(380), SC(260), A(C_CYAN, (BYTE)(40 * a)));
    if (g_imgLogo) DrawAlpha(g, g_imgLogo, cx - SC(30), SC(52), SC(60), SC(60), a);
    HFONT f = CreateFontW(-SC(46), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, FaceHead());
    RECT tr = {0, SC(124) + (int)(SC(16) * (1.0f - e)), W, SC(184) + (int)(SC(16) * (1.0f - e))};
    TextR(dc, f, Mix(C_BG, RGB(255, 255, 255), a), tr, L2(L"Bem-vindo ao PORTHEON", L"Welcome to PORTHEON"), DT_CENTER | DT_VCENTER); DeleteObject(f);
    RECT st = {SC(240), SC(190), W - SC(240), SC(240)};
    TextR(dc, g_fText, Mix(C_BG, C_SUB, a), st, L2(L"Crie a sua conta: o seu nome nos jogos, a sua foto e o seu status.", L"Create your account: your name in the games, your photo and your status."), DT_CENTER | DT_VCENTER);
    Profile *p = CurProf(); int ay = SC(368), r = SC(84);
    float pu = Pulse(2400);
    for (int k = 3; k >= 1; k--) Ring(g, cx, ay, r + SC(10 * k) + (int)(SC(4) * pu), A(Mix(C_CYAN, C_PURPLE, k / 3.0f), (BYTE)(40 / k)), 1.5f);
    AvatarSt(dc, g, cx, ay, r, p, TRUE);
    RECT nr = {0, ay + r + SC(20), W, ay + r + SC(56)}; TextR(dc, g_fCard, RGB(255, 255, 255), nr, p->name, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
    RECT b1 = {cx - SC(170), SC(560), cx + SC(170), SC(618)}; IconButton(dc, g, b1, L2(L"Criar minha conta", L"Create my account"), H_WELCOMEGO, 1, 5);
    RECT b2 = {cx - SC(110), SC(632), cx + SC(110), SC(676)}; Button(dc, g, b2, L2(L"Agora não", L"Not now"), H_WELCOMESKIP, 2);
    RECT hint = {0, SC(700), W, SC(730)};
    TextR(dc, g_fSmall, C_DIM, hint, L2(L"Dá para mudar tudo depois: clique na sua foto, no canto de cima.", L"You can change it all later: click your picture at the top."), DT_CENTER | DT_VCENTER);
}

/* ---------------------------------------------------------------- who is playing? (4.4.0: photos and status, as on a console) */
static void PaintProfiles(HDC dc, GpGraphics *g)
{
    int W = SC(BASE_W); DWORD t = GetTickCount() - g_viewT0; float a = Clamp01(t / 400.0f), e = EaseOut3(a);
    Glow(g, W / 2, SC(380), SC(560), SC(260), A(C_PURPLE, (BYTE)(34 * a)));
    if (g_imgLogo) DrawAlpha(g, g_imgLogo, W / 2 - SC(26), SC(64), SC(52), SC(52), a);
    HFONT f = CreateFontW(-SC(44), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, FaceHead());
    RECT tr = {0, SC(134), W, SC(194)}; TextR(dc, f, Mix(C_BG, RGB(255, 255, 255), a), tr, L2(L"Quem está jogando?", L"Who is playing?"), DT_CENTER | DT_VCENTER); DeleteObject(f);
    int n = g_nprof + (g_nprof < MAXPROF ? 1 : 0), step = SC(186), r = SC(70);
    if (n * step > W - SC(80)) { step = (W - SC(80)) / n; r = step * 36 / 100; }   /* 8 profiles and "add" still fit the width */
    int x0 = W / 2 - (n - 1) * step / 2, cy = SC(372) + (int)(SC(24) * (1.0f - e));
    for (int i = 0; i < n; i++) {
        int cx = x0 + i * step; BOOL add = i == g_nprof, hot = g_hot == (add ? H_PROFADD : H_PROFPICK + i);
        int yy = cy - (hot ? SC(8) : 0);
        if (add) {
            Dot(g, cx, yy, r, A(RGB(255, 255, 255), hot ? 30 : 10)); Ring(g, cx, yy, r, A(RGB(255, 255, 255), hot ? 240 : 90), 2.0f);   /* 6.0: round, as on the console */
            IconPlus(g, cx - r * 3 / 10, yy - r * 3 / 10, r * 6 / 10, A(hot ? RGB(255, 255, 255) : C_SUB, 255));
            RECT nr = {cx - step / 2, yy + r + SC(16), cx + step / 2, yy + r + SC(44)}; TextR(dc, g_fBtn, hot ? RGB(255, 255, 255) : C_SUB, nr, L2(L"Adicionar usuário", L"Add user"), DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
            AddHit(cx - r, yy - r, cx + r, yy + r + SC(48), H_PROFADD);
        } else {
            if (hot) Glow(g, cx, yy, r + SC(40), r + SC(40), A(PROF_COLORS[g_prof[i].color % 12], 70));
            AvatarSt(dc, g, cx, yy, r, &g_prof[i], hot || i == g_curProf);
            RECT nr = {cx - step / 2, yy + r + SC(16), cx + step / 2, yy + r + SC(44)}; TextR(dc, g_fBtn, RGB(255, 255, 255), nr, g_prof[i].name, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
            RECT sr = {cx - step / 2, yy + r + SC(44), cx + step / 2, yy + r + SC(66)}; TextR(dc, g_fSmall, ProfStatusColor(g_prof[i].status), sr, ProfStatusName(g_prof[i].status), DT_CENTER | DT_VCENTER);
            AddHit(cx - r, yy - r, cx + r, yy + r + SC(66), H_PROFPICK + i);
        }
    }
    RECT mb = {W / 2 - SC(140), SC(606), W / 2 + SC(140), SC(652)}; IconButton(dc, g, mb, L2(L"Gerenciar usuários", L"Manage users"), H_PROFMANAGE, 2, 6);
    RECT hint = {0, SC(690), W, SC(720)};
    TextR(dc, g_fSmall, C_DIM, hint, L2(L"Cada usuário tem os próprios saves, o próprio nome nos jogos, a sua foto e o seu status.  Mouse, teclado ou controle.", L"Each user has their own saves, their own name in the games, a photo and a status.  Mouse, keyboard or controller."), DT_CENTER | DT_VCENTER);
}

/* ---------------------------------------------------------------- 4.4.0: the game while it loads
   As a console: PLAY shows the game's own picture over the whole screen, its tile and name, and a turning ring, until
   the game's window shows (FocusTimer); then PORTHEON leaves the screen and frees its memory. The picture is built once;
   only the ring's corner is drawn again (LaunchTick, 20 a second), so loading the game has the PC to itself. */
static GpBitmap *g_launchBg; static int g_launchBgW, g_launchBgH, g_launchBgGame = -1;
static void LaunchBgFree(void) { FreeBmp(&g_launchBg); g_launchBgGame = -1; }
static GpBitmap *LaunchBg(int gi, int w, int h)
{
    if (g_launchBg && g_launchBgW == w && g_launchBgH == h && g_launchBgGame == gi) return g_launchBg;
    LaunchBgFree(); LoadArt(gi);
    GpGraphics *g = NULL; GpBitmap *b = NewBitmap(w, h, &g); if (!b || !g) return b;
    Fill(g, 0, 0, w, h, A(RGB(0, 0, 0), 255));
    if (g_art[gi]) DrawCover(g, g_art[gi], 0, 0, w, h);
    else {
        POINT a = {0, 0}, z = {w, h}; GpBrush *lb = NULL; GdipCreateLineBrushI(&a, &z, A(G[gi].c2, 255), A(Mix(G[gi].c1, C_BG, 0.35f), 255), 0, &lb);
        if (lb) { GdipFillRectangleI(g, lb, 0, 0, w, h); GdipDeleteBrush(lb); }
    }
    /* a cinema veil: dark at the bottom where the name goes, and at the edges */
    VGrad(g, 0, h / 2, w, h - h / 2, A(RGB(0, 0, 0), 0), A(RGB(0, 0, 0), 235));
    VGrad(g, 0, 0, w, h / 5, A(RGB(0, 0, 0), 120), A(RGB(0, 0, 0), 0));
    HGrad(g, 0, 0, w / 4, h, A(RGB(0, 0, 0), 110), A(RGB(0, 0, 0), 0));
    HGrad(g, w - w / 4, 0, w / 4, h, A(RGB(0, 0, 0), 0), A(RGB(0, 0, 0), 110));
    GdipDeleteGraphics(g);
    g_launchBg = b; g_launchBgW = w; g_launchBgH = h; g_launchBgGame = gi; return b;
}
#define LAUNCH_SPIN_X (BASE_W - MX - 46)
#define LAUNCH_SPIN_Y (BASE_H - 58)
static void PaintLaunch(HDC dc, GpGraphics *g, RECT rc)
{
    int gi = g_launchGame >= 0 && g_launchGame < NGAMES ? g_launchGame : g_sel;
    int X = 0, Y = 0, W = SC(BASE_W), H = SC(BASE_H);
    if (g_ox || g_oy) { X = -g_ox; Y = -g_oy; W = rc.right; H = rc.bottom; }
    DWORD t = GetTickCount() - g_launchT0; float in = Clamp01(t / 380.0f), e = EaseOut3(in);
    GpBitmap *b = LaunchBg(gi, W, H);
    if (b) { GdipSetCompositingMode(g, 1); GdipDrawImageRectI(g, (GpImage *)b, X, Y, W, H); GdipSetCompositingMode(g, 0); }
    else Fill(g, X, Y, W, H, 0xFF000000);
    /* 6.0 (the user: "the way into the games exactly as on the PS5"): the console shows the game's own art over the whole
       screen and a small white ring turning at the lower right until the game is up; nothing else on it */
    (void)e; (void)dc;
    int scx = SC(LAUNCH_SPIN_X), scy = SC(LAUNCH_SPIN_Y), sr = SC(15);
    Ring(g, scx, scy, sr, A(RGB(255, 255, 255), 46), (float)SC(3));
    {   GpPen *pn = NULL; GdipCreatePen1(A(RGB(255, 255, 255), 240), (float)SC(3), 2, &pn);
        if (pn) { GdipSetPenStartCap(pn, 2); GdipSetPenEndCap(pn, 2); GdipDrawArcI(g, pn, scx - sr, scy - sr, 2 * sr, 2 * sr, (float)((GetTickCount() / 3) % 360), 90.0f); GdipDeletePen(pn); } }
    if (t > 12000) {   /* a slow start can always go back to PORTHEON */
        RECT hr = {SC(MX + 40), SC(BASE_H - 44), SC(BASE_W / 2 + 200), SC(BASE_H - 20)};
        TextR(dc, g_fSmall, C_SUB, hr, L2(L"Demorando? Esc volta para o PORTHEON (o jogo continua abrindo).", L"Taking long? Esc goes back to PORTHEON (the game keeps opening)."), DT_LEFT | DT_VCENTER);
    }
    if (in < 1.0f) Fill(g, X, Y, W, H, A(RGB(0, 0, 0), (BYTE)(255 * (1.0f - in))));
}
/* timer 13 (and Animate in the window that hands over): the first moment fades in whole, then only the ring's corner */
static void LaunchTick(void)
{
    if (g_view != V_LAUNCH) { KillTimer(g_wnd, 13); return; }
    g_anim = GetTickCount();
    DWORD t = GetTickCount() - g_launchT0;
    if (t < 420u) InvalidateRect(g_wnd, NULL, FALSE);   /* 6.0: only the fade in is whole (no bars any more) */
    else if (t > 12000 && t < 12300) Dirty(MX + 40, BASE_H - 44, BASE_W / 2 + 160 - MX, 24);   /* the slow-start hint, its strip only */
    else Dirty(LAUNCH_SPIN_X - 330, LAUNCH_SPIN_Y - 30, 360, 60);
}
static void LaunchShow(int gi, BOOL fadeIn)
{
    g_launchGame = gi; g_launchT0 = GetTickCount() - (fadeIn ? 0 : 1000);
    g_view = V_LAUNCH; g_overlay = OV_NONE; g_focus = -1; g_hot = -1;
    InvalidateRect(g_wnd, NULL, FALSE);
}

/* ---------------------------------------------------------------- the home screen */
static const int HOME_ORDER[NGAMES] = {12, 4, 5, 3, 2, 1, 0, 6, 11, 7, 8, 9, 10};   /* 6.1: then Bloodborne and inFAMOUS Second Son */   /* the newest first, as a console does (4.0.2: UFC 5; 4.3: UFC 6, UFC 5; 5.0: UFC Undisputed 3, 2012; 6.0: then Zelda and God of War III) */
#define TAPE_Y 600   /* 6.0: the cards (6.2: a little higher and taller, for the live cards; 7.0: under the bigger video, y 600..736) */
#define TAPE_H 136
/* Released (the two pages): on page 2 the cards stand left of the video, n across (the built-in games' four: two by two;
   a custom game's two: one over the other); with HOME_PAGES 0 a card keeps the place PaintHome gave it */
static RECT HomeCardRect(RECT r, int k, int n)
{
#if HOME_PAGES
    int x0 = SC(MX + 12) - SC(7), cw = (SC(VIT_X - 20 - (MX + 12)) + SC(14)) / n, top = SC(HP_CARDS_Y) + (k / n) * SC(TAPE_H + 14);
    r.left = x0 + (k % n) * cw; r.right = r.left + cw; r.top = top; r.bottom = top + SC(TAPE_H);
#else
    (void)k; (void)n;
#endif
    return r;
}
#define TILE_TOP(cy, sz, base, grow) (HOME_PAGES ? (cy) - ((base) + (grow)) / 2 : (cy) - (sz) / 2)   /* Released: the tiles hang from one line (the chosen one grows downward), as the console's row; 7.0: centred */
/* 5.0: the dynamic shelf order: HOME_ORDER, ready games first (L1/R1 and the shelves follow it) */
static int g_homeOrd[NGAMES];
static void HomeOrder(void)
{
    int n = 0;
    for (int pass = 0; pass < 2; pass++) for (int k = 0; k < NGAMES; k++) { int gi = HOME_ORDER[k]; BOOL ready = St(gi) == S_READY || St(gi) == S_RUNNING; if (ready == (pass == 0)) g_homeOrd[n++] = gi; }
}
/* perf 11-13: the home's pictures are made while the opening plays, not in the first frame of the home (286 ms of black
   between the opening and the home with a 4K background: 137 ms for the background, 6.6 ms a tile). The background of the
   game the home will open on is made once, while wWinMain waits for the Vulkan probe's thread (RunGuarded); then one tile
   a tick during the opening (Animate). Both through the same GameBg / TileBmp the paint calls, with the same sizes and
   the look the home's paint would have set (SkinSync), so the pictures are the very same; a wrong guess of the game, or
   an opening skipped half-way, only leaves the rest to the paint, as before. */
static int g_preGame = -1, g_preTile;
static void HomePrewarmBg(void)
{
    if (g_view != V_BOOT || g_csel >= 0 || g_preGame >= 0) return;
    if (!PIni(L"account_rev", 0) || !PIni(L"guide_seen", 0)) return;   /* the welcome or the install helper comes first */
    int gi = g_sel;
    HomeOrder();
    if (St(gi) != S_READY) for (int k = 0; k < NGAMES; k++) if (St(g_homeOrd[k]) == S_READY) { gi = g_homeOrd[k]; break; }   /* (OpenOnReady) */
    int wasSel = g_sel, wasView = g_view;
    g_sel = gi; g_view = V_HOME; SkinSync();   /* the look that game's home paints with */
    int X, Y, W, H; RECT rc = {0}; BgFrame(rc, &X, &Y, &W, &H);
    if (W > 0 && H > 0) GameBg(gi, W, H);
    g_sel = wasSel; g_view = wasView; SkinSync();
    StatusDirty();   /* (the PC is read after this: the paint asks the games' status again) */
    g_preGame = gi; g_preTile = 0;
}
static void HomePrewarmTile(void)   /* Animate, each tick of the opening: the next tile of the row the home will show */
{
    if (g_preGame < 0 || g_preTile >= NGAMES || g_view != V_BOOT) return;
    HomeOrder();
    int selK = 0; for (int k = 0; k < NGAMES; k++) if (g_homeOrd[k] == g_preGame) selK = k;
    int base = SC(HPV(68, 84)), grow = SC(HPV(40, 44)), gap = SC(HPV(10, 14)), first = selK > 2 ? selK - 2 : 0;
    RECT cl; GetClientRect(g_wnd, &cl);
    while (g_preTile < NGAMES) {
        int k = g_preTile++, x = SC(MX + 12) + (k - first) * (base + gap) + (k > selK ? grow : 0);
        if (k < first || x - SC(24) >= cl.right - g_ox) continue;   /* (PaintHomeIn draws only the tiles in sight) */
        TileBmp(g_homeOrd[k], SC(128) + SC(46), 0); return;        /* (GameTile's size for the row) */
    }
}
/* 6.2: "EM BREVE" across the top of a cover */
static void SoonRibbon(HDC dc, GpGraphics *g, int x, int y, int sz)
{
    int h = sz * 25 / 100; RECT r = {x, y, x + sz, y + h};
    GpPath *p = RoundPath(x, y, sz - 1, sz - 1, TILE_RAD(sz)); CLIPPATH(g, p);
    Fill(g, x, y, sz, h, A(RGB(14, 8, 26), 232)); Fill(g, x, y + h - SC(2), sz, SC(2), A(RGB(255, 72, 164), 255));
    ClipBase(g); GdipDeletePath(p);
    HFONT f = CreateFontW(-(h * 64 / 100), 0, 0, 0, FW_BLACK, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
    TextR(dc, f, RGB(255, 255, 255), r, L2(L"EM BREVE", L"SOON"), DT_CENTER | DT_VCENTER); DeleteObject(f);
}
/* 6.0: one of the four cards at the bottom, as the console's activity cards: smoked glass on its own, a white edge under
   the mouse or the focus, a small white icon, the title and two lines */
static void HomeCard(HDC dc, GpGraphics *g, RECT r, int k, int icon, const wchar_t *title, const wchar_t *l1, const wchar_t *l2, COLORREF accent)
{
    BOOL hot = g_hot == H_HOMECARD + k; (void)accent;
    r = HomeCardRect(r, k, 2);   /* Released: page 2's place */
    r.left += SC(7); r.right -= SC(7);   /* a gap between the cards */
    if (Seen(r)) {
        if (hot) Glow(g, (r.left + r.right) / 2, (r.top + r.bottom) / 2, (r.right - r.left) * 6 / 10, (r.bottom - r.top), A(RGB(255, 255, 255), 30));
        SkinCard(g, r, hot);   /* 6.2: the game's own card */
    }
    int is = SC(20), x = r.left + SC(20), y = r.top + SC(18); UINT32 ic = A(RGB(255, 255, 255), 255);   /* 6.2: the title where the live cards have theirs */
    switch (icon) { case 0: IconMonitor(g, x, y, is, ic); break; case 1: IconSave(g, x, y, is, ic); break; case 2: IconBolt(g, x, y, is, ic); break; case 3: IconGlobe(g, x, y, is, ic); break;
                    case 5: IconChip(g, x, y, is, ic); break; case 6: IconGpu(g, x, y, is, ic); break; case 7: IconDownload(g, x, y, is, ic); break; default: IconFolder(g, x, y, is, ic); }
    RECT tt = {x + is + SC(12), y - SC(4), r.right - SC(16), y + is + SC(4)}; TextR(dc, g_fBtn, RGB(255, 255, 255), tt, title, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    RECT a = {x, y + SC(30), r.right - SC(16), y + SC(50)}; TextR(dc, g_fSmall, RGB(228, 228, 234), a, l1, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    RECT b = {x, y + SC(50), r.right - SC(16), y + SC(70)}; TextR(dc, g_fSmall, RGB(150, 150, 162), b, l2, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    {   /* 6.2: where the card leads, at its foot (as the console's cards) */
        RECT f = {x, r.bottom - SC(34), r.right - SC(16), r.bottom - SC(14)}; TextR(dc, g_fSmall, RGB(120, 120, 132), f, L2(L"Abrir  ›", L"Open  ›"), DT_LEFT | DT_VCENTER); }
    AddHit(r.left, r.top, r.right, r.bottom, H_HOMECARD + k);
}
/* 6.0: a button as the console draws them: the main one white with dark words, the others smoked glass with white
   words; a white edge and a soft light under the mouse or the focus */
static void Ps5Button(HDC dc, GpGraphics *g, RECT r, const wchar_t *t, int id, BOOL primary, int icon)   /* icon: -1 none, else IconButton's */
{
    BOOL hot = g_hot == id; int rad = SC(12);
    BOOL glass = HOME_PAGES && primary && !hot;   /* Released: the main button too is dark glass until the mouse or the focus is on it (as PLAY, ui.c) */
    if (hot || (g_skin && primary && !glass)) Glow(g, (r.left + r.right) / 2, (r.top + r.bottom) / 2 + SC(4), (r.right - r.left) * 7 / 10, (r.bottom - r.top) + SC(8), A(primary ? SkinAcc() : RGB(255, 255, 255), primary ? (hot ? 92 : 52) : 30));
    if (glass) ShapeBox(g, r, rad, HP_GLASS, HP_GLASS_EDGE, 1.0f);
    else if (primary) ShapeBox(g, r, rad, A(hot ? RGB0(255, 255, 255) : BTN_FILL, 255), hot ? A(RGB0(255, 255, 255), 255) : 0, 2.0f);
    else ShapeBox(g, r, rad, A(RGB0(14, 15, 20), hot ? 196 : 158), A(RGB0(255, 255, 255), hot ? 230 : 40), hot ? 2.0f : 1.0f);   /* 6.2: dark glass: the words read over any art */
    COLORREF tc = primary && !glass ? BTN_INK : RGB(255, 255, 255); UINT32 ic = A(tc, 255);
    int is = SC(16); HFONT f = !primary || TextW(dc, g_fPlay, t) + (icon >= 0 ? is + SC(10) : 0) > r.right - r.left - SC(36) ? g_fBtn : g_fPlay;   /* 7.0 fix: only the main button speaks in the big letters */
    if (icon >= 0 && f == g_fBtn && TextW(dc, f, t) + is + SC(10) > r.right - r.left - SC(24)) icon = -1;   /* 7.0: a long word in a narrow pill (German's "Auf diesem PC suchen"): the words whole, without the drawing */
    int w = TextW(dc, f, t) + (icon >= 0 ? is + SC(10) : 0), x = (r.left + r.right - w) / 2, cy = (r.top + r.bottom) / 2;
    if (w > r.right - r.left - SC(16)) x = r.left + SC(10);
    if (icon >= 0) { int iy = cy - is / 2; switch (icon) { case 0: IconFolder(g, x, iy, is, ic); break; case 2: IconSearch(g, x, iy, is, ic); break; case 4: IconDownload(g, x, iy, is, ic); break; case 5: IconPlus(g, x, iy, is, ic); break; default: IconGear(g, x, iy, is, ic); } x += is + SC(10); }
    RECT tr = {x, r.top, r.right - SC(6), r.bottom}; TextR(dc, f, tc, tr, t, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    AddHit(r.left, r.top, r.right, r.bottom, id);
}
static void PaintHomeIn(HDC dc, GpGraphics *g)   /* Released: one page of the home (g_hpDraw); PaintHome, after it, turns the pages. With HOME_PAGES 0: the whole home of 11-12 */
{
    /* 6.0 (the user, 2026-11-07: "PORTHEON exactly like this interface" - a PS5 tutorial video - "with PORTHEON's logo;
       the way into the games too"): the console's home. One row of square tiles under the sections; the chosen one is
       bigger, with a white edge, and its name stands beside it; the row slides so the chosen tile stays among the first
       three (the tiles before it slide out to the left and still answer the arrows). The games ready to play come first,
       then the ones to install (a small download mark), and "Install" at the end, as the console's library tile. The
       game's art fills the screen; its name, one line of facts, PLAY in white and "..." for its options; four cards at the
       bottom say what the player needs to know, as the console's activity cards. */
    HomeOrder();
    if (HOME_PAGES && g_hpDraw == 1) {   /* Released review: page 2 is words and cards, not the clean art of page 1: its column goes darker (solid to x 300, then fading out before the video, a soft top under the bar; nothing at the video, so the video-only paint needs none) */
        RECT cl; GetClientRect(g_wnd, &cl); int x0 = -g_ox, xs = SC(300), xe = SC(VIT_X - 16), top = SC(64), tf = SC(36), bot = cl.bottom - g_oy + SC(HP_SLIDE);
        for (int s = 0; s <= 16; s++) {
            UINT32 vc = A(RGB0(0, 0, 0), (BYTE)(s ? 150 * (33 - 2 * s) / 32 : 150)); int sx = s ? xs + (s - 1) * (xe - xs) / 16 : x0, sw = (s ? xs + s * (xe - xs) / 16 : xs) - sx;
            VGrad(g, sx, top, sw, tf, A(RGB0(0, 0, 0), 0), vc); Fill(g, sx, top + tf, sw, bot - top - tf, vc);
        }
    }
    for (int k = 0; k < 4; k++) { g_cardKind[k] = 0; SetRectEmpty(&g_liveR[k]); }   /* 6.2: HomeSlot marks the live ones again */
    int selK = 0; for (int k = 0; k < NGAMES; k++) if (g_homeOrd[k] == g_sel) selK = k;
    if (g_csel >= 0) selK = NGAMES + g_csel;   /* 6.1: a custom game is shown: the row slides to its tile */
    int base = SC(HPV(68, 84)), grow = SC(HPV(40, 44)), gap = SC(HPV(10, 14)), cy = SC(HPV(136, 146)), first = selK > 2 ? selK - 2 : 0;   /* 7.0: tiles of 84, the chosen one 128 (y 82..210); Released (the two pages): 68 and 108, hung from y 82, 10 apart */
    int x = SC(MX + 12) - first * (base + gap);
    for (int k = 0; HP_ON(0) && k <= NGAMES; k++) {   /* (page 2 has no row) */
        if (k == NGAMES) {   /* "Install": the console's library tile */
            x = LibHomeTiles(dc, g, x, cy, base, grow, gap);   /* 6.1: the player's own games and "+ ADICIONAR" before it (ui_library.c) */
            int sz = base, y = TILE_TOP(cy, sz, base, grow); BOOL hot = g_hot == H_NAV + 2; RECT r = {x, y, x + sz, y + sz};
            if (Seen(r)) {
                if (HOME_PAGES) Box(g, r, TILE_RAD(sz), A(RGB0(14, 15, 20), 150), 0, 0);   /* Released: dark glass under it (the art is nearly clean now and showed through) */
                Box(g, r, TILE_RAD(sz), A(RGB(255, 255, 255), hot ? 46 : 20), A(RGB(255, 255, 255), hot ? 235 : 60), hot ? 2.0f : 1.0f);
                int is = sz * 30 / 100; IconDownload(g, x + (sz - is) / 2, y + sz * 22 / 100, is, A(RGB(255, 255, 255), 255));   /* 6.1: "+" is the Add tile now */
                RECT lr = {x, y + sz * 60 / 100, x + sz, y + sz * 86 / 100}; TextR(dc, g_fTag, RGB(236, 236, 240), lr, L2(L"INSTALAR", L"INSTALL"), DT_CENTER | DT_VCENTER);
            }
            AddHit(x, y, x + sz, y + sz, H_NAV + 2);
            break;
        }
        int gi = g_homeOrd[k]; BOOL ready = St(gi) == S_READY || St(gi) == S_RUNNING, sel = gi == g_sel && g_csel < 0;   /* 6.1: g_csel */
        float t = g_tileScale[gi]; int sz = base + (int)(grow * t), y = TILE_TOP(cy, sz, base, grow);
        RECT r = {x, y, x + sz, y + sz};
        if (x >= SC(MX + 12) - SC(4) && Seen((RECT){x - SC(24), y - SC(24), x + sz + SC(24), y + sz + SC(24)})) {   /* the tiles that slid out stay out of sight */
            if (sel) Glow(g, x + sz / 2, y + sz / 2 + SC(6), sz * 7 / 10, sz * 7 / 10, A(RGB(0, 0, 0), 150));
            if (sel && g_skin) Glow(g, x + sz / 2, y + sz / 2, sz, sz, A(g_skin->acc, 110));   /* 6.2: the chosen tile glows in its game's color */
            GameTile(dc, g, gi, x, y, sz, 0);
            if (SOON(gi)) SoonRibbon(dc, g, x, y, sz);   /* 6.2 */
            if (sel && !g_fxLight) {   /* 6.2: a light passes once over the chosen tile, as a console's focus does */
                float sp = (float)(int)(GetTickCount() - g_heroT0 - 120) / 820.0f;
                if (g_heroT0 && sp > 0.0f && sp < 1.0f) { GpPath *sp0 = RoundPath(x, y, sz - 1, sz - 1, TILE_RAD(sz)); CLIPPATH(g, sp0); VitSweep(g, r, Smooth(sp), 64); ClipBase(g); GdipDeletePath(sp0); }
            }
            if (!ready && !SOON(gi)) {   /* to install: a small download mark in the corner, the cover stays clear */
                int br = SC(11), bx = x + sz - br - SC(5), by2 = y + br + SC(5);   /* 6.1: top right (the bottom right covered the name on God of War III's and Bloodborne's covers) */
                Dot(g, bx, by2, br, A(RGB(0, 0, 0), 190)); IconDownload(g, bx - SC(6), by2 - SC(6), SC(12), A(RGB(255, 255, 255), 255));
            }   /* 6.2 owner: no "NOVO" badge on the tiles */
            if (sel) { float e = EaseOut3(MotionT(g_heroT0, 260)); RECT o = r; InflateRect(&o, SC(4), SC(4)); Box(g, o, TILE_RAD(sz) + SC(4), 0, A(RGB0(255, 255, 255), (BYTE)(140 + 115 * e)), (float)SC(2) + 0.5f); }   /* 7.0: a white line around it, a gap apart (the console's focus) */
            else if (g_hot == H_CARD + gi) Box(g, r, TILE_RAD(sz), 0, A(RGB(255, 255, 255), 190), 1.5f);
        }
        AddHit(x, y, x + sz, y + sz, H_CARD + gi);
        x += sz + gap;
        if (sel && HOME_PAGES) {   /* Released: its name in small letters after the tile, under the next ones (the row does not open for it) */
            float e = EaseOut3(MotionT(g_heroT0, 300));
            Text(dc, g_fText, Mix(RGB(0, 0, 0), RGB(255, 255, 255), e), x + SC(2), TILE_TOP(cy, sz, base, grow) + base + SC(13), G[gi].name);
        } else
        if (sel) {   /* the chosen game's name beside its tile, as the console shows it */
            float e = EaseOut3(MotionT(g_heroT0, 300)); int nw = TextW(dc, g_fCard, G[gi].name);
            Text(dc, g_fCard, Mix(RGB(0, 0, 0), RGB(255, 255, 255), e), x + SC(4), cy - SC(13), G[gi].name);
            x += nw + SC(30);
        }
    }

    if (g_csel >= 0) { PaintHomeCustom(dc, g); if (!HOME_PAGES) VitHome(dc, g); if (HP_ON(1)) { HomeLogsPill(dc, g, 752); PaintCredits(dc, g, 752); } return; }   /* 6.2: the vitrine too */   /* 6.1: a custom game is shown (ui_library.c) */
    /* the chosen game: its name big (it slides in from the left when the game changes), one line of facts */
    int gsel = g_sel, st = St(gsel); float he = EaseOut3(MotionT(g_heroT0, 420)); int tx = SC(HERO_L) - (int)(SC(18) * (1.0f - he)), ty = SC(262);   /* 7.0: the facts at 262, the name at 280, the chips at 358, PLAY at 400 */
    COLORREF wc = Mix(RGB(0, 0, 0), RGB(255, 255, 255), he), sc2 = Mix(RGB(0, 0, 0), RGB(200, 200, 210), he);
    wchar_t lab[96];
    if (ZELDAG(gsel)) _snwprintf(lab, 96, L"%s  ·  %s  ·  %s", MAKER(gsel), CONSOLE(gsel), L2(L"Emulador ou PC nativo", L"Emulator or native PC"));   /* 6.0 */
    else _snwprintf(lab, 96, L"%s  ·  %s  ·  %s  ·  %s", MAKER(gsel), G[gsel].year, CONSOLE(gsel), SOON(gsel) ? L2(L"Em breve", L"Coming soon") : X360G(gsel) ? L2(L"Port para PC", L"PC port") : L2(L"Port nativo para PC", L"Native PC port"));
#if HOME_PAGES
    /* Released (the two pages). Page 1: the name low on the left, in the game's own lettering, nothing above it. Page 2:
       the name small, the line of facts, the chips and the quick options, laid out upward from the cards (HP_CARDS_Y) */
    int tb = 0, dy = 0, p2chip = 0, p2pill = 0;
    if (HP_ON(0)) {
        if (g_skin) Glow(g, tx + SC(150), SC(HP_TITLE_Y) + SC(40), SC(300), SC(96), A(g_skin->acc, 36));
        tb = SkinTitle(dc, tx - SC(3), SC(HP_TITLE_Y), SC(64), SC(HERO_R) - tx, gsel, wc, Mix(RGB(0, 0, 0), SkinAcc(), he));
        dy = tb + SC(10) - SC(HP_STAT_Y); if (dy < 0) dy = 0; if (dy > SC(16)) dy = SC(16);   /* (the tall faces: the line and the buttons move down with the name) */
    } else {
        int rows = QuickRows(dc, gsel, SC(HERO_L), SC(HERO_R));
        p2pill = SC(HP_CARDS_Y - 20 - 34) - (rows > 1 ? SC(42) : 0); p2chip = rows ? p2pill - SC(12 + 26) : SC(HP_CARDS_Y - 20 - 26);
        Text(dc, g_fSmall, sc2, tx, p2chip - SC(30), lab);
        SkinTitle(dc, tx - SC(2), p2chip - SC(30 + 52), SC(36), SC(HERO_R) - tx, gsel, wc, Mix(RGB(0, 0, 0), SkinAcc(), he));
    }
    (void)ty;
#else
    Text(dc, g_fSmall, sc2, tx, ty, lab);
    if (g_skin) Glow(g, tx + SC(150), ty + SC(66), SC(300), SC(96), A(g_skin->acc, 44));
    int tb = SkinTitle(dc, tx - SC(3), ty + SC(18), SC(64), SC(HERO_R) - tx, gsel, wc, Mix(RGB(0, 0, 0), SkinAcc(), he));   /* 7.0: no bar above the name */
    int dy = tb + SC(12) - SC(HOME_CHIP_Y); if (dy < 0) dy = 0; if (dy > SC(20)) dy = SC(20);   /* 7.0 fix: the tall faces (UFC 5, Fight Night, Bloodborne) touched the chips: what is under the name moves down with it, up to 20 */
    int p2chip = 0, p2pill = 0;
#endif
    wchar_t info[300], pt[128]; FmtPlayTime(gsel, pt, 128);
    { const GameVer *hv = st == S_READY ? GameVerOf(gsel) : NULL;   /* 6.0: the game's own version on the home screen too */
      if (hv && hv->tag[0] && g_ver[gsel][0]) _snwprintf(info, 300, L"%s  ·  %s  ·  %s %s  ·  %s", StatusText(st), g_ver[gsel], L2(L"jogo", L"game"), hv->tag, pt);
      else if (hv && hv->tag[0]) _snwprintf(info, 300, L"%s  ·  %s %s  ·  %s", StatusText(st), L2(L"jogo", L"game"), hv->tag, pt);   /* 6.1 review: no bare "?" */
      else if (g_ver[gsel][0]) _snwprintf(info, 300, L"%s  ·  %s  ·  %s", StatusText(st), g_ver[gsel], pt);
      else _snwprintf(info, 300, L"%s  ·  %s", StatusText(st), pt); }   /* 6.1: no "?" for a port not installed yet */
    (void)info;   /* 6.2: the facts as chips (home62.c), the same line as before */
    if (HP_ON(1)) HomeChips(dc, g, gsel, st, tx, HOME_PAGES ? p2chip : SC(HOME_CHIP_Y) + dy, SC(HERO_R), he);
    BOOL say = st != S_READY || g_proc || SOON(gsel);   /* 7.0: a game ready to play needs no sentence (the console shows none): only what is missing, running or to come */
    if (HP_ON(0) && (say || HOME_PAGES)) {   /* what PLAY does now, in one plain sentence; Released (page 1): always one line: that sentence, or "ready to play" and the time played */
        wchar_t what[260];
        if (!say) { wchar_t pk[48]; _snwprintf(pk, 48, L"playtime_%s", G[gsel].id);
                    if (PIni(pk, 0) >= 60) _snwprintf(what, 260, L"%s  ·  %s", StatusText(st), pt); else _snwprintf(what, 260, L"%s", StatusText(st)); what[259] = 0; }
        else if (SOON(gsel)) _snwprintf(what, 260, L"%s", L2(L"Em breve no PORTHEON. Enquanto isso, veja o trailer aqui ao lado.", L"Coming soon to PORTHEON. Meanwhile, watch the trailer right here."));
        else if (g_proc) _snwprintf(what, 260, L2(L"%s está aberto. Parar fecha o jogo.", L"%s is open. Stop closes the game."), G[gsel].name);
        else if (st == S_NOPORT && NGPORTG(gsel)) _snwprintf(what, 260, L"%s", FNCG(gsel) ? L2(L"Aperte \"Instalar o port\": o instalador transforma a SUA .iso num programa de PC.", L"Press \"Install the port\": the installer turns YOUR .iso into a PC program.") : BBG(gsel) ? L2(L"Aperte \"Instalar o port\": o GitHub oficial, o seu .zip ou a pasta já extraída.", L"Press \"Install the port\": the official GitHub, your .zip or the extracted folder.")
                                                                                   : L2(L"Aperte \"Instalar o port\": o seu .zip ou a pasta já extraída.", L"Press \"Install the port\": your .zip or the extracted folder."));   /* 6.1 (one line: the port is not in PORTHEON) */
        else if (st == S_PICK) _snwprintf(what, 260, L"%s", L2(L"Falta o jogo neste PC: siga os passos de instalação ou mostre a pasta onde ele está.", L"The game is not on this PC yet: follow the install steps or show the folder it is in."));
        else if (st != S_READY) _snwprintf(what, 260, L"%s", StatusText(st));
        else if (X360G(gsel)) _snwprintf(what, 260, L2(L"Jogar abre o %s a 60 FPS como %s. Vídeo e controles ficam em Configurar o port.", L"Play opens %s at 60 FPS as %s. Video and controls are in Configure the port."), G[gsel].name, CurProf()->name);
        else if (ZELDAG(gsel)) { int ze = ZeldaEngine(); _snwprintf(what, 260, ze == 3 ? L2(L"Jogar abre o %s nativo no PC (Ocarina Recomp), sem emulador.", L"Play opens the native PC %s (Ocarina Recomp), with no emulator.")
                                                                                      : L2(L"Jogar abre o %s no emulador Lichii %s, com as opções escolhidas aqui.", L"Play opens %s on the Lichii %s emulator, with the options picked here."), G[gsel].name, ze == 2 ? L"Frizti v3" : L"MDias v4"); }   /* 6.0 */
        else if (NGPORTG(gsel)) _snwprintf(what, 260, L"%s", NgPlayWhat(gsel));   /* 6.1 */
        else if (GOW3G(gsel)) _snwprintf(what, 260, L2(L"Jogar abre o %s pelo port nativo para PC. Vídeo e controles ficam no launcher do port.", L"Play opens %s through the native PC port. Video and controls are in the port's launcher."), G[gsel].name);   /* 6.0 */
        else _snwprintf(what, 260, L2(L"Jogar abre o %s %s, como %s, com as configurações escolhidas para este PC.", L"Play opens %s %s as %s, with the settings picked for this PC."), G[gsel].name,
                        GetInt(gsel, G[gsel].ps5 ? L"fullscreen" : L"fullscreen", 1) ? L2(L"em tela cheia sem bordas", L"in borderless full screen") : L2(L"em janela", L"in a window"), CurProf()->name);   /* 6.0 final: every UFC engine's full screen is SDL's borderless desktop one */
        RECT wr = {tx, SC(HPV(HP_STAT_Y, 476)) + dy, SC(HERO_R), SC(HPV(HP_STAT_Y + 24, 500)) + dy};
        if (HOME_PAGES) { Dot(g, tx + SC(5), (wr.top + wr.bottom) / 2 + SC(1), SC(4), A(st == S_READY || st == S_RUNNING ? C_GOOD : st == S_PICK || st == S_NOPORT ? C_ORANGE : C_BAD, (BYTE)(255 * he))); wr.left += SC(18); }   /* (the chip's dot, HomeChips) */
        TextR(dc, g_fText, Mix(RGB(0, 0, 0), RGB(232, 232, 238), he), wr, what, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);   /* 6.2: up to the vitrine */
    }

    /* PLAY in white and "..." for the game's options, as the console's game hub; for a game still to install, its steps */
    tx = SC(HERO_L); int by = SC(HPV(HP_BTN_Y, 400)) + dy, bh = SC(HPV(54, 60));   /* 7.0: the buttons are pills 60 high (y 400..460); the column ends at the video (x 644); Released: 54 high, at 610 */
    if (!HP_ON(0)) { (void)by; (void)bh; }   /* (page 2 has no buttons) */
    else if (SOON(gsel)) {   /* 6.2: nothing to press yet */
        RECT b1 = {tx, by, tx + SC(280), by + bh}; Box(g, b1, bh / 2, A(RGB(255, 72, 164), 255), 0, 0.0f);
        TextR(dc, g_fCard, RGB(255, 255, 255), b1, L2(L"EM BREVE", L"COMING SOON"), DT_CENTER | DT_VCENTER);
    } else if (st == S_NOPORT && NGPORTG(gsel)) {   /* 6.1: the port is installed from inside */
        RECT b1 = {tx, by, tx + SC(280), by + bh}; Ps5Button(dc, g, b1, L2(L"Instalar o port", L"Install the port"), H_EXTINST, TRUE, 4);
        RECT b2 = {b1.right + SC(14), by, b1.right + SC(14) + SC(210), by + bh}; Ps5Button(dc, g, b2, L2(L"Como instalar", L"How to install"), H_GUIDE, FALSE, 0);   /* 7.0 fix: 210 (250), in the small letters of every second button */
    } else if (st == S_PICK) {
        BOOL scan = !X360G(gsel) && !ZELDAG(gsel) && !NGPORTG(gsel);   /* 5.0: the scan finds PS4/PS5 dumps only */
        int w1 = scan ? 220 : 250, w2 = scan ? 190 : 210, w3 = 170;   /* 7.0: three buttons share the 604 units left of the video (250 + 210 + 200 ran under it) */
        const wchar_t *t3 = L2(L"Procurar no PC", L"Find on this PC");
        if (scan) { int need = MulDiv(TextW(dc, g_fBtn, t3), 96, g_dpi) + 40, d = need - w3; if (d > 50) d = 50; if (d > 0) { w3 += d; w1 -= d; } }   /* a longer language (German): the third pill takes up to 50 units from the first */
        RECT b1 = {tx, by, tx + SC(w1), by + bh}; Ps5Button(dc, g, b1, L2(L"Instalar", L"Install"), H_GUIDE, TRUE, 4);
        const wchar_t *t2 = L2(L"Escolher a pasta", L"Choose the folder"); int icw = SC(16) + SC(10) + SC(24);   /* 7.0 fix: the second and third pills keep their drawings together or lose them together (German kept one of the two) */
        BOOL ico = TextW(dc, g_fBtn, t2) + icw <= SC(w2) && (!scan || TextW(dc, g_fBtn, t3) + icw <= SC(w3));
        RECT b2 = {b1.right + SC(scan ? 12 : 14), by, b1.right + SC(scan ? 12 : 14) + SC(w2), by + bh}; Ps5Button(dc, g, b2, t2, H_BROWSE, FALSE, ico ? 0 : -1);
        if (scan) { RECT b3 = {b2.right + SC(12), by, b2.right + SC(12) + SC(w3), by + bh}; Ps5Button(dc, g, b3, t3, H_SCAN, FALSE, ico ? 2 : -1); }
    } else {
        RECT b1 = {tx, by, tx + SC(HPV(232, 248)), by + bh}; PlayButton(dc, g, b1, NULL);
        RECT b2 = {b1.right + SC(14), by, b1.right + SC(14) + bh, by + bh}; BOOL oh = g_hot == H_OPTIONS;   /* "...": a glass circle */
        int ocx = (b2.left + b2.right) / 2, ocy = (b2.top + b2.bottom) / 2;
        if (oh) Glow(g, ocx, ocy, SC(44), SC(44), A(RGB0(255, 255, 255), 30));
        Dot(g, ocx, ocy, bh / 2, A(RGB0(14, 15, 20), oh ? 196 : 158)); Ring(g, ocx, ocy, bh / 2 - 1, A(RGB0(255, 255, 255), oh ? 230 : 40), oh ? 2.0f : 1.0f);
        for (int q = -1; q <= 1; q++) Dot(g, ocx + q * SC(9), ocy, SC(3), A(RGB0(255, 255, 255), 255));
        AddHit(b2.left, b2.top, b2.right, b2.bottom, H_OPTIONS);
    }
    if (HP_ON(1)) QuickRow(dc, g, gsel, SC(HERO_L), HOME_PAGES ? p2pill : SC(say ? HOME_QUICK_Y + 34 : HOME_QUICK_Y) + dy, SC(HERO_R));   /* 6.2: the quick options (glass pop-overs); 7.0: under PLAY, or under the sentence when there is one */
    if (!HOME_PAGES) VitHome(dc, g);   /* 6.2: the vitrine (Released: PaintHome draws it, still, under both pages) */
    if (!HP_ON(1)) return;   /* (page 1 ends here: the logs pill, the cards and the credits are page 2's) */
    HomeLogsPill(dc, g, 752);
    if (SOON(gsel)) { PaintCredits(dc, g, 752); return; }   /* 6.2: no cards: there is nothing to set up yet */

    /* the four cards at the bottom, apart, as the console's activity cards (no strip behind them) */
    RECT tape = {SC(MX + 12) - SC(7), SC(TAPE_Y), SC(MX + 12) - SC(7) + 4 * ((SC(CW - 24) + SC(14) + 3) / 4), SC(TAPE_Y + TAPE_H)};
    /* the cards */
    int cyTop = tape.top, cw = (tape.right - tape.left) / 4, cx0 = tape.left;
    wchar_t a1[160], b1[160], a2[160], b2[160], a3[160], b3[160], a4[160], b4[160];
    {   int fl = GetInt(gsel, L"fps_limit", 60), res = GetInt(gsel, L"resolution", 1); wchar_t fn[48]; FpsName(fl, FALSE, fn, 48);
        static const wchar_t *const rs[4] = {L"1280x720", L"1920x1080", L"2560x1440", L"3840x2160"};
        _snwprintf(a1, 160, L"%s  ·  %s", fn, PS4G(gsel) && GetInt(gsel, L"pgame:ultrawide", 0) ? L"3440x1440" : rs[res >= 0 && res < 4 ? res : 1]);
        int tier = PcTier(); _snwprintf(b1, 160, L"%s", tier == 2 ? L2(L"PC forte: tudo no máximo", L"Strong PC: everything up") : tier == 1 ? L2(L"PC fraco: ajustado para ficar liso", L"Weak PC: tuned to run smooth") : L2(L"PC muito fraco: modo leve", L"Very weak PC: light mode")); }
    if (EXTG(gsel)) {   /* 6.0: Zelda and God of War III: how they play, the player's game, the extra step, the first time */
        const GameVer *gv = GameVerOf(gsel);
        if (ZELDAG(gsel)) {
            int ze = ZeldaEngine(); wchar_t rr[MAX_PATH]; BOOL nat = ZeldaRecompRoot(rr);
            _snwprintf(a1, 160, L"%s", ze == 3 ? L2(L"PC nativo (Ocarina Recomp)", L"Native PC (Ocarina Recomp)") : ze == 2 ? L"Lichii Frizti v3" : L"Lichii MDias v4");
            _snwprintf(b1, 160, L"%s", ze == 3 ? L2(L"Sem emulador: roda direto no PC", L"No emulator: runs straight on the PC") : L2(L"Dynamic FPS e correções AMD inclusos", L"Dynamic FPS and AMD fixes included"));
            _snwprintf(a2, 160, L"%s", gv->tag[0] ? gv->tag : L2(L"Escolha o arquivo do jogo", L"Choose the game file")); _snwprintf(b2, 160, L"%s", L2(L"Seu .nup, .nsp, .xci ou pasta", L"Your .nup, .nsp, .xci or folder"));
            _snwprintf(a3, 160, L"%s", nat ? L2(L"Jogo nativo pronto", L"Native game ready") : L"Ocarina Setup 0.2.1"); _snwprintf(b3, 160, L"%s", nat ? rr : L2(L"Recompila a versão 1.1.1 (30 min a 2 h)", L"Recompiles version 1.1.1 (30 min to 2 h)"));
            { int mb = ZeldaShaderCacheMb(); _snwprintf(a4, 160, L"%s", mb > 50 ? L2(L"Cache de shaders instalado", L"Shader cache installed") : L2(L"Monta os shaders ao abrir", L"Builds its shaders at start")); }
            _snwprintf(b4, 160, L"%s", L2(L"Áreas novas podem engasgar 1 vez", L"New areas may stutter once"));
        } else if (NGPORTG(gsel)) NgCards(gsel, a1, b1, a2, b2, a3, b3, a4, b4);   /* 6.1 */
        else {
            _snwprintf(a1, 160, L"%s", L2(L"Port nativo (gow3_pc)", L"Native port (gow3_pc)")); _snwprintf(b1, 160, L"%s", g_ver[gsel][0] ? g_ver[gsel] : L"God of War III Remastered");
            _snwprintf(a2, 160, L"%s", gv->tag[0] ? gv->tag : L2(L"Escolha a pasta do jogo", L"Choose the game folder")); _snwprintf(b2, 160, L"%s", L2(L"PS4 CUSA01623 com o update 01.02", L"PS4 CUSA01623 with update 01.02"));
            _snwprintf(a3, 160, L"%s", L2(L"Launcher do port", L"The port's launcher")); _snwprintf(b3, 160, L"%s", L2(L"Vídeo, FSR, controles e mods", L"Video, FSR, controls and mods"));
            _snwprintf(a4, 160, L"%s", L2(L"Prepara o jogo uma vez", L"Prepares the game once")); _snwprintf(b4, 160, L"%s", L2(L"Alguns minutos na primeira vez", L"A few minutes the first time"));
        }
        RECT r0 = {cx0, cyTop, cx0 + cw, cyTop + SC(TAPE_H)}; HomeCard(dc, g, r0, 0, 0, L2(L"Como joga", L"How it plays"), a1, b1, C_CYAN);
        RECT r1 = {r0.right, cyTop, r0.right + cw, cyTop + SC(TAPE_H)}; HomeCard(dc, g, r1, 1, 4, L2(L"Seu jogo", L"Your game"), a2, b2, C_GOOD);
        RECT r2 = {r1.right, cyTop, r1.right + cw, cyTop + SC(TAPE_H)}; HomeCard(dc, g, r2, 2, ZELDAG(gsel) ? 5 : NGPORTG(gsel) && !PortOk(gsel) ? 7 : 6, ZELDAG(gsel) ? L2(L"PC nativo", L"Native PC") : NGPORTG(gsel) && !PortOk(gsel) ? L2(L"Instalar o port", L"Install the port") : L2(L"Configurar o port", L"Configure the port"), a3, b3, C_PURPLE);
        RECT r3 = {r2.right, cyTop, r2.right + cw, cyTop + SC(TAPE_H)}; HomeSlot(dc, g, r3, 3, St(gsel) == S_READY ? 3 : 0, gsel, 2, L2(L"Primeira vez", L"First time"), a4, b4);   /* 6.2: the last session once played */
        PaintCredits(dc, g, 752);
        return;
    }
    if (X360G(gsel)) {   /* 5.0: UFC Undisputed 3: what its port does by itself, and where its own settings are */
        _snwprintf(a1, 160, L"%s", L2(L"60 FPS fixos pelo port", L"60 FPS, fixed by the port")); _snwprintf(b1, 160, L"%s", L"UFC3MONS BETA 1.3");
        _snwprintf(a2, 160, L"%s", L2(L"Vídeo, controles, saves e mods", L"Video, controls, saves and mods")); _snwprintf(b2, 160, L"%s", L2(L"Na janela do próprio port", L"In the port's own window"));
        _snwprintf(a3, 160, L"%s", L2(L"Original (ISO) ou Reborn", L"Original (ISO) or Reborn")); _snwprintf(b3, 160, L"%s", L2(L"Escolha a pasta em Arquivos do jogo", L"Pick the folder in Game files"));
        _snwprintf(a4, 160, L"%s", L2(L"Prepara os shaders uma vez", L"Prepares its shaders once")); _snwprintf(b4, 160, L"%s", L2(L"Alguns minutos na primeira vez", L"A few minutes the first time"));
        RECT r0 = {cx0, cyTop, cx0 + cw, cyTop + SC(TAPE_H)}; HomeCard(dc, g, r0, 0, 0, L2(L"Desempenho", L"Performance"), a1, b1, C_CYAN);
        RECT r1 = {r0.right, cyTop, r0.right + cw, cyTop + SC(TAPE_H)}; HomeCard(dc, g, r1, 1, 6, L2(L"Configurar o port", L"Configure the port"), a2, b2, C_PURPLE);
        RECT r2 = {r1.right, cyTop, r1.right + cw, cyTop + SC(TAPE_H)}; HomeCard(dc, g, r2, 2, 4, L2(L"Seu jogo", L"Your game"), a3, b3, C_GOOD);
        RECT r3 = {r2.right, cyTop, r2.right + cw, cyTop + SC(TAPE_H)}; HomeSlot(dc, g, r3, 3, 3, gsel, 2, L2(L"Primeira partida", L"First session"), a4, b4);
        PaintCredits(dc, g, 752);
        return;
    }
    if (G[gsel].ps5) {   /* 4.0.2: UFC 5 had only the performance card (no saves, cheats or online in PORTHEON); 4.2: UFC 6 too */
        /* 4.3.6 (the user: "a new face, professional and direct, that a layperson understands"): the four cards say in
           plain words what the player gets - the picture preset, whether this PC is ready, the picture's sync, and
           whether the first fight will stutter - instead of priority classes, present modes and shader megabytes */
        {   int gp = GetInt(gsel, L"pgame:gfx_preset", 0); int gi = OptIndex(&F_GFX, gp); _snwprintf(a1, 160, L"%s", OptName(&GFXP[gi < 0 ? 0 : gi]));
            int bf = GetInt(gsel, L"bench_fps", 0);
            if (UFC6G(gsel) && bf > 0) _snwprintf(b1, 160, L2(L"Testado neste PC: cerca de %d FPS na luta", L"Tested on this PC: about %d FPS in a fight"), bf);
            else _snwprintf(b1, 160, L"%s", L2(L"Mude em Vídeo > Preset gráfico", L"Change it in Video > Graphics preset")); }
        int tier = PcTier();
        {   /* "AMD Ryzen 5 5500 + NVIDIA GeForce RTX 4060" -> "Ryzen 5 5500 + RTX 4060": the card's width holds it */
            static const wchar_t *const cut[] = {L"AMD ", L"Intel(R) Core(TM) ", L"Intel(R) ", L"NVIDIA GeForce ", L"NVIDIA ", L"AMD Radeon(TM) ", L"Intel(R) Arc(TM) "};
            wchar_t c[80], v[80]; _snwprintf(c, 80, L"%s", g_cpuName[0] ? g_cpuName : L"CPU"); _snwprintf(v, 80, L"%s", g_haveGpu ? g_gpu.name : L"");
            for (int i = 0; i < (int)(sizeof(cut) / sizeof(cut[0])); i++) {
                size_t n = wcslen(cut[i]);
                if (!wcsncmp(c, cut[i], n)) memmove(c, c + n, (wcslen(c + n) + 1) * sizeof(wchar_t));
                if (!wcsncmp(v, cut[i], n)) memmove(v, v + n, (wcslen(v + n) + 1) * sizeof(wchar_t));
            }
            wchar_t *at = wcsstr(c, L" with "); if (at) *at = 0;   /* "Ryzen 5 5600G with Radeon Graphics" */
            at = wcsstr(c, L" CPU @"); if (at) *at = 0;             /* "i5-10400F CPU @ 2.90GHz" */
            for (size_t n = wcslen(c); n && c[n - 1] == L' '; ) c[--n] = 0;   /* the CPUID brand string ends in padding */
            for (size_t n = wcslen(v); n && v[n - 1] == L' '; ) v[--n] = 0;
            _snwprintf(a2, 160, L"%s%s%s", c, v[0] ? L"  +  " : L"", v); }
        _snwprintf(b2, 160, L"%s", tier == 2 ? L2(L"Pronto para jogar no máximo", L"Ready to play at the top settings") : tier == 1 ? L2(L"Pronto, com ajustes para ficar liso", L"Ready, tuned to run smooth") : L2(L"Pronto no modo leve", L"Ready in light mode"));
        if (UFC5G(gsel)) {
            _snwprintf(a3, 160, L"%s", GetInt(gsel, L"present", 2) == 2 ? L2(L"Modo liso: 60/30 FPS estáveis, sem cortes", L"Smooth mode: steady 60/30 FPS, no tearing") : L2(L"Sem VSync: o máximo de FPS", L"No VSync: the most FPS"));   /* 5.2: smooth is UFC 5's default */
            _snwprintf(b3, 160, L"%s", GetInt(gsel, L"crowd_off", 1) ? L2(L"Torcida desligada (mais FPS)", L"Crowd off (more FPS)") : L2(L"Torcida ligada", L"Crowd on"));
        } else {
            int pm = GetInt(gsel, L"present", 0);   /* 4.3.6: Immediate is the default */
            _snwprintf(a3, 160, L"%s", pm == 2 ? L2(L"VSync ligado: sem cortes na imagem", L"VSync on: no tearing") : pm ? L2(L"Mailbox: sem cortes, sem limite", L"Mailbox: no tearing, uncapped") : L2(L"Sem VSync: o máximo de FPS", L"No VSync: the most FPS"));
            /* 6.1: UFC 1-4 keep the switch in portheon.ini (pgame:cam3p), and their ports take Select / View too */
            _snwprintf(b3, 160, L"%s", !GetInt(gsel, PS4G(gsel) ? L"pgame:cam3p" : L"cam3p", 0) ? L2(L"Mude em Vídeo, se quiser", L"Change it in Video if you like") :
                                      gsel == 1 ? L2(L"Câmera por cima do ombro: Select na luta em pé", L"Over-the-shoulder camera: Select in a stand-up fight") :
                                      PS4G(gsel) ? L2(L"Câmera de TV: Select na luta em pé", L"TV camera: Select in a stand-up fight") :
                                      L2(L"Câmera de TV: touchpad na luta em pé", L"TV camera: touchpad in a stand-up fight"));
        }
        if (UFC6G(gsel)) {   /* the engine's shader journal: what is built again before the game asks for it */
            wchar_t jp[MAX_PATH], pd[MAX_PATH]; PortDir(gsel, pd); _snwprintf(jp, MAX_PATH, L"%s\\bin\\_PipelineCache\\%s.shaders", pd, G[gsel].tid);
            WIN32_FILE_ATTRIBUTE_DATA fa; static DWORD jAt; static double jMb = -1;
            if (GetTickCount() - jAt > 5000 || jMb < 0) { jAt = GetTickCount(); jMb = GetFileAttributesExW(jp, GetFileExInfoStandard, &fa) ? (((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow) / 1048576.0 : 0; }
            _snwprintf(a4, 160, L"%s", jMb > 0.05 ? L2(L"Pronta: sem engasgos", L"Ready: no stutter") : L2(L"A 1ª luta pode engasgar um pouco", L"The 1st fight may stutter a little"));
        } else _snwprintf(a4, 160, L"%s", L2(L"A 1ª luta pode engasgar um pouco", L"The 1st fight may stutter a little"));
        _snwprintf(b4, 160, L"%s", L2(L"Os efeitos ficam salvos neste PC", L"The effects stay saved on this PC"));
        RECT r0 = {cx0, cyTop, cx0 + cw, cyTop + SC(TAPE_H)}; HomeCard(dc, g, r0, 0, 0, L2(L"Qualidade", L"Quality"), a1, b1, C_CYAN);
        RECT r1 = {r0.right, cyTop, r0.right + cw, cyTop + SC(TAPE_H)}; HomeSlot(dc, g, r1, 1, 1, gsel, 5, L2(L"Seu PC", L"Your PC"), a2, b2);   /* 6.2: live */
        RECT r2 = {r1.right, cyTop, r1.right + cw, cyTop + SC(TAPE_H)}; HomeSlot(dc, g, r2, 2, 2, gsel, 6, L2(L"Imagem", L"Picture"), a3, b3);
        RECT r3 = {r2.right, cyTop, r2.right + cw, cyTop + SC(TAPE_H)}; HomeSlot(dc, g, r3, 3, 3, gsel, 2, L2(L"Primeira luta", L"First fight"), a4, b4);
        PaintCredits(dc, g, 752);
        return;
    }
    {   static SaveInfo sv[24]; static int nsv = -1, svGame = -1, svProf = -1; static DWORD svAt;
        if (svGame != gsel || svProf != g_curProf || GetTickCount() - svAt > 5000) { nsv = SaveList(gsel, sv, 24); svGame = gsel; svProf = g_curProf; svAt = GetTickCount(); }
        if (nsv > 0) { SYSTEMTIME s; FILETIME lt; FileTimeToLocalFileTime(&sv[0].mt, &lt); FileTimeToSystemTime(&lt, &s);
                       _snwprintf(a2, 160, L2(L"%d save(s)  ·  último: %02d/%02d %02d:%02d", L"%d save(s)  ·  latest: %02d/%02d %02d:%02d"), nsv, s.wDay, s.wMonth, s.wHour, s.wMinute); }
        else _snwprintf(a2, 160, L"%s", L2(L"Nenhum save ainda neste perfil", L"No saves yet in this profile"));
        _snwprintf(b2, 160, L"%s", PIni(L"save_autobackup", 1) ? L2(L"Backup automático ligado", L"Automatic backup on") : L2(L"Backup automático desligado", L"Automatic backup off")); }
    {   CheatsLoad(gsel); int on = CheatsOn(gsel);
        if (g_ncheat > 0) { _snwprintf(a3, 160, L2(L"%d disponíveis  ·  %d ligadas", L"%d available  ·  %d on"), g_ncheat, on); _snwprintf(b3, 160, L"%s", L2(L"Só no modo offline", L"Offline modes only")); }
        else { _snwprintf(a3, 160, L"%s", L2(L"Ainda não há trapaças para este jogo", L"No cheats for this game yet")); _snwprintf(b3, 160, L"%s", L2(L"Veja o UFC 4", L"See UFC 4")); } }
    if (gsel == 2) { _snwprintf(a4, 160, L"%s", L2(L"Online com amigos (servidor do port)", L"Online with friends (the port's server)")); _snwprintf(b4, 160, L"%s", L2(L"Hospedar ou entrar numa sala", L"Host or join a room")); }
    else { _snwprintf(a4, 160, L"%s", ARENA_GAME(gsel) ? L2(L"Lutas online no PORTHEON ARENA", L"Online fights in PORTHEON ARENA") : L2(L"Jogue com um amigo pelo Parsec", L"Play with a friend through Parsec"));   /* 7.2 */ _snwprintf(b4, 160, L"%s", GetInt(gsel, L"parsec", 0) ? L2(L"Modo Parsec ligado", L"Parsec mode on") : L2(L"Modo Parsec desligado", L"Parsec mode off")); }
    RECT r0 = {cx0, cyTop, cx0 + cw, cyTop + SC(TAPE_H)}; HomeCard(dc, g, r0, 0, 0, L2(L"Desempenho", L"Performance"), a1, b1, C_CYAN);
    RECT r1 = {r0.right, cyTop, r0.right + cw, cyTop + SC(TAPE_H)}; HomeSlot(dc, g, r1, 1, 1, gsel, 1, TR(L"Saves", L"Saves"), a2, b2);   /* 6.2: live (saves and cheats are quick options) */
    RECT r2 = {r1.right, cyTop, r1.right + cw, cyTop + SC(TAPE_H)}; HomeSlot(dc, g, r2, 2, 2, gsel, 2, L2(L"Trapaças", L"Cheats"), a3, b3);
    RECT r3 = {r2.right, cyTop, r2.right + cw, cyTop + SC(TAPE_H)}; HomeSlot(dc, g, r3, 3, 3, gsel, 3, TR(L"Online", L"Online"), a4, b4);
    PaintCredits(dc, g, 752);
}
#if HOME_PAGES
/* Released: the way to the other page: "Mais" at the bottom centre of page 1, "Voltar" at the top centre of page 2 */
static void HomeMoreHint(HDC dc, GpGraphics *g, int page)
{
    const wchar_t *t = page ? L2(L"Voltar", L"Back") : L2(L"Mais", L"More"); BOOL hot = g_hot == H_HOMEMORE;
    int tw = TextW(dc, g_fSmall, t), w = tw + SC(50), h = SC(28), x = SC(BASE_W / 2) - w / 2, y = SC(page ? 82 : 756);
    RECT r = {x, y, x + w, y + h};
    Box(g, r, h / 2, A(RGB0(14, 15, 20), hot ? 196 : 120), A(RGB0(255, 255, 255), hot ? 230 : 40), hot ? 1.6f : 1.0f);
    Chevron(g, x + SC(19), y + h / 2 + (page ? -SC(1) : SC(1)), SC(4), page ? 3 : 0, A(RGB0(255, 255, 255), hot ? 255 : 210));
    RECT tr = {x + SC(32), y, r.right - SC(8), r.bottom}; TextR(dc, g_fSmall, hot ? RGB0(255, 255, 255) : RGB0(220, 220, 228), tr, t, DT_LEFT | DT_VCENTER);
    AddHit(r.left, r.top, r.right, r.bottom, H_HOMEMORE);
}
static void LibHomeCover(HDC dc, GpGraphics *g);   /* ui_library.c */
int WINAPI GdipFlush(GpGraphics *, int);
static void FocusDefault(void);                    /* ui_input.c */
#endif
/* Released: the home screen (HOME_PAGES, ui.c). One page is drawn at a time; while the page changes the one that leaves
   moves away and fades into the art (the background drawn over it), then the new one arrives the same way. The video is
   drawn last and still: both pages keep its rectangle free, so the web view (a child window) is never moved or hidden. */
static void PaintHome(HDC dc, GpGraphics *g)
{
#if HOME_PAGES
    float t = MotionT(g_pageT0, HP_SLIDE_MS), veil = 0.0f; int sh = 0, n0 = g_nhits;
    g_hpDraw = g_homePage;
    if (t < 1.0f) {
        int dir = g_homePage ? -1 : 1;   /* to page 2 the home moves up; back to page 1, down */
        if (t < 0.45f) { float e = t / 0.45f; g_hpDraw = !g_homePage; sh = dir * (int)(SC(HP_SLIDE) * e * e); veil = e; }
        else { float e = EaseOut3((t - 0.45f) / 0.55f); sh = -dir * (int)(SC(HP_SLIDE) * (1.0f - e)); veil = 1.0f - e; }
    }
    /* while the page changes: what is under the top bar (the art, already drawn) is kept aside, the page is drawn moved
       and clipped under the bar, and the kept picture is blended back over it (GDI's AlphaBlend: a few ms at 1080p; the
       same fade through GDI+ took 35 ms a frame) */
    static HBITMAP keep; static int keepW, keepH;
    RECT cl; GetClientRect(g_wnd, &cl); int cut = g_oy + SC(74), sv = 0; HDC kdc = NULL; HGDIOBJ kold = NULL;
    RECT pr0 = g_paintRect; HRGN rg0 = g_paintRgn;
    if (t < 1.0f && cl.bottom > cut) {
        if (!keep || keepW != cl.right || keepH != cl.bottom) {
            if (keep) DeleteObject(keep);
            BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), cl.right, -cl.bottom, 1, 32, BI_RGB}}; void *bits = NULL;
            keep = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0); keepW = cl.right; keepH = cl.bottom;
        }
        if (keep && (kdc = CreateCompatibleDC(dc)) != NULL) { GdipFlush(g, 1); kold = SelectObject(kdc, keep); BitBlt(kdc, 0, cut, cl.right, cl.bottom - cut, dc, 0, cut, SRCCOPY); }
        sv = SaveDC(dc); IntersectClipRect(dc, 0, cut, cl.right, cl.bottom);   /* the text */
        g_paintRgn = NULL; if (g_paintRect.top < cut) g_paintRect.top = cut;    /* GDI+ (ClipBase) */
    } else if (t >= 1.0f && keep) { DeleteObject(keep); keep = NULL; }
    if (sh) { g_oy += sh; GdipTranslateWorldTransform(g, 0.0f, (float)sh, 0); }   /* (GDI+ by its transform, the text by g_oy; the hit regions are kept unmoved) */
    if (sv) ClipBase(g);
    PaintHomeIn(dc, g);
    if (sh) { g_oy -= sh; GdipTranslateWorldTransform(g, 0.0f, (float)-sh, 0); }
    if (sv) { RestoreDC(dc, sv); g_paintRect = pr0; g_paintRgn = rg0; ClipBase(g); }
    if (g_hpDraw != g_homePage) g_nhits = n0;   /* the page that leaves takes no click and no focus */
    if (kdc) {
        GdipFlush(g, 1);
        if (veil > 0.01f) { BLENDFUNCTION bf = {AC_SRC_OVER, 0, (BYTE)(veil >= 1.0f ? 255 : (int)(255 * veil)), 0}; AlphaBlend(dc, 0, cut, cl.right, cl.bottom - cut, kdc, 0, cut, cl.right, cl.bottom - cut, bf); }
        SelectObject(kdc, kold); DeleteDC(kdc);
    }
    if (g_csel >= 0) LibHomeCover(dc, g);   /* a custom game's cover, where a built-in game has its video */
    VitHome(dc, g);
    if (t >= 1.0f && g_overlay != OV_QUICK) HomeMoreHint(dc, g, g_homePage);   /* (review: not over the quick list's dim) */
    if (g_pageFocus && g_hpDraw == g_homePage) {   /* the new page is drawn: the keyboard's or the controller's focus goes to its first stop */
        g_pageFocus = FALSE;
        if (g_kbNav && !g_overlay) { FocusDefault(); g_hot = g_focus; InvalidateRect(g_wnd, NULL, FALSE); }
    }
#else
    PaintHomeIn(dc, g);
#endif
}

/* ---------------------------------------------------------------- a game's hub */
static const struct { int tab, icon; const wchar_t *pt, *en; } HUB[] = {
    {TAB_OVERVIEW, 0, L"Visão geral", L"Overview"}, {TAB_GUIDE, 9, L"Guia de configuração", L"Setup guide"}, {TAB_VIDEO, 1, L"Vídeo", L"Video"},
    {TAB_GAMEPLAY, 2, L"Jogabilidade", L"Gameplay"}, {TAB_ONLINE, 3, L"Online", L"Online"}, {TAB_SAVES, 4, L"Saves", L"Saves"},
    {TAB_CHEATS, 5, L"Trapaças", L"Cheats"}, {TAB_DLC, 6, L"DLC", L"DLC"}, {TAB_EXTRAS, 7, L"Extras", L"Extras"}, {TAB_VITRINE, 10, L"Vitrine", L"Showcase"} /* 6.2 */, {TAB_GAME, 8, L"Arquivos do jogo", L"Game files"}};
#define NHUB (int)(sizeof(HUB) / sizeof(HUB[0]))
/* 4.0.2: UFC 5 (its own launcher and settings) has no setup guide, online, saves, cheats or DLC sections; 4.3.1: UFC 5
   and 6 have Online (Parsec mode) */
static BOOL HubTabOk(int g, int tab)
{
    if (tab == TAB_VITRINE) return TRUE;   /* 6.2: every game's vitrine (its YouTube links) */
    if (X360G(g)) return tab == TAB_OVERVIEW || tab == TAB_GAME;   /* 5.0: UFC Undisputed 3 sets itself up in its own window */
    if (EXTG(g)) return tab == TAB_OVERVIEW || tab == TAB_VIDEO || tab == TAB_GAME;   /* 6.0: Zelda's options, God of War III's launcher */
    if (!G[g].ps5) return TRUE;
    return tab == TAB_OVERVIEW || tab == TAB_VIDEO || tab == TAB_GAMEPLAY || tab == TAB_ONLINE || tab == TAB_EXTRAS || tab == TAB_GAME;
}
static void HubIcon(GpGraphics *g, int icon, int x, int y, int s, UINT32 c)
{
    switch (icon) { case 0: IconInfo(g, x, y, s, c); break; case 1: IconMonitor(g, x, y, s, c); break; case 2: IconPad(g, x, y, s, c); break; case 3: IconGlobe(g, x, y, s, c); break;
                    case 4: IconSave(g, x, y, s, c); break; case 5: IconBolt(g, x, y, s, c); break; case 6: IconBox(g, x, y, s, c); break; case 7: IconSliders(g, x, y, s, c); break;
                    case 9: IconCheck(g, x, y, s, c); break;
                    case 10: {   /* 6.2: play in a frame (the vitrine) */
                        RECT fr = {x, y + s / 8, x + s, y + s - s / 8}; Box(g, fr, s / 5, 0, c, 1.6f);
                        GpPointF t[3] = {{x + s * 0.40f, y + s * 0.34f}, {x + s * 0.40f, y + s * 0.66f}, {x + s * 0.68f, y + s * 0.50f}}; GpBrush *b = NULL; GpPath *tp = NULL; GdipCreateSolidFill(c, &b); GdipCreatePath(0, &tp); if (b && tp) { GdipAddPathPolygon(tp, t, 3); GdipFillPath(g, b, tp); } if (tp) GdipDeletePath(tp); if (b) GdipDeleteBrush(b); break; }
                    default: IconFolder(g, x, y, s, c); }
}
static void PaintGuide(HDC dc, GpGraphics *g, RECT pr);
static void PaintSaves(HDC dc, GpGraphics *g, RECT pr);
static void PaintCheats(HDC dc, GpGraphics *g, RECT pr);
static void PaintDlc(HDC dc, GpGraphics *g, RECT pr);
static void PaintOverview(HDC dc, GpGraphics *g, RECT pr);
static void PaintHub(HDC dc, GpGraphics *g)
{
    int gsel = g_sel, st = St(gsel);
    /* the header: the icon, the name, PLAY */
    {   RECT hd = {SC(MX), SC(74), SC(MX + CW), SC(200)}; GlassPanel(g, hd, SkinRad(SC(18)), 120);   /* 5.0: the header on glass, the hub's sections on a glass rail */
        RECT rl = {SC(MX), SC(214), SC(MX) + SC(270), SC(742)}; GlassPanel(g, rl, SkinRad(SC(18)), 130); }   /* 7.0 fix: the rail starts where the panel does (214; it was 206), and no colored line along the header's top */
    int ix = SC(MX + 12), iy = SC(84), is = SC(104);
    RECT ir = {ix, iy, ix + is, iy + is}; GameTile(dc, g, gsel, ix, iy, is, 1); Box(g, ir, TILE_RAD(is), 0, A(RGB(255, 255, 255), 90), 1.5f);
    int tx = ir.right + SC(24);
    wchar_t lab[96]; if (ZELDAG(gsel)) _snwprintf(lab, 96, L"%s  ·  %s", MAKER(gsel), CONSOLE(gsel)); else _snwprintf(lab, 96, L"%s  ·  %s  ·  %s", MAKER(gsel), G[gsel].year, CONSOLE(gsel)); Spaced(dc, g_fTag, RGB(216, 208, 236), tx, iy + SC(6), lab, 2);   /* 6.0: Zelda's "year" field is its console */
    SkinTitle(dc, tx - SC(2), iy + SC(18), SC(40), SC(MX + CW) - SC(296) - tx, gsel, RGB(255, 255, 255), SkinAcc());
    int pw = StatusPill(dc, g, tx, iy + SC(70), st);
    wchar_t sub[200]; const GameVer *hv = GameVerOf(gsel);   /* 6.0: and the game's own version */
    _snwprintf(sub, 200, hv->tag[0] ? L"%s  ·  %s  ·  %s %s" : g_ver[gsel][0] ? L"%s  ·  %s" : L"%s", X360G(gsel) ? T(S_X360PORT) : ZELDAG(gsel) ? L2(L"Emulador ou PC nativo (Switch 2)", L"Emulator or native PC (Switch 2)") : BBG(gsel) ? L2(L"Port nativo bbport-windows (PS4)", L"Native bbport-windows port (PS4)") : FNCG(gsel) ? L2(L"Port nativo para PC (Xbox 360)", L"Native PC port (Xbox 360)") : GOW3G(gsel) || NGPORTG(gsel) ? L2(L"Port nativo para PC (PS4)", L"Native PC port (PS4)") : G[gsel].ps5 ? T(S_PS5PORT) : T(S_NATIVE), g_ver[gsel][0] ? g_ver[gsel] : NGPORTG(gsel) && !PortOk(gsel) ? L2(L"port não instalado", L"port not installed") : L2(L"versão do port desconhecida", L"port version unknown"), L2(L"jogo", L"game"), hv->tag);   /* 4.3: "Port nativo para PC (PS5)"; 6.0: Zelda, God of War III */
    Text(dc, g_fSmall, RGB(200, 192, 224), tx + pw + SC(14), iy + SC(76), sub);
    RECT pb = {SC(MX + CW) - SC(276), iy + SC(18), SC(MX + CW) - SC(16), iy + SC(78)}; PlayButton(dc, g, pb, NULL);   /* 7.0: 16 inside the header's edge (the white pill touched it) */
    /* the sections */
    int my = SC(222), row = 0, nrow = 0; for (int k = 0; k < NHUB; k++) if (HubTabOk(gsel, HUB[k].tab)) nrow++;
    int step = nrow > 10 ? SC(39) : SC(42), rh = step - SC(4);   /* 6.2: 11 sections (Vitrine) on UFC 1-4: closer rows, the buttons stay on the rail */
    for (int k = 0; k < NHUB; k++) {
        if (!HubTabOk(gsel, HUB[k].tab)) continue;
        RECT r = {SC(MX + 12), my + row * step, SC(MX + 12) + SC(246), my + row * step + rh}; row++;   /* 4.0.1: 10 sections (the setup guide) */
        BOOL on = g_tab == HUB[k].tab, hot = g_hot == H_TAB + HUB[k].tab;
        if (on) Box(g, r, rh / 2, A(RGB0(255, 255, 255), 30), A(RGB0(255, 255, 255), 40), 1.0f);   /* 7.0 fix: the open section is a white-glass pill (no colored bar, no colored fill); the color stays in its icon */
        else if (hot) Box(g, r, rh / 2, A(RGB0(255, 255, 255), 16), 0, 0);
        HubIcon(g, HUB[k].icon, r.left + SC(16), r.top + (rh - SC(18)) / 2, SC(18), A(on ? C_CYAN : hot ? RGB(255, 255, 255) : C_SUB, 255));
        wchar_t t[64]; wcsncpy(t, TR(HUB[k].pt, HUB[k].en), 63); t[63] = 0;
        if (HUB[k].tab == TAB_CHEATS) { int on2 = CheatsOn(gsel); if (on2) _snwprintf(t, 64, L"%s (%d)", TR(HUB[k].pt, HUB[k].en), on2); }
        RECT tr = {r.left + SC(48), r.top, r.right - SC(8), r.bottom}; TextR(dc, g_fBtn, on || hot ? RGB(255, 255, 255) : C_TEXT, tr, t, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        AddHit(r.left, r.top, r.right, r.bottom, H_TAB + HUB[k].tab);
    }
    int by = my + row * step + SC(4);
    RECT bb = {SC(MX + 12), by, SC(MX + 12) + SC(120), by + SC(40)}; Button(dc, g, bb, L2(L"‹ Voltar", L"‹ Back"), H_BACK, 2);
    RECT lb = {bb.right + SC(6), by, SC(MX + 12) + SC(246), by + SC(40)}; Button(dc, g, lb, T(S_LOGS), H_LOGS, 2);
    RECT ab = {SC(MX + 12), by + SC(44), SC(MX + 12) + SC(246), by + SC(82)}; Button(dc, g, ab, T(S_ADV), H_ADV, 2);
    /* the panel */
    RECT pr = {SC(MX + 274), SC(214), SC(MX + CW), SC(742)}; Panel(g, pr);
    g_itBottom = pr.bottom - SC(12);   /* 6.2 */
    ClipBase(g); GdipSetClipRectI(g, pr.left, pr.top, pr.right - pr.left, pr.bottom - pr.top, 1);
    int sdc = SaveDC(dc); IntersectClipRect(dc, pr.left + g_ox, pr.top + g_oy, pr.right + g_ox, pr.bottom - SC(8) + g_oy);   /* 5.0: the text (GDI) stays inside the panel too */
    switch (g_tab) {
    case TAB_OVERVIEW: PaintOverview(dc, g, pr); break;
    case TAB_GUIDE: PaintGuide(dc, g, pr); break;
    case TAB_SAVES: PaintSaves(dc, g, pr); break;
    case TAB_CHEATS: PaintCheats(dc, g, pr); break;
    case TAB_DLC: PaintDlc(dc, g, pr); break;
    case TAB_VITRINE: PaintVitrineTab(dc, g, pr); break;   /* 6.2 (home62.c) */
    default: { static Item it[40]; int n = BuildItems(gsel, g_tab, it); PaintItems(dc, g, it, n, pr.left + SC(24), pr.top + SC(22), pr.right - pr.left - SC(48)); }
    }
    RestoreDC(dc, sdc);
    ClipBase(g);
}
static void PanelTitle(HDC dc, GpGraphics *g, RECT pr, const wchar_t *t, const wchar_t *sub)
{
    (void)g;
    Text(dc, g_fH2, RGB(255, 255, 255), pr.left + SC(24), pr.top + SC(18), t);
    if (sub) { RECT r = {pr.left + SC(24), pr.top + SC(56), pr.right - SC(24), pr.top + SC(100)}; Wrap(dc, g_fSmall, C_SUB, r, sub, TRUE); }
}
static const Field F_OV_FPS = {S_FPS, L"fps_limit", FPS4X, N(FPS4X), 60};
static void PaintOverview(HDC dc, GpGraphics *g, RECT pr)
{
    int gsel = g_sel;
    static Item it[40]; int n = 0;
    { int warn = FolderNote(gsel, g_note[0], 900); n = AddI(it, n, warn ? IT_WARN : IT_NOTE, NULL, NULL, g_note[0], 0, 3); }
    if (EXTG(gsel)) {   /* 6.0: Zelda and God of War III */
        if (ZELDAG(gsel)) { n = AddI(it, n, IT_DROP, &FZ_ENGINE, NULL, NULL, 0, 2); n = AddI(it, n, IT_SWITCH, &FZ_FULL, NULL, NULL, 0, 1);
                            n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Ocarina Setup: recompilar para PC", L"Ocarina Setup: recompile for PC"), H_RECOMP, 1); }
        if (NGPORTG(gsel)) n = AddI(it, n, IT_FBUTTON, NULL, NULL, PortOk(gsel) ? L2(L"Reinstalar / atualizar o port", L"Reinstall / update the port") : L2(L"Instalar o port", L"Install the port"), H_EXTINST, 1);   /* 6.1 */
        if ((!NGPORTG(gsel) || PortOk(gsel)) && !FNCG(gsel)) n = AddI(it, n, IT_FBUTTON, NULL, NULL, ZELDAG(gsel) ? L2(L"Abrir o emulador (configurar)", L"Open the emulator (settings)") : L2(L"Abrir o launcher do port", L"Open the port's launcher"), H_EXTCFG, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Criar atalho na área de trabalho", L"Make a desktop shortcut"), H_SHORTCUT, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Adicionar à biblioteca da Steam", L"Add to the Steam library"), H_STEAM, 1);
        wchar_t ptx[160]; FmtPlayTime(gsel, ptx, 160);
        if (ZELDAG(gsel)) _snwprintf(g_note[2], 900, L2(L"Perfil: %s  ·  %s. O Zelda: Ocarina of Time de Switch 2 roda no emulador Lichii (MDias v4, em português e com as correções para AMD; o Frizti v3 é a outra opção) ou NATIVO no PC: \"Ocarina Setup: recompilar para PC\" abre o Ocarina Setup 0.2.1, que já vem dentro do PORTHEON, para transformar o seu jogo (versão 1.1.1) num programa de PC, e o PORTHEON passa a abrir ele sozinho quando fica pronto. O recompilador é feito só para este jogo. Use o SEU jogo: o PORTHEON não vem com ele nem com chaves de console.",
                                                        L"Profile: %s  ·  %s. The Switch 2 Zelda: Ocarina of Time runs on the Lichii emulator (MDias v4, with the AMD fixes; Frizti v3 is the other choice) or NATIVELY on the PC: \"Ocarina Setup: recompile for PC\" opens Ocarina Setup 0.2.1, which comes inside PORTHEON, to turn your game (version 1.1.1) into a PC program, and PORTHEON opens it by itself once it is ready. The recompiler is made for this game only. Use YOUR game: PORTHEON ships neither it nor console keys."), CurProf()->name, ptx);
        else if (NGPORTG(gsel)) { _snwprintf(g_note[2], 900, L2(L"Perfil: %s  ·  %s. %s", L"Profile: %s  ·  %s. %s"), CurProf()->name, ptx, NgCredits(gsel)); g_note[2][899] = 0; }   /* 6.1 */
        else _snwprintf(g_note[2], 900, L2(L"Perfil: %s  ·  %s. O port nativo gow3_pc roda o God of War III Remastered de PS4 (CUSA01623, update 01.02) direto no PC, com o vídeo do shadPS4. Na primeira vez o launcher do port prepara o jogo a partir do seu eboot.bin. Vídeo, FSR, controles e mods ficam no launcher do port. Use o SEU jogo: o PORTHEON não vem com ele.",
                                           L"Profile: %s  ·  %s. The native gow3_pc port runs the PS4 God of War III Remastered (CUSA01623, update 01.02) directly on the PC, with shadPS4's video core. The first time, the port's launcher prepares the game from your eboot.bin. Video, FSR, controls and mods are in the port's launcher. Use YOUR game: PORTHEON does not ship it."), CurProf()->name, ptx);
        n = AddI(it, n, IT_NOTE, NULL, NULL, g_note[2], 0, 3);
        PaintItems(dc, g, it, n, pr.left + SC(24), pr.top + SC(22), pr.right - pr.left - SC(48));
        return;
    }
    if (X360G(gsel)) {   /* 5.0: UFC Undisputed 3: its port's own configurator, the shortcuts, and what to expect */
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Configurar o port", L"Configure the port"), H_PORTCFG, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Criar atalho na área de trabalho", L"Make a desktop shortcut"), H_SHORTCUT, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Adicionar à biblioteca da Steam", L"Add to the Steam library"), H_STEAM, 1);
        wchar_t ptx[160]; FmtPlayTime(gsel, ptx, 160);
        _snwprintf(g_note[2], 900, L2(L"Perfil: %s  ·  %s. O port (UFC3MONS) roda o UFC Undisputed 3 do Xbox 360 direto no PC e trava o jogo em 60 FPS. A primeira vez que o jogo abre ele prepara os shaders por alguns minutos; depois abre direto. Vídeo (a tela cheia do port já é sem bordas), controles, saves, trapaças, mods e DLC ficam em \"Configurar o port\".",
                                      L"Profile: %s  ·  %s. The port (UFC3MONS) runs the Xbox 360 UFC Undisputed 3 directly on the PC and keeps the game at a fixed 60 FPS. The first time the game opens it prepares its shaders for a few minutes; after that it opens straight away. Video (the port's full screen is already borderless), controls, saves, cheats, mods and DLC are in \"Configure the port\"."), CurProf()->name, ptx);
        n = AddI(it, n, IT_NOTE, NULL, NULL, g_note[2], 0, 3);
        PaintItems(dc, g, it, n, pr.left + SC(24), pr.top + SC(22), pr.right - pr.left - SC(48));
        return;
    }
    if (UFC5G(gsel)) {   /* 4.0.2: UFC 5 (4.3: back, as in 4.0.6) */
        n = AddI(it, n, IT_DROP, &F5_RES, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F5_LANG, NULL, NULL, 0, 1);
        n = AddI(it, n, IT_SWITCH, &F5_FULL, NULL, NULL, 0, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Melhores configurações para este PC", L"Best settings for this PC"), H_BESTCFG, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Criar atalho na área de trabalho", L"Make a desktop shortcut"), H_SHORTCUT, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Adicionar à biblioteca da Steam", L"Add to the Steam library"), H_STEAM, 1);   /* 5.0 */
        wchar_t pt5[160]; FmtPlayTime(gsel, pt5, 160);
        _snwprintf(g_note[2], 900, L2(L"Perfil: %s  ·  %s. A primeira partida compila os shaders do jogo: alguns minutos com FPS baixo. Na luta, o UFC 5 depende do processador: num Ryzen 5 5500 com RTX 4060 ele roda entre 23 e 26 FPS, e o PORTHEON acerta a velocidade do jogo pelo FPS real (Vídeo > Luta na velocidade do PS5), então a luta não fica em câmera lenta.",
                                      L"Profile: %s  ·  %s. The first session compiles the game's shaders: a few minutes at low FPS. In a fight UFC 5 depends on the processor: on a Ryzen 5 5500 with an RTX 4060 it runs at 23 to 26 FPS, and PORTHEON sets the game's speed from the real FPS (Video > Fights at the PS5's pace), so the fight is not in slow motion."), CurProf()->name, pt5);
        n = AddI(it, n, IT_NOTE, NULL, NULL, g_note[2], 0, 3);
        PaintItems(dc, g, it, n, pr.left + SC(24), pr.top + SC(22), pr.right - pr.left - SC(48));
        return;
    }
    if (G[gsel].ps5) {   /* 4.2: UFC 6 */
        n = AddI(it, n, IT_DROP, &F6_RSCALE, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F6_LANG, NULL, NULL, 0, 1);
        n = AddI(it, n, IT_SWITCH, &F6_FULL, NULL, NULL, 0, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Melhores configurações para este PC", L"Best settings for this PC"), H_BESTCFG, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Criar atalho na área de trabalho", L"Make a desktop shortcut"), H_SHORTCUT, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Adicionar à biblioteca da Steam", L"Add to the Steam library"), H_STEAM, 1);   /* 5.0 */
        wchar_t pt5[160]; FmtPlayTime(gsel, pt5, 160);
        _snwprintf(g_note[2], 900, L2(L"Perfil: %s  ·  %s. A primeira partida monta os shaders do jogo (o menu e a primeira luta podem engasgar); depois eles ficam guardados, também quando o driver da placa é atualizado. Num Ryzen 5 5500 com RTX 4060 (medido em 29/10/2026): tela inicial a 60 FPS, menu principal e entradas dos lutadores entre 15 e 30 FPS (a placa de vídeo é o limite nessas cenas) e a luta entre 22 e 43 FPS no perfil Rápido.",
                                      L"Profile: %s  ·  %s. The first session builds the game's shaders (the menu and the first fight can stutter); after that they stay saved, also when the graphics driver is updated. On a Ryzen 5 5500 with an RTX 4060 (measured 2026-10-29): the title screen at 60 FPS, the main menu and the walkouts between 15 and 30 FPS (the graphics card is the limit in those scenes) and fights between 22 and 43 FPS in the Fast profile."), CurProf()->name, pt5);
        n = AddI(it, n, IT_NOTE, NULL, NULL, g_note[2], 0, 3);
        /* 4.3: what this PC's memory means for UFC 6 (the user's call: cards of 12, 8, 6 and 4 GB, little RAM) */
        int vmb = g_haveGpu ? (int)(g_gpu.vram >> 20) : 0;
        if (g_ramGb && g_ramGb < 12) n = AddI(it, n, IT_WARN, NULL, NULL, L2(L"Este PC tem menos de 12 GB de RAM e o UFC 6 usa cerca de 10 GB: feche o navegador e outros programas antes de jogar e deixe o Windows gerenciar o arquivo de paginação. Se o jogo fechar por falta de memória, o PORTHEON avisa.",
                                                                        L"This PC has less than 12 GB of RAM and UFC 6 uses about 10 GB: close the browser and other programs before playing and let Windows manage the page file. If the game closes for lack of memory, PORTHEON says so."), 0, 3);
        if (vmb && vmb <= 5632) n = AddI(it, n, IT_WARN, NULL, NULL, L2(L"Placa de vídeo de 4 ou 5 GB: o UFC 6 roda no perfil de 4 GB (parte dos dados da placa fica na RAM: mais lento, mas sem fechar por falta de memória de vídeo). Use o perfil Rápido e feche o navegador antes de jogar.",
                                                                       L"4 or 5 GB graphics card: UFC 6 runs in the 4 GB profile (part of the card's data stays in RAM: slower, but without closing for lack of video memory). Use the Fast profile and close the browser before playing."), 0, 3);   /* UFC6.exe 1.4.0: up to 5.5 GB */
        else if (vmb && vmb <= 6656) n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Placa de vídeo de 6 GB: o UFC 6 roda no perfil de 6 GB (os buffers maiores ficam na RAM). Numa troca de cena pesada pode engasgar por um instante.",
                                                                            L"6 GB graphics card: UFC 6 runs in the 6 GB profile (the biggest buffers stay in RAM). A heavy scene change can stutter for a moment."), 0, 3);
        PaintItems(dc, g, it, n, pr.left + SC(24), pr.top + SC(22), pr.right - pr.left - SC(48));
        return;
    }
    n = AddI(it, n, IT_DROP, gsel == 2 ? &F4_FPS3 : &F4_FPS4, NULL, NULL, 0, 1);
    n = AddI(it, n, IT_DROP, &F4_RES, NULL, NULL, 0, 1);
    n = AddI(it, n, IT_DROP, &F4_LANG, NULL, NULL, 0, 1);
    n = AddI(it, n, IT_SWITCH, &F4_FULL, NULL, NULL, 0, 1);
    n = AddI(it, n, IT_SWITCH, &F4_PARSEC, NULL, NULL, 0, 1);
    n = AddI(it, n, IT_SWITCH, &F4_SKIP, NULL, NULL, 0, 1);
    n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Melhores configurações para este PC", L"Best settings for this PC"), H_BESTCFG, 1);
    n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Verificar arquivos do jogo", L"Check the game's files"), H_VERIFY, 1);
    n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Criar atalho na área de trabalho", L"Make a desktop shortcut"), H_SHORTCUT, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Adicionar à biblioteca da Steam", L"Add to the Steam library"), H_STEAM, 1);   /* 5.0 */
    wchar_t pt[160]; FmtPlayTime(gsel, pt, 160);
    _snwprintf(g_note[2], 900, L2(L"Perfil: %s  ·  %s. O PORTHEON já escolheu as configurações para o seu PC; mude o que quiser aqui ou nas seções ao lado. Enquanto joga, deixe a janela do jogo em foco.",
                                  L"Profile: %s  ·  %s. PORTHEON already picked the settings for your PC; change anything here or in the sections beside. While you play, keep the game's window in focus."), CurProf()->name, pt);
    n = AddI(it, n, IT_NOTE, NULL, NULL, g_note[2], 0, 3);
    PaintItems(dc, g, it, n, pr.left + SC(24), pr.top + SC(22), pr.right - pr.left - SC(48));
}

/* ---------------------------------------------------------------- 4.0.1: the setup guide
   Every setting of this game in four pages (video, gameplay, extras; then cheats, saves and online): what it does, what
   PORTHEON recommends for this PC (RecValue: the values the automatic setup writes) and what it is now, with "Use" to
   take the recommendation. It opens by itself the first time a game's hub opens (OpenGame). */
typedef struct { const Field *f; const wchar_t *pt, *en; } GuideRow;
static int g_guidePage;
static int GuideRows(int g, int page, GuideRow *r)
{
    int n = 0;
    #define ROW(F, P, E) do { r[n].f = (F); r[n].pt = (P); r[n].en = (E); n++; } while (0)
    if (page == 0) {
        ROW(g == 2 ? &F4_FPS3 : &F4_FPS4, L"Quadros por segundo. 60 é o estável; acima é experimental (UFC 3: até 144).", L"Frames per second. 60 is the steady one; higher is experimental (UFC 3: up to 144).");
        ROW(&F4_RES, L"Tamanho em que o jogo desenha. 1080p é o seguro; 2K/4K pedem placa forte.", L"The size the game draws at. 1080p is safe; 2K/4K need a strong card.");
        ROW(&F4_WIN, L"O tamanho da janela no monitor. \"Igual ao jogo\" é o mais nítido.", L"The window's size on the monitor. \"Same as the game\" is the sharpest.");
        ROW(&F4_PRES, L"Como a imagem chega ao monitor. Imediato: sem VSync e sem trava de FPS (pode rasgar um pouco). Mailbox: sem rasgar.", L"How frames reach the monitor. Immediate: no VSync and no FPS lock (may tear a little). Mailbox: no tearing.");
        ROW(&F4_UPS, L"FSR 1.0 deixa nítida uma resolução menor esticada. Desligado na nativa.", L"FSR 1.0 sharpens a lower resolution enlarged. Off at native resolution.");
        ROW(&F4_FULL, L"O jogo ocupa o monitor inteiro. Em janela é só para testes.", L"The game fills the monitor. Windowed is only for tests.");
        if (g != 0) ROW(&F4_NEO, L"Gráficos mais ricos do PS4 Pro. Só em placa NVIDIA/AMD forte (8 GB).", L"The PS4 Pro's richer graphics. Only on a strong NVIDIA/AMD card (8 GB).");
        ROW(&F4_ULTRA, L"Para PC muito fraco: 720p, modo PS4, 30 FPS e FSR 1.0.", L"For a very weak PC: 720p, PS4 mode, 30 FPS and FSR 1.0.");
    } else if (page == 1) {
        ROW(&F4_LANG, L"Textos e narração do jogo (quando o jogo tem esse idioma).", L"The game's texts and commentary (when the game has that language).");
        ROW(&F4_PADTYPE, L"Automático acha todo controle. Escolha o seu se o jogo não reconhecer.", L"Automatic finds every controller. Pick yours if the game misses it.");
        ROW(&F4_SKIP, L"Vai direto para o menu, sem os vídeos de abertura.", L"Goes straight to the menu, without the intro videos.");
        ROW(&F4_LOWLAT, L"Um quadro na fila em vez de vários: para o Parsec.", L"One frame queued instead of several: for Parsec.");
        ROW(&F4_PADBG, L"Só para o Parsec: o controle funciona com outra janela na frente.", L"Only for Parsec: the controller works with another window in front.");
    } else if (page == 2) {
        if ((g == 1 || g == 2) && PatchState(g, L"crowd", -1) >= 0) ROW(&FP_CROWD, L"A torcida na arena. Desligada dá de 5 a 10 FPS a mais na luta.", L"The crowd in the arena. Off gives 5 to 10 more FPS in the fight.");
        if (g == 3) ROW(&F4P_CROWD, L"A torcida na arena. \"Flashes a 25%\": menos flashes de câmera na plateia (o público continua). Desligada dá de 5 a 10 FPS a mais na luta.", L"The crowd in the arena. \"Flashes at 25%\": fewer camera flashes in the stands (the audience stays). Off gives 5 to 10 more FPS in the fight.");
        /* 4.0.4: the camera's blur too, and UFC 1 and 2 have the switch */
        if (g <= 2 && PatchState(g, L"blur", -1) >= 0) ROW(&FP_BLUR, L"O borrão da câmera e dos lutadores em movimento. Desligado: imagem mais nítida.", L"The blur of the camera and of fighters in motion. Off: a sharper image.");
        if (g == 3) ROW(&F4P_BLUR, L"O borrão da câmera e dos lutadores em movimento. Desligado: imagem mais nítida.", L"The blur of the camera and of fighters in motion. Off: a sharper image.");
        if (g == 3) ROW(&F4P_SHADOW, L"As sombras dos lutadores e do octógono. Desligadas dão mais FPS em placas fracas; a imagem fica mais chapada.", L"The fighters' and the octagon's shadows. Off gives more FPS on weak cards; the picture looks flatter.");   /* 5.0 */
        if (g == 3) ROW(&F4N_ENGINE, L"Automático: o Native Plus (em processador ou placa Intel, o motor certo para eles). Se o jogo fechar com ele, o PORTHEON volta sozinho ao motor de antes. Os outros ficam aqui para testar.", L"Automatic: Native Plus (on an Intel processor or card, the right engine for them). If the game closes on it, PORTHEON goes back to the engine from before by itself. The others stay here to try.");
        if (g == 2) ROW(&F4_ENGINE, L"Automático: o motor do online. Motor novo: correções da Carreira e Intel Arc. Padrão do port: o antigo.", L"Automatic: the online engine. New engine: Career fixes and Intel Arc. The port's own: the old one.");
        if (g == 1) ROW(&F2_ENGINE, L"Automático: o motor do online. Padrão do port: o antigo, se este rodar pior no seu PC.", L"Automatic: the online engine. The port's own: the old one, if this one runs worse on your PC.");
        if (g == 3) ROW(&F4_CAREER, L"Deixa a Carreira com FPS estável e evita os fechamentos com código 3.", L"Keeps Career at a steady FPS and avoids its code 3 closes.");
        ROW(&F4_BOOST, L"O jogo ganha prioridade no processador: mais FPS.", L"The game gets priority on the processor: more FPS.");
        ROW(&F4_CPUALL, L"O motor usa todas as threads do processador: mais FPS em processadores com muitas.", L"The engine uses every processor thread: more FPS on processors with many.");
        ROW(&F4_ASYNC, L"Shaders novos compilam ao lado do jogo: menos travadinhas na 1ª vez de cada cena.", L"New shaders compile beside the game: fewer hitches the 1st time of each scene.");
        if (g != 0) ROW(&F4_GPUSAFE, L"Grava o vídeo numa thread só: menos travamentos da placa. Liga sozinho após um código 23.", L"Records video work on one thread: fewer GPU stops. Turns on by itself after a code 23.");
        ROW(&F4_SKIN, L"Só se a pele de um lutador aparecer errada (verde, preta ou manchada).", L"Only if a fighter's skin looks wrong (green, black or blotchy).");
    }
    #undef ROW
    return n;
}
static void GuideValName(const Field *f, int v, wchar_t *out, int n)
{
    if (v < 0) wcsncpy(out, L"—", n);
    else if (!f->opts) wcsncpy(out, v ? L2(L"Ligado", L"On") : L2(L"Desligado", L"Off"), n);
    else { int i = OptIndex(f, v); if (i >= 0) wcsncpy(out, OptName(&f->opts[i]), n); else _snwprintf(out, n, L"%d", v); }
    out[n - 1] = 0;
}
/* "Use" on one row, and "Apply all" (every row of the three pages: the automatic setup, plus the extras it leaves alone) */
static void GuideUse(int g, const Field *f)
{
    int v = RecValue(g, f->key); if (v < 0) return;
    PutInt(g, f->key, v); g_kvGame = -1;
    wchar_t vn[96]; GuideValName(f, v, vn, 96);
    Msg(C_GOOD, L2(L"%s: %s (o recomendado para este PC). Vale no próximo JOGAR.", L"%s: %s (the recommendation for this PC). Applies at the next PLAY."), T(f->label), vn);
}
static void GuideApplyAll(int g)
{
    if (!PS4G(g)) return;
    DetectPc(); g_kvGame = -1; AutoConfig4(g);
    GuideRow r[16];
    for (int p = 0; p < 3; p++) { int n = GuideRows(g, p, r); for (int i = 0; i < n; i++) { int v = RecValue(g, r[i].f->key); if (v >= 0 && GetInt(g, r[i].f->key, r[i].f->def) != v) PutInt(g, r[i].f->key, v); } }
    if (GetInt(g, L"parsec", 0)) ParsecApply(g);
    g_kvGame = -1; LoadEdits();
    wchar_t sum[400]; PcSummary(sum, 400);
    Msg(C_GOOD, L2(L"%s: todas as opções no recomendado para este PC (%s).", L"%s: every setting at the recommendation for this PC (%s)."), G[g].name, sum);
}
/* the last page: the other sections, each with what it is for and a button that opens it */
static void GuideOther(HDC dc, GpGraphics *g, int x, int y, int w, int bottom)
{
    int gsel = g_sel; CheatsLoad(gsel); int nc = g_ncheat > 0 ? g_ncheat : 0;
    wchar_t c1[300], c2[300];
    if (nc) _snwprintf(c1, 300, L2(L"%d trapaças para o %s. Só nos modos offline: ligue na seção Trapaças antes do JOGAR.", L"%d cheats for %s. Offline modes only: turn them on in the Cheats section before PLAY."), nc, G[gsel].name);
    else _snwprintf(c1, 300, L2(L"O %s ainda não tem trapaças.", L"%s has no cheats yet."), G[gsel].name);
    _snwprintf(c2, 300, L"%s", PIni(L"save_autobackup", 1) ? L2(L"Backup automático ligado: cada sessão vira um .zip em Documentos\\PORTHEON\\Backups. Restaure ou leve para outro PC na seção Saves.", L"Automatic backup on: each session becomes a .zip in Documents\\PORTHEON\\Backups. Restore it or take it to another PC in the Saves section.")
                                                            : L2(L"Backup automático desligado: ligue na seção Saves para guardar seus saves a cada sessão.", L"Automatic backup off: turn it on in the Saves section to keep your saves after each session."));
    struct { int tab, icon; const wchar_t *title, *text; } cards[6] = {
        {TAB_CHEATS, 5, L2(L"Trapaças", L"Cheats"), c1},
        {TAB_SAVES, 4, L2(L"Saves", L"Saves"), c2},
        {TAB_ONLINE, 3, L2(L"Online", L"Online"), ARENA_GAME(gsel) ? L2(L"PORTHEON ARENA (novo): fila pública, salas e convites para achar adversário, com a luta pelo Parsec.", L"PORTHEON ARENA (new): public queue, rooms and invites to find an opponent, with the fight through Parsec.")   /* 7.2 */
                                                                  : L2(L"Jogue com um amigo pelo Parsec: o controle dele vira o Jogador 2. O modo Parsec ajusta latência e FPS sozinho.", L"Play with a friend through Parsec: their controller becomes Player 2. Parsec mode sets latency and FPS by itself.")},
        {TAB_DLC, 6, L"DLC", L2(L"O que o jogo desbloqueia jogando já funciona. DLC comprada à parte depende do motor dos ports.", L"What the game unlocks as you play already works. DLC bought separately depends on the ports' engine.")},
        {TAB_GAME, 8, L2(L"Arquivos do jogo", L"Game files"), L2(L"Verifique se os arquivos estão completos, troque a pasta do jogo ou crie um atalho na área de trabalho.", L"Check that the files are complete, change the game folder or make a desktop shortcut.")},
        {-1, 0, L2(L"Se der problema", L"If something goes wrong"), L2(L"O jogo fechou? \"Aplicar tudo\" e mande o log mais novo (Logs) no Discord. FPS caiu? Resolução menor ou \"Ultra desempenho\".", L"The game closed? \"Apply all\" and send the newest log (Logs) on Discord. FPS dropped? A lower resolution or \"Ultra performance\".")}};
    int gap = SC(14), cw = (w - gap) / 2, ch = (bottom - y - 2 * gap) / 3; if (ch > SC(118)) ch = SC(118);
    for (int k = 0; k < 6; k++) {
        int cx = x + (k % 2) * (cw + gap), cy = y + (k / 2) * (ch + gap);
        RECT r = {cx, cy, cx + cw, cy + ch}; Box(g, r, SC(14), A(RGB(255, 255, 255), 8), A(C_LINE, 255), 1.0f);
        HubIcon(g, cards[k].tab < 0 ? 0 : cards[k].icon, cx + SC(16), cy + SC(16), SC(18), A(C_CYAN, 255));
        Text(dc, g_fBtn, RGB(255, 255, 255), cx + SC(44), cy + SC(15), cards[k].title);
        RECT tr = {cx + SC(16), cy + SC(42), cx + cw - SC(16), cy + ch - SC(8)}; Wrap(dc, g_fSmall, C_SUB, tr, cards[k].text, TRUE);
        RECT b = {cx + cw - SC(112), cy + SC(10), cx + cw - SC(12), cy + SC(38)};
        if (cards[k].tab >= 0) Button(dc, g, b, L2(L"Abrir", L"Open"), H_GUIDEOPEN + cards[k].tab, 2);
        else Button(dc, g, b, T(S_LOGS), H_LOGS, 2);
    }
}
static void PaintGuide(HDC dc, GpGraphics *g, RECT pr)
{
    int gsel = g_sel, x = pr.left + SC(24), w = pr.right - pr.left - SC(48), y = pr.top + SC(18);
    wchar_t t[96]; _snwprintf(t, 96, L2(L"Guia de configuração do %s", L"%s setup guide"), G[gsel].name);
    const wchar_t *all = L2(L"Aplicar tudo para o meu PC", L"Apply all for my PC"); int aw = SpacedW(dc, g_fBtn, all, 1) + SC(44); if (aw < SC(220)) aw = SC(220); if (aw > w / 2) aw = w / 2;
    RECT ab = {x + w - aw, y, x + w, y + SC(36)}; Button(dc, g, ab, all, H_GUIDEALL, 1);
    RECT hr = {x, y - SC(2), ab.left - SC(12), y + SC(36)}; TextR(dc, g_fH2, RGB(255, 255, 255), hr, t, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    wchar_t sum[300], sub[700]; PcSummary(sum, 300);
    _snwprintf(sub, 700, L2(L"%s. \"Para o seu PC\" é o que o PORTHEON recomenda; \"Usar\" troca uma opção e \"Aplicar tudo\" troca todas. Clique numa opção para ler o que ela faz. Vale no próximo JOGAR.",
                            L"%s. \"For your PC\" is what PORTHEON recommends; \"Use\" changes one setting and \"Apply all\" changes them all. Click a setting to read what it does. Applies at the next PLAY."),
                sum);
    RECT sr = {x, y + SC(44), x + w, y + SC(84)}; int sh = Wrap(dc, g_fSmall, C_SUB, sr, sub, TRUE); if (sh > SC(40)) sh = SC(40);
    /* the pages */
    static const wchar_t *const pp[4] = {L"Vídeo e desempenho", L"Jogabilidade", L"Extras", L"Trapaças, saves e online"};
    static const wchar_t *const pe[4] = {L"Video and performance", L"Gameplay", L"Extras", L"Cheats, saves and online"};
    int py = y + SC(52) + sh, px = x;
    for (int p = 0; p < 4; p++) {
        const wchar_t *lab = TR(pp[p], pe[p]); int tw = TextW(dc, g_fBtn, lab) + SC(32);
        RECT r = {px, py, px + tw, py + SC(32)}; BOOL on = g_guidePage == p, hot = g_hot == H_GUIDEPAGE + p;
        if (on) Box(g, r, SC(16), A(C_HOT, 245), 0, 0); else Box(g, r, SC(16), A(RGB(255, 255, 255), hot ? 26 : 10), A(hot ? C_HOT : C_FIELDB, 255), 1.0f);
        TextR(dc, g_fBtn, on ? C_INK : hot ? RGB(255, 255, 255) : C_TEXT, r, lab, DT_CENTER | DT_VCENTER);
        AddHit(r.left, r.top, r.right, r.bottom, H_GUIDEPAGE + p); px = r.right + SC(8);
    }
    int ty = py + SC(46);
    if (g_guidePage == 3) { GuideOther(dc, g, x, ty, w, pr.bottom - SC(16)); return; }
    /* the table: the setting and what it does, the recommendation, the current value, Use */
    int cB = x + w * 56 / 100, cC = x + w * 75 / 100, bw = SC(76);
    Spaced(dc, g_fTag, C_SUB, x + SC(2), ty, L2(L"OPÇÃO E O QUE ELA FAZ", L"SETTING AND WHAT IT DOES"), 1);
    Spaced(dc, g_fTag, C_CYAN, cB, ty, L2(L"PARA O SEU PC", L"FOR YOUR PC"), 1);
    Spaced(dc, g_fTag, C_SUB, cC, ty, L2(L"AGORA", L"NOW"), 1);
    GuideRow rows[16]; int n = GuideRows(gsel, g_guidePage, rows), rh = SC(40), ry = ty + SC(20);
    if (n > 0 && ry + n * rh > pr.bottom - SC(10)) rh = (pr.bottom - SC(10) - ry) / n;
    for (int i = 0; i < n; i++) {
        const Field *f = rows[i].f; int rec = RecValue(gsel, f->key), cur = GetInt(gsel, f->key, f->def); BOOL same = rec < 0 || rec == cur;
        int top = ry + i * rh; BOOL hot = g_hot == H_GUIDEROW + 20 + i;
        Line(g, x, top, x + w, top, A(C_LINE, 255), 1.0f);
        RECT lr = {x, top + 1, cB - SC(12), top + rh};
        if (hot) Box(g, lr, SC(8), A(RGB(255, 255, 255), 14), 0, 0);
        RECT l1 = {x + SC(8), top + SC(3), cB - SC(16), top + rh / 2 + SC(2)}; TextR(dc, g_fBtn, RGB(255, 255, 255), l1, T(f->label), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        RECT l2 = {x + SC(8), top + rh / 2, cB - SC(16), top + rh - SC(2)}; TextR(dc, g_fSmall, C_SUB, l2, TR(rows[i].pt, rows[i].en), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        AddHit(lr.left, lr.top, lr.right, lr.bottom, H_GUIDEROW + 20 + i);
        wchar_t vr[96], vc[96]; GuideValName(f, rec, vr, 96); GuideValName(f, cur, vc, 96);
        /* 4.0.4: a long value a size smaller, not cut ("60 FPS (recommen...") */
        RECT br = {cB, top, cC - SC(10), top + rh}; TextR(dc, TextW(dc, g_fText, vr) > br.right - br.left ? g_fSmall : g_fText, C_CYAN, br, vr, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        RECT cr = {cC, top, x + w - bw - SC(12), top + rh}; TextR(dc, TextW(dc, g_fText, vc) > cr.right - cr.left ? g_fSmall : g_fText, same ? C_TEXT : C_ORANGE, cr, vc, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        if (!same) { RECT b = {x + w - bw, top + (rh - SC(30)) / 2, x + w, top + (rh + SC(30)) / 2}; Button(dc, g, b, L2(L"Usar", L"Use"), H_GUIDEROW + i, 0); }
        else IconCheck(g, x + w - bw / 2 - SC(9), top + (rh - SC(18)) / 2, SC(18), A(C_GOOD, 255));
    }
}
static void PaintSaves(HDC dc, GpGraphics *g, RECT pr)
{
    int gsel = g_sel;
    wchar_t sub[300]; _snwprintf(sub, 300, L2(L"Os saves do %s no perfil %s. Um backup é um arquivo .zip em Documentos\\PORTHEON\\Backups: dá para levar para outro PC.", L"%s's saves in the %s profile. A backup is a .zip file in Documents\\PORTHEON\\Backups: it can go to another PC."), G[gsel].name, CurProf()->name);
    PanelTitle(dc, g, pr, L2(L"Saves", L"Saves"), sub);
    static SaveInfo sv[24]; int n = SaveList(gsel, sv, 24);
    int y = pr.top + SC(108), x = pr.left + SC(24), w = pr.right - pr.left - SC(48);
    if (!n) { RECT r = {x, y, x + w, y + SC(70)}; NoteBox(dc, g, x, y, w, SC(60), L2(L"Ainda não há saves neste perfil. Eles aparecem aqui depois que você jogar e o jogo salvar.", L"No saves in this profile yet. They show up here once you play and the game saves."), FALSE); (void)r; y += SC(70); }
    for (int i = 0; i < n && i < 7; i++) {
        RECT r = {x, y, x + w, y + SC(50)}; BOOL hot = g_hot == H_SAVEITEM + i;
        Box(g, r, SC(12), A(RGB(255, 255, 255), hot ? 22 : 9), A(hot ? C_HOT : C_LINE, 255), 1.0f);
        IconSave(g, x + SC(16), y + SC(15), SC(20), A(C_GOOD, 255));
        wchar_t t[300]; _snwprintf(t, 300, L"%s%s%s", sv[i].title[0] ? sv[i].title : sv[i].slot, sv[i].sub[0] ? L"  ·  " : L"", sv[i].sub);
        RECT tr = {x + SC(50), y + SC(6), x + w - SC(230), y + SC(28)}; TextR(dc, g_fBtn, RGB(255, 255, 255), tr, t, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        RECT sr = {x + SC(50), y + SC(26), x + w - SC(230), y + SC(46)}; TextR(dc, g_fSmall, C_DIM, sr, sv[i].slot, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        SYSTEMTIME s; FILETIME lt; FileTimeToLocalFileTime(&sv[i].mt, &lt); FileTimeToSystemTime(&lt, &s);
        wchar_t d[80]; _snwprintf(d, 80, L"%02d/%02d/%04d %02d:%02d  ·  %.1f MB", s.wDay, s.wMonth, s.wYear, s.wHour, s.wMinute, sv[i].size / 1048576.0);
        RECT dr = {x + w - SC(230), y, x + w - SC(16), y + SC(50)}; TextR(dc, g_fSmall, C_SUB, dr, d, DT_RIGHT | DT_VCENTER);
        AddHit(r.left, r.top, r.right, r.bottom, H_SAVEITEM + i);
        y += SC(56);
    }
    if (n > 7) { wchar_t more[64]; _snwprintf(more, 64, L2(L"e mais %d...", L"and %d more..."), n - 7); Text(dc, g_fSmall, C_DIM, x, y, more); y += SC(24); }
    /* Released (11-13): "Import saves..." is the fifth button (UFC 1-4: the port's own importer, extras.c SavesImport) */
    int by = pr.bottom - SC(118), nb = PS4G(gsel) ? 5 : 4, bw = (w - (nb - 1) * SC(12)) / nb;
    RECT b1 = {x, by, x + bw, by + SC(44)}; IconButton(dc, g, b1, nb == 5 ? L2(L"Fazer backup", L"Back up now") : L2(L"Fazer backup agora", L"Back up now"), H_SAVEBACKUP, 1, 1);   /* five buttons: "agora" did not fit */
    RECT b2 = {b1.right + SC(12), by, b1.right + SC(12) + bw, by + SC(44)}; IconButton(dc, g, b2, L2(L"Restaurar backup", L"Restore a backup"), H_SAVERESTORE, 0, 4);
    RECT b5 = {b2.right + SC(12), by, b2.right + SC(12) + bw, by + SC(44)}; if (nb == 5) IconButton(dc, g, b5, L2(L"Importar saves...", L"Import saves..."), H_SAVEIMPORT, 0, 5); else b5.right = b2.right;
    RECT b3 = {b5.right + SC(12), by, b5.right + SC(12) + bw, by + SC(44)}; IconButton(dc, g, b3, L2(L"Pasta dos saves", L"Saves folder"), H_SAVEOPEN, 0, 0);
    RECT b4 = {b3.right + SC(12), by, x + w, by + SC(44)}; IconButton(dc, g, b4, L2(L"Pasta dos backups", L"Backups folder"), H_SAVEDELBAK, 0, 0);
    SwitchW(dc, g, x, by + SC(58), w, PIni(L"save_autobackup", 1), L2(L"Backup automático antes de cada JOGAR (guarda os 8 últimos)", L"Automatic backup before each PLAY (keeps the last 8)"), H_SAVEAUTO);
}
static void PaintCheats(HDC dc, GpGraphics *g, RECT pr)
{
    int gsel = g_sel; CheatsLoad(gsel);
    PanelTitle(dc, g, pr, L2(L"Trapaças", L"Cheats"), L2(L"Ligue e aperte JOGAR. Só para os modos offline (Carreira, lutas): desligue antes de jogar online. Se o jogo travar com uma trapaça, desligue ela.",
                                                       L"Turn on and press PLAY. For the offline modes only (Career, fights): turn them off before playing online. If the game hangs with a cheat on, turn it off."));
    int y = pr.top + SC(108), x = pr.left + SC(24), w = pr.right - pr.left - SC(48);
    if (!g_ncheat) {
        NoteBox(dc, g, x, y, w, SC(96), gsel == 3 ? L2(L"As trapaças do UFC 4 não estão nesta instalação (games\\UFC4\\cheats). Extraia o pacote do PORTHEON de novo.", L"UFC 4's cheats are not in this install (games\\UFC4\\cheats). Extract the PORTHEON package again.")
                                         : L2(L"Ainda não há trapaças para este jogo. As trapaças do PORTHEON vêm da comunidade (GoldHEN) e são conferidas no código do próprio jogo antes de entrar aqui; o UFC 4 já tem as suas. Mande sugestões no Discord.",
                                              L"No cheats for this game yet. PORTHEON's cheats come from the community (GoldHEN) and are checked on the game's own code before they land here; UFC 4 has its own already. Send suggestions on Discord."), FALSE);
        return;
    }
    for (int i = 0; i < g_ncheat && y < pr.bottom - SC(120); i++) {
        RECT r = {x, y, x + w, y + SC(62)}; BOOL hot = g_hot == H_CHEAT + i, on = g_cheat[i].on;
        Box(g, r, SC(12), A(on ? RGB(40, 30, 10) : RGB(255, 255, 255), on ? 110 : (hot ? 22 : 9)), A(on ? C_ORANGE : hot ? C_HOT : C_LINE, 255), 1.0f);
        RECT sw = {x + SC(16), y + SC(19), x + SC(60), y + SC(43)};
        if (on) GradBox(g, sw, SC(12), A(C_ORANGE, 255), A(RGB(255, 98, 92), 255), TRUE, 0); else Box(g, sw, SC(12), A(RGB(28, 22, 44), 255), A(hot ? C_HOT : C_FIELDB, 255), 1.0f);
        Dot(g, on ? sw.right - SC(12) : sw.left + SC(12), (sw.top + sw.bottom) / 2, SC(9), on ? 0xFFFFFFFF : 0xFF8A92AA);
        RECT tr = {x + SC(76), y + SC(8), x + w - SC(16), y + SC(30)}; TextR(dc, g_fBtn, RGB(255, 255, 255), tr, g_cheat[i].name, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        RECT dr = {x + SC(76), y + SC(30), x + w - SC(16), y + SC(56)}; TextR(dc, g_fSmall, C_SUB, dr, g_cheat[i].desc, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        AddHit(r.left, r.top, r.right, r.bottom, H_CHEAT + i);
        y += SC(68);
    }
    int by = pr.bottom - SC(64);
    RECT b1 = {x, by, x + SC(220), by + SC(42)}; Button(dc, g, b1, L2(L"Desligar todas", L"Turn all off"), H_CHEATSOFF, 2);
    RECT nt = {b1.right + SC(16), by, x + w, by + SC(42)};
    TextR(dc, g_fSmall, C_DIM, nt, L2(L"Valem a partir do próximo JOGAR. As marcadas como experimentais ainda não foram testadas numa luta.", L"They apply from the next PLAY. The ones marked experimental were not tested in a fight yet."), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);   /* 6.0 final: no credits shown (the user's rule; the sources are in each pack's "about") */
}
static void PaintDlc(HDC dc, GpGraphics *g, RECT pr)
{
    int gsel = g_sel;
    PanelTitle(dc, g, pr, L"DLC", NULL);
    int y = pr.top + SC(70), x = pr.left + SC(24), w = pr.right - pr.left - SC(48);
    const wchar_t *a = L2(L"O que já funciona: tudo o que o jogo desbloqueia jogando (lutadores, roupas e itens ganhos na Carreira e nos desafios) e o conteúdo dos updates que vem dentro do próprio jogo. Exemplo: no UFC 3, o Bruce Lee vem terminando a Carreira e entrando no Hall da Fama.",
                          L"What already works: everything the game unlocks as you play (fighters, outfits and items earned in Career and challenges) and the update content that comes inside the game itself. Example: in UFC 3, Bruce Lee comes from finishing Career and entering the Hall of Fame.");
    const wchar_t *b = L2(L"DLC comprada à parte (pacotes de lutadores, por exemplo): o motor dos ports ainda não monta conteúdo adicional (add-ons) do PS4, então uma pasta de DLC ainda não é lida. Isso depende do motor (SacredFire), não do PORTHEON: quando ele ganhar esse suporte, a pasta de DLC aparece aqui.",
                          L"DLC bought separately (fighter packs, for example): the ports' engine does not mount PS4 add-on content yet, so a DLC folder is not read yet. That is up to the engine (SacredFire), not PORTHEON: once it gets that support, the DLC folder shows up here.");
    RECT r1 = {0, 0, w - SC(48), 0}; int h1 = Wrap(dc, g_fSmall, C_SUB, r1, a, FALSE) + SC(20); NoteBox(dc, g, x, y, w, h1, a, FALSE); y += h1 + SC(10);
    int h2 = Wrap(dc, g_fSmall, C_SUB, r1, b, FALSE) + SC(20); NoteBox(dc, g, x, y, w, h2, b, TRUE); y += h2 + SC(10);
    wchar_t c[300]; _snwprintf(c, 300, L2(L"O %s precisa só do jogo base + o update %s. Trapaças, saves e perfis ficam nas outras seções.", L"%s only needs the base game + update %s. Cheats, saves and profiles are in the other sections."), G[gsel].name, G[gsel].upd);
    int h3 = Wrap(dc, g_fSmall, C_SUB, r1, c, FALSE) + SC(20); NoteBox(dc, g, x, y, w, h3, c, FALSE);
}

/* 5.0: the system screens' titles, in the home screen's poster style: a violet blade, a spaced eyebrow, the title */
static void ScreenTitle(HDC dc, GpGraphics *g, const wchar_t *eyebrow, const wchar_t *title)
{
    int x = SC(MX + 12);
    (void)g;   /* 7.0 fix: no colored bar before the eyebrow */
    Spaced(dc, g_fTag, RGB(206, 198, 228), x, SC(70), eyebrow, 3);
    Text(dc, g_fTitle, RGB(255, 255, 255), x - SC(2), SC(84), title);
}
/* ---------------------------------------------------------------- the system settings */
static const struct { const wchar_t *pt, *en; int icon; } SETS[NSETS] = {
    {L"Idioma", L"Language", 0}, {L"Perfis", L"Profiles", 1}, {L"Interface", L"Interface", 2}, {L"Atualizações", L"Updates", 3}, {L"Sobre e contatos", L"About and contacts", 4}};
static const Field FS_SOUNDS = {S_COUNT, L"pini:ui_sounds", NULL, 0, 1}, FS_BOOT = {S_COUNT, L"pini:boot_anim", NULL, 0, 1}, FS_PICK = {S_COUNT, L"pini:profile_pick", NULL, 0, 1},
    FS_PAD = {S_COUNT, L"pini:pad_nav", NULL, 0, 1}, FS_CHECK = {S_COUNT, L"pini:check_game_files", NULL, 0, 1}, FS_UPD = {S_COUNT, L"pini:update_check", NULL, 0, 1},
    FS_FULL = {S_COUNT, L"pini:ui_fullscreen", NULL, 0, 1},   /* 4.0.4: full screen by default */
    FS_CLOSE = {S_COUNT, L"pini:close_on_play", NULL, 0, 1},   /* 4.3.6: PORTHEON closes while a game runs */
    FS_BETA = {S_COUNT, L"pini:beta_updates", NULL, 0, 0},     /* 4.3.8: the test-versions channel (update.c, BetaRelease) */
    FS_FIXES = {S_COUNT, L"pini:remote_fixes", NULL, 0, 1},    /* 4.3.8: the remote fix list (remote.c) */
    FS_SPLASH = {S_COUNT, L"pini:launch_splash", NULL, 0, 1},  /* 4.4.0: the game's picture while it loads (PaintLaunch) */
    FS_MUSIC = {S_COUNT, L"pini:game_music", NULL, 0, 1},      /* 6.0: the chosen game's music (music.c) */
    FS_HAPTIC = {S_COUNT, L"pini:pad_haptics", NULL, 0, 1};    /* 6.0: each game's vibration on the controller (pad.c) */
static const wchar_t *SetLabel(const Field *f)
{
    if (f == &FS_SOUNDS) return L2(L"Sons da interface", L"Interface sounds");
    if (f == &FS_MUSIC) return L2(L"Música do jogo escolhido (como no console)", L"The chosen game's music (as on a console)");
    if (f == &FS_HAPTIC) return L2(L"Vibração do controle em cada jogo", L"Controller vibration for each game");
    if (f == &FS_BOOT) return L2(L"Animação de abertura (logo do PORTHEON)", L"Opening animation (PORTHEON logo)");
    if (f == &FS_SPLASH) return L2(L"Tela do jogo enquanto ele carrega (como num console)", L"The game's screen while it loads (as on a console)");
    if (f == &FS_PICK) return L2(L"Perguntar quem vai jogar ao abrir", L"Ask who is playing at start");
    if (f == &FS_PAD) return L2(L"Navegar pelo PORTHEON com o controle", L"Drive PORTHEON with the controller");
    if (f == &FS_CHECK) return L2(L"Conferir os arquivos do jogo antes de jogar", L"Check the game's files before playing");
    if (f == &FS_UPD) return L2(L"Procurar atualizações do PORTHEON sozinho", L"Look for PORTHEON updates by itself");
    if (f == &FS_FULL) return L2(L"Tela cheia (F11)", L"Full screen (F11)");
    if (f == &FS_BETA) return L2(L"Receber versões de teste (para quem ajuda a testar)", L"Get test versions (for those who help testing)");
    if (f == &FS_FIXES) return L2(L"Receber correções pela internet (sem precisar de versão nova)", L"Get fixes over the Internet (no new version needed)");
    if (f == &FS_LIVE) return L2(L"Painéis ao vivo na tela inicial (PC, controles, atividade)", L"Live panels on the home screen (PC, controllers, activity)");   /* 6.2 */
    if (f == &FS_GSKIN) return L2(L"Visual próprio de cada jogo (cores, formas e desenhos)", L"Each game's own look (colors, shapes and drawings)");   /* 6.2 */
    if (f == &FS_CLOSE) return L2(L"Fechar o PORTHEON enquanto o jogo roda (só o jogo fica aberto)", L"Close PORTHEON while the game runs (only the game stays open)");
    return T(f->label);
}
typedef struct { int icon; const wchar_t *label, *value, *url; } LinkInfo;
static LinkInfo g_links[20]; static int g_nlinks; static wchar_t g_linkText[20][2][160];
static void LoadLinks(void)   /* the author's links: the built-in ones, then links.txt next to PORTHEON.exe ("Name | https://...") */
{
    g_nlinks = 0;
    g_links[g_nlinks++] = (LinkInfo){7, L"GitHub", L"github.com/" AUTHOR_NICK L"/PORTHEON", URL_AUTHOR};
    g_links[g_nlinks++] = (LinkInfo){0, L"Discord", L"discord.gg/FEQ752rUz", URL_DISCORD};   /* 4.4.0: the phone and the PIX key came out */
    wchar_t p[MAX_PATH]; Join(p, g_exeDir, L"links.txt");
    DWORD n = 0; BYTE *b = ReadWhole(p, &n); if (!b) return;
    char *s = (char *)realloc(b, n + 1); if (!s) { free(b); return; } s[n] = 0;
    char *first = (n >= 3 && (BYTE)s[0] == 0xEF && (BYTE)s[1] == 0xBB && (BYTE)s[2] == 0xBF) ? s + 3 : s;   /* Notepad's UTF-8 mark */
    for (char *line = first; *line && g_nlinks < 20; ) {
        char *e = strchr(line, '\n'); if (e) *e = 0; char *cr = strchr(line, '\r'); if (cr) *cr = 0;
        char *bar = strchr(line, '|');
        if (bar && line[0] != '#') {
            *bar = 0; char *nm = line, *url = bar + 1; while (*nm == ' ') nm++; while (*url == ' ') url++;
            size_t l = strlen(nm); while (l && nm[l - 1] == ' ') nm[--l] = 0;
            int k = g_nlinks; MultiByteToWideChar(CP_UTF8, 0, nm, -1, g_linkText[k][0], 160); MultiByteToWideChar(CP_UTF8, 0, url, -1, g_linkText[k][1], 160);
            if (!wcsncmp(g_linkText[k][1], L"https://", 8) || !wcsncmp(g_linkText[k][1], L"http://", 7)) { g_links[g_nlinks++] = (LinkInfo){7, g_linkText[k][0], g_linkText[k][1] + (g_linkText[k][1][4] == L's' ? 8 : 7), g_linkText[k][1]}; }
        }
        if (!e) break; line = e + 1;
    }
    free(s);
}
static void PaintSettings(HDC dc, GpGraphics *g)
{
    ScreenTitle(dc, g, L2(L"PORTHEON  ·  SISTEMA", L"PORTHEON  ·  SYSTEM"), L2(L"Configurações", L"Settings"));
    int my = SC(148);
    for (int k = 0; k < NSETS; k++) {
        RECT r = {SC(MX + 12), my + k * SC(50), SC(MX + 12) + SC(246), my + k * SC(50) + SC(42)}; BOOL on = g_setSec == k, hot = g_hot == H_SETSEC + k;
        if (on) Box(g, r, SC(21), A(RGB0(255, 255, 255), 30), A(RGB0(255, 255, 255), 40), 1.0f);   /* 7.0 fix: a pill, no bar (as the hub's rail) */
        else if (hot) Box(g, r, SC(21), A(RGB0(255, 255, 255), 16), 0, 0);
        UINT32 ic = A(on || hot ? RGB(255, 255, 255) : C_SUB, 255); int ix = r.left + SC(16), iy = r.top + SC(12), is = SC(18);
        switch (SETS[k].icon) { case 0: IconGlobe(g, ix, iy, is, ic); break; case 1: IconUser(g, ix, iy, is, ic); break; case 2: IconSliders(g, ix, iy, is, ic); break; case 3: IconDownload(g, ix, iy, is, ic); break; default: IconInfo(g, ix, iy, is, ic); }
        RECT tr = {r.left + SC(48), r.top, r.right - SC(8), r.bottom}; TextR(dc, g_fBtn, on || hot ? RGB(255, 255, 255) : C_TEXT, tr, TR(SETS[k].pt, SETS[k].en), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        AddHit(r.left, r.top, r.right, r.bottom, H_SETSEC + k);
    }
    RECT bb = {SC(MX + 12), my + NSETS * SC(50) + SC(10), SC(MX + 12) + SC(246), my + NSETS * SC(50) + SC(52)}; Button(dc, g, bb, L2(L"‹ Voltar", L"‹ Back"), H_BACK, 2);
    RECT pr = {SC(MX + 274), SC(140), SC(MX + CW), SC(742)};
    if (HOME_PAGES) Fill(g, pr.left, pr.top + SC(8), 1, pr.bottom - pr.top - SC(16), A(RGB0(255, 255, 255), 30));   /* Released (the console's settings: a plain column of sections, plain rows beside it): no glass frame, a thin line between the two */
    else Panel(g, pr);
    int x = pr.left + SC(28), y = pr.top + SC(24), w = pr.right - pr.left - SC(56);
    switch (g_setSec) {
    case SET_LANG: {
        Text(dc, g_fH2, RGB(255, 255, 255), x, y, L2(L"Idioma do PORTHEON", L"PORTHEON's language")); y += SC(52);
        int cw = (w - 3 * SC(12)) / 4;
        for (int i = 0; i < NLANG; i++) {
            RECT r = {x + (i % 4) * (cw + SC(12)), y + (i / 4) * SC(60), x + (i % 4) * (cw + SC(12)) + cw, y + (i / 4) * SC(60) + SC(50)};
            BOOL on = g_lang == i, hot = g_hot == H_LANGPICK + i;
            if (on) Box(g, r, SC(25), A(C_HOT, 245), 0, 0); else Box(g, r, SC(25), A(RGB(255, 255, 255), hot ? 26 : 10), A(hot ? C_HOT : C_FIELDB, 255), 1.0f);   /* 7.0 fix: pills, as every button */
            /* each language's name in its own script needs its own font */
            const wchar_t *face = i == LANG_JA ? L"Yu Gothic UI" : i == LANG_KO ? L"Malgun Gothic" : i == LANG_ZHS ? L"Microsoft YaHei UI" : i == LANG_ZHT ? L"Microsoft JhengHei UI" : L"Segoe UI";
            HFONT f = CreateFontW(-SC(16), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, face);
            TextR(dc, f, on ? C_INK : hot ? RGB(255, 255, 255) : C_TEXT, r, LANGS[i].name, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS); DeleteObject(f);
            AddHit(r.left, r.top, r.right, r.bottom, H_LANGPICK + i);
        }
        y += 4 * SC(60) + SC(16);
        wchar_t gl[400]; _snwprintf(gl, 400, L2(L"O idioma dos próprios jogos (textos e narração) fica em cada jogo, em Jogabilidade > Idioma do jogo. Use o botão abaixo para colocar todos os jogos em %s (quando o jogo tem esse idioma; senão ele mostra inglês).",
                                                L"The games' own language (texts and commentary) is in each game, under Gameplay > Game language. Use the button below to set every game to %s (when the game has that language; otherwise it shows English)."), LANGS[g_lang].name);
        RECT r1 = {0, 0, w - SC(48), 0}; int h = Wrap(dc, g_fSmall, C_SUB, r1, gl, FALSE) + SC(20); NoteBox(dc, g, x, y, w, h, gl, FALSE); y += h + SC(10);
        RECT b = {x, y, x + SC(340), y + SC(44)}; Button(dc, g, b, L2(L"Usar este idioma nos 4 jogos", L"Use this language in all 4 games"), H_LANGBTN, 0);
        break; }
    case SET_PROFILES: {
        Text(dc, g_fH2, RGB(255, 255, 255), x, y, L2(L"Perfis", L"Profiles")); y += SC(52);
        for (int i = 0; i < g_nprof; i++) {
            RECT r = {x, y, x + w, y + SC(70)}; BOOL hot = g_hot == H_PROFPICK + i;
            Box(g, r, SC(14), A(RGB(255, 255, 255), hot ? 22 : 9), A(i == g_curProf ? C_CYAN : hot ? C_HOT : C_LINE, 255), 1.0f);
            AvatarSt(dc, g, x + SC(40), y + SC(35), SC(24), &g_prof[i], FALSE);
            Text(dc, g_fBtn, RGB(255, 255, 255), x + SC(80), y + SC(14), g_prof[i].name);
            { int nw = TextW(dc, g_fBtn, g_prof[i].name); Text(dc, g_fSmall, ProfStatusColor(g_prof[i].status), x + SC(92) + nw, y + SC(16), ProfStatusName(g_prof[i].status)); }   /* 4.4.0 */
            wchar_t s[200]; if (g_prof[i].id) _snwprintf(s, 200, L2(L"Saves próprios  ·  %%LOCALAPPDATA%%\\UFCnNative\\profiles\\%d", L"Own saves  ·  %%LOCALAPPDATA%%\\UFCnNative\\profiles\\%d"), g_prof[i].id);
            else _snwprintf(s, 200, L"%s", L2(L"Os saves que já estavam neste PC", L"The saves that were already on this PC"));
            Text(dc, g_fSmall, C_SUB, x + SC(80), y + SC(40), s);
            if (i == g_curProf) { RECT t = {x + w - SC(240), y, x + w - SC(110), y + SC(70)}; TextR(dc, g_fSmall, C_CYAN, t, L2(L"jogando agora", L"playing now"), DT_RIGHT | DT_VCENTER); }
            RECT eb = {x + w - SC(96), y + SC(16), x + w - SC(16), y + SC(54)}; Box(g, eb, SC(19), A(RGB(255, 255, 255), hot ? 30 : 12), A(hot ? C_HOT : C_FIELDB, 255), 1.0f);
            TextR(dc, g_fBtn, hot ? RGB(255, 255, 255) : C_TEXT, eb, L2(L"Editar", L"Edit"), DT_CENTER | DT_VCENTER);
            AddHit(r.left, r.top, r.right, r.bottom, H_PROFPICK + i);
            y += SC(80);
        }
        if (g_nprof < MAXPROF) { RECT b = {x, y + SC(6), x + SC(240), y + SC(50)}; IconButton(dc, g, b, L2(L"Novo perfil", L"New profile"), H_PROFADD, 1, 5); y += SC(64); }
        const wchar_t *nt = L2(L"Cada perfil tem os próprios saves e o próprio nome nos jogos (o nome que aparece no jogo). O cache de shaders é de todos, então um perfil novo não fica engasgando. O primeiro perfil usa os saves que já estavam neste PC.",
                               L"Each profile has its own saves and its own name in the games (the name the game shows). The shader cache is shared, so a new profile does not stutter. The first profile uses the saves that were already on this PC.");
        RECT r1 = {0, 0, w - SC(48), 0}; int h = Wrap(dc, g_fSmall, C_SUB, r1, nt, FALSE) + SC(20); NoteBox(dc, g, x, y, w, h, nt, FALSE);
        break; }
    case SET_UI: {
        Text(dc, g_fH2, RGB(255, 255, 255), x, y, L2(L"Interface", L"Interface")); y += SC(48);   /* 6.2: 48 (one more row) */
        static const Field *sw[] = {&FS_GSKIN, &FS_SOUNDS, &FS_HAPTIC, &FS_LIVE, &FS_BOOT, &FS_SPLASH, &FS_PICK, &FS_PAD, &FS_CHECK, &FS_FULL, &FS_CLOSE, &F_SHORTCUT};
        for (int i = 0; i < (int)(sizeof(sw) / sizeof(sw[0])); i++) { g_swF[i] = sw[i]; SwitchW(dc, g, x, y, w, sw[i] == &FS_FULL ? g_full : PIni(sw[i]->key + 5, sw[i]->def) != 0, SetLabel(sw[i]), H_SWITCH + i); y += SC(27); }   /* 6.0: 11 switches: closer rows (6.2: 12, closer still) */
        {   /* 5.0: the visual effects: automatic (light on weak PCs, remote desktop and battery saver), full, light */
            y += SC(6); Text(dc, g_fText, C_TEXT, x, y + SC(5), L2(L"Efeitos visuais", L"Visual effects"));
            static const wchar_t *const fp[3] = {L"Automático", L"Completo", L"Leve"}, *const fe[3] = {L"Automatic", L"Full", L"Light"};
            int m = PIni(L"ui_effects", 0), sx = x + SC(200);
            for (int k = 0; k < 3; k++) {
                RECT r = {sx + k * SC(150), y, sx + k * SC(150) + SC(142), y + SC(34)}; BOOL on = m == k, hot = g_hot == H_FXPICK + k;
                COLORREF ink = RGB0(255, 255, 255);
                if (on && g_themeMono) ink = AccentBox(g, r, SC(17));   /* 6.0 final: white with dark letters, as PLAY; 7.0 fix: pills */
                else if (on) NeonFill(g, r, SC(17), 0.0f, 255); else Box(g, r, SC(17), A(RGB(200, 180, 255), hot ? 26 : 8), A(hot ? C_CYAN : C_FIELDB, 255), 1.0f);
                TextR(dc, g_fBtn, on ? ink : hot ? RGB(255, 255, 255) : C_TEXT, r, TR(fp[k], fe[k]), DT_CENTER | DT_VCENTER);
                AddHit(r.left, r.top, r.right, r.bottom, H_FXPICK + k);
            }
            const wchar_t *now = g_fxLight ? L2(L"agora: leve", L"now: light") : L2(L"agora: completo", L"now: full");
            Text(dc, g_fSmall, C_SUB, sx + 3 * SC(150) + SC(8), y + SC(9), now);
            y += SC(40);
        }
        {   /* 6.2: the vitrine of each game: the player's videos and PORTHEON's live edits, the edits only, or nothing */
            Text(dc, g_fText, C_TEXT, x, y + SC(5), L2(L"Vídeos e edits dos jogos", L"Game videos and edits"));
            static const wchar_t *const vp[3] = {L"Ligado", L"Só locais", L"Desligado"}, *const ve[3] = {L"On", L"Local only", L"Off"};
            int m = VitMode(), sx = x + SC(200);
            for (int k = 0; k < 3; k++) {
                RECT r = {sx + k * SC(150), y, sx + k * SC(150) + SC(142), y + SC(34)}; BOOL on = m == k, hot = g_hot == H_VITMODE + k;
                COLORREF ink = RGB0(255, 255, 255);
                if (on && g_themeMono) ink = AccentBox(g, r, SC(17));
                else if (on) NeonFill(g, r, SC(17), 0.0f, 255); else Box(g, r, SC(17), A(RGB(200, 180, 255), hot ? 26 : 8), A(hot ? C_CYAN : C_FIELDB, 255), 1.0f);
                TextR(dc, g_fBtn, on ? ink : hot ? RGB(255, 255, 255) : C_TEXT, r, TR(vp[k], ve[k]), DT_CENTER | DT_VCENTER);
                AddHit(r.left, r.top, r.right, r.bottom, H_VITMODE + k);
            }
            RECT nr = {sx + 3 * SC(150) + SC(8), y, x + w, y + SC(34)};
            TextR(dc, g_fSmall, C_SUB, nr, m == 0 ? L2(L"YouTube + vídeos + edits", L"YouTube + videos + edits") : m == 1 ? L2(L"só vídeos e edits locais", L"local videos and edits only") : L2(L"nada no quadro", L"nothing in the panel"), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
            y += SC(40);
        }
        {   /* 6.2: the YouTube edits play with their music (PORTHEON's own music pauses meanwhile); M / R3 too */
            Text(dc, g_fText, C_TEXT, x, y + SC(5), L2(L"Som dos vídeos", L"Video sound"));
            static const wchar_t *const sp[2] = {L"Ligado", L"Desligado"}, *const se[2] = {L"On", L"Off"};
            int m = YtSoundPref() ? 0 : 1, sx = x + SC(200);
            for (int k = 0; k < 2; k++) {
                RECT r = {sx + k * SC(150), y, sx + k * SC(150) + SC(142), y + SC(34)}; BOOL on = m == k, hot = g_hot == H_YTSNDPICK + k;
                COLORREF ink = RGB0(255, 255, 255);
                if (on && g_themeMono) ink = AccentBox(g, r, SC(17));
                else if (on) NeonFill(g, r, SC(17), 0.0f, 255); else Box(g, r, SC(17), A(RGB(200, 180, 255), hot ? 26 : 8), A(hot ? C_CYAN : C_FIELDB, 255), 1.0f);
                TextR(dc, g_fBtn, on ? ink : hot ? RGB(255, 255, 255) : C_TEXT, r, TR(sp[k], se[k]), DT_CENTER | DT_VCENTER);
                AddHit(r.left, r.top, r.right, r.bottom, H_YTSNDPICK + k);
            }
            RECT nr = {sx + 2 * SC(150) + SC(8), y, x + w, y + SC(34)};
            TextR(dc, g_fSmall, C_SUB, nr, m == 0 ? L2(L"os vídeos tocam com som  ·  M / R3", L"the videos play with sound  ·  M / R3")
                                                  : L2(L"vídeos sem som  ·  M / R3 liga", L"videos muted  ·  M / R3 turns it on"), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
            y += SC(40);
        }
        {   /* 6.0: PORTHEON's color (theme.c): a round swatch of each, the chosen one ringed as on the console; the logo's
               black and white (the default) comes first, then the seven colors */
            Text(dc, g_fText, C_TEXT, x, y + SC(8), L2(L"Cor do PORTHEON", L"PORTHEON's color"));
            int sx = x + SC(200);
            for (int pos = 0; pos < NTHEMES; pos++) {
                int k = pos == 0 ? THEME_MONO : pos - 1;
                int cx = sx + pos * SC(58) + SC(18), cy = y + SC(18), rr0 = SC(14); BOOL on = g_theme == k, hot = g_hot == H_THEMEPICK + k;
                if (THEMES[k].hue < 0.0f) {   /* white on the left, the logo's black on the right */
                    Dot(g, cx, cy, rr0, A(RGB0(244, 244, 246), 255));
                    GpBrush *bk = NULL; GdipCreateSolidFill(A(RGB0(12, 12, 14), 255), &bk); if (bk) { GdipFillPieI(g, bk, cx - rr0, cy - rr0, 2 * rr0, 2 * rr0, -90.0f, 180.0f); GdipDeleteBrush(bk); }
                    Ring(g, cx, cy, rr0, A(RGB0(255, 255, 255), 200), 1.2f);
                } else {
                    float hd = THEMES[k].hue; int hi = (int)(hd / 60.0f) % 6; float f = hd / 60.0f - (int)(hd / 60.0f);
                    float vv = 0.93f, ss = 0.75f, p = vv * (1 - ss), q = vv * (1 - ss * f), t2 = vv * (1 - ss * (1 - f)), rr, gg, bb;
                    switch (hi) { case 0: rr = vv; gg = t2; bb = p; break; case 1: rr = q; gg = vv; bb = p; break; case 2: rr = p; gg = vv; bb = t2; break;
                                  case 3: rr = p; gg = q; bb = vv; break; case 4: rr = t2; gg = p; bb = vv; break; default: rr = vv; gg = p; bb = q; }
                    Dot(g, cx, cy, rr0, A(RGB0((int)(rr * 255), (int)(gg * 255), (int)(bb * 255)), 255));   /* the swatch's own color, untouched by the theme */
                }
                if (on || hot) Ring(g, cx, cy, rr0 + SC(5), A(RGB0(255, 255, 255), on ? 255 : 140), on ? 2.4f : 1.4f);
                AddHit(cx - SC(24), cy - SC(22), cx + SC(24), cy + SC(22), H_THEMEPICK + k);
            }
            RECT nr = {sx + NTHEMES * SC(58) + SC(6), y, x + w, y + SC(36)}; TextR(dc, g_fSmall, C_SUB, nr, TR(THEMES[g_theme].pt, THEMES[g_theme].en), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
            y += SC(50);
        }
        break; }   /* 6.0: the keys' help left this page (it no longer fits with the music and vibration switches; the console shows none) */
    case SET_UPDATES: {
        Text(dc, g_fH2, RGB(255, 255, 255), x, y, L2(L"Atualizações", L"Updates")); y += SC(56);
        wchar_t v[200]; _snwprintf(v, 200, L2(L"Você tem o Portheon %s.", L"You have Portheon %s."), APP_SHOW); Text(dc, g_fBtn, RGB(255, 255, 255), x, y, v); y += SC(34);
        if (g_updState >= 1) { _snwprintf(v, 200, L2(L"A versão %s está disponível.", L"Version %s is available."), g_updTag); Text(dc, g_fText, C_CYAN, x, y, v); y += SC(30); }
        RECT b1 = {x, y + SC(8), x + SC(260), y + SC(52)}; IconButton(dc, g, b1, g_updState >= 1 ? L2(L"Atualizar agora", L"Update now") : L2(L"Procurar atualização", L"Check for updates"), H_UPDCHECK, 1, 4);
        RECT b2 = {b1.right + SC(12), y + SC(8), b1.right + SC(12) + SC(220), y + SC(52)}; Button(dc, g, b2, L2(L"Novidades da versão", L"What's new"), H_CHANGELOG, 0);
        y += SC(70);
        g_swF[0] = &FS_UPD; SwitchW(dc, g, x, y, w, PIni(L"update_check", 1) != 0, SetLabel(&FS_UPD), H_SWITCH + 0); y += SC(50);
        g_swF[1] = &FS_FIXES; SwitchW(dc, g, x, y, w, PIni(L"remote_fixes", 1) != 0, SetLabel(&FS_FIXES), H_SWITCH + 1); y += SC(50);   /* 4.3.8 */
        g_swF[2] = &FS_BETA; SwitchW(dc, g, x, y, w, PIni(L"beta_updates", 0) != 0, SetLabel(&FS_BETA), H_SWITCH + 2); y += SC(50);   /* 4.3.8 */
        const wchar_t *nt = L2(L"A atualização baixa da página oficial do PORTHEON no GitHub, confere o pacote (SHA-256) e se instala sozinha. Configurações, perfis, saves e caches ficam.",
                               L"The update downloads from PORTHEON's official GitHub page, checks the package (SHA-256) and installs itself. Settings, profiles, saves and caches stay.");
        RECT r1 = {0, 0, w - SC(48), 0}; int h = Wrap(dc, g_fSmall, C_SUB, r1, nt, FALSE) + SC(20); NoteBox(dc, g, x, y, w, h, nt, FALSE);
        break; }
    default: {   /* about and contacts */
        if (g_imgLogo) GdipDrawImageRectI(g, g_imgLogo, x, y, SC(96), SC(96));
        Text(dc, g_fTitle, RGB(255, 255, 255), x + SC(116), y + SC(6), L"Portheon " APP_SHOW);
        wchar_t by[200]; _snwprintf(by, 200, L2(L"Ports & Mods  ·  criado por %s (%s)", L"Ports & Mods  ·  created by %s (%s)"), AUTHOR_NAME, AUTHOR_NICK); Text(dc, g_fBtn, C_CYAN, x + SC(118), y + SC(56), by);
        wchar_t cp[200]; _snwprintf(cp, 200, L2(L"© 2026 %s. Todos os direitos sobre o nome, o logo e a interface do PORTHEON. Código: GPL-2.0-or-later.", L"© 2026 %s. All rights to PORTHEON's name, logo and interface. Code: GPL-2.0-or-later."), AUTHOR_NAME);
        Text(dc, g_fSmall, C_SUB, x + SC(118), y + SC(80), cp);
        y += SC(118);
        if (!g_nlinks) LoadLinks();
        int cw = (w - SC(12)) / 2;
        for (int i = 0; i < g_nlinks; i++) {
            RECT r = {x + (i % 2) * (cw + SC(12)), y + (i / 2) * SC(54), x + (i % 2) * (cw + SC(12)) + cw, y + (i / 2) * SC(54) + SC(46)}; BOOL hot = g_hot == H_LINK + i;
            Box(g, r, SC(23), A(RGB(255, 255, 255), hot ? 26 : 10), A(hot ? C_HOT : C_FIELDB, 255), 1.0f);   /* 7.0 fix: a pill */
            int is = SC(18), ix = r.left + SC(18), iy = r.top + SC(14); UINT32 ic = A(hot ? C_HOT : C_SUB, 255);
            if (g_links[i].icon == 0) IconChat(g, ix, iy, is, ic); else IconLink(g, ix, iy, is, ic);
            int split = ix + is + SC(12) + TextW(dc, g_fBtn, g_links[i].label) + SC(12); if (split > r.left + cw * 45 / 100) split = r.left + cw * 45 / 100;   /* 7.0 fix: the address takes what the name leaves (GitHub's was cut with "...") */
            RECT t1 = {ix + is + SC(12), r.top, split, r.bottom}; TextR(dc, g_fBtn, RGB(255, 255, 255), t1, g_links[i].label, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
            RECT t2 = {split, r.top, r.right - SC(18), r.bottom}; TextR(dc, g_fSmall, hot ? C_TEXT : C_SUB, t2, g_links[i].value, DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
            AddHit(r.left, r.top, r.right, r.bottom, H_LINK + i);
        }
        y += ((g_nlinks + 1) / 2) * SC(54) + SC(8);
        const wchar_t *cr = L2(L"Sem afiliação com a Electronic Arts, o UFC, a THQ, a Sony ou a Microsoft. É preciso ter a sua própria cópia de cada jogo. As licenças de código aberto estão na pasta licenses.",
                               L"Not affiliated with Electronic Arts, the UFC, THQ, Sony or Microsoft. You need your own copy of each game. The open-source licenses are in the licenses folder.");
        RECT r1 = {0, 0, w - SC(48), 0}; int h = Wrap(dc, g_fSmall, C_SUB, r1, cr, FALSE) + SC(20); if (y + h < pr.bottom) NoteBox(dc, g, x, y, w, h, cr, FALSE);
    }
    }
}

/* ---------------------------------------------------------------- the install helper (6 steps) */
typedef struct { const wchar_t *title, *body; } Step;
#define NSTEPS 6
static void InstallSteps(int g, Step *s, wchar_t bufs[NSTEPS][1200])
{
    s[0].title = L2(L"O que você precisa", L"What you need");
    _snwprintf(bufs[0], 1200, L2(L"O %s do PORTHEON roda o jogo de PS4 (%s) direto no PC, sem emulador. Você precisa do SEU jogo de PS4 já extraído numa pasta: o jogo base mais o update %s. O PORTHEON não vem com os jogos e não indica sites de download: use o seu próprio disco ou a sua compra digital.\n\nComo tirar o jogo do seu PS4: num PS4 com GoldHEN, faça o dump do jogo instalado (Configurações do GoldHEN > dump do jogo, ou o app PS4 Dumper) para um HD externo. O update também sai no dump.",
                                L"PORTHEON's %s runs the PS4 game (%s) directly on the PC, with no emulator. You need YOUR PS4 game already extracted into a folder: the base game plus update %s. PORTHEON ships no games and points to no download sites: use your own disc or your digital purchase.\n\nHow to get the game from your PS4: on a PS4 with GoldHEN, dump the installed game (GoldHEN settings > game dump, or the PS4 Dumper app) to an external drive. The update comes out of the dump too."), G[g].name, G[g].tid, G[g].upd);
    if (g == 3) {   /* 4.0.1: which build of UFC 4's update the port runs (checks.c, Ufc4Build) */
        size_t l = wcslen(bufs[0]);
        _snwprintf(bufs[0] + l, 1200 - l, L"\n\n%s", L2(L"UFC 4: o update 28.00 refeito para firmware 5.05 (o \"backport\"). O 28.00 original, para firmware 6.72+, fecha com erro neste port: o PORTHEON avisa quando a pasta tem esse.",
                                                        L"UFC 4: the 28.00 update rebuilt for firmware 5.05 (the \"backport\"). The original 28.00, for firmware 6.72+, closes with errors on this port: PORTHEON warns when the folder has that one."));
        bufs[0][1199] = 0;
    }
    s[0].body = bufs[0];
    s[1].title = L2(L"Extraia o jogo", L"Extract the game");
    s[1].body = L2(L"O port lê a PASTA do jogo, com eboot.bin, sce_sys, sce_module e databigs dentro. Um arquivo .pkg, .iso, .rar ou .zip fechado não serve.\n\nTem um .pkg do seu jogo? Extraia com um extrator de PKG de PS4 (por exemplo \"Instalar pacote\" do shadPS4, ou o PS4 PKG Tool): a pasta que ele cria é a que você usa. Tem .rar ou .zip? Descompacte primeiro (7-Zip ou WinRAR). Deixe o jogo fora do OneDrive e numa pasta normal (por exemplo D:\\Jogos\\UFC 4).",
                   L"The port reads the game's FOLDER, with eboot.bin, sce_sys, sce_module and databigs in it. A packed .pkg, .iso, .rar or .zip does not work.\n\nHave a .pkg of your game? Extract it with a PS4 PKG extractor (for example shadPS4's \"Install package\", or PS4 PKG Tool): the folder it makes is the one you use. Have a .rar or .zip? Unpack it first (7-Zip or WinRAR). Keep the game out of OneDrive, in a normal folder (for example D:\\Games\\UFC 4).");
    s[2].title = L2(L"Junte o update", L"Merge the update");
    _snwprintf(bufs[2], 1200, L2(L"O %s precisa do update %s. O jeito mais fácil: deixe a pasta do jogo (%s) e a do update (%s-patch) lado a lado dentro de uma mesma pasta, e escolha essa pasta de fora: o port junta as duas sozinho, sem ocupar espaço a mais.\n\nOu copie tudo da pasta do update para dentro da pasta do jogo, substituindo os arquivos.\n\nCuidado: a pasta do update sozinha (sem o jogo base) não abre o jogo.",
                                L"%s needs update %s. The easiest way: keep the game folder (%s) and the update folder (%s-patch) side by side in one folder, and pick that outer folder: the port merges them by itself, using no extra space.\n\nOr copy everything from the update folder into the game folder, replacing files.\n\nCareful: the update folder by itself (without the base game) does not start the game."), G[g].name, G[g].upd, G[g].tid, G[g].tid);
    s[2].body = bufs[2];
    s[3].title = L2(L"Coloque no PORTHEON", L"Put it in PORTHEON");
    s[3].body = L2(L"Clique em \"Escolher a pasta\" e selecione a pasta do jogo, ou arraste a pasta para a janela do PORTHEON: ele reconhece qual UFC é. \"Procurar no PC\" acha sozinho os jogos que já estão no computador.\n\nNão escolha o disco inteiro (D:\\) nem pastas da Microsoft Store / Xbox.",
                   L"Click \"Choose the folder\" and pick the game folder, or drop the folder on PORTHEON's window: it recognizes which UFC it is. \"Find on this PC\" finds the games already on the computer by itself.\n\nDo not pick a whole drive (D:\\) or Microsoft Store / Xbox folders.");
    s[4].title = L2(L"Confira os arquivos", L"Check the files");
    s[4].body = L2(L"O PORTHEON confere o jogo: a versão do update, se a pasta tem o jogo base, e se cada arquivo grande do jogo (databigs) está inteiro. Um download que parou no meio deixa um arquivo cortado, e o jogo fecha ao carregar: aqui ele diz qual é.",
                   L"PORTHEON checks the game: the update's version, whether the folder has the base game, and whether each large game file (databigs) is whole. A download that stopped halfway leaves a cut file, and the game closes while loading: here it says which one.");
    s[5].title = L2(L"Jogue", L"Play");
    s[5].body = L2(L"Aperte JOGAR. A primeira vez demora mais: o port prepara o jogo e monta o cache de shaders (as primeiras lutas podem engasgar um pouco). As configurações já vêm ajustadas para o seu PC. Conecte o controle antes de abrir; dois controles são dois jogadores. Enquanto joga, deixe a janela do jogo em foco.",
                   L"Press PLAY. The first time takes longer: the port prepares the game and builds the shader cache (the first fights can stutter a little). Settings come tuned for your PC. Connect the controller before starting; two controllers are two players. While you play, keep the game's window in focus.");
    if (G[g].ps5) {   /* 4.0.2: UFC 5 was a PS5 game, run by the engine bundled with its port; 4.2: UFC 6 */
        _snwprintf(bufs[0], 1200, L2(L"O %s roda o jogo de PS5 (%s) no PC pelo motor que vem no PORTHEON. Você precisa do SEU jogo de PS5 já extraído numa pasta, na versão %s. O PORTHEON não vem com os jogos e não indica sites de download: use a sua própria compra.\n\nO jogo depende do processador: num Ryzen 5 5500 com RTX 4060, a luta roda entre 32 e 43 FPS no perfil Rápido. Precisa de um processador com AVX2, de 16 GB de RAM e de uma placa de vídeo com Vulkan 1.3 (GTX 10 e mais antigas não rodam).",
                                     L"%s runs the PS5 game (%s) on the PC through the engine bundled with PORTHEON. You need YOUR PS5 game already extracted into a folder, at version %s. PORTHEON ships no games and points to no download sites: use your own purchase.\n\nThe game depends on the processor: on a Ryzen 5 5500 with an RTX 4060, fights run at 32 to 43 FPS in the Fast profile. It needs a processor with AVX2, 16 GB of RAM and a graphics card with Vulkan 1.3 (GTX 10 and older cannot run it)."), G[g].name, G[g].tid, G[g].upd);
        s[0].body = bufs[0];
        s[1].body = L2(L"O port lê a PASTA do jogo, com eboot.bin, sce_sys, sce_module e Data dentro. Um arquivo .pkg, .iso, .rar ou .zip fechado não serve: extraia ou descompacte antes (7-Zip ou WinRAR). O jogo ocupa uns 155 GB: deixe-o fora do OneDrive, num disco com espaço (por exemplo D:\\Jogos\\UFC 6).",
                       L"The port reads the game's FOLDER, with eboot.bin, sce_sys, sce_module and Data in it. A packed .pkg, .iso, .rar or .zip does not work: extract or unpack it first (7-Zip or WinRAR). The game takes about 155 GB: keep it out of OneDrive, on a drive with room (for example D:\\Games\\UFC 6).");
        _snwprintf(bufs[2], 1200, L2(L"O UFC 6 precisa da versão %s (o update já junto com o jogo, como sai do dump). Se o seu dump tem o jogo base e o update em pastas separadas, junte os dois numa pasta só, copiando o update por cima do jogo.",
                                     L"UFC 6 needs version %s (the update already in the game, as it comes out of the dump). If your dump has the base game and the update in separate folders, merge them into one folder, copying the update over the game."), G[g].upd);
        s[2].body = bufs[2];
        s[4].body = L2(L"O PORTHEON confere se a pasta é do UFC 6 e se tem o eboot.bin. A primeira partida monta os shaders do jogo: o menu e a primeira luta podem engasgar; depois eles ficam guardados.",
                       L"PORTHEON checks that the folder is UFC 6 and has eboot.bin. The first session builds the game's shaders: the menu and the first fight can stutter; after that they stay saved.");
        /* 4.3.2: the UFC 1-4 text says two controllers are two players; these engines have one PS5 user (UFC 5 too) */
        s[5].body = L2(L"Aperte JOGAR. A primeira vez demora mais: o motor monta os shaders do jogo (o menu e as primeiras lutas podem engasgar um pouco). As configurações já vêm ajustadas para o seu PC. Conecte o controle antes de abrir; neste motor joga uma pessoa por vez. Enquanto joga, deixe a janela do jogo em foco.",
                       L"Press PLAY. The first time takes longer: the engine builds the game's shaders (the menu and the first fights can stutter a little). Settings come tuned for your PC. Connect the controller before starting; this engine has one player at a time. While you play, keep the game's window in focus.");
    }
    if (UFC5G(g)) {   /* 4.3: UFC 5's own texts again, as in 4.0.6 */
        _snwprintf(bufs[0], 1200, L2(L"O %s roda o jogo de PS5 (%s) no PC pelo motor que vem no PORTHEON. Você precisa do SEU jogo de PS5 já extraído numa pasta, na versão %s. O PORTHEON não vem com os jogos e não indica sites de download: use a sua própria compra.\n\nO jogo depende do processador: num Ryzen 5 5500 com RTX 4060, a luta roda entre 23 e 26 FPS, e o PORTHEON acerta a velocidade do jogo pelo FPS real, sem câmera lenta. Precisa de um processador com AVX2 e de uma placa de vídeo com Vulkan 1.3.",
                                     L"%s runs the PS5 game (%s) on the PC through the engine bundled with PORTHEON. You need YOUR PS5 game already extracted into a folder, at version %s. PORTHEON ships no games and points to no download sites: use your own purchase.\n\nThe game depends on the processor: on a Ryzen 5 5500 with an RTX 4060, fights run at 23 to 26 FPS, and PORTHEON sets the game's speed from the real FPS, with no slow motion. It needs a processor with AVX2 and a graphics card with Vulkan 1.3."), G[g].name, G[g].tid, G[g].upd);
        s[1].body = L2(L"O port lê a PASTA do jogo, com eboot.bin, sce_sys, sce_module e Data dentro. Um arquivo .pkg, .iso, .rar ou .zip fechado não serve: extraia ou descompacte antes (7-Zip ou WinRAR). O jogo ocupa uns 110 GB: deixe-o fora do OneDrive, num disco com espaço (por exemplo D:\\Jogos\\UFC 5).",
                       L"The port reads the game's FOLDER, with eboot.bin, sce_sys, sce_module and Data in it. A packed .pkg, .iso, .rar or .zip does not work: extract or unpack it first (7-Zip or WinRAR). The game takes about 110 GB: keep it out of OneDrive, on a drive with room (for example D:\\Games\\UFC 5).");
        _snwprintf(bufs[2], 1200, L2(L"O UFC 5 precisa da versão %s (o update já junto com o jogo, como sai do dump). Se o seu dump tem o jogo base e o update em pastas separadas, junte os dois numa pasta só, copiando o update por cima do jogo.",
                                     L"UFC 5 needs version %s (the update already in the game, as it comes out of the dump). If your dump has the base game and the update in separate folders, merge them into one folder, copying the update over the game."), G[g].upd);
        s[4].body = L2(L"O PORTHEON confere se a pasta é do UFC 5 e se tem o eboot.bin. A primeira partida compila os shaders do jogo: alguns minutos com FPS baixo; depois eles ficam no cache.",
                       L"PORTHEON checks that the folder is UFC 5 and has eboot.bin. The first session compiles the game's shaders: a few minutes at low FPS; after that they stay in the cache.");
    }
    if (X360G(g)) {   /* 5.0: UFC Undisputed 3, the Xbox 360 game through the UFC3MONS port */
        _snwprintf(bufs[0], 1200, L2(L"O %s roda o jogo de Xbox 360 (%s) direto no PC pelo port UFC3MONS (%s), que vem no PORTHEON. Você precisa do SEU jogo de Xbox 360: a imagem .iso do disco ou o jogo já extraído (Original ou Reborn). O PORTHEON não vem com os jogos e não indica sites de download: use a sua própria cópia.",
                                     L"%s runs the Xbox 360 game (%s) directly on the PC through the UFC3MONS port (%s), which comes with PORTHEON. You need YOUR Xbox 360 game: the disc's .iso image or the game already extracted (Original or Reborn). PORTHEON ships no games and points to no download sites: use your own copy."), G[g].name, G[g].tid, G[g].upd);
        s[0].body = bufs[0];
        s[1].title = L2(L"A pasta do jogo", L"The game folder");
        s[1].body = L2(L"O port aceita dois jeitos: uma pasta com o .iso do Xbox 360 dentro (o port acha o .iso sozinho), ou a pasta do jogo extraído, a que tem o default.xex. Um .rar, .zip ou .7z fechado não serve: descompacte antes (7-Zip ou WinRAR). Deixe o jogo fora do OneDrive, numa pasta normal (por exemplo D:\\Jogos\\UFC Undisputed 3).",
                       L"The port takes two ways: a folder with the Xbox 360 .iso in it (the port finds the .iso by itself), or the extracted game's folder, the one with default.xex. A packed .rar, .zip or .7z does not work: unpack it first (7-Zip or WinRAR). Keep the game out of OneDrive, in a normal folder (for example D:\\Games\\UFC Undisputed 3).");
        s[2].title = L2(L"Original ou Reborn", L"Original or Reborn");
        s[2].body = L2(L"O port roda as duas edições: o jogo Original (o .iso ou o jogo extraído) e o Reborn (extraído). Ele vê qual é e abre do jeito certo; não há update para juntar.",
                       L"The port runs both editions: the Original game (the .iso or the extracted game) and Reborn (extracted). It sees which one it is and starts it the right way; there is no update to merge.");
        s[3].body = L2(L"Clique em \"Escolher a pasta\" e selecione a pasta do jogo (a do default.xex ou a que tem o .iso), ou arraste a pasta ou o próprio .iso para a janela do PORTHEON.\n\nNão escolha o disco inteiro (D:\\) nem pastas da Microsoft Store / Xbox.",
                       L"Click \"Choose the folder\" and pick the game folder (the one with default.xex or the one that holds the .iso), or drop the folder or the .iso itself on PORTHEON's window.\n\nDo not pick a whole drive (D:\\) or Microsoft Store / Xbox folders.");
        s[4].body = L2(L"O PORTHEON confere se a pasta tem o default.xex ou um .iso. Se o jogo não abrir, o port explica o motivo no ufc3.log e no RELATORIO.md, na pasta do jogo.",
                       L"PORTHEON checks that the folder has default.xex or an .iso. If the game does not start, the port explains why in ufc3.log and RELATORIO.md, in the game folder.");
        s[5].body = L2(L"Aperte JOGAR. A primeira vez demora mais: o port prepara os shaders por alguns minutos. O jogo roda a 60 FPS fixos. Vídeo, controles, saves, trapaças, mods e DLC ficam em \"Configurar o port\", na Visão geral do jogo.",
                       L"Press PLAY. The first time takes longer: the port prepares its shaders for a few minutes. The game runs at a fixed 60 FPS. Video, controls, saves, cheats, mods and DLC are in \"Configure the port\", on the game's Overview.");
    }
    if (ZELDAG(g)) {   /* 6.0: Zelda: Ocarina of Time (Switch 2), on the Lichii emulator or natively (Ocarina Recomp) */
        s[0].body = L2(L"O PORTHEON roda o Zelda: Ocarina of Time de Switch 2 de dois jeitos: no emulador Lichii (MDias v4, em português e com correções para AMD), que vem no PORTHEON, ou NATIVO no PC, recompilado pelo Ocarina Recomp, que também vem no PORTHEON. Você precisa do SEU jogo: o PORTHEON não vem com ele, nem com as chaves ou o firmware do console, e não indica sites de download.",
                       L"PORTHEON runs the Switch 2 Zelda: Ocarina of Time two ways: on the Lichii emulator (MDias v4, with AMD fixes), which comes with PORTHEON, or NATIVELY on the PC, recompiled by Ocarina Recomp, which also comes with PORTHEON. You need YOUR game: PORTHEON ships neither it nor the console's keys or firmware, and points to no download sites.");
        s[1].title = L2(L"O arquivo do jogo", L"The game file");
        s[1].body = L2(L"O emulador abre um arquivo .nup (não precisa de chaves), .nsp ou .xci (precisam das SUAS chaves prod.keys e title.keys, tiradas do seu console, em %APPDATA%\\eden\\keys), ou a pasta extraída do jogo (com exefs e romfs).",
                       L"The emulator opens a .nup file (needs no keys), .nsp or .xci (need YOUR prod.keys and title.keys, taken from your console, in %APPDATA%\\eden\\keys), or the game's extracted folder (with exefs and romfs).");
        s[2].title = L2(L"Versão e cache", L"Version and cache");
        s[2].body = L2(L"No emulador vale a versão que você tiver. Para o PC nativo, o Ocarina Recomp precisa da versão 1.1.1. O \"Instalador de Shader - Zelda OOT\" da comunidade coloca o cache de shaders em %APPDATA%\\eden\\cache\\shader: se você usar, o PORTHEON aproveita e o jogo já abre sem engasgos.",
                       L"On the emulator, any version you have works. For native PC, Ocarina Recomp needs version 1.1.1. The community's \"Instalador de Shader - Zelda OOT\" puts the shader cache in %APPDATA%\\eden\\cache\\shader: if you use it, PORTHEON makes use of it and the game opens without stutter.");
        s[3].body = L2(L"Clique em \"Escolher a pasta\" (abre a escolha do arquivo; Cancelar escolhe uma pasta extraída) ou arraste o arquivo do jogo para a janela do PORTHEON.",
                       L"Click \"Choose the folder\" (it opens the file choice; Cancel picks an extracted folder) or drop the game file on PORTHEON's window.");
        s[4].title = L2(L"PC nativo (opcional)", L"Native PC (optional)");
        s[4].body = L2(L"Em Arquivos do jogo, \"Ocarina Setup: recompilar para PC\" abre o Ocarina Setup 0.2.1 (já vem dentro do PORTHEON), que transforma o seu jogo 1.1.1 num programa de PC. Leva de 30 minutos a 2 horas e usa uns 40 GB. Quando termina, o PORTHEON acha o jogo nativo sozinho e o JOGAR passa a abrir ele, sem emulador.",
                       L"In Game files, \"Ocarina Setup: recompile for PC\" opens Ocarina Setup 0.2.1 (it comes inside PORTHEON), which turns your 1.1.1 game into a PC program. It takes 30 minutes to 2 hours and uses about 40 GB. When it ends, PORTHEON finds the native game by itself and PLAY opens it, with no emulator.");
        s[5].body = L2(L"Aperte JOGAR. No emulador, áreas novas podem engasgar na primeira vez enquanto os shaders são montados. Controles, resolução e gráficos ficam no emulador (Abrir o emulador); neblina, 30 FPS e a compilação segura para AMD ficam na aba Vídeo do PORTHEON.",
                       L"Press PLAY. On the emulator, new areas can stutter the first time while shaders are built. Controls, resolution and graphics are in the emulator (Open the emulator); fog, 30 FPS and AMD safe compiling are in PORTHEON's Video tab.");
    }
    if (GOW3G(g)) {   /* 6.0: God of War III Remastered (PS4) through the gow3_pc port */
        s[0].body = L2(L"O PORTHEON roda o God of War III Remastered de PS4 (CUSA01623) direto no PC pelo port nativo gow3_pc, que vem no PORTHEON. Você precisa do SEU jogo de PS4 já extraído numa pasta, com o update 01.02. O PORTHEON não vem com o jogo e não indica sites de download.",
                       L"PORTHEON runs the PS4 God of War III Remastered (CUSA01623) directly on the PC through the native gow3_pc port, which comes with PORTHEON. You need YOUR PS4 game already extracted into a folder, with update 01.02. PORTHEON does not ship the game and points to no download sites.");
        s[2].body = L2(L"O port precisa da versão 01.02: o update junto com o jogo (o eboot.bin da 01.02). Se o seu dump tem o jogo e o update em pastas separadas, copie o update por cima do jogo.",
                       L"The port needs version 01.02: the update merged into the game (01.02's eboot.bin). If your dump has the game and the update in separate folders, copy the update over the game.");
        s[3].body = L2(L"Clique em \"Escolher a pasta\" e selecione a pasta do jogo (a que tem eboot.bin, sce_sys e sce_module), ou arraste a pasta para a janela do PORTHEON. \"Procurar no PC\" também acha o jogo.",
                       L"Click \"Choose the folder\" and pick the game folder (the one with eboot.bin, sce_sys and sce_module), or drop the folder on PORTHEON's window. \"Find on this PC\" finds the game too.");
        s[4].body = L2(L"O PORTHEON confere se a pasta é do God of War III e qual versão ela tem. Na primeira vez o launcher do port prepara o jogo a partir do seu eboot.bin.",
                       L"PORTHEON checks that the folder is God of War III and which version it has. The first time, the port's launcher prepares the game from your eboot.bin.");
        s[5].body = L2(L"Aperte JOGAR. A primeira vez demora mais: o port prepara o jogo e os shaders. Vídeo, FSR, controles e mods ficam no launcher do port (Arquivos do jogo > Abrir o launcher do port).",
                       L"Press PLAY. The first time takes longer: the port prepares the game and its shaders. Video, FSR, controls and mods are in the port's launcher (Game files > Open the port's launcher).");
    }
    if (NGPORTG(g))   /* 6.1: Bloodborne, inFAMOUS Second Son (extgames.c NgGuide) */
        for (int k = 0; k < NSTEPS; k++) { const wchar_t *t = NULL, *b = NgGuide(g, k, &t); if (b) s[k].body = b; if (t) s[k].title = t; }
}
static wchar_t g_instCheck[900]; static int g_instCheckGame = -1, g_instCheckBad;
static void InstallVerify(int g)
{
    wchar_t d[MAX_PATH]; GameDir(g, d); g_instCheckGame = g;
    if (EXTG(g)) {   /* 6.0: a Zelda file or folder (or the native build), a God of War III dump */
        if (ExtStatus(g) == S_READY) { _snwprintf(g_instCheck, 900, L2(L"Tudo certo! %s", L"All set! %s"), GameVerOf(g)->line); g_instCheckBad = GameVerOf(g)->ok < 0; return; }
        _snwprintf(g_instCheck, 900, L"%s", NGPORTG(g) ? (PortOk(g) ? NgPickText(g) : NgNoPortText(g)) : ZELDAG(g) ? L2(L"Ainda não achei o Zelda: escolha o arquivo do jogo (.nup, .nsp ou .xci) ou a pasta extraída (passo 4).", L"Zelda is not found yet: choose the game file (.nup, .nsp or .xci) or the extracted folder (step 4).")
                                                       : L2(L"Ainda não achei o God of War III: escolha a pasta do jogo de PS4 extraído, CUSA01623 (passo 4).", L"God of War III is not found yet: choose the extracted PS4 game's folder, CUSA01623 (step 4)."));
        g_instCheckBad = 1; return;
    }
    if (!d[0] || !DirExists(d)) { _snwprintf(g_instCheck, 900, L"%s", L2(L"Nenhuma pasta escolhida para este jogo ainda (passo 4).", L"No folder picked for this game yet (step 4).")); g_instCheckBad = 1; return; }
    if (X360G(g)) {   /* 5.0 */
        wchar_t iso[MAX_PATH], p[MAX_PATH]; Join(p, d, L"default.xex");
        if (FileExists(p)) _snwprintf(g_instCheck, 900, L2(L"Tudo certo: o jogo extraído (default.xex) está em %s. Pode jogar!", L"All set: the extracted game (default.xex) is in %s. You can play!"), d);
        else if (X3Iso(d, iso, MAX_PATH)) _snwprintf(g_instCheck, 900, L2(L"Tudo certo: o port vai usar %s. Pode jogar!", L"All set: the port will use %s. You can play!"), iso);
        else { _snwprintf(g_instCheck, 900, L2(L"Não achei o UFC Undisputed 3 em %s: escolha a pasta com o default.xex ou a que tem o .iso do Xbox 360.", L"UFC Undisputed 3 was not found in %s: pick the folder with default.xex or the one that holds the Xbox 360 .iso."), d); g_instCheckBad = 1; return; }
        g_instCheckBad = 0; return;
    }
    wchar_t why[400]; int det = DetectFolder(d, NULL, why, 400);
    if (det < 0) { _snwprintf(g_instCheck, 900, L2(L"Não achei um UFC em %s: escolha a pasta que tem eboot.bin, sce_sys e sce_module.", L"No UFC found in %s: pick the folder with eboot.bin, sce_sys and sce_module."), d); g_instCheckBad = 1; return; }
    if (det != g) { _snwprintf(g_instCheck, 900, L2(L"Essa pasta é do %s, não do %s.", L"That folder is %s, not %s."), G[det].name, G[g].name); g_instCheckBad = 1; return; }
    if (why[0]) { wcsncpy(g_instCheck, why, 899); g_instCheck[899] = 0; g_instCheckBad = 1; return; }
    if (PS4G(g)) { GameCheck r; CheckGameFiles(g, d, &r); if (GameCheckText(g, &r, g_instCheck, 900)) { g_instCheckBad = 1; return; } }
    _snwprintf(g_instCheck, 900, L2(L"Tudo certo: %s na versão %s, com o jogo base e os arquivos completos. Pode jogar!", L"All set: %s at version %s, with the base game and complete files. You can play!"), G[g].name, G[g].upd);
    g_instCheckBad = 0;
}
static void InstallArt(HDC dc, GpGraphics *g, RECT r, int gi, int step)
{
    Box(g, r, SC(16), A(RGB(255, 255, 255), 7), A(C_LINE, 255), 1.0f);
    int x = r.left + SC(26), y = r.top + SC(26);
    if (X360G(gi) && (step == 1 || step == 2 || step == 4)) {   /* 5.0: UFC Undisputed 3: what the port takes */
        IconFolder(g, x, y, SC(34), A(C_CYAN, 255)); Text(dc, g_fCard, C_TEXT, x + SC(46), y + SC(4), L"UFC Undisputed 3");
        const wchar_t *it[4] = {L"default.xex", L2(L"ou: o .iso do Xbox 360", L"or: the Xbox 360 .iso"), L"Original / Reborn", L"60 FPS"};
        for (int i = 0; i < 4; i++) {
            int yy = y + SC(60 + i * 40); BOOL ok = step != 4 || g_instCheckGame != gi || !g_instCheckBad || i >= 2;
            if (ok) IconCheck(g, x + SC(8), yy - SC(6), SC(14), A(C_GOOD, 255)); else IconCross(g, x + SC(9), yy - SC(5), SC(12), A(C_ORANGE, 255));
            Text(dc, g_fText, C_TEXT, x + SC(34), yy - SC(10), it[i]);
        }
        return;
    }
    if (step == 0) {
        /* 4.0.4: the two lines go under the picture, and the name fits the box (they ran past its right edge) */
        GameTile(dc, g, gi, x, y, SC(120), 2);
        const wchar_t *nm = G[gi].card ? G[gi].card : G[gi].name; RECT nr = {x + SC(140), y + SC(10), r.right - SC(14), y + SC(40)};
        TextR(dc, TextW(dc, g_fCard, nm) > nr.right - nr.left ? g_fBtn : g_fCard, C_TEXT, nr, nm, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        wchar_t t[96]; _snwprintf(t, 96, X360G(gi) ? L"%s  ·  port %s" : L"%s  ·  update %s", G[gi].tid, G[gi].upd); Text(dc, g_fSmall, C_SUB, x + SC(140), y + SC(44), t);
        RECT l1 = {x + SC(22), y + SC(138), r.right - SC(14), y + SC(158)}, l2 = {x + SC(22), y + SC(164), r.right - SC(14), y + SC(184)};
        IconCheck(g, x, y + SC(141), SC(14), A(C_GOOD, 255)); TextR(dc, g_fSmall, C_GOOD, l1, L2(L"o seu próprio jogo", L"your own game"), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        IconCross(g, x + SC(1), y + SC(168), SC(12), A(C_BAD, 255)); TextR(dc, g_fSmall, C_BAD, l2, L2(L"o PORTHEON não traz jogos", L"PORTHEON ships no games"), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    } else if (step == 1) {
        IconFolder(g, x, y, SC(34), A(C_CYAN, 255)); Text(dc, g_fCard, C_TEXT, x + SC(46), y + SC(4), G[gi].tid);
        const wchar_t *files[4] = {L"eboot.bin", L"sce_sys", L"sce_module", G[gi].ps5 ? L"Data" : L"databigs"};
        for (int i = 0; i < 4; i++) { int yy = y + SC(52 + i * 30); Line(g, x + SC(16), yy - SC(8), x + SC(16), yy + SC(8), A(C_LINE, 255), 1.0f); IconCheck(g, x + SC(28), yy - SC(6), SC(13), A(C_GOOD, 255)); Text(dc, g_fText, C_TEXT, x + SC(52), yy - SC(10), files[i]); }
        int yy = y + SC(196);
        const wchar_t *bad[2] = {L2(L"jogo.pkg", L"game.pkg"), L2(L"jogo.iso", L"game.iso")};
        for (int i = 0; i < 2; i++) { IconCross(g, x + SC(28), yy + i * SC(28) - SC(5), SC(11), A(C_BAD, 255)); Text(dc, g_fText, C_SUB, x + SC(52), yy + i * SC(28) - SC(10), bad[i]); }
        Text(dc, g_fSmall, C_BAD, x + SC(150), yy + SC(4), L2(L"fechado: extraia antes", L"packed: extract first"));
    } else if (step == 2) {
        IconFolder(g, x, y, SC(28), A(C_SUB, 255)); Text(dc, g_fText, C_TEXT, x + SC(40), y + SC(4), G[gi].tid); Text(dc, g_fSmall, C_DIM, x + SC(40), y + SC(24), L2(L"o jogo (01.00)", L"the game (01.00)"));
        IconFolder(g, x, y + SC(60), SC(28), A(C_SUB, 255)); { wchar_t t[48]; _snwprintf(t, 48, L"%s-patch", G[gi].tid); Text(dc, g_fText, C_TEXT, x + SC(40), y + SC(64), t); }
        { wchar_t t[64]; _snwprintf(t, 64, L2(L"o update %s", L"update %s"), G[gi].upd); Text(dc, g_fSmall, C_DIM, x + SC(40), y + SC(84), t); }
        Line(g, x + SC(120), y + SC(118), x + SC(120), y + SC(152), A(C_CYAN, 255), (float)SC(2)); Chevron(g, x + SC(120), y + SC(150), SC(6), 0, A(C_CYAN, 255));
        IconFolder(g, x, y + SC(172), SC(34), A(C_CYAN, 255)); Text(dc, g_fCard, C_TEXT, x + SC(46), y + SC(176), G[gi].tid);
        { wchar_t t[64]; _snwprintf(t, 64, L2(L"pronto: versão %s", L"ready: version %s"), G[gi].upd); IconCheck(g, x + SC(46), y + SC(208), SC(13), A(C_GOOD, 255)); Text(dc, g_fSmall, C_GOOD, x + SC(66), y + SC(204), t); }
    } else if (step == 3) {
        RECT win = {x, y + SC(20), r.right - SC(26), r.bottom - SC(40)}; Box(g, win, SC(12), A(C_BG, 255), A(C_FIELDB, 255), 1.0f);
        RECT bar = {win.left, win.top, win.right, win.top + SC(22)}; Box(g, bar, SC(12), A(RGB(36, 28, 58), 255), 0, 0);
        GpPen *pen = NULL; GdipCreatePen1(A(C_CYAN, 200), (float)SC(2), 2, &pen);
        RECT dz = {win.left + SC(24), win.top + SC(40), win.right - SC(24), win.bottom - SC(20)};
        for (int xx = dz.left; xx < dz.right; xx += SC(14)) { GdipDrawLineI(g, pen, xx, dz.top, xx + SC(7), dz.top); GdipDrawLineI(g, pen, xx, dz.bottom, xx + SC(7), dz.bottom); }
        for (int yy = dz.top; yy < dz.bottom; yy += SC(14)) { GdipDrawLineI(g, pen, dz.left, yy, dz.left, yy + SC(7)); GdipDrawLineI(g, pen, dz.right, yy, dz.right, yy + SC(7)); }
        GdipDeletePen(pen);
        float b = sinf((float)(g_anim % 1600) / 1600.0f * 6.2831853f) * SC(5);
        IconFolder(g, (dz.left + dz.right) / 2 - SC(22), dz.top + SC(26) + (int)b, SC(44), A(C_CYAN, 255));
        RECT t = {dz.left, dz.bottom - SC(48), dz.right, dz.bottom - SC(14)}; TextR(dc, g_fBtn, C_TEXT, t, L2(L"arraste a pasta aqui", L"drop the folder here"), DT_CENTER | DT_VCENTER);
    } else if (step == 4) {
        static const wchar_t *const it4[4] = {L"param.sfo", L"update", L"databigs", L".big"}, *const it5[4] = {L"param.json", L"eboot.bin", L"sce_module", L"Data"};
        const wchar_t *const *it = G[gi].ps5 ? it5 : it4;   /* 4.0.2: UFC 5's own files */
        for (int i = 0; i < 4; i++) {
            int yy = y + SC(10 + i * 44); BOOL ok = g_instCheckGame == gi ? !g_instCheckBad || i < 1 : TRUE;
            Dot(g, x + SC(12), yy + SC(12), SC(12), A(ok ? C_GOOD : C_ORANGE, 60)); if (ok) IconCheck(g, x + SC(5), yy + SC(5), SC(14), A(C_GOOD, 255)); else IconCross(g, x + SC(6), yy + SC(6), SC(12), A(C_ORANGE, 255));
            Text(dc, g_fText, C_TEXT, x + SC(36), yy + SC(2), it[i]);
        }
    } else {
        RECT pb = {r.left + SC(40), (r.top + r.bottom) / 2 - SC(30), r.right - SC(40), (r.top + r.bottom) / 2 + SC(30)};
        /* 4.0.4: the PS5's white Play, as on the home screen */
        float p = Pulse(1800); for (int k = 3; k >= 1; k--) { RECT gl = {pb.left - SC(3 * k), pb.top - SC(3 * k), pb.right + SC(3 * k), pb.bottom + SC(3 * k)}; Box(g, gl, SC(14 + 3 * k), A(C_HOT, (BYTE)((10 + 16 * p) / k)), 0, 0); }
        Box(g, pb, SC(14), A(C_HOT, 245), 0, 0);
        wchar_t sc[48]; const wchar_t *pl = Sentence(T(S_PLAY), sc, 48); int tw = TextW(dc, g_fPlay, pl); int x0 = (pb.left + pb.right - (SC(30) + tw)) / 2;
        PlayTriangle(g, x0, (pb.top + pb.bottom) / 2, SC(18), A(C_INK, 255)); RECT tr = {x0 + SC(30), pb.top, pb.right, pb.bottom}; TextR(dc, g_fPlay, C_INK, tr, pl, DT_LEFT | DT_VCENTER);
    }
}
static void GameChips(HDC dc, GpGraphics *g, int x, int y)
{
    for (int i = 0; i < NGAMES; i++) {
        int gi = HOME_ORDER[i]; if (SOON(gi)) continue; const wchar_t *n = ZELDAG(gi) ? L"Zelda" : ISSG(gi) ? L"inFAMOUS" : FNCG(gi) ? L"Fight Night" : G[gi].name; int w = TextW(dc, g_fBtn, n) + SC(34); RECT r = {x, y, x + w, y + SC(34)};   /* 6.1: short names */
        if (r.right > SC(MX + CW)) break;
        BOOL on = gi == g_guideGame, hot = g_hot == H_GUIDEGAME + gi;
        if (on) Box(g, r, SC(17), A(C_HOT, 245), 0, 0); else Box(g, r, SC(17), A(RGB(255, 255, 255), hot ? 22 : 9), A(hot ? C_HOT : C_FIELDB, 255), 1.0f);
        TextR(dc, g_fBtn, on ? C_INK : hot ? RGB(255, 255, 255) : C_SUB, r, n, DT_CENTER | DT_VCENTER); AddHit(r.left, r.top, r.right, r.bottom, H_GUIDEGAME + gi);
        x = r.right + SC(8);
    }
}
static void PaintInstall(HDC dc, GpGraphics *g)
{
    int gi = g_guideGame, st = g_guideStep;
    ScreenTitle(dc, g, L2(L"PORTHEON  ·  6 PASSOS", L"PORTHEON  ·  6 STEPS"), L2(L"Instalar um jogo", L"Install a game"));
    { RECT fb = {SC(MX + CW) - SC(330), SC(84), SC(MX + CW), SC(128)}; Button(dc, g, fb, L2(L"Encontrar todos os meus jogos", L"Find all my games"), H_SCAN, 1); }   /* 6.2 */
    GameChips(dc, g, SC(MX + 12), SC(134));
    Step s[NSTEPS]; static wchar_t bufs[NSTEPS][1200]; InstallSteps(gi, s, bufs);
    int my = SC(190);
    for (int k = 0; k < NSTEPS; k++) {
        RECT r = {SC(MX + 12), my + k * SC(56), SC(MX + 12) + SC(270), my + k * SC(56) + SC(48)}; BOOL on = st == k, hot = g_hot == H_INSTSTEP + k;
        if (on) Box(g, r, SC(14), A(RGB(255, 255, 255), 30), 0, 0); else if (hot) Box(g, r, SC(14), A(RGB(255, 255, 255), 14), 0, 0);
        RECT cr = {r.left + SC(10), r.top + SC(8), r.left + SC(42), r.top + SC(40)};
        COLORREF ink = RGB0(255, 255, 255);
        if (k <= st) ink = AccentBox(g, cr, SC(16)); else Box(g, cr, SC(16), A(C_FIELD, 255), A(C_FIELDB, 255), 1.0f);
        wchar_t num[4]; _snwprintf(num, 4, L"%d", k + 1);
        if (k < st) IconCheck(g, cr.left + SC(9), cr.top + SC(10), SC(14), A(ink, 255)); else TextR(dc, g_fBtn, k <= st ? ink : C_SUB, cr, num, DT_CENTER | DT_VCENTER);
        RECT tr = {r.left + SC(54), r.top, r.right - SC(8), r.bottom}; TextR(dc, g_fBtn, on || hot ? RGB(255, 255, 255) : C_TEXT, tr, s[k].title, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        AddHit(r.left, r.top, r.right, r.bottom, H_INSTSTEP + k);
    }
    RECT pr = {SC(MX + 300), SC(190), SC(MX + CW), SC(742)}; Panel(g, pr);
    Text(dc, g_fH2, RGB(255, 255, 255), pr.left + SC(28), pr.top + SC(22), s[st].title);
    RECT tr = {pr.left + SC(28), pr.top + SC(70), pr.left + SC(28) + SC(500), pr.bottom - SC(150)}; Wrap(dc, g_fBody, C_TEXT, tr, s[st].body, TRUE);
    RECT art = {pr.right - SC(28) - SC(330), pr.top + SC(70), pr.right - SC(28), pr.top + SC(70) + SC(300)}; InstallArt(dc, g, art, gi, st);
    int by = pr.bottom - SC(70), x = pr.left + SC(28);
    if (st == 3 || st == 4) {
        RECT b1 = {x, by, x + SC(210), by + SC(46)}; IconButton(dc, g, b1, L2(L"Escolher a pasta", L"Choose the folder"), H_INSTPICK, st == 3 ? 1 : 0, 0); x = b1.right + SC(10);
        if (!X360G(gi)) { RECT b2 = {x, by, x + SC(190), by + SC(46)}; IconButton(dc, g, b2, g_scanning ? L2(L"Procurando...", L"Looking...") : L2(L"Procurar no PC", L"Find on this PC"), H_INSTSCAN, 0, 2); x = b2.right + SC(10); }   /* 5.0: the scan knows PS4/PS5 dumps only */
    }
    if (st == 4) {
        RECT b3 = {x, by, x + SC(190), by + SC(46)}; IconButton(dc, g, b3, L2(L"Verificar agora", L"Check now"), H_INSTVERIFY, 1, 3);
        if (g_instCheckGame != gi) InstallVerify(gi);
        RECT r1 = {0, 0, pr.right - pr.left - SC(56) - SC(48), 0}; int h = Wrap(dc, g_fSmall, C_SUB, r1, g_instCheck, FALSE) + SC(20);
        NoteBox(dc, g, pr.left + SC(28), by - h - SC(14), pr.right - pr.left - SC(56), h, g_instCheck, g_instCheckBad);
    }
    if (st == 5 && St(gi) == S_READY) { RECT b = {x, by, x + SC(230), by + SC(46)}; Button(dc, g, b, L2(L"Ir para o jogo", L"Go to the game"), H_CONFIG, 1); }
    RECT nb = {pr.right - SC(28) - SC(150), by, pr.right - SC(28), by + SC(46)}; Button(dc, g, nb, st < NSTEPS - 1 ? L2(L"Próximo  ›", L"Next  ›") : L2(L"Concluir", L"Done"), H_INSTNEXT, st < 3 ? 1 : 2);
    if (st > 0) { RECT pb = {nb.left - SC(10) - SC(130), by, nb.left - SC(10), by + SC(46)}; Button(dc, g, pb, L2(L"‹  Voltar", L"‹  Back"), H_INSTPREV, 2); }
}

/* ---------------------------------------------------------------- the online center */
static void PaintOnline(HDC dc, GpGraphics *g)
{
    int gi = g_guideGame;
    ScreenTitle(dc, g, L2(L"PORTHEON  ·  JOGUE COM AMIGOS", L"PORTHEON  ·  PLAY WITH FRIENDS"), L2(L"Online e multijogador", L"Online and multiplayer"));
    GameChips(dc, g, SC(MX + 12), SC(134));
    RECT pr = {SC(MX + 12), SC(186), SC(MX + CW), SC(742)}; Panel(g, pr);
    g_itBottom = pr.bottom - SC(12);   /* 6.2 */
    int keep = g_sel; g_sel = gi;   /* the items read and write the game shown here */
    static Item it[40]; int n = OnlineItems(gi, it, 0);
    /* 4.3.2: not for UFC 5 and 6, whose engines have one PS5 user (OnlineItems says so) */
    if (gi != 2 && PS4G(gi)) n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Dois jogadores no mesmo PC: dois controles, um para cada jogador (o segundo entra na escolha de lutador).", L"Two players on one PC: two controllers, one per player (the second one joins at fighter select)."), 0, 3);
    ClipBase(g); GdipSetClipRectI(g, pr.left, pr.top, pr.right - pr.left, pr.bottom - pr.top, 1);
    PaintItems(dc, g, it, n, pr.left + SC(24), pr.top + SC(22), pr.right - pr.left - SC(48));
    ClipBase(g);
    g_sel = keep;
}

/* ---------------------------------------------------------------- overlays */
static void Dim(GpGraphics *g, RECT rc) { Fill(g, -g_ox, -g_oy, rc.right, rc.bottom, A(RGB(3, 4, 9), 210)); }
static RECT Card(GpGraphics *g, int x, int y, int w, int h)
{
    RECT r = {SC(x), SC(y), SC(x + w), SC(y + h)};
    RECT sh = {r.left + SC(8), r.top + SC(14), r.right + SC(8), r.bottom + SC(14)}; Box(g, sh, SC(22), A(RGB(0, 0, 0), 140), 0, 0);
    Box(g, r, SC(22), A(RGB(14, 10, 26), 250), 0, 0);
    VGrad(g, r.left + SC(1), r.top + SC(1), r.right - r.left - SC(2), SC(110), A(RGB(90, 50, 170), 60), A(RGB(14, 10, 26), 0));
    GradBox(g, r, SC(22), A(C_CYAN, 170), A(C_PURPLE, 170), FALSE, 1.2f);
    return r;
}
static void CloseX(GpGraphics *g, RECT card, int id)
{
    RECT x = {card.right - SC(52), card.top + SC(18), card.right - SC(18), card.top + SC(52)}; BOOL hot = g_hot == id;
    Box(g, x, SC(17), A(RGB(255, 255, 255), hot ? 28 : 10), 0, 0);
    IconCross(g, x.left + SC(11), x.top + SC(11), SC(12), A(hot ? RGB(255, 255, 255) : C_SUB, 255));
    AddHit(x.left, x.top, x.right, x.bottom, id);
}
static void PaintUpdateOverlay(HDC dc, GpGraphics *g, RECT rc)
{
    Dim(g, rc);
    RECT c = Card(g, 220, 110, 840, 580);
    wchar_t t[160]; _snwprintf(t, 160, L2(L"PORTHEON %s está disponível", L"PORTHEON %s is available"), g_updTag);
    Text(dc, g_fH2, RGB(255, 255, 255), c.left + SC(34), c.top + SC(28), t);
    _snwprintf(t, 160, L2(L"Você tem a %s. A atualização baixa, confere (SHA-256) e instala sozinha; configurações, perfis e saves ficam.", L"You have %s. The update downloads, checks itself (SHA-256) and installs by itself; settings, profiles and saves stay."), L"Portheon " APP_SHOW);
    RECT st = {c.left + SC(34), c.top + SC(70), c.right - SC(34), c.top + SC(110)}; Wrap(dc, g_fSmall, C_SUB, st, t, TRUE);
    if ((g_updState != 2 && g_updState != 3 && g_updState != 4) || (g_updAuto && g_updState != 4)) CloseX(g, c, H_UPDLATER);   /* 6.1 */
    RECT nb = {c.left + SC(34), c.top + SC(118), c.right - SC(34), c.top + SC(390)}; Box(g, nb, SC(12), A(RGB(255, 255, 255), 6), A(C_LINE, 255), 1.0f);
    RECT nt = {nb.left + SC(18), nb.top + SC(14), nb.right - SC(18), nb.bottom - SC(12)};
    int saved = SaveDC(dc); IntersectClipRect(dc, nt.left, nt.top, nt.right, nt.bottom);
    Wrap(dc, g_fSmall, C_TEXT, nt, g_updNotes && g_updNotes[0] ? g_updNotes : L2(L"As notas desta versão estão na página da release no GitHub.", L"This version's notes are on its GitHub release page."), TRUE);
    RestoreDC(dc, saved);
    int py = c.top + SC(408);
    if (g_updState == 2 || g_updState == 3 || g_updState == 4) {
        RECT bar = {c.left + SC(34), py, c.right - SC(34), py + SC(10)}; Box(g, bar, SC(5), A(RGB(255, 255, 255), 20), 0, 0);
        double f = g_dlTotal > 0 ? (double)g_dlGot / g_dlTotal : 0; if (g_updState >= 3) f = 1;
        if (f > 0) { RECT fb = bar; fb.right = bar.left + (int)((bar.right - bar.left) * f); if (fb.right - fb.left > SC(10)) GradBox(g, fb, SC(5), A(C_CYAN, 255), A(C_PURPLE, 255), TRUE, 0); }
        wchar_t p[200];
        if (g_updState == 3) _snwprintf(p, 200, L"%s", L2(L"Conferindo o pacote (SHA-256)...", L"Checking the package (SHA-256)..."));
        else if (g_updState == 4) _snwprintf(p, 200, L"%s", L2(L"Instalando: o PORTHEON vai fechar e abrir de novo sozinho.", L"Installing: PORTHEON closes and opens again by itself."));
        else {
            double secs = (GetTickCount64() - g_dlT0) / 1000.0, mbps = secs > 0.5 ? g_dlGot / 1048576.0 / secs : 0;
            int left = mbps > 0.05 && g_dlTotal ? (int)((g_dlTotal - g_dlGot) / 1048576.0 / mbps) : -1;
            if (left >= 0) _snwprintf(p, 200, L2(L"%.1f de %.1f MB  ·  %.1f MB/s  ·  faltam %d s", L"%.1f of %.1f MB  ·  %.1f MB/s  ·  %d s left"), g_dlGot / 1048576.0, g_dlTotal / 1048576.0, mbps, left);
            else _snwprintf(p, 200, L2(L"%.1f MB baixados...", L"%.1f MB downloaded..."), g_dlGot / 1048576.0);
        }
        Text(dc, g_fSmall, C_SUB, c.left + SC(34), py + SC(20), p);
        if (g_updState == 2) { RECT cb = {c.right - SC(34) - SC(160), c.bottom - SC(78), c.right - SC(34), c.bottom - SC(30)}; Button(dc, g, cb, L2(L"Cancelar", L"Cancel"), H_UPDCANCEL, 2); }
    } else {
        if (g_updState == 5 && g_updErr[0]) { RECT er = {c.left + SC(34), py, c.right - SC(34), py + SC(44)}; Wrap(dc, g_fSmall, C_BAD, er, g_updErr, TRUE); }
        RECT go = {c.right - SC(34) - SC(240), c.bottom - SC(78), c.right - SC(34), c.bottom - SC(30)}; Button(dc, g, go, g_updState == 5 ? L2(L"Tentar de novo", L"Try again") : L2(L"ATUALIZAR AGORA", L"UPDATE NOW"), H_UPDGO, 1);
        RECT lt = {go.left - SC(12) - SC(150), go.top, go.left - SC(12), go.bottom}; Button(dc, g, lt, L2(L"Depois", L"Later"), H_UPDLATER, 2);
    }
}
static void PaintNews(HDC dc, GpGraphics *g, RECT rc)
{
    Dim(g, rc);
    RECT c = Card(g, 170, 66, 940, 680);
    {   wchar_t t[96]; _snwprintf(t, 96, L2(L"Novidades do Portheon %s", L"What's new in Portheon %s"), APP_SHOW); t[95] = 0;   /* 4.0.3: one text for every version, so it is translated */
        Text(dc, g_fH2, RGB(255, 255, 255), c.left + SC(34), c.top + SC(26), t); }
    CloseX(g, c, H_NEWSCLOSE);
    /* 4.4.0 (2026-11-04): the user's "a new interface like a video game": accounts with a photo and a status, the new
       opening, the game's picture while it loads, PORTHEON stepping out of the game's way, the cleaner author corner */
    /* 5.2 (2026-11-07): compatibility for every PC, the installer, UFC 5's smooth mode, UFC 5 01.032.
       6.0 (2026-11-07, 5.2 was never published): the console look, Zelda and God of War III, UFC 6's new options,
       keyboards, 480p/360p, fixes */
    /* 6.1 (2026-11-08): Bloodborne and inFAMOUS, the player's own games, UFC 1/3/4's TV camera, UFC 2 online/FPS, the reports */
    /* 6.2 (2026-11-08, the user: "put everything you find amazing in their interface into ours, let's impress"; then "the
       edits WITH their music"): the vitrine (YouTube edits and trailers with sound, the player's video, PORTHEON's live
       edit), the live cards, the chips and quick options */
    /* 7.1 (2026-11-14, "Portheon Update 7.1"): only what this version changes, one line per change, pt[i] and en[i] the
       same line. Each line can be dropped alone (keep both arrays the same length). The 7.0 / 6.2 lines are in
       _build-tools\backup\pre-7.1-src-1114\portheon-src\ui_screens.c; the 7.1 lines in
       _build-tools\backup\pre-7.2-src-1116\portheon-src\ui_screens.c. 7.2 (2026-11-16): PORTHEON ARENA inside PORTHEON;
       the last line sums up 7.1 for players coming from 7.0 (7.1's GitHub release had no Windows zip, so 7.0's updater
       never offered it). */
    static const wchar_t *const pt[] = {
        L"PORTHEON ARENA lá em cima, ao lado de Jogos e Online, com servidores por região (Brasil, Estados Unidos, Europa, Rússia, Coreia / Ásia). A luta usa o link de convite do Parsec (Share), que funciona entre desconhecidos (o Peer ID dava o erro 99). Quem entra clica em \"Entrar na luta\"; quem hospeda não precisa mais ligar o Modo Parsec. Todos precisam estar na 7.3.",
        L"UFC 6: DLSS e AMD FSR também na versão 01.004 do jogo (ainda sem luta de teste nela). O jogo está pronto para a próxima versão (01.007): o PORTHEON acha as correções pelo código do jogo.",
        L"UFC 6: se o jogo fecha ao carregar duas vezes seguidas, o PORTHEON tenta sozinho o outro motor. Se o modo PS5 Pro cai mais de uma vez na mesma checagem do jogo, ele abre no motor normal para a luta não cair no meio.",
        L"UFC 4: corrigido o fechamento ao abrir em pastas do jogo que passaram pelo launcher do Native Plus, e o motor agora recebe os ajustes de memória e de limpeza de texturas do launcher dele (pensado para placas de 4 a 6 GB e sessões longas; ainda sem medição nessas placas).",
        L"Presets gráficos em 7 níveis em todos os UFC: Muito baixo, Baixo, Médio, Alto, Ultra, Máximo e Extremo.",
        L"Linux (Wine/Proton): o ARENA avisa que ainda não abre aqui. © 2026 Marcelo Miyake (XAndromedaXR).",
    };   /* 7.3 (2026-11-17) */
    static const wchar_t *const en[] = {
        L"PORTHEON ARENA on the top bar, next to Games and Online, with servers by region (Brazil, United States, Europe, Russia, Korea / Asia). Fights use Parsec's Share link, which works between strangers (the Peer ID gave error 99). The guest clicks \"Join the fight\"; the host no longer has to turn Parsec mode on. Everyone needs 7.3.",
        L"UFC 6: DLSS and AMD FSR also on game version 01.004 (no test fight on it yet). The game is ready for the next version (01.007): PORTHEON finds the fixes by the game's code.",
        L"UFC 6: when the game closes while loading twice in a row, PORTHEON tries the other engine by itself. When PS5 Pro mode ends more than once on the same check of the game, it opens the usual engine so fights do not end halfway.",
        L"UFC 4: fixed the close at start in game folders that went through Native Plus's launcher, and the engine now gets its launcher's memory and texture clean-up settings (meant for 4 to 6 GB cards and long sessions; not measured on those cards yet).",
        L"Graphics presets in 7 steps in every UFC: Very low, Low, Medium, High, Ultra, Max and Extreme.",
        L"Linux (Wine/Proton): the ARENA says it does not open here yet. © 2026 Marcelo Miyake (XAndromedaXR).",
    };

    int y = c.top + SC(84);
    for (int i = 0; i < (int)(sizeof(pt) / sizeof(pt[0])); i++) {
        const wchar_t *s = g_lang == LANG_PT ? pt[i] : TR(pt[i], en[i]);
        RECT r = {c.left + SC(66), y, c.right - SC(40), y + SC(80)};
        int h = Wrap(dc, g_fText, C_TEXT, r, s, FALSE);
        Oct(g, c.left + SC(44), y + SC(12), SC(6), A(C_CYAN, 255), 0, 0);   /* 5.0: an octagon bullet */
        Wrap(dc, g_fText, C_TEXT, r, s, TRUE);
        y += h + SC(14);
    }
    RECT ok = {c.right - SC(34) - SC(170), c.bottom - SC(72), c.right - SC(34), c.bottom - SC(26)}; Button(dc, g, ok, L2(L"Fechar", L"Close"), H_NEWSCLOSE, 1);
}
/* the profile editor: photo, name, status, color, symbol. 4.4.0: also "create your account" (g_peWelcome: the first
   start's welcome screen edits the profile in use) */
static int g_peIndex = -1, g_peColor, g_peAvatar, g_peStatus, g_pePhoto;   /* g_peIndex -1: a new profile; g_pePhoto 0 keep, 1 the new picture (g_pePhotoImg), 2 removed */
static BOOL g_peWelcome;
static Profile ProfEditPreview(void)
{
    Profile pv = {L"", g_peColor, g_peAvatar, g_peIndex >= 0 ? g_prof[g_peIndex].id : -1, g_peStatus, 0};
    GetWindowTextW(g_editProf, pv.name, 32);
    pv.photo = g_pePhoto == 1 ? 2 : g_pePhoto == 2 ? 0 : g_peIndex >= 0 ? g_prof[g_peIndex].photo : 0;
    return pv;
}
static void PaintProfEdit(HDC dc, GpGraphics *g, RECT rc)
{
    Dim(g, rc);
    RECT c = Card(g, 236, 64, 808, 668);
    const wchar_t *title = g_peWelcome ? L2(L"Criar sua conta", L"Create your account") : g_peIndex < 0 ? L2(L"Novo usuário", L"New user") : L2(L"Editar perfil", L"Edit profile");
    Text(dc, g_fH2, RGB(255, 255, 255), c.left + SC(34), c.top + SC(24), title);
    Text(dc, g_fSmall, C_SUB, c.left + SC(36), c.top + SC(62), L2(L"Como você quer aparecer no PORTHEON e nos jogos.", L"How you want to show in PORTHEON and in the games."));
    CloseX(g, c, H_PROFCANCEL);
    Profile pv = ProfEditPreview();
    /* the left column: the picture, big, and its buttons */
    int px = c.left + SC(150), py = c.top + SC(206), pr = SC(86);
    Glow(g, px, py, pr + SC(46), pr + SC(46), A(PROF_COLORS[g_peColor % 12], 60));
    AvatarSt(dc, g, px, py, pr, &pv, TRUE);
    RECT b1 = {c.left + SC(40), c.top + SC(318), c.left + SC(260), c.top + SC(364)}; IconButton(dc, g, b1, pv.photo ? L2(L"Trocar a foto", L"Change the photo") : L2(L"Escolher foto", L"Pick a photo"), H_PHOTO, 0, 0);
    if (pv.photo) { RECT b2 = {c.left + SC(40), c.top + SC(374), c.left + SC(260), c.top + SC(416)}; Button(dc, g, b2, L2(L"Remover foto", L"Remove the photo"), H_PHOTODEL, 2); }
    RECT ph = {c.left + SC(40), c.top + SC(pv.photo ? 424 : 374), c.left + SC(260), c.top + SC(pv.photo ? 470 : 420)};
    {   SelectObject(dc, g_fSmall); SetTextColor(dc, C_DIM); GdiText(dc, TRUE);
        DrawTextW(dc, L2(L"PNG, JPG, BMP ou GIF. A foto fica só neste PC.", L"PNG, JPG, BMP or GIF. The photo stays on this PC."), -1, &ph, DT_WORDBREAK | DT_CENTER | DT_NOPREFIX); GdiText(dc, FALSE); }
    /* the right column */
    int x = c.left + SC(296), y = c.top + SC(100), w = c.right - SC(34) - x;
    Text(dc, g_fSmall, C_SUB, x, y, L2(L"Nome (o nome que aparece nos jogos)", L"Name (the name the games show)"));
    RECT er = {x, y + SC(22), x + w, y + SC(62)}; Box(g, er, SC(10), A(C_FIELD, 255), A(C_CYAN, 255), 1.0f);
    RECT ei = {er.left + SC(14), er.top + SC(10), er.right - SC(12), er.bottom - SC(10)}; g_editWant[3] = TRUE; PlaceEdit(3, g_editProf, ei, TRUE);
    y += SC(84); Text(dc, g_fSmall, C_SUB, x, y, L2(L"Status", L"Status")); y += SC(22);
    {   int cw = (w - 2 * SC(10)) / 3;
        for (int k = 0; k < 3; k++) {
            RECT r = {x + k * (cw + SC(10)), y, x + k * (cw + SC(10)) + cw, y + SC(44)}; BOOL on = g_peStatus == k, hot = g_hot == H_PSTAT + k;
            Box(g, r, SC(22), A(on ? ProfStatusColor(k) : RGB(255, 255, 255), on ? 40 : hot ? 22 : 9), A(on ? ProfStatusColor(k) : hot ? C_HOT : C_FIELDB, on ? 230 : 255), on ? 1.6f : 1.0f);
            StatusBadge(g, r.left + SC(22), (r.top + r.bottom) / 2, SC(6), k);
            RECT tr = {r.left + SC(38), r.top, r.right - SC(8), r.bottom}; TextR(dc, g_fBtn, on || hot ? RGB(255, 255, 255) : C_TEXT, tr, ProfStatusName(k), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
            AddHit(r.left, r.top, r.right, r.bottom, H_PSTAT + k);
        }
        y += SC(52); Text(dc, g_fSmall, ProfStatusColor(g_peStatus), x, y, ProfStatusHint(g_peStatus)); }
    y += SC(40); Text(dc, g_fSmall, C_SUB, x, y, L2(L"Cor", L"Color")); y += SC(22);
    for (int i = 0; i < 12; i++) {
        int cx = x + SC(18) + (i % 6) * SC(46), cy = y + SC(18) + (i / 6) * SC(44); BOOL on = i == g_peColor, hot = g_hot == H_COLOR + i;
        Dot(g, cx, cy, SC(16), A(PROF_COLORS[i], 255)); if (on || hot) Ring(g, cx, cy, SC(20), A(on ? RGB(255, 255, 255) : C_CYAN, 255), 2.0f);
        AddHit(cx - SC(20), cy - SC(20), cx + SC(20), cy + SC(20), H_COLOR + i);
    }
    y += SC(100); Text(dc, g_fSmall, C_SUB, x, y, pv.photo ? L2(L"Símbolo (aparece quando não há foto)", L"Symbol (shows when there is no photo)") : L2(L"Símbolo", L"Symbol")); y += SC(22);
    for (int i = 0; i < NAVATARS; i++) {
        int cx = x + SC(22) + i * SC(52), cy = y + SC(24); BOOL on = i == g_peAvatar, hot = g_hot == H_AVATAR + i;
        Profile a = pv; a.avatar = i; a.photo = 0; Avatar(dc, g, cx, cy, SC(20), &a, FALSE);
        if (on || hot) Ring(g, cx, cy, SC(25), A(on ? RGB(255, 255, 255) : C_CYAN, 255), 2.0f);
        AddHit(cx - SC(25), cy - SC(25), cx + SC(25), cy + SC(25), H_AVATAR + i);
    }
    int by = c.bottom - SC(74);
    RECT sb = {c.right - SC(34) - SC(190), by, c.right - SC(34), by + SC(48)}; Button(dc, g, sb, g_peWelcome ? L2(L"Começar", L"Let's go") : L2(L"Salvar", L"Save"), H_PROFSAVE, 1);
    RECT cb = {sb.left - SC(12) - SC(150), by, sb.left - SC(12), by + SC(48)}; Button(dc, g, cb, g_peWelcome ? L2(L"Agora não", L"Not now") : L2(L"Cancelar", L"Cancel"), H_PROFCANCEL, 2);
    if (g_peIndex > 0 && !g_peWelcome) { RECT db = {c.left + SC(34), by, c.left + SC(34) + SC(190), by + SC(48)}; Button(dc, g, db, L2(L"Remover usuário", L"Remove user"), H_PROFDEL, 2); }
}

/* ---------------------------------------------------------------- 4.4.0: the profile card under the top bar's picture
   As a console's: who is playing and their status (a click changes it), then edit, switch user, add a user and manage */
static void PaintProfMenu(HDC dc, GpGraphics *g, RECT rc)
{
    Fill(g, -g_ox, -g_oy, rc.right, rc.bottom, A(RGB(0, 0, 0), 80));
    Profile *p = CurProf(); BOOL canAdd = g_nprof < MAXPROF;
    int nAct = canAdd ? 4 : 3, w = 372, h = 112 + 3 * 54 + 18 + nAct * 46 + 14, x = BASE_W - MX - w, y = 68;
    RECT c = {SC(x), SC(y), SC(x + w), SC(y + h)};
    RECT sh = {c.left + SC(6), c.top + SC(12), c.right + SC(6), c.bottom + SC(12)}; Box(g, sh, SC(20), A(RGB(0, 0, 0), 130), 0, 0);
    Box(g, c, SC(20), A(RGB(16, 12, 28), 250), 0, 0);
    VGrad(g, c.left + SC(1), c.top + SC(1), c.right - c.left - SC(2), SC(100), A(PROF_COLORS[p->color % 12], 46), A(RGB(16, 12, 28), 0));
    GradBox(g, c, SC(20), A(C_CYAN, 170), A(C_PURPLE, 170), FALSE, 1.2f);
    AvatarSt(dc, g, c.left + SC(56), c.top + SC(56), SC(34), p, FALSE);
    RECT nr = {c.left + SC(104), c.top + SC(28), c.right - SC(20), c.top + SC(58)}; TextR(dc, g_fCard, RGB(255, 255, 255), nr, p->name, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    RECT sr = {c.left + SC(104), c.top + SC(60), c.right - SC(20), c.top + SC(82)}; TextR(dc, g_fSmall, ProfStatusColor(p->status), sr, ProfStatusName(p->status), DT_LEFT | DT_VCENTER);
    int yy = c.top + SC(112);
    for (int k = 0; k < 3; k++) {
        RECT r = {c.left + SC(12), yy, c.right - SC(12), yy + SC(50)}; BOOL hot = g_hot == H_PSTAT + k, on = p->status == k;
        if (hot || on) Box(g, r, SC(12), A(RGB(255, 255, 255), hot ? 24 : 10), 0, 0);
        StatusBadge(g, r.left + SC(26), (r.top + r.bottom) / 2, SC(7), k);
        RECT t1 = {r.left + SC(48), r.top + SC(4), r.right - SC(40), r.top + SC(26)}; TextR(dc, g_fBtn, RGB(255, 255, 255), t1, ProfStatusName(k), DT_LEFT | DT_VCENTER);
        RECT t2 = {r.left + SC(48), r.top + SC(25), r.right - SC(40), r.bottom - SC(4)}; TextR(dc, g_fSmall, C_SUB, t2, ProfStatusHint(k), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        if (on) IconCheck(g, r.right - SC(30), (r.top + r.bottom) / 2 - SC(7), SC(14), A(ProfStatusColor(k), 255));
        AddHit(r.left, r.top, r.right, r.bottom, H_PSTAT + k);
        yy += SC(54);
    }
    Line(g, c.left + SC(20), yy + SC(6), c.right - SC(20), yy + SC(6), A(RGB(255, 255, 255), 30), 1.0f); yy += SC(18);
    static const struct { int id, icon; const wchar_t *pt, *en; } act[4] = {
        {H_PMEDIT, 0, L"Editar perfil e foto", L"Edit profile and photo"}, {H_PMSWITCH, 1, L"Trocar de usuário", L"Switch user"},
        {H_PMADD, 2, L"Adicionar usuário", L"Add user"}, {H_PMMANAGE, 3, L"Gerenciar usuários", L"Manage users"}};
    for (int k = 0; k < 4; k++) {
        if (act[k].id == H_PMADD && !canAdd) continue;
        RECT r = {c.left + SC(12), yy, c.right - SC(12), yy + SC(42)}; BOOL hot = g_hot == act[k].id;
        if (hot) Box(g, r, SC(12), A(RGB(255, 255, 255), 24), 0, 0);
        UINT32 ic = A(hot ? RGB(255, 255, 255) : C_SUB, 255); int ix = r.left + SC(18), iy = (r.top + r.bottom) / 2 - SC(8), is = SC(16);
        switch (act[k].icon) { case 0: IconUser(g, ix, iy, is, ic); break; case 1: IconPad(g, ix - SC(2), iy, is + SC(4), ic); break; case 2: IconPlus(g, ix + SC(1), iy + SC(1), is - SC(2), ic); break; default: IconGear(g, ix, iy, is, ic); }
        RECT t = {r.left + SC(48), r.top, r.right - SC(30), r.bottom}; TextR(dc, g_fBtn, hot ? RGB(255, 255, 255) : C_TEXT, t, TR(act[k].pt, act[k].en), DT_LEFT | DT_VCENTER);
        Chevron(g, r.right - SC(20), (r.top + r.bottom) / 2, SC(4), 1, A(hot ? C_HOT : C_DIM, 255));
        AddHit(r.left, r.top, r.right, r.bottom, act[k].id);
        yy += SC(46);
    }
}

