/* ui_items.c - part of ui.c (4.0): the settings lists of a game's hub (Video, Gameplay, Online, Extras, Game files):
 * dropdowns, switches, text fields, buttons and notes laid out in three columns inside the hub's panel. */
enum { IT_DROP, IT_SWITCH, IT_EDIT, IT_FBUTTON, IT_NOTE, IT_WARN, IT_HEAD, IT_HOSTIP, IT_PADPICK, IT_PADPICK2 /* Released: Player 2's controller */ };
typedef struct { int type; const Field *f; HWND edit; const wchar_t *text; int id, span; } Item;
static int OnlineItems(int g, Item *it, int n);   /* online.c */
static void ArenaOpen(int g);                     /* online.c, 7.2 */
static BOOL KeysCustom(int g);                    /* ui_keys.c (6.0) */
static void HostIpWidget(HDC dc, GpGraphics *g, int x, int y, int w);
static void HostIpMenu(void);
static void CopyHostIp(void);
static void PadPickWidget(HDC dc, GpGraphics *g, int x, int y, int w);
static void PadPickMenu(void);
static void Pad2PickWidget(HDC dc, GpGraphics *g, int x, int y, int w);   /* Released: online.c */
static void Pad2PickMenu(void);
static void PmOpen2(int g);   /* Released: ui_padmap62.c */
static const Field *g_dropF[40], *g_swF[40];
static RECT g_editRect[4]; static BOOL g_editShown[4], g_editWant[4];
static void PlaceEdit(int k, HWND h, RECT r, BOOL show)
{
    OffsetRect(&r, g_ox, g_oy);
    if (show && memcmp(&r, &g_editRect[k], sizeof(RECT))) { SetWindowPos(h, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE); g_editRect[k] = r; }
    if (show != g_editShown[k]) { ShowWindow(h, show ? SW_SHOWNA : SW_HIDE); g_editShown[k] = show; }
}
static HWND g_editProf;   /* the profile editor's name field */
static int EditIndex(HWND h) { return h == g_editGame ? 0 : h == g_editPlayer ? 1 : h == g_editAddr ? 2 : 3; }
static wchar_t g_note[4][900];   /* the texts made for this paint */
static int AddI(Item *it, int n, int type, const Field *f, HWND e, const wchar_t *text, int id, int span)
{
    if (n >= 40) return n;
    it[n].type = type; it[n].f = f; it[n].edit = e; it[n].text = text; it[n].id = id; it[n].span = span; return n + 1;
}
static int FolderNoteNow(int g, int st, const wchar_t *d, wchar_t *out, int n);
static int FolderNote(int g, wchar_t *out, int n)   /* 0 fine, 1 warning (kept for 2 s: it reads the game folder) */
{
    static int cg = -1, cst = -1, cwarn; static DWORD ct; static wchar_t cdir[MAX_PATH], ctext[900];
    int st = St(g); wchar_t d[MAX_PATH]; GameDir(g, d);
    if (cg != g || cst != st || !g_stTick || GetTickCount() - ct > 2000 || wcscmp(cdir, d)) { cwarn = FolderNoteNow(g, st, d, ctext, 900); cg = g; cst = st; ct = GetTickCount(); wcscpy(cdir, d); }
    wcsncpy(out, ctext, n); out[n - 1] = 0; return cwarn;
}
static int FolderNoteNow(int g, int st, const wchar_t *d, wchar_t *out, int n)
{
    wchar_t miss[64], why[400];
    if (st == S_NOCPU && (G[g].ps5 || X360G(g))) { NoAvx2Text(g, out, n); return 1; }
    if (st == S_NOCPU) { CpuLacksPs4(miss, 64); _snwprintf(out, n, L2(L"Este processador (%s) não tem %s, que o código do jogo de PS4 usa direto. O modo de compatibilidade do PORTHEON (Intel Core de 2ª e 3ª geração) ainda não existe para este jogo: ele precisa de um Intel Core de 4ª geração ou mais novo, ou de um AMD Ryzen.",
                                                                    L"This processor (%s) lacks %s, which PS4 game code uses directly. PORTHEON's compatibility mode (Intel Core 2nd and 3rd generation) does not exist for this game yet: it needs an Intel Core 4th generation or newer, or an AMD Ryzen."), g_cpuName, miss); return 1; }
    if (st == S_NOPORT && NGPORTG(g)) { _snwprintf(out, n, L"%s", NgNoPortText(g)); return 1; }   /* 6.1 */
    if (st == S_NOPORT) { _snwprintf(out, n, L2(L"O port do %s não está em games\\%s. Extraia o pacote do PORTHEON inteiro de novo.", L"The %s port is not in games\\%s. Extract the whole PORTHEON package again."), G[g].name, G[g].id); return 1; }
    if (EXTG(g)) {   /* 6.0: Zelda (a Switch file or folder, or the native build) and God of War III (a PS4 dump) */
        const GameVer *gv = GameVerOf(g);
        if (st == S_PICK) {
            if (NGPORTG(g)) _snwprintf(out, n, L"%s", NgPickText(g));   /* 6.1 */
            else if (ZELDAG(g)) _snwprintf(out, n, L"%s", L2(L"Escolha o SEU Zelda: Ocarina of Time de Switch 2: o arquivo .nup (não precisa de chaves), .nsp ou .xci (precisam das suas chaves prod.keys e title.keys em %APPDATA%\\eden\\keys), ou a pasta extraída com exefs e romfs. Pode também arrastar o arquivo para esta janela. O PORTHEON não vem com o jogo.",
                                                         L"Choose YOUR Zelda: Ocarina of Time for Switch 2: the .nup file (needs no keys), .nsp or .xci (need your prod.keys and title.keys in %APPDATA%\\eden\\keys), or the extracted folder with exefs and romfs. You can also drop the file on this window. PORTHEON ships no game."));
            else _snwprintf(out, n, L"%s", L2(L"Escolha a pasta do SEU God of War III Remastered de PS4 já extraído (CUSA01623, com eboot.bin, sce_sys e sce_module), com o update 01.02 junto. O PORTHEON não vem com o jogo.",
                                              L"Choose the folder of YOUR extracted PS4 God of War III Remastered (CUSA01623, with eboot.bin, sce_sys and sce_module), with update 01.02 merged in. PORTHEON ships no game."));
            return 1;
        }
        _snwprintf(out, n, L2(L"Tudo certo: aperte JOGAR. %s", L"All set: press PLAY. %s"), gv->line); out[n - 1] = 0;
        return gv->ok < 0;
    }
    if (X360G(g)) {   /* 5.0: UFC Undisputed 3: default.xex or an Xbox 360 .iso */
        wchar_t iso[MAX_PATH], p[MAX_PATH];
        if (st == S_PICK) { _snwprintf(out, n, L"%s", L2(L"Escolha a pasta do UFC Undisputed 3: a do jogo extraído (com o default.xex, Original ou Reborn) ou a pasta onde está o .iso do Xbox 360. Pode também arrastar a pasta ou o .iso para esta janela.",
                                                           L"Choose the UFC Undisputed 3 folder: the extracted game's (with default.xex, Original or Reborn) or the folder that holds the Xbox 360 .iso. You can also drop the folder or the .iso on this window.")); return 1; }
        Join(p, d, L"default.xex");
        if (FileExists(p)) _snwprintf(out, n, L"%s", L2(L"Tudo certo: o jogo extraído (default.xex) foi encontrado nessa pasta. Aperte JOGAR.", L"All set: the extracted game (default.xex) was found in that folder. Press PLAY."));
        else if (X3Iso(d, iso, MAX_PATH)) _snwprintf(out, n, L2(L"Tudo certo: o port vai abrir o %s dessa pasta. Aperte JOGAR.", L"All set: the port will start %s from that folder. Press PLAY."), iso);
        else _snwprintf(out, n, L"%s", L2(L"Aperte JOGAR.", L"Press PLAY."));
        return 0;
    }
    int det = DetectFolder(d, NULL, why, 400);
    if (st == S_PICK) {
        _snwprintf(out, n, L2(L"Escolha a pasta do jogo já extraída (com eboot.bin, sce_sys e sce_module), arraste ela para esta janela ou use \"Procurar no PC\". O %s precisa de: %s, versão %s. Dúvidas: \"Instalar jogo\".",
                              L"Choose the extracted game folder (with eboot.bin, sce_sys and sce_module), drop it on this window or use \"Find on this PC\". %s needs: %s, version %s. Questions: \"Install a game\"."), G[g].name, G[g].tid, G[g].upd);
        return 1;
    }
    if (det >= 0 && det != g) { _snwprintf(out, n, L2(L"Atenção: essa pasta parece ser do %s, não do %s.", L"Careful: that folder looks like %s, not %s."), G[det].name, G[g].name); return 1; }
    if (why[0]) { _snwprintf(out, n, L"%s", why); return 1; }
    if (PS4G(g)) { GameCheck r; CheckGameFiles(g, d, &r); if (GameCheckText(g, &r, out, n)) return 1; }   /* 4.0: the archives too (UFC 1-4) */
    if (g == 3 && Ufc4Build(d) == 2) { _snwprintf(out, n, L"%s", Ufc4BuildText()); return 1; }   /* 4.0.1: the 6.72+ build of the update */
    /* 6.0: and the game's version, with or without its update (gamever.c) */
    const GameVer *gv = GameVerOf(g);
    _snwprintf(out, n, L2(L"Tudo certo: o jogo foi encontrado nessa pasta e os arquivos dele estão completos. Aperte JOGAR. %s", L"All set: the game was found in that folder and its files are complete. Press PLAY. %s"), gv->line);
    out[n - 1] = 0;
    return gv->ok < 0;
}
static const Field F_PROFNAME = {S_PLAYER, L"profile_name", NULL, 0, 0};
/* 4.3: the game fixes of UFC 5 and UFC 6 (F6_REALTIME, F6_AUDIOWD), one text for both games */
static const wchar_t *RealtimeNote(void)
{
    /* 5.0: shorter, so the Video tab fits with the FSR 1 row */
    return L2(L"Luta na velocidade do PS5: o PORTHEON faz a luta andar 60 passos por segundo, separados do FPS da tela, então ela não fica em câmera lenta mesmo a 30 FPS.",
              L"Fights at the PS5's pace: PORTHEON makes the fight move 60 steps a second, apart from the screen's FPS, so it is not in slow motion even at 30 FPS.");
}
/* 4.3.1: UFC 5's own way to the same switch (UFC5.exe 1.18.0 patches the game's clock; 1.17.0 set the game's time
   scale from the measured FPS, which stays as the fallback for another version of the game) */
static const wchar_t *RealtimeNote5(void)
{
    /* 6.0 final: shorter (the old five lines ran past the Video panel, which does not scroll) */
    return L2(L"Luta na velocidade do PS5: o UFC 5 fazia cada quadro andar 1/60 s, então abaixo de 60 FPS a luta ficava em câmera lenta. Ligada, cada quadro anda o tempo real que levou (até 50 ms), e o jogo fica no tempo certo com qualquer FPS. O replay do nocaute continua lento, como no jogo.",
              L"PS5 fight speed: UFC 5 moved each frame by 1/60 s, so below 60 FPS fights ran in slow motion. When on, each frame moves by the real time it took (up to 50 ms), so the game keeps real time at any FPS. The knockout replay stays slow, as in the game.");
}
static const wchar_t *AudioWdNote(void)
{
    return L2(L"Vigia de áudio: o jogo cria uma thread que só confere quanto o áudio pesa no processador (para aliviá-lo quando o processador fica sobrecarregado), e neste motor ela ocupava quase um núcleo inteiro o tempo todo. Desligada, esse núcleo fica livre para o jogo; o áudio em si continua rodando.",
              L"Audio watchdog: the game starts a thread that only checks how much the audio weighs on the processor (to lighten it when the processor is overloaded), and in this engine it kept almost a whole core busy all the time. Turned off, that core is free for the game; the audio itself keeps running.");
}
/* 4.3.2: the "live broadcast camera" (F4_CAM3P, F56_CAM3P): one text for every game, 4.3.3 plus what differs per game */
static const wchar_t *Cam3pNote(int g)
{
    const wchar_t *base = L2(L"Câmera de transmissão ao vivo: ligada, na luta em pé o Select (View no controle de Xbox, touchpad no de PlayStation) troca entre a câmera normal e a câmera de transmissão ao vivo, de perto, atrás do lutador e por cima do ombro, como os cinegrafistas da beira do octógono numa transmissão do UFC; aperte de novo para voltar. No chão e no clinche fica a câmera normal, e o jogo sempre abre na câmera normal.",
              L"Live broadcast camera: when on, in a stand-up fight Select (View on an Xbox pad, the touchpad on a PlayStation pad) switches between the normal camera and the live broadcast camera, close up behind the fighter, over the shoulder, like the cage-side cameramen of a UFC broadcast; press it again to go back. Ground and clinch keep the normal camera, and the game always opens on the normal camera.");
    /* 4.3.3: UFC 3's patch is in the Classic style's camera, UFC 4's in the other three styles' (_build-tools\camera3p);
       in the PS5 games' engines an Xbox pad's View arrives as Share, and a --keymap replaces the whole default keymap */
    const wchar_t *extra = g == 2 ? L2(L"No UFC 3 ela funciona com o estilo de câmera Clássico do jogo (Estilo de câmera, nas opções de jogabilidade).",
                                       L"In UFC 3 it works with the game's Classic camera style (Camera Style, in the gameplay options).") :
                           g == 3 ? L2(L"No UFC 4 ela funciona com os estilos de câmera Médio, Aberto e Fechado do jogo (o Clássico fica normal).",
                                       L"In UFC 4 it works with the game's Medium, Wide and Tight camera styles (Classic stays normal).") :
                           G[g].ps5 ? L2(L"No UFC 5 e no UFC 6 use o touchpad de um controle de PlayStation: o View do controle de Xbox não chega nesses dois jogos.",
                                         L"In UFC 5 and UFC 6 use a PlayStation controller's touchpad: an Xbox controller's View button does not reach these two games.") : NULL;
    /* 4.3.3 final: UFC 6 (UFC6.exe 1.7.1) has only the TV director; UFC 3, 4 and 5 the four cameras (UFC 2's are a cheat).
       4.3.4: UFC 6 (UFC6.exe 1.8.0) the broadcast's fixed cameras around the cage, with cuts */
    /* 4.3.5 (UFC6.exe 1.9.0): the in-game route, and the game's "Impact moments" replays that cut in with low cameras */
    if (G[g].ps5 && !UFC5G(g)) { extra = NULL;   /* 6.0 final: shorter, so the whole note fits under the cheat switches */
        base = L2(L"Câmera de transmissão ao vivo: na luta em pé, o touchpad do controle de PlayStation troca entre a câmera normal e a de TV (câmeras paradas na beira da grade, com cortes). No jogo use Estilo da câmera = Clássica e Momentos impactantes = Não.",
                  L"Live broadcast camera: in a stand-up fight a PlayStation controller's touchpad switches between the normal camera and the TV one (still cameras at the cage side, with cuts). In the game use Camera style = Classic and Impact moments = No."); }
    /* 6.1: UFC 1, 3 and 4 have UFC 6's TV camera (normal <-> TV); UFC 1's follows the fight without cuts (no room) */
    else if (g == 0) base = L2(L"Câmera de transmissão ao vivo (experimental): na luta em pé, o Select (View no Xbox, touchpad no PlayStation) troca entre a câmera normal e a de TV (uma câmera no alto, ao lado da grade, que acompanha a luta).",
                               L"Live broadcast camera (experimental): in a stand-up fight Select (View on Xbox, the touchpad on PlayStation) switches between the normal camera and the TV one (one camera high at the cage side that follows the fight).");
    /* 6.1 review: UFC 3 and 4 in one short text with their camera-style condition (the switch says "experimental"), so
       the whole note fits at the bottom of the Gameplay tab */
    else if (g == 2) { extra = NULL; base = L2(L"Câmera de transmissão ao vivo: na luta em pé, o Select (View no Xbox, touchpad no PlayStation) troca entre a câmera normal e a de TV (câmeras paradas no alto, com cortes). Só com o estilo de câmera Clássico do jogo.",
                                              L"Live broadcast camera: in a stand-up fight Select (View on Xbox, the touchpad on PlayStation) switches between the normal camera and the TV one (still cameras high up, with cuts). Only with the game's Classic camera style."); }
    else if (g == 3) { extra = NULL; base = L2(L"Câmera de transmissão ao vivo: na luta em pé, o Select (View no Xbox, touchpad no PlayStation) troca entre a câmera normal e a de TV (câmeras paradas no alto, com cortes). Só com os estilos de câmera Médio, Aberto e Fechado do jogo (o Clássico fica normal).",
                                              L"Live broadcast camera: in a stand-up fight Select (View on Xbox, the touchpad on PlayStation) switches between the normal camera and the TV one (still cameras high up, with cuts). Only with the game's Medium, Wide and Tight camera styles (Classic stays normal)."); }
    else if (UFC5G(g)) base = L2(L"Câmeras da luta: ligadas, na luta em pé cada toque no Select (View no controle de Xbox, touchpad no de PlayStation) passa para a próxima câmera: normal, 3ª pessoa (por cima do ombro), transmissão ao vivo (como na TV, câmeras em volta do octógono com cortes) e 1ª pessoa (dos olhos do lutador). O jogo sempre abre na câmera normal.",
                                                      L"Fight cameras: when on, in a stand-up fight each press of Select (View on an Xbox pad, the touchpad on a PlayStation pad) moves to the next camera: normal, 3rd person (over the shoulder), live broadcast (as on TV, cameras around the octagon with cuts) and 1st person (from the fighter's eyes). The game always opens on the normal camera.");
    if (!extra) return base;
    static wchar_t both[1600]; _snwprintf(both, 1600, L"%s %s", base, extra); both[1599] = 0; return both;
}
static int BuildItems(int g, int tab, Item *it)
{
    int n = 0; HzScanOnce();   /* perf 11-13: F_HZ's list, made at the first list built instead of at the start (tune62.c) */
    switch (tab) {
    case TAB_GAME:
        n = AddI(it, n, IT_EDIT, NULL, g_editGame, L2(L"Pasta do jogo", L"Game folder"), 0, 2);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Escolher a pasta...", L"Choose the folder..."), H_BROWSE, 1);
        { int warn = FolderNote(g, g_note[0], 900); n = AddI(it, n, warn ? IT_WARN : IT_NOTE, NULL, NULL, g_note[0], 0, 3); }
        if (PS4G(g)) n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Verificar arquivos do jogo", L"Check the game's files"), H_VERIFY, 1);
        if (NGPORTG(g)) n = AddI(it, n, IT_FBUTTON, NULL, NULL, PortOk(g) ? L2(L"Reinstalar / atualizar o port", L"Reinstall / update the port") : L2(L"Instalar o port", L"Install the port"), H_EXTINST, 1);   /* 6.1 */
        if (!X360G(g) && !ZELDAG(g) && !NGPORTG(g)) n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Procurar no PC", L"Find on this PC"), H_SCAN, 1);   /* 5.0: the scan knows PS4/PS5 dumps only */
        if (X360G(g)) n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Configurar o port", L"Configure the port"), H_PORTCFG, 1);
        if (EXTG(g) && !FNCG(g)) n = AddI(it, n, IT_FBUTTON, NULL, NULL, ZELDAG(g) ? L2(L"Abrir o emulador (configurar)", L"Open the emulator (settings)") : L2(L"Abrir o launcher do port", L"Open the port's launcher"), H_EXTCFG, 1);   /* 6.0 */
        if (ZELDAG(g)) n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Ocarina Setup: recompilar para PC", L"Ocarina Setup: recompile for PC"), H_RECOMP, 1);   /* 6.0: Ocarina Setup 0.2.1 (Ocarina Recomp) */
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Como instalar (passo a passo)", L"How to install (step by step)"), H_GUIDE, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Abrir a pasta do jogo", L"Open the game folder"), H_OPENGAME, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Criar atalho na área de trabalho", L"Make a desktop shortcut"), H_SHORTCUT, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Adicionar à biblioteca da Steam", L"Add to the Steam library"), H_STEAM, 1);   /* 5.0 */
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Este PC roda?", L"Does this PC run it?"), H_PCCHECK, 1);   /* 4.3.8 */
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Gerar relatório para o Discord", L"Make a report for Discord"), H_REPORT, 1);   /* 4.3.5 */
        if (UFC6G(g)) n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Preparar o UFC 6 (primeira vez sem travadas)", L"Prepare UFC 6 (a first time without stutter)"), H_WARMUP, 1);   /* 4.3.7 */
        if (UFC6G(g)) n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Testar o desempenho do PC", L"Test this PC's performance"), H_BENCH, 1);   /* 4.3.5 */
        n = AddI(it, n, IT_SWITCH, &F_SHORTCUT, NULL, NULL, 0, 3);
        { wchar_t pd[MAX_PATH]; PortDir(g, pd);
          /* 6.1 review: no bare "?" when the port's version is not known, or the port is not installed */
          const wchar_t *ver = g_ver[g][0] ? g_ver[g] : NGPORTG(g) && !PortOk(g) ? L2(L"port não instalado", L"port not installed") : L2(L"versão desconhecida", L"version unknown");
          if (ZELDAG(g)) { wchar_t rr[MAX_PATH]; BOOL nat = ZeldaRecompRoot(rr);   /* 6.0 */
              _snwprintf(g_note[1], 900, L2(L"Emuladores: %s\\emu (Lichii MDias v4%s%s)  ·  dados, chaves, saves e cache: %%APPDATA%%\\eden  ·  recompilador: %s\\recomp%s%s",
                                            L"Emulators: %s\\emu (Lichii MDias v4%s%s)  ·  data, keys, saves and cache: %%APPDATA%%\\eden  ·  recompiler: %s\\recomp%s%s"),
                         pd, ZeldaEmuOk(1) ? L"" : L2(L" (faltando)", L" (missing)"), ZeldaEmuOk(2) ? L", Frizti v3" : L"", pd, nat ? L2(L"  ·  jogo nativo: ", L"  ·  native game: ") : L"", nat ? rr : L""); } else
          if (FNCG(g)) _snwprintf(g_note[1], 900, L2(L"Port: %s  ·  configurações: %s\\dist\\FNCReborn.toml", L"Port: %s  ·  settings: %s\\dist\\FNCReborn.toml"), pd, pd); else
          if (ISSG(g)) _snwprintf(g_note[1], 900, L2(L"Port: %s  ·  configurações: %s\\user\\launcher.ini  ·  saves: %s\\user\\saves  ·  logs: %s\\user\\logs", L"Port: %s  ·  settings: %s\\user\\launcher.ini  ·  saves: %s\\user\\saves  ·  logs: %s\\user\\logs"), pd, pd, pd, pd); else   /* 6.1 */
          if (GOW3G(g) || BBG(g)) _snwprintf(g_note[1], 900, L2(L"Port: %s  ·  %s  ·  saves, cache e logs: %s\\user", L"Port: %s  ·  %s  ·  saves, cache and logs: %s\\user"), pd, ver, pd); else
          if (G[g].ps5) _snwprintf(g_note[1], 900, L"Port: %s  ·  %s", pd, ver); else
          if (X360G(g)) _snwprintf(g_note[1], 900, L2(L"Port: %s  ·  %s  ·  configurações: %%LOCALAPPDATA%%\\UFC3MONS-P11-Camisas  ·  logs: ufc3.log e RELATORIO.md na pasta do jogo", L"Port: %s  ·  %s  ·  settings: %%LOCALAPPDATA%%\\UFC3MONS-P11-Camisas  ·  logs: ufc3.log and RELATORIO.md in the game folder"), pd, ver); else   /* 4.0.2: UFC 5 keeps everything in its folder */
          _snwprintf(g_note[1], 900, L2(L"Port: %s  ·  %s%s  ·  dados: %%LOCALAPPDATA%%\\%s", L"Port: %s  ·  %s%s  ·  data: %%LOCALAPPDATA%%\\%s"), pd, g_ver[g][0] ? L2(L"versão ", L"version ") : L"", ver, G[g].data);
          n = AddI(it, n, IT_NOTE, NULL, NULL, g_note[1], 0, 3); }
        break;
    case TAB_VIDEO:
        /* 5.1: UFC Undisputed 3's video lives in its own port (Configure the port); PORTHEON's presets wrote nothing for it */
        if (ZELDAG(g)) {   /* 6.0: the emulator's own switches (env EDEN_NX2_*, they win over its nx2_switches.txt) */
            n = AddI(it, n, IT_DROP, &FZ_ENGINE, NULL, NULL, 0, 2); n = AddI(it, n, IT_DROP, &FZ_FOG, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_SWITCH, &FZ_FULL, NULL, NULL, 0, 1); n = AddI(it, n, IT_SWITCH, &FZ_FPS30, NULL, NULL, 0, 2);
            n = AddI(it, n, IT_SWITCH, &FZ_PRELOAD, NULL, NULL, 0, 2); n = AddI(it, n, IT_SWITCH, &FZ_AMDGFX, NULL, NULL, 0, 2); n = AddI(it, n, IT_SWITCH, &FZ_CRUMBS, NULL, NULL, 0, 2);
            n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"O Lichii MDias v4 já vem com o Dynamic FPS (o jogo anda no tempo certo quando o FPS cai), o modo de resolução máxima e as correções para placas AMD. A neblina, a compilação segura para AMD e o diagnóstico só valem na MDias v4 (a Frizti v3 não tem essas opções). Resolução, filtros, DLSS, geração de quadros e controles ficam no próprio emulador (Arquivos do jogo > Abrir o emulador). Jogando nativo (Ocarina Recomp), as opções ficam no totk.ini da pasta do jogo nativo.",
                                                    L"Lichii MDias v4 comes with Dynamic FPS (the game keeps real time when the FPS drops), the max-resolution mode and the AMD fixes. The fog, AMD safe compiling and the diagnostics only work on MDias v4 (Frizti v3 does not have them). Resolution, filters, DLSS, frame generation and controls are in the emulator itself (Game files > Open the emulator). Playing natively (Ocarina Recomp), the options are in the native game folder's totk.ini."), 0, 3);
            break;
        }
        if (NGPORTG(g)) {   /* 6.1: Bloodborne, inFAMOUS: everything is in the port's own launcher */
            if (FNCG(g) && PortOk(g)) { /* 6.2: no launcher of its own: the settings are in the game */ } else if (PortOk(g)) n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Abrir o launcher do port (vídeo, controles e mods)", L"Open the port's launcher (video, controls and mods)"), H_EXTCFG, 3);
            else n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Instalar o port", L"Install the port"), H_EXTINST, 3);
            n = AddI(it, n, IT_NOTE, NULL, NULL, NgVideoNote(g), 0, 3);
            break;
        }
        if (GOW3G(g)) {   /* 6.0: the port's own launcher has video, controls and mods */
            n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Abrir o launcher do port (vídeo, resolução, FSR e controles)", L"Open the port's launcher (video, resolution, FSR and controls)"), H_EXTCFG, 3);
            n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"O God of War III tem resolução, FSR, FPS e controles no launcher do próprio port (botão acima). Na primeira vez ele prepara o jogo a partir do seu eboot.bin e monta os shaders: demora alguns minutos.",
                                                    L"God of War III has its resolution, FSR, FPS and controls in the port's own launcher (button above). The first time it prepares the game from your eboot.bin and builds its shaders: it takes a few minutes."), 0, 3);
            break;
        }
        if (X360G(g)) { n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Configurar o port (vídeo, resolução e FSR)", L"Configure the port (video, resolution and FSR)"), H_PORTCFG, 3);
                        n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"O UFC Undisputed 3 tem resolução, FSR 1, FPS e VSync no próprio port: use o botão acima. Os presets do PORTHEON não valem para ele. A tela cheia do port (\"Abrir o jogo em tela cheia\") já é sem bordas: não troca a resolução do monitor.",
                                                                L"UFC Undisputed 3 has its resolution, FSR 1, FPS and VSync in its own port: use the button above. PORTHEON's presets do not apply to it. The port's full screen (\"Open the game fullscreen\") is already borderless: it does not change the monitor's resolution."), 0, 3); break; }
        if (UFC6G(g) && GetInt(g, L"render_scale", -1) < 0) { int gp = GetInt(g, L"pgame:gfx_preset", 0); if (gp == 3 || gp == 4) PutInt(g, L"pgame:gfx_preset", 5); }   /* 5.1 */
        n = AddI(it, n, IT_DROP, &F_GFX, NULL, NULL, 0, 2);   /* 4.3.3: graphics presets, every game */
        if (!X360G(g)) n = AddI(it, n, IT_DROP, G[g].ps5 ? &F_FSR5 : &F_FSR, NULL, NULL, 0, 1);   /* 5.0: FSR 1 (PORTHEON's layer) */
        if (!X360G(g) && GetInt(g, L"pgame:fsr", 0) > 0)
            n = AddI(it, n, IT_NOTE, NULL, NULL, G[g].ps5 ?
                L2(L"FSR 1 da AMD: o PORTHEON passa a nitidez do FSR 1 (RCAS) em cada quadro, deixando a imagem mais definida, sem custo de FPS que se perceba (medido no UFC 6: luta a 50-55 FPS com ela ligada). Funciona em placas AMD, NVIDIA e Intel. Para mais FPS no UFC 6, use também a Resolução 3D em 0,75x. Vale no próximo Jogar.",
                   L"AMD FSR 1: PORTHEON applies FSR 1's sharpening (RCAS) to every frame, for a crisper picture at no noticeable FPS cost (measured on UFC 6: fights at 50-55 FPS with it on). Works on AMD, NVIDIA and Intel cards. For more FPS on UFC 6, also use the 3D resolution at 0.75x. Applies at the next Play.") :
                L2(L"FSR 1 da AMD: o jogo desenha numa resolução menor e o PORTHEON amplia para a sua tela com o FSR 1 e deixa a imagem nítida. Funciona em placas AMD, NVIDIA e Intel. Quanto menor a porcentagem, mais FPS e menos detalhe. Se o jogo não aceitar desenhar menor, fica só a nitidez. Vale no próximo Jogar.",
                   L"AMD FSR 1: the game draws at a lower resolution and PORTHEON enlarges it to your screen with FSR 1 and sharpens it. Works on AMD, NVIDIA and Intel cards. The lower the percentage, the more FPS and the less detail. If the game does not accept drawing smaller, only the sharpening stays. Applies at the next Play."), 0, 3);
        if (UFC5G(g)) {   /* 4.0.2: UFC 5 (4.3: back, as in 4.0.6) */
            n = AddI(it, n, IT_DROP, &F_TEX, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F_FXQ, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F_HZ, NULL, NULL, 0, 1);   /* 6.2 */
            n = AddI(it, n, IT_DROP, &F5_RES, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F5_RSCALE, NULL, NULL, 0, 1);   /* 6.0 (the controller's vibration went to Gameplay) */
            n = AddI(it, n, IT_DROP, &F5_PRES, NULL, NULL, 0, 1);
            /* 5.1: the dropdowns share rows and the switches pair up, so every row stays inside the panel */
            n = AddI(it, n, IT_DROP, &F5_FPS, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F5_SPEED, NULL, NULL, 0, 1);   /* 4.3.4; 5.0: UFC5.exe 1.29.0 */
            n = AddI(it, n, IT_DROP, &F5_OVL, NULL, NULL, 0, 1);          /* 5.1: as UFC 6's options (UFC5.exe 1.30.0) */
            n = AddI(it, n, IT_SWITCH, &F5_FULL, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_SWITCH, &F6_REALTIME, NULL, NULL, 0, 2);   /* 4.3.1: UFC5.exe 1.17.0; two columns, so its name fits */
            n = AddI(it, n, IT_SWITCH, &F5_FXAA, NULL, NULL, 0, 2);       /* 4.3.2: UFC5.exe 1.20.0 */
            n = AddI(it, n, IT_SWITCH, &F5_SHADOWS, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_SWITCH, &F5_FSR2, NULL, NULL, 0, 2);
            n = AddI(it, n, IT_SWITCH, &F5_SEATS, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"O desempenho do UFC 5 fica na configuração que mais deu FPS neste motor, escolhida para o seu PC quando o PORTHEON abre. A resolução quase não muda o FPS: o limite é o processador.",
                                                    L"UFC 5's performance stays on the setup that gave this engine the most FPS, picked for your PC when PORTHEON opens. Resolution barely changes the FPS: the processor is the limit."), 0, 3);
            n = AddI(it, n, IT_NOTE, NULL, NULL, RealtimeNote5(), 0, 3);
            break;
        } else if (G[g].ps5) {   /* 4.2: UFC 6; 4.3: VSync (present mode) can be chosen */
            n = AddI(it, n, IT_DROP, &F6_RSCALE, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F6_RES, NULL, NULL, 0, 1);   /* 5.1: the 3D size replaces Fast/Quality and 0.75x */
            n = AddI(it, n, IT_DROP, &F6_PRES, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_DROP, &F_TEX, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F_FXQ, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F_HZ, NULL, NULL, 0, 1);   /* 6.2 */
            n = AddI(it, n, IT_SWITCH, &F6_HDR, NULL, NULL, 0, 1);   /* 6.2: UFC6.exe 1.24.0 */
            n = AddI(it, n, IT_SWITCH, &F6_BLUR, NULL, NULL, 0, 2);   /* Released: experimental game patch (motion_blur_off); fills HDR's row */
            /* 7.1: the game's own "off" paths for its film grain, its film effects and its ray tracing (UFC6.exe patches
               them: film_grain_off, film_fx_off, rt_off), experimental; the long one takes two columns */
            n = AddI(it, n, IT_SWITCH, &F6_GRAIN, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_SWITCH, &F6_FILMFX, NULL, NULL, 0, 2);
            /* 7.1 game test (2026-11-15): the "no ray tracing" switch (F6_RTOFF, rt_off) is not shown: with it the game
               closed when the main menu loaded, on both engines, and UFC6.exe 1.29.0 no longer applies it */
            {   /* Released: only where the new engine's folder is in this install */
                wchar_t ne[MAX_PATH]; _snwprintf(ne, MAX_PATH, L"%s\\games\\UFC6\\novo\\UFC 6 novo.exe", g_exeDir); ne[MAX_PATH - 1] = 0;
                if (FileExists(ne)) {   /* 11-13: the Pro mode switch; 7.1: the upscaler list and its presets while it is on */
                    n = AddI(it, n, IT_SWITCH, &F6_PROSW, NULL, NULL, 0, 3);   /* a row of its own (the ray tracing switch that shared it is gone) */
                    BOOL fsrHave = Ufc6FsrHave(), pro = GetInt(g, L"pro6", 1) != 0;
                    int ups = GetInt(g, L"ups6", 0);
                    /* 7.1 review (player#6): the compatibility engine runs neither the Pro mode nor an upscaler (UFC6.exe's
                       ChooseEngine: the new engine needs !compat). compat_engine in UFC6.ini, or a card without
                       barycentrics, which the new engine needs too (checks.c's probe, read only once it is complete) */
                    BOOL noBary = g_vkcReady && g_vkc.device && g_vkc.name[0] && !g_vkc.bary;
                    BOOL compat = noBary || GetInt(g, L"compat_engine", 0) != 0;
                    /* 7.1 review (player#2): UFC6.exe's own RTX test (vendor 0x10de and "RTX" in the card's name) */
                    BOOL rtx = g_haveGpu && g_gpu.vendor == 0x10de && wcsstr(g_gpu.name, L"RTX") != NULL;
                    if (pro) {
                        /* the AMD row only with AMD's files in novo\fsr (or while it is the choice, so the list shows it) */
                        n = AddI(it, n, IT_DROP, fsrHave || ups == 2 ? &F6_UPS : &F6_UPSN, NULL, NULL, 0, 2);
                        /* 7.1 review (correctness#2): the preset list only for a chosen upscaler; Automatic plays its own
                           (Performance), so a list there would show a preset that does not play */
                        if (ups == 1 || ups == 2) n = AddI(it, n, IT_DROP, ups == 2 ? &F6_UPSQF : &F6_DLSSQ, NULL, NULL, 0, 1);
                        if (compat)
                            n = AddI(it, n, IT_WARN, NULL, NULL, noBary ?
                                L2(L"Esta placa não tem um recurso de vídeo que o motor novo do UFC 6 precisa (baricêntricas), então o UFC 6 roda nela no motor de compatibilidade: o modo PS5 Pro e os upscalers (DLSS e FSR) não rodam nesta placa.",
                                   L"This card lacks a graphics feature UFC 6's new engine needs (barycentrics), so UFC 6 runs on it on the compatibility engine: the PS5 Pro mode and the upscalers (DLSS and FSR) do not run on this card.") :
                                L2(L"Neste PC o UFC 6 está no motor de compatibilidade, que não tem o modo PS5 Pro nem os upscalers (DLSS e FSR): enquanto ele estiver em uso, o jogo abre sem eles.",
                                   L"On this PC UFC 6 is on the compatibility engine, which has neither the PS5 Pro mode nor the upscalers (DLSS and FSR): while it is in use, the game opens without them."), 0, 3);
                        else if (ups == 2 && !fsrHave)
                            n = AddI(it, n, IT_WARN, NULL, NULL, L2(L"A pasta games\\UFC6\\novo\\fsr não tem os arquivos do AMD FSR (amd_fidelityfx_loader_vk.dll). Com o AMD FSR escolhido, o jogo abre no motor de sempre, sem o modo Pro e sem upscaler. Escolha outro upscaler ou extraia o PORTHEON inteiro de novo.",
                                                                    L"The games\\UFC6\\novo\\fsr folder does not have the AMD FSR files (amd_fidelityfx_loader_vk.dll). With AMD FSR chosen, the game opens on the usual engine, without the Pro mode and without an upscaler. Pick another upscaler or extract the whole PORTHEON again."), 0, 3);
                        /* 7.1 review (player#2): Automatic and NVIDIA DLSS are DLSS for UFC6.exe; on a card that is not an RTX
                           it rules DLSS out and opens the usual engine, without the Pro mode (the card unknown: nothing said) */
                        else if (g_haveGpu && !rtx && (ups == 0 || ups == 1))
                            n = AddI(it, n, IT_WARN, NULL, NULL, fsrHave ?
                                L2(L"Esta placa não é uma NVIDIA RTX: com Automático ou NVIDIA DLSS o jogo abre no motor de sempre, sem o modo PS5 Pro. Para ter o modo Pro nesta placa, escolha AMD FSR ou Nenhum.",
                                   L"This card is not an NVIDIA RTX: with Automatic or NVIDIA DLSS the game opens on the usual engine, without the PS5 Pro mode. For the Pro mode on this card, pick AMD FSR or None.") :
                                L2(L"Esta placa não é uma NVIDIA RTX: com Automático ou NVIDIA DLSS o jogo abre no motor de sempre, sem o modo PS5 Pro. Para ter o modo Pro nesta placa, escolha Nenhum.",
                                   L"This card is not an NVIDIA RTX: with Automatic or NVIDIA DLSS the game opens on the usual engine, without the PS5 Pro mode. For the Pro mode on this card, pick None."), 0, 3);
                    }
                    /* 7.1 (critic 4.11): the players' game code is a backport in which the game's own PS5 Pro check
                       always answers "no" (fg Check), so the Pro mode is the engine's, not the game's Pro profile */
                    n = AddI(it, n, IT_NOTE, NULL, NULL, fsrHave ?
                        L2(L"Modo PS5 Pro: o motor novo se apresenta ao jogo como um PS5 Pro e liga os ajustes de Pro do próprio motor; o jogo em si continua com os gráficos do PS5 comum (esta versão do jogo não usa o perfil do Pro). Num teste deu alguns FPS a mais. Com um upscaler escolhido, o PORTHEON faz o jogo pedir a ampliação da imagem nesse modo, e o upscaler monta a imagem de 1080p a partir de uma menor. NVIDIA DLSS: placas RTX (nas outras placas, Automático e NVIDIA DLSS abrem o motor de sempre, sem o modo Pro). AMD FSR: FSR 4 nas Radeon RX 9000 com driver atual, FSR 3.1 nas outras placas que rodam o motor novo (nas RX 6000 e 7000 o FSR 4 pesa demais) (as GTX 10 e as RX 5000 não rodam); os arquivos do Visual C++ 2015-2022 da Microsoft que ele usa já vêm com o PORTHEON. Só no jogo 01.005/01.006; fora disso o PORTHEON usa o motor de sempre. Se o modo Pro ou o upscaler fecharem o jogo logo no começo, o PORTHEON pode tentar de novo uma vez e depois segue no motor de sempre, com um aviso.",
                           L"PS5 Pro mode: the new engine presents itself to the game as a PS5 Pro and turns on the engine's own Pro settings; the game itself keeps the regular PS5's graphics (this version of the game does not use its Pro profile). In a test it gave a few more FPS. With an upscaler chosen, PORTHEON makes the game ask for its picture to be enlarged in this mode, and the upscaler builds the 1080p picture from a smaller one. NVIDIA DLSS: RTX cards (on other cards, Automatic and NVIDIA DLSS open the usual engine, without the Pro mode). AMD FSR: FSR 4 on Radeon RX 9000 with a current driver, FSR 3.1 on the other cards that run the new engine (on RX 6000 and 7000 FSR 4 is far too heavy) (GTX 10 and RX 5000 cards do not); the Microsoft Visual C++ 2015-2022 files it uses come with PORTHEON. Game 01.005/01.006 only; otherwise PORTHEON plays the usual engine. If the Pro mode or the upscaler closes the game right at the start, PORTHEON may try once more and then goes on with the usual engine, with a notice.") :
                        L2(L"Modo PS5 Pro: o motor novo se apresenta ao jogo como um PS5 Pro e liga os ajustes de Pro do próprio motor; o jogo em si continua com os gráficos do PS5 comum (esta versão do jogo não usa o perfil do Pro). Num teste deu alguns FPS a mais. Com um upscaler escolhido, o PORTHEON faz o jogo pedir a ampliação da imagem nesse modo, e o upscaler monta a imagem de 1080p a partir de uma menor. NVIDIA DLSS: placas RTX (nas outras placas, Automático e NVIDIA DLSS abrem o motor de sempre, sem o modo Pro). Só no jogo 01.005/01.006; fora disso o PORTHEON usa o motor de sempre. Se o modo Pro ou o upscaler fecharem o jogo logo no começo, o PORTHEON pode tentar de novo uma vez e depois segue no motor de sempre, com um aviso.",
                           L"PS5 Pro mode: the new engine presents itself to the game as a PS5 Pro and turns on the engine's own Pro settings; the game itself keeps the regular PS5's graphics (this version of the game does not use its Pro profile). In a test it gave a few more FPS. With an upscaler chosen, PORTHEON makes the game ask for its picture to be enlarged in this mode, and the upscaler builds the 1080p picture from a smaller one. NVIDIA DLSS: RTX cards (on other cards, Automatic and NVIDIA DLSS open the usual engine, without the Pro mode). Game 01.005/01.006 only; otherwise PORTHEON plays the usual engine. If the Pro mode or the upscaler closes the game right at the start, PORTHEON may try once more and then goes on with the usual engine, with a notice."), 0, 3);
                }
            }
            n = AddI(it, n, IT_DROP, &F6_FPS, NULL, NULL, 0, 1);   /* 4.3.4: UFC6.exe 1.8.0 */
            n = AddI(it, n, IT_DROP, &F6_SHADOWS, NULL, NULL, 0, 1);   /* Released: experimental game patch (shadow_res); in the FPS limit's row */
            /* 4.3.2: each two-column switch shares its row with a one-column one, so the notes stay inside the panel in
               every language (German at 150% ran past it with one row per switch) */
            n = AddI(it, n, IT_SWITCH, &F6_REALTIME, NULL, NULL, 0, 2);   /* 4.3; 4.3.1: two columns, so its name fits */
            n = AddI(it, n, IT_SWITCH, &F6_FULL, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_SWITCH, &F6_FXAA, NULL, NULL, 0, 2);       /* 4.3.2 */
            n = AddI(it, n, IT_SWITCH, &F6_OVERLAY, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_SWITCH, &F6_DOF, NULL, NULL, 0, 2);        /* 4.3.2 */
            n = AddI(it, n, IT_SWITCH, &F6_WIDE21, NULL, NULL, 0, 1);     /* Released: experimental game patch (wide21); in the depth of field's row */
            n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"O UFC 6 roda no motor ajustado para placas de 8 GB. A Resolução 3D escolhe o tamanho da imagem 3D: 50% (960x540) dá mais FPS, 100% é 1920x1080, 2K e 4K desenham maior e pedem placas de 8 e 12 GB. Automático segue o perfil. Sem VSync, a não ser que você escolha.",
                                                    L"UFC 6 runs on the engine tuned for 8 GB cards. 3D resolution picks the 3D picture's size: 50% (960x540) gives more FPS, 100% is 1920x1080, 2K and 4K draw bigger and need 8 and 12 GB cards. Automatic follows the profile. No VSync unless you pick it."), 0, 3);
            n = AddI(it, n, IT_NOTE, NULL, NULL, RealtimeNote(), 0, 3);
            /* 7.2 (reports of 11-15/16): every RX 6000 log (6600 to 6950 XT, both UFC 6 engines) plays the menus at about 110 FPS
               and the fights at 1-15 FPS, the card busy 50-60 ms a frame on the game's compute effects; RX 7000 and 9000 cards hold
               25-60. Not solved: the player is told before PLAY, not after a long wait */
            if (g_haveGpu && g_gpu.vendor == 0x1002 && wcsstr(g_gpu.name, L"RX 6")) n = AddI(it, n, IT_WARN, NULL, NULL, L2(L"Radeon RX 6000: nos relatórios, todas as RX 6000 (da 6600 à 6950 XT) rodam os menus do UFC 6 a uns 110 FPS, mas as lutas caem para 1 a 15 FPS, nos dois motores. É um limite do motor com essas placas, ainda sem solução; as RX 7000 e RX 9000 ficam entre 25 e 60 FPS. O AMD FSR não resolve: a placa já fica ocupada antes dele.", L"Radeon RX 6000: in the reports, every RX 6000 (6600 to 6950 XT) runs UFC 6's menus at about 110 FPS, but fights drop to 1-15 FPS, on both engines. It is a limit of the engine with these cards, not solved yet; RX 7000 and RX 9000 cards hold 25-60 FPS. AMD FSR does not help: the card is busy before it."), 0, 3);
            break;
        } else {
        static const Field *v4[6] = {&F4_RES, &F4_WIN, NULL, &F4_PRES, &F4_UPS, &F4_SHP};
        for (int i = 0; i < 6; i++) n = AddI(it, n, IT_DROP, i == 2 ? (FpsFree(g) ? &F4_FPS3 : &F4_FPS4) : v4[i], NULL, NULL, 0, 1);
        n = AddI(it, n, IT_DROP, &F_TUPS, NULL, NULL, 0, 1);   /* Released: FSR 3.1 / FSR 4 / TAA */
        n = AddI(it, n, IT_DROP, &F_TEX, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F_FXQ, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F_HZ, NULL, NULL, 0, 1);   /* 6.2 */
        if (g >= 1) n = AddI(it, n, IT_DROP, &F4P_SCENE, NULL, NULL, 0, 1);   /* 6.0 final: UFC 2-4's 3D scene at 720p/540p/450p/360p */
        if (g <= 2) n = AddI(it, n, IT_DROP, &F4_IRES, NULL, NULL, 0, 1);   /* Released: Native Plus's small internal sizes, UFC 1-3 (experimental) */
        n = AddI(it, n, IT_SWITCH, &F4_FULL, NULL, NULL, 0, 1);
        if (g != 0) n = AddI(it, n, IT_SWITCH, &F4_NEO, NULL, NULL, 0, 1);
        n = AddI(it, n, IT_SWITCH, &F4_ULTRA, NULL, NULL, 0, 1);
        n = AddI(it, n, IT_SWITCH, &F4_HDR, NULL, NULL, 0, 1);   /* 6.2 (3440x1440 is in the Resolution list now) */
        n = AddI(it, n, IT_SWITCH, &F4_WIDE21, NULL, NULL, 0, 1);   /* Released: real 21:9 */
        n = AddI(it, n, IT_SWITCH, &F_LEARN, NULL, NULL, 0, 1);   /* 6.2 */
        int fpsKind = FpsExperimental(g, GetInt(g, L"fps_limit", 60)) ? IT_WARN : IT_NOTE;
        if (g == 0) n = AddI(it, n, fpsKind, NULL, NULL, L2(L"UFC 1: as lutas rodam no ritmo do próprio jogo (30 quadros por segundo de jogo, como no PS4), com o menu a 60. O jogo conta o tempo da luta por quadro: acima disso a luta ficaria acelerada. Acima de 60 é experimental.",
                                                             L"UFC 1: fights run at the game's own pace (30 game frames per second, as on the PS4), menus at 60. The game counts fight time per frame: faster would speed the fight up. Above 60 is experimental."), 0, 3);
        else if (g == 1) n = AddI(it, n, fpsKind, NULL, NULL, L2(L"UFC 2: as lutas rodam em qualquer FPS (medido em 16/11: o relógio do round anda no tempo certo com a luta a 90 FPS); 240 e sem limite são experimentais (em taxas muito altas a placa de vídeo pode travar, código 23: o PORTHEON volta para 60 sozinho). Num monitor de 144 Hz, 144 FPS fica mais liso que 60.",
                                                                 L"UFC 2: fights run at any rate (measured 11-16: the round clock keeps real time with the fight at 90 FPS); 240 and unlimited are experimental (at very high rates the graphics card may stop, code 23: PORTHEON goes back to 60 by itself). On a 144 Hz monitor, 144 FPS is smoother than 60."), 0, 3);   /* 7.2 */
        else if (g == 2) n = AddI(it, n, fpsKind, NULL, NULL, L2(L"UFC 3: as lutas rodam em qualquer FPS; 240 e sem limite são experimentais (em taxas muito altas a placa de vídeo pode travar, código 23: o PORTHEON volta para 60 sozinho).",
                                                                 L"UFC 3: fights run at any rate; 240 and unlimited are experimental (at very high rates the graphics card may stop, code 23: PORTHEON goes back to 60 by itself)."), 0, 3);
        else n = AddI(it, n, fpsKind, NULL, NULL, L2(L"Acima de 60 FPS (90 a 240 e sem limite) é experimental: este jogo conta o tempo por quadro, então a luta pode acelerar e o jogo pode fechar com código 3 (o PORTHEON volta para 60 sozinho). O recomendado é 60.",
                                                     L"Above 60 FPS (90 to 240 and unlimited) is experimental: this game counts time per frame, so the fight may speed up and the game may close with code 3 (PORTHEON goes back to 60 by itself). 60 is recommended."), 0, 3);
        n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Upscaling: o motor dos ports tem o AMD FSR 1.0, que roda em qualquer placa (NVIDIA, AMD e Intel). DLSS não existe nestes ports: ele precisa de vetores de movimento do jogo que o motor não tem. Para mais FPS em PC fraco: \"Ultra desempenho\" ou 1280x720 com FSR.",
                                                L"Upscaling: the ports' engine has AMD FSR 1.0, which runs on any card (NVIDIA, AMD and Intel). DLSS does not exist in these ports: it needs motion vectors from the game that the engine does not have. For more FPS on a weak PC: \"Ultra performance\" or 1280x720 with FSR."), 0, 3);
        if (GpuIsIntel()) n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Placa Intel: a cada JOGAR ficam desligados o modo PS4 Pro, 2K/4K e mais de 60 FPS (travavam a placa, código 23), com VSync.",
                                                               L"Intel graphics: every PLAY turns off PS4 Pro mode, 2K/4K and more than 60 FPS (they stopped the GPU, code 23), with VSync."), 0, 3);
        break; }
    case TAB_GAMEPLAY:
        if (UFC5G(g)) {   /* 4.0.2: UFC 5 (4.3: back, as in 4.0.6) */
            n = AddI(it, n, IT_DROP, &F5_LANG, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F5_SPK, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_DROP, &F5_VIB, NULL, NULL, 0, 1);   /* 6.0: from Video, where the 3D resolution took its place */
            n = AddI(it, n, IT_EDIT, NULL, g_editPlayer, L2(L"Nome no jogo (o do seu perfil)", L"Name in the game (your profile's)"), 0, 1);
            if (CAM3P_UFC5) n = AddI(it, n, IT_SWITCH, &F56_CAM3P, NULL, NULL, 0, 3);   /* 4.3.2: UFC5.exe 1.21.0 */
            n = AddI(it, n, IT_FBUTTON, NULL, NULL, KeysCustom(g) ? L2(L"Teclado: suas teclas (mudar)", L"Keyboard: your keys (change)") : L2(L"Teclado: configurar as teclas", L"Keyboard: set up the keys"), H_KEYS, 1);   /* 6.0 */
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Controle: mudar os botões", L"Controller: change the buttons"), H_PADMAP, 1);   /* 6.2 */
            n = AddI(it, n, IT_NOTE, NULL, NULL, KeysCustom(g) ? L2(L"Controles de PS5, PS4 e Xbox funcionam direto. O teclado usa as teclas que você escolheu (botão acima).", L"PS5, PS4 and Xbox controllers work as they are. The keyboard uses the keys you picked (button above).")
                                                               : L2(L"Controles de PS5, PS4 e Xbox funcionam direto. No teclado: J = X, L = O, K = Quadrado, I = Triângulo, Enter = Options, setas = direcional, WASD = analógico esquerdo. Para mudar, use o botão acima.",
                                                    L"PS5, PS4 and Xbox controllers work as they are. Keyboard: J = X, L = O, K = Square, I = Triangle, Enter = Options, arrows = d-pad, WASD = left stick. To change them, use the button above."), 0, 3);
            if (CAM3P_UFC5) n = AddI(it, n, IT_NOTE, NULL, NULL, Cam3pNote(g), 0, 3);
            break;
        }
        if (G[g].ps5) {   /* 4.2: UFC 6 */
            n = AddI(it, n, IT_DROP, &F6_LANG, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_EDIT, NULL, g_editPlayer, L2(L"Nome no jogo (o do seu perfil)", L"Name in the game (your profile's)"), 0, 1);
            n = AddI(it, n, IT_DROP, &F6_VOL, NULL, NULL, 0, 1);   /* Released: the engine's master volume (UFC6.exe: --audio-master-volume) */
            n = AddI(it, n, IT_SWITCH, &F6_LOWLAT, NULL, NULL, 0, 2);     /* 4.3.2: the controller's lag, next to UFC 1-4's own switch */
            n = AddI(it, n, IT_SWITCH, &F6_ACTIONS, NULL, NULL, 0, 1);    /* 6.2: UFC6.exe 1.23.0 */
            if (CAM3P_UFC6) n = AddI(it, n, IT_SWITCH, &F56_CAM3P, NULL, NULL, 0, 3);     /* 4.3.2: UFC6.exe 1.6.0 */
            n = AddI(it, n, IT_SWITCH, &F6_UNLOCK, NULL, NULL, 0, 3);   /* 4.3.4: UFC6.exe 1.8.0 (Extras had no room) */
            n = AddI(it, n, IT_SWITCH, &F6_NODIL, NULL, NULL, 0, 3);    /* 4.3.5: UFC6.exe 1.9.0 */
            n = AddI(it, n, IT_SWITCH, &F6_PARTICLES, NULL, NULL, 0, 1);   /* 6.0 final: UFC6.exe 1.21.0, the three in one row */
            n = AddI(it, n, IT_SWITCH, &F6_INFSTAM, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_SWITCH, &F6_NODMG, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_FBUTTON, NULL, NULL, KeysCustom(g) ? L2(L"Teclado: suas teclas (mudar)", L"Keyboard: your keys (change)") : L2(L"Teclado: configurar as teclas", L"Keyboard: set up the keys"), H_KEYS, 1);   /* 6.0 */
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Controle: mudar os botões", L"Controller: change the buttons"), H_PADMAP, 1);   /* 6.2 */
            n = AddI(it, n, IT_NOTE, NULL, NULL, KeysCustom(g) ? L2(L"Controles de PS5, PS4 e Xbox funcionam direto. O teclado usa as teclas que você escolheu (botão acima).", L"PS5, PS4 and Xbox controllers work as they are. The keyboard uses the keys you picked (button above).")
                                                               : L2(L"Controles de PS5, PS4 e Xbox funcionam direto. Teclado: J = X, L = O, K = Quadrado, I = Triângulo, WASD = analógico.",
                                                    L"PS5, PS4 and Xbox controllers work as they are. Keyboard: J = X, L = O, K = Square, I = Triangle, WASD = stick."), 0, 3);   /* 6.0 final: one line (the cheat switches took a row) */
            if (CAM3P_UFC6) n = AddI(it, n, IT_NOTE, NULL, NULL, Cam3pNote(g), 0, 3);
            break;
        }
        n = AddI(it, n, IT_DROP, &F4_LANG, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F4_VOL, NULL, NULL, 0, 1);
        n = AddI(it, n, IT_EDIT, NULL, g_editPlayer, L2(L"Nome no jogo (o do seu perfil)", L"Name in the game (your profile's)"), 0, 1);
        n = AddI(it, n, IT_DROP, &F4_PADTYPE, NULL, NULL, 0, 1); n = AddI(it, n, IT_PADPICK, NULL, NULL, NULL, 0, 2);
        if (g <= 2) {   /* Released: Player 2's controller and Player 2's own buttons (UFC 1-3) */
            n = AddI(it, n, IT_PADPICK2, NULL, NULL, NULL, 0, 2);
            n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Jogador 2: mudar os botões", L"Player 2: change the buttons"), H_PADMAP2, 1);
        }
        /* 11-13: "Controle com a janela fora de foco (Parsec)" takes two columns (one column cut it with "..."); the camera shares the next row */
        n = AddI(it, n, IT_SWITCH, &F4_SKIP, NULL, NULL, 0, 1); n = AddI(it, n, IT_SWITCH, &F4_PADBG, NULL, NULL, 0, 2); n = AddI(it, n, IT_SWITCH, &F4_LOWLAT, NULL, NULL, 0, 1);
        if (CAM3P_XML[g]) n = AddI(it, n, IT_SWITCH, &F4_CAM3P, NULL, NULL, 0, 2);   /* 4.3.2 (WriteCam3p) */
        if (g == 1 || g == 3) n = AddI(it, n, IT_SWITCH, &F4_CAMFP, NULL, NULL, 0, 1);   /* 6.2 */
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, KeysCustom(g) ? L2(L"Teclado: suas teclas (mudar)", L"Keyboard: your keys (change)") : L2(L"Teclado: configurar as teclas", L"Keyboard: set up the keys"), H_KEYS, 1);   /* 6.0 */
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Controle: mudar os botões", L"Controller: change the buttons"), H_PADMAP, 1);   /* 6.2 */
        n = AddI(it, n, IT_NOTE, NULL, NULL, KeysCustom(g) ? L2(L"Controles: PS5, PS4, PS3, Xbox Series/One, Xbox 360 e genéricos. \"Tipo de controle\" liga o driver certo e, nos de PlayStation, ignora o Xbox virtual do DS4Windows/Steam. Dois controles são dois jogadores. Insert (ou L3+R3) abre o menu do port. O teclado usa as teclas que você escolheu (botão acima).",
                                                                L"Controllers: PS5, PS4, PS3, Xbox Series/One, Xbox 360 and generic. \"Controller type\" turns on the right driver and, for PlayStation pads, ignores DS4Windows/Steam virtual Xbox pads. Two controllers are two players. Insert (or L3+R3) opens the port's menu. The keyboard uses the keys you picked (button above).")
                                                           : L2(L"Controles: PS5, PS4, PS3, Xbox Series/One, Xbox 360 e genéricos. \"Tipo de controle\" liga o driver certo e, nos de PlayStation, ignora o Xbox virtual do DS4Windows/Steam. Dois controles são dois jogadores. Insert (ou L3+R3) abre o menu do port. No teclado: Espaço = X, Shift = O, E = Quadrado, Q = Triângulo, Enter = Options, WASD = analógico; para mudar, use o botão acima.",
                                                L"Controllers: PS5, PS4, PS3, Xbox Series/One, Xbox 360 and generic. \"Controller type\" turns on the right driver and, for PlayStation pads, ignores DS4Windows/Steam virtual Xbox pads. Two controllers are two players. Insert (or L3+R3) opens the port's menu. Keyboard: Space = X, Shift = O, E = Square, Q = Triangle, Enter = Options, WASD = stick; to change them, use the button above."), 0, 3);
        n = AddI(it, n, GetInt(g, L"pini:pad_background", 0) && !GetInt(g, L"parsec", 0) ? IT_WARN : IT_NOTE, NULL, NULL,
                 L2(L"Enquanto joga, deixe a janela do jogo em foco (sem Alt+Tab e sem clicar em outro monitor): fora de foco o jogo pode fechar sozinho. \"Controle com a janela fora de foco\" é para o Parsec.",
                    L"While you play, keep the game's window in focus (no Alt+Tab, no clicks on another monitor): out of focus the game can close by itself. \"Controller works out of focus\" is for Parsec."), 0, 3);
        if (CAM3P_XML[g]) n = AddI(it, n, IT_NOTE, NULL, NULL, Cam3pNote(g), 0, 3);
        break;
    case TAB_ONLINE:
        n = OnlineItems(g, it, n);
        break;
    case TAB_EXTRAS:
        n = AddI(it, n, IT_SWITCH, &F_MFG, NULL, NULL, 0, 3);   /* 4.3.3: MFG SUPER MOTION, every game (its note: the switch's message) */
        if (UFC5G(g)) {   /* 4.0.2: UFC 5 (4.3: back, as in 4.0.6) */
            n = AddI(it, n, IT_DROP, &F5_CROWD, NULL, NULL, 0, 1);   /* 4.0.3 */
            n = AddI(it, n, IT_SWITCH, &F5_FASTSH, NULL, NULL, 0, 1); n = AddI(it, n, IT_SWITCH, &F5_PRIO, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_SWITCH, &F6_AUDIOWD, NULL, NULL, 0, 1);   /* 4.3: the same key in UFC5.ini (UFC5.exe 1.16.0) */
            n = AddI(it, n, IT_SWITCH, &F5_ECO, NULL, NULL, 0, 1); n = AddI(it, n, IT_SWITCH, &F5_DIAG, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Melhores configurações para este PC", L"Best settings for this PC"), H_BESTCFG, 1);
            n = AddI(it, n, IT_NOTE, NULL, NULL, AudioWdNote(), 0, 3);
            break;
        }
        if (G[g].ps5) {   /* 4.2: UFC 6; 4.3: the priority in three steps and the engine's AMD CPU compatibility */
            n = AddI(it, n, IT_DROP, &F6_CROWD, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_DROP, &F6_PRIO, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_DROP, &F6_CTHREADS, NULL, NULL, 0, 1);   /* Released: compile_threads, a key UFC6.exe already read */
            n = AddI(it, n, IT_SWITCH, &F6_AMDCPU, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_SWITCH, &F6_AUDIOWD, NULL, NULL, 0, 1);   /* 4.3 */
            n = AddI(it, n, IT_SWITCH, &F6_ECO, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_SWITCH, &F6_CROWDRED, NULL, NULL, 0, 2);   /* Released: experimental game patches (crowd_reduced, skip_logos), one row */
            n = AddI(it, n, IT_SWITCH, &F6_SKIPLOGOS, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Melhores configurações para este PC", L"Best settings for this PC"), H_BESTCFG, 1);
            n = AddI(it, n, IT_NOTE, NULL, NULL, AudioWdNote(), 0, 3);
            n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Prioridade Alta (máximo) põe o jogo na frente dos outros programas no processador; o ganho é pequeno e o Windows pode ficar lento enquanto o jogo roda. Compatibilidade de CPU AMD: o PS5 tem um processador AMD, e algumas contas saem um pouco diferentes num Intel; ligada, o motor corrige essas contas (mais fiel ao PS5, um pouco mais pesado num Intel). Num Ryzen ela vem ligada e não custa nada, porque as contas já são as mesmas. Nenhuma opção passa a luta de 60 FPS sozinha: o limite é a velocidade do processador.",
                                                    L"High (maximum) priority puts the game ahead of other programs on the processor; the gain is small and Windows can feel slow while the game runs. AMD CPU compatibility: the PS5 has an AMD processor, and some math comes out slightly different on an Intel; when on, the engine corrects that math (closer to the PS5, a little heavier on an Intel). On a Ryzen it comes on and costs nothing, because the math is already the same. No option takes a fight past 60 FPS on its own: the limit is the processor's speed."), 0, 3);
            n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Torcida desligada: o público some e o octógono, os lutadores e o juiz continuam (mais FPS na luta). O PORTHEON guarda a lista de shaders do jogo quando o driver da placa de vídeo é atualizado, para a primeira partida depois não travar. Nos Ryzen X3D de dois chips (7900X3D, 7950X3D, 9900X3D, 9950X3D) o jogo fica no chip com V-Cache; nos outros processadores ele usa todos os núcleos.",
                                                    L"Crowd off: the audience goes away and the octagon, the fighters and the referee stay (more FPS in a fight). PORTHEON keeps the game's shader list when the graphics driver is updated, so the first session afterwards does not stutter. On two-chip Ryzen X3D (7900X3D, 7950X3D, 9900X3D, 9950X3D) the game stays on the V-Cache chip; on other processors it uses every core."), 0, 3);
            break;
        }
        if ((g == 1 || g == 2) && PatchState(g, L"crowd", -1) >= 0) n = AddI(it, n, IT_DROP, &FP_CROWD, NULL, NULL, 0, 1);
        if (g <= 2 && PatchState(g, L"blur", -1) >= 0) n = AddI(it, n, IT_DROP, &FP_BLUR, NULL, NULL, 0, 1);   /* 4.0.4: UFC 1 and 2 too */
        if (g == 3) { n = AddI(it, n, IT_DROP, &F4P_CROWD, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F4P_BLUR, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F4P_SHADOW, NULL, NULL, 0, 1); }   /* 5.0: shadows */
        if (g == 2 || g == 3) n = AddI(it, n, IT_DROP, g == 3 ? &F4N_ENGINE : &F4_ENGINE, NULL, NULL, 0, 1);
        if (g == 1) n = AddI(it, n, IT_DROP, &F2_ENGINE, NULL, NULL, 0, 1);   /* 4.3.3: the v3.0 engine */
        n = AddI(it, n, IT_DROP, &F4_SKIN, NULL, NULL, 0, 1);
        if (g <= 2) {   /* Released (2026-11-13): UFC4 Native Plus's options for UFC 1-3; the game-code ones are not yet tested in a fight */
            n = AddI(it, n, IT_DROP, &F13_SHADOW, NULL, NULL, 0, 1); n = AddI(it, n, IT_DROP, &F4_PREP, NULL, NULL, 0, 1);
            n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Variáveis extras do motor (avançado)", L"Extra engine variables (advanced)"), H_ENGENV, 1);
            if (g == 0) n = AddI(it, n, IT_SWITCH, &F1_CROWDOFF, NULL, NULL, 0, 1);
            if (g == 2) n = AddI(it, n, IT_SWITCH, &F3_CROWD25, NULL, NULL, 0, 2);
            n = AddI(it, n, IT_SWITCH, &F4_STATS, NULL, NULL, 0, 1); n = AddI(it, n, IT_SWITCH, &F4_LOWEND, NULL, NULL, 0, 2);
        }
        n = AddI(it, n, IT_SWITCH, &F4_BOOST, NULL, NULL, 0, 1);
        if (g == 3) { n = AddI(it, n, IT_SWITCH, &F4_MOUTH, NULL, NULL, 0, 1); n = AddI(it, n, IT_SWITCH, &F4_CAREER, NULL, NULL, 0, 1); }
        n = AddI(it, n, IT_SWITCH, &F4_CPUALL, NULL, NULL, 0, 2);   /* 11-13: two columns, as the video safe mode below (one column cut both names with "...") */
        n = AddI(it, n, IT_SWITCH, &F4_ASYNC, NULL, NULL, 0, 1);   /* 4.0.1 */
        if (g != 0) n = AddI(it, n, IT_SWITCH, &F4_GPUSAFE, NULL, NULL, 0, 2);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Melhores configurações para este PC", L"Best settings for this PC"), H_BESTCFG, 1);
        if (g == 3) n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Torcida e motion blur: patches da comunidade (Lepexa). Motor novo: o SacredFire mais recente (no UFC 4, 11% mais FPS e menos travadinhas num Ryzen 5 5500 + RTX 4060); o automático usa ele em todas as placas e, se o jogo fechar logo no começo com ele, volta sozinho para o do port. Pele verde ou de cor errada? Teste as alternativas de \"Cor da pele\".",
                                                             L"Crowd and motion blur: community patches (Lepexa). New engine: the newest SacredFire (in UFC 4, 11% more FPS and fewer stutters on a Ryzen 5 5500 + RTX 4060); automatic uses it on every card and, if the game closes right at the start with it, goes back to the port's own by itself. Green or wrongly colored skin? Try the \"Skin color\" alternatives."), 0, 3);
        if (g == 2) n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Motor novo: o SacredFire mais recente, com as correções da Carreira do GNSB e mais FPS; o automático usa ele em todas as placas e, se o jogo fechar logo no começo com ele, volta sozinho para o do port. Pele preta/manchada no lutador criado? Teste as alternativas de \"Cor da pele\".",
                                                             L"New engine: the newest SacredFire, with GNSB's Career fixes and more FPS; automatic uses it on every card and, if the game closes right at the start with it, goes back to the port's own by itself. Black or blotchy skin on a created fighter? Try the \"Skin color\" alternatives."), 0, 3);
        if (g == 0) n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"O UFC 1 já roda no modo seguro de vídeo (gravação numa thread só), que acabou com os congelamentos da luta.",
                                                             L"UFC 1 already runs in the safe video mode (recording on one thread), which ended its fight freezes."), 0, 3);
        if (g <= 2) n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Opções vindas do motor do UFC 4 (UFC4 Native Plus, de Abdullah Tech). As marcadas como experimentais mudam o código do jogo e ainda não foram testadas numa luta: se a imagem ficar errada ou o jogo fechar, volte para o original. Estatísticas de quadros: o FPS vai para o log do jogo a cada 5 segundos. Perfil de PC fraco: só age em processador de até 4 threads (sem threads de preparo de desenho). Tudo vale no próximo JOGAR.",
                                                            L"Options from UFC 4's engine (UFC4 Native Plus, by Abdullah Tech). The ones marked experimental change the game's code and have not been tested in a fight yet: if the picture looks wrong or the game closes, go back to the original. Frame statistics: the FPS goes to the game's log every 5 seconds. Low-end PC profile: it only acts on a processor with up to 4 threads (no draw-preparation threads). Everything applies at the next PLAY."), 0, 3);
        break;
    }
    return n;
}
static int ItemHeight(HDC dc, const Item *it, int w)
{
    switch (it->type) {
    case IT_DROP: case IT_EDIT: case IT_FBUTTON: case IT_HOSTIP: case IT_PADPICK: case IT_PADPICK2: return SC(64);
    case IT_SWITCH: return SC(36);
    case IT_HEAD: return SC(30);
    default: { RECT r = {0, 0, w - SC(48), 0}; return Wrap(dc, g_fSmall, C_SUB, r, it->text, FALSE) + SC(18); }
    }
}
static const wchar_t *FieldName(const Field *f)   /* 5.0: fields named outside the string table */
{
    { const wchar_t *x = ExtFieldName(f); if (x) return x; }   /* 6.0: Zelda's and God of War III's (extgames.c) */
    if (f == &F5_SPEED) return L2(L"Velocidade da luta", L"Fight speed");
    if (f == &F_FSR) return L2(L"FSR 1 da AMD (amplia e deixa nítido)", L"AMD FSR 1 (upscale and sharpen)");
    if (f == &F_FSR5) return L2(L"FSR 1 da AMD (nitidez)", L"AMD FSR 1 (sharpening)");
    if (f == &F5_OVL) return L2(L"Contador de FPS", L"FPS counter");
    if (f == &F6_RSCALE) return L2(L"Resolução 3D (modos tipo FSR)", L"3D resolution (FSR-style modes)");
    if (f == &F5_RSCALE) return L2(L"Resolução 3D (PC fraco: 480p/360p)", L"3D resolution (weak PC: 480p/360p)");   /* 6.0 */
    if (f == &F4P_SCENE) return L2(L"Resolução 3D (PC fraco: 720p a 360p)", L"3D resolution (weak PC: 720p to 360p)");   /* 6.0 final: UFC 2-4 */
    if (f == &F6_PARTICLES) return L2(L"Sem faíscas, suor e fumaça", L"No sparks, sweat or smoke");   /* 6.0 final: UFC 6 (particle effects off) */
    if (f == &F6_INFSTAM) return L2(L"Stamina infinita (trapaça)", L"Infinite stamina (cheat)");      /* both fighters */
    if (f == &F6_NODMG) return L2(L"Sem dano, sem nocaute (trapaça)", L"No damage, no KO (cheat)");   /* both fighters */
    if (f == &F4_CAMFP) return L2(L"Com 1ª pessoa (olhos)", L"With 1st person (eyes)");
    if (f == &F4_WIDE21) return L2(L"21:9 real (campo de visão ultrawide)", L"Real 21:9 (ultrawide field of view)");
    if (f == &F_TUPS) return L2(L"Upscaler temporal (FSR 3 / FSR 4 / TAA)", L"Temporal upscaler (FSR 3 / FSR 4 / TAA)");
    if (f == &F_TEX) return L2(L"Qualidade das texturas", L"Texture quality");
    /* Released (2026-11-13): UFC4 Native Plus's options in UFC 1-3 and UFC 6 */
    if (f == &F4_IRES) return L2(L"Resolução interna extra (experimental)", L"Extra internal resolution (experimental)");
    if (f == &F13_SHADOW) return L2(L"Qualidade das sombras (experimental)", L"Shadow quality (experimental)");
    if (f == &F4_PREP) return L2(L"Threads de preparo de desenho", L"Draw-preparation threads");
    if (f == &F4_STATS) return L2(L"Estatísticas de quadros no log", L"Frame statistics in the log");
    if (f == &F4_LOWEND) return L2(L"Perfil de PC fraco (CPU de até 4 threads)", L"Low-end PC profile (CPU with up to 4 threads)");
    if (f == &F1_CROWDOFF) return L2(L"Torcida desligada (experimental)", L"Crowd off (experimental)");
    if (f == &F3_CROWD25) return L2(L"Torcida reduzida: flashes a 25% (experimental)", L"Reduced crowd: flashes at 25% (experimental)");
    if (f == &F6_CTHREADS) return L2(L"Threads de compilação de shaders", L"Shader compile threads");
    if (f == &F_FXQ) return L2(L"Efeitos e sombras (shaders)", L"Effects and shadows (shaders)");
    if (f == &F_HZ) return L2(L"Taxa do monitor no jogo", L"Monitor rate in the game");
    if (f == &F_LEARN) return L2(L"Ajustar sozinho a cada partida", L"Tune itself after each session");
    if (f == &F4_UW) return L2(L"Janela 3440x1440", L"3440x1440 window");
    if (f == &F4_HDR || f == &F6_HDR) return L2(L"HDR (experimental)", L"HDR (experimental)");
    if (f == &F6_DLSS) return L2(L"Modo PS5 Pro + DLSS", L"PS5 Pro mode + DLSS");
    if (f == &F_SRV38) return L2(L"Versão do servidor online", L"Online server version");
    /* 7.1 (critic 4.11): not "the Pro's graphics": the game itself never takes its PS5 Pro profile on these eboots */
    if (f == &F6_PROSW) return L2(L"Modo PS5 Pro (motor novo)", L"PS5 Pro mode (new engine)");
    if (f == &F6_DLSSSW) return L2(L"DLSS (placas RTX)", L"DLSS (RTX cards)");
    if (f == &F6_UPS || f == &F6_UPSN) return L2(L"Upscaler (com o modo PS5 Pro)", L"Upscaler (with the PS5 Pro mode)");   /* 7.1 */
    if (f == &F6_DLSSQ || f == &F6_UPSQF) return L2(L"Preset do upscaler", L"Upscaler preset");   /* 7.1: DLSS and FSR */
    if (f == &F6_NEWENG) return L2(L"Motor novo (experimental; não vale no modo Parsec)", L"New engine (experimental; not in Parsec mode)");
    if (f == &F6_PRO) return L2(L"Modo PS5 Pro (precisa do motor novo)", L"PS5 Pro mode (needs the new engine)");
    /* 7.1: UFC 6's film grain, film effects and ray tracing off (UFC6.exe), not tested in a game yet */
    if (f == &F6_GRAIN) return L2(L"Sem granulado de filme (experimental)", L"No film grain (experimental)");
    if (f == &F6_FILMFX) return L2(L"Sem efeito de filme: aberração cromática, vinheta e sujeira na lente (experimental)", L"No film effect: chromatic aberration, vignette and lens dirt (experimental)");
    if (f == &F6_RTOFF) return L2(L"Sem ray tracing (experimental)", L"No ray tracing (experimental)");
    if (f == &F6_ACTIONS) return L2(L"Menu de ações no jogo (LB+RB+Start)", L"In-game actions menu (LB+RB+Start)");   /* 6.2 */
    /* Released (2026-11-13): UFC 6's experimental game patches (UFC6.exe), not tested in a game yet */
    if (f == &F6_WIDE21) return L2(L"21:9 real em janela/tela 21:9 (experimental)", L"Real 21:9 on a 21:9 window/screen (experimental)");
    if (f == &F6_BLUR) return L2(L"Sem desfoque de movimento (experimental)", L"No motion blur (experimental)");
    if (f == &F6_SHADOWS) return L2(L"Sombras (experimental)", L"Shadows (experimental)");
    if (f == &F6_CROWDRED) return L2(L"Torcida reduzida (experimental)", L"Reduced crowd (experimental)");
    if (f == &F6_SKIPLOGOS) return L2(L"Logos de abertura curtos (experimental)", L"Short intro logos (experimental)");
    if (f == &F5_SHADOWS) return L2(L"Sem sombras (mais FPS)", L"No shadows (more FPS)");
    if (f == &F5_SEATS) return L2(L"Sem cadeiras vazias atrás da grade", L"No empty seats behind the cage");
    if (f == &F5_FSR2) return L2(L"Sem o FSR 2 do jogo (modo F9)", L"Without the game's FSR 2 (F9 mode)");
    return T(f->label);
}
static void Dropdown(HDC dc, GpGraphics *g, int x, int y, int w, const Field *f, int id, const wchar_t *label)
{
    Text(dc, g_fSmall, C_SUB, x + SC(2), y + SC(1), label ? label : FieldName(f));
    RECT r = {x, y + SC(20), x + w, y + SC(58)};
    BOOL hot = g_hot == id;
    Box(g, r, SC(10), A(hot ? RGB(30, 22, 48) : C_FIELD, 245), A(hot ? C_HOT : C_FIELDB, 255), 1.0f);
    int v = GetInt(g_sel, f->key, f->def), i = OptIndex(f, v);
    wchar_t custom[32]; const wchar_t *name = i >= 0 ? OptName(&f->opts[i]) : (_snwprintf(custom, 32, L"%d", v), custom);
    RECT tr = {r.left + SC(16), r.top, r.right - SC(40), r.bottom};
    TextR(dc, TextW(dc, g_fText, name) > tr.right - tr.left ? g_fSmall : g_fText, C_TEXT, tr, name, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);   /* 11-13: a long choice a size smaller before it is cut (as the switches) */
    Chevron(g, r.right - SC(22), (r.top + r.bottom) / 2, SC(6), 0, A(hot ? C_HOT : C_SUB, 255));
    AddHit(r.left, r.top, r.right, r.bottom, id);
}
static void SwitchW(HDC dc, GpGraphics *g, int x, int y, int w, BOOL on, const wchar_t *label, int id)
{
    RECT r = {x, y + SC(4), x + SC(44), y + SC(28)};
    BOOL hot = g_hot == id;
    COLORREF knob = RGB0(255, 255, 255);
    if (on) knob = AccentBox(g, r, SC(12));   /* 6.0 final: white with a dark knob in the logo's black and white */
    else Box(g, r, SC(12), A(RGB(28, 22, 44), 255), A(hot ? C_HOT : C_FIELDB, 255), 1.0f);
    int kx = on ? r.right - SC(21) : r.left + SC(3); Dot(g, kx + SC(9), r.top + SC(12), SC(9), on ? A(knob, 255) : 0xFF8A92AA);
    RECT tr = {x + SC(56), y, x + w, y + SC(32)};
    HFONT f = TextW(dc, g_fText, label) > tr.right - tr.left ? g_fSmall : g_fText;   /* 4.0.4: a long name a size smaller, not cut */
    if (TextW(dc, f, label) > tr.right - tr.left) {   /* 11-13: still too long in the small size (a one-column switch): two lines, centred on the switch, not cut */
        TEXTMETRICW tm; SelectObject(dc, f); GetTextMetricsW(dc, &tm);
        RECT wr = {tr.left, y + SC(16) - tm.tmHeight, tr.right, y + SC(16) + tm.tmHeight};
        Wrap(dc, f, on || hot ? C_TEXT : C_SUB, wr, label, TRUE);
    } else
    TextR(dc, f, on || hot ? C_TEXT : C_SUB, tr, label, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    AddHit(r.left, r.top, min(x + w, x + SC(56) + TextW(dc, f, label)), r.bottom, id);
}
static void Switch(HDC dc, GpGraphics *g, int x, int y, int w, const Field *f, int id)
{
    const wchar_t *nm = FieldName(f);
    /* 6.1 review: UFC 1, 3 and 4's TV camera is not tested in a fight yet: the switch itself says so (UFC 2 keeps the
       community's camera mod under the same switch) */
    if (f == &F4_CAM3P && (g_sel == 0 || g_sel == 2 || g_sel == 3)) nm = L2(L"Câmera de transmissão ao vivo (experimental, Select alterna)", L"Live broadcast camera (experimental, Select toggles)");
    SwitchW(dc, g, x, y, w, GetInt(g_sel, f->key, f->def) != 0, nm, id);
}
static void NoteBox(HDC dc, GpGraphics *g, int x, int y, int w, int h, const wchar_t *text, BOOL warn)
{
    COLORREF c = warn ? C_ORANGE : C_SUB;
    RECT bg = {x, y, x + w, y + h - SC(4)}; Box(g, bg, SC(10), A(warn ? RGB(60, 40, 10) : RGB(255, 255, 255), warn ? 96 : 9), A(warn ? C_ORANGE : C_LINE, warn ? 130 : 255), 1.0f);
    Dot(g, x + SC(18), y + SC(17), SC(8), A(c, 255));
    RECT ic = {x + SC(10), y + SC(9), x + SC(26), y + SC(25)}; TextR(dc, g_fTag, RGB(10, 12, 20), ic, warn ? L"!" : L"i", DT_CENTER | DT_VCENTER);
    RECT tr = {x + SC(36), y + SC(8), x + w - SC(12), y + h}; Wrap(dc, g_fSmall, warn ? RGB(255, 210, 150) : RGB(186, 176, 214), tr, text, TRUE);
}
/* 6.2: the options added this version pushed the notes (and on some pages options) out of the panel, which cut them. The list now moves a whole row at
   a time (mouse wheel, or the pad's up/down at the edge of what is shown): only rows that fit are drawn, so nothing is cut and no hidden row can be clicked. */
static int g_itRow, g_itMax, g_itBottom, g_itKey = -1, g_itFirstY, g_itLastY;
static void PaintItems(HDC dc, GpGraphics *g, Item *it, int n, int x0, int y0, int w)
{
    int col = 0, colW = (w - 2 * SC(24)) / 3, gap = SC(24), rowH = 0, nd = 0, ns = 0, rowOf[48], rowHs[48], rowY[48], nr = 0;
    if (n > 48) n = 48;
    for (int i = 0; i < n; i++) {
        int span = it[i].span; if (span < 1) span = 1;
        if (it[i].type == IT_NOTE || it[i].type == IT_WARN || it[i].type == IT_HEAD) span = 3;
        if (col + span > 3 || (i > 0 && col && (it[i].type == IT_SWITCH) != (it[i - 1].type == IT_SWITCH))) { rowHs[nr++] = rowH; col = 0; rowH = 0; }
        int iw = span * colW + (span - 1) * gap, h = ItemHeight(dc, &it[i], iw);
        rowOf[i] = nr; if (h > rowH) rowH = h;
        col += span; if (col >= 3) { rowHs[nr++] = rowH; col = 0; rowH = 0; }
    }
    if (col) rowHs[nr++] = rowH;
    int avail = g_itBottom > y0 ? g_itBottom - y0 : 1 << 28, mx = 0, key = g_sel * 1000 + g_tab * 10 + g_view;
    for (int r = nr - 1, tot = 0; r >= 0; r--) { tot += rowHs[r] + SC(10); if (tot - SC(10) > avail) { mx = r + 1; break; } }
    if (key != g_itKey) { g_itKey = key; g_itRow = 0; }
#ifdef PORTHEON_TEST
    { wchar_t tr[8]; if (GetEnvironmentVariableW(L"PORTHEON_TEST_ITROW", tr, 8)) g_itRow = _wtoi(tr); }
#endif
    g_itMax = mx; if (g_itRow > mx) g_itRow = mx;
    int y = y0, lastRow = -1;
    for (int r = 0; r < nr; r++) rowY[r] = -1;   /* 11-13: every row, not only up to the first that does not fit (the rows after it were drawn at whatever the stack held: UFC 6's Video page showed two switches over another row) */
    for (int r = 0; r < nr; r++) { if (r < g_itRow) continue; if (y + rowHs[r] > y0 + avail) break; rowY[r] = y; lastRow = r; y += rowHs[r] + SC(10); }
    g_itFirstY = y0; g_itLastY = lastRow >= 0 ? rowY[lastRow] : y0;
    /* 11-13: the pad's up/down moves the list from the first/last row shown that holds a list or a switch: with a note as the
       last row shown (UFC 6's Video page: the DLSS note) the rows under it could only be reached with the mouse wheel */
    { int fy = -1, ly = -1; for (int i = 0; i < n; i++) if ((it[i].type == IT_DROP || it[i].type == IT_SWITCH) && rowY[rowOf[i]] >= 0) { if (fy < 0) fy = rowY[rowOf[i]]; ly = rowY[rowOf[i]]; }
      if (fy >= 0) { g_itFirstY = fy; g_itLastY = ly; } }
    col = 0;
    for (int i = 0; i < n; i++) {
        int span = it[i].span; if (span < 1) span = 1;
        if (it[i].type == IT_NOTE || it[i].type == IT_WARN || it[i].type == IT_HEAD) span = 3;
        if (i > 0 && rowOf[i] != rowOf[i - 1]) col = 0;
        int x = x0 + col * (colW + gap), iw = span * colW + (span - 1) * gap; col += span;
        y = rowY[rowOf[i]];
        if (y < 0) {   /* 11-13: a row not shown keeps its number, so the focus stays on the same option when the list moves */
            if (it[i].type == IT_DROP && nd < 40) g_dropF[nd++] = it[i].f; else if (it[i].type == IT_SWITCH && ns < 40) g_swF[ns++] = it[i].f;
            continue; }
        int h = ItemHeight(dc, &it[i], iw);
        switch (it[i].type) {
        case IT_DROP: if (nd < 40) { g_dropF[nd] = it[i].f; Dropdown(dc, g, x, y, iw, it[i].f, H_DROP + nd, NULL); nd++; } break;
        case IT_SWITCH: if (ns < 40) { g_swF[ns] = it[i].f; Switch(dc, g, x, y, iw, it[i].f, H_SWITCH + ns); ns++; } break;
        case IT_EDIT: {
            Text(dc, g_fSmall, C_SUB, x + SC(2), y + SC(1), it[i].text);
            RECT r = {x, y + SC(20), x + iw, y + SC(58)}; Box(g, r, SC(10), A(C_FIELD, 255), A(GetFocus() == it[i].edit ? C_CYAN : C_FIELDB, 255), 1.0f);
            int eh = SC(20); RECT er = {r.left + SC(14), (r.top + r.bottom - eh) / 2, r.right - SC(12), (r.top + r.bottom - eh) / 2 + eh};
            int k = EditIndex(it[i].edit); g_editWant[k] = TRUE; PlaceEdit(k, it[i].edit, er, TRUE);
            break; }
        case IT_FBUTTON: { RECT r = {x, y + SC(20), x + iw, y + SC(58)}; Button(dc, g, r, it[i].text, it[i].id, it[i].id == H_BROWSE && St(g_sel) == S_PICK ? 1 : 0); break; }
        case IT_HOSTIP: HostIpWidget(dc, g, x, y, iw); break;
        case IT_PADPICK: PadPickWidget(dc, g, x, y, iw); break;
        case IT_PADPICK2: Pad2PickWidget(dc, g, x, y, iw); break;
        case IT_HEAD: Section(dc, g, x, y + SC(6), it[i].text); break;
        default: NoteBox(dc, g, x, y, iw, h, it[i].text, it[i].type == IT_WARN); break;
        }
    }
    if (mx > 0) {   /* where the list is */
        int bx = x0 + w + SC(8), bh = avail - SC(4), th = bh * (nr - mx) / nr, ty = y0 + (bh - th) * g_itRow / mx;
        RECT tr = {bx, y0, bx + SC(4), y0 + bh}, tm = {bx, ty, bx + SC(4), ty + th};
        Box(g, tr, SC(2), A(C_SUB, 36), 0, 0.0f); Box(g, tm, SC(2), A(C_SUB, 190), 0, 0.0f);
    }
}
