/* pad.c - part of portheon.c (4.0): PORTHEON driven like a console, and its sounds.
 *  - Controllers: SDL3 (the copy in a port's bin folder, loaded at run time: PlayStation 3/4/5, Xbox, Switch and the
 *    generic ones SDL knows, with gamecontrollerdb.txt), or XInput when no port is there. D-pad / left stick move the
 *    focus, Cross/A selects, Circle/B goes back, L1/R1 change sections, Options/Start plays the game, Triangle/Y opens
 *    the game's options. Read only while PORTHEON's window is in front; released while a game runs (PadPause), so the
 *    game gets the controllers to itself, and not read at all then (4.1).
 *  - Keyboard: arrows, Enter/Space, Esc/Backspace, Q/E or PageUp/PageDown, F11 full screen.
 *  - Sounds: short tones made here (no sound file is shipped), off in the settings. */

/* ---------------------------------------------------------------- sounds */
static BYTE *g_snd[6]; static DWORD g_sndLen[6];
enum { SND_MOVE, SND_OK, SND_BACK, SND_BOOT, SND_ERR, SND_PLAY };
static void SndMake(int k, const float *freq, int nf, int ms, float vol, BOOL chord)
{
    int rate = 44100, n = rate * ms / 1000; DWORD bytes = (DWORD)n * 2;
    BYTE *w = (BYTE *)malloc(44 + bytes); if (!w) return;
    memcpy(w, "RIFF", 4); *(DWORD *)(w + 4) = 36 + bytes; memcpy(w + 8, "WAVEfmt ", 8); *(DWORD *)(w + 16) = 16;
    *(WORD *)(w + 20) = 1; *(WORD *)(w + 22) = 1; *(DWORD *)(w + 24) = rate; *(DWORD *)(w + 28) = rate * 2; *(WORD *)(w + 32) = 2; *(WORD *)(w + 34) = 16;
    memcpy(w + 36, "data", 4); *(DWORD *)(w + 40) = bytes;
    short *s = (short *)(w + 44);
    for (int i = 0; i < n; i++) {
        float t = (float)i / rate, v = 0, p = (float)i / n;
        if (chord) { for (int f = 0; f < nf; f++) { float st = f * 0.06f; if (t >= st) v += sinf(6.2831853f * freq[f] * (t - st)) * expf(-(t - st) * 2.6f) * (1.0f - 0.25f * f); } v *= 0.42f; }
        else { float f = nf > 1 ? freq[0] + (freq[1] - freq[0]) * p : freq[0]; v = sinf(6.2831853f * f * t) + 0.25f * sinf(6.2831853f * 2 * f * t); v *= 0.7f; }
        float env = chord ? 1.0f : (p < 0.08f ? p / 0.08f : 1.0f - (p - 0.08f) / 0.92f);
        if (chord && p > 0.85f) env = (1.0f - p) / 0.15f;
        int x = (int)(v * env * vol * 32767.0f); if (x > 32767) x = 32767; if (x < -32767) x = -32767; s[i] = (short)x;
    }
    g_snd[k] = w; g_sndLen[k] = 44 + bytes;
}
static void SoundsInit(void)
{
    static BOOL done; if (done) return; done = TRUE;
    static const float mv[] = {1320}, ok[] = {660, 990}, bk[] = {990, 620}, er[] = {220, 180}, pl[] = {523.25f, 659.25f, 783.99f, 1046.5f};
    static const float boot[] = {261.63f, 392.0f, 523.25f, 659.25f, 783.99f};
    SndMake(SND_MOVE, mv, 1, 28, 0.10f, FALSE); SndMake(SND_OK, ok, 2, 90, 0.16f, FALSE); SndMake(SND_BACK, bk, 2, 90, 0.14f, FALSE);
    SndMake(SND_ERR, er, 2, 160, 0.16f, FALSE); SndMake(SND_PLAY, pl, 4, 700, 0.20f, TRUE); SndMake(SND_BOOT, boot, 5, 1900, 0.22f, TRUE);
}
static void Snd(int k)
{
    if (!PIni(L"ui_sounds", 1) || k < 0 || k > 5) return;
    SoundsInit();
    if (g_snd[k]) PlaySoundW((LPCWSTR)g_snd[k], NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

/* ---------------------------------------------------------------- controllers */
typedef BYTE (SDLCALL_Init)(uint32_t);   /* SDL3's bool: one byte (AL) */
typedef void (SDLCALL_Void)(void);
typedef uint32_t *(SDLCALL_GetGamepads)(int *);
typedef void *(SDLCALL_OpenGamepad)(uint32_t);
typedef void (SDLCALL_CloseGamepad)(void *);
typedef BYTE (SDLCALL_GetButton)(void *, int);
typedef int16_t (SDLCALL_GetAxis)(void *, int);
typedef BYTE (SDLCALL_Connected)(void *);
typedef BYTE (SDLCALL_SetHint)(const char *, const char *);
typedef int (SDLCALL_AddMappings)(const char *);
typedef void (SDLCALL_Free)(void *);
typedef BYTE (SDLCALL_Rumble)(void *, uint16_t, uint16_t, uint32_t);   /* 6.0: SDL_RumbleGamepad (low, high motor, ms) */
typedef const char *(SDLCALL_NameForId)(uint32_t);   /* 6.2: the Controles card (home62.c): name, type, battery, cable */
typedef int (SDLCALL_TypeForId)(uint32_t);
typedef int (SDLCALL_Power)(void *, int *);
typedef uint32_t (SDLCALL_PadId)(void *);
typedef int (SDLCALL_ConnState)(void *);
typedef char *(SDLCALL_MapFor)(uint32_t);   /* 6.2: the Controller screen */
static struct {
    HMODULE dll; BOOL ok, paused; void *pad;
    SDLCALL_Init *Init; SDLCALL_Void *Quit, *Update; SDLCALL_GetGamepads *GetGamepads; SDLCALL_OpenGamepad *Open; SDLCALL_CloseGamepad *Close;
    SDLCALL_GetButton *Button; SDLCALL_GetAxis *Axis; SDLCALL_Connected *Connected; SDLCALL_SetHint *Hint; SDLCALL_AddMappings *Map; SDLCALL_Free *Free;
    SDLCALL_Rumble *Rumble;
    SDLCALL_NameForId *NameForId; SDLCALL_TypeForId *TypeForId; SDLCALL_Power *Power; SDLCALL_PadId *PadId; SDLCALL_ConnState *ConnState; SDLCALL_MapFor *MapFor;   /* 6.2 */
} g_sdl;
typedef struct { WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; } XPad;
typedef struct { DWORD packet; XPad pad; } XState;
typedef DWORD (WINAPI *XGetState)(DWORD, XState *);
typedef struct { WORD left, right; } XVib;
typedef DWORD (WINAPI *XSetState)(DWORD, XVib *);
static XGetState g_xget; static XSetState g_xset; static HMODULE g_xdll;
static DWORD g_padHeld, g_padRepeatAt; static int g_padLastDir = -1; static BOOL g_padSeen;
static BOOL g_padResync;   /* 4.3.2: the next read only marks what is held (set when the controllers come back after a game) */
#define SDL_INIT_GAMEPAD 0x2000u
enum { PB_A, PB_B, PB_X, PB_Y, PB_BACK, PB_GUIDE, PB_START, PB_LS, PB_RS, PB_LB, PB_RB, PB_UP, PB_DOWN, PB_LEFT, PB_RIGHT };
static BOOL PadLoadSdl(void)
{
    if (g_sdl.dll) return g_sdl.ok;
    for (int g = NGAMES - 1; g >= 0 && !g_sdl.dll; g--) { wchar_t d[MAX_PATH], p[MAX_PATH]; PortDir(g, d); Join(p, d, L"bin\\SDL3.dll"); if (FileExists(p)) g_sdl.dll = LoadLibraryExW(p, NULL, LOAD_WITH_ALTERED_SEARCH_PATH); }
    if (!g_sdl.dll) { g_sdl.dll = (HMODULE)1; return FALSE; }
    #define SYM(f, n) g_sdl.f = (void *)GetProcAddress(g_sdl.dll, n)
    SYM(Init, "SDL_Init"); SYM(Quit, "SDL_Quit"); SYM(Update, "SDL_UpdateGamepads"); SYM(GetGamepads, "SDL_GetGamepads"); SYM(Open, "SDL_OpenGamepad");
    SYM(Close, "SDL_CloseGamepad"); SYM(Button, "SDL_GetGamepadButton"); SYM(Axis, "SDL_GetGamepadAxis"); SYM(Connected, "SDL_GamepadConnected");
    SYM(Hint, "SDL_SetHint"); SYM(Map, "SDL_AddGamepadMappingsFromFile"); SYM(Free, "SDL_free"); SYM(Rumble, "SDL_RumbleGamepad");
    SYM(NameForId, "SDL_GetGamepadNameForID"); SYM(TypeForId, "SDL_GetGamepadTypeForID"); SYM(Power, "SDL_GetGamepadPowerInfo");   /* 6.2 (optional) */
    SYM(PadId, "SDL_GetGamepadID"); SYM(ConnState, "SDL_GetGamepadConnectionState"); SYM(MapFor, "SDL_GetGamepadMappingForID");
    #undef SYM
    g_sdl.ok = g_sdl.Init && g_sdl.Quit && g_sdl.Update && g_sdl.GetGamepads && g_sdl.Open && g_sdl.Close && g_sdl.Button && g_sdl.Axis && g_sdl.Connected;
    return g_sdl.ok;
}
static BOOL g_sdlUp;
static void PadStart(void)
{
    if (!PIni(L"pad_nav", 1) || g_sdl.paused) return;
    if (PadLoadSdl() && !g_sdlUp) {
        if (g_sdl.Hint) { g_sdl.Hint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1"); g_sdl.Hint("SDL_JOYSTICK_HIDAPI_PS3", "1"); }
        if (g_sdl.Init(SDL_INIT_GAMEPAD)) {   /* SDL3: true on success */
            g_sdlUp = TRUE;
            if (g_sdl.Map) { wchar_t db[MAX_PATH]; char a[MAX_PATH * 3]; PadDbPath(db); if (FileExists(db)) { WideCharToMultiByte(CP_UTF8, 0, db, -1, a, sizeof(a), NULL, NULL); g_sdl.Map(a); } }
        }
    }
    if (!g_sdlUp && !g_xget) {
        g_xdll = LoadLibraryW(L"xinput1_4.dll"); if (!g_xdll) g_xdll = LoadLibraryW(L"xinput9_1_0.dll");
        if (g_xdll) { g_xget = (XGetState)(void *)GetProcAddress(g_xdll, "XInputGetState"); g_xset = (XSetState)(void *)GetProcAddress(g_xdll, "XInputSetState"); }
    }
}

/* ---------------------------------------------------------------- vibration (6.0)
   The user: "put a vibration feel for each game". As the console's controller does on its menus: when the controller
   moves to another game, that game's own pattern (UFC: two quick jabs; Zelda: a soft swell; God of War III: a heavy blow
   and its echo), and a faint tick on the other moves. Only when the controller itself moved (not the mouse), never
   while a game runs (the controllers are released then), off in Settings > Interface (pini pad_haptics). */
typedef struct { WORD lo, hi, ms; } HapStep;   /* lo/hi = 0: a pause; ms = 0 ends the pattern */
static const HapStep HAP_UFC[] = {{0x4C00, 0x9800, 45}, {0, 0, 65}, {0x6C00, 0xC000, 55}, {0, 0, 0}};
static const HapStep HAP_ZELDA[] = {{0x1200, 0x0500, 70}, {0x2A00, 0x0C00, 90}, {0x1200, 0x0500, 80}, {0, 0, 0}};
static const HapStep HAP_GOW[] = {{0xE400, 0x3400, 120}, {0, 0, 70}, {0x7000, 0x1800, 80}, {0, 0, 0}};
static const HapStep HAP_TICK[] = {{0x0A00, 0x2200, 26}, {0, 0, 0}};
static const HapStep *g_hapSeq; static int g_hapStep; static DWORD g_hapNext, g_padActAt; static BOOL g_xBuzz;
static void HapSet(WORD lo, WORD hi, WORD ms)
{
    if (g_sdlUp && g_sdl.pad && g_sdl.Rumble) { g_sdl.Rumble(g_sdl.pad, lo, hi, ms ? ms + 12 : 0); return; }
    if (g_xset) { XVib v = {lo, hi}; for (DWORD i = 0; i < 4; i++) g_xset(i, &v); g_xBuzz = lo || hi; }
}
static void HapTick(void)   /* every 33 ms (PadPoll), and when a pattern starts */
{
    if (!g_hapSeq) return;
    DWORD now = GetTickCount();
    if (g_hapNext && (int)(now - g_hapNext) < 0) return;
    const HapStep *s = &g_hapSeq[g_hapStep];
    if (!s->ms) { g_hapSeq = NULL; if (g_xBuzz) HapSet(0, 0, 0); return; }
    HapSet(s->lo, s->hi, s->ms); g_hapNext = now + s->ms; if (!g_hapNext) g_hapNext = 1; g_hapStep++;
}
static void PadHaptic(const HapStep *s)
{
    if (!g_padSeen || g_sdl.paused || g_proc || GetTickCount() - g_padActAt > 400 || !PIni(L"pad_haptics", 1)) return;
    g_hapSeq = s; g_hapStep = 0; g_hapNext = 0; HapTick();
}
static void PadHapticGame(int g) { PadHaptic(ZELDAG(g) ? HAP_ZELDA : GOW3G(g) || NGPORTG(g) ? HAP_GOW : HAP_UFC); }   /* 6.1: Bloodborne, inFAMOUS */
static void HapStopAll(void)   /* PadPause: the pattern ends and every motor stops */
{
    g_hapSeq = NULL;
    if (g_sdlUp && g_sdl.pad && g_sdl.Rumble) g_sdl.Rumble(g_sdl.pad, 0, 0, 0);
    if (g_xset && g_xBuzz) { XVib v = {0, 0}; for (DWORD i = 0; i < 4; i++) g_xset(i, &v); g_xBuzz = FALSE; }
}
static void PadPause(BOOL pause)
{
    if (pause && !g_sdl.paused) {
        g_sdl.paused = TRUE;
        HapStopAll();   /* 6.0 final: no vibration left running into the game (XInput motors stay on until told) */
        if (g_sdlUp) { if (g_sdl.pad) { g_sdl.Close(g_sdl.pad); g_sdl.pad = NULL; } g_sdl.Quit(); g_sdlUp = FALSE; }
    } else if (!pause && g_sdl.paused) { g_sdl.paused = FALSE; g_padResync = TRUE; PadStart(); }
}
static DWORD PadRead(void)   /* a bit per PB_* button, the stick folded into the d-pad */
{
    DWORD b = 0;
    if (g_sdlUp) {
        g_sdl.Update();
        if (g_sdl.pad && !g_sdl.Connected(g_sdl.pad)) { g_sdl.Close(g_sdl.pad); g_sdl.pad = NULL; }
        if (!g_sdl.pad) { int n = 0; uint32_t *ids = g_sdl.GetGamepads(&n); if (ids && n > 0) g_sdl.pad = g_sdl.Open(ids[0]); if (ids && g_sdl.Free) g_sdl.Free(ids); }
        if (!g_sdl.pad) return 0;
        static const int map[15] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};   /* SDL_GamepadButton: SOUTH EAST WEST NORTH BACK GUIDE START LSTICK RSTICK LSHOULDER RSHOULDER UP DOWN LEFT RIGHT */
        for (int i = 0; i < 15; i++) if (g_sdl.Button(g_sdl.pad, map[i])) b |= 1u << i;
        int lx = g_sdl.Axis(g_sdl.pad, 0), ly = g_sdl.Axis(g_sdl.pad, 1);
        if (lx < -20000) b |= 1u << PB_LEFT; if (lx > 20000) b |= 1u << PB_RIGHT; if (ly < -20000) b |= 1u << PB_UP; if (ly > 20000) b |= 1u << PB_DOWN;
        g_padSeen = TRUE;
        return b;
    }
    if (g_xget) {
        XState st;
        for (DWORD i = 0; i < 4; i++) if (!g_xget(i, &st)) {
            WORD w = st.pad.buttons; g_padSeen = TRUE;
            if (w & 0x1000) b |= 1u << PB_A; if (w & 0x2000) b |= 1u << PB_B; if (w & 0x4000) b |= 1u << PB_X; if (w & 0x8000) b |= 1u << PB_Y;
            if (w & 0x0020) b |= 1u << PB_BACK; if (w & 0x0010) b |= 1u << PB_START; if (w & 0x0100) b |= 1u << PB_LB; if (w & 0x0200) b |= 1u << PB_RB;
            if (w & 0x0080) b |= 1u << PB_RS;   /* 6.2: R3 / RS */
            if (w & 0x0001) b |= 1u << PB_UP; if (w & 0x0002) b |= 1u << PB_DOWN; if (w & 0x0004) b |= 1u << PB_LEFT; if (w & 0x0008) b |= 1u << PB_RIGHT;
            if (st.pad.lx < -20000) b |= 1u << PB_LEFT; if (st.pad.lx > 20000) b |= 1u << PB_RIGHT; if (st.pad.ly > 20000) b |= 1u << PB_UP; if (st.pad.ly < -20000) b |= 1u << PB_DOWN;
            break;
        }
    }
    return b;
}
static void NavKey(int vk);   /* ui.c */
static BOOL PadMapPoll(void);   /* ui_padmap62.c */
/* every 33 ms (the animation timer): a press, or a held direction repeating after 380 ms every 110 ms */
static void PadPoll(void)
{
    /* 4.1: nothing is read while a game runs (with no SDL3 that was 4 XInput slots 30 times a second next to the game);
       the first read after it marks what is still held, so the button that closed the game does not press PORTHEON's.
       4.3.2: the animation timer stops during a game, so PadPause(FALSE) asks for that first read too (g_padResync) */
    static BOOL wasPlaying;
    if (g_sdl.paused || g_proc) { wasPlaying = TRUE; g_hapSeq = NULL; return; }
    if (wasPlaying || g_padResync) { wasPlaying = g_padResync = FALSE; g_padHeld = PadRead(); return; }
    HapTick();   /* 6.0 */
    if (GetForegroundWindow() != g_wnd) { g_padHeld = PadRead() ? g_padHeld : 0; return; }
    if (PadMapPoll()) return;   /* 6.2: an action waiting for a button */
    DWORD b = PadRead(), down = b & ~g_padHeld, now = GetTickCount();
    static const struct { int pb, vk; } K[] = {{PB_A, VK_RETURN}, {PB_B, VK_ESCAPE}, {PB_Y, VK_APPS}, {PB_START, VK_PLAY}, {PB_LB, VK_PRIOR}, {PB_RB, VK_NEXT}, {PB_BACK, VK_TAB}, {PB_X, VK_F8}, {PB_RS, 'M'}};   /* 6.2: Square / X = the game's logs (home62.c); R3 = the videos' sound (yt.c) */
    for (int i = 0; i < (int)(sizeof(K) / sizeof(K[0])); i++) if (down & (1u << K[i].pb)) { g_padActAt = now; NavKey(K[i].vk); }
    static const int dirs[4] = {PB_UP, PB_DOWN, PB_LEFT, PB_RIGHT}, vks[4] = {VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT};
    for (int i = 0; i < 4; i++) {
        BOOL go = FALSE;
        if (down & (1u << dirs[i])) { go = TRUE; g_padLastDir = i; g_padRepeatAt = now + 380; }
        else if ((b & (1u << dirs[i])) && g_padLastDir == i && (int)(now - g_padRepeatAt) >= 0) { go = TRUE; g_padRepeatAt = now + 110; }
        if (go) {   /* 6.0: a move to another game plays that game's pattern (Select); any other move, a faint tick */
            int was = g_sel; g_padActAt = now; NavKey(vks[i]);
            if (g_sel == was && !g_hapSeq) PadHaptic(HAP_TICK);
        }
    }
    if (!(b & 0x7800)) g_padLastDir = -1;
    g_padHeld = b;
}

/* ---------------------------------------------------------------- 6.2: the controllers connected (the Controles card)
   Read once a second while the home screen shows (home62.c LiveTick), never while a game runs (the 4.1 rule). SDL3: every
   pad SDL sees, its name and type; the battery and the cable only for the one PORTHEON opened (player 1). XInput: the 4
   slots and their batteries. type: 0 generic, 1 Xbox, 2 DualShock 4, 3 DualSense, 4 Switch Pro, 5 PS3. */
typedef struct { int type, batt, wired; wchar_t name[48]; } PadLive;
typedef struct { BYTE type, level; } XBatt;
typedef DWORD (WINAPI *XGetBatt)(DWORD, BYTE, XBatt *);
static int PadList(PadLive *out, int max)
{
    int n = 0;
#ifdef PORTHEON_TEST   /* UI shots: PORTHEON_TEST_PADS=1 shows two controllers */
    if (GetEnvironmentVariableW(L"PORTHEON_TEST_PADS", NULL, 0)) {
        wchar_t tp[8] = L""; GetEnvironmentVariableW(L"PORTHEON_TEST_PADS", tp, 8);
        if (tp[0] == L'x') { out[0] = (PadLive){1, 40, 0, L"Xbox Series X Controller"}; return 1; }
        if (tp[0] == L's') { out[0] = (PadLive){4, 40, 0, L"Switch Pro Controller"}; return 1; }
        out[0] = (PadLive){3, 80, 1, L"DualSense Wireless Controller"}; out[1] = (PadLive){1, 40, 0, L"Xbox Series X Controller"}; return max < 2 ? max : 2;
    }
#endif
    if (g_sdl.paused || g_proc) return 0;
    if (g_sdlUp && g_sdl.GetGamepads) {
        int c = 0; uint32_t *ids = g_sdl.GetGamepads(&c); uint32_t p1 = g_sdl.pad && g_sdl.PadId ? g_sdl.PadId(g_sdl.pad) : 0;
        for (int i = 0; ids && i < c && n < max; i++) {
            PadLive *p = &out[n++]; memset(p, 0, sizeof(*p)); p->batt = -1; p->wired = -1;
            int t = g_sdl.TypeForId ? g_sdl.TypeForId(ids[i]) : 0;   /* SDL_GamepadType: 2/3 Xbox, 4 PS3, 5 PS4, 6 PS5, 7 Switch Pro */
            p->type = t == 2 || t == 3 ? 1 : t == 5 ? 2 : t == 6 ? 3 : t == 7 ? 4 : t == 4 ? 5 : 0;
            const char *nm = g_sdl.NameForId ? g_sdl.NameForId(ids[i]) : NULL;
            if (nm && *nm) MultiByteToWideChar(CP_UTF8, 0, nm, -1, p->name, 48); else wcscpy(p->name, L"Controle");
            p->name[47] = 0;
            if (ids[i] == p1 && g_sdl.pad) {
                int pct = -1, ps = g_sdl.Power ? g_sdl.Power(g_sdl.pad, &pct) : -1;   /* SDL_PowerState: 1 on battery, 2 no battery, 3 charging, 4 charged */
                if (ps == 1 || ps == 3 || ps == 4) p->batt = pct >= 0 ? pct : ps == 4 ? 100 : -1;
                int cs = g_sdl.ConnState ? g_sdl.ConnState(g_sdl.pad) : 0; p->wired = cs == 1 ? 1 : cs == 2 ? 0 : -1;
                if (n > 1) { PadLive t0 = out[0]; out[0] = *p; *p = t0; }   /* player 1 first */
            }
        }
        if (ids && g_sdl.Free) g_sdl.Free(ids);
        return n;
    }
    if (g_xget) {
        static XGetBatt gb; static BOOL tried; if (!tried) { tried = TRUE; if (g_xdll) gb = (XGetBatt)(void *)GetProcAddress(g_xdll, "XInputGetBatteryInformation"); }
        for (DWORD i = 0; i < 4 && n < max; i++) { XState st; if (g_xget(i, &st)) continue;
            PadLive *p = &out[n++]; memset(p, 0, sizeof(*p)); p->type = 1; p->batt = -1; p->wired = -1; wcscpy(p->name, L"Xbox / XInput");
            XBatt b = {0, 0}; if (gb && !gb(i, 0, &b)) { if (b.type == 1) p->wired = 1; else if (b.type == 2 || b.type == 3) { p->wired = 0; p->batt = b.level >= 3 ? 100 : b.level == 2 ? 60 : b.level == 1 ? 20 : 5; } } }   /* 6.2 review: LOW (1) and EMPTY (0) show one red segment, MEDIUM three, FULL four */
    }
    return n;
}
static void PadIdentify(void)   /* the Controles card: player 1's controller buzzes (whatever moved last) */
{
    if (g_sdl.paused || g_proc) return;
    g_padActAt = GetTickCount(); g_padSeen = TRUE;
    static const HapStep id[] = {{0x5000, 0xA000, 160}, {0, 0, 90}, {0x5000, 0xA000, 160}, {0, 0, 0}};
    if (!PIni(L"pad_haptics", 1)) return;
    g_hapSeq = id; g_hapStep = 0; g_hapNext = 0; HapTick();
}
