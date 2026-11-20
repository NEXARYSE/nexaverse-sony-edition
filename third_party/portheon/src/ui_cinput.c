/* ui_cinput.c - part of ui.c (6.2, the user 2026-11-09: "the keyboards too, for every game they add, with a drawing"):
 * the keyboard of a game from the player's library. Click a key on the drawing, press the key it should become: saved as
 * keymap_c<id> in portheon.ini ("from:to" virtual keys). While that game runs and is in front, a low-level keyboard hook
 * of PORTHEON's turns each key into the chosen one (SendInput with scan codes, so DirectInput and raw-input games get it
 * too). Nothing is written into the game; the hook ends with the game (g_crun). */
#define OV_CKEYS 63
enum { H_CKEY = 1100 /* 96 */, H_CKSAVE = 1196, H_CKCLEAR, H_CKCANCEL };
static int g_ckId, g_ckSel; static BYTE g_ckMap[256]; static wchar_t g_ckName[96]; static UINT g_ckVkOf[96];

static UINT CkVk(const wchar_t *n)
{
    if (!n[1]) {
        wchar_t c = towupper(n[0]); if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) return c;
        switch (n[0]) { case L'`': return VK_OEM_3; case L'-': return VK_OEM_MINUS; case L'=': return VK_OEM_PLUS; case L'[': return VK_OEM_4; case L']': return VK_OEM_6; case L'\\': return VK_OEM_5;
                        case L';': return VK_OEM_1; case L'\'': return VK_OEM_7; case L',': return VK_OEM_COMMA; case L'.': return VK_OEM_PERIOD; case L'/': return VK_OEM_2; }
        return 0;
    }
    static const struct { const wchar_t *n; UINT vk; } T[] = {{L"Backspace", VK_BACK}, {L"Tab", VK_TAB}, {L"CapsLock", VK_CAPITAL}, {L"Return", VK_RETURN}, {L"Left Shift", VK_LSHIFT}, {L"Right Shift", VK_RSHIFT},
        {L"Left Ctrl", VK_LCONTROL}, {L"Right Ctrl", VK_RCONTROL}, {L"Left Alt", VK_LMENU}, {L"Right Alt", VK_RMENU}, {L"Space", VK_SPACE}, {L"Up", VK_UP}, {L"Down", VK_DOWN}, {L"Left", VK_LEFT}, {L"Right", VK_RIGHT}};
    for (int i = 0; i < (int)(sizeof(T) / sizeof(T[0])); i++) if (!wcscmp(n, T[i].n)) return T[i].vk;
    return 0;
}
/* what a key is called on this PC's keyboard (an ABNT2 keyboard shows Ç) */
static void CkVkName(UINT vk, wchar_t *o, int n)
{
    if (vk >= VK_LEFT && vk <= VK_DOWN) { static const wchar_t *const ar[4] = {L"←", L"↑", L"→", L"↓"}; _snwprintf(o, n, L"%s", ar[vk - VK_LEFT]); return; }
    UINT ch = MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR) & 0x7fff;
    if (ch > 32 && !(vk >= VK_NUMPAD0 && vk <= VK_DIVIDE)) { o[0] = towupper((wchar_t)ch); o[1] = 0; return; }
    static const struct { UINT vk; const wchar_t *k; } T[] = {{VK_BACK, L"Backspace"}, {VK_TAB, L"Tab"}, {VK_CAPITAL, L"CapsLock"}, {VK_RETURN, L"Return"}, {VK_LSHIFT, L"Left Shift"}, {VK_RSHIFT, L"Right Shift"},
        {VK_LCONTROL, L"Left Ctrl"}, {VK_RCONTROL, L"Right Ctrl"}, {VK_LMENU, L"Left Alt"}, {VK_RMENU, L"Right Alt"}, {VK_SPACE, L"Space"}, {VK_UP, L"Up"}, {VK_DOWN, L"Down"}, {VK_LEFT, L"Left"}, {VK_RIGHT, L"Right"}};
    for (int i = 0; i < (int)(sizeof(T) / sizeof(T[0])); i++) if (T[i].vk == vk) { KeyShow(T[i].k, o, n); return; }
    if (vk >= VK_F1 && vk <= VK_F24) { _snwprintf(o, n, L"F%u", vk - VK_F1 + 1); return; }
    UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC); o[0] = 0; if (sc) GetKeyNameTextW((LONG)(sc << 16), o, n);
    if (!o[0]) _snwprintf(o, n, L"#%u", vk);
}
static void CkKeyName(int id, wchar_t *k, int n) { _snwprintf(k, n, L"keymap_c%d", id); k[n - 1] = 0; }
static void CkLoad(int id, BYTE *map)
{
    memset(map, 0, 256); wchar_t k[32], v[1600]; CkKeyName(id, k, 32); GetPrivateProfileStringW(L"PORTHEON", k, L"", v, 1600, g_pIni);
    for (wchar_t *s = v; *s; ) { int a = _wtoi(s); while (*s && *s != L':' && *s != L',') s++; int b = 0; if (*s == L':') { s++; b = _wtoi(s); while (*s && *s != L',') s++; } if (*s) s++;
        if (a > 0 && a < 256 && b > 0 && b < 256) map[a] = (BYTE)b; }
}
static void CkSave(void)
{
    wchar_t k[32], v[1600] = L""; size_t l = 0; CkKeyName(g_ckId, k, 32);
    for (int i = 1; i < 256 && l < 1580; i++) if (g_ckMap[i]) l += (size_t)_snwprintf(v + l, 1600 - l, L"%s%d:%d", l ? L"," : L"", i, g_ckMap[i]);
    WritePrivateProfileStringW(L"PORTHEON", k, v[0] ? v : NULL, g_pIni);
    g_overlay = OV_NONE; g_ckSel = 0; Snd(SND_OK);
    Msg(C_GOOD, v[0] ? L2(L"Teclado do %s salvo: vale na próxima vez que ele abrir.", L"%s keyboard saved: it applies the next time it starts.") : L2(L"Teclado do %s: as teclas normais.", L"%s keyboard: the normal keys."), g_ckName);
}
static void CkOpen(int id, const wchar_t *name)
{
    g_ckId = id; g_ckSel = 0; wcsncpy(g_ckName, name, 95); g_ckName[95] = 0; CkLoad(id, g_ckMap);
    g_overlay = OV_CKEYS; g_focus = -1; Snd(SND_OK); InvalidateRect(g_wnd, NULL, FALSE);
}
#ifdef PORTHEON_TEST
void CkOpenT(void) { if (g_ncg > 0) { CkOpen(g_cg[0].id, g_cg[0].name); g_ckMap['W'] = VK_UP; g_ckMap['S'] = VK_DOWN; g_ckMap['A'] = VK_LEFT; g_ckMap['D'] = VK_RIGHT; g_ckMap[VK_SPACE] = VK_RETURN; g_ckSel = 'Q'; } }
#endif
/* a key while one is chosen on the drawing; TRUE: taken */
static BOOL CkCaptureKey(WPARAM vk, LPARAM lp)
{
    if (g_overlay != OV_CKEYS || !g_ckSel) return FALSE;
    if (lp & 0x40000000) return TRUE;
    if (vk == VK_ESCAPE) { g_ckSel = 0; Snd(SND_BACK); InvalidateRect(g_wnd, NULL, FALSE); return TRUE; }
    UINT v = (UINT)vk;
    if (v == VK_SHIFT) v = ((lp >> 16) & 0xff) == 0x36 ? VK_RSHIFT : VK_LSHIFT;
    else if (v == VK_CONTROL) v = (lp & (1 << 24)) ? VK_RCONTROL : VK_LCONTROL;
    else if (v == VK_MENU) v = (lp & (1 << 24)) ? VK_RMENU : VK_LMENU;
    g_ckMap[g_ckSel] = v == (UINT)g_ckSel ? 0 : (BYTE)v; g_ckSel = 0; Snd(SND_OK); InvalidateRect(g_wnd, NULL, FALSE);
    return TRUE;
}
static int g_ckCount;
static void CkKey(HDC dc, GpGraphics *g, const wchar_t *name, RECT r)
{
    UINT vk = CkVk(name); int id = H_CKEY + g_ckCount; if (g_ckCount >= 96 || !vk) return;
    g_ckVkOf[g_ckCount++] = vk;
    BOOL sel = g_ckSel == (int)vk, hot = g_hot == id, map = g_ckMap[vk] != 0;
    Box(g, r, SC(6), sel ? A(RGB(255, 255, 255), (BYTE)(140 + 100 * Pulse(800))) : map ? A(RGB(255, 255, 255), 232) : A(RGB(255, 255, 255), hot ? 44 : 14), A(RGB(255, 255, 255), hot ? 200 : map || sel ? 0 : 40), 1.0f);
    wchar_t cap[32]; CkVkName(vk, cap, 32);
    RECT t = {r.left + SC(4), r.top + SC(2), r.right - SC(3), r.top + SC(21)};
    TextR(dc, g_fSmall, map || sel ? RGB(20, 20, 26) : C_SUB, t, cap, DT_LEFT | DT_TOP | DT_END_ELLIPSIS);
    if (map) { wchar_t to[32], a[40]; CkVkName(g_ckMap[vk], to, 32); _snwprintf(a, 40, L"→ %s", to); a[39] = 0;
        RECT b = {r.left + SC(4), r.bottom - SC(22), r.right - SC(3), r.bottom - SC(3)}; TextR(dc, g_fTag, RGB(10, 10, 14), b, a, DT_LEFT | DT_BOTTOM | DT_END_ELLIPSIS); }
    AddHit(r.left, r.top, r.right, r.bottom, id);
}
static void PaintCKeys(HDC dc, GpGraphics *g, RECT rc)
{
    Dim(g, rc);
    RECT c = Card(g, 120, 130, 1040, 520);
    wchar_t t[160]; _snwprintf(t, 160, L2(L"Teclado: %s", L"Keyboard: %s"), g_ckName); t[159] = 0;
    RECT tr = {c.left + SC(34), c.top + SC(20), c.right - SC(90), c.top + SC(62)}; TextR(dc, g_fH2, RGB(255, 255, 255), tr, t, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    CloseX(g, c, H_CKCANCEL);
    RECT nr = {c.left + SC(34), c.top + SC(66), c.right - SC(34), c.top + SC(120)};
    Wrap(dc, g_fSmall, g_ckSel ? C_TEXT : C_SUB, nr, g_ckSel ? L2(L"Agora aperte a tecla que essa deve virar. Aperte a mesma tecla para desfazer. Esc cancela.", L"Now press the key this one should become. Press the same key to undo. Esc cancels.")
                                                  : L2(L"Clique numa tecla do desenho e aperte a tecla que ela deve virar. Vale com o PORTHEON aberto e o jogo na frente. Em jogo online com anti-cheat, deixe sem trocas.", L"Click a key on the drawing and press the key it should become. It works with PORTHEON open and the game in front. In online games with anti-cheat, leave it unchanged."), TRUE);
    int x0 = c.left + SC(34), y0 = c.top + SC(128), W = c.right - c.left - SC(68), u4 = W / 74, gap = SC(4), rh = u4 * 4;
    g_ckCount = 0;
    for (int row = 0; row < 5; row++) {
        int x = x0;
        for (int i = 0; KB_ROW[row][i].k; i++) {
            RECT r = {x, y0 + row * (rh + gap), x + KB_ROW[row][i].w * u4 - gap, y0 + row * (rh + gap) + rh};
            CkKey(dc, g, KB_ROW[row][i].k, r); x += KB_ROW[row][i].w * u4;
        }
    }
    int ax = x0 + 62 * u4;
    RECT up = {ax + 4 * u4, y0 + 3 * (rh + gap), ax + 8 * u4 - gap, y0 + 3 * (rh + gap) + rh}; CkKey(dc, g, L"Up", up);
    static const wchar_t *const bottom[3] = {L"Left", L"Down", L"Right"};
    for (int i = 0; i < 3; i++) { RECT r = {ax + i * 4 * u4, y0 + 4 * (rh + gap), ax + (i + 1) * 4 * u4 - gap, y0 + 4 * (rh + gap) + rh}; CkKey(dc, g, bottom[i], r); }
    int by = c.bottom - SC(62);
    RECT sv = {c.right - SC(34) - SC(180), by, c.right - SC(34), by + SC(44)}; Button(dc, g, sv, L2(L"SALVAR", L"SAVE"), H_CKSAVE, 1);
    RECT cl = {sv.left - SC(12) - SC(210), by, sv.left - SC(12), by + SC(44)}; Button(dc, g, cl, L2(L"Limpar tudo", L"Clear all"), H_CKCLEAR, 0);
}
static void CkClick(int h)
{
    if (h >= H_CKEY && h < H_CKEY + g_ckCount) { UINT vk = g_ckVkOf[h - H_CKEY]; g_ckSel = g_ckSel == (int)vk ? 0 : (int)vk; Snd(SND_MOVE); }
    else if (h == H_CKSAVE) CkSave();
    else if (h == H_CKCLEAR) { memset(g_ckMap, 0, sizeof(g_ckMap)); g_ckSel = 0; Snd(SND_MOVE); }
    else if (h == H_CKCANCEL) { g_overlay = OV_NONE; g_ckSel = 0; Snd(SND_BACK); }
    else g_ckSel = 0;
    InvalidateRect(g_wnd, NULL, FALSE);
}

/* ---- while the game runs */
#define CK_MARK 0x50544851u
static BYTE g_ckLive[256]; static wchar_t g_ckExe[MAX_PATH], g_ckDir[MAX_PATH]; static int g_ckRunId; static HWND g_ckLastW; static BOOL g_ckLastOk;
static BOOL CkFront(void)
{
    HWND w = GetForegroundWindow(); if (!w) return FALSE;
    if (w == g_ckLastW) return g_ckLastOk;
    g_ckLastW = w; g_ckLastOk = FALSE;
    DWORD pid = 0; GetWindowThreadProcessId(w, &pid); if (!pid || pid == GetCurrentProcessId()) return FALSE;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); if (!h) return FALSE;
    wchar_t img[MAX_PATH * 2]; DWORD n = MAX_PATH * 2;
    if (QueryFullProcessImageNameW(h, 0, img, &n)) {
        size_t dl = wcslen(g_ckDir);
        g_ckLastOk = (g_ckExe[0] && !_wcsicmp(img, g_ckExe)) || (dl && !_wcsnicmp(img, g_ckDir, dl) && img[dl] == L'\\');
    }
    CloseHandle(h); return g_ckLastOk;
}
static DWORD g_ckThread;
static void CkSend(UINT to, BOOL up)
{
    INPUT in; memset(&in, 0, sizeof(in)); in.type = INPUT_KEYBOARD;
    in.ki.wScan = (WORD)MapVirtualKeyW(to, MAPVK_VK_TO_VSC); in.ki.dwExtraInfo = CK_MARK;   /* only its own keys are left alone: Steam Input's and a remote pad's keys are remapped too */
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (up ? KEYEVENTF_KEYUP : 0);
    if (to == VK_UP || to == VK_DOWN || to == VK_LEFT || to == VK_RIGHT || to == VK_RCONTROL || to == VK_RMENU || to == VK_INSERT || to == VK_DELETE || to == VK_HOME || to == VK_END || to == VK_PRIOR || to == VK_NEXT || to == VK_DIVIDE) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    SendInput(1, &in, sizeof(in));
}
static DWORD WINAPI CkSender(LPVOID ready)
{
    MSG m; PeekMessageW(&m, NULL, 0, 0, PM_NOREMOVE); SetEvent((HANDLE)ready);   /* its queue exists now */
    while (GetMessageW(&m, NULL, 0, 0) > 0) if (m.message == WM_APP + 77) CkSend((UINT)m.wParam, (BOOL)m.lParam);
    return 0;
}
static LRESULT CALLBACK CkHook(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION) {
        const KBDLLHOOKSTRUCT *k = (const KBDLLHOOKSTRUCT *)lp;
        if (k->dwExtraInfo != CK_MARK && k->vkCode < 256 && g_ckLive[k->vkCode] && CkFront()) {
            PostThreadMessageW(g_ckThread, WM_APP + 77, g_ckLive[k->vkCode], wp == WM_KEYUP || wp == WM_SYSKEYUP);   /* sent by another thread: this one must stay free to answer the hook for the key it sends */
            return 1;
        }
    }
    return CallNextHookEx(NULL, code, wp, lp);
}
static DWORD WINAPI CkThread(LPVOID unused)
{
    (void)unused;
    HHOOK hk = SetWindowsHookExW(WH_KEYBOARD_LL, CkHook, GetModuleHandleW(NULL), 0); if (!hk) return 0;
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL), sender = CreateThread(NULL, 0, CkSender, ev, 0, &g_ckThread);
    if (!sender) { UnhookWindowsHookEx(hk); if (ev) CloseHandle(ev); return 0; }
    WaitForSingleObject(ev, 2000); CloseHandle(ev);
    UINT_PTR t = SetTimer(NULL, 0, 1000, NULL); MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) { if (m.message == WM_TIMER && *(volatile int *)&g_crun != g_ckRunId) break; DispatchMessageW(&m); }
    KillTimer(NULL, t); UnhookWindowsHookEx(hk); PostThreadMessageW(g_ckThread, WM_QUIT, 0, 0); WaitForSingleObject(sender, 1000); CloseHandle(sender); return 0;
}
static void CkStart(const CGame *c)   /* LibPlay, once g_crun is set */
{
    BYTE map[256]; CkLoad(c->id, map); BOOL any = FALSE; for (int i = 0; i < 256; i++) if (map[i]) any = TRUE;
    if (!any) return;
    memcpy(g_ckLive, map, 256); g_ckRunId = c->id; g_ckLastW = NULL; g_ckLastOk = FALSE; g_ckExe[0] = g_ckDir[0] = 0;
    if (c->kind == CK_EXE || c->kind == CK_EMU) wcsncpy(g_ckExe, c->target, MAX_PATH - 1);
    if (c->watchDir[0] && LibWatchDirOk(c->watchDir)) wcsncpy(g_ckDir, c->watchDir, MAX_PATH - 1);
    HANDLE th = CreateThread(NULL, 0, CkThread, NULL, 0, NULL); if (th) CloseHandle(th);
}
