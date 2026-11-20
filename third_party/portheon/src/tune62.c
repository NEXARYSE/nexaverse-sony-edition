/* tune62.c - part of portheon.c (6.2, the owner's list: "shadows, shaders", "as many Hz as you want", "that adjusts itself for
 * each PC"): the effects quality of each UFC, the monitor's refresh rate while a game runs, and the tuning from the player's
 * own sessions: UFC 1-4 report their FPS in the log; a game that stayed below its target loses one thing for the next PLAY. */

/* ---- Effects: what each engine really lets PORTHEON change, one choice instead of many switches */
static const Opt FXQO[] = {O(L"Como está", L"As it is", 0), O(L"Baixo (mais FPS)", L"Low (most FPS)", 1), O(L"Médio", L"Medium", 2), O(L"Alto", L"High", 3), O(L"Ultra", L"Ultra", 4)};
static const Field F_FXQ = {S_COUNT, L"pgame:fxq", FXQO, N(FXQO), 0};
static void FxQuality(int g, int v)
{
    if (v < 1 || v > 4) return;
    int i = v - 1;
    if (UFC5G(g)) {
        static const int crowd[4] = {1, 0, 0, 0}, seats[4] = {1, 1, 0, 0}, shadows[4] = {1, 0, 0, 0}, fxaa[4] = {0, 1, 1, 1};
        PutInt(g, L"crowd_off", crowd[i]); PutInt(g, L"seats_off", seats[i]); PutInt(g, L"shadows_off", shadows[i]); PutInt(g, L"fxaa", fxaa[i]);
    } else if (G[g].ps5) {
        static const int crowd[4] = {1, 0, 0, 0}, dof[4] = {1, 1, 1, 0}, fxaa[4] = {0, 0, 1, 1};
        PutInt(g, L"crowd_off", crowd[i]); PutInt(g, L"dof_off", dof[i]); PutInt(g, L"fxaa", fxaa[i]);
    } else if (PS4G(g)) {
        static const int off[4] = {1, 0, 0, 0}, blur[4] = {1, 1, 0, 0}, neo[4] = {0, 0, 1, 1}, sh4[4] = {3, 1, 0, 0};
        if (g != 0) PutInt(g, L"neo", neo[i]);
        if (g == 3) { PIniSet(L"ufc4_crowd_off", off[i]); PIniSet(L"ufc4_blur_off", blur[i]); PIniSet(L"ufc4_shadows", sh4[i]); }
        else {
            if ((g == 1 || g == 2) && PatchState(g, L"crowd", -1) >= 0) PutInt(g, L"patch:crowd", off[i]);
            if (PatchState(g, L"blur", -1) >= 0) PutInt(g, L"patch:blur", blur[i]);
        }
    }
    Msg(C_GOOD, L"%s", L2(L"Efeitos ajustados: valem na próxima vez que o jogo abrir.", L"Effects set: they apply the next time the game starts."));
}

/* ---- The monitor's refresh rate: the rates this monitor really has at its current size, for the time the game runs.
   CDS_FULLSCREEN changes it only while PORTHEON lives, so it stays open (CloseOnPlay) and puts it back when the game ends. */
static Opt HZO[24] = {O(L"O do Windows (não muda)", L"Windows' own (no change)", 0)};
static wchar_t g_hzLab[24][16];
static Field F_HZ = {S_COUNT, L"pgame:hz", HZO, 1, 0};
static void HzScan(void)
{
    DEVMODEW dm; memset(&dm, 0, sizeof(dm)); dm.dmSize = sizeof(dm); DWORD w = 0, h = 0; int hz[23], n = 0;
    if (EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm)) { w = dm.dmPelsWidth; h = dm.dmPelsHeight; }
    for (DWORD i = 0; EnumDisplaySettingsW(NULL, i, &dm); i++) {
        if (dm.dmPelsWidth != w || dm.dmPelsHeight != h || dm.dmDisplayFrequency <= 1) continue;
        int v = (int)dm.dmDisplayFrequency, dup = 0; for (int k = 0; k < n; k++) if (hz[k] == v) dup = 1;
        if (dup || n >= 23) continue;
        int k = n++; while (k > 0 && hz[k - 1] > v) { hz[k] = hz[k - 1]; k--; } hz[k] = v;
    }
    for (int k = 0; k < n; k++) { _snwprintf(g_hzLab[k + 1], 16, L"%d Hz", hz[k]); HZO[k + 1].pt = HZO[k + 1].en = g_hzLab[k + 1]; HZO[k + 1].value = hz[k]; }
    F_HZ.nopts = n + 1;
}
/* perf 11-13: the scan walks every display mode (342 here: 18 ms) and wWinMain ran it before the window existed; only the
   list of rates in a game's hub needs it (HzApply reads the chosen rate itself), so BuildItems asks for it the first time */
static void HzScanOnce(void) { static BOOL done; if (!done) { done = TRUE; HzScan(); } }
static void HzApply(int g)
{
    int hz = GetInt(g, L"pgame:hz", 0); if (!hz || g_hzOn) return;
    DEVMODEW dm; memset(&dm, 0, sizeof(dm)); dm.dmSize = sizeof(dm);
    if (!EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm) || (int)dm.dmDisplayFrequency == hz) return;
    dm.dmDisplayFrequency = (DWORD)hz; dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
    if (ChangeDisplaySettingsExW(NULL, &dm, NULL, CDS_FULLSCREEN, NULL) == DISP_CHANGE_SUCCESSFUL) g_hzOn = TRUE;
}
static void HzRestore(void) { if (g_hzOn) { ChangeDisplaySettingsExW(NULL, NULL, NULL, 0, NULL); g_hzOn = FALSE; } }

/* ---- Tuning from the player's own sessions (UFC 1-4, on Automatic). The engine writes "Frame stats: 59.2 FPS" every 5 s
   while BB_FRAME_STATS is set: it is set only until the game has been steady for three sessions. */
static const Field F_LEARN = {S_COUNT, L"pgame:learn", NULL, 0, 1};
#define LEARN_STATS_DEFAULT 0   /* max-perf 11-14: measured on UFC 2 (the engine UFC 3 also runs): 12 frames of 63-167 ms per minute with the report, none without (two pairs) */
static BOOL LearnOn(int g) { return PS4G(g) && g <= 3 && GetInt(g, L"pgame:learn", 1) && GetInt(g, L"pgame:gfx_preset", 0) == 0 && GetInt(g, L"pgame:learn_ok", 0) < 3; }
static void LearnEnv(int g)
{
    if (!LearnOn(g)) return;
    /* Released 11-13: not for UFC 4. The engine's 5 s report (BB_FRAME_STATS) is itself one frame of 55-110 ms in a fight:
       5 to 13 of them per 100 s with it, 0 or 1 without (measured from outside the engine, ufc4-hitch\NOTES.md). So UFC 4
       is no longer tuned from its sessions; its starting options still come from RecValue. */
    if (g == 3) return;
    /* max-perf 11-13: learn_stats in portheon.ini decides it for UFC 1-3 (1 = the report and the tuning from the sessions,
       0 = neither, as UFC 4). Default 0 since 11-14: the report costs every player long frames every 5 s for at least
       three sessions (for ever on a PC that never gets "steady"); the price is that UFC 1-3 are no longer tuned from
       their sessions. learn_stats=1 brings both back. */
    if (!PIni(L"learn_stats", LEARN_STATS_DEFAULT)) return;
    wchar_t t[8]; if (!GetEnvironmentVariableW(L"BB_FRAME_STATS", t, 8)) EnvPush(L"BB_FRAME_STATS", L"1");
    PutInt(g, L"pgame:learn_pending", 1);
}
static int LearnCmp(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return x < y ? -1 : x > y; }
/* one thing less for the next PLAY; the text says what, NULL when nothing is left */
static const wchar_t *LearnStep(int g)
{
    if (g != 0 && GetInt(g, L"neo", 0)) { PutInt(g, L"neo", 0); return L2(L"modo PS4 Pro desligado", L"PS4 Pro mode off"); }
    if (g == 3 && !PIni(L"ufc4_crowd_off", 0)) { PIniSet(L"ufc4_crowd_off", 1); return L2(L"torcida desligada", L"crowd off"); }
    if ((g == 1 || g == 2) && PatchState(g, L"crowd", -1) == 0) { PutInt(g, L"patch:crowd", 1); return L2(L"torcida desligada", L"crowd off"); }
    if (g == 3 && !PIni(L"ufc4_blur_off", 0)) { PIniSet(L"ufc4_blur_off", 1); return L2(L"borrão de movimento desligado", L"motion blur off"); }
    if (g <= 2 && PatchState(g, L"blur", -1) == 0) { PutInt(g, L"patch:blur", 1); return L2(L"borrão de movimento desligado", L"motion blur off"); }
    int res = GetInt(g, L"resolution", 1);
    if (res >= 2) { PutInt(g, L"resolution", 1); return L2(L"resolução em 1080p", L"resolution at 1080p"); }
    if (res == 1) { PutInt(g, L"resolution", 0); PutInt(g, L"upscaler", 1); PutInt(g, L"sharpness", 2); return L2(L"resolução em 720p com FSR", L"resolution at 720p with FSR"); }
    return NULL;
}
static void LearnAfter(int g)
{
    if (!PS4G(g) || g > 3 || !GetInt(g, L"pgame:learn_pending", 0)) return;
    PutInt(g, L"pgame:learn_pending", 0);
    char *t = ReadLogTail(g, 4u << 20); if (!t) return;
    static float v[1440]; int n = 0;
    for (const char *p = t; n < 1440 && (p = strstr(p, "Frame stats: ")) != NULL; p += 13) { float f = (float)atof(p + 13); if (f > 1.0f) v[n++] = f; }
    free(t);
    if (n < 12) return;   /* under a minute of game: nothing to learn from */
    qsort(v, (size_t)n, sizeof(float), LearnCmp);
    float low = v[n * 30 / 100]; int lim = GetInt(g, L"fps_limit", 60), target = g == 0 ? 30 : lim > 0 ? lim : 60;
    /* Released (11-12): with no limit the target was the monitor's rate (120 on a 144 Hz screen), so a UFC 4 at 84-108 FPS
       on an RTX 4060 "failed" it and lost the PS4 Pro mode, the crowd and the resolution one session at a time. These
       games' logic is made for 60: a game that holds 60 is fine. */
    if (low >= target * 0.93f) { PutInt(g, L"pgame:learn_ok", GetInt(g, L"pgame:learn_ok", 0) + 1); return; }
    PutInt(g, L"pgame:learn_ok", 0);
    if (low >= target * 0.85f) return;
    const wchar_t *what = LearnStep(g);
    if (what) Msg(C_GOOD, L2(L"O %s ficou em %.0f FPS (meta: %d). Para a próxima partida, o PORTHEON ajustou: %s.", L"%s stayed at %.0f FPS (target: %d). For the next session PORTHEON changed: %s."), G[g].name, low, target, what);
}
static void LearnPendingAll(void) { for (int g = 0; g <= 3; g++) LearnAfter(g); }   /* a game PORTHEON closed for (CloseOnPlay) */
