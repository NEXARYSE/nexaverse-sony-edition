/* PORTHEON 6.2 (2026-11-09, the user: "let them set up their controllers in the options as they want, where the kick is,
 * with a drawing of the controller"): the Controller screen. Every UFC engine reads the pads through SDL3, so a remap is a
 * gamepad mapping (SDL_GAMECONTROLLERCONFIG) built at PLAY from the mapping SDL has for each connected pad, with the chosen
 * buttons swapped: nothing in the game is patched. Saved per game in portheon.ini: padmap_<id> = 16 numbers. */
#define OV_PADMAP 62
enum { H_PMROW = 984 /* 16 */, H_PMSAVE = 1000, H_PMRESET, H_PMCANCEL, NPM = 16 };
static int g_pm[NPM], g_pmGame, g_pmCap = -1; static BOOL g_pmArmed;
static const char *const PM_KEY[NPM] = {"a", "b", "x", "y", "leftshoulder", "rightshoulder", "lefttrigger", "righttrigger", "leftstick", "rightstick", "start", "back", "dpup", "dpdown", "dpleft", "dpright"};
static int g_pmCustom; static wchar_t g_pmName[96];   /* 6.2: a game of the player's library (its id), or 0 */
/* Released (2026-11-13, UFC4 Native Plus's "pad remap for player 2" in UFC 1-3): a second layout, padmap2_<id>, for the
   controllers whose GUID is not Player 1's (PmEnvFor). Two pads of the same model share a GUID and both get Player 1's. */
static int g_pmP2;
/* the pad on screen: 0 PlayStation, 1 Xbox (and the ones SDL does not name), 2 Switch */
static int PmStyle(void) { if (g_npads <= 0) return 0; int t = g_pads[0].type; return t == 1 || t == 0 ? 1 : t == 4 ? 2 : 0; }
/* a button by what this pad has printed on it */
static const wchar_t *PmName(int i)
{
    static const wchar_t *const xb[12] = {L"A", L"B", L"X", L"Y", L"LB", L"RB", L"LT", L"RT", L"LS", L"RS", L"Menu", L"View"}, *const sw[12] = {L"B", L"A", L"Y", L"X", L"L", L"R", L"ZL", L"ZR", L"LS", L"RS", L"+", L"−"};
    static const wchar_t *const ps[12] = {L"X", L"Bola", L"Quadrado", L"Triângulo", L"L1", L"R1", L"L2", L"R2", L"L3", L"R3", L"Options", L"Share"}, *const pe[12] = {L"Cross", L"Circle", L"Square", L"Triangle", L"L1", L"R1", L"L2", L"R2", L"L3", L"R3", L"Options", L"Share"};
    static const wchar_t *const dp[4] = {L"Cima", L"Baixo", L"Esquerda", L"Direita"}, *const de[4] = {L"Up", L"Down", L"Left", L"Right"};
    if (i >= 12) return TR(dp[i - 12], de[i - 12]);
    int st = PmStyle();
    if (st == 1) return xb[i]; if (st == 2) return sw[i];
    if (i == 11 && g_npads > 0 && g_pads[0].type == 3) return L"Create";
    return TR(ps[i], pe[i]);
}
static void PmRowText(int i, wchar_t *o, int n)
{
    static const wchar_t *const ap[4] = {L"chute da frente", L"chute de trás", L"soco da frente", L"soco de trás"}, *const ae[4] = {L"lead kick", L"rear kick", L"lead punch", L"rear punch"};
    if (i < 4 && !g_pmCustom) _snwprintf(o, n, L"%s: %s", PmName(i), TR(ap[i], ae[i]));
    else if (i >= 12) _snwprintf(o, n, L"%s: %s", L2(L"Direcional", L"D-pad"), PmName(i));
    else _snwprintf(o, n, L"%s", PmName(i));
    o[n - 1] = 0;
}

static void PmDefault(void) { for (int i = 0; i < NPM; i++) g_pm[i] = i; }
static BOOL PmIsDefault(void) { for (int i = 0; i < NPM; i++) if (g_pm[i] != i) return FALSE; return TRUE; }
static void PmKey(int g, wchar_t *k, int n) { if (g_pmCustom) _snwprintf(k, n, L"padmap_c%d", g_pmCustom); else _snwprintf(k, n, g_pmP2 ? L"padmap2_%s" : L"padmap_%s", G[g].id); k[n - 1] = 0; }
static void PmLoad(int g)
{
    PmDefault();
    wchar_t k[48], v[128]; PmKey(g, k, 48); GetPrivateProfileStringW(L"PORTHEON", k, L"", v, 128, g_pIni);
    int t[NPM], n = 0; wchar_t *s = v;
    while (*s && n < NPM) { t[n++] = _wtoi(s); while (*s && *s != L',') s++; if (*s) s++; }
    if (n != NPM) return;
    int seen = 0; for (int i = 0; i < NPM; i++) { if (t[i] < 0 || t[i] >= NPM || (seen & (1 << t[i]))) return; seen |= 1 << t[i]; }
    memcpy(g_pm, t, sizeof(g_pm));
}
static void PmSave(void)
{
    wchar_t k[48], v[128] = L""; PmKey(g_pmGame, k, 48);
    if (!PmIsDefault()) for (int i = 0; i < NPM; i++) { size_t l = wcslen(v); _snwprintf(v + l, 128 - l, L"%s%d", i ? L"," : L"", g_pm[i]); }
    WritePrivateProfileStringW(L"PORTHEON", k, v[0] ? v : NULL, g_pIni);
    g_overlay = OV_NONE; g_pmCap = -1; Snd(SND_OK);
    if (g_pmP2 && !g_pmCustom) Msg(C_GOOD, PmIsDefault() ? L2(L"Jogador 2 do %s: os botões padrão do jogo.", L"%s, Player 2: the game's default buttons.") : L2(L"Botões do Jogador 2 do %s salvos: valem no próximo JOGAR, nos controles de modelo diferente do controle do Jogador 1.", L"%s, Player 2's buttons saved: they apply at the next PLAY, on controllers of a different model from Player 1's."), G[g_pmGame].name);
    else
    Msg(C_GOOD, PmIsDefault() ? L2(L"Controle do %s: os botões padrão do jogo.", L"%s controller: the game's default buttons.") : L2(L"Controle do %s salvo: vale no próximo JOGAR.", L"%s controller saved: it applies at the next PLAY."), g_pmCustom ? g_pmName : G[g_pmGame].name);
}
static void PmOpen(int g) { g_pmP2 = 0; g_pmCustom = 0; g_pmGame = g; g_pmCap = -1; g_pmArmed = FALSE; g_npads = PadList(g_pads, 4); PmLoad(g); g_overlay = OV_PADMAP; g_focus = -1; Snd(SND_OK); InvalidateRect(g_wnd, NULL, FALSE); }
static void PmOpenC(int id, const wchar_t *name) { PmOpen(0); g_pmCustom = id; wcsncpy(g_pmName, name, 95); g_pmName[95] = 0; PmLoad(0); }   /* 6.2: a game the player added */
static void PmOpen2(int g) { if (g < 0 || g > 2) return; PmOpen(g); g_pmP2 = 1; PmLoad(g); InvalidateRect(g_wnd, NULL, FALSE); }   /* Released: Player 2's layout (UFC 1-3) */
#ifdef PORTHEON_TEST
void PmOpenT(int g) { PmOpen(g); }
#endif

/* the first button or trigger held, as an index of PM_KEY; -1 none */
static int PmRaw(void)
{
    if (g_sdlUp && g_sdl.pad) {
        static const signed char sdl[NPM] = {0, 1, 2, 3, 9, 10, -1, -1, 7, 8, 6, 4, 11, 12, 13, 14};
        g_sdl.Update();
        for (int i = 0; i < NPM; i++) if (sdl[i] >= 0 && g_sdl.Button(g_sdl.pad, sdl[i])) return i;
        if (g_sdl.Axis(g_sdl.pad, 4) > 16000) return 6;
        if (g_sdl.Axis(g_sdl.pad, 5) > 16000) return 7;
        return -1;
    }
    if (g_xget) {
        static const WORD bit[NPM] = {0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0, 0, 0x0040, 0x0080, 0x0010, 0x0020, 0x0001, 0x0002, 0x0004, 0x0008};
        XState st;
        for (DWORD p = 0; p < 4; p++) if (!g_xget(p, &st)) {
            for (int i = 0; i < NPM; i++) if (bit[i] && (st.pad.buttons & bit[i])) return i;
            if (st.pad.lt > 128) return 6;
            if (st.pad.rt > 128) return 7;
            return -1;
        }
    }
    return -1;
}
/* PadPoll, every 33 ms: while an action waits for a button, the pad belongs to this screen; TRUE: taken */
static BOOL PadMapPoll(void)
{
    if (g_overlay != OV_PADMAP || g_pmCap < 0) return FALSE;
    int p = PmRaw();
    if (p < 0) { g_pmArmed = TRUE; return TRUE; }   /* the press that chose the action must end first */
    if (!g_pmArmed) return TRUE;
    for (int j = 0; j < NPM; j++) if (j != g_pmCap && g_pm[j] == p) g_pm[j] = g_pm[g_pmCap];   /* the button was taken: the two swap */
    g_pm[g_pmCap] = p; g_pmCap = -1; g_pmArmed = FALSE; g_padHeld = PadRead(); Snd(SND_OK); InvalidateRect(g_wnd, NULL, FALSE);
    return TRUE;
}
static BOOL PadMapBack(void) { if (g_overlay == OV_PADMAP && g_pmCap >= 0) { g_pmCap = -1; Snd(SND_BACK); InvalidateRect(g_wnd, NULL, FALSE); return TRUE; } return FALSE; }

/* PLAY: the mapping of every connected pad with the chosen buttons swapped */
static void PmTransform(const char *m, char *out, size_t cap)
{
    char v[NPM][24]; memset(v, 0, sizeof(v));
    size_t o = 0; int field = 0; const char *p = m;
    char rest[512] = ""; size_t rn = 0;
    while (*p) {
        const char *e = strchr(p, ','); size_t n = e ? (size_t)(e - p) : strlen(p);
        if (field < 2) { if (o + n + 2 < cap) { memcpy(out + o, p, n); o += n; out[o++] = ','; } }
        else if (n) {
            const char *c = memchr(p, ':', n); int k = -1;
            if (c) for (int i = 0; i < NPM && k < 0; i++) if (strlen(PM_KEY[i]) == (size_t)(c - p) && !memcmp(p, PM_KEY[i], c - p)) k = i;
            if (k >= 0 && n - (size_t)(c + 1 - p) < 24) memcpy(v[k], c + 1, n - (size_t)(c + 1 - p));
            else if (rn + n + 2 < sizeof(rest)) { memcpy(rest + rn, p, n); rn += n; rest[rn++] = ','; rest[rn] = 0; }
        }
        field++; p += n; if (*p == ',') p++;
    }
    for (int i = 0; i < NPM; i++) if (v[g_pm[i]][0]) o += (size_t)snprintf(out + o, cap - o, "%s:%s,", PM_KEY[i], v[g_pm[i]]);
    snprintf(out + o, cap - o, "%s", rest);
}
static void PmEnvFor(int g, int custom)
{
    int was[NPM], wasC = g_pmCustom, wasP = g_pmP2, m1[NPM], m2[NPM]; BOOL two = FALSE; char p1[64] = ""; memcpy(was, g_pm, sizeof(was));
    g_pmCustom = custom;
    /* Released: Player 2's own layout (UFC 1-3): the pads whose GUID is not Player 1's get it. Player 1's GUID is the
       port's "gamepad" setting or, with none chosen, the first pad SDL lists */
    if (!custom && g >= 0 && g <= 2) {
        g_pmP2 = 1; PmLoad(g); two = !PmIsDefault();
        if (two) { wchar_t w1[64]; GetStr(g, L"gamepad", w1, 64, L""); WideCharToMultiByte(CP_UTF8, 0, w1, -1, p1, sizeof(p1), NULL, NULL); }
    }
    memcpy(m2, g_pm, sizeof(m2));
    g_pmP2 = 0; PmLoad(g); memcpy(m1, g_pm, sizeof(m1));
    if ((!PmIsDefault() || two) && PadLoadSdl() && g_sdl.MapFor) {
        BOOL local = FALSE;
        if (!g_sdlUp) {
            if (g_sdl.Hint) g_sdl.Hint("SDL_JOYSTICK_HIDAPI_PS3", "1");
            if (g_sdl.Init(SDL_INIT_GAMEPAD)) { local = TRUE; if (g_sdl.Map) { wchar_t db[MAX_PATH]; char a[MAX_PATH * 3]; PadDbPath(db); if (FileExists(db)) { WideCharToMultiByte(CP_UTF8, 0, db, -1, a, sizeof(a), NULL, NULL); g_sdl.Map(a); } } }
        }
        if (g_sdlUp || local) {
            static char all[6144]; size_t o = 0; all[0] = 0; int n = 0;
            g_sdl.Update();
            uint32_t *ids = g_sdl.GetGamepads(&n);
            for (int i = 0; ids && i < n && i < 8; i++) {
                char *m = g_sdl.MapFor(ids[i]); if (!m) continue;
                if (two) {   /* a mapping starts with the pad's GUID */
                    size_t gl = strcspn(m, ",");
                    if (!p1[0] && gl < sizeof(p1)) { memcpy(p1, m, gl); p1[gl] = 0; }
                    memcpy(g_pm, strlen(p1) == gl && !_strnicmp(m, p1, gl) ? m1 : m2, sizeof(g_pm));
                }
                char one[768]; PmTransform(m, one, sizeof(one)); if (g_sdl.Free) g_sdl.Free(m);
                if (!strstr(all, one) && o + strlen(one) + 2 < sizeof(all)) o += (size_t)snprintf(all + o, sizeof(all) - o, "%s\n", one);
            }
            if (ids && g_sdl.Free) g_sdl.Free(ids);
            if (local) g_sdl.Quit();
            if (o) { static wchar_t w[6144]; MultiByteToWideChar(CP_UTF8, 0, all, -1, w, 6144); EnvPush(L"SDL_GAMECONTROLLERCONFIG", w); }
        }
    }
    memcpy(g_pm, was, sizeof(was)); g_pmCustom = wasC; g_pmP2 = wasP;
}
static void PadMapEnv(int g) { PmEnvFor(g, 0); }
static void PadMapEnvC(int id) { PmEnvFor(0, id); }   /* 6.2: LibPlay */

static void PmSpot(HDC dc, GpGraphics *g, int cx, int cy, int r, const wchar_t *t, int act, COLORREF col)
{
    BOOL cap = g_pmCap == act, hot = g_hot == H_PMROW + act || g_focus == H_PMROW + act, on = cap || hot, moved = g_pm[act] != act;
    Oct(g, cx, cy, r, on ? A(RGB0(255, 255, 255), cap ? (BYTE)(150 + 100 * Pulse(800)) : 235) : A(col, moved ? 96 : 34), A(col, moved ? 230 : 130), 1.0f);
    if (!t[0]) Dot(g, cx, cy, SC(3), A(on ? RGB0(14, 14, 18) : col, 255));   /* 7.0 fix: a button with no letter of its own: a plain dot (its place on the pad and its row in the list say which it is) */
    int w = TextW(dc, g_fSmall, t); Text(dc, g_fSmall, on ? RGB0(14, 14, 18) : col, cx - w / 2, cy - SC(9), t);
    AddHit(cx - r, cy - r, cx + r, cy + r, H_PMROW + act);
}
static void PmBar(HDC dc, GpGraphics *g, int x, int y, int w, int h, const wchar_t *t, int act)
{
    BOOL cap = g_pmCap == act, hot = g_hot == H_PMROW + act || g_focus == H_PMROW + act, on = cap || hot, moved = g_pm[act] != act;
    RECT r = {x, y, x + w, y + h}; Box(g, r, (w < h ? w : h) / 2, on ? A(RGB(255, 255, 255), cap ? (BYTE)(150 + 100 * Pulse(800)) : 235) : A(RGB(255, 255, 255), moved ? 60 : 26), A(RGB(255, 255, 255), moved ? 150 : 70), 1.0f);
    int tw = TextW(dc, g_fSmall, t); Text(dc, g_fSmall, on ? RGB(14, 14, 18) : RGB(255, 255, 255), x + (w - tw) / 2, y + (h - SC(18)) / 2, t);
    AddHit(x, y, x + w, y + h, H_PMROW + act);
}
/* 6.2 (the user: "with a drawing of the controller each one has"): the pad that is connected, as it is laid out */
static void PmDrawPad(HDC dc, GpGraphics *g, int x0, int y0, int W)
{
    int st = PmStyle(); UINT32 body = A(RGB0(38, 38, 44), 255);
    Oct(g, x0 + SC(92), y0 + SC(160), SC(44), body, 0, 0); Oct(g, x0 + W - SC(92), y0 + SC(160), SC(44), body, 0, 0);
    { RECT b = {x0 + SC(62), y0 + SC(34), x0 + W - SC(62), y0 + SC(166)}; Box(g, b, st == 0 ? SC(46) : SC(62), body, 0, 0); }
    PmBar(dc, g, x0 + SC(78), y0 + SC(14), SC(86), SC(18), PmName(4), 4); PmBar(dc, g, x0 + W - SC(164), y0 + SC(14), SC(86), SC(18), PmName(5), 5);
    PmBar(dc, g, x0 + SC(92), y0 - SC(8), SC(66), SC(20), PmName(6), 6); PmBar(dc, g, x0 + W - SC(158), y0 - SC(8), SC(66), SC(20), PmName(7), 7);
    int fx = x0 + W - SC(128), fy, dx, dy, lx, ly, rx = x0 + W - SC(st ? 196 : 198), ry, dp = st ? 34 : 40;
    if (st == 0) { fy = y0 + SC(84); dx = x0 + SC(128); dy = y0 + SC(84); lx = x0 + SC(198); ly = y0 + SC(142); ry = y0 + SC(142); }
    else { fy = y0 + SC(80); lx = x0 + SC(128); ly = y0 + SC(80); dx = x0 + SC(196); dy = y0 + SC(128); ry = y0 + SC(128); }
    /* 7.0 fix: no console maker's symbols or colors: every face button is a white-edged circle; a pad with letters shows them, the first layout shows a dot */
    static const wchar_t *const fc[3][4] = {{L"", L"", L"", L""}, {L"A", L"B", L"X", L"Y"}, {L"B", L"A", L"Y", L"X"}};
    const COLORREF fcol = RGB0(235, 235, 240);
    PmSpot(dc, g, fx, fy + SC(28), SC(15), fc[st][0], 0, fcol); PmSpot(dc, g, fx + SC(28), fy, SC(15), fc[st][1], 1, fcol);
    PmSpot(dc, g, fx - SC(28), fy, SC(15), fc[st][2], 2, fcol); PmSpot(dc, g, fx, fy - SC(28), SC(15), fc[st][3], 3, fcol);
    PmBar(dc, g, dx - SC(13), dy - SC(dp), SC(26), SC(dp - 12), L"↑", 12); PmBar(dc, g, dx - SC(13), dy + SC(12), SC(26), SC(dp - 12), L"↓", 13);
    PmBar(dc, g, dx - SC(dp), dy - SC(13), SC(dp - 12), SC(26), L"←", 14); PmBar(dc, g, dx + SC(12), dy - SC(13), SC(dp - 12), SC(26), L"→", 15);
    PmSpot(dc, g, lx, ly, SC(21), PmName(8), 8, RGB0(255, 255, 255)); PmSpot(dc, g, rx, ry, SC(21), PmName(9), 9, RGB0(255, 255, 255));
    if (st == 0) {
        RECT tp = {x0 + W / 2 - SC(54), y0 + SC(42), x0 + W / 2 + SC(54), y0 + SC(106)}; Box(g, tp, SC(8), A(RGB0(255, 255, 255), 10), A(RGB0(255, 255, 255), 44), 1.0f);
        TextR(dc, g_fTag, C_DIM, tp, L"TOUCHPAD", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        PmBar(dc, g, x0 + W / 2 - SC(114), y0 + SC(44), SC(56), SC(20), PmName(11), 11); PmBar(dc, g, x0 + W / 2 + SC(58), y0 + SC(44), SC(56), SC(20), PmName(10), 10);
    } else {
        Oct(g, x0 + W / 2, y0 + SC(50), SC(14), A(RGB0(255, 255, 255), 18), A(RGB0(255, 255, 255), 60), 1.0f);
        PmBar(dc, g, x0 + W / 2 - SC(74), y0 + SC(60), SC(54), SC(20), PmName(11), 11); PmBar(dc, g, x0 + W / 2 + SC(20), y0 + SC(60), SC(54), SC(20), PmName(10), 10);
    }
}
static void PaintPadMap(HDC dc, GpGraphics *g, RECT rc)
{
    Dim(g, rc);
    RECT c = Card(g, 120, 34, 1040, 736);
    wchar_t t[160]; _snwprintf(t, 160, g_pmP2 && !g_pmCustom ? L2(L"Controle do %s: Jogador 2", L"%s controller: Player 2") : L2(L"Controle do %s", L"%s controller"), g_pmCustom ? g_pmName : G[g_pmGame].name); t[159] = 0;
    Text(dc, g_fH2, RGB(255, 255, 255), c.left + SC(34), c.top + SC(20), t);
    {   RECT kr = {c.left + SC(34), c.top + SC(62), c.left + SC(330), c.top + SC(86)};
        TextR(dc, g_fSmall, C_SUB, kr, g_npads > 0 ? PadKind(g_pads[0].type) : L2(L"Nenhum controle conectado", L"No controller connected"), DT_LEFT | DT_VCENTER | DT_SINGLELINE); }
    CloseX(g, c, H_PMCANCEL);
    /* the drawing */
    int W = SC(520), x0 = (c.left + c.right - W) / 2, y0 = c.top + SC(66);
    PmDrawPad(dc, g, x0, y0, W);
    /* the list */
    int cols = 2, colW = (c.right - c.left - SC(68) - SC(32)) / cols, rowH = SC(35), top = y0 + SC(216);
    for (int i = 0; i < NPM; i++) {
        int x = c.left + SC(34) + (i / 8) * (colW + SC(32)), y = top + (i % 8) * rowH;
        RECT r = {x, y, x + colW, y + rowH - SC(4)}; BOOL hot = g_hot == H_PMROW + i || g_focus == H_PMROW + i, cap = g_pmCap == i, moved = g_pm[i] != i;
        if (cap || hot) Box(g, r, (r.bottom - r.top) / 2, A(RGB(255, 255, 255), cap ? 26 : 14), cap ? A(C_CYAN, 220) : 0, cap ? 1.4f : 0);   /* 7.0 fix: pills (the row and its slot) */
        RECT lr = {r.left + SC(14), r.top, r.right - SC(176), r.bottom};
        { wchar_t rt[64]; PmRowText(i, rt, 64); TextR(dc, g_fText, cap || hot ? RGB(255, 255, 255) : C_TEXT, lr, rt, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS); }
        RECT kr = {r.right - SC(168), r.top + SC(3), r.right - SC(6), r.bottom - SC(3)};
        Box(g, kr, (kr.bottom - kr.top) / 2, A(cap ? C_CYAN : RGB(255, 255, 255), cap ? (BYTE)(60 + 80 * Pulse(900)) : moved ? 34 : 18), A(moved ? C_CYAN : C_LINE, 255), 1.0f);
        TextR(dc, g_fBtn, RGB(255, 255, 255), kr, cap ? L2(L"aperte no controle...", L"press on the pad...") : PmName(g_pm[i]), DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
        AddHit(r.left, r.top, r.right, r.bottom, H_PMROW + i);
    }
    int by = c.bottom - SC(60);
    RECT nr = {c.left + SC(34), by - SC(g_pmCustom || g_pmP2 ? 30 : 4), c.right - SC(34) - SC(420), by + SC(46)};
    Wrap(dc, g_fSmall, C_DIM, nr, g_pmP2 && !g_pmCustom ? L2(L"Botões do Jogador 2: valem nos controles de modelo diferente do controle do Jogador 1 (dois controles iguais usam os botões do Jogador 1). Clique numa ação e aperte o botão. Vale no próximo JOGAR.", L"Player 2's buttons: they apply to controllers of a different model from Player 1's (two identical controllers use Player 1's buttons). Click an action and press the button. Applies at the next PLAY.") :
                                      g_pmCustom ? L2(L"Clique numa ação e aperte o botão no controle. Vale em jogos e emuladores que leem o controle pelo SDL e abrem direto daqui. Jogos da Steam e da Epic usam o Steam Input ou a opção do próprio jogo.", L"Click an action and press the button on the pad. It works in games and emulators that read the pad through SDL and start from here. Steam and Epic games use Steam Input or the game's own option.")
                                             : L2(L"Clique numa ação e aperte o botão no controle. Se já era de outra, as duas trocam. Esc cancela. Vale no próximo JOGAR.", L"Click an action and press the button on the pad. If it belonged to another, the two swap. Esc cancels. Applies at the next PLAY."), TRUE);
    RECT sv = {c.right - SC(34) - SC(180), by, c.right - SC(34), by + SC(44)}; Button(dc, g, sv, L2(L"SALVAR", L"SAVE"), H_PMSAVE, 1);
    RECT df = {sv.left - SC(12) - SC(210), by, sv.left - SC(12), by + SC(44)}; Button(dc, g, df, L2(L"Botões padrão", L"Default buttons"), H_PMRESET, 0);
}
static void PadMapClick(int h)
{
    if (h >= H_PMROW && h < H_PMROW + NPM) { int i = h - H_PMROW; if (g_pmCap == i) g_pmCap = -1; else { g_pmCap = i; g_pmArmed = FALSE; Snd(SND_MOVE); } }
    else if (h == H_PMRESET) { PmDefault(); g_pmCap = -1; Snd(SND_MOVE); }
    else if (h == H_PMSAVE) PmSave();
    else if (h == H_PMCANCEL) { g_overlay = OV_NONE; g_pmCap = -1; Snd(SND_BACK); }
    else g_pmCap = -1;
    InvalidateRect(g_wnd, NULL, FALSE);
}
