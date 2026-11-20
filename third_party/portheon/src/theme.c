/* theme.c - part of portheon.c (included right before ui.c). 6.0 (the user, 2026-11-07: "improve PORTHEON's interface
 * too, if you want to put a new color, something new and revolutionary, feel free"): PORTHEON's color, picked in
 * Settings > Interface. The whole interface was drawn around the brand purple (C_PURPLE, C_CYAN, C_VIOLET and the
 * purple-tinted greys and darks): every color the interface code makes (RGB() in ui*.c and online.c, which portheon.c
 * compiles with RGB pointing here) whose hue is in the purple family is turned to the chosen color's hue, keeping its
 * saturation and brightness; greens, oranges, reds, blues and neutral greys stay as they are, so the status colors keep
 * their meaning. PORTHEON's own background picture and logo get the same turn (ThemeImage); the games' art never does. */

static int g_theme;   /* 0 purple (the original) .. NTHEMES-1 */
/* 6.0 final (the user: "the colors all in the new logo's, and whoever wants another color picks it in the options"):
   the new logo is white on black, so the default is the logo's own black, white and silver (THEME_MONO, a hue of -1:
   the purple family turns grey, its strong accents a light silver); the seven colors stay in Settings > Interface */
static const struct { const wchar_t *pt, *en; float hue; } THEMES[] = {
    {L"Roxo PORTHEON", L"PORTHEON purple", 264.0f}, {L"Vermelho octógono", L"Octagon red", 356.0f}, {L"Azul elétrico", L"Electric blue", 214.0f},
    {L"Verde neon", L"Neon green", 146.0f}, {L"Dourado cinturão", L"Belt gold", 44.0f}, {L"Rosa", L"Pink", 324.0f}, {L"Ciano", L"Cyan", 188.0f},
    {L"Preto e branco (do logo)", L"Black and white (the logo's)", -1.0f}};
#define NTHEMES (int)(sizeof(THEMES) / sizeof(THEMES[0]))
#define THEME_MONO (NTHEMES - 1)   /* the default, shown first in Settings > Interface */
#define RGB0(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
static float g_themeDelta;   /* degrees added to the purple family's hue */
static int g_themeUser = -1, g_themeKey;   /* 6.2: the color picked in Settings; bumped at every change (ThemeRGB's cache) */
static BOOL g_skinOn; static float g_skinHue;   /* 6.2: a game's own look is in force (skin.c) */
static BOOL g_themeMono;     /* the logo's black and white */
static void ThemeSet(int th)
{
    g_theme = th >= 0 && th < NTHEMES ? th : THEME_MONO;
    g_themeMono = THEMES[g_theme].hue < 0.0f;
    g_themeDelta = g_themeMono ? 0.0f : THEMES[g_theme].hue - THEMES[0].hue;
    g_themeUser = g_theme; g_skinOn = FALSE; g_themeKey++;
}
/* 6.2: the hue of the game on screen (skin.c), or back to the color picked in Settings (hue < 0) */
static void ThemeSkin(float hue)
{
    if (hue < 0.0f) { if (g_skinOn) ThemeSet(g_themeUser); return; }
    if (g_skinOn && g_skinHue == hue) return;
    g_skinOn = TRUE; g_skinHue = hue; g_theme = 1; g_themeMono = FALSE; g_themeDelta = hue - THEMES[0].hue; g_themeKey++;
}
static void ThemeLoad(void) { ThemeSet(PIni(L"ui_theme", THEME_MONO)); }
static COLORREF ThemeRGB(int r, int g, int b)
{
    if (!g_theme) return RGB0(r, g, b);
    static uint32_t key[4096]; static COLORREF val[4096]; static int keyTheme = -1;
    if (keyTheme != g_themeKey) { memset(key, 0xff, sizeof(key)); keyTheme = g_themeKey; }
    uint32_t in = (uint32_t)(r & 255) | ((uint32_t)(g & 255) << 8) | ((uint32_t)(b & 255) << 16);
    uint32_t h = (in * 2654435761u) >> 20;
    if (key[h] == in) return val[h];
    float R = (r & 255) / 255.0f, Gc = (g & 255) / 255.0f, B = (b & 255) / 255.0f;
    float mx = R > Gc ? (R > B ? R : B) : (Gc > B ? Gc : B), mn = R < Gc ? (R < B ? R : B) : (Gc < B ? Gc : B), d = mx - mn;
    COLORREF out = RGB0(r, g, b);
    if (mx > 0.0f && d > 0.0f && (g_themeMono || d / mx >= 0.20f)) {   /* the black and white also takes the faintly tinted greys */
        float hue = mx == R ? 60.0f * fmodf((Gc - B) / d, 6.0f) : mx == Gc ? 60.0f * ((B - R) / d + 2.0f) : 60.0f * ((R - Gc) / d + 4.0f);
        if (hue < 0) hue += 360.0f;
        if (hue >= 222.0f && hue <= 318.0f && g_themeMono) {   /* 6.0 final: the logo's grey scale: light tints near white, strong
                                                                    colors (the switches, the chosen chips, the avatar) a middle grey, so the white
                                                                    knob, text or letter on them still reads */
            float s = d / mx; int gv = (int)(mx * (s >= 0.5f ? 0.55f : 1.0f) * 255.0f + 0.5f); if (gv > 255) gv = 255;
            out = RGB0(gv, gv, gv);
        } else if (hue >= 222.0f && hue <= 318.0f && d / mx >= 0.20f) {   /* the purple family: violet, lavender, the purple-tinted darks */
            float nh = fmodf(hue + g_themeDelta + 720.0f, 360.0f), s = d / mx, v = mx;
            float c = v * s, x = c * (1.0f - fabsf(fmodf(nh / 60.0f, 2.0f) - 1.0f)), m = v - c, rr, gg, bb;
            if (nh < 60) { rr = c; gg = x; bb = 0; } else if (nh < 120) { rr = x; gg = c; bb = 0; } else if (nh < 180) { rr = 0; gg = c; bb = x; }
            else if (nh < 240) { rr = 0; gg = x; bb = c; } else if (nh < 300) { rr = x; gg = 0; bb = c; } else { rr = c; gg = 0; bb = x; }
            out = RGB0((int)((rr + m) * 255.0f + 0.5f), (int)((gg + m) * 255.0f + 0.5f), (int)((bb + m) * 255.0f + 0.5f));
        }
    }
    key[h] = in; val[h] = out; return out;
}
/* the same turn for PORTHEON's own pictures (the background and the logo): a hue rotation that keeps the luminance
   (the SVG hueRotate matrix), as a GDI+ color matrix (row vectors); alpha is the drawing's opacity */
static GpImageAttributes *ThemeImageAttr(float alpha)
{
    /* 6.0 final: the black and white leaves PORTHEON's pictures as they are (the logo is already white): no color matrix,
       so GDI+ keeps its fast path as with the original purple (a matrix on the 1024 px logo made every draw slower) */
    if ((!g_theme || g_themeMono || g_skinOn) && alpha >= 0.99f) return NULL;
    GpImageAttributes *at = NULL; GdipCreateImageAttributes(&at); if (!at) return NULL;
    GpColorMatrix cm; memset(&cm, 0, sizeof(cm));
    if (g_theme && !g_themeMono && !g_skinOn) {
        float a = g_themeDelta * 3.14159265f / 180.0f, c = cosf(a), s = sinf(a);
        float A[3][3] = {{0.213f + c * 0.787f - s * 0.213f, 0.715f - c * 0.715f - s * 0.715f, 0.072f - c * 0.072f + s * 0.928f},
                         {0.213f - c * 0.213f + s * 0.143f, 0.715f + c * 0.285f + s * 0.140f, 0.072f - c * 0.072f - s * 0.283f},
                         {0.213f - c * 0.213f - s * 0.787f, 0.715f - c * 0.715f + s * 0.715f, 0.072f + c * 0.928f + s * 0.072f}};
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) cm.m[i][j] = A[j][i];
    } else { cm.m[0][0] = cm.m[1][1] = cm.m[2][2] = 1.0f; }
    cm.m[3][3] = alpha; cm.m[4][4] = 1.0f;
    GdipSetImageAttributesColorMatrix(at, 0, TRUE, &cm, NULL, 0);
    return at;
}
#undef RGB
#define RGB(r, g, b) ThemeRGB((r), (g), (b))
