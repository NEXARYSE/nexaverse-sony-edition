/* music.c - part of portheon.c (6.0). The user (2026-11-07): "put each game's music when you choose the game inside
 * PORTHEON, before playing". As on a console's home screen: the game that stays chosen for a moment plays its own theme,
 * the sce_sys\snd0.at9 of the player's own copy (PS4 and PS5 games carry one; Zelda and UFC Undisputed 3 have none).
 * The file is ATRAC9 in a RIFF/WAVE box: LibAtrac9 (MIT, src\atrac9, its license in licenses\LibAtrac9-LICENSE.txt)
 * decodes it to 16-bit PCM once, and one thread plays it through waveOut: a fade in after a short pause on the game,
 * the file's own loop, a fade out when the choice changes, when PLAY is pressed, in the settings and when PORTHEON
 * goes behind another window. A copy of the file is kept beside the covers (%LOCALAPPDATA%\PORTHEON\art\<id>_snd0.at9),
 * so the music still plays when the game's drive is out. Nothing of it runs while a game runs: PLAY stops it, and the
 * light --play instance that waits for the game never starts the thread. Nothing of the games ships with PORTHEON.
 * portheon.ini [PORTHEON]: game_music (1 = on) and music_volume (0-100, 55). */
#include "atrac9/libatrac9.h"

typedef struct { short *pcm; DWORD frames, loopStart, loopEnd; int rate; float norm; } Tune;   /* always stereo; norm: the file's peak brought to 0.6 */

/* the ATRAC9 file: "fmt " (WAVE_FORMAT_EXTENSIBLE, the ATRAC9 GUID, the 4 config bytes at +44), "fact" (samples, then
   the samples to skip at the start), "smpl" (the loop) and "data" (superframes) */
static BOOL TuneDecode(const BYTE *b, DWORD n, Tune *t)
{
    memset(t, 0, sizeof(*t));
    if (n < 12 || memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4)) return FALSE;
    const BYTE *fmt = NULL, *data = NULL; DWORD fmtN = 0, dataN = 0, samples = 0, skip = 0, ls = 0, le = 0; BOOL loop = FALSE;
    for (DWORD o = 12; o + 8 <= n; ) {
        DWORD sz; memcpy(&sz, b + o + 4, 4); const BYTE *c = b + o + 8; if (sz > n - o - 8) sz = n - o - 8;
        if (!memcmp(b + o, "fmt ", 4)) { fmt = c; fmtN = sz; }
        else if (!memcmp(b + o, "data", 4)) { data = c; dataN = sz; }
        else if (!memcmp(b + o, "fact", 4) && sz >= 8) { memcpy(&samples, c, 4); memcpy(&skip, c + 4, 4); }
        else if (!memcmp(b + o, "smpl", 4) && sz >= 60) { DWORD cnt; memcpy(&cnt, c + 28, 4); if (cnt) { memcpy(&ls, c + 44, 4); memcpy(&le, c + 48, 4); loop = TRUE; } }
        if (sz > 0xFFFFFFF0u - o) break;
        o += 8 + sz + (sz & 1);
    }
    static const BYTE at9[16] = {0xD2, 0x42, 0xE1, 0x47, 0xBA, 0x36, 0x8D, 0x4D, 0x88, 0xFC, 0x61, 0x65, 0x4F, 0x8C, 0x83, 0x6C};
    WORD tag = 0; if (fmt && fmtN >= 48) memcpy(&tag, fmt, 2);
    if (!fmt || fmtN < 48 || tag != 0xFFFE || memcmp(fmt + 24, at9, 16) || !data || !dataN) return FALSE;
    void *h = Atrac9GetHandle(); if (!h) return FALSE;
    unsigned char cfg[4]; memcpy(cfg, fmt + 44, 4);
    Atrac9CodecInfo ci; BOOL ok = FALSE; short *pcm = NULL, *tmp = NULL; BYTE *sf = NULL; DWORD at = 0;
    if (Atrac9InitDecoder(h, cfg) == 0 && Atrac9GetCodecInfo(h, &ci) == 0 && ci.channels >= 1 && ci.channels <= 8
        && ci.superframeSize > 0 && ci.superframeSize <= 0x10000 && ci.frameSamples > 0 && ci.frameSamples <= 4096
        && ci.framesInSuperframe > 0 && ci.framesInSuperframe <= 16 && ci.samplingRate >= 8000 && ci.samplingRate <= 96000) {
        DWORD supers = dataN / (DWORD)ci.superframeSize, total = supers * (DWORD)(ci.framesInSuperframe * ci.frameSamples);
        if (total && total <= (DWORD)ci.samplingRate * 600) {   /* 10 minutes at most */
            tmp = (short *)malloc((size_t)ci.frameSamples * ci.channels * sizeof(short));
            pcm = (short *)malloc((size_t)total * 2 * sizeof(short));
            sf = (BYTE *)calloc(1, (size_t)ci.superframeSize + 512);   /* each superframe copied with zero padding: the decoder takes no length */
            if (tmp && pcm && sf) {
                for (DWORD s = 0; s < supers; s++) {
                    memcpy(sf, data + (size_t)s * ci.superframeSize, ci.superframeSize);
                    const BYTE *p = sf; BOOL bad = FALSE;
                    for (int f = 0; f < ci.framesInSuperframe; f++) {
                        int used = 0;
                        if (Atrac9Decode(h, p, tmp, &used, 0) != 0 || used <= 0 || p + used > sf + ci.superframeSize + 256) { bad = TRUE; break; }
                        p += used;
                        for (int i = 0; i < ci.frameSamples; i++) {
                            pcm[(size_t)(at + i) * 2] = tmp[(size_t)i * ci.channels];
                            pcm[(size_t)(at + i) * 2 + 1] = tmp[(size_t)i * ci.channels + (ci.channels > 1 ? 1 : 0)];
                        }
                        at += ci.frameSamples;
                    }
                    if (bad) break;
                }
                ok = at > (DWORD)ci.samplingRate / 2;   /* half a second at least */
            }
        }
    }
    Atrac9ReleaseHandle(h); free(tmp); free(sf);
    if (!ok) { free(pcm); return FALSE; }
    if (skip >= at / 2) skip = 0;
    DWORD len = at - skip; if (samples && samples < len) len = samples;
    if (skip) memmove(pcm, pcm + (size_t)skip * 2, (size_t)len * 2 * sizeof(short));
    t->pcm = pcm; t->frames = len; t->rate = ci.samplingRate; t->loopStart = 0; t->loopEnd = len;
    {   /* the console's previews are mastered quiet (UFC 6 peaks at 0.16, UFC 5 at 0.26): every game at one level */
        int peak = 1; for (size_t i = 0; i < (size_t)len * 2; i++) { int v = pcm[i] < 0 ? -pcm[i] : pcm[i]; if (v > peak) peak = v; }
        t->norm = 0.6f * 32767.0f / peak; if (t->norm > 4.0f) t->norm = 4.0f; if (t->norm < 1.0f) t->norm = 1.0f;
    }
    if (loop && le > ls) {
        DWORD a = ls > skip ? ls - skip : 0, z = le > skip ? le - skip : 0; if (z > len) z = len;
        if (z > a + (DWORD)ci.samplingRate) { t->loopStart = a; t->loopEnd = z; }   /* a loop of one second or more */
    }
    return TRUE;
}
static void TuneFree(Tune *t) { free(t->pcm); memset(t, 0, sizeof(*t)); }

/* 6.1: a custom game's music is want = MUS_CUSTOM + its id: the snd0.at9 library.c copied from the player's own PS4
   dump beside the emulator's game (LibConsoleArt), %LOCALAPPDATA%\PORTHEON\art\c<id>_snd0.at9. Steam, Epic and other
   programs have none unless that file is there. */
#define MUS_CUSTOM 1000
static void MusicSrc(int g, wchar_t *out)
{
    out[0] = 0;
    if (g >= MUS_CUSTOM) { wchar_t a[MAX_PATH]; LibArtPath(g - MUS_CUSTOM, L"snd0", L"at9", a); if (FileExists(a)) wcscpy(out, a); return; }
    wchar_t c[MAX_PATH], f[64], d[MAX_PATH], p[MAX_PATH];
    LocalData(c, L"PORTHEON"); Join(c, c, L"art"); CreateDirectoryW(c, NULL); _snwprintf(f, 64, L"%s_snd0.at9", G[g].id); f[63] = 0; Join(c, c, f);
    GameDir(g, d);
    if (d[0] && DirExists(d)) {
        Join(p, d, L"sce_sys\\snd0.at9");
        if (!FileExists(p)) {
            p[0] = 0; wchar_t pat[MAX_PATH]; Join(pat, d, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
                    wchar_t sub[MAX_PATH], q[MAX_PATH]; Join(sub, d, fd.cFileName); Join(q, sub, L"sce_sys\\snd0.at9");
                    if (FileExists(q)) { wcscpy(p, q); break; }
                } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
        }
        if (p[0] && FileExists(p)) {
            WIN32_FILE_ATTRIBUTE_DATA a, k;
            if (!GetFileAttributesExW(c, GetFileExInfoStandard, &k) || !GetFileAttributesExW(p, GetFileExInfoStandard, &a) || a.nFileSizeLow != k.nFileSizeLow) CopyFileW(p, c, FALSE);
            wcscpy(out, p); return;
        }
    }
    if (FileExists(c)) wcscpy(out, c);
}

static struct { HANDLE th, wake; volatile LONG want, quit, fast, vol; BOOL init; } g_mus = {NULL, NULL, -1, 0, 0, 55, FALSE};
static int g_musOn = -1; static DWORD g_musReadAt;
static void MusicPrefs(void)   /* the two settings, read again at most once a second (the UI thread) */
{
    DWORD now = GetTickCount();
    if (g_musOn >= 0 && now - g_musReadAt < 1000) return;
    g_musReadAt = now; g_musOn = PIni(L"game_music", 1);
    int v = PIni(L"music_volume", 55); InterlockedExchange(&g_mus.vol, v < 0 ? 0 : v > 100 ? 100 : v);
}
static int MusicVol(void) { return (int)g_mus.vol; }
/* the next block of samples, with the gain moving toward its target (fades) and the loop */
static void MusicFill(const Tune *t, short *out, DWORD frames, DWORD *pos, float *gain, float target, float step)
{
    for (DWORD i = 0; i < frames; i++) {
        if (*gain < target) { *gain += step; if (*gain > target) *gain = target; }
        else if (*gain > target) { *gain -= step; if (*gain < target) *gain = target; }
        float gg = *gain * t->norm;
        float l = t->pcm[(size_t)*pos * 2] * gg, r = t->pcm[(size_t)*pos * 2 + 1] * gg;
        out[i * 2] = (short)(l > 32767.0f ? 32767.0f : l < -32767.0f ? -32767.0f : l);
        out[i * 2 + 1] = (short)(r > 32767.0f ? 32767.0f : r < -32767.0f ? -32767.0f : r);
        if (++*pos >= t->loopEnd) *pos = t->loopStart;
    }
}
#define MUS_BUFS 4
#define MUS_MS 100
static DWORD WINAPI MusicThread(LPVOID unused)
{
    (void)unused;
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    Tune t = {0}; HWAVEOUT wo = NULL; WAVEHDR hdr[MUS_BUFS]; short *buf[MUS_BUFS] = {0};
    int playing = -1, failed = -1; DWORD pos = 0, bf = 0; float gain = 0.0f, target = 0.0f, step = 0.0f;
    memset(hdr, 0, sizeof(hdr));
    while (!g_mus.quit) {
        int want = (int)g_mus.want;
        if (!wo) {
            if (want < 0 || want == failed) { WaitForSingleObject(g_mus.wake, INFINITE); if (g_mus.want != want) failed = -1; continue; }
            /* a short pause on the game first, as the console does: a quick pass over the row plays nothing */
            if (WaitForSingleObject(g_mus.wake, 650) == WAIT_OBJECT_0 || g_mus.want != want || g_mus.quit) continue;
            wchar_t src[MAX_PATH]; MusicSrc(want, src); DWORD n = 0; BYTE *file = src[0] ? ReadWhole(src, &n) : NULL;
            BOOL ok = file && n < (32u << 20) && TuneDecode(file, n, &t); free(file);
            if (!ok) { failed = want; continue; }
            if (g_mus.want != want || g_mus.quit) { TuneFree(&t); continue; }
            WAVEFORMATEX wf = {WAVE_FORMAT_PCM, 2, (DWORD)t.rate, (DWORD)t.rate * 4, 4, 16, 0};
            if (waveOutOpen(&wo, WAVE_MAPPER, &wf, (DWORD_PTR)ev, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) { wo = NULL; TuneFree(&t); failed = want; continue; }
            bf = (DWORD)t.rate * MUS_MS / 1000; pos = 0; gain = 0.0f; target = MusicVol() / 100.0f; step = target / (t.rate * 1.4f); playing = want;
#ifdef PORTHEON_TEST   /* UI tests: what plays (PORTHEON_TEST_MUSICLOG=<file>) */
            { wchar_t lf[MAX_PATH]; if (GetEnvironmentVariableW(L"PORTHEON_TEST_MUSICLOG", lf, MAX_PATH)) { FILE *f = _wfopen(lf, L"a"); if (f) { fwprintf(f, L"play %s %lu frames %d Hz loop %lu..%lu norm %.2f vol %d src %s\n", want >= MUS_CUSTOM ? L"custom" : G[want].id, t.frames, t.rate, t.loopStart, t.loopEnd, t.norm, MusicVol(), src); fclose(f); } } }
#endif
            for (int i = 0; i < MUS_BUFS; i++) {
                buf[i] = (short *)calloc(bf, 4); hdr[i].lpData = (LPSTR)buf[i]; hdr[i].dwBufferLength = bf * 4; hdr[i].dwFlags = 0;
                if (!buf[i] || waveOutPrepareHeader(wo, &hdr[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) continue;
                MusicFill(&t, buf[i], bf, &pos, &gain, target, step); waveOutWrite(wo, &hdr[i], sizeof(WAVEHDR));
            }
            continue;
        }
        /* playing: a change (another game, PLAY, the settings, PORTHEON behind) fades out, then the device is closed */
        if (g_mus.want != playing && target > 0.0f) { target = 0.0f; step = (MusicVol() / 100.0f) / (t.rate * (g_mus.fast ? 0.12f : 0.45f)); }
        if (target == 0.0f && gain <= 0.0f) {
            waveOutReset(wo);
            for (int i = 0; i < MUS_BUFS; i++) { if (hdr[i].dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(wo, &hdr[i], sizeof(WAVEHDR)); free(buf[i]); buf[i] = NULL; memset(&hdr[i], 0, sizeof(WAVEHDR)); }
            waveOutClose(wo); wo = NULL; TuneFree(&t); playing = -1; continue;
        }
        if (target > 0.0f) { float v = MusicVol() / 100.0f; if (v != target) { target = v; step = 1.0f / (t.rate * 0.3f); } }
        WaitForSingleObject(ev, 40);
        for (int i = 0; i < MUS_BUFS; i++)
            if (buf[i] && (hdr[i].dwFlags & WHDR_DONE)) { hdr[i].dwFlags &= ~WHDR_DONE; MusicFill(&t, buf[i], bf, &pos, &gain, target, step); waveOutWrite(wo, &hdr[i], sizeof(WAVEHDR)); }
    }
    if (wo) {
        waveOutReset(wo);
        for (int i = 0; i < MUS_BUFS; i++) { if (hdr[i].dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(wo, &hdr[i], sizeof(WAVEHDR)); free(buf[i]); }
        waveOutClose(wo);
    }
    TuneFree(&t); CloseHandle(ev); return 0;
}
static void MusicWant(int g)
{
    if (g >= 0 && !g_mus.init) {   /* the thread starts the first time a game wants its music */
        g_mus.init = TRUE; g_mus.wake = CreateEventW(NULL, FALSE, FALSE, NULL);
        g_mus.th = CreateThread(NULL, 512 * 1024, MusicThread, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    }
    if (!g_mus.init || g_mus.want == g) return;
    InterlockedExchange(&g_mus.fast, 0); InterlockedExchange(&g_mus.want, g); SetEvent(g_mus.wake);
}
/* called every frame of the animation timer and on window changes: which game's music should be playing now */
static void MusicFollow(void)   /* 6.2 (the owner): each game's background music is gone: the sound of the home screen is the video */
{
    (void)MusicPrefs; (void)g_ytSound;
    MusicWant(-1);
}
static void MusicStop(BOOL fast)   /* PLAY: out in about a tenth of a second */
{
    if (!g_mus.init) return;
    InterlockedExchange(&g_mus.fast, fast ? 1 : 0); InterlockedExchange(&g_mus.want, -1); SetEvent(g_mus.wake);
}
static void MusicQuit(void)
{
    if (!g_mus.init) return;
    MusicStop(TRUE); Sleep(140);
    InterlockedExchange(&g_mus.quit, 1); SetEvent(g_mus.wake);
    if (g_mus.th) { WaitForSingleObject(g_mus.th, 600); CloseHandle(g_mus.th); g_mus.th = NULL; }
}
