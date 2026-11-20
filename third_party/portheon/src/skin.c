/* skin.c - part of ui.c (6.2, the user 2026-11-09: "the best interface of all, for each game", "original and unique"):
 * each game's own look on its home screen and its hub. A look is a color (the interface's purple family turns to the
 * game's hue, theme.c), the shape of its buttons and cards, the corners of its panels, the lettering of its name, a
 * drawing laid over its art and the side the name and the video stand on. The drawing goes into the cached background
 * (GameBg), so a look costs nothing per frame. Settings > Interface turns the looks off (game_skins=0 in portheon.ini);
 * custom games, Settings and the Online center keep the color picked in Settings. */
enum { SKM_OCTAGON = 1, SKM_BLOCKS, SKM_DIAGONALS, SKM_SPLIT, SKM_EMBER, SKM_BELT, SKM_CHEVRON, SKM_TRI, SKM_MEANDER, SKM_MOON, SKM_SPARKS, SKM_ROPES };
enum { SKB_ROUND, SKB_SQUARE, SKB_SLANT, SKB_CUT, SKB_PILL };
enum { SKC_GLASS, SKC_TOPLINE, SKC_LEFTBAR, SKC_CUT };
typedef struct {
    const wchar_t *id; float hue; COLORREF acc, acc2; BOOL darkInk;
    const wchar_t *font; int weight; BOOL italic; int spacing, split;   /* split: the last word of the name takes the color */
    int shape, rad, card, motif, side; float zoom, ax, ay;              /* side 1: the video on the left, the name on the right (7.0: not read, see SKIN_SIDE); zoom, ax, ay: the art's crop (7.0: UFC 2 and UFC 3 moved so their fighters stand behind the video, not under the name) */
    COLORREF gs, gm, gm2, gh; float gAmt; int gMode;                     /* the art's color grade: shadows, middle tones (and a second one), lights, how much, and how it spreads */
} GameSkin;
/* 6.2 (the owner: "all in this style", "even more incredible the effect on each one's colors", "not generic"): one layout for
   all (the video on the left, the name on the right, cards with a bar), and for each game its own grade of the art */
enum { SKG_EVEN, SKG_FLOOR, SKG_ACROSS, SKG_EDGES, SKG_RISING };
#define SKS SKB_PILL, 16, SKC_GLASS   /* 7.0 (the console's look): one shape for all: pill buttons, glass cards (6.2: SKB_CUT, 12, SKC_LEFTBAR) */
static const GameSkin SKINS[] = {
    {L"UFC1",       208.0f, RGB0(96, 178, 255),  RGB0(210, 228, 246), TRUE,  L"Bahnschrift SemiBold Condensed", 700, FALSE, 2, 1, SKS, SKM_OCTAGON,   1, 1.0f,  0.5f, 0.5f,  RGB0(6, 14, 34),   RGB0(40, 112, 205),  RGB0(40, 112, 205),  RGB0(220, 238, 255), 0.64f, SKG_EVEN},
    {L"UFC2",        47.0f, RGB0(255, 206, 0),   RGB0(255, 255, 255), TRUE,  L"Segoe UI Black",                 900, TRUE,  0, 1, SKS, SKM_BLOCKS,    1, 1.35f, 0.0f, 0.42f, RGB0(26, 16, 0),   RGB0(214, 150, 0),   RGB0(214, 150, 0),   RGB0(255, 238, 160), 0.60f, SKG_FLOOR},
    {L"UFC3",       356.0f, RGB0(228, 30, 42),   RGB0(240, 240, 244), FALSE, L"Impact",                         400, TRUE,  1, 1, SKS, SKM_DIAGONALS, 1, 1.35f, 0.0f, 0.5f,  RGB0(20, 0, 4),    RGB0(196, 18, 34),   RGB0(196, 18, 34),   RGB0(255, 232, 226), 0.62f, SKG_EDGES},
    {L"UFC4",       214.0f, RGB0(46, 140, 255),  RGB0(232, 48, 58),   TRUE,  L"Bahnschrift SemiBold Condensed", 600, TRUE,  1, 1, SKS, SKM_SPLIT,     1, 1.0f,  0.5f, 0.5f,  RGB0(8, 4, 14),    RGB0(210, 30, 48),   RGB0(28, 110, 235),  RGB0(240, 240, 255), 0.62f, SKG_ACROSS},
    {L"UFC6",        44.0f, RGB0(226, 186, 96),  RGB0(255, 246, 220), TRUE,  L"Bahnschrift",                    700, FALSE, 3, 1, SKS, SKM_BELT,      1, 1.0f,  0.5f, 0.5f,  RGB0(6, 5, 2),     RGB0(150, 112, 40),  RGB0(150, 112, 40),  RGB0(255, 236, 176), 0.76f, SKG_EVEN},
    {L"UFC5",        14.0f, RGB0(255, 96, 44),   RGB0(255, 200, 120), TRUE,  L"Arial Black",                    900, FALSE, 0, 1, SKS, SKM_EMBER,     1, 1.0f,  0.5f, 0.5f,  RGB0(24, 6, 0),    RGB0(226, 84, 20),   RGB0(226, 84, 20),   RGB0(255, 222, 176), 0.62f, SKG_FLOOR},
    {L"UFC3U",        2.0f, RGB0(206, 36, 40),   RGB0(190, 196, 206), FALSE, L"Arial Black",                    900, TRUE,  0, 1, SKS, SKM_CHEVRON,   1, 1.0f,  0.5f, 0.5f,  RGB0(10, 2, 2),    RGB0(150, 22, 26),   RGB0(150, 22, 26),   RGB0(208, 214, 224), 0.58f, SKG_EVEN},
    {L"ZELDA",      132.0f, RGB0(226, 190, 84),  RGB0(120, 214, 140), TRUE,  L"Georgia",                        700, FALSE, 1, 0, SKS, SKM_TRI,       1, 1.9f,  0.0f, 0.5f,  RGB0(0, 22, 18),   RGB0(26, 150, 92),   RGB0(26, 150, 92),   RGB0(250, 226, 130), 0.62f, SKG_EDGES},
    {L"GOW3",         6.0f, RGB0(200, 46, 38),   RGB0(196, 150, 84),  FALSE, L"Constantia",                     700, FALSE, 3, 1, SKS, SKM_MEANDER,   1, 1.0f,  0.5f, 0.5f,  RGB0(26, 2, 0),    RGB0(170, 30, 20),   RGB0(170, 30, 20),   RGB0(232, 190, 120), 0.62f, SKG_FLOOR},
    {L"BLOODBORNE", 350.0f, RGB0(168, 34, 48),   RGB0(232, 222, 196), FALSE, L"Palatino Linotype",              400, TRUE,  2, 0, SKS, SKM_MOON,      1, 1.0f,  0.5f, 0.5f,  RGB0(10, 4, 20),   RGB0(120, 18, 40),   RGB0(120, 18, 40),   RGB0(236, 226, 196), 0.64f, SKG_EDGES},
    {L"INFAMOUS",    24.0f, RGB0(255, 126, 36),  RGB0(255, 214, 150), TRUE,  L"Bahnschrift Condensed",          700, FALSE, 1, 0, SKS, SKM_SPARKS,    1, 1.0f,  0.5f, 0.5f,  RGB0(14, 6, 18),   RGB0(255, 120, 30),  RGB0(170, 40, 200),  RGB0(255, 230, 200), 0.62f, SKG_RISING},
    {L"GTA6",        322.0f, RGB0(255, 72, 164),  RGB0(255, 196, 120), TRUE,  L"Bahnschrift Condensed",          700, FALSE, 1, 1, SKS, SKM_SPARKS,    1, 1.3f,  0.0f, 0.5f,  RGB0(24, 8, 44),  RGB0(226, 60, 160),  RGB0(255, 136, 64),  RGB0(255, 228, 214), 0.34f, SKG_RISING},
    {L"FNCREBORN",   42.0f, RGB0(236, 182, 64),  RGB0(255, 240, 210), TRUE,  L"Impact",                         400, FALSE, 1, 0, SKS, SKM_ROPES,     1, 1.0f,  0.5f, 0.5f,  RGB0(20, 12, 2),   RGB0(170, 120, 40),  RGB0(170, 120, 40),  RGB0(255, 240, 205), 0.56f, SKG_EDGES},
};
static BOOL g_skinsOn = TRUE;
static const GameSkin *g_skin;   /* the look of what is being drawn; NULL: PORTHEON's own */
static void SkinsLoad(void) { g_skinsOn = PIni(L"game_skins", 1) != 0; }
static const GameSkin *SkinOf(int gi)
{
    static const GameSkin *map[NGAMES]; static BOOL made;
    if (!made) { made = TRUE; for (int i = 0; i < NGAMES; i++) for (int k = 0; k < (int)(sizeof(SKINS) / sizeof(SKINS[0])); k++) if (!wcscmp(SKINS[k].id, G[i].id)) map[i] = &SKINS[k]; }
    return g_skinsOn && gi >= 0 && gi < NGAMES ? map[gi] : NULL;
}
static void SkinSync(void)   /* before every paint */
{
    g_skin = (g_view == V_HOME && g_csel < 0) || g_view == V_GAME ? SkinOf(g_sel) : NULL;
    ThemeSkin(g_skin ? g_skin->hue : -1.0f);
}
#define SKIN_SIDE(k) 0   /* 7.0 (the console's home): the name on the left and the video on the right for every game; ((k)->side) brings 6.2's sides back */
static int SkinSide(void) { return g_skin && g_view == V_HOME ? SKIN_SIDE(g_skin) : 0; }
static int SkinRad(int def) { return g_skin ? SC(g_skin->rad) : def; }
static COLORREF SkinAcc(void) { return g_skin ? g_skin->acc : RGB0(255, 255, 255); }
static __attribute__((unused)) COLORREF SkinInk(void) { return g_skin && !g_skin->darkInk ? RGB0(255, 255, 255) : RGB0(14, 14, 18); }

/* a button's outline in the game's shape */
static GpPath *SkinPath(RECT r, int rad)
{
    int sh = g_skin ? g_skin->shape : SKB_ROUND, w = r.right - r.left - 1, h = r.bottom - r.top - 1;
    if (sh == SKB_SQUARE) return RoundPath(r.left, r.top, w, h, SC(3));
    if (sh == SKB_PILL || sh == SKB_ROUND) return RoundPath(r.left, r.top, w, h, h <= SC(64) ? h / 2 : rad);   /* 7.0: a button is a pill, with or without a game's look */
    if (sh == SKB_SLANT || sh == SKB_CUT) {
        float l = (float)r.left, t = (float)r.top, R = (float)(r.left + w), B = (float)(r.top + h), k = h * 0.24f, c = (float)(h / 3 < SC(12) ? h / 3 : SC(12));
        GpPointF sl[4] = {{l + k, t}, {R, t}, {R - k, B}, {l, B}}, ct[8] = {{l + c, t}, {R - c, t}, {R, t + c}, {R, B - c}, {R - c, B}, {l + c, B}, {l, B - c}, {l, t + c}};
        GpPath *p = NULL; GdipCreatePath(0, &p); if (sh == SKB_SLANT) GdipAddPathPolygon(p, sl, 4); else GdipAddPathPolygon(p, ct, 8); return p;
    }
    return RoundPath(r.left, r.top, w, h, rad);
}
static void ShapeBox(GpGraphics *g, RECT r, int rad, UINT32 fill, UINT32 border, float bw)
{
    GpPath *p = SkinPath(r, rad);
    if (fill) { GpBrush *b = NULL; GdipCreateSolidFill(fill, &b); if (b) { GdipFillPath(g, b, p); GdipDeleteBrush(b); } }
    if (border) { GpPen *pen = NULL; GdipCreatePen1(border, bw, 2, &pen); if (pen) { GdipDrawPath(g, pen, p); GdipDeletePen(pen); } }
    GdipDeletePath(p);
}
/* a home card */
static void SkinCard(GpGraphics *g, RECT r, BOOL hot)
{
    UINT32 fill = HOME_PAGES && g_hpDraw == 1 ? A(RGB0(14, 15, 20), hot ? 214 : 190) /* Released review: page 2's cards are dark glass, their second lines read over any art */ : A(RGB0(0, 0, 0), hot ? 176 : 132), edge = A(RGB0(255, 255, 255), hot ? 220 : 30); float bw = hot ? 2.0f : 1.0f;
    int st = g_skin ? g_skin->card : SKC_GLASS;
    if (st == SKC_CUT) {
        float l = (float)r.left, t = (float)r.top, R = (float)r.right - 1, B = (float)r.bottom - 1, c = (float)SC(16);
        GpPointF q[8] = {{l + c, t}, {R, t}, {R, B - c}, {R - c, B}, {l, B}, {l, t + c}, {l + c, t}, {l + c, t}};
        GpPath *p = NULL; GdipCreatePath(0, &p); GdipAddPathPolygon(p, q, 6);
        GpBrush *b = NULL; GdipCreateSolidFill(fill, &b); if (b) { GdipFillPath(g, b, p); GdipDeleteBrush(b); }
        GpPen *pen = NULL; GdipCreatePen1(hot ? edge : A(SkinAcc(), 120), bw, 2, &pen); if (pen) { GdipDrawPath(g, pen, p); GdipDeletePen(pen); }
        GdipDeletePath(p); return;
    }
    if (g_skin) {   /* the game's color rises from the card's bar */
        GpPath *p = RoundPath(r.left, r.top, r.right - r.left - 1, r.bottom - r.top - 1, SkinRad(0)); POINT a = {r.left - 1, r.top}, z = {r.right + 1, r.top}; GpBrush *lb = NULL;   /* edge to edge: this brush repeats past its end */
        GpBrush *b0 = NULL; GdipCreateSolidFill(fill, &b0); if (b0) { GdipFillPath(g, b0, p); GdipDeleteBrush(b0); }
        GdipCreateLineBrushI(&a, &z, A(SkinAcc(), hot ? 64 : 40), A(SkinAcc(), 0), 0, &lb); if (lb) { GdipFillPath(g, lb, p); GdipDeleteBrush(lb); }
        GpPen *pen = NULL; GdipCreatePen1(edge, bw, 2, &pen); if (pen) { GdipDrawPath(g, pen, p); GdipDeletePen(pen); }   /* 7.0: a white edge (it was the game's color) */
        GdipDeletePath(p);
    } else Box(g, r, SC(16), fill, edge, bw);
    if (st == SKC_TOPLINE) Fill(g, r.left + SC(10), r.top, r.right - r.left - SC(20), SC(3), A(SkinAcc(), 235));
    if (st == SKC_LEFTBAR) Fill(g, r.left, r.top + SC(12), SC(4), r.bottom - r.top - SC(24), A(SkinAcc(), 235));
}
/* the game's name in its own lettering; the last word can take the color. max: the width it must fit */
static int SkinTitle(HDC dc, int x, int y, int size, int max, int gi, COLORREF c, COLORREF accent)   /* 7.0 fix: returns where the letters end (the base line, or the tails of g, j, p, q, y) */
{
    const GameSkin *s = SkinOf(gi); const wchar_t *name = G[gi].name;
    const wchar_t *face = s ? s->font : L"Bahnschrift"; int wt = s ? s->weight : FW_BOLD, sp = s ? SC(s->spacing) : 0, split = s ? s->split : 0;
    HFONT f = NULL; int sz = size; size_t len = wcslen(name);
    for (;;) {
        if (f) DeleteObject(f);
        f = CreateFontW(-sz, 0, 0, 0, wt, s && s->italic, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, face);
        if (max <= 0 || TextW(dc, f, name) + sp * (int)len <= max || sz <= size / 2) break;
        sz = sz * 9 / 10;
    }
    const wchar_t *last = split ? wcsrchr(name, L' ') : NULL; int top = y + (size - sz) / 2;
    SetTextCharacterExtra(dc, sp);
    if (last) {
        wchar_t head[96]; size_t n = (size_t)(last - name) + 1; if (n > 95) n = 95; wcsncpy(head, name, n); head[n] = 0;
        Text(dc, f, c, x, top, head); Text(dc, f, accent, x + TextW(dc, f, head), top, last + 1);   /* (the width already counts the spacing) */
    } else Text(dc, f, c, x, top, name);
    SetTextCharacterExtra(dc, 0);
    TEXTMETRICW tm; SelectObject(dc, f); GetTextMetricsW(dc, &tm); int bottom = top + tm.tmAscent + (wcspbrk(name, L"gjpqyQ") ? tm.tmDescent * 2 / 3 : 0);   /* (the tails end above the font's own lowest line) */
    SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT)); DeleteObject(f);
    return bottom;
}

/* the drawing over the art: in 1280 units, on the side away from the name */
static BOOL g_skM;
#define SKX(x) ((g_skM ? 1280.0f - (float)(x) : (float)(x)) * s)
#define SKY(y) ((float)(y) * s)
static void SkFill(GpGraphics *g, const GpPointF *p, int n, UINT32 c)
{
    GpPath *path = NULL; GdipCreatePath(0, &path); if (!path) return; GdipAddPathPolygon(path, p, n);
    GpBrush *b = NULL; GdipCreateSolidFill(c, &b); if (b) { GdipFillPath(g, b, path); GdipDeleteBrush(b); }
    GdipDeletePath(path);
}
static void SkQuad(GpGraphics *g, float s, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, UINT32 c)
{
    GpPointF p[4] = {{SKX(x0), SKY(y0)}, {SKX(x1), SKY(y1)}, {SKX(x2), SKY(y2)}, {SKX(x3), SKY(y3)}}; SkFill(g, p, 4, c);
}
static void SkTri(GpGraphics *g, float s, float x0, float y0, float x1, float y1, float x2, float y2, UINT32 c)
{
    GpPointF p[3] = {{SKX(x0), SKY(y0)}, {SKX(x1), SKY(y1)}, {SKX(x2), SKY(y2)}}; SkFill(g, p, 3, c);
}
static void SkLine(GpGraphics *g, float s, float x0, float y0, float x1, float y1, UINT32 c, float w)
{
    GpPen *pen = NULL; GdipCreatePen1(c, w * s, 2, &pen); if (!pen) return;
    GdipDrawLineI(g, pen, (int)SKX(x0), (int)SKY(y0), (int)SKX(x1), (int)SKY(y1)); GdipDeletePen(pen);
}
static void SkinMotif(GpGraphics *g, const GameSkin *k, int w, int h)
{
    float s = w / 1280.0f, H = h / s; g_skM = SKIN_SIDE(k) != 0;
    {   /* the light of the game: a halo behind the video and a floor of its color */
        int two = k->gMode == SKG_ACROSS || k->gMode == SKG_RISING;
        Glow(g, (int)SKX(952), (int)SKY(HPV(502, 406)), (int)(380 * s), (int)(280 * s), A(two ? k->gm : k->acc, 165));   /* 7.0: the video's new place (x 664..1240, y 244..568; Released, the two pages: y 340..664) */
        VGrad(g, 0, h * 70 / 100, w, h - h * 70 / 100, A(two ? k->gm2 : k->acc, 0), A(two ? k->gm2 : k->acc, 112));
    }
#define AC(a) A(k->acc, (a))
#define AD(a) A(k->acc2, (a))
    switch (k->motif) {
    case SKM_OCTAGON:   /* steel rings of the octagon */
        Oct(g, (int)SKX(1040), (int)SKY(330), (int)(170 * s), AC(12), 0, 0);
        for (int i = 0; i < 4; i++) Oct(g, (int)SKX(1040), (int)SKY(330), (int)((170 + i * 95) * s), 0, AC(78 - i * 16), 1.6f);
        break;
    case SKM_BLOCKS:   /* the slanted yellow tiles of its menus */
        SkQuad(g, s, 830, 66, 1280, 66, 1280, 214, 790, 214, AC(40)); SkQuad(g, s, 790, 214, 1280, 214, 1280, 220, 788, 220, AC(225));
        SkQuad(g, s, 1136, 500, 1280, 500, 1280, 556, 1118, 556, AC(205)); SkQuad(g, s, 1094, 500, 1122, 500, 1104, 556, 1076, 556, AC(120));
        break;
    case SKM_DIAGONALS: {   /* the red and white bars of its screens */
        float dx = H * 0.30f;
        SkQuad(g, s, 1238, 0, 1380, 0, 1380 - dx, H, 1238 - dx, H, AC(215)); SkQuad(g, s, 1216, 0, 1226, 0, 1226 - dx, H, 1216 - dx, H, AD(170));
        SkQuad(g, s, 1182, 0, 1200, 0, 1200 - dx, H, 1182 - dx, H, A(RGB0(60, 60, 66), 200)); SkQuad(g, s, 1160, 0, 1165, 0, 1165 - dx, H, 1160 - dx, H, AC(230));
        SkTri(g, s, 0, 0, 96, 0, 0, 300, AC(150));
        break; }
    case SKM_SPLIT:   /* the red corner and the blue corner */
        Glow(g, 0, 0, w * 4 / 10, h * 5 / 10, A(RGB0(226, 38, 52), 74)); Glow(g, w, h, w * 6 / 10, h * 8 / 10, A(RGB0(30, 120, 255), 130));
        SkQuad(g, s, 1012, 0, 1016, 0, 702, H, 698, H, A(RGB0(255, 255, 255), 46));
        SkTri(g, s, 0, 0, 72, 0, 0, 72, A(RGB0(232, 48, 58), 235)); SkTri(g, s, 1280, H, 1208, H, 1280, H - 72, A(RGB0(46, 140, 255), 235));
        break;
    case SKM_EMBER:   /* embers and claw marks */
        Glow(g, (int)SKX(800), h, w * 6 / 10, h * 6 / 10, AC(135));
        for (int i = 0; i < 7; i++) { float x = 860.0f + i * 62, y = 186.0f + (i * 37) % 70, l = 150.0f + (i * 53) % 90;
            SkQuad(g, s, x, y, x + 10, y, x - 28, y + l, x - 38, y + l, (i & 1 ? AC(70 + (i * 29) % 110) : AD(60 + (i * 31) % 90))); }
        break;
    case SKM_BELT: {   /* a champion's gold: a great ring and its rays */
        int cx = (int)SKX(1110), cy = (int)SKY(350), r1 = (int)(340 * s), r2 = (int)(316 * s);
        GpPen *p1 = NULL, *p2 = NULL; GdipCreatePen1(AC(84), 2.0f * s, 2, &p1); GdipCreatePen1(AC(40), 1.0f * s, 2, &p2);
        if (p1) { GdipDrawEllipseI(g, p1, cx - r1, cy - r1, 2 * r1, 2 * r1); GdipDeletePen(p1); } if (p2) { GdipDrawEllipseI(g, p2, cx - r2, cy - r2, 2 * r2, 2 * r2); GdipDeletePen(p2); }
        for (int i = 0; i < 24; i++) { float a = i * 6.2831853f / 24, c = cosf(a), n = sinf(a); SkLine(g, s, 1110 + c * 350, 350 + n * 350, 1110 + c * 384, 350 + n * 384, AC(i % 2 ? 56 : 100), 2.0f); }
        break; }
    case SKM_CHEVRON:   /* steel chevrons */
        for (int i = 0; i < 3; i++) { float x = 1004.0f + i * 96; GpPointF p[6] = {{SKX(x), SKY(90)}, {SKX(x + 62), SKY(90)}, {SKX(x + 196), SKY(380)}, {SKX(x + 62), SKY(670)}, {SKX(x), SKY(670)}, {SKX(x + 134), SKY(380)}}; SkFill(g, p, 6, (i == 1 ? AD(92) : AC(150 - i * 36))); }
        break;
    case SKM_TRI: {   /* a shrine's gold triangles, in a green light */
        static const short T[8][4] = {{1040, 120, 46, 110}, {1180, 200, 26, 90}, {960, 262, 18, 80}, {1230, 420, 40, 90}, {1090, 520, 22, 80}, {900, 540, 30, 60}, {1200, 620, 16, 70}, {1000, 640, 14, 50}};
        Glow(g, (int)SKX(920), h * 4 / 10, w * 5 / 10, h * 6 / 10, AD(86));
        for (int i = 0; i < 8; i++) { float x = T[i][0], y = T[i][1], r = T[i][2]; SkTri(g, s, x, y - r, x + r * 0.87f, y + r * 0.5f, x - r * 0.87f, y + r * 0.5f, AC(T[i][3])); }
        break; }
    case SKM_MEANDER: {   /* a Greek key down the edge, in bronze, with a blood-red light */
        Glow(g, w, h * 4 / 10, w * 5 / 10, h * 7 / 10, AC(100));
        SkLine(g, s, 1230, 0, 1230, H, AD(140), 2.0f);
        for (int y = 6; y < (int)H; y += 44) { static const signed char P[7][2] = {{0, 0}, {36, 0}, {36, 32}, {10, 32}, {10, 12}, {24, 12}, {24, 22}};
            for (int j = 0; j < 6; j++) SkLine(g, s, 1238.0f + P[j][0], (float)y + P[j][1], 1238.0f + P[j + 1][0], (float)y + P[j + 1][1], AD(170), 2.4f); }
        break; }
    case SKM_MOON: {   /* pointed gothic arches over a crimson mist (its art already has the moon) */
        Glow(g, w * 6 / 10, h, w * 7 / 10, h * 5 / 10, AC(135));
        for (int i = 0; i < 3; i++) { float cx = 1090.0f, hw = 150.0f - i * 44, top = 150.0f + i * 66, base = 640.0f, px = cx - hw, py = base;
            for (int n = 1; n <= 16; n++) { float u = n / 16.0f, x = cx - hw * (1.0f - u * u * u), y = base - (base - top) * sinf(u * 1.5707963f), x2 = 2 * cx - x, ox = 2 * cx - px; SkLine(g, s, px, py, x, y, AD(84 - i * 20), 1.6f); SkLine(g, s, ox, py, x2, y, AD(84 - i * 20), 1.6f); px = x; py = y; } }
        break; }
    case SKM_SPARKS: {   /* smoke and neon shards */
        unsigned v = 7; SkQuad(g, s, 1280, 120, 1280, 330, 690, H, 480, H, AC(26));
        for (int i = 0; i < 46; i++) { v = v * 1103515245u + 12345u; float x = 760.0f + (v >> 8) % 520; v = v * 1103515245u + 12345u; float y = (float)((v >> 8) % (unsigned)H); v = v * 1103515245u + 12345u; float z = 4.0f + (v >> 8) % 26; v = v * 1103515245u + 12345u; int al = 40 + (v >> 8) % 170;
            SkTri(g, s, x, y, x + z, y - z * 0.4f, x + z * 0.3f, y + z, (i & 1 ? AC(al) : AD(al))); }
        break; }
    case SKM_ROPES:   /* a ring's ropes under the lights */
        SkQuad(g, s, 880, 0, 960, 0, 1270, 640, 760, 640, A(RGB0(255, 240, 200), 22)); SkQuad(g, s, 1120, 0, 1180, 0, 1280, 300, 980, 640, A(RGB0(255, 240, 200), 16));
        for (int i = 0; i < 3; i++) { float y = 410.0f + i * 50; SkLine(g, s, 620, y + 18, 1280, y - 6, AC(165), 4.0f); SkLine(g, s, 620, y + 16, 1280, y - 8, A(RGB0(255, 255, 255), 60), 1.0f); }
        break;
    }
#undef AC
#undef AD
    g_skM = FALSE;
}
#undef SKX
#undef SKY
/* the art's color grade: every pixel goes along a ramp of the game's colors by its light, in the shape the game asks for
   (once per game and size: the result is the cached background) */
typedef struct { UINT Width, Height; INT Stride; INT PixelFormat; void *Scan0; UINT_PTR Reserved; } GpBitmapData;
int WINAPI GdipBitmapLockBits(GpBitmap *, const GpRectI *, UINT, INT, GpBitmapData *);
int WINAPI GdipBitmapUnlockBits(GpBitmap *, GpBitmapData *);
static void SkRamp(BYTE *lut, COLORREF s, COLORREF m, COLORREF h)
{
    for (int i = 0; i < 256; i++) {
        int t = i < 128 ? i * 2 : (i - 128) * 2; COLORREF a = i < 128 ? s : m, b = i < 128 ? m : h;
        lut[i * 3 + 0] = (BYTE)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t / 255);
        lut[i * 3 + 1] = (BYTE)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t / 255);
        lut[i * 3 + 2] = (BYTE)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t / 255);
    }
}
static void SkinGrade(GpBitmap *bmp, const GameSkin *k, int w, int h)
{
    GpRectI rc = {0, 0, w, h}; GpBitmapData d; memset(&d, 0, sizeof(d));
    if (GdipBitmapLockBits(bmp, &rc, 3 /* read and write */, 0x26200A /* 32 bpp ARGB */, &d) != 0) return;
    static BYTE A1[768], A2[768]; SkRamp(A1, k->gs, k->gm, k->gh); SkRamp(A2, k->gs, k->gm2, k->gh);
    int base = (int)(k->gAmt * 256.0f), cx = w / 2, cy = h / 2, rr = cx * cx + cy * cy;
    for (int y = 0; y < h; y++) {
        BYTE *p = (BYTE *)d.Scan0 + y * d.Stride;
        int amtY = k->gMode == SKG_FLOOR ? base * (150 + 106 * y / h) / 256 : base, mixY = k->gMode == SKG_RISING ? 256 * y / h : -1;
        for (int x = 0; x < w; x++, p += 4) {
            int L = (p[0] * 29 + p[1] * 150 + p[2] * 77) >> 8, a = amtY, mix = mixY < 0 ? (k->gMode == SKG_ACROSS ? 256 * x / w : 0) : mixY;
            if (k->gMode == SKG_EDGES) { int dx = x - cx, dy = y - cy; a = base * (140 + 116 * (dx * dx + dy * dy) / rr) / 256; }
            for (int c = 0; c < 3; c++) {
                int g2 = (A1[L * 3 + c] * (256 - mix) + A2[L * 3 + c] * mix) >> 8;
                p[c] = (BYTE)((p[c] * (256 - a) + g2 * a) >> 8);
            }
        }
    }
    GdipBitmapUnlockBits(bmp, &d);
}
static void SkinDrawArt(GpGraphics *g, GpImage *art, const GameSkin *k, int w, int h)
{
    if (!k || k->zoom <= 1.0f) { DrawCover(g, art, 0, 0, w, h); return; }
    int W = (int)(w * k->zoom), H = (int)(h * k->zoom); DrawCover(g, art, (int)((w - W) * k->ax), (int)((h - H) * k->ay), W, H);
}
