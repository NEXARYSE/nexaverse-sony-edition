/* PORTHEON 6.2 (2026-11-09, the user: "nobody left with a doubt", "achievements, points"): the Help center (F1) and the
 * Achievements screen, both opened from the top bar's power menu. Achievements are worked out from what portheon.ini
 * already holds (playtime_<id>, lastplay_<id>): nothing new is stored, nothing leaves the PC. */
enum { OV_HELP = 60, OV_ACHV = 61 };
enum { H_HELPTOPIC = 970 /* 10 */, H_X62CLOSE = 981 };
#define NHELP 10
static int g_helpSel;
static struct { BOOL played[NGAMES]; int secs[NGAMES]; int pts, total, unlocked; } g_ach;

static void AchLoad(void)
{
    memset(&g_ach, 0, sizeof(g_ach));
    int ufcPlayed = 0;
    long long all = 0;
    for (int g = 0; g < NGAMES; g++) {
        if (SOON(g)) continue;   /* 6.2 */
        wchar_t k[64]; _snwprintf(k, 64, L"playtime_%s", G[g].id); int s = PIni(k, 0);
        _snwprintf(k, 64, L"lastplay_%s", G[g].id);
        BOOL p = s >= 60 || PIni(k, 0) > 0;
        g_ach.secs[g] = s; g_ach.played[g] = p; all += s;
        if (p) { g_ach.pts += 10; g_ach.unlocked++; if (g < 6) ufcPlayed++; }
        if (s >= 5 * 3600) { g_ach.pts += 25; g_ach.unlocked++; }
        if (s >= 20 * 3600) { g_ach.pts += 50; g_ach.unlocked++; }
    }
    g_ach.total = NGAMES * 3 + 3;
    if (ufcPlayed >= 6) { g_ach.pts += 100; g_ach.unlocked++; }
    if (all >= 50 * 3600LL) { g_ach.pts += 100; g_ach.unlocked++; }
    if (all >= 100 * 3600LL) { g_ach.pts += 200; g_ach.unlocked++; }
}
#ifdef PORTHEON_TEST
void AchLoadT(void) { AchLoad(); }   /* the UI shots (ui.c) */
#endif
static void Extra62Open(int ov) { if (ov == OV_ACHV) AchLoad(); g_overlay = ov; g_focus = -1; Snd(SND_OK); InvalidateRect(g_wnd, NULL, FALSE); }

static void HelpTopic(int i, const wchar_t **title, const wchar_t **body)
{
    static const wchar_t *const T[NHELP][4] = {
        {L"Primeiros passos", L"First steps",
         L"1. Abra o PORTHEON. Ele procura os seus jogos no PC sozinho.\n2. Se algum não aparecer como pronto, escolha o jogo, abra Arquivos do jogo e indique a pasta do jogo extraído.\n3. Clique em JOGAR. O PORTHEON ajusta o vídeo para o seu PC sozinho.\n\nA primeira partida de cada jogo demora mais e pode engasgar um pouco, porque os efeitos gráficos estão sendo preparados. Da segunda em diante fica bem mais liso.",
         L"1. Open PORTHEON. It looks for your games on the PC by itself.\n2. If a game does not show as ready, pick it, open Game files and point to the folder of the extracted game.\n3. Click PLAY. PORTHEON sets the video up for your PC by itself.\n\nThe first match of each game takes longer and may stutter a little, because the graphics effects are being prepared. From the second one on it is much smoother."},
        {L"Onde coloco o jogo?", L"Where do I put the game?",
         L"Em qualquer pasta do PC, de preferência num SSD e fora do OneDrive.\n\nA pasta certa é a que tem o eboot.bin e a pasta sce_sys dentro dela. Se o jogo estiver dentro de outra pasta a mais, indique a de dentro.\n\nO jogo precisa estar extraído e completo. Faltou arquivo? Extraia de novo.\n\nO PORTHEON não vem com nenhum jogo: você usa a sua própria cópia.",
         L"In any folder on the PC, preferably on an SSD and outside OneDrive.\n\nThe right folder is the one with eboot.bin and the sce_sys folder inside. If the game sits inside one more folder, point to the inner one.\n\nThe game must be extracted and complete. Files missing? Extract it again.\n\nPORTHEON comes with no game: you use your own copy."},
        {L"Atualizar sem perder nada", L"Updating without losing anything",
         L"Use o botão de atualizar no topo do PORTHEON. Ele instala a versão nova na mesma pasta e mantém saves, caches e configurações.\n\nNunca extraia uma versão nova por cima da antiga.\n\nOs saves do UFC 5 e 6 ficam em games\\UFC5\\bin\\_SaveData e games\\UFC6\\bin\\_SaveData. O UFC 1 a 4 guarda os saves fora da pasta do PORTHEON.",
         L"Use the update button at the top of PORTHEON. It installs the new version in the same folder and keeps saves, caches and settings.\n\nNever extract a new version over the old one.\n\nUFC 5 and 6 saves are in games\\UFC5\\bin\\_SaveData and games\\UFC6\\bin\\_SaveData. UFC 1 to 4 keep their saves outside PORTHEON's folder."},
        {L"O jogo fechou ou travou", L"The game closed or froze",
         L"Jogue de novo primeiro: o PORTHEON lê o que aconteceu e ajusta o próximo JOGAR sozinho, em vários passos.\n\nSe repetir:\n• Atualize o driver da placa de vídeo.\n• Feche programas com sobreposição (RivaTuner, gravadores).\n• Na tela inicial, abra Logs do sistema (Quadrado / X no controle) e envie o relatório no Discord.\n\nSem relatório não dá para saber o que houve no seu PC.",
         L"Play again first: PORTHEON reads what happened and adjusts the next PLAY by itself, in several steps.\n\nIf it repeats:\n• Update your graphics driver.\n• Close programs with overlays (RivaTuner, recorders).\n• On the home screen, open System logs (Square / X on the pad) and send the report on Discord.\n\nWithout a report nobody can tell what happened on your PC."},
        {L"FPS baixo ou engasgos", L"Low FPS or stutter",
         L"Na página do jogo, aba Vídeo:\n• Escolha o preset Baixo ou Médio.\n• No UFC 6, Resolução 3D em 50% dá o maior ganho.\n• No UFC 5, tire público e sombras.\n\nTambém ajuda: jogo num SSD, 16 GB de RAM com arquivo de paginação ativado, e o notebook na tomada.\n\nA entrada dos lutadores no UFC 5 é a parte mais pesada: segure o botão por 1,5 s para pular.",
         L"On the game's page, Video tab:\n• Pick the Low or Medium preset.\n• In UFC 6, 3D resolution at 50% gives the biggest gain.\n• In UFC 5, turn off the crowd and shadows.\n\nAlso helps: the game on an SSD, 16 GB of RAM with the page file on, a laptop on AC power.\n\nThe fighters' walkouts in UFC 5 are the heaviest part: hold the button for 1.5 s to skip."},
        {L"Placas de vídeo e drivers", L"Graphics cards and drivers",
         L"UFC 1 a 4: placa com Vulkan 1.3 e driver atualizado.\n\nUFC 5 e 6: precisam de uma placa mais nova que a série GTX 10 e de processador com AVX2. Recomendado 16 GB de RAM e, no UFC 6, 8 GB de memória de vídeo.\n\nRTX 50 no UFC 6: o driver 621 tem uma falha conhecida ao preparar os efeitos. O PORTHEON contorna, mas o driver 610.88 é o caminho mais estável.\n\nRX 5000 não roda o UFC 6.",
         L"UFC 1 to 4: a card with Vulkan 1.3 and an up-to-date driver.\n\nUFC 5 and 6: need a card newer than the GTX 10 series and a CPU with AVX2. 16 GB of RAM recommended and, in UFC 6, 8 GB of video memory.\n\nRTX 50 in UFC 6: driver 621 has a known fault while preparing effects. PORTHEON works around it, but driver 610.88 is the most stable path.\n\nRX 5000 does not run UFC 6."},
        {L"Jogar com amigos", L"Playing with friends",
         L"No mesmo PC: nos UFC 1 a 4, dois controles são dois jogadores.\n\nPela internet: ative o Modo Parsec na aba Online do jogo e conecte o amigo pelo Parsec. No UFC 6 o próximo controle conectado é o Jogador 2. No UFC 5 é um jogador por vez.\n\nO modo online do UFC 2 está em teste: veja a aba Online dele.",
         L"On one PC: in UFC 1 to 4, two controllers are two players.\n\nOver the internet: turn on Parsec mode in the game's Online tab and connect your friend through Parsec. In UFC 6 the next controller connected is Player 2. In UFC 5 it is one player at a time.\n\nUFC 2's online mode is in testing: see its Online tab."},
        {L"Controles e teclado", L"Controllers and keyboard",
         L"Controles de PS5, PS4, PS3, Xbox e genéricos funcionam sem configurar.\n\nTeclado: na página do jogo, abra a tela de Teclado, clique no comando e aperte a tecla que quiser. Se a tecla já estava em uso, as duas trocam de lugar. Vale do UFC 1 ao 6.\n\nControle: na aba Jogabilidade, use Controle: mudar os botões.\n\nCâmeras: nos UFC 2, 3 e 4, ligue a câmera de TV com 1ª pessoa e aperte Select ou o touchpad durante a luta. No UFC 3, escolha a câmera Clássica nas opções do jogo.",
         L"PS5, PS4, PS3, Xbox and generic controllers work with no setup.\n\nKeyboard: on the game's page, open the Keyboard screen, click a control and press the key you want. If the key was taken, the two swap. It covers UFC 1 to 6.\n\nController: in the Gameplay tab, use Controller: change the buttons.\n\nCameras: in UFC 2, 3 and 4, turn on the TV camera with 1st person and press Select or the touchpad during a fight. In UFC 3, choose the Classic camera in the game's options."},
        {L"Adicionar outros jogos", L"Adding other games",
         L"Na fileira de jogos da tela inicial, use o quadro Adicionar. Dá para trazer jogos da Steam, da Epic, de emuladores, ports nativos e qualquer .exe.\n\nOs jogos adicionados abrem do jeito deles: as opções de vídeo e as correções do PORTHEON valem para os jogos dele.",
         L"In the home screen's row of games, use the Add tile. You can bring games from Steam, Epic, emulators, native ports and any .exe.\n\nAdded games start their own way: PORTHEON's video options and fixes apply to its own games."},
        {L"Vídeos da tela inicial", L"Home screen videos",
         L"Os clipes tocam pelo player oficial do YouTube: nada é baixado.\n\nSom: tecla M ou R3 no controle.\nDesligar ou deixar só vídeos locais: Configurações > Interface.\nSeu próprio clipe: aba de vídeos do jogo, Colar link do YouTube.\n\nSem internet, ou sem o WebView2 da Microsoft, o quadro não aparece e a música do jogo volta.",
         L"The clips play through YouTube's own player: nothing is downloaded.\n\nSound: the M key or R3 on the pad.\nTurn off or local videos only: Settings > Interface.\nYour own clip: the game's video tab, Paste a YouTube link.\n\nWith no internet, or without Microsoft's WebView2, the panel does not show and the game's music comes back."},
    };
    *title = L2(T[i][0], T[i][1]); *body = L2(T[i][2], T[i][3]);
}
static void PaintHelp62(HDC dc, GpGraphics *g, RECT rc)
{
    Dim(g, rc);
    RECT c = Card(g, 90, 56, 1100, 700);
    Text(dc, g_fH2, RGB(255, 255, 255), c.left + SC(34), c.top + SC(24), L2(L"Central de ajuda", L"Help center"));
    CloseX(g, c, H_X62CLOSE);
    int x = c.left + SC(26), y = c.top + SC(86), w = SC(310), rowh = SC(56);
    for (int i = 0; i < NHELP; i++) {
        const wchar_t *t, *b; HelpTopic(i, &t, &b);
        RECT r = {x, y, x + w, y + rowh - SC(6)};
        BOOL on = i == g_helpSel, hot = g_hot == H_HELPTOPIC + i;
        Box(g, r, (r.bottom - r.top) / 2, on ? A(RGB(255, 255, 255), 235) : hot ? A(RGB(255, 255, 255), 34) : A(RGB(255, 255, 255), 12), 0, 0);   /* 7.0 fix: pills */
        Text(dc, g_fText, on ? RGB(14, 14, 18) : C_TEXT, r.left + SC(22), r.top + SC(13), t);
        AddHit(r.left, r.top, r.right, r.bottom, H_HELPTOPIC + i);
        y += rowh;
    }
    const wchar_t *t, *b; HelpTopic(g_helpSel, &t, &b);
    int bx = x + w + SC(36), by = c.top + SC(90);
    Text(dc, g_fH2, RGB(255, 255, 255), bx, by, t);
    by += SC(58);
    wchar_t para[1400]; const wchar_t *p = b;
    while (*p) {   /* paragraphs: Wrap has no line breaks of its own */
        const wchar_t *e = wcschr(p, L'\n'); size_t n = e ? (size_t)(e - p) : wcslen(p); if (n > 1399) n = 1399;
        wmemcpy(para, p, n); para[n] = 0;
        if (n) { RECT r = {bx, by, c.right - SC(40), by + SC(120)}; by += Wrap(dc, g_fText, C_TEXT, r, para, TRUE) + SC(4); } else by += SC(14);
        p += e ? (size_t)(e - p) + 1 : wcslen(p);
    }
}
static void Medal(HDC dc, GpGraphics *g, int cx, int cy, BOOL on, const wchar_t *label)
{
    Oct(g, cx, cy, SC(13), on ? A(RGB(255, 255, 255), 245) : A(RGB(255, 255, 255), 18), on ? 0 : A(RGB(255, 255, 255), 60), on ? 0 : 1.0f);
    int w = TextW(dc, g_fSmall, label);
    Text(dc, g_fSmall, on ? RGB(14, 14, 18) : C_SUB, cx - w / 2, cy - SC(9), label);
}
static void PaintAch62(HDC dc, GpGraphics *g, RECT rc)
{
    Dim(g, rc);
    RECT c = Card(g, 150, 46, 980, 716);
    Text(dc, g_fH2, RGB(255, 255, 255), c.left + SC(34), c.top + SC(22), L2(L"Conquistas", L"Achievements"));
    CloseX(g, c, H_X62CLOSE);
    wchar_t s[120]; _snwprintf(s, 120, L2(L"%d pontos  ·  nível %d  ·  %d de %d conquistas", L"%d points  ·  level %d  ·  %d of %d achievements"), g_ach.pts, g_ach.pts / 100 + 1, g_ach.unlocked, g_ach.total); s[119] = 0;
    Text(dc, g_fText, C_TEXT, c.left + SC(34), c.top + SC(68), s);
    int top = c.top + SC(104), x0 = c.left + SC(34), x1 = c.right - SC(250);
    {   const wchar_t *hd[3] = {L2(L"Jogou", L"Played"), L"5 h", L"20 h"};
        for (int i = 0; i < 3; i++) Text(dc, g_fSmall, C_SUB, x1 + SC(10 + 60 * i) - TextW(dc, g_fSmall, hd[i]) / 2, top, hd[i]); }
    int y = top + SC(24), row = SC(44);
    for (int i = 0; i < NGAMES; i++) {
        if (SOON(i)) continue;   /* 6.2 */
        Text(dc, g_fText, g_ach.played[i] ? C_TEXT : C_SUB, x0, y + SC(9), G[i].name);
        wchar_t h[24]; _snwprintf(h, 24, L"%d h", g_ach.secs[i] / 3600); Text(dc, g_fSmall, C_SUB, x1 - SC(120), y + SC(12), h);
        Medal(dc, g, x1 + SC(10), y + SC(18), g_ach.played[i], L"10");
        Medal(dc, g, x1 + SC(70), y + SC(18), g_ach.secs[i] >= 5 * 3600, L"25");
        Medal(dc, g, x1 + SC(130), y + SC(18), g_ach.secs[i] >= 20 * 3600, L"50");
        y += row;
    }
    RECT n = {x0, y + SC(4), c.right - SC(34), y + SC(50)};
    Wrap(dc, g_fSmall, C_SUB, n, L2(L"Bônus: +100 por jogar os seis UFCs, +100 com 50 horas no total e +200 com 100 horas. As conquistas vêm do tempo de jogo guardado neste PC.",
                                    L"Bonus: +100 for playing all six UFCs, +100 at 50 hours in total and +200 at 100 hours. Achievements come from the play time kept on this PC."), TRUE);
}
static void Extra62Click(int h)
{
    if (h == H_X62CLOSE) { g_overlay = OV_NONE; Snd(SND_BACK); }
    else if (h >= H_HELPTOPIC && h < H_HELPTOPIC + NHELP) { g_helpSel = h - H_HELPTOPIC; Snd(SND_OK); }
    InvalidateRect(g_wnd, NULL, FALSE);
}
