/* extras.c - part of portheon.c (4.0): profiles, the save manager, cheats and the DLC page.
 *  - Profiles: like a console's users. Each has a name (the name the games show), a color and an avatar. The first
 *    profile plays with the saves that were already on this PC (%LOCALAPPDATA%\UFCnNative\user); every other one has
 *    its own (...\UFCnNative\profiles\<id>\user, BB_USER_DIR), while the shader caches stay shared (BB_GPU_USER_DIR),
 *    so a new profile does not compile every shader again.
 *  - Saves: the slots of each game (savedata\<user>\<title>\<slot>, named by the .sce_sys\param.bin the port writes),
 *    backups as .zip files in Documents\PORTHEON\Backups (Windows' own tar.exe), restore, and an automatic backup
 *    before each PLAY (the last 8 are kept): a save broken by a crash comes back with one click.
 *  - Cheats: PORTHEON's cheat packs (games\UFCn\cheats\*.json, ported from the GoldHEN cheat repository's UFC 4 cheats
 *    and checked on the games' own code), switched on here and written to %LOCALAPPDATA%\UFCnNative\cheats.json, which
 *    the port's link step reads (scripts\cheats.py): every cheat is code the game runs from a segment of its own.
 *  - DLC: what the ports do with add-on content. */

/* ---------------------------------------------------------------- profiles */
#define MAXPROF 8
/* 4.4.0, the user: "create your account at the start, put your photo, set online / offline / busy, add or remove
   users, like a PS5". status: 0 online, 1 busy (no notifications but errors), 2 offline (shown as offline on this PC).
   photo: 1 = %LOCALAPPDATA%\PORTHEON\avatars\<id>.png (a 256x256 square cut from the player's picture); 2 is only the
   editor's preview of a picture not saved yet. */
enum { PST_ONLINE, PST_BUSY, PST_OFFLINE };
typedef struct { wchar_t name[32]; int color, avatar, id, status, photo; } Profile;
static Profile g_prof[MAXPROF]; static int g_nprof, g_curProf;
static const COLORREF PROF_COLORS[12] = {RGB(124, 58, 237) /* 5.0: the default is the brand purple */, RGB(36, 170, 255), RGB(255, 92, 46), RGB(72, 206, 136), RGB(255, 196, 40), RGB(255, 64, 129),
                                         RGB(0, 200, 200), RGB(120, 140, 255), RGB(214, 38, 48), RGB(160, 210, 60), RGB(255, 140, 0), RGB(180, 180, 196)};
#define NAVATARS 8   /* drawn in ui.c (Avatar): glove, belt, octagon, fist, star, flame, crown, the initial */
static void ProfilesSave(void)
{
    wchar_t k[32], v[16];
    PIniSet(L"profiles", g_nprof); PIniSet(L"profile_cur", g_curProf);
    for (int i = 0; i < MAXPROF; i++) {
        _snwprintf(k, 32, L"prof_name_%d", i); WritePrivateProfileStringW(L"PORTHEON", k, i < g_nprof ? g_prof[i].name : NULL, g_pIni);
        _snwprintf(k, 32, L"prof_color_%d", i); _snwprintf(v, 16, L"%d", g_prof[i].color); WritePrivateProfileStringW(L"PORTHEON", k, i < g_nprof ? v : NULL, g_pIni);
        _snwprintf(k, 32, L"prof_avatar_%d", i); _snwprintf(v, 16, L"%d", g_prof[i].avatar); WritePrivateProfileStringW(L"PORTHEON", k, i < g_nprof ? v : NULL, g_pIni);
        _snwprintf(k, 32, L"prof_id_%d", i); _snwprintf(v, 16, L"%d", g_prof[i].id); WritePrivateProfileStringW(L"PORTHEON", k, i < g_nprof ? v : NULL, g_pIni);
        _snwprintf(k, 32, L"prof_status_%d", i); _snwprintf(v, 16, L"%d", g_prof[i].status); WritePrivateProfileStringW(L"PORTHEON", k, i < g_nprof ? v : NULL, g_pIni);
        _snwprintf(k, 32, L"prof_photo_%d", i); _snwprintf(v, 16, L"%d", g_prof[i].photo == 1); WritePrivateProfileStringW(L"PORTHEON", k, i < g_nprof ? v : NULL, g_pIni);
    }
}
static void ProfilesLoad(void)
{
    g_nprof = PIni(L"profiles", 0); if (g_nprof > MAXPROF) g_nprof = MAXPROF;
    wchar_t k[32];
    for (int i = 0; i < g_nprof; i++) {
        _snwprintf(k, 32, L"prof_name_%d", i); GetPrivateProfileStringW(L"PORTHEON", k, L"", g_prof[i].name, 32, g_pIni);
        _snwprintf(k, 32, L"prof_color_%d", i); g_prof[i].color = PIni(k, i) % 12;
        _snwprintf(k, 32, L"prof_avatar_%d", i); g_prof[i].avatar = PIni(k, 0) % NAVATARS;
        _snwprintf(k, 32, L"prof_id_%d", i); g_prof[i].id = PIni(k, i);
        _snwprintf(k, 32, L"prof_status_%d", i); g_prof[i].status = PIni(k, PST_ONLINE); if (g_prof[i].status < 0 || g_prof[i].status > 2) g_prof[i].status = PST_ONLINE;
        _snwprintf(k, 32, L"prof_photo_%d", i); g_prof[i].photo = PIni(k, 0) ? 1 : 0;
        if (!g_prof[i].name[0]) _snwprintf(g_prof[i].name, 32, L"Player %d", i + 1);
    }
    if (!g_nprof) {   /* the first start of 4.0: one profile with the name the games already had */
        g_nprof = 1; g_prof[0].id = 0; g_prof[0].color = 0; g_prof[0].avatar = 0; g_prof[0].status = PST_ONLINE; g_prof[0].photo = 0;
        wchar_t n[64] = L""; for (int g = NGAMES - 1; g >= 0 && !n[0]; g--) { wchar_t sf[MAX_PATH]; SfPath(g, sf); if (FileExists(sf)) { g_kvGame = -1; GetStr(g, L"player", n, 64, L""); } }
        g_kvGame = -1;
        wcsncpy(g_prof[0].name, n[0] ? n : L"Player 1", 31); g_prof[0].name[31] = 0;
        PIniSet(L"prof_next_id", 1);
        ProfilesSave();
    }
    g_curProf = PIni(L"profile_cur", 0); if (g_curProf < 0 || g_curProf >= g_nprof) g_curProf = 0;
}
static Profile *CurProf(void) { return &g_prof[g_curProf]; }
static const wchar_t *ProfName(void) { return CurProf()->name; }   /* 4.3.6: for portheon.c before this file (Ufc2OnlineEnv) */
static void ProfileUserDir(int g, const Profile *p, wchar_t *out)
{
    wchar_t d[MAX_PATH]; LocalData(d, G[g].data);
    if (!p->id) { Join(out, d, L"user"); return; }
    wchar_t s[64]; _snwprintf(s, 64, L"profiles\\%d\\user", p->id); Join(out, d, s);
}
static int ProfileAdd(const wchar_t *name, int color, int avatar)
{
    if (g_nprof >= MAXPROF) return -1;
    Profile *p = &g_prof[g_nprof];
    wcsncpy(p->name, name && name[0] ? name : L"Player", 31); p->name[31] = 0;
    p->color = color % 12; p->avatar = avatar % NAVATARS; p->status = PST_ONLINE; p->photo = 0;
    int nid = PIni(L"prof_next_id", 1); if (nid < 1) nid = 1;
    for (int i = 0; i < g_nprof; i++) if (g_prof[i].id >= nid) nid = g_prof[i].id + 1;
    p->id = nid; PIniSet(L"prof_next_id", nid + 1);
    g_nprof++; ProfilesSave(); return g_nprof - 1;
}
/* a profile's saves stay on disk when it is removed (its folder is named in the message) */
static void PhotoPath(int id, wchar_t *out);
static void PhotoForget(int id);
static void ProfileRemove(int i)
{
    if (i <= 0 || i >= g_nprof) return;   /* the first profile owns the saves that were here before: it stays */
    { wchar_t ph[MAX_PATH]; PhotoPath(g_prof[i].id, ph); PhotoForget(g_prof[i].id); DeleteFileW(ph); }   /* 4.4.0: its photo goes (not a save) */
    for (int k = i; k < g_nprof - 1; k++) g_prof[k] = g_prof[k + 1];
    g_nprof--; if (g_curProf >= g_nprof) g_curProf = 0; else if (g_curProf > i) g_curProf--;
    ProfilesSave();
}
static void ProfileSelect(int i)
{
    if (i < 0 || i >= g_nprof) return;
    g_curProf = i; PIniSet(L"profile_cur", i);
    if (g_editPlayer) SetWindowTextW(g_editPlayer, g_prof[i].name);
}
/* ---------------------------------------------------------------- 4.4.0: the account's photo and status */
static COLORREF ProfStatusColor(int st) { return st == PST_BUSY ? RGB(255, 84, 84) : st == PST_OFFLINE ? RGB(128, 136, 158) : RGB(56, 214, 120); }
static const wchar_t *ProfStatusName(int st) { return st == PST_BUSY ? L2(L"Ocupado", L"Busy") : st == PST_OFFLINE ? L2(L"Offline", L"Offline") : L2(L"Online", L"Online"); }
static const wchar_t *ProfStatusHint(int st)
{
    return st == PST_BUSY ? L2(L"Não perturbe: só os avisos de erro aparecem", L"Do not disturb: only error notices show")
         : st == PST_OFFLINE ? L2(L"Aparece offline neste PC", L"Shows as offline on this PC")
         : L2(L"Disponível, com todos os avisos", L"Available, with every notice");
}
static void ProfileSetStatus(int i, int st)
{
    if (i < 0 || i >= g_nprof || st < PST_ONLINE || st > PST_OFFLINE) return;
    g_prof[i].status = st; ProfilesSave();
}
static void PhotoPath(int id, wchar_t *out)
{
    wchar_t d[MAX_PATH], f[32]; LocalData(d, L"PORTHEON"); Join(d, d, L"avatars"); CreateDirectoryW(d, NULL);
    _snwprintf(f, 32, L"%d.png", id); Join(out, d, f);
}
/* the photos already read (one per profile id; read again after a change) */
typedef struct { int id; BOOL tried; GpImage *img; } PhotoSlot;
static PhotoSlot g_photos[MAXPROF + 2];
static GpImage *g_pePhotoImg;   /* the editor's picture, not saved yet (Profile.photo == 2 shows it) */
static void PhotoForget(int id)
{
    for (int i = 0; i < MAXPROF + 2; i++)
        if (g_photos[i].tried && g_photos[i].id == id) { if (g_photos[i].img) GdipDisposeImage(g_photos[i].img); g_photos[i].img = NULL; g_photos[i].tried = FALSE; }
}
static void PhotosFree(void)   /* also while a game runs: nothing of PORTHEON's pictures stays in memory */
{
    for (int i = 0; i < MAXPROF + 2; i++) { if (g_photos[i].img) GdipDisposeImage(g_photos[i].img); g_photos[i].img = NULL; g_photos[i].tried = FALSE; }
}
static GpImage *PhotoGet(const Profile *p)
{
    if (p->photo == 2) return g_pePhotoImg;
    if (p->photo != 1) return NULL;
    int slot = -1;
    for (int i = 0; i < MAXPROF + 2; i++) { if (g_photos[i].tried && g_photos[i].id == p->id) return g_photos[i].img; if (!g_photos[i].tried && slot < 0) slot = i; }
    if (slot < 0) { PhotosFree(); slot = 0; }
    wchar_t ph[MAX_PATH]; PhotoPath(p->id, ph);
    g_photos[slot].id = p->id; g_photos[slot].tried = TRUE; g_photos[slot].img = LoadImageFile(ph);
    return g_photos[slot].img;
}
int WINAPI GdipGetPropertyItemSize(GpImage *, PROPID, UINT *);
int WINAPI GdipGetPropertyItem(GpImage *, PROPID, UINT, void *);
int WINAPI GdipImageRotateFlip(GpImage *, int);
/* the player picks a picture: it is turned upright (a phone's photo carries its turn in the EXIF data), cut to a square
   around the middle (a little above it, where a face usually is) and made 256x256. NULL: none picked, or not a picture */
static GpImage *PhotoPick(void)
{
    wchar_t file[MAX_PATH] = L"", *pics = NULL;
    OPENFILENAMEW of = {sizeof(of)}; of.hwndOwner = g_wnd; of.lpstrFile = file; of.nMaxFile = MAX_PATH;
    of.lpstrFilter = L"PNG, JPG, BMP, GIF\0*.png;*.jpg;*.jpeg;*.jfif;*.bmp;*.gif\0";
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR; of.lpstrTitle = L2(L"Escolha a sua foto", L"Pick your photo");
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Pictures, 0, NULL, &pics)) && pics) of.lpstrInitialDir = pics;
#ifdef PORTHEON_TEST   /* PORTHEON_TEST_PHOTO=<file>: the picture without the file dialog (UI tests) */
    BOOL got = GetEnvironmentVariableW(L"PORTHEON_TEST_PHOTO", file, MAX_PATH) > 0 || GetOpenFileNameW(&of);
#else
    BOOL got = GetOpenFileNameW(&of);
#endif
    if (pics) CoTaskMemFree(pics);
    if (!got) return NULL;
    GpImage *src = LoadImageFile(file); UINT w = 0, h = 0;
    if (src) { GdipGetImageWidth(src, &w); GdipGetImageHeight(src, &h); }
    if (!src || !w || !h) {
        if (src) GdipDisposeImage(src);
        Msg(C_ORANGE, L"%s", L2(L"Essa imagem não abriu: use uma foto PNG, JPG, BMP ou GIF de até 24 MB.", L"That picture did not open: use a PNG, JPG, BMP or GIF photo of up to 24 MB."));
        return NULL;
    }
    {   /* EXIF orientation (0x0112): 3 = upside down, 6 = turned right, 8 = turned left */
        UINT sz = 0;
        if (!GdipGetPropertyItemSize(src, 0x0112, &sz) && sz >= 16 && sz < 4096) {
            BYTE *buf = (BYTE *)malloc(sz);
            if (buf && !GdipGetPropertyItem(src, 0x0112, sz, buf)) {
                struct { PROPID id; ULONG length; WORD type; void *value; } *pi = (void *)buf;
                int o = pi->value && pi->length >= 2 ? *(WORD *)pi->value : 1;
                int rf = o == 3 ? 2 /* Rotate180FlipNone */ : o == 6 ? 1 /* Rotate90FlipNone */ : o == 8 ? 3 /* Rotate270FlipNone */ : 0;
                if (rf) { GdipImageRotateFlip(src, rf); GdipGetImageWidth(src, &w); GdipGetImageHeight(src, &h); }
            }
            free(buf);
        }
    }
    UINT s = w < h ? w : h;
    GpBitmap *b = NULL;
    if (GdipCreateBitmapFromScan0(256, 256, 0, 0x26200A /* PixelFormat32bppARGB */, NULL, &b) || !b) { GdipDisposeImage(src); return NULL; }
    GpGraphics *gr = NULL; GdipGetImageGraphicsContext((GpImage *)b, &gr);
    if (gr) {
        GdipSetInterpolationMode(gr, 7); GdipSetPixelOffsetMode(gr, 4);
        GdipDrawImageRectRectI(gr, src, 0, 0, 256, 256, (INT)(w - s) / 2, (INT)(h - s) / 3, (INT)s, (INT)s, 2 /* UnitPixel */, NULL, NULL, NULL);
        GdipDeleteGraphics(gr);
    }
    GdipDisposeImage(src);
    return (GpImage *)b;
}
static BOOL PhotoSave(int id, GpImage *img)
{
    static const CLSID png = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
    wchar_t ph[MAX_PATH]; PhotoPath(id, ph); PhotoForget(id);
    return img && !GdipSaveImageToFile(img, ph, &png, NULL);
}
static void PhotoDelete(int id) { wchar_t ph[MAX_PATH]; PhotoPath(id, ph); PhotoForget(id); DeleteFileW(ph); }

static void SavesAutoBackup(int g);
static int CopyTree(const wchar_t *src, const wchar_t *dst, wchar_t *bad);
static ULONGLONG DirSize(const wchar_t *d, FILETIME *newest, int depth);
/* 5.1 (the user's call, 2026-11-05: "this savedata in UFC 4 for everyone"): UFC 4's starter save, games\UFC4\save-inicial
   (a profile past the intro and one created fighter, in the port's layout: <slot>\DATA, INDEX and <slot>.sce_sys\param.bin).
   It goes into the profile's saves at PLAY only where that title has no profile yet, for both UFC 4 title IDs (US and EU
   dumps), slot by slot, never over a file that is there. */
static void StarterSave4(int g, const wchar_t *userDir)
{
    if (g != 3) return;
    wchar_t src[MAX_PATH]; Join(src, g_exeDir, L"games\\UFC4\\save-inicial\\CUSA14209"); if (!DirExists(src)) return;
    static const wchar_t *const tids[2] = {L"CUSA14209", L"CUSA14204"};
    for (int t = 0; t < 2; t++) {
        wchar_t dst[MAX_PATH], prof[MAX_PATH], sub[64]; _snwprintf(sub, 64, L"savedata\\1\\%s", tids[t]); Join(dst, userDir, sub);
        Join(prof, dst, L"PROFILE\\DATA"); if (FileExists(prof)) continue;
        SHCreateDirectoryExW(NULL, dst, NULL);
        wchar_t q[MAX_PATH]; Join(q, src, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(q, &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {   /* each slot folder only when the player has none of that name */
            if (fd.cFileName[0] == L'.' || !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            wchar_t a[MAX_PATH], b[MAX_PATH], bad[MAX_PATH] = L""; Join(a, src, fd.cFileName); Join(b, dst, fd.cFileName);
            if (DirExists(b) && !DirSize(b, NULL, 0)) {   /* an empty slot from an intro that was closed: kept aside, never deleted */
                wchar_t old[MAX_PATH]; _snwprintf(old, MAX_PATH, L"%s.vazio-%llu", b, (unsigned long long)GetTickCount64()); old[MAX_PATH - 1] = 0; MoveFileW(b, old);
            }
            if (!DirExists(b)) CopyTree(a, b, bad);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
}
/* at PLAY: the profile's name in the game, its own saves folder, the shared shader cache, a backup of its saves */
static void ProfileEnv(int g)
{
    Profile *p = CurProf();
    { wchar_t u[MAX_PATH]; ProfileUserDir(g, p, u); StarterSave4(g, u); }   /* 5.1 */
    wchar_t cur[64]; GetStr(g, L"player", cur, 64, L""); if (wcscmp(cur, p->name)) PutStr(g, L"player", p->name);
    if (p->id) {
        wchar_t u[MAX_PATH], shared[MAX_PATH], d[MAX_PATH]; ProfileUserDir(g, p, u); LocalData(d, G[g].data); Join(shared, d, L"user");
        SHCreateDirectoryExW(NULL, u, NULL); CreateDirectoryW(shared, NULL);
        EnvPush(L"BB_USER_DIR", u); EnvPush(L"BB_GPU_USER_DIR", shared);
    }
    SavesAutoBackup(g);
}

/* ---------------------------------------------------------------- saves */
typedef struct { wchar_t dir[MAX_PATH], slot[64], title[128], sub[128]; FILETIME mt; ULONGLONG size; } SaveInfo;
static ULONGLONG DirSize(const wchar_t *d, FILETIME *newest, int depth)
{
    ULONGLONG s = 0; wchar_t pat[MAX_PATH]; Join(pat, d, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (fd.cFileName[0] == L'.' && (!fd.cFileName[1] || (fd.cFileName[1] == L'.' && !fd.cFileName[2]))) continue;
        if (newest && CompareFileTime(&fd.ftLastWriteTime, newest) > 0) *newest = fd.ftLastWriteTime;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { if (depth < 6) { wchar_t sub[MAX_PATH]; Join(sub, d, fd.cFileName); s += DirSize(sub, newest, depth + 1); } }
        else s += ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
    } while (FindNextFileW(h, &fd));
    FindClose(h); return s;
}
static void SaveParamTitle(const wchar_t *slotDir, wchar_t *title, wchar_t *sub)   /* <slot>.sce_sys\param.bin: title at 0, subtitle at 0x80 */
{
    title[0] = sub[0] = 0;
    wchar_t p[MAX_PATH]; _snwprintf(p, MAX_PATH, L"%s.sce_sys\\param.bin", slotDir); p[MAX_PATH - 1] = 0;
    DWORD n = 0; BYTE *b = ReadWhole(p, &n); if (!b) return;
    if (n >= 0x80) { char t[129]; memcpy(t, b, 128); t[128] = 0; MultiByteToWideChar(CP_UTF8, 0, t, -1, title, 128); title[127] = 0; }
    if (n >= 0x100) { char t[129]; memcpy(t, b + 0x80, 128); t[128] = 0; MultiByteToWideChar(CP_UTF8, 0, t, -1, sub, 128); sub[127] = 0; }
    free(b);
}
static void SaveRoot(int g, wchar_t *out) { wchar_t u[MAX_PATH]; ProfileUserDir(g, CurProf(), u); Join(out, u, L"savedata"); }
static int SaveCmp(const void *a, const void *b) { return CompareFileTime(&((const SaveInfo *)b)->mt, &((const SaveInfo *)a)->mt); }
static int SaveList(int g, SaveInfo *out, int max)
{
    wchar_t root[MAX_PATH]; SaveRoot(g, root); int n = 0;
    wchar_t pu[MAX_PATH]; Join(pu, root, L"*"); WIN32_FIND_DATAW fu; HANDLE hu = FindFirstFileW(pu, &fu);   /* users (1, 2...) */
    if (hu == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(fu.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fu.cFileName[0] == L'.') continue;
        wchar_t ud[MAX_PATH], pt[MAX_PATH]; Join(ud, root, fu.cFileName); Join(pt, ud, L"CUSA*");
        WIN32_FIND_DATAW ft; HANDLE ht = FindFirstFileW(pt, &ft);
        if (ht == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(ft.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            wchar_t td[MAX_PATH], ps[MAX_PATH]; Join(td, ud, ft.cFileName); Join(ps, td, L"*");
            WIN32_FIND_DATAW fs; HANDLE hs = FindFirstFileW(ps, &fs);
            if (hs == INVALID_HANDLE_VALUE) continue;
            do {
                if (!(fs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fs.cFileName[0] == L'.' || wcsstr(fs.cFileName, L".sce_sys") || n >= max) continue;
                SaveInfo *s = &out[n++]; Join(s->dir, td, fs.cFileName); wcsncpy(s->slot, fs.cFileName, 63); s->slot[63] = 0;
                s->mt = fs.ftLastWriteTime; s->size = DirSize(s->dir, &s->mt, 0);
                SaveParamTitle(s->dir, s->title, s->sub);
            } while (FindNextFileW(hs, &fs));
            FindClose(hs);
        } while (FindNextFileW(ht, &ft));
        FindClose(ht);
    } while (FindNextFileW(hu, &fu));
    FindClose(hu);
    qsort(out, n, sizeof(SaveInfo), SaveCmp);
    return n;
}
static BOOL BackupRoot(int g, wchar_t *out, BOOL autoDir)   /* Documents\PORTHEON\Backups\UFC4\<profile>[\auto] */
{
    PWSTR docs = NULL; if (FAILED(SHGetKnownFolderPath(&FOLDERID_Documents, 0, NULL, &docs)) || !docs) return FALSE;
    wchar_t safe[32]; wcsncpy(safe, CurProf()->name, 31); safe[31] = 0;
    for (wchar_t *c = safe; *c; c++) if (wcschr(L"\\/:*?\"<>|", *c)) *c = L'_';
    _snwprintf(out, MAX_PATH, L"%s\\PORTHEON\\Backups\\%s\\%s%s", docs, G[g].id, safe, autoDir ? L"\\auto" : L""); out[MAX_PATH - 1] = 0;
    CoTaskMemFree(docs);
    return SHCreateDirectoryExW(NULL, out, NULL) == ERROR_SUCCESS || DirExists(out);
}
static DWORD RunTar(const wchar_t *args, const wchar_t *cwd)   /* Windows' own bsdtar (Windows 10 1803+): its exit code, or 9999 */
{
    wchar_t tar[MAX_PATH], cmd[4 * MAX_PATH]; GetSystemDirectoryW(tar, MAX_PATH); wcscat(tar, L"\\tar.exe");
    if (!FileExists(tar)) return 9999;
    _snwprintf(cmd, 4 * MAX_PATH, L"\"%s\" %s", tar, args); cmd[4 * MAX_PATH - 1] = 0;
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi; DWORD code = 9999;
    if (CreateProcessW(tar, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, NULL, cwd, &si, &pi)) {   /* 6.0 final: below normal: it never competes with a game's start */
        WaitForSingleObject(pi.hProcess, 120000); GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    return code;
}
/* a .zip of the profile's savedata folder; file: the zip made */
static BOOL SavesBackupTo(int g, BOOL autoDir, wchar_t *file)
{
    wchar_t root[MAX_PATH], user[MAX_PATH], dir[MAX_PATH]; SaveRoot(g, root); ProfileUserDir(g, CurProf(), user);
    if (!DirExists(root) || !DirSize(root, NULL, 0)) return FALSE;
    if (!BackupRoot(g, dir, autoDir)) return FALSE;
    SYSTEMTIME t; GetLocalTime(&t);
    _snwprintf(file, MAX_PATH, L"%s\\%s %04d-%02d-%02d %02dh%02d%02d.zip", dir, G[g].name, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond); file[MAX_PATH - 1] = 0;
    wchar_t args[3 * MAX_PATH]; _snwprintf(args, 3 * MAX_PATH, L"-a -c -f \"%s\" savedata", file); args[3 * MAX_PATH - 1] = 0;
    DWORD code = RunTar(args, user);
    if (code || !FileExists(file)) { DeleteFileW(file); return FALSE; }
    return TRUE;
}
static int FileTimeCmp(const void *a, const void *b) { return CompareFileTime(&((const WIN32_FIND_DATAW *)a)->ftLastWriteTime, &((const WIN32_FIND_DATAW *)b)->ftLastWriteTime); }
static void SavesAutoBackup(int g)
{
    if (!PIni(L"save_autobackup", 1)) return;
    {   /* 6.0 final: no new zip (and no tar.exe before the game starts) when nothing in the saves is newer than the newest
           automatic backup: two PLAYs in a row, a short or crashed session */
        wchar_t root[MAX_PATH], bd[MAX_PATH], bp[MAX_PATH]; FILETIME saves = {0, 0}, last = {0, 0}; SaveRoot(g, root);
        if (DirExists(root) && DirSize(root, &saves, 0) && BackupRoot(g, bd, TRUE)) {
            Join(bp, bd, L"*.zip"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(bp, &fd);
            if (h != INVALID_HANDLE_VALUE) { do if (CompareFileTime(&fd.ftLastWriteTime, &last) > 0) last = fd.ftLastWriteTime; while (FindNextFileW(h, &fd)); FindClose(h); }
            if ((last.dwLowDateTime | last.dwHighDateTime) && CompareFileTime(&saves, &last) <= 0) return;   /* the newest zip already holds these saves */
        }
    }
    wchar_t f[MAX_PATH]; if (!SavesBackupTo(g, TRUE, f)) return;
    wchar_t dir[MAX_PATH], pat[MAX_PATH]; if (!BackupRoot(g, dir, TRUE)) return; Join(pat, dir, L"*.zip");
    static WIN32_FIND_DATAW all[256]; int n = 0; HANDLE h = FindFirstFileW(pat, &all[0]);
    if (h == INVALID_HANDLE_VALUE) return;
    do { if (++n >= 256) break; } while (FindNextFileW(h, &all[n]));
    FindClose(h);
    qsort(all, n, sizeof(all[0]), FileTimeCmp);
    for (int i = 0; i + 8 < n; i++) { wchar_t old[MAX_PATH]; Join(old, dir, all[i].cFileName); DeleteFileW(old); }   /* the 8 newest stay */
}
static void SavesBackupNow(int g)
{
    wchar_t f[MAX_PATH];
    if (SavesBackupTo(g, FALSE, f)) Msg(C_GOOD, L2(L"Backup dos saves do %s feito: %s", L"%s saves backed up: %s"), G[g].name, f);
    else Msg(C_ORANGE, L2(L"O %s ainda não tem saves neste perfil (jogue uma vez primeiro).", L"%s has no saves in this profile yet (play once first)."), G[g].name);
}
static void SavesOpenFolder(int g)
{
    wchar_t r[MAX_PATH]; SaveRoot(g, r); SHCreateDirectoryExW(NULL, r, NULL);
    ShellExecuteW(g_wnd, L"open", r, NULL, NULL, SW_SHOWNORMAL);
}
static void SavesOpenBackups(int g) { wchar_t d[MAX_PATH]; if (BackupRoot(g, d, FALSE)) ShellExecuteW(g_wnd, L"open", d, NULL, NULL, SW_SHOWNORMAL); }
static void DelTree(const wchar_t *d);   /* update.c */
/* restore a backup: the saves of now go to a backup of their own first, then the chosen one is unpacked in their place */
static void SavesRestore(int g)
{
    if (g_proc) { Msg(C_ORANGE, L"%s", L2(L"Feche o jogo antes de restaurar um backup.", L"Close the game before restoring a backup.")); return; }
    wchar_t dir[MAX_PATH], file[MAX_PATH] = L""; if (!BackupRoot(g, dir, FALSE)) return;
    OPENFILENAMEW of = {sizeof(of)}; of.hwndOwner = g_wnd; of.lpstrFilter = L"Backup (*.zip)\0*.zip\0"; of.lpstrFile = file; of.nMaxFile = MAX_PATH;
    of.lpstrInitialDir = dir; of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    wchar_t title[96]; _snwprintf(title, 96, L2(L"Restaurar saves do %s", L"Restore %s saves"), G[g].name); of.lpstrTitle = title;
    if (!GetOpenFileNameW(&of)) return;
    wchar_t q[700]; _snwprintf(q, 700, L2(L"Restaurar os saves do %s com o backup\n%s?\n\nOs saves de agora viram um backup antes (pasta auto).", L"Restore %s's saves from the backup\n%s?\n\nThe current saves become a backup first (auto folder)."), G[g].name, PathFindFileNameW(file)); q[699] = 0;
    if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    wchar_t root[MAX_PATH], user[MAX_PATH], keep[MAX_PATH]; SaveRoot(g, root); ProfileUserDir(g, CurProf(), user);
    BOOL had = DirExists(root) && DirSize(root, NULL, 0) > 0;
    if (had && !SavesBackupTo(g, TRUE, keep)) { Msg(C_BAD, L"%s", L2(L"Não consegui guardar os saves de agora: nada foi mudado.", L"Could not keep the current saves: nothing was changed.")); return; }
    SHCreateDirectoryExW(NULL, user, NULL);
    wchar_t tmp[MAX_PATH]; Join(tmp, user, L"savedata.restore"); DelTree(tmp);
    if (had && !MoveFileW(root, tmp)) { Msg(C_BAD, L"%s", L2(L"Os saves estão em uso (o jogo ou um antivírus): feche e tente de novo.", L"The saves are in use (the game or an antivirus): close it and try again.")); return; }
    wchar_t args[3 * MAX_PATH]; _snwprintf(args, 3 * MAX_PATH, L"-x -f \"%s\"", file); args[3 * MAX_PATH - 1] = 0;
    DWORD code = RunTar(args, user);
    if (code || !DirExists(root)) {   /* put the old ones back */
        DelTree(root); if (had) MoveFileW(tmp, root);
        Msg(C_BAD, L2(L"O backup não abriu (tar %lu): os saves de antes continuam.", L"The backup did not unpack (tar %lu): the saves of before stay."), code); return;
    }
    DelTree(tmp);
    Msg(C_GOOD, L2(L"Saves do %s restaurados de %s.", L"%s saves restored from %s."), G[g].name, PathFindFileNameW(file));
}

/* ---------------------------------------------------------------- Released (11-13): import saves (UFC 1-4)
   Each port ships scripts\savedata_import.py (the port's own, GPL): it finds this game's saves under a folder (a shadPS4
   user or savedata folder, a decrypted PS4 save with sce_sys\param.sfo, Apollo-named folders), writes them in the port's
   layout into the profile's user folder and moves a save it replaces to <user>\savedata-backup\<time> first. The source
   is only read. PORTHEON picks the folder, shows what --list found, keeps its own .zip of the profile's saves
   (SavesBackupTo, the hand-made backups' folder: never pruned) and only then runs --import: two safety copies. The
   script runs hidden on the port's own Python (-I -E -s: nothing of the PC's Python), its words read through a pipe,
   with a time limit. Its exit codes: 0 saves found / imported, 3 nothing to import, 2 a bad source or a failed copy. */
#define SIMP_NOSTART 9998u
#define SIMP_TIMEOUT 9997u
static DWORD SaveImpRun(const wchar_t *exe, wchar_t *cmd, const wchar_t *cwd, DWORD ms, wchar_t *out, int n)
{
    out[0] = 0;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE}; HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 1 << 16)) return SIMP_NOSTART;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi = {0};
    si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = wr; si.hStdError = wr; si.hStdInput = nul != INVALID_HANDLE_VALUE ? nul : NULL;
    BOOL ok = CreateProcessW(exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, cwd, &si, &pi);
    CloseHandle(wr); if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) { CloseHandle(rd); return SIMP_NOSTART; }
    static char buf[16384]; DWORD have = 0, code = SIMP_TIMEOUT; ULONGLONG end = GetTickCount64() + ms; BOOL done = FALSE;
    for (;;) {
        DWORD av = 0, got = 0;
        while (PeekNamedPipe(rd, NULL, 0, NULL, &av, NULL) && av) {   /* what does not fit is read and dropped: the script never waits on a full pipe */
            char tmp[4096]; if (!ReadFile(rd, tmp, sizeof(tmp), &got, NULL) || !got) break;
            DWORD room = (DWORD)sizeof(buf) - 1 - have, keep = got < room ? got : room; memcpy(buf + have, tmp, keep); have += keep;
        }
        if (done) break;
        if (WaitForSingleObject(pi.hProcess, 40) == WAIT_OBJECT_0) { done = TRUE; continue; }   /* one more pass empties the pipe */
        if (GetTickCount64() > end) { TerminateProcess(pi.hProcess, SIMP_TIMEOUT); WaitForSingleObject(pi.hProcess, 3000); break; }
    }
    if (done) GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(rd);
    buf[have] = 0; MultiByteToWideChar(CP_UTF8, 0, buf, -1, out, n); out[n - 1] = 0;
    return code;
}
/* the command line: --game only for a chosen game folder with its param.sfo (it names the save title: an European or
   another region's copy has its own); --title is the fallback the script takes without it */
static void SaveImpCmd(int g, const wchar_t *py, const wchar_t *script, const wchar_t *src, BOOL doImport, BOOL *haveGame, wchar_t *cmd, int n)
{
    wchar_t user[MAX_PATH], game[MAX_PATH], sfo[MAX_PATH], gopt[MAX_PATH + 16] = L"", full[MAX_PATH];
    ProfileUserDir(g, CurProf(), user); GameDir(g, game); StripSlash(game); Join(sfo, game, L"sce_sys\\param.sfo");
    BOOL hg = game[0] && !wcschr(game, L'"') && FileExists(sfo) && !(GetFullPathNameW(game, MAX_PATH, full, NULL) && PathIsRootW(full));
    if (hg) { _snwprintf(gopt, MAX_PATH + 16, L" --game \"%s\"", game); gopt[MAX_PATH + 15] = 0; }
    if (haveGame) *haveGame = hg;
    _snwprintf(cmd, n, L"\"%s\" -I -E -s \"%s\" --source \"%s\" --user \"%s\"%s --title %s %s", py, script, src, user, gopt, G[g].tid, doImport ? L"--import" : L"--list");
    cmd[n - 1] = 0;
}
/* what --list printed: "N save(s) for CUSAxxxxx in <source>:" then one line per save, two spaces first:
   "  NAME (title / subtitle): kind, saved <when>, N files, N KB[ - replaces the one the port has (backed up first)]" */
typedef struct { int n, repl; wchar_t title[16], list[1200]; } SaveImpFound;
static void SaveImpParse(const wchar_t *o, SaveImpFound *f)
{
    memset(f, 0, sizeof(*f));
    const wchar_t *p = wcsstr(o, L" save(s) for "); if (!p) return;
    wcsncpy(f->title, p + 13, 9); f->title[9] = 0;
    p = wcschr(p, L'\n'); int l = 0;
    while (p && p[1] == L' ' && p[2] == L' ') {
        const wchar_t *s = p + 3, *e = wcschr(s, L'\n'); size_t len = e ? (size_t)(e - s) : wcslen(s);
        wchar_t line[400]; if (len > 399) len = 399; wcsncpy(line, s, len); line[len] = 0;
        BOOL repl = wcsstr(line, L"replaces the one the port has") != NULL;
        wchar_t *k = wcsstr(line, L", saved "); if (k) { *k = 0; k = wcsrchr(line, L':'); if (k) *k = 0; }
        for (wchar_t *c = line; *c; c++) if (*c < 32) *c = L' ';
        if (wcslen(line) > 72) wcscpy(line + 69, L"...");
        f->n++; f->repl += repl;
        if (f->n <= 8 && l < 1000) l += _snwprintf(f->list + l, 1200 - l, L"%s  - %s%s", l ? L"\n" : L"", line, repl ? L2(L"   [substitui o deste perfil]", L"   [replaces this profile's]") : L"");
        p = e;
    }
    if (f->n > 8 && l < 1100) _snwprintf(f->list + l, 1200 - l, L2(L"\n  ... e mais %d", L"\n  ... and %d more"), f->n - 8);
    f->list[1199] = 0;
}
/* the script's own words for a message box: the last part, whole lines, no carriage returns */
static void SaveImpTail(const wchar_t *o, wchar_t *out, int n)
{
    size_t len = wcslen(o); while (len && (o[len - 1] == L'\n' || o[len - 1] == L'\r' || o[len - 1] == L' ')) len--;
    size_t from = len > (size_t)(n - 1) ? len - (size_t)(n - 1) : 0;
    if (from) { while (from < len && o[from] != L'\n') from++; if (from < len) from++; }
    int k = 0; for (size_t i = from; i < len && k < n - 1; i++) if (o[i] != L'\r') out[k++] = o[i];
    out[k] = 0;
}
static void SavesImport(int g)
{
    if (!PS4G(g)) return;
    if ((g_proc && g_procGame == g) || FindProcess(G[g].exe) || FindProcess(L"sacredfire-game.exe")) { Msg(C_ORANGE, L2(L"Feche o %s antes de importar saves.", L"Close %s before importing saves."), G[g].name); return; }
    wchar_t dir[MAX_PATH], py[MAX_PATH], script[MAX_PATH];
    PortDir(g, dir); Join(py, dir, L"python\\python.exe"); Join(script, dir, L"scripts\\savedata_import.py");
    if (!FileExists(py) || !FileExists(script)) {
        Msg(C_BAD, L2(L"Falta o importador de saves do %s (scripts\\savedata_import.py ou o python do port). Extraia o PORTHEON de novo.", L"%s's save importer is missing (scripts\\savedata_import.py or the port's python). Extract PORTHEON again."), G[g].name); return;
    }
    static wchar_t last[MAX_PATH]; wchar_t src[MAX_PATH], t[200], full[MAX_PATH];
    wcscpy(src, last);
    _snwprintf(t, 200, L2(L"Importar saves do %s: escolha a pasta com os saves (shadPS4 ou PS4 descriptografado)", L"Import %s saves: choose the folder with the saves (shadPS4 or decrypted PS4)"), G[g].name); t[199] = 0;
    BOOL picked = PickFolder(src, t); SetFocus(g_wnd); if (!picked) return;
    src[MAX_PATH - 1] = 0; StripSlash(src);
    if (wcschr(src, L'"') || !DirExists(src)) return;
    if (GetFullPathNameW(src, MAX_PATH, full, NULL) && PathIsRootW(full)) {
        Msg(C_ORANGE, L"%s", L2(L"Isso é o disco inteiro. Escolha a pasta onde os saves estão (a pasta user ou savedata do shadPS4, ou a pasta do save descriptografado do PS4).",
                               L"That is a whole drive. Choose the folder the saves are in (shadPS4's user or savedata folder, or the decrypted PS4 save's folder).")); return;
    }
    wcscpy(last, src);
    static wchar_t out[8192]; wchar_t cmd[6 * MAX_PATH], tail[900], q[3200]; BOOL haveGame = FALSE;
    SaveImpCmd(g, py, script, src, FALSE, &haveGame, cmd, 6 * MAX_PATH);
    HCURSOR oc = SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_WAIT));
    DWORD code = SaveImpRun(py, cmd, dir, 90000, out, 8192);
    SetCursor(oc);
    SaveImpTail(out, tail, 900);
    if (code == 3) {   /* nothing of this game there; the script says why */
        BOOL enc = wcsstr(out, L"encrypted PS4 save") != NULL;
        _snwprintf(q, 3200, L"%s\n\n%s\n\n%s\n%s",
            enc ? L2(L"Essa pasta tem saves de PS4 criptografados: a cópia que o PS4 faz no pendrive (arquivos \"sdimg_...\" com um \".bin\"). Só um PS4 abre esses arquivos.\n\nDescriptografe antes (Apollo Save Tool num PS4 desbloqueado: \"Copy save game\" com descriptografia; ou Save Wizard) e importe a pasta descriptografada.",
                     L"That folder holds encrypted PS4 saves: the copy a PS4 makes to a USB drive (\"sdimg_...\" files with a \".bin\"). Only a PS4 can open them.\n\nDecrypt them first (Apollo Save Tool on a jailbroken PS4: \"Copy save game\" with decryption; or Save Wizard) and import the decrypted folder.")
                : L2(L"Nenhum save deste jogo foi encontrado nessa pasta.", L"No save of this game was found in that folder."),
            L2(L"Escolha a pasta user ou savedata do shadPS4 (user\\savedata\\1\\CUSAxxxxx), um save de PS4 descriptografado (com a pasta sce_sys dentro), ou uma pasta com eles dentro.",
               L"Choose shadPS4's user or savedata folder (user\\savedata\\1\\CUSAxxxxx), a decrypted PS4 save (with its sce_sys folder), or a folder with those in it."),
            L2(L"O que o port respondeu:", L"What the port answered:"), tail);
        q[3199] = 0; MessageBoxW(g_wnd, q, L"PORTHEON", MB_OK | MB_ICONINFORMATION);
        Msg(C_ORANGE, L2(L"Nenhum save do %s para importar nessa pasta.", L"No %s save to import in that folder."), G[g].name); return;
    }
    if (code != 0) {
        _snwprintf(q, 3200, L"%s\n\n%s\n%s",
            code == SIMP_TIMEOUT ? L2(L"A busca dos saves demorou demais e foi interrompida. Escolha a pasta onde os saves estão, não uma pasta grande com muita coisa dentro.", L"Looking for the saves took too long and was stopped. Choose the folder the saves are in, not a big folder with many things in it.")
            : code == SIMP_NOSTART ? L2(L"Não consegui abrir o importador de saves do port (o python do port não iniciou; um antivírus pode ter bloqueado).", L"Could not start the port's save importer (the port's python did not start; an antivirus may have blocked it).")
            : L2(L"O importador não conseguiu ler essa pasta. Nada foi mudado.", L"The importer could not read that folder. Nothing was changed."),
            L2(L"O que o port respondeu:", L"What the port answered:"), tail[0] ? tail : L"-");
        q[3199] = 0; MessageBoxW(g_wnd, q, L"PORTHEON", MB_OK | MB_ICONWARNING);
        Msg(C_BAD, L2(L"Importação de saves do %s: a pasta não pôde ser lida (código %lu). Nada foi mudado.", L"%s save import: the folder could not be read (code %lu). Nothing was changed."), G[g].name, code); return;
    }
    static SaveImpFound f; SaveImpParse(out, &f);
    wchar_t head[200], repl[200] = L"", nogame[300] = L"";
    if (f.n) _snwprintf(head, 200, L2(L"Importar %d save(s) para o %s, no perfil %s?", L"Import %d save(s) into %s, profile %s?"), f.n, G[g].name, CurProf()->name);
    else _snwprintf(head, 200, L2(L"Importar estes saves para o %s, no perfil %s?", L"Import these saves into %s, profile %s?"), G[g].name, CurProf()->name);
    head[199] = 0;
    if (f.repl) { _snwprintf(repl, 200, L2(L"%d deles substituem saves que este perfil já tem.\n\n", L"%d of them replace saves this profile already has.\n\n"), f.repl); repl[199] = 0; }
    if (!haveGame) { _snwprintf(nogame, 300, L2(L"A pasta do jogo ainda não foi escolhida: os saves vão para o título %s. Se a sua cópia do jogo for de outra região, escolha a pasta do jogo primeiro e importe depois.\n\n",
                                                 L"The game folder has not been chosen yet: the saves go to title %s. If your copy of the game is from another region, choose the game folder first and import afterwards.\n\n"), f.title[0] ? f.title : G[g].tid); nogame[299] = 0; }
    _snwprintf(q, 3200, L"%s\n\n%s\n\n%s%s%s", head, f.n ? f.list : tail, repl, nogame,
        L2(L"Antes de importar, o PORTHEON guarda os saves de agora num .zip (Documentos\\PORTHEON\\Backups) e o port move os que forem substituídos para a pasta savedata-backup. A pasta escolhida só é lida: nada nela muda.",
           L"Before importing, PORTHEON keeps the current saves in a .zip (Documents\\PORTHEON\\Backups) and the port moves the ones that get replaced to its savedata-backup folder. The chosen folder is only read: nothing in it changes."));
    q[3199] = 0;
    if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    if ((g_proc && g_procGame == g) || FindProcess(G[g].exe) || FindProcess(L"sacredfire-game.exe")) { Msg(C_ORANGE, L2(L"Feche o %s antes de importar saves.", L"Close %s before importing saves."), G[g].name); return; }
    /* PORTHEON's own copy first (the same .zip "Back up now" makes); a profile with no saves yet has nothing to keep */
    wchar_t root[MAX_PATH], keep[MAX_PATH] = L""; SaveRoot(g, root);
    BOOL had = DirExists(root) && DirSize(root, NULL, 0) > 0;
    oc = SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_WAIT));
    if (had && !SavesBackupTo(g, FALSE, keep)) { SetCursor(oc); Msg(C_BAD, L"%s", L2(L"Não consegui guardar os saves de agora: nada foi importado.", L"Could not keep the current saves: nothing was imported.")); return; }
    SaveImpCmd(g, py, script, src, TRUE, NULL, cmd, 6 * MAX_PATH);
    code = SaveImpRun(py, cmd, dir, 600000, out, 8192);
    SetCursor(oc);
    SaveImpTail(out, tail, 900);
    wchar_t user[MAX_PATH], pb[MAX_PATH]; ProfileUserDir(g, CurProf(), user); Join(pb, user, L"savedata-backup");
    if (code != 0) {
        _snwprintf(q, 3200, L"%s\n\n%s%s%s%s\n\n%s\n%s",
            code == SIMP_TIMEOUT ? L2(L"A importação demorou demais e foi interrompida: parte dos saves pode não ter sido copiada.", L"The import took too long and was stopped: some saves may not have been copied.")
            : code == SIMP_NOSTART ? L2(L"Não consegui abrir o importador de saves do port. Nada foi importado.", L"Could not start the port's save importer. Nothing was imported.")
            : L2(L"A importação não terminou bem: pelo menos um save não foi copiado.", L"The import did not end well: at least one save was not copied."),
            had ? L2(L"Os saves de antes estão guardados em:\n", L"The saves from before are kept in:\n") : L"", had ? keep : L"", had ? L"\n" : L"",
            had ? pb : L"", L2(L"O que o port respondeu:", L"What the port answered:"), tail[0] ? tail : L"-");
        q[3199] = 0; MessageBoxW(g_wnd, q, L"PORTHEON", MB_OK | MB_ICONWARNING);
        Msg(C_BAD, L2(L"A importação de saves do %s falhou (código %lu). Para voltar atrás: Saves > Restaurar backup.", L"%s save import failed (code %lu). To go back: Saves > Restore a backup."), G[g].name, code); return;
    }
    if (had) Msg(C_GOOD, L2(L"%d save(s) importado(s) para o %s (perfil %s). Os saves de antes ficaram em %s; para voltar atrás: Restaurar backup.", L"%d save(s) imported into %s (profile %s). The saves from before are in %s; to go back: Restore a backup."), f.n, G[g].name, CurProf()->name, keep);
    else Msg(C_GOOD, L2(L"%d save(s) importado(s) para o %s (perfil %s).", L"%d save(s) imported into %s (profile %s)."), f.n, G[g].name, CurProf()->name);
}

/* ---------------------------------------------------------------- cheats
   games\UFCn\cheats\<pack>.json: {"cheats": [{"id", "name_pt", "name_en", "desc_pt", "desc_en", ...}]}. PORTHEON only
   reads the ids and texts; the code is the port's (scripts\cheats.py). The chosen ids go to cheats.json in the game's
   data folder before PLAY. */
#define MAXCHEATS 40
typedef struct { char id[48]; wchar_t name[96], desc[400]; BOOL on; } CheatInfo;
static CheatInfo g_cheat[MAXCHEATS]; static int g_ncheat = -1, g_cheatGame = -1;
static BOOL JsonField(const char *obj, const char *end, const char *key, char *out, int n)   /* "key": "value" inside obj..end */
{
    char k[64]; _snprintf(k, 64, "\"%s\"", key); k[63] = 0;
    const char *p = obj;
    while ((p = strstr(p, k)) && p < end) {
        p += strlen(k); while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p != ':') continue; p++; while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p != '"') return FALSE; p++;
        int i = 0; while (*p && *p != '"' && p < end && i < n - 1) { if (*p == '\\' && p[1]) { p++; out[i++] = *p == 'n' ? '\n' : *p; p++; continue; } out[i++] = *p++; }
        out[i] = 0; return TRUE;
    }
    return FALSE;
}
static void CheatStatePath(int g, wchar_t *out) { wchar_t d[MAX_PATH]; LocalData(d, G[g].data); Join(out, d, L"cheats.json"); }
static void CheatsLoad(int g)
{
    if (g_cheatGame == g && g_ncheat >= 0) return;
    g_ncheat = 0; g_cheatGame = g;
    wchar_t d[MAX_PATH], pat[MAX_PATH]; PortDir(g, d); Join(pat, d, L"cheats\\*.json");
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    char *state = NULL; { wchar_t sp[MAX_PATH]; CheatStatePath(g, sp); DWORD n = 0; BYTE *b = ReadWhole(sp, &n); if (b) { state = (char *)realloc(b, n + 1); if (state) state[n] = 0; } }
    do {
        wchar_t f[MAX_PATH]; _snwprintf(f, MAX_PATH, L"%s\\cheats\\%s", d, fd.cFileName); f[MAX_PATH - 1] = 0;
        DWORD n = 0; BYTE *b = ReadWhole(f, &n); if (!b) continue;
        char *s = (char *)realloc(b, n + 1); if (!s) { free(b); continue; } s[n] = 0;
        /* each cheat is an object of the "cheats" array, "id" first; the pack may be written with any spacing */
        const char *p = strstr(s, "\"cheats\"");
        while (p && (p = strstr(p, "\"id\"")) && g_ncheat < MAXCHEATS) {
            const char *e = strstr(p + 4, "\"id\""); if (!e) e = s + n;
            CheatInfo *c = &g_cheat[g_ncheat]; char t[800];
            if (JsonField(p, e, "id", c->id, 48)) {
                const char *nk = g_lang == LANG_PT ? "name_pt" : "name_en", *dk = g_lang == LANG_PT ? "desc_pt" : "desc_en";
                if (JsonField(p, e, nk, t, 800) || JsonField(p, e, "name_en", t, 800)) MultiByteToWideChar(CP_UTF8, 0, t, -1, c->name, 96); else MultiByteToWideChar(CP_UTF8, 0, c->id, -1, c->name, 96);
                if (JsonField(p, e, dk, t, 800) || JsonField(p, e, "desc_en", t, 800)) MultiByteToWideChar(CP_UTF8, 0, t, -1, c->desc, 400); else c->desc[0] = 0;
                c->name[95] = c->desc[399] = 0;
                if (g_lang > LANG_EN) {   /* 4.0: the other languages come from lang_data.inc, keyed by the English text */
                    const wchar_t *tn = TR(c->name, c->name), *td = TR(c->desc, c->desc);
                    if (tn != c->name) wcsncpy(c->name, tn, 95); if (td != c->desc) wcsncpy(c->desc, td, 399); c->name[95] = c->desc[399] = 0; }
                char q[64]; _snprintf(q, 64, "\"%s\"", c->id); q[63] = 0;
                c->on = state && strstr(state, q) != NULL;
                if (g == 1 && !strcmp(c->id, "ufc2_camera_modes")) c->on = GetInt(g, L"pgame:camfp", 0) != 0;   /* 6.2: the Gameplay switch owns this one */
                g_ncheat++;
            }
            p = e;
        }
        free(s);
    } while (FindNextFileW(h, &fd));
    FindClose(h); free(state);
}
static void CheatsSave(int g)
{
    char buf[4096]; int n = snprintf(buf, sizeof(buf), "{\n  \"enabled\": [");
    BOOL first = TRUE;
    for (int i = 0; i < g_ncheat && n > 0 && n < (int)sizeof(buf) - 80; i++) if (g_cheat[i].on) { n += snprintf(buf + n, sizeof(buf) - n, "%s\"%s\"", first ? "" : ", ", g_cheat[i].id); first = FALSE; }
    if (n > 0 && n < (int)sizeof(buf) - 8) n += snprintf(buf + n, sizeof(buf) - n, "]\n}\n");
    wchar_t p[MAX_PATH], d[MAX_PATH]; CheatStatePath(g, p); LocalData(d, G[g].data); CreateDirectoryW(d, NULL);
    WriteIfDifferent(p, buf);
}
static int CheatsOn(int g) { CheatsLoad(g); int k = 0; for (int i = 0; i < g_ncheat; i++) k += g_cheat[i].on; return k; }
static void CheatToggle(int g, int i)
{
    CheatsLoad(g); if (i < 0 || i >= g_ncheat) return;
    g_cheat[i].on = !g_cheat[i].on; CheatsSave(g);
    Msg(g_cheat[i].on ? C_GOOD : C_SUB, g_cheat[i].on ? L2(L"Trapaça ligada: %s (vale no próximo JOGAR).", L"Cheat on: %s (from the next PLAY).") : L2(L"Trapaça desligada: %s.", L"Cheat off: %s."), g_cheat[i].name);
}
static void CheatsAllOff(int g) { CheatsLoad(g); for (int i = 0; i < g_ncheat; i++) g_cheat[i].on = FALSE; CheatsSave(g); Msg(C_SUB, L"%s", L2(L"Todas as trapaças desligadas.", L"Every cheat off.")); }
/* before PLAY: cheats.json says what is on (an empty list when the player turned them all off) */
static void CheatsPrepare(int g) { g_ncheat = -1; CheatsLoad(g); CheatsSave(g); }
