/* video.c - PORTHEON 6.2: the game trailers / backgrounds player (Windows Media Foundation Source Reader).
 *
 * Self-contained: include it in the unity build (portheon.c) or compile it alone. One decode thread per player
 * reads the first video stream of a .mp4/.mov/.wmv with the Source Reader, has MF convert it to RGB32
 * (MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING: the D3D-free video processor), and paces the frames by their
 * own timestamps (the file's frame rate, variable rates too). Audio is never selected: PORTHEON plays the game's
 * music itself. Frames go through a triple buffer, so the UI thread never waits for the decoder and the decoder never
 * copies twice. When a new frame is ready the thread posts VIDEO_MSG (lParam = the VPlayer) to the window given to
 * VideoSetWindow, so the UI can repaint only the video rect. Paused, or with that window minimized, the thread sleeps
 * and reads nothing (no CPU).
 *
 * Link: -lmfplat -lmfreadwrite -lmfuuid -lole32
 */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <d3d11.h>
#include <shlobj.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

#ifndef VIDEO_MSG
#define VIDEO_MSG (WM_APP + 0x70)   /* a new video frame is ready: lParam = the VPlayer */
#endif

typedef struct VPlayer VPlayer;
VPlayer *VideoOpen(const wchar_t *path, BOOL loop);   /* NULL on failure */
/* 6.2 review: cancel (may be NULL) is polled while the file opens; when *cancel != gen the open stops early and NULL comes back */
VPlayer *VideoOpenEx(const wchar_t *path, BOOL loop, volatile LONG *cancel, LONG gen);
BOOL VideoFrame(VPlayer *v, const BYTE **bgra, int *w, int *h, int *stride, UINT *serial);   /* latest frame, valid until the next call */
/* 6.2 review: the same, with the player locked until VideoFrameUnlock: the decoder cannot free or resize the buffers
   while the UI reads them (a size change mid-file reallocates them) */
BOOL VideoFrameLock(VPlayer *v, const BYTE **bgra, int *w, int *h, int *stride, UINT *serial);
void VideoFrameUnlock(VPlayer *v);
void VideoPause(VPlayer *v, BOOL pause);
void VideoClose(VPlayer *v);
void VideoSetWindow(HWND hwnd);
int VideoFind(int game, const wchar_t *gameId, wchar_t *out, int n);

struct VPlayer {
    wchar_t path[MAX_PATH];
    BOOL loop;
    HANDLE thread, wake;              /* wake: pause/unpause/close */
    CRITICAL_SECTION cs;
    volatile LONG quit, paused;
    BYTE *buf[3];                     /* triple buffer: front = the UI's, ready = newest decoded, back = being filled */
    int front, ready, back;
    BOOL fresh;                       /* ready holds a frame the UI has not taken yet */
    BOOL any;                         /* at least one frame decoded */
    int w, h, stride;                 /* stride = w * 4, top-down BGRA, alpha = 255: the picture shown (the display aperture) */
    int cx, cy, fh;                   /* 6.2 review: the aperture's offset in the decoded frame, and the decoded height (1080p
                                         often decodes as 1920x1088: the 8 rows under the aperture are padding, often green) */
    volatile LONG *cancel; LONG gen;  /* VideoOpenEx */
    UINT serial;
    HANDLE started;                   /* set once the thread knows whether the file opened */
    BOOL openOk;
    BOOL hw;                          /* decoding on the GPU (DXVA through D3D11) */
};

static HWND g_vidWnd;
static BOOL g_vidUnpaced;   /* tests only: decode as fast as possible (measures the decoder) */

void VideoSetWindow(HWND hwnd) { g_vidWnd = hwnd; }

static LONGLONG VidNow100ns(void)
{
    static LARGE_INTEGER f; LARGE_INTEGER c;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (LONGLONG)((double)c.QuadPart * 10000000.0 / (double)f.QuadPart);
}

static void VidRelease(void *p) { if (p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p); }

/* GPU decode (DXVA): a D3D11 device with video support behind an MF DXGI device manager. d3d11.dll is loaded at run
   time, so PORTHEON needs no -ld3d11 and a PC without it simply decodes on the CPU. */
typedef HRESULT (WINAPI *VidD3D11CreateDevice_t)(void *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
static const GUID VID_IID_ID3D10Multithread = { 0x9b7e4e00, 0x342c, 0x4106, { 0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0 } };
static IMFDXGIDeviceManager *VidMakeDxgi(ID3D11Device **devOut)
{
    static HMODULE d3d; static VidD3D11CreateDevice_t create;
    static const D3D_FEATURE_LEVEL fl[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3 };
    ID3D11Device *dev = NULL; IMFDXGIDeviceManager *mgr = NULL; ID3D10Multithread *mtp = NULL; UINT tok = 0; HRESULT hr;
    wchar_t ev[8];
    *devOut = NULL;
    if (GetEnvironmentVariableW(L"PORTHEON_VIDEO_SW", ev, 8) && ev[0] == L'1') return NULL;   /* test switch: CPU only */
    if (!d3d && (d3d = LoadLibraryW(L"d3d11.dll"))) create = (VidD3D11CreateDevice_t)(void *)GetProcAddress(d3d, "D3D11CreateDevice");
    if (!create) return NULL;
    hr = create(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                fl, sizeof fl / sizeof fl[0], D3D11_SDK_VERSION, &dev, NULL, NULL);
    if (hr == E_INVALIDARG) hr = create(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                        fl + 1, sizeof fl / sizeof fl[0] - 1, D3D11_SDK_VERSION, &dev, NULL, NULL);   /* Win 7-era runtimes: no 11_1 */
    if (FAILED(hr) || !dev) return NULL;
    if (SUCCEEDED(dev->lpVtbl->QueryInterface(dev, &VID_IID_ID3D10Multithread, (void **)&mtp)) && mtp) {   /* MF uses it from its own threads */
        mtp->lpVtbl->SetMultithreadProtected(mtp, TRUE); VidRelease(mtp);
    }
    if (FAILED(MFCreateDXGIDeviceManager(&tok, &mgr)) || !mgr) { VidRelease(dev); return NULL; }
    if (FAILED(mgr->lpVtbl->ResetDevice(mgr, (IUnknown *)dev, tok))) { VidRelease(mgr); VidRelease(dev); return NULL; }
    *devOut = dev;
    return mgr;
}

/* Opens the reader on the first video stream as RGB32 (on the GPU when mgr is given); returns the frame size. */
/* 6.2 review: the part of the decoded frame meant to be seen (MF_MT_MINIMUM_DISPLAY_APERTURE, else the geometric aperture);
   FALSE when the type says nothing (the whole frame) */
static BOOL VidAperture(IMFMediaType *t, int fw, int fh, int *ax, int *ay, int *aw, int *ah)
{
    MFVideoArea a; UINT32 n = 0;
    if (!t) return FALSE;
    if (FAILED(t->lpVtbl->GetBlob(t, &MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8 *)&a, sizeof a, &n)) || n != sizeof a)
        if (FAILED(t->lpVtbl->GetBlob(t, &MF_MT_GEOMETRIC_APERTURE, (UINT8 *)&a, sizeof a, &n)) || n != sizeof a) return FALSE;
    *ax = a.OffsetX.value; *ay = a.OffsetY.value; *aw = (int)a.Area.cx; *ah = (int)a.Area.cy;
    if (*ax < 0 || *ay < 0 || *aw < 16 || *ah < 16 || *ax + *aw > fw || *ay + *ah > fh) return FALSE;
    return TRUE;
}
static void VidSetSize(VPlayer *v, IMFSourceReader *r, IMFMediaType *cur, int fw, int fh)   /* v->w/h/cx/cy/fh from the frame and its aperture */
{
    int ax = 0, ay = 0, aw = fw, ah = fh; IMFMediaType *nat = NULL;
    if (!VidAperture(cur, fw, fh, &ax, &ay, &aw, &ah)) {   /* the RGB32 output type may drop it: the file's own type has it */
        ax = ay = 0; aw = fw; ah = fh;
        if (r && SUCCEEDED(r->lpVtbl->GetNativeMediaType(r, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &nat)) && nat) {
            UINT64 ns = 0;
            if (!VidAperture(nat, fw, fh, &ax, &ay, &aw, &ah)) {
                ax = ay = 0; aw = fw; ah = fh;
                if (SUCCEEDED(nat->lpVtbl->GetUINT64(nat, &MF_MT_FRAME_SIZE, &ns)) && (int)(ns >> 32) == fw && (int)(ns & 0xffffffff) >= 16 && (int)(ns & 0xffffffff) < fh)
                    ah = (int)(ns & 0xffffffff);   /* the file says 1080, the decoder hands 1088 */
            }
            nat->lpVtbl->Release(nat);
        }
    }
    v->cx = ax; v->cy = ay; v->w = aw; v->h = ah; v->fh = fh; v->stride = aw * 4;
}
static IMFSourceReader *VidOpenReader(const wchar_t *path, IMFDXGIDeviceManager *mgr, int *w, int *h, LONG *defStride)
{
    IMFAttributes *at = NULL; IMFSourceReader *r = NULL; IMFMediaType *mt = NULL, *cur = NULL;
    HRESULT hr = MFCreateAttributes(&at, 4);
    if (SUCCEEDED(hr)) {
        at->lpVtbl->SetUINT32(at, &MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
        at->lpVtbl->SetUINT32(at, &MF_READWRITE_DISABLE_CONVERTERS, FALSE);
        if (mgr) {
            at->lpVtbl->SetUINT32(at, &MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
            at->lpVtbl->SetUnknown(at, &MF_SOURCE_READER_D3D_MANAGER, (IUnknown *)mgr);
        }
        hr = MFCreateSourceReaderFromURL(path, at, &r);
    }
    VidRelease(at);
    if (FAILED(hr) || !r) return NULL;
    r->lpVtbl->SetStreamSelection(r, (DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);   /* no audio: muted, less work */
    hr = r->lpVtbl->SetStreamSelection(r, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
    if (SUCCEEDED(hr)) hr = MFCreateMediaType(&mt);
    if (SUCCEEDED(hr)) {
        mt->lpVtbl->SetGUID(mt, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
        mt->lpVtbl->SetGUID(mt, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
        hr = r->lpVtbl->SetCurrentMediaType(r, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, mt);
    }
    VidRelease(mt);
    if (SUCCEEDED(hr)) hr = r->lpVtbl->GetCurrentMediaType(r, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &cur);
    if (SUCCEEDED(hr)) {
        UINT64 fs = 0; UINT32 st = 0;
        hr = cur->lpVtbl->GetUINT64(cur, &MF_MT_FRAME_SIZE, &fs);
        *w = (int)(fs >> 32); *h = (int)(fs & 0xffffffff);
        *defStride = SUCCEEDED(cur->lpVtbl->GetUINT32(cur, &MF_MT_DEFAULT_STRIDE, &st)) ? (LONG)st : (LONG)(*w * 4);
        if (SUCCEEDED(hr) && (*w <= 0 || *h <= 0 || *w > 8192 || *h > 8192)) hr = E_FAIL;
    }
    VidRelease(cur);
    if (FAILED(hr)) { VidRelease(r); return NULL; }
    return r;
}

/* Copies one decoded sample into v->buf[v->back] (top-down, alpha 255). */
static BOOL VidCopySample(VPlayer *v, IMFSample *s, LONG defStride)
{
    IMFMediaBuffer *mb = NULL; IMF2DBuffer *b2 = NULL; BYTE *src = NULL; LONG pitch = 0; BOOL ok = FALSE, two = FALSE;
    DWORD maxLen = 0, len = 0;
    if (FAILED(s->lpVtbl->ConvertToContiguousBuffer(s, &mb)) || !mb) return FALSE;
    if (SUCCEEDED(mb->lpVtbl->QueryInterface(mb, &IID_IMF2DBuffer, (void **)&b2)) && b2 &&
        SUCCEEDED(b2->lpVtbl->Lock2D(b2, &src, &pitch))) two = TRUE;
    else if (SUCCEEDED(mb->lpVtbl->Lock(mb, &src, &maxLen, &len))) {
        pitch = defStride;
        if (pitch < 0) src += (size_t)(-pitch) * (v->fh - 1);   /* bottom-up: scan0 is the last row in memory */
        if ((DWORD)(abs(pitch) * v->fh) > len) { mb->lpVtbl->Unlock(mb); src = NULL; }
    } else src = NULL;
    if (src) {
        BYTE *dst = v->buf[v->back]; int y, x;
        for (y = 0; y < v->h; y++) {
            const UINT32 *sr = (const UINT32 *)(src + (ptrdiff_t)pitch * (y + v->cy)) + v->cx; UINT32 *dr = (UINT32 *)(dst + (size_t)v->stride * y);
            for (x = 0; x < v->w; x++) dr[x] = sr[x] | 0xFF000000u;   /* RGB32's X byte is undefined: make it opaque */
        }
        ok = TRUE;
        if (two) b2->lpVtbl->Unlock2D(b2); else mb->lpVtbl->Unlock(mb);
    }
    VidRelease(b2); VidRelease(mb);
    return ok;
}

static BOOL VidCancelled(VPlayer *v) { return v->cancel && *v->cancel != v->gen; }
static BOOL VidShouldSleep(VPlayer *v) { return v->paused || (g_vidWnd && IsIconic(g_vidWnd)); }

static DWORD WINAPI VidThread(LPVOID p)
{
    VPlayer *v = (VPlayer *)p; IMFSourceReader *r = NULL; LONG defStride = 0; int w = 0, h = 0, i;
    BOOL com = SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED)), mf = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE));
    LONGLONG base = 0, firstTs = -1;   /* wall clock (100 ns) at which timestamp firstTs is due */
    ID3D11Device *dev = NULL; IMFDXGIDeviceManager *mgr = NULL;
    if (mf && !VidCancelled(v) && (mgr = VidMakeDxgi(&dev)) && (VidCancelled(v) || !(r = VidOpenReader(v->path, mgr, &w, &h, &defStride)))) {
        VidRelease(mgr); VidRelease(dev); mgr = NULL; dev = NULL;   /* no DXVA for this file/GPU: decode on the CPU */
    }
    if (mf && !r && !VidCancelled(v)) r = VidOpenReader(v->path, NULL, &w, &h, &defStride);
    v->hw = mgr != NULL;
    if (r && VidCancelled(v)) { VidRelease(r); r = NULL; }
    if (r) {
        IMFMediaType *cur = NULL; r->lpVtbl->GetCurrentMediaType(r, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &cur);
        VidSetSize(v, r, cur, w, h); VidRelease(cur);
        for (i = 0; i < 3; i++) if (!(v->buf[i] = (BYTE *)malloc((size_t)v->stride * v->h))) { VidRelease(r); r = NULL; break; }
    }
    v->openOk = r != NULL; SetEvent(v->started);
    if (!r) goto done;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    {   int passFrames = 0, emptyPasses = 0;   /* 6.2 review: a pass of the file that gave no frame (a broken file) must not spin */
    while (!v->quit) {
        DWORD si = 0, flags = 0; LONGLONG ts = 0; IMFSample *s = NULL; HRESULT hr;
        if (VidShouldSleep(v)) {   /* paused or hidden: read nothing; restart the clock on wake */
            LONGLONG t0 = VidNow100ns();
            while (!v->quit && VidShouldSleep(v)) WaitForSingleObject(v->wake, v->paused ? INFINITE : 250);
            base += VidNow100ns() - t0;
            continue;
        }
        hr = r->lpVtbl->ReadSample(r, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &si, &flags, &ts, &s);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR)) { VidRelease(s); break; }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            VidRelease(s);
            if (!v->loop) break;
            if (!passFrames && ++emptyPasses >= 2) break;   /* twice through without one frame: give up (the UI keeps its edit) */
            if (passFrames) emptyPasses = 0;
            passFrames = 0;
            { PROPVARIANT pv; memset(&pv, 0, sizeof pv); pv.vt = VT_I8; pv.hVal.QuadPart = 0;
              GUID tf; memset(&tf, 0, sizeof tf);   /* GUID_NULL = 100 ns units (no -luuid needed) */
              if (FAILED(r->lpVtbl->SetCurrentPosition(r, &tf, &pv))) break; }
            firstTs = -1;   /* timestamps start again: re-anchor the clock */
            continue;
        }
        if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {   /* size changed mid-file (rare): reallocate */
            IMFMediaType *cur = NULL; UINT64 fs = 0;
            if (SUCCEEDED(r->lpVtbl->GetCurrentMediaType(r, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &cur)) &&
                SUCCEEDED(cur->lpVtbl->GetUINT64(cur, &MF_MT_FRAME_SIZE, &fs))) {
                int nw = (int)(fs >> 32), nh = (int)(fs & 0xffffffff); UINT32 st = 0;
                defStride = SUCCEEDED(cur->lpVtbl->GetUINT32(cur, &MF_MT_DEFAULT_STRIDE, &st)) ? (LONG)st : (LONG)(nw * 4);
                if (nw > 0 && nh > 0 && nw <= 8192 && nh <= 8192) {
                    VPlayer t; memset(&t, 0, sizeof t); VidSetSize(&t, r, cur, nw, nh);   /* the new size and aperture, applied under the lock */
                    if (t.w != v->w || t.h != v->h || t.cx != v->cx || t.cy != v->cy || t.fh != v->fh) {
                        BYTE *nb[3]; BOOL okb = TRUE;
                        for (i = 0; i < 3; i++) if (!(nb[i] = (BYTE *)malloc((size_t)t.stride * t.h))) okb = FALSE;
                        if (okb) {   /* the UI reads the front buffer only with the lock held (VideoFrameLock): freeing here is safe */
                            EnterCriticalSection(&v->cs);
                            for (i = 0; i < 3; i++) { free(v->buf[i]); v->buf[i] = nb[i]; }
                            v->w = t.w; v->h = t.h; v->cx = t.cx; v->cy = t.cy; v->fh = t.fh; v->stride = t.stride; v->fresh = FALSE; v->any = FALSE;
                            LeaveCriticalSection(&v->cs);
                        } else for (i = 0; i < 3; i++) free(nb[i]);
                    }
                }
            }
            VidRelease(cur);
        }
        if (!s) continue;   /* a gap or a stream tick */
        {   /* pace by the timestamp; a decoder that falls >150 ms behind re-anchors instead of racing */
            LONGLONG now = VidNow100ns(), due;
            if (firstTs < 0) { firstTs = ts; base = now; }
            due = base + (ts - firstTs);
            if (due - now > 20000000) { base = now - (ts - firstTs); due = now; }   /* a jump forward: don't stall */
            while (!g_vidUnpaced && !v->quit && !VidShouldSleep(v) && due > now) {
                WaitForSingleObject(v->wake, (DWORD)((due - now + 9999) / 10000));
                now = VidNow100ns();
            }
            if (now - due > 1500000) base += now - due;
        }
        if (!v->quit && VidCopySample(v, s, defStride)) {
            int t; passFrames++;
            EnterCriticalSection(&v->cs);
            t = v->ready; v->ready = v->back; v->back = t; v->fresh = TRUE; v->any = TRUE; v->serial++;
            LeaveCriticalSection(&v->cs);
            if (g_vidWnd) PostMessageW(g_vidWnd, VIDEO_MSG, 0, (LPARAM)v);
        }
        VidRelease(s);
    }
    }
    VidRelease(r);
done:
    VidRelease(mgr); VidRelease(dev);
    if (mf) MFShutdown();
    if (com) CoUninitialize();
    return 0;
}

VPlayer *VideoOpen(const wchar_t *path, BOOL loop) { return VideoOpenEx(path, loop, NULL, 0); }
VPlayer *VideoOpenEx(const wchar_t *path, BOOL loop, volatile LONG *cancel, LONG gen)
{
    VPlayer *v; DWORD a;
    if (!path || !*path || (a = GetFileAttributesW(path)) == INVALID_FILE_ATTRIBUTES || (a & FILE_ATTRIBUTE_DIRECTORY)) return NULL;
    if (!(v = (VPlayer *)calloc(1, sizeof *v))) return NULL;
    wcsncpy(v->path, path, MAX_PATH - 1);
    v->loop = loop; v->front = 0; v->ready = 1; v->back = 2; v->cancel = cancel; v->gen = gen;
    InitializeCriticalSection(&v->cs);
    v->wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    v->started = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (v->wake && v->started) v->thread = CreateThread(NULL, 0, VidThread, v, 0, NULL);
    if (v->thread) { int k; for (k = 0; k < 200 && WaitForSingleObject(v->started, 50) == WAIT_TIMEOUT && !(cancel && *cancel != gen); k++) ; }
    if (!v->thread || !v->openOk || (cancel && *cancel != gen)) { if (v->thread) { v->quit = 1; SetEvent(v->wake); } VideoClose(v); return NULL; }
    return v;
}

BOOL VideoFrame(VPlayer *v, const BYTE **bgra, int *w, int *h, int *stride, UINT *serial)
{
    BOOL ok = VideoFrameLock(v, bgra, w, h, stride, serial);
    if (v) VideoFrameUnlock(v);
    return ok;
}
void VideoFrameUnlock(VPlayer *v) { if (v) LeaveCriticalSection(&v->cs); }
BOOL VideoFrameLock(VPlayer *v, const BYTE **bgra, int *w, int *h, int *stride, UINT *serial)   /* always locks (when v): pair with VideoFrameUnlock */
{
    BOOL ok;
    if (!v) return FALSE;
    EnterCriticalSection(&v->cs);
    if (v->fresh) { int t = v->front; v->front = v->ready; v->ready = t; v->fresh = FALSE; }
    ok = v->any;
    if (ok) {
        if (bgra) *bgra = v->buf[v->front];
        if (w) *w = v->w;
        if (h) *h = v->h;
        if (stride) *stride = v->stride;
        if (serial) *serial = v->serial;
    }
    return ok;
}

void VideoPause(VPlayer *v, BOOL pause)
{
    if (!v) return;
    InterlockedExchange(&v->paused, pause ? 1 : 0);
    SetEvent(v->wake);
}

static void VidFree(VPlayer *v)
{
    int i;
    if (v->thread) { WaitForSingleObject(v->thread, INFINITE); CloseHandle(v->thread); }
    if (v->wake) CloseHandle(v->wake);
    if (v->started) CloseHandle(v->started);
    DeleteCriticalSection(&v->cs);
    for (i = 0; i < 3; i++) free(v->buf[i]);
    free(v);
}
static DWORD WINAPI VidReaper(LPVOID p) { VidFree((VPlayer *)p); return 0; }
/* 6.2 review: the UI thread never waits for the decoder (a ReadSample or a device teardown can take a while): the thread
   is told to stop and a small thread waits for it and frees the player */
void VideoClose(VPlayer *v)
{
    HANDLE t;
    if (!v) return;
    InterlockedExchange(&v->quit, 1);
    if (v->wake) SetEvent(v->wake);
    if (!v->thread || !(t = CreateThread(NULL, 0, VidReaper, v, 0, NULL))) { VidFree(v); return; }
    CloseHandle(t);
}

/* The first .mp4/.mov/.wmv (in that order) in dir; 1 if found. */
static int VidFindIn(const wchar_t *dir, wchar_t *out, int n)
{
    static const wchar_t *ext[] = { L"mp4", L"mov", L"wmv" };
    int e;
    for (e = 0; e < 3; e++) {
        wchar_t pat[MAX_PATH]; WIN32_FIND_DATAW fd; HANDLE f;
        if (_snwprintf(pat, MAX_PATH, L"%s\\*.%s", dir, ext[e]) < 0) continue;
        pat[MAX_PATH - 1] = 0;
        if ((f = FindFirstFileW(pat, &fd)) == INVALID_HANDLE_VALUE) continue;
        do {
            const wchar_t *dot = wcsrchr(fd.cFileName, L'.');
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (!dot || _wcsicmp(dot + 1, ext[e])) continue;   /* *.mov also matches *.movie on 8.3 names */
            if (_snwprintf(out, n, L"%s\\%s", dir, fd.cFileName) < 0) continue;
            out[n - 1] = 0; FindClose(f); return 1;
        } while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    return 0;
}

int VideoFind(int game, const wchar_t *gameId, wchar_t *out, int n)
{
    wchar_t base[MAX_PATH], dir[MAX_PATH], *sl;
    (void)game;
    if (!gameId || !*gameId || !out || n <= 0) return 0;
    out[0] = 0;
    if (GetModuleFileNameW(NULL, base, MAX_PATH) && (sl = wcsrchr(base, L'\\'))) {
        *sl = 0;
        _snwprintf(dir, MAX_PATH, L"%s\\videos\\%s", base, gameId); dir[MAX_PATH - 1] = 0;
        if (VidFindIn(dir, out, n)) return 1;
    }
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, base))) {
        _snwprintf(dir, MAX_PATH, L"%s\\PORTHEON\\videos\\%s", base, gameId); dir[MAX_PATH - 1] = 0;
        if (VidFindIn(dir, out, n)) return 1;
    }
    out[0] = 0;
    return 0;
}
