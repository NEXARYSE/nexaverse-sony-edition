/* ui_input.c - part of ui.c (4.0): the paint of the whole window, and what the mouse, the keyboard and the
 * controller do. Focus: every hit region is a place to go; the arrows move to the nearest one that way (NavKey). */

/* ---------------------------------------------------------------- the whole window
   Drawn into a back buffer that stays between paints; a paint redraws (and copies) only the rectangle Windows asks for */
static HBITMAP g_back; static int g_backW, g_backH;
static void PaintFocusRing(GpGraphics *g)
{
    RECT r; if (!g_kbNav || g_focus < 0 || !HitRect(g_focus, &r)) return;
    /* 7.0: the console's focus: a white line 3 units outside what the keyboard or the controller is on (a pill for the
       buttons and rows, round corners for the cards and the video), with a faint dark line around it so it reads on any
       art; still: nothing pulses. The chosen tile on the home screen already has its own white line (PaintHome). */
    if (g_view == V_HOME && !g_overlay && g_focus == (g_csel >= 0 ? H_CUSTOM + g_csel : H_CARD + g_sel)) return;
    int h = r.bottom - r.top; InflateRect(&r, SC(3), SC(3));
    int rad = h <= SC(64) ? (r.bottom - r.top) / 2 : SC(20);
    RECT o = r; InflateRect(&o, SC(2), SC(2)); Box(g, o, rad + SC(2), 0, A(RGB0(0, 0, 0), 96), 1.5f);
    Box(g, r, rad, 0, A(RGB0(255, 255, 255), 255), (float)SC(2));
}

static void PaintInner(HDC wdc, const RECT *want, HRGN rgn);
static void Paint(HDC wdc, const RECT *want, HRGN rgn)
{
#ifdef PORTHEON_TEST   /* PORTHEON_TEST_PAINTLOG=<file>: each paint's time and area (UI speed tests) */
    static wchar_t lp[MAX_PATH]; static int has = -1; if (has < 0) has = GetEnvironmentVariableW(L"PORTHEON_TEST_PAINTLOG", lp, MAX_PATH) > 0;
    LARGE_INTEGER f, a, b; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&a);
    { wchar_t fs[4]; if (GetEnvironmentVariableW(L"PORTHEON_TEST_FULL", fs, 4) && !GetEnvironmentVariableW(L"PORTHEON_TEST_PARTIAL", NULL, 0)) { want = NULL; rgn = NULL; } }
    if (want && IsRectEmpty(want) && GetTickCount() - g_vitTestInvalAt < 60) { want = &g_vitTestInval; rgn = NULL; }   /* 6.2: see home62.c */   /* a window bigger than this screen: paint all of it for the shots (5.0: PORTHEON_TEST_PARTIAL keeps the real partial paints, for the idle cost) */
    PaintInner(wdc, want, rgn);
    g_testPaints++;   /* Released: the .go hook's keys wait for a painted frame (ui.c) */
    QueryPerformanceCounter(&b);
    YtTestAfterPaint(g_back, g_backW, g_backH);   /* 6.2: PORTHEON_TEST_YTSHOT (yt.c) */
    if (has) { FILE *o = _wfopen(lp, L"a"); if (o) { fwprintf(o, L"%d %.2f %ld %ld %d\n", g_view, (b.QuadPart - a.QuadPart) * 1000.0 / f.QuadPart, want ? want->right - want->left : -1, want ? want->bottom - want->top : -1, g_full); fclose(o); } }
    {   /* 4.4.0, PORTHEON_TEST_DUMP=<base>: when <base>.req exists, the frame just drawn goes to <base>.png (a window off
           the screen gives PrintWindow nothing) */
        static wchar_t dp[MAX_PATH]; static int hasD = -1; if (hasD < 0) hasD = GetEnvironmentVariableW(L"PORTHEON_TEST_DUMP", dp, MAX_PATH - 8) > 0;
        wchar_t rq[MAX_PATH], png[MAX_PATH];
        if (hasD && g_back) { _snwprintf(rq, MAX_PATH, L"%s.req", dp); _snwprintf(png, MAX_PATH, L"%s.png", dp);
            if (FileExists(rq)) { GpImage *bm = NULL; static const CLSID pc = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
                if (!GdipCreateBitmapFromHBITMAP(g_back, NULL, &bm) && bm) { GdipSaveImageToFile(bm, png, &pc, NULL); GdipDisposeImage(bm); }
                DeleteFileW(rq); } }
    }
#else
    PaintInner(wdc, want, rgn);
#endif
}
static void PaintInner(HDC wdc, const RECT *want, HRGN rgn)
{
    RECT rc; GetClientRect(g_wnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return;
    if (!g_back || g_backW != rc.right || g_backH != rc.bottom) {
        if (g_back) DeleteObject(g_back);
        /* 4.0.1: a 32-bit DIB section, not a device bitmap: GDI+ draws straight into its memory (with a device
           bitmap every translucent shape was copied out and back in, 3 to 5 times slower, worst in 4K) */
        BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), rc.right, -rc.bottom, 1, 32, BI_RGB}}; void *bits = NULL;
        g_back = CreateDIBSection(wdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!g_back) g_back = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
        g_backW = rc.right; g_backH = rc.bottom; want = &rc; rgn = NULL;
    }
    RECT pr = want ? *want : rc; IntersectRect(&pr, &pr, &rc); if (IsRectEmpty(&pr)) { pr = rc; rgn = NULL; }
    g_paintRect = pr; g_paintRgn = rgn;
    HDC dc = CreateCompatibleDC(wdc); HGDIOBJ ob = SelectObject(dc, g_back);
    if (rgn) ExtSelectClipRgn(dc, rgn, RGN_COPY); else IntersectClipRect(dc, pr.left, pr.top, pr.right, pr.bottom);
    /* 6.2: a paint the vitrine alone asked for (its video or its edit): the background under it and the vitrine, nothing
       else is there; the hit regions of the last whole paint stay */
    SkinSync();   /* 6.2: the look of the game on screen */
    g_itMax = 0;   /* 6.2: PaintItems says it again if this screen has a list that does not fit */
    BOOL vitOnly = VitFastOk(&pr);
    SetBkMode(dc, TRANSPARENT); if (!vitOnly) g_nhits = 0;
    GpGraphics *g = NULL; GdipCreateFromHDC(dc, &g); GdipSetSmoothingMode(g, 4); GdipSetPixelOffsetMode(g, 4); GdipSetInterpolationMode(g, 5);
    /* full screen: the 1280x800 frame in the middle of the monitor. 4.0.1: GDI+ moves it with its own world transform;
       the DC's viewport origin is not kept (GDI+ puts it back to 0 when it draws), so the text helpers set it (GdiText) */
    GdipTranslateWorldTransform(g, (float)g_ox, (float)g_oy, 0);
    ClipBase(g);
    if (vitOnly) {
        PaintBackground(dc, g, rc);
#if HOME_PAGES
        if (g_csel >= 0) LibHomeCover(dc, g);   /* Released: a custom game's cover stands in that rectangle (PaintHome) */
#endif
        VitHomeEx(dc, g, FALSE);   /* 6.2 review: no hit added (the last whole paint's list stays) */
        GdipDeleteGraphics(g); SetViewportOrgEx(dc, 0, 0, NULL);
        BitBlt(wdc, pr.left, pr.top, pr.right - pr.left, pr.bottom - pr.top, dc, pr.left, pr.top, SRCCOPY);
        SelectObject(dc, ob); DeleteDC(dc); return;
    }
    for (int k = 0; k < 4; k++) g_editWant[k] = FALSE;
    if (g_overlay == OV_VITFULL) PaintVitFull(dc, g, rc);   /* 6.2: the vitrine full screen covers everything */
    else if (g_view == V_BOOT) PaintBoot(dc, g, rc);
    else if (g_view == V_LAUNCH) { PaintLaunch(dc, g, rc); PaintToast(dc, g); }   /* 4.4.0: the game while it loads */
    else {
        PaintBackground(dc, g, rc);
        if (g_view == V_PROFILES) PaintProfiles(dc, g);
        else if (g_view == V_WELCOME) PaintWelcome(dc, g);   /* 4.4.0 */
        else {
            PaintTopBar(dc, g);
            switch (g_view) {
            case V_GAME: PaintHub(dc, g); break;
            case V_SETTINGS: PaintSettings(dc, g); break;
            case V_INSTALL: PaintInstall(dc, g); break;
            case V_ONLINE: PaintOnline(dc, g); break;
            default: PaintHome(dc, g);
            }
            if (g_view != V_HOME) PaintStatus(dc, g);
        }
        if (g_view != V_PROFILES && g_view != V_WELCOME && !g_fxLight) {   /* 5.0: a new screen comes up from black, as a film cut */
            float vt = MotionT(g_viewT0, 280); if (vt < 1.0f) Fill(g, -g_ox, -g_oy, rc.right, rc.bottom, A(RGB(0, 0, 0), (BYTE)(220 * (1.0f - EaseOut3(vt)))));
        }
        if (g_overlay) {
            g_nhits = 0;   /* only the overlay answers the mouse */
            for (int k = 0; k < 4; k++) g_editWant[k] = FALSE;
            if (g_overlay == OV_UPDATE) PaintUpdateOverlay(dc, g, rc);
            else if (g_overlay == OV_PROFEDIT) PaintProfEdit(dc, g, rc);
            else if (g_overlay == OV_PROFMENU) PaintProfMenu(dc, g, rc);   /* 4.4.0 */
            else if (g_overlay == OV_KEYS) PaintKeys(dc, g, rc);           /* 6.0 */
            else if (g_overlay == OV_HELP) PaintHelp62(dc, g, rc);         /* 6.2 */
            else if (g_overlay == OV_ACHV) PaintAch62(dc, g, rc);          /* 6.2 */
            else if (g_overlay == OV_PADMAP) PaintPadMap(dc, g, rc);       /* 6.2 */
            else if (g_overlay == OV_CKEYS) PaintCKeys(dc, g, rc);         /* 6.2 */
            else if (g_overlay == OV_LIBADD || g_overlay == OV_CGAME) PaintLibOverlay(dc, g, rc);   /* 6.1: the Add screen, a custom game's page */
            else if (g_overlay == OV_QUICK) PaintQuickPop(dc, g, rc);   /* 6.2: a quick option's pop-over */
            else PaintNews(dc, g, rc);
        } else PaintToast(dc, g);
        PaintFocusRing(g);
    }
    GdipDeleteGraphics(g);
    SetViewportOrgEx(dc, 0, 0, NULL);
    BitBlt(wdc, pr.left, pr.top, pr.right - pr.left, pr.bottom - pr.top, dc, pr.left, pr.top, SRCCOPY);
    SelectObject(dc, ob); DeleteDC(dc);
    HWND eds[4] = {g_editGame, g_editPlayer, g_editAddr, g_editProf};
    for (int k = 0; k < 4; k++) if (!g_editWant[k] && eds[k]) PlaceEdit(k, eds[k], g_editRect[k], FALSE);
}

/* ---------------------------------------------------------------- moving between screens */
static void OpenGame(void)
{
    if (g_csel >= 0) { CPageOpen(g_csel); return; }   /* 6.1: a custom game has a simple page, not a hub */
    if (SOON(g_sel)) return;   /* 6.2 */
    SetView(V_GAME); g_tab = TAB_OVERVIEW; LoadEdits();
    /* 4.0.1: the first time a game's hub opens, its setup guide (what each setting does, and its value for this PC) */
    if (PS4G(g_sel) && !GetInt(g_sel, L"pgame:guide_seen", 0)) { PutInt(g_sel, L"pgame:guide_seen", 1); g_tab = TAB_GUIDE; g_guidePage = 0; }
}
static void OpenGuide(int g) { if (SOON(g)) { Msg(C_SUB, L"%s", L2(L"GTA VI: em breve no PORTHEON.", L"GTA VI: coming soon to PORTHEON.")); return; } g_guideGame = g; g_guideStep = 0; g_instCheckGame = -1; SetView(V_INSTALL); }
static int g_afterBoot = V_HOME;
static void FirstRunGuide(void)
{
    if (PIni(L"guide_seen", 0)) return;
    PIniSet(L"guide_seen", 1);
    for (int i = 0; i < NGAMES; i++) if (Status(i) == S_READY) return;
    g_guideGame = SOON(g_sel) ? 3 : g_sel; g_guideStep = 0; g_viewBack = V_HOME; g_afterBoot = V_INSTALL;   /* after the boot: the install helper */
    if (!PIni(L"scan_done", 0)) StartScan(FALSE);   /* the first time, look for games already on this PC */
}
static int AfterAccount(void) { return (PIni(L"profile_pick", 1) && g_nprof > 1) ? V_PROFILES : g_afterBoot; }
/* 5.0 (the user: "organized when you enter"): PORTHEON opens on a game that can be played: the last one when it is
   ready, else the first ready game of the shelf */
static void OpenOnReady(void)
{
    if (St(g_sel) == S_READY) return;
    HomeOrder(); for (int k = 0; k < NGAMES; k++) if (St(g_homeOrd[k]) == S_READY) { g_sel = g_homeOrd[k]; LoadEdits(); return; }
}
static void BootDone(void)
{
    if (g_view != V_BOOT) return;
    BootLogoFree();   /* 6.0 final: the opening's small logo */
    OpenOnReady();
    /* 4.4.0: the first start of 4.4 (a new player, or one coming from an older version) creates the account once */
    g_view = !PIni(L"account_rev", 0) ? V_WELCOME : AfterAccount();
    g_viewT0 = GetTickCount(); g_focus = -1; Motion(300); InvalidateRect(g_wnd, NULL, FALSE);
}
static void PeDropPhoto(void) { if (g_pePhotoImg) { GdipDisposeImage(g_pePhotoImg); g_pePhotoImg = NULL; } }
static void WelcomeDone(void) { PIniSet(L"account_rev", 1); g_peWelcome = FALSE; if (g_view == V_WELCOME) SetView(AfterAccount()); }
static void Back(void)
{
    if (LibAddBack()) return;   /* 6.1: the Add screen: back to its sources */
    if (g_overlay == OV_VITFULL || g_overlay == OV_QUICK) { Home62Back(); return; }   /* 6.2 */
    if (PadMapBack()) return;   /* 6.2 */
    if (g_overlay == OV_KEYS && g_keyCap >= 0) { g_keyCap = -1; g_keyWarn[0] = 0; Snd(SND_BACK); InvalidateRect(g_wnd, NULL, FALSE); return; }   /* 6.0: a choice waiting for a key */
    if (g_overlay) {
        if (g_overlay != OV_UPDATE || (g_updState != 2 && g_updState != 3 && g_updState != 4) || (g_updAuto && g_updState != 4)) {   /* 6.1: the automatic download runs behind */
            if (g_overlay == OV_PROFEDIT) { PlaceEdit(3, g_editProf, g_editRect[3], FALSE); PeDropPhoto(); }
            g_overlay = OV_NONE; Snd(SND_BACK);
        }
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    if (g_view == V_BOOT) { BootDone(); return; }
    if (g_view == V_WELCOME) { Snd(SND_BACK); WelcomeDone(); return; }   /* "Not now" */
    if (g_view == V_LAUNCH) { if (g_proc) { KillTimer(g_wnd, 13); SetView(V_HOME); } return; }   /* 4.4.0: a slow start goes back to PORTHEON */
    if (g_view == V_HOME) { if (HOME_PAGES && g_homePage) { Snd(SND_BACK); HomePageSet(0); } return; }   /* Released: Esc / B / the right button on page 2: back to page 1 */
    SaveEdits(); Snd(SND_BACK);
    SetView(g_view == V_GAME || g_view == V_PROFILES ? V_HOME : g_viewBack == V_GAME ? V_GAME : V_HOME);
}
static void ProfEditOpen(int i)
{
    g_peIndex = i; g_peColor = i >= 0 ? g_prof[i].color : (g_nprof * 5) % 12; g_peAvatar = i >= 0 ? g_prof[i].avatar : 0;
    g_peStatus = i >= 0 ? g_prof[i].status : PST_ONLINE; g_pePhoto = 0; PeDropPhoto();
    SetWindowTextW(g_editProf, i >= 0 ? g_prof[i].name : L"");
    g_overlay = OV_PROFEDIT; InvalidateRect(g_wnd, NULL, FALSE); UpdateWindow(g_wnd); SetFocus(g_editProf);
}
static void ProfEditSave(void)
{
    wchar_t n[32]; GetWindowTextW(g_editProf, n, 32);
    { wchar_t *s = n; while (*s == L' ') s++; memmove(n, s, (wcslen(s) + 1) * sizeof(wchar_t)); size_t l = wcslen(n); while (l && n[l - 1] == L' ') n[--l] = 0; }
    if (!n[0]) { Msg(C_ORANGE, L"%s", L2(L"Dê um nome ao perfil.", L"Give the profile a name.")); Snd(SND_ERR); return; }
    int idx = g_peIndex; BOOL made = FALSE;
    if (idx < 0) { idx = ProfileAdd(n, g_peColor, g_peAvatar); if (idx < 0) return; made = TRUE; }
    Profile *p = &g_prof[idx];
    wcsncpy(p->name, n, 31); p->name[31] = 0; p->color = g_peColor; p->avatar = g_peAvatar; p->status = g_peStatus;
    /* 4.4.0: the photo: the new picture is written as avatars\<id>.png; "remove" deletes it */
    if (g_pePhoto == 1 && g_pePhotoImg) {
        if (PhotoSave(p->id, g_pePhotoImg)) p->photo = 1;
        else Msg(C_ORANGE, L"%s", L2(L"Não consegui guardar a foto (pasta %LOCALAPPDATA%\\PORTHEON\\avatars).", L"Could not keep the photo (folder %LOCALAPPDATA%\\PORTHEON\\avatars)."));
    } else if (g_pePhoto == 2) { PhotoDelete(p->id); p->photo = 0; }
    ProfilesSave(); PeDropPhoto();
    if (made) ProfileSelect(idx);
    if (idx == g_curProf) SetWindowTextW(g_editPlayer, n);
    if (g_peWelcome) Msg(C_GOOD, L2(L"Conta pronta. Bem-vindo, %s!", L"Your account is ready. Welcome, %s!"), n);
    else if (made) Msg(C_GOOD, L2(L"Usuário %s criado, com saves próprios.", L"User %s made, with their own saves."), n);
    else Msg(C_GOOD, L"%s", L2(L"Perfil salvo.", L"Profile saved."));
    PlaceEdit(3, g_editProf, g_editRect[3], FALSE); g_overlay = OV_NONE; Snd(SND_OK); SetFocus(g_wnd);
    if (g_peWelcome) WelcomeDone();
    InvalidateRect(g_wnd, NULL, FALSE);
}
static void ProfEditDelete(void)
{
    if (g_peIndex <= 0) return;
    wchar_t q[400]; _snwprintf(q, 400, L2(L"Remover o usuário %s?\n\nOs saves dele continuam no disco (pasta profiles\\%d de cada jogo).", L"Remove the user %s?\n\nTheir saves stay on disk (folder profiles\\%d of each game)."), g_prof[g_peIndex].name, g_prof[g_peIndex].id);
    if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    ProfileRemove(g_peIndex); PlaceEdit(3, g_editProf, g_editRect[3], FALSE); PeDropPhoto(); g_overlay = OV_NONE; SetFocus(g_wnd); LoadEdits(); InvalidateRect(g_wnd, NULL, FALSE);
}
/* 4.4.0: once the game's window shows, PORTHEON lets go of what it drew with: the back buffer (33 MB in 4K), the games'
   pictures, the photos, and its memory pages (the game gets the RAM) */
static void UiFreeMemory(void)
{
    if (g_back) { DeleteObject(g_back); g_back = NULL; g_backW = g_backH = 0; }
    LaunchBgFree(); PhotosFree(); PeDropPhoto(); VitStop();   /* 6.2: the vitrine's pictures and video */
    for (int i = 0; i < NGAMES; i++) ForgetArt(i);
    ScaleChanged();
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}
/* Released (11-13, the user: "turn our PORTHEON into a console"): the console mode is the player's own switch in the power
   menu: PORTHEON opens with Windows (the user's own Run key, nothing system-wide; it already opens in full screen), and
   the PC can be put to sleep, restarted or turned off from here, each after a question. Off until the player turns it on. */
#define CONSOLE_RUN L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
static BOOL ConsoleModeOn(void)
{
    HKEY k; wchar_t v[MAX_PATH + 8]; DWORD n = sizeof(v), t = 0; BOOL on = FALSE;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, CONSOLE_RUN, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) return FALSE;
    if (RegQueryValueExW(k, L"PORTHEON", NULL, &t, (BYTE *)v, &n) == ERROR_SUCCESS && t == REG_SZ) on = TRUE;
    RegCloseKey(k); return on;
}
static void ConsoleModeSet(BOOL on)
{
    HKEY k; wchar_t exe[MAX_PATH], v[MAX_PATH + 8];
    if (RegOpenKeyExW(HKEY_CURRENT_USER, CONSOLE_RUN, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    if (on) { DWORD l = GetModuleFileNameW(NULL, exe, MAX_PATH); if (l && l < MAX_PATH) { _snwprintf(v, MAX_PATH + 8, L"\"%s\"", exe); v[MAX_PATH + 7] = 0; RegSetValueExW(k, L"PORTHEON", 0, REG_SZ, (const BYTE *)v, (DWORD)((wcslen(v) + 1) * sizeof(wchar_t))); } }
    else RegDeleteValueW(k, L"PORTHEON");
    RegCloseKey(k);
    Msg(C_GOOD, on ? L2(L"Modo console ligado: o PORTHEON abre em tela cheia quando o Windows iniciar. Desligue aqui mesmo quando quiser.", L"Console mode on: PORTHEON opens in full screen when Windows starts. Turn it off right here whenever you like.")
                   : L2(L"Modo console desligado: o PORTHEON não abre mais com o Windows.", L"Console mode off: PORTHEON no longer opens with Windows."));
}
static void PcPower(int what)   /* 0 sleep, 1 restart, 2 turn off; nothing while a game runs */
{
    static const wchar_t *const qpt[] = {L"Suspender o PC agora?", L"Reiniciar o PC agora?", L"Desligar o PC agora?"};
    static const wchar_t *const qen[] = {L"Put the PC to sleep now?", L"Restart the PC now?", L"Turn the PC off now?"};
    if (g_proc) { Msg(C_ORANGE, L2(L"Feche o jogo antes.", L"Close the game first.")); return; }
    if (MessageBoxW(g_wnd, L2(qpt[what], qen[what]), L"PORTHEON", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    if (what == 0) {
        HMODULE pp = LoadLibraryW(L"powrprof.dll");
        BOOLEAN (WINAPI *sus)(BOOLEAN, BOOLEAN, BOOLEAN) = pp ? (BOOLEAN (WINAPI *)(BOOLEAN, BOOLEAN, BOOLEAN))(void *)GetProcAddress(pp, "SetSuspendState") : NULL;
        if (sus) sus(FALSE, FALSE, FALSE);
        if (pp) FreeLibrary(pp);
        return;
    }
    wchar_t sys[MAX_PATH], cmd[MAX_PATH + 40]; STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi;
    if (!GetSystemDirectoryW(sys, MAX_PATH)) return;
    _snwprintf(cmd, MAX_PATH + 40, L"\"%s\\shutdown.exe\" %s /t 0", sys, what == 1 ? L"/r" : L"/s"); cmd[MAX_PATH + 39] = 0;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
}
static void PowerMenu(void)
{
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (g_full ? MF_CHECKED : 0), 1, L2(L"Tela cheia (F11)", L"Full screen (F11)"));
    AppendMenuW(m, MF_STRING, 2, L2(L"Trocar de perfil", L"Switch profile"));
    AppendMenuW(m, MF_STRING, 4, L2(L"Central de ajuda (F1)", L"Help center (F1)"));
    AppendMenuW(m, MF_STRING, 5, L2(L"Conquistas", L"Achievements"));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    BOOL con = ConsoleModeOn();
    AppendMenuW(m, MF_STRING | (con ? MF_CHECKED : 0), 6, L2(L"Modo console: abrir junto com o Windows", L"Console mode: open with Windows"));
    AppendMenuW(m, MF_STRING, 7, L2(L"Suspender o PC", L"Put the PC to sleep"));
    AppendMenuW(m, MF_STRING, 8, L2(L"Reiniciar o PC", L"Restart the PC"));
    AppendMenuW(m, MF_STRING, 9, L2(L"Desligar o PC", L"Turn the PC off"));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, 3, L2(L"Fechar o PORTHEON", L"Close PORTHEON"));
    int c = PopupAt(m, H_POWER);
    if (c == 6) { ConsoleModeSet(!con); return; }
    if (c >= 7 && c <= 9) { PcPower(c - 7); return; }
    if (c == 1) SetFullscreen(!g_full); else if (c == 2) SetView(V_PROFILES); else if (c == 3) PostMessageW(g_wnd, WM_CLOSE, 0, 0); else if (c == 4) Extra62Open(OV_HELP); else if (c == 5) Extra62Open(OV_ACHV);
}
static void ApplyLangToGames(void)
{
    int v = LANGS[g_lang].ps4;
    /* 4.2: UFC 6 takes the PS5 console language, the same codes (LANG6 has no Indonesian: English). 4.3: UFC 5 again,
       with its own list (LANG5, in PORTHEON's language order; Ukrainian -> Russian, Indonesian -> English), as in 4.0.6 */
    static const int ufc5[NLANG] = {0, 2, 5, 6, 7, 8, 9, 10, 11, 11, 12, 2, 13, 14, 15, 16};
    for (int g = 0; g < NGAMES; g++) { if (!PortOk(g) || X360G(g) || EXTG(g)) continue; g_kvGame = -1; PutInt(g, L"language", UFC5G(g) ? ufc5[g_lang] : G[g].ps5 && v == 29 ? 1 : v); }
    g_kvGame = -1;
    Msg(C_GOOD, L2(L"Os jogos vão abrir em %s (onde o jogo tem esse idioma).", L"The games will start in %s (where the game has that language)."), LANGS[g_lang].name);
}
static void SetLang(int l)
{
    if (l < 0 || l >= NLANG) return;
    g_lang = l; LangSave(l); MakeFonts(); ScaleChanged(); g_ncheat = -1; g_nlinks = 0;
    Msg(C_GOOD, L"%s", LANGS[l].name);
}

/* ---------------------------------------------------------------- the keyboard and the controller: the focus */
static void FocusDefault(void)
{
    static const int pref[] = {H_PLAY, H_GUIDE, H_PROFPICK, H_TAB + TAB_OVERVIEW, H_SETSEC, H_INSTNEXT, H_GUIDEGAME + 3, H_UPDGO, H_NEWSCLOSE, H_PROFSAVE, H_WELCOMEGO, H_PSTAT};
    RECT r;
    if (g_view == V_PROFILES && HitRect(H_PROFPICK + g_curProf, &r)) { g_focus = H_PROFPICK + g_curProf; return; }
    if (g_view == V_GAME && HitRect(H_TAB + g_tab, &r)) { g_focus = H_TAB + g_tab; return; }
    if (g_view == V_SETTINGS && HitRect(H_SETSEC + g_setSec, &r)) { g_focus = H_SETSEC + g_setSec; return; }
    if (HOME_PAGES && g_view == V_HOME && !g_overlay && g_homePage) {   /* Released: page 2's first stop: the first quick option, else the first card, else the logs pill */
        static const int p2[] = {H_QCHIP, H_HOMECARD, H_CCARD, H_HOMELOGS, H_CRED_DISCORD, H_HOMEMORE};
        for (int i = 0; i < (int)(sizeof(p2) / sizeof(p2[0])); i++) if (HitRect(p2[i], &r)) { g_focus = p2[i]; return; }
    }
    for (int i = 0; i < (int)(sizeof(pref) / sizeof(pref[0])); i++) if (HitRect(pref[i], &r)) { g_focus = pref[i]; return; }
    if (HOME_PAGES && g_view == V_HOME && !g_overlay) { int id = g_csel >= 0 ? H_CUSTOM + g_csel : H_CARD + g_sel; if (HitRect(id, &r)) { g_focus = id; return; } }   /* Released: a game with nothing to press (to come): its tile */
    g_focus = g_nhits ? g_hits[0].id : -1;
}
/* Released: the home's two pages (ui.c, HOME_PAGES): the mouse wheel turns them (WM_MOUSEWHEEL) */
static BOOL HomeWheel(int delta)
{
    if (!HOME_PAGES || g_view != V_HOME || g_overlay || !delta) return FALSE;
    if ((delta < 0) != (g_homePage != 0)) { g_kbNav = FALSE; Snd(SND_MOVE); HomePageSet(delta < 0); }
    return TRUE;
}
static void FocusMove(int dx, int dy)
{
    RECT cur;
    if (g_focus < 0 || !HitRect(g_focus, &cur)) { FocusDefault(); return; }
    if (dy && g_itMax > 0 && ((g_focus >= H_DROP && g_focus < H_DROP + 40) || (g_focus >= H_SWITCH && g_focus < H_SWITCH + 40))) {   /* 6.2: the list moves a row */
        int r = g_itRow;
        if (dy > 0 && cur.top >= g_itLastY - SC(4) && g_itRow < g_itMax) r++;
        if (dy < 0 && cur.top <= g_itFirstY + SC(24) && g_itRow > 0) r--;
        if (r != g_itRow) { g_itRow = r; Snd(SND_MOVE); InvalidateRect(g_wnd, NULL, FALSE); return; }
    }
    int cx = (cur.left + cur.right) / 2, cy = (cur.top + cur.bottom) / 2, best = -1; double bestD = 1e18;
    /* 4.0.4: left and right stay in the same row when it goes on that way (as on a PS5: the top bar's icons, not the
       tiles under them) */
    BOOL rowOnly = FALSE;
    if (dx) for (int i = 0; i < g_nhits && !rowOnly; i++) {
        RECT r = g_hits[i].r; int x = (r.left + r.right) / 2, id = g_hits[i].id;
        if (id != g_focus && id != H_MSG && id != H_TOAST && id != H_SKIPBOOT && r.top < cur.bottom && r.bottom > cur.top && (dx > 0 ? r.left >= cur.right - SC(4) && x > cx : r.right <= cur.left + SC(4) && x < cx)) rowOnly = TRUE;
    }
    for (int i = 0; i < g_nhits; i++) {
        int id = g_hits[i].id; if (id == g_focus || id == H_MSG || id == H_TOAST || id == H_SKIPBOOT) continue;
        RECT r = g_hits[i].r; int x = (r.left + r.right) / 2, y = (r.top + r.bottom) / 2;
        if (rowOnly && !(r.top < cur.bottom && r.bottom > cur.top)) continue;
        int ax = dx > 0 ? x - cur.right : dx < 0 ? cur.left - x : 0, ay = dy > 0 ? y - cur.bottom : dy < 0 ? cur.top - y : 0;   /* along the direction */
        if (dx && (dx > 0 ? r.left < cur.right - SC(4) && x <= cx : r.right > cur.left + SC(4) && x >= cx)) continue;
        if (dy && (dy > 0 ? r.top < cur.bottom - SC(4) && y <= cy : r.bottom > cur.top + SC(4) && y >= cy)) continue;
        if ((dx && ax < -SC(30)) || (dy && ay < -SC(30))) continue;
        int ox = dx ? 0 : (x < cur.left ? cur.left - x : x > cur.right ? x - cur.right : 0), oy = dy ? 0 : (y < cur.top ? cur.top - y : y > cur.bottom ? y - cur.bottom : 0);
        double along = dx ? fabs((double)(x - cx)) : fabs((double)(y - cy)), across = dx ? fabs((double)(y - cy)) + oy : fabs((double)(x - cx)) + ox;
        double d = along + across * 2.2;
        if (d < bestD) { bestD = d; best = id; }
    }
    if (g_view == V_HOME && !g_overlay && best >= 0) {   /* 7.0: the video is on the right, under the far end of the row of tiles */
        RECT tr; int selId = g_csel >= 0 ? H_CUSTOM + g_csel : H_CARD + g_sel;
        BOOL bestRow = (best >= H_CARD && best < H_CARD + NGAMES) || (best >= H_CUSTOM && best < H_CUSTOM + g_ncg) || best == H_LIBADD || best == H_NAV + 2;
        BOOL curTile = (g_focus >= H_CARD && g_focus < H_CARD + NGAMES) || (g_focus >= H_CUSTOM && g_focus < H_CUSTOM + g_ncg);
        if (dy < 0 && cur.top > SC(238) && bestRow && HitRect(selId, &tr)) best = selId;   /* up from the hero or the video: back to the chosen tile, never another game */
        if (dy > 0 && curTile && HitRect(H_PLAY, &tr)) best = H_PLAY;                      /* down from a tile: PLAY */
    }
#if HOME_PAGES
    if (g_view == V_HOME && !g_overlay && dy) {   /* Released: the two pages. Page 1: tiles -> PLAY's row -> (right) the video -> "Mais"; Down from PLAY's row or from "Mais" opens page 2. Page 2: Up from its first row goes back */
        BOOL row = (g_focus >= H_CARD && g_focus < H_CARD + NGAMES) || (g_focus >= H_CUSTOM && g_focus < H_CUSTOM + g_ncg) || g_focus == H_LIBADD || g_focus == H_NAV + 2;
        if (!g_homePage && dy > 0 && !row && !TopBarHit(g_focus) && g_focus != H_VITRINE && cur.top > SC(238)) { Snd(SND_MOVE); HomePageSet(1); return; }
        if (g_homePage && dy < 0 && (best < 0 || TopBarHit(best) || best == H_HOMEMORE)) { Snd(SND_BACK); HomePageSet(0); return; }
        if (!g_homePage && dy < 0 && g_focus == H_HOMEMORE) { RECT tr; if (HitRect(H_VITRINE, &tr)) best = H_VITRINE; else if (HitRect(H_PLAY, &tr)) best = H_PLAY; }   /* Up from "Mais": the way it was reached (a tile stands straight above it and would win) */
    }
#endif
    if (best >= 0 && best != g_focus) {
        g_focus = best; Snd(SND_MOVE);
        if (g_view == V_HOME && best >= H_CARD && best < H_CARD + NGAMES && (best - H_CARD != g_sel || g_csel >= 0)) Select(best - H_CARD);   /* like a console: the game under the focus is the one shown */
        else if (g_view == V_HOME && !g_overlay && best >= H_CUSTOM && best < H_CUSTOM + g_ncg && best - H_CUSTOM != g_csel) CSelect(best - H_CUSTOM);   /* 6.1: a custom tile */
    }
}
static void OnClick(int h);
static void NavKey(int vk)
{
    g_kbNav = TRUE; g_lastInput = GetTickCount();
    switch (vk) {
    case VK_UP: FocusMove(0, -1); break;
    case VK_DOWN: FocusMove(0, 1); break;
    case VK_LEFT: FocusMove(-1, 0); break;
    case VK_RIGHT: FocusMove(1, 0); break;
    case VK_RETURN: case VK_SPACE:
        if (g_view == V_BOOT) { BootDone(); break; }
        if (g_view == V_LAUNCH) break;   /* 4.4.0: the loading picture */
        if (QuietInput()) break;   /* 4.3.1: a game has just ended (portheon.c) */
        /* 4.3.2: PORTHEON stays in front while UFC 5/6 open their window, and PLAY is the STOP button then: a key never
           stops a game that is opening (the mouse still can) */
        if (g_proc && (g_focus < 0 || GetTickCount64() - g_playStart < 20000)) break;
        if (g_focus < 0) FocusDefault();
        if (g_focus >= 0) { int f = g_focus; if (g_view == V_HOME && f >= H_CARD && f < H_CARD + NGAMES && f - H_CARD == g_sel && g_csel < 0) { if (St(g_sel) == S_READY) { Snd(SND_PLAY); Play(); } else if (NGPORTG(g_sel) && St(g_sel) == S_NOPORT) ExtInstallMenu(g_sel); else OpenGuide(g_sel); }
                            else if (g_view == V_HOME && !g_overlay && g_csel >= 0 && f == H_CUSTOM + g_csel) { Snd(SND_PLAY); Play(); }   /* 6.1 */
                            else OnClick(f); }
        break;
    case VK_ESCAPE: case VK_BACK: Back(); break;
    case VK_PLAY: if (g_proc && GetTickCount64() - g_playStart < 20000) break;   /* 4.3.2: as above */
        if ((g_view == V_HOME || g_view == V_GAME) && !g_overlay && (g_proc || !QuietInput())) { if (St(g_sel) == S_READY || g_proc || (g_csel >= 0 && g_view == V_HOME)) { Snd(SND_PLAY); Play(); } else Snd(SND_ERR); } break;   /* 6.1: g_csel */
    case VK_APPS: if (g_view == V_HOME && !g_overlay) { Snd(SND_OK); OpenGame(); } break;
    case VK_PRIOR: case VK_NEXT: {
        int d = vk == VK_NEXT ? 1 : -1;
        if (g_overlay) { LibOverlayPage(d); break; }   /* 6.1: the Add screen's list pages */
        if (g_view == V_GAME) { int k = 0; while (k < NHUB && HUB[k].tab != g_tab) k++; do k = (k + d + NHUB) % NHUB; while (!HubTabOk(g_sel, HUB[k].tab)); SaveEdits(); g_tab = HUB[k].tab; g_focus = H_TAB + g_tab; Snd(SND_MOVE); }
        else if (g_view == V_SETTINGS) { g_setSec = (g_setSec + d + NSETS) % NSETS; g_focus = H_SETSEC + g_setSec; Snd(SND_MOVE); }
        else if (g_view == V_HOME) LibHomeCycle(d);   /* 6.1: the built-in games in their order, then the player's own (ui_library.c) */
        else if (g_view == V_INSTALL || g_view == V_ONLINE) { int k = 0; while (k < NGAMES && HOME_ORDER[k] != g_guideGame) k++; k = (k + d + NGAMES) % NGAMES; g_guideGame = HOME_ORDER[k]; g_instCheckGame = -1; Snd(SND_MOVE); }
        break; }
    case VK_TAB: if (!g_overlay && g_view != V_BOOT && g_view != V_LAUNCH && g_view != V_WELCOME) SetView(V_PROFILES); break;
    case VK_F11: SetFullscreen(!g_full); break;
    case VK_F1: if (!g_overlay) Extra62Open(OV_HELP); break;   /* 6.2 */
    case VK_F8: if (g_view == V_HOME && !g_overlay && g_csel < 0) { Snd(SND_OK); HomeLogs(); } break;   /* 6.2: Square / X: the game's logs (a built-in game's) */
    case 'M': if (g_view == V_HOME && (!g_overlay || g_overlay == OV_VITFULL) && VitMode() == 0) { Snd(SND_OK); g_ytNoSave = TRUE; YtSetSound(!YtSoundPref(), TRUE); g_ytNoSave = FALSE; } break;   /* 6.2: M / R3: the videos' sound on or off */
    }
    if (g_focus >= 0) { if (g_focus == H_PLAY && g_hot != H_PLAY) g_sheenT0 = GetTickCount(); g_hot = g_focus; }
    InvalidateRect(g_wnd, NULL, FALSE);
}

/* ---------------------------------------------------------------- what a click does */
static void OnClick(int h)
{
    if (h < 0) { if (g_overlay == OV_PROFMENU) { g_overlay = OV_NONE; InvalidateRect(g_wnd, NULL, FALSE); } else if (g_overlay == OV_QUICK || g_overlay == OV_VITFULL) Home62Back(); return; }   /* 6.2 */   /* 4.4.0: a click beside the profile card closes it */
    StatusDirty();
    if (g_view == V_BOOT) { BootDone(); return; }
    if (g_view == V_LAUNCH) return;   /* 4.4.0: the loading picture takes no clicks (Esc goes back) */
    if (g_overlay == OV_PROFMENU) {   /* 4.4.0: the profile card */
        g_overlay = OV_NONE; Snd(SND_OK);
        if (h >= H_PSTAT && h <= H_PSTAT_LAST) {
            ProfileSetStatus(g_curProf, h - H_PSTAT);
            Msg(C_GOOD, L2(L"Status: %s. %s.", L"Status: %s. %s."), ProfStatusName(h - H_PSTAT), ProfStatusHint(h - H_PSTAT));
        }
        else if (h == H_PMEDIT) { ProfEditOpen(g_curProf); return; }
        else if (h == H_PMSWITCH) SetView(V_PROFILES);
        else if (h == H_PMADD) { ProfEditOpen(-1); return; }
        else if (h == H_PMMANAGE) { g_setSec = SET_PROFILES; SetView(V_SETTINGS); }
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    if (g_overlay == OV_UPDATE) {
        if (h == H_UPDGO) StartDownload();
        else if (h == H_UPDLATER) { g_overlay = OV_NONE; WritePrivateProfileStringW(L"PORTHEON", L"update_skip", g_updTag, g_pIni); }
        else if (h == H_UPDCANCEL) UpdCancel();   /* 6.1 review: also stops the automatic install's timer, and remembers it for this version */
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    if (g_overlay == OV_NEWS) { if (h == H_NEWSCLOSE) { g_overlay = OV_NONE; Snd(SND_BACK); } InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (g_overlay == OV_KEYS) { KeysClick(h); return; }   /* 6.0 */
    if (g_overlay == OV_HELP || g_overlay == OV_ACHV) { Extra62Click(h); return; }   /* 6.2 */
    if (g_overlay == OV_PADMAP) { PadMapClick(h); return; }   /* 6.2 */
    if (g_overlay == OV_CKEYS) { CkClick(h); return; }   /* 6.2 */
    if (g_overlay == OV_QUICK || g_overlay == OV_VITFULL) { Home62OverlayClick(h); return; }   /* 6.2 */
    if (LibOverlayClick(h)) return;   /* 6.1: the Add screen, a custom game's page */
    if (g_overlay == OV_PROFEDIT) {
        if (h == H_PROFSAVE) ProfEditSave();
        else if (h == H_PROFCANCEL) { BOOL w = g_peWelcome; Back(); if (w) WelcomeDone(); }   /* in the welcome: "Not now" */
        else if (h == H_PROFDEL) ProfEditDelete();
        else if (h == H_PHOTO) { GpImage *im = PhotoPick(); if (im) { PeDropPhoto(); g_pePhotoImg = im; g_pePhoto = 1; Snd(SND_OK); } SetFocus(g_wnd); }   /* 4.4.0 */
        else if (h == H_PHOTODEL) { PeDropPhoto(); g_pePhoto = 2; Snd(SND_MOVE); }
        else if (h >= H_PSTAT && h <= H_PSTAT_LAST) { g_peStatus = h - H_PSTAT; Snd(SND_MOVE); }
        else if (h >= H_COLOR && h < H_COLOR + 12) { g_peColor = h - H_COLOR; Snd(SND_MOVE); }
        else if (h >= H_AVATAR && h < H_AVATAR + NAVATARS) { g_peAvatar = h - H_AVATAR; Snd(SND_MOVE); }
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    Snd(SND_OK);
    if (h == H_WELCOMEGO) { g_peWelcome = TRUE; ProfEditOpen(g_curProf); return; }   /* 4.4.0: create your account */
    if (h == H_WELCOMESKIP) { WelcomeDone(); return; }
    if (Home62Click(h)) return;   /* 6.2: the vitrine, the quick options, the live cards, the Logs pill */
    if (LibClick(h)) return;   /* 6.1: the custom tiles, "+ ADICIONAR", the custom game's cards (ui_library.c) */
    if (h >= H_CARD && h < H_CARD + NGAMES) { int g = h - H_CARD; if (g == g_sel && g_csel < 0 && g_view == V_HOME) { if (St(g) == S_READY) OpenGame(); else if (NGPORTG(g) && St(g) == S_NOPORT) ExtInstallMenu(g); else OpenGuide(g); } else Select(g); return; }
    if (h >= H_SYSTILE && h < H_SYSTILE + 3) { int k = h - H_SYSTILE; if (k == 0) OpenGuide(g_sel); else if (k == 1) { g_guideGame = SOON(g_sel) ? 3 : g_sel; SetView(V_ONLINE); } else SetView(V_SETTINGS); return; }
    if (h >= H_HOMECARD && h < H_HOMECARD + 4) { static const int tabs[4] = {TAB_GUIDE, TAB_SAVES, TAB_CHEATS, TAB_ONLINE}, tabs5[4] = {TAB_VIDEO, TAB_EXTRAS, TAB_VIDEO, TAB_EXTRAS};   /* 4.3: the PS5 performance center */
        if (X360G(g_sel)) { if (h - H_HOMECARD == 1) { StartPort(g_sel, FALSE); return; } SetView(V_GAME); g_tab = h - H_HOMECARD == 2 ? TAB_GAME : TAB_OVERVIEW; LoadEdits(); return; }   /* 5.0: UFC Undisputed 3 */
        if (EXTG(g_sel)) {   /* 6.0: 0 how it plays, 1 the game, 2 Zelda's recompile / God of War III's own window, 3 the overview */
            int k = h - H_HOMECARD;
            if (k == 2) { if (ZELDAG(g_sel)) ZeldaRecompile(); else if (NGPORTG(g_sel) && (FNCG(g_sel) || !PortOk(g_sel))) ExtInstallMenu(g_sel); else StartPort(g_sel, FALSE); return; }   /* 6.1 */
            SetView(V_GAME); g_tab = k == 0 && ZELDAG(g_sel) ? TAB_VIDEO : k == 1 ? TAB_GAME : TAB_OVERVIEW; LoadEdits(); return;
        }
        SetView(V_GAME); g_tab = G[g_sel].ps5 ? tabs5[h - H_HOMECARD] : tabs[h - H_HOMECARD]; g_guidePage = 0; LoadEdits(); return; }   /* 4.0.1: Performance opens the setup guide */
    /* 4.0.1: the setup guide (ui_screens.c): "Use" on a row, a row's own text, its pages, the sections it opens */
    if (h >= H_GUIDEROW && h < H_GUIDEROW + 40) {
        GuideRow r[16]; int n = GuideRows(g_sel, g_guidePage, r), i = (h - H_GUIDEROW) % 20;
        if (i < n) { if (h - H_GUIDEROW < 20) GuideUse(g_sel, r[i].f); else Msg(C_SUB, L"%s: %s", T(r[i].f->label), TR(r[i].pt, r[i].en)); }
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    if (h >= H_GUIDEPAGE && h < H_GUIDEPAGE + 4) { g_guidePage = h - H_GUIDEPAGE; InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (h >= H_GUIDEOPEN && h < H_GUIDEOPEN + NTABS) { SaveEdits(); g_tab = h - H_GUIDEOPEN; InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (h == H_GUIDEALL) { GuideApplyAll(g_sel); InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (h >= H_NAV && h < H_NAV + 4) { int k = h - H_NAV; SaveEdits(); if (k == 0) SetView(V_HOME); else if (k == 1) { g_guideGame = SOON(g_sel) ? 3 : g_sel; SetView(V_ONLINE); } else if (k == 2) OpenGuide(g_sel); else SetView(V_SETTINGS); return; }
    if (h >= H_TAB && h < H_TAB + NTABS) { SaveEdits(); g_tab = h - H_TAB; InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (h >= H_SETSEC && h < H_SETSEC + NSETS) { g_setSec = h - H_SETSEC; InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (h >= H_LANGPICK && h < H_LANGPICK + NLANG) { SetLang(h - H_LANGPICK); InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (h >= H_PROFPICK && h < H_PROFPICK + MAXPROF) {
        int i = h - H_PROFPICK;
        if (g_view == V_PROFILES) { ProfileSelect(i); LoadEdits(); Msg(C_GOOD, L2(L"Bem-vindo, %s!", L"Welcome, %s!"), g_prof[i].name); SetView(V_HOME); }
        else ProfEditOpen(i);
        return;
    }
    if (h == H_PROFADD) { ProfEditOpen(-1); return; }
    if (h >= H_CHEAT && h < H_CHEAT + MAXCHEATS) { CheatToggle(g_sel, h - H_CHEAT); InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (h >= H_SAVEITEM && h < H_SAVEITEM + 30) { SavesOpenFolder(g_sel); return; }
    if (h >= H_LINK && h < H_LINK + 20) {
        int i = h - H_LINK; if (!g_nlinks) LoadLinks();
        if (i < g_nlinks && g_links[i].url) OpenUrl(g_links[i].url);
        return;
    }
    if (h >= H_GUIDEGAME && h < H_GUIDEGAME + NGAMES) { g_guideGame = h - H_GUIDEGAME; g_instCheckGame = -1; if (g_view == V_INSTALL) g_guideStep = g_guideStep > 0 ? g_guideStep : 0; InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (h >= H_INSTSTEP && h < H_INSTSTEP + NSTEPS) { g_guideStep = h - H_INSTSTEP; InvalidateRect(g_wnd, NULL, FALSE); return; }
    if (h == H_MSG || h == H_TOAST) { MessageBoxW(g_wnd, g_msg, L"PORTHEON", MB_OK | (g_msgColor == C_BAD || g_msgColor == C_ORANGE ? MB_ICONWARNING : MB_ICONINFORMATION)); return; }
    if (h >= H_DROP && h < H_DROP + 40) { if (g_dropF[h - H_DROP]) { int keep = g_sel; if (g_view == V_ONLINE) g_sel = g_guideGame; DropdownMenu(g_dropF[h - H_DROP], h); g_sel = keep; } return; }
    if (h >= H_FXPICK && h <= H_FXPICK_LAST) { PIniSet(L"ui_effects", h - H_FXPICK); FxTier(); InvalidateRect(g_wnd, NULL, FALSE); return; }   /* 5.0 */
    if (h >= H_THEMEPICK && h < H_THEMEPICK + NTHEMES) {   /* 6.0: PORTHEON's color: the cached pictures are drawn again in it */
        PIniSet(L"ui_theme", h - H_THEMEPICK); ThemeLoad(); ScaleChanged(); Msg(C_GOOD, L2(L"Cor do PORTHEON: %s.", L"PORTHEON's color: %s."), TR(THEMES[g_theme].pt, THEMES[g_theme].en));
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    if (h >= H_SWITCH && h < H_SWITCH + 40) {
        const Field *f = g_swF[h - H_SWITCH]; if (!f) return;
        if (f == &FS_FULL) { SetFullscreen(!g_full); return; }
        if (g_view == V_SETTINGS) { int v = !PIni(f->key + 5, f->def); PIniSet(f->key + 5, v); if (f == &FS_PAD) { if (v) PadStart(); } if (f == &FS_GSKIN) { SkinsLoad(); ScaleChanged(); } InvalidateRect(g_wnd, NULL, FALSE); return; }
        int keep = g_sel; if (g_view == V_ONLINE) g_sel = g_guideGame; ToggleSwitch(f); g_sel = keep; return;
    }
    switch (h) {
    case H_SKIPBOOT: BootDone(); break;
    case H_PROFILE: g_overlay = OV_PROFMENU; g_focus = -1; break;   /* 4.4.0: the profile card */
    case H_PROFMANAGE: g_setSec = SET_PROFILES; SetView(V_SETTINGS); break;
    case H_POWER: PowerMenu(); break;
    case H_FULLSCREEN: SetFullscreen(!g_full); break;   /* 4.0.4: the window button at the top */
    case H_LANGBTN: if (g_view == V_SETTINGS && g_setSec == SET_LANG) ApplyLangToGames(); else { g_setSec = SET_LANG; SetView(V_SETTINGS); } break;
    case H_OPTIONS: OpenGame(); break;
    case H_CONFIG: OpenGame(); break;
    case H_BACK: Back(); break;
    case H_GUIDE: OpenGuide(g_sel); break;
    case H_BROWSE: Browse(); break;
    case H_SCAN: StartScan(TRUE); break;
    case H_VERIFY: { wchar_t d[MAX_PATH], t[900]; GameDir(g_sel, d); GameCheck r; CheckGameFiles(g_sel, d, &r);
                     if (!d[0]) Msg(C_ORANGE, L"%s", T(S_CHOOSEFIRST)); else if (GameCheckText(g_sel, &r, t, 900)) Msg(C_BAD, L"%s", t);
                     else Msg(C_GOOD, L2(L"Os arquivos do %s estão completos.", L"%s's files are complete."), G[g_sel].name); break; }
    case H_OPENGAME: OpenGameFolder(); break;
    case H_SHORTCUT: MakeShortcut(g_sel, FALSE); break;
    case H_STEAM: AddToSteam(g_sel); break;   /* 5.0 */
    case H_KEYS: KeysOpen(g_sel); return;   /* 6.0 */
    case H_PADMAP: PmOpen(g_sel); return;   /* 6.2 */
    case H_PADMAP2: PmOpen2(g_sel); return;   /* Released: Player 2's buttons (UFC 1-3) */
    case H_PADPICK2: Pad2PickMenu(); break;   /* Released: Player 2's controller (UFC 1-3) */
    case H_ENGENV: EngineEnvEdit(g_sel); break;   /* Released: the engine's extra variables (UFC 1-3) */
    case H_RECOMP: ZeldaRecompile(); break;   /* 6.0: Ocarina Recomp */
    case H_EXTINST: ExtInstallMenu(g_sel); break;   /* 6.1: Bloodborne's / inFAMOUS's port */
    case H_EXTCFG: {
        if (NGPORTG(g_sel) && !PortOk(g_sel)) { ExtInstallMenu(g_sel); break; }   /* 6.1 */
        if (EXTG(g_sel) && StartPort(g_sel, FALSE))
            Msg(C_GOOD, ZELDAG(g_sel) ? L2(L"Abrindo o emulador do %s (controles, gráficos, chaves e firmware ficam nele)...", L"Opening %s's emulator (controls, graphics, keys and firmware are set in it)...")
                                      : L2(L"Abrindo o launcher do port do %s (vídeo, controles, mods e preparação do jogo)...", L"Opening the %s port's launcher (video, controls, mods and the game's preparation)..."), G[g_sel].name);
        break; }
    case H_PORTCFG: if (X360G(g_sel) && StartPort(g_sel, FALSE)) Msg(C_GOOD, L2(L"Abrindo a configuração do port do %s (vídeo, controles, saves, mods)...", L"Opening the %s port's settings (video, controls, saves, mods)..."), G[g_sel].name); break;   /* 5.0 */
    case H_BESTCFG: BestSettings(); break;
    case H_REPORT: MakeReport(g_sel); break;   /* 4.3.5 */
    case H_BENCH: StartBench(g_sel); break;
    case H_WARMUP: StartWarmup(g_sel); break;   /* 4.3.7 */
    case H_PCCHECK: PcVerdictBox(g_sel, FALSE); break;   /* 4.3.8 */
    case H_HOSTIP: HostIpMenu(); break;
    case H_PADPICK: { int keep = g_sel; if (g_view == V_ONLINE) g_sel = g_guideGame; PadPickMenu(); g_sel = keep; break; }
    case H_COPYIP: CopyHostIp(); break;
    case H_PARSECDL: ShellExecuteW(g_wnd, L"open", L"https://parsec.app/downloads", NULL, NULL, SW_SHOWNORMAL); break;
    case H_ARENA: ArenaOpen(g_view == V_ONLINE ? g_guideGame : g_sel); break;   /* 7.2 */
    case H_ARENATOP: ArenaOpen(g_view == V_ONLINE ? g_guideGame : g_sel); break;   /* 7.3: the top bar */
    case H_SAVEBACKUP: SavesBackupNow(g_sel); break;
    case H_SAVERESTORE: SavesRestore(g_sel); break;
    case H_SAVEIMPORT: SavesImport(g_sel); break;   /* Released (11-13) */
    case H_SAVEOPEN: SavesOpenFolder(g_sel); break;
    case H_SAVEDELBAK: SavesOpenBackups(g_sel); break;
    case H_SAVEAUTO: PIniSet(L"save_autobackup", !PIni(L"save_autobackup", 1)); break;
    case H_CHEATSOFF: CheatsAllOff(g_sel); break;
    case H_UPDCHECK: if (g_updState >= 1) g_overlay = OV_UPDATE; else StartUpdateCheck(TRUE); break;
    case H_CHANGELOG: g_overlay = OV_NEWS; break;
    case H_UPDATE: g_overlay = OV_UPDATE; break;
    case H_INSTPICK: Select(g_guideGame); Browse(); g_instCheckGame = -1; if (St(g_guideGame) == S_READY && g_guideStep == 3) g_guideStep = 4; break;
    case H_INSTSCAN: StartScan(TRUE); break;
    case H_INSTVERIFY: InstallVerify(g_guideGame); Msg(g_instCheckBad ? C_ORANGE : C_GOOD, L"%s", g_instCheck); break;
    case H_INSTNEXT: if (g_guideStep < NSTEPS - 1) g_guideStep++; else { Select(g_guideGame); SetView(V_HOME); } break;
    case H_INSTPREV: if (g_guideStep > 0) g_guideStep--; break;
    case H_CRED_AUTHOR: OpenUrl(URL_AUTHOR); break;
    case H_CRED_DISCORD: OpenUrl(URL_DISCORD); break;
    case H_ADV: SaveEdits();
        if (g_proc && g_procGame == g_sel) Msg(C_SUB, L"%s", L2(L"O jogo está aberto: feche o jogo primeiro.", L"The game is running: close it first."));
        else AdvMenu();
        break;
    case H_LOGS: OpenLogs(); break;
    case H_PLAY: if (!g_proc && QuietInput()) break; Snd(SND_PLAY); Play(); break;
    }
    InvalidateRect(g_wnd, NULL, FALSE);
}
