/*
 * PORTHEON-Setup.exe (5.2) - the installable PORTHEON: installer and uninstaller in one native program.
 * Copyright (C) 2026 Marcelo Miyake (XAndromedaXR). GPL-2.0-or-later.
 *
 * The user asked for "an installable PORTHEON app": players kept extracting the zip into Program Files (where PORTHEON
 * cannot write next to itself), into OneDrive, or over an older folder by hand, and they never knew before installing
 * whether their PC runs which game. This program, in one window, PT or EN by Windows' own language:
 *  - says what this PC runs (Windows, the processor's instruction families, the graphics card, its driver and Vulkan,
 *    RAM, free disk), game by game, with the same rules PORTHEON itself uses (checks.c, portheon.c): warnings only;
 *    it refuses only a Windows too old for the installer itself (before 1803: no tar.exe);
 *  - installs into <the fixed drive with the most free space>:\PORTHEON by default, never into Program Files;
 *  - can be run again at any time: it extracts over the folder (resumable after a failure, an update in place) and
 *    never touches what the player made (saves, caches, the games' ini files);
 *  - makes the shortcuts, the "Apps & features" entry (HKCU, no administrator) and "Desinstalar PORTHEON.exe";
 *  - logs every step to %TEMP%\portheon-setup.log ("Ver registro").
 *
 * The package travels inside the exe: the release zip is appended after the program (an overlay Windows ignores) and
 * the last 64 bytes say where it is: {"PORTHEON-PAYLOAD", u64 offset, u64 size, SHA-256 of the zip}. package.ps1
 * (make-setup.ps1) writes it. The same exe without the overlay (the first <offset> bytes) is the uninstaller.
 *
 * Uninstall deletes by <dir>\portheon-files.txt (the package's files, written at install), never "everything in the folder".
 * Extraction: Windows' own tar.exe (bsdtar, in System32 since Windows 10 1803) reads the zip after the overlay was
 * copied to a temp file and its SHA-256 checked; it extracts into a stage folder inside the target (same drive), and
 * each file is then moved into place, which is where the player's files are skipped. Why not an inflater of our own:
 * PORTHEON's own updater (update.c) already trusts tar.exe with the same zips on the same PCs, and bsdtar handles
 * zip64, deflate64 and odd archives better than a few hundred lines written for this would.
 *
 * Hidden modes (tests, and PORTHEON's future updates):
 *   --quiet-install <dir> [--shortcuts]       the installer without a window (exit 0 ok, 1 failed, 2 refused, 3 old Windows, 4 no package)
 *   --selftest <zip> <dir> [--sha <hex>]      the same with an outside zip as the package
 *   --quiet-uninstall <dir> [--delete-saves]  the uninstaller without a window (saves copied to Documents unless --delete-saves)
 *   --report                                  the PC checks to stdout and the log
 *   --snap <file.bmp> [uninstall]             (test mode only) draws the window off screen into a bitmap
 * With PORTHEON_SETUP_TEST=1 the uninstall key is ...\Uninstall\PORTHEON-SetupTest, no shortcut is made or removed, the
 * saves backup goes to %TEMP%\portheon-setup-test-docs and PORTHEON is never opened.
 */
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <bcrypt.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <cpuid.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "../iconrefresh.h"   /* 6.0 final: the new logo on the shortcuts over an older install (Windows' icon cache) */

#define PATHN 1024
#define UNINST_EXE L"Desinstalar PORTHEON.exe"
#define PORTHEON_AUMID L"XAndromedaXR.PORTHEON"   /* portheon.c sets it on itself: the shortcuts carry the same one */
#define STAGE_DIR L".pst"   /* see STAGE_EXTRA */

/* ---------------------------------------------------------------- the language: PT-BR or English, like L2 in portheon.c */
static BOOL g_pt, g_test, g_quiet;
#define L2(pt, en) (g_pt ? (pt) : (en))

static HINSTANCE g_inst; static HWND g_wnd;
static wchar_t g_self[PATHN], g_selfDir[PATHN], g_logPath[PATHN];
static CRITICAL_SECTION g_cs;

/* ---------------------------------------------------------------- small helpers */
static void Join(wchar_t *out, const wchar_t *a, const wchar_t *b)
{
    wchar_t t[PATHN]; size_t n = wcslen(a);
    _snwprintf(t, PATHN, (n && a[n - 1] == L'\\') ? L"%s%s" : L"%s\\%s", a, b); t[PATHN - 1] = 0; wcscpy(out, t);
}
/* every error text goes through this: a 1024-letter path plus a 400-letter reason can fill the buffer, and _snwprintf
   then leaves it without its terminator (SetStatus would read past the end) */
static void SnW(wchar_t *out, int n, const wchar_t *fmt, ...)
{
    if (n <= 0) return;
    va_list a; va_start(a, fmt); _vsnwprintf(out, (size_t)n - 1, fmt, a); va_end(a); out[n - 1] = 0;
}
static BOOL FileExists(const wchar_t *p) { DWORD a = GetFileAttributesW(p); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }
static BOOL DirExists(const wchar_t *p) { DWORD a = GetFileAttributesW(p); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }
static BOOL DotName(const wchar_t *n) { return n[0] == L'.' && (!n[1] || (n[1] == L'.' && !n[2])); }
static BOOL DirHasFiles(const wchar_t *d)
{
    wchar_t q[PATHN]; Join(q, d, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd); BOOL any = FALSE;
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    do { if (!DotName(fd.cFileName)) any = TRUE; } while (!any && FindNextFileW(f, &fd));
    FindClose(f); return any;
}
static BOOL PathUnder(const wchar_t *p, const wchar_t *dir)   /* p is dir or inside it */
{
    size_t n = wcslen(dir); while (n && dir[n - 1] == L'\\') n--;
    return n && !_wcsnicmp(p, dir, n) && (p[n] == 0 || p[n] == L'\\');
}
static void Utf8Out(HANDLE h, const wchar_t *s)
{
    if (!h || h == INVALID_HANDLE_VALUE) return;
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL); if (n <= 1) return;
    char *u = (char *)malloc(n); if (!u) return;
    WideCharToMultiByte(CP_UTF8, 0, s, -1, u, n, NULL, NULL); DWORD wr; WriteFile(h, u, (DWORD)(n - 1), &wr, NULL); free(u);
}
static void Out(const wchar_t *s) { Utf8Out(GetStdHandle(STD_OUTPUT_HANDLE), s); Utf8Out(GetStdHandle(STD_OUTPUT_HANDLE), L"\r\n"); }   /* quiet modes: a GUI program still writes to a redirected stdout */

/* ---------------------------------------------------------------- the log: %TEMP%\portheon-setup.log, every step */
static void Log(const wchar_t *fmt, ...)
{
    wchar_t m[2048]; va_list a; va_start(a, fmt); _vsnwprintf(m, 2047, fmt, a); va_end(a); m[2047] = 0;
    SYSTEMTIME st; GetLocalTime(&st); wchar_t line[2200];
    _snwprintf(line, 2200, L"%04d-%02d-%02d %02d:%02d:%02d  %s\r\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, m); line[2199] = 0;
    EnterCriticalSection(&g_cs);
    HANDLE h = CreateFileW(g_logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { Utf8Out(h, line); CloseHandle(h); }
    LeaveCriticalSection(&g_cs);
}

/* what an error code means to a player (the texts the user asked for: disk full, access denied, folder in use) */
static const wchar_t *ErrWhy(DWORD e)
{
    switch (e) {
    case ERROR_DISK_FULL: case ERROR_HANDLE_DISK_FULL:
        return L2(L"o disco está cheio: libere espaço nessa unidade ou escolha uma pasta em outra unidade",
                  L"the disk is full: free some space on that drive or pick a folder on another drive");
    case ERROR_ACCESS_DENIED: case ERROR_WRITE_PROTECT: case ERROR_PRIVILEGE_NOT_HELD:
        return L2(L"acesso negado: a pasta é protegida, ou o antivírus (ou o \"Acesso controlado a pastas\" do Windows Defender) bloqueou a gravação. Escolha uma pasta como D:\\PORTHEON ou libere o instalador no antivírus",
                  L"access denied: the folder is protected, or the antivirus (or Windows Defender's \"Controlled folder access\") blocked the write. Pick a folder such as D:\\PORTHEON or allow the installer in the antivirus");
    case ERROR_SHARING_VIOLATION: case ERROR_LOCK_VIOLATION: case ERROR_USER_MAPPED_FILE: case ERROR_BUSY:
        return L2(L"um arquivo da pasta está em uso: feche o PORTHEON, os jogos e o antivírus e tente de novo",
                  L"a file in the folder is in use: close PORTHEON, the games and the antivirus and try again");
    case ERROR_FILENAME_EXCED_RANGE: case ERROR_BUFFER_OVERFLOW:
        return L2(L"o caminho da pasta é longo demais: escolha uma pasta mais curta, como D:\\PORTHEON",
                  L"the folder path is too long: pick a shorter folder, such as D:\\PORTHEON");
    case ERROR_PATH_NOT_FOUND:   /* what MAX_PATH APIs say when a path inside the folder passes 260 letters */
        return L2(L"uma pasta sumiu ou o caminho ficou longo demais: escolha uma pasta mais curta, como D:\\PORTHEON, e tente de novo",
                  L"a folder went missing or the path got too long: pick a shorter folder, such as D:\\PORTHEON, and try again");
    case ERROR_VIRUS_INFECTED: case ERROR_VIRUS_DELETED:
        return L2(L"o antivírus bloqueou um arquivo do pacote: restaure-o da quarentena e libere a pasta do PORTHEON",
                  L"the antivirus blocked a file of the package: restore it from its quarantine and allow PORTHEON's folder");
    }
    return NULL;
}
static void ErrText(DWORD e, wchar_t *out, int n)
{
    const wchar_t *w = ErrWhy(e);
    if (w) _snwprintf(out, n, L"%s (%lu)", w, e); else _snwprintf(out, n, L2(L"erro %lu do Windows", L"Windows error %lu"), e);
    out[n - 1] = 0;
}

/* ---------------------------------------------------------------- Windows (checks.c's WinBuild) */
static DWORD WinBuild(void)
{
    static DWORD b; if (b) return b;
    OSVERSIONINFOW ov = {sizeof(ov)};
    typedef LONG (WINAPI *RGV)(OSVERSIONINFOW *); RGV rgv = (RGV)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    if (rgv && !rgv(&ov)) b = ov.dwBuildNumber;
    return b ? b : 1;
}

/* ---------------------------------------------------------------- the processor (copied from portheon.c and checks.c, 5.2) */
static BOOL CpuAvxSaved(void)   /* Windows saves the AVX registers (OSXSAVE and XCR0 bits 1-2); xgetbv only with OSXSAVE */
{
    unsigned a, b, c, d; if (!__get_cpuid(1, &a, &b, &c, &d) || !((c >> 27) & 1)) return FALSE;
    unsigned lo, hi; __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0)); (void)hi;
    return (lo & 6) == 6;
}
/* the PS5 games' own code (UFC 5, UFC 6) runs natively and was built for the PS5's Zen 2 */
static BOOL CpuPs5Ok(wchar_t *miss, int n)
{
    static int cached = -1; static wchar_t names[160];
    if (cached < 0) {
        unsigned a, b, c, d, c1 = 0, b7 = 0, ce = 0;
        if (__get_cpuid(1, &a, &b, &c, &d)) c1 = c;
        if (__get_cpuid_count(7, 0, &a, &b, &c, &d)) b7 = b;
        if (__get_cpuid(0x80000001, &a, &b, &c, &d)) ce = c;
        names[0] = 0;
        if (!((c1 >> 20) & 1)) wcscat(names, L"SSE4.2, ");
        if (!((c1 >> 23) & 1)) wcscat(names, L"POPCNT, ");
        if (!((c1 >> 22) & 1)) wcscat(names, L"MOVBE, ");
        if (!((c1 >> 12) & 1)) wcscat(names, L"FMA, ");
        if (!((c1 >> 29) & 1)) wcscat(names, L"F16C, ");
        if (!((c1 >> 28) & 1)) wcscat(names, L"AVX, ");
        if (!((b7 >> 5) & 1)) wcscat(names, L"AVX2, ");
        if (!((b7 >> 3) & 1)) wcscat(names, L"BMI1, ");
        if (!((b7 >> 8) & 1)) wcscat(names, L"BMI2, ");
        if (!((ce >> 5) & 1)) wcscat(names, L"LZCNT, ");
        if (((c1 >> 28) & 1) && !CpuAvxSaved()) wcscat(names, L"XSAVE, ");
        size_t l = wcslen(names); if (l >= 2) names[l - 2] = 0;
        cached = names[0] == 0;
    }
    if (miss && n > 0) { wcsncpy(miss, names, n); miss[n - 1] = 0; }
    return cached;
}
/* the PS4 ports (UFC 1-4) run the PS4's Jaguar code: BMI1, MOVBE, LZCNT and AVX */
static BOOL CpuLacksPs4(wchar_t *out, int n)
{
    static int cached = -1; static wchar_t names[64];
    if (cached < 0) {
        unsigned a, b, c, d; BOOL avx = FALSE, movbe = FALSE, bmi1 = FALSE, lzcnt = FALSE;
        if (__get_cpuid(1, &a, &b, &c, &d)) { movbe = (c >> 22) & 1; avx = (c >> 28) & 1; }
        if (__get_cpuid_count(7, 0, &a, &b, &c, &d)) bmi1 = (b >> 3) & 1;
        if (__get_cpuid(0x80000001, &a, &b, &c, &d)) lzcnt = (c >> 5) & 1;
        names[0] = 0;
        if (avx && !CpuAvxSaved()) { avx = FALSE; wcscat(names, L"AVX (XSAVE), "); }
        else if (!avx) wcscat(names, L"AVX, ");
        if (!bmi1) wcscat(names, L"BMI1, ");
        if (!movbe) wcscat(names, L"MOVBE, ");
        if (!lzcnt) wcscat(names, L"LZCNT, ");
        size_t l = wcslen(names); if (l >= 2) names[l - 2] = 0;
        cached = names[0] != 0;
    }
    if (out && n > 0) { wcsncpy(out, names, n); out[n - 1] = 0; }
    return cached;
}
/* what PORTHEON's compatibility mode cannot translate (Intel Core 2nd/3rd generation: the plans of UFC 1-4) */
static BOOL CpuBaseOk(void)
{
    unsigned a, b, c, d;
    if (!__get_cpuid(1, &a, &b, &c, &d)) return FALSE;
    BOOL sse42 = (c >> 20) & 1, popcnt = (c >> 23) & 1, avx = (c >> 28) & 1, osx = (c >> 27) & 1;
    if (!avx || !osx || !sse42 || !popcnt) return FALSE;
    unsigned lo, hi; __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0)); (void)hi;
    return (lo & 6) == 6;
}
/* UFC Undisputed 3's recompiled Xbox 360 code: SSSE3, SSE4.1, SSE4.2, POPCNT */
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

/* ---------------------------------------------------------------- the graphics card through Vulkan (checks.c's VulkanCheckProbe
   and portheon.c's DetectGpu in one probe). A fresh Windows, the Microsoft Basic Display Adapter or a broken driver has no
   vulkan-1.dll at all: LoadLibrary fails and that is the answer ("install the graphics driver"). */
typedef void *(WINAPI *PFN_GIPA_)(void *, const char *);
typedef struct { int sType; const void *pNext; const char *app; uint32_t appVer; const char *eng; uint32_t engVer; uint32_t api; } VkAppInfo__;
typedef struct { int sType; const void *pNext; uint32_t flags; const VkAppInfo__ *app; uint32_t nl; const char *const *l; uint32_t ne; const char *const *e; } VkInstInfo__;
typedef struct { BOOL ran, loader, device, bary; uint32_t api, vendor, driver, drvId; uint64_t vram; wchar_t name[256], miss[200], drv[64]; } VkInfo;
static VkInfo g_vk;
#define VK13 ((1u << 22) | (3u << 12))
static void DriverDecode(uint32_t vendor, uint32_t v, char *out, int n)   /* driverVersion packed the vendor's way */
{
    if (vendor == 0x10DE) _snprintf(out, n, "%u.%02u", v >> 22, (v >> 14) & 0xff);
    else if (vendor == 0x8086) _snprintf(out, n, "%u.%u", v >> 14, v & 0x3fff);
    else _snprintf(out, n, "%u.%u.%u", v >> 22, (v >> 12) & 0x3ff, v & 0xfff);
    out[n - 1] = 0;
}
static const wchar_t *VendorName(uint32_t v) { return v == 0x10DE ? L"NVIDIA" : v == 0x1002 ? L"AMD" : v == 0x8086 ? L"Intel" : L""; }
static DWORD WINAPI VkProbe(LPVOID unused)
{
    (void)unused;
    HMODULE vk = LoadLibraryExW(L"vulkan-1.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!vk) return 0;
    g_vk.loader = TRUE;
    PFN_GIPA_ gipa = (PFN_GIPA_)(void *)GetProcAddress(vk, "vkGetInstanceProcAddr");
    int (WINAPI *create)(const VkInstInfo__ *, const void *, void **) = gipa ? (int (WINAPI *)(const VkInstInfo__ *, const void *, void **))gipa(NULL, "vkCreateInstance") : NULL;
    VkAppInfo__ ai = {0, NULL, "PORTHEON-Setup", 1, "none", 1, VK13}; VkInstInfo__ ii = {1, NULL, 0, &ai, 0, NULL, 0, NULL}; void *inst = NULL;
    BOOL inst13 = TRUE;
    if (!create || create(&ii, NULL, &inst) != 0 || !inst) {
        /* a Vulkan 1.0 loader refuses apiVersion 1.3: ask again as 1.0, to name the card and its old Vulkan */
        inst = NULL; ai.api = 1u << 22; inst13 = FALSE;
        if (!create || create(&ii, NULL, &inst) != 0 || !inst) { FreeLibrary(vk); return 0; }
    }
    int (WINAPI *enumDev)(void *, uint32_t *, void **) = (int (WINAPI *)(void *, uint32_t *, void **))gipa(inst, "vkEnumeratePhysicalDevices");
    void (WINAPI *props)(void *, void *) = (void (WINAPI *)(void *, void *))gipa(inst, "vkGetPhysicalDeviceProperties");
    void (WINAPI *mem)(void *, void *) = (void (WINAPI *)(void *, void *))gipa(inst, "vkGetPhysicalDeviceMemoryProperties");
    typedef struct { char name[256]; uint32_t spec; } ExtProp;
    int (WINAPI *devExt)(void *, const char *, uint32_t *, ExtProp *) = (int (WINAPI *)(void *, const char *, uint32_t *, ExtProp *))gipa(inst, "vkEnumerateDeviceExtensionProperties");
    void (WINAPI *feat2)(void *, void *) = inst13 ? (void (WINAPI *)(void *, void *))gipa(inst, "vkGetPhysicalDeviceFeatures2") : NULL;
    void (WINAPI *props2)(void *, void *) = inst13 ? (void (WINAPI *)(void *, void *))gipa(inst, "vkGetPhysicalDeviceProperties2") : NULL;
    void (WINAPI *destroy)(void *, const void *) = (void (WINAPI *)(void *, const void *))gipa(inst, "vkDestroyInstance");
    uint32_t n = 8; void *devs[8]; int best = -1, bestScore = -1;
    static unsigned char buf[8192];
    if (enumDev && props && enumDev(inst, &n, devs) >= 0) {
        for (uint32_t i = 0; i < n && i < 8; i++) {
            memset(buf, 0, sizeof(buf)); props(devs[i], buf);
            uint32_t api = *(uint32_t *)buf, type = *(uint32_t *)(buf + 16);
            if (type == 4) continue;   /* a CPU (software) device is no game card */
            int score = (api >= VK13 ? 4 : 0) + (type == 2 ? 2 : type == 1 ? 1 : 0);   /* what the engines pick */
            if (score > bestScore) { bestScore = score; best = (int)i; }
        }
    }
    if (best >= 0) {
        void *dv = devs[best]; memset(buf, 0, sizeof(buf)); props(dv, buf);
        g_vk.device = TRUE; g_vk.api = *(uint32_t *)buf; MultiByteToWideChar(CP_UTF8, 0, (char *)buf + 20, -1, g_vk.name, 256);
        g_vk.driver = *(uint32_t *)(buf + 4); g_vk.vendor = *(uint32_t *)(buf + 8);
        if (mem) {   /* VkPhysicalDeviceMemoryProperties: the biggest device-local heap is the video memory */
            memset(buf, 0, sizeof(buf)); mem(dv, buf);
            uint32_t nh = *(uint32_t *)(buf + 260);
            for (uint32_t h = 0; h < nh && h < 16; h++) { uint64_t size = *(uint64_t *)(buf + 264 + h * 16); uint32_t fl = *(uint32_t *)(buf + 272 + h * 16); if ((fl & 1) && size > g_vk.vram) g_vk.vram = size; }
        }
        {   /* VkPhysicalDeviceDriverProperties (Vulkan 1.2): the driver's version as its maker writes it */
            static struct { int sType; void *pNext; uint32_t id; char name[256], info[256]; uint8_t conf[4]; } dp;
            static struct { int sType; void *pNext; unsigned char props[1024]; } p2;
            char info[64] = "", num[32] = "";
            if (props2 && g_vk.api >= ((1u << 22) | (2u << 12))) {
                memset(&dp, 0, sizeof(dp)); memset(&p2, 0, sizeof(p2)); dp.sType = 1000196000; p2.sType = 1000059001; p2.pNext = &dp;
                props2(dv, &p2); dp.info[255] = 0; g_vk.drvId = dp.id;
                const char *s = dp.info; while (*s == ' ') s++; strncpy(info, s, 63); info[63] = 0;
            }
            if (info[0] >= '0' && info[0] <= '9') MultiByteToWideChar(CP_UTF8, 0, info, -1, g_vk.drv, 64);
            else { DriverDecode(g_vk.vendor, g_vk.driver, num, 32); MultiByteToWideChar(CP_UTF8, 0, num, -1, g_vk.drv, 64); }
            g_vk.drv[63] = 0;
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
        if (rob && feat2) {   /* robustBufferAccess2, robustImageAccess2, nullDescriptor */
            struct { int sType; void *pNext; uint32_t f[3]; } r2 = {1000286000, NULL, {0, 0, 0}};
            struct { int sType; void *pNext; uint32_t f[64]; } f2; memset(&f2, 0, sizeof(f2)); f2.sType = 1000059000; f2.pNext = &r2;
            feat2(dv, &f2);
            if (!r2.f[0] || !r2.f[1] || !r2.f[2]) rob = FALSE;
        } else if (!feat2) rob = rob && inst13;
        if (bar && feat2) {   /* what UFC 5's and UFC 6's main engines require: RX 5000 and GTX 10 cards lack it */
            struct { int sType; void *pNext; uint32_t f; } fb = {1000203000, NULL, 0};
            struct { int sType; void *pNext; uint32_t f[64]; } f2; memset(&f2, 0, sizeof(f2)); f2.sType = 1000059000; f2.pNext = &fb;
            feat2(dv, &f2); bar = fb.f != 0;
        } else if (!feat2) bar = FALSE;
        g_vk.bary = bar;
        wchar_t *mm = g_vk.miss;
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
/* portheon.c 4.3.3: a broken driver or an overlay's implicit layer can crash or hang inside vulkan-1.dll. The probe runs on
   a thread of its own without implicit layers; a fault ends that thread alone, a hang is given up after the timeout. */
static volatile DWORD g_guardTid;
static void GuardBail(void) { ExitThread(1); }
static LONG CALLBACK GuardVeh(EXCEPTION_POINTERS *e)
{
    DWORD c = e->ExceptionRecord->ExceptionCode;
    if (!g_guardTid || GetCurrentThreadId() != g_guardTid) return EXCEPTION_CONTINUE_SEARCH;
    if (c != EXCEPTION_ACCESS_VIOLATION && c != EXCEPTION_ILLEGAL_INSTRUCTION && c != EXCEPTION_PRIV_INSTRUCTION &&
        c != EXCEPTION_INT_DIVIDE_BY_ZERO && c != EXCEPTION_IN_PAGE_ERROR && c != EXCEPTION_DATATYPE_MISALIGNMENT) return EXCEPTION_CONTINUE_SEARCH;
    e->ContextRecord->Rsp = (e->ContextRecord->Rsp & ~(DWORD64)15) - 0x88;
    e->ContextRecord->Rip = (DWORD64)(UINT_PTR)GuardBail;
    return EXCEPTION_CONTINUE_EXECUTION;
}
static BOOL RunGuarded(LPTHREAD_START_ROUTINE fn, DWORD ms)
{
    wchar_t old[256]; DWORD had = GetEnvironmentVariableW(L"VK_LOADER_LAYERS_DISABLE", old, 256);
    SetEnvironmentVariableW(L"VK_LOADER_LAYERS_DISABLE", L"~implicit~");
    static PVOID veh; if (!veh) veh = AddVectoredExceptionHandler(1, GuardVeh);
    DWORD tid = 0; HANDLE t = CreateThread(NULL, 1 << 20, fn, NULL, CREATE_SUSPENDED, &tid); BOOL ok = FALSE;
    if (t) {
        g_guardTid = tid; ResumeThread(t);
        if (WaitForSingleObject(t, ms) == WAIT_OBJECT_0) { DWORD code = 1; GetExitCodeThread(t, &code); ok = code == 0; g_guardTid = 0; }
        CloseHandle(t);
    }
    SetEnvironmentVariableW(L"VK_LOADER_LAYERS_DISABLE", had && had < 256 ? old : NULL);
    return ok;
}

/* ---------------------------------------------------------------- this PC and what it runs */
static wchar_t g_cpuName[64], g_adapter[128]; static int g_ramGb; static USHORT g_native;
static volatile LONG g_checksDone;
static void PcRead(void)
{
    unsigned r[12] = {0}; char s[49];
    for (int i = 0; i < 3; i++) __get_cpuid(0x80000002 + i, &r[i * 4], &r[i * 4 + 1], &r[i * 4 + 2], &r[i * 4 + 3]);
    memcpy(s, r, 48); s[48] = 0;
    { const char *t = s; while (*t == ' ') t++; MultiByteToWideChar(CP_ACP, 0, t, -1, g_cpuName, 64); g_cpuName[63] = 0;
      size_t l = wcslen(g_cpuName); while (l && g_cpuName[l - 1] == L' ') g_cpuName[--l] = 0; if (!l) wcscpy(g_cpuName, L"?"); }
    MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms); g_ramGb = (int)((ms.ullTotalPhys + (512ull << 20)) >> 30);
    {   /* an ARM64 Windows runs this x64 program emulated: the games' native x64 code would be emulated too */
        typedef BOOL (WINAPI *W2)(HANDLE, USHORT *, USHORT *); W2 w2 = (W2)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsWow64Process2");
        USHORT pm = 0, nm = 0; if (w2 && w2(GetCurrentProcess(), &pm, &nm)) g_native = nm;
    }
    {   DISPLAY_DEVICEW dd = {sizeof(dd)};   /* the adapter Windows shows, for a PC whose Vulkan says nothing */
        for (DWORD i = 0; EnumDisplayDevicesW(NULL, i, &dd, 0); i++) if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) { wcsncpy(g_adapter, dd.DeviceString, 127); g_adapter[127] = 0; break; }
    }
    memset(&g_vk, 0, sizeof(g_vk));
    BOOL ran = RunGuarded(VkProbe, 15000);
    if (!ran) { BOOL loader = g_vk.loader; memset(&g_vk, 0, sizeof(g_vk)); g_vk.loader = loader; }
    g_vk.ran = ran;
    Log(L"PC: Windows build %lu, native machine 0x%04x, CPU \"%s\", RAM %d GB, adapter \"%s\"", WinBuild(), g_native, g_cpuName, g_ramGb, g_adapter);
    Log(L"Vulkan: probe %s, loader %d, device %d, \"%s\", API %u.%u, vendor 0x%04x, driver %s, VRAM %llu MB, barycentrics %d, missing [%s]",
        ran ? L"ok" : L"FAILED (crash or timeout)", g_vk.loader, g_vk.device, g_vk.name, g_vk.api >> 22, (g_vk.api >> 12) & 0x3ff, g_vk.vendor, g_vk.drv,
        (unsigned long long)(g_vk.vram >> 20), g_vk.bary, g_vk.miss);
}
enum { K_PS4, K_UFC5, K_UFC6, K_X360, K_SWITCH, K_GOW3, K_BB, K_ISS };   /* 6.0: Zelda (an emulator / native build), God of War III (PS4 code, no compatibility mode); 6.1: Bloodborne (AVX, the rest translated), inFAMOUS (no rule stated) */
#define NG 11
static const struct { const wchar_t *name; int kind; } GAMES[NG] = {
    {L"UFC 1", K_PS4}, {L"UFC 2", K_PS4}, {L"UFC 3", K_PS4}, {L"UFC 4", K_PS4}, {L"UFC Undisputed 3", K_X360}, {L"UFC 5", K_UFC5}, {L"UFC 6", K_UFC6},
    {L"Zelda: Ocarina of Time", K_SWITCH}, {L"God of War III", K_GOW3}, {L"Bloodborne", K_BB}, {L"inFAMOUS Second Son", K_ISS}};
static int g_lv[NG]; static wchar_t g_why[NG][900];
static void Why(int g, int level, const wchar_t *fmt, ...)
{
    wchar_t t[600]; va_list a; va_start(a, fmt); _vsnwprintf(t, 599, fmt, a); va_end(a); t[599] = 0;
    size_t l = wcslen(g_why[g]); if (l + wcslen(t) + 4 < 900) { if (l) wcscat(g_why[g], L" "); wcscat(g_why[g], t); }
    if (level > g_lv[g]) g_lv[g] = level;
}
/* the rules of checks.c's PcVerdict, per game: 0 runs, 1 runs with a warning, 2 does not run */
static void Verdicts(void)
{
    wchar_t m[160];
    for (int g = 0; g < NG; g++) {
        int k = GAMES[g].kind; g_lv[g] = 0; g_why[g][0] = 0;
        if (WinBuild() < 17763 && k != K_X360)   /* Windows 10 1809: CreateFileMapping2; the engines' manifests want 1903 */
            Why(g, 2, L2(L"Windows antigo demais (versão %lu): os ports precisam do Windows 10 1903 ou mais novo, ou do Windows 11.", L"Windows too old (build %lu): the ports need Windows 10 1903 or newer, or Windows 11."), WinBuild());
        if (g_native == IMAGE_FILE_MACHINE_ARM64)
            Why(g, 1, L"%s", L2(L"Windows em ARM: o jogo roda emulado, bem mais lento (e pode não abrir).", L"Windows on ARM: the game runs emulated, much slower (and may not start)."));
        /* the processor */
        if (k == K_X360) { if (!CpuX360Ok(m, 160)) Why(g, 2, L2(L"O processador não tem %s (precisa de um Intel Core de 2008 ou mais novo, AMD FX ou Ryzen).", L"The processor lacks %s (it needs an Intel Core from 2008 or newer, AMD FX or Ryzen)."), m); }
        else if (k == K_UFC5 || k == K_UFC6) {
            if (!CpuPs5Ok(m, 160)) Why(g, 2, L2(L"O processador não tem %s, que o código de PS5 usa direto (precisa de um Intel Core de 4ª geração ou mais novo, ou de um AMD Ryzen).", L"The processor lacks %s, which the PS5 code uses directly (it needs an Intel Core 4th generation or newer, or an AMD Ryzen)."), m);
        } else if (k == K_BB) {   /* 6.1: the port translates BMI1/MOVBE/LZCNT (Sandy/Ivy Bridge, best effort) but needs AVX */
            if (CpuLacksPs4(m, 160)) { if (wcsstr(m, L"AVX")) Why(g, 2, L2(L"O processador não tem %s: o port do Bloodborne precisa de AVX.", L"The processor lacks %s: the Bloodborne port needs AVX."), m);
                                       else Why(g, 1, L2(L"O processador não tem %s: o port do Bloodborne traduz essas instruções (melhor esforço).", L"The processor lacks %s: the Bloodborne port translates those instructions (best effort)."), m); }
        } else if (k == K_ISS) {   /* 6.1: its README states no CPU rule; the PS4 code very likely uses them */
            if (CpuLacksPs4(m, 160)) Why(g, 1, L2(L"O processador não tem %s: o inFAMOUS pode não abrir.", L"The processor lacks %s: inFAMOUS may not start."), m);
        } else if (k == K_SWITCH) {   /* 6.0: the emulator and the recompiled game have no instruction rule */
        } else if (k == K_GOW3) {
            if (CpuLacksPs4(m, 160)) Why(g, 2, L2(L"O processador não tem %s, que o código do God of War III (PS4) usa direto: precisa de um Intel Core de 4ª geração ou mais novo, ou de um AMD Ryzen.", L"The processor lacks %s, which God of War III's (PS4) code uses directly: it needs an Intel Core 4th generation or newer, or an AMD Ryzen."), m);
        } else if (CpuLacksPs4(m, 160)) {
            if (CpuBaseOk()) Why(g, 1, L2(L"O processador não tem %s: roda no modo de compatibilidade do PORTHEON (a primeira preparação demora mais).", L"The processor lacks %s: it runs in PORTHEON's compatibility mode (the first preparation takes longer)."), m);
            else Why(g, 2, L2(L"O processador não tem %s (nem AVX/SSE4.2, que o modo de compatibilidade não traduz): precisa de um Intel Core de 2ª geração ou mais novo, AMD FX ou Ryzen.", L"The processor lacks %s (and AVX/SSE4.2, which the compatibility mode cannot translate): it needs an Intel Core 2nd generation or newer, AMD FX or Ryzen."), m);
        }
        /* the card and its Vulkan */
        if (!g_vk.loader) Why(g, 2, L"%s", L2(L"Sem Vulkan (falta o vulkan-1.dll): o driver da placa de vídeo não está instalado. Instale o driver mais novo do site da NVIDIA, da AMD ou da Intel.", L"No Vulkan (vulkan-1.dll is missing): the graphics driver is not installed. Install the newest driver from NVIDIA's, AMD's or Intel's site."));
        else if (!g_vk.ran) Why(g, 1, L"%s", L2(L"Não consegui ler a placa de vídeo pelo Vulkan (o driver travou no teste): atualize o driver da placa.", L"Could not read the graphics card through Vulkan (the driver hung or crashed in the test): update the card's driver."));
        else if (!g_vk.device) Why(g, 2, L"%s", L2(L"O Vulkan não encontrou nenhuma placa de vídeo: instale o driver da placa (o \"Adaptador de vídeo básico da Microsoft\" não roda jogos).", L"Vulkan found no graphics card: install the card's driver (the \"Microsoft Basic Display Adapter\" runs no games)."));
        else if (k != K_X360 && k != K_SWITCH) {   /* 6.0: the Switch emulator has no Vulkan 1.3 rule */
            if (g_vk.api < VK13 && !wcsncmp(g_vk.name, L"Microsoft Direct3D12", 20))
                Why(g, 2, L"%s", L2(L"O Vulkan deste PC é o da Microsoft (Pacote de compatibilidade OpenCL, OpenGL e Vulkan), não o da placa: instale o driver da placa pelo site do fabricante.", L"This PC's Vulkan is Microsoft's (OpenCL, OpenGL and Vulkan Compatibility Pack), not the card's: install the card's driver from its maker's site."));
            else if (g_vk.api < VK13) Why(g, 2, L2(L"A placa tem Vulkan %u.%u e o jogo precisa do 1.3: atualize o driver (placas muito antigas não chegam no 1.3).", L"The card has Vulkan %u.%u and the game needs 1.3: update the driver (very old cards never reach 1.3)."), g_vk.api >> 22, (g_vk.api >> 12) & 0x3ff);
            else if (g_vk.miss[0]) Why(g, 2, L2(L"A placa não tem recursos do Vulkan que o motor exige (%s): atualize o driver da placa.", L"The card lacks Vulkan features the engine requires (%s): update the card's driver."), g_vk.miss);
            else if (k == K_UFC5 && !g_vk.bary) Why(g, 2, L"%s", L2(L"A placa não tem o recurso que o motor do UFC 5 exige (baricêntricas): as RX 5000 e GTX 10 não têm, nem com o driver mais novo.", L"The card lacks the feature UFC 5's engine requires (barycentrics): RX 5000 and GTX 10 cards lack it, even on the newest driver."));
            else if (k == K_UFC6 && !g_vk.bary) Why(g, 1, L"%s", L2(L"A placa não tem baricêntricas: o PORTHEON usa o motor de compatibilidade do UFC 6 (experimental).", L"The card has no barycentrics: PORTHEON uses UFC 6's compatibility engine (experimental)."));
        }
        /* memory */
        int vram = (int)(g_vk.vram >> 30);
        if (k == K_UFC5 || k == K_UFC6) {
            if (g_ramGb && g_ramGb < 16) Why(g, 1, L2(L"RAM: %d GB (o jogo usa de 12 a 14 GB: feche outros programas).", L"RAM: %d GB (the game uses 12 to 14 GB: close other programs)."), g_ramGb);
            if (vram && vram < 6) Why(g, 1, L2(L"Memória de vídeo: %d GB (use o preset Rápido; abaixo de 6 GB pode fechar nas trocas de cena).", L"Video memory: %d GB (use the Fast preset; below 6 GB it may close at scene changes)."), vram);
        } else {
            if (g_ramGb && g_ramGb < 8) Why(g, 1, L2(L"RAM: %d GB (abaixo de 8 GB feche outros programas antes de jogar).", L"RAM: %d GB (below 8 GB close other programs before playing)."), g_ramGb);
            if (vram && vram < 2 && k == K_PS4) Why(g, 1, L2(L"Memória de vídeo: %d GB (use o modo Ultra desempenho).", L"Video memory: %d GB (use the Ultra performance mode)."), vram);
        }
    }
}
static const wchar_t *LevelName(int lv) { return lv == 0 ? L2(L"Roda", L"Runs") : lv == 1 ? L2(L"Roda com aviso", L"Runs with a warning") : L2(L"Não roda", L"Does not run"); }
static void PcSummary(wchar_t *out, int n)
{
    DWORD b = WinBuild(); wchar_t gpu[600];
    if (!g_vk.loader) _snwprintf(gpu, 600, L2(L"%s - sem Vulkan: instale o driver da placa de vídeo", L"%s - no Vulkan: install the graphics driver"), g_adapter[0] ? g_adapter : L"?");
    else if (!g_vk.device) _snwprintf(gpu, 600, L2(L"%s - o Vulkan não achou placa de vídeo", L"%s - Vulkan found no graphics card"), g_adapter[0] ? g_adapter : L"?");
    else { const wchar_t *vn = VendorName(g_vk.vendor);
        _snwprintf(gpu, 600, L2(L"%s, %llu GB, driver %s%s%s, Vulkan %u.%u", L"%s, %llu GB, driver %s%s%s, Vulkan %u.%u"), g_vk.name, (unsigned long long)((g_vk.vram + (512ull << 20)) >> 30),
                   vn, vn[0] ? L" " : L"", g_vk.drv, g_vk.api >> 22, (g_vk.api >> 12) & 0x3ff); }
    gpu[599] = 0;
    _snwprintf(out, n, L2(L"Windows %s (versão %lu)%s\r\nProcessador: %s\r\nPlaca de vídeo: %s\r\nRAM: %d GB",
                          L"Windows %s (build %lu)%s\r\nProcessor: %s\r\nGraphics card: %s\r\nRAM: %d GB"),
               b >= 22000 ? L"11" : L"10", b, g_native == IMAGE_FILE_MACHINE_ARM64 ? L" ARM64" : L", 64 bits", g_cpuName, gpu, g_ramGb);
    out[n - 1] = 0;
}

/* ---------------------------------------------------------------- the package: the zip after the program */
#pragma pack(push, 1)
typedef struct { char magic[16]; uint64_t off, size; uint8_t sha[32]; } Footer;
#pragma pack(pop)
_Static_assert(sizeof(Footer) == 64, "the footer is 64 bytes");
typedef struct { wchar_t file[PATHN]; uint64_t off, size; uint8_t sha[32]; BOOL haveSha; } Payload;
static Payload g_pl; static BOOL g_hasPayload; static uint64_t g_stubLen;
static BOOL ReadAt(HANDLE f, uint64_t off, void *buf, DWORD n)
{
    LARGE_INTEGER li; li.QuadPart = (LONGLONG)off; DWORD got = 0;
    return SetFilePointerEx(f, li, NULL, FILE_BEGIN) && ReadFile(f, buf, n, &got, NULL) && got == n;
}
static BOOL ReadFooter(const wchar_t *exe, Footer *ft, uint64_t *fileSize)
{
    HANDLE f = CreateFileW(exe, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    LARGE_INTEGER sz; BOOL ok = GetFileSizeEx(f, &sz) && sz.QuadPart > 4096 + 64 && ReadAt(f, (uint64_t)sz.QuadPart - 64, ft, 64);
    CloseHandle(f);
    if (fileSize) *fileSize = ok ? (uint64_t)sz.QuadPart : 0;
    return ok && !memcmp(ft->magic, "PORTHEON-PAYLOAD", 16) && ft->off >= 4096 && ft->size > 22 && ft->off + ft->size + 64 == (uint64_t)sz.QuadPart;
}
/* what the zip holds, from its central directory: entries, uncompressed bytes (the disk it needs), the top folder
   ("PORTHEON-5.2" -> the version), the longest name, and whether a player's file got into it (package.ps1 leaves out
   every "_" folder - _SaveData, _PipelineCache, _ShaderCache... - and every .ini/.log but the four shipped presets) */
typedef struct { BOOL ok; uint64_t total; DWORD files, entries, maxName; wchar_t top[128], ver[64], playerEx[300]; int player;
                 DWORD runtimeFiles; wchar_t *names; size_t namesLen, namesCap; } ZipInfo;   /* names: every file below the top folder, "a\\b.dll\0...\0" */
static ZipInfo g_zip;
/* package.ps1's rule for what the engines create at run time: a FOLDER named "_" + letters/underscores (_SaveData,
   _PipelineCache, _ShaderCache, _Logs...). Files may start with "_" and ship (Python's _ctypes.pyd, __init__.py). */
static BOOL RuntimeDirName(const wchar_t *s, size_t n)
{
    if (n < 2 || s[0] != L'_') return FALSE;
    for (size_t i = 1; i < n; i++) if (!((s[i] >= L'A' && s[i] <= L'Z') || (s[i] >= L'a' && s[i] <= L'z') || s[i] == L'_')) return FALSE;
    return TRUE;
}
/* 6.0: games\ZELDA (Lichii, Ocarina Recomp with llvm's "__algorithm"... header folders) and games\GOW3 (its launcher's
   PyInstaller runtime in "_internal") are third-party trees shipped as they are: their "_" folders and the recompiler's
   own totk.ini template are program files, not the player's (package.ps1 ships them the same way) */
static BOOL ThirdPartyZipName(const char *nm)
{
    const char *s = strchr(nm, '/'); if (!s) return FALSE; s++;   /* after "PORTHEON-x.y/" */
    return !_strnicmp(s, "games/ZELDA/", 12) || !_strnicmp(s, "games/GOW3/", 11) || !_strnicmp(s, "games/BLOODBORNE/", 17) || !_strnicmp(s, "games/INFAMOUS/", 15);   /* 6.1 */
}
static BOOL ThirdPartyRel(const wchar_t *rel) { return !_wcsnicmp(rel, L"games\\ZELDA\\", 12) || !_wcsnicmp(rel, L"games\\GOW3\\", 11) || !_wcsnicmp(rel, L"games\\BLOODBORNE\\", 17) || !_wcsnicmp(rel, L"games\\INFAMOUS\\", 15); }
static int ZipPlayerName(const char *nm)
{
    if (ThirdPartyZipName(nm)) return 0;   /* 6.0 */
    const char *seg = nm;
    for (const char *p = nm; *p; p++) if (*p == '/') {   /* folder segments only: the part after the last '/' is the file */
        wchar_t w[260]; int n = (int)(p - seg); if (n > 0 && n < 259) { for (int i = 0; i < n; i++) w[i] = (wchar_t)(unsigned char)seg[i]; if (RuntimeDirName(w, (size_t)n)) return 2; }
        seg = p + 1;
    }
    const char *base = strrchr(nm, '/'); base = base ? base + 1 : nm; size_t l = strlen(base);
    if (l > 4 && (!_stricmp(base + l - 4, ".ini") || !_stricmp(base + l - 4, ".log")))
        return (_stricmp(base, "ReShade.ini") && _stricmp(base, "ReShadePreset.ini") && _stricmp(base, "AeonSR.ini") && _stricmp(base, "kyty_settings.ini")) ? 1 : 0;
    return FALSE;
}
static BOOL ZipScan(const Payload *pl, ZipInfo *z)
{
    memset(z, 0, sizeof(*z));
    HANDLE f = CreateFileW(pl->file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    DWORD tail = pl->size < 65557 ? (DWORD)pl->size : 65557; BYTE *t = (BYTE *)malloc(tail), *cd = NULL; BOOL ok = FALSE;
    if (!t || !ReadAt(f, pl->off + pl->size - tail, t, tail)) goto done;
    int e = -1; for (int i = (int)tail - 22; i >= 0; i--) if (t[i] == 'P' && t[i + 1] == 'K' && t[i + 2] == 5 && t[i + 3] == 6) { e = i; break; }
    if (e < 0) goto done;
    uint64_t n = *(uint16_t *)(t + e + 10), cdSize = *(uint32_t *)(t + e + 12), cdOff = *(uint32_t *)(t + e + 16);
    if (n == 0xFFFF || cdSize == 0xFFFFFFFF || cdOff == 0xFFFFFFFF) {   /* zip64: the locator 20 bytes before, then the zip64 record */
        BYTE z64[56]; uint64_t eocd = pl->size - tail + (uint64_t)e;
        if (eocd < 20 || e < 20 || *(uint32_t *)(t + e - 20) != 0x07064b50) goto done;
        uint64_t r = *(uint64_t *)(t + e - 20 + 8);
        if (r + 56 > pl->size || !ReadAt(f, pl->off + r, z64, 56) || *(uint32_t *)z64 != 0x06064b50) goto done;
        n = *(uint64_t *)(z64 + 32); cdSize = *(uint64_t *)(z64 + 40); cdOff = *(uint64_t *)(z64 + 48);
    }
    if (cdOff + cdSize > pl->size || cdSize > (256u << 20) || !(cd = (BYTE *)malloc((size_t)cdSize + 1)) || !ReadAt(f, pl->off + cdOff, cd, (DWORD)cdSize)) goto done;
    uint64_t p = 0;
    for (uint64_t i = 0; i < n; i++) {
        if (p + 46 > cdSize || *(uint32_t *)(cd + p) != 0x02014b50) goto done;
        uint64_t us = *(uint32_t *)(cd + p + 24); uint32_t nl = *(uint16_t *)(cd + p + 28), xl = *(uint16_t *)(cd + p + 30), cl = *(uint16_t *)(cd + p + 32);
        if (p + 46 + nl + xl + cl > cdSize || nl == 0 || nl > 1000) goto done;
        if (us == 0xFFFFFFFF) {   /* the zip64 extra field (0x0001): the uncompressed size first */
            for (uint32_t x = 0; x + 4 <= xl; ) { uint16_t id = *(uint16_t *)(cd + p + 46 + nl + x), sz = *(uint16_t *)(cd + p + 46 + nl + x + 2);
                if (id == 1 && sz >= 8) { us = *(uint64_t *)(cd + p + 46 + nl + x + 4); break; } x += 4u + sz; }
        }
        char nm[1001]; memcpy(nm, cd + p + 46, nl); nm[nl] = 0;
        z->entries++;
        if (nm[nl - 1] != '/') {
            z->files++; z->total += us;
            {   /* the list the uninstaller deletes by (portheon-files.txt): the path below the top folder, '\' separated */
                const char *rel = strchr(nm, '/'); wchar_t w[1001]; int wn = rel ? MultiByteToWideChar(CP_UTF8, 0, rel + 1, -1, w, 1001) : 0;
                if (wn > 1) {
                    for (int k = 0; k < wn; k++) if (w[k] == L'/') w[k] = L'\\';
                    if (z->namesLen + (size_t)wn + 1 > z->namesCap) {
                        size_t nc = z->namesCap ? z->namesCap * 2 : 65536; while (nc < z->namesLen + (size_t)wn + 1) nc *= 2;
                        wchar_t *nn = (wchar_t *)realloc(z->names, nc * sizeof(wchar_t)); if (!nn) goto done; z->names = nn; z->namesCap = nc;
                    }
                    memcpy(z->names + z->namesLen, w, (size_t)wn * sizeof(wchar_t)); z->namesLen += (size_t)wn; z->names[z->namesLen] = 0;
                }
            }
            if (nl > z->maxName) z->maxName = nl;
            { int pk = ZipPlayerName(nm); if (pk == 2) z->runtimeFiles++;   /* MoveTree never moves those: the file count check leaves them out */
              if (!z->player && pk) { z->player = 1; MultiByteToWideChar(CP_UTF8, 0, nm, -1, z->playerEx, 300); z->playerEx[299] = 0; } }
        }
        if (i == 0) { char *s = strchr(nm, '/'); if (s) *s = 0; MultiByteToWideChar(CP_UTF8, 0, nm, -1, z->top, 128); z->top[127] = 0; }
        p += 46 + nl + xl + cl;
    }
    if (!z->top[0]) goto done;
    wcscpy(z->ver, !_wcsnicmp(z->top, L"PORTHEON-", 9) ? z->top + 9 : z->top);
    ok = z->ok = TRUE;
done:
    free(t); free(cd); CloseHandle(f); return ok;
}
static void HexSha(const uint8_t *d, wchar_t *out) { for (int i = 0; i < 32; i++) _snwprintf(out + i * 2, 3, L"%02x", d[i]); out[64] = 0; }

/* ---------------------------------------------------------------- progress and status, from the worker to the window */
#define WM_APP_PROG (WM_APP + 1)
#define WM_APP_STATUS (WM_APP + 2)
#define WM_APP_DONE (WM_APP + 3)
#define WM_APP_CHECKS (WM_APP + 4)
static wchar_t g_status[1600]; static volatile LONG g_cancel; static HANDLE g_tarProc;
static void SetStatus(const wchar_t *fmt, ...)
{
    wchar_t t[1600]; va_list a; va_start(a, fmt); _vsnwprintf(t, 1599, fmt, a); va_end(a); t[1599] = 0;
    EnterCriticalSection(&g_cs); wcscpy(g_status, t); LeaveCriticalSection(&g_cs);
    Log(L"status: %s", t);
    if (g_quiet) Out(t);
    if (g_wnd) PostMessageW(g_wnd, WM_APP_STATUS, 0, 0);
}
static void Prog(int permille)
{
    static int last = -1; if (permille < 0) permille = 0; if (permille > 1000) permille = 1000;
    if (permille == last) return; last = permille;
    if (g_wnd) PostMessageW(g_wnd, WM_APP_PROG, (WPARAM)permille, 0);
}

/* ---------------------------------------------------------------- disk */
static BOOL FreeOn(const wchar_t *path, uint64_t *fr)   /* the free space of the drive that holds path (it may not exist yet) */
{
    wchar_t p[PATHN]; wcsncpy(p, path, PATHN - 1); p[PATHN - 1] = 0;
    for (;;) {
        ULARGE_INTEGER a, t, f;
        if (DirExists(p) && GetDiskFreeSpaceExW(p, &a, &t, &f)) { *fr = a.QuadPart; return TRUE; }
        wchar_t *s = wcsrchr(p, L'\\'); if (!s) return FALSE;
        if (s == p + 2) { s[1] = 0; if (GetDiskFreeSpaceExW(p, &a, &t, &f)) { *fr = a.QuadPart; return TRUE; } return FALSE; }
        *s = 0;
    }
}
static uint64_t NeedBytes(void)   /* the files, 5% for the cluster slack, and the temp copy of the zip (about zip x 2.2 in all) */
{
    uint64_t z = g_pl.size, t = g_zip.ok ? g_zip.total : z * 12 / 10;
    return t + t / 20 + z;
}
static void Gb(uint64_t b, wchar_t *out) { _snwprintf(out, 32, L"%.1f GB", b / 1073741824.0); out[31] = 0; }
/* the fixed drive with the most free space: PORTHEON grows (shader caches, UFC 5/6 saves) and the games beside it */
static void DefaultFolder(wchar_t *out)
{
    DWORD m = GetLogicalDrives(); uint64_t best = 0; wchar_t bd = L'C';
    for (int i = 2; i < 26; i++) {
        if (!(m & (1u << i))) continue;
        wchar_t r[4] = {(wchar_t)(L'A' + i), L':', L'\\', 0}; ULARGE_INTEGER a, t, f;
        if (GetDriveTypeW(r) != DRIVE_FIXED || !GetDiskFreeSpaceExW(r, &a, &t, &f)) continue;
        if (a.QuadPart > best) { best = a.QuadPart; bd = r[0]; }
    }
    _snwprintf(out, PATHN, L"%c:\\PORTHEON", bd);
}

/* ---------------------------------------------------------------- the folder rules
   0: fine, 1: fine with a warning (msg), 2: refused (msg). out: the folder that will be used. */
static BOOL KnownDir(REFKNOWNFOLDERID id, wchar_t *out)
{
    PWSTR p = NULL; out[0] = 0;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, NULL, &p)) && p) { wcsncpy(out, p, PATHN - 1); out[PATHN - 1] = 0; }
    if (p) CoTaskMemFree(p); return out[0] != 0;
}
static BOOL IsPortheonDir(const wchar_t *d)
{
    wchar_t e[PATHN]; Join(e, d, L"PORTHEON.exe"); if (FileExists(e)) return TRUE;
    Join(e, d, UNINST_EXE); return FileExists(e);
}
static BOOL InOneDrive(const wchar_t *p)
{
    static const wchar_t *const vars[] = {L"OneDrive", L"OneDriveConsumer", L"OneDriveCommercial"};
    for (int i = 0; i < 3; i++) { wchar_t v[PATHN]; DWORD n = GetEnvironmentVariableW(vars[i], v, PATHN); if (n && n < PATHN && PathUnder(p, v)) return TRUE; }
    return StrStrIW(p, L"\\OneDrive\\") != NULL || StrStrIW(p, L"\\OneDrive -") != NULL;
}
/* the long form of a path (C:\PROGRA~1 -> C:\Program Files), so two spellings of one folder compare equal */
static void LongForm(const wchar_t *in, wchar_t *out)
{
    wchar_t t[PATHN]; DWORD n = GetLongPathNameW(in, t, PATHN);
    if (n && n < PATHN) wcscpy(out, t); else { wcsncpy(out, in, PATHN - 1); out[PATHN - 1] = 0; }
    size_t l = wcslen(out); while (l > 3 && out[l - 1] == L'\\') out[--l] = 0;
}
/* a folder that is, or holds, one of Windows' own user or system folders: never a PORTHEON folder of its own (an install
   there gets a PORTHEON subfolder; the uninstaller never empties it). which: the folder it clashes with. */
static BOOL ShellFolderClash(const wchar_t *dir, wchar_t *which)
{
    static const KNOWNFOLDERID *const ids[] = {&FOLDERID_Windows, &FOLDERID_System, &FOLDERID_ProgramFiles, &FOLDERID_ProgramFilesX86, &FOLDERID_ProgramData,
        &FOLDERID_UserProfiles, &FOLDERID_Profile, &FOLDERID_Public, &FOLDERID_Desktop, &FOLDERID_Documents, &FOLDERID_Downloads, &FOLDERID_Music,
        &FOLDERID_Pictures, &FOLDERID_Videos, &FOLDERID_SavedGames, &FOLDERID_SkyDrive, &FOLDERID_LocalAppData, &FOLDERID_RoamingAppData, &FOLDERID_Programs};
    wchar_t d[PATHN]; LongForm(dir, d);
    if (wcslen(d) <= 3) { if (which) wcscpy(which, d); return TRUE; }   /* a drive's root */
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        wchar_t k[PATHN], kl[PATHN]; if (!KnownDir(ids[i], k)) continue; LongForm(k, kl);
        if (PathUnder(kl, d)) { if (which) wcscpy(which, kl); return TRUE; }   /* d is that folder or one of its parents */
    }
    static const wchar_t *const vars[] = {L"OneDrive", L"OneDriveConsumer", L"OneDriveCommercial"};
    for (int i = 0; i < 3; i++) { wchar_t v[PATHN], vl[PATHN]; DWORD n = GetEnvironmentVariableW(vars[i], v, PATHN);
        if (n && n < PATHN) { LongForm(v, vl); if (PathUnder(vl, d)) { if (which) wcscpy(which, vl); return TRUE; } } }
    return FALSE;
}
/* the stage folder the zip is extracted into before each file moves to its place: <dir>\.pst\x\<top>\...; short, because
   that path is longer than the final one and this program (like the ports) lives with the 260-letter limit */
#define STAGE_EXTRA 7   /* "\.pst\x" */
static int ResolveTarget(const wchar_t *in, wchar_t *out, wchar_t *msg, int msgN)
{
    wchar_t t[PATHN]; msg[0] = 0; int lv = 0;
    while (*in == L' ' || *in == L'"') in++;
    wcsncpy(t, in, PATHN - 1); t[PATHN - 1] = 0;
    for (size_t l = wcslen(t); l && (t[l - 1] == L' ' || t[l - 1] == L'"'); ) t[--l] = 0;
    for (wchar_t *c = t; *c; c++) if (*c == L'/') *c = L'\\';
    if (!t[0]) { _snwprintf(msg, msgN, L"%s", L2(L"Escolha a pasta onde instalar.", L"Pick the folder to install into.")); return 2; }
    if (!GetFullPathNameW(t, PATHN, out, NULL)) wcscpy(out, t);
    if (!(iswalpha(out[0]) && out[1] == L':' && out[2] == L'\\')) { _snwprintf(msg, msgN, L"%s", L2(L"Use uma pasta numa unidade deste PC (C:, D:...), não da rede.", L"Use a folder on a drive of this PC (C:, D:...), not on the network.")); return 2; }
    out[0] = towupper(out[0]);
    for (size_t l = wcslen(out); l > 3 && out[l - 1] == L'\\'; ) out[--l] = 0;
    if (wcslen(out) == 3) wcscat(out, L"PORTHEON");   /* a drive's root: its own folder, never files all over D:\ */
    {   /* the app writes next to itself (saves, caches, settings): Program Files and Windows are read-only to it */
        static const KNOWNFOLDERID *const ro[] = {&FOLDERID_ProgramFiles, &FOLDERID_ProgramFilesX86, &FOLDERID_Windows, &FOLDERID_ProgramFilesX64};
        for (int i = 0; i < 4; i++) { wchar_t k[PATHN]; if (KnownDir(ro[i], k) && PathUnder(out, k)) {
            _snwprintf(msg, msgN, L2(L"Não instale dentro de \"%s\": o PORTHEON grava os saves, os caches e as configurações na própria pasta, e o Windows não deixa gravar ali. Use uma pasta como %c:\\PORTHEON.", L"Do not install inside \"%s\": PORTHEON writes its saves, caches and settings into its own folder, and Windows does not let it write there. Use a folder such as %c:\\PORTHEON."), k, out[0]);
            msg[msgN - 1] = 0; return 2; } }
    }
    {   wchar_t r[4] = {out[0], L':', L'\\', 0}; UINT dt = GetDriveTypeW(r);
        if (dt == DRIVE_REMOTE || dt == DRIVE_CDROM || dt == DRIVE_NO_ROOT_DIR) { _snwprintf(msg, msgN, L2(L"A unidade %c: não serve (rede, CD/DVD ou não existe): use um disco deste PC.", L"Drive %c: cannot be used (network, CD/DVD or missing): use a disk of this PC."), out[0]); msg[msgN - 1] = 0; return 2; }
        if (dt == DRIVE_REMOVABLE) { _snwprintf(msg, msgN, L"%s ", L2(L"Unidade removível: funciona, mas o PORTHEON só abre com ela conectada.", L"Removable drive: it works, but PORTHEON only opens while it is plugged in.")); lv = 1; }
    }
    /* a folder with other things in it (D:\Jogos) gets a PORTHEON folder of its own: the uninstaller empties its folder */
    if (DirExists(out) && DirHasFiles(out) && !IsPortheonDir(out)) { size_t l = wcslen(out); if (l + 10 < PATHN) wcscat(out, L"\\PORTHEON"); }
    /* Downloads, Documents, C:\Users, the OneDrive root...: even empty, never the PORTHEON folder itself */
    else if (ShellFolderClash(out, NULL)) { size_t l = wcslen(out); if (l + 10 < PATHN) wcscat(out, L"\\PORTHEON"); }
    if (InOneDrive(out)) {
        size_t l = wcslen(msg); _snwprintf(msg + l, msgN - (int)l, L"%s ", L2(L"Esta pasta está no OneDrive: ele sincroniza tudo pela internet e pode tirar do disco arquivos que os jogos usam. Prefira uma pasta fora dele, como D:\\PORTHEON.", L"This folder is in OneDrive: it syncs everything over the Internet and may take files the games use off the disk. Prefer a folder outside it, such as D:\\PORTHEON."));
        lv = 1;
    }
    /* the deepest path while extracting: <out>\.pst\x\ + the zip's longest name (top folder included); it is also
       longer than the final <out>\<name below the top folder> */
    if (g_zip.ok && wcslen(out) + STAGE_EXTRA + 1 + g_zip.maxName >= 259) {
        _snwprintf(msg, msgN, L"%s", L2(L"O caminho da pasta é longo demais para os arquivos do PORTHEON (o Windows limita a 260 letras): escolha uma pasta mais curta, como D:\\PORTHEON.", L"The folder path is too long for PORTHEON's files (Windows limits paths to 260 characters): pick a shorter folder, such as D:\\PORTHEON."));
        return 2;
    }
    msg[msgN - 1] = 0; return lv;
}

/* ---------------------------------------------------------------- processes in the folder (update.c's BusyIn) */
static BOOL BusyIn(const wchar_t *dir, wchar_t *who, int whoN)
{
    BOOL busy = FALSE; size_t n = wcslen(dir); DWORD self = GetCurrentProcessId();
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32W pe = {sizeof(pe)};
    if (sn == INVALID_HANDLE_VALUE) return FALSE;
    if (Process32FirstW(sn, &pe)) do {
        if (pe.th32ProcessID == self) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID); if (!h) continue;
        wchar_t p[PATHN]; DWORD len = PATHN;
        if (QueryFullProcessImageNameW(h, 0, p, &len) && len > n && !_wcsnicmp(p, dir, n) && p[n] == L'\\') { busy = TRUE; if (who) { wcsncpy(who, pe.szExeFile, whoN - 1); who[whoN - 1] = 0; } }
        CloseHandle(h);
    } while (!busy && Process32NextW(sn, &pe));
    CloseHandle(sn); return busy;
}

/* ---------------------------------------------------------------- files */
static BOOL DelTree(const wchar_t *d)   /* never follows a junction or a link: it removes the link itself */
{
    wchar_t q[PATHN]; Join(q, d, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd);
    if (f != INVALID_HANDLE_VALUE) {
        do {
            if (DotName(fd.cFileName)) continue;
            wchar_t p[PATHN]; Join(p, d, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) SetFileAttributesW(p, fd.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) DelTree(p);
            else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveDirectoryW(p);
            else DeleteFileW(p);
        } while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    return RemoveDirectoryW(d) || !DirExists(d);
}
/* the stage and other leftovers through "\\?\": a tree an older or broken run left deeper than 260 letters still goes */
static BOOL DelTreeLong(const wchar_t *d)
{
    wchar_t p[PATHN]; if (!wcsncmp(d, L"\\\\?\\", 4) || wcslen(d) + 5 >= PATHN) return DelTree(d);
    _snwprintf(p, PATHN, L"\\\\?\\%s", d); p[PATHN - 1] = 0; return DelTree(p);
}
static BOOL MakeDirs(const wchar_t *d, DWORD *err)
{
    int r = SHCreateDirectoryExW(NULL, d, NULL);
    if (r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS || r == ERROR_FILE_EXISTS) { if (DirExists(d)) return TRUE; }
    if (err) *err = r ? (DWORD)r : GetLastError(); return FALSE;
}
/* the player's own files: never from a package, and a copy already in the folder stays (update.c's KeepPlayers, plus the
   files package.ps1 leaves out, in case a zip ever carried one) */
static BOOL KeepPlayers(const wchar_t *rel, const wchar_t *target)
{
    if (!ThirdPartyRel(rel))   /* 6.0: Zelda's and God of War III's own "_" folders are program files */
        for (const wchar_t *s = rel, *e; (e = wcschr(s, L'\\')) != NULL; s = e + 1) if (RuntimeDirName(s, (size_t)(e - s))) return TRUE;   /* _SaveData, _PipelineCache, _ShaderCache, _Logs... */
    if (!FileExists(target)) return FALSE;
    static const wchar_t *const tuned[] = {L"games\\UFC5\\bin\\AeonSR.ini", L"games\\UFC5\\bin\\ReShadePreset.ini"};
    for (int i = 0; i < 2; i++) if (!_wcsicmp(rel, tuned[i])) return TRUE;
    size_t n = wcslen(rel); if (n > 14 && !_wcsicmp(rel + n - 14, L"-community.xml")) return TRUE;
    const wchar_t *b = wcsrchr(rel, L'\\'); b = b ? b + 1 : rel;
    static const wchar_t *const own[] = {L"UFC5.ini", L"UFC6.ini", L"portheon.ini", L"pular.txt", L"rtx50-pular.txt", L"rtx50-pular-cs.txt", L"portheon-remote-skip.txt", L"portheon-patch.json", L"UFC 6 Parsec.exe"};
    for (int i = 0; i < 9; i++) if (!_wcsicmp(b, own[i])) return TRUE;
    return FALSE;
}
typedef struct { DWORD done, total, kept; wchar_t *err; int errN; } MoveCtx;
static BOOL MoveTree(const wchar_t *src, const wchar_t *dst, const wchar_t *rel, MoveCtx *c)
{
    wchar_t q[PATHN]; Join(q, src, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd); BOOL ok = TRUE;
    if (f == INVALID_HANDLE_VALUE) {   /* a stage folder it cannot read (a path over 260 letters, access): an incomplete install, never "done" */
        DWORD e = GetLastError(); if (e == ERROR_FILE_NOT_FOUND || e == ERROR_NO_MORE_FILES || (e == ERROR_PATH_NOT_FOUND && wcslen(q) < MAX_PATH)) return TRUE;
        wchar_t w[400]; ErrText(e, w, 400); SnW(c->err, c->errN, L2(L"não consegui ler a pasta temporária %s: %s", L"could not read the temporary folder %s: %s"), rel[0] ? rel : src, w);
        Log(L"stage folder unreadable: %s (error %lu)", src, e); return FALSE;
    }
    do {
        if (DotName(fd.cFileName)) continue;
        if (g_cancel) { ok = FALSE; SnW(c->err, c->errN, L"%s", L2(L"cancelado", L"cancelled")); break; }
        wchar_t s[PATHN], t[PATHN], r[PATHN]; Join(s, src, fd.cFileName); Join(t, dst, fd.cFileName);
        if (rel[0]) Join(r, rel, fd.cFileName); else wcscpy(r, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (RuntimeDirName(fd.cFileName, wcslen(fd.cFileName)) && !ThirdPartyRel(r)) { Log(L"skip folder (player's, never from a package): %s", r); continue; }   /* 6.0: not in Zelda's / God of War III's trees */
            DWORD e = 0;
            if (!DirExists(t) && !MakeDirs(t, &e)) { wchar_t w[400]; ErrText(e, w, 400); SnW(c->err, c->errN, L2(L"não consegui criar a pasta %s: %s", L"could not create the folder %s: %s"), r, w); ok = FALSE; break; }
            if (!MoveTree(s, t, r, c)) { ok = FALSE; break; }
            continue;
        }
        c->done++;
        if (KeepPlayers(r, t)) { c->kept++; Log(L"kept the player's file: %s", r); continue; }
        DWORD a = GetFileAttributesW(t);
        if (a != INVALID_FILE_ATTRIBUTES && (a & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))) SetFileAttributesW(t, FILE_ATTRIBUTE_NORMAL);
        BOOL m = FALSE; DWORD e = 0;
        for (int k = 0; k < 5 && !m; k++) { if (k) Sleep(800); m = MoveFileExW(s, t, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED); if (!m) e = GetLastError(); }
        if (!m) { wchar_t w[400]; ErrText(e, w, 400); SnW(c->err, c->errN, L2(L"não consegui gravar %s: %s", L"could not write %s: %s"), r, w); c->err[c->errN - 1] = 0; Log(L"move failed: %s (error %lu)", r, e); ok = FALSE; break; }
        if (c->total) Prog(850 + (int)(130ull * c->done / c->total));
    } while (FindNextFileW(f, &fd));
    FindClose(f); return ok;
}
/* the overlay into a temp file, hashed while it is copied: nothing is extracted from a package that does not match */
static BOOL CopyPayloadOnce(const Payload *pl, const wchar_t *dst, BOOL allowReuse, BOOL *stale, wchar_t *err, int errN)
{
    BOOL ok = FALSE, reuse = FALSE; BCRYPT_ALG_HANDLE alg = NULL; BCRYPT_HASH_HANDLE h = NULL; BYTE dig[32]; DWORD e = 0;
    HANDLE in = INVALID_HANDLE_VALUE, out = INVALID_HANDLE_VALUE; BYTE *buf = (BYTE *)malloc(1 << 20);
    if (!buf || BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0 || BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0) != 0) {
        SnW(err, errN, L"%s", L2(L"o Windows não conferiu o pacote (BCrypt SHA-256 indisponível)", L"Windows could not check the package (BCrypt SHA-256 unavailable)")); goto done; }
    /* resuming: a temp copy of the right size from the last run is checked instead of copied again; only when there is an
       expected SHA-256 to check it against (never in selftest), and its name carries that SHA (DoInstall) */
    WIN32_FILE_ATTRIBUTE_DATA fa; reuse = allowReuse && pl->haveSha && GetFileAttributesExW(dst, GetFileExInfoStandard, &fa) && (((uint64_t)fa.nFileSizeHigh << 32) | fa.nFileSizeLow) == pl->size;
    in = CreateFileW(reuse ? dst : pl->file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (in == INVALID_HANDLE_VALUE) { e = GetLastError(); wchar_t w[400]; ErrText(e, w, 400); SnW(err, errN, L2(L"não consegui ler o pacote: %s", L"could not read the package: %s"), w); goto done; }
    if (!reuse) {
        LARGE_INTEGER li; li.QuadPart = (LONGLONG)pl->off; SetFilePointerEx(in, li, NULL, FILE_BEGIN);
        out = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (out == INVALID_HANDLE_VALUE) { e = GetLastError(); wchar_t w[400]; ErrText(e, w, 400); SnW(err, errN, L2(L"não consegui criar %s: %s", L"could not create %s: %s"), dst, w); goto done; }
    } else Log(L"resume: %s is already there (%llu bytes), checking it", dst, (unsigned long long)pl->size);
    for (uint64_t left = pl->size; left; ) {
        if (g_cancel) { SnW(err, errN, L"%s", L2(L"cancelado", L"cancelled")); goto done; }
        DWORD want = left > (1u << 20) ? (1u << 20) : (DWORD)left, got = 0, wr = 0;
        if (!ReadFile(in, buf, want, &got, NULL) || got != want) { e = GetLastError(); SnW(err, errN, L2(L"o pacote está incompleto ou corrompido (leitura parou, erro %lu): baixe o instalador de novo", L"the package is incomplete or damaged (reading stopped, error %lu): download the installer again"), e); goto done; }
        BCryptHashData(h, buf, got, 0);
        if (!reuse && (!WriteFile(out, buf, got, &wr, NULL) || wr != got)) { e = GetLastError(); wchar_t w[400]; ErrText(e ? e : ERROR_DISK_FULL, w, 400); SnW(err, errN, L2(L"não consegui copiar o pacote para %s: %s", L"could not copy the package to %s: %s"), dst, w); goto done; }
        left -= got; Prog(30 + (int)(170 * (pl->size - left) / pl->size));
    }
    if (BCryptFinishHash(h, dig, 32, 0) != 0) { SnW(err, errN, L"%s", L2(L"o Windows não conferiu o pacote (SHA-256)", L"Windows could not check the package (SHA-256)")); goto done; }
    {   wchar_t hx[65], wx[65]; HexSha(dig, hx);
        if (pl->haveSha) { HexSha(pl->sha, wx); Log(L"SHA-256 of the package: %s (expected %s)", hx, wx); }
        else Log(L"SHA-256 of the package: %s (no expected value: selftest)", hx);
        if (pl->haveSha && memcmp(dig, pl->sha, 32)) {
            if (reuse) {   /* the old temp copy is stale (another build of the same version): not the package's fault */
                Log(L"resume: %s does not match, copying the package again", dst); *stale = TRUE;
                CloseHandle(in); in = INVALID_HANDLE_VALUE; DeleteFileW(dst); goto done;
            }
            SnW(err, errN, L"%s", L2(L"o pacote dentro do instalador está corrompido (o SHA-256 não confere): baixe o PORTHEON-Setup.exe de novo, da página oficial", L"the package inside the installer is damaged (the SHA-256 does not match): download PORTHEON-Setup.exe again from the official page"));
            if (out != INVALID_HANDLE_VALUE) { CloseHandle(out); out = INVALID_HANDLE_VALUE; }
            CloseHandle(in); in = INVALID_HANDLE_VALUE; DeleteFileW(dst); goto done;
        }
    }
    ok = TRUE;
done:
    if (out != INVALID_HANDLE_VALUE) { CloseHandle(out); if (!ok) DeleteFileW(dst); }
    if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
    if (h) BCryptDestroyHash(h);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    free(buf);
    if (!ok && reuse && !*stale && !g_cancel) { Log(L"resume: %s could not be read back, copying the package again", dst); *stale = TRUE; DeleteFileW(dst); }
    return ok;
}
static BOOL CopyPayload(const Payload *pl, const wchar_t *dst, wchar_t *err, int errN)
{
    BOOL stale = FALSE;
    if (CopyPayloadOnce(pl, dst, TRUE, &stale, err, errN)) return TRUE;
    if (!stale) return FALSE;
    err[0] = 0; return CopyPayloadOnce(pl, dst, FALSE, &stale, err, errN);   /* only the package itself can be "damaged" */
}
/* Windows' tar.exe extracts the zip into stage; -v names each entry ("x <name>"), which drives the progress bar */
static BOOL RunTar(const wchar_t *zip, const wchar_t *stage, wchar_t *err, int errN)
{
    wchar_t tar[PATHN], cmd[3 * PATHN]; GetSystemDirectoryW(tar, PATHN); wcscat(tar, L"\\tar.exe");
    if (!FileExists(tar)) { SnW(err, errN, L"%s", L2(L"falta o tar.exe do Windows (System32\\tar.exe, do Windows 10 versão 1803 em diante): atualize o Windows pelo Windows Update", L"Windows' tar.exe is missing (System32\\tar.exe, Windows 10 version 1803 and newer): update Windows through Windows Update")); return FALSE; }
    _snwprintf(cmd, 3 * PATHN, L"\"%s\" -xvf \"%s\" -C \"%s\"", tar, zip, stage); cmd[3 * PATHN - 1] = 0;
    Log(L"run: %s", cmd);
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE}; HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) { SnW(err, errN, L"%s", L2(L"não consegui abrir o tar.exe", L"could not start tar.exe")); return FALSE; }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    STARTUPINFOW si = {sizeof(si)}; si.dwFlags = STARTF_USESTDHANDLES; si.hStdInput = nul; si.hStdOutput = wr; si.hStdError = wr;
    PROCESS_INFORMATION pi = {0};
    BOOL started = CreateProcessW(tar, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, stage, &si, &pi);
    DWORD se = GetLastError();
    CloseHandle(wr); if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!started) { CloseHandle(rd); wchar_t w[400]; ErrText(se, w, 400); SnW(err, errN, L2(L"não consegui abrir o tar.exe: %s", L"could not start tar.exe: %s"), w); return FALSE; }
    EnterCriticalSection(&g_cs); g_tarProc = pi.hProcess; LeaveCriticalSection(&g_cs);   /* Cancel ends it from the window thread, under the lock */
    char buf[8192], line[2048]; int ll = 0; DWORD got, count = 0; char tailMsg[1024] = "";
    while (ReadFile(rd, buf, sizeof(buf), &got, NULL) && got) {
        for (DWORD i = 0; i < got; i++) {
            char ch = buf[i];
            if (ch == '\n' || ch == '\r') {
                line[ll] = 0;
                if (ll) {
                    if (line[0] == 'x' && line[1] == ' ') { count++; if (g_zip.entries) Prog(200 + (int)(650ull * count / g_zip.entries)); }
                    else { wchar_t w[2048]; MultiByteToWideChar(CP_ACP, 0, line, -1, w, 2048); Log(L"tar: %s", w); strncpy(tailMsg, line, 1023); tailMsg[1023] = 0; }
                }
                ll = 0;
            } else if (ll < (int)sizeof(line) - 1) line[ll++] = ch;
        }
    }
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, INFINITE); DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
    EnterCriticalSection(&g_cs); g_tarProc = NULL; CloseHandle(pi.hProcess); LeaveCriticalSection(&g_cs); CloseHandle(pi.hThread);
    Log(L"tar exit code %lu, %lu entries", code, count);
    if (g_cancel) { SnW(err, errN, L"%s", L2(L"cancelado", L"cancelled")); return FALSE; }
    if (code == 0) return TRUE;
    uint64_t fr = 0; wchar_t tw[1024]; MultiByteToWideChar(CP_ACP, 0, tailMsg, -1, tw, 1024);
    if ((FreeOn(stage, &fr) && fr < (64ull << 20)) || StrStrIA(tailMsg, "space") || StrStrIA(tailMsg, "disk full")) { wchar_t w[400]; ErrText(ERROR_DISK_FULL, w, 400); SnW(err, errN, L"%s", w); }
    else if (StrStrIA(tailMsg, "denied") || StrStrIA(tailMsg, "Permission")) { wchar_t w[400]; ErrText(ERROR_ACCESS_DENIED, w, 400); SnW(err, errN, L"%s (tar: %s)", w, tw); }
    else SnW(err, errN, L2(L"o tar.exe não conseguiu extrair o pacote (código %lu: %s). Veja o registro.", L"tar.exe could not extract the package (code %lu: %s). See the log."), code, tw[0] ? tw : L"?");
    err[errN - 1] = 0; return FALSE;
}

/* ---------------------------------------------------------------- shortcuts, the uninstall entry, the uninstaller */
static BOOL MakeLink(const wchar_t *lnk, const wchar_t *exe, const wchar_t *wd, const wchar_t *desc)
{
    IShellLinkW *sl = NULL; IPersistFile *pf = NULL; IPropertyStore *ps = NULL; BOOL ok = FALSE;
    static const GUID iidPs = {0x886d8eeb, 0x8cf2, 0x4446, {0x8d, 0x02, 0xcd, 0xba, 0x1d, 0xbd, 0xcf, 0x99}};
    static const PROPERTYKEY pkId = {{0x9f4c2855, 0x9f79, 0x4b39, {0xa8, 0xd0, 0xe1, 0xd4, 0x2d, 0xe1, 0xd5, 0xf3}}, 5};   /* PKEY_AppUserModel_ID */
    if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl)) && sl) {
        sl->lpVtbl->SetPath(sl, exe); sl->lpVtbl->SetWorkingDirectory(sl, wd); sl->lpVtbl->SetIconLocation(sl, exe, 0); sl->lpVtbl->SetDescription(sl, desc);
        if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &iidPs, (void **)&ps)) && ps) {   /* the taskbar groups PORTHEON's window with its shortcut */
            PROPVARIANT pv; memset(&pv, 0, sizeof(pv)); pv.vt = VT_LPWSTR; pv.pwszVal = (LPWSTR)PORTHEON_AUMID;
            ps->lpVtbl->SetValue(ps, &pkId, &pv); ps->lpVtbl->Commit(ps); ps->lpVtbl->Release(ps);
        }
        if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf)) && pf) { ok = SUCCEEDED(pf->lpVtbl->Save(pf, lnk, TRUE)); pf->lpVtbl->Release(pf); }
        sl->lpVtbl->Release(sl);
    }
    return ok;
}
static BOOL LinkTarget(const wchar_t *lnk, wchar_t *out)
{
    IShellLinkW *sl = NULL; IPersistFile *pf = NULL; BOOL ok = FALSE; out[0] = 0;
    if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl)) && sl) {
        if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf)) && pf) {
            if (SUCCEEDED(pf->lpVtbl->Load(pf, lnk, STGM_READ)) && SUCCEEDED(sl->lpVtbl->GetPath(sl, out, PATHN, NULL, SLGP_RAWPATH))) ok = out[0] != 0;
            pf->lpVtbl->Release(pf);
        }
        sl->lpVtbl->Release(sl);
    }
    return ok;
}
static void LinkPaths(wchar_t *desk, wchar_t *start)
{
    desk[0] = start[0] = 0;
    if (KnownDir(&FOLDERID_Desktop, desk)) Join(desk, desk, L"PORTHEON.lnk");   /* the name PORTHEON's own PortheonShortcut uses */
    if (KnownDir(&FOLDERID_Programs, start)) Join(start, start, L"PORTHEON.lnk");
}
static const wchar_t *UninstKey(void) { return g_test ? L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PORTHEON-SetupTest" : L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PORTHEON"; }
static BOOL RegInstallDir(wchar_t *out)
{
    DWORD n = PATHN * sizeof(wchar_t), t = 0; out[0] = 0;
    return RegGetValueW(HKEY_CURRENT_USER, UninstKey(), L"InstallLocation", RRF_RT_REG_SZ, &t, out, &n) == ERROR_SUCCESS && out[0];
}
static BOOL RegWrite(const wchar_t *dir, wchar_t *err, int errN)
{
    HKEY k; LONG r = RegCreateKeyExW(HKEY_CURRENT_USER, UninstKey(), 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL);
    if (r != ERROR_SUCCESS) { wchar_t w[400]; ErrText((DWORD)r, w, 400); SnW(err, errN, L2(L"não consegui registrar o desinstalador: %s", L"could not register the uninstaller: %s"), w); return FALSE; }
    wchar_t v[PATHN + 64], exe[PATHN], un[PATHN]; Join(exe, dir, L"PORTHEON.exe"); Join(un, dir, UNINST_EXE);
    #define SZ(name, val) RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)(val), (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)))
    #define DW(name, val) do { DWORD d_ = (val); RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE *)&d_, 4); } while (0)
    SZ(L"DisplayName", L"PORTHEON"); SZ(L"DisplayVersion", g_zip.ver); SZ(L"Publisher", L"PORTHEON");
    _snwprintf(v, PATHN + 64, L"\"%s\",0", exe); v[PATHN + 63] = 0; SZ(L"DisplayIcon", v);
    SZ(L"InstallLocation", dir);
    _snwprintf(v, PATHN + 64, L"\"%s\" --uninstall", un); v[PATHN + 63] = 0; SZ(L"UninstallString", v);
    SYSTEMTIME st; GetLocalTime(&st); wchar_t d[16]; _snwprintf(d, 16, L"%04d%02d%02d", st.wYear, st.wMonth, st.wDay); SZ(L"InstallDate", d);
    DW(L"EstimatedSize", (DWORD)((g_zip.total + 1023) / 1024)); DW(L"NoModify", 1); DW(L"NoRepair", 1);
    #undef SZ
    #undef DW
    RegCloseKey(k); Log(L"uninstall entry: HKCU\\%s", UninstKey()); return TRUE;
}
/* the uninstaller: this program without its package (the first <offset> bytes) */
static BOOL WriteStub(const wchar_t *dir, wchar_t *err, int errN)
{
    wchar_t un[PATHN], tmp[PATHN]; Join(un, dir, UNINST_EXE); _snwprintf(tmp, PATHN, L"%s.new", un);
    HANDLE in = CreateFileW(g_self, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE out = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); BOOL ok = in != INVALID_HANDLE_VALUE && out != INVALID_HANDLE_VALUE;
    DWORD e = GetLastError(); BYTE *buf = (BYTE *)malloc(1 << 16);
    for (uint64_t left = g_stubLen; ok && left; ) {
        DWORD want = left > (1u << 16) ? (1u << 16) : (DWORD)left, got = 0, wr = 0;
        if (!buf || !ReadFile(in, buf, want, &got, NULL) || got != want || !WriteFile(out, buf, got, &wr, NULL) || wr != got) { e = GetLastError(); ok = FALSE; }
        left -= got;
    }
    free(buf); if (in != INVALID_HANDLE_VALUE) CloseHandle(in); if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    if (ok) { SetFileAttributesW(un, FILE_ATTRIBUTE_NORMAL); ok = MoveFileExW(tmp, un, MOVEFILE_REPLACE_EXISTING); if (!ok) e = GetLastError(); }
    if (!ok) { DeleteFileW(tmp); wchar_t w[400]; ErrText(e, w, 400); SnW(err, errN, L2(L"não consegui gravar o desinstalador (%s): %s", L"could not write the uninstaller (%s): %s"), UNINST_EXE, w); }
    else Log(L"uninstaller: %s (%llu bytes)", un, (unsigned long long)g_stubLen);
    return ok;
}

/* ---------------------------------------------------------------- portheon-files.txt: what this installer put in the folder
   The uninstaller deletes by this list (plus the runtime folders and files the ports make beside them), never by "all
   that is in the folder": a folder the player shares with other games, mods or pictures keeps them. UTF-8, one path
   below the folder per line. An update keeps the lines of an older list whose files are still there (an older version's
   files that the new one no longer ships still go at uninstall). */
#define MANIFEST L"portheon-files.txt"
static BOOL SafeRel(const wchar_t *r)   /* a relative path that stays inside the folder */
{
    if (!r[0] || r[0] == L'\\' || wcschr(r, L':')) return FALSE;
    for (const wchar_t *s = r; *s; ) { const wchar_t *e = wcschr(s, L'\\'); size_t n = e ? (size_t)(e - s) : wcslen(s);
        if (n == 0 || (n == 2 && s[0] == L'.' && s[1] == L'.') || (n == 1 && s[0] == L'.')) return FALSE; s += n; if (*s) s++; }
    return TRUE;
}
static wchar_t *ReadManifest(const wchar_t *dir, size_t *outLen)   /* a "a\0b\0\0" list, or NULL when there is none */
{
    wchar_t p[PATHN]; Join(p, dir, MANIFEST); *outLen = 0;
    HANDLE f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); if (f == INVALID_HANDLE_VALUE) return NULL;
    LARGE_INTEGER sz; char *u = NULL; wchar_t *w = NULL; DWORD got = 0;
    if (GetFileSizeEx(f, &sz) && sz.QuadPart < (64 << 20) && (u = (char *)malloc((size_t)sz.QuadPart + 1)) && ReadFile(f, u, (DWORD)sz.QuadPart, &got, NULL) && got == (DWORD)sz.QuadPart) {
        u[got] = 0; int n = MultiByteToWideChar(CP_UTF8, 0, u, (int)got + 1, NULL, 0);
        if (n > 0 && (w = (wchar_t *)malloc(((size_t)n + 2) * sizeof(wchar_t)))) {
            MultiByteToWideChar(CP_UTF8, 0, u, (int)got + 1, w, n);
            size_t o = 0; wchar_t *s = w;
            while (*s) {   /* lines -> a list, in place (the list is never longer than the text) */
                wchar_t *e = s; while (*e && *e != L'\n' && *e != L'\r') e++;
                wchar_t c = *e; *e = 0; size_t l = wcslen(s);
                if (l && SafeRel(s)) { memmove(w + o, s, (l + 1) * sizeof(wchar_t)); o += l + 1; }
                s = c ? e + 1 : e;
            }
            w[o] = 0; *outLen = o;
        }
    }
    free(u); CloseHandle(f); return w;
}
static BOOL WriteManifest(const wchar_t *dir, wchar_t *err, int errN)
{
    size_t oldLen = 0; wchar_t *old = ReadManifest(dir, &oldLen); BOOL ok = FALSE; DWORD e = 0, lines = 0;
    wchar_t p[PATHN], tmp[PATHN]; Join(p, dir, MANIFEST); _snwprintf(tmp, PATHN, L"%s.new", p); tmp[PATHN - 1] = 0;
    HANDLE f = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { e = GetLastError(); goto done; }
    ok = TRUE;
    for (int pass = 0; pass < 2 && ok; pass++) {
        const wchar_t *list = pass ? g_zip.names : old; if (!list) continue;
        for (const wchar_t *s = list; *s && ok; s += wcslen(s) + 1) {
            if (!SafeRel(s)) continue;
            {   BOOL rt = FALSE; for (const wchar_t *a = s, *e; !rt && (e = wcschr(a, L'\\')) != NULL; a = e + 1) rt = RuntimeDirName(a, (size_t)(e - a));
                if (rt && !ThirdPartyRel(s)) continue; }   /* a runtime folder in a package is never installed (MoveTree), so never listed (6.0: Zelda's / God of War III's "_" folders are) */
            if (!pass) {   /* an older line: kept when its file is still there and the new package does not have it anyway */
                wchar_t a[PATHN]; Join(a, dir, s); if (!FileExists(a)) continue;
                BOOL dup = FALSE; for (const wchar_t *n = g_zip.names; n && *n && !dup; n += wcslen(n) + 1) dup = !_wcsicmp(n, s);
                if (dup) continue;
            }
            int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL); char u[3200]; DWORD wr;
            if (n <= 1 || n > 3190) continue;
            WideCharToMultiByte(CP_UTF8, 0, s, -1, u, n, NULL, NULL); u[n - 1] = '\r'; u[n] = '\n';
            if (!WriteFile(f, u, (DWORD)n + 1, &wr, NULL) || wr != (DWORD)n + 1) { e = GetLastError(); ok = FALSE; }
            lines++;
        }
    }
    CloseHandle(f);
    if (ok) { SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL); ok = MoveFileExW(tmp, p, MOVEFILE_REPLACE_EXISTING); if (!ok) e = GetLastError(); }
done:
    free(old);
    if (!ok) { DeleteFileW(tmp); wchar_t w[400]; ErrText(e ? e : ERROR_DISK_FULL, w, 400); SnW(err, errN, L2(L"não consegui gravar a lista de arquivos (%s): %s", L"could not write the file list (%s): %s"), MANIFEST, w); }
    else Log(L"file list: %s (%lu lines)", p, lines);
    return ok;
}
/* ---------------------------------------------------------------- install */
static wchar_t g_target[PATHN]; static BOOL g_wantLinks = TRUE;
static BOOL DoInstall(const wchar_t *dir, BOOL links, wchar_t *err, int errN)
{
    err[0] = 0; g_cancel = 0; DWORD e = 0; wchar_t w[400], who[64] = L"";
    Log(L"install PORTHEON %s into \"%s\" (package %llu bytes, %lu files, %llu bytes extracted)", g_zip.ver, dir, (unsigned long long)g_pl.size, g_zip.files, (unsigned long long)g_zip.total);
    if (g_zip.player) Log(L"WARNING: the package has a player's file (%s): it is skipped", g_zip.playerEx);
    else Log(L"the package holds no player's file (no _ folder, no .ini but the shipped presets): extracting over the folder is safe");
    Prog(0);
    SetStatus(L2(L"Preparando a pasta %s...", L"Preparing the folder %s..."), dir);
    if (!MakeDirs(dir, &e)) { ErrText(e, w, 400); SnW(err, errN, L2(L"não consegui criar a pasta: %s", L"could not create the folder: %s"), w); return FALSE; }
    {   wchar_t probe[PATHN]; Join(probe, dir, L".portheon-write-test");
        HANDLE h = CreateFileW(probe, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
        if (h == INVALID_HANDLE_VALUE) { e = GetLastError(); ErrText(e, w, 400); SnW(err, errN, L2(L"não consigo gravar na pasta: %s", L"cannot write into the folder: %s"), w); return FALSE; }
        CloseHandle(h);
    }
    if (BusyIn(dir, who, 64)) { SnW(err, errN, L2(L"%s está aberto e usa essa pasta: feche o PORTHEON e os jogos e tente de novo", L"%s is open and uses that folder: close PORTHEON and the games and try again"), who); return FALSE; }
    {   uint64_t fr = 0; wchar_t a[32], b[32];
        if (FreeOn(dir, &fr)) { Gb(fr, a); Gb(NeedBytes(), b); Log(L"free on the target: %s, needed about %s", a, b); } }
    if (IsPortheonDir(dir)) Log(L"the folder already has PORTHEON: an update in place; the player's files stay");
    /* the temp copy of the zip: %TEMP% when it has room, else the stage folder on the target drive */
    wchar_t stageRoot[PATHN], stage[PATHN], zip[PATHN], top[PATHN];
    Join(stageRoot, dir, STAGE_DIR); Join(stage, stageRoot, L"x");
    {   wchar_t old[PATHN]; Join(old, dir, L".portheon-setup");   /* the stage name of the first 5.2 builds: deeper paths, sometimes left behind */
        if (DirExists(old)) Log(L"old stage folder %s: %s", old, DelTreeLong(old) ? L"removed" : L"NOT removed"); }
    {   /* the temp zip's name carries the start of the package's SHA-256: a copy left by another build of the same version
           is never taken for this one (it would fail the check and look like a damaged download); older ones go */
        wchar_t tp[PATHN], q[PATHN]; uint64_t fr = 0; GetTempPathW(PATHN, tp); wchar_t nm[128], hx[65];
        if (g_pl.haveSha) { HexSha(g_pl.sha, hx); hx[12] = 0; } else wcscpy(hx, L"selftest");
        _snwprintf(nm, 128, L"portheon-setup-%s-%s.zip", g_zip.ver, hx); nm[127] = 0;
        Join(q, tp, L"portheon-setup-*.zip"); WIN32_FIND_DATAW fd; HANDLE ff = FindFirstFileW(q, &fd);
        if (ff != INVALID_HANDLE_VALUE) { do { if (_wcsicmp(fd.cFileName, nm)) { wchar_t o[PATHN]; Join(o, tp, fd.cFileName); if (DeleteFileW(o)) Log(L"old temp package removed: %s", o); } } while (FindNextFileW(ff, &fd)); FindClose(ff); }
        if (FreeOn(tp, &fr) && fr > g_pl.size + (256ull << 20)) Join(zip, tp, nm);
        else { if (!MakeDirs(stageRoot, &e)) { ErrText(e, w, 400); SnW(err, errN, L"%s", w); return FALSE; } Join(zip, stageRoot, L"package.zip"); }
    }
    if (DirExists(stageRoot)) Log(L"a stage folder from an unfinished install is there: starting the extraction over (it is idempotent)");
    SetStatus(L"%s", L2(L"Conferindo o pacote (SHA-256)...", L"Checking the package (SHA-256)..."));
    if (!CopyPayload(&g_pl, zip, err, errN)) return FALSE;
    if (g_cancel) { SnW(err, errN, L"%s", L2(L"cancelado", L"cancelled")); return FALSE; }
    SetStatus(L2(L"Extraindo %lu arquivos...", L"Extracting %lu files..."), g_zip.files);
    DelTreeLong(stage);
    if (!MakeDirs(stage, &e)) { ErrText(e, w, 400); SnW(err, errN, L2(L"não consegui criar a pasta temporária: %s", L"could not create the temporary folder: %s"), w); return FALSE; }
    SetFileAttributesW(stageRoot, FILE_ATTRIBUTE_HIDDEN);
    if (!RunTar(zip, stage, err, errN)) return FALSE;
    Join(top, stage, g_zip.top);
    if (!DirExists(top)) { SnW(err, errN, L2(L"o pacote não tem a pasta %s: baixe o instalador de novo", L"the package has no %s folder: download the installer again"), g_zip.top); return FALSE; }
    SetStatus(L"%s", L2(L"Colocando os arquivos no lugar (os saves e as configurações do jogador ficam)...", L"Putting the files in place (the player's saves and settings stay)..."));
    /* the list first: a move that stops halfway still leaves the uninstaller knowing every file that may be there */
    if (!WriteManifest(dir, err, errN)) return FALSE;
    MoveCtx mc = {0, g_zip.files, 0, err, errN};
    if (!MoveTree(top, dir, L"", &mc)) return FALSE;
    Log(L"%lu files in place, %lu of the player's kept, %lu in runtime folders skipped", mc.done - mc.kept, mc.kept, g_zip.runtimeFiles);
    if (mc.done + g_zip.runtimeFiles < g_zip.files) {   /* every file of the zip was moved or kept: anything less is not an install */
        SnW(err, errN, L2(L"só %lu dos %lu arquivos do pacote chegaram à pasta (o tar.exe ou o antivírus pulou alguns). Veja o registro", L"only %lu of the package's %lu files reached the folder (tar.exe or the antivirus skipped some). See the log"), mc.done + g_zip.runtimeFiles, g_zip.files);
        return FALSE;
    }
    SetStatus(L"%s", L2(L"Criando o desinstalador e os atalhos...", L"Making the uninstaller and the shortcuts..."));
    if (!WriteStub(dir, err, errN)) return FALSE;
    if (!RegWrite(dir, err, errN)) return FALSE;
    if (links && !g_test) {
        wchar_t desk[PATHN], start[PATHN], exe[PATHN]; LinkPaths(desk, start); Join(exe, dir, L"PORTHEON.exe");
        const wchar_t *desc = L2(L"PORTHEON: os ports do UFC para PC", L"PORTHEON: the UFC PC ports");
        if (desk[0]) Log(L"desktop shortcut %s: %s", desk, MakeLink(desk, exe, dir, desc) ? L"ok" : L"FAILED");
        if (start[0]) Log(L"Start menu shortcut %s: %s", start, MakeLink(start, exe, dir, desc) ? L"ok" : L"FAILED");
    } else Log(L"shortcuts: skipped (%s)", g_test ? L"PORTHEON_SETUP_TEST=1" : L"not asked");
    {   /* 6.0 final: an install over an older PORTHEON keeps the exe's path, and Windows its old icon for it */
        wchar_t exe[PATHN]; Join(exe, dir, L"PORTHEON.exe"); IconRefreshAll(exe);
        Log(L"icons: Windows told that %s's icon changed", exe);
    }
    Prog(990);
    DelTreeLong(stageRoot); DeleteFileW(zip);
    Prog(1000);
    Log(L"install done");
    return TRUE;
}
static DWORD WINAPI InstallThread(LPVOID unused)
{
    (void)unused; CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    static wchar_t err[1200]; BOOL ok = DoInstall(g_target, g_wantLinks, err, 1200);
    if (ok) SetStatus(L2(L"Pronto! O Portheon %s está instalado em %s.%s\r\nPara desinstalar: Configurações > Aplicativos, ou \"%s\" na pasta.", L"Done! Portheon %s is installed in %s.%s\r\nTo uninstall: Settings > Apps, or \"%s\" in the folder."),
                      g_zip.ver, g_target, g_wantLinks && !g_test ? L2(L" Os atalhos estão na área de trabalho e no menu Iniciar.", L" The shortcuts are on the desktop and in the Start menu.") : L"", UNINST_EXE);
    else SetStatus(L2(L"Não deu para instalar: %s.\r\nNada do jogador foi apagado. Corrija e clique em \"Tentar de novo\": a instalação continua de onde parou.", L"Could not install: %s.\r\nNothing of the player's was deleted. Fix it and click \"Try again\": the install picks up where it stopped."), err);
    CoUninitialize();
    if (g_wnd) PostMessageW(g_wnd, WM_APP_DONE, ok, 0);
    return ok ? 0 : 1;
}

/* ---------------------------------------------------------------- uninstall */
/* a game the player keeps inside the folder (a PS4/PS5 dump, an Xbox 360 or PS3 game, a .pkg/.iso): the uninstaller
   never deletes hundreds of GB of games. _SaveData folders hold sce_sys too, so only eboot.bin/default.xex/PS3_GAME count. */
static BOOL IsDump(const wchar_t *d)
{
    wchar_t p[PATHN];
    Join(p, d, L"eboot.bin"); if (FileExists(p)) return TRUE;
    Join(p, d, L"default.xex"); if (FileExists(p)) return TRUE;
    Join(p, d, L"PS3_GAME"); if (DirExists(p)) return TRUE;
    Join(p, d, L"exefs"); if (DirExists(p)) { Join(p, d, L"romfs"); if (DirExists(p)) return TRUE; }   /* 6.0: an extracted Switch game (Zelda) */
    return FALSE;
}
typedef struct { DWORD deleted, kept, failed; wchar_t firstKept[PATHN], firstFail[PATHN]; } DelCtx;
static BOOL DelTreeKeep(const wchar_t *d, DelCtx *c)   /* TRUE: d still holds something */
{
    wchar_t q[PATHN]; Join(q, d, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd); BOOL left = FALSE;
    if (f == INVALID_HANDLE_VALUE) {   /* a folder it cannot read (access, a path over 260 letters) is a failure, never "nothing left" */
        DWORD e = GetLastError(); if (e == ERROR_FILE_NOT_FOUND || e == ERROR_NO_MORE_FILES || (e == ERROR_PATH_NOT_FOUND && wcslen(q) < MAX_PATH)) return FALSE;   /* already gone */
        c->failed++; if (!c->firstFail[0]) wcsncpy(c->firstFail, d, PATHN - 1); Log(L"could not read the folder %s (error %lu)", d, e); return TRUE;
    }
    do {
        if (DotName(fd.cFileName)) continue;
        wchar_t p[PATHN]; Join(p, d, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {   /* a junction or link: only the link goes, never what it points at */
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveDirectoryW(p); else DeleteFileW(p);
            continue;
        }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (IsDump(p)) { c->kept++; if (!c->firstKept[0]) wcscpy(c->firstKept, p); Log(L"kept (a game): %s", p); left = TRUE; continue; }
            if (DelTreeKeep(p, c)) left = TRUE;
            else if (!RemoveDirectoryW(p)) { left = TRUE; Log(L"could not remove the folder %s (error %lu)", p, GetLastError()); }
            continue;
        }
        const wchar_t *x = PathFindExtensionW(fd.cFileName); uint64_t sz = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        static const wchar_t *const gameExt[] = {L".pkg", L".iso", L".nsp", L".xci", L".nup", L".nsz", L".xcz"};   /* 6.0: Zelda's Switch files too */
        BOOL gameFile = FALSE; for (int i = 0; i < 7 && !gameFile; i++) gameFile = !_wcsicmp(x, gameExt[i]);
        if (gameFile && sz > (64ull << 20)) { c->kept++; if (!c->firstKept[0]) wcscpy(c->firstKept, p); Log(L"kept (a game file): %s", p); left = TRUE; continue; }
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL);
        BOOL ok = FALSE; for (int k = 0; k < 4 && !ok; k++) { if (k) Sleep(500); ok = DeleteFileW(p) || !FileExists(p); }
        if (ok) c->deleted++; else { c->failed++; left = TRUE; if (!c->firstFail[0]) wcscpy(c->firstFail, p); Log(L"could not delete %s (error %lu)", p, GetLastError()); }
    } while (FindNextFileW(f, &fd));
    FindClose(f); return left;
}
static int CopyTree(const wchar_t *s, const wchar_t *d, DWORD *n)   /* failures */
{
    int fails = 0; DWORD e; if (!MakeDirs(d, &e)) return 1;
    wchar_t q[PATHN]; Join(q, s, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd);
    if (f == INVALID_HANDLE_VALUE) return 0;
    do {
        if (DotName(fd.cFileName) || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        wchar_t a[PATHN], b[PATHN]; Join(a, s, fd.cFileName); Join(b, d, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) fails += CopyTree(a, b, n);
        else if (CopyFileW(a, b, FALSE)) (*n)++; else { fails++; Log(L"backup: could not copy %s (error %lu)", a, GetLastError()); }
    } while (FindNextFileW(f, &fd));
    FindClose(f); return fails;
}
/* the saves (every _SaveData folder under games\, UFC 5's and UFC 6's) and the ini files the player tuned */
static int BackupWalk(const wchar_t *d, const wchar_t *rel, const wchar_t *out, int depth, DWORD *n)
{
    int fails = 0; wchar_t q[PATHN]; Join(q, d, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd);
    if (f == INVALID_HANDLE_VALUE) return 0;
    do {
        if (DotName(fd.cFileName) || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        wchar_t p[PATHN], r[PATHN], o[PATHN]; Join(p, d, fd.cFileName); Join(r, rel, fd.cFileName); Join(o, out, r);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (RuntimeDirName(fd.cFileName, wcslen(fd.cFileName)) && !ThirdPartyRel(r)) {   /* 6.0: God of War III's "_internal" is its launcher, not a save */
                /* what the engines keep for the player (_SaveData, UFC 6's _Trophies...) goes along; the caches they rebuild do not */
                static const wchar_t *const caches[] = {L"_PipelineCache", L"_ShaderCache", L"_TempData", L"_Logs", L"_Textures", L"_Cache", L"_Temp"};
                BOOL cache = FALSE; for (int i = 0; i < 7 && !cache; i++) cache = !_wcsicmp(fd.cFileName, caches[i]);
                if (!cache) { Log(L"backup: %s", r); fails += CopyTree(p, o, n); }
            }
            else if (!_wcsicmp(r, L"games\\GOW3\\user") || !_wcsicmp(r, L"games\\BLOODBORNE\\user") || !_wcsicmp(r, L"games\\INFAMOUS\\user")) { Log(L"backup: %s", r); fails += CopyTree(p, o, n); }   /* 6.0: God of War III's saves (its port keeps them in user\); 6.1: Bloodborne's, inFAMOUS's */
            else if (ThirdPartyRel(r) || !_wcsicmp(r, L"games\\ZELDA") || !_wcsicmp(r, L"games\\GOW3") || !_wcsicmp(r, L"games\\BLOODBORNE") || !_wcsicmp(r, L"games\\INFAMOUS")) { if (!_wcsicmp(r, L"games\\GOW3") || !_wcsicmp(r, L"games\\BLOODBORNE") || !_wcsicmp(r, L"games\\INFAMOUS")) fails += BackupWalk(p, r, out, depth + 1, n); }   /* 6.0: their program trees hold no other save */
            else if (depth < 4 && !IsDump(p) && _wcsicmp(fd.cFileName, L"engines") && _wcsicmp(fd.cFileName, L"pylibs")) fails += BackupWalk(p, r, out, depth + 1, n);
        } else if (!_wcsicmp(PathFindExtensionW(fd.cFileName), L".ini")) {
            DWORD e; wchar_t od[PATHN]; wcscpy(od, o); wchar_t *s = wcsrchr(od, L'\\'); if (s) *s = 0;
            if (MakeDirs(od, &e) && CopyFileW(p, o, FALSE)) (*n)++; else fails++;
        }
    } while (FindNextFileW(f, &fd));
    FindClose(f); return fails;
}
static wchar_t g_backupDir[PATHN];
static BOOL BackupSaves(const wchar_t *dir, wchar_t *err, int errN)
{
    wchar_t docs[PATHN], name[64], games[PATHN]; SYSTEMTIME st; GetLocalTime(&st);
    if (g_test) { GetTempPathW(PATHN, docs); Join(docs, docs, L"portheon-setup-test-docs"); }
    else if (!KnownDir(&FOLDERID_Documents, docs)) { SnW(err, errN, L"%s", L2(L"não achei a pasta Documentos para guardar os saves", L"could not find the Documents folder to keep the saves in")); return FALSE; }
    _snwprintf(name, 64, L"PORTHEON-saves-%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay); Join(g_backupDir, docs, name);
    if (DirExists(g_backupDir)) { _snwprintf(name, 64, L"PORTHEON-saves-%04d-%02d-%02d-%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond); Join(g_backupDir, docs, name); }
    Join(games, dir, L"games"); DWORD n = 0;
    int fails = BackupWalk(games, L"games", g_backupDir, 0, &n);
    {   /* PORTHEON's own settings and profiles live in %LOCALAPPDATA%\PORTHEON\portheon.ini: copied (it stays there too) */
        wchar_t la[PATHN], ini[PATHN], o[PATHN]; if (KnownDir(&FOLDERID_LocalAppData, la)) { Join(ini, la, L"PORTHEON\\portheon.ini"); Join(o, g_backupDir, L"portheon.ini");
            if (FileExists(ini)) { DWORD e; if (MakeDirs(g_backupDir, &e) && CopyFileW(ini, o, FALSE)) n++; else fails++; } }
    }
    Log(L"backup: %lu files into %s, %d failures", n, g_backupDir, fails);
    if (fails) { SnW(err, errN, L2(L"não consegui copiar %d arquivo(s) dos saves para %s: nada foi apagado", L"could not copy %d save file(s) to %s: nothing was deleted"), fails, g_backupDir); return FALSE; }
    if (!n) { g_backupDir[0] = 0; Log(L"backup: no saves or ini files to keep"); }
    return TRUE;
}
static wchar_t g_udir[PATHN]; static BOOL g_keepSaves = TRUE; static DWORD g_waitPid; static DelCtx g_del;
/* what the folder's root holds of PORTHEON's beyond the package's own files (the list) */
static const wchar_t *const ROOT_OWN[] = {L"PORTHEON.exe.old", UNINST_EXE, L"Desinstalar PORTHEON.exe.new", MANIFEST, L"portheon-files.txt.new", L".portheon-write-test", STAGE_DIR, L".portheon-setup"};
/* a folder without the list (extracted by hand, or an install older than the list): only when its top holds nothing
   but what a PORTHEON zip has (5.x) - a PORTHEON.exe dropped into D:\Games never empties D:\Games */
static const wchar_t *const ROOT_ZIP[] = {L"PORTHEON.exe", L"games", L"licenses", L"src", L"LICENSE", L"README.md", L"links.txt", L"gamecontrollerdb.txt", L"portheon.sh"};
static BOOL RootKnown(const wchar_t *n)
{
    for (size_t i = 0; i < sizeof(ROOT_OWN) / sizeof(ROOT_OWN[0]); i++) if (!_wcsicmp(n, ROOT_OWN[i])) return TRUE;
    for (size_t i = 0; i < sizeof(ROOT_ZIP) / sizeof(ROOT_ZIP[0]); i++) if (!_wcsicmp(n, ROOT_ZIP[i])) return TRUE;
    return !_wcsicmp(PathFindExtensionW(n), L".log");   /* logs a port or a crash left */
}
static void DelOne(const wchar_t *p, DelCtx *c)
{
    DWORD a = GetFileAttributesW(p); if (a == INVALID_FILE_ATTRIBUTES) return;
    if (a & FILE_ATTRIBUTE_DIRECTORY) { if (a & FILE_ATTRIBUTE_REPARSE_POINT) RemoveDirectoryW(p); else if (!DelTreeKeep(p, c)) RemoveDirectoryW(p); return; }
    if (a & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL);
    BOOL ok = FALSE; for (int k = 0; k < 4 && !ok; k++) { if (k) Sleep(500); ok = DeleteFileW(p) || !FileExists(p); }
    if (ok) c->deleted++; else { c->failed++; if (!c->firstFail[0]) wcscpy(c->firstFail, p); Log(L"could not delete %s (error %lu)", p, GetLastError()); }
}
/* by the list: its files; the folders under games\ it has files in, whole (the ports' runtime files, caches and saves
   live there; a game dump the player put there stays, DelTreeKeep); PORTHEON's own root files; then the list's empty
   folders, deepest first. Anything else the folder holds stays, and with it the folder. */
static void UninstallByList(const wchar_t *dir, const wchar_t *list, DelCtx *c)
{
    static wchar_t done[64][MAX_PATH]; int nd = 0;
    for (const wchar_t *s = list; *s; s += wcslen(s) + 1) {
        wchar_t p[PATHN]; Join(p, dir, s);
        DWORD a = GetFileAttributesW(p); if (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) DelOne(p, c);
    }
    for (const wchar_t *s = list; *s; s += wcslen(s) + 1) {
        if (_wcsnicmp(s, L"games\\", 6)) continue;
        const wchar_t *e = wcschr(s + 6, L'\\'); if (!e || e == s + 6 || e - s >= MAX_PATH) continue;   /* games\<name>\... only */
        wchar_t g[MAX_PATH]; wcsncpy(g, s, (size_t)(e - s)); g[e - s] = 0;
        BOOL seen = FALSE; for (int i = 0; i < nd && !seen; i++) seen = !_wcsicmp(done[i], g);
        if (seen || nd >= 64) continue;
        wcscpy(done[nd++], g); wchar_t p[PATHN]; Join(p, dir, g);
        if (DirExists(p)) { Log(L"PORTHEON's folder %s: deleted whole (games kept)", g); DelOne(p, c); }
    }
    for (size_t i = 0; i < sizeof(ROOT_OWN) / sizeof(ROOT_OWN[0]); i++) { wchar_t p[PATHN]; Join(p, dir, ROOT_OWN[i]); if (DirExists(p)) DelTreeLong(p); else DelOne(p, c); }
    for (const wchar_t *s = list; *s; s += wcslen(s) + 1) {
        wchar_t r[PATHN]; wcsncpy(r, s, PATHN - 1); r[PATHN - 1] = 0;
        for (wchar_t *b; (b = wcsrchr(r, L'\\')) != NULL; ) { *b = 0; wchar_t p[PATHN]; Join(p, dir, r); RemoveDirectoryW(p); }   /* fails while not empty */
    }
}
static BOOL DoUninstall(const wchar_t *dir0, BOOL keep, wchar_t *err, int errN)
{
    err[0] = 0; wchar_t who[64] = L"", dir[PATHN], clash[PATHN] = L""; memset(&g_del, 0, sizeof(g_del)); g_backupDir[0] = 0;
    LongForm(dir0, dir);
    Log(L"uninstall \"%s\" (keep saves: %d)", dir, keep);
    if (g_waitPid) { HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, g_waitPid); if (p) { WaitForSingleObject(p, 20000); CloseHandle(p); } }
    wchar_t reg0[PATHN], regL[PATHN]; BOOL registered = RegInstallDir(reg0);   /* a retry after a half-finished uninstall */
    if (registered) { LongForm(reg0, regL); registered = !_wcsicmp(regL, dir); }
    size_t listLen = 0; wchar_t *list = ReadManifest(dir, &listLen);
    if (wcslen(dir) < 4 || (!IsPortheonDir(dir) && !registered && !list)) { free(list); SnW(err, errN, L2(L"%s não é uma pasta do PORTHEON (não tem o PORTHEON.exe): nada foi apagado", L"%s is not a PORTHEON folder (it has no PORTHEON.exe): nothing was deleted"), dir); return FALSE; }
    if (ShellFolderClash(dir, clash)) { free(list); SnW(err, errN, L2(L"%s é (ou contém) a pasta %s do Windows e não pode ser esvaziada: nada foi apagado", L"%s is (or holds) Windows' folder %s and cannot be emptied: nothing was deleted"), dir, clash); return FALSE; }
    if (!list) {   /* no list: the whole folder goes only if its top is a PORTHEON zip's and nothing else */
        wchar_t q[PATHN]; Join(q, dir, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd); wchar_t odd[MAX_PATH] = L"";
        if (f != INVALID_HANDLE_VALUE) {
            do { if (!DotName(fd.cFileName) && !RootKnown(fd.cFileName)) { wcsncpy(odd, fd.cFileName, MAX_PATH - 1); odd[MAX_PATH - 1] = 0; } } while (!odd[0] && FindNextFileW(f, &fd));
            FindClose(f);
        }
        if (odd[0]) {
            SnW(err, errN, L2(L"a pasta %s tem coisas que não são do PORTHEON (por exemplo \"%s\") e não tem a lista %s de uma instalação: nada foi apagado. Tire de lá o que é seu, ou apague o PORTHEON à mão",
                              L"the folder %s holds things that are not PORTHEON's (for example \"%s\") and has no %s from an install: nothing was deleted. Move out what is yours, or delete PORTHEON by hand"), dir, odd, MANIFEST);
            return FALSE;
        }
        Log(L"no %s: the folder's top holds only a PORTHEON zip's files, the whole folder goes (games kept)", MANIFEST);
    } else Log(L"%s found: only its files and PORTHEON's own folders go", MANIFEST);
    if (BusyIn(dir, who, 64)) { free(list); SnW(err, errN, L2(L"%s está aberto: feche o PORTHEON e os jogos e tente de novo", L"%s is open: close PORTHEON and the games and try again"), who); return FALSE; }
    Prog(50);
    if (keep) { SetStatus(L"%s", L2(L"Guardando os saves...", L"Keeping the saves...")); if (!BackupSaves(dir, err, errN)) { free(list); return FALSE; } }
    Prog(300);
    if (!g_test) {   /* the shortcuts that open this folder's PORTHEON.exe, and only those */
        wchar_t desk[PATHN], start[PATHN], t[PATHN]; LinkPaths(desk, start);
        const wchar_t *l[2] = {desk, start};
        for (int i = 0; i < 2; i++) if (l[i][0] && FileExists(l[i]) && LinkTarget(l[i], t) && PathUnder(t, dir)) Log(L"shortcut %s: %s", l[i], DeleteFileW(l[i]) ? L"deleted" : L"NOT deleted");
    } else Log(L"shortcuts: left alone (PORTHEON_SETUP_TEST=1)");
    SetStatus(L"%s", L2(L"Apagando os arquivos do PORTHEON...", L"Deleting PORTHEON's files..."));
    if (list) UninstallByList(dir, list, &g_del); else DelTreeKeep(dir, &g_del);
    free(list);
    RemoveDirectoryW(dir);   /* only when nothing is left in it */
    Prog(900);
    if (DirExists(dir) && !g_del.kept) {   /* what stayed, for the log and the message */
        wchar_t q[PATHN]; Join(q, dir, L"*"); WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd);
        if (f != INVALID_HANDLE_VALUE) { do { if (!DotName(fd.cFileName)) { Join(g_del.firstKept, dir, fd.cFileName); break; } } while (FindNextFileW(f, &fd)); FindClose(f); }
        if (g_del.firstKept[0]) { g_del.kept++; Log(L"kept (not PORTHEON's): %s and maybe more", g_del.firstKept); }
    }
    /* the Settings > Apps entry goes only once the files did: after a failure it stays, to uninstall again from there */
    wchar_t exe[PATHN]; Join(exe, dir, L"PORTHEON.exe");
    if (g_del.failed || FileExists(exe)) Log(L"uninstall entry HKCU\\%s: left (the uninstall did not finish)", UninstKey());
    else {
        wchar_t reg[PATHN], regl[PATHN]; BOOL has = RegInstallDir(reg); if (has) LongForm(reg, regl);
        if (!has || !_wcsicmp(regl, dir)) { LONG r = RegDeleteTreeW(HKEY_CURRENT_USER, UninstKey()); Log(L"uninstall entry HKCU\\%s: %s", UninstKey(), r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND ? L"removed" : L"NOT removed"); }
        else Log(L"uninstall entry points at another folder (%s): left", reg);
    }
    Prog(1000);
    Log(L"uninstall: %lu files deleted, %lu kept, %lu failures", g_del.deleted, g_del.kept, g_del.failed);
    if (g_del.failed) { SnW(err, errN, L2(L"%lu arquivo(s) não foram apagados (em uso ou protegidos), por exemplo %s: feche o que estiver usando e desinstale de novo", L"%lu file(s) were not deleted (in use or protected), for example %s: close what uses them and uninstall again"), g_del.failed, g_del.firstFail); return FALSE; }
    if (FileExists(exe)) { SnW(err, errN, L2(L"o PORTHEON.exe continua em %s: veja o registro", L"PORTHEON.exe is still in %s: see the log"), dir); return FALSE; }
    return TRUE;
}
static DWORD WINAPI UninstallThread(LPVOID unused)
{
    (void)unused; CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    static wchar_t err[1200], extra[PATHN + 300]; BOOL ok = DoUninstall(g_udir, g_keepSaves, err, 1200);
    extra[0] = 0;
    if (g_del.kept) SnW(extra, PATHN + 300, L2(L"\r\n%lu item(ns) que não são do PORTHEON (jogos, arquivos seus) ficaram na pasta, por exemplo %s.", L"\r\n%lu item(s) that are not PORTHEON's (games, your own files) stayed in the folder, for example %s."), g_del.kept, g_del.firstKept);
    if (ok) SetStatus(L2(L"O PORTHEON foi desinstalado.%s%s%s", L"PORTHEON was uninstalled.%s%s%s"), g_backupDir[0] ? L2(L"\r\nSeus saves estão em ", L"\r\nYour saves are in ") : L"", g_backupDir, extra);
    else SetStatus(L2(L"A desinstalação não terminou: %s.%s", L"The uninstall did not finish: %s.%s"), err, extra);
    CoUninitialize();
    if (g_wnd) PostMessageW(g_wnd, WM_APP_DONE, ok, 0);
    return ok ? 0 : 1;
}
/* the uninstaller lives inside the folder it deletes: it runs from a copy in %TEMP%, which deletes itself at the end
   (cmd waits two seconds for it to exit, then del: MoveFileEx's delay-until-reboot needs administrator rights) */
static void SelfDeleteLater(void)
{
    wchar_t cmd[PATHN * 2], sys[PATHN]; GetSystemDirectoryW(sys, PATHN);
    _snwprintf(cmd, PATHN * 2, L"\"%s\\cmd.exe\" /d /c ping -n 3 127.0.0.1 >nul & del /f /q \"%s\"", sys, g_self); cmd[PATHN * 2 - 1] = 0;
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi; wchar_t tp[PATHN]; GetTempPathW(PATHN, tp);
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, tp, &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); Log(L"self-delete scheduled: %s", g_self); }
}
static void DropOldUninstallers(void)   /* the temp copies a past uninstall could not delete */
{
    wchar_t tp[PATHN], q[PATHN]; GetTempPathW(PATHN, tp); Join(q, tp, L"portheon-uninstall-*.exe");
    WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(q, &fd); if (f == INVALID_HANDLE_VALUE) return;
    do { wchar_t p[PATHN]; Join(p, tp, fd.cFileName); if (_wcsicmp(p, g_self)) DeleteFileW(p); } while (FindNextFileW(f, &fd));
    FindClose(f);
}
/* runs the uninstaller again from %TEMP%; returns the child's process handle (or NULL) */
static HANDLE RelaunchFromTemp(const wchar_t *dir, const wchar_t *extraArgs)
{
    wchar_t tp[PATHN], copy[PATHN], nm[64], cmd[PATHN * 3]; GetTempPathW(PATHN, tp);
    _snwprintf(nm, 64, L"portheon-uninstall-%lu.exe", GetTickCount()); Join(copy, tp, nm);
    if (!CopyFileW(g_self, copy, FALSE)) { Log(L"could not copy the uninstaller to %s (error %lu)", copy, GetLastError()); return NULL; }
    _snwprintf(cmd, PATHN * 3, L"\"%s\" --uninstall-from \"%s\" %lu%s", copy, dir, GetCurrentProcessId(), extraArgs); cmd[PATHN * 3 - 1] = 0;
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi;
    if (!CreateProcessW(copy, cmd, NULL, NULL, FALSE, 0, NULL, tp, &si, &pi)) { Log(L"could not start %s (error %lu)", copy, GetLastError()); DeleteFileW(copy); return NULL; }
    Log(L"uninstaller relaunched from %s", copy); CloseHandle(pi.hThread); return pi.hProcess;
}

/* ---------------------------------------------------------------- the window */
enum { IDC_TITLE = 100, IDC_SUB, IDC_INFO, IDC_LIST, IDC_DETAIL, IDC_FLBL, IDC_FOLDER, IDC_PICK, IDC_DISK, IDC_PROG, IDC_STATUS, IDC_KEEP, IDC_LOG, IDC_GO, IDC_CANCEL };
enum { PG_CHECKS, PG_WORK, PG_DONE, PG_FAIL, PG_UNINST, PG_UWORK, PG_UDONE, PG_UFAIL };
static int g_page; static BOOL g_uninstMode, g_fromTemp, g_working, g_closeAfter; static UINT g_dpi = 96;
static HFONT g_font, g_fontTitle; static HICON g_bigIcon; static int g_diskLevel; static wchar_t g_snap[PATHN];
/* GetDpiForWindow is Windows 10 1607+: looked up by name, so an older Windows still gets the "too old" message */
static UINT DpiOf(HWND h)
{
    typedef UINT (WINAPI *F)(HWND); F f = (F)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (f && h) return f(h);
    HDC dc = GetDC(NULL); int d = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(NULL, dc); return d > 0 ? (UINT)d : 96;
}
static int S(int v) { return MulDiv(v, (int)g_dpi, 96); }
static HWND C(int id) { return GetDlgItem(g_wnd, id); }
#define CW 760
#define CH 668
static void Layout(void)
{
    int w = S(CW), m = S(20), iw = w - 2 * m;
    MoveWindow(C(IDC_TITLE), S(84), S(16), w - S(104), S(30), FALSE);
    MoveWindow(C(IDC_SUB), S(84), S(48), w - S(104), S(20), FALSE);
    if (g_uninstMode) {
        MoveWindow(C(IDC_INFO), m, S(90), iw, S(150), FALSE);
        MoveWindow(C(IDC_KEEP), m, S(250), iw, S(44), FALSE);
    } else {
        MoveWindow(C(IDC_INFO), m, S(80), iw, S(72), FALSE);
        MoveWindow(C(IDC_LIST), m, S(156), iw, S(182), FALSE);
        MoveWindow(C(IDC_DETAIL), m, S(342), iw, S(52), FALSE);
        MoveWindow(C(IDC_FLBL), m, S(400), iw, S(18), FALSE);
        MoveWindow(C(IDC_FOLDER), m, S(420), iw - S(130), S(26), FALSE);
        MoveWindow(C(IDC_PICK), w - m - S(120), S(419), S(120), S(28), FALSE);
        MoveWindow(C(IDC_DISK), m, S(452), iw, S(36), FALSE);
        HWND lv = C(IDC_LIST);
        ListView_SetColumnWidth(lv, 0, S(150)); ListView_SetColumnWidth(lv, 1, S(140)); ListView_SetColumnWidth(lv, 2, iw - S(150) - S(140) - S(24));
    }
    MoveWindow(C(IDC_PROG), m, S(496), iw, S(18), FALSE);
    MoveWindow(C(IDC_STATUS), m, S(520), iw, S(84), FALSE);
    MoveWindow(C(IDC_LOG), m, S(620), S(130), S(30), FALSE);
    MoveWindow(C(IDC_GO), w - m - S(120) - S(10) - S(170), S(620), S(170), S(30), FALSE);
    MoveWindow(C(IDC_CANCEL), w - m - S(120), S(620), S(120), S(30), FALSE);
    InvalidateRect(g_wnd, NULL, TRUE);
}
static void MakeFonts(void)
{
    if (g_font) DeleteObject(g_font); if (g_fontTitle) DeleteObject(g_fontTitle); if (g_bigIcon) DestroyIcon(g_bigIcon);
    g_font = CreateFontW(-MulDiv(9, (int)g_dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontTitle = CreateFontW(-MulDiv(15, (int)g_dpi, 72), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_bigIcon = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(1), IMAGE_ICON, S(56), S(56), 0);
    for (int id = IDC_TITLE; id <= IDC_CANCEL; id++) if (C(id)) SendMessageW(C(id), WM_SETFONT, (WPARAM)(id == IDC_TITLE ? g_fontTitle : g_font), TRUE);
}
static void Show(int id, BOOL on) { ShowWindow(C(id), on ? SW_SHOW : SW_HIDE); }
static void ShowPage(int pg)
{
    g_page = pg;
    BOOL inst = !g_uninstMode;
    Show(IDC_LIST, inst); Show(IDC_DETAIL, inst); Show(IDC_FLBL, inst); Show(IDC_FOLDER, inst); Show(IDC_PICK, inst); Show(IDC_DISK, inst);
    Show(IDC_KEEP, !inst);
    Show(IDC_PROG, pg != PG_CHECKS && pg != PG_UNINST); Show(IDC_STATUS, TRUE);
    EnableWindow(C(IDC_FOLDER), pg == PG_CHECKS || pg == PG_FAIL); EnableWindow(C(IDC_PICK), pg == PG_CHECKS || pg == PG_FAIL);
    EnableWindow(C(IDC_KEEP), pg == PG_UNINST);
    const wchar_t *go = L"", *cancel = L2(L"Cancelar", L"Cancel"); BOOL goOn = TRUE, goShow = TRUE;
    switch (pg) {
    case PG_CHECKS: go = L2(L"Instalar", L"Install"); goOn = g_checksDone != 0; break;
    case PG_WORK: go = L2(L"Instalar", L"Install"); goOn = FALSE; break;
    case PG_DONE: go = L2(L"Abrir o PORTHEON", L"Open PORTHEON"); goOn = !g_test; cancel = L2(L"Fechar", L"Close"); break;
    case PG_FAIL: go = L2(L"Tentar de novo", L"Try again"); cancel = L2(L"Fechar", L"Close"); break;
    case PG_UNINST: go = L2(L"Desinstalar", L"Uninstall"); break;
    case PG_UWORK: go = L2(L"Desinstalar", L"Uninstall"); goOn = FALSE; break;
    case PG_UDONE: go = L2(L"Abrir os saves", L"Open the saves"); goShow = g_backupDir[0] != 0; cancel = L2(L"Fechar", L"Close"); break;
    case PG_UFAIL: go = L2(L"Tentar de novo", L"Try again"); cancel = L2(L"Fechar", L"Close"); break;
    }
    SetWindowTextW(C(IDC_GO), go); EnableWindow(C(IDC_GO), goOn); Show(IDC_GO, goShow);
    SetWindowTextW(C(IDC_CANCEL), cancel); EnableWindow(C(IDC_CANCEL), TRUE);
}
static void UpdateFolderInfo(void)
{
    wchar_t in[PATHN], out[PATHN], msg[1200], t[1600], a[32], b[32]; GetWindowTextW(C(IDC_FOLDER), in, PATHN);
    int lv = ResolveTarget(in, out, msg, 1200); uint64_t fr = 0;
    if (lv == 2) { _snwprintf(t, 1600, L"%s", msg); g_diskLevel = 2; }
    else {
        BOOL have = FreeOn(out, &fr); Gb(have ? fr : 0, a); Gb(NeedBytes(), b);
        BOOL low = have && fr < NeedBytes();
        _snwprintf(t, 1600, L2(L"Instala em: %s   -   livre: %s, precisa de cerca de %s.%s %s", L"Installs into: %s   -   free: %s, needs about %s.%s %s"), out, have ? a : L"?", b,
                   low ? L2(L" ESPAÇO INSUFICIENTE: libere espaço ou escolha outra unidade.", L" NOT ENOUGH SPACE: free some space or pick another drive.") : L"",
                   IsPortheonDir(out) ? L2(L"Já tem um PORTHEON aí: ele é atualizado e seus saves e configurações ficam.", L"There is a PORTHEON there already: it is updated and your saves and settings stay.") : msg);
        g_diskLevel = low ? 2 : lv;
    }
    t[1599] = 0; SetWindowTextW(C(IDC_DISK), t); InvalidateRect(C(IDC_DISK), NULL, TRUE);
}
static void FillChecks(void)
{
    wchar_t s[1600]; PcSummary(s, 1600); SetWindowTextW(C(IDC_INFO), s);
    HWND lv = C(IDC_LIST); ListView_DeleteAllItems(lv); int first = -1;
    for (int g = 0; g < NG; g++) {
        LVITEMW it = {0}; it.mask = LVIF_TEXT; it.iItem = g; it.pszText = (LPWSTR)GAMES[g].name; ListView_InsertItem(lv, &it);
        ListView_SetItemText(lv, g, 1, (LPWSTR)LevelName(g_lv[g]));
        ListView_SetItemText(lv, g, 2, g_why[g][0] ? g_why[g] : (LPWSTR)L"-");
        if (first < 0 && g_lv[g]) first = g;
    }
    if (first >= 0) { ListView_SetItemState(lv, first, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); }
    else SetWindowTextW(C(IDC_DETAIL), L2(L"Este PC roda todos os jogos do PORTHEON. Clique numa linha para ver os detalhes.", L"This PC runs every PORTHEON game. Click a row for the details."));
}
static void PickFolder(void)
{
    IFileOpenDialog *d = NULL;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&d)) || !d) return;
    DWORD o = 0; d->lpVtbl->GetOptions(d, &o); d->lpVtbl->SetOptions(d, o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    d->lpVtbl->SetTitle(d, L2(L"Onde instalar o PORTHEON", L"Where to install PORTHEON"));
    {   wchar_t cur[PATHN]; GetWindowTextW(C(IDC_FOLDER), cur, PATHN); wchar_t *s = wcsrchr(cur, L'\\'); if (s && !DirExists(cur)) { if (s == cur + 2) s[1] = 0; else *s = 0; }
        IShellItem *si = NULL; if (DirExists(cur) && SUCCEEDED(SHCreateItemFromParsingName(cur, NULL, &IID_IShellItem, (void **)&si)) && si) { d->lpVtbl->SetFolder(d, si); si->lpVtbl->Release(si); } }
    if (SUCCEEDED(d->lpVtbl->Show(d, g_wnd))) {
        IShellItem *r = NULL; PWSTR p = NULL;
        if (SUCCEEDED(d->lpVtbl->GetResult(d, &r)) && r) {
            if (SUCCEEDED(r->lpVtbl->GetDisplayName(r, SIGDN_FILESYSPATH, &p)) && p) {
                wchar_t out[PATHN], msg[600]; ResolveTarget(p, out, msg, 600); SetWindowTextW(C(IDC_FOLDER), out);
                Log(L"folder picked: %s -> %s", p, out); CoTaskMemFree(p); }
            r->lpVtbl->Release(r);
        }
    }
    d->lpVtbl->Release(d);
}
static void StartInstall(void)
{
    wchar_t in[PATHN], msg[1200], q[1600], who[64] = L""; GetWindowTextW(C(IDC_FOLDER), in, PATHN);
    int lv = ResolveTarget(in, g_target, msg, 1200);
    if (lv == 2) { MessageBoxW(g_wnd, msg, L"PORTHEON", MB_OK | MB_ICONWARNING); return; }
    uint64_t fr = 0;
    if (FreeOn(g_target, &fr) && fr < NeedBytes()) {
        wchar_t a[32], b[32]; Gb(fr, a); Gb(NeedBytes(), b);
        _snwprintf(q, 1600, L2(L"A unidade %c: tem %s livres e o PORTHEON precisa de cerca de %s. A instalação deve parar com \"disco cheio\".\n\nInstalar mesmo assim?", L"Drive %c: has %s free and PORTHEON needs about %s. The install will likely stop with \"disk full\".\n\nInstall anyway?"), g_target[0], a, b);
        if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    }
    if (BusyIn(g_target, who, 64)) { _snwprintf(q, 1600, L2(L"%s está aberto e usa essa pasta: feche o PORTHEON e os jogos e clique em Instalar de novo.", L"%s is open and uses that folder: close PORTHEON and the games and click Install again."), who); MessageBoxW(g_wnd, q, L"PORTHEON", MB_OK | MB_ICONWARNING); return; }
    g_working = TRUE; g_cancel = 0; g_closeAfter = FALSE; ShowPage(PG_WORK); SendMessageW(C(IDC_PROG), PBM_SETPOS, 0, 0);
    HANDLE t = CreateThread(NULL, 0, InstallThread, NULL, 0, NULL); if (t) CloseHandle(t); else { g_working = FALSE; ShowPage(PG_FAIL); }
}
static void StartUninstall(void)
{
    g_keepSaves = SendMessageW(C(IDC_KEEP), BM_GETCHECK, 0, 0) == BST_CHECKED;
    wchar_t q[PATHN + 600];
    _snwprintf(q, PATHN + 600, L2(L"Apagar o PORTHEON de %s?%s", L"Delete PORTHEON from %s?%s"), g_udir,
               g_keepSaves ? L2(L"\n\nOs saves e as configurações vão antes para Documentos.", L"\n\nThe saves and settings go to Documents first.") : L2(L"\n\nOS SAVES DO UFC 5 E DO UFC 6 TAMBÉM SERÃO APAGADOS.", L"\n\nUFC 5'S AND UFC 6'S SAVES WILL BE DELETED TOO."));
    if (!g_snap[0] && MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    g_working = TRUE; ShowPage(PG_UWORK); SendMessageW(C(IDC_PROG), PBM_SETPOS, 0, 0);
    HANDLE t = CreateThread(NULL, 0, UninstallThread, NULL, 0, NULL); if (t) CloseHandle(t); else { g_working = FALSE; ShowPage(PG_UFAIL); }
}
static void OpenPortheon(void)
{
    wchar_t exe[PATHN], cmd[PATHN + 4]; Join(exe, g_target, L"PORTHEON.exe"); _snwprintf(cmd, PATHN + 4, L"\"%s\"", exe);
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi;
    if (CreateProcessW(exe, cmd, NULL, NULL, FALSE, 0, NULL, g_target, &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); Log(L"opened %s", exe); DestroyWindow(g_wnd); }
    else { wchar_t w[400], m[600]; ErrText(GetLastError(), w, 400); _snwprintf(m, 600, L2(L"Não consegui abrir o PORTHEON: %s", L"Could not open PORTHEON: %s"), w); MessageBoxW(g_wnd, m, L"PORTHEON", MB_OK | MB_ICONWARNING); }
}
static void ViewLog(void)
{
    wchar_t cmd[PATHN + 32], sys[PATHN]; GetWindowsDirectoryW(sys, PATHN); wcscat(sys, L"\\notepad.exe");
    _snwprintf(cmd, PATHN + 32, L"\"%s\" \"%s\"", sys, g_logPath);
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi;
    if (CreateProcessW(sys, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    else ShellExecuteW(g_wnd, L"open", g_logPath, NULL, NULL, SW_SHOWNORMAL);
}
static void SnapTo(HWND w, const wchar_t *path)   /* test mode: the window as a picture, without showing it on the screen */
{
    RECT r; GetWindowRect(w, &r); int W = r.right - r.left, H = r.bottom - r.top;
    HDC sdc = GetDC(NULL), mdc = CreateCompatibleDC(sdc); HBITMAP bm = CreateCompatibleBitmap(sdc, W, H); HGDIOBJ o = SelectObject(mdc, bm);
    /* off the screen DWM composes nothing: the frame and each child control are printed (WM_PRINT) and put together */
    PrintWindow(w, mdc, 0);
    for (HWND c = GetWindow(w, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        if (!IsWindowVisible(c)) continue;
        RECT cr; GetWindowRect(c, &cr); int cw = cr.right - cr.left, ch = cr.bottom - cr.top;
        HDC cdc = CreateCompatibleDC(sdc); HBITMAP cb = CreateCompatibleBitmap(sdc, cw, ch); HGDIOBJ co = SelectObject(cdc, cb);
        RECT z = {0, 0, cw, ch}; FillRect(cdc, &z, GetSysColorBrush(COLOR_WINDOW));
        wchar_t cls[32]; GetClassNameW(c, cls, 32);
        if (!_wcsicmp(cls, L"Static") || !_wcsicmp(cls, L"Button") || !_wcsicmp(cls, L"Edit")) SendMessageW(c, WM_PAINT, (WPARAM)cdc, 0);   /* they paint into a given DC */
        else PrintWindow(c, cdc, 0);
        BitBlt(mdc, cr.left - r.left, cr.top - r.top, cw, ch, cdc, 0, 0, SRCCOPY);
        SelectObject(cdc, co); DeleteObject(cb); DeleteDC(cdc);
    }
    SelectObject(mdc, o);
    BITMAPINFOHEADER bi = {sizeof(bi), W, H, 1, 24, BI_RGB, 0, 0, 0, 0, 0}; DWORD stride = ((DWORD)W * 3 + 3) & ~3u, sz = stride * (DWORD)H;
    BYTE *px = (BYTE *)malloc(sz);
    if (px && GetDIBits(mdc, bm, 0, (UINT)H, px, (BITMAPINFO *)&bi, DIB_RGB_COLORS)) {
        BITMAPFILEHEADER fh = {0x4D42, (DWORD)(sizeof(fh) + sizeof(bi) + sz), 0, 0, (DWORD)(sizeof(fh) + sizeof(bi))};
        HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); DWORD wr;
        if (f != INVALID_HANDLE_VALUE) { WriteFile(f, &fh, sizeof(fh), &wr, NULL); WriteFile(f, &bi, sizeof(bi), &wr, NULL); WriteFile(f, px, sz, &wr, NULL); CloseHandle(f); Log(L"snap: %s", path); }
    }
    free(px); DeleteObject(bm); DeleteDC(mdc); ReleaseDC(NULL, sdc);
}
static DWORD WINAPI ChecksThread(LPVOID unused)
{
    (void)unused; PcRead(); Verdicts();
    for (int g = 0; g < NG; g++) Log(L"verdict %s: %s%s%s", GAMES[g].name, LevelName(g_lv[g]), g_why[g][0] ? L" - " : L"", g_why[g]);
    InterlockedExchange(&g_checksDone, 1);
    if (g_wnd) PostMessageW(g_wnd, WM_APP_CHECKS, 0, 0);
    return 0;
}
static HWND Ctl(const wchar_t *cls, const wchar_t *text, DWORD style, int id, DWORD ex)
{
    return CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, g_wnd, (HMENU)(INT_PTR)id, g_inst, NULL);
}
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_CREATE: {
        g_wnd = h; g_dpi = DpiOf(h);
        wchar_t t[200];
        if (g_uninstMode) wcscpy(t, L2(L"Desinstalar o PORTHEON", L"Uninstall PORTHEON"));
        else _snwprintf(t, 200, L2(L"Instalar o Portheon %s", L"Install Portheon %s"), g_zip.ver);
        Ctl(L"STATIC", t, SS_LEFT | SS_NOPREFIX, IDC_TITLE, 0);
        Ctl(L"STATIC", L2(L"Os ports do UFC 1, 2, 3, 4, 5, 6 e UFC Undisputed 3 para PC, e o Zelda e o God of War III", L"The PC ports of UFC 1, 2, 3, 4, 5, 6 and UFC Undisputed 3, plus Zelda and God of War III"), SS_LEFT | SS_NOPREFIX, IDC_SUB, 0);
        Ctl(L"STATIC", L2(L"Verificando este PC...", L"Checking this PC..."), SS_LEFT | SS_NOPREFIX, IDC_INFO, 0);
        HWND lv = Ctl(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | WS_TABSTOP, IDC_LIST, WS_EX_CLIENTEDGE);
        ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
        {   const wchar_t *cols[3] = {L2(L"Jogo", L"Game"), L2(L"Neste PC", L"On this PC"), L2(L"Por quê", L"Why")};
            for (int i = 0; i < 3; i++) { LVCOLUMNW c = {0}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.cx = 100; c.pszText = (LPWSTR)cols[i]; ListView_InsertColumn(lv, i, &c); } }
        Ctl(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, IDC_DETAIL, 0);
        Ctl(L"STATIC", L2(L"Pasta de instalação (o PORTHEON grava os saves e os caches nela):", L"Install folder (PORTHEON keeps its saves and caches in it):"), SS_LEFT, IDC_FLBL, 0);
        Ctl(L"EDIT", g_target, ES_AUTOHSCROLL | WS_TABSTOP, IDC_FOLDER, WS_EX_CLIENTEDGE);
        Ctl(L"BUTTON", L2(L"Escolher...", L"Choose..."), BS_PUSHBUTTON | WS_TABSTOP, IDC_PICK, 0);
        Ctl(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, IDC_DISK, 0);
        Ctl(PROGRESS_CLASSW, L"", PBS_SMOOTH, IDC_PROG, 0); SendMessageW(C(IDC_PROG), PBM_SETRANGE32, 0, 1000);
        Ctl(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, IDC_STATUS, 0);
        {   wchar_t k[300]; _snwprintf(k, 300, L2(L"Guardar meus saves e configurações em Documentos\\PORTHEON-saves-<data> antes de apagar", L"Keep my saves and settings in Documents\\PORTHEON-saves-<date> before deleting"));
            Ctl(L"BUTTON", k, BS_AUTOCHECKBOX | BS_MULTILINE | WS_TABSTOP, IDC_KEEP, 0); SendMessageW(C(IDC_KEEP), BM_SETCHECK, BST_CHECKED, 0); }
        Ctl(L"BUTTON", L2(L"Ver registro", L"View log"), BS_PUSHBUTTON | WS_TABSTOP, IDC_LOG, 0);
        Ctl(L"BUTTON", L"", BS_DEFPUSHBUTTON | WS_TABSTOP, IDC_GO, 0);
        Ctl(L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP, IDC_CANCEL, 0);
        if (g_uninstMode) {
            wchar_t info[PATHN + 900];
            _snwprintf(info, PATHN + 900, L2(L"Isto apaga o PORTHEON de:\r\n%s\r\n\r\nSaem os programas, os ports, os caches de shaders, os atalhos e a entrada em Configurações > Aplicativos. Ficam: jogos que você colocou dentro dessa pasta (pastas com eboot.bin, default.xex, .pkg, .iso), arquivos que não vieram do PORTHEON e as configurações em %%LOCALAPPDATA%%\\PORTHEON.",
                                                L"This deletes PORTHEON from:\r\n%s\r\n\r\nThe programs, the ports, the shader caches, the shortcuts and the entry in Settings > Apps go. What stays: games you put inside that folder (folders with eboot.bin, default.xex, .pkg, .iso), files that did not come with PORTHEON, and the settings in %%LOCALAPPDATA%%\\PORTHEON."), g_udir);
            info[PATHN + 899] = 0; SetWindowTextW(C(IDC_INFO), info);
        }
        MakeFonts(); Layout(); ShowPage(g_uninstMode ? PG_UNINST : PG_CHECKS);
        if (!g_uninstMode) UpdateFolderInfo();
        if (g_snap[0] && g_uninstMode) SetTimer(h, 1, 300, NULL);
        return 0; }
    case WM_DPICHANGED: {
        g_dpi = HIWORD(wp); RECT *r = (RECT *)lp; MakeFonts();
        SetWindowPos(h, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE); Layout(); return 0; }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        if (g_bigIcon) DrawIconEx(dc, S(20), S(14), g_bigIcon, S(56), S(56), 0, NULL, DI_NORMAL);
        EndPaint(h, &ps); return 0; }
    case WM_PRINTCLIENT: {   /* --snap */
        RECT rc; GetClientRect(h, &rc); FillRect((HDC)wp, &rc, GetSysColorBrush(COLOR_WINDOW));
        if (g_bigIcon) DrawIconEx((HDC)wp, S(20), S(14), g_bigIcon, S(56), S(56), 0, NULL, DI_NORMAL);
        return 0; }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp; HWND c = (HWND)lp; int id = GetDlgCtrlID(c);
        SetBkColor(dc, GetSysColor(COLOR_WINDOW));
        SetTextColor(dc, id == IDC_DISK && g_diskLevel == 2 ? RGB(190, 0, 0) : id == IDC_DISK && g_diskLevel == 1 ? RGB(180, 95, 0)
                         : id == IDC_STATUS && (g_page == PG_FAIL || g_page == PG_UFAIL) ? RGB(190, 0, 0)
                         : id == IDC_STATUS && (g_page == PG_DONE || g_page == PG_UDONE) ? RGB(0, 120, 40)
                         : id == IDC_SUB ? GetSysColor(COLOR_GRAYTEXT) : GetSysColor(COLOR_WINDOWTEXT));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW); }
    case WM_NOTIFY: {
        NMHDR *nh = (NMHDR *)lp;
        if (nh->idFrom == IDC_LIST && nh->code == NM_CUSTOMDRAW) {
            NMLVCUSTOMDRAW *cd = (NMLVCUSTOMDRAW *)lp;
            if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) return CDRF_NOTIFYSUBITEMDRAW;
            if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
                int g = (int)cd->nmcd.dwItemSpec;
                cd->clrText = cd->iSubItem == 1 && g >= 0 && g < NG ? (g_lv[g] == 0 ? RGB(0, 125, 40) : g_lv[g] == 1 ? RGB(185, 100, 0) : RGB(195, 0, 0)) : GetSysColor(COLOR_WINDOWTEXT);
                return CDRF_NEWFONT;
            }
            return CDRF_DODEFAULT;
        }
        if (nh->idFrom == IDC_LIST && nh->code == LVN_ITEMCHANGED) {
            NMLISTVIEW *nl = (NMLISTVIEW *)lp;
            if ((nl->uNewState & LVIS_SELECTED) && nl->iItem >= 0 && nl->iItem < NG) {
                wchar_t t[1100]; int g = nl->iItem;
                _snwprintf(t, 1100, L"%s: %s. %s", GAMES[g].name, LevelName(g_lv[g]), g_why[g][0] ? g_why[g] : L2(L"Tudo certo neste PC.", L"Everything is fine on this PC.")); t[1099] = 0;
                SetWindowTextW(C(IDC_DETAIL), t);
            }
        }
        return 0; }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDC_FOLDER && HIWORD(wp) == EN_CHANGE) { UpdateFolderInfo(); return 0; }
        if (id == IDC_PICK) { PickFolder(); return 0; }
        if (id == IDC_LOG) { ViewLog(); return 0; }
        if (id == IDC_GO) {
            if (g_page == PG_CHECKS || g_page == PG_FAIL) StartInstall();
            else if (g_page == PG_DONE) OpenPortheon();
            else if (g_page == PG_UNINST || g_page == PG_UFAIL) StartUninstall();
            else if (g_page == PG_UDONE && g_backupDir[0]) ShellExecuteW(h, L"open", g_backupDir, NULL, NULL, SW_SHOWNORMAL);
            return 0;
        }
        if (id == IDC_CANCEL || id == IDCANCEL) { SendMessageW(h, WM_CLOSE, 0, 0); return 0; }
        return 0; }
    case WM_APP_PROG: SendMessageW(C(IDC_PROG), PBM_SETPOS, wp, 0); return 0;
    case WM_APP_STATUS: { wchar_t t[1600]; EnterCriticalSection(&g_cs); wcscpy(t, g_status); LeaveCriticalSection(&g_cs); SetWindowTextW(C(IDC_STATUS), t); InvalidateRect(C(IDC_STATUS), NULL, TRUE); return 0; }
    case WM_APP_CHECKS: FillChecks(); if (g_page == PG_CHECKS) EnableWindow(C(IDC_GO), TRUE); if (g_snap[0]) SetTimer(h, 1, 400, NULL); return 0;
    case WM_TIMER: KillTimer(h, 1); if (g_snap[0]) { UpdateWindow(h); SnapTo(h, g_snap); DestroyWindow(h); } return 0;
    case WM_APP_DONE:
        g_working = FALSE;
        if (g_uninstMode) ShowPage(wp ? PG_UDONE : PG_UFAIL); else ShowPage(wp ? PG_DONE : PG_FAIL);
        if (!g_uninstMode && !wp) UpdateFolderInfo();
        SendMessageW(h, WM_APP_STATUS, 0, 0);
        if (g_closeAfter) DestroyWindow(h);
        return 0;
    case WM_CLOSE:
        if (g_working) {
            if (g_uninstMode) return 0;   /* half a delete is worse than waiting a few seconds */
            int a = MessageBoxW(h, L2(L"Parar a instalação? Rodar o instalador de novo continua de onde parou.", L"Stop the install? Running the installer again picks up where it stopped."), L"PORTHEON", MB_YESNO | MB_ICONQUESTION);
            /* the install may have ended while the box was open (WM_APP_DONE runs inside its loop): its result page stays */
            if (!g_working) return 0;
            if (a == IDYES) {
                InterlockedExchange(&g_cancel, 1); g_closeAfter = TRUE;
                EnterCriticalSection(&g_cs); if (g_tarProc) TerminateProcess(g_tarProc, 1); LeaveCriticalSection(&g_cs);
                Log(L"cancelled by the player");
            }
            return 0;
        }
        DestroyWindow(h); return 0;
    case WM_DESTROY:
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}
static int RunWindow(void)
{
    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES}; InitCommonControlsEx(&ic);
    WNDCLASSEXW wc = {sizeof(wc)}; wc.lpfnWndProc = WndProc; wc.hInstance = g_inst; wc.lpszClassName = L"PortheonSetup";
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW); wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(1)); wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    RegisterClassExW(&wc);
    UINT dpi = DpiOf(NULL); g_dpi = dpi; DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r = {0, 0, MulDiv(CW, (int)dpi, 96), MulDiv(CH, (int)dpi, 96)}; AdjustWindowRectEx(&r, style, FALSE, 0);
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int ww = r.right - r.left, wh = r.bottom - r.top, x = wa.left + (wa.right - wa.left - ww) / 2, y = wa.top + (wa.bottom - wa.top - wh) / 2;
    if (y < wa.top) y = wa.top;
    if (g_snap[0]) { x = -ww - 4000; y = 0; }   /* test pictures: off every screen, never activated */
    wchar_t title[120]; if (g_uninstMode) wcscpy(title, L2(L"PORTHEON - desinstalar", L"PORTHEON - uninstall")); else _snwprintf(title, 120, L2(L"Portheon %s - instalação", L"Portheon %s setup"), g_zip.ver);
    HWND w = CreateWindowExW(g_snap[0] ? WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW : 0, wc.lpszClassName, title, style, x, y, ww, wh, NULL, NULL, g_inst, NULL);
    if (!w) return 1;
    ShowWindow(w, g_snap[0] ? SW_SHOWNOACTIVATE : SW_SHOWNORMAL); UpdateWindow(w);
    if (!g_uninstMode) { HANDLE t = CreateThread(NULL, 0, ChecksThread, NULL, 0, NULL); if (t) CloseHandle(t); }
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) { if (!IsDialogMessageW(w, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    g_wnd = NULL;
    return 0;
}

/* ---------------------------------------------------------------- start */
static void Report(void)
{
    PcRead(); Verdicts(); wchar_t s[1600]; PcSummary(s, 1600); Out(s); Log(L"%s", s);
    for (int g = 0; g < NG; g++) { wchar_t t[1200]; _snwprintf(t, 1200, L"%-17s %-20s %s", GAMES[g].name, LevelName(g_lv[g]), g_why[g]); t[1199] = 0; Out(t); Log(L"%s", t); }
}
/* "D:\PORTHEON\" --x: CommandLineToArgvW reads \" as a quote inside the argument, so the folder would swallow the next
   flags. No argument of this program ever holds a quote, so every run of backslashes right before a quote becomes two
   (one literal backslash, then the closing quote): "D:\" stays D:\ and "D:\x\" is D:\x\. NULL: use the line as it is. */
static wchar_t *FolderSafeCmdLine(const wchar_t *cl)
{
    size_t n = wcslen(cl); wchar_t *o = (wchar_t *)malloc((n * 2 + 1) * sizeof(wchar_t)); if (!o) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; ) {
        if (cl[i] == L'\\') {
            size_t k = i; while (k < n && cl[k] == L'\\') k++;
            if (k < n && cl[k] == L'"') { o[j++] = L'\\'; o[j++] = L'\\'; }
            else while (i < k) o[j++] = cl[i++];
            i = k; continue;
        }
        o[j++] = cl[i++];
    }
    o[j] = 0; return o;
}
static BOOL HexTo(const wchar_t *h, uint8_t *out)
{
    if (wcslen(h) != 64) return FALSE;
    for (int i = 0; i < 32; i++) { unsigned v; if (swscanf(h + i * 2, L"%2x", &v) != 1) return FALSE; out[i] = (uint8_t)v; }
    return TRUE;
}
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdLine, int show)
{
    (void)prev; (void)cmdLine; (void)show; g_inst = inst;
    InitializeCriticalSection(&g_cs);
    g_pt = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_PORTUGUESE;
    { wchar_t v[8]; g_test = GetEnvironmentVariableW(L"PORTHEON_SETUP_TEST", v, 8) && v[0] == L'1'; }
    GetTempPathW(PATHN, g_logPath); Join(g_logPath, g_logPath, L"portheon-setup.log");
    GetModuleFileNameW(NULL, g_self, PATHN); wcscpy(g_selfDir, g_self); { wchar_t *s = wcsrchr(g_selfDir, L'\\'); if (s) *s = 0; }
    int argc = 0; wchar_t *cl = FolderSafeCmdLine(GetCommandLineW()); wchar_t **av = CommandLineToArgvW(cl ? cl : GetCommandLineW(), &argc); free(cl);
    Log(L"---- PORTHEON-Setup %s, %s%s", g_self, GetCommandLineW(), g_test ? L"  [PORTHEON_SETUP_TEST=1]" : L"");
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    Footer ft; uint64_t fsz = 0;
    if (ReadFooter(g_self, &ft, &fsz)) {
        g_hasPayload = TRUE; g_stubLen = ft.off; wcscpy(g_pl.file, g_self); g_pl.off = ft.off; g_pl.size = ft.size; memcpy(g_pl.sha, ft.sha, 32); g_pl.haveSha = TRUE;
        Log(L"package inside: offset %llu, %llu bytes", (unsigned long long)ft.off, (unsigned long long)ft.size);
    } else { g_stubLen = fsz; if (!fsz) { WIN32_FILE_ATTRIBUTE_DATA fa; if (GetFileAttributesExW(g_self, GetFileExInfoStandard, &fa)) g_stubLen = ((uint64_t)fa.nFileSizeHigh << 32) | fa.nFileSizeLow; } Log(L"no package inside: uninstaller only"); }
    DropOldUninstallers();
    const wchar_t *mode = argc > 1 ? av[1] : L"";
    int rc = 0; wchar_t err[1600];

    if (!_wcsicmp(mode, L"--report")) { g_quiet = TRUE; Report(); goto out; }

    if (!_wcsicmp(mode, L"--quiet-uninstall") || !_wcsicmp(mode, L"--uninstall-from") || !_wcsicmp(mode, L"--uninstall")
        || (!_wcsicmp(mode, L"--snap") && argc > 3 && !_wcsicmp(av[3], L"uninstall")) || (!g_hasPayload && !mode[0])) {
        BOOL quiet = !_wcsicmp(mode, L"--quiet-uninstall"); for (int i = 2; i < argc; i++) if (!_wcsicmp(av[i], L"--quiet")) quiet = TRUE;
        g_quiet = quiet;
        if (!_wcsicmp(mode, L"--snap")) { if (!g_test) { rc = 2; goto out; } wcsncpy(g_snap, av[2], PATHN - 1); }
        if ((!_wcsicmp(mode, L"--quiet-uninstall") || !_wcsicmp(mode, L"--uninstall-from")) && argc > 2) {
            /* "D:\PORTHEON\" on a command line arrives as D:\PORTHEON" (the backslash escapes the quote): quotes go */
            const wchar_t *a = av[2]; while (*a == L' ' || *a == L'"') a++; wcsncpy(g_udir, a, PATHN - 1); g_udir[PATHN - 1] = 0;
            for (size_t l = wcslen(g_udir); l && (g_udir[l - 1] == L'"' || g_udir[l - 1] == L' '); ) g_udir[--l] = 0;
        }
        else {
            /* its own folder only as the installed uninstaller, or as the registered folder: a stub left next to some
               other PORTHEON.exe (a build folder, a Downloads folder) must never offer to empty that folder */
            wchar_t reg[PATHN]; BOOL hasReg = RegInstallDir(reg); const wchar_t *nm = wcsrchr(g_self, L'\\'); nm = nm ? nm + 1 : g_self;
            if (IsPortheonDir(g_selfDir) && (!_wcsicmp(nm, UNINST_EXE) || (hasReg && !_wcsicmp(reg, g_selfDir)))) wcscpy(g_udir, g_selfDir);
            else if (hasReg && IsPortheonDir(reg)) wcscpy(g_udir, reg);
        }
        if (g_snap[0] && !g_udir[0]) wcscpy(g_udir, L"D:\\PORTHEON");
        { wchar_t full[PATHN]; if (g_udir[0] && GetFullPathNameW(g_udir, PATHN, full, NULL)) wcscpy(g_udir, full); size_t l = wcslen(g_udir); while (l > 3 && g_udir[l - 1] == L'\\') g_udir[--l] = 0; }
        /* the saves are kept unless asked not to: the window's box starts ticked, and without a window (tools, PORTHEON's
           own updates) a forgotten flag must never cost the player the UFC 5/6 saves; --delete-saves skips the copy */
        g_keepSaves = TRUE; for (int i = 2; i < argc; i++) if (!_wcsicmp(av[i], L"--delete-saves")) g_keepSaves = FALSE;
        if (!_wcsicmp(mode, L"--uninstall-from") && argc > 3) { g_waitPid = wcstoul(av[3], NULL, 10); g_fromTemp = TRUE; }
        if (!g_udir[0]) {
            const wchar_t *t = L2(L"Este é o desinstalador do PORTHEON, mas não achei nenhum PORTHEON instalado. Para instalar, use o PORTHEON-Setup.exe da página oficial.", L"This is PORTHEON's uninstaller, but no installed PORTHEON was found. To install, use PORTHEON-Setup.exe from the official page.");
            Log(L"%s", t); if (quiet) Out(t); else MessageBoxW(NULL, t, L"PORTHEON", MB_OK | MB_ICONINFORMATION); rc = 2; goto out;
        }
        /* running from inside the folder it deletes: the work is done by a copy in %TEMP% */
        if (!g_fromTemp && !g_snap[0] && PathUnder(g_self, g_udir)) {
            wchar_t extra[PATHN] = L"";
            if (quiet) wcscpy(extra, g_keepSaves ? L" --quiet --keep-saves" : L" --quiet --delete-saves");
            HANDLE ch = RelaunchFromTemp(g_udir, extra);
            if (!ch) { const wchar_t *t = L2(L"Não consegui copiar o desinstalador para a pasta temporária (veja o registro).", L"Could not copy the uninstaller to the temporary folder (see the log)."); if (quiet) Out(t); else MessageBoxW(NULL, t, L"PORTHEON", MB_OK | MB_ICONWARNING); rc = 1; goto out; }
            /* the copy waits for this process to end (its exe is in the folder), so nothing waits for the copy here */
            if (quiet) Out(L"the uninstall goes on from a copy in %TEMP% (see portheon-setup.log)");
            CloseHandle(ch); goto out;
        }
        if (quiet) {
            rc = DoUninstall(g_udir, g_keepSaves, err, 1600) ? 0 : 1;
            if (rc) { Out(err); Log(L"failed: %s", err); } else Out(L"uninstalled");
        } else { g_uninstMode = TRUE; RunWindow(); }
        if (g_fromTemp) SelfDeleteLater();
        goto out;
    }

    /* the installer */
    BOOL selftest = !_wcsicmp(mode, L"--selftest"), quiet = selftest || !_wcsicmp(mode, L"--quiet-install");
    g_quiet = quiet;
    if (selftest) {
        if (argc < 4) { Out(L"usage: --selftest <zip> <dir> [--sha <hex>]"); rc = 2; goto out; }
        if (!GetFullPathNameW(av[2], PATHN, g_pl.file, NULL)) wcsncpy(g_pl.file, av[2], PATHN - 1);
        WIN32_FILE_ATTRIBUTE_DATA fa; if (!GetFileAttributesExW(g_pl.file, GetFileExInfoStandard, &fa)) { Out(L"no such zip"); rc = 4; goto out; }
        g_pl.off = 0; g_pl.size = ((uint64_t)fa.nFileSizeHigh << 32) | fa.nFileSizeLow; g_pl.haveSha = FALSE;
        for (int i = 4; i + 1 < argc; i++) if (!_wcsicmp(av[i], L"--sha")) g_pl.haveSha = HexTo(av[i + 1], g_pl.sha);
        if (g_hasPayload) g_stubLen = ft.off;
        g_hasPayload = TRUE;
    }
    if (!_wcsicmp(mode, L"--snap")) { if (!g_test || argc < 3) { rc = 2; goto out; } wcsncpy(g_snap, av[2], PATHN - 1); }
    if (WinBuild() < 17134) {   /* Windows 10 1803: tar.exe, and every port needs newer anyway */
        wchar_t t[600]; _snwprintf(t, 600, L2(L"Este Windows é antigo demais (versão %lu). O PORTHEON precisa do Windows 10 versão 1803 ou mais novo (os jogos pedem a 1903), ou do Windows 11: atualize pelo Windows Update.", L"This Windows is too old (build %lu). PORTHEON needs Windows 10 version 1803 or newer (the games want 1903), or Windows 11: update through Windows Update."), WinBuild());
        Log(L"%s", t); if (quiet) Out(t); else MessageBoxW(NULL, t, L"PORTHEON", MB_OK | MB_ICONERROR); rc = 3; goto out;
    }
    if (!g_hasPayload) {
        const wchar_t *t = L2(L"Este é o desinstalador do PORTHEON, sem o pacote de instalação dentro. Para instalar, baixe o PORTHEON-Setup.exe da página oficial.", L"This is PORTHEON's uninstaller, without the install package inside. To install, download PORTHEON-Setup.exe from the official page.");
        Log(L"%s", t); if (quiet) Out(t); else MessageBoxW(NULL, t, L"PORTHEON", MB_OK | MB_ICONINFORMATION); rc = 4; goto out;
    }
    if (!ZipScan(&g_pl, &g_zip)) {
        _snwprintf(err, 1600, L"%s", L2(L"O pacote dentro do instalador está corrompido (não é um zip legível): baixe o PORTHEON-Setup.exe de novo.", L"The package inside the installer is damaged (not a readable zip): download PORTHEON-Setup.exe again."));
        Log(L"%s", err); if (quiet) Out(err); else MessageBoxW(NULL, err, L"PORTHEON", MB_OK | MB_ICONERROR); rc = 1; goto out;
    }
    Log(L"package: %s, version %s, %lu entries, %lu files, %llu bytes, longest name %lu", g_zip.top, g_zip.ver, g_zip.entries, g_zip.files, (unsigned long long)g_zip.total, g_zip.maxName);
    if (quiet) {
        if (argc < (selftest ? 4 : 3)) { Out(L"usage: --quiet-install <dir> [--shortcuts]"); rc = 2; goto out; }
        const wchar_t *d = av[selftest ? 3 : 2]; wchar_t msg[1200];
        g_wantLinks = FALSE; for (int i = 2; i < argc; i++) if (!_wcsicmp(av[i], L"--shortcuts")) g_wantLinks = TRUE;
        /* the same folder rules as the window, result included: an update in place names a PORTHEON folder, which stays as
           given; a drive root, a Windows folder (Downloads...) or a folder with other things in it gets its own \PORTHEON,
           so a later uninstall never empties the player's folder. ResolveTarget also drops the quote a "D:\x\" leaves. */
        if (ResolveTarget(d, g_target, msg, 1200) == 2) { Out(msg); Log(L"refused: %s", msg); rc = 2; goto out; }
        if (msg[0]) Log(L"folder note: %s", msg);
        Log(L"folder: \"%s\" -> %s", d, g_target);
        PcRead(); Verdicts(); for (int g = 0; g < NG; g++) Log(L"verdict %s: %s%s%s", GAMES[g].name, LevelName(g_lv[g]), g_why[g][0] ? L" - " : L"", g_why[g]);
        rc = DoInstall(g_target, g_wantLinks, err, 1600) ? 0 : 1;
        if (rc) { Out(err); Log(L"failed: %s", err); } else { wchar_t t[PATHN + 64]; _snwprintf(t, PATHN + 64, L"installed %s into %s", g_zip.ver, g_target); Out(t); }
        goto out;
    }
    /* the window: the folder of an installed PORTHEON (an update in place), else the roomiest fixed drive */
    { wchar_t reg[PATHN]; if (RegInstallDir(reg) && IsPortheonDir(reg)) wcscpy(g_target, reg); else DefaultFolder(g_target); }
    RunWindow();
out:
    Log(L"exit %d", rc);
    if (av) LocalFree(av);
    CoUninitialize();
    return rc;
}
