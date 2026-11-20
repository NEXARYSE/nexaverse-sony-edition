/* iconrefresh.h - used by PORTHEON (portheon.c) and by its installer (setup\setup.c).
 * 6.0 final (the user, 2026-11-07: "PORTHEON's icon is the old one on the desktop and in the taskbar when I start it"):
 * Windows keeps a program's icon in its icon cache by the file's path and the icon's index, so a new PORTHEON.exe put
 * where the old one was (an update, or the installer over an older install) goes on showing the old logo on its
 * shortcuts and on its taskbar button (the button shows the icon of the shortcut that carries PORTHEON's taskbar ID).
 * IconRefreshAll tells the shell that the picture of "<exe>",0 changed (SHUpdateImage, with its place in the system
 * image list), that PORTHEON's shortcuts changed, and to refresh its icons (SHCNE_ASSOCCHANGED, what installers send).
 * Only notifications: nothing of Windows' own files is touched. Call it with COM initialized, off the UI thread. */
#ifndef PORTHEON_ICONREFRESH_H
#define PORTHEON_ICONREFRESH_H
static void IconRefreshAll(const wchar_t *exe)
{
    SHFILEINFOW fi; memset(&fi, 0, sizeof(fi));
    if (SHGetFileInfoW(exe, 0, &fi, sizeof(fi), SHGFI_SYSICONINDEX)) {
        typedef void (WINAPI *UpdateImageFn)(LPCWSTR, int, UINT, int);
        HMODULE sh = GetModuleHandleW(L"shell32.dll");
        UpdateImageFn up = sh ? (UpdateImageFn)(void *)GetProcAddress(sh, "SHUpdateImageW") : NULL;
        if (up) up(exe, 0, 0, fi.iIcon);
        else SHChangeNotify(SHCNE_UPDATEIMAGE, SHCNF_DWORD | SHCNF_FLUSHNOWAIT, NULL, (LPCVOID)(INT_PTR)fi.iIcon);
    }
    /* the shortcuts PORTHEON and its installer make, and the taskbar pin Windows copies from the Start menu one */
    static const KNOWNFOLDERID *const kf[3] = {&FOLDERID_Desktop, &FOLDERID_Programs, &FOLDERID_RoamingAppData};
    static const wchar_t *const tail[3] = {L"PORTHEON.lnk", L"PORTHEON.lnk", L"Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar\\PORTHEON.lnk"};
    for (int i = 0; i < 3; i++) {
        PWSTR d = NULL; wchar_t lnk[MAX_PATH];
        if (FAILED(SHGetKnownFolderPath(kf[i], 0, NULL, &d)) || !d) continue;
        _snwprintf(lnk, MAX_PATH, L"%s\\%s", d, tail[i]); lnk[MAX_PATH - 1] = 0; CoTaskMemFree(d);
        if (GetFileAttributesW(lnk) != INVALID_FILE_ATTRIBUTES) SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW | SHCNF_FLUSHNOWAIT, lnk, NULL);
    }
    SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW | SHCNF_FLUSHNOWAIT, exe, NULL);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, NULL, NULL);
}
#endif
