/* ui_keys.c - part of ui.c (6.0, the user 2026-11-07: "options for the keyboard, for whoever has no controller, to set
 * up the keyboard buttons to play"). The Keyboard screen (OV_KEYS): every PlayStation control of the game with the key
 * that presses it; click a control (or Enter on it), then press a key (or, in UFC 5 and 6, a mouse button).
 * UFC 5 and UFC 6: the layout is written as keymap= in UFC5.ini / UFC6.ini ("Cross=J;Circle=L;..."), and their launchers
 * pass it to the engine as --keymap (the engine's own names, each checked against the UFC 6 engine on 2026-11-07). A
 * layout equal to the engine's own keys is written empty, so the engine keeps its defaults (and their extras).
 * UFC 1-4: the SacredFire engines read "key.<action>=<SDL scancode name>" lines from bbport.ini (BB_CONFIG, set by the
 * port's launcher to %LOCALAPPDATA%\UFCnNative\bbport.ini); each line replaces that action's keys only (the parser at
 * va 0x1400760d4 of games\engines\intel-arc\sacredfire-game.exe: "key." resets the action's key count, then up to 4
 * comma-separated keys). Its 25 actions and default keys come from that engine's tables (va 0x140510c20 / 0x140510d10,
 * _build-tools\scripts\sf_keydefaults.py). PORTHEON rewrites only the key.* lines and keeps every other line. */

typedef struct { const wchar_t *ctl, *pt, *en, *def; } KeyCtl;
/* UFC 5/6: the KytyPS5 engine's default keyboard (hostInput.cpp DefaultKeyboardInput) */
static const KeyCtl KC5[] = {
    {L"Cross", L"X (Xis)", L"Cross (X)", L"J"}, {L"Circle", L"Círculo", L"Circle", L"L"}, {L"Square", L"Quadrado", L"Square", L"K"},
    {L"Triangle", L"Triângulo", L"Triangle", L"I"}, {L"L1", L"L1", L"L1", L"Q"}, {L"R1", L"R1", L"R1", L"E"}, {L"L2", L"L2", L"L2", L"Z"},
    {L"R2", L"R2", L"R2", L"C"}, {L"L3", L"L3 (apertar o analógico esq.)", L"L3 (left stick press)", L"Left Shift"},
    {L"R3", L"R3 (apertar o analógico dir.)", L"R3 (right stick press)", L"Left Ctrl"}, {L"Options", L"Options (pausa)", L"Options (pause)", L"Return"},
    {L"TouchPad", L"Touchpad (câmera de TV)", L"Touchpad (TV camera)", L"Backspace"}, {L"TouchPadRight", L"Touchpad, lado direito", L"Touchpad, right side", L"Tab"},
    {L"Up", L"Direcional: cima", L"D-pad: up", L"Up"}, {L"Down", L"Direcional: baixo", L"D-pad: down", L"Down"},
    {L"Left", L"Direcional: esquerda", L"D-pad: left", L"Left"}, {L"Right", L"Direcional: direita", L"D-pad: right", L"Right"},
    {L"LeftStickUp", L"Analógico esq.: cima", L"Left stick: up", L"W"}, {L"LeftStickDown", L"Analógico esq.: baixo", L"Left stick: down", L"S"},
    {L"LeftStickLeft", L"Analógico esq.: esquerda", L"Left stick: left", L"A"}, {L"LeftStickRight", L"Analógico esq.: direita", L"Left stick: right", L"D"},
    {L"RightStickUp", L"Analógico dir.: cima", L"Right stick: up", L"T"}, {L"RightStickDown", L"Analógico dir.: baixo", L"Right stick: down", L"G"},
    {L"RightStickLeft", L"Analógico dir.: esquerda", L"Right stick: left", L"F"}, {L"RightStickRight", L"Analógico dir.: direita", L"Right stick: right", L"H"},
};
/* UFC 1-4: the SacredFire engine's actions and default keys (its own tables, see above) */
static const KeyCtl KC4[] = {
    {L"cross", L"X (Xis)", L"Cross (X)", L"Space"}, {L"circle", L"Círculo", L"Circle", L"Left Shift"}, {L"square", L"Quadrado", L"Square", L"E"},
    {L"triangle", L"Triângulo", L"Triangle", L"Q"}, {L"l1", L"L1", L"L1", L"1"}, {L"r1", L"R1", L"R1", L"3"}, {L"l2", L"L2", L"L2", L"R"},
    {L"r2", L"R2", L"R2", L"F"}, {L"l3", L"L3 (apertar o analógico esq.)", L"L3 (left stick press)", L"Z"},
    {L"r3", L"R3 (apertar o analógico dir.)", L"R3 (right stick press)", L"C"}, {L"options", L"Options (pausa)", L"Options (pause)", L"Return"},
    {L"touchpad", L"Touchpad (câmeras da luta)", L"Touchpad (fight cameras)", L"Tab"}, {L"touchpad_right", L"Touchpad, lado direito", L"Touchpad, right side", L"Backspace"},
    {L"up", L"Direcional: cima", L"D-pad: up", L"I"}, {L"down", L"Direcional: baixo", L"D-pad: down", L"K"},
    {L"left", L"Direcional: esquerda", L"D-pad: left", L"J"}, {L"right", L"Direcional: direita", L"D-pad: right", L"L"},
    {L"move_up", L"Analógico esq.: cima", L"Left stick: up", L"W"}, {L"move_down", L"Analógico esq.: baixo", L"Left stick: down", L"S"},
    {L"move_left", L"Analógico esq.: esquerda", L"Left stick: left", L"A"}, {L"move_right", L"Analógico esq.: direita", L"Left stick: right", L"D"},
    {L"look_up", L"Analógico dir.: cima", L"Right stick: up", L"Up"}, {L"look_down", L"Analógico dir.: baixo", L"Right stick: down", L"Down"},
    {L"look_left", L"Analógico dir.: esquerda", L"Right stick: left", L"Left"}, {L"look_right", L"Analógico dir.: direita", L"Right stick: right", L"Right"},
};
#define NKC (int)(sizeof(KC5) / sizeof(KC5[0]))   /* both tables have 25 controls, in the same order of kinds */
#define KC_COL1 13   /* the buttons in the left column, the directions in the right one */
static int g_keysGame = -1, g_keyCap = -1;
static wchar_t g_keyVal[NKC][24];
static wchar_t g_keyWarn[240];

static BOOL KeysGameOk(int g) { return g >= 0 && g < NGAMES && (G[g].ps5 || PS4G(g)); }
static const KeyCtl *KCT(int g) { return PS4G(g) ? KC4 : KC5; }
/* UFC 1-4: %LOCALAPPDATA%\UFCnNative\bbport.ini, as UTF-8 text (NULL when the game has no data folder) */
static BOOL BbportPath(int g, wchar_t *out) { if (!PS4G(g) || !G[g].data) return FALSE; wchar_t d[MAX_PATH]; LocalData(d, G[g].data); Join(out, d, L"bbport.ini"); return TRUE; }
static char *ReadText(const wchar_t *p, DWORD *len)
{
    *len = 0; HANDLE f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(f, NULL); if (sz > (1u << 20)) { CloseHandle(f); return NULL; }
    char *b = (char *)malloc(sz + 1); DWORD got = 0; if (!b) { CloseHandle(f); return NULL; }
    ReadFile(f, b, sz, &got, NULL); CloseHandle(f); b[got] = 0; *len = got; return b;
}
static void KeysLoad(int g)
{
    const KeyCtl *kc = KCT(g);
    for (int i = 0; i < NKC; i++) wcscpy(g_keyVal[i], kc[i].def);
    if (PS4G(g)) {   /* the key.* lines of bbport.ini (the first key of each) */
        wchar_t p[MAX_PATH]; DWORD n; char *b = BbportPath(g, p) ? ReadText(p, &n) : NULL; if (!b) return;
        for (char *l = b; l && *l;) {
            char *e = strchr(l, '\n'); if (e) *e = 0;
            char *cr = strchr(l, '\r'); if (cr) *cr = 0;
            if (!strncmp(l, "key.", 4)) {
                char *eq = strchr(l, '=');
                if (eq) { *eq = 0; char *v = eq + 1; while (*v == ' ') v++; char *c = strchr(v, ','); if (c) *c = 0;
                          wchar_t a[32], vv[32]; MultiByteToWideChar(CP_UTF8, 0, l + 4, -1, a, 32); MultiByteToWideChar(CP_UTF8, 0, v, -1, vv, 32);
                          for (size_t k = wcslen(vv); k && vv[k - 1] == L' '; ) vv[--k] = 0;
                          for (int i = 0; i < NKC; i++) if (!_wcsicmp(a, kc[i].ctl) && vv[0]) { wcsncpy(g_keyVal[i], vv, 23); g_keyVal[i][23] = 0; } }
            }
            l = e ? e + 1 : NULL;
        }
        free(b); return;
    }
    wchar_t km[2400]; GetStr(g, L"keymap", km, 2400, L"");
    wchar_t *ctx = NULL;
    for (wchar_t *t = wcstok(km, L";", &ctx); t; t = wcstok(NULL, L";", &ctx)) {
        wchar_t *eq = wcschr(t, L'='); if (!eq) continue; *eq = 0;
        for (int i = 0; i < NKC; i++) if (!_wcsicmp(t, kc[i].ctl)) { wcsncpy(g_keyVal[i], eq + 1, 23); g_keyVal[i][23] = 0; }
    }
}
static BOOL KeysCustom(int g)
{
    if (PS4G(g)) { wchar_t p[MAX_PATH]; DWORD n; char *b = BbportPath(g, p) ? ReadText(p, &n) : NULL; BOOL has = b && (!strncmp(b, "key.", 4) || strstr(b, "\nkey.")); free(b); return has; }
    wchar_t km[16]; GetStr(g, L"keymap", km, 16, L""); return km[0] != 0;
}
static void KeysOpen(int g)
{
    if (!KeysGameOk(g)) return;
    g_keysGame = g; g_keyCap = -1; g_keyWarn[0] = 0; KeysLoad(g);
    g_overlay = OV_KEYS; g_focus = -1; InvalidateRect(g_wnd, NULL, FALSE);
}
#ifdef PORTHEON_TEST
void KeysOpenT(int g) { KeysOpen(g); }   /* the UI shots (ui.c) */
#endif
/* UFC 1-4: bbport.ini without its key.* lines, plus the new ones (none when the layout is the engine's own) */
static BOOL KeysSaveBbport(int g, BOOL same)
{
    wchar_t p[MAX_PATH]; if (!BbportPath(g, p)) return FALSE;
    wchar_t d[MAX_PATH]; wcscpy(d, p); *wcsrchr(d, L'\\') = 0; CreateDirectoryW(d, NULL);
    DWORD n; char *b = ReadText(p, &n);
    size_t cap = (size_t)n + 4096; char *o = (char *)malloc(cap); if (!o) { free(b); return FALSE; }
    size_t k = 0; o[0] = 0;
    if (b) for (char *l = b; l && *l;) {
        char *e = strchr(l, '\n'); size_t len = e ? (size_t)(e - l + 1) : strlen(l);
        if (strncmp(l, "key.", 4)) { memcpy(o + k, l, len); k += len; if (!e) { o[k++] = '\r'; o[k++] = '\n'; } }
        l = e ? e + 1 : NULL;
    }
    if (!same) {
        const KeyCtl *kc = KCT(g);
        for (int i = 0; i < NKC; i++) {
            char a[32], v[48]; WideCharToMultiByte(CP_UTF8, 0, kc[i].ctl, -1, a, 32, NULL, NULL); WideCharToMultiByte(CP_UTF8, 0, g_keyVal[i], -1, v, 48, NULL, NULL);
            k += (size_t)snprintf(o + k, cap - k, "key.%s=%s\r\n", a, v);
        }
    }
    HANDLE f = CreateFileW(p, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    BOOL ok = f != INVALID_HANDLE_VALUE; if (ok) { DWORD w; ok = WriteFile(f, o, (DWORD)k, &w, NULL) && w == k; CloseHandle(f); }
    free(o); free(b); return ok;
}
static void KeysSave(void)
{
    int g = g_keysGame; if (!KeysGameOk(g)) { g_overlay = OV_NONE; return; }
    const KeyCtl *kc = KCT(g);
    BOOL same = TRUE; for (int i = 0; i < NKC; i++) if (_wcsicmp(g_keyVal[i], kc[i].def)) same = FALSE;
    if (PS4G(g)) {
        if (!KeysSaveBbport(g, same)) { Msg(C_BAD, L"%s", L2(L"Não consegui gravar o bbport.ini do jogo (pasta %LOCALAPPDATA%). Feche o jogo e tente de novo.", L"Could not write the game's bbport.ini (the %LOCALAPPDATA% folder). Close the game and try again.")); Snd(SND_ERR); return; }
    } else {
        wchar_t km[2400] = L"";
        if (!same) for (int i = 0; i < NKC; i++) { size_t k = wcslen(km); _snwprintf(km + k, 2400 - k, L"%s%s=%s", k ? L";" : L"", kc[i].ctl, g_keyVal[i]); }
        km[2399] = 0; PutStr(g, L"keymap", km); PutStr(g, L"keymap_bad", L"");
    }
    g_overlay = OV_NONE; g_keyCap = -1; Snd(SND_OK);
    Msg(C_GOOD, same ? L2(L"Teclado do %s: as teclas padrão do jogo. Vale no próximo JOGAR.", L"%s keyboard: the game's default keys. Applies at the next PLAY.")
                     : L2(L"Teclado do %s salvo: vale no próximo JOGAR. O controle continua funcionando junto.", L"%s keyboard saved: it applies at the next PLAY. A controller keeps working too."), G[g].name);
}
/* the SDL name of a key (the engines' names); NULL: not offered (the engines' own keys, signs this layout has no name for) */
static const wchar_t *KeyName(WPARAM vk, LPARAM lp, wchar_t *buf)
{
    BOOL ext = (lp >> 24) & 1; UINT sc = (UINT)((lp >> 16) & 0xff);
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) { buf[0] = (wchar_t)vk; buf[1] = 0; return buf; }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) { _snwprintf(buf, 24, L"Keypad %d", (int)(vk - VK_NUMPAD0)); return buf; }
    switch (vk) {
    case VK_SPACE: return L"Space"; case VK_TAB: return L"Tab"; case VK_BACK: return L"Backspace"; case VK_CAPITAL: return L"CapsLock";
    case VK_RETURN: return ext ? L"Keypad Enter" : L"Return";
    case VK_SHIFT: return sc == 0x36 ? L"Right Shift" : L"Left Shift"; case VK_LSHIFT: return L"Left Shift"; case VK_RSHIFT: return L"Right Shift";
    case VK_CONTROL: return ext ? L"Right Ctrl" : L"Left Ctrl"; case VK_LCONTROL: return L"Left Ctrl"; case VK_RCONTROL: return L"Right Ctrl";
    case VK_MENU: return ext ? L"Right Alt" : L"Left Alt"; case VK_LMENU: return L"Left Alt"; case VK_RMENU: return L"Right Alt";
    case VK_UP: return L"Up"; case VK_DOWN: return L"Down"; case VK_LEFT: return L"Left"; case VK_RIGHT: return L"Right";
    case VK_INSERT: return PS4G(g_keysGame) ? NULL : L"Insert";   /* UFC 1-4: Insert opens the port's own menu */
    case VK_DELETE: return L"Delete"; case VK_HOME: return L"Home"; case VK_END: return L"End";
    case VK_PRIOR: return L"PageUp"; case VK_NEXT: return L"PageDown";
    case VK_ADD: return L"Keypad +"; case VK_SUBTRACT: return L"Keypad -"; case VK_MULTIPLY: return L"Keypad *"; case VK_DIVIDE: return L"Keypad /"; case VK_DECIMAL: return L"Keypad .";
    }
    if (vk >= VK_OEM_1 && vk <= VK_OEM_8) {   /* the sign the key types on this keyboard, when SDL has a name for it */
        UINT ch = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_CHAR) & 0x7fff;
        if (ch && wcschr(L",.-=;/`[]\\'", (wchar_t)ch)) { buf[0] = (wchar_t)ch; buf[1] = 0; return buf; }
    }
    return NULL;
}
/* what the screen shows for a key name */
static void KeyShow(const wchar_t *k, wchar_t *out, int n)
{
    static const struct { const wchar_t *k, *pt, *en; } S[] = {
        {L"Up", L"Seta ↑", L"Arrow ↑"}, {L"Down", L"Seta ↓", L"Arrow ↓"}, {L"Left", L"Seta ←", L"Arrow ←"}, {L"Right", L"Seta →", L"Arrow →"},
        {L"Return", L"Enter", L"Enter"}, {L"Space", L"Espaço", L"Space"}, {L"Left Shift", L"Shift esq.", L"Left Shift"}, {L"Right Shift", L"Shift dir.", L"Right Shift"},
        {L"Left Ctrl", L"Ctrl esq.", L"Left Ctrl"}, {L"Right Ctrl", L"Ctrl dir.", L"Right Ctrl"}, {L"Left Alt", L"Alt esq.", L"Left Alt"}, {L"Right Alt", L"Alt Gr", L"Right Alt"},
        {L"Mouse:Left", L"Mouse esquerdo", L"Left mouse"}, {L"Mouse:Right", L"Mouse direito", L"Right mouse"}, {L"Mouse:Middle", L"Mouse do meio", L"Middle mouse"},
        {L"Mouse:X1", L"Mouse lateral 1", L"Mouse side 1"}, {L"Mouse:X2", L"Mouse lateral 2", L"Mouse side 2"}, {L"Keypad Enter", L"Enter do teclado num.", L"Keypad Enter"}};
    for (int i = 0; i < (int)(sizeof(S) / sizeof(S[0])); i++) if (!wcscmp(k, S[i].k)) { _snwprintf(out, n, L"%s", TR(S[i].pt, S[i].en)); return; }
    if (!wcsncmp(k, L"Keypad ", 7)) { _snwprintf(out, n, L2(L"Num %s", L"Num %s"), k + 7); return; }
    _snwprintf(out, n, L"%s", k);
}
static void KeysAssign(const wchar_t *name)
{
    int i = g_keyCap; if (i < 0 || i >= NKC || !name) return;
    for (int j = 0; j < NKC; j++) if (j != i && !_wcsicmp(g_keyVal[j], name)) { wcscpy(g_keyVal[j], g_keyVal[i]); break; }   /* the key was taken: the two swap */
    wcsncpy(g_keyVal[i], name, 23); g_keyVal[i][23] = 0;
    g_keyCap = -1; g_keyWarn[0] = 0; Snd(SND_OK); InvalidateRect(g_wnd, NULL, FALSE);
}
/* a key while a control waits for one; TRUE: taken */
static BOOL KeysCaptureKey(WPARAM vk, LPARAM lp)
{
    if (g_overlay != OV_KEYS || g_keyCap < 0) return FALSE;
    if (lp & 0x40000000) return TRUE;   /* held */
    if (vk == VK_ESCAPE) { g_keyCap = -1; g_keyWarn[0] = 0; Snd(SND_BACK); InvalidateRect(g_wnd, NULL, FALSE); return TRUE; }
    wchar_t b[24]; const wchar_t *nm = KeyName(vk, lp, b);
    if (!nm) {
        _snwprintf(g_keyWarn, 240, L"%s", vk >= VK_F1 && vk <= VK_F24 ? L2(L"As teclas F1 a F12 ficam com o próprio jogo (tela cheia, menus do motor): escolha outra tecla.", L"F1 to F12 belong to the game itself (full screen, the engine's menus): pick another key.")
                                 : vk == VK_INSERT ? L2(L"O Insert abre o menu do port no UFC 1 a 4: escolha outra tecla.", L"Insert opens the port's menu in UFC 1 to 4: pick another key.")
                                 : L2(L"Essa tecla não pode ser usada no jogo: escolha outra (letras, números, setas, Shift, Ctrl, Espaço, Enter ou o teclado numérico).", L"That key cannot be used in the game: pick another (letters, digits, arrows, Shift, Ctrl, Space, Enter or the keypad)."));
        Snd(SND_ERR); InvalidateRect(g_wnd, NULL, FALSE); return TRUE;
    }
    KeysAssign(nm); return TRUE;
}
static BOOL KeysCaptureMouse(const wchar_t *name)
{
    if (g_overlay != OV_KEYS || g_keyCap < 0) return FALSE;
    if (PS4G(g_keysGame)) {   /* the SacredFire engines read keys only */
        _snwprintf(g_keyWarn, 240, L"%s", L2(L"No UFC 1 a 4 o mouse não vira botão do controle: escolha uma tecla do teclado.", L"In UFC 1 to 4 the mouse cannot be a controller button: pick a keyboard key."));
        Snd(SND_ERR); InvalidateRect(g_wnd, NULL, FALSE); return TRUE;
    }
    KeysAssign(name); return TRUE;
}
static BOOL g_keysDraw;   /* 6.2: the drawn keyboard instead of the list */
static void KeysClick(int h)
{
    if (h >= H_KEYROW && h < H_KEYROW + NKC) {
        int i = h - H_KEYROW;
        if (g_keyCap == i) { if (!KeysCaptureMouse(L"Mouse:Left")) g_keyCap = -1; }   /* a second click on the waiting control: the left mouse button */
        else { g_keyCap = i; g_keyWarn[0] = 0; Snd(SND_MOVE); }
    } else if (h == H_KEYRESET) { const KeyCtl *kc = KCT(g_keysGame); for (int i = 0; i < NKC; i++) wcscpy(g_keyVal[i], kc[i].def); g_keyCap = -1; Snd(SND_MOVE); }
    else if (h == H_KEYSAVE) KeysSave();
    else if (h == 1003) { g_keysDraw = !g_keysDraw; g_keyCap = -1; Snd(SND_MOVE); }
    else if (h == H_KEYCANCEL) { g_overlay = OV_NONE; g_keyCap = -1; Snd(SND_BACK); }
    else g_keyCap = -1;
    InvalidateRect(g_wnd, NULL, FALSE);
}
/* 6.2 (the user: "with a drawing of the keyboard"): the same keys as the list, on a drawn keyboard */
static const struct { const wchar_t *k; int w; } KB_ROW[5][15] = {   /* w: quarters of a key */
    {{L"`", 4}, {L"1", 4}, {L"2", 4}, {L"3", 4}, {L"4", 4}, {L"5", 4}, {L"6", 4}, {L"7", 4}, {L"8", 4}, {L"9", 4}, {L"0", 4}, {L"-", 4}, {L"=", 4}, {L"Backspace", 8}, {NULL, 0}},
    {{L"Tab", 6}, {L"Q", 4}, {L"W", 4}, {L"E", 4}, {L"R", 4}, {L"T", 4}, {L"Y", 4}, {L"U", 4}, {L"I", 4}, {L"O", 4}, {L"P", 4}, {L"[", 4}, {L"]", 4}, {L"\\", 6}, {NULL, 0}},
    {{L"CapsLock", 7}, {L"A", 4}, {L"S", 4}, {L"D", 4}, {L"F", 4}, {L"G", 4}, {L"H", 4}, {L"J", 4}, {L"K", 4}, {L"L", 4}, {L";", 4}, {L"'", 4}, {L"Return", 9}, {NULL, 0}},
    {{L"Left Shift", 9}, {L"Z", 4}, {L"X", 4}, {L"C", 4}, {L"V", 4}, {L"B", 4}, {L"N", 4}, {L"M", 4}, {L",", 4}, {L".", 4}, {L"/", 4}, {L"Right Shift", 11}, {NULL, 0}},
    {{L"Left Ctrl", 6}, {L"Left Alt", 6}, {L"Space", 28}, {L"Right Alt", 6}, {L"Right Ctrl", 6}, {NULL, 0}}};
static BOOL g_keysDraw;
static void KbKey(HDC dc, GpGraphics *g, const KeyCtl *kc, const wchar_t *name, RECT r)
{
    int who = -1; for (int i = 0; i < NKC && who < 0; i++) if (!_wcsicmp(g_keyVal[i], name)) who = i;
    BOOL on = who >= 0;
    Box(g, r, SC(6), on ? A(RGB(255, 255, 255), 232) : A(RGB(255, 255, 255), 14), A(RGB(255, 255, 255), on ? 0 : 40), 1.0f);
    wchar_t cap[32]; KeyShow(name, cap, 32);
    RECT t = {r.left + SC(4), r.top + SC(2), r.right - SC(3), r.top + SC(21)};
    TextR(dc, g_fSmall, on ? RGB(20, 20, 26) : C_SUB, t, cap, DT_LEFT | DT_TOP | DT_END_ELLIPSIS);
    if (on) {
        wchar_t a[32]; const wchar_t *c = kc[who].ctl;
        if (!wcsncmp(c, L"LeftStick", 9)) _snwprintf(a, 32, L"LS %s", c + 9); else if (!wcsncmp(c, L"RightStick", 10)) _snwprintf(a, 32, L"RS %s", c + 10); else if (!wcsncmp(c, L"TouchPad", 8)) wcscpy(a, L"Touch"); else _snwprintf(a, 32, L"%s", c);
        a[31] = 0;
        RECT b = {r.left + SC(4), r.bottom - SC(22), r.right - SC(3), r.bottom - SC(3)};
        TextR(dc, g_fTag, RGB(10, 10, 14), b, a, DT_LEFT | DT_BOTTOM | DT_END_ELLIPSIS);
    }
}
static void PaintKeyboard(HDC dc, GpGraphics *g, RECT c, const KeyCtl *kc)
{
    int x0 = c.left + SC(34), y0 = c.top + SC(128), W = c.right - c.left - SC(68), u4 = W / 74, gap = SC(4), rh = u4 * 4;   /* 60 quarters + the arrow cluster */
    for (int row = 0; row < 5; row++) {
        int x = x0;
        for (int i = 0; KB_ROW[row][i].k; i++) {
            RECT r = {x, y0 + row * (rh + gap), x + KB_ROW[row][i].w * u4 - gap, y0 + row * (rh + gap) + rh};
            KbKey(dc, g, kc, KB_ROW[row][i].k, r); x += KB_ROW[row][i].w * u4;
        }
    }
    int ax = x0 + 62 * u4;
    RECT up = {ax + 4 * u4, y0 + 3 * (rh + gap), ax + 8 * u4 - gap, y0 + 3 * (rh + gap) + rh}; KbKey(dc, g, kc, L"Up", up);
    static const wchar_t *const bottom[3] = {L"Left", L"Down", L"Right"};
    for (int i = 0; i < 3; i++) { RECT r = {ax + i * 4 * u4, y0 + 4 * (rh + gap), ax + (i + 1) * 4 * u4 - gap, y0 + 4 * (rh + gap) + rh}; KbKey(dc, g, kc, bottom[i], r); }
}
static void PaintKeys(HDC dc, GpGraphics *g, RECT rc)
{
    Dim(g, rc);
    RECT c = Card(g, 120, 40, 1040, 720);
    int gk = g_keysGame >= 0 ? g_keysGame : g_sel; const KeyCtl *kc = KCT(gk);
    wchar_t t[160]; _snwprintf(t, 160, L2(L"Teclado do %s", L"%s keyboard"), G[gk].name);
    Text(dc, g_fH2, RGB(255, 255, 255), c.left + SC(34), c.top + SC(22), t);
    RECT st = {c.left + SC(34), c.top + SC(62), c.right - SC(80), c.top + SC(102)};
    Wrap(dc, g_fSmall, C_SUB, st, PS4G(gk) ? L2(L"Para jogar sem controle: clique numa ação e aperte a tecla que você quer. Se a tecla já era de outra ação, as duas trocam. O controle continua funcionando junto, e o Insert abre o menu do port.",
                                                L"To play without a controller: click an action and press the key you want. If the key belonged to another action, the two swap. A controller keeps working too, and Insert opens the port's menu.")
                                           : L2(L"Para jogar sem controle: clique numa ação e aperte a tecla que você quer (ou um botão do mouse; clique de novo na mesma ação para o botão esquerdo). Se a tecla já era de outra ação, as duas trocam. O controle continua funcionando junto.",
                                                L"To play without a controller: click an action and press the key you want (or a mouse button; click the same action again for the left button). If the key belonged to another action, the two swap. A controller keeps working too."), TRUE);
    CloseX(g, c, H_KEYCANCEL);
    int colW = (c.right - c.left - SC(34 * 2) - SC(40)) / 2, rowH = SC(31);
    if (g_keysDraw) PaintKeyboard(dc, g, c, kc);
    else for (int col = 0; col < 2; col++) {
        int x = c.left + SC(34) + col * (colW + SC(40)), y = c.top + SC(112);
        Spaced(dc, g_fTag, C_CYAN, x, y, col == 0 ? L2(L"BOTÕES", L"BUTTONS") : L2(L"DIRECIONAL E ANALÓGICOS", L"D-PAD AND STICKS"), 2);
        y += SC(24);
        for (int i = col == 0 ? 0 : KC_COL1; i < (col == 0 ? KC_COL1 : NKC); i++) {
            RECT r = {x, y, x + colW, y + rowH - SC(4)}; BOOL hot = g_hot == H_KEYROW + i || g_focus == H_KEYROW + i, cap = g_keyCap == i;
            if (cap || hot) Box(g, r, (r.bottom - r.top) / 2, A(RGB(255, 255, 255), cap ? 26 : 14), cap ? A(C_CYAN, 220) : 0, cap ? 1.4f : 0);
            RECT lr = {r.left + SC(12), r.top, r.right - SC(170), r.bottom};
            TextR(dc, g_fText, cap || hot ? RGB(255, 255, 255) : C_TEXT, lr, TR(kc[i].pt, kc[i].en), DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
            RECT kr = {r.right - SC(160), r.top + SC(3), r.right - SC(6), r.bottom - SC(3)};
            BOOL changed = _wcsicmp(g_keyVal[i], kc[i].def) != 0;
            Box(g, kr, (kr.bottom - kr.top) / 2, A(cap ? C_CYAN : RGB(255, 255, 255), cap ? (BYTE)(60 + 80 * Pulse(900)) : changed ? 34 : 18), A(changed ? C_CYAN : C_LINE, 255), 1.0f);
            wchar_t kn[48]; if (cap) _snwprintf(kn, 48, L"%s", L2(L"aperte uma tecla...", L"press a key...")); else KeyShow(g_keyVal[i], kn, 48);
            TextR(dc, g_fBtn, RGB(255, 255, 255), kr, kn, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
            AddHit(r.left, r.top, r.right, r.bottom, H_KEYROW + i);
            y += rowH;
        }
    }
    int fy = c.bottom - SC(118);
    RECT nr = {c.left + SC(34), fy, c.right - SC(34), fy + SC(40)};
    Wrap(dc, g_fSmall, g_keyWarn[0] ? C_ORANGE : C_DIM, nr, g_keyWarn[0] ? g_keyWarn : PS4G(gk) ? L2(L"Esc cancela a escolha. Esc, Insert e F1 a F12 ficam com o jogo e o port. Vale no próximo JOGAR.", L"Esc cancels a choice. Esc, Insert and F1 to F12 belong to the game and the port. Applies at the next PLAY.")
                                                                                                 : L2(L"Esc cancela a escolha. Esc e F1 a F12 ficam com o próprio jogo. Vale no próximo JOGAR.", L"Esc cancels a choice. Esc and F1 to F12 belong to the game itself. Applies at the next PLAY."), TRUE);
    RECT sv = {c.right - SC(34) - SC(190), c.bottom - SC(70), c.right - SC(34), c.bottom - SC(24)}; Button(dc, g, sv, L2(L"SALVAR", L"SAVE"), H_KEYSAVE, 1);
    RECT cn = {sv.left - SC(12) - SC(150), sv.top, sv.left - SC(12), sv.bottom}; Button(dc, g, cn, L2(L"Cancelar", L"Cancel"), H_KEYCANCEL, 2);
    RECT df = {c.left + SC(34), sv.top, c.left + SC(34) + SC(260), sv.bottom}; Button(dc, g, df, L2(L"Teclas padrão do jogo", L"The game's default keys"), H_KEYRESET, 0);
    RECT dw = {df.right + SC(12), sv.top, df.right + SC(12) + SC(190), sv.bottom}; Button(dc, g, dw, g_keysDraw ? L2(L"Ver a lista", L"Show the list") : L2(L"Ver o teclado", L"Show the keyboard"), 1003, 0);
}
