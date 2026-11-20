/* gamever.c - part of portheon.c (included by it, after detect.c). 6.0 (the user, 2026-11-07: "show the version of each
 * game the person is playing, whether the update is in too, all clear so nobody is confused"): the version of the
 * player's own game files, read from them (PS4 sce_sys\param.sfo APP_VER, PS5 sce_sys\param.json contentVersion), what
 * PORTHEON's port needs, and for UFC 5 and 6 what their launcher found at the last PLAY (UFC5.ini / UFC6.ini:
 * game_version, game_code, game_fixes, game_clock, game_moved). Nothing is written to the game folder. */

typedef struct {
    int ok;            /* 1 the right version, 0 unknown / no game yet, -1 something is missing or another version */
    wchar_t ver[24];   /* "01.006", "01.14" ("" when unknown) */
    wchar_t tag[64];   /* the short form for the headers: "v01.006 · atualizado" */
    wchar_t line[400]; /* one sentence for the game page */
} GameVer;
static GameVer g_gv[NGAMES]; static DWORD g_gvAt[NGAMES]; static wchar_t g_gvDir[NGAMES][MAX_PATH];
static void ExtVersion(int g, GameVer *v);   /* extgames.c: Zelda and God of War III */

/* "key": "value" in a JSON text, the first one (param.json is flat enough for this) */
static BOOL GvJsonStr(const char *b, const char *key, char *out, int n)
{
    char k[64]; snprintf(k, sizeof(k), "\"%s\"", key); out[0] = 0;
    const char *p = strstr(b, k); if (!p) return FALSE;
    p += strlen(k); while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != ':') return FALSE; p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != '"') return FALSE; p++;
    int i = 0; while (*p && *p != '"' && i < n - 1) out[i++] = *p++;
    out[i] = 0; return i > 0;
}
static BOOL PsJsonVersions(const wchar_t *dir, char *content, char *origin)
{
    content[0] = origin[0] = 0;
    wchar_t p[MAX_PATH]; Join(p, dir, L"sce_sys\\param.json");
    HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    char *b = (char *)malloc(262144); DWORD got = 0;
    if (!b) { CloseHandle(h); return FALSE; }
    ReadFile(h, b, 262143, &got, NULL); CloseHandle(h); b[got] = 0;
    GvJsonStr(b, "contentVersion", content, 24); GvJsonStr(b, "originContentVersion", origin, 24);
    free(b); return content[0] != 0;
}
/* "01.006.000" -> "01.006" */
static void ShortVer(const char *in, wchar_t *out, int n)
{
    char t[24]; snprintf(t, sizeof(t), "%s", in);
    char *d1 = strchr(t, '.'); char *d2 = d1 ? strchr(d1 + 1, '.') : NULL; if (d2) *d2 = 0;
    MultiByteToWideChar(CP_UTF8, 0, t, -1, out, n); out[n - 1] = 0;
}
/* the best APP_VER of a PS4 game in d or in the folders right under it (the game and its update side by side) */
static BOOL Ps4BestVersion(int g, const wchar_t *d, char *best)
{
    best[0] = 0; char v[16];
    if (DumpAt(d, v) == g && v[0]) strcpy(best, v);
    wchar_t pat[MAX_PATH]; Join(pat, d, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
            wchar_t sub[MAX_PATH]; Join(sub, d, fd.cFileName);
            if (DumpAt(sub, v) == g && strcmp(v, best) > 0) strcpy(best, v);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return best[0] != 0;
}
static void GameVerRead(int g, GameVer *v)
{
    memset(v, 0, sizeof(*v));
    if (EXTG(g)) { ExtVersion(g, v); return; }
    wchar_t d[MAX_PATH]; GameDir(g, d);
    if (X360G(g)) {   /* UFC Undisputed 3: the Xbox 360 game has no update to put in; the port's own version */
        v->ok = d[0] && DirExists(d) ? 1 : 0;
        _snwprintf(v->tag, 64, L"%s", L"Xbox 360");
        _snwprintf(v->line, 400, L2(L"Jogo do Xbox 360 (o original ou o Reborn): não precisa de atualização. Port: %s.", L"The Xbox 360 game (original or Reborn): no update needed. Port: %s."), g_ver[g][0] ? g_ver[g] : L"UFC3MONS");
        return;
    }
    if (!d[0] || !DirExists(d)) { _snwprintf(v->line, 400, L"%s", L2(L"Versão do jogo: escolha a pasta do jogo primeiro.", L"Game version: choose the game folder first.")); return; }
    if (PS4G(g)) {
        char best[16];
        if (!Ps4BestVersion(g, d, best)) { _snwprintf(v->line, 400, L"%s", L2(L"Versão do jogo: não consegui ler o param.sfo da pasta.", L"Game version: could not read the folder's param.sfo.")); return; }
        MultiByteToWideChar(CP_UTF8, 0, best, -1, v->ver, 24);
        static const char *const want[4] = {"01.08", "01.13", "01.14", "28.00"};
        wchar_t w[16]; MultiByteToWideChar(CP_UTF8, 0, want[g], -1, w, 16);
        if (VersionOk(g, best)) {
            v->ok = 1; _snwprintf(v->tag, 64, L2(L"v%s · atualizado", L"v%s · updated"), v->ver);
            _snwprintf(v->line, 400, L2(L"Versão do jogo: %s, com a atualização que o port precisa (%s). Tudo certo.", L"Game version: %s, with the update the port needs (%s). All set."), v->ver, w);
        } else if (strcmp(best, want[g]) < 0) {
            v->ok = -1; _snwprintf(v->tag, 64, L2(L"v%s · falta o update", L"v%s · update missing"), v->ver);
            _snwprintf(v->line, 400, L2(L"Versão do jogo: %s, SEM a atualização %s que o port precisa. Coloque o update %s na pasta do jogo (ou as duas pastas lado a lado e escolha a de fora). Veja \"Como instalar\".",
                                        L"Game version: %s, WITHOUT the update %s the port needs. Put update %s in the game folder (or both folders side by side and pick the outer one). See \"How to install\"."), v->ver, w, w);
        } else {
            v->ok = -1; _snwprintf(v->tag, 64, L2(L"v%s · outra versão", L"v%s · other version"), v->ver);
            _snwprintf(v->line, 400, L2(L"Versão do jogo: %s, mais nova que a %s que o port conhece: pode não abrir. Use a atualização %s.", L"Game version: %s, newer than the %s the port knows: it may not start. Use update %s."), v->ver, w, w);
        }
        return;
    }
    char cv[24], ov[24];
    if (!PsJsonVersions(d, cv, ov)) { _snwprintf(v->line, 400, L"%s", L2(L"Versão do jogo: não consegui ler o sce_sys\\param.json da pasta.", L"Game version: could not read the folder's sce_sys\\param.json.")); return; }
    ShortVer(cv, v->ver, 24);
    wchar_t base[24] = L""; if (ov[0] && strcmp(ov, cv)) ShortVer(ov, base, 24);
    BOOL upd = wcscmp(v->ver, L"01.000") != 0;
    wchar_t last[200] = L"";
    {   /* what the game's launcher found at the last PLAY */
        wchar_t lv[32]; GetStr(g, L"game_version", lv, 32, L""); int fixes = GetInt(g, L"game_fixes", -1);
        if (lv[0] && fixes >= 0) {
            wchar_t lvs[24]; char l8[32]; WideCharToMultiByte(CP_UTF8, 0, lv, -1, l8, 32, NULL, NULL); ShortVer(l8, lvs, 24);
            if (UFC5G(g) && fixes > 0) _snwprintf(last, 200, L2(L" Na última vez que abriu: %d correções do PORTHEON aplicadas%s%s.", L" The last time it opened: %d of PORTHEON's fixes applied%s%s."), fixes,
                                                   GetInt(g, L"game_clock", 0) ? L2(L", com a velocidade certa", L", with the right speed") : L2(L", sem a do relógio (FPS limitado a 60)", L", without the clock one (FPS capped at 60)"),
                                                   GetInt(g, L"game_moved", 0) ? L2(L", achadas em outro lugar do executável", L", found elsewhere in the executable") : L"");
            else if (fixes > 0) _snwprintf(last, 200, L2(L" Na última vez que abriu: %d correções do PORTHEON aplicadas.", L" The last time it opened: %d of PORTHEON's fixes applied."), fixes);
            else _snwprintf(last, 200, L"%s", L2(L" Na última vez que abriu, nenhuma correção do PORTHEON coube neste executável (eboot.bin).", L" The last time it opened, none of PORTHEON's fixes fit this executable (eboot.bin)."));
            if (UFC6G(g)) { wchar_t code[24]; GetStr(g, L"game_code", code, 24, L"");
                if (code[0] && code[0] != L'?' && !wcsstr(code, v->ver)) { size_t k = wcslen(last); _snwprintf(last + k, 200 - k, L2(L" O código do eboot.bin é do %s.", L" The eboot.bin's code is %s's."), code); } }
            last[199] = 0;
        }
    }
    if (UFC6G(g)) {
        BOOL known = !wcscmp(v->ver, L"01.004") || !wcscmp(v->ver, L"01.005") || !wcscmp(v->ver, L"01.006");
        v->ok = known ? 1 : -1;
        _snwprintf(v->tag, 64, known ? L2(L"v%s · suportado", L"v%s · supported") : L2(L"v%s · não suportado", L"v%s · not supported"), v->ver);
        if (known) _snwprintf(v->line, 400, L2(L"Versão do jogo: %s%s%s%s (o PORTHEON tem as correções do 01.004, 01.005 e 01.006).%s", L"Game version: %s%s%s%s (PORTHEON has the fixes for 01.004, 01.005 and 01.006).%s"),
                           v->ver, upd ? L2(L", com atualização", L", with an update") : L"", base[0] ? L2(L" sobre a base ", L" over base ") : L"", base, last);
        /* 7.3 (UFC6.exe 1.31.0): a version newer than 01.006 (01.007 and later) gets the fixes found by the game's own code */
        else if (!wcsncmp(v->ver, L"01.", 3) && wcscmp(v->ver, L"01.006") > 0) {
            wchar_t code[24]; GetStr(g, L"game_code", code, 24, L""); int fx = GetInt(g, L"game_fixes", 0);
            v->ok = 1; _snwprintf(v->tag, 64, L2(L"v%s · nova, correções pelo código", L"v%s · new, fixes by code"), v->ver);
            if (!wcscmp(code, L"new") && fx > 0)
                _snwprintf(v->line, 400, L2(L"Versão do jogo: %s, mais nova que a 01.006. Na última vez o PORTHEON achou %d correções pelo código do jogo; as câmeras de transmissão e a velocidade v3 ficam de fora até esta versão ganhar a tabela dela.",
                                            L"Game version: %s, newer than 01.006. Last time PORTHEON found %d fixes by the game's code; the broadcast cameras and speed v3 stay out until this version gets its own table."), v->ver, fx);
            else
                _snwprintf(v->line, 400, L2(L"Versão do jogo: %s, mais nova que a 01.006: no JOGAR o PORTHEON procura as correções pelo código do jogo (as câmeras de transmissão e a velocidade v3 ficam de fora até esta versão ganhar a tabela dela).%s",
                                            L"Game version: %s, newer than 01.006: at PLAY PORTHEON looks for the fixes by the game's code (the broadcast cameras and speed v3 stay out until this version gets its own table).%s"), v->ver, last);
        }
        else _snwprintf(v->line, 400, L2(L"Versão do jogo: %s. O PORTHEON tem as correções do 01.004, 01.005 e 01.006: em outra versão a luta pode ficar em câmera lenta ou fechar. Use uma dessas.%s", L"Game version: %s. PORTHEON has the fixes for 01.004, 01.005 and 01.006: on another version fights may run in slow motion or close. Use one of those.%s"), v->ver, last);
    } else if (UFC5G(g)) {
        BOOL best = !wcscmp(v->ver, L"01.031"), moved = !wcscmp(v->ver, L"01.032");
        v->ok = best || moved ? 1 : -1;
        _snwprintf(v->tag, 64, best ? L2(L"v%s · recomendado", L"v%s · recommended") : moved ? L2(L"v%s · suportado", L"v%s · supported") : L2(L"v%s · não testado", L"v%s · untested"), v->ver);
        if (best) _snwprintf(v->line, 400, L2(L"Versão do jogo: %s, a recomendada: todas as correções do PORTHEON são feitas para ela.%s", L"Game version: %s, the recommended one: all of PORTHEON's fixes are made for it.%s"), v->ver, last);
        else if (moved) _snwprintf(v->line, 400, L2(L"Versão do jogo: %s. O PORTHEON procura as correções dentro dela; a 01.031 é a mais testada.%s", L"Game version: %s. PORTHEON looks for its fixes inside it; 01.031 is the most tested.%s"), v->ver, last);
        else _snwprintf(v->line, 400, L2(L"Versão do jogo: %s. As correções do PORTHEON são para a 01.031 (e procuradas na 01.032): nesta versão o jogo pode fechar ou andar rápido demais.%s", L"Game version: %s. PORTHEON's fixes are for 01.031 (and searched for in 01.032): on this version the game may close or run too fast.%s"), v->ver, last);
    }
    v->line[399] = 0;
}
/* the version of game g, read again at most every 3 s or when its folder changes */
static const GameVer *GameVerOf(int g)
{
    if (g < 0 || g >= NGAMES) { static GameVer none; return &none; }
    wchar_t d[MAX_PATH]; GameDir(g, d);
    if (!g_gvAt[g] || GetTickCount() - g_gvAt[g] > 3000 || wcscmp(g_gvDir[g], d)) { GameVerRead(g, &g_gv[g]); g_gvAt[g] = GetTickCount(); if (!g_gvAt[g]) g_gvAt[g] = 1; wcscpy(g_gvDir[g], d); }
    return &g_gv[g];
}
