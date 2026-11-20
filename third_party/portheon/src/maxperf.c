/* maxperf.c - "every game uses everything the PC has" (2026-11-13). Included by portheon.c.

   Two things, both for the game PORTHEON starts and nothing else on the PC:

   1. The PLAY record: %LOCALAPPDATA%\PORTHEON\play-<game>.txt, written again at every PLAY, one line per thing that was
      applied to the game (and read back where Windows lets it be read): the engine's priority class, EcoQoS, its
      processor mask, the worker counts, the Vulkan layers left out, the graphics card, the power source, PORTHEON's own
      state while the game runs. It goes into the report file with the game's log.

   2. The graphics card on a PC with two (a laptop's integrated card + the dedicated one): Windows' own per-program
      preference, the value Settings > System > Display > Graphics writes
      (HKCU\Software\Microsoft\DirectX\UserGpuPreferences, "<the engine's exe>" = "GpuPreference=2;" = high performance).
      UFC5.exe has done this since 4.0; now the engines of UFC 1-4 get it too. Per user, per exe, no administrator, and
      only where it can matter: Windows must list two or more real graphics cards (DXGI), and a preference the player
      already chose for that exe is never changed. gpu_pref=0 in portheon.ini turns it off and takes PORTHEON's value
      away again (only one PORTHEON wrote: gpu_pref_set_<game>=1). gpu_pref=2 writes it on a one-card PC too (a test).
      On a one-card PC nothing is written. Whether a Vulkan engine follows it depends on the card's driver (NVIDIA's
      Optimus layer and AMD's switchable-graphics layer read this preference); not provable on a one-card PC. */
#include <dxgi.h>

static wchar_t g_mpLog[MAX_PATH];
static void MpLogOpen(int g)
{
    wchar_t d[MAX_PATH], f[64]; LocalData(d, L"PORTHEON"); _snwprintf(f, 64, L"play-%s.txt", G[g].id); f[63] = 0; Join(g_mpLog, d, f);
    HANDLE h = CreateFileW(g_mpLog, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { g_mpLog[0] = 0; return; }
    DWORD w; WriteFile(h, "\xEF\xBB\xBF", 3, &w, NULL); CloseHandle(h);
}
static void MpLog(const wchar_t *fmt, ...)
{
    if (!g_mpLog[0]) return;
    wchar_t t[1500]; SYSTEMTIME st; GetLocalTime(&st);
    int n = _snwprintf(t, 16, L"%02d:%02d:%02d ", st.wHour, st.wMinute, st.wSecond);
    va_list ap; va_start(ap, fmt); _vsnwprintf(t + n, 1500 - n - 3, fmt, ap); va_end(ap); t[1496] = 0; wcscat(t, L"\r\n");
    char u[4600]; int len = WideCharToMultiByte(CP_UTF8, 0, t, -1, u, sizeof(u), NULL, NULL);
    HANDLE h = CreateFileW(g_mpLog, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w; if (len > 1) WriteFile(h, u, (DWORD)(len - 1), &w, NULL); CloseHandle(h);
}

/* the real graphics cards Windows lists (no software adapter, no Microsoft Basic Render Driver); names: "A; B" */
static int MpDxgiCards(wchar_t *names, int n)
{
    static const GUID iidF1 = {0x770aae78, 0xf26f, 0x4dba, {0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87}};
    int cards = 0; if (names && n) names[0] = 0;
    HMODULE dx = LoadLibraryExW(L"dxgi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32); if (!dx) return 0;
    HRESULT (WINAPI *create)(const IID *, void **) = (HRESULT (WINAPI *)(const IID *, void **))(void *)GetProcAddress(dx, "CreateDXGIFactory1");
    IDXGIFactory1 *f = NULL;
    if (create && SUCCEEDED(create(&iidF1, (void **)&f)) && f) {
        IDXGIAdapter1 *a = NULL; UINT seen[8][3]; int ns = 0;   /* by model, not by adapter: a virtual display adapter (Parsec, an indirect display driver) lists the same card a second time */
        for (UINT i = 0; i < 16 && f->lpVtbl->EnumAdapters1(f, i, &a) == S_OK; i++) {
            DXGI_ADAPTER_DESC1 d;
            if (SUCCEEDED(a->lpVtbl->GetDesc1(a, &d)) && !(d.Flags & 2 /* DXGI_ADAPTER_FLAG_SOFTWARE */) && !(d.VendorId == 0x1414 && d.DeviceId == 0x8c)) {
                BOOL dup = FALSE; for (int k = 0; k < ns; k++) if (seen[k][0] == d.VendorId && seen[k][1] == d.DeviceId && seen[k][2] == d.SubSysId) dup = TRUE;
                if (!dup && ns < 8) {
                    seen[ns][0] = d.VendorId; seen[ns][1] = d.DeviceId; seen[ns][2] = d.SubSysId; ns++; cards++;
                    if (names && (int)(wcslen(names) + wcslen(d.Description) + 4) < n) { if (names[0]) wcscat(names, L"; "); wcscat(names, d.Description); }
                }
            }
            a->lpVtbl->Release(a);
        }
        f->lpVtbl->Release(f);
    }
    FreeLibrary(dx); return cards;
}
#define MP_GPUKEY L"Software\\Microsoft\\DirectX\\UserGpuPreferences"
/* 0: nothing to do (one card)  1: written now  2: already high performance  3: the player's own choice kept
   4: PORTHEON's value taken away (option off)  5: option off, nothing of PORTHEON's there  -1: the registry refused.
   now: the value as it is in the registry after this call ("" = none). */
static int MpGpuPref(int g, const wchar_t *exe, int cards, wchar_t *now, int nn)
{
    int opt = PIni(L"gpu_pref", 1), mine = GIni(L"gpu_pref_set", g, 0), r; HKEY k; wchar_t cur[512] = L""; DWORD cb = sizeof(cur) - 2, type = 0;
    now[0] = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, MP_GPUKEY, 0, NULL, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS) return -1;
    BOOL has = RegQueryValueExW(k, exe, NULL, &type, (BYTE *)cur, &cb) == ERROR_SUCCESS && type == REG_SZ; if (!has) cur[0] = 0;
    cur[511] = 0;
    wchar_t *gp = wcsstr(cur, L"GpuPreference=");
    if (!opt) {   /* off: only PORTHEON's own value goes */
        r = 5;
        if (mine && gp && gp[14] == L'2') {
            wchar_t *e = wcschr(gp, L';'); e = e ? e + 1 : gp + wcslen(gp); memmove(gp, e, (wcslen(e) + 1) * sizeof(wchar_t));
            if (cur[0]) RegSetValueExW(k, exe, 0, REG_SZ, (const BYTE *)cur, (DWORD)((wcslen(cur) + 1) * sizeof(wchar_t))); else RegDeleteValueW(k, exe);
            r = 4;
        }
        if (mine) GIniSet(L"gpu_pref_set", g, -1);
    }
    else if (gp) r = gp[14] == L'2' ? 2 : 3;
    else if (cards < 2 && opt != 2) r = 0;
    else {
        if (wcslen(cur) + 20 < 512) wcscat(cur, L"GpuPreference=2;");   /* next to what is there (Windows 11 keeps other per-program switches in the same value) */
        r = RegSetValueExW(k, exe, 0, REG_SZ, (const BYTE *)cur, (DWORD)((wcslen(cur) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS ? 1 : -1;
        if (r == 1) GIniSet(L"gpu_pref_set", g, 1);
    }
    cb = sizeof(cur) - 2; cur[0] = 0;
    if (RegQueryValueExW(k, exe, NULL, &type, (BYTE *)cur, &cb) == ERROR_SUCCESS && type == REG_SZ) { cur[511] = 0; wcsncpy(now, cur, nn - 1); now[nn - 1] = 0; }
    RegCloseKey(k); return r;
}
static const wchar_t *MpPrioName(DWORD c)
{
    return c == IDLE_PRIORITY_CLASS ? L"idle" : c == BELOW_NORMAL_PRIORITY_CLASS ? L"below normal" : c == NORMAL_PRIORITY_CLASS ? L"normal" : c == ABOVE_NORMAL_PRIORITY_CLASS ? L"above normal"
         : c == HIGH_PRIORITY_CLASS ? L"high" : c == REALTIME_PRIORITY_CLASS ? L"realtime" : L"?";
}
static int PowerState(void);
/* at PLAY, with the game's environment already set (just before the game's process is created) */
static void MpPlayRecord(int g)
{
    MpLogOpen(g);
    MpLog(L"PORTHEON %s: PLAY %s (%s)", APP_VER, G[g].name, G[g].id);
    { size_t cl = wcslen(g_cpuName); while (cl && g_cpuName[cl - 1] == L' ') g_cpuName[--cl] = 0; }   /* the CPUID brand string is padded */
    MpLog(L"PC: %s, %d threads, %d GB RAM; %s (%llu MB); monitor %dx%d at %d Hz", g_cpuName, g_threads, g_ramGb, g_haveGpu ? g_gpu.name : L"graphics card not read",
          g_haveGpu ? (unsigned long long)(g_gpu.vram >> 20) : 0ull, g_monW, g_monH, g_monHz);
    {   int pw = PowerState(); MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms);
        MpLog(L"Power: %s%s%s (PORTHEON changes no power plan; it warns before PLAY on battery, battery saver or the Power saver plan)", pw & 1 ? L"battery" : L"AC", pw & 2 ? L", battery saver on" : L"", pw & 4 ? L", Power saver plan" : L"");
        MpLog(L"Memory at PLAY: %llu MB RAM free of %llu, %llu MB of commit free", ms.ullAvailPhys >> 20, ms.ullTotalPhys >> 20, ms.ullAvailPageFile >> 20); }
    if (PS4G(g)) {
        wchar_t names[300], dir[MAX_PATH], exe[MAX_PATH], now[512]; int cards = MpDxgiCards(names, 300);
        PortDir(g, dir); Join(exe, dir, L"bin\\sacredfire-game.exe");
        int r = MpGpuPref(g, exe, cards, now, 512);
        MpLog(L"Graphics cards Windows lists: %d (%s)", cards, names[0] ? names : L"none read");
        MpLog(L"GPU preference (Windows' per-program setting, gpu_pref=%d): %s; value for %s now: %s", PIni(L"gpu_pref", 1),
              r == 0 ? L"not needed, one graphics card" : r == 1 ? L"set to high performance" : r == 2 ? L"already high performance" : r == 3 ? L"the player's own choice kept"
              : r == 4 ? L"off, PORTHEON's value removed" : r == 5 ? L"off" : L"the registry refused", exe, now[0] ? now : L"(none)");
        MpLog(L"Engine process: the engine sets above normal priority, EcoQoS off and a 1 ms timer for itself at start; PORTHEON checks it when the engine appears (engine_boost=%d)", PIni(L"engine_boost", 1));
        MpLog(L"Processor: all %d threads%s; V-Cache die mask 0x%llx (x3d_ccd=%d; 0 = no two-die X3D processor, nothing pinned)", g_threads, g_threads < 6 ? L" (under 6: the engine's own worker counts)" : L"",
              (unsigned long long)VCacheMask(), PIni(L"x3d_ccd", 1));
    }
    static const wchar_t *const ev[] = {L"BB_PREP_WORKERS", L"BB_COPY_THREADS", L"BB_ASYNC_PIPELINES", L"BB_ASYNC_PIPELINE_THREADS", L"BB_DRIVER_PIPELINE_CACHE", L"BB_VK_RECORD_THREAD", L"BB_DRAW_PIPE",
                                        L"BB_FRAME_STATS", L"BB_FPS_LIMIT", L"BB_UPSCALER", L"BB_WRITE_WINDOW", L"BB_TOUCH_COMMIT", L"BB_DEFERRED_READBACK", L"VK_LOADER_LAYERS_DISABLE", L"DISABLE_RTSS_LAYER",
                                        L"DISABLE_VK_LAYER_VALVE_steam_fossilize_1", L"DISABLE_VK_LAYER_VALVE_steam_overlay_1", L"DISABLE_VK_LAYER_reshade_1", L"VK_INSTANCE_LAYERS"};
    if (G[g].ps5) MpLog(L"%s has its own launcher (games\\%s): the engine's priority, EcoQoS, processor mask, Vulkan layers and GPU preference are set and logged by it, in the session log of the game (the Logs button)", G[g].name, G[g].id);
    else for (int i = 0; i < N(ev); i++) { wchar_t v[600]; DWORD n = GetEnvironmentVariableW(ev[i], v, 600); if (n && n < 600) MpLog(L"Game environment: %s=%s", ev[i], v); }
    if (PS4G(g)) {
        wchar_t fsv[8]; BOOL fs = GetEnvironmentVariableW(L"BB_FRAME_STATS", fsv, 8) != 0;
        MpLog(L"Engine's 5-second frame report (BB_FRAME_STATS): %s", fs ? L"ON (the frame_stats option or a variable of the player's): it costs one long frame every few seconds in a fight" : L"off");
    }
    MpLog(L"PORTHEON while the game runs: below normal priority, EcoQoS on, low memory priority, its timers stopped; close_on_play=%d", PIni(L"close_on_play", 1));
}
/* BoostThread, when it has found the engine: before = the class it had when found */
static void MpEngineRecord(DWORD pid, HANDLE h, DWORD before, int eco)
{
    DWORD_PTR pm = 0, sm = 0; GetProcessAffinityMask(h, &pm, &sm);
    MpLog(L"Engine process %lu found: priority class %s -> %s (read back); EcoQoS off: %s; timer resolution kept when hidden: %s; processor mask 0x%llx of 0x%llx", pid, MpPrioName(before),
          MpPrioName(GetPriorityClass(h)), eco ? L"accepted by Windows" : L"refused by Windows", eco == 3 ? L"yes" : L"no (Windows 11 only)", (unsigned long long)pm, (unsigned long long)sm);
}
/* FreezeWatch, once: the card the engine itself names in its log ("GPU: NVIDIA GeForce RTX 4060, 7957 MiB VRAM; ...") */
static BOOL MpEngineGpu(int g, const char *buf)
{
    const char *v = strstr(buf, " MiB VRAM"); if (!v) return FALSE;
    const char *s = v; while (s > buf && s[-1] != '\n') s--;
    if (strncmp(s, "GPU: ", 5)) return FALSE;
    s += 5; const char *e = s; while (e < v && *e != ',') e++;
    char nm[200]; size_t n = (size_t)(e - s); if (!n || n >= sizeof(nm)) return FALSE;
    memcpy(nm, s, n); nm[n] = 0;
    wchar_t w[200]; MultiByteToWideChar(CP_UTF8, 0, nm, -1, w, 200); w[199] = 0;
    BOOL same = !g_haveGpu || StrStrIW(w, g_gpu.name) || StrStrIW(g_gpu.name, w);
    MpLog(L"Graphics card the engine runs on (its own log): %s%s%s", w, same ? L"" : L" - NOT the best card PORTHEON found: ", same ? L"" : g_gpu.name);
    {   wchar_t k[48]; _snwprintf(k, 48, L"gpu_other_%s", G[g].id); k[47] = 0;
        WritePrivateProfileStringW(L"PORTHEON", k, same ? NULL : w, g_pIni); }   /* the next PLAY of this game says it (PlayRoomCheck) */
    return TRUE;
}
