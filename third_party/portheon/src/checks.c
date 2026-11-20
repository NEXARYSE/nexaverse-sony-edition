/* checks.c - part of portheon.c (4.0): what this PC and a game folder need before PLAY, said in words.
 * Players' logs of 2026-10-22/24 and their "it opens and closes in my face", "error 0xC0000135", "code 3 at once":
 *  - the engine did not start at all (0xC0000135): Windows could not load one of the DLLs it imports. The usual ones:
 *    vulkan-1.dll (no video driver with Vulkan: a fresh Windows, the Microsoft Basic Display Adapter, a "lite" Windows),
 *    api-ms-win-core-memory-l1-1-7.dll (CreateFileMapping2, Windows 10 1809+) or a port file an antivirus took;
 *  - the engine started and stopped at once: a graphics card or driver without Vulkan 1.3 or one of the extensions
 *    the renderer cannot do without (it asserts on them);
 *  - the game crashed at its first screens: a game folder that is only the update ("CUSA01968-patch", no base game:
 *    the UFC 2 crash at 0xa1cafe), or .big archives cut short by a download ("Failed to mount bigfile", UFC 4).
 * Each one is checked before PLAY (fast: the engine's import table, one Vulkan instance, 32 bytes of each archive)
 * and named in the message, with what to do. A check that finds something asks before it blocks a game. */

/* ---------------------------------------------------------------- Windows */
static DWORD WinBuild(void)
{
    static DWORD b; if (b) return b;
    OSVERSIONINFOW ov = {sizeof(ov)};
    typedef LONG (WINAPI *RGV)(OSVERSIONINFOW *); RGV rgv = (RGV)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    if (rgv && !rgv(&ov)) b = ov.dwBuildNumber;
    return b ? b : 1;
}

/* ---------------------------------------------------------------- the engine's imports
   The DLLs (and their functions) bin\sacredfire-game.exe and the port's own DLLs import, from their PE import tables.
   One the bin folder has is the port's; any other must load from Windows, and each function must be in it. */
typedef struct { char dll[64], fn[64]; int kind; } ImpMiss;   /* kind 1: the DLL, 2: a function in it, 3: a port DLL missing */
static BYTE *ReadWhole(const wchar_t *p, DWORD *n)
{
    HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(h, NULL), got = 0; BYTE *b = NULL;
    if (sz && sz < (64u << 20) && (b = (BYTE *)malloc(sz))) { if (!ReadFile(h, b, sz, &got, NULL) || got != sz) { free(b); b = NULL; } }
    CloseHandle(h); if (n) *n = b ? sz : 0; return b;
}
static DWORD RvaOff(const BYTE *b, DWORD n, DWORD rva)   /* a PE's RVA as a file offset (0: outside every section) */
{
    const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)(b + ((const IMAGE_DOS_HEADER *)b)->e_lfanew);
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++)
        if (rva >= s[i].VirtualAddress && rva < s[i].VirtualAddress + s[i].Misc.VirtualSize) { DWORD o = rva - s[i].VirtualAddress + s[i].PointerToRawData; return o < n ? o : 0; }
    return 0;
}
/* the first problem among pe's imports; binDir: the port's bin folder (its DLLs are checked by name only) */
static BOOL PeImportProblem(const wchar_t *pe, const wchar_t *binDir, ImpMiss *m)
{
    DWORD n = 0; BYTE *b = ReadWhole(pe, &n); BOOL bad = FALSE;
    if (!b) return FALSE;
    if (n < 0x400 || b[0] != 'M' || b[1] != 'Z') { free(b); return FALSE; }
    DWORD lfa = ((IMAGE_DOS_HEADER *)b)->e_lfanew;
    if (lfa + sizeof(IMAGE_NT_HEADERS64) > n) { free(b); return FALSE; }
    const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)(b + lfa);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) { free(b); return FALSE; }
    DWORD irva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress, io = irva ? RvaOff(b, n, irva) : 0;
    for (const IMAGE_IMPORT_DESCRIPTOR *d = (const IMAGE_IMPORT_DESCRIPTOR *)(b + io); io && (BYTE *)(d + 1) <= b + n && d->Name && !bad; d++) {
        DWORD no = RvaOff(b, n, d->Name); if (!no) break;
        char dll[64]; strncpy(dll, (const char *)b + no, 63); dll[63] = 0;
        wchar_t wd[64], local[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, dll, -1, wd, 64); Join(local, binDir, wd);
        if (FileExists(local)) continue;   /* the port's own DLL: its file is there */
        /* the port's own DLLs must be in bin (Windows has no SDL3, libc++, libunwind or winpthread of its own) */
        if (!_stricmp(dll, "SDL3.dll") || !_stricmp(dll, "libc++.dll") || !_stricmp(dll, "libunwind.dll") || !_stricmp(dll, "libwinpthread-1.dll")) {
            strcpy(m->dll, dll); m->fn[0] = 0; m->kind = 3; bad = TRUE; break; }
        HMODULE h = LoadLibraryExW(wd, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!h) { strcpy(m->dll, dll); m->fn[0] = 0; m->kind = 1; bad = TRUE; break; }
        DWORD to = RvaOff(b, n, d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk);
        for (const ULONGLONG *t = (const ULONGLONG *)(b + to); to && (BYTE *)(t + 1) <= b + n && *t; t++) {
            if (*t & IMAGE_ORDINAL_FLAG64) continue;
            DWORD fo = RvaOff(b, n, (DWORD)(*t & 0x7fffffff)); if (!fo || fo + 2 >= n) continue;
            const char *fn = (const char *)b + fo + 2;
            if (!GetProcAddress(h, fn)) { strcpy(m->dll, dll); strncpy(m->fn, fn, 63); m->fn[63] = 0; m->kind = 2; bad = TRUE; break; }
        }
        FreeLibrary(h);
    }
    free(b); return bad;
}
/* the engine and every DLL of the port's bin folder; cached per game until the engine file changes */
static BOOL EngineImportProblem(int g, ImpMiss *m)
{
    static FILETIME seen[NGAMES]; static BOOL had[NGAMES]; static ImpMiss last[NGAMES];
    wchar_t d[MAX_PATH], bin[MAX_PATH], exe[MAX_PATH]; PortDir(g, d); Join(bin, d, L"bin"); Join(exe, bin, L"sacredfire-game.exe");
    WIN32_FILE_ATTRIBUTE_DATA fa; if (!GetFileAttributesExW(exe, GetFileExInfoStandard, &fa)) return FALSE;
    if (!CompareFileTime(&fa.ftLastWriteTime, &seen[g]) && seen[g].dwLowDateTime | seen[g].dwHighDateTime) { if (had[g]) *m = last[g]; return had[g]; }
    seen[g] = fa.ftLastWriteTime;
    /* 6.0 final: a clean result is kept in portheon.ini (the engine's size and time and the Windows build): the 21-23 MB
       import scan ran in every PLAY instance. A crash with a missing DLL clears it (DllDiagnosis) */
    wchar_t ik[32], sig[64], old[64]; _snwprintf(ik, 32, L"imp_ok_%s", G[g].id);
    _snwprintf(sig, 64, L"%lu-%08lx%08lx-%lu", fa.nFileSizeLow, fa.ftLastWriteTime.dwHighDateTime, fa.ftLastWriteTime.dwLowDateTime, WinBuild());
    GetPrivateProfileStringW(L"PORTHEON", ik, L"", old, 64, g_pIni);
    if (!wcscmp(old, sig)) { had[g] = FALSE; return FALSE; }
    BOOL bad = PeImportProblem(exe, bin, m);
    static const wchar_t *const dlls[] = {L"SDL3.dll", L"libc++.dll", L"libunwind.dll", L"libwinpthread-1.dll"};
    for (int i = 0; i < 4 && !bad; i++) { wchar_t p[MAX_PATH]; Join(p, bin, dlls[i]); if (FileExists(p)) bad = PeImportProblem(p, bin, m); }
    had[g] = bad; if (bad) last[g] = *m;
    WritePrivateProfileStringW(L"PORTHEON", ik, bad ? NULL : sig, g_pIni);
    return bad;
}

/* ---------------------------------------------------------------- the graphics card: what the renderer requires
   Vulkan 1.3, VK_KHR_swapchain, VK_KHR_push_descriptor, VK_EXT/KHR_vertex_attribute_divisor and VK_EXT/KHR_robustness2
   with robustBufferAccess2, robustImageAccess2 and nullDescriptor (shadPS4's vk_instance.cpp asserts on each). */
/* 5.2: drv, the driver's own version as its maker writes it (NVIDIA "581.57", AMD "26.10.1", Intel "101.6083"), from
   VkPhysicalDeviceDriverProperties.driverInfo on Vulkan 1.2+ (driverID in drvId), else decoded from driverVersion per
   vendor; drvNum is its leading digits and dots, what the remote list's drv< / drv>= / drv= compare (remote.c) */
typedef struct { BOOL done, loader, device, bary; uint32_t api, vendor, devid, driver, drvId; wchar_t name[256], miss[200], drv[64]; char drvNum[32]; } VkCheck;   /* 4.3.8: bary, vendor, devid, driver (PcVerdict) */
static VkCheck g_vkc;
static volatile LONG g_vkcReady;   /* 7.1 review: TRUE once VulkanCheck's probe has finished (the Video tab reads g_vkc.bary without waiting) */
static void DriverDecode(uint32_t vendor, uint32_t v, char *out, int n)   /* driverVersion packed the vendor's way */
{
    if (vendor == 0x10DE) _snprintf(out, n, "%u.%02u", v >> 22, (v >> 14) & 0xff);          /* NVIDIA: 10.8.6.8 bits */
    else if (vendor == 0x8086) _snprintf(out, n, "%u.%u", v >> 14, v & 0x3fff);             /* Intel on Windows: 18.14 bits */
    else _snprintf(out, n, "%u.%u.%u", v >> 22, (v >> 12) & 0x3ff, v & 0xfff);              /* AMD and others: VK_MAKE_VERSION */
    out[n - 1] = 0;
}
static const wchar_t *VendorName(uint32_t v) { return v == 0x10DE ? L"NVIDIA" : v == 0x1002 ? L"AMD" : v == 0x8086 ? L"Intel" : L""; }
static DWORD WINAPI VulkanCheckProbe(LPVOID unused);
static void VulkanCheck(void)   /* 4.3.3: on a guarded thread (RunGuarded, see DetectPc) */
{
    if (g_vkc.done) return;
    memset(&g_vkc, 0, sizeof(g_vkc)); g_vkc.done = TRUE;
    if (!RunGuarded(VulkanCheckProbe, 15000)) { BOOL loader = g_vkc.loader; memset(&g_vkc, 0, sizeof(g_vkc)); g_vkc.done = TRUE; g_vkc.loader = loader; g_vkc.device = TRUE; g_vkc.api = (1u << 22) | (3u << 12); }   /* unreadable: no warning, the game tries */
    InterlockedExchange(&g_vkcReady, 1);
}
/* 6.1: a card was read and its Vulkan is older than need (extgames.c ExtPreflight) */
static BOOL VulkanApiBelow(uint32_t need) { VulkanCheck(); return g_vkc.loader && g_vkc.device && g_vkc.api < need; }
static DWORD WINAPI VulkanCheckProbe(LPVOID unused)
{
    (void)unused;
    HMODULE vk = LoadLibraryExW(L"vulkan-1.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!vk) return 0;
    g_vkc.loader = TRUE;
    PFN_GIPA_ gipa = (PFN_GIPA_)(void *)GetProcAddress(vk, "vkGetInstanceProcAddr");
    int (WINAPI *create)(const VkInstInfo__ *, const void *, void **) = gipa ? (int (WINAPI *)(const VkInstInfo__ *, const void *, void **))gipa(NULL, "vkCreateInstance") : NULL;
    VkAppInfo__ ai = {0, NULL, "PORTHEON", 1, "none", 1, (1u << 22) | (3u << 12)}; VkInstInfo__ ii = {1, NULL, 0, &ai, 0, NULL, 0, NULL}; void *inst = NULL;
    if (!create || create(&ii, NULL, &inst) != 0 || !inst) { FreeLibrary(vk); return 0; }
    int (WINAPI *enumDev)(void *, uint32_t *, void **) = (int (WINAPI *)(void *, uint32_t *, void **))gipa(inst, "vkEnumeratePhysicalDevices");
    void (WINAPI *props)(void *, void *) = (void (WINAPI *)(void *, void *))gipa(inst, "vkGetPhysicalDeviceProperties");
    typedef struct { char name[256]; uint32_t spec; } ExtProp;
    int (WINAPI *devExt)(void *, const char *, uint32_t *, ExtProp *) = (int (WINAPI *)(void *, const char *, uint32_t *, ExtProp *))gipa(inst, "vkEnumerateDeviceExtensionProperties");
    void (WINAPI *feat2)(void *, void *) = (void (WINAPI *)(void *, void *))gipa(inst, "vkGetPhysicalDeviceFeatures2");
    void (WINAPI *props2)(void *, void *) = (void (WINAPI *)(void *, void *))gipa(inst, "vkGetPhysicalDeviceProperties2");   /* 5.2: the driver's own version */
    void (WINAPI *destroy)(void *, const void *) = (void (WINAPI *)(void *, const void *))gipa(inst, "vkDestroyInstance");
    uint32_t n = 8; void *devs[8]; int best = -1, bestScore = -1;
    static unsigned char buf[8192];
    if (enumDev && props && enumDev(inst, &n, devs) >= 0) {
        for (uint32_t i = 0; i < n && i < 8; i++) {
            memset(buf, 0, sizeof(buf)); props(devs[i], buf);
            uint32_t api = *(uint32_t *)buf, type = *(uint32_t *)(buf + 16);
            if (type == 4) continue;   /* a CPU (software) device is no game card */
            /* what the engine picks: a device with the API it needs first, then a discrete card */
            int score = (api >= ((1u << 22) | (3u << 12)) ? 4 : 0) + (type == 2 ? 2 : type == 1 ? 1 : 0);
            if (score > bestScore) { bestScore = score; best = (int)i; }
        }
    }
    if (best >= 0) {
        void *dv = devs[best]; memset(buf, 0, sizeof(buf)); props(dv, buf);
        g_vkc.device = TRUE; g_vkc.api = *(uint32_t *)buf; MultiByteToWideChar(CP_UTF8, 0, (char *)buf + 20, -1, g_vkc.name, 256);
        g_vkc.driver = *(uint32_t *)(buf + 4); g_vkc.vendor = *(uint32_t *)(buf + 8); g_vkc.devid = *(uint32_t *)(buf + 12);
        {   /* 5.2: VkPhysicalDeviceDriverProperties (sType 1000196000, core in 1.2) chained into VkPhysicalDeviceProperties2
               (1000059001): { sType, pNext, driverID, driverName[256], driverInfo[256], conformanceVersion } */
            static struct { int sType; void *pNext; uint32_t id; char name[256], info[256]; uint8_t conf[4]; } dp;
            static struct { int sType; void *pNext; unsigned char props[1024]; } p2;
            char info[64] = "";
            if (props2 && g_vkc.api >= ((1u << 22) | (2u << 12))) {
                memset(&dp, 0, sizeof(dp)); memset(&p2, 0, sizeof(p2)); dp.sType = 1000196000; p2.sType = 1000059001; p2.pNext = &dp;
                props2(dv, &p2); dp.info[255] = 0; g_vkc.drvId = dp.id;
                const char *s = dp.info; while (*s == ' ') s++; strncpy(info, s, 63); info[63] = 0;
            }
            if (info[0] >= '0' && info[0] <= '9') {
                int k = 0; while (k < 31 && ((info[k] >= '0' && info[k] <= '9') || info[k] == '.')) { g_vkc.drvNum[k] = info[k]; k++; }
                g_vkc.drvNum[k] = 0; MultiByteToWideChar(CP_UTF8, 0, info, -1, g_vkc.drv, 64);
            } else { DriverDecode(g_vkc.vendor, g_vkc.driver, g_vkc.drvNum, 32); MultiByteToWideChar(CP_UTF8, 0, g_vkc.drvNum, -1, g_vkc.drv, 64); }
            g_vkc.drv[63] = 0;
        }
        BOOL sw = FALSE, pd = FALSE, div = FALSE, rob = FALSE, bar = FALSE;
        uint32_t ne = 0;
        if (devExt && devExt(dv, NULL, &ne, NULL) >= 0 && ne && ne < 8192) {
            ExtProp *ep = (ExtProp *)calloc(ne, sizeof(ExtProp));
            if (ep && devExt(dv, NULL, &ne, ep) >= 0)
                for (uint32_t k = 0; k < ne; k++) {
                    const char *e = ep[k].name;
                    if (!strcmp(e, "VK_KHR_swapchain")) sw = TRUE;
                    else if (!strcmp(e, "VK_KHR_push_descriptor")) pd = TRUE;
                    else if (!strcmp(e, "VK_EXT_vertex_attribute_divisor") || !strcmp(e, "VK_KHR_vertex_attribute_divisor")) div = TRUE;
                    else if (!strcmp(e, "VK_EXT_robustness2") || !strcmp(e, "VK_KHR_robustness2")) rob = TRUE;
                    else if (!strcmp(e, "VK_KHR_fragment_shader_barycentric") || !strcmp(e, "VK_NV_fragment_shader_barycentric")) bar = TRUE;
                }
            free(ep);
        }
        if (rob && feat2) {   /* VkPhysicalDeviceRobustness2FeaturesEXT { sType, pNext, robustBufferAccess2, robustImageAccess2, nullDescriptor } */
            struct { int sType; void *pNext; uint32_t f[3]; } r2 = {1000286000, NULL, {0, 0, 0}};
            struct { int sType; void *pNext; uint32_t f[64]; } f2; memset(&f2, 0, sizeof(f2)); f2.sType = 1000059000; f2.pNext = &r2;
            feat2(dv, &f2);
            if (!r2.f[0] || !r2.f[1] || !r2.f[2]) rob = FALSE;
        }
        /* 4.3.8: what UFC 5's and UFC 6's main engines require (VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR, the
           same structure type as NV's): RX 5000 and GTX 10 cards lack it */
        if (bar && feat2) {
            struct { int sType; void *pNext; uint32_t f; } fb = {1000203000, NULL, 0};
            struct { int sType; void *pNext; uint32_t f[64]; } f2; memset(&f2, 0, sizeof(f2)); f2.sType = 1000059000; f2.pNext = &fb;
            feat2(dv, &f2); bar = fb.f != 0;
        }
        g_vkc.bary = bar;
        wchar_t *mm = g_vkc.miss;
        if (!sw) wcscat(mm, L"VK_KHR_swapchain, ");
        if (!pd) wcscat(mm, L"VK_KHR_push_descriptor, ");
        if (!div) wcscat(mm, L"vertex_attribute_divisor, ");
        if (!rob) wcscat(mm, L"robustness2, ");
        size_t l = wcslen(mm); if (l >= 2) mm[l - 2] = 0;
    }
    if (destroy) destroy(inst, NULL);
    FreeLibrary(vk);
    return 0;
}

/* ---------------------------------------------------------------- the PC before PLAY: Windows, the driver, the engine's files */
static int g_pcProblem;   /* the last check: 0 fine, 1 Windows, 2 no Vulkan loader, 3 no Vulkan card, 4 Vulkan too old, 5 missing feature, 6 a port DLL, 7 a Windows DLL */
static BOOL PcProblemText(int g, wchar_t *out, int n)
{
    out[0] = 0; g_pcProblem = 0;
    DWORD b = WinBuild();
    if (b < 17763) {   /* Windows 10 1809: CreateFileMapping2; the engine's manifest wants 1903 (18362) for UTF-8 */
        _snwprintf(out, n, L2(L"Este Windows é antigo demais para os ports (versão %lu). O UFC 1 a 4 precisa do Windows 10 versão 1903 ou mais novo, ou do Windows 11: atualize pelo Windows Update (ou instale uma versão atual do Windows 10/11).",
                              L"This Windows is too old for the ports (build %lu). UFC 1-4 need Windows 10 version 1903 or newer, or Windows 11: update through Windows Update (or install a current Windows 10/11)."), b);
        out[n - 1] = 0; g_pcProblem = 1; return TRUE;
    }
    ImpMiss m = {{0}};
    if (EngineImportProblem(g, &m)) {
        wchar_t dll[64], fn[64]; MultiByteToWideChar(CP_ACP, 0, m.dll, -1, dll, 64); MultiByteToWideChar(CP_ACP, 0, m.fn, -1, fn, 64);
        if (m.kind == 3) { _snwprintf(out, n, L2(L"Falta o arquivo do port games\\%s\\bin\\%s. Extraia o pacote do PORTHEON inteiro de novo numa pasta nova; se um antivírus apagou o arquivo, restaure-o na quarentena dele.",
                                                 L"The port file games\\%s\\bin\\%s is missing. Extract the whole PORTHEON package again into a new folder; if an antivirus deleted the file, restore it from its quarantine."), G[g].id, dll); g_pcProblem = 6; }
        else if (!_wcsicmp(dll, L"vulkan-1.dll")) { _snwprintf(out, n, L"%s", L2(L"Falta o Vulkan (vulkan-1.dll): o driver da placa de vídeo não está instalado ou está incompleto. Instale o driver mais novo do site da NVIDIA, da AMD ou da Intel (não o do Windows Update) e reinicie o PC.",
                                                                                   L"Vulkan is missing (vulkan-1.dll): the graphics driver is not installed or incomplete. Install the newest driver from NVIDIA's, AMD's or Intel's site (not the one from Windows Update) and restart the PC.")); g_pcProblem = 2; }
        else if (!_wcsnicmp(dll, L"api-ms-win-crt", 14)) { _snwprintf(out, n, L2(L"Falta o \"Universal C Runtime\" do Windows (%s). Atualize o Windows pelo Windows Update.", L"Windows' \"Universal C Runtime\" is missing (%s). Update Windows through Windows Update."), dll); g_pcProblem = 7; }
        else if (m.kind == 2) { _snwprintf(out, n, L2(L"O Windows deste PC não tem a função %s (de %s), que o motor dos ports usa. Atualize para o Windows 10 versão 1903 ou mais novo, ou para o Windows 11 (Windows Update). Versões \"lite\" do Windows às vezes tiram partes do sistema.",
                                                      L"This PC's Windows lacks the function %s (from %s) that the ports' engine uses. Update to Windows 10 version 1903 or newer, or Windows 11 (Windows Update). \"Lite\" editions of Windows sometimes remove parts of the system."), fn, dll); g_pcProblem = 7; }
        else { _snwprintf(out, n, L2(L"O Windows deste PC não tem o %s, que o motor dos ports usa. Atualize para o Windows 10 versão 1903 ou mais novo, ou para o Windows 11 (Windows Update). Versões \"lite\" do Windows às vezes tiram partes do sistema.",
                                     L"This PC's Windows has no %s, which the ports' engine uses. Update to Windows 10 version 1903 or newer, or Windows 11 (Windows Update). \"Lite\" editions of Windows sometimes remove parts of the system."), dll); g_pcProblem = 7; }
        out[n - 1] = 0; return TRUE;
    }
    VulkanCheck();
    if (!g_vkc.loader) { _snwprintf(out, n, L"%s", L2(L"Falta o Vulkan (vulkan-1.dll): o driver da placa de vídeo não está instalado. Instale o driver mais novo do site da NVIDIA, da AMD ou da Intel e reinicie o PC.",
                                                       L"Vulkan is missing (vulkan-1.dll): the graphics driver is not installed. Install the newest driver from NVIDIA's, AMD's or Intel's site and restart the PC.")); g_pcProblem = 2; }
    else if (!g_vkc.device) { _snwprintf(out, n, L"%s", L2(L"O Vulkan não encontrou nenhuma placa de vídeo. Instale o driver do site da NVIDIA, da AMD ou da Intel (o \"Adaptador de vídeo básico da Microsoft\" não roda jogos) e reinicie o PC.",
                                                             L"Vulkan found no graphics card. Install the driver from NVIDIA's, AMD's or Intel's site (the \"Microsoft Basic Display Adapter\" runs no games) and restart the PC.")); g_pcProblem = 3; }
    else if (g_vkc.api < ((1u << 22) | (3u << 12)) && !wcsncmp(g_vkc.name, L"Microsoft Direct3D12", 20)) {
        /* 4.0.3: "Microsoft Direct3D12 (NVIDIA GeForce GTX 1650)" with Vulkan 1.2 is Dozen, Microsoft's Vulkan over DirectX 12
           (the "OpenCL, OpenGL and Vulkan Compatibility Pack"), not the card's own driver: the card is fine, its driver is
           missing (a player's UFC 3 log, code 23 "Vulkan 1.3 is required, but only 1.2 is supported", 2026-10-24) */
        _snwprintf(out, n, L2(L"O Vulkan deste PC é o da Microsoft (\"%s\", Vulkan %u.%u, do \"Pacote de compatibilidade OpenCL, OpenGL e Vulkan\"), não o do driver da placa de vídeo, e os ports precisam do Vulkan 1.3. A placa serve: instale o driver mais novo dela pelo site da NVIDIA, da AMD ou da Intel (não o do Windows Update) e reinicie o PC.",
                              L"This PC's Vulkan is Microsoft's (\"%s\", Vulkan %u.%u, from the \"OpenCL, OpenGL and Vulkan Compatibility Pack\"), not the graphics card driver's, and the ports need Vulkan 1.3. The card is fine: install its newest driver from NVIDIA's, AMD's or Intel's site (not the one from Windows Update) and restart the PC."),
                   g_vkc.name, g_vkc.api >> 22, (g_vkc.api >> 12) & 0x3ff); g_pcProblem = 4; }
    else if (g_vkc.api < ((1u << 22) | (3u << 12))) {
        _snwprintf(out, n, L2(L"A placa %s tem Vulkan %u.%u, e os ports precisam do Vulkan 1.3. Atualize o driver da placa pelo site do fabricante; se ela já estiver no driver mais novo, ela é antiga demais para o UFC 1 a 4 (precisa de NVIDIA GTX 900 ou mais nova, AMD RX 400 ou mais nova, ou Intel UHD 600/Iris Xe/Arc).",
                              L"The %s card has Vulkan %u.%u, and the ports need Vulkan 1.3. Update the card's driver from its maker's site; if it already has the newest driver, it is too old for UFC 1-4 (they need an NVIDIA GTX 900 or newer, AMD RX 400 or newer, or Intel UHD 600/Iris Xe/Arc)."),
                   g_vkc.name, g_vkc.api >> 22, (g_vkc.api >> 12) & 0x3ff); g_pcProblem = 4; }
    else if (g_vkc.miss[0]) {
        _snwprintf(out, n, L2(L"A placa %s não tem recursos do Vulkan que o motor exige (%s). Atualize o driver da placa pelo site do fabricante; se não resolver, a placa ou o driver não suporta os ports.",
                              L"The %s card lacks Vulkan features the engine requires (%s). Update the card's driver from its maker's site; if that does not help, the card or its driver cannot run the ports."), g_vkc.name, g_vkc.miss); g_pcProblem = 5; }
    out[n - 1] = 0;
    return out[0] != 0;
}
/* before PLAY: a problem stops the game with the explanation (a Vulkan or feature doubt asks first: the check could be wrong) */
static BOOL PcReady(int g)
{
    wchar_t t[900]; if (!PcProblemText(g, t, 900)) return TRUE;
    if (g_pcProblem >= 4 && g_pcProblem <= 5) {
        wchar_t q[1100]; _snwprintf(q, 1100, L"%s\n\n%s", t, L2(L"Tentar abrir o jogo mesmo assim?", L"Try to start the game anyway?")); q[1099] = 0;
        if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONWARNING) == IDYES) return TRUE;
    }
    Msg(C_BAD, L"%s", t); return FALSE;
}
/* after a 0xC0000135 / 0xC0000139: the same checks, fresh */
static BOOL DllDiagnosis(int g, wchar_t *out, int n)
{
    if (g < 0) return FALSE;
    g_vkc.done = FALSE;
    { wchar_t ik[32]; _snwprintf(ik, 32, L"imp_ok_%s", G[g].id); WritePrivateProfileStringW(L"PORTHEON", ik, NULL, g_pIni); }   /* 6.0 final: after a DLL crash the import scan runs again */
    return PcProblemText(g, out, n);
}
/* 4.0.3: code 23 is also how the engine stops when the Vulkan device is too old at the start ("Vulkan 1.3 is required, but
   only 1.2 is supported by device!"): that is the driver, not a graphics effect, so the code-23 fallbacks do not apply */
static BOOL WantArcEngine(int g);
static BOOL WantNpEngine(int g);
static BOOL VulkanTooOldAfterExit(int g, wchar_t *out, int n)
{
    char *t = ReadLogTail(g, 1u << 20); if (!t) return FALSE;
    BOOL old = strstr(t, "is required, but only") != NULL;
    /* 4.3.1: the engine also stops at the start on a driver without an extension it cannot do without ("Required Vulkan
       extension unavailable: VK_..."). That is the driver too, not a graphics effect. The new engine of UFC 3 and 4 (the
       automatic one on every card since 4.3.1) knows one optional extension more than the ports' own engines; should a
       driver ever refuse it a required one, the next PLAY goes back to the port's own engine, as NewEngineFallback does. */
    char ext[96] = ""; const char *r = strstr(t, "Required Vulkan extension unavailable: ");
    if (r) { r += 39; int i = 0; while (r[i] && r[i] != '\r' && r[i] != '\n' && r[i] != ' ' && i < 95) { ext[i] = r[i]; i++; } ext[i] = 0; }
    free(t);
    if (ext[0]) {
        BOOL back = (g == 2 || g == 3) && GetInt(g, L"pgame:engine", 0) == 0 && WantArcEngine(g) && !WantV30Engine(g) && !WantNpEngine(g) && !GpuIsIntel()
                    && !(g_haveGpu && g_gpu.vendor == 0x1002 && PIni(L"amd_new_engine", 1));
        if (back) PIniSet(g == 2 ? L"ufc3_old_engine" : L"ufc4_old_engine", 1);
        wchar_t w[96]; MultiByteToWideChar(CP_UTF8, 0, ext, -1, w, 96);
        _snwprintf(out, n, L2(L"O jogo não abriu: o driver da placa de vídeo não tem a extensão Vulkan %s, que o motor exige (o log diz \"Required Vulkan extension unavailable\"). %sInstale o driver mais novo da placa pelo site da NVIDIA, da AMD ou da Intel (não o do Windows Update) e reinicie o PC.",
                              L"The game did not start: the graphics driver lacks the Vulkan extension %s, which the engine requires (the log says \"Required Vulkan extension unavailable\"). %sInstall the card's newest driver from NVIDIA's, AMD's or Intel's site (not the one from Windows Update) and restart the PC."),
                   w, back ? L2(L"O próximo JOGAR usa o motor do próprio port. ", L"The next PLAY uses the port's own engine. ") : L"");
        out[n - 1] = 0; return TRUE;
    }
    if (!old) return FALSE;
    if (DllDiagnosis(g, out, n)) return TRUE;
    _snwprintf(out, n, L"%s", L2(L"O jogo não abriu: o Vulkan escolhido não tem a versão 1.3 que os ports precisam (o log diz \"Vulkan 1.3 is required\"). Instale o driver mais novo da placa pelo site da NVIDIA, da AMD ou da Intel (não o do Windows Update) e reinicie o PC.",
                                 L"The game did not start: the Vulkan it got lacks the 1.3 version the ports need (the log says \"Vulkan 1.3 is required\"). Install the card's newest driver from NVIDIA's, AMD's or Intel's site (not the one from Windows Update) and restart the PC."));
    out[n - 1] = 0; return TRUE;
}

/* ---------------------------------------------------------------- the game's own files
   The databigs archives are EA "BIG" v3 files ("EB" 00 03): the big-endian 32-bit total size of the archive at 0x1C
   (checked on every archive of the four games' dumps). A download cut short leaves a file smaller than that size, and
   the game then fails to mount it ("Failed to mount bigfile", UFC 4 3.4.3 log of 2026-10-24) and crashes at once. The
   archives each port needs (language packs aside) were taken from the four games' own dumps (base + update). */
static const char *const NEED_BIGS[NGAMES][11] = {
    {"game_audio.big", "game_misc.big", "game_rendch.big", "game_rendstd.big", "game_ui.big", "inxp_audio.big", "inxp_misc.big", "inxp_rendch.big", "inxp_rendstd.big", "inxp_ui.big", NULL},
    {"game_audio_170.big", "game_misc_210.big", "game_rendch_190.big", "game_rendstd_180.big", "game_ui_200.big", "inxp_audio_50.big", "inxp_misc_60.big", "inxp_rendch_20.big", "inxp_rendstd_30.big", "inxp_ui_40.big", NULL},
    {"game_audio_170.big", "game_rendch_190.big", "game_rendstd_180.big", "game_ui_200.big", "inxp_audio_50.big", "inxp_misc_60.big", "inxp_rendch_20.big", "inxp_rendstd_30.big", "inxp_ui_40.big", NULL},
    {"game_rendch_190.big", "game_rendstd_180.big", "game_ui_200.big", "inxp_audio_50.big", "inxp_misc_60.big", "inxp_rendch_20.big", "inxp_rendstd_30.big", "inxp_ui_40.big", NULL},
};
typedef struct { int problem; wchar_t file[96]; ULONGLONG have, want; int missing; wchar_t list[300]; } GameCheck;
enum { GC_OK, GC_NODATA, GC_UPDATEONLY, GC_MISSING, GC_CUT, GC_BADHEAD };
static BOOL SfoCategory(const wchar_t *dir, char *cat)   /* param.sfo's CATEGORY: gd (a game) or gp (its update) */
{
    wchar_t p[MAX_PATH]; Join(p, dir, L"sce_sys\\param.sfo"); cat[0] = 0;
    DWORD n = 0; BYTE *b = ReadWhole(p, &n); if (!b) return FALSE;
    if (n >= 20 && !memcmp(b, "\0PSF", 4)) {
        uint32_t kt = *(uint32_t *)(b + 8), dt = *(uint32_t *)(b + 12), cnt = *(uint32_t *)(b + 16);
        for (uint32_t i = 0; i < cnt && 20 + (i + 1) * 16 <= n; i++) {
            BYTE *e = b + 20 + i * 16; uint16_t ko = *(uint16_t *)e; uint32_t len = *(uint32_t *)(e + 4), off = *(uint32_t *)(e + 12);
            if ((uint64_t)kt + ko + 9 > n || (uint64_t)dt + off + len > n) continue;
            if (!strcmp((const char *)b + kt + ko, "CATEGORY")) { size_t l = strnlen((const char *)b + dt + off, len); if (l > 3) l = 3; memcpy(cat, b + dt + off, l); cat[l] = 0; }
        }
    }
    free(b); return cat[0] != 0;
}
/* the folders that make up the game: d itself when it has eboot.bin, else its sub-folders that do (a base, its update,
   a "-merged" one made by the port) */
static int GameParts(const wchar_t *d, wchar_t parts[][MAX_PATH], int max)
{
    int n = 0;
    if (HasEboot(d)) { wcscpy(parts[n++], d); }
    wchar_t pat[MAX_PATH]; Join(pat, d, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.' || n >= max) continue;
            wchar_t s[MAX_PATH]; Join(s, d, fd.cFileName); if (HasEboot(s)) wcscpy(parts[n++], s);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    /* the base game beside a chosen update folder ("CUSA01968-patch" picked by itself, CUSA01968 next to it) */
    if (n == 1 && !wcscmp(parts[0], d) && n < max) {
        wchar_t par[MAX_PATH]; wcscpy(par, d); wchar_t *s = wcsrchr(par, L'\\'); const wchar_t *base = s ? s + 1 : par;
        static const wchar_t *const suf[] = {L"-patch", L"-update", L"_patch", L"_update", L" patch", L" update"};
        for (int i = 0; i < 6 && s; i++) {
            size_t bl = wcslen(base), sl = wcslen(suf[i]);
            if (bl > sl && !_wcsicmp(base + bl - sl, suf[i])) {
                wchar_t sib[MAX_PATH]; _snwprintf(sib, MAX_PATH, L"%.*s", (int)(wcslen(d) - sl), d); sib[MAX_PATH - 1] = 0;
                if (HasEboot(sib)) wcscpy(parts[n++], sib);
                break;
            }
        }
    }
    return n;
}
static void CheckGameFiles(int g, const wchar_t *d, GameCheck *r)
{
    memset(r, 0, sizeof(*r));
    wchar_t parts[8][MAX_PATH]; int np = GameParts(d, parts, 8);
    if (!np) { r->problem = GC_NODATA; return; }
    BOOL seen[11] = {0}; BOOL anyBig = FALSE, gpOnly = TRUE;
    for (int i = 0; i < np; i++) {
        char cat[8]; if (!SfoCategory(parts[i], cat) || strcmp(cat, "gp")) gpOnly = FALSE;
        wchar_t pat[MAX_PATH], db[MAX_PATH]; Join(db, parts[i], L"databigs"); Join(pat, db, L"*.big");
        WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            anyBig = TRUE;
            char an[MAX_PATH]; WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, an, MAX_PATH, NULL, NULL);
            for (int k = 0; k < 11 && NEED_BIGS[g][k]; k++) if (!_stricmp(an, NEED_BIGS[g][k])) seen[k] = TRUE;
            if (r->problem) continue;   /* the first broken archive is the one named */
            ULONGLONG size = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            wchar_t f[MAX_PATH]; Join(f, db, fd.cFileName);
            HANDLE fh = CreateFileW(f, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            BYTE hd[32]; DWORD got = 0;
            if (fh == INVALID_HANDLE_VALUE) continue;
            ReadFile(fh, hd, 32, &got, NULL); CloseHandle(fh);
            if (got < 32 || hd[0] != 'E' || hd[1] != 'B') { if (size < 32 || (hd[0] == 0 && hd[1] == 0)) { r->problem = GC_BADHEAD; wcsncpy(r->file, fd.cFileName, 95); r->have = size; } continue; }
            uint32_t want = ((uint32_t)hd[28] << 24) | ((uint32_t)hd[29] << 16) | ((uint32_t)hd[30] << 8) | hd[31];
            if (want && (uint32_t)size < want && size < 0x100000000ull) { r->problem = GC_CUT; wcsncpy(r->file, fd.cFileName, 95); r->file[95] = 0; r->have = size; r->want = want; }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (r->problem) return;
    if (!anyBig) { r->problem = gpOnly ? GC_UPDATEONLY : GC_NODATA; return; }
    for (int k = 0; k < 11 && NEED_BIGS[g][k]; k++) if (!seen[k]) {
        r->missing++;
        wchar_t w[64]; MultiByteToWideChar(CP_UTF8, 0, NEED_BIGS[g][k], -1, w, 64);
        if (wcslen(r->list) + wcslen(w) + 3 < 300) { if (r->list[0]) wcscat(r->list, L", "); wcscat(r->list, w); }
    }
    if (r->missing) r->problem = gpOnly && r->missing >= 5 ? GC_UPDATEONLY : GC_MISSING;
}
static BOOL GameCheckText(int g, const GameCheck *r, wchar_t *out, int n)
{
    out[0] = 0;
    switch (r->problem) {
    case GC_UPDATEONLY:
        _snwprintf(out, n, L2(L"Essa pasta tem só a ATUALIZAÇÃO do %s (o patch %s), sem o jogo base: por isso o jogo fecha logo ao abrir. Coloque a pasta do jogo base (%s, versão 1.00) ao lado da pasta do update e escolha a pasta de fora que tem as duas, ou copie o update por cima do jogo base.",
                              L"That folder only has %s's UPDATE (patch %s), without the base game: that is why the game closes right away. Put the base game folder (%s, version 1.00) next to the update folder and pick the outer folder that holds both, or copy the update over the base game."),
                   G[g].name, G[g].upd, G[g].tid); break;
    case GC_NODATA:
        _snwprintf(out, n, L2(L"A pasta do %s não tem a pasta databigs com os arquivos do jogo: o jogo está incompleto (ou ainda dentro do .pkg). Extraia o jogo inteiro de novo (veja \"Instalar jogo\").",
                              L"%s's folder has no databigs folder with the game's files: the game is incomplete (or still inside the .pkg). Extract the whole game again (see \"Install a game\")."), G[g].name); break;
    case GC_CUT:
        _snwprintf(out, n, L2(L"O arquivo do jogo databigs\\%s está incompleto (%.1f de %.1f MB): o download ou a extração parou no meio, e o jogo fecha ao carregar. Baixe/extraia esse jogo de novo ou copie esse arquivo de novo.",
                              L"The game file databigs\\%s is incomplete (%.1f of %.1f MB): the download or the extraction stopped halfway, and the game closes while loading. Download/extract the game again, or copy that file again."),
                   r->file, r->have / 1048576.0, r->want / 1048576.0); break;
    case GC_BADHEAD:
        _snwprintf(out, n, L2(L"O arquivo do jogo databigs\\%s está vazio ou corrompido. Extraia o jogo de novo ou copie esse arquivo de novo.", L"The game file databigs\\%s is empty or damaged. Extract the game again, or copy that file again."), r->file); break;
    case GC_MISSING:
        _snwprintf(out, n, L2(L"Faltam %d arquivo(s) do %s na pasta databigs (%s). O jogo base ou o update não foi copiado inteiro: extraia o jogo de novo (base %s + update %s).",
                              L"%d file(s) of %s are missing from the databigs folder (%s). The base game or the update was not copied whole: extract the game again (base %s + update %s)."),
                   r->missing, G[g].name, r->list, G[g].tid, G[g].upd); break;
    default: return FALSE;
    }
    out[n - 1] = 0; return TRUE;
}
/* 4.0.1: UFC 4's update 28.00 exists in two builds: rebuilt for firmware 5.05 (the one the port was made and tested on)
   and the original one for firmware 6.72+, which the port accepts by signature. None of the 14 logs with the 6.72+
   build that PORTHEON got (about 10 players, 2026-10-19 to 10-24) ended normally: code 3 at start (a call to address 1,
   or a missing nav.lua) or code 23 in the fight, while the 5.05 build ends normally. The port's own game_check.py
   --variant tells them apart (0.3 s); the answer is kept in portheon.ini for the folder and its eboot.bin files.
   1: 5.05, 2: 6.72+, 0: not known */
static int Ufc4Build(const wchar_t *d)
{
    wchar_t parts[8][MAX_PATH]; int np = GameParts(d, parts, 8); if (!np) return 0;
    /* 4.0.3: with the base game and its update side by side, d is the base (1.00): game_check.py found it unsupported and
       the 6.72+ warning never showed (a player's RTX 4060 log, CUSA14204-merged, 2026-10-25). The folder the port plays,
       "<base>-merged" (merge_update.py makes it at the first PLAY), has the update's eboot.bin: that one is checked. */
    const wchar_t *chk = d; static wchar_t merged[MAX_PATH];
    for (int i = 0, k = np; i < k && chk == d; i++) {
        _snwprintf(merged, MAX_PATH, L"%s-merged", parts[i]); merged[MAX_PATH - 1] = 0;
        if (HasEboot(merged) && np < 8) { chk = merged; wcscpy(parts[np++], merged); }
    }
    ULONGLONG h = 1469598103934665603ull;
    for (const wchar_t *p = d; *p; p++) h = (h ^ (ULONGLONG)towlower(*p)) * 1099511628211ull;
    for (int i = 0; i < np; i++) {
        wchar_t e[MAX_PATH]; Join(e, parts[i], L"eboot.bin"); WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(e, GetFileExInfoStandard, &fa)) h = (((h ^ fa.nFileSizeLow) * 1099511628211ull) ^ fa.ftLastWriteTime.dwLowDateTime) * 1099511628211ull;
    }
    wchar_t sig[24], cached[48]; _snwprintf(sig, 24, L"%016llx", h); sig[23] = 0;
    GetPrivateProfileStringW(L"PORTHEON", L"ufc4_build", L"", cached, 48, g_pIni);
    if (!wcsncmp(cached, sig, 16) && cached[16] == L'|') return _wtoi(cached + 17);
    wchar_t pd[MAX_PATH], exe[MAX_PATH], py[MAX_PATH], sd[MAX_PATH], cmd[3 * MAX_PATH + 64];
    PortDir(3, pd); Join(py, pd, L"python"); Join(exe, py, L"python.exe"); Join(sd, pd, L"scripts");
    if (!FileExists(exe)) return 0;
    _snwprintf(cmd, 3 * MAX_PATH + 64, L"\"%s\" -E -s \"%s\\game_check.py\" \"%s\" --variant", exe, sd, chk); cmd[3 * MAX_PATH + 63] = 0;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE}; HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return 0;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = {sizeof(si)}; si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; si.hStdOutput = wr; si.hStdError = wr;
    PROCESS_INFORMATION pi = {0};
    BOOL ok = CreateProcessW(exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, sd, &si, &pi);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); return 0; }
    char buf[2048]; DWORD got = 0, k;
    while (got < sizeof(buf) - 1 && ReadFile(rd, buf + got, sizeof(buf) - 1 - got, &k, NULL) && k) got += k;
    buf[got] = 0; CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, 15000); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    int b = !strncmp(buf, "5.05", 4) ? 1 : !strncmp(buf, "6.72+", 5) ? 2 : 0;
    { wchar_t v[48]; _snwprintf(v, 48, L"%s|%d", sig, b); WritePrivateProfileStringW(L"PORTHEON", L"ufc4_build", v, g_pIni); }   /* "not known" too: asked once */
    return b;
}
static const wchar_t *Ufc4BuildText(void)
{
    return L2(L"Este UFC 4 é o update 28.00 original, para firmware 6.72+. O port do UFC 4 foi feito e testado no update 28.00 refeito para firmware 5.05 (o \"backport\"): com o 6.72+, nenhum dos logs que recebemos terminou bem (código 3 ao abrir ou código 23 na luta). Use o UFC 4 com o update 28.00 para firmware 5.05.",
              L"This UFC 4 is the original 28.00 update, for firmware 6.72+. The UFC 4 port was made and tested on the 28.00 update rebuilt for firmware 5.05 (the \"backport\"): with the 6.72+ one, none of the logs we got ended well (code 3 at start or code 23 in the fight). Use UFC 4 with the 28.00 update for firmware 5.05.");
}
/* 7.1 (U3-PATH; merge_update.py of 7.1, lane ufc5-scripts): the script now prints, at every PLAY of UFC 1-4,
     merge_update: longest file path in the game folder: N characters (limit 259, Windows long paths on|off)
   (N = the folder the engine plays, the inner game folder or its -merged, + 1 + its longest file path). The merge works
   at any length now, but the engine cannot open a file past 259 characters while Windows' long paths are off (UFC 1-3
   lose videos, UFC 4 also an audio bank). This reads that line from the port's newest log when the log is of the same
   chosen folder (the merge_update.py command line at its top names it): N when N > 259 with long paths off, else 0.
   A log of an older merge_update.py has no such line: 0, nothing is said (the 7.0 behaviour). head: the log's start. */
static int LongPathFromLog(const char *head, const wchar_t *gameDir)
{
    const char *a = strstr(head, "merge_update.py\" \""); if (!a) return 0;
    a += 18; const char *q = strchr(a, '"'); if (!q || q - a >= 3 * MAX_PATH) return 0;
    char arg[3 * MAX_PATH]; memcpy(arg, a, (size_t)(q - a)); arg[q - a] = 0;
    wchar_t wa[MAX_PATH], wd[MAX_PATH]; if (!MultiByteToWideChar(CP_UTF8, 0, arg, -1, wa, MAX_PATH)) return 0;
    wcsncpy(wd, gameDir, MAX_PATH - 1); wd[MAX_PATH - 1] = 0;
    for (wchar_t *s = wa + wcslen(wa); s > wa && (s[-1] == L'\\' || s[-1] == L'/'); ) *--s = 0;
    for (wchar_t *s = wd + wcslen(wd); s > wd && (s[-1] == L'\\' || s[-1] == L'/'); ) *--s = 0;
    if (_wcsicmp(wa, wd)) return 0;   /* the newest log is of another folder */
    const char *m = strstr(head, "merge_update: longest file path in the game folder: "); if (!m) return 0;
    int n = atoi(m + 52); const char *e = strchr(m, '\n'), *off = strstr(m, "Windows long paths off");
    return n > 259 && off && (!e || off < e) ? n : 0;
}
static int LongPathWarnN(int g, const wchar_t *gameDir)
{
    if (!PS4G(g) || !G[g].data) return 0;
    wchar_t d[MAX_PATH], pat[MAX_PATH], best[MAX_PATH] = L""; FILETIME bt = {0, 0};
    LocalData(d, G[g].data); Join(d, d, L"logs"); Join(pat, d, L"*.log");
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); if (h == INVALID_HANDLE_VALUE) return 0;
    do { if (CompareFileTime(&fd.ftLastWriteTime, &bt) > 0) { bt = fd.ftLastWriteTime; Join(best, d, fd.cFileName); } } while (FindNextFileW(h, &fd));
    FindClose(h); if (!best[0]) return 0;
    HANDLE f = CreateFileW(best, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL); if (f == INVALID_HANDLE_VALUE) return 0;
    char *b = (char *)malloc((64u << 10) + 1); DWORD got = 0; int n = 0;
    if (b && ReadFile(f, b, 64u << 10, &got, NULL)) { b[got] = 0; n = LongPathFromLog(b, gameDir); }
    free(b); CloseHandle(f); return n;
}
/* before PLAY (cached while the folder and its archives do not change): a problem asks whether to play anyway */
static BOOL GameFilesReady(int g)
{
    if (!PIni(L"check_game_files", 1)) return TRUE;
    wchar_t d[MAX_PATH]; GameDir(g, d); if (!d[0]) return TRUE;
    {   int lp = LongPathWarnN(g, d);   /* 7.1: said at every PLAY from that folder, never blocks */
        if (lp) { wchar_t w[600]; _snwprintf(w, 600, L2(L"O caminho mais longo de um arquivo do jogo tem %d caracteres e o Windows abre no máximo 259: alguns arquivos (por exemplo vídeos) podem não abrir no jogo. Mova a pasta do jogo para perto da raiz do disco (por exemplo D:\\Jogos) e escolha de novo.",
                                                           L"The longest game file path has %d characters and Windows opens at most 259: some files (videos, for example) may not open in the game. Move the game folder near the root of the drive (for example D:\\Games) and choose it again."), lp);
                  w[599] = 0; PlayWarn(w); } }
    static wchar_t okDir[NGAMES][MAX_PATH]; static ULONGLONG okAt[NGAMES];
    if (!wcscmp(okDir[g], d) && GetTickCount64() - okAt[g] < 600000) return TRUE;
    GameCheck r; CheckGameFiles(g, d, &r);
    wchar_t t[900];
    if (!GameCheckText(g, &r, t, 900)) {
        if (g != 3 || Ufc4Build(d) != 2) { wcscpy(okDir[g], d); okAt[g] = GetTickCount64(); return TRUE; }
        wcsncpy(t, Ufc4BuildText(), 899); t[899] = 0;   /* 4.0.1: UFC 4 with the 6.72+ build of its update */
    }
    wchar_t q[1100]; _snwprintf(q, 1100, L"%s\n\n%s", t, L2(L"Jogar mesmo assim?", L"Play anyway?")); q[1099] = 0;
    if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) { wcscpy(okDir[g], d); okAt[g] = GetTickCount64(); return TRUE; }
    Msg(C_BAD, L"%s", t); return FALSE;
}
/* 7.1: a "[WinError 3] ... -> '<temp>.merge-tmp'" line (merge_update.py of 7.0 or older) whose temp path, as Python
   quotes it (each backslash doubled), is 260 characters or more once read back */
static BOOL MergeTmpTooLong(const char *t)
{
    for (const char *m = t; (m = strstr(m, "[WinError 3]")) != NULL; m++) {
        const char *e = strchr(m, '\n'); if (!e) e = m + strlen(m);
        const char *a = NULL; for (const char *p = m; p < e && (p = strstr(p, "-> '")) != NULL && p < e; p++) a = p;
        if (!a) continue;
        a += 4; const char *z = a; while (z < e && *z != '\'') z++;
        if (z >= e || z - a < 10 || strncmp(z - 10, ".merge-tmp", 10)) continue;
        int len = 0;   /* characters, not UTF-8 bytes */
        for (const char *p = a; p < z; p++) { if (p[0] == '\\' && p + 1 < z && p[1] == '\\') p++; if (((unsigned char)*p & 0xC0) != 0x80) len++; }
        if (len >= 260) return TRUE;
    }
    return FALSE;
}
/* 4.0, code 2: the port's launcher stopped before the game (one of its preparation scripts failed: "erro 2" in the
   players' messages). The reason is in its log, in English: the common ones said in words, else its last line. */
static BOOL Code2Diagnosis(int g, wchar_t *out, int n)
{
    char *t = ReadLogTail(g, 256u << 10); if (!t) return FALSE;
    out[0] = 0; const wchar_t *msg = NULL;
    if (strstr(t, "needs the") && strstr(t, "update merged"))
        msg = L2(L"O jogo está sem o update que o port precisa. Copie tudo da pasta do update para dentro da pasta do jogo (substituindo), ou deixe as duas pastas lado a lado e escolha a pasta de fora. Veja \"Instalar jogo\", passo 3.",
                 L"The game lacks the update the port needs. Copy everything from the update folder into the game folder (replacing files), or keep both folders side by side and pick the outer folder. See \"Install a game\", step 3.");
    else if (strstr(t, "but eboot.bin is not"))
        msg = L2(L"O param.sfo é do update, mas o eboot.bin é de outra versão: copie o eboot.bin da pasta do update para a pasta do jogo, substituindo o antigo.",
                 L"param.sfo is from the update but eboot.bin is from another version: copy eboot.bin from the update folder into the game folder, replacing the old one.");
    else if (strstr(t, "this folder is another game"))
        msg = L2(L"Essa pasta é de outro jogo ou de outra região que o port não roda. Confira o jogo escolhido (veja a versão e o código CUSA em \"Arquivos do jogo\").",
                 L"That folder is another game, or a region the port does not run. Check the chosen game (see its version and CUSA code in \"Game files\").");
    else if (strstr(t, "could not be read as a decrypted") || strstr(t, "eboot.bin is neither") || strstr(t, "encrypted/compressed SELF"))
        msg = L2(L"O eboot.bin está criptografado ou danificado: o port precisa do eboot descriptografado (o que sai do dump do seu PS4 ou de um extrator de PKG). Faça o dump/extração de novo.",
                 L"eboot.bin is encrypted or damaged: the port needs the decrypted eboot (what comes out of your PS4's dump or a PKG extractor). Dump/extract the game again.");
    else if (strstr(t, "no sce_module folder"))
        msg = L2(L"A pasta tem o eboot.bin mas não tem a pasta sce_module: o jogo está incompleto. Coloque o jogo base e o update lado a lado e escolha a pasta de fora, ou copie a pasta sce_module do jogo base.",
                 L"The folder has eboot.bin but no sce_module folder: the game is incomplete. Put the base game and the update side by side and pick the outer folder, or copy the sce_module folder from the base game.");
    else if (strstr(t, "No eboot.bin in"))
        msg = L2(L"Não há eboot.bin nessa pasta nem nas pastas dentro dela: escolha a pasta do jogo já extraído (veja \"Instalar jogo\").", L"There is no eboot.bin in that folder or the folders inside it: pick the extracted game's folder (see \"Install a game\").");
    /* 7.1: merge_update.py of 7.1 says when a game file vanished while it prepared the game (WinError 3 also comes from
       an unplugged disk, not only from a long path) */
    else if (strstr(t, "merge_update: a game file is missing:"))
        msg = L2(L"Um arquivo do jogo sumiu durante a preparação: o disco foi desligado, ou a pasta do jogo foi movida ou apagada? Ligue o disco de novo (ou escolha a pasta do jogo de novo) e jogue de novo.",
                 L"A game file disappeared while the game was being prepared: was the drive unplugged, or the game folder moved or deleted? Connect the drive again (or choose the game folder again) and play again.");
    /* 7.1 (U3-PATH, a merge_update.py of 7.0 or older: "[WinError 3] ... -> '...\x.mkv.merge-tmp'"): Windows' 260-character
       limit, only when the temp path in the line really is that long (the U3-PATH Check: a vanished source gives the
       same error) */
    else if (MergeTmpTooLong(t))
        msg = L2(L"O caminho da pasta do jogo ficou longo demais para o Windows (mais de 259 letras num arquivo). Mova a pasta do jogo para perto da raiz do disco (por exemplo D:\\Jogos\\UFC 3) e escolha de novo.",
                 L"The game folder's path got too long for Windows (more than 259 characters in one file). Move the game folder near the root of the drive (for example D:\\Games\\UFC 3) and choose it again.");
    else if (strstr(t, "[WinError 112]") || strstr(t, "No space left") || strstr(t, "Errno 28"))
        msg = L2(L"O disco C: está cheio: a preparação do jogo precisa de uns 300 MB livres em %LOCALAPPDATA%. Libere espaço e tente de novo.", L"Drive C: is full: preparing the game needs about 300 MB free in %LOCALAPPDATA%. Free some space and try again.");
    else if (strstr(t, "[WinError 362]") || strstr(t, "cloud file"))
        msg = L2(L"A pasta do jogo está no OneDrive e os arquivos não estão baixados no PC. Mova o jogo para uma pasta fora do OneDrive (por exemplo C:\\Jogos).", L"The game folder is in OneDrive and its files are not downloaded to the PC. Move the game to a folder outside OneDrive (for example C:\\Games).");
    else if (strstr(t, "PermissionError") || strstr(t, "[WinError 5]") || strstr(t, "[Errno 13]"))
        msg = L2(L"O Windows negou o acesso a um arquivo do jogo ou do port (um antivírus, a \"Proteção contra ransomware / Acesso controlado a pastas\" ou uma pasta só de leitura). Libere o PORTHEON no antivírus ou mova o jogo para uma pasta normal.",
                 L"Windows denied access to a game or port file (an antivirus, \"Ransomware protection / Controlled folder access\" or a read-only folder). Allow PORTHEON in the antivirus or move the game to a normal folder.");
    if (msg) { _snwprintf(out, n, L"%s", msg); out[n - 1] = 0; free(t); return TRUE; }
    /* else: the last line that says what failed */
    const char *best = NULL;
    for (const char *p = t; (p = strstr(p, "Error")) != NULL; p++) best = p;
    const char *f = NULL; for (const char *p = t; (p = strstr(p, "failed (see the log)")) != NULL; p++) f = p;
    if (best || f) {
        const char *s = best ? best : f; while (s > t && s[-1] != '\n') s--;
        const char *e = strchr(s, '\n'); size_t l = e ? (size_t)(e - s) : strlen(s); if (l > 300) l = 300;
        char line[304]; memcpy(line, s, l); line[l] = 0; if (l && line[l - 1] == '\r') line[l - 1] = 0;
        wchar_t w[320]; MultiByteToWideChar(CP_UTF8, 0, line, -1, w, 320);
        _snwprintf(out, n, L2(L"A preparação do %s falhou antes de o jogo abrir (código 2): %s. Mande o log mais novo (botão Logs) no Discord.", L"Preparing %s failed before the game started (code 2): %s. Send the newest log (Logs button) on Discord."), G[g].name, w);
        out[n - 1] = 0;
    }
    free(t); return out[0] != 0;
}
/* after a crash: an archive the game could not mount (its own log) or a broken one (the check) explains it */
static BOOL GameFilesAfterCrash(int g, wchar_t *out, int n)
{
    char *t = ReadLogTail(g, 4u << 20); BOOL found = FALSE;
    /* 4.0.3: the port's own log names the update build ("Update build: 28.00 for firmware 6.72+"), whatever folder was chosen */
    if (t && g == 3 && strstr(t, "for firmware 6.72+")) { _snwprintf(out, n, L"%s", Ufc4BuildText()); out[n - 1] = 0; free(t); return TRUE; }
    if (t) {
        const char *m = strstr(t, "Failed to mount bigfile: '");
        if (m) {
            m += 26; const char *e = strchr(m, '\''); const char *s = m;
            for (const char *p = m; e && p < e; p++) if (*p == '/') s = p + 1;
            char nm[96] = ""; if (e && e - s < 95) { memcpy(nm, s, (size_t)(e - s)); nm[e - s] = 0; }
            wchar_t w[96]; MultiByteToWideChar(CP_UTF8, 0, nm, -1, w, 96);
            _snwprintf(out, n, L2(L"O %s fechou porque não conseguiu abrir o arquivo do jogo databigs\\%s (o log diz \"Failed to mount bigfile\"): esse arquivo está faltando, incompleto ou corrompido. Extraia o jogo de novo, ou copie esse arquivo de novo do jogo original.",
                                  L"%s closed because it could not open the game file databigs\\%s (the log says \"Failed to mount bigfile\"): that file is missing, incomplete or damaged. Extract the game again, or copy that file again from the original game."), G[g].name, w);
            out[n - 1] = 0; found = TRUE;
        }
        free(t);
    }
    if (found) return TRUE;
    wchar_t d[MAX_PATH]; GameDir(g, d); if (!d[0]) return FALSE;
    GameCheck r; CheckGameFiles(g, d, &r);
    if (r.problem == GC_MISSING && r.missing < 2) r.problem = GC_OK;   /* one missing archive does not make a crash: the usual text says more */
    if (GameCheckText(g, &r, out, n)) return TRUE;
    if (g == 3 && Ufc4Build(d) == 2) { _snwprintf(out, n, L"%s", Ufc4BuildText()); out[n - 1] = 0; return TRUE; }   /* 4.0.1 */
    return FALSE;
}

/* ---------------------------------------------------------------- the processor (4.0)
   An Intel Core of the 2nd (Sandy Bridge) or 3rd (Ivy Bridge) generation has AVX but not BMI1, MOVBE and LZCNT, which
   the PS4 game code uses (Sandy Bridge also lacks F16C). For UFC 3, the port's scripts now carry @deanzx06's Sandy
   Bridge patch v2 (after T0ug's Ivy Bridge patch of the Bloodborne port, GPL-2.0): every such instruction of the game
   (6,486) jumps to an equivalent that this CPU has. PORTHEON turns it on when the CPU needs it (BB_LEGACY_CPU=1) and
   then lets the engine skip its CPU check (BB_SKIP_CPU_CHECK=1), which it only does on a CPU with AVX, SSE4.2 and
   POPCNT: those cannot be translated.
   4.0.2: UFC 1 (3,750 instructions) and UFC 2 (3,432) carry plans of their own, made by the same generator
   (tools\build_cpu_compat.py in each port), which now also translates the 256-bit F16C register forms those two use.
   A port has the mode when its folder has the plan (LegacyPlanIn); each plan only matches that port's game version. */
static BOOL LegacyPlanIn(int g)
{
    wchar_t d[MAX_PATH], p[MAX_PATH]; PortDir(g, d); Join(p, d, L"tools\\legacy_plan_data.py");
    wchar_t s[MAX_PATH]; Join(s, d, L"scripts\\cpu_compat.py");
    return FileExists(p) && FileExists(s);
}
static BOOL CpuBaseOk(void)   /* AVX (and the OS saving its registers), SSE4.2, POPCNT */
{
    unsigned a, b, c, d;
    if (!__get_cpuid(1, &a, &b, &c, &d)) return FALSE;
    BOOL sse42 = (c >> 20) & 1, popcnt = (c >> 23) & 1, avx = (c >> 28) & 1, osx = (c >> 27) & 1;
    if (!avx || !osx || !sse42 || !popcnt) return FALSE;
    unsigned lo, hi; __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    return (lo & 6) == 6;
}
static BOOL CpuLegacyOk(int g)
{
    if (!CpuLacksPs4(NULL, 0)) return TRUE;
    return CpuBaseOk() && LegacyPlanIn(g) && PIni(L"legacy_cpu", 1);
}
/* 5.2 (UFC Undisputed 3): the ReXGlue recompilation (ufc3-launcher-l1.exe, librexruntime.dll, rexgpu-xenos.dll) runs
   SSSE3, SSE4.1, SSE4.2 and POPCNT natively (no AVX: sampled in the audit), so only CPUs from before 2008-2011 (Core 2
   Conroe/Merom, Phenom II, Athlon II) lack something; they died silently with 0xC000001D. miss gets the names. */
static BOOL CpuX360Ok(wchar_t *miss, int n)
{
    unsigned a, b, c = 0, d; wchar_t names[64] = L"";
    if (!__get_cpuid(1, &a, &b, &c, &d)) c = 0;
    if (!((c >> 9) & 1)) wcscat(names, L"SSSE3, ");
    if (!((c >> 19) & 1)) wcscat(names, L"SSE4.1, ");
    if (!((c >> 20) & 1)) wcscat(names, L"SSE4.2, ");
    if (!((c >> 23) & 1)) wcscat(names, L"POPCNT, ");
    size_t l = wcslen(names); if (l >= 2) names[l - 2] = 0;
    if (miss && n > 0) { wcsncpy(miss, names, n); miss[n - 1] = 0; }
    return names[0] == 0;
}
/* 5.2: UFC Undisputed 3 got none of the file checks the other ports have: a DLL an antivirus took, or a Windows
   without one of the system DLLs, ended in 0xC0000135 with nothing said. Its data folder must hold the two game exes'
   DLLs (librexruntime, rexgpu-xenos, libc++, libunwind) and the shaders; the DLLs' imports are read (cached while the
   files do not change). The ~100 MB game exes themselves are only checked to be there and readable: their import
   tables name the same Windows DLLs librexruntime.dll does. A missing file stops PLAY (the port cannot start without
   it); a Windows DLL or function that seems missing asks first, as PcReady does. */
static BOOL X3FilesReady(int g)
{
    static const wchar_t *const files[] = {L"librexruntime.dll", L"rexgpu-xenos.dll", L"libc++.dll", L"libunwind.dll"};
    wchar_t d[MAX_PATH], data[MAX_PATH], f[MAX_PATH], which[MAX_PATH]; PortDir(g, d); Join(data, d, L"data");
    for (int i = 0; i < 4; i++) {
        Join(f, data, files[i]); int bad = 0;
        if (!FileExists(f)) bad = 1;
        else { HANDLE h = CreateFileW(f, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
               if (h == INVALID_HANDLE_VALUE) { DWORD e = GetLastError(); if (e == ERROR_VIRUS_INFECTED || e == ERROR_VIRUS_DELETED || e == ERROR_ACCESS_DENIED) bad = 2; } else CloseHandle(h); }
        if (bad) { _snwprintf(which, MAX_PATH, L"games\\%s\\data\\%s", G[g].id, files[i]); which[MAX_PATH - 1] = 0; AntivirusHelp(g, bad, which, TRUE); return FALSE; }
    }
    {   /* at least one of the two game exes (Reborn, Original), readable */
        wchar_t e1[MAX_PATH], e2[MAX_PATH], sh[MAX_PATH]; Join(e1, data, L"ufc3-launcher-l1.exe"); Join(e2, data, L"ufc3-original-v48.exe"); Join(sh, data, L"shaders");
        if (!FileExists(e1) && !FileExists(e2)) { _snwprintf(which, MAX_PATH, L"games\\%s\\data\\ufc3-launcher-l1.exe", G[g].id); which[MAX_PATH - 1] = 0; AntivirusHelp(g, 1, which, TRUE); return FALSE; }
        if (!DirExists(sh)) { _snwprintf(which, MAX_PATH, L"games\\%s\\data\\shaders", G[g].id); which[MAX_PATH - 1] = 0; AntivirusHelp(g, 1, which, TRUE); return FALSE; }
    }
    static FILETIME seen; static BOOL had; static ImpMiss last;
    ImpMiss m = {{0}}; BOOL bad = FALSE;
    Join(f, data, L"librexruntime.dll"); WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExW(f, GetFileExInfoStandard, &fa) && !CompareFileTime(&fa.ftLastWriteTime, &seen) && (seen.dwLowDateTime | seen.dwHighDateTime)) { bad = had; m = last; }
    else {
        for (int i = 0; i < 4 && !bad; i++) { Join(f, data, files[i]); bad = PeImportProblem(f, data, &m); }
        Join(f, data, L"librexruntime.dll"); if (GetFileAttributesExW(f, GetFileExInfoStandard, &fa)) seen = fa.ftLastWriteTime;
        had = bad; if (bad) last = m;
    }
    if (!bad) return TRUE;
    wchar_t dll[64], fn[64], t[700]; MultiByteToWideChar(CP_ACP, 0, m.dll, -1, dll, 64); MultiByteToWideChar(CP_ACP, 0, m.fn, -1, fn, 64);
    if (m.kind == 3) { _snwprintf(which, MAX_PATH, L"games\\%s\\data\\%s", G[g].id, dll); which[MAX_PATH - 1] = 0; AntivirusHelp(g, 1, which, TRUE); return FALSE; }
    if (m.kind == 2) _snwprintf(t, 700, L2(L"O Windows deste PC não tem a função %s (de %s), que o UFC Undisputed 3 usa. Atualize o Windows (Windows Update).", L"This PC's Windows lacks the function %s (from %s) that UFC Undisputed 3 uses. Update Windows (Windows Update)."), fn, dll);
    else _snwprintf(t, 700, L2(L"O Windows deste PC não tem o %s, que o UFC Undisputed 3 usa. Atualize o Windows (Windows Update); versões \"lite\" do Windows às vezes tiram partes do sistema.", L"This PC's Windows has no %s, which UFC Undisputed 3 uses. Update Windows (Windows Update); \"lite\" editions of Windows sometimes remove parts of the system."), dll);
    t[699] = 0;
    wchar_t q[900]; _snwprintf(q, 900, L"%s\n\n%s", t, L2(L"Tentar abrir o jogo mesmo assim?", L"Try to start the game anyway?")); q[899] = 0;
    if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONWARNING) == IDYES) return TRUE;
    Msg(C_BAD, L"%s", t); return FALSE;
}
static void CpuNoMessage(int g, const wchar_t *miss)
{
    /* 5.2: a Pentium or Celeron (no AVX at all up to the 10th generation) is named as such */
    if (!CpuBaseOk() && (StrStrIW(g_cpuName, L"Pentium") || StrStrIW(g_cpuName, L"Celeron")))
        Msg(C_BAD, L2(L"Este processador (%s) não tem AVX, que o código do jogo de PS4 usa em todo lugar: o UFC 1 a 4 não roda nele, nem com o modo de compatibilidade. Pentium e Celeron não têm AVX nem nas gerações novas (até a 10ª: G5400, G6400, J4125, N5000...): precisa de um Intel Core i3 ou melhor (2ª geração, 2011, ou mais novo), de um AMD FX ou de um Ryzen.",
                      L"This processor (%s) lacks AVX, which the PS4 game code uses everywhere: UFC 1-4 cannot run on it, not even in the compatibility mode. Pentium and Celeron chips have no AVX even in new generations (up to the 10th: G5400, G6400, J4125, N5000...): it needs an Intel Core i3 or better (2nd generation, 2011, or newer), an AMD FX or a Ryzen."), g_cpuName);
    else if (!CpuBaseOk())
        Msg(C_BAD, L2(L"Este processador (%s) não tem AVX/SSE4.2, que o código do jogo de PS4 usa em todo lugar: o UFC 1 a 4 não roda nele, nem com o modo de compatibilidade. Precisa de um Intel Core de 2ª geração (2011) ou mais novo, de um AMD FX ou de um Ryzen.",
                      L"This processor (%s) lacks AVX/SSE4.2, which the PS4 game code uses everywhere: UFC 1-4 cannot run on it, not even in the compatibility mode. It needs an Intel Core 2nd generation (2011) or newer, an AMD FX or a Ryzen."), g_cpuName);
    else
        Msg(C_BAD, L2(L"Este processador (%s) não tem %s. O modo de compatibilidade do PORTHEON (Intel Core de 2ª e 3ª geração) ainda não existe para o %s; os outros UFC que têm esse modo rodam neste PC. Para este, use um Intel Core de 4ª geração ou mais novo, ou um AMD Ryzen.",
                      L"This processor (%s) lacks %s. PORTHEON's compatibility mode (Intel Core 2nd and 3rd generation) does not exist for %s yet; the other UFC games that have it run on this PC. For this one, use an Intel Core 4th generation or newer, or an AMD Ryzen."), g_cpuName, miss, G[g].name);
}
static void LegacyEnv(int g)
{
    (void)g;
    EnvPush(L"BB_LEGACY_CPU", L"1");
    EnvPush(L"BB_SKIP_CPU_CHECK", L"1");
}

/* ---------------------------------------------------------------- "Este PC roda?" (4.3.8)
   The user: "fix every error of every UFC, so they run on everyone's PC". Before a game's first PLAY (and from its Game
   files tab) PORTHEON says in plain words what will run on this PC, what will not and why: the processor, the card and
   its Vulkan, the feature UFC 5/6's engines need (fragment shader barycentrics), RAM and video memory, and the warnings
   of the remote fix list (remote.c). Level: 0 fine, 1 runs with a warning, 2 does not run. */
static void FixWarnings(int g, wchar_t out[1200]);
static void VerdictLine(wchar_t *out, int n, const wchar_t *mark, const wchar_t *t)
{
    size_t l = wcslen(out); if (l + wcslen(t) + 8 >= (size_t)n) return;
    _snwprintf(out + l, n - l, L"%s%s %s", l ? L"\n" : L"", mark, t); out[n - 1] = 0;
}
static int PcVerdict(int g, wchar_t *out, int n)
{
    out[0] = 0; int level = 0; wchar_t t[600];
    const wchar_t *OK = L"\x2714", *WARN = L"\x26A0", *NO = L"\x2716";
    VulkanCheck();
    { size_t l = wcslen(g_cpuName); while (l && g_cpuName[l - 1] == L' ') g_cpuName[--l] = 0; }   /* the CPUID name ends in spaces */
    /* the processor */
    /* 5.0: the Xbox 360 port has no PS4/PS5 instruction needs; 5.2: its own (CpuX360Ok) */
    /* 6.0: Zelda runs on an emulator (or its native build): no instruction rule from PORTHEON; God of War III runs the PS4
       game's own code, so the PS4 rule */
    if (ZELDAG(g) || FNCG(g)) { _snwprintf(t, 600, L2(L"Processador: %s", L"Processor: %s"), g_cpuName); VerdictLine(out, n, OK, t); }
    else if (NGPORTG(g)) {   /* 6.1: Bloodborne's port needs AVX and translates BMI1/MOVBE/LZCNT (best effort); inFAMOUS's README states no CPU rule */
        wchar_t miss[64] = L"";
        if (!CpuLacksPs4(miss, 64)) { _snwprintf(t, 600, L2(L"Processador: %s", L"Processor: %s"), g_cpuName); VerdictLine(out, n, OK, t); }
        else if (BBG(g) && wcsstr(miss, L"AVX")) { _snwprintf(t, 600, L2(L"Processador: %s, sem %s. O port do Bloodborne precisa de AVX: não roda nele.", L"Processor: %s, without %s. The Bloodborne port needs AVX: it does not run on it."), g_cpuName, miss); VerdictLine(out, n, NO, t); level = 2; }
        else { _snwprintf(t, 600, BBG(g) ? L2(L"Processador: %s, sem %s. O port traduz essas instruções (Intel 2ª/3ª geração): melhor esforço, mais lento.", L"Processor: %s, without %s. The port translates those instructions (Intel 2nd/3rd gen): best effort, slower.")
                                         : L2(L"Processador: %s, sem %s, que o código de PS4 do jogo costuma usar: o inFAMOUS pode não abrir.", L"Processor: %s, without %s, which the game's PS4 code usually uses: inFAMOUS may not start."), g_cpuName, miss);
               VerdictLine(out, n, WARN, t); if (level < 1) level = 1; }
    }
    else if (GOW3G(g)) {
        wchar_t miss[64] = L"";
        if (!CpuLacksPs4(miss, 64)) { _snwprintf(t, 600, L2(L"Processador: %s", L"Processor: %s"), g_cpuName); VerdictLine(out, n, OK, t); }
        else { _snwprintf(t, 600, L2(L"Processador: %s, sem %s, que o código do God of War III (PS4) usa direto. O jogo não roda nele.", L"Processor: %s, without %s, which God of War III's (PS4) code uses directly. The game does not run on it."), g_cpuName, miss); VerdictLine(out, n, NO, t); level = 2; }
    }
    else if (X360G(g)) {
        if (!CpuX360Ok(NULL, 0)) { NoAvx2Text(g, t, 600); VerdictLine(out, n, NO, t); level = 2; }
        else { _snwprintf(t, 600, L2(L"Processador: %s", L"Processor: %s"), g_cpuName); VerdictLine(out, n, OK, t); }
    }
    else if (G[g].ps5) {
        if (!CpuPs5Ok(NULL, 0)) { NoAvx2Text(g, t, 600); VerdictLine(out, n, NO, t); level = 2; }   /* 5.2: every PS5 instruction family */
        else { _snwprintf(t, 600, L2(L"Processador: %s", L"Processor: %s"), g_cpuName); VerdictLine(out, n, OK, t); }
    } else {
        wchar_t miss[64] = L"";
        if (!CpuLacksPs4(miss, 64)) { _snwprintf(t, 600, L2(L"Processador: %s", L"Processor: %s"), g_cpuName); VerdictLine(out, n, OK, t); }
        else if (CpuLegacyOk(g)) { _snwprintf(t, 600, L2(L"Processador: %s, sem %s. Roda no modo de compatibilidade do PORTHEON (a primeira preparação demora mais).", L"Processor: %s, without %s. It runs in PORTHEON's compatibility mode (the first preparation takes longer)."), g_cpuName, miss); VerdictLine(out, n, WARN, t); if (level < 1) level = 1; }
        else { _snwprintf(t, 600, L2(L"Processador: %s, sem %s. Este UFC não roda nele.", L"Processor: %s, without %s. This UFC does not run on it."), g_cpuName, miss); VerdictLine(out, n, NO, t); level = 2; }
    }
    /* the card and its Vulkan */
    if (!g_vkc.loader) { VerdictLine(out, n, NO, L2(L"Sem Vulkan: o driver da placa de vídeo não está instalado. Instale o driver mais novo do site da NVIDIA, da AMD ou da Intel.", L"No Vulkan: the graphics driver is not installed. Install the newest driver from NVIDIA's, AMD's or Intel's site.")); level = 2; }
    else if (!g_vkc.device) { VerdictLine(out, n, NO, L2(L"Nenhuma placa de vídeo com Vulkan foi encontrada. Atualize o driver da placa.", L"No graphics card with Vulkan was found. Update the card's driver.")); level = 2; }
    else {
        uint32_t need = X360G(g) || ZELDAG(g) ? 0 : (1u << 22) | (3u << 12);   /* 5.0: UFC Undisputed 3: its own renderer, no Vulkan 1.3 rule from PORTHEON (6.0: Zelda's emulator either) */
        if (g_vkc.api < need) { _snwprintf(t, 600, L2(L"Placa: %s, com Vulkan %u.%u: o jogo precisa do 1.3. Atualize o driver (placas muito antigas não chegam no 1.3).", L"Card: %s, with Vulkan %u.%u: the game needs 1.3. Update the driver (very old cards never reach 1.3)."), g_vkc.name, g_vkc.api >> 22, (g_vkc.api >> 12) & 0x3ff); VerdictLine(out, n, NO, t); level = 2; }
        else if (g_vkc.miss[0] && !X360G(g) && !EXTG(g)) { _snwprintf(t, 600, L2(L"Placa: %s, falta %s. Atualize o driver da placa.", L"Card: %s, missing %s. Update the card's driver."), g_vkc.name, g_vkc.miss); VerdictLine(out, n, NO, t); level = 2; }
        else if (G[g].ps5 && !g_vkc.bary) {
            if (UFC6G(g)) { _snwprintf(t, 600, L2(L"Placa: %s, sem o recurso que o motor principal do UFC 6 exige (baricêntricas). O PORTHEON usa o motor de compatibilidade (experimental): pode não rodar nesta placa ainda.", L"Card: %s, without the feature UFC 6's main engine requires (barycentrics). PORTHEON uses the compatibility engine (experimental): it may not run on this card yet."), g_vkc.name); VerdictLine(out, n, WARN, t); if (level < 1) level = 1; }
            else { _snwprintf(t, 600, L2(L"Placa: %s, sem o recurso que o motor do UFC 5 exige (baricêntricas). O UFC 5 não roda nela (as RX 5000 e GTX 10 não têm esse recurso, nem com o driver mais novo).", L"Card: %s, without the feature UFC 5's engine requires (barycentrics). UFC 5 does not run on it (RX 5000 and GTX 10 cards lack it, even on the newest driver)."), g_vkc.name); VerdictLine(out, n, NO, t); level = 2; }
        }
        else { _snwprintf(t, 600, L2(L"Placa: %s, Vulkan %u.%u", L"Card: %s, Vulkan %u.%u"), g_vkc.name, g_vkc.api >> 22, (g_vkc.api >> 12) & 0x3ff); VerdictLine(out, n, OK, t); }
        /* 5.2: the driver's version, so a player can compare it with the maker's newest (and say it on Discord) */
        if (g_vkc.drv[0]) { const wchar_t *vn = VendorName(g_vkc.vendor); _snwprintf(t, 600, L2(L"Driver de vídeo: %s%s%s", L"Graphics driver: %s%s%s"), vn, vn[0] ? L" " : L"", g_vkc.drv); VerdictLine(out, n, OK, t); }
    }
    /* 5.2: a laptop on battery or the Power saver plan (said, never changed by PORTHEON) */
    {   int pw = PowerState();
        if (pw & 1) { VerdictLine(out, n, WARN, L2(L"Notebook na bateria: ligue o carregador (na bateria a placa e o processador rodam bem mais devagar).", L"Laptop on battery: plug in the charger (on battery the graphics card and the processor run much slower).")); if (level < 1) level = 1; }
        if (pw & 4) { VerdictLine(out, n, WARN, L2(L"Plano de energia \"Economia de energia\": escolha \"Equilibrado\" ou \"Alto desempenho\" nas Opções de energia do Windows.", L"\"Power saver\" power plan: pick \"Balanced\" or \"High performance\" in Windows' Power Options.")); if (level < 1) level = 1; }
    }
    /* memory */
    int vram = g_haveGpu ? (int)(g_gpu.vram >> 30) : 0;
    if (G[g].ps5) {
        if (g_ramGb && g_ramGb < 16) { _snwprintf(t, 600, L2(L"RAM: %d GB. O %s usa cerca de 12 a 14 GB: feche outros programas e deixe o arquivo de paginação grande (o PORTHEON ajuda se faltar).", L"RAM: %d GB. %s uses about 12 to 14 GB: close other programs and keep a big page file (PORTHEON helps if it runs out)."), g_ramGb, G[g].name); VerdictLine(out, n, WARN, t); if (level < 1) level = 1; }
        else if (g_ramGb) { _snwprintf(t, 600, L2(L"RAM: %d GB", L"RAM: %d GB"), g_ramGb); VerdictLine(out, n, OK, t); }
        if (vram && vram < 6) { _snwprintf(t, 600, L2(L"Memória de vídeo: %d GB. Use o preset Rápido; abaixo de 6 GB o jogo pode fechar nas trocas de cena.", L"Video memory: %d GB. Use the Fast preset; below 6 GB the game may close at scene changes."), vram); VerdictLine(out, n, WARN, t); if (level < 1) level = 1; }
    } else {
        if (g_ramGb && g_ramGb < 8) { _snwprintf(t, 600, L2(L"RAM: %d GB. Abaixo de 8 GB feche outros programas antes de jogar.", L"RAM: %d GB. Below 8 GB close other programs before playing."), g_ramGb); VerdictLine(out, n, WARN, t); if (level < 1) level = 1; }
        if (BBG(g)) { MEMORYSTATUSEX ms = {sizeof(ms)};   /* 6.1: the port's README: ~6 GB of free commit (RAM + page file) */
            if (GlobalMemoryStatusEx(&ms) && ms.ullAvailPageFile < (6ull << 30)) { _snwprintf(t, 600, L2(L"Memória livre (RAM + arquivo de paginação): %llu GB. O Bloodborne pede ~6 GB (10 GB em 1440p/4K): aumente o arquivo de paginação.", L"Free memory (RAM + page file): %llu GB. Bloodborne asks ~6 GB (10 GB at 1440p/4K): enlarge the page file."), ms.ullAvailPageFile >> 30); VerdictLine(out, n, WARN, t); if (level < 1) level = 1; } }
        if (vram && vram < 2 && !X360G(g) && !EXTG(g)) { _snwprintf(t, 600, L2(L"Memória de vídeo: %d GB. Use o modo Ultra desempenho (720p).", L"Video memory: %d GB. Use Ultra performance mode (720p)."), vram); VerdictLine(out, n, WARN, t); if (level < 1) level = 1; }
    }
    /* the remote fix list's warnings for this card */
    wchar_t w[1200]; w[0] = 0; if (!X360G(g) && !EXTG(g)) FixWarnings(g, w);
    for (wchar_t *line = w, *nx; line && *line; line = nx) { nx = wcschr(line, L'\n'); if (nx) *nx++ = 0; VerdictLine(out, n, WARN, line); if (level < 1) level = 1; }
    return level;
}
/* the verdict in a box: from the Game files tab (always), before a game's first PLAY (only with a warning or a "no";
   FALSE: the player chose not to play) */
static BOOL PcVerdictBox(int g, BOOL atPlay)
{
    /* 6.0 final: at PLAY the flag is read first: a game already checked on this PC starts with no Vulkan probe and no
       file read (it ran the whole verdict before every PLAY when it had nothing to say). 2 = checked, nothing to say */
    wchar_t k[48]; _snwprintf(k, 48, L"pccheck_%s", G[g].id);
    if (atPlay && PIni(k, 0)) return TRUE;
    wchar_t body[2400], head[200], all[2800]; int lv = PcVerdict(g, body, 2400);
    if (atPlay) { PIniSet(k, lv ? 1 : 2); if (lv == 0) return TRUE; }
    _snwprintf(head, 200, lv == 0 ? L2(L"O %s deve rodar neste PC.", L"%s should run on this PC.") : lv == 1 ? L2(L"O %s roda neste PC, com avisos:", L"%s runs on this PC, with warnings:") : L2(L"O %s não roda neste PC:", L"%s does not run on this PC:"), G[g].name);
    _snwprintf(all, 2800, L"%s\n\n%s%s", head, body, atPlay ? L2(L"\n\nJogar mesmo assim?", L"\n\nPlay anyway?") : L""); all[2799] = 0;
    if (!atPlay) { MessageBoxW(g_wnd, all, L2(L"Este PC roda?", L"Does this PC run it?"), MB_OK | (lv == 2 ? MB_ICONERROR : lv ? MB_ICONWARNING : MB_ICONINFORMATION)); return TRUE; }
    return MessageBoxW(g_wnd, all, L2(L"Este PC roda?", L"Does this PC run it?"), MB_YESNO | (lv == 2 ? MB_ICONERROR : MB_ICONWARNING)) == IDYES;
}
