/*
 * PORTHEON 4.0.4 - Ports & Mods
 * Copyright (C) 2026 Marcelo Miyake (XAndromedaXR). GPL-2.0-or-later.
 * One launcher, drawn like a game console, for the native PC ports of EA SPORTS UFC, UFC 2, UFC 3 and UFC 4:
 * options are written to %LOCALAPPDATA%\UFCnNative\sacredfire.ini (the ports' own file) and the port is started as
 * games\UFCn\UFCn.exe --play.
 * 4.0: the console interface (boot, profiles, home with the games, a hub per game, system settings; mouse, keyboard
 * and controller), profiles with their own saves, the save manager, cheats, the install helper with a check of the
 * game's files, the PC checks behind "error 0xC0000135", Intel Core 2nd/3rd generation support for UFC 3, and 16
 * languages. ui.c, detect.c, online.c, update.c, lang.c, checks.c, extras.c and pad.c are part of this file.
 * The whole window is drawn by hand (GDI/GDI+) except the text fields. Win32, no dependencies beyond Windows itself.
 * 4.2: UFC 6 (PS5) takes UFC 5's place, through its own launcher games\UFC6\UFC6.exe (PORTHEON UFC 6's engine).
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
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <bcrypt.h>
#include <math.h>
#include <ctype.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <propsys.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <stdint.h>
#include <cpuid.h>
#include <winhttp.h>
#include <tlhelp32.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef PORTHEON_TEST   /* PORTHEON_TEST_QUIET=1: tests while the person at the PC works: PORTHEON never takes the focus,
                          and a window start opens off the screen without activation */
static BOOL TestQuiet(void) { return GetEnvironmentVariableW(L"PORTHEON_TEST_QUIET", NULL, 0) > 0; }
static BOOL TestFg(HWND w) { return TestQuiet() ? FALSE : SetForegroundWindow(w); }
#define SetForegroundWindow TestFg
#endif

#ifndef APP_VER
#define APP_VER L"7.3"   /* 7.3 = "Portheon Update 7.3" (2026-11-17): the ARENA on the top bar with regions and Parsec's Share link (7.2.1's fix), UFC6.exe 1.31.0, UFC 4's memory pool, seven graphics presets. 7.2 = "Portheon Update 7.2": PORTHEON ARENA (online fights, arena folder) inside PORTHEON. 7.1 = "Portheon Update 7.1" (the owner's words; digits only here, for the updater's VerCmp): the Steam button's file fixed, UFC 6's upscaler list (DLSS / AMD FSR) and its film grain, film effect and ray tracing switches, the reports' fixes. 7.0 = "PORTHEON Released": 6.2 + UFC 4 on the Native Plus engine, the crowd at a quarter. 6.2: 6.1 + the new home screen (the vitrine with YouTube clips, live cards, status chips, quick options), Fight Night Champion and the optional in-game actions menu of UFC 6 */
#endif
#define APP_NAME L"Update"                   /* 7.1: the version's name, shown before its number (7.0: "Released", shown alone) */
#define APP_SHOW APP_NAME L" " APP_VER       /* what the screens show after "Portheon ": "Portheon Update 7.1" (the owner, 11-14: unlike 7.0 the number is shown); APP_VER alone goes to the updater, logs and reports */
/* 3.5.3: who made PORTHEON and where to find him (the home screen's footer, About, the news). 4.4.0: only the Discord
   and the GitHub name stay (the user: the PIX key and the phone number came out) */
#define AUTHOR_NAME   L"Marcelo Miyake"
#define AUTHOR_NICK   L"XAndromedaXR"
#define URL_AUTHOR    L"https://github.com/XAndromedaXR/PORTHEON/commits?author=XAndromedaXR"
#define URL_DISCORD   L"https://discord.gg/FEQ752rUz"   /* 4.3.1: the server's new invite */
#define WM_APP_EXITED (WM_APP + 1)
#define WM_APP_FROZEN (WM_APP + 2)
#define WM_APP_AUTOCFG (WM_APP + 3)
#define WM_APP_UPDATE (WM_APP + 4)
#define WM_APP_SCAN (WM_APP + 5)     /* the scan for game folders ended (detect.c) */
#define WM_APP_DLDONE (WM_APP + 6)   /* the update download ended: wParam 1 = verified (update.c) */
#define WM_APP_DIRECTPLAY (WM_APP + 7)   /* 3.5.2: a desktop shortcut asked the open PORTHEON for a game (wParam: the game) */
#define WM_APP_BACKUP (WM_APP + 8)       /* 4.0: a save backup or restore ended (wParam 1 = done) */

/* ---------------------------------------------------------------- GDI+ flat API (subset) */
typedef struct { UINT32 GdiplusVersion; void *DebugEventCallback; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } GdiplusStartupInput;
typedef void GpImage; typedef void GpGraphics; typedef void GpBrush; typedef void GpPen; typedef void GpPath; typedef void GpImageAttributes;
typedef struct { float m[5][5]; } GpColorMatrix;
int WINAPI GdiplusStartup(ULONG_PTR *, const GdiplusStartupInput *, void *);
int WINAPI GdipCreateBitmapFromStream(IStream *, GpImage **);
int WINAPI GdipLoadImageFromFile(const WCHAR *, GpImage **);
int WINAPI GdipGetImageWidth(GpImage *, UINT *);
int WINAPI GdipGetImageHeight(GpImage *, UINT *);
int WINAPI GdipCreateFromHDC(HDC, GpGraphics **);
int WINAPI GdipSetInterpolationMode(GpGraphics *, int);
int WINAPI GdipSetSmoothingMode(GpGraphics *, int);
int WINAPI GdipSetPixelOffsetMode(GpGraphics *, int);
int WINAPI GdipDrawImageRectRectI(GpGraphics *, GpImage *, INT, INT, INT, INT, INT, INT, INT, INT, int, GpImageAttributes *, void *, void *);
int WINAPI GdipDrawImageRectI(GpGraphics *, GpImage *, INT, INT, INT, INT);
int WINAPI GdipCreateSolidFill(UINT32, GpBrush **);
int WINAPI GdipCreateLineBrushI(const POINT *, const POINT *, UINT32, UINT32, int, GpBrush **);
int WINAPI GdipFillRectangleI(GpGraphics *, GpBrush *, INT, INT, INT, INT);
int WINAPI GdipDeleteBrush(GpBrush *);
int WINAPI GdipCreatePen1(UINT32, float, int, GpPen **);
int WINAPI GdipCreatePen2(GpBrush *, float, int, GpPen **);
int WINAPI GdipDeletePen(GpPen *);
int WINAPI GdipCreatePath(int, GpPath **);
int WINAPI GdipDeletePath(GpPath *);
int WINAPI GdipAddPathArcI(GpPath *, INT, INT, INT, INT, float, float);
int WINAPI GdipAddPathLineI(GpPath *, INT, INT, INT, INT);
int WINAPI GdipAddPathEllipseI(GpPath *, INT, INT, INT, INT);
int WINAPI GdipClosePathFigure(GpPath *);
int WINAPI GdipFillPath(GpGraphics *, GpBrush *, GpPath *);
int WINAPI GdipDrawPath(GpGraphics *, GpPen *, GpPath *);
int WINAPI GdipFillEllipseI(GpGraphics *, GpBrush *, INT, INT, INT, INT);
int WINAPI GdipFillPieI(GpGraphics *, GpBrush *, INT, INT, INT, INT, float, float);   /* 6.0: the black-and-white color's swatch */
int WINAPI GdipDrawEllipseI(GpGraphics *, GpPen *, INT, INT, INT, INT);
int WINAPI GdipDrawArcI(GpGraphics *, GpPen *, INT, INT, INT, INT, float, float);
int WINAPI GdipSetPenStartCap(GpPen *, int);   /* 4.4.0: the loading ring's round ends */
int WINAPI GdipSetPenEndCap(GpPen *, int);
int WINAPI GdipDrawLineI(GpGraphics *, GpPen *, INT, INT, INT, INT);
int WINAPI GdipDeleteGraphics(GpGraphics *);
int WINAPI GdipTranslateWorldTransform(GpGraphics *, float, float, int);
int WINAPI GdipSetCompositingMode(GpGraphics *, int);
int WINAPI GdipDisposeImage(GpImage *);
int WINAPI GdipSaveImageToFile(GpImage *, const WCHAR *, const CLSID *, const void *);   /* 4.4.0: the profile photo */
int WINAPI GdipCreateBitmapFromHBITMAP(HBITMAP, HPALETTE, GpImage **);   /* 4.4.0: test builds' frame dump */
typedef void GpBitmap;
int WINAPI GdipCreateBitmapFromScan0(INT, INT, INT, INT, BYTE *, GpBitmap **);
int WINAPI GdipGetImageGraphicsContext(GpImage *, GpGraphics **);
int WINAPI GdipSaveImageToStream(GpImage *, IStream *, const CLSID *, const void *);
int WINAPI GdipSetClipPath(GpGraphics *, GpPath *, int);
int WINAPI GdipSetClipRectI(GpGraphics *, INT, INT, INT, INT, int);
int WINAPI GdipSetClipHrgn(GpGraphics *, HRGN, int);
int WINAPI GdipResetClip(GpGraphics *);
int WINAPI GdipCreateImageAttributes(GpImageAttributes **);
int WINAPI GdipSetImageAttributesColorMatrix(GpImageAttributes *, int, BOOL, const GpColorMatrix *, const GpColorMatrix *, int);
int WINAPI GdipDisposeImageAttributes(GpImageAttributes *);
/* 4.3: the neon look (ui.c): an angled gradient with several stops, and soft radial light */
typedef struct { INT X, Y, Width, Height; } GpRectI;
int WINAPI GdipCreateLineBrushFromRectWithAngleI(const GpRectI *, UINT32, UINT32, float, BOOL, int, GpBrush **);
int WINAPI GdipSetLinePresetBlend(GpBrush *, const UINT32 *, const float *, INT);
int WINAPI GdipCreatePathGradientFromPath(const GpPath *, GpBrush **);
int WINAPI GdipSetPathGradientCenterColor(GpBrush *, UINT32);
int WINAPI GdipSetPathGradientSurroundColorsWithCount(GpBrush *, const UINT32 *, INT *);
/* 5.0: the octagon-cut corners and the glass */
typedef struct { float X, Y; } GpPointF;
int WINAPI GdipAddPathPolygon(GpPath *, const GpPointF *, INT);
int WINAPI GdipSetPathGradientCenterPoint(GpBrush *, const GpPointF *);
typedef void GpFontFamily; typedef struct { float X, Y, Width, Height; } GpRectF;   /* 5.0: the poster number */
int WINAPI GdipCreateFontFamilyFromName(const WCHAR *, void *, GpFontFamily **);
int WINAPI GdipDeleteFontFamily(GpFontFamily *);
int WINAPI GdipBitmapGetPixel(GpBitmap *, INT, INT, UINT32 *);   /* 5.0: a picture's mean light */
int WINAPI GdipAddPathString(GpPath *, const WCHAR *, INT, const GpFontFamily *, INT, float, const GpRectF *, const void *);

/* ---------------------------------------------------------------- look */
#define A(c, a) (((UINT32)(a) << 24) | ((UINT32)GetRValue(c) << 16) | ((UINT32)GetGValue(c) << 8) | (UINT32)GetBValue(c))
/* 5.0 look (the user: "black, purple, translucent glass, cinematic"): near-black with a violet cast, the brand's
   purple as the one accent (C_CYAN is now its light violet end, so every "ready" and every focus reads in one family),
   frosted glass panels (GlassPanel, ui.c) and octagon-cut corners (RoundPath), PORTHEON's own shape */
#define C_BG      RGB(6, 5, 11)
#define C_SIDE    RGB(4, 3, 8)
#define C_TEXT    RGB(240, 238, 248)
#define C_SUB     RGB(158, 150, 182)
#define C_DIM     RGB(104, 97, 128)
#define C_CYAN    RGB(192, 150, 255)
#define C_PURPLE  RGB(124, 58, 237)
#define C_ORANGE  RGB(255, 170, 64)
#define C_FIELD   RGB(16, 13, 26)
#define C_FIELDB  RGB(46, 38, 70)
#define C_CARD    RGB(14, 11, 23)
#define C_CARDB   RGB(36, 30, 56)
#define C_SURF    RGB(11, 9, 19)
#define C_LINE    RGB(38, 32, 58)
#define C_GOOD    RGB(80, 220, 150)
#define C_BAD     RGB(255, 92, 110)
#define C_HOT     RGB(255, 255, 255)
#define C_INK     RGB(10, 8, 18)
#define C_VIOLET  RGB(160, 100, 255)   /* 5.0: the middle of the brand gradient */

/* the window is laid out once in these logical units and scaled as a whole (g_dpi: 96 = 100%) */
#define BASE_W 1280
#define BASE_H 800
#define MX 28                   /* side margin */
#define CW (BASE_W - 2 * MX)    /* content width */
#define HDR_H 64

/* ---------------------------------------------------------------- strings (PT-BR / EN) */
enum { S_READY, S_PICK, S_NOPORT, S_RUNNING, S_GAME, S_VIDEO, S_GAMEPLAY, S_BROWSE, S_PORT, S_CLICKCHANGE, S_ADV, S_LOGS, S_PLAY, S_STOP,
       S_ABOUT, S_RES, S_WIN, S_FPS, S_PRESENT, S_UPSCALE, S_SHARP, S_LANG, S_VOLUME, S_PLAYER, S_FULL, S_NEO, S_SKIP, S_LOWLAT, S_ULTRA,
       S_NATIVE, S_PS5PORT, S_PROFILE, S_SHADERS, S_OVERLAY, S_X3D, S_VIB, S_FASTSH, S_ECO, S_PRIO, S_LPAGES, S_BINDLESS, S_PICKGAME,
       S_PICKPORT, S_NOTFOUND, S_STARTING, S_CLOSED, S_CLOSEDERR, S_CANTSTART, S_ULTRAON, S_CHOOSEFIRST, S_SESSION, S_NOLOGS, S_SHTHREADS, S_SPKVOL, S_CALIB, S_STAGEPREP, S_FRAMEGEN, S_RES5, S_P60, S_PCROWD, S_PBLUR, S_PARSEC, S_READBACK, S_X360PORT, S_SCALE, S_RENDERER, S_AA, S_ANISO, S_MODE, S_AUDIO, S_KEYBOARD, S_PS3PORT,
       S_NOCPU, S_NOGPU, S_ONLINEMODE, S_ENGINE, S_BOOST, S_MOUTH, S_CAREERFPS, S_DIAG, S_PADTYPE, S_PADBG, S_PAD1, S_SHORTCUT, S_CPUALL, S_GPUSAFE, S_SKINMODE, S_ASYNCPIPE, S_AMDCPU, S_PRIO6,
       S_REALTIME, S_AUDIOWD, S_FXAA, S_LOWLAT6, S_DOF6, S_SCALE6, S_CAM3P, S_GFX, S_MFG, S_UNLOCK6, S_NODIL6, S_PSHADOW4, S_COUNT };
static const wchar_t *TXT[2][S_COUNT] = {
  { L"Pronto para jogar", L"Escolha a pasta do jogo", L"Port não encontrado", L"Em execução", L"JOGO", L"VÍDEO", L"JOGABILIDADE", L"Procurar",
    L"Port", L"clique para trocar", L"Configurações avançadas", L"Logs", L"JOGAR", L"PARAR",
    L"Sobre", L"Resolução do jogo", L"Tamanho da janela", L"Limite de FPS", L"Modo de apresentação", L"Upscaling", L"Nitidez",
    L"Idioma do jogo", L"Volume", L"Nome do jogador", L"Tela cheia sem bordas", L"Modo PS4 Pro", L"Pular vídeos de introdução", L"Baixa latência (co-op)", L"Ultra desempenho (PC fraco)",
    L"Port nativo para PC", L"Port nativo para PC (PS5)", L"Perfil de desempenho", L"Shaders", L"FPS na tela", L"Modo X3D (Ryzen)", L"Vibração do controle",
    L"Compilar shaders rápido", L"Sem economia de energia", L"Prioridade alta", L"Large pages", L"Texturas bindless (exp.)", L"Escolha a pasta do jogo %s",
    L"Escolha a pasta do port do %s (a que tem o %s)", L"%s não foi encontrado nessa pasta.", L"Iniciando o %s...", L"O jogo foi fechado.",
    L"O jogo fechou com um erro (código %lu). Abra os logs para ver os detalhes.", L"Não foi possível iniciar o %s (erro %lu).",
    L"Ultra desempenho ligado: 720p, modo PS4, 30 FPS, FSR 1.0.", L"Escolha a pasta do jogo primeiro.", L"Última sessão: %s.", L"Ainda não há logs: jogue uma vez primeiro.", L"Threads de compilação", L"Alto-falante do controle", L"Calibrar FPS sozinho", L"Preparação paralela (exp.)", L"Geração de quadros", L"Resolução (modo janela)" , L"FPS da luta (patch)", L"Torcida", L"Motion blur", L"Modo Parsec (jogar com um amigo)", L"Leitura de imagens (tatuagens)", L"Port para PC (Xbox 360)", L"Resolução interna", L"Renderizador", L"Anti-aliasing", L"Filtro de texturas", L"Modo de jogo", L"Áudio", L"Teclado", L"Port para PC (PS3)",
    L"Processador sem suporte", L"Placa de vídeo sem suporte", L"Modo online", L"Motor de vídeo", L"Prioridade alta para o jogo (mais FPS)", L"Correção da boca caída", L"Carreira: FPS estável depois do sparring", L"Diagnóstico completo na próxima partida", L"Tipo de controle", L"Controle com a janela fora de foco (Parsec)", L"Controle do Jogador 1", L"Atalho na área de trabalho ao jogar", L"Usar o processador inteiro (mais threads)", L"Modo seguro de vídeo (contra o código 23)", L"Cor da pele dos lutadores (teste)", L"Compilar shaders sem travar a luta",
    L"Compatibilidade de CPU AMD", L"Prioridade do jogo no processador",
    L"Luta na velocidade do PS5 (sem câmera lenta)", L"Desligar o vigia de áudio do jogo (menos CPU)", L"Anti-aliasing (FXAA, bordas mais suaves)", L"Controle mais rápido (menos atraso, um pouco menos de FPS)", L"Sem desfoque de profundidade (menus e entradas mais leves)", L"Resolução 3D em 0,75x (mais FPS, imagem mais suave)",
    L"Câmera de transmissão ao vivo (Select alterna)", L"Preset gráfico", L"Gerador de quadros (MFG SUPER MOTION)", L"Liberar todos os lutadores e skins", L"Sem câmera lenta contextual (dilatação temporal)", L"Sombras"},
  { L"Ready to play", L"Choose your game folder", L"Port not found", L"Running", L"GAME", L"VIDEO", L"GAMEPLAY", L"Browse",
    L"Port", L"click to change", L"Advanced settings", L"Logs", L"PLAY", L"STOP",
    L"About", L"Game resolution", L"Window size", L"Frame rate limit", L"Present mode", L"Upscaling", L"Sharpening",
    L"Game language", L"Volume", L"Player name", L"Borderless full screen", L"PS4 Pro mode", L"Skip intro videos", L"Low input latency (co-op)", L"Ultra performance (weak PCs)",
    L"Native PC port", L"Native PC port (PS5)", L"Performance profile", L"Shaders", L"FPS on screen", L"X3D mode (Ryzen)", L"Controller vibration",
    L"Fast shader compiling", L"No Windows power saving", L"High priority", L"Large pages", L"Bindless textures (exp.)", L"Choose the %s game folder",
    L"Choose the folder of the %s port (the one with %s)", L"%s was not found in that folder.", L"Starting %s...", L"The game closed.",
    L"The game closed with an error (code %lu). Open the logs for details.", L"Could not start %s (error %lu).",
    L"Ultra performance is on: 720p, PS4 mode, 30 FPS, FSR 1.0.", L"Choose the game folder first.", L"Last session: %s.", L"No logs yet: play once first.", L"Shader compile threads", L"Controller speaker", L"Self-calibrate FPS", L"Parallel preparation (exp.)", L"Frame generation", L"Resolution (windowed)" , L"Fight FPS (patch)", L"Crowd", L"Motion blur", L"Parsec mode (play with a friend)", L"Image readback (tattoos)", L"PC port (Xbox 360)", L"Internal resolution", L"Renderer", L"Anti-aliasing", L"Texture filtering", L"Game mode", L"Audio", L"Keyboard", L"PC port (PS3)",
    L"Processor not supported", L"Graphics card not supported", L"Online mode", L"Video engine", L"High priority for the game (more FPS)", L"Drooping mouth fix", L"Career: steady FPS after sparring", L"Full diagnostic in the next session", L"Controller type", L"Controller works out of focus (Parsec)", L"Player 1 controller", L"Desktop shortcut when playing", L"Use the whole processor (more threads)", L"Safe video mode (against code 23)", L"Fighters' skin color (test)", L"Compile shaders without freezing the fight",
    L"AMD CPU compatibility", L"The game's processor priority",
    L"Fights at the PS5's pace (no slow motion)", L"Turn off the game's audio watchdog (less CPU)", L"Anti-aliasing (FXAA, smoother edges)", L"Faster controller (less lag, a little less FPS)", L"No depth-of-field blur (lighter menus and walkouts)", L"3D resolution at 0.75x (more FPS, softer picture)",
    L"Live broadcast camera (Select toggles)", L"Graphics preset", L"Frame generator (MFG SUPER MOTION)", L"Unlock every fighter and skin", L"No contextual slow motion (time dilation)", L"Shadows"},
};
/* 4.0: 16 languages (lang.c). PT-BR and English are written here, next to each other; the others are looked up by the
   English text in lang.c's table and fall back to English where a text has no translation yet. */
static int g_lang = 0;   /* LANG_PT, LANG_EN, ... (lang.c) */
static const wchar_t *TR(const wchar_t *pt, const wchar_t *en);
#define T(k) TR(TXT[0][k], TXT[1][k])
#define L2(pt, en) TR(pt, en)

/* ---------------------------------------------------------------- games */
/* 4.0: UFC 1 to 4, the native PC ports (the user asked for these four only: UFC Undisputed 3 left PORTHEON, and the
   "coming soon" tiles of UFC 5 and UFC 6 with it). ps5/x360/ps3 stay in the table, always FALSE, so the few checks
   that read them compile to nothing. */
/* 4.0.2: UFC 5 is back (the user asked for it), through its own launcher games\UFC5\UFC5.exe and the KytyPS5 build of
   Cozma's cozma-ufc5 branch (927a181b) as bin\UFC 5.exe: settings in games\UFC5\UFC5.ini, as in 3.5.0 */
/* 4.2: UFC 6 in UFC 5's place (the user's call, 2026-10-28): games\UFC6\UFC6.exe runs PORTHEON UFC 6's engine (the
   KytyPS5 build for UFC 6, 3309ab2) with the same protocol UFC5.exe had (--auto-config, --play --headless, its own ini) */
/* 4.3: UFC 5 and UFC 6 together (the user's call, 2026-10-29): UFC 6 keeps its place (4), UFC 5 comes back as 5 with
   4.0.6's launcher and engine (games\UFC5\UFC5.exe, UFC5.ini [UFC5]). Both speak the same protocol, so the PS5 code
   paths take the game's index and read its own ini, title ID and texts (PsIni, PsDump, PsFind). */
/* 5.0: UFC Undisputed 3 (THQ, Xbox 360) as the 7th game (the user's call, 2026-11-05): games\UFC3U\UFC3.exe runs the
   community port UFC3MONS (a native Windows recompilation of the Xbox 360 game) with its own configurator. None of the
   SacredFire or PS5 code: PORTHEON keeps only the game folder (portheon.ini [UFC3U]), starts "UFC3.exe --play --game
   <folder>" and waits for it, as for the others. */
/* 6.0 (the user, 2026-11-07: "put Zelda inside PORTHEON too, and God of War III"): two games that are not UFC ports, each
   with its own launcher, as UFC Undisputed 3 (extgames.c): 7 = Zelda: Ocarina of Time (Switch 2) on the Lichii emulator
   or natively through Ocarina Recomp, 8 = God of War III Remastered (PS4) through the gow3_pc port */
/* 6.1 (the user, 2026-11-08): 9 = Bloodborne (PS4) through Supermedo's bbport-windows port, 10 = inFAMOUS Second Son (PS4)
   through its PC port; both ports are installed from inside PORTHEON (extgames.c ExtInstallMenu), neither ships with it */
#define NGAMES 13   /* 6.2: 11 = Fight Night Champion (Xbox 360) through FNC Reborn, whose official installer ships in games\FNCREBORN; 12 = GTA VI, "EM BREVE": its cover and its trailer, nothing to play yet (SOON) */
/* each game's colors (its tile when the dump has no art), its title ID, the update the port needs, its year */
typedef struct { const wchar_t *id, *name, *exe, *data; BOOL ps5, x360; const wchar_t *card; BOOL ps3; COLORREF c1, c2; const wchar_t *tid, *upd, *year; } Game;
static const Game G[NGAMES] = {
    {L"UFC1", L"UFC 1", L"UFC1.exe", L"UFC1Native", FALSE, FALSE, L"EA SPORTS UFC", FALSE, RGB(214, 38, 48), RGB(44, 8, 14), L"CUSA00222", L"1.08", L"2014"},
    {L"UFC2", L"UFC 2", L"UFC2.exe", L"UFC2Native", FALSE, FALSE, L"EA SPORTS UFC 2", FALSE, RGB(236, 168, 40), RGB(56, 30, 6), L"CUSA01968", L"1.13", L"2016"},
    {L"UFC3", L"UFC 3", L"UFC3.exe", L"UFC3Native", FALSE, FALSE, L"EA SPORTS UFC 3", FALSE, RGB(44, 128, 255), RGB(8, 18, 58), L"CUSA06534", L"1.14", L"2018"},
    {L"UFC4", L"UFC 4", L"UFC4.exe", L"UFC4Native", FALSE, FALSE, L"EA SPORTS UFC 4", FALSE, RGB(255, 92, 46), RGB(68, 12, 22), L"CUSA14209", L"28.00", L"2020"},
    {L"UFC6", L"UFC 6", L"UFC6.exe", NULL, TRUE, FALSE, L"EA SPORTS UFC 6", FALSE, RGB(255, 64, 40), RGB(52, 8, 6), L"PPSA23566", L"01.004-01.006", L"2026"},
    {L"UFC5", L"UFC 5", L"UFC5.exe", NULL, TRUE, FALSE, L"EA SPORTS UFC 5", FALSE, RGB(150, 84, 255), RGB(20, 10, 52), L"PPSA03541", L"01.031", L"2023"},
    {L"UFC3U", L"UFC Undisputed 3", L"UFC3.exe", NULL, FALSE, TRUE, L"UFC Undisputed 3", FALSE, RGB(200, 24, 32), RGB(28, 6, 8), L"5451087D", L"BETA 1.3", L"2012"},
    {L"ZELDA", L"Zelda: Ocarina of Time", L"emu\\mdias\\lichii.exe", NULL, FALSE, FALSE, L"The Legend of Zelda: Ocarina of Time", FALSE, RGB(48, 168, 96), RGB(8, 32, 18), L"0400486024DEC000", L"1.1.1", L"Switch 2"},
    {L"GOW3", L"God of War III", L"God of War III.exe", NULL, FALSE, FALSE, L"God of War III Remastered", FALSE, RGB(200, 36, 30), RGB(34, 6, 6), L"CUSA01623", L"01.02", L"2015"},
    {L"BLOODBORNE", L"Bloodborne", L"Bloodborne.exe", NULL, FALSE, FALSE, L"Bloodborne", FALSE, RGB(150, 24, 30), RGB(16, 6, 8), L"CUSA03173", L"01.09", L"2015"},
    {L"INFAMOUS", L"inFAMOUS Second Son", L"inFAMOUS Second Son Launcher.exe", NULL, FALSE, FALSE, L"inFAMOUS Second Son", FALSE, RGB(220, 60, 40), RGB(30, 10, 8), L"CUSA00004", L"-", L"2014"},
    {L"FNCREBORN", L"Fight Night Champion", L"dist\\FNCReborn.exe", NULL, FALSE, FALSE, L"Fight Night Champion", FALSE, RGB(232, 176, 72), RGB(34, 16, 8), L"Xbox 360", L"-", L"2011"},
    {L"GTA6", L"Grand Theft Auto VI", L"GTA6.exe", L"GTA6Native", FALSE, FALSE, L"Grand Theft Auto VI", FALSE, RGB(255, 72, 164), RGB(40, 12, 60), L"-", L"-", L"2026"},
};
#define EMU(g) 0
#define PS4G(g) ((g) >= 0 && (g) < 4)          /* UFC 1-4: the SacredFire ports */
#define UFC6G(g) ((g) == 4)
#define UFC5G(g) ((g) == 5)
#define X360G(g) ((g) == 6)                    /* 5.0: UFC Undisputed 3, the Xbox 360 port (UFC3MONS) */
#define ARENA_GAME(g) (PS4G(g) || UFC6G(g))     /* 7.2: the games of PORTHEON ARENA (online.c); not UFC 5 (one PS5 player) */
#define ZELDAG(g) ((g) == 7)                   /* 6.0: Zelda: Ocarina of Time (Switch 2), extgames.c */
#define GOW3G(g) ((g) == 8)                    /* 6.0: God of War III Remastered (PS4), extgames.c */
#define BBG(g) ((g) == 9)                      /* 6.1: Bloodborne (PS4), bbport-windows, extgames.c */
#define ISSG(g) ((g) == 10)                    /* 6.1: inFAMOUS Second Son (PS4), its PC port, extgames.c */
#define FNCG(g) ((g) == 11)                    /* 6.2: Fight Night Champion (Xbox 360), FNC Reborn, extgames.c */
#define NGPORTG(g) (BBG(g) || ISSG(g) || FNCG(g))         /* 6.1: a port the player installs from inside PORTHEON */
#define EXTG(g) ((g) >= 7 && (g) <= 11)        /* 6.0: the games beyond UFC, each with its own launcher (6.1: 7 to 10) */
/* the publisher and the console a game came from (the home screen's, the hub's and the loading screen's tag) */
#define MAKER(g) (SOON(g) ? L"ROCKSTAR GAMES" : X360G(g) ? L"THQ" : ZELDAG(g) ? L"NINTENDO" : GOW3G(g) ? L"SANTA MONICA STUDIO" : BBG(g) ? L"FROMSOFTWARE" : ISSG(g) ? L"SUCKER PUNCH" : L"EA SPORTS")
#define CONSOLE(g) (SOON(g) ? L"PS5 E XBOX SERIES" : X360G(g) || FNCG(g) ? L"XBOX 360" : ZELDAG(g) ? L"SWITCH 2" : G[g].ps5 ? L"PS5" : L"PS4")
#define OWNINI(g) (G[g].ps5)                   /* settings in the port's own ini, not in sacredfire.ini */
#define SOON(g) ((g) == 12)   /* 6.2: GTA VI: shown with its trailer, nothing else opens */

/* ---------------------------------------------------------------- option tables */
typedef struct { const wchar_t *pt, *en; int value; } Opt;
#define O(pt, en, v) {pt, en, v}
static const Opt RES4[] = {O(L"1280x720", L"1280x720", 0), O(L"1920x1080", L"1920x1080", 1), O(L"2560x1440 (2K, experimental)", L"2560x1440 (2K, experimental)", 2), O(L"3840x2160 (4K, experimental)", L"3840x2160 (4K, experimental)", 3), O(L"3440x1440 (ultrawide, imagem ampliada)", L"3440x1440 (ultrawide, enlarged picture)", 4)};
static const Opt WIN4[] = {O(L"Igual ao jogo", L"Same as the game", 0), O(L"1920x1080 (Full HD)", L"1920x1080 (Full HD)", 1), O(L"2560x1440 (2K)", L"2560x1440 (2K)", 2), O(L"3840x2160 (4K)", L"3840x2160 (4K)", 3)};
/* 3.5: UFC 1, 2 and 4 count game time per frame and "higher rates can break UFC 4" (the port's notes): 3 of the 4 UFC 4
   crash logs of 2026-10-22 ran above 60 (90, and no limit on an RTX 4090). They keep 30 and 60; UFC 3 runs fights at
   any rate and goes to 144 (no "unlimited": its code 23 at very high rates). */
/* 3.5.1: no FPS cap in any of them any more (players asked for it back: UFC 2 lost FPS with the 3.5 cap): UFC 1, 2 and 4
   offer 90 to 240 FPS and no limit again, marked experimental (the Video tab and the choice itself say what can
   happen), UFC 3 adds 240 and no limit the same way. 60 stays the default and what every automatic setup writes. */
static const Opt FPS4X[] = {O(L"30 FPS (PS4, mais estável)", L"30 FPS (PS4, most stable)", 30), O(L"60 FPS (recomendado)", L"60 FPS (recommended)", 60), O(L"75 FPS (experimental)", L"75 FPS (experimental)", 75),
    O(L"90 FPS (experimental)", L"90 FPS (experimental)", 90), O(L"100 FPS (experimental)", L"100 FPS (experimental)", 100), O(L"120 FPS (experimental)", L"120 FPS (experimental)", 120), O(L"144 FPS (experimental)", L"144 FPS (experimental)", 144),
    O(L"165 FPS (experimental)", L"165 FPS (experimental)", 165), O(L"180 FPS (experimental)", L"180 FPS (experimental)", 180),
    O(L"240 FPS (experimental)", L"240 FPS (experimental)", 240), O(L"Sem limite (experimental)", L"Unlimited (experimental)", 0)};
static const Opt FPS3[] = {O(L"30 FPS (PS4)", L"30 FPS (PS4)", 30), O(L"60 FPS", L"60 FPS", 60), O(L"75 FPS", L"75 FPS", 75), O(L"90 FPS", L"90 FPS", 90), O(L"100 FPS", L"100 FPS", 100), O(L"120 FPS", L"120 FPS", 120), O(L"144 FPS", L"144 FPS", 144),
    O(L"165 FPS", L"165 FPS", 165), O(L"180 FPS", L"180 FPS", 180), O(L"240 FPS (experimental)", L"240 FPS (experimental)", 240), O(L"Sem limite (experimental)", L"Unlimited (experimental)", 0)};
/* 4.1: no VSync (FIFO) any more, the user's call: Immediate for everyone. 4.3.4: VSync is back as a choice (the user:
   "VSync again in UFC 4 and the others, but every game still starts without it"); Immediate stays the default */
static const Opt PRES4[] = {O(L"Imediato (sem limite, pode rasgar)", L"Immediate (uncapped, may tear)", 2), O(L"Mailbox", L"Mailbox", 0), O(L"VSync (sem rasgar, limita ao monitor)", L"VSync (no tearing, capped to the monitor)", 1)};
static const Opt UPS4[] = {O(L"Desligado (bilinear)", L"Off (bilinear)", 0), O(L"AMD FSR 1.0", L"AMD FSR 1.0", 1)};
static const Opt SHP4[] = {O(L"Desligada", L"Off", 0), O(L"Baixa", L"Low", 1), O(L"Média", L"Medium", 2), O(L"Alta", L"High", 3)};
static const Opt LANG4[] = {O(L"Inglês (EUA)", L"English (US)", 1), O(L"Francês", L"French", 2), O(L"Alemão", L"German", 4), O(L"Italiano", L"Italian", 5),
    O(L"Espanhol (Espanha)", L"Spanish (Spain)", 3), O(L"Espanhol (América Latina)", L"Spanish (Latin America)", 20), O(L"Português (Brasil)", L"Portuguese (Brazil)", 17),
    O(L"Português (Portugal)", L"Portuguese (Portugal)", 7), O(L"Polonês", L"Polish", 16), O(L"Russo", L"Russian", 8), O(L"Japonês", L"Japanese", 0), O(L"Chinês (Tradicional)", L"Chinese (Traditional)", 10)};
static const Opt VOL4[] = {O(L"50%", L"50%", 50), O(L"75%", L"75%", 75), O(L"100%", L"100%", 100), O(L"125%", L"125%", 125), O(L"150%", L"150%", 150), O(L"200%", L"200%", 200), O(L"300%", L"300%", 300)};
#define N(a) (int)(sizeof(a) / sizeof(a[0]))

/* a "field": a dropdown or a switch bound to one key of the port's settings. Keys with a prefix live elsewhere:
   "patch:<kind>" the community patch XML of UFC 2/3, "pini:<key>" portheon.ini, "pgame:<key>" portheon.ini per game
   (<key>_<id>), "online" UFC 3's online_mode (offline/join/host as 0/1/2). */
typedef struct { int label; const wchar_t *key; const Opt *opts; int nopts; int def; BOOL perf; } Field;
static const Field F4_RES = {S_RES, L"resui", RES4, N(RES4), 1}, F4_WIN = {S_WIN, L"display", WIN4, N(WIN4), 0},
    F4_FPS3 = {S_FPS, L"fps_limit", FPS3, N(FPS3), 60}, F4_FPS4 = {S_FPS, L"fps_limit", FPS4X, N(FPS4X), 60},
    F4_PRES = {S_PRESENT, L"present_mode", PRES4, N(PRES4), 0}, F4_UPS = {S_UPSCALE, L"upscaler", UPS4, N(UPS4), 0}, F4_SHP = {S_SHARP, L"sharpness", SHP4, N(SHP4), 2},
    F4_LANG = {S_LANG, L"language", LANG4, N(LANG4), 17}, F4_VOL = {S_VOLUME, L"volume", VOL4, N(VOL4), 100},
    F4_FULL = {S_FULL, L"fullscreen", NULL, 0, 1}, F4_NEO = {S_NEO, L"neo", NULL, 0, 0}, F4_SKIP = {S_SKIP, L"skip_movies", NULL, 0, 1},
    F4_LOWLAT = {S_LOWLAT, L"low_latency", NULL, 0, 0}, F4_ULTRA = {S_ULTRA, L"ultra", NULL, 0, 0},
    F4_PARSEC = {S_PARSEC, L"parsec", NULL, 0, 0};   /* "parsec" lives in portheon.ini (see ParsecSet), not in sacredfire.ini */
/* community patches of UFC 2 and 3 (patches\UFCn-community.xml, by Lepexa and Antigravity): PORTHEON turns them on and
   off in the XML, for every game version in it. 3.5: UFC 4's crowd and object motion blur come back as PORTHEON's own
   lines in UFC4-mouthfix.xml (pini:ufc4_crowd_off / pini:ufc4_blur_off), since UFC 4 keeps its community file off. */
static const Opt PCROWD[] = {O(L"Ligada (original)", L"On (original)", 0), O(L"Desligada (+5 a 10 FPS)", L"Off (+5 to 10 FPS)", 1)};
/* Released: UFC 4's crowd at a quarter (value 2), UFC4 Native Plus's "Medium" crowd (WriteUfc4Patches) */
static const Opt PCROWD4[] = {O(L"Ligada (original)", L"On (original)", 0), O(L"Flashes da torcida a 25%", L"Crowd camera flashes at 25%", 2), O(L"Desligada (+5 a 10 FPS)", L"Off (+5 to 10 FPS)", 1)};
static const Opt PBLUR[] = {O(L"Ligado (original)", L"On (original)", 0), O(L"Desligado (imagem mais nítida)", L"Off (sharper image)", 1)};
static const Field FP_CROWD = {S_PCROWD, L"patch:crowd", PCROWD, N(PCROWD), 0}, FP_BLUR = {S_PBLUR, L"patch:blur", PBLUR, N(PBLUR), 0},
    F4P_CROWD = {S_PCROWD, L"pini:ufc4_crowd_off", PCROWD4, N(PCROWD4), 0}, F4P_BLUR = {S_PBLUR, L"pini:ufc4_blur_off", PBLUR, N(PBLUR), 0};
/* 5.0: UFC 4's shadows, the three "Disable Shadows" community patches (fighters, octagon, all: the shadow map at 1x1
   plus both) as PORTHEON's own lines in UFC4-mouthfix.xml (WriteUfc4Patches); on by default, as the game ships */
static const Opt PSHADOW4[] = {O(L"Ligadas (original)", L"On (original)", 0), O(L"Sem sombra dos lutadores", L"No fighter shadows", 1),
    O(L"Sem sombra no octógono", L"No octagon shadows", 2), O(L"Desligadas (mais FPS)", L"Off (more FPS)", 3)};
static const Field F4P_SHADOW = {S_PSHADOW4, L"pini:ufc4_shadows", PSHADOW4, N(PSHADOW4), 0};
/* 3.5: UFC 3 and 4 can run the engine rebuilt for Intel Arc (games\engines\intel-arc, see EnsureEngine) */
/* 3.5.1: the Arc engine is also the newest SacredFire, with GNSB's Career fixes for UFC 3 (see WantArcEngine) */
static const Opt ENG4[] = {O(L"Automático", L"Automatic", 0), O(L"Padrão do port", L"The port's own", 1), O(L"Motor novo (Carreira e Intel Arc)", L"New engine (Career and Intel Arc)", 2),
                           O(L"Motor do online", L"Online engine", 3)};
static const Field F4_ENGINE = {S_ENGINE, L"pgame:engine", ENG4, N(ENG4), 0};
/* Released: UFC 4 also has the engine of UFC4 Native Plus 1.2.0 (games\engines\native-plus, GPL-2.0, see WantNpEngine);
   it is what Automatic runs there */
static const Opt ENG4N[] = {O(L"Automático", L"Automatic", 0), O(L"Native Plus", L"Native Plus", 4), O(L"Motor novo (Carreira e Intel Arc)", L"New engine (Career and Intel Arc)", 2),
                            O(L"Padrão do port", L"The port's own", 1), O(L"Motor do online", L"Online engine", 3)};
static const Field F4N_ENGINE = {S_ENGINE, L"pgame:engine", ENG4N, N(ENG4N), 0};
/* 4.3.3: UFC 2 has no "new engine" (it stops with code 20 there): its own, or the v3.0 engine */
static const Opt ENG2[] = {O(L"Automático", L"Automatic", 0), O(L"Padrão do port", L"The port's own", 1), O(L"Motor do online", L"Online engine", 3)};
static const Field F2_ENGINE = {S_ENGINE, L"pgame:engine", ENG2, N(ENG2), 0};
static const Field F4_BOOST = {S_BOOST, L"pini:engine_boost", NULL, 0, 1}, F4_MOUTH = {S_MOUTH, L"pini:ufc4_mouth_fix", NULL, 0, 0},
    F4_CAREER = {S_CAREERFPS, L"pini:career_safe_ufc4", NULL, 0, 1};
/* UFC 3 online (its launcher: offline / join / host, sacredfire.ini online_mode) */
static const Opt ONL3[] = {O(L"Offline", L"Offline", 0), O(L"Hospedar (eu crio a sala)", L"Host (I create the room)", 2), O(L"Entrar (na sala do amigo)", L"Join (my friend's room)", 1)};
static const Field F3_ONLINE = {S_ONLINEMODE, L"online", ONL3, N(ONL3), 0};
/* 3.5: the controllers of UFC 1-4. The engine reads them through SDL3, which takes its settings from the environment:
   PORTHEON sets them before each PLAY (see PadEnv). */
static const Opt PADT[] = {O(L"Automático (todos os controles)", L"Automatic (every controller)", 0), O(L"PS5 (DualSense / Edge)", L"PS5 (DualSense / Edge)", 1),
    O(L"PS4 (DualShock 4)", L"PS4 (DualShock 4)", 2), O(L"PS3 (DualShock 3)", L"PS3 (DualShock 3)", 3), O(L"Xbox Series / Xbox One", L"Xbox Series / Xbox One", 4),
    O(L"Xbox 360", L"Xbox 360", 5), O(L"Genérico (USB / DirectInput)", L"Generic (USB / DirectInput)", 6)};
static const Field F4_PADTYPE = {S_PADTYPE, L"pgame:padtype", PADT, N(PADT), 0}, F4_PADBG = {S_PADBG, L"pini:pad_background", NULL, 0, 0};
static const Field F_SHORTCUT = {S_SHORTCUT, L"pini:desktop_shortcut", NULL, 0, 1};   /* 3.5.2: see MakeShortcut */
/* 3.5.3: the whole processor for the engine's helpers, the safe video path after a GPU stop, and a test for the fighters'
   skin colors (see CpuEnv, GpuSafeEnv and SkinEnv) */
static const Opt SKIN4[] = {O(L"Padrão", L"Default", 0), O(L"Alternativa A (cópia na placa de vídeo)", L"Alternative A (copy on the graphics card)", 1),
    O(L"Alternativa B (cópia simples)", L"Alternative B (plain copy)", 2)};
static const Field F4_CPUALL = {S_CPUALL, L"pini:cpu_all", NULL, 0, 1}, F4_GPUSAFE = {S_GPUSAFE, L"pgame:gpu_safe", NULL, 0, 0},
    F4_SKIN = {S_SKINMODE, L"pgame:skin_mode", SKIN4, N(SKIN4), 0};
/* 4.0.1: new pipelines compiled beside the game instead of on its GPU thread (see AsyncEnv) */
static const Field F4_ASYNC = {S_ASYNCPIPE, L"pini:async_pipelines", NULL, 0, 1};
/* 4.2: UFC 6 (games\UFC6\UFC6.ini [UFC6], values exactly as UFC6.exe reads them). Its performance setup is the one
   measured on 2026-10-27/28 (the 8 GB profile): PORTHEON shows what stays free to change. "resolution" is the window's
   size (full screen always takes the monitor's); "quality" 0 lets the game's dynamic resolution go down to 960x544 in
   fights (gpu-timestamp-scale 100: 33-43 fps on a Ryzen 5 5500 + RTX 4060, and the fight runs per frame), 1 keeps
   1600x900 (scale 25, ~30 fps); "language" is the PS5 console language, the same codes as LANGS[].ps4. */
static const Opt RES6[] = {O(L"Automática", L"Automatic", 0), O(L"1280x720", L"1280x720", 1), O(L"1600x900", L"1600x900", 2), O(L"1920x1080 (Full HD)", L"1920x1080 (Full HD)", 3), O(L"2560x1440 (2K)", L"2560x1440 (2K)", 4), O(L"3840x2160 (4K)", L"3840x2160 (4K)", 5), O(L"2560x1080 (ultrawide 21:9)", L"2560x1080 (ultrawide 21:9)", 6), O(L"3440x1440 (ultrawide 21:9)", L"3440x1440 (ultrawide 21:9)", 7), O(L"5120x1440 (super ultrawide 32:9)", L"5120x1440 (super ultrawide 32:9)", 8)};
static const Opt LANG6[] = {O(L"Português (Brasil)", L"Portuguese (Brazil)", 17), O(L"Português (Portugal)", L"Portuguese (Portugal)", 7), O(L"Inglês (EUA)", L"English (US)", 1),
    O(L"Inglês (Reino Unido)", L"English (UK)", 18), O(L"Espanhol", L"Spanish", 3), O(L"Espanhol (América Latina)", L"Spanish (Latin America)", 20), O(L"Francês", L"French", 2),
    O(L"Alemão", L"German", 4), O(L"Italiano", L"Italian", 5), O(L"Holandês", L"Dutch", 6), O(L"Polonês", L"Polish", 16), O(L"Russo", L"Russian", 8), O(L"Turco", L"Turkish", 19),
    O(L"Japonês", L"Japanese", 0), O(L"Coreano", L"Korean", 9), O(L"Chinês (Simplificado)", L"Chinese (Simplified)", 11), O(L"Chinês (Tradicional)", L"Chinese (Traditional)", 10)};
/* the crowd: UFC6.exe skips the stands' people (VS c342cc1c86e12a66) while crowd_off=1; drawn by default since it left
   black blocks over the arena's stands (about 2% FPS). 4.3.4 (UFC6.exe 1.8.0): a game patch instead (the crowd's render
   jobs and animation skipped), the stands stay whole */
static const Opt PCROWD5[] = {O(L"Ligada (original)", L"On (original)", 0), O(L"Desligada (mais FPS)", L"Off (more FPS)", 1)};
static const Field F6_RES = {S_RES5, L"resolution", RES6, N(RES6), 0}, F6_FULL = {S_FULL, L"fullscreen", NULL, 0, 1},
    F6_LANG = {S_LANG, L"language", LANG6, N(LANG6), 17},
    F6_CROWD = {S_PCROWD, L"crowd_off", PCROWD5, N(PCROWD5), 0}, F6_OVERLAY = {S_OVERLAY, L"fps_overlay", NULL, 0, 0},
    F6_ECO = {S_ECO, L"no_power_throttling", NULL, 0, 1};
/* 4.3.4 (UFC6.exe 1.8.0): every fighter of the game unlocked (a community patch, unlock_all in UFC6.ini), off by default */
static const Field F6_UNLOCK = {S_UNLOCK6, L"unlock_all", NULL, 0, 0};
/* 6.0 final (UFC6.exe 1.21.0; the user: "an option to turn off the sparks and the sweat drops when strikes land, and
   cheats such as infinite stamina"): game patches tested in fights, all off by default (UFC6.ini) */
static const Field F6_PARTICLES = {S_COUNT, L"particles_off", NULL, 0, 0}, F6_INFSTAM = {S_COUNT, L"infinite_stamina", NULL, 0, 0},
    F6_NODMG = {S_COUNT, L"no_damage", NULL, 0, 0},
    F6_ACTIONS = {S_COUNT, L"actions_menu", NULL, 0, 0};   /* 6.2 (UFC6.exe 1.23.0): the in-game actions menu, on by default */
/* Released (2026-11-13): UFC 6's experimental game patches (UFC6.exe: wide21, motion_blur_off, shadow_res, crowd_reduced,
   skip_logos in UFC6.ini), found by static analysis of the game's code and not tested in a game yet: all off by default,
   every row says "(experimental)". wide21 only takes effect on a 21:9 window or screen (UFC6.exe checks the size). */
static const Opt SHADOW6[] = {O(L"Do jogo (original)", L"The game's (original)", 0), O(L"Metade da resolução", L"Half resolution", 1), O(L"Um quarto da resolução", L"Quarter resolution", 2)};
static const Field F6_WIDE21 = {S_COUNT, L"wide21", NULL, 0, 0}, F6_BLUR = {S_COUNT, L"motion_blur_off", NULL, 0, 0},
    F6_SHADOWS = {S_COUNT, L"shadow_res", SHADOW6, N(SHADOW6), 0}, F6_CROWDRED = {S_COUNT, L"crowd_reduced", NULL, 0, 0},
    F6_SKIPLOGOS = {S_COUNT, L"skip_logos", NULL, 0, 0};
/* 4.3.5 (UFC6.exe 1.9.0): the game's contextual slow motion off whatever its menu says (no_time_dilation in UFC6.ini), on by default */
static const Field F6_NODIL = {S_NODIL6, L"no_time_dilation", NULL, 0, 1};
/* 4.3: the user's "everything to the maximum" for UFC 6. high_priority grows from a switch to three steps (1 stays above
   normal, the measured default; 2 is Windows' High class). amd_cpu passes the engine's --amd-cpu ("AMD CPU
   compatibility": the instructions an Intel processor computes differently from the PS5's AMD chip made exact; native on
   a Ryzen, so UFC6.exe turns it on there by itself); present picks Immediate (no VSync, the default) or Mailbox.
   4.3.1 (2026-10-29): UFC 5 and 6 run AMD-only instructions at the start (SSE4a INSERTQ/EXTRQ, and UFC 6 SHA-NI); on an
   Intel processor both engines emulate them when they fault, with or without --amd-cpu (KytyPS5's X64InstructionEmulator,
   called by the exception handler for every illegal instruction), so the switch stays the player's choice there. */
static const Opt PRIO6[] = {O(L"Normal", L"Normal", 0), O(L"Acima do normal (recomendado)", L"Above normal (recommended)", 1), O(L"Alta (máximo)", L"High (maximum)", 2)};
static const Opt PRES6[] = {O(L"Imediato (sem VSync)", L"Immediate (no VSync)", 0), O(L"Mailbox (sem rasgar a imagem)", L"Mailbox (no tearing)", 1),
    O(L"VSync (sem rasgar, limita ao monitor)", L"VSync (no tearing, capped to the monitor)", 2)};   /* 4.3.4: VSync (Fifo) back as a choice */
/* 4.3.4: UFC 6's frame limit (UFC6.exe 1.8.0 fps_limit: the engine's own limiter, KYTY_DEBUG_FRAME_LIMIT_MS = 1000/N;
   0 = none, the default, so nothing changes unless a limit is picked). UFC 5's engine has no limiter: its limit is the
   virtual vblank UFC5.exe already sets (vblank in UFC5.ini: 60 = the default since 5.2's smooth mode, 120, 144, 200 Hz) */
static const Opt FPS6[] = {O(L"Ilimitado (padrão)", L"Unlimited (default)", 0), O(L"30 FPS", L"30 FPS", 30), O(L"60 FPS", L"60 FPS", 60), O(L"75 FPS", L"75 FPS", 75), O(L"90 FPS", L"90 FPS", 90), O(L"100 FPS", L"100 FPS", 100),
    O(L"120 FPS", L"120 FPS", 120), O(L"144 FPS", L"144 FPS", 144), O(L"165 FPS", L"165 FPS", 165), O(L"180 FPS", L"180 FPS", 180), O(L"240 FPS", L"240 FPS", 240), O(L"280 FPS", L"280 FPS", 280), O(L"360 FPS", L"360 FPS", 360)};
static const Opt FPS5[] = {O(L"60 FPS (padrão, modo liso)", L"60 FPS (default, smooth mode)", 0), O(L"120 FPS", L"120 FPS", 1), O(L"144 FPS", L"144 FPS", 2), O(L"200 FPS (sem limite prático)", L"200 FPS (practically unlimited)", 3)};   /* 6.0 final: "(padrão)" was still on 120 */
static const Opt PRES5[] = {O(L"Imediato (sem VSync)", L"Immediate (no VSync)", 0), O(L"Mailbox (sem rasgar a imagem)", L"Mailbox (no tearing)", 1),
    O(L"VSync (sem rasgar, limita ao monitor)", L"VSync (no tearing, capped to the monitor)", 2)};
static const Field F6_FPS = {S_FPS, L"fps_limit", FPS6, N(FPS6), 0}, F5_FPS = {S_FPS, L"vblank", FPS5, N(FPS5), 0}, F5_PRES = {S_PRESENT, L"present", PRES5, N(PRES5), 2};   /* 5.2: UFC 5's smooth mode (60 Hz + VSync) is the default, as UFC5.exe 1.32.0 rev 18 */
static const Field F6_PRIO = {S_PRIO6, L"high_priority", PRIO6, N(PRIO6), 1}, F6_AMDCPU = {S_AMDCPU, L"amd_cpu", NULL, 0, 0},
    F6_PRES = {S_PRESENT, L"present", PRES6, N(PRES6), 0};   /* 4.3.2: Mailbox, as UFC6.exe 1.5.0 (Defaults432); 4.3.6: Immediate, as UFC6.exe 1.11.1 (Defaults436) */
/* 4.3 (UFC6.exe 1.2.0, UFC5.exe 1.16.0): the games' own fixes, patched into the game's code at start through the
   engine's --game-patch. realtime: the fight runs at the PS5's pace, 60 steps a second apart from the drawn FPS
   (GameClockUseVBlank kept off; with it the fight ran at the drawn ~30 FPS, half the PS5's pace); audio_watchdog_off:
   the "FB_AUDIOCORE watchdog" thread, which kept one processor thread at 96-99% in these engines, is not started.
   Both keys are in UFC6.ini and UFC5.ini. 4.3.1 (UFC5.exe 1.17.0): UFC 5's fight loop reaches only ~25 frames a second
   here, so its realtime is a time scale UFC5.exe sets from the measured FPS instead of a patch. */
static const Field F6_REALTIME = {S_REALTIME, L"realtime", NULL, 0, 1}, F6_AUDIOWD = {S_AUDIOWD, L"audio_watchdog_off", NULL, 0, 1};
/* 5.0 (players: "the fight is too fast"): UFC 5's fight speed. With the clock patch the fight moves at the PS5's pace
   (round clock 1.69x of real time, fast by design); UFC5.exe 1.29.0 scales the game's TimeScale by this percentage.
   S_COUNT: its name comes from FieldName (ui_items.c), not the string table. */
static const Opt SPEED5[] = {{L"100% (igual ao PS5)", L"100% (as on the PS5)", 100}, {L"95%", L"95%", 95}, {L"90% (padrão)", L"90% (default)", 90}, {L"85%", L"85%", 85}, {L"80% (mais lenta)", L"80% (slowest)", 80}};
static const Field F5_SPEED = {S_COUNT, L"fight_speed", SPEED5, 5, 90};
/* 4.3.2: the UFC 6 engine's FXAA over the presented frame (UFC6.exe 1.5.0 passes --fxaa true); off by default, the game's own look */
static const Field F6_FXAA = {S_FXAA, L"fxaa", NULL, 0, 0};
/* 4.3.2: UFC 5's engine has no FXAA: UFC5.exe 1.20.0 enables PORTHEON's Vulkan layer (games\UFC5\bin\PortheonFXAA.dll),
   which draws the UFC 6 engine's FXAA over every presented frame; the same key in UFC5.ini */
static const Field F5_FXAA = {S_FXAA, L"fxaa", NULL, 0, 0};
/* 4.3.2: UFC 6's "zero input lag" switch: UFC6.exe 1.5.0 passes --gpu-frames-ahead 0 instead of 1 (up to one frame less
   between the pad and the screen, for some FPS); on by default (the engine's own setting, and the safer one) */
static const Field F6_LOWLAT = {S_LOWLAT6, L"low_latency", NULL, 0, 1};   /* on by default from UFC6.exe 1.5.0 (Defaults432) */
/* 4.3.2: two of the game's render settings patched at load by UFC6.exe 1.5.0 (GAME_PATCHES): no depth of field (on by
   default: the heaviest GPU pass of menus, fighter select and walkouts in this engine) and a 75% 3D resolution (off) */
static const Field F6_DOF = {S_DOF6, L"dof_off", NULL, 0, 1};   /* 5.1: render_scale_75's switch went into F6_RSCALE */
/* 5.1 (UFC6.exe 1.16.0, the user: "FSR modes Quality / Balanced / Performance" and "High 1080p, Ultra 2K, Max 4K"): the size
   the game draws its 3D picture at before it scales it to the screen (render_scale in UFC6.ini, percent of 1920x1080;
   0 = from the profile, as before). Above 100 the game draws bigger and scales down (2K: 2560x1440, checked). */
/* 6.0 (UFC6.exe 1.19.0, the user: "480p and 720p for weak PCs, in every UFC"): 360p (33%) and 480p (44%) for weak PCs,
   and 67% is now the exact 720p (2/3: 1280x720) */
static const Opt RSCALE6[] = {{L"Automático (pelo perfil)", L"Automatic (from the profile)", 0},
    {L"360p 33% (640x360, PC muito fraco)", L"360p 33% (640x360, very weak PC)", 33}, {L"480p 44% (854x480, PC fraco)", L"480p 44% (854x480, weak PC)", 44},
    {L"540p 50% (960x540, desempenho)", L"540p 50% (960x540, performance)", 50},
    {L"Equilíbrio 59% (1133x637)", L"Balanced 59% (1133x637)", 59}, {L"720p 67% (1280x720, qualidade)", L"720p 67% (1280x720, quality)", 67},
    {L"Alta 75% (1440x810)", L"High 75% (1440x810)", 75}, {L"1080p nativa 100% (1920x1080)", L"1080p native 100% (1920x1080)", 100},
    {L"2K 133% (2560x1440, placa de 8 GB ou mais)", L"2K 133% (2560x1440, 8 GB card or more)", 133}, {L"4K 200% (3840x2160, placa de 12 GB ou mais)", L"4K 200% (3840x2160, 12 GB card or more)", 200},
    {L"Escala 4K do próprio jogo (3200x1800 nas lutas, muito pesada, 12 GB ou mais)", L"The game's own 4K scale (3200x1800 in fights, very heavy, 12 GB or more)", 300}};   /* 6.2: UFC6.exe 1.26.0 */
static const Field F6_RSCALE = {S_COUNT, L"render_scale", RSCALE6, N(RSCALE6), 0};
/* 6.0 (UFC5.exe 1.33.0, the same request): UFC 5's 3D picture at a lower size through FSR2's ratio, percent of the game's
   own size (render_scale in UFC5.ini); the sizes are at the engine's 1080p-TV output (1600x900, the usual one below 4K) */
static const Opt RSCALE5[] = {{L"Normal (1068x600, o do jogo)", L"Normal (1068x600, the game's)", 100}, {L"540p 90% (960x540)", L"540p 90% (960x540)", 90},
    {L"480p 80% (856x480, PC fraco)", L"480p 80% (856x480, weak PC)", 80}, {L"FSR 2 Desempenho 75% (800x452)", L"FSR 2 Performance 75% (800x452)", 75},
    {L"400p 67% (712x400)", L"400p 67% (712x400)", 67}, {L"360p 60% (640x360, PC muito fraco)", L"360p 60% (640x360, very weak PC)", 60},
    {L"FSR 2 Ultra desempenho 50% (536x300)", L"FSR 2 Ultra performance 50% (536x300)", 50}};
static const Field F5_RSCALE = {S_COUNT, L"render_scale", RSCALE5, N(RSCALE5), 100};
/* 6.0 final (the user: "720p and 480p for weak PCs, for every UFC"): UFC 2, 3 and 4 draw their 3D scene at a smaller size
   through the community "Resolution WxH" patches (UFC 2/3: their community XML, every version; UFC 4: Lepexa's lines,
   from the UFC2-UFC4-PC-Port v3.4 XML, in PORTHEON's UFC4-mouthfix.xml). The game's own size is 1600x900; the window
   and the menus stay at the chosen output. Not with 2K/4K output (pgame:scene, SceneRes / Ufc4Prepare) */
static const Opt PSCENE[] = {{L"Do jogo (1600x900)", L"The game's (1600x900)", 0}, {L"720p (1280x720, mais FPS)", L"720p (1280x720, more FPS)", 1},
    {L"540p (960x540, PC fraco)", L"540p (960x540, weak PC)", 2}, {L"450p (800x450, PC muito fraco)", L"450p (800x450, very weak PC)", 3},
    {L"360p (640x360, o mínimo)", L"360p (640x360, the least)", 4}};
static const Field F4P_SCENE = {S_COUNT, L"pgame:scene", PSCENE, N(PSCENE), 0};
/* 4.0.2: UFC 5 (games\UFC5\UFC5.ini, values exactly as UFC5.exe stores them). Its performance setup is UFC5.exe's own
   (--auto-config, the setup that gave this engine the most FPS): PORTHEON shows what stays free to change. 4.3: back
   from 4.0.6 next to UFC 6. */
static const Opt RES5[] = {O(L"Nativa do monitor", L"Monitor native", 0), O(L"1280x720", L"1280x720", 1), O(L"1600x900", L"1600x900", 2), O(L"1920x1080 (Full HD)", L"1920x1080 (Full HD)", 3), O(L"2560x1440 (2K)", L"2560x1440 (2K)", 4), O(L"3840x2160 (4K)", L"3840x2160 (4K)", 5), O(L"2560x1080 (ultrawide 21:9)", L"2560x1080 (ultrawide 21:9)", 6), O(L"3440x1440 (ultrawide 21:9)", L"3440x1440 (ultrawide 21:9)", 7), O(L"5120x1440 (super ultrawide 32:9)", L"5120x1440 (super ultrawide 32:9)", 8)};
static const Opt LANG5[] = {O(L"Português (Brasil)", L"Portuguese (Brazil)", 0), O(L"Português (Portugal)", L"Portuguese (Portugal)", 1), O(L"Inglês (EUA)", L"English (US)", 2),
    O(L"Inglês (Reino Unido)", L"English (UK)", 3), O(L"Espanhol", L"Spanish", 4), O(L"Espanhol (América Latina)", L"Spanish (Latin America)", 5), O(L"Francês", L"French", 6),
    O(L"Alemão", L"German", 7), O(L"Italiano", L"Italian", 8), O(L"Holandês", L"Dutch", 9), O(L"Polonês", L"Polish", 10), O(L"Russo", L"Russian", 11), O(L"Turco", L"Turkish", 12),
    O(L"Japonês", L"Japanese", 13), O(L"Coreano", L"Korean", 14), O(L"Chinês (Simplificado)", L"Chinese (Simplified)", 15), O(L"Chinês (Tradicional)", L"Chinese (Traditional)", 16)};
static const Opt VIB5[] = {O(L"0%", L"0%", 0), O(L"25%", L"25%", 25), O(L"50%", L"50%", 50), O(L"75%", L"75%", 75), O(L"100%", L"100%", 100)};
static const Field F5_RES = {S_RES5, L"resolution", RES5, N(RES5), 0}, F5_FULL = {S_FULL, L"fullscreen", NULL, 0, 1},
    F5_VIB = {S_VIB, L"controller_vibration", VIB5, N(VIB5), 100}, F5_LANG = {S_LANG, L"language", LANG5, N(LANG5), 0},
    F5_SPK = {S_SPKVOL, L"controller_volume", VIB5, N(VIB5), 50}, F5_FASTSH = {S_FASTSH, L"fast_shaders", NULL, 0, 1},
    F5_ECO = {S_ECO, L"no_power_throttling", NULL, 0, 1}, F5_PRIO = {S_PRIO, L"high_priority", NULL, 0, 1},
    F5_DIAG = {S_DIAG, L"diag_full_next", NULL, 0, 0};
/* 4.0.3: UFC 5's crowd (UFC5.exe 1.15 skips its shaders while crowd_off=1: about +10% FPS in a fight then). 4.3.2:
   UFC5.exe 1.20.0 writes crowd_off by the PC's tier (drawn on strong PCs, where it no longer costs FPS in the 1080p
   mode); the default here only shows while the key is missing */
static const Field F5_CROWD = {S_PCROWD, L"crowd_off", PCROWD5, N(PCROWD5), 1};
/* 4.3.2: the "live broadcast camera" (the user's name for it: a close camera behind a fighter, over the shoulder),
   the community's UFC 2 mod carried to every UFC (the user's call, 2026-10-31: an option, off in every game). UFC 1-4: patches\UFCn-camera-3a-pessoa.xml, on while pgame:cam3p is (WriteCam3p);
   UFC 5 and 6: cam3p in UFC5.ini / UFC6.ini, patched at load by the game's launcher (GAME_PATCHES). In a fight, Select
   (View on an Xbox pad, the touchpad on a PlayStation pad) switches the stand-up camera to behind a fighter and back.
   6.1: in UFC 1, 3 and 4 the switch is now UFC 6's TV camera (UFC 3/4: still cage-side cameras with cuts; UFC 1: one
   cage-side camera that follows the fight); UFC 2 keeps the community mod, UFC 5 its four cameras. */
static char *ReadText(const wchar_t *p, DWORD *len);   /* ui_keys.c */
static const Field F4_CAMFP = {S_COUNT, L"pgame:camfp", NULL, 0, 0};   /* 6.2 */
static const Field F4_CAM3P = {S_CAM3P, L"pgame:cam3p", NULL, 0, 0}, F56_CAM3P = {S_CAM3P, L"cam3p", NULL, 0, 0};
/* 4.3.3: graphics presets (the user's "low, medium, high and ultra, for all"), the first row of every Video tab.
   Automatic, the default, is PORTHEON's own setup for this PC (BestSettings): the performance measured so far stays
   untouched until a player picks another. Low / Medium / High / Ultra set only options each game already has (GfxPreset). */
/* 6.2 (the user: "textures low/medium/high/ultra", "3440x1440", "HDR"): what UFC 1-4's engine really has: BB_ANISO (the texture filter), BB_WINDOW_SIZE, BB_HDR */
static const Opt TEXO[] = {O(L"Padrão do jogo", L"Game default", 0), O(L"Baixo (mais suave)", L"Low (softer)", 1), O(L"Médio (filtro 4x)", L"Medium (4x filter)", 2), O(L"Alto (filtro 8x)", L"High (8x filter)", 3), O(L"Ultra (filtro 16x, mais nítido)", L"Ultra (16x filter, sharper)", 4)};
/* Released: the engine's temporal upscalers (BB_UPSCALER: fsr3, fsr4, fsr411, taa), the list of UFC4 Native Plus's
   launcher, for UFC 1 to 4 (every SacredFire engine here has them; FSR 4 falls back to FSR 3.1 by itself) */
static const Opt TUPS[] = {O(L"Desligado", L"Off", 0), O(L"AMD FSR 3.1", L"AMD FSR 3.1", 1), O(L"AMD FSR 4 (ou 3.1 sem suporte)", L"AMD FSR 4 (3.1 if unsupported)", 2),
    O(L"AMD FSR 4.1.1 (experimental)", L"AMD FSR 4.1.1 (experimental)", 3), O(L"TAA (antisserrilhado temporal)", L"TAA (temporal anti-aliasing)", 4)};
static const Field F_TUPS = {S_COUNT, L"pgame:tups", TUPS, N(TUPS), 0}, F4_WIDE21 = {S_COUNT, L"pgame:wide21", NULL, 0, 0};
static const Field F_TEX = {S_COUNT, L"pgame:tex", TEXO, N(TEXO), 0}, F4_UW = {S_COUNT, L"pgame:ultrawide", NULL, 0, 0}, F4_HDR = {S_COUNT, L"pgame:hdr", NULL, 0, 0}, F6_HDR = {S_COUNT, L"hdr", NULL, 0, 0};
/* Released (2026-11-13, the owner: "every option UFC 4's engine has, in UFC 1, 2 and 3 too, and in 6 if possible; only
   the DLC stays in 4"): what UFC4 Native Plus 1.2.0's launcher (GPL-2.0, Abdullah Tech) has and UFC 1-3 lacked.
   Engine side (NpOptsEnv; every SacredFire engine here reads these variables): the frame statistics, the number of
   draw-preparation threads (value 17 = none), the low-end profile (no preparation threads on a CPU with up to 4
   threads) and three more internal sizes (never run on these games: experimental).
   Game side (WriteOwnPatches, patches\UFCn-portheon.xml; NOT YET TESTED IN A FIGHT, so every label says experimental):
   the shadow map at half and a quarter of its size, UFC 1's crowd off, UFC 3's crowd camera flashes at a quarter (what
   Native Plus's "reduced crowd" really changes: the emit rate of the effect "glare_flash").
   UFC 6 (UFC6.ini): the engine's master volume (UFC6.exe passes --audio-master-volume) and the shader compile threads
   (compile_threads, a key UFC6.exe already had). */
static const Opt PREPO[] = {O(L"Automático", L"Automatic", 0), O(L"0 (sem threads de preparo)", L"0 (no preparation threads)", 17), O(L"1", L"1", 1), O(L"2", L"2", 2),
    O(L"4", L"4", 4), O(L"6", L"6", 6), O(L"8", L"8", 8), O(L"12", L"12", 12), O(L"16", L"16", 16)};
static const Opt IRESO[] = {O(L"Pela lista Resolução", L"From the Resolution list", 0), O(L"854x480 (experimental)", L"854x480 (experimental)", 1),
    O(L"960x540 (experimental)", L"960x540 (experimental)", 2), O(L"1600x900 (experimental)", L"1600x900 (experimental)", 3)};
static const Opt SHADQ[] = {O(L"Alta (original)", L"High (original)", 0), O(L"Média: mapa pela metade", L"Medium: half-size map", 1),
    O(L"Baixa: mapa a um quarto", L"Low: quarter-size map", 2)};
static const Field F4_STATS = {S_COUNT, L"pgame:frame_stats", NULL, 0, 0}, F4_LOWEND = {S_COUNT, L"pgame:lowend", NULL, 0, 0},
    F4_PREP = {S_COUNT, L"pgame:prep_workers", PREPO, N(PREPO), 0}, F4_IRES = {S_COUNT, L"pgame:ires", IRESO, N(IRESO), 0},
    F13_SHADOW = {S_COUNT, L"pgame:shadowq", SHADQ, N(SHADQ), 0}, F1_CROWDOFF = {S_COUNT, L"pgame:crowdoff", NULL, 0, 0},
    F3_CROWD25 = {S_COUNT, L"pgame:crowd25", NULL, 0, 0};
static const Opt VOL6[] = {O(L"0% (sem som)", L"0% (muted)", 0), O(L"25%", L"25%", 25), O(L"50%", L"50%", 50), O(L"75%", L"75%", 75), O(L"100% (padrão)", L"100% (default)", 100),
    O(L"125%", L"125%", 125), O(L"150%", L"150%", 150), O(L"200%", L"200%", 200)};
static const Opt CTHR6[] = {O(L"Automático (pelo PC)", L"Automatic (by the PC)", 0), O(L"1", L"1", 1), O(L"2", L"2", 2), O(L"4", L"4", 4), O(L"6", L"6", 6), O(L"8", L"8", 8)};
static const Field F6_VOL = {S_VOLUME, L"volume", VOL6, N(VOL6), 100}, F6_CTHREADS = {S_COUNT, L"compile_threads", CTHR6, N(CTHR6), 0};
/* Released (7.0): UFC 6's new engine and its PS5 Pro mode (UFC6.exe: new_engine / pro_mode in UFC6.ini, games\UFC6\novo) */
static const Field F6_NEWENG = {S_COUNT, L"new_engine", NULL, 0, 0}, F6_PRO = {S_COUNT, L"pro_mode", NULL, 0, 0};
/* 11-12 (the user: "leave it on already; the player only picks the DLSS preset, which goes with the Pro mode"): one
   list, dlss in UFC6.ini (UFC6.exe: 0 automatic = quality where DLSS can run, 1 off, 2..6 the presets, 7 Pro only) */
static const Opt DLSS6[] = {O(L"Automático (Desempenho, se a placa for RTX)", L"Automatic (Performance, on an RTX card)", 0),
    /* measured 11-12: ultra performance (2) came out as a plain stretch and DLAA (6) as a black fight, so they are not
       offered; UFC6.exe plays 2 as performance and 6 as quality */
    O(L"Desempenho (mais FPS)", L"Performance (more FPS)", 3), O(L"Equilibrado", L"Balanced", 4),
    O(L"Qualidade (mais nitidez)", L"Quality (sharper)", 5),
    O(L"Só o modo PS5 Pro, sem DLSS", L"PS5 Pro mode only, no DLSS", 7), O(L"Desligado (motor de sempre)", L"Off (the usual engine)", 1)};
static const Field F6_DLSS = {S_COUNT, L"dlss", DLSS6, N(DLSS6), 0};
/* 11-13 (the user: "the PS5 Pro mode as an option, DLSS as an option, and when it is turned on its presets appear, like
   FSR"): two switches and the preset list, all three views of the same dlss key of UFC6.ini (GetInt / PutInt: pro6,
   dlss6, dlssq6). DLSS here is made from the Pro mode's upscale requests, so DLSS on means the Pro mode on; the preset
   last chosen is remembered in dlss_preset. Automatic (0, a fresh install) shows as both on with Performance. */
/* 11-13: which online server files the host runs (Ufc2ServerStart): see the comment there */
static const Opt SRV38[] = {O(L"Padrão (v3.4, a de sempre)", L"Standard (v3.4, as before)", 0), O(L"Nova (v3.8: estatísticas do FighterNet)", L"New (v3.8: FighterNet stats)", 1),
    O(L"Nova + loja do Ultimate Team (experimental)", L"New + Ultimate Team store (experimental)", 2)};
static const Field F_SRV38 = {S_COUNT, L"pini:online_srv38", SRV38, N(SRV38), 0};
static const Opt DLSSQ6[] = {O(L"Desempenho (mais FPS)", L"Performance (more FPS)", 3), O(L"Equilibrado", L"Balanced", 4), O(L"Qualidade (mais nitidez)", L"Quality (sharper)", 5)};
static const Field F6_PROSW = {S_COUNT, L"pro6", NULL, 0, 1}, F6_DLSSSW = {S_COUNT, L"dlss6", NULL, 0, 1}, F6_DLSSQ = {S_COUNT, L"dlssq6", DLSSQ6, N(DLSSQ6), 3};
/* 7.1 (the owner: AMD FSR as an option in UFC 6, like DLSS): the DLSS switch became the upscaler list, upscaler in
   UFC6.ini (0 automatic = DLSS on an RTX card where novo\dlss\nvngx_dlss.dll is, else the usual engine; 1 NVIDIA DLSS;
   2 AMD FSR from novo\fsr; 3 is the game's own FSR 2, not in 7.1), shown while the Pro switch is on. "None" (9 here) is
   dlss=7, the Pro mode alone. The AMD row is offered only when games\UFC6\novo\fsr holds AMD's loader
   (amd_fidelityfx_loader_vk.dll, SDK 2.4: FSR 4 where the card and the driver have it, else FSR 3.1) or
   amd_fidelityfx_vk.dll (SDK 1.1.4, FSR 3.1 only): F6_UPS with it, F6_UPSN without. GetInt / PutInt "ups6". */
static const Opt UPS6[] = {O(L"Automático (DLSS se a placa for RTX)", L"Automatic (DLSS on an RTX card)", 0), O(L"NVIDIA DLSS (placas RTX)", L"NVIDIA DLSS (RTX cards)", 1),
    O(L"AMD FSR 4 (FSR 3.1 nas placas sem FSR 4), experimental", L"AMD FSR 4 (FSR 3.1 on cards without FSR 4), experimental", 2),
    O(L"Nenhum (só o modo PS5 Pro)", L"None (PS5 Pro mode only)", 9)};
static const Opt UPS6N[] = {O(L"Automático (DLSS se a placa for RTX)", L"Automatic (DLSS on an RTX card)", 0), O(L"NVIDIA DLSS (placas RTX)", L"NVIDIA DLSS (RTX cards)", 1),
    O(L"Nenhum (só o modo PS5 Pro)", L"None (PS5 Pro mode only)", 9)};
static const Field F6_UPS = {S_COUNT, L"ups6", UPS6, N(UPS6), 0}, F6_UPSN = {S_COUNT, L"ups6", UPS6N, N(UPS6N), 0};
/* FSR also takes ultra performance (DLSS's came out as a plain stretch, 11-12; UFC6.exe keeps 2 for FSR) */
static const Opt UPSQF6[] = {O(L"Ultra desempenho (mais FPS ainda)", L"Ultra performance (even more FPS)", 2), O(L"Desempenho (mais FPS)", L"Performance (more FPS)", 3),
    O(L"Equilibrado", L"Balanced", 4), O(L"Qualidade (mais nitidez)", L"Quality (sharper)", 5)};
static const Field F6_UPSQF = {S_COUNT, L"dlssq6", UPSQF6, N(UPSQF6), 3};
/* 7.1 (lane ufc6-filmgrain-re): the game's own "off" paths, patched by UFC6.exe (film_grain_off: FilmGrainEnable;
   film_fx_off: the filmic pass with chromatic aberration and lens distortion, the vignette and the lens dirt; rt_off:
   the game's ray tracing check answers "unsupported", every mode). Static analysis only, never run in a game: off by
   default, every label says experimental. */
static const Field F6_GRAIN = {S_COUNT, L"film_grain_off", NULL, 0, 0}, F6_FILMFX = {S_COUNT, L"film_fx_off", NULL, 0, 0}, F6_RTOFF = {S_COUNT, L"rt_off", NULL, 0, 0};
/* 7.3 (the owner: "cada preset de acordo: muito baixo, baixo, medio, alto, ultra, maximo, extremo"): seven steps. Very
   low (8) and Extreme (9) are new; the stored numbers of the old ones stay, so a saved preset keeps its meaning. */
static const Opt GFXP[] = {O(L"Automático (o melhor para este PC)", L"Automatic (the best for this PC)", 0),
                           O(L"Muito baixo (PC muito fraco, o máximo de FPS)", L"Very low (very weak PC, the most FPS)", 8), O(L"Baixo (mais FPS)", L"Low (more FPS)", 1),
                           O(L"Médio", L"Medium", 2), O(L"Alto (1080p)", L"High (1080p)", 3), O(L"Ultra (2K)", L"Ultra (2K)", 4),
                           O(L"Máximo (4K)", L"Max (4K)", 6),   /* 5.1 */
                           O(L"Extremo (4K, o mais pesado)", L"Extreme (4K, the heaviest)", 9),
                           O(L"Ultrawide (3440x1440)", L"Ultrawide (3440x1440)", 7),   /* 6.2 */
                           O(L"Personalizado (você mudou uma opção)", L"Custom (you changed a setting)", 5)};   /* 4.3.5: PresetTouched */
static const Field F_GFX = {S_GFX, L"pgame:gfx_preset", GFXP, N(GFXP), 0};
/* 5.0 (the user: "AMD FSR as an option in every UFC's settings"): FSR 1 through PORTHEON's Vulkan layer
   (games\engines\fsr\PortheonFSR.dll, FsrEnv): the game draws at this percentage of the screen and FSR 1 enlarges and
   sharpens it; 100 = the full size, only sharpened. S_COUNT: named by FieldName (ui_items.c). */
static const Opt FSRO[] = {{L"Desligado", L"Off", 0}, {L"Ultra qualidade (77%)", L"Ultra quality (77%)", 77}, {L"Qualidade (67%)", L"Quality (67%)", 67},
                           {L"Equilíbrio (59%)", L"Balanced (59%)", 59}, {L"Desempenho (50%)", L"Performance (50%)", 50}, {L"Só nitidez (100%)", L"Sharpen only (100%)", 100}};
static const Field F_FSR = {S_COUNT, L"pgame:fsr", FSRO, 6, 0};
/* UFC 5 and 6 (measured 2026-11-05): their engines always draw at the window's size, so FSR 1 is its sharpening there */
static const Opt FSRO5[] = {{L"Desligado", L"Off", 0}, {L"Nitidez FSR 1 (recomendado)", L"FSR 1 sharpening (recommended)", 100}, {L"Nitidez FSR 1 forte", L"Strong FSR 1 sharpening", 101}};
static const Field F_FSR5 = {S_COUNT, L"pgame:fsr", FSRO5, 3, 0};
/* 5.0 (the user: "UFC 5 with graphics options like UFC 6"): the UFC5.ini keys UFC5.exe already reads, shown as UFC 6's
   are: its FPS counter (fps_overlay: 0 off, 1 top left, 2 top right), the shadows and the empty seats behind the cage
   (shadows_off / seats_off, skipped shaders; UFC5.exe writes them by the PC's tier when absent) and the game's own FSR 2
   (fsr2_off=1: the engine's F9 mode; off by default since UFC5.exe 1.30.0, rev 17). S_COUNT: named by FieldName (ui_items.c). */
/* 6.0 final (UFC5.exe 1.34.0): the counter is a window of its own, so it shows only with the game in a window: over a
   full-screen game it made Windows compose every frame (more input lag, no G-Sync/FreeSync) */
static const Opt OVL5[] = {{L"Desligado", L"Off", 0}, {L"No canto esquerdo, só em janela (padrão)", L"Top left, windowed only (default)", 1}, {L"No canto direito, só em janela", L"Top right, windowed only", 2}};
static const Field F5_OVL = {S_COUNT, L"fps_overlay", OVL5, 3, 1}, F5_SHADOWS = {S_COUNT, L"shadows_off", NULL, 0, 1},
    F5_SEATS = {S_COUNT, L"seats_off", NULL, 0, 1}, F5_FSR2 = {S_COUNT, L"fsr2_off", NULL, 0, 0};   /* defaults: what UFC5.exe reads when a key is missing */
/* 4.3.3: the frame generator MFG SUPER MOTION (NODIX TECH, a separate, unsigned program PORTHEON does not ship): with
   pgame:mfg on, PLAY also starts the player's own install, with the player's own settings (MfgStart) */
static const Field F_MFG = {S_MFG, L"pgame:mfg", NULL, 0, 0};
#define MFG_URL L"https://github.com/NODIX-TECH/MFG-SUPER-MOTION/releases/tag/1.0.4"
/* 4.3.3 (2026-10-31): UFC 3 and 4's XML = the camera modes (normal, 3rd person = the mod, live broadcast = a TV director,
   1st person; _build-tools\camera3p\ufc3\modes2, ufc4\modes3); UFC 2's modes are the cheat pack games\UFC2\cheats\
   ufc2-camera.json; UFC5.exe 1.23.0 the same four modes, UFC6.exe 1.7.0 normal <-> the mod only (the user's call) */
#define CAM3P_UFC5 1   /* UFC5.exe / UFC6.exe carry the camera's GAME_PATCHES: the switch shows for that game */
#define CAM3P_UFC6 1

/* ---------------------------------------------------------------- state */
static HINSTANCE g_inst;
static HWND g_wnd, g_editGame, g_editPlayer, g_editAddr;
static int g_dpi = 96, g_sel = 3, g_hot = -1;
static HFONT g_fTitle, g_fCard, g_fText, g_fSmall, g_fSection, g_fPlay, g_fBtn, g_fFoot, g_fTag, g_fHero, g_fBrand, g_fNav, g_fH2, g_fTile, g_fBody;
static HBRUSH g_brField;
static wchar_t g_exeDir[MAX_PATH], g_pIni[MAX_PATH], g_msg[1024];   /* 3.5.3: room for the longer explanations */
static COLORREF g_msgColor = C_SUB;
static ULONG_PTR g_gdip;
static GpImage *g_imgBg, *g_imgLogo;
static HANDLE g_job, g_proc; static int g_procGame = -1;
static wchar_t g_ver[NGAMES][48];
static ULONGLONG g_playStart;
static DWORD g_stTick;   /* ui.c: when the games' status was last read (0: read again at the next paint) */
/* 4.0 views (ui.c), like a console: the boot animation, "who is playing?", the home screen with the games, one game's
   hub (its sections are the tabs), the system settings, the install helper and the online center; an overlay on
   top (the update, the news, the profile editor) */
/* 4.4.0: V_WELCOME = "create your account" (once, after the boot), V_LAUNCH = the game's picture while it loads (the
   PLAY instance, until the game's window shows); OV_PROFMENU = the profile card under the top bar's picture */
enum { V_BOOT, V_PROFILES, V_HOME, V_GAME, V_SETTINGS, V_INSTALL, V_ONLINE, V_WELCOME, V_LAUNCH };
enum { OV_NONE, OV_GUIDE, OV_UPDATE, OV_NEWS, OV_PROFEDIT, OV_PROFMENU, OV_KEYS /* 6.0: the keyboard screen (ui_keys.c) */ };
enum { TAB_OVERVIEW, TAB_VIDEO, TAB_GAMEPLAY, TAB_ONLINE, TAB_SAVES, TAB_CHEATS, TAB_DLC, TAB_EXTRAS, TAB_GAME, TAB_GUIDE, TAB_VITRINE /* 6.2: the vitrine's videos */, NTABS };
enum { SET_LANG, SET_PROFILES, SET_UI, SET_UPDATES, SET_ABOUT, NSETS };
static int g_view = V_BOOT, g_tab = TAB_OVERVIEW, g_overlay = OV_NONE, g_guideGame = 3, g_guideStep = 0, g_setSec = SET_LANG;
static int g_viewBack = V_HOME;   /* where Esc goes from the settings, the installer and the online center */
static int g_launchGame = -1; static DWORD g_launchT0;   /* 4.4.0: V_LAUNCH: which game is loading, since when */
static void LaunchShow(int gi, BOOL fadeIn);   /* 4.4.0 (ui_screens.c, ui_input.c) */
static void UiFreeMemory(void);

#define SC(x) MulDiv((x), g_dpi, 96)
static DWORD g_anim;   /* milliseconds, for the effects */
/* 5.0 ("super optimized for everyone"): g_calm freezes the effects that loop (pulses, turning edges) when nobody has
   touched PORTHEON for a while, when it is not in front, or in the light effects tier (g_fxLight: weak PCs, remote
   desktop, battery saver, or the player's choice in Settings > Interface); g_lastInput: the last real input */
static BOOL g_calm, g_fxLight, g_fxLightHard; static DWORD g_lastInput;   /* 6.2 fix: g_fxLightHard = light chosen in Settings or a very weak PC (not only battery saver / remote desktop) */
static float Pulse(DWORD period) { if (g_calm) return 0.5f; return 0.5f + 0.5f * sinf((float)(g_anim % period) / period * 6.2831853f); }

/* hit regions, rebuilt on every paint; 4.0: each one is also a place the keyboard and the controller can move to */
enum { H_CARD = 0, H_TPLAY = 8, H_SYSTILE = 13, H_DROP = 20, H_SWITCH = 60, H_TAB = 100, H_NAV = 120, H_SETSEC = 128, H_LANGPICK = 140,
       H_PROFPICK = 160, H_SAVEITEM = 170, H_CHEAT = 200, H_LINK = 240, H_INSTSTEP = 267 /* 6.1: H_GUIDEGAME moved to 760 */, H_COLOR = 273, H_AVATAR = 285,
       H_HOMECARD = 296,
       H_BROWSE = 300, H_PORT, H_ADV, H_LOGS, H_PLAY, H_ABOUT, H_EN, H_PT, H_NAVLIB, H_NAVGUIDE, H_NAVNEWS, H_UPDATE, H_BACK, H_CONFIG,
       H_GUIDE, H_SCAN, H_OPENGAME, H_BESTCFG, H_HOSTIP, H_COPYIP, H_PARSECDL, H_REBORNON, H_REBORNUPD, H_GPREV, H_GNEXT, H_GCLOSE,
       H_GPICK, H_GSCAN, H_UPDGO, H_UPDLATER, H_UPDCANCEL, H_NEWSCLOSE, H_TUTOR, H_PORTWIN, H_PADPICK,
       H_SOON, H_SOON_LAST = H_SOON + 1, H_MSG,
       H_SHORTCUT, H_STEAM, H_CRED_AUTHOR, H_CRED_DISCORD,
       /* 4.0 */
       H_PROFILE, H_PROFADD, H_PROFMANAGE, H_PROFSAVE, H_PROFDEL, H_PROFCANCEL, H_OPTIONS, H_VERIFY, H_SAVEBACKUP, H_SAVERESTORE,
       H_SAVEOPEN, H_SAVEAUTO, H_SAVEDELBAK, H_DLCOPEN, H_DLCHELP, H_CHEATSOFF, H_CHEATSINFO, H_FULLSCREEN, H_UPDCHECK, H_TOAST,
       H_INSTPICK, H_INSTSCAN, H_INSTVERIFY, H_INSTNEXT, H_INSTPREV, H_ONLINEGAME, H_SETTINGS, H_POWER, H_CHANGELOG, H_SKIPBOOT,
       H_GITHUB, H_HELPKEYS, H_PROFEDIT, H_LANGBTN, H_REPORT, H_BENCH, H_WARMUP, H_PCCHECK,   /* 4.3.5; 4.3.7 H_WARMUP; 4.3.8 H_PCCHECK */
       /* 4.4.0: the account's photo and status, the profile card, the welcome screen */
       H_PHOTO, H_PHOTODEL, H_PSTAT, H_PSTAT_LAST = H_PSTAT + 2, H_PMEDIT, H_PMSWITCH, H_PMADD, H_PMMANAGE, H_WELCOMEGO, H_WELCOMESKIP,
       H_PORTCFG,   /* 5.0: UFC Undisputed 3: its port's own configurator (UFC3.exe --configure) */
       H_FXPICK, H_FXPICK_LAST = H_FXPICK + 2,   /* 5.0: Settings > Interface > visual effects (automatic, full, light) */
       H_KEYS, H_KEYSAVE, H_KEYRESET, H_KEYCANCEL,   /* 6.0: the keyboard screen (ui_keys.c) */
       H_RECOMP, H_EXTCFG,                           /* 6.0: Zelda's recompile, Zelda's emulator / God of War III's launcher window */
       H_EXTINST,                                    /* 6.1: "Install the port" (Bloodborne, inFAMOUS) */
       H_SAVEIMPORT,                                 /* Released (11-13): Saves > "Import saves..." (extras.c SavesImport) */
       H_ARENA, H_ARENATOP /* 7.3: "PORTHEON ARENA" on the top bar, next to Games and Online */,                                      /* 7.2: Online > "Open PORTHEON ARENA" (online.c ArenaOpen) */
       /* 4.0.1: the setup guide (ui_screens.c) */
       H_GUIDEALL, H_GUIDEROW = 600, H_GUIDEPAGE = 640, H_GUIDEOPEN = 650,
       H_KEYROW = 700 /* 6.0: the keyboard screen's controls, up to 32 */, H_THEMEPICK = 740 /* 6.0: Settings > Interface > color, up to 8 */,
       H_GUIDEGAME = 760 /* 6.1: the guide's game chips, up to 24 (at 260, 6.0's Zelda and GoW3 chips were install steps 1-2) */ };
typedef char HitIdsGuideFit[H_GUIDEGAME + NGAMES <= 784 ? 1 : -1];   /* 6.1: compile-time checks of the id blocks */
typedef char HitIdsCardsFit[H_CARD + NGAMES <= H_SYSTILE ? 1 : -1];
static int g_ox, g_oy;    /* 4.0 full screen: where the 1280x800 frame starts in the window (ui.c) */
static DWORD g_msgAt;     /* 4.0: when the message on the status line came (the notification shows it for 8 s) */
typedef struct { RECT r; int id; } Hit;
static Hit g_hits[400]; static int g_nhits;
static void AddHit(int l, int t, int r, int b, int id) { if (g_nhits < 400) { SetRect(&g_hits[g_nhits].r, l, t, r, b); g_hits[g_nhits++].id = id; } }
static int HitAt(int x, int y) { POINT p = {x - g_ox, y - g_oy}; for (int i = g_nhits - 1; i >= 0; i--) if (PtInRect(&g_hits[i].r, p)) return g_hits[i].id; return -1; }
static BOOL HitRect(int id, RECT *out) { for (int i = 0; i < g_nhits; i++) if (g_hits[i].id == id) { *out = g_hits[i].r; return TRUE; } return FALSE; }

/* ---------------------------------------------------------------- small helpers */
static BOOL FileExists(const wchar_t *p) { DWORD a = GetFileAttributesW(p); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }
static BOOL DirExists(const wchar_t *p) { DWORD a = GetFileAttributesW(p); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }
static void Join(wchar_t *out, const wchar_t *a, const wchar_t *b)   /* a\b, one separator even when a is a drive root (C:\) */
{
    size_t n = wcslen(a); BOOL sep = n && (a[n - 1] == L'\\' || a[n - 1] == L'/');
    _snwprintf(out, MAX_PATH, sep ? L"%s%s" : L"%s\\%s", a, b); out[MAX_PATH - 1] = 0;
}
static void StripSlash(wchar_t *p) { size_t n = wcslen(p); while (n > 3 && (p[n - 1] == L'\\' || p[n - 1] == L'/')) p[--n] = 0; }
#include "lang.c"
/* 4.0: the parts included further down that earlier code calls */
static BOOL PcReady(int g); static BOOL GameFilesReady(int g); static BOOL GameFilesAfterCrash(int g, wchar_t *out, int n);
static BOOL DllDiagnosis(int g, wchar_t *out, int n); static void CpuNoMessage(int g, const wchar_t *miss); static void LegacyEnv(int g);
static BOOL VulkanTooOldAfterExit(int g, wchar_t *out, int n);
static BOOL Code2Diagnosis(int g, wchar_t *out, int n);
static BOOL LegacyPlanIn(int g); static void ProfileEnv(int g); static void CheatsPrepare(int g); static void PadPause(BOOL pause); static void MakeFonts(void);
static void VitStop(void); static void SessionRecord(int g, ULONGLONG ms, DWORD code);   /* 6.2 (home62.c) */
static void YtDestroy(void); static void YtWndMsg(WPARAM wp, LPARAM lp);                  /* 6.2 (yt.c) */
static void Msg(COLORREF c, const wchar_t *fmt, ...)
{
    va_list ap; va_start(ap, fmt); _vsnwprintf(g_msg, 1024, fmt, ap); va_end(ap); g_msg[1023] = 0; g_msgColor = c; g_msgAt = GetTickCount();
    g_stTick = 0;   /* something happened: the status shown is read again */
    if (g_wnd) InvalidateRect(g_wnd, NULL, FALSE);
}
static void LocalData(wchar_t *out, const wchar_t *sub)   /* %LOCALAPPDATA%\<sub> */
{
    wchar_t la[MAX_PATH] = L""; GetEnvironmentVariableW(L"LOCALAPPDATA", la, MAX_PATH); Join(out, la, sub);
}
static int PIni(const wchar_t *k, int def) { return GetPrivateProfileIntW(L"PORTHEON", k, def, g_pIni); }
static int g_uiVitC = -1, g_uiLiveC = -1;   /* 6.2: ui_vitrine and ui_live kept in memory (home62.c VitMode, LiveOn) */
static BOOL g_ytNoSave;                      /* 6.2: M / R3 last this session only (a stray R3 no longer mutes the videos for good) */
static BOOL g_ytSound;                       /* 6.2: a YouTube edit plays with its sound: PORTHEON's music pauses (yt.c) */
static void PIniSet(const wchar_t *k, int v)
{
    wchar_t b[16]; _snwprintf(b, 16, L"%d", v); WritePrivateProfileStringW(L"PORTHEON", k, b, g_pIni);
    if (!wcscmp(k, L"ui_vitrine")) g_uiVitC = -1; else if (!wcscmp(k, L"ui_live")) g_uiLiveC = -1;
}

static void PortDir(int g, wchar_t *out)
{
    wchar_t k[32]; _snwprintf(k, 32, L"port_%s", G[g].id);
    GetPrivateProfileStringW(L"PORTHEON", k, L"", out, MAX_PATH, g_pIni);
    if (out[0] && DirExists(out)) return;
    wchar_t t[MAX_PATH]; Join(t, g_exeDir, L"games"); Join(out, t, G[g].id);
}
static void PortExe(int g, wchar_t *out) { wchar_t d[MAX_PATH]; PortDir(g, d); Join(out, d, G[g].exe); }

/* ---------------------------------------------------------------- settings files */
/* UFC 1-4: %LOCALAPPDATA%\UFCnNative\sacredfire.ini - UTF-8 with BOM, CRLF, key=value, unknown keys kept */
#define MAXL 128
typedef struct { wchar_t k[48], v[MAX_PATH]; } KV;
static KV g_kv[MAXL]; static int g_nkv, g_kvGame = -1;

static void SfPath(int g, wchar_t *out) { if (!G[g].data) { out[0] = 0; return; } wchar_t d[MAX_PATH]; LocalData(d, G[g].data); Join(out, d, L"sacredfire.ini"); }
static void SfLoad(int g)
{
    g_nkv = 0; g_kvGame = g;
    wchar_t p[MAX_PATH]; SfPath(g, p);
    HANDLE f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD sz = GetFileSize(f, NULL); if (sz > 256 * 1024) sz = 256 * 1024;
    char *b = (char *)HeapAlloc(GetProcessHeap(), 0, sz + 1); DWORD n = 0; ReadFile(f, b, sz, &n, NULL); CloseHandle(f); b[n] = 0;
    char *s = b; if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s += 3;
    while (*s && g_nkv < MAXL) {
        char *e = s; while (*e && *e != '\n') e++;
        char save = *e; *e = 0; char *cr = strchr(s, '\r'); if (cr) *cr = 0;
        char *eq = strchr(s, '=');
        if (eq && eq != s) { *eq = 0; MultiByteToWideChar(CP_UTF8, 0, s, -1, g_kv[g_nkv].k, 48); MultiByteToWideChar(CP_UTF8, 0, eq + 1, -1, g_kv[g_nkv].v, MAX_PATH); g_nkv++; }
        *e = save; s = *e ? e + 1 : e;
    }
    HeapFree(GetProcessHeap(), 0, b);
}
static void SfSave(void)
{
    if (g_kvGame < 0) return;
    wchar_t p[MAX_PATH], d[MAX_PATH]; SfPath(g_kvGame, p); wcscpy(d, p); *wcsrchr(d, L'\\') = 0; CreateDirectoryW(d, NULL);
    HANDLE f = CreateFileW(p, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD w; WriteFile(f, "\xEF\xBB\xBF", 3, &w, NULL);
    for (int i = 0; i < g_nkv; i++) {
        wchar_t line[MAX_PATH + 64]; _snwprintf(line, MAX_PATH + 64, L"%s=%s\r\n", g_kv[i].k, g_kv[i].v);
        char u[(MAX_PATH + 64) * 3]; int n = WideCharToMultiByte(CP_UTF8, 0, line, -1, u, sizeof(u), NULL, NULL);
        if (n > 1) WriteFile(f, u, n - 1, &w, NULL);
    }
    CloseHandle(f);
}
static KV *SfFind(const wchar_t *k) { for (int i = 0; i < g_nkv; i++) if (!wcscmp(g_kv[i].k, k)) return &g_kv[i]; return NULL; }

/* UFC 6 (4.2): games\UFC6\UFC6.ini [UFC6], which its launcher UFC6.exe reads and writes (a new file starts as UTF-16, so
   a folder name in any language survives the Windows ini functions). 4.3: UFC 5 again, games\UFC5\UFC5.ini [UFC5], left
   as UFC5.exe makes it (4.0.6). Returns the section. */
static const wchar_t *PsIni(int g, wchar_t *out)
{
    wchar_t d[MAX_PATH]; PortDir(g, d);
    if (UFC5G(g)) { Join(out, d, L"UFC5.ini"); return L"UFC5"; }
    Join(out, d, L"UFC6.ini");
    if (!FileExists(out) && DirExists(d)) { HANDLE f = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL); if (f != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(f, "\xFF\xFE", 2, &w, NULL); CloseHandle(f); } }
    return L"UFC6";
}
/* 5.0: UFC Undisputed 3 keeps its settings in its port (%LOCALAPPDATA%\UFC3MONS-P11-Camisas, its configurator's); what
   PORTHEON keeps for it (the game folder, last_code) goes to %LOCALAPPDATA%\PORTHEON\ufc3u.ini [UFC3U], never to a
   sacredfire.ini. A new file starts as UTF-16, so a folder name in any language survives the Windows ini functions. */
static const wchar_t *X3Ini(wchar_t *out)
{
    wchar_t d[MAX_PATH]; LocalData(d, L"PORTHEON"); CreateDirectoryW(d, NULL); Join(out, d, L"ufc3u.ini");
    if (!FileExists(out)) { HANDLE f = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL); if (f != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(f, "\xFF\xFE", 2, &w, NULL); CloseHandle(f); } }
    return L"UFC3U";
}
/* 6.0: Zelda and God of War III keep PORTHEON's choices for them in %LOCALAPPDATA%\PORTHEON\zelda.ini [ZELDA] /
   gow3.ini [GOW3] (their emulator and port keep their own settings); UTF-16 from the start, as X3Ini */
static const wchar_t *ExtIni(int g, wchar_t *out)
{
    wchar_t d[MAX_PATH], f[32]; LocalData(d, L"PORTHEON"); CreateDirectoryW(d, NULL); _snwprintf(f, 32, L"%s.ini", ZELDAG(g) ? L"zelda" : GOW3G(g) ? L"gow3" : BBG(g) ? L"bloodborne" : FNCG(g) ? L"fncreborn" : L"infamous"); Join(out, d, f);
    if (!FileExists(out)) { HANDLE h = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL); if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, "\xFF\xFE", 2, &w, NULL); CloseHandle(h); } }
    return G[g].id;
}
/* one access layer for every game */
static void GetStr(int g, const wchar_t *k, wchar_t *out, int n, const wchar_t *def)
{
    if (G[g].ps5) { wchar_t ini[MAX_PATH]; const wchar_t *sec = PsIni(g, ini); GetPrivateProfileStringW(sec, k, def, out, n, ini); return; }
    if (X360G(g)) { wchar_t ini[MAX_PATH]; const wchar_t *sec = X3Ini(ini); GetPrivateProfileStringW(sec, k, def, out, n, ini); return; }
    if (EXTG(g)) { wchar_t ini[MAX_PATH]; const wchar_t *sec = ExtIni(g, ini); GetPrivateProfileStringW(sec, k, def, out, n, ini); return; }
    if (g_kvGame != g) SfLoad(g);
    KV *kv = SfFind(k); wcsncpy(out, kv ? kv->v : def, n); out[n - 1] = 0;
}
/* community patch switches: one "kind" covers every version's entry in patches\UFCn-community.xml */
static BOOL PatchKindMatch(const char *kind, const char *name, size_t n)
{
    char nm[160]; if (n >= sizeof(nm)) n = sizeof(nm) - 1; memcpy(nm, name, n); nm[n] = 0;
    if (!strcmp(kind, "60fps")) return !strcmp(nm, "60+ FPS");
    if (!strcmp(kind, "crowd")) return !strncmp(nm, "Disable Crowd", 13);
    if (!strcmp(kind, "blur")) return strstr(nm, "Motion Blur") != NULL;
    if (!strcmp(kind, "skin0")) return !strncmp(nm, "Skinning Method 0", 17);
    if (!strcmp(kind, "skin2")) return !strncmp(nm, "Skinning Method 2", 17);
    if (!strcmp(kind, "mesh")) return !strcmp(nm, "Mesh Wrap");
    if (!strcmp(kind, "res720")) return !strcmp(nm, "Resolution 1280x720");   /* 6.0 final: the 3D scene's size (F4P_SCENE) */
    if (!strcmp(kind, "res540")) return !strcmp(nm, "Resolution 960x540");
    if (!strcmp(kind, "res450")) return !strncmp(nm, "Resolution 800x450", 18);
    if (!strcmp(kind, "res360")) return !strncmp(nm, "Resolution 640x360", 18);
    return FALSE;
}
static void PatchXmlPath(int g, wchar_t *out)
{
    wchar_t d[MAX_PATH], f[64]; PortDir(g, d); _snwprintf(f, 64, L"patches\\UFC%d-community.xml", g + 1);
    _snwprintf(out, MAX_PATH, L"%s\\%s", d, f); out[MAX_PATH - 1] = 0;
}
/* get (set < 0): 1 when every matching entry is on, -1 when the game has no such patch. set 0/1: rewrites the XML. */
static int PatchState(int g, const wchar_t *kindW, int set)
{
    char kind[16]; WideCharToMultiByte(CP_UTF8, 0, kindW, -1, kind, 16, NULL, NULL);
    wchar_t path[MAX_PATH]; PatchXmlPath(g, path);
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    DWORD sz = GetFileSize(h, NULL), got = 0; char *buf = (char *)malloc(sz + 1);
    if (!buf || sz > (8u << 20)) { CloseHandle(h); free(buf); return -1; }
    ReadFile(h, buf, sz, &got, NULL); CloseHandle(h); buf[got] = 0;
    int found = 0, on = 0; BOOL changed = FALSE;
    for (char *m = buf; (m = strstr(m, "<Metadata ")) != NULL; m++) {
        char *end = strchr(m, '>'); if (!end) break;
        char *nm = strstr(m, " Name=\""); if (!nm || nm > end) continue; nm += 7;
        char *nq = strchr(nm, '"'); if (!nq || nq > end) continue;
        if (!PatchKindMatch(kind, nm, (size_t)(nq - nm))) continue;
        char *en = strstr(m, "isEnabled=\""); if (!en || en > end) continue;
        found++; BOOL isOn = !strncmp(en + 11, "true\"", 5); on += isOn;
        if (set >= 0 && isOn != (set != 0)) {   /* "true" and "false" differ in length: rebuild the buffer */
            size_t pre = (size_t)(en + 11 - buf), oldLen = isOn ? 4 : 5; const char *nv = set ? "true" : "false";
            size_t rest = strlen(en + 11 + oldLen), nl = strlen(nv);
            char *nb = (char *)malloc(pre + nl + rest + 1); if (!nb) break;
            memcpy(nb, buf, pre); memcpy(nb + pre, nv, nl); memcpy(nb + pre + nl, en + 11 + oldLen, rest + 1);
            m = nb + (m - buf); free(buf); buf = nb; changed = TRUE;
        }
    }
    if (changed) {
        h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) { DWORD wr; WriteFile(h, buf, (DWORD)strlen(buf), &wr, NULL); CloseHandle(h); }
    }
    free(buf);
    if (!found) return -1;
    return set >= 0 ? set : on == found;
}
static int OnlineMode(int g);
static void SetOnlineMode(int g, int mode);
static BOOL Ufc6FsrHave(void)   /* 7.1: AMD's runtime in games\UFC6\novo\fsr (F6_UPS) */
{
    wchar_t p[MAX_PATH]; _snwprintf(p, MAX_PATH, L"%s\\games\\UFC6\\novo\\fsr\\amd_fidelityfx_loader_vk.dll", g_exeDir); p[MAX_PATH - 1] = 0;
    if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) return TRUE;
    _snwprintf(p, MAX_PATH, L"%s\\games\\UFC6\\novo\\fsr\\amd_fidelityfx_vk.dll", g_exeDir); p[MAX_PATH - 1] = 0;
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}
static int GetInt(int g, const wchar_t *k, int def);
static BOOL Ups6Fsr(int g) { return GetInt(g, L"dlss", 0) != 7 && GetInt(g, L"upscaler", 0) == 2; }   /* 7.1: the preset list is FSR's (ultra performance too) */
static int GetInt(int g, const wchar_t *k, int def)
{
    if (!wcsncmp(k, L"patch:", 6)) { int v = PatchState(g, k + 6, -1); return v < 0 ? def : v; }
    if (!wcsncmp(k, L"pini:", 5)) return PIni(k + 5, def);
    if (!wcsncmp(k, L"pgame:", 6)) { wchar_t pk[64]; _snwprintf(pk, 64, L"%s_%s", k + 6, G[g].id); return PIni(pk, def); }
    if (!wcscmp(k, L"online")) return OnlineMode(g);
    if (!wcscmp(k, L"resui")) return GetInt(g, L"pgame:ultrawide", 0) ? 4 : GetInt(g, L"resolution", def);   /* 6.2: UFC 1-4's Resolution list, with 3440x1440 */
    if (!wcscmp(k, L"parsec")) { wchar_t pk[32]; _snwprintf(pk, 32, L"parsec_%s", G[g].id); return PIni(pk, def); }
    if (!wcscmp(k, L"pro6")) return GetInt(g, L"dlss", 0) != 1;                                    /* 11-13: UFC 6's dlss key, three views */
    if (!wcscmp(k, L"dlss6")) { int d = GetInt(g, L"dlss", 0); return d == 0 || (d >= 2 && d <= 6); }
    /* 7.1 (fsr Check: "ultra performance would never stick"): 2 stays 2 when the upscaler is AMD FSR; DLSS plays 2 as 3 */
    if (!wcscmp(k, L"dlssq6")) {
        int d = GetInt(g, L"dlss", 0), lo = Ups6Fsr(g) ? 2 : 3;
        if (d == 2 && lo == 3) d = 3;
        if (d == 6) d = 5;
        if (d < lo || d > 5) { d = GetInt(g, L"dlss_preset", 3); if (d == 2 && lo == 3) d = 3; }
        return d >= lo && d <= 5 ? d : 3;
    }
    /* 7.1: the upscaler list: 9 = None (dlss=7, the Pro mode alone); 3 (the game's own FSR 2) is not in 7.1.
       7.1 review: Automatic is upscaler 0, whatever preset the dlss key holds, as UFC6.exe 1.29.0's ChooseEngine reads it
       (cross/contract#4: a 7.0 install's dlss=3..5 with no upscaler key plays as Automatic with that preset). 7.1 itself
       writes dlss=0 for Automatic (PutInt below), so only 7.0 installs show that state */
    if (!wcscmp(k, L"ups6")) { if (GetInt(g, L"dlss", 0) == 7) return 9; int u = GetInt(g, L"upscaler", 0); return u == 1 || u == 2 ? u : 0; }
    wchar_t b[32], d[16]; _snwprintf(d, 16, L"%d", def); GetStr(g, k, b, 32, d); return b[0] ? _wtoi(b) : def; }
static void PutStr(int g, const wchar_t *k, const wchar_t *v)
{
    if (G[g].ps5) { wchar_t ini[MAX_PATH]; const wchar_t *sec = PsIni(g, ini); WritePrivateProfileStringW(sec, k, v, ini); return; }
    if (X360G(g)) { wchar_t ini[MAX_PATH]; const wchar_t *sec = X3Ini(ini); WritePrivateProfileStringW(sec, k, v, ini); return; }
    if (EXTG(g)) { wchar_t ini[MAX_PATH]; const wchar_t *sec = ExtIni(g, ini); WritePrivateProfileStringW(sec, k, v, ini); return; }   /* 6.0 */
    if (g_kvGame != g) SfLoad(g);
    KV *kv = SfFind(k);
    if (!kv && g_nkv < MAXL) { kv = &g_kv[g_nkv++]; wcsncpy(kv->k, k, 47); kv->k[47] = 0; }
    if (kv) { wcsncpy(kv->v, v, MAX_PATH - 1); kv->v[MAX_PATH - 1] = 0; }
    SfSave();
}
static void PutInt(int g, const wchar_t *k, int v)
{
    if (!wcsncmp(k, L"patch:", 6)) { PatchState(g, k + 6, v); return; }
    if (!wcsncmp(k, L"pini:", 5)) { PIniSet(k + 5, v); return; }
    if (!wcsncmp(k, L"pgame:", 6)) { wchar_t pk[64]; _snwprintf(pk, 64, L"%s_%s", k + 6, G[g].id); PIniSet(pk, v); return; }
    if (!wcscmp(k, L"online")) { SetOnlineMode(g, v); return; }
    if (!wcscmp(k, L"pro6") || !wcscmp(k, L"dlss6") || !wcscmp(k, L"dlssq6")) {   /* 11-13: see F6_PROSW */
        int d = GetInt(g, L"dlss", 0), q = GetInt(g, L"dlssq6", 3), nd = d;
        if (k[0] == L'p') nd = !v ? 1 : d == 1 ? 7 : d;                      /* Pro off = the usual engine; on = Pro alone until DLSS is on */
        else if (k[4] == L'6') nd = v ? q : (d == 1 ? 1 : 7);                /* DLSS on = its last preset (and the Pro mode); off keeps the Pro mode */
        else { nd = v; PutInt(g, L"dlss_preset", v); }
        if (d >= 2 && d <= 5) PutInt(g, L"dlss_preset", d);   /* 7.1: 2 too (FSR's ultra performance) */
        PutInt(g, L"dlss", nd); PutInt(g, L"new_engine", 0); PutInt(g, L"pro_mode", 0); PutInt(g, L"dlss_auto_off", 0); PutInt(g, L"pro_job_n", 0);
        return;
    }
    /* 7.1: the upscaler list (F6_UPS): None = dlss 7 and upscaler 0 (fsr Check: so FSR cannot run from "None"); another
       choice brings back the last preset when the Pro mode was alone (7) or off (1), and writes upscaler. As the Pro
       and preset views above, it clears new_engine / pro_mode / dlss_auto_off. */
    if (!wcscmp(k, L"ups6")) {
        int d = GetInt(g, L"dlss", 0);
        if (d >= 2 && d <= 5) PutInt(g, L"dlss_preset", d);
        if (v == 9) { PutInt(g, L"dlss", 7); PutInt(g, L"upscaler", 0); }
        /* 7.1 review (correctness#2): Automatic writes dlss 0 (UFC6.exe's Automatic: Performance, gives up for this PC
           after a failed start); a preset left in dlss with upscaler 0 was DLSS chosen by hand for UFC6.exe. The preset
           in use was kept in dlss_preset above */
        else if (v != 1 && v != 2) { PutInt(g, L"dlss", 0); PutInt(g, L"upscaler", 0); }
        else {
            int p = GetInt(g, L"dlss_preset", 3); if (p < 2 || p > 5) p = 3;
            if (d == 7 || d == 1 || d == 0) PutInt(g, L"dlss", p);   /* review: from Automatic too, so the preset list shows what plays */
            PutInt(g, L"upscaler", v);
        }
        PutInt(g, L"new_engine", 0); PutInt(g, L"pro_mode", 0); PutInt(g, L"dlss_auto_off", 0); PutInt(g, L"pro_job_n", 0);
        return;
    }
    if (!wcscmp(k, L"resui")) { PutInt(g, L"pgame:ultrawide", v == 4); if (v != 4) PutInt(g, L"resolution", v); else if (GetInt(g, L"resolution", 1) == 0) PutInt(g, L"resolution", 1); return; }   /* 6.2: 3440x1440 = the 1080p picture or more, in a 3440x1440 window */
    wchar_t b[16]; _snwprintf(b, 16, L"%d", v); PutStr(g, k, b);
}
/* The online of UFC 2 (4.3.6) and UFC 3 (6.2) is PORTHEON's choice (online_UFC2, online_UFC3): the ports' own launchers stay
   "offline" and get the server's address from Ufc2OnlineEnv. A UFC 3 with UFC3.exe's old host / join in its sacredfire.ini
   carries that choice over once. */
static int OnlineMode(int g)
{
    if (g == 1) { int m = PIni(L"online_UFC2", 0); return m >= 0 && m <= 2 ? m : 0; }   /* 4.3.6: UFC 2 online (Ufc2OnlineEnv) */
    if (g == 2) {
        int m = PIni(L"online_UFC3", -1);
        if (m < 0) {
            wchar_t old[16]; GetStr(2, L"online_mode", old, 16, L"offline");
            m = !wcscmp(old, L"host") ? 2 : !wcscmp(old, L"join") ? 1 : 0;
            if (m) { PIniSet(L"online_UFC3", m); PutStr(2, L"online_mode", L"offline"); }
        }
        return m >= 0 && m <= 2 ? m : 0;
    }
    wchar_t b[16]; GetStr(g, L"online_mode", b, 16, L"offline");
    return !wcscmp(b, L"host") ? 2 : !wcscmp(b, L"join") ? 1 : 0;
}
static void SetOnlineMode(int g, int mode)
{
    if (g == 1 || g == 2) { PIniSet(g == 1 ? L"online_UFC2" : L"online_UFC3", mode); return; }   /* the port's own launcher keeps "offline" (Ufc2OnlineEnv) */
    PutStr(g, L"online_mode", mode == 2 ? L"host" : mode == 1 ? L"join" : L"offline");
}

/* ---------------------------------------------------------------- game folder */
static BOOL HasEboot(const wchar_t *d) { wchar_t p[MAX_PATH]; Join(p, d, L"eboot.bin"); return FileExists(p); }
static BOOL GameOk(const wchar_t *d)
{
    if (!d[0] || !DirExists(d)) return FALSE;
    if (HasEboot(d)) return TRUE;
    wchar_t pat[MAX_PATH], c[MAX_PATH]; Join(pat, d, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); BOOL ok = FALSE;
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    do { if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.') { Join(c, d, fd.cFileName); if (HasEboot(c)) { ok = TRUE; break; } } } while (FindNextFileW(h, &fd));
    FindClose(h); return ok;
}
/* 4.0.2: UFC 5's launcher needs the dump folder itself (eboot.bin and a UFC 5 sce_sys\param.json). A folder picked one or
   two levels above it (the extractor's output, "pasta 55\ufc 5" in a player's video of 2026-10-24, which the install
   guide accepted and UFC5.exe then refused) is resolved to it: the one with Data, then the one named after PPSA03541. */
/* 4.2: the same for UFC 6 (its dump "ufc 6\UFC6\PPSA23566-app0" holds a second eboot.bin in "dec": Data wins) */
static int DetectFolder(const wchar_t *d, wchar_t *outDir, wchar_t *why, int wn);   /* detect.c */
static BOOL IsUfc6Json(const wchar_t *path);                                       /* detect.c */
static BOOL IsUfc5Json(const wchar_t *path);                                       /* detect.c (4.3: back) */
/* 4.3: g's own PS5 dump (UFC 6: PPSA23566, UFC 5: its PPSA title IDs) */
static BOOL PsDump(int g, const wchar_t *d)
{
    wchar_t p[MAX_PATH]; if (!d[0] || !HasEboot(d)) return FALSE;
    Join(p, d, L"Data"); if (UFC6G(g) && !DirExists(p)) return FALSE;   /* 4.3.6: eboot + sce_sys without Data is no UFC 6 to play */
    Join(p, d, L"sce_sys\\param.json"); return FileExists(p) && (UFC5G(g) ? IsUfc5Json(p) : IsUfc6Json(p));
}
static BOOL PsFind(int g, const wchar_t *d, wchar_t *out, int depth)
{
    if (!d[0] || !DirExists(d)) return FALSE;
    if (PsDump(g, d)) { wcscpy(out, d); return TRUE; }
    if (depth <= 0) return FALSE;
    wchar_t pat[MAX_PATH], c[MAX_PATH], r[MAX_PATH]; int best = -1; Join(pat, d, L"*");
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.' || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        Join(c, d, fd.cFileName);
        if (!PsFind(g, c, r, depth - 1)) continue;
        wchar_t data[MAX_PATH]; Join(data, r, L"Data");
        int score = (DirExists(data) ? 2 : 0) + (StrStrIW(r, G[g].tid) ? 1 : 0);
        if (score > best) { best = score; wcscpy(out, r); }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return best >= 0;
}
/* 4.0.2: the folder of another UFC never counts for g (a UFC 4 folder saved as UFC 5's started UFC 5 on UFC 4's files) */
/* 5.0: UFC Undisputed 3's folder for UFC3.exe --game: the extracted game (Original or Reborn, default.xex at its root) or
   a folder that holds the Xbox 360 .iso (the port finds the .iso in it) */
static BOOL X3Iso(const wchar_t *d, wchar_t *name, int n)   /* the first *.iso in d */
{
    wchar_t pat[MAX_PATH]; WIN32_FIND_DATAW fd; Join(pat, d, L"*.iso"); HANDLE h = FindFirstFileW(pat, &fd); BOOL ok = FALSE;
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    do if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) { if (name) { wcsncpy(name, fd.cFileName, n - 1); name[n - 1] = 0; } ok = TRUE; } while (!ok && FindNextFileW(h, &fd));
    FindClose(h); return ok;
}
static BOOL X3Folder(const wchar_t *d)
{
    wchar_t p[MAX_PATH]; if (!d[0] || !DirExists(d)) return FALSE;
    Join(p, d, L"default.xex"); return FileExists(p) || X3Iso(d, NULL, 0);
}
static void X3Normalize(wchar_t *p)   /* an .iso file picked or dropped stands for its folder */
{
    if (p[0] && FileExists(p) && !_wcsicmp(PathFindExtensionW(p), L".iso")) { wchar_t *s = wcsrchr(p, L'\\'); if (s) { *s = 0; if (wcslen(p) == 2 && p[1] == L':') wcscat(p, L"\\"); } }
}
/* 6.0: Zelda and God of War III (extgames.c) */
static BOOL ExtGameOk(int g, const wchar_t *d);
static int ExtStatus(int g);
static BOOL ExtCommand(int g, BOOL play, wchar_t *exe, wchar_t *dir, wchar_t *cmd, int ncmd);
static BOOL ExtLogsDir(int g, wchar_t *out);
static void ZeldaRecompile(void);
static void ExtInstallMenu(int g);   /* 6.1: extgames.c, Bloodborne's / inFAMOUS's "Install the port" */
static BOOL ExtPreflight(int g);     /* 6.1: extgames.c, before their PLAY */
static int ZeldaEngine(void);   /* 6.0 final: StartPort asks whether Zelda runs natively (no console window then) */
static BOOL ZeldaExt(const wchar_t *p);
static DWORD WINAPI WaitTreeThread(LPVOID p);
static BOOL GameOkFor(int g, const wchar_t *d)
{
    if (EXTG(g)) return ExtGameOk(g, d);
    if (X360G(g)) return X3Folder(d);
    if (G[g].ps5) return PsDump(g, d);
    if (!GameOk(d)) return FALSE;
    int det = DetectFolder(d, NULL, NULL, 0); return det < 0 || det == g;
}
/* the game folder PORTHEON uses for g */
static void GameDir(int g, wchar_t *p) { GetStr(g, L"game", p, MAX_PATH, L""); }
static BOOL PortOk(int g) { wchar_t e[MAX_PATH]; PortExe(g, e); return FileExists(e); }
static BOOL CpuLacksPs4(wchar_t *out, int n);
/* 5.2: Windows saves the AVX registers (OSXSAVE set and XCR0 bits 1-2). In a virtual machine, or with XSAVE turned off
   in Windows, CPUID still says AVX/AVX2 and the first ymm instruction faults (0xC000001D). xgetbv only runs when
   OSXSAVE is set: without it the instruction itself is illegal. */
static BOOL CpuAvxSaved(void)
{
    unsigned a, b, c, d; if (!__get_cpuid(1, &a, &b, &c, &d) || !((c >> 27) & 1)) return FALSE;
    unsigned lo, hi; __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0)); (void)hi;
    return (lo & 6) == 6;
}
/* 5.2 (UFC5.exe 1.32.0 / UFC6.exe 1.18.0 check the same): the PS5 games' own code runs natively and was built for the
   PS5's Zen 2, so AVX2 alone said too little (a VM's CPU mask, or AVX2 with XSAVE off, passed and the game faulted).
   Every family the eboots use is checked: SSE4.2, POPCNT, MOVBE, FMA, F16C, AVX, AVX2, BMI1, BMI2, LZCNT, and the OS
   saving the AVX registers. miss gets the names (XSAVE: AVX present but not saved by Windows / the VM). */
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
/* 5.2: the instructions UFC 3 and 4 run that only AMD processors have (SSE4a: INSERTQ/EXTRQ, CPUID 0x80000001 ECX bit 6).
   The ports' own engines patch them at run time (BB_SSE4A_PATCH); the new engine (games\engines\intel-arc) and the v3.0
   engine do not. debug_no_sse4a=1 in portheon.ini acts as an Intel processor, to test the rules on a Ryzen. */
static BOOL CpuHasSse4a(void)
{
    if (PIni(L"debug_no_sse4a", 0)) return FALSE;
    unsigned a, b, c, d; return __get_cpuid(0x80000001, &a, &b, &c, &d) && ((c >> 6) & 1);
}
static BOOL CpuX360Ok(wchar_t *miss, int n);   /* checks.c (5.2): what UFC Undisputed 3's recompiled code needs */
static BOOL X3FilesReady(int g);              /* checks.c (5.2): its port's files and DLL imports */
static BOOL CpuLegacyOk(int g);   /* checks.c: the CPU lacks only what PORTHEON can translate for this game */
static BOOL UnderWine(void);   /* update.c */
static int Status(int g)   /* S_READY, S_PICK, S_NOPORT, S_RUNNING, S_NOCPU */
{
    if (g_proc && g_procGame == g) return S_RUNNING;
    if (SOON(g)) return S_NOPORT;   /* 6.2 */
    if (EXTG(g)) return ExtStatus(g);   /* 6.0: Zelda's emulator builds or its native build, God of War III's port */
    if (!PortOk(g)) return S_NOPORT;
    if (X360G(g)) {   /* 5.0: no PS4/PS5 processor checks; 5.2: its own (SSSE3, SSE4.1, SSE4.2, POPCNT) */
        if (!CpuX360Ok(NULL, 0)) return S_NOCPU;
        wchar_t p[MAX_PATH]; GameDir(g, p); return X3Folder(p) ? S_READY : S_PICK;
    }
    if (G[g].ps5) { if (!CpuPs5Ok(NULL, 0)) return S_NOCPU; }   /* 4.0.2: UFC 5's PS5 code needs AVX2; no compatibility mode. 5.2: and the rest of Zen 2's */
    else if (CpuLacksPs4(NULL, 0) && !CpuLegacyOk(g)) return S_NOCPU;
    wchar_t p[MAX_PATH]; GameDir(g, p);
    if (G[g].ps5 && p[0] && !PsDump(g, p)) {   /* 4.0.2: a folder above UFC 5's dump becomes the dump's own (each path tried once) */
        static wchar_t tried[NGAMES][MAX_PATH]; wchar_t r[MAX_PATH];
        if (wcscmp(tried[g], p)) { wcscpy(tried[g], p); if (PsFind(g, p, r, 2)) { PutStr(g, L"game", r); wcscpy(p, r); if (g == g_sel && g_editGame) SetWindowTextW(g_editGame, r); } }
    }
    return GameOkFor(g, p) ? S_READY : S_PICK;
}

static void ReadVersion(int g)
{
    wchar_t d[MAX_PATH], p[MAX_PATH]; PortDir(g, d); Join(p, d, L"VERSION"); g_ver[g][0] = 0;
    HANDLE f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { if (FNCG(g) && PortOk(g)) wcscpy(g_ver[g], L"FNC Reborn"); return; }   /* 6.2: no VERSION file of its own */
    char b[64]; DWORD n = 0; ReadFile(f, b, 63, &n, NULL); CloseHandle(f); b[n] = 0;
    char *e = b; while (*e && *e != '\r' && *e != '\n') e++; *e = 0;
    if (e - b > 46) b[46] = 0;   /* 6.0: a long VERSION line ran into the next game's (no terminator when it does not fit) */
    MultiByteToWideChar(CP_UTF8, 0, b, -1, g_ver[g], 48); g_ver[g][47] = 0;
}

/* play time per game (3.5): playtime_<id> seconds and lastplay_<id> (Unix time) in portheon.ini */
static void PlayTimeAdd(int g, ULONGLONG ms)
{
    wchar_t k[48]; _snwprintf(k, 48, L"playtime_%s", G[g].id);
    PIniSet(k, PIni(k, 0) + (int)(ms / 1000));
    _snwprintf(k, 48, L"lastplay_%s", G[g].id);
    FILETIME ft; GetSystemTimeAsFileTime(&ft); ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
    PIniSet(k, (int)(u.QuadPart / 10000000ull - 11644473600ull));
}

/* ---------------------------------------------------------------- images */
static GpImage *LoadPngRes(int id)
{
    HRSRC r = FindResourceW(g_inst, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA); if (!r) return NULL;
    DWORD sz = SizeofResource(g_inst, r); void *p = LockResource(LoadResource(g_inst, r)); if (!p) return NULL;
    IStream *s = SHCreateMemStream((const BYTE *)p, sz); if (!s) return NULL;
    GpImage *img = NULL; GdipCreateBitmapFromStream(s, &img); s->lpVtbl->Release(s); return img;
}
/* the games' own art (3.3): PS4/PS5 sce_sys\pic1.png (or pic0.png) and icon0.png, from the player's own dump.
   Nothing of the games is shipped with PORTHEON. */
static GpImage *g_icon[NGAMES], *g_art[NGAMES]; static BOOL g_artTried[NGAMES];
static GpImage *LoadImageFile(const wchar_t *path)
{
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(f, NULL), got = 0; GpImage *img = NULL;
    if (sz && sz < (24u << 20)) {
        BYTE *b = (BYTE *)malloc(sz);
        if (b && ReadFile(f, b, sz, &got, NULL) && got == sz) {
            IStream *st = SHCreateMemStream(b, sz);
            if (st) { GdipCreateBitmapFromStream(st, &img); st->lpVtbl->Release(st); }
        }
        free(b);
    }
    CloseHandle(f); return img;
}
static GpImage *FindArt(const wchar_t *d, const wchar_t *const *names, int n, wchar_t *found)   /* found: the file used */
{
    found[0] = 0;
    if (!d[0] || !DirExists(d)) return NULL;
    wchar_t p[MAX_PATH];
    for (int i = 0; i < n; i++) { Join(p, d, names[i]); if (FileExists(p)) { GpImage *img = LoadImageFile(p); if (img) wcscpy(found, p); return img; } }
    wchar_t pat[MAX_PATH]; Join(pat, d, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); GpImage *img = NULL;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
        wchar_t sub[MAX_PATH]; Join(sub, d, fd.cFileName);
        for (int i = 0; i < n && !img; i++) { Join(p, sub, names[i]); if (FileExists(p) && (img = LoadImageFile(p)) != NULL) wcscpy(found, p); }
    } while (!img && FindNextFileW(h, &fd));
    FindClose(h); return img;
}
/* 4.0.4: the covers stay when the dump is not there (the user asked for it): each picture read from the player's dump
   is copied to %LOCALAPPDATA%\PORTHEON\art (<id>_icon.png, <id>_pic.png), and a game whose folder is missing shows that
   copy. Before such a copy existed, the desktop shortcut's icon (icons\<id>.ico: the same icon0.png in an .ico, see
   GameIco) stands in for the tile. Still only the player's own files: PORTHEON ships no game art. */
static void ArtCachePath(int g, const wchar_t *kind, wchar_t *out)
{
    wchar_t d[MAX_PATH], f[64]; LocalData(d, L"PORTHEON"); Join(d, d, L"art"); CreateDirectoryW(d, NULL);
    _snwprintf(f, 64, L"%s_%s.png", G[g].id, kind); Join(out, d, f);
}
static void ArtKeep(int g, const wchar_t *kind, const wchar_t *src)
{
    if (!src[0]) return;
    wchar_t c[MAX_PATH]; ArtCachePath(g, kind, c);
    WIN32_FILE_ATTRIBUTE_DATA a, b;
    if (GetFileAttributesExW(src, GetFileExInfoStandard, &a) && GetFileAttributesExW(c, GetFileExInfoStandard, &b)
        && a.nFileSizeLow == b.nFileSizeLow && a.nFileSizeHigh == b.nFileSizeHigh) return;   /* the same picture is there */
    CopyFileW(src, c, FALSE);
}
static GpImage *IcoPng(const wchar_t *path)   /* the PNG inside an .ico that GameIco wrote (22-byte header, then the PNG) */
{
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(f, NULL), got = 0; GpImage *img = NULL;
    if (sz > 30 && sz < (8u << 20)) {
        BYTE *b = (BYTE *)malloc(sz);
        if (b && ReadFile(f, b, sz, &got, NULL) && got == sz && b[2] == 1 && !memcmp(b + 22, "\x89PNG", 4)) {
            IStream *st = SHCreateMemStream(b + 22, sz - 22);
            if (st) { GdipCreateBitmapFromStream(st, &img); st->lpVtbl->Release(st); }
        }
        free(b);
    }
    CloseHandle(f); return img;
}
/* 5.0: the biggest PNG picture inside a multi-image .ico (UFC Undisputed 3's steam\icon.ico) */
static GpImage *IcoBestPng(const wchar_t *path)
{
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(f, NULL), got = 0; GpImage *img = NULL;
    if (sz > 22 && sz < (8u << 20)) {
        BYTE *b = (BYTE *)malloc(sz);
        if (b && ReadFile(f, b, sz, &got, NULL) && got == sz && b[2] == 1) {
            int cnt = b[4] | (b[5] << 8), best = -1, bw = 0;
            for (int i = 0; i < cnt && 6 + i * 16 + 16 <= (int)sz; i++) {
                const BYTE *e = b + 6 + i * 16; int w = e[0] ? e[0] : 256; DWORD len, off; memcpy(&len, e + 8, 4); memcpy(&off, e + 12, 4);
                if (off + 8 <= sz && len <= sz - off && !memcmp(b + off, "\x89PNG", 4) && w > bw) { bw = w; best = i; }
            }
            if (best >= 0) {
                DWORD len, off; memcpy(&len, b + 6 + best * 16 + 8, 4); memcpy(&off, b + 6 + best * 16 + 12, 4);
                IStream *st = SHCreateMemStream(b + off, len);
                if (st) { GdipCreateBitmapFromStream(st, &img); st->lpVtbl->Release(st); }
            }
        }
        free(b);
    }
    CloseHandle(f); return img;
}
static void ArtChanged(int g);   /* ui.c: drops the cached bitmaps made from g's art */
/* 6.1: Zelda's own icon, from the player's EXTRACTED Switch dump only: the control data (control.nacp with its
   icon_<Language>.dat, a 256x256 JPEG) in the picked folder (or the picked file's folder) or up to two folders down.
   A packed .nup/.nsp/.xci is never opened (its NCAs are encrypted): PORTHEON's own art stays then. */
static BOOL NacpIconIn(const wchar_t *dir, wchar_t *out)
{
    wchar_t p[MAX_PATH]; Join(p, dir, L"control.nacp"); if (!FileExists(p)) return FALSE;
    static const wchar_t *const pt[] = {L"icon_BrazilianPortuguese.dat", L"icon_Portuguese.dat"}, *const en[] = {L"icon_AmericanEnglish.dat", L"icon_BritishEnglish.dat"};
    for (int k = 0; k < 2; k++) {
        if (k == 0 && g_lang != LANG_PT) continue;   /* the Portuguese icon first for a Portuguese PORTHEON */
        const wchar_t *const *l = k ? en : pt;
        for (int i = 0; i < 2; i++) { Join(p, dir, l[i]); if (FileExists(p)) { wcscpy(out, p); return TRUE; } }
    }
    wchar_t pat[MAX_PATH]; Join(pat, dir, L"icon_*.dat"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    FindClose(h); Join(out, dir, fd.cFileName); return TRUE;
}
/* 6.1 review: *budget = the folders still allowed to be looked into in the whole search (LoadArt gives 96), so a picked
   Downloads-like folder costs at most that many directory reads on the UI thread, not 64 x 64 */
static BOOL NacpIconFind(const wchar_t *dir, int depth, wchar_t *out, int *budget)
{
    if (*budget <= 0) return FALSE;
    --*budget;
    if (NacpIconIn(dir, out)) return TRUE;
    if (depth <= 0) return FALSE;
    wchar_t pat[MAX_PATH]; Join(pat, dir, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); BOOL got = FALSE; int seen = 0;
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;   /* 6.1 review: no junctions (a loop, another drive) */
        if (!_wcsicmp(fd.cFileName, L"romfs") || !_wcsicmp(fd.cFileName, L"exefs")) continue;   /* the game's data: thousands of folders, never the icon */
        wchar_t sub[MAX_PATH]; Join(sub, dir, fd.cFileName); got = NacpIconFind(sub, depth - 1, out, budget);
    } while (!got && *budget > 0 && ++seen < 64 && FindNextFileW(h, &fd));
    FindClose(h); return got;
}
static void LoadArt(int g)
{
    if (g_artTried[g]) return;
    g_artTried[g] = TRUE;
    wchar_t d[MAX_PATH], fp[MAX_PATH], fi[MAX_PATH], c[MAX_PATH]; GameDir(g, d);
    static const wchar_t *const pic[] = {L"sce_sys\\pic1.png", L"sce_sys\\pic0.png", L"PS3_GAME\\PIC1.PNG", L"PS3_GAME\\PIC0.PNG"};
    static const wchar_t *const ico[] = {L"sce_sys\\icon0.png", L"PS3_GAME\\ICON0.PNG"};
    g_art[g] = FindArt(d, pic, 4, fp); g_icon[g] = FindArt(d, ico, 2, fi);
    BOOL iconCached = FALSE;
    if (ZELDAG(g) && !g_icon[g] && d[0]) {   /* 6.1: the Switch dump's own icon (NacpIconFind), when it is extracted */
        /* 6.1 review: the icon kept from an earlier start (ArtKeep) first: the folder scan only when there is none */
        ArtCachePath(g, L"icon", c); g_icon[g] = LoadImageFile(c); iconCached = g_icon[g] != NULL;
        wchar_t zd[MAX_PATH]; wcscpy(zd, d); if (!DirExists(zd)) PathRemoveFileSpecW(zd);
        if (wcslen(zd) <= 3) zd[0] = 0;   /* a file: its folder; never a whole drive */
        int budget = 96;
        if (!g_icon[g] && zd[0] && DirExists(zd) && NacpIconFind(zd, 2, fi, &budget)) g_icon[g] = LoadImageFile(fi);
        if (!g_icon[g] || iconCached) fi[0] = 0;
    }
    if (g_art[g]) ArtKeep(g, L"pic", fp); else { ArtCachePath(g, L"pic", c); g_art[g] = LoadImageFile(c); }
    if (g_icon[g]) { if (!iconCached) ArtKeep(g, L"icon", fi); }   /* 6.1: Zelda's icon_*.dat (a JPEG) too: LoadImageFile reads the content, not the name; 6.1 review: not the cache onto itself */
    else {
        ArtCachePath(g, L"icon", c); g_icon[g] = LoadImageFile(c);
        if (!g_icon[g]) { wchar_t f[64]; LocalData(c, L"PORTHEON"); Join(c, c, L"icons"); _snwprintf(f, 64, L"%s.ico", G[g].id); Join(c, c, f); g_icon[g] = IcoPng(c); }
    }
    /* 5.0 (the user: "keep the other squares even when not installed, with the games' covers"): a game that is not on
       this PC yet shows the cover and the picture that come with PORTHEON, games\<id>\art\cover.jpg and background.jpg */
    {   /* 6.1 review: also PORTHEON's own games\<id>\art when the port lives elsewhere (the player's folder, or
           %LOCALAPPDATA%\PORTHEON\ports when PORTHEON's folder is not writable) */
        wchar_t pd[MAX_PATH], gd[MAX_PATH], own[MAX_PATH], p[MAX_PATH]; PortDir(g, pd); Join(gd, g_exeDir, L"games"); Join(own, gd, G[g].id);
        for (int k = 0; k < 2; k++) {
            const wchar_t *b = k ? own : pd; if (k && !_wcsicmp(own, pd)) break;
            if (!g_icon[g]) { Join(p, b, L"art\\cover.jpg"); g_icon[g] = LoadImageFile(p); }
            if (!g_art[g]) { Join(p, b, L"art\\background.jpg"); g_art[g] = LoadImageFile(p); }
        } }
    /* 5.0: UFC Undisputed 3 (an Xbox 360 game: no sce_sys pictures) shows the port's own Steam pictures, games\UFC3U\steam:
       the wide hero picture behind the home screen (the 600x900 grid cover when it is missing) and icon.ico on the tile */
    if (X360G(g)) {
        wchar_t pd[MAX_PATH], p[MAX_PATH]; PortDir(g, pd);
        static const wchar_t *const bg[] = {L"steam\\hero.png", L"steam\\wide.png", L"steam\\grid.png"};
        for (int i = 0; i < 3 && !g_art[g]; i++) { Join(p, pd, bg[i]); g_art[g] = LoadImageFile(p); }
        if (!g_icon[g]) { Join(p, pd, L"steam\\icon.ico"); g_icon[g] = IcoBestPng(p); }
        if (!g_icon[g]) { Join(p, pd, L"steam\\grid.png"); g_icon[g] = LoadImageFile(p); }
    }
    /* 6.2 (the owner sent these covers): over anything found in the player's copy. A wide picture that is big enough is the game's background too. */
    {   static const struct { int g, ico, bg; } OWN[] = {{12, 301, 302}, {8, 303, 304}, {11, 305, 0}, {6, 307, 0}, {7, 308, 309}, {9, 310, 311}, {10, 312, 0}};
        for (int k = 0; k < (int)(sizeof(OWN) / sizeof(OWN[0])); k++) if (OWN[k].g == g) {
            GpImage *i = LoadPngRes(OWN[k].ico), *b = OWN[k].bg ? LoadPngRes(OWN[k].bg) : NULL;
            if (i) { if (g_icon[g]) GdipDisposeImage(g_icon[g]); g_icon[g] = i; }
            if (b) { if (g_art[g]) GdipDisposeImage(g_art[g]); g_art[g] = b; }
        }
    }
}
static void ForgetArt(int g)
{
    if (g_art[g]) GdipDisposeImage(g_art[g]);
    if (g_icon[g]) GdipDisposeImage(g_icon[g]);
    g_art[g] = g_icon[g] = NULL; g_artTried[g] = FALSE;
    ArtChanged(g);
}

/* ---------------------------------------------------------------- this PC
   The video card through Vulkan (name, vendor, memory, whether the CPU can see all of its memory: Resizable BAR, and
   3.5: whether any card has fragmentShaderBarycentric, which UFC 5's engine needs), processor threads, RAM, monitor. */
typedef struct { wchar_t name[256]; uint32_t vendor, type; uint64_t vram, bar; } PcGpu;
static PcGpu g_gpu; static BOOL g_haveGpu, g_oldCpu; static int g_threads, g_ramGb, g_monW, g_monH, g_monHz;
static wchar_t g_cpuName[64];
/* why a PS5 game cannot start on this processor (4.3: UFC 5's own 4.0.6 text, UFC 6's 4.2 text: both translated) */
/* 5.2: the text names what is missing (CpuPs5Ok), and says why a new Pentium or Celeron cannot run it (up to the 10th
   generation they have no AVX at all: their owners read "4th generation or newer" and did not understand); X360G: UFC
   Undisputed 3's own needs (CpuX360Ok). ui_items.c shows it for S_NOCPU. */
static void NoAvx2Text(int g, wchar_t *out, int n)
{
    wchar_t miss[160] = L"";
    BOOL pent = StrStrIW(g_cpuName, L"Pentium") || StrStrIW(g_cpuName, L"Celeron");
    if (X360G(g)) {
        CpuX360Ok(miss, 160);
        _snwprintf(out, n, L2(L"Este processador (%s) não tem %s, que o código do UFC Undisputed 3 (recompilado do Xbox 360) usa direto: ele precisa de um processador com SSE4.2 (Intel Core de 1ª geração, de 2008, ou mais novo; AMD FX, A-series de 2012 ou mais nova, ou Ryzen).",
                              L"This processor (%s) lacks %s, which UFC Undisputed 3's code (recompiled from the Xbox 360) uses directly: it needs a processor with SSE4.2 (Intel Core 1st generation, from 2008, or newer; AMD FX, A-series from 2012 or newer, or Ryzen)."),
                   g_cpuName, miss[0] ? miss : L"SSE4.2");
        out[n - 1] = 0; return;
    }
    CpuPs5Ok(miss, 160);
    BOOL xs = wcsstr(miss, L"XSAVE") != NULL;
    /* 5.2: the Pentium/Celeron sentence only for a chip with no AVX at all (CPUID 1 ECX bit 28): a Pentium Gold of the
       11th generation or newer has AVX2 and can miss only XSAVE (a virtual machine) or another flag */
    { unsigned a1, b1, c1 = 0, d1; if (!wcsstr(miss, L"AVX") || (__get_cpuid(1, &a1, &b1, &c1, &d1) && (c1 & (1u << 28)))) pent = FALSE; }
    _snwprintf(out, n, L2(L"Este processador (%s) não tem %s, que o código do %s (PS5) usa direto: ele precisa de um Intel Core de 4ª geração ou mais novo, ou de um AMD Ryzen.%s%s",
                          L"This processor (%s) lacks %s, which %s's PS5 code uses directly: it needs an Intel Core 4th generation or newer, or an AMD Ryzen.%s%s"),
               g_cpuName, miss[0] ? miss : L"AVX2", G[g].name,
               pent ? L2(L" Pentium e Celeron não têm AVX nem nas gerações novas (até a 10ª: G5400, G6400, J4125, N5000...): precisa de um Intel Core i3 ou melhor.",
                         L" Pentium and Celeron chips have no AVX even in new generations (up to the 10th: G5400, G6400, J4125, N5000...): it needs an Intel Core i3 or better.") : L"",
               xs ? L2(L" (XSAVE: o processador tem AVX, mas o Windows ou a máquina virtual não o deixa usar; numa máquina virtual, repasse as instruções do processador.)",
                       L" (XSAVE: the processor has AVX, but Windows or the virtual machine does not let it be used; in a virtual machine, pass the processor's instructions through.)") : L"");
    out[n - 1] = 0;
}
/* an old processor, the same rule as UFC5.exe: no AVX2 (Intel Core 3rd generation and older, AMD FX/Phenom/older APUs,
   most Pentium/Celeron) or a Ryzen 1000-3000 (Zen, Zen+, Zen 2) */
static BOOL CpuIsOld(void)
{
    unsigned a, b, c, d, r[12]; char s[49];
    for (int i = 0; i < 3; i++) __get_cpuid(0x80000002 + i, &r[i * 4], &r[i * 4 + 1], &r[i * 4 + 2], &r[i * 4 + 3]);
    memcpy(s, r, 48); s[48] = 0;
    { const char *t = s; while (*t == ' ') t++; MultiByteToWideChar(CP_ACP, 0, t, -1, g_cpuName, 64); g_cpuName[63] = 0; }
    if (!__get_cpuid_count(7, 0, &a, &b, &c, &d) || !(b & (1u << 5))) return TRUE;
    const char *p = StrStrIA(s, "Ryzen");
    if (!p || StrStrIA(s, "Threadripper")) return FALSE;
    for (p += 5; *p; ) {   /* the model number: the first word of 4+ digits ("Ryzen 5 3600", "Ryzen 7 PRO 2700U") */
        while (*p == ' ') p++;
        int dig = 0; while (p[dig] >= '0' && p[dig] <= '9') dig++;
        if (dig >= 4) return p[0] >= '1' && p[0] <= '3';
        while (*p && *p != ' ') p++;
    }
    return FALSE;
}
typedef void *(WINAPI *PFN_GIPA_)(void *, const char *);
typedef struct { int sType; const void *pNext; const char *app; uint32_t appVer; const char *eng; uint32_t engVer; uint32_t api; } VkAppInfo__;
typedef struct { int sType; const void *pNext; uint32_t flags; const VkAppInfo__ *app; uint32_t nl; const char *const *l; uint32_t ne; const char *const *e; } VkInstInfo__;

/* 4.3.3: a player's PORTHEON closed at start inside vulkan-1.dll (crash.txt of 2026-10-31: 0xC0000005 at
   vulkan-1.dll+0x72200; an implicit Vulkan layer of an overlay, or a broken driver install). The graphics-card probes
   (DetectGpu below, checks.c's VulkanCheck) now run on a thread of their own, without implicit layers (PORTHEON needs
   none: VK_LOADER_LAYERS_DISABLE=~implicit~, only for the probe), and a fault in there ends that thread alone: PORTHEON
   goes on as on a PC whose card it could not read. A probe that hangs is given up after the timeout. */
static volatile DWORD g_guardTid;
static void GuardBail(void) { ExitThread(1); }
static LONG CALLBACK GuardVeh(EXCEPTION_POINTERS *e)
{
    DWORD c = e->ExceptionRecord->ExceptionCode;
    if (!g_guardTid || GetCurrentThreadId() != g_guardTid) return EXCEPTION_CONTINUE_SEARCH;
    if (c != EXCEPTION_ACCESS_VIOLATION && c != EXCEPTION_ILLEGAL_INSTRUCTION && c != EXCEPTION_PRIV_INSTRUCTION &&
        c != EXCEPTION_INT_DIVIDE_BY_ZERO && c != EXCEPTION_IN_PAGE_ERROR && c != EXCEPTION_DATATYPE_MISALIGNMENT) return EXCEPTION_CONTINUE_SEARCH;
    e->ContextRecord->Rsp = (e->ContextRecord->Rsp & ~(DWORD64)15) - 0x88;   /* a fresh, aligned frame for the bail-out */
    e->ContextRecord->Rip = (DWORD64)(UINT_PTR)GuardBail;
    return EXCEPTION_CONTINUE_EXECUTION;
}
static void (*g_guardMeanwhile)(void);   /* perf 11-13: called once by the next RunGuarded, between starting its thread and waiting for it */
static BOOL RunGuarded(LPTHREAD_START_ROUTINE fn, DWORD ms)   /* TRUE when fn ran to its end */
{
    wchar_t old[256]; DWORD had = GetEnvironmentVariableW(L"VK_LOADER_LAYERS_DISABLE", old, 256);
    SetEnvironmentVariableW(L"VK_LOADER_LAYERS_DISABLE", L"~implicit~");
    static PVOID veh; if (!veh) veh = AddVectoredExceptionHandler(1, GuardVeh);   /* stays: a thread that hangs may fault later */
    DWORD tid = 0; HANDLE t = CreateThread(NULL, 1 << 20, fn, NULL, CREATE_SUSPENDED, &tid); BOOL ok = FALSE;
    if (t) {
        g_guardTid = tid; ResumeThread(t);
        if (g_guardMeanwhile) { void (*mw)(void) = g_guardMeanwhile; g_guardMeanwhile = NULL; mw(); }   /* perf 11-13: work of this thread's own, done while the other runs (wWinMain: the home's background during the Vulkan probe) */
        if (WaitForSingleObject(t, ms) == WAIT_OBJECT_0) { DWORD code = 1; GetExitCodeThread(t, &code); ok = code == 0; g_guardTid = 0; }
        CloseHandle(t);
    }
    SetEnvironmentVariableW(L"VK_LOADER_LAYERS_DISABLE", had && had < 256 ? old : NULL);
    return ok;
}
static DWORD WINAPI DetectGpu(LPVOID unused);
static void DetectPc(void)
{
    SYSTEM_INFO si; GetSystemInfo(&si); g_threads = (int)si.dwNumberOfProcessors; g_oldCpu = CpuIsOld();
    MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms); g_ramGb = (int)((ms.ullTotalPhys + (512ull << 20)) >> 30);
    DEVMODEW dm = {.dmSize = sizeof(dm)}; EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm);
    g_monW = dm.dmPelsWidth ? (int)dm.dmPelsWidth : 1920; g_monH = dm.dmPelsHeight ? (int)dm.dmPelsHeight : 1080; g_monHz = dm.dmDisplayFrequency > 1 ? (int)dm.dmDisplayFrequency : 60;
    g_haveGpu = FALSE;
    if (!RunGuarded(DetectGpu, 15000)) g_haveGpu = FALSE;
}
static DWORD WINAPI DetectGpu(LPVOID unused)
{
    (void)unused;
    HMODULE vk = LoadLibraryW(L"vulkan-1.dll"); if (!vk) return 0;
    PFN_GIPA_ gipa = (PFN_GIPA_)(void *)GetProcAddress(vk, "vkGetInstanceProcAddr");
    int (WINAPI *create)(const VkInstInfo__ *, const void *, void **) = gipa ? (int (WINAPI *)(const VkInstInfo__ *, const void *, void **))gipa(NULL, "vkCreateInstance") : NULL;
    VkAppInfo__ ai = {0, NULL, "PORTHEON", 1, "none", 1, (1u << 22) | (1u << 12)}; VkInstInfo__ ii = {1, NULL, 0, &ai, 0, NULL, 0, NULL}; void *inst = NULL;
    if (!create || create(&ii, NULL, &inst) != 0 || !inst) { FreeLibrary(vk); return 0; }
    int (WINAPI *enumDev)(void *, uint32_t *, void **) = (int (WINAPI *)(void *, uint32_t *, void **))gipa(inst, "vkEnumeratePhysicalDevices");
    void (WINAPI *props)(void *, void *) = (void (WINAPI *)(void *, void *))gipa(inst, "vkGetPhysicalDeviceProperties");
    void (WINAPI *mem)(void *, void *) = (void (WINAPI *)(void *, void *))gipa(inst, "vkGetPhysicalDeviceMemoryProperties");
    void (WINAPI *destroy)(void *, const void *) = (void (WINAPI *)(void *, const void *))gipa(inst, "vkDestroyInstance");

    uint32_t n = 8; void *devs[8]; uint64_t bestScore = 0;
    if (enumDev && props && mem && enumDev(inst, &n, devs) >= 0) {
        static unsigned char buf[8192];
        for (uint32_t i = 0; i < n && i < 8; i++) {
            PcGpu c = {0}; memset(buf, 0, sizeof(buf)); props(devs[i], buf);
            c.vendor = *(uint32_t *)(buf + 8); c.type = *(uint32_t *)(buf + 16); MultiByteToWideChar(CP_UTF8, 0, (char *)buf + 20, -1, c.name, 256);
            memset(buf, 0, sizeof(buf)); mem(devs[i], buf);
            uint32_t nt = *(uint32_t *)buf, nh = *(uint32_t *)(buf + 260);
            for (uint32_t h = 0; h < nh && h < 16; h++) { uint64_t size = *(uint64_t *)(buf + 264 + h * 16); uint32_t fl = *(uint32_t *)(buf + 272 + h * 16); if ((fl & 1) && size > c.vram) c.vram = size; }
            for (uint32_t t = 0; t < nt && t < 32; t++) {   /* device-local + host-visible memory: its heap is the BAR window */
                uint32_t fl = *(uint32_t *)(buf + 4 + t * 8), hi = *(uint32_t *)(buf + 8 + t * 8);
                if ((fl & 3) == 3 && hi < 16) { uint64_t hs = *(uint64_t *)(buf + 264 + hi * 16); if (hs > c.bar) c.bar = hs; }
            }
            uint64_t score = c.vram + (c.type == 2 ? (1ull << 40) : 0);
            if (!g_haveGpu || score > bestScore) { g_gpu = c; bestScore = score; g_haveGpu = TRUE; }
        }
    }
    if (destroy) destroy(inst, NULL);
    FreeLibrary(vk);
    return 0;
}
static BOOL GpuIsIntel(void) { return g_haveGpu && g_gpu.vendor == 0x8086; }
static BOOL GpuIsArc(void) { return g_haveGpu && g_gpu.vendor == 0x8086 && g_gpu.type == 2; }
static BOOL GpuHasRebar(void) { return g_haveGpu && g_gpu.bar > (1ull << 30); }
static int PcTier(void)   /* 2 strong, 1 weak, 0 very weak: the same rule as UFC5.exe */
{
    int vmb = g_haveGpu ? (int)(g_gpu.vram >> 20) : 0;
    if (g_threads <= 4 || (vmb && vmb < 3800) || g_ramGb < 12) return 0;
    if (g_threads < 12 || (vmb && vmb < 7500) || g_ramGb < 16 || g_oldCpu) return 1;   /* an old core is weak, whatever its thread count */
    return 2;
}
static void PcSig(wchar_t *out, int n)
{
    _snwprintf(out, n, L"%d|%d|%s|%llu|%dx%d@%d", g_threads, g_ramGb, g_haveGpu ? g_gpu.name : L"-", g_haveGpu ? (unsigned long long)(g_gpu.vram >> 20) : 0ull, g_monW, g_monH, g_monHz);
    out[n - 1] = 0;
}
static void PcSummary(wchar_t *out, int n)
{
    int tier = PcTier(); const wchar_t *tn = tier == 0 ? L2(L"PC muito fraco", L"very weak PC") : tier == 1 ? L2(L"PC fraco", L"weak PC") : L2(L"PC forte", L"strong PC");
    _snwprintf(out, n, L"%s: %s (%llu GB), %d threads, %d GB RAM, %dx%d %d Hz%s%s", tn, g_haveGpu ? g_gpu.name : L"GPU?",
               g_haveGpu ? (unsigned long long)((g_gpu.vram + (512ull << 20)) >> 30) : 0ull, g_threads, g_ramGb, g_monW, g_monH, g_monHz,
               GpuIsArc() ? L2(L", modo Intel Arc", L", Intel Arc mode") : L"",
               g_oldCpu ? L2(L", processador antigo (modo leve)", L", old processor (light mode)") : L"");
    out[n - 1] = 0;
}
/* the PS4 ports run the game's own x86-64 code, built for the PS4's Jaguar CPU: without these instructions the engine
   stops at once ("this CPU lacks BMI1, MOVBE, LZCNT", photo of a player's log, 2026-10-19). Fills the missing names.
   An Intel Core of the 3rd generation (Ivy Bridge) or older has none of BMI1, MOVBE and LZCNT, and LZCNT there runs as
   BSR, a different instruction that gives wrong results without any error: these processors cannot run UFC 1-4. */
static BOOL CpuLacksPs4(wchar_t *out, int n)
{
    static int cached = -1; static wchar_t names[64];
    if (cached < 0) {
        unsigned a, b, c, d; BOOL avx = FALSE, movbe = FALSE, bmi1 = FALSE, lzcnt = FALSE;
        if (__get_cpuid(1, &a, &b, &c, &d)) { movbe = (c >> 22) & 1; avx = (c >> 28) & 1; }
        if (__get_cpuid_count(7, 0, &a, &b, &c, &d)) bmi1 = (b >> 3) & 1;
        if (__get_cpuid(0x80000001, &a, &b, &c, &d)) lzcnt = (c >> 5) & 1;
        names[0] = 0;
        /* 5.2: AVX that Windows / a VM does not save is no AVX (the engine faults on its first ymm instruction):
           CpuBaseOk then fails too, and CpuNoMessage explains it */
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

/* ---------------------------------------------------------------- automatic settings of UFC 1-4
   Parsec mode (UFC 1-4): the host runs the game and a friend joins through Parsec, whose virtual pad becomes Player 2
   (one player per controller is the engine's default). What lowers the delay on the host: one frame queued instead of
   several (low_latency, the launcher's BB_FRAMES_AHEAD=1), a steady 60 FPS for Parsec's 60 FPS stream and its encoder,
   and Mailbox (no tearing in the stream; Intel keeps its VSync). The settings it replaces are kept in portheon.ini and
   come back when it is turned off. */
static void ParsecApply(int g)
{
    PutInt(g, L"low_latency", 1);
    if (GetInt(g, L"fps_limit", 60) != 60) PutInt(g, L"fps_limit", 60);
    if (!GpuIsIntel()) PutInt(g, L"present_mode", 0);
    /* the controllers keep working with Parsec's window in front: PadEnv turns that on in Parsec mode (3.5 set
       pad_background for good here, and it stayed on after Parsec mode was off) */
}
static void ParsecSet(int g, BOOL on)
{
    if (G[g].ps5) {   /* 4.3.1: UFC5.exe / UFC6.exe read "parsec" in their own ini and set the engine up for it themselves */
        wchar_t pk[32]; _snwprintf(pk, 32, L"parsec_%s", G[g].id); WritePrivateProfileStringW(L"PORTHEON", pk, on ? L"1" : NULL, g_pIni);
        PutStr(g, L"parsec", on ? L"1" : L"0"); return;
    }
    static const wchar_t *const keys[3] = {L"low_latency", L"fps_limit", L"present_mode"};
    wchar_t pk[48], b[16];
    for (int i = 0; i < 3; i++) {
        _snwprintf(pk, 48, L"parsec_%s_%s", G[g].id, keys[i]);
        if (on) { _snwprintf(b, 16, L"%d", GetInt(g, keys[i], i == 1 ? 60 : 0)); WritePrivateProfileStringW(L"PORTHEON", pk, b, g_pIni); }
        else { int v = GetPrivateProfileIntW(L"PORTHEON", pk, -1, g_pIni); if (v >= 0) PutInt(g, keys[i], v); WritePrivateProfileStringW(L"PORTHEON", pk, NULL, g_pIni); }
    }
    _snwprintf(pk, 48, L"parsec_%s", G[g].id); WritePrivateProfileStringW(L"PORTHEON", pk, on ? L"1" : NULL, g_pIni);
    if (on) ParsecApply(g);
}
/* 7.3: a fight started from the ARENA's host button ("PORTHEON.exe --play UFCn --arena") needs Parsec mode (UFC 6:
   the 2-player engine; UFC 1-4: local versus and the pad with the window out of focus). PORTHEON turns it on for
   that game and marks it (arena_parsec_<id>); the next PLAY that does not come from the ARENA turns it off again, so
   playing alone keeps its own engine and frame rate. A player who turned Parsec mode on by hand has no mark. */
static BOOL g_arenaPlay;
static void ArenaParsecKey(int g, wchar_t *pk) { _snwprintf(pk, 48, L"arena_parsec_%s", G[g].id); pk[47] = 0; }
static void ArenaHostPrep(int g)
{
    if (g < 0 || g >= NGAMES || !ARENA_GAME(g)) return;
    g_arenaPlay = TRUE;
    if (GetInt(g, L"parsec", 0)) return;
    wchar_t pk[48]; ArenaParsecKey(g, pk);
    ParsecSet(g, TRUE); WritePrivateProfileStringW(L"PORTHEON", pk, L"1", g_pIni);
}
static void ArenaParsecAtPlay(int g)   /* Play(): a start that is not the ARENA's gives back what ArenaHostPrep turned on */
{
    /* 7.3: PLAY while the ARENA window is open counts as the ARENA's (the host may start the game from PORTHEON's own
       button instead of the fight's): Parsec mode on for that game, off again at the first PLAY with the ARENA closed */
    if (!g_arenaPlay && g >= 0 && g < NGAMES && ARENA_GAME(g) && FindWindowW(L"PortheonArena", NULL) && PIni(L"arena_parsec_auto", 1)) ArenaHostPrep(g);
    BOOL arena = g_arenaPlay; g_arenaPlay = FALSE;
    if (arena || g < 0 || g >= NGAMES || !ARENA_GAME(g)) return;
    wchar_t pk[48]; ArenaParsecKey(g, pk);
    if (!GetPrivateProfileIntW(L"PORTHEON", pk, 0, g_pIni)) return;
    WritePrivateProfileStringW(L"PORTHEON", pk, NULL, g_pIni);
    if (GetInt(g, L"parsec", 0)) ParsecSet(g, FALSE);
}
/* the FPS limits of the PS4 ports (FPS4X, FPS3): UFC 1, 2 and 4 count game time per frame, so above 60 is experimental
   there; UFC 3 runs fights at any rate and is experimental above 144 (its code 23 at very high rates). 0 = no limit. */
/* 7.2 (2026-11-16, the owner: "tirar o limite de fps do 2"): UFC 2 runs its fights at any rate too, measured on the RTX
   4060: a fight at 85-95 FPS with the limit at 144, round clock 4:35 -> 3:05 in 90 real seconds (1.00x), 570 s without a
   crash. So UFC 2 gets UFC 3's rules: up to 144 is not experimental, the automatic setup follows the monitor, and a
   code 3 above 60 goes back to 60 once (ufc2_fps60 / ufc3_fps60) */
static BOOL FpsFree(int g) { return g == 1 || g == 2; }
static const wchar_t *Fps60Key(int g) { return g == 1 ? L"ufc2_fps60" : L"ufc3_fps60"; }
static int FpsSafeMax(int g) { return FpsFree(g) ? 144 : 60; }
static BOOL FpsExperimental(int g, int fl) { return fl == 0 || fl > FpsSafeMax(g); }
/* Released: 75, 100, 165 and 180 are in the lists (FPS4X, FPS3) but were put back to 60 here at every PLAY; they stay
   now, and PLAY hands them to the engine itself (BB_FPS_LIMIT), since the ports' launchers only know 30/60/90/120/144/240/0 */
static BOOL FpsAllowed(int g, int fl) { return fl == 0 || fl == 30 || fl == 60 || fl == 75 || fl == 90 || fl == 100 || fl == 120 || fl == 144 || fl == 165 || fl == 180 || fl == 240 || (FpsFree(g) && fl > 0 && fl <= 144); }
/* for the messages: "240 FPS" / "Sem limite de FPS", or with at "a 240 FPS" / "sem limite de FPS" */
static void FpsName(int fl, BOOL at, wchar_t *out, int n)
{
    if (fl) _snwprintf(out, n, at ? L2(L"a %d FPS", L"at %d FPS") : L"%d FPS", fl);
    else _snwprintf(out, n, L"%s", at ? L2(L"sem limite de FPS", L"with no FPS limit") : L2(L"Sem limite de FPS", L"No FPS limit"));
    out[n - 1] = 0;
}
/* 3.5, at every start of PORTHEON and every PLAY of UFC 1-4 (logs of 2026-10-22): the debug switches of the ports' own
   window stay off. Two players had the Vulkan validation layer on ("Validation Warning ... VUID-..." on an MX450), which
   makes a game crawl and can close it, and the shader cache was wiped at each start ("Shader cache: cleared for this
   launch"), which rebuilds every shader in every fight. The FPS limit goes back to the game's range. */
static void SanitizePs4(int g)
{
    if (!PS4G(g)) return;
    wchar_t sf[MAX_PATH]; SfPath(g, sf); if (!FileExists(sf)) return;
    g_kvGame = -1;
    static const wchar_t *const debugKeys[] = {L"validation", L"clear_cache", L"menu_trace", L"frame_stats"};
    for (int i = 0; i < N(debugKeys); i++) if (GetInt(g, debugKeys[i], 0)) PutInt(g, debugKeys[i], 0);
    int fl = GetInt(g, L"fps_limit", 60);
    if (!FpsAllowed(g, fl)) PutInt(g, L"fps_limit", FpsFree(g) && fl > 144 ? 144 : 60);
    g_kvGame = -1;
}
/* the best settings of UFC 1-4 for this PC (what the ports' notes say holds on these games):
   - FPS 60 (UFC 1 fights stay at its own 30); 3.5: a very weak PC runs UFC 2 and 4 at the PS4's own 30 (the port's
     notes: "30 FPS (PS4) is the most stable"), half the CPU work per second;
   - resolution: 1080p; very weak PCs 720p shown at the monitor's size, "ultra" below 4 threads/3 GB;
   - PS4 Pro mode only on strong NVIDIA/AMD cards (UFC 2-4; UFC 1 has none);
   - Intel: VSync (Fifo), the present mode its driver always has, and never PS4 Pro mode;
   - weak PCs: the crowd off (+5 to 10 FPS in the fight) in UFC 2, 3 and 4; 3.5.3: the motion blur off on every PC. */
/* 4.0.1: the recommended value of one setting for this PC, in one place: the automatic setup writes these, and the setup
   guide (ui_screens.c) shows them next to the current ones. -1: no recommendation (the key does not apply to g). */
static int RecValue(int g, const wchar_t *k)
{
    if (G[g].ps5) return -1;   /* 4.0.2: UFC 5 sets itself up (UFC5.exe --auto-config) */
    int t = PcTier(); BOOL nvamd = g_haveGpu && (g_gpu.vendor == 0x10DE || g_gpu.vendor == 0x1002);
    int vmb = g_haveGpu ? (int)(g_gpu.vram >> 20) : 0; BOOL parsec = GetInt(g, L"parsec", 0) != 0;
    #define K(s) (!wcscmp(k, s))
    if (K(L"fullscreen") || K(L"skip_movies")) return 1;
    if (K(L"frame_stats") || K(L"validation") || K(L"clear_cache") || K(L"upscaler") || K(L"sharpness")) return 0;
    if (K(L"low_latency")) return parsec ? 1 : 0;   /* Parsec mode stays on through a new auto setup (ParsecApply) */
    /* 5.2: UFC 3 runs its fights at any rate (FpsSafeMax), so it follows the monitor up to 144 (the FPS3 steps at or
       under its Hz) on NVIDIA/AMD cards; Intel cards (IntelGuard: above 60 stopped them, code 23), Parsec mode's
       steady 60, very weak PCs and a UFC 3 that crashed above 60 once (ufc3_fps60, OnExited) keep 60 */
    if (K(L"fps_limit") && FpsFree(g) && !parsec && t >= 1 && !GpuIsIntel() && !PIni(Fps60Key(g), 0))
        return g_monHz >= 144 ? 144 : g_monHz >= 120 ? 120 : g_monHz >= 90 ? 90 : 60;
    if (K(L"fps_limit")) return parsec ? 60 : (t == 0 && (g == 1 || g == 3)) ? 30 : 60;
    if (K(L"present_mode")) return 2;   /* 4.1: Immediate everywhere (the user: no VSync, no FPS cap complaints) */
    /* 720p shown at the monitor's size without FSR 1.0: 720p + FSR stopped a UFC 4 fight's GPU (code 23, 2026-10-20) */
    if (K(L"resolution") || K(L"resui")) return t == 0 ? 0 : 1;
    if (K(L"display")) return t == 0 && g_monH >= 1080 ? 1 : 0;
    if (K(L"ultra")) return t == 0 && (g_threads <= 4 || (vmb && vmb < 3000)) ? 1 : 0;
    if (K(L"neo")) return g == 0 ? -1 : (t == 2 && nvamd && vmb >= 7500) ? 1 : 0;
    if (K(L"patch:crowd")) return (g == 1 || g == 2) && PatchState(g, L"crowd", -1) >= 0 ? (t < 2 ? 1 : 0) : -1;
    /* 3.5.3: motion blur off on every PC, in every UFC that has the patch (UFC 3's community one, UFC 4's own line);
       4.0.4: UFC 2 too, camera and object blur (PORTHEON's lines in its community XML). UFC 1 has no community XML:
       its blur lines were taken out after a Career crash (the user's call), so it has no switch. */
    if (K(L"patch:blur")) return g <= 2 && PatchState(g, L"blur", -1) >= 0 ? 1 : -1;
    if (K(L"pini:ufc4_crowd_off")) return g == 3 ? (t < 2 ? 1 : 0) : -1;
    if (K(L"pini:ufc4_blur_off")) return g == 3 ? 1 : -1;
    /* what the automatic setup leaves alone, as the guide recommends it */
    if (K(L"language")) return LANGS[g_lang].ps4;
    if (K(L"pgame:padtype") || K(L"pgame:engine") || K(L"pgame:skin_mode") || K(L"pini:pad_background") || K(L"pini:ufc4_mouth_fix")) return 0;
    if (K(L"pgame:gpu_safe")) return g == 0 ? -1 : GetInt(g, k, 0);   /* PORTHEON turns it on after a code 23: kept */
    if (K(L"pini:engine_boost") || K(L"pini:cpu_all") || K(L"pini:async_pipelines")) return 1;
    if (K(L"pini:career_safe_ufc4")) return g == 3 ? 1 : -1;
    #undef K
    return -1;
}
static void AutoConfig4(int g)
{
    static const wchar_t *const keys[] = {L"fullscreen", L"skip_movies", L"low_latency", L"frame_stats", L"validation", L"clear_cache",
        L"fps_limit", L"present_mode", L"resolution", L"display", L"upscaler", L"sharpness", L"ultra", L"neo",
        L"patch:crowd", L"patch:blur", L"pini:ufc4_crowd_off", L"pini:ufc4_blur_off"};
    for (int i = 0; i < N(keys); i++) { int v = RecValue(g, keys[i]); if (v >= 0) PutInt(g, keys[i], v); }
    if (GetInt(g, L"parsec", 0)) ParsecApply(g);   /* Parsec mode stays on through a new auto setup */
}
/* PORTHEON 3.1: a port set up before on an old processor gets, once, only what eases the CPU in a fight (its other
   settings stay): PS4 Pro mode off, at most 60 FPS, and the crowd off in UFC 2-4 */
static void OldCpu4(int g)
{
    PutInt(g, L"neo", 0); PutInt(g, L"low_latency", 0);
    { int fl = GetInt(g, L"fps_limit", 60); if (fl == 0 || fl > 60) PutInt(g, L"fps_limit", 60); }   /* 3.5.1: 0 (no limit) is back */
    if ((g == 1 || g == 2) && PatchState(g, L"crowd", -1) >= 0) PatchState(g, L"crowd", 1);
    if (g == 3) PIniSet(L"ufc4_crowd_off", 1);
    if (GetInt(g, L"parsec", 0)) ParsecApply(g);
}
/* on the first start, and again when the PC changes (other card, more RAM, other monitor): every installed UFC 1-4 port
   gets the settings above. A port that was already set up by hand before this version keeps its settings. */
static int AutoConfigAll(BOOL force)
{
    wchar_t sig[400]; PcSig(sig, 400); int done = 0;
    for (int g = 0; g < 4; g++) {
        if (!PortOk(g)) continue;
        wchar_t k[32], old[400]; _snwprintf(k, 32, L"auto_%s", G[g].id);
        GetPrivateProfileStringW(L"PORTHEON", k, L"", old, 400, g_pIni);
        wchar_t sf[MAX_PATH]; SfPath(g, sf);
        BOOL had = FileExists(sf);
        wchar_t ko[32]; _snwprintf(ko, 32, L"oldcpu_%s", G[g].id);
        BOOL oldDone = GetPrivateProfileIntW(L"PORTHEON", ko, 0, g_pIni);
        if (g_oldCpu) WritePrivateProfileStringW(L"PORTHEON", ko, L"1", g_pIni);
        if (!force && !wcscmp(old, sig)) {
            if (g_oldCpu && !oldDone && had) { g_kvGame = -1; OldCpu4(g); done++; }
            continue;
        }
        if (force || !had || old[0]) { g_kvGame = -1; AutoConfig4(g); done++; }
        else if (g_oldCpu && !oldDone) { g_kvGame = -1; OldCpu4(g); done++; }
        WritePrivateProfileStringW(L"PORTHEON", k, sig, g_pIni);
    }
    g_kvGame = -1;
    return done;
}

static int g_autoDone4;   /* how many UFC 1-4 ports the automatic setup changed at this start (WM_APP_AUTOCFG says so) */
/* 4.0.2, UFC 5 (as in 3.5.0): UFC5.exe --auto-config (no window) reads the processor, RAM and video card and changes
   UFC5.ini only the first time or when that hardware changed (exit code 1); "Best settings" adds --force. It posts
   WM_APP_AUTOCFG with lParam 5 and the exit code. 4.2: UFC6.exe for UFC 6, the same way (it also finds the dump on the
   Desktop and writes it to UFC6.ini). */
/* 4.3: one thread per PS5 game: p = the game's index, plus 0x100 for --force; the message's lParam is 1 + the index */
static DWORD WINAPI PsAutoCfgThread(void *p)
{
    int g = (int)((UINT_PTR)p & 0xFF); BOOL force = ((UINT_PTR)p & 0x100) != 0;
    if (g < 0 || g >= NGAMES || !G[g].ps5) return 0;
    wchar_t exe[MAX_PATH], dir[MAX_PATH], cmd[MAX_PATH + 48]; PortExe(g, exe); PortDir(g, dir);
    if (!FileExists(exe)) return 0;
    _snwprintf(cmd, MAX_PATH + 48, force ? L"\"%s\" --auto-config --force" : L"\"%s\" --auto-config", exe);
    STARTUPINFOW si = {sizeof(si)}; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; PROCESS_INFORMATION pi = {0};
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, NULL, dir, &si, &pi)) return 0;
    CloseHandle(pi.hThread);
    DWORD code = 0;
    if (WaitForSingleObject(pi.hProcess, 30000) == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    PostMessageW(g_wnd, WM_APP_AUTOCFG, force ? 2 : code, 1 + g);
    return 0;
}
static void PsAutoCfg(int g, BOOL force) { HANDLE th = CreateThread(NULL, 0, PsAutoCfgThread, (void *)(UINT_PTR)(g | (force ? 0x100 : 0)), 0, NULL); if (th) CloseHandle(th); }

/* UFC 1 fight crash for everyone (logs sent by players on 2026-10-19, RTX 3060 Ti and RTX 4060 Laptop, NVIDIA 617.42):
   right after "shader 0xb61e59a3b11b03ad: break condition has no definition", the NVIDIA shader compiler
   (nvgpucomp64.dll+0x16a2f81) reads a bad address and the game closes with code 3. The two fight compute shaders
   0xb61e59a3b11b03ad and 0xb61e59a384acbdea get corrected SPIR-V, carried inside PORTHEON.exe (resources 201/202) and
   written to the engine's shader patch folder (%LOCALAPPDATA%\UFC1Native\user\shader\patch) before every start. */
static void InstallUfc1ShaderPatches(void)
{
    static const struct { int id; const wchar_t *name; } P[] = {{201, L"cs_0xb61e59a3b11b03ad_0.spv"}, {202, L"cs_0xb61e59a384acbdea_0.spv"}};
    wchar_t d[MAX_PATH]; LocalData(d, G[0].data); CreateDirectoryW(d, NULL);
    wcsncat(d, L"\\user", MAX_PATH - wcslen(d) - 1); CreateDirectoryW(d, NULL);
    wcsncat(d, L"\\shader", MAX_PATH - wcslen(d) - 1); CreateDirectoryW(d, NULL);
    wcsncat(d, L"\\patch", MAX_PATH - wcslen(d) - 1); CreateDirectoryW(d, NULL);
    for (int i = 0; i < 2; i++) {
        HRSRC r = FindResourceW(g_inst, MAKEINTRESOURCEW(P[i].id), (LPCWSTR)RT_RCDATA); if (!r) continue;
        DWORD sz = SizeofResource(g_inst, r); const void *p = LockResource(LoadResource(g_inst, r)); if (!p) continue;
        wchar_t f[MAX_PATH]; _snwprintf(f, MAX_PATH, L"%s\\%s", d, P[i].name); f[MAX_PATH - 1] = 0;
        HANDLE h = CreateFileW(f, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {   /* already there and identical: leave it */
            DWORD fs = GetFileSize(h, NULL), got = 0; BOOL same = FALSE;
            if (fs == sz) { void *b = malloc(sz); if (b) { ReadFile(h, b, sz, &got, NULL); same = got == sz && !memcmp(b, p, sz); free(b); } }
            CloseHandle(h); if (same) continue;
        }
        h = CreateFileW(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) { DWORD wr; WriteFile(h, p, sz, &wr, NULL); CloseHandle(h); }
    }
}

/* ---------------------------------------------------------------- community patches of UFC 2 and 3 */
/* 3.2.3 dropped the community 60+ FPS patch; the updater keeps an existing UFCn-community.xml, so take its entries out. */
static void StripPatch60(int g)
{
    wchar_t path[MAX_PATH]; PatchXmlPath(g, path);
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD sz = GetFileSize(h, NULL), got = 0; char *buf = (char *)malloc(sz + 1);
    if (!buf || sz > (8u << 20)) { CloseHandle(h); free(buf); return; }
    ReadFile(h, buf, sz, &got, NULL); CloseHandle(h); buf[got] = 0;
    BOOL changed = FALSE;
    for (char *m = buf; (m = strstr(m, "<Metadata ")) != NULL;) {
        char *tag = strchr(m, '>'), *nm = strstr(m, " Name=\"60+ FPS\"");
        char *close = strstr(m, "</Metadata>");
        if (!tag || !close || !nm || nm > tag) { m++; continue; }
        char *from = m; while (from > buf && (from[-1] == ' ' || from[-1] == '\t')) from--;
        char *to = close + 11; while (*to == '\r' || *to == '\n') { to++; if (to[-1] == '\n') break; }
        memmove(from, to, strlen(to) + 1); m = from; changed = TRUE;
    }
    if (changed) {
        h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) { DWORD wr; WriteFile(h, buf, (DWORD)strlen(buf), &wr, NULL); CloseHandle(h); }
    }
    free(buf);
}
/* the face fix of UFC 2 and 3 against the mesh stretching that pulls the mouth and the face out of shape: "Skinning
   Method 2" (GPU compute), which leaves the CPU free in Career. face_fix in portheon.ini: 1 = this (default), 2 = CPU
   skinning, 0 = off. 3.4.1: Intel graphics skin on the CPU (an Arc B580 stopped twice in Career training right after GPU
   compute skinning, code 23); intel_cpu_skin=0 goes back. */
static void FaceFix(int g)
{
    if (g < 1 || g > 2) return;
    StripPatch60(g);
    int mode = PIni(L"face_fix", 1);
    BOOL mesh = PatchState(g, L"mesh", -1) >= 0, skin2 = PatchState(g, L"skin2", -1) >= 0, skin0 = PatchState(g, L"skin0", -1) >= 0;
    BOOL intel = GpuIsIntel() && PIni(L"intel_cpu_skin", 1) && skin0;
    BOOL useMesh = (mode == 1 || mode == 2) && mesh, useSkin2 = mode == 1 && !mesh && skin2 && !intel,
         useSkin0 = mode == 2 || intel || (mode == 1 && !mesh && !skin2);
    if (mesh) PatchState(g, L"mesh", useMesh ? 1 : 0);
    if (skin2) PatchState(g, L"skin2", useSkin2 ? 1 : 0);
    if (skin0) PatchState(g, L"skin0", useSkin0 ? 1 : 0);
}
/* 6.0 final: UFC 2 and 3's 3D scene size (F4P_SCENE): the matching "Resolution WxH" entries of the community XML on,
   the others off; none at 2K/4K output */
static int SceneOf(int g) { int s = GetInt(g, L"pgame:scene", 0); if (s < 0 || s > 4 || GetInt(g, L"resolution", 1) >= 2) s = 0; return s; }
static void SceneRes(int g)
{
    if (g < 1 || g > 2) return;
    static const wchar_t *const k[5] = {NULL, L"res720", L"res540", L"res450", L"res360"};
    int s = SceneOf(g);
    for (int i = 1; i <= 4; i++) if (PatchState(g, k[i], -1) >= 0) PatchState(g, k[i], s == i ? 1 : 0);
}
static void SyncExperimentPatches(int g)
{
    wchar_t dir[MAX_PATH], src[MAX_PATH], pat[MAX_PATH], dst[MAX_PATH]; PortDir(g, dir); PatchXmlPath(g, src);
    if (!FileExists(src)) return;
    _snwprintf(pat, MAX_PATH, L"%s\\patches\\experiments\\*", dir); pat[MAX_PATH - 1] = 0;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
        _snwprintf(dst, MAX_PATH, L"%s\\patches\\experiments\\%s\\%s", dir, fd.cFileName, PathFindFileNameW(src)); dst[MAX_PATH - 1] = 0;
        CopyFileW(src, dst, FALSE);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

/* ---------------------------------------------------------------- UFC 4's patches
   3.3: UFC 4 runs with no community patch (every *-community.xml in its patches folder is switched off at each PLAY);
   it keeps the port's own UFC4.xml and PORTHEON's own file, UFC4-mouthfix.xml. */
static void XmlAllOff(const wchar_t *path)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD sz = GetFileSize(h, NULL), got = 0;
    if (sz > (8u << 20)) { CloseHandle(h); return; }
    char *buf = (char *)malloc(sz + 1), *out = (char *)malloc(sz + sz / 4 + 64);
    if (!buf || !out) { CloseHandle(h); free(buf); free(out); return; }
    ReadFile(h, buf, sz, &got, NULL); CloseHandle(h); buf[got] = 0;
    size_t o = 0; char *p = buf, *m; BOOL changed = FALSE;
    while ((m = strstr(p, "<Metadata ")) != NULL) {
        char *end = strchr(m, '>'); if (!end) break;
        char *en = strstr(m, "isEnabled=\"");
        if (!en || en > end) { size_t k = (size_t)(end + 1 - p); memcpy(out + o, p, k); o += k; p = end + 1; continue; }
        char *vq = strchr(en + 11, '"'); if (!vq) break;
        if (strncmp(en + 11, "false\"", 6)) changed = TRUE;
        size_t k = (size_t)(en + 11 - p); memcpy(out + o, p, k); o += k;
        memcpy(out + o, "false", 5); o += 5; p = vq;
    }
    size_t rest = strlen(p); memcpy(out + o, p, rest); o += rest; out[o] = 0;
    if (changed) {
        h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) { DWORD wr; WriteFile(h, out, (DWORD)o, &wr, NULL); CloseHandle(h); }
    }
    free(buf); free(out);
}
static void XmlDirAllOff(const wchar_t *dir, int depth)
{
    wchar_t pat[MAX_PATH], f[MAX_PATH]; _snwprintf(pat, MAX_PATH, L"%s\\*", dir); pat[MAX_PATH - 1] = 0;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == L'.') continue;
        Join(f, dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { if (depth < 2) XmlDirAllOff(f, depth + 1); }
        else if (StrStrIW(fd.cFileName, L"-community.xml") && !StrStrIW(fd.cFileName, L".bak")) XmlAllOff(f);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
/* a file with exactly this text (written only when it differs) */
static void WriteIfDifferent(const wchar_t *path, const char *text)
{
    size_t n = strlen(text);
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD sz = GetFileSize(h, NULL), got = 0; BOOL same = FALSE;
        if (sz == n) { char *b = (char *)malloc(n + 1); if (b) { ReadFile(h, b, (DWORD)n, &got, NULL); same = got == n && !memcmp(b, text, n); free(b); } }
        CloseHandle(h); if (same) return;
    }
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { DWORD wr; WriteFile(h, text, (DWORD)n, &wr, NULL); CloseHandle(h); }
}
/* The port's own UFC4.xml: the offline network fix. Without it the game creates DirtySDK's UPnP module, fails network
   start-up and later reads the destroyed module: "Guest fault: read access to 0000000000000018 at guest offset 0x137cde4
   (thread SimThread)" right after "NetLib"/"SocketRecv" start (RTX 4090 player, log of 2026-10-22 23:56,
   "Patches: 0 writes from 2 available patches"). At 2K/4K the engine reads its patches from patches\experiments\<res>,
   which had only the mouth fix: 3.5 writes this file into patches\ and into every experiments folder at each PLAY. */
static const char UFC4_XML[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<!-- ufc4: patches for EA SPORTS UFC 4 (CUSA14209, CUSA14204) 28.00, the native port's own fixes.\n"
    "     Addresses are PS4 virtual addresses (eboot base 0x400000), CUSA14209's; another executable\n"
    "     (CUSA14204) gets the write where the Signature matches (patches.py). -->\n"
    "<Patch>\n"
    "    <TitleID>\n"
    "        <ID>CUSA14209</ID>\n"
    "        <ID>CUSA14204</ID>\n"
    "    </TitleID>\n"
    "    <Metadata Title=\"EA SPORTS UFC 4\"\n"
    "              Name=\"Offline network: skip UPnP (DirtySDK -noupnp)\"\n"
    "              Note=\"NetConnStartup (0x13256b0) creates ProtoUpnp unless its parameters carry -noupnp. ProtoUpnp opens and binds a UDP socket; the offline runtime has no sockets, so the creation failed, NetConnStartup failed whole (-6) and the game later read the destroyed DirtyCM module (fault at 0x137cde4). This takes the -noupnp branch: UPnP is never used offline.\"\n"
    "              Author=\"ufc4 port\"\n"
    "              PatchVer=\"1.0\"\n"
    "              AppVer=\"28.00\"\n"
    "              AppElf=\"eboot.bin\"\n"
    "              isEnabled=\"true\">\n"
    "        <PatchList>\n"
    "            <!-- strstr(params, \"-noupnp\"); test rax,rax; je create_upnp  ->  never jump (as if found) -->\n"
    "            <Line Type=\"bytes\" Address=\"0x0172593b\" Value=\"9090\" Signature=\"48 85 C0 74 16 48 8B 7D B8 EB 29\" SignatureOffset=\"3\"/>\n"
    "        </PatchList>\n"
    "    </Metadata>\n"
    "</Patch>\n";
/* UFC4-mouthfix.xml, PORTHEON's own lines for UFC 4 (28.00 and 01.00), written at every PLAY:
   - the mouth fix, MeshWrapEnabled = 0 (3.3; off by default since 3.4.3, mesh wrap also fits the hair to the head);
   - CPU skinning ("Skinning Method 0", Lepexa), for Intel graphics and the mouth fix's mode 2;
   - 3.5: the crowd off ("Disable Crowd", Lepexa: jz -> jmp at the crowd set-up and its call skipped; each pattern
     matches the 28.00 eboot exactly once, at 0x5d3f6d and 0x5d3f5a) and the object motion blur off (the
     "ObjectMotionBlurEnabled" setting written 0 in its two readers, 0x3b3c53 and 0x3b67ca), both chosen in Extras;
   - 4.0.4: the camera motion blur off with the object one (the same switch): the renderer turns the blur pass and the
     shader's "MoBlurSwitch" on when the scene's blur amount ([rbx+0xf0], 0x475cf6) is above 0.001; it now reads 0.
     UFC 1-3 get the same in their community XML (tools\blur404.py). */
/* Released: crowdOff 2 = the crowd at a quarter, UFC4 Native Plus 1.2.0's "Medium" crowd (launcher.py, GPL-2.0, by
   Abdullah Tech): the crowd spawner's fEmitRate is multiplied by 4.0 (vmulss at 0x424128, constant at 0x22faff8); the
   line points it at a 1.0 the game already has (0x2284270). The pattern holds the instruction's own displacement, so it
   only matches an executable laid out as the 28.00 one (once there). */
/* Released: real 21:9 (pgame:wide21). The games hold the picture's proportion as the float 16/9 (39 8E E3 3F) in the
   viewport setup and in the camera's default (UFC 1 and 4 also in a camera fallback, UFC 4 in a projection vector):
   UFC4 Native Plus's launcher (GPL-2.0, Abdullah Tech) writes 64/27 (26 B4 17 40) over UFC 4's four places; the same
   code was found in UFC 1, 2 and 3 (_build-tools\released-wip-1112\opts). UFC 1 to 3 get their own file,
   patches\UFCn-portheon.xml; UFC 4's lines go into UFC4-mouthfix.xml (WriteUfc4Patches). */
/* 21:9 is only real where the picture can be wide: full screen on a monitor of 20:9 or wider, or a window (which
   PORTHEON then opens at 21:9, Wide21Window). On a 16:9 screen in full screen the switch does nothing. */
static BOOL Wide21On(int g)
{
    if (!PS4G(g) || !GetInt(g, L"pgame:wide21", 0)) return FALSE;
    if (GetInt(g, L"fullscreen", 1)) { int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN); return h > 0 && w * 9 >= h * 20; }
    return TRUE;
}
static const wchar_t *Wide21Window(void)
{
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    if (w >= 3440 && h >= 1440 + 80) return L"3440x1440";
    if (w >= 2560 && h >= 1080 + 80) return L"2560x1080";
    return L"1706x720";
}
/* The engine's presenter keeps the game's 16:9 picture ("fit": black bars at the sides of a wide window). UFC4 Native
   Plus's launcher makes a copy of its engine with 15 bytes of the inlined fit changed so that the picture fills the
   window; every SacredFire engine here has that same code exactly once. PORTHEON changes the port's
   bin\sacredfire-game.exe in place and back: always back before EnsureEngine (which compares that file with the engines
   it knows), and to "fill" after it when 21:9 is on. The file's times are kept, so the engine check stays the same. */
static BOOL PresentStretch(int g, BOOL want)
{
    static const unsigned char FIT[15] = {0x45, 0x89, 0xf1, 0x45, 0x29, 0xc1, 0x41, 0xd1, 0xe9, 0x41, 0x29, 0xcf, 0x41, 0xd1, 0xef},
                               FILL[15] = {0x45, 0x89, 0xf0, 0x44, 0x89, 0xf9, 0x45, 0x31, 0xc9, 0x45, 0x31, 0xff, 0x90, 0x90, 0x90};
    if (!PS4G(g)) return FALSE;
    wchar_t d[MAX_PATH], cur[MAX_PATH]; PortDir(g, d); _snwprintf(cur, MAX_PATH, L"%s\\bin\\sacredfire-game.exe", d); cur[MAX_PATH - 1] = 0;
    HANDLE h = CreateFileW(cur, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    DWORD sz = GetFileSize(h, NULL), got = 0; BOOL ok = FALSE; FILETIME ct, at, wt; BOOL haveT = GetFileTime(h, &ct, &at, &wt);
    unsigned char *b = sz > 15 && sz < (64u << 20) ? (unsigned char *)malloc(sz) : NULL;
    if (b && ReadFile(h, b, sz, &got, NULL) && got == sz) {
        const unsigned char *from = want ? FIT : FILL, *to = want ? FILL : FIT; DWORD pos = 0; int hits = 0, done = 0;
        for (DWORD i = 0; i + 15 <= sz; i++) {
            if (b[i] == from[0] && !memcmp(b + i, from, 15)) { pos = i; hits++; }
            else if (b[i] == to[0] && !memcmp(b + i, to, 15)) done++;
        }
        if (hits == 1 && done == 0) { DWORD wr = 0; SetFilePointer(h, (LONG)pos, NULL, FILE_BEGIN); ok = WriteFile(h, to, 15, &wr, NULL) && wr == 15; if (ok && haveT) SetFileTime(h, &ct, &at, &wt); }
        else if (hits == 0) ok = want ? done == 1 : TRUE;   /* already as wanted (or an engine without this code: nothing to undo) */
    }
    free(b); CloseHandle(h);
    return ok;
}
typedef struct { const char *a, *v; } MaskLine;
static const MaskLine W21_1[] = {{"48 B8 00 00 00 00 39 8E E3 3F 48 8D B3", "48b80000000026b41740"}, {"C7 02 00 00 00 00 C3 C7 02 39 8E E3 3F C3", "c70200000000c3c70226b41740c3"},
                                 {"C7 83 A4 00 00 00 39 8E E3 3F", "c783a400000026b41740"}},
    W21_2[] = {{"48 B8 00 00 00 00 39 8E E3 3F C5 F0 57 C9", "48b80000000026b41740"}, {"C7 02 00 00 00 00 C3 C7 02 39 8E E3 3F C3", "c70200000000c3c70226b41740c3"}},
    W21_3[] = {{"48 B9 00 00 00 00 39 8E E3 3F C5 F1 EF C9", "48b90000000026b41740"}, {"48 8B 42 08 C7 00 39 8E E3 3F C3", "488b4208c70026b41740c3"}},
    W21_4[] = {{"48 B9 00 00 00 00 39 8E E3 3F 48 C7 83 88 00 00 00", "48b90000000026b41740"}, {"48 8B 42 08 C7 00 39 8E E3 3F C3", "488b4208c70026b41740c3"},
               {"EB 0F B9 39 8E E3 3F C7 83 A0 00 00 00", "eb0fb926b41740"}, {"48 B9 39 8E E3 3F 39 8E E3 3F C4 C1 7A 5E", "48b926b4174026b41740"}};
static int Wide21Xml(char *x, int cap, int n, const char *title, const char *ver, const MaskLine *l, int nl, BOOL on)
{
    if (n <= 0 || n >= cap) return n;
    n += snprintf(x + n, cap - n, "    <Metadata Title=\"%s\" Name=\"Aspect 21:9\" Author=\"Abdullah Tech (UFC4 Native Plus, GPL-2.0), PORTHEON\" PatchVer=\"1.0\" AppVer=\"%s\" AppElf=\"eboot.bin\" isEnabled=\"%s\">\r\n        <PatchList>\r\n",
                  title, ver, on ? "true" : "false");
    for (int i = 0; i < nl && n > 0 && n < cap; i++) n += snprintf(x + n, cap - n, "            <Line Type=\"mask\" Address=\"%s\" Value=\"%s\"/>\r\n", l[i].a, l[i].v);
    if (n > 0 && n < cap) n += snprintf(x + n, cap - n, "        </PatchList>\r\n    </Metadata>\r\n");
    return n;
}
/* Released (2026-11-13): one more entry of patches\UFCn-portheon.xml, a single mask line */
static int OwnXml(char *x, int cap, int n, const char *title, const char *name, const char *author, const char *ver, const char *addr, const char *val, BOOL on)
{
    if (n <= 0 || n >= cap) return n;
    return n + snprintf(x + n, cap - n, "    <Metadata Title=\"%s\" Name=\"%s\" Author=\"%s\" PatchVer=\"1.0\" AppVer=\"%s\" AppElf=\"eboot.bin\" isEnabled=\"%s\">\r\n        <PatchList>\r\n"
                        "            <Line Type=\"mask\" Address=\"%s\" Value=\"%s\"/>\r\n        </PatchList>\r\n    </Metadata>\r\n", title, name, author, ver, on ? "true" : "false", addr, val);
}
/* Released (2026-11-13), UFC 1 to 3's shadows and crowd as game-code lines. NOT YET TESTED IN A FIGHT: found by static
   disassembly of the executables on the owner's PC (UFC 1 CUSA00222 01.08, UFC 2 CUSA01968 01.13, UFC 3 CUSA06534 01.14;
   _build-tools\released-wip-1112\opts\research-shadows.md and research-crowd.md); each mask matches exactly once there,
   and on another version a mask that is not found leaves that entry out.
   - Shadows (pgame:shadowq 1 / 2): the three games share UFC 4's ShadowManager; one caller fills its parameter block
     with the self-shadow map's width, height and slice count as immediates (UFC 1 0x5925bb 2048x2048x6, UFC 2 0x1db1e6
     and UFC 3 0x49fe10 1024x1024x6). Medium writes half the side, Low a quarter; the texel size the shaders get is
     computed from the same block. PORTHEON's own lines.
   - UFC 1's crowd off (pgame:crowdoff): Lepexa's UFC 2 "Disable Crowd" idea on UFC 1's code: after the venue's "Crowd"
     attribute is looked up, the "not there" branch (je 0x65b610 at 0x65b564) is always taken, so the crowd is never
     created (call 0x65aa30). The UFC 1 bytes are PORTHEON's.
   - UFC 3's crowd flashes at a quarter (pgame:crowd25): the twin of UFC4 Native Plus's "reduced crowd" (launcher.py,
     GPL-2.0, Abdullah Tech): function 0x5d1c0 multiplies the emit rate of the effect "glare_flash" by 4.0 (vmulss at
     0x5d283); the multiply becomes an 8-byte NOP (x1). It thins the camera flashes in the stands, not the people. */
static const char *const SHQ_ADDR[3] = {"C7 45 84 00 08 00 00 C7 45 88 00 08 00 00 C7 45 8C 06 00 00 00",
                                        "C7 84 24 EC 00 00 00 00 04 00 00 C7 84 24 F0 00 00 00 00 04 00 00 C7 84 24 F4 00 00 00 06 00 00 00",
                                        "C7 45 94 00 04 00 00 C7 45 98 00 04 00 00 C7 45 9C 06 00 00 00"},
    *const SHQ_HALF[3] = {"c7458400040000c7458800040000", "c78424ec00000000020000c78424f000000000020000", "c7459400020000c7459800020000"},
    *const SHQ_QUARTER[3] = {"c7458400020000c7458800020000", "c78424ec00000000010000c78424f000000000010000", "c7459400010000c7459800010000"};
static BOOL SafeOn(int g);   /* the compatible mode, further down */
static void WriteOwnPatches(int g)   /* UFC 1 to 3: patches\UFCn-portheon.xml, written at every PLAY */
{
    if (g < 0 || g > 2) return;
    static const char *const title[3] = {"UFC", "UFC 2", "UFC 3"};
    static const char *const ids[3] = {"        <ID>CUSA00222</ID>\r\n", "        <ID>CUSA01968</ID>\r\n        <ID>CUSA01990</ID>\r\n        <ID>CUSA01936</ID>\r\n", "        <ID>CUSA06534</ID>\r\n        <ID>CUSA06536</ID>\r\n"};
    static const char *const vers[3][4] = {{"01.08", "01.00", NULL}, {"01.00", "01.11", "01.13", NULL}, {"01.00", "01.14", NULL}};
    static const MaskLine *const wl[3] = {W21_1, W21_2, W21_3}; static const int wn[3] = {3, 2, 2};
    BOOL wide = Wide21On(g);
    int shq = GetInt(g, L"pgame:shadowq", 0); BOOL crowd1 = g == 0 && GetInt(g, L"pgame:crowdoff", 0), crowd25 = g == 2 && GetInt(g, L"pgame:crowd25", 0);
    if (SafeOn(g)) { shq = 0; crowd1 = crowd25 = FALSE; }   /* the compatible mode: none of the untested lines (the settings stay as they are) */
    char x[16384]; int n = snprintf(x, sizeof(x), "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n<!-- PORTHEON: this game's own lines (written at every PLAY from its settings) -->\r\n<Patch>\r\n    <TitleID>\r\n%s    </TitleID>\r\n", ids[g]);
    for (int v = 0; vers[g][v]; v++) n = Wide21Xml(x, (int)sizeof(x), n, title[g], vers[g][v], wl[g], wn[g], wide);
    for (int v = 0; vers[g][v]; v++) {   /* Released (2026-11-13): shadows and crowd, not yet tested in a fight (see above) */
        n = OwnXml(x, (int)sizeof(x), n, title[g], "Shadows: half-size shadow map", "PORTHEON", vers[g][v], SHQ_ADDR[g], SHQ_HALF[g], shq == 1);
        n = OwnXml(x, (int)sizeof(x), n, title[g], "Shadows: quarter-size shadow map", "PORTHEON", vers[g][v], SHQ_ADDR[g], SHQ_QUARTER[g], shq == 2);
        if (g == 0) n = OwnXml(x, (int)sizeof(x), n, title[g], "Crowd off", "Lepexa (Disable Crowd, UFC 2), PORTHEON", vers[g][v],
                               "84 C0 0F 84 A6 00 00 00 89 9D B0 FE FF FF", "84c0e9a700000090", crowd1);
        if (g == 2) n = OwnXml(x, (int)sizeof(x), n, title[g], "Crowd flashes reduced (25%)", "Abdullah Tech (UFC4 Native Plus, GPL-2.0), PORTHEON", vers[g][v],
                               "C5 FA 59 05 ?? ?? ?? ?? C5 FA 2C D0 E8 ?? ?? ?? ?? 48 8B 5D B0", "0f1f840000000000", crowd25);
    }
    if (n <= 0 || n >= (int)sizeof(x) - 16) return;
    snprintf(x + n, sizeof(x) - n, "</Patch>\r\n");
    wchar_t d[MAX_PATH], pd[MAX_PATH], f[MAX_PATH], name[40]; PortDir(g, d); Join(pd, d, L"patches"); CreateDirectoryW(pd, NULL);
    _snwprintf(name, 40, L"UFC%d-portheon.xml", g + 1); Join(f, pd, name); WriteIfDifferent(f, x);
    wchar_t pat[MAX_PATH]; _snwprintf(pat, MAX_PATH, L"%s\\experiments\\*", pd); pat[MAX_PATH - 1] = 0;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do { if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.') {
             _snwprintf(f, MAX_PATH, L"%s\\experiments\\%s\\%s", pd, fd.cFileName, name); f[MAX_PATH - 1] = 0; WriteIfDifferent(f, x); } } while (FindNextFileW(h, &fd));
    FindClose(h);
}
static void WriteUfc4Patches(const wchar_t *dir, int mouth, int crowdOff, BOOL blurOff, int shadows, int scene)
{
    static const char *const ver[2] = {"28.00", "01.00"};
    char x[20000]; int n = snprintf(x, sizeof(x),
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
        "<!-- PORTHEON %ls: UFC 4's own lines (written at every PLAY from PORTHEON's UFC 4 settings) -->\r\n"
        "<Patch>\r\n    <TitleID>\r\n        <ID>CUSA14209</ID>\r\n        <ID>CUSA14204</ID>\r\n    </TitleID>\r\n", APP_VER);
    for (int v = 0; v < 2 && n > 0 && n < (int)sizeof(x); v++)
        n += snprintf(x + n, sizeof(x) - n,
        "    <Metadata Title=\"UFC 4\" Name=\"Mouth fix: mesh wrap off\" Author=\"Lepexa (Mesh Wrap), PORTHEON\" PatchVer=\"1.1\" AppVer=\"%s\" AppElf=\"eboot.bin\" isEnabled=\"%s\">\r\n"
        "        <PatchList>\r\n"
        "            <Line Type=\"mask\" Address=\"8A 00 41 88 85 F9 00 00 00\" Value=\"41c685f90000000090\"/>\r\n"
        "            <Line Type=\"mask\" Address=\"E8 59 0F D5 FF 41 88 87 F9 00 00 00\" Value=\"31c0909090\"/>\r\n"
        "        </PatchList>\r\n    </Metadata>\r\n"
        "    <Metadata Title=\"UFC 4\" Name=\"Mouth fix: CPU skinning\" Author=\"Lepexa (Skinning Method 0)\" PatchVer=\"1.0\" AppVer=\"%s\" AppElf=\"eboot.bin\" isEnabled=\"%s\">\r\n"
        "        <PatchList>\r\n            <Line Type=\"mask\" Address=\"41 8B 85 E0 00 00 00 83 F8 04 74 09 83 F8 01\" Value=\"b800000000909083f804740983f801\"/>\r\n        </PatchList>\r\n    </Metadata>\r\n"
        "    <Metadata Title=\"UFC 4\" Name=\"Crowd off\" Author=\"Lepexa (Disable Crowd), PORTHEON\" PatchVer=\"1.0\" AppVer=\"%s\" AppElf=\"eboot.bin\" isEnabled=\"%s\">\r\n"
        "        <PatchList>\r\n"
        "            <Line Type=\"mask\" Address=\"02 48 85 FF 49 89 DF 74 18\" Value=\"024885ff4989dfeb18\"/>\r\n"
        "            <Line Type=\"mask\" Address=\"E8 31 0F 00 00 48 8B\" Value=\"9090909090488b\"/>\r\n"
        "        </PatchList>\r\n    </Metadata>\r\n"
        "    <Metadata Title=\"UFC 4\" Name=\"Object motion blur off\" Author=\"Lepexa (Disable Object Motion Blur), PORTHEON\" PatchVer=\"1.0\" AppVer=\"%s\" AppElf=\"eboot.bin\" isEnabled=\"%s\">\r\n"
        "        <PatchList>\r\n"
        "            <Line Type=\"mask\" Address=\"8A 00 41 88 85 EA 00 00 00\" Value=\"41c685ea0000000090\"/>\r\n"
        "            <Line Type=\"mask\" Address=\"E8 11 0F D5 FF 41 88 87 EA 00 00 00\" Value=\"31c0909090418887ea000000\"/>\r\n"
        "        </PatchList>\r\n    </Metadata>\r\n"
        "    <Metadata Title=\"UFC 4\" Name=\"Camera motion blur off\" Author=\"PORTHEON\" PatchVer=\"1.0\" AppVer=\"%s\" AppElf=\"eboot.bin\" isEnabled=\"%s\">\r\n"
        "        <PatchList>\r\n            <Line Type=\"mask\" Address=\"C5 FA 10 83 F0 00 00 00 C5 F8 2E 05 ?? ?? ?? ?? 41 0F 97 C7\" Value=\"c5f857c00f1f4000\"/>\r\n        </PatchList>\r\n    </Metadata>\r\n"
        /* 5.0: the shadows (Lepexa's "Disable Shadows Fighters" / "In Octagon" / "All Shadows": the two jumps over the
           shadow passes, and for "all" the shadow map size read as 1x1) */
        "    <Metadata Title=\"UFC 4\" Name=\"Shadows: fighters off\" Author=\"Lepexa (Disable Shadows Fighters), PORTHEON\" PatchVer=\"1.0\" AppVer=\"%s\" AppElf=\"eboot.bin\" isEnabled=\"%s\">\r\n"
        "        <PatchList>\r\n            <Line Type=\"mask\" Address=\"0F 8E B7 00 00 00 4C 8D A5\" Value=\"e9b8000000904c8da5\"/>\r\n        </PatchList>\r\n    </Metadata>\r\n"
        "    <Metadata Title=\"UFC 4\" Name=\"Shadows: octagon off\" Author=\"Lepexa (Disable Shadows In Octagon), PORTHEON\" PatchVer=\"1.0\" AppVer=\"%s\" AppElf=\"eboot.bin\" isEnabled=\"%s\">\r\n"
        "        <PatchList>\r\n            <Line Type=\"mask\" Address=\"0F 8E 4F 01 00 00 4C\" Value=\"e950010000904c\"/>\r\n        </PatchList>\r\n    </Metadata>\r\n"
        "    <Metadata Title=\"UFC 4\" Name=\"Shadows: shadow map 1x1\" Author=\"Lepexa (Disable All Shadows), PORTHEON\" PatchVer=\"1.0\" AppVer=\"%s\" AppElf=\"eboot.bin\" isEnabled=\"%s\">\r\n"
        "        <PatchList>\r\n            <Line Type=\"mask\" Address=\"8B 46 20 89 85 60 FE FF FF 8B 46 24 89 85 64 FE FF FF\" Value=\"8b01000000898560feffff898564feffff90\"/>\r\n        </PatchList>\r\n    </Metadata>\r\n",
        ver[v], (mouth == 1 || mouth == 2) ? "true" : "false", ver[v], (mouth == 2 || mouth == 3) ? "true" : "false",
        ver[v], crowdOff == 1 ? "true" : "false", ver[v], blurOff ? "true" : "false", ver[v], blurOff ? "true" : "false",
        ver[v], (shadows == 1 || shadows == 3) ? "true" : "false", ver[v], (shadows == 2 || shadows == 3) ? "true" : "false", ver[v], shadows == 3 ? "true" : "false");
    /* 6.0 final: the 3D scene's size (F4P_SCENE): Lepexa's "Resolution WxH" lines (UFC2-UFC4-PC-Port v3.4's XML): the
       game's 1600x900 render size (0x384, 0x640) written smaller */
    {   static const struct { const char *name, *val; } SC4[4] = {
            {"Resolution 1280x720", "070000eb15b8d0020000be0005000089"}, {"Resolution 960x540", "070000eb15b81c020000bec003000089"},
            {"Resolution 800x450 (Ultra Performance)", "070000eb15b8c2010000be2003000089"}, {"Resolution 640x360 (Extreme Performance)", "070000eb15b868010000be8002000089"}};
        for (int v = 0; v < 2 && n > 0 && n < (int)sizeof(x); v++)
            for (int i = 0; i < 4 && n > 0 && n < (int)sizeof(x); i++)
                n += snprintf(x + n, sizeof(x) - n,
                    "    <Metadata Title=\"UFC 4\" Name=\"%s\" Author=\"Lepexa\" PatchVer=\"1.0\" AppVer=\"%s\" AppElf=\"eboot.bin\" isEnabled=\"%s\">\r\n"
                    "        <PatchList>\r\n            <Line Type=\"mask\" Address=\"07 00 00 eb 15 b8 84 03 00 00 be 40 06 00 00 89\" Value=\"%s\"/>\r\n        </PatchList>\r\n    </Metadata>\r\n",
                    SC4[i].name, ver[v], scene == i + 1 ? "true" : "false", SC4[i].val);
    }
    for (int v = 0; v < 2 && n > 0 && n < (int)sizeof(x); v++)   /* Released: the crowd at a quarter */
        n += snprintf(x + n, sizeof(x) - n,
            "    <Metadata Title=\"UFC 4\" Name=\"Crowd reduced (25%%)\" Author=\"Abdullah Tech (UFC4 Native Plus, GPL-2.0), PORTHEON\" PatchVer=\"1.0\" AppVer=\"%s\" AppElf=\"eboot.bin\" isEnabled=\"%s\">\r\n"
            "        <PatchList>\r\n            <Line Type=\"mask\" Address=\"C5 FA 10 45 A4 C5 FA 59 05 C8 6E ED 01\" Value=\"c5fa1045a4c5fa59054001e601\"/>\r\n        </PatchList>\r\n    </Metadata>\r\n",
            ver[v], crowdOff == 2 ? "true" : "false");
    for (int v = 0; v < 2; v++) n = Wide21Xml(x, (int)sizeof(x), n, "UFC 4", ver[v], W21_4, 4, Wide21On(3));   /* Released: real 21:9 */
    if (n <= 0 || n >= (int)sizeof(x) - 16) return;
    snprintf(x + n, sizeof(x) - n, "</Patch>\r\n");
    wchar_t f[MAX_PATH]; Join(f, dir, L"UFC4-mouthfix.xml"); WriteIfDifferent(f, x);
    Join(f, dir, L"UFC4.xml"); WriteIfDifferent(f, UFC4_XML);
}
static void Ufc4Prepare(void)
{
    wchar_t d[MAX_PATH], pd[MAX_PATH]; PortDir(3, d); Join(pd, d, L"patches"); CreateDirectoryW(pd, NULL);
    XmlDirAllOff(pd, 0);   /* every community patch off */
    int mode = PIni(L"ufc4_mouth_fix", 0);
    /* 3.4.2: Intel graphics also skin UFC 4's fighters on the CPU, as UFC 2 and 3 do since 3.4.1: Arc cards stopped
       (code 23) on fighter draws right after GPU compute skinning. intel_cpu_skin=0 goes back. */
    if (GpuIsIntel() && PIni(L"intel_cpu_skin", 1)) mode = mode == 1 ? 2 : mode == 0 ? 3 : mode;
    int crowdOff = PIni(L"ufc4_crowd_off", 0), blurOff = PIni(L"ufc4_blur_off", 0) != 0; int shadows = PIni(L"ufc4_shadows", 0);
    int scene = SceneOf(3);   /* 6.0 final */
    WriteUfc4Patches(pd, mode, crowdOff, blurOff, shadows, scene);
    wchar_t pat[MAX_PATH]; _snwprintf(pat, MAX_PATH, L"%s\\experiments\\*", pd); pat[MAX_PATH - 1] = 0;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.') {
                 wchar_t ed[MAX_PATH]; _snwprintf(ed, MAX_PATH, L"%s\\experiments\\%s", pd, fd.cFileName); ed[MAX_PATH - 1] = 0;
                 WriteUfc4Patches(ed, mode, crowdOff, blurOff, shadows, scene); } } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (GetInt(3, L"resolution", 1) >= 2 && GetInt(3, L"neo", 0)) PutInt(3, L"neo", 0);
}

/* 4.3.2: the third-person fight camera of UFC 1-4 (F4_CAM3P): the port's patches\UFCn-camera-3a-pessoa.xml, and its
   copy in every patches\experiments folder (the engine's patches at 2K/4K), with its patch on while the switch is.
   UFC 2's file is the community mod exactly as it was shared (1.13), so a copy put in by hand is replaced by the same
   text; UFC 1, 3 and 4 get the same idea carried to their own code (_build-tools\camera3p). %s is isEnabled.
   6.1: UFC 1, 3 and 4 = UFC 6's live broadcast camera (UFC6.exe 1.21.0, ps5cam.py 'tvfix'). UFC 3 (Classic style) and
   UFC 4 (Medium/Wide/Tight): normal <-> TV, four still cameras high at the cage side with cuts (camera3p\ufc3\tv61,
   ufc4\tv61; the 3rd/1st-person modes are gone, as in UFC 6). UFC 1: one TV camera high at the cage side that follows
   the fight, no cuts (camera3p\ufc1\tv61: no proven code cave for the cut director). Same file names, so the 6.0
   files are replaced on the next PLAY (WriteIfDifferent). Rebuilt with camera3p\tools\set_cam3p_xml.py. */
static const char *const CAM3P_XML[4] = {
    "<?xml version=\"1.0\"?>\n"
    "<Patch>\n"
    "    <TitleID>\n"
    "        <ID>CUSA00222</ID>\n"
    "    </TitleID>\n"
    "    <Metadata Title=\"UFC\" Name=\"Camera: live broadcast (Select toggles)\" Author=\"PORTHEON\" PatchVer=\"2.0\" AppVer=\"01.08\" AppElf=\"eboot.bin\" Note=\"Press Select / View / touchpad during a stand-up fight to switch between the normal camera and a TV camera: one camera high at the cage side, looking down on both fighters and following the fight from the side the game picks (no cuts in UFC 1). Press again for the normal camera. Starts on normal. Ground and clinch cameras stay normal.\" isEnabled=\"%s\">\n"
    "        <PatchList>\n"
    "            <Line Type=\"mask\" Address=\"41 B9 14 01 00 00 41 B8 03 01 00 00 BA 07 01 00 00 B9 06 01 00 00 41 BF 05 01 00 00 41 BB 0D 01 00 00 BE 0C 01 00 00\" Value=\"b9060100008d5101448d41fd448d490e448d79ff448d59078d71064885db75068905cd9ab80190\"/>\n"
    "            <Line Type=\"mask\" Address=\"C4 E3 79 04 D8 00 C4 C1 78 29 00 C5 E8 C2 E3 02 C5 E0 C2 1D 17 A7 AF 01 02 C5 D8 54 DB C5 E0 C2 D9 00 C5 F8 50 C3 83 F8 0F 74 54 C4 E3 79 04 D8 55 C5 E8 C2 E3 02 C5 E0 C2 1D F1 A6 AF 01 02 C5 D8 54 DB C5 E0 C2 D9 00 C5 F8 50 C3 83 F8 0F 74 2E C4 E3 79 04 C0 AA C5 E8 C2 D0 02 C5 F8 C2 05 CB A6 AF 01 02 C5 E8 54 C0 C5 E8 57 D2 C5 F8 C2 C2 00 C5 F8 50 C0 83 F8 0F 0F 85 C8 FE FF FF C4 C1 78 29 08 C3 66 66 2E 0F 1F 84 00 00 00 00 00\" Value=\"f605fbe45a02100f95c088c1860df2e45a02f6d120c1300dece45a02744ac4c1785cecc4e3510ce90ac4e35140f57fc4c1782ef77232c5f851f6c5d05eeec4e27918355115be01c5d059eec4c15058ecc5fa103528da7801c4e37904f651c5d058eec4e3510cc008c4c1782900c5f85cd8c5e0c2d900c5f850c324073c077405c4c1782908c3cccccccccccccccccccc\"/>\n"
    "        </PatchList>\n"
    "    </Metadata>\n"
    "</Patch>\n",
    "<?xml version=\"1.0\"?>\n"
    "<Patch>\n"
    "    <TitleID>\n"
    "        <ID>CUSA01968</ID>\n"
    "        <ID>CUSA01990</ID>\n"
    "        <ID>CUSA01936</ID>\n"
    "    </TitleID>\n"
    "    <Metadata Title=\"UFC 2\" Name=\"Camera: 3rd person while standing (Select toggles)\" Author=\"Claude\" PatchVer=\"1.1\" AppVer=\"01.13\" AppElf=\"eboot.bin\" Note=\"Press Select / View / touchpad during a fight to switch the stand-up camera between normal and 3rd person behind Player 1, over the shoulder. Starts on normal. Ground and clinch cameras stay normal.\" isEnabled=\"%s\">\n"
    "        <PatchList>\n"
    "            <Line Type=\"mask\" Address=\"C5 F8 57 C0 C5 FA 2A C1 B9 0D 01 00 00 C4 E2 70 F7 C8\" Value=\"c5fa2ac189051d367f0189c1c1e90d83e101\"/>\n"
    "            <Line Type=\"mask\" Address=\"C5 F8 28 0D 38 78 50 00 C4 E3 79 04 D8 00 C4 C1 78 29 07 C5 F0 C2 E3 02 C5 E0 C2 1D 2F 78 50 00 02 C5 E0 54 DC C5 E0 C2 DA 00 C5 F8 50 C3 83 F8 0F 74 50 C4 E3 79 04 D8 55 C5 F0 C2 E3 02 C5 E0 C2 1D 09 78 50 00 02 C5 E0 54 DC C5 E0 C2 D2 00 C5 F8 50 C2 83 F8 0F 74 2A C4 E3 79 04 C0 AA C5 F0 C2 C8 02 C5 F8 C2 05 E3 77 50 00 02 C5 F8 54 C1 C5 F0 57 C9 C5 F8 C2 C1 00 C5 F8 50 C0 83 F8 0F 75 0A\" Value=\"8b05caadff00c1e81483e0018b0dc2adff008905bcadff00f7d121c1310db6adff00f605afadff0001745dc5f82845a0c5f85c55b0c4e36940e2ffc5f852e4c5e859d4c4e37904eaf9c5d37deac5e858f2c5c858f2c5c858f5c4e279180d7ef84100c5c859c9c5f858c1c4e371210d28b242001dc5f858c1eb0ecccccccccccccccccc\"/>\n"
    "        </PatchList>\n"
    "    </Metadata>\n"
    "</Patch>\n",
    "<?xml version=\"1.0\"?>\n"
    "<Patch>\n"
    "    <TitleID>\n"
    "        <ID>CUSA06534</ID>\n"
    "    </TitleID>\n"
    "    <Metadata Title=\"UFC 3\" Name=\"Camera: live broadcast (Select toggles)\" Author=\"PORTHEON\" PatchVer=\"4.0\" AppVer=\"01.14\" AppElf=\"eboot.bin\" Note=\"Classic camera style only: set Camera Style to Classic. In a stand-up fight, Select / View / touchpad switches between the normal camera and the live broadcast camera of UFC 6: still cameras high at the cage side, looking down on the fight and turning with it, with a cut to another camera every few seconds or when the fight comes too near or goes too far. Press again for the normal camera. Starts on normal at every launch. Ground and the Medium/Wide/Tight styles stay normal; the Classic clinch camera may switch too.\" isEnabled=\"%s\">\n"
    "        <PatchList>\n"
    "            <Line Type=\"mask\" Address=\"B9 0D 01 00 00 C4 E2 70 F7 C8\" Value=\"e8f7ba5bff9090909090\"/>\n"
    "            <Line Type=\"mask\" Address=\"C5 F8 10 15 0F B4 75 01\" Value=\"e822bc4200909090\"/>\n"
    "            <Line Type=\"mask\" Address=\"55 48 89 E5 41 56 53 89 F1 48 89 D3 8D 41 9C 3D\" Value=\"8b056a300202c1e81483e0018b0d6230020289055c300202f7d121c174298b0554300202ffc083e00189054930020231c0890549300202890567300202c70559300202286b6ece833d2a300202007509c5f8101598f73201c3c5f8285d90c5f82865a0c5d85cebc4e3510ce80ac4e35140f571b90ad7233cc5f96ef9c5f82ef772cec5ca51f6c5c8c6fe00c5d05eefc56058c4b90000003fc5796ec9c44130c6c900c4413859c1c4637904cd46b900000080c5796ed1c4413057cac5fa103ddd2f0202c4437904d855c4c1782efb730ac5797ed9890dc62f0202c4e279183dbd2f0202c463390cc702ff05952f0202833d8a2f020200745df7058a2f020201000000750e813d762f0202b60100007345eb0c813d682f0202360100007337c578101d6a2f0202c441205cd8c4432140db51b900002943c5796ee1c441782edc7214b90020d244c5796ee1c441782edc0f823201000031c9890d272f0202890d1d2f0202c441705cd0833d352f020200740dc5781015172f0202c441285cd0c4432940d151c5797ed181e10000008081c90000803f890de22e02028b0de82e020283e103890de32e0202ffc1890dd72e02028b05d52e020283f801742283f802743583f8037448b90000803fba0000000041b80000d84141b900003841eb46b99eef673fba7593d83e41b86666c24141b9cdcc2441eb2eb9c74b773fbaa69b84be41b80000d84141b900003841eb16b99eef673fba7593d8be41b86666c24141b9cdcc2441c5796ed1c52a59154c2e0202c44128c6d200c4412859d1c5796edac44120c6db00c52059ddc4412858d3b90000003fc5796ed9c52259dec441796ee0c4412258dcc44120c6db00c4412859d3c4413858c2c441796ee1c4437904e451c4413858c4eb08c5781005012e0202c44138c2e803c4c17850cd31d2f7c1070000000f94c28915d52d02028915f32d020285d20f8497fdffffc5781105cf2d0202c4e3390cc908e984fdffff89059e2d020289c1c1e90d83e101c3\"/>\n"
    "        </PatchList>\n"
    "    </Metadata>\n"
    "</Patch>\n",
    "<?xml version=\"1.0\"?>\n"
    "<Patch>\n"
    "    <TitleID>\n"
    "        <ID>CUSA14209</ID>\n"
    "    </TitleID>\n"
    "    <Metadata Title=\"UFC 4\" Name=\"Camera: live broadcast (Select toggles)\" Author=\"PORTHEON\" PatchVer=\"4.0\" AppVer=\"28.00\" AppElf=\"eboot.bin\" Note=\"Camera Style Medium, Wide or Tight only (Classic is not changed). In a stand-up fight, the touch pad (Select / View on an Xbox pad) switches between the normal camera and the live broadcast camera of UFC 6: still cameras high at the cage side, looking down on the fight and turning with it, with a cut to another camera every few seconds or when the fight comes too near or goes too far. Press again for the normal camera. Starts on normal at every launch. The ground and submission cameras are separate and stay normal.\" isEnabled=\"%s\">\n"
    "        <PatchList>\n"
    "            <Line Type=\"mask\" Address=\"55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 18 48 8B 05 80 F4 21 01\" Value=\"505152488d1536d281018b02c1e81483e0018b4a04894204f7d121c18b420801c883e00189420885c07517c7421000000000c7423400000000c74230286b6eceeb5fc5f8108b30030000c5f81093f0040000c5c057ffc5f05cdac4e3610cdf0ac4e36140e37fc5fa102d52bd9400c5f82ee5762dc5f851e4c5e05edcc4e37904eb02c5d0592dcee29300e9e1099000c4e3710c8d40fdffff08c5f8118d40fdffff5a5958488dbba0010000e9c3e1baff89058ad18101b90c010000e90c3602000000803f000000000000d841000038419eef673f7593d83e6666c241cdcc2441c74b773fa69b84be0000d841000038419eef673f7593d8be6666c241cdcc24410000003f0000003f0000003f0000003f0000003f0000003f0000003f0000003f\"/>\n"
    "            <Line Type=\"mask\" Address=\"55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 28 4C 8B 2D 10 EA 91 00\" Value=\"c5f058cac5f0590d84f66fffc4e37904d155c5fa107230c5f82ef27305c5fa115230c4e279187230c4e3710cce02ff4214837a100074478b421c83e00183f001c1e00705360100003942147331c5f8107220c5c85cf1c4e34940f651b800002943c5f96ef8c5f82ef77213b80020d244c5f96ef8c5f82ef70f82a300000031c0894214894210c5f810b540fdffff837a34007405c5f8107220c5c85cf1c4e34940f551c5f97ef025000000800d0000803f89420c8b421883e00389421cffc0894218ffc8c1e004488d0d82f56fffc5f8103401c4e279187a0cc5c059fec5c0c6ff00c5c059fdc5c8c6d655c5e859d3c5c058fac5d85925a5f56fffc5c8c6d6aac5d858e2c5c059fcc5f058cfc5c8c6d6ffc5c857f6c4e3490cd202c5f058caeb05c5f8104a20c5f0c2d103c5f850c231c9a9070000000f94c1894a10894a3485c90f84eaf46fffc5f8114a20e9cef46fff\"/>\n"
    "            <Line Type=\"mask\" Address=\"48 8D BB A0 01 00 00 48 8D B5 40 FD FF FF 4C 8D BB 50 02 00 00\" Value=\"e98f1d45009090\"/>\n"
    "            <Line Type=\"mask\" Address=\"B9 0C 01 00 00 C4 E2 79 31 C0 C4 E2 70 F7 C8\" Value=\"e9e4c9fdff\"/>\n"
    "        </PatchList>\n"
    "    </Metadata>\n"
    "</Patch>\n",
};
/* the switch of a port that had the mod in by hand starts as that file was (the first start of this version) */
static void Cam3pMigrate(int g)
{
    wchar_t key[32]; _snwprintf(key, 32, L"cam3p_%s", G[g].id);
    if ((int)PIni(key, -1) >= 0) return;
    wchar_t d[MAX_PATH], f[MAX_PATH], name[48]; PortDir(g, d); _snwprintf(name, 48, L"patches\\UFC%d-camera-3a-pessoa.xml", g + 1); Join(f, d, name);
    BOOL was = FALSE; HANDLE h = CreateFileW(f, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        char b[16384]; DWORD got = 0; ReadFile(h, b, sizeof(b) - 1, &got, NULL); CloseHandle(h); b[got] = 0;
        was = strstr(b, "isEnabled=\"true\"") != NULL;
    }
    PIniSet(key, was ? 1 : 0);
}
static void WriteCam3p(int g)
{
    if (!PS4G(g) || !CAM3P_XML[g]) return;
    Cam3pMigrate(g);
    char x[16384]; int n = snprintf(x, sizeof(x), CAM3P_XML[g], GetInt(g, L"pgame:cam3p", 0) && !(g == 1 && GetInt(g, L"pgame:camfp", 0)) ? "true" : "false");   /* 6.2: UFC 2's modes are a cheat pack; the old mod would override it */
    if (n <= 0 || n >= (int)sizeof(x)) return;
    if (g == 3 && GetInt(g, L"pgame:camfp", 0)) {   /* 6.2: not UFC 3: the game aims at the point between the fighters, and from the eyes that only shows the fighter's own cut body */   /* 6.2: the four-mode cycle with the fighter's-eyes view (Select: normal, 3rd person, TV, 1st person) */
        wchar_t pf[MAX_PATH], pdir[MAX_PATH]; PortDir(g, pdir); Join(pf, pdir, L"camera-modos.xml"); DWORD got = 0; char *t = ReadText(pf, &got);
        if (t && got > 0 && got < sizeof(x)) { memcpy(x, t, got); x[got] = 0; n = (int)got; char *e = strstr(x, "isEnabled=\""); if (e) { e += 11; if (!strncmp(e, "false", 5)) { memmove(e + 4, e + 5, strlen(e + 5) + 1); memcpy(e, "true", 4); } } }
        free(t);
    }
    wchar_t d[MAX_PATH], pd[MAX_PATH], f[MAX_PATH], name[40]; PortDir(g, d); Join(pd, d, L"patches"); CreateDirectoryW(pd, NULL);
    _snwprintf(name, 40, L"UFC%d-camera-3a-pessoa.xml", g + 1); Join(f, pd, name); WriteIfDifferent(f, x);
    wchar_t pat[MAX_PATH]; _snwprintf(pat, MAX_PATH, L"%s\\experiments\\*", pd); pat[MAX_PATH - 1] = 0;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do { if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.') {
             _snwprintf(f, MAX_PATH, L"%s\\experiments\\%s\\%s", pd, fd.cFileName, name); f[MAX_PATH - 1] = 0; WriteIfDifferent(f, x); } } while (FindNextFileW(h, &fd));
    FindClose(h);
}

/* Intel graphics (Arc, integrated Arc and older Iris/UHD), logs of 2026-10-18/20 from an integrated Arc on driver 101.9150:
   UFC 4 hung in the tiling pass of the PS4 Pro image (3200x1800, code 23), UFC 3 on a shader at a 144 FPS limit (code 23),
   UFC 2 crashed inside igvk64.dll (code 3: the Intel driver itself, only a driver update cures it). At every PLAY: no
   PS4 Pro mode, no experimental 2K/4K, at most 60 FPS, no low-latency queue; 4.1: Immediate (its driver has no
   Mailbox, and VSync is gone from PORTHEON; the 60 FPS limit stays; 4.3.4: VSync is a choice again and Intel keeps
   it). intel_guard=0 in portheon.ini turns it off.
   Returns TRUE when something was changed. */
static BOOL IntelGuard(int g)
{
    if (g < 0 || g > 3 || !GpuIsIntel() || !PIni(L"intel_guard", 1)) return FALSE;
    BOOL ch = FALSE;
    if (GetInt(g, L"neo", 0)) { PutInt(g, L"neo", 0); ch = TRUE; }
    if (GetInt(g, L"resolution", 1) > 1) { PutInt(g, L"resolution", 1); ch = TRUE; }
    if (GetInt(g, L"fps_limit", 60) > 60 || GetInt(g, L"fps_limit", 60) == 0) { PutInt(g, L"fps_limit", 60); ch = TRUE; }
    if (GetInt(g, L"low_latency", 0)) { PutInt(g, L"low_latency", 0); ch = TRUE; }
    if (GetInt(g, L"present_mode", 2) == 0) { PutInt(g, L"present_mode", 2); ch = TRUE; }   /* 4.3.4: VSync (1) may stay, it is back */
    return ch;
}

/* ---------------------------------------------------------------- the Intel Arc engine of UFC 3 and 4 (3.5)
   games\engines\intel-arc\sacredfire-game.exe: SacredFire rebuilt with 4 fixes for Intel Arc (package "PORTHEON 3.4.3 -
   correção do crash", 2026-10-22, SHA-256 49D036CC...): no 4-sample storage images where the driver hands out 1 sample
   (the code 23 of the Career), an image-copy assertion turned into a warning, and 9 PlayStation functions UFC 4 calls at
   start (code 20). It is a newer SacredFire than the ports' own engine, so it runs only where it is needed: Intel cards
   (automatic), or whoever picks it in Extras. Before a PLAY the port's bin\sacredfire-game.exe is made the wanted one;
   the port's own engine is kept in bin\std\ (refreshed whenever an update brings a new one). */
static BOOL SameFile(const wchar_t *a, const wchar_t *b)
{
    WIN32_FILE_ATTRIBUTE_DATA x, y;
    if (!GetFileAttributesExW(a, GetFileExInfoStandard, &x) || !GetFileAttributesExW(b, GetFileExInfoStandard, &y)) return FALSE;
    if (x.nFileSizeLow != y.nFileSizeLow || x.nFileSizeHigh != y.nFileSizeHigh) return FALSE;
    if (!CompareFileTime(&x.ftLastWriteTime, &y.ftLastWriteTime)) return TRUE;   /* 6.0 final: a CopyFileW copy (same size and time): no 2 x 14 MB compare at each PLAY */
    HANDLE f1 = CreateFileW(a, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    HANDLE f2 = CreateFileW(b, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    BOOL same = f1 != INVALID_HANDLE_VALUE && f2 != INVALID_HANDLE_VALUE;
    static BYTE b1[1 << 16], b2[1 << 16];
    while (same) {
        DWORD n1 = 0, n2 = 0;
        if (!ReadFile(f1, b1, sizeof(b1), &n1, NULL) || !ReadFile(f2, b2, sizeof(b2), &n2, NULL) || n1 != n2) { same = FALSE; break; }
        if (!n1) break;
        if (memcmp(b1, b2, n1)) same = FALSE;
    }
    if (f1 != INVALID_HANDLE_VALUE) CloseHandle(f1);
    if (f2 != INVALID_HANDLE_VALUE) CloseHandle(f2);
    return same;
}
static void ArcEnginePath(wchar_t *out) { _snwprintf(out, MAX_PATH, L"%s\\games\\engines\\intel-arc\\sacredfire-game.exe", g_exeDir); out[MAX_PATH - 1] = 0; }
/* 4.3.3: the engine of the community package "UFC2-UFC4-PC-Port-v3.0" (ufc2-v3.0, 2026-10-31; GPL-2.0, its source is
   games\engines\port-v30\source). A newer SacredFire (parallel shader warm-up, the driver's pipeline cache, deferred
   readbacks) that runs with the ports' own SDL3.dll and files. Picked in Extras ("Motor v3.0", pgame:engine = 3) in UFC 2,
   3 and 4. 6.2 (the user: "the whole engine, in 2 and 3"): the file is now that package's newest build, and it is what
   Automatic runs in UFC 2 and 3, offline too. UFC 3 on a processor without SSE4a (Intel) keeps the old automatic rules
   offline: this engine does not translate INSERTQ/EXTRQ. ufc3_no_v30: a Career crash sent UFC 3 back (Ufc3CareerCrash). */
static void V30EnginePath(wchar_t *out) { _snwprintf(out, MAX_PATH, L"%s\\games\\engines\\port-v30\\sacredfire-game.exe", g_exeDir); out[MAX_PATH - 1] = 0; }
static BOOL WantV30Engine(int g)
{
    wchar_t v[MAX_PATH]; V30EnginePath(v);
    if (g < 1 || g > 3 || !FileExists(v)) return FALSE;
    if (g <= 2 && OnlineMode(g)) return TRUE;   /* 4.3.6 UFC 2, 6.2 UFC 3: their online needs it */
    int m = GetInt(g, L"pgame:engine", 0);
    return m == 3 || (m == 0 && (g == 1 || (g == 2 && CpuHasSse4a() && !PIni(L"ufc3_no_v30", 0))));
}
/* Released (the user: "put Native Plus as the main one, not as an option"): the runtime of UFC4 Native Plus 1.2.0
   (github.com/abdullah-tech7/UFC4-Native-Plus, by Abdullah Tech, on schans-arg's UFC4-PC-Port and GNSB's UFC 3 renderer;
   GPL-2.0, its notices are in games\engines\native-plus). The same SacredFire family: it runs with the port's own files
   and command line. Automatic picks it for UFC 4, except where it lacks what another engine has: an Intel processor (no
   BB_SSE4A_PATCH, as the new engine), an Intel card (the new engine's Arc fixes) and Parsec mode (no
   BB_LOCAL_MULTIPLAYER). ufc4_no_np: it closed with an error and Automatic went back (NpEngineFallback);
   np_engine_auto=0 in portheon.ini keeps it a choice only. */
static void NpEnginePath(wchar_t *out) { _snwprintf(out, MAX_PATH, L"%s\\games\\engines\\native-plus\\sacredfire-game.exe", g_exeDir); out[MAX_PATH - 1] = 0; }
static BOOL WantNpEngine(int g)
{
    wchar_t v[MAX_PATH]; NpEnginePath(v);
    if (g != 3 || !FileExists(v)) return FALSE;
    int m = GetInt(g, L"pgame:engine", 0);
    if (m == 4) return TRUE;
    return m == 0 && PIni(L"np_engine_auto", 1) && !PIni(L"ufc4_no_np", 0) && CpuHasSse4a() && !GpuIsIntel() && !GetInt(g, L"parsec", 0);
}
/* a UFC 4 that closed with an error on the automatic Native Plus engine, in its first 90 s or with the card stopped
   (code 23, unless the log names the video memory or the disk): the next PLAY takes the engine Automatic ran before */
static BOOL LogHas(int g, const char *text);
static int GIni(const wchar_t *pre, int g, int def);
static void GIniSet(const wchar_t *pre, int g, int v);   /* 7.1: Gpu23Escalate */
static BOOL NpEngineFallback(int g, DWORD code, ULONGLONG playedMs)
{
    if (g != 3 || GIni(L"engine_ran", g, 0) != 3 || GetInt(g, L"pgame:engine", 0) != 0) return FALSE;
    if (code == 0 || code == 1 || code == 0xC000013Au) return FALSE;
    if (code == 23) { if (LogHas(g, "ErrorOutOfDeviceMemory") || LogHas(g, "No space left on device")) return FALSE; }
    else if (playedMs > 90000) return FALSE;
    PIniSet(L"ufc4_no_np", 1);
    return TRUE;
}
static BOOL WantArcEngine(int g)
{
    if (g != 2 && g != 3) return FALSE;
    wchar_t arc[MAX_PATH]; ArcEnginePath(arc); if (!FileExists(arc)) return FALSE;
    int m = GetInt(g, L"pgame:engine", 0);
    /* 5.2: an Intel processor (no SSE4a) on Automatic keeps the port's own engine, the only one that patches UFC 3/4's
       INSERTQ/EXTRQ; this rule comes first, so no crash rule below (Ufc3CareerCrash, Gpu23Escalate, ufcN_new_engine)
       can switch it back. With an Intel card the new engine stays (its Arc fixes), unless it already closed on one of
       those instructions (ufcN_old_engine, set by OnExited). Picked by hand (m == 2), it stays the player's choice. */
    if (m == 0 && !CpuHasSse4a() && (!GpuIsIntel() || PIni(g == 2 ? L"ufc3_old_engine" : L"ufc4_old_engine", 0))) return FALSE;
    /* 3.5.1: Ufc3CareerCrash; 3.5.3: Gpu23Escalate (UFC 3 or 4 after a GPU stop the shader skip could not cure) */
    /* 4.0: AMD cards too. Both code 23 logs of 2026-10-23 (RX 5700 and RX 5700 XT, AMD 26.10.1) stopped on fighter and
       arena draws with the port's own engine; the new one carries shadPS4's GPU hang fixes (#5358 unmapped index
       buffers, #5137 undefined sampler fields, #5231 submit waits, #5262 buffer barriers). amd_new_engine=0 goes back. */
    BOOL amd = g_haveGpu && g_gpu.vendor == 0x1002 && PIni(L"amd_new_engine", 1);
    /* 4.3.1: every card. UFC 4's 3D opening, RTX 4060 + Ryzen 5 5500, no FPS limit (2026-10-29): 156 FPS against the
       port's own engine's 141, fewer stutters; it also has the GPU-hang backports, UFC 3's Career fixes and the
       functions UFC 4 calls at start. NewEngineFallback puts a game that closes right at the start with it back on the
       port's own (ufc3_old_engine / ufc4_old_engine); new_engine_all=0 keeps NVIDIA cards on the port's own. */
    BOOL all = PIni(L"new_engine_all", 1) && !PIni(g == 2 ? L"ufc3_old_engine" : L"ufc4_old_engine", 0);
    return m == 2 || (m == 0 && (all || GpuIsIntel() || amd || PIni(g == 2 ? L"ufc3_new_engine" : L"ufc4_new_engine", 0)));
}
/* 4.3.1: a UFC 3 or 4 that closed with an error in its first 90 s on the new engine, picked automatically on a card
   where the port's own engine used to be the automatic one (not Intel, not AMD, not after a crash of the port's own),
   goes back to the port's own engine for the next PLAY. Returns TRUE when it switched. */
static BOOL NewEngineFallback(int g, DWORD code, ULONGLONG playedMs)
{
    /* a fault, a missing PS4 function or an access violation; a GPU stop (23) has its own steps, where this engine is one */
    if (WantV30Engine(g) || WantNpEngine(g)) return FALSE;   /* 6.2: the new engine did not run */
    if ((g != 2 && g != 3) || (code != 3 && code != 20 && code != 3221225477u) || playedMs > 90000) return FALSE;
    if (GetInt(g, L"pgame:engine", 0) != 0 || !WantArcEngine(g)) return FALSE;
    if (GpuIsIntel() || (g_haveGpu && g_gpu.vendor == 0x1002 && PIni(L"amd_new_engine", 1))) return FALSE;
    if (PIni(g == 2 ? L"ufc3_new_engine" : L"ufc4_new_engine", 0)) return FALSE;
    PIniSet(g == 2 ? L"ufc3_old_engine" : L"ufc4_old_engine", 1);
    return TRUE;
}
/* 4.0.3: the engine file held by a sacredfire-game.exe still running from a crash, or by an antivirus scan ("Could not
   switch the video engine of UFC 4 (the game or an antivirus holds the file)", a player's photo of 2026-10-24): the
   leftover engine of this very port is closed and the copy tried again for about 3 s */
static void KillEngineAt(const wchar_t *exe)
{
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32W pe = {sizeof(pe)};
    if (sn == INVALID_HANDLE_VALUE) return;
    if (Process32FirstW(sn, &pe)) do {
        if (_wcsicmp(pe.szExeFile, L"sacredfire-game.exe")) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID); if (!h) continue;
        wchar_t img[MAX_PATH]; DWORD n = MAX_PATH;
        if (QueryFullProcessImageNameW(h, 0, img, &n) && !_wcsicmp(img, exe)) { TerminateProcess(h, 1); WaitForSingleObject(h, 3000); }
        CloseHandle(h);
    } while (Process32NextW(sn, &pe));
    CloseHandle(sn);
}
static BOOL CopyEngine(const wchar_t *src, const wchar_t *dst)
{
    for (int i = 0; i < 8; i++) {
        SetFileAttributesW(dst, FILE_ATTRIBUTE_NORMAL);
        if (CopyFileW(src, dst, FALSE)) return TRUE;
        if (i == 0 && !g_proc) KillEngineAt(dst);
        Sleep(400);
    }
    return FALSE;
}
/* 4.3.3: MFG SUPER MOTION's install, found by its Inno Setup uninstall entry (per user or for all users) */
static BOOL MfgExe(wchar_t *out)
{
    static const wchar_t *const keys[] = {L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{6B2C3E1A-8D47-4F7B-9C55-2A1E7D90B3F4}_is1",
                                          L"Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{6B2C3E1A-8D47-4F7B-9C55-2A1E7D90B3F4}_is1"};
    for (int r = 0; r < 2; r++) for (int k = 0; k < 2; k++) {
        wchar_t dir[MAX_PATH]; DWORD n = sizeof(dir);
        if (RegGetValueW(r ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, keys[k], L"InstallLocation", RRF_RT_REG_SZ, NULL, dir, &n) != ERROR_SUCCESS || !dir[0]) continue;
        size_t l = wcslen(dir); if (dir[l - 1] == L'\\') dir[l - 1] = 0;
        _snwprintf(out, MAX_PATH, L"%s\\Mfg Super Motion.exe", dir); out[MAX_PATH - 1] = 0;
        if (FileExists(out)) return TRUE;
    }
    return FALSE;
}
static void MfgStart(int g)   /* PLAY: the player's MFG SUPER MOTION beside the game, unless it already runs */
{
    wchar_t exe[MAX_PATH];
    if (!GetInt(g, L"pgame:mfg", 0) || !MfgExe(exe)) return;
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32W pe = {sizeof(pe)}; BOOL running = FALSE;
    if (sn != INVALID_HANDLE_VALUE) { if (Process32FirstW(sn, &pe)) do running = !_wcsicmp(pe.szExeFile, L"Mfg Super Motion.exe"); while (!running && Process32NextW(sn, &pe)); CloseHandle(sn); }
    if (running) return;
    wchar_t dir[MAX_PATH]; wcscpy(dir, exe); wchar_t *s = wcsrchr(dir, L'\\'); if (s) *s = 0;
    ShellExecuteW(NULL, L"open", exe, NULL, dir, SW_SHOWMINNOACTIVE);   /* minimized: the game keeps the focus */
}
static int EnsureEngine(int g)   /* 1 = the Arc engine runs, 2 = the v3.0 engine (4.3.3), 3 = Native Plus (Released), 0 = the port's own, -1 = could not switch */
{
    if (g < 1 || g > 3) return 0;
    wchar_t d[MAX_PATH], cur[MAX_PATH], stdd[MAX_PATH], std[MAX_PATH], arc[MAX_PATH], v30[MAX_PATH], np[MAX_PATH];
    PortDir(g, d); _snwprintf(cur, MAX_PATH, L"%s\\bin\\sacredfire-game.exe", d); _snwprintf(stdd, MAX_PATH, L"%s\\bin\\std", d);
    Join(std, stdd, L"sacredfire-game.exe"); ArcEnginePath(arc); V30EnginePath(v30); NpEnginePath(np);
    if (!FileExists(cur)) return -1;
    BOOL haveArc = FileExists(arc), curIsArc = haveArc && SameFile(cur, arc), want = g != 1 && WantArcEngine(g);
    BOOL curIsV30 = FileExists(v30) && SameFile(cur, v30), wantV30 = WantV30Engine(g);
    BOOL curIsNp = g == 3 && FileExists(np) && SameFile(cur, np), wantNp = WantNpEngine(g);
    if (!curIsArc && !curIsV30 && !curIsNp && !SameFile(cur, std)) { CreateDirectoryW(stdd, NULL); CopyFileW(cur, std, FALSE); }
    if (wantNp) return curIsNp ? 3 : CopyEngine(np, cur) ? 3 : -1;
    if (wantV30) return curIsV30 ? 2 : CopyEngine(v30, cur) ? 2 : -1;
    if (curIsV30 || curIsNp) {   /* back from the v3.0 or the Native Plus engine: the new one, or the port's own kept in bin\std */
        if (want) return CopyEngine(arc, cur) ? 1 : -1;
        return FileExists(std) ? (CopyEngine(std, cur) ? 0 : -1) : -1;
    }
    if (g == 1) return 0;
    if (want && !curIsArc) return CopyEngine(arc, cur) ? 1 : -1;
    if (!want && curIsArc) {
        if (FileExists(std)) return CopyEngine(std, cur) ? 0 : -1;
        /* the Arc package's own INSTALAR.bat kept the port's engine as sacredfire-game.exe.ORIGINAL.bak */
        wchar_t bak[MAX_PATH]; _snwprintf(bak, MAX_PATH, L"%s.ORIGINAL.bak", cur); bak[MAX_PATH - 1] = 0;
        if (FileExists(bak) && !SameFile(bak, arc)) { CreateDirectoryW(stdd, NULL); CopyFileW(bak, std, FALSE); return CopyEngine(bak, cur) ? 0 : -1; }
        return 1;   /* only the Arc engine is here: it runs */
    }
    return curIsArc;
}

/* GPU hangs (code 23) in UFC 1-4 (3.3): the engine's breadcrumbs name the pixel shader the GPU got stuck in
   ("#12736169 STUCK: indexed draw vs ... ps 00000000f74dbf6a", UFC 4 on an RX 5700 in a walkout, log of 2026-10-21
   23:05). PORTHEON writes that shader into %LOCALAPPDATA%\UFCnNative\portheon-skip-shaders-v2.txt and every PLAY hands
   the file to the engine (BB_SKIP_FILE, hex hashes; the engine skips those draws), so the same scene does not stop the
   GPU again. skip_learn=0 in portheon.ini turns it off; deleting the file brings the shaders back. */
static void SkipFilePath(int g, wchar_t *out) { wchar_t d[MAX_PATH]; LocalData(d, G[g].data); Join(out, d, L"portheon-skip-shaders-v2.txt"); }
/* the last bytes (at most max) of the newest log of UFC 1-4, malloc'd and 0-terminated; NULL when there is none */
static char *ReadLogTail(int g, DWORD max)
{
    if (!G[g].data) return NULL;   /* 4.0.2: UFC 5 keeps its logs in games\UFC5\bin\_Logs */
    if (!PS4G(g) || !G[g].data) return NULL;
    wchar_t d[MAX_PATH], pat[MAX_PATH], best[MAX_PATH] = L""; FILETIME bt = {0, 0};
    LocalData(d, G[g].data); Join(d, d, L"logs"); Join(pat, d, L"*.log");
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    do { if (CompareFileTime(&fd.ftLastWriteTime, &bt) > 0) { bt = fd.ftLastWriteTime; Join(best, d, fd.cFileName); } } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!best[0]) return NULL;
    HANDLE f = CreateFileW(best, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(f, NULL), want = sz > max ? max : sz, got = 0;
    if (sz > want) SetFilePointer(f, (LONG)(sz - want), NULL, FILE_BEGIN);
    char *b = (char *)malloc(want + 1); if (!b) { CloseHandle(f); return NULL; }
    ReadFile(f, b, want, &got, NULL); CloseHandle(f); b[got] = 0;
    return b;
}
static BOOL LogHas(int g, const char *text)   /* 4.0.3: the newest log's last 2 MB contain text */
{
    char *b = ReadLogTail(g, 2u << 20); if (!b) return FALSE;
    BOOL found = strstr(b, text) != NULL; free(b); return found;
}
/* 6.1: Windows would not give the engine the game's memory. The port's own engines say "cannot create N MiB of guest
   memory (error 1450)"; the v3.0 engine commits as the game maps it and words the failure differently ("cannot commit N
   MiB of guest memory (section offset ...", then "STOP: out of memory (...", exit code 25) */
static BOOL LogTextNoCommit(const char *t)
{
    return strstr(t, "of guest memory (error") || strstr(t, "of guest memory (section offset") || strstr(t, "STOP: out of memory (");
}
static BOOL LogNoCommit(int g)
{
    char *t = ReadLogTail(g, 2u << 20); if (!t) return FALSE;
    BOOL r = LogTextNoCommit(t); free(t); return r;
}
/* 6.0: a 16-hex shader hash in the port's skip file (BB_SKIP_FILE), added once */
static void SkipFileEnsure(int g, const char *hash)
{
    wchar_t sf[MAX_PATH]; SkipFilePath(g, sf); char t[8192] = ""; DWORD n = 0;
    HANDLE r = CreateFileW(sf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (r != INVALID_HANDLE_VALUE) { ReadFile(r, t, sizeof(t) - 1, &n, NULL); CloseHandle(r); t[n] = 0; }
    if (strstr(t, hash)) return;
    wchar_t dd[MAX_PATH]; wcscpy(dd, sf); wchar_t *bs = wcsrchr(dd, L'\\'); if (bs) { *bs = 0; CreateDirectoryW(dd, NULL); }
    HANDLE w = CreateFileW(sf, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (w != INVALID_HANDLE_VALUE) { char line[32]; _snprintf(line, 32, "%s\r\n", hash); DWORD wr; WriteFile(w, line, (DWORD)strlen(line), &wr, NULL); CloseHandle(w); }
}
/* 7.1 (U2-HANG F2, the ufc1-4 Check's strict rule): a frozen game the player closed. The engine ended with code 0
   ("Window closed") but the GPU never came back: after the LAST "GPU wait stuck" there is a "Wait over 5 s (" block,
   no "Wait over 5 s ended", no "Growth (60 s)" and no "Frame stats:" line (the game ran on after a stall), and then
   "Window closed". Such an end counts as code 23 (the crash streak, the stuck-shader rule and Gpu23Escalate see it);
   5.2-7.0 took it as a clean session of 5+ minutes and reset the streak. t: the log's end, 0-terminated. */
static BOOL FrozenCloseText(const char *t)
{
    const char *last = NULL; for (const char *m = t; (m = strstr(m, "GPU wait stuck")) != NULL; m++) last = m;
    if (!last) return FALSE;
    const char *w = strstr(last, "Wait over 5 s ("); if (!w) return FALSE;
    if (strstr(last, "Wait over 5 s ended") || strstr(last, "Growth (60 s)") || strstr(last, "Frame stats:")) return FALSE;
    return strstr(w, "Window closed") != NULL;
}
static BOOL LogFrozenClosed(int g)
{
    if (!PS4G(g)) return FALSE;
    char *t = ReadLogTail(g, 2u << 20); if (!t) return FALSE;
    BOOL r = FrozenCloseText(t); free(t); return r;
}
/* 7.1: whether a stuck pixel shader (cur, its draw's index count idx) is switched off, given the one that stopped the
   session before (prev, "" = none since the last clean session): a big draw (3000 indices or more: a fighter's body,
   the arena) only when it is the same twice; a small one at once, unless another shader stopped last time (then the
   stops move around: a random GPU hang, not one effect) */
static BOOL StuckLearnDecide(const wchar_t *prev, const wchar_t *cur, long idx)
{
    if (!cur[0] || idx <= 0) return FALSE;
    if (idx >= 3000) return !wcscmp(prev, cur);
    return !prev[0] || !wcscmp(prev, cur);
}
/* 7.1 review (correctness#5, player#5): *why says why nothing was learned, for the code 23 text: 0 no stuck draw named
   (no STUCK line, no index count, learning off), 1 a big draw stopped for the first time, 2 a small draw while another
   shader stopped last time (the stops move around), 3 the shader is already in the skip file */
static BOOL LearnStuckShader(int g, wchar_t *hashOut, int *why)
{
    *why = 0;
    if (!PS4G(g) || !G[g].data || !PIni(L"skip_learn", 1)) return FALSE;
    char *b = ReadLogTail(g, 2u << 20);   /* the end of the log holds the breadcrumbs */
    if (!b) return FALSE;
    char hash[17] = ""; const char *m = b, *last = NULL; long idx = 0;
    while ((m = strstr(m, "STUCK:")) != NULL) { last = m; m += 6; }
    if (last) { const char *ps = strstr(last, " ps "), *eol = strchr(last, '\n');
        if (ps && (!eol || ps < eol)) { const char *ic = strstr(ps, ", "); if (ic && (!eol || ic < eol)) idx = strtol(ic + 2, NULL, 10);
            ps += 4; int n = 0; while (n < 16 && isxdigit((unsigned char)ps[n])) n++; if (n == 16) { memcpy(hash, ps, 16); hash[16] = 0; } } }
    free(b);
    /* 3.4.1: only small draws are switched off (an effect, a hint, a UI piece). A big one is a fighter's body (UFC 3 on an
       Arc B580 got stuck on a 5,166-index draw, log of 2026-10-22 01:09): skipping it would leave the fighter invisible. */
    /* 6.0 (the UFC 4 RX 5000 players: the same 6744-index draw stuck session after session, with nothing left to change):
       a big draw stuck on the same pixel shader as the session before is switched off after all - a missing layer of one
       mesh beats a game that stops every time (stuck_ps_<id> in portheon.ini keeps the last one) */
    /* 7.1 (U2-HANG, a GTX 1650 Ti laptop: three fight stops on three different shaders, the third a 2208-index draw that
       was switched off at once although the stops were random): stuck_ps_<id> keeps the last stuck shader of every size,
       and a small draw is learned at once only when no other shader stopped last (StuckLearnDecide). StreakAfter clears
       the key after a clean session of 5 minutes or more, so an old stop does not block learning for good. */
    if (hash[0] && idx) {
        wchar_t k[48], prev[32] = L"", hw[32]; _snwprintf(k, 48, L"stuck_ps_%s", G[g].id);
        GetPrivateProfileStringW(L"PORTHEON", k, L"", prev, 32, g_pIni); MultiByteToWideChar(CP_UTF8, 0, hash, -1, hw, 32);
        WritePrivateProfileStringW(L"PORTHEON", k, hw, g_pIni);
        if (!StuckLearnDecide(prev, hw, idx)) { *why = idx >= 3000 ? 1 : 2; hash[0] = 0; }
    }
    if (!idx) hash[0] = 0;
    if (!hash[0]) return FALSE;
    wchar_t sf[MAX_PATH]; SkipFilePath(g, sf);
    {   /* already in the file? */
        HANDLE r = CreateFileW(sf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (r != INVALID_HANDLE_VALUE) { char t[8192]; DWORD n = 0; ReadFile(r, t, sizeof(t) - 1, &n, NULL); CloseHandle(r); t[n] = 0;
            if (strstr(t, hash)) { MultiByteToWideChar(CP_UTF8, 0, hash, -1, hashOut, 32); *why = 3; return FALSE; } }
    }
    HANDLE w = CreateFileW(sf, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (w == INVALID_HANDLE_VALUE) return FALSE;
    char line[32]; _snprintf(line, 32, "%s\r\n", hash); DWORD wr; WriteFile(w, line, (DWORD)strlen(line), &wr, NULL); CloseHandle(w);
    MultiByteToWideChar(CP_UTF8, 0, hash, -1, hashOut, 32);
    return TRUE;
}
/* 3.5.1, UFC 3 Career crashes (a player's Career closed at its last fight, again and again, whatever the settings): the
   port's own UFC 3 engine is GNSB's 0.1.6, older than the Career fixes GNSB made after it - 0.1.7's "UI back-pressure"
   (UFC-57: the crash in the game's "UI Update" thread after title fights and the retirement fight, GNSB issues #16, #25
   and #34) and the appearance back-pressure of Create Fighter (UFC-58). The Arc engine is the newest SacredFire and
   has both ("UI back-pressure (UFC-57)" in its log). A code 3 of UFC 3 with a guest fault in its log, on the port's
   own engine with "Video engine" on Automatic, makes the next PLAY use it (ufc3_new_engine=1 in portheon.ini).
   Returns 1 when it switched, 2 when the fault was in "UI Update" but the engine was chosen by hand, 0 otherwise. */
static int Ufc3CareerCrash(void)
{
    char *t = ReadLogTail(2, 1u << 20); if (!t) return 0;
    const char *f = strstr(t, "Guest fault:"); BOOL ui = FALSE;
    if (f) { const char *e = strchr(f, '\n'), *u = strstr(f, "UI Update"); ui = u && (!e || u < e); }   /* "(thread 7728 UI Update)" */
    free(t);
    if (!f) return 0;
    /* 6.2: Automatic runs the v3.0 engine, which has no proof of GNSB's Career fixes: the crash of "UI Update" goes to the new one */
    if (ui && !OnlineMode(2) && WantV30Engine(2) && GetInt(2, L"pgame:engine", 0) == 0) { PIniSet(L"ufc3_no_v30", 1); return 1; }
    if (WantV30Engine(2) || WantArcEngine(2)) return 0;
    if (!CpuHasSse4a()) return 0;   /* 5.2: an Intel processor stays on the port's own engine (WantArcEngine's first rule) */
    wchar_t arc[MAX_PATH]; ArcEnginePath(arc); if (!FileExists(arc)) return 0;
    if (GetInt(2, L"pgame:engine", 0) != 0) return ui ? 2 : 0;
    PIniSet(L"ufc3_new_engine", 1); return 1;
}
/* 3.5.3: where a code 3 happened, from the engine's fatal report ("Guest fault: read access to 0000000000000018 at guest
   offset 0x137cde4 (thread 7728 SimThread)"): " (guest offset 0x137cde4, thread SimThread)", or "" without one */
static void GuestFault(int g, wchar_t *out, int n)
{
    out[0] = 0;
    char *t = ReadLogTail(g, 1u << 20); if (!t) return;
    const char *f = NULL, *m = t;
    while ((m = strstr(m, " fault: ")) != NULL) { const char *a = strstr(m, " access to "), *e = strchr(m, '\n'); if (a && (!e || a < e)) f = m; m += 8; }
    if (f) {
        const char *e = strchr(f, '\n'), *at = strstr(f, " at "), *th = strstr(f, "(thread ");
        if (at && (!e || at < e)) {
            char where[96] = "", who[64] = "";
            const char *a0 = at + 4, *a1 = th && (!e || th < e) ? th : (e ? e : a0 + strlen(a0));
            size_t l = (size_t)(a1 - a0); if (l > 95) l = 95; memcpy(where, a0, l); where[l] = 0;
            while (l && (where[l - 1] == ' ' || where[l - 1] == '\r')) where[--l] = 0;
            if (th && (!e || th < e)) {   /* "(thread 7728 SimThread)": the name after the number */
                const char *p = th + 8; while (*p >= '0' && *p <= '9') p++; while (*p == ' ') p++;
                size_t k = 0; while (p[k] && p[k] != ')' && p[k] != '\n' && k < 63) { who[k] = p[k]; k++; } who[k] = 0;
            }
            wchar_t ww[96], wt[64]; MultiByteToWideChar(CP_UTF8, 0, where, -1, ww, 96); MultiByteToWideChar(CP_UTF8, 0, who, -1, wt, 64);
            if (wt[0]) _snwprintf(out, n, L" (%s, thread %s)", ww, wt); else _snwprintf(out, n, L" (%s)", ww);
            out[n - 1] = 0;
        }
    }
    free(t);
}
/* 3.5.3: a GPU stop (code 23) the shader skip could not cure (a big draw, a fighter or the arena: the RX 5700 log of
   2026-10-23, 26,190 indices with 4x MSAA, AMD driver 26.10.1). The next PLAY of UFC 3 or 4 first takes the newest
   SacredFire ("Motor novo": its shadPS4 backports bind no indices for an unmapped index buffer, #5358, and clamp
   undefined sampler fields, #5137, two GPU-hang causes); a stop again there, or in UFC 2, turns on the safe video
   mode (GpuSafeEnv). Returns 1 new engine, 2 safe mode, 0 nothing left to change. */
static BOOL GpuSafe(int g);
/* 4.1: a Radeon RX 5000 (RDNA1: 5300 to 5700 XT, four digits after "RX 5"; an RX 580 has three) */
static BOOL GpuIsRdna1(void)
{
    if (!g_haveGpu || g_gpu.vendor != 0x1002) return FALSE;
    const wchar_t *p = wcsstr(g_gpu.name, L"RX 5");
    return p && iswdigit(p[4]) && iswdigit(p[5]) && iswdigit(p[6]) && !iswdigit(p[7]);
}
static int Gpu23Escalate(int g)
{
    /* 4.1: UFC 4 on RX 5700 XT, 5700 and 5500 XT (AMD driver 26.10.1, logs of 2026-10-26/27): two of the three stops were
       on the hair draws whose DB_ALPHA_TO_MASK the engine emulates with a shader dither on 4x MSAA targets (ps 58cfcfd8).
       On these cards the first step is the card's own alpha-to-coverage there (BB_ALPHA_TO_MASK=coverage; the engine's
       notes saw a darker torso with it in UFC 3's Create Fighter, so only after a stop). Returns 3. */
    if (g >= 1 && g <= 3 && GpuIsRdna1() && !GetInt(g, L"pgame:a2c", 0)) { PutInt(g, L"pgame:a2c", 1); return 3; }
    if (g == 2 || g == 3) {
        wchar_t arc[MAX_PATH]; ArcEnginePath(arc);
        /* 5.2: not on an Intel processor (no SSE4a): the new engine cannot run UFC 3/4 there (WantArcEngine) */
        if (FileExists(arc) && GetInt(g, L"pgame:engine", 0) == 0 && !WantArcEngine(g) && CpuHasSse4a()) { PIniSet(g == 2 ? L"ufc3_new_engine" : L"ufc4_new_engine", 1); return 1; }
    }
    if (g != 0 && !GpuSafe(g)) { PutInt(g, L"pgame:gpu_safe", 1); return 2; }
    /* 7.1 (U2-HANG F3): nothing left to change by itself (UFC 2 with the safe video mode already on): the next PLAY offers
       the compatible mode at once (SafePrompt at 2 crashes in a row) instead of after another stop. UFC 2 to 4 only:
       UFC 1 has no step above, and its first stop would ask already. 7.1 review (correctness#4, player#4): through its
       own key safe_ask_<id>, so crash_streak stays the real count (SafePrompt says it as a fact) */
    if (g >= 1 && PS4G(g) && !SafeOn(g)) { GIniSet(L"safe_ask", g, 1); return 4; }
    return 0;
}

/* ---------------------------------------------------------------- UFC 1-4: freeze catcher
   The SacredFire engine prints "GPU wait stuck" when its GPU command thread has waited more than 2 s for the graphics
   card. The catcher then saves the call stack of every thread of sacredfire-game.exe beside the log
   (<log>.stacks.txt): which thread waits for which, the information a freeze needs to be fixed. */
typedef struct { uint64_t base, size; wchar_t name[64]; } ModInfo;
typedef HRESULT (WINAPI *GetThreadDescriptionFn)(HANDLE, PWSTR *);
static volatile LONG g_watchStop, g_watchSession;
static wchar_t g_frozenFile[MAX_PATH];

static void Out(FILE *f, const wchar_t *fmt, ...)
{
    wchar_t w[1200]; va_list ap; va_start(ap, fmt); _vsnwprintf(w, 1200, fmt, ap); va_end(ap); w[1199] = 0;
    char u[3600]; int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), NULL, NULL); if (n > 1) fwrite(u, 1, (size_t)n - 1, f);
}
static int ModCmp(const void *a, const void *b) { uint64_t x = ((const ModInfo *)a)->base, y = ((const ModInfo *)b)->base; return x < y ? -1 : x > y; }
static int ModAt(const ModInfo *m, int n, uint64_t a)
{
    int lo = 0, hi = n - 1, found = -1;
    while (lo <= hi) { int mid = (lo + hi) / 2; if (m[mid].base <= a) { found = mid; lo = mid + 1; } else hi = mid - 1; }
    return found >= 0 && a < m[found].base + m[found].size ? found : -1;
}
static void Where(wchar_t *out, int cap, uint64_t a, const ModInfo *m, int n)
{
    int i = ModAt(m, n, a);
    if (i >= 0) _snwprintf(out, cap, L"%s+0x%llx", m[i].name, (unsigned long long)(a - m[i].base));
    else _snwprintf(out, cap, L"0x%llx", (unsigned long long)a);
    out[cap - 1] = 0;
}
/* the 8 bytes before a value found on a stack end with a call instruction: it is a return address */
static BOOL CallBefore(const unsigned char *b)
{
#define BB(k) b[8 - (k)]
    if (BB(5) == 0xE8) return TRUE;                                                   /* call rel32 */
    if (BB(6) == 0xFF && BB(5) == 0x15) return TRUE;                                  /* call [rip+disp32] */
    if (BB(2) == 0xFF && (BB(1) & 0xF8) == 0xD0) return TRUE;                         /* call reg */
    if ((BB(3) & 0xF0) == 0x40 && BB(2) == 0xFF && (BB(1) & 0xF8) == 0xD0) return TRUE; /* call r8-r15 */
    if (BB(2) == 0xFF && (BB(1) & 0xF8) == 0x10 && (BB(1) & 7) != 4 && (BB(1) & 7) != 5) return TRUE;   /* call [reg] */
    if (BB(3) == 0xFF && (BB(2) & 0xF8) == 0x50) return TRUE;                         /* call [reg+disp8] */
    if (BB(4) == 0xFF && BB(3) == 0x54) return TRUE;                                  /* call [reg+index+disp8] */
    if (BB(6) == 0xFF && (BB(5) & 0xF8) == 0x90) return TRUE;                         /* call [reg+disp32] */
    if ((BB(4) & 0xF0) == 0x40 && BB(3) == 0xFF && (BB(2) & 0xF8) == 0x50) return TRUE;
    if ((BB(7) & 0xF0) == 0x40 && BB(6) == 0xFF && (BB(5) & 0xF8) == 0x90) return TRUE;
    if ((BB(3) & 0xF0) == 0x40 && BB(2) == 0xFF && (BB(1) & 0xF8) == 0x10) return TRUE;
    return FALSE;
#undef BB
}
static DWORD FindProcess(const wchar_t *exe)
{
    DWORD pid = 0; HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32W pe = {sizeof(pe)};
    if (sn == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(sn, &pe)) do { if (!_wcsicmp(pe.szExeFile, exe)) pid = pe.th32ProcessID; } while (Process32NextW(sn, &pe));
    CloseHandle(sn); return pid;
}
static BOOL SaveStacks(DWORD pid, const wchar_t *path, const char *why)
{
    HANDLE hp = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hp) return FALSE;
    static ModInfo mods[1024]; int nm = 0;
    HANDLE ms = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (ms != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me = {sizeof(me)};
        if (Module32FirstW(ms, &me)) do {
            if (nm < 1024) { mods[nm].base = (uint64_t)(uintptr_t)me.modBaseAddr; mods[nm].size = me.modBaseSize; wcsncpy(mods[nm].name, me.szModule, 63); mods[nm].name[63] = 0; nm++; }
        } while (Module32NextW(ms, &me));
        CloseHandle(ms);
    }
    qsort(mods, nm, sizeof(ModInfo), ModCmp);
    FILE *f = _wfopen(path, L"wb"); if (!f) { CloseHandle(hp); return FALSE; }
    SYSTEMTIME t; GetLocalTime(&t);
    Out(f, L"PORTHEON %s freeze catcher, %04d-%02d-%02d %02d:%02d:%02d: the threads of sacredfire-game.exe (pid %lu)\r\n", APP_VER, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, pid);
    { wchar_t w[300]; MultiByteToWideChar(CP_UTF8, 0, why, -1, w, 300); w[299] = 0; Out(f, L"Trigger: %s\r\n\r\nModules:\r\n", w); }
    for (int i = 0; i < nm; i++) Out(f, L"  %016llx %08llx %s\r\n", (unsigned long long)mods[i].base, (unsigned long long)mods[i].size, mods[i].name);
    GetThreadDescriptionFn gtd = (GetThreadDescriptionFn)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription");
    static unsigned char stack[128 * 1024];
    HANDLE ts = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); THREADENTRY32 te = {sizeof(te)}; int nt = 0;
    if (ts != INVALID_HANDLE_VALUE && Thread32First(ts, &te)) do {
        if (te.th32OwnerProcessID != pid) continue;
        nt++;
        HANDLE ht = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
        if (!ht) { Out(f, L"\r\nthread %lu: cannot open it (error %lu)\r\n", te.th32ThreadID, GetLastError()); continue; }
        wchar_t tname[128] = L"";
        if (gtd) { PWSTR d = NULL; if (SUCCEEDED(gtd(ht, &d)) && d) { wcsncpy(tname, d, 127); tname[127] = 0; LocalFree(d); } }
        CONTEXT ctx; memset(&ctx, 0, sizeof(ctx)); ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        SIZE_T have = 0; BOOL got = FALSE;
        if (SuspendThread(ht) != (DWORD)-1) {
            got = GetThreadContext(ht, &ctx);
            if (got) while (have < sizeof(stack)) {   /* the stack from rsp up, page by page, until an unreadable one */
                uint64_t at = ctx.Rsp + have; SIZE_T want = 4096 - (SIZE_T)(at & 4095), n = 0;
                if (want > sizeof(stack) - have) want = sizeof(stack) - have;
                if (!ReadProcessMemory(hp, (LPCVOID)(uintptr_t)at, stack + have, want, &n) || !n) break;
                have += n;
            }
            ResumeThread(ht);
        }
        wchar_t w1[200];
        Out(f, L"\r\nthread %lu \"%s\" base priority %ld\r\n", te.th32ThreadID, tname, te.tpBasePri);
        if (!got) { Out(f, L"  (no context)\r\n"); CloseHandle(ht); continue; }
        Where(w1, 200, ctx.Rip, mods, nm);
        Out(f, L"  rip %s   rsp %llx rbp %llx rax %llx rcx %llx rdx %llx r8 %llx\r\n", w1, (unsigned long long)ctx.Rsp, (unsigned long long)ctx.Rbp,
            (unsigned long long)ctx.Rax, (unsigned long long)ctx.Rcx, (unsigned long long)ctx.Rdx, (unsigned long long)ctx.R8);
        uint64_t bp = ctx.Rbp;   /* frame pointer chain (the engine keeps frame pointers) */
        for (int d = 0; d < 40 && bp; d++) {
            uint64_t fr[2]; SIZE_T n = 0;
            if (!ReadProcessMemory(hp, (LPCVOID)(uintptr_t)bp, fr, sizeof(fr), &n) || n != sizeof(fr) || fr[0] <= bp) break;
            Where(w1, 200, fr[1], mods, nm); Out(f, L"  fp#%-2d %s\r\n", d, w1); bp = fr[0];
        }
        int shown = 0;           /* return addresses found on the stack */
        for (SIZE_T o = 0; o + 8 <= have && shown < 72; o += 8) {
            uint64_t v; memcpy(&v, stack + o, 8);
            int mi = ModAt(mods, nm, v); if (mi < 0 || v < mods[mi].base + 8) continue;
            unsigned char code[8]; SIZE_T n = 0;
            if (!ReadProcessMemory(hp, (LPCVOID)(uintptr_t)(v - 8), code, 8, &n) || n != 8 || !CallBefore(code)) continue;
            Out(f, L"  [rsp+%05llx] %s+0x%llx\r\n", (unsigned long long)o, mods[mi].name, (unsigned long long)(v - mods[mi].base)); shown++;
        }
        CloseHandle(ht);
    } while (Thread32Next(ts, &te));
    if (ts != INVALID_HANDLE_VALUE) CloseHandle(ts);
    Out(f, L"\r\n%d threads\r\n", nt);
    fclose(f); CloseHandle(hp); return nt > 0;
}
static void GIniSet(const wchar_t *pre, int g, int v);
static DWORD_PTR VCacheMask(void);
#include "maxperf.c"   /* max-perf 11-13: the PLAY record (play-<game>.txt) and Windows' GPU preference for the engines of UFC 1-4 */
typedef struct { int game; LONG session; FILETIME start; } WatchArgs;
static DWORD WINAPI FreezeWatch(LPVOID p)
{
    WatchArgs a = *(WatchArgs *)p; free(p);
    wchar_t dir[MAX_PATH], logs[MAX_PATH], log[MAX_PATH] = L"";
    LocalData(dir, G[a.game].data); Join(logs, dir, L"logs");
    ULARGE_INTEGER s0; s0.LowPart = a.start.dwLowDateTime; s0.HighPart = a.start.dwHighDateTime; s0.QuadPart -= 30ull * 10000000ull;
    LONGLONG offset = 0; DWORD stuckAt = 0; char carry[64] = ""; char why[200] = ""; BOOL gpuSeen = FALSE;   /* max-perf: the card the engine names */
    static char buf[1 << 20];
    while (!g_watchStop && g_watchSession == a.session) {
        Sleep(1000);
        if (!log[0]) {   /* the log this launch writes: the newest one created after the start */
            wchar_t pat[MAX_PATH]; Join(pat, logs, L"*.log"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
            ULARGE_INTEGER best = {0};
            if (h != INVALID_HANDLE_VALUE) {
                do { ULARGE_INTEGER c; c.LowPart = fd.ftCreationTime.dwLowDateTime; c.HighPart = fd.ftCreationTime.dwHighDateTime;
                     if (c.QuadPart >= s0.QuadPart && c.QuadPart > best.QuadPart) { best = c; Join(log, logs, fd.cFileName); } } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
            if (!log[0]) continue;
        }
        HANDLE f = CreateFileW(log, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
        if (f == INVALID_HANDLE_VALUE) continue;
        for (;;) {
            LARGE_INTEGER at; at.QuadPart = offset; SetFilePointerEx(f, at, NULL, FILE_BEGIN);
            size_t keep = strlen(carry); memcpy(buf, carry, keep);
            DWORD got = 0; if (!ReadFile(f, buf + keep, (DWORD)(sizeof(buf) - keep - 1), &got, NULL) || !got) break;
            offset += got; buf[keep + got] = 0;
            for (size_t i = keep; i < keep + got; i++) if (!buf[i]) buf[i] = ' ';   /* stray NUL bytes would end the search early */
            if (!gpuSeen) gpuSeen = MpEngineGpu(a.game, buf);
            char *hit = strstr(buf, "GPU wait stuck");
            if (hit && !stuckAt) { stuckAt = GetTickCount(); char *e = strchr(hit, '\n'); size_t n = e ? (size_t)(e - hit) : strlen(hit); if (n > 190) n = 190; memcpy(why, hit, n); why[n] = 0; if (n && why[n - 1] == '\r') why[n - 1] = 0; }
            size_t tot = keep + got, c = tot > 40 ? 40 : tot; memcpy(carry, buf + tot - c, c); carry[c] = 0;
        }
        CloseHandle(f);
        if (stuckAt && GetTickCount() - stuckAt >= 5000) {   /* 5 s later: the engine's own wait report is in the log too */
            DWORD pid = FindProcess(L"sacredfire-game.exe");
            wchar_t out[MAX_PATH]; wcscpy(out, log); wchar_t *dot = wcsrchr(out, L'.'); if (dot) *dot = 0; wcsncat(out, L".stacks.txt", MAX_PATH - wcslen(out) - 1);
            BOOL ok = pid && SaveStacks(pid, out, why);
            if (ok) { wcscpy(g_frozenFile, out); PostMessageW(g_wnd, WM_APP_FROZEN, (WPARAM)a.game, 0); }
            break;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- engine priority (3.5)
   The game engine started by a port's launcher gets "above normal" priority and no Windows power throttling (EcoQoS),
   the same treatment that gave UFC 5 more FPS in the fight: the launcher's own priority does not pass to the processes
   it starts. engine_boost=0 in portheon.ini (Extras) turns it off. */
typedef struct { ULONG Version, ControlMask, StateMask; } PowerThrottle_;
/* 4.3.3: while a game runs, PORTHEON itself steps aside: below-normal priority and EcoQoS (Windows' execution-speed
   throttling) on its own process, so every core and the processor's boost stay with the game; what PORTHEON still does
   then (the focus watch every 400 ms, the engine's freeze watch) needs almost nothing. Back to normal at the game's end. */
static void SelfQuiet(BOOL on)
{
    SetPriorityClass(GetCurrentProcess(), on ? BELOW_NORMAL_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
    typedef BOOL (WINAPI *SetPI)(HANDLE, int, LPVOID, DWORD);
    SetPI spi = (SetPI)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetProcessInformation");
    PowerThrottle_ st = {1, 1 /* EXECUTION_SPEED */, on ? 1u : 0u};
    if (spi) spi(GetCurrentProcess(), 4 /* ProcessPowerThrottling */, &st, sizeof(st));
    /* 4.4.0: PORTHEON's memory pages are the first Windows gives up when the game needs RAM (MEMORY_PRIORITY_LOW = 2,
       normal = 5) */
    ULONG mp = on ? 2u : 5u;
    if (spi) spi(GetCurrentProcess(), 0 /* ProcessMemoryPriority */, &mp, sizeof(mp));
}
/* ---------------------------------------------------------------- 6.1: the processes a game's launcher started
   A process's parent id stays in its record after the parent ends, and Windows gives that id to a new process later: a
   walk by parent id alone also takes "orphans" of a long-gone process that had the same id. A child counts here only
   when it was created after its parent (and the parent is still known: alive, or held by a handle). */
static ULONGLONG ProcBornH(HANDLE h)   /* a process's creation time (0: not known) */
{
    FILETIME c, e, k, u; if (!h || !GetProcessTimes(h, &c, &e, &k, &u)) return 0;
    return ((ULONGLONG)c.dwHighDateTime << 32) | c.dwLowDateTime;
}
static ULONGLONG ProcBornPid(DWORD pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); if (!h) return 0;
    ULONGLONG t = ProcBornH(h); CloseHandle(h); return t;
}
typedef struct { DWORD pid; ULONGLONG born; wchar_t exe[64]; } PNode;
/* the live tree under root, nearest first (n[0] = root); a child only when created after its parent */
static int ProcTree(DWORD root, PNode *n, int max)
{
    int k = 0, done = 0; if (max < 1) return 0;
    n[0].pid = root; n[0].born = ProcBornPid(root); n[0].exe[0] = 0; k = 1;
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (sn == INVALID_HANDLE_VALUE) return k;
    while (done < k) {
        DWORD par = n[done].pid; ULONGLONG pb = n[done].born; done++;
        if (!pb) continue;   /* a parent of unknown age: its "children" are not trusted */
        PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
        if (Process32FirstW(sn, &pe)) do {
            if (pe.th32ParentProcessID != par || pe.th32ProcessID == par || pe.th32ProcessID == root || k >= max) continue;
            BOOL dup = FALSE; for (int i = 0; i < k && !dup; i++) dup = n[i].pid == pe.th32ProcessID;
            if (dup) continue;
            ULONGLONG cb = ProcBornPid(pe.th32ProcessID); if (!cb || cb < pb) continue;
            n[k].pid = pe.th32ProcessID; n[k].born = cb; wcsncpy(n[k].exe, pe.szExeFile, 63); n[k].exe[63] = 0; k++;
        } while (Process32NextW(sn, &pe));
    }
    CloseHandle(sn); return k;
}
/* programs that are never a game's own process, even when a game's shortcut started them: the stores' clients and the
   shell (a STOP never closes Steam or Explorer, and their other processes are not followed) */
static BOOL ProcShared(const wchar_t *exe)
{
    static const wchar_t *const n[] = {L"steam.exe", L"steamwebhelper.exe", L"steamservice.exe", L"EpicGamesLauncher.exe", L"EpicWebHelper.exe",
                                       L"GalaxyClient.exe", L"EADesktop.exe", L"Origin.exe", L"upc.exe", L"UbisoftConnect.exe", L"Battle.net.exe",
                                       L"explorer.exe", L"PORTHEON.exe", L"svchost.exe", L"RuntimeBroker.exe", L"dllhost.exe", L"sihost.exe",
                                       L"chrome.exe", L"msedge.exe", L"firefox.exe", L"opera.exe", L"brave.exe"};   /* a link opened in the browser */
    const wchar_t *b = wcsrchr(exe, L'\\'); b = b ? b + 1 : exe;
    for (size_t i = 0; i < sizeof(n) / sizeof(n[0]); i++) if (!_wcsicmp(b, n[i])) return TRUE;
    return FALSE;
}
/* a tree held by handles: the ids stay valid (Windows does not reuse the id of a process someone holds), so a process
   whose parent ended is still found by its parent's id. kill[i]: the handle may terminate it */
#define PT_MAX 64
typedef struct { int n; HANDLE h[PT_MAX]; DWORD pid[PT_MAX]; ULONGLONG born[PT_MAX]; BYTE kill[PT_MAX]; } PTree;
static void PTreeFree(PTree *t) { for (int i = 0; i < t->n; i++) if (t->h[i]) CloseHandle(t->h[i]); t->n = 0; }
static BOOL PTreeHas(const PTree *t, DWORD pid) { for (int i = 0; i < t->n; i++) if (t->pid[i] == pid) return TRUE; return FALSE; }
static void PTreeAddH(PTree *t, HANDLE h, BOOL kill)   /* takes h (the started process) */
{
    if (!h) return; DWORD pid = GetProcessId(h);
    if (t->n >= PT_MAX || PTreeHas(t, pid)) { CloseHandle(h); return; }
    t->h[t->n] = h; t->pid[t->n] = pid; t->born[t->n] = ProcBornH(h); t->kill[t->n] = (BYTE)kill; t->n++;
}
static void PTreeScan(PTree *t)   /* adds the children of every member (alive or not) created after it */
{
    if (!t->n) return;
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (sn == INVALID_HANDLE_VALUE) return;
    for (int done = 0; done < t->n; done++) {
        DWORD par = t->pid[done]; ULONGLONG pb = t->born[done]; if (!pb) continue;
        PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
        if (Process32FirstW(sn, &pe)) do {
            if (pe.th32ParentProcessID != par || pe.th32ProcessID == par || t->n >= PT_MAX || PTreeHas(t, pe.th32ProcessID) || ProcShared(pe.szExeFile)) continue;
            BOOL kill = TRUE;
            HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (!h) { kill = FALSE; h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID); }   /* an elevated child */
            if (!h) continue;
            ULONGLONG cb = ProcBornH(h);
            if (!cb || cb < pb || GetProcessId(h) != pe.th32ProcessID) { CloseHandle(h); continue; }
            t->h[t->n] = h; t->pid[t->n] = pe.th32ProcessID; t->born[t->n] = cb; t->kill[t->n] = (BYTE)kill; t->n++;
        } while (Process32NextW(sn, &pe));
    }
    CloseHandle(sn);
}
static BOOL PTreeLive(const PTree *t, int i) { return t->h[i] && WaitForSingleObject(t->h[i], 0) == WAIT_TIMEOUT; }
static int PTreeAlive(const PTree *t, HANDLE *out, int max, BOOL *anyKill)   /* the live members' handles */
{
    int k = 0; if (anyKill) *anyKill = FALSE;
    for (int i = 0; i < t->n; i++) if (PTreeLive(t, i)) { if (k < max) out[k++] = t->h[i]; if (anyKill && t->kill[i]) *anyKill = TRUE; }
    return k;
}
static void PTreeKill(PTree *t)   /* the newest first (the game before its launcher) */
{
    PTreeScan(t);
    for (int i = t->n - 1; i >= 0; i--) if (t->kill[i] && PTreeLive(t, i)) TerminateProcess(t->h[i], 1);
}
static DWORD FindEngine(DWORD parent)   /* the engine a port's launcher started (0: not yet) */
{
    /* the UFC 1-4 ports' sacredfire-game.exe; 4.3.2: the KytyPS5 engines UFC6.exe and UFC5.exe start, for the focus
       hand-off (FocusTimer) */
    /* single window (2026-11-13): "UFC 6 novo.exe", UFC 6's new engine (PS5 Pro / DLSS, the automatic choice on an RTX
       card), was missing: PORTHEON never found that game's window, so it stayed open at full size behind the game for the
       whole session (measured: the 400 ms FocusTimer and the loading ring, 0.4-0.7 s of CPU per 30 s) and close_on_play
       did nothing */
    static const wchar_t *const names[] = {L"sacredfire-game.exe", L"UFC 6.exe", L"UFC 6 Parsec.exe", L"UFC 6 compat.exe", L"UFC 6 novo.exe", L"UFC 5.exe", L"UFC 5 AMD.exe",
                                           L"ufc3-launcher-l1.exe", L"ufc3-original-v48.exe", L"gow3-probe.exe",   /* 5.0: UFC Undisputed 3 (Reborn, Original). 4.3.3: UFC 6 Parsec.exe = Parsec mode, 2 players; 4.3.4: UFC 6 compat.exe = cards without barycentrics; 6.0: God of War III's game (its launcher's child) */
                                           L"bb-probe.exe", L"iss-probe.exe"};   /* 6.1: Bloodborne's and inFAMOUS's games */
    /* 6.1: the whole tree under parent, nearest first (as WaitTreeThread): Bloodborne's "Bloodborne.exe --play" starts
       "Bloodborne.exe --run", which starts bin\bb-probe.exe; a child only when created after its parent (ProcTree) */
    PNode nd[64]; int n = ProcTree(parent, nd, 64);
    for (int k = 1; k < n; k++) for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) if (!_wcsicmp(nd[k].exe, names[i])) return nd[k].pid;
    return 0;
}
static DWORD WINAPI BoostThread(LPVOID p)
{
    DWORD parent = (DWORD)(UINT_PTR)p, done = 0; LONG session = g_watchSession; BOOL ps4 = PS4G(g_procGame);
    typedef BOOL (WINAPI *SetPI)(HANDLE, int, LPVOID, DWORD);
    SetPI spi = (SetPI)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetProcessInformation");
    /* 5.2: for the whole session, not 2 minutes: a first PLAY in the processor compatibility mode, or a first
       merge_update, prepares for longer, and the engine then ran at normal priority with EcoQoS allowed. Every 500 ms
       for the first 2 minutes, then every 2 s; each new engine process (UFC Undisputed 3's launcher can start its game
       again) is set once. The loop ends with the game (OnExited sets g_watchStop) or with PORTHEON (CloseTimer). */
    for (int i = 0; !g_watchStop && g_watchSession == session; i++) {
        /* 6.0 final: after 2 minutes one look every 10 s (a restarted engine is still caught). Single window (11-13): for
           UFC 1-4 already once their engine is found and set: each look lists every process of the PC, and at two a
           second that was PORTHEON's whole CPU use while it stayed with a game (measured 0.31-0.45 s per 30 s, UFC 3/4's
           90 s; UFC Undisputed 3, whose launcher can start its game again, keeps the fast looks) */
        Sleep(i < 240 && !(ps4 && done) ? 500 : 10000);
        DWORD pid = FindEngine(parent);
        if (!pid || pid == done) continue;
        done = pid;
        HANDLE h = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (h) {
            DWORD was = GetPriorityClass(h); int eco = 3;   /* max-perf: what was found and what Windows accepted, for the PLAY record */
            if (was == NORMAL_PRIORITY_CLASS) SetPriorityClass(h, ABOVE_NORMAL_PRIORITY_CLASS);
            /* 3.5.1: Windows 11 stops honoring the timer resolution of a process whose window is out of sight (behind
               another window, minimized): the game would keep its frame pacing only while in front. 4.3.2: both flags
               in one call, since Windows keeps only the last call's masks; Windows 10 refuses the timer flag (error
               87), so there the execution-speed one goes alone. */
            PowerThrottle_ st = {1, 1 /* EXECUTION_SPEED */ | 4 /* IGNORE_TIMER_RESOLUTION */, 0};
            if (!spi) eco = 0;
            else if (!spi(h, 4 /* ProcessPowerThrottling */, &st, sizeof(st))) { st.ControlMask = 1; eco = spi(h, 4, &st, sizeof(st)) ? 1 : 0; }
            MpEngineRecord(pid, h, was, eco);
            /* max-perf 11-13: once more 2 s later. The engine makes its own EcoQoS call at start (execution speed only);
               Windows keeps the last call's masks, so one that came after PORTHEON's would drop the Windows 11 flag that
               keeps the timer resolution while the window is hidden. The same values again: nothing new on Windows 10. */
            Sleep(2000);
            DWORD alive = 0;
            if (!g_watchStop && g_watchSession == session && GetExitCodeProcess(h, &alive) && alive == STILL_ACTIVE) {
                if (GetPriorityClass(h) == NORMAL_PRIORITY_CLASS) SetPriorityClass(h, ABOVE_NORMAL_PRIORITY_CLASS);
                PowerThrottle_ s2 = {1, 1 | 4, 0}; int e2 = 3;
                if (!spi) e2 = 0; else if (!spi(h, 4, &s2, sizeof(s2))) { s2.ControlMask = 1; e2 = spi(h, 4, &s2, sizeof(s2)) ? 1 : 0; }
                MpLog(L"Engine process %lu, 2 s later: priority class %s (read back); EcoQoS off applied again: %s", pid, MpPrioName(GetPriorityClass(h)), e2 ? L"accepted" : L"refused");
            }
            CloseHandle(h);
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- the game window's focus (3.5.1)
   A player's stability tests with UFC 2 and 4 (2026-10-23): once the game's window loses the focus (Alt+Tab, a click on
   a second monitor) the game closes sooner or later; kept in focus it ran for hours. Since 3.5 the controller also
   drove a game whose window had no focus, so a game that opened behind another window (PORTHEON minimized itself at
   PLAY, and Windows lets a new window take the focus only when its process may) could be played to the end out of
   focus. Now, for UFC 1-4:
   - PORTHEON stays in front until the game's window shows, hands it the focus (the foreground process may) and only
     then minimizes, without activating anything else;
   - the controller works out of focus only when it is turned on (Gameplay) or in Parsec mode, as before 3.5;
   - while playing, a focus loss makes the game's taskbar button blink until the game is back in front, and the losses
     are counted: a crash after them says so (OnExited). */
static DWORD g_enginePid; static HANDLE g_engineH; static HWND g_gameWnd; static HWINEVENTHOOK g_fgHook; static int g_focusLost; static BOOL g_gameFocused; static ULONGLONG g_focusLostAt;
/* 3.5.2: the game a desktop shortcut started (PORTHEON closes after its normal end), and the game whose shortcut this
   session's PLAY made (said once the game ends) */
static int g_directPlay = -1, g_shortcutNew = -1;
/* 4.3.1: input right after a game ends does not start it again. UFC 2 and UFC 3 started again by themselves a second
   after they closed (2026-10-29): the game had lost the focus to the browser the player was typing in, PORTHEON took the
   foreground back with the game's card focused, and the next Space pressed PLAY. Now PORTHEON comes to the front only if
   the player was in the game (OnExited), and for a moment after a game ends (in a PORTHEON that RelaunchSelf opens after
   one too) Enter, Space, the controller's A and Start and the PLAY button do nothing; an Enter or Space held since before
   (a key repeat) never presses anything. */
static ULONGLONG g_quietUntil;
static BOOL QuietInput(void) { return GetTickCount64() < g_quietUntil; }
static void AutoShortcut(int g); static void AddToSteam(int g);   /* 5.0 */
typedef struct { DWORD pid; HWND w; } WndFind;
static BOOL CALLBACK FindGameWnd(HWND w, LPARAM lp)
{
    WndFind *f = (WndFind *)lp; DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
    if (pid == f->pid && IsWindowVisible(w) && !GetWindow(w, GW_OWNER)) { f->w = w; return FALSE; }
    return TRUE;
}
static void BlinkGame(void) { if (g_gameWnd) { FLASHWINFO fi = {sizeof(fi), g_gameWnd, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0}; FlashWindowEx(&fi); } }
static void CALLBACK ForegroundHook(HWINEVENTHOOK h, DWORD ev, HWND w, LONG obj, LONG child, DWORD tid, DWORD ms)
{
    (void)h; (void)ev; (void)obj; (void)child; (void)tid; (void)ms;
    if (!g_gameWnd || !w) return;
    DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
    if (pid == g_enginePid) g_gameFocused = TRUE;
    else if (g_gameFocused && IsWindow(g_gameWnd) && IsWindowVisible(g_gameWnd)) {   /* a closing game's window going away is no loss */
        g_gameFocused = FALSE; g_focusLost++; g_focusLostAt = GetTickCount64();
        if (PS4G(g_procGame)) BlinkGame();   /* 4.3.2: UFC 5 and 6 run on out of focus; only UFC 1-4 can close */
    }
}
/* 4.3.6, the user: "when PORTHEON's games run, only they stay open; PORTHEON closes". Once the game's window has the
   focus (and the engine its priority, BoostThread, which finds it first), PORTHEON writes down which game it left
   running (pending_* in portheon.ini) and exits; the game's own launcher stays with the game. The next PORTHEON tells
   how that game ended (LateExited), with the exit code UFC6.exe/UFC5.exe leave in their ini (last_code) and, for UFC
   1-4, what the engine's log says. A game that never got its window or the focus keeps PORTHEON open, as before, to
   explain an early end. Lost while closed: the freeze watch of UFC 1-4 (its stacks file) and the focus-loss count.
   close_on_play=0 (Settings > Interface) keeps PORTHEON open. */
static ULONGLONG g_closeAt;
static BOOL g_hzOn;   /* 6.2: the monitor is at the rate this game asked for (tune62.c) */
static HANDLE g_u2srv, g_u2job;   /* 4.3.6: the online server of UFC 2 (6.2: or 3) while this PC hosts (Ufc2OnlineEnv) */
static int g_u2game = 1;          /* 6.2: the game it serves */
static BOOL CloseOnPlay(void) { return PIni(L"close_on_play", 1) && !UnderWine() && !g_u2srv && !g_hzOn; }   /* the host's server lives in PORTHEON's job */
static void PendingClear(void)
{
    static const wchar_t *const k[] = {L"pending_game", L"pending_pid", L"pending_born", L"pending_t0"};
    for (int i = 0; i < 4; i++) WritePrivateProfileStringW(L"PORTHEON", k[i], NULL, g_pIni);
}
static ULONGLONG ProcBorn(HANDLE h) { FILETIME c, e, k, u; ULARGE_INTEGER v = {0}; if (h && GetProcessTimes(h, &c, &e, &k, &u)) { v.LowPart = c.dwLowDateTime; v.HighPart = c.dwHighDateTime; } return v.QuadPart; }
static long long UnixNow(void) { FILETIME ft; GetSystemTimeAsFileTime(&ft); ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime; return (long long)(u.QuadPart / 10000000ull) - 11644473600ll; }
static void CloseTimer(void);

/* ---------------------------------------------------------------- 5.2: crash memory and the compatible mode
   UFC 1-4 and UFC Undisputed 3 (UFC 5 does its own in UFC5.exe 1.32.0; UFC 6 has UFC6.exe's repair ladder). Before
   5.2 nothing was kept between sessions: a player whose game closed again and again got the same settings every time.
   Now, in portheon.ini:
   - crash_streak_<id>: +1 at every error end (OnExited, LateExited); a clean session of 5 minutes or more sets it back
     to 0 (STOP ends with code 1, a normal end);
   - running_<id>: set at PLAY, cleared at the end. Still set at the next start with no game running and no pending_*
     to explain it: the PC froze or restarted in the game, or PORTHEON was killed. That counts as a crash;
   - at 2 crashes in a row, PLAY asks once whether to play in the compatible mode (SafePrompt); after 3 clean sessions
     in it, PLAY offers to go back. SafeSet saves what it changes (safe_<id>_<key>), as ParsecSet does, and gives it back.
   - safe_ask_<id> (7.1 review): a code 23 with nothing left for Gpu23Escalate to change asks at the next PLAY after one
     error; cleared with the streak (an answer, SafeSet, a clean session of 5 minutes or more).
   The game page's chip to leave the mode calls SafeSet(g, FALSE). */
static int GIni(const wchar_t *pre, int g, int def) { wchar_t k[64]; _snwprintf(k, 64, L"%s_%s", pre, G[g].id); k[63] = 0; return PIni(k, def); }
static void GIniSet(const wchar_t *pre, int g, int v)   /* v < 0 removes the key */
{
    wchar_t k[64], b[16]; _snwprintf(k, 64, L"%s_%s", pre, G[g].id); k[63] = 0; _snwprintf(b, 16, L"%d", v);
    WritePrivateProfileStringW(L"PORTHEON", k, v < 0 ? NULL : b, g_pIni);
}
static BOOL StreakGame(int g) { return PS4G(g) || X360G(g); }
static BOOL SafeOn(int g) { return g >= 0 && g < NGAMES && StreakGame(g) && GIni(L"safe", g, 0) != 0; }
static void RunningMark(int g) { if (StreakGame(g)) GIniSet(L"running", g, 1); }
static void RunningClear(int g) { if (g >= 0 && StreakGame(g)) GIniSet(L"running", g, -1); }
static void StreakAfter(int g, DWORD code, ULONGLONG playedMs)
{
    if (g < 0 || !StreakGame(g)) return;
    /* UFC3.exe passes the game's own exit code through, and a player's own close can end with a non-zero one (OnExited
       says "if you did not close it"): for UFC Undisputed 3 only an NTSTATUS error (0xC0000000 and up, which includes
       RunningLeft's 0xFFFFFFFF) is a crash; any other code says nothing either way */
    if (X360G(g) && code != 0 && code != 1 && code < 0xC0000000u) return;
    if (code != 0 && code != 1) { GIniSet(L"crash_streak", g, GIni(L"crash_streak", g, 0) + 1); GIniSet(L"safe_clean", g, -1); return; }
    if (playedMs < 300000) return;   /* a short clean session says nothing either way */
    GIniSet(L"crash_streak", g, -1); GIniSet(L"safe_ask", g, -1);   /* 7.1 review: and Gpu23Escalate's pending offer */
    GIniSet(L"stuck_ps", g, -1);   /* 7.1: the last stuck shader is forgotten after a clean session (LearnStuckShader) */
    if (SafeOn(g)) GIniSet(L"safe_clean", g, GIni(L"safe_clean", g, 0) + 1);
}
/* the SPIR-V shader cache of UFC 1-4 (%LOCALAPPDATA%\UFCnNative\user\cache, shared by every profile through
   BB_GPU_USER_DIR) is renamed to cache.old once, never deleted: the engine builds a fresh one, and the old one stays
   for a report or to be put back by hand */
static void SafeCacheAside(int g)
{
    if (!PS4G(g) || !G[g].data) return;
    wchar_t d[MAX_PATH], c[MAX_PATH], o[MAX_PATH]; LocalData(d, G[g].data); Join(c, d, L"user\\cache"); Join(o, d, L"user\\cache.old");
    if (DirExists(c) && !DirExists(o) && !FileExists(o)) MoveFileExW(c, o, 0);
}
static void SafeSet(int g, BOOL on)
{
    if (g < 0 || !StreakGame(g)) return;
    /* UFC Undisputed 3's video settings live in its port's configurator: only PORTHEON's own extras go off there */
    /* 7.1: pgame:tups, the temporal upscaler (FSR 3.1 / 4 / TAA), was outside the compatible mode, which said "no FSR" */
    static const wchar_t *const keys4[] = {L"resolution", L"ultra", L"neo", L"upscaler", L"sharpness", L"fps_limit", L"present_mode", L"low_latency",
                                           L"pgame:fsr", L"pgame:mfg", L"pgame:gpu_safe", L"pgame:engine", L"patch:crowd", L"pini:ufc4_crowd_off", L"pgame:tups"};
    static const int defs4[] = {1, 0, 0, 0, 2, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0};   /* what a missing key means (the Field defaults: F4_PRES is 0, Mailbox) */
    static const wchar_t *const keys3u[] = {L"pgame:mfg"}; static const int defs3u[] = {0};
    const wchar_t *const *keys = PS4G(g) ? keys4 : keys3u; const int *defs = PS4G(g) ? defs4 : defs3u; int nk = PS4G(g) ? N(keys4) : N(keys3u);
    wchar_t pk[96], b[16]; g_kvGame = -1;
    if (!on) {
        for (int i = 0; i < nk; i++) {
            _snwprintf(pk, 96, L"safe_%s_%s", G[g].id, keys[i]); pk[95] = 0;
            int v = GetPrivateProfileIntW(L"PORTHEON", pk, -1, g_pIni);
            if (v >= 0) PutInt(g, keys[i], v);
            WritePrivateProfileStringW(L"PORTHEON", pk, NULL, g_pIni);
        }
        GIniSet(L"safe", g, -1); GIniSet(L"safe_clean", g, -1); GIniSet(L"crash_streak", g, -1); GIniSet(L"safe_ask", g, -1); g_kvGame = -1;
        return;
    }
    if (SafeOn(g)) return;   /* already on: what it saved stays the player's own */
    int vmb = g_haveGpu ? (int)(g_gpu.vram >> 20) : 0;
    for (int i = 0; i < nk; i++) {
        const wchar_t *k = keys[i]; int cur = GetInt(g, k, defs[i]), want = -1;
        if (!wcscmp(k, L"resolution")) want = 0;                                   /* 720p */
        else if (!wcscmp(k, L"ultra")) want = !vmb || vmb < 4000 ? 1 : -1;          /* small or unknown video memory */
        else if (!wcscmp(k, L"neo") || !wcscmp(k, L"upscaler") || !wcscmp(k, L"sharpness")) want = g == 0 && !wcscmp(k, L"neo") ? -1 : 0;
        else if (!wcscmp(k, L"fps_limit")) want = cur == 30 ? -1 : 60;             /* 30 (the PS4's own) stays */
        else if (!wcscmp(k, L"present_mode")) want = 1;                             /* FIFO, the mode every driver has */
        else if (!wcscmp(k, L"low_latency")) want = GetInt(g, L"parsec", 0) ? -1 : 0;
        else if (!wcscmp(k, L"pgame:fsr") || !wcscmp(k, L"pgame:mfg") || !wcscmp(k, L"pgame:tups")) want = 0;
        else if (!wcscmp(k, L"pgame:gpu_safe")) want = g == 0 ? -1 : 1;             /* UFC 1 runs this way already */
        /* the port's own engine, except on Intel cards (the Arc fixes) and where PORTHEON put the new engine after a crash
           of the port's own (ufcN_new_engine: that one already failed); UFC 2 online keeps the v3.0 engine it needs */
        else if (!wcscmp(k, L"pgame:engine")) want = g == 0 || GpuIsIntel() || (g >= 2 && PIni(g == 2 ? L"ufc3_new_engine" : L"ufc4_new_engine", 0)) ? -1 : 1;
        else if (!wcscmp(k, L"patch:crowd")) want = (g == 1 || g == 2) && PatchState(g, L"crowd", -1) >= 0 ? 1 : -1;
        else if (!wcscmp(k, L"pini:ufc4_crowd_off")) want = g == 3 ? 1 : -1;
        if (want < 0 || cur == want || cur < 0) continue;
        _snwprintf(pk, 96, L"safe_%s_%s", G[g].id, k); pk[95] = 0; _snwprintf(b, 16, L"%d", cur);
        WritePrivateProfileStringW(L"PORTHEON", pk, b, g_pIni);
        PutInt(g, k, want);
    }
    SafeCacheAside(g);
    GIniSet(L"safe", g, 1); GIniSet(L"safe_clean", g, -1); GIniSet(L"crash_streak", g, -1); GIniSet(L"safe_ask", g, -1); g_kvGame = -1;
}
/* at PLAY: the offer to play in the compatible mode after 2 crashes in a row, or to leave it after 3 clean sessions.
   Never blocks: either answer plays. "No" to the offer starts the count again. */
static void SafePrompt(int g)
{
    if (!StreakGame(g) || !PIni(L"safe_offer", 1)) return;
    wchar_t q[900];
    /* 7.1 review (correctness#4, player#4): also when Gpu23Escalate had nothing left to change (safe_ask_<id>), with a
       first sentence that is true after a single stop; 2 or more errors in a row are said with their count as before */
    int streak = GIni(L"crash_streak", g, 0); BOOL ask = GIni(L"safe_ask", g, 0) != 0;
    if (!SafeOn(g) && (streak >= 2 || ask)) {
        /* 5.2: the text names only what SafeSet changes for this game (the same exceptions as there): UFC Undisputed 3
           only loses the frame generator, UFC 1 keeps its PS4 Pro switch, safe video mode and engine */
        wchar_t what[400] = L"";
        if (X360G(g)) wcscpy(what, L2(L"Nele só o gerador de quadros do PORTHEON fica desligado.", L"In it only PORTHEON's frame generator goes off."));
        else {
            BOOL eng = g != 0 && !GpuIsIntel() && !(g >= 2 && PIni(g == 2 ? L"ufc3_new_engine" : L"ufc4_new_engine", 0));
            BOOL crowd = g == 3 || ((g == 1 || g == 2) && PatchState(g, L"crowd", -1) >= 0);
            wcscat(what, L2(L"Ele usa 720p, 60 FPS com VSync", L"It uses 720p, 60 FPS with VSync"));
            if (g != 0) wcscat(what, L2(L", sem PS4 Pro", L", no PS4 Pro"));
            wcscat(what, L2(L", sem FSR e sem gerador de quadros", L", no FSR and no frame generator"));
            if (g != 0) wcscat(what, L2(L", o modo seguro de vídeo", L", the safe video mode"));
            if (crowd) wcscat(what, L2(L", a torcida desligada", L", the crowd off"));
            if (eng) wcscat(what, L2(L", o motor do próprio port", L", the port's own engine"));
            if (g <= 2) wcscat(what, L2(L", sem as opções experimentais e avançadas", L", none of the experimental and advanced options"));
            if (G[g].data) wcscat(what, L2(L", e começa um cache de shaders novo (o antigo fica guardado como cache.old)", L", and starts a fresh shader cache (the old one is kept as cache.old)"));
            wcscat(what, L".");
        }
        wchar_t head[300];
        if (streak >= 2) _snwprintf(head, 300, L2(L"O %s fechou com erro nas %d últimas vezes.", L"%s closed with an error the last %d times."), G[g].name, streak);
        else _snwprintf(head, 300, L2(L"O %s fechou com erro (a placa de vídeo parou) e o PORTHEON não tem mais nada para mudar sozinho para esse erro.",
                                      L"%s closed with an error (the graphics card stopped) and PORTHEON has nothing left to change by itself for that error."), G[g].name);
        head[299] = 0;
        _snwprintf(q, 900, L2(L"%s\n\nJogar no modo compatível? %s O que você tinha volta quando sair do modo compatível.%s\n\nSim: jogar no modo compatível.   Não: jogar como está.",
                              L"%s\n\nPlay in the compatible mode? %s What you had comes back when you leave the compatible mode.%s\n\nYes: play in the compatible mode.   No: play as it is."),
                   head, what,
                   X360G(g) ? L2(L"\n\nNo UFC Undisputed 3 o vídeo é do configurador do port (botão Configurar do port): ali, escolha a resolução mais baixa.", L"\n\nIn UFC Undisputed 3 the video settings are in the port's configurator (the port's Configure button): pick the lowest resolution there.") : L"");
        q[899] = 0;
        if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONQUESTION) == IDYES) {
            SafeSet(g, TRUE);
            /* 5.2: the game page's button to leave the mode is not there yet (ui_items.c): only the offer is named */
            Msg(C_ORANGE, L2(L"Modo compatível ligado no %s. Depois de 3 sessões sem erro o PORTHEON oferece voltar às suas configurações.", L"Compatible mode is on for %s. After 3 sessions without an error PORTHEON offers to go back to your settings."), G[g].name);
        } else { GIniSet(L"crash_streak", g, -1); GIniSet(L"safe_ask", g, -1); }
        return;
    }
    if (SafeOn(g) && GIni(L"safe_clean", g, 0) >= 3) {
        _snwprintf(q, 900, L2(L"O %s rodou 3 sessões seguidas sem erro no modo compatível.\n\nVoltar para as suas configurações de antes?\n\nSim: voltar ao normal.   Não: continuar no modo compatível.",
                              L"%s ran 3 sessions in a row without an error in the compatible mode.\n\nGo back to your settings from before?\n\nYes: back to normal.   No: stay in the compatible mode."), G[g].name);
        q[899] = 0;
        if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONQUESTION) == IDYES) SafeSet(g, FALSE);
        else GIniSet(L"safe_clean", g, -1);
    }
}
/* ---------------------------------------------------------------- single window (2026-11-13)
   The owner: "abrindo cada ufc de forma unica, sem nada em segundo plano". Measured before (single-window\NOTES.md):
   UFC 1-4's window is called "EA SPORTS™ UFC® 3" (the game's own title) and already carries the game's own icon, which
   the engine makes from the dump's icon0.png and owns for as long as it runs. So only the name is given here, once: a
   window keeps a text set from outside (measured: UFC 1 and 2 still carry it 60 s in, PORTHEON gone since second 3;
   UFC 3 and 4 at 95 s, after PORTHEON's 90 s), and nothing of PORTHEON has to stay for it. It is sent when the game's window is found and, if the engine was too busy to take it then, at
   each 1 s tick of CloseTimer until it holds. Only the game's own window (SDL's class), never an error box; a title
   with "[" in it is the engine's status line (Native Plus: key remap on/off) and is left alone. UFC 5 and UFC 6 have
   their own launchers for this (UFC6.exe's WinDress). window_name=0 in portheon.ini turns it off. */
static BOOL g_nameSet;
static void GameWndName(void)
{
    if (g_nameSet || !g_gameWnd || !PS4G(g_procGame) || !PIni(L"window_name", 1)) return;
    wchar_t t[160], cls[32]; t[0] = 0; DWORD_PTR r;
    if (!GetClassNameW(g_gameWnd, cls, 32) || wcscmp(cls, L"SDL_app")) { g_nameSet = TRUE; return; }
    GetWindowTextW(g_gameWnd, t, 160);   /* another process's caption: read from the window itself, no message sent */
    if (wcschr(t, L'[')) return;
    if (!wcscmp(t, G[g_procGame].name) || SendMessageTimeoutW(g_gameWnd, WM_SETTEXT, 0, (LPARAM)G[g_procGame].name, SMTO_ABORTIFHUNG, 200, &r)) g_nameSet = TRUE;
}
static void FocusWatchStart(void)
{
    g_enginePid = 0; g_gameWnd = NULL; g_focusLost = 0; g_gameFocused = FALSE; g_nameSet = FALSE;
    if (g_engineH) { CloseHandle(g_engineH); g_engineH = NULL; }
    if (!g_fgHook) g_fgHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, ForegroundHook, 0, 0, WINEVENT_OUTOFCONTEXT);
    SetTimer(g_wnd, 11, 400, NULL);   /* FocusTimer: looks for the game's window */
}
static void FocusWatchStop(void)
{
    KillTimer(g_wnd, 11);
    if (g_fgHook) { UnhookWinEvent(g_fgHook); g_fgHook = NULL; }
    g_gameWnd = NULL; g_enginePid = 0;
    if (g_engineH) { CloseHandle(g_engineH); g_engineH = NULL; }
}
static void FocusTimer(void)
{
    if (!g_proc) { KillTimer(g_wnd, 11); return; }
    /* 6.0 final: Zelda's emulator or native build is the started process itself (no engine child), God of War III's game
       is gow3-probe.exe under its launcher: found now, so PORTHEON hands the focus over, steps aside and frees its memory
       for them too (it kept the loading picture turning for the whole game) */
    if (!g_enginePid) g_enginePid = ZELDAG(g_procGame) || ISSG(g_procGame) || FNCG(g_procGame) ? GetProcessId(g_proc) : FindEngine(GetProcessId(g_proc));   /* 6.1: inFAMOUS: its launcher (PLAY is pressed there) */
    if (!g_enginePid) return;
    /* 6.1: the engine found under PORTHEON's tree is held (STOP closes it even after the launcher in between left, as
       Bloodborne's "--run" process can; the handle keeps its id from going to another program) */
    if (!g_engineH && g_enginePid != GetProcessId(g_proc)) {
        g_engineH = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, g_enginePid);
        if (g_engineH && ProcBornH(g_engineH) < ProcBornH(g_proc)) { CloseHandle(g_engineH); g_engineH = NULL; }
    }
    WndFind f = {g_enginePid, NULL}; EnumWindows(FindGameWnd, (LPARAM)&f);
    if (!f.w) return;
    KillTimer(g_wnd, 11); g_gameWnd = f.w;
    if (g_view == V_LAUNCH) { KillTimer(g_wnd, 13); g_view = V_HOME; }   /* 4.4.0: the game shows: the loading picture ends */
    if (GetForegroundWindow() != f.w) SetForegroundWindow(f.w);
    g_gameFocused = GetForegroundWindow() == f.w;
    if (!g_gameFocused) {   /* PORTHEON was not in front any more (a click elsewhere): the player has to click the game */
        BlinkGame();
        if (!PS4G(g_procGame))   /* 4.3.2: UFC 5 and 6 (their engines ignore the controller out of focus, unless Parsec mode) */
            Msg(C_ORANGE, L2(L"O %s abriu sem o foco: clique na janela do jogo para o controle funcionar.", L"%s opened without the focus: click the game's window so the controller works."), G[g_procGame].name);
        else Msg(C_ORANGE, L2(L"O %s abriu sem o foco: clique na janela do jogo antes de jogar (fora de foco ele pode fechar sozinho).", L"%s opened without the focus: click the game's window before playing (out of focus it can close by itself)."), G[g_procGame].name);
    }
    /* out of the way, without taking the focus back. Single window (11-13): when the player has PORTHEON close during
       the game (close_on_play, the default) and the game has the focus, the window is hidden, not minimized: no second
       button on the taskbar for the 3 s before it closes, for UFC 3/4's 90 s watch, or for a whole session it has to
       stay for (an online host's server, a changed monitor rate). It comes back by itself when the game ends with
       something to say (OnExited, WM_APP_FROZEN), and starting PORTHEON again shows it (OtherInstance). With
       close_on_play=0, under Wine, for Zelda/God of War and for a game that opened behind another window it is
       minimized as before, so the player has its button. */
    ShowWindow(g_wnd, PIni(L"close_on_play", 1) && !UnderWine() && !EXTG(g_procGame) && g_gameFocused ? SW_HIDE : SW_SHOWMINNOACTIVE);
    GameWndName();   /* single window: UFC 1-4's window gets the game's name */
    UiFreeMemory();   /* 4.4.0: the RAM PORTHEON drew with goes back to the game */
    /* 4.3.6: CloseTimer. 5.2: UFC 3 and 4 on the automatic engine with the new (or v3.0) one in keep PORTHEON 90 s, the
       window NewEngineFallback acts in: a crash there still reaches OnExited with its real exit code (PORTHEON runs below
       normal priority with EcoQoS meanwhile, SelfQuiet) */
    if (CloseOnPlay() && !EXTG(g_procGame)) {   /* 6.0 final: Zelda and God of War III keep a minimized, idle PORTHEON (no timer, one thread waiting) for their session's end */
        int pg = g_procGame; BOOL watch = (pg == 2 || pg == 3) && GetInt(pg, L"pgame:engine", 0) == 0 && GIni(L"engine_ran", pg, 0) > 0;
        g_closeAt = GetTickCount64() + (watch ? 90000 : 3000); SetTimer(g_wnd, 12, 1000, NULL);
    }
}

/* ---------------------------------------------------------------- controllers (3.5)
   The ports read controllers through SDL3 and set none of its options themselves, so the environment PORTHEON starts
   them with decides (the launcher passes it to the game):
   - every type: SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS from Gameplay's "out of focus" switch, so a controller keeps
     driving the game while Parsec's window, an overlay or a second monitor has the focus (by default SDL drops the
     input of an unfocused window: a guest's pad "freezing" in Parsec). 3.5.1: off unless turned on or in Parsec mode
     (see the game window's focus above);
   - Automatic / PS3: SDL's own DualShock 3 driver (SDL_JOYSTICK_HIDAPI_PS3, off by default on Windows);
   - PS5 / PS4: Sony's pads read directly with their full reports (rumble, light bar over Bluetooth), and XInput
     devices ignored, so a DS4Windows / Steam virtual Xbox pad of the same controller is not a second player;
   - Xbox: XInput only (no second HID copy of the same pad);
   - Generic: DirectInput on, and the community mapping file gamecontrollerdb.txt next to PORTHEON.exe when it is there
     (SDL_GAMECONTROLLERCONFIG_FILE: USB pads SDL does not know become gamepads);
   - Parsec mode: one player per controller (BB_LOCAL_MULTIPLAYER=1), so the friend's virtual pad is Player 2.
   Player 1's own controller is the port's "gamepad" setting (sacredfire.ini, its GUID), chosen in Gameplay. */
typedef struct { const wchar_t *k; wchar_t old[512]; DWORD had; } EnvSave;
static EnvSave g_envSave[96]; static int g_nEnvSave;   /* Released: 96 (40 could fill up at a UFC 1-4 PLAY, and a full table drops the variable) */
static void EnvPush(const wchar_t *k, const wchar_t *v)
{
    if (g_nEnvSave >= 96) return;
    EnvSave *e = &g_envSave[g_nEnvSave++]; e->k = k; e->had = GetEnvironmentVariableW(k, e->old, 512); SetEnvironmentVariableW(k, v);
}
static void EnvPopAll(void) { while (g_nEnvSave) { EnvSave *e = &g_envSave[--g_nEnvSave]; SetEnvironmentVariableW(e->k, e->had && e->had < 512 ? e->old : NULL); } }
static void PadDbPath(wchar_t *out) { Join(out, g_exeDir, L"gamecontrollerdb.txt"); }
static void PadEnv(int g)
{
    int t = GetInt(g, L"pgame:padtype", 0);
    EnvPush(L"SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", PIni(L"pad_background", 0) || GetInt(g, L"parsec", 0) ? L"1" : L"0");
    if (t == 0 || t == 3) EnvPush(L"SDL_JOYSTICK_HIDAPI_PS3", L"1");
    if (t == 1 || t == 2) { EnvPush(L"SDL_JOYSTICK_HIDAPI_PS4", L"1"); EnvPush(L"SDL_JOYSTICK_HIDAPI_PS5", L"1"); EnvPush(L"SDL_JOYSTICK_ENHANCED_REPORTS", L"1"); EnvPush(L"SDL_XINPUT_ENABLED", L"0"); }
    if (t == 4 || t == 5) { EnvPush(L"SDL_JOYSTICK_HIDAPI_XBOX", L"0"); EnvPush(L"SDL_XINPUT_ENABLED", L"1"); }
    if (t == 6) EnvPush(L"SDL_JOYSTICK_DIRECTINPUT", L"1");
    { wchar_t db[MAX_PATH]; PadDbPath(db); if (FileExists(db)) EnvPush(L"SDL_GAMECONTROLLERCONFIG_FILE", db); }
    if (GetInt(g, L"parsec", 0)) EnvPush(L"BB_LOCAL_MULTIPLAYER", L"1");
}
/* 3.5.3, "use the whole processor" (Extras, on by default): players saw the UFCs keep half of the processor idle. The
   engine sizes its helpers by the hardware threads: draw preparation workers half of them (at most 8; they run at idle
   priority and only take cores the game leaves free) and guest memory copy threads a quarter (at most 4, on the critical
   path of a frame). Now the workers get every thread but two (at most 16) and the copies a third (2 to 6). The game's
   own code still runs on the threads the game made: one busy core can stay the limit of a fight. cpu_all=0 goes back. */
/* 5.0 (the user: "force the X3D where it helps"): on a Ryzen with two dies where only one has the 3D V-Cache (7950X3D,
   9950X3D...) the game's threads stay on the V-Cache die, as UFC6.exe and UFC 5's engine already do. One L3 at least
   twice the other's size is that die; one die, or two alike, change nothing. x3d_ccd=0 in portheon.ini turns it off. */
static DWORD_PTR VCacheMask(void)
{
    if (!PIni(L"x3d_ccd", 1)) return 0;
    DWORD len = 0; GetLogicalProcessorInformationEx(RelationCache, NULL, &len); if (!len) return 0;
    BYTE *b = (BYTE *)malloc(len); DWORD best = 0, second = 0; DWORD_PTR mask = 0; int n = 0;
    if (b && GetLogicalProcessorInformationEx(RelationCache, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)b, &len))
        for (DWORD off = 0; off < len; ) {
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX p = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(b + off); if (!p->Size) break;
            if (p->Relationship == RelationCache && p->Cache.Level == 3 && p->Cache.GroupMask.Group == 0) {
                n++; if (p->Cache.CacheSize > best) { second = best; best = p->Cache.CacheSize; mask = p->Cache.GroupMask.Mask; } else if (p->Cache.CacheSize > second) second = p->Cache.CacheSize;
            }
            off += p->Size;
        }
    free(b);
    return n >= 2 && second && best >= second * 2 ? mask : 0;
}
/* 5.0: FSR 1 for the game this PLAY starts: PORTHEON's layer next to any layer the PC already has (the game's own
   launcher, UFC5.exe/UFC6.exe, and the engine inherit it); its log in %LOCALAPPDATA%\PORTHEON\fsr-<game>.log */
#define H_PADMAP 982   /* 6.2: the Controller screen's button (ui_padmap62.c) */
#define H_ENGENV 1003    /* Released: Extras > the engine's extra variables (EngineEnvEdit) */
#define H_PADPICK2 1004  /* Released: Player 2's controller (online.c) */
#define H_PADMAP2 1005   /* Released: Player 2's buttons (ui_padmap62.c) */
static void PadMapEnv(int g);   /* ui_padmap62.c */
static void Pad2Apply(int g);   /* online.c */
static void FsrEnv(int g)
{
    int m = GetInt(g, L"pgame:fsr", 0), tex = GetInt(g, L"pgame:tex", 0); if (m <= 0 && tex <= 0) return;   /* 6.2: the texture quality uses the same layer */
    wchar_t dir[MAX_PATH], js[MAX_PATH], dl[MAX_PATH]; Join(dir, g_exeDir, L"games\\engines\\fsr"); Join(js, dir, L"VkLayer_PORTHEON_fsr.json"); Join(dl, dir, L"PortheonFSR.dll");
    if (!FileExists(js) || !FileExists(dl)) return;
    static wchar_t lp[2048 + MAX_PATH], il[2048 + 32]; wchar_t cur[2048];
    DWORD n = GetEnvironmentVariableW(L"VK_LAYER_PATH", cur, 2048); _snwprintf(lp, 2048 + MAX_PATH, L"%s%s%s", dir, n && n < 2048 ? L";" : L"", n && n < 2048 ? cur : L""); lp[2048 + MAX_PATH - 1] = 0;
    n = GetEnvironmentVariableW(L"VK_INSTANCE_LAYERS", cur, 2048); _snwprintf(il, 2048 + 32, L"%s%sVK_LAYER_PORTHEON_fsr", n && n < 2048 ? cur : L"", n && n < 2048 ? L";" : L""); il[2048 + 32 - 1] = 0;
    static wchar_t sc[16], lg[MAX_PATH]; _snwprintf(sc, 16, L"%.2f", m >= 100 ? 1.0 : m / 100.0);
    { wchar_t d[MAX_PATH], f[48]; LocalData(d, L"PORTHEON"); _snwprintf(f, 48, L"fsr-%s.log", G[g].id); Join(lg, d, f); }
    EnvPush(L"VK_LAYER_PATH", lp); EnvPush(L"VK_INSTANCE_LAYERS", il);
    if (m > 0) { EnvPush(L"PORTHEON_FSR_SCALE", sc); EnvPush(L"PORTHEON_FSR_SHARP", m == 101 ? L"0.0" : m == 100 ? L"0.35" : L"0.25"); }
    EnvPush(L"PORTHEON_FSR_LOG", lg);
    if (tex > 0) {   /* 6.2: 1 Low (softer, a positive mip bias), 2 Medium 4x, 3 High 8x, 4 Ultra 16x and a sharper mip choice */
        static const wchar_t *const an[5] = {L"0", L"0", L"4", L"8", L"16"}, *const bi[5] = {L"0", L"1.0", L"0", L"0", L"-0.5"};
        if (tex > 4) tex = 4; EnvPush(L"PORTHEON_TEX_ANISO", an[tex]); EnvPush(L"PORTHEON_TEX_BIAS", bi[tex]);
    }
}
/* 7.3 (reports of 2026-11-16, two notebooks, UFC 4 on both of its engines: "mapping direct memory 0, size=4194304000
   failed (error 0x80020016)", then a fault in AssetStream::Translate a few seconds into the load, every start): the
   game folder's samplegame\data\common\config\videoheap.cfg had been rewritten by the Native Plus engine's own launcher
   (its ensure_videoheap_config: Render heap 4000M and UI heap 1024M, for 4K), which also gives its engine a bigger PS4
   memory pool (BB_DMEM_MB 7168-7680). PORTHEON gave the stock pool, where the stock heap fits (2723M: the same line of
   this PC's log maps 2855272448 bytes) and 4000M does not. A game folder with that edit gets the pool that launcher
   uses; the file itself is the player's and is left alone. Both UFC 4 engines read BB_DMEM_MB. */
static int VideoHeapMb(const wchar_t *dir)   /* the PS4 Render heap videoheap.cfg asks for (MB), 0: the game's own {{neoMode}?..} line or no file */
{
    wchar_t f[MAX_PATH]; _snwprintf(f, MAX_PATH, L"%s\\samplegame\\data\\common\\config\\videoheap.cfg", dir); f[MAX_PATH - 1] = 0;
    HANDLE h = CreateFileW(f, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    static char b[65536]; DWORD n = 0; BOOL ok = ReadFile(h, b, sizeof(b) - 1, &n, NULL); CloseHandle(h);
    if (!ok) return 0;
    b[n] = 0;
    for (const char *q = b; (q = strstr(q, "AddAllocator.kettle")) != NULL; q++) {   /* the kettle (PS4) Render heap's own line, not a comment */
        const char *e = strchr(q, '\n'); const char *k = strstr(q, "SharedVideoMemoryPPNL"); const char *z = strstr(q, "size=");
        if (!k || (e && k > e) || !z || (e && z > e)) continue;
        if (q > b && q[-1] == '#') continue;
        int mb = atoi(z + 5); if (mb >= 100) return mb;   /* a plain number (the stock line is "{{neoMode}?2723M:2211M}") */
    }
    return 0;
}
static int Ufc4HeapMb(int g)   /* the game folder, or the folder one down when base and update sit side by side */
{
    wchar_t gd[MAX_PATH]; GameDir(g, gd); if (!gd[0]) return 0;
    int mb = VideoHeapMb(gd); if (mb) return mb;
    wchar_t pat[MAX_PATH]; _snwprintf(pat, MAX_PATH, L"%s\\*", gd); pat[MAX_PATH - 1] = 0;
    WIN32_FIND_DATAW fd; HANDLE fh = FindFirstFileW(pat, &fd);
    if (fh != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
            wchar_t sub[MAX_PATH]; _snwprintf(sub, MAX_PATH, L"%s\\%s", gd, fd.cFileName); sub[MAX_PATH - 1] = 0;
            mb = VideoHeapMb(sub);
        } while (!mb && FindNextFileW(fh, &fd));
        FindClose(fh);
    }
    return mb;
}
/* 7.3 (the owner: "o pessoal perdeu FPS no UFC 4, antes mais de 100 e agora 40", "perda de FPS ao jogar por muito
   tempo"): since 7.0 UFC 4 runs on the Native Plus engine, but PORTHEON started it without the memory settings that
   engine's own launcher always gives it (launcher.py of Native Plus 1.2.0): the PS4 memory pool, the texture cache's
   cap and the times after which idle textures and video memory are given back. The same values as that launcher, by
   render size; an engine_env line or a variable already set wins. np_mem_env=0 in portheon.ini turns this off.
   And (reports of 2026-11-16, two notebooks, both UFC 4 engines: "mapping direct memory 0, size=4194304000 failed
   (error 0x80020016)", then a fault in AssetStream::Translate, every start): that launcher also rewrites the game
   folder's samplegame\data\common\config\videoheap.cfg (Render heap 4000M, UI heap 1024M, for 4K). The stock pool has
   room for the stock heap (2723M) only: 4194304000 bytes is exactly 4000M. 2243 + 26 + 4000 + 4 + 99 + 1024 + 1 MB
   are mapped at the load with that file, so its pool is 7680 MB on every engine (the file is the player's and stays). */
static void NpEnvOnce(const wchar_t *k, const wchar_t *v) { if (!GetEnvironmentVariableW(k, NULL, 0)) EnvPush(k, v); }
static int g_u4HeapMb;   /* the last PLAY's videoheap.cfg Render heap (MB, 0 = the game's own), for the end of the session */
static void Ufc4MemEnv(int g)
{
    if (g != 3) return;
    g_u4HeapMb = Ufc4HeapMb(g);
    BOOL np = WantNpEngine(g) && PIni(L"np_mem_env", 1);
    int pool = 0;
    MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms); int ram = (int)(ms.ullTotalPhys >> 20);
    if (np) {
        int res = GetInt(g, L"resolution", 1);   /* RES4: 0 720p, 1 1080p, 2 2K, 3 4K, 4 ultrawide 3440x1440 */
        int cap = res == 3 ? 5120 : (res == 2 || res == 4) ? 4096 : 3072, want = res == 3 ? 12288 : (res == 2 || res == 4) ? 10240 : 7680;
        int base = ram >= 12 * 1024 ? 7680 : 7168;
        pool = ram >= 12 * 1024 ? want : base;
        if (pool > (ram - 3072 > base ? ram - 3072 : base)) pool = ram - 3072 > base ? ram - 3072 : base;
        static wchar_t vc[16]; _snwprintf(vc, 16, L"%d", cap);
        NpEnvOnce(L"BB_GC_BUDGET_CAP_MB", vc);
        NpEnvOnce(L"BB_VRAM_IDLE_SECONDS", L"30"); NpEnvOnce(L"BB_GC_IDLE_SECONDS", L"45"); NpEnvOnce(L"BB_GC_PRESSURE_IDLE_SECONDS", L"20");
    }
    if (g_u4HeapMb > 2723 && PIni(L"ufc4_dmem_auto", 1)) {
        int need = 2243 + 26 + g_u4HeapMb + 4 + 99 + 1024 + 1 + 280;   /* the load's mappings with that file, the UI heap at its 1024M, and the spare 7680 leaves at 4000M */
        if (need < 7680) need = 7680;
        if (need > 16384) need = 16384;
        if (pool < need) pool = need;
    }
    if (pool) { static wchar_t vp[16]; _snwprintf(vp, 16, L"%d", pool); NpEnvOnce(L"BB_DMEM_MB", vp); }
    /* the automatic fallbacks the heap's crash booked (the Native Plus engine given up, the safe video mode, the new
       engine, the crash count that offers the compatible mode) were about a cause that is not the card's: undone once,
       when the last session's log shows it */
    if (g_u4HeapMb > 2723 && LogHas(g, "mapping direct memory 0, size=") && LogHas(g, "failed (error 0x80020016)") && !PIni(L"ufc4_heap_undo", 0)) {
        PIniSet(L"ufc4_heap_undo", 1);
        if (GetInt(g, L"pgame:engine", 0) == 0) { PIniSet(L"ufc4_no_np", 0); PIniSet(L"ufc4_new_engine", 0); }
        if (GetInt(g, L"pgame:gpu_safe", 0)) PutInt(g, L"pgame:gpu_safe", 0);
        GIniSet(L"crash_streak", g, -1); GIniSet(L"safe_ask", g, -1);
    }
}
static void CpuEnv(int g)
{
    Ufc4MemEnv(g);   /* 7.3: here because CpuEnv runs for every PS4 game at PLAY */
    if (!PIni(L"cpu_all", 1) || g_threads < 6) return;
    wchar_t v[16]; int prep = g_threads - 2, copies = g_threads / 3;
    if (prep > 16) prep = 16;
    if (copies > 6) copies = 6;
    if (copies < 2) copies = 2;
    _snwprintf(v, 16, L"%d", prep); EnvPush(L"BB_PREP_WORKERS", v);
    _snwprintf(v, 16, L"%d", copies); EnvPush(L"BB_COPY_THREADS", v);
}
/* 3.5.3, the safe video mode (Extras; turned on by itself after a GPU stop the shader skip could not cure): the engine
   records the GPU work on the game's GPU thread instead of its recording threads and draw pipe, as UFC 1 always does
   (BB_VK_RECORD_THREAD=0, BB_DRAW_PIPE=0, the path that ended UFC 1's freezes). A little less FPS on strong PCs. */
static BOOL GpuSafe(int g) { return GetInt(g, L"pgame:gpu_safe", 0) != 0; }
static void GpuSafeEnv(int g)
{
    if (g == 0 || !GpuSafe(g)) return;   /* UFC 1 runs this way already (ufc1_stable_engine) */
    EnvPush(L"BB_VK_RECORD_THREAD", L"0"); EnvPush(L"BB_DRAW_PIPE", L"0");
}
/* 3.5.3, the fighters' skin colors (a test, for the green skin a player saw on Charles Oliveira in UFC 4): the games
   compress each fighter's skin on the graphics card, writing BC blocks through an uncompressed view, and the engine
   moves those textures through memory (SHADPS4_BC_MODE 3, its default since mancelli's UFC 3 fix). Alternative A copies
   them on the graphics card (2), alternative B also skips the write-back of their storage views (0), as the older
   engines did. Extras > "Fighters' skin color (test)". */
static void SkinEnv(int g)
{
    int m = GetInt(g, L"pgame:skin_mode", 0);
    if (m == 1) EnvPush(L"SHADPS4_BC_MODE", L"2"); else if (m == 2) EnvPush(L"SHADPS4_BC_MODE", L"0");
}
/* 4.0.1, the hitches of a first fight: the engine compiles the pipelines of a new scene on its GPU thread, and the game
   waits (a UFC 4 Career fight with an empty cache: 583 compiles, 20.7 s, frames up to 9 s). BB_ASYNC_PIPELINES=1 has
   the passes drawn every frame compile on helper threads instead. Extras > "Compile shaders without freezing the fight";
   async_pipelines=0 in portheon.ini goes back. */
/* 6.1 (the user: shader compile speed for every UFC): what the four SacredFire engines read about pipelines, from their
   strings: BB_ASYNC_PIPELINES, BB_ASYNC_PIPELINE_THREADS, BB_PIPELINE_CACHE (all engines), BB_DRIVER_PIPELINE_CACHE,
   BB_BACKGROUND_WARMUP, BB_PARALLEL_WARMUP (only the v3.0 and new engines). What players' logs prove: async compiles
   (on by default since 4.0.1, every UFC 1-4) with the engine's own thread count, which already grows with the processor
   ("compile on 1 threads" on a 4-thread PC, 20261108_145527.log line 492; "compile on 3 threads" on 12 threads, 16
   test logs, e.g. tests\F4N.log line 698), and the shader cache on disk (always on: "WarmUp: Pipeline cache isn't
   compatible with current system: rebuilding it"). No log of any port or player has ever had a different thread
   count, the driver cache ("Driver pipeline cache: disabled", 35 logs) or the warm-ups on, so they stay as the engines
   have them; portheon.ini can try two of them: async_pipeline_threads=N (1-8; 0 = the engine's own count) and
   driver_pipeline_cache=1 (v3.0/new engine only). Neither on the cards with known problems (RTX 50, any Intel card,
   Radeon RX 5000), nor in the compatible or the safe video mode. */
static BOOL GpuPipeTrouble(void)
{
    if (!g_haveGpu) return TRUE;
    if (g_gpu.vendor == 0x8086 || GpuIsRdna1()) return TRUE;
    return g_gpu.vendor == 0x10de && wcsstr(g_gpu.name, L"RTX 50") != NULL;
}
static void AsyncEnv(int g)
{
    if (!PIni(L"async_pipelines", 1) || SafeOn(g)) return;   /* 5.2: off in the compatible mode */
    /* Released: Native Plus's own launcher runs its engine with BB_ASYNC_PIPELINES=0 on AMD cards (launcher.py); the same here */
    if (WantNpEngine(g) && g_haveGpu && g_gpu.vendor == 0x1002 && PIni(L"np_amd_sync_pipelines", 1)) { EnvPush(L"BB_ASYNC_PIPELINES", L"0"); return; }
    EnvPush(L"BB_ASYNC_PIPELINES", L"1");
    if (GpuSafe(g) || GpuPipeTrouble()) return;
    static wchar_t nt[8]; int n = PIni(L"async_pipeline_threads", 0), cap = g_threads - 2;
    if (cap > 8) cap = 8;
    if (n > cap) n = cap;
    if (n > 0) { _snwprintf(nt, 8, L"%d", n); nt[7] = 0; EnvPush(L"BB_ASYNC_PIPELINE_THREADS", nt); }
    if (PIni(L"driver_pipeline_cache", 0)) EnvPush(L"BB_DRIVER_PIPELINE_CACHE", L"1");
}
/* Released (2026-11-13): UFC4 Native Plus's advanced options for UFC 1 to 3 (launcher.py's tuning: frame_stats,
   prep_workers, the low-end profile, runtime_env), all through variables every SacredFire engine here reads:
   - pgame:frame_stats: BB_FRAME_STATS=1, the engine's "Frame stats: 59.2 FPS, worst frame ..." log line every 5 s;
   - pgame:prep_workers: BB_PREP_WORKERS=<n> (17 in portheon.ini = 0), after CpuEnv's own count;
   - pgame:lowend: on a CPU with up to 4 threads, BB_PREP_WORKERS=0 (Native Plus's profile; its 1600x900 is in the list below);
   - pgame:ires: BB_INTERNAL_WIDTH / BB_INTERNAL_HEIGHT = 854x480, 960x540 or 1600x900 (the ports' launchers keep a value
     that is already in the environment). Never run on UFC 1-3: experimental. Only with the 720p/1080p choices (2K/4K
     read their patches from patches\experiments) and not with the 21:9 options;
   - engine_env_<id> in portheon.ini: the player's own "NAME=value;NAME=value" list, BB_* and SHADPS4_BC_MODE only,
     set last so it wins (Extras > "Variáveis extras do motor" opens the file). */
static wchar_t g_xenvK[12][48];   /* EnvPush keeps the name's pointer until EnvPopAll */
static void NpOptsEnv(int g)
{
    if (g < 0 || g > 2) return;
    if (GetInt(g, L"pgame:frame_stats", 0) && !GetEnvironmentVariableW(L"BB_FRAME_STATS", NULL, 0)) EnvPush(L"BB_FRAME_STATS", L"1");
    if (SafeOn(g)) return;   /* the compatible mode: no thread count, internal size or free variable of the player's (the settings stay as they are) */
    int pw = GetInt(g, L"pgame:prep_workers", 0);
    if (GetInt(g, L"pgame:lowend", 0) && g_threads <= 4) pw = 17;
    if (pw >= 1 && pw <= 17) { wchar_t v[16]; _snwprintf(v, 16, L"%d", pw == 17 ? 0 : pw); v[15] = 0; EnvPush(L"BB_PREP_WORKERS", v); }
    int ir = GetInt(g, L"pgame:ires", 0);
    if (ir >= 1 && ir <= 3 && GetInt(g, L"resolution", 1) <= 1 && !GetInt(g, L"pgame:ultrawide", 0) && !Wide21On(g)) {
        static const wchar_t *const iw[3] = {L"854", L"960", L"1600"}, *const ih[3] = {L"480", L"540", L"900"};
        EnvPush(L"BB_INTERNAL_WIDTH", iw[ir - 1]); EnvPush(L"BB_INTERNAL_HEIGHT", ih[ir - 1]);
    }
    wchar_t ek[48], es[1024]; _snwprintf(ek, 48, L"engine_env_%s", G[g].id); ek[47] = 0;
    GetPrivateProfileStringW(L"PORTHEON", ek, L"", es, 1024, g_pIni);
    int nx = 0; wchar_t *ctx = NULL;
    for (wchar_t *t = wcstok(es, L";", &ctx); t && nx < 12; t = wcstok(NULL, L";", &ctx)) {
        while (*t == L' ') t++;
        wchar_t *eq = wcschr(t, L'='); if (!eq || eq == t) continue;
        *eq = 0; wchar_t *val = eq + 1; while (*val == L' ') val++;
        size_t l = wcslen(t); while (l && t[l - 1] == L' ') t[--l] = 0;
        if (l >= 48 || (wcsncmp(t, L"BB_", 3) && wcscmp(t, L"SHADPS4_BC_MODE"))) continue;
        BOOL ok = TRUE; for (size_t i = 0; i < l; i++) if (!((t[i] >= L'A' && t[i] <= L'Z') || (t[i] >= L'0' && t[i] <= L'9') || t[i] == L'_')) ok = FALSE;
        if (!ok) continue;
        wcscpy(g_xenvK[nx], t); EnvPush(g_xenvK[nx], val); nx++;
    }
}
/* Extras > "Variáveis extras do motor": the line engine_env_<id>= in portheon.ini (made when missing), opened in Notepad */
static void EngineEnvEdit(int g)
{
    if (g < 0 || g > 2) return;
    wchar_t ek[48], es[8]; _snwprintf(ek, 48, L"engine_env_%s", G[g].id); ek[47] = 0;
    GetPrivateProfileStringW(L"PORTHEON", ek, L"?", es, 8, g_pIni);
    if (!wcscmp(es, L"?")) WritePrivateProfileStringW(L"PORTHEON", ek, L"", g_pIni);
    wchar_t arg[MAX_PATH + 4]; _snwprintf(arg, MAX_PATH + 4, L"\"%s\"", g_pIni); arg[MAX_PATH + 3] = 0;
    ShellExecuteW(g_wnd, L"open", L"notepad.exe", arg, NULL, SW_SHOWNORMAL);
    Msg(C_SUB, L2(L"Abri o portheon.ini. Na linha %s= escreva as variáveis do motor separadas por ponto e vírgula (só BB_... e SHADPS4_BC_MODE), por exemplo BB_ANISO=16;BB_WINDOW_SIZE=1600x900. Salve o arquivo: vale no próximo JOGAR. Linha vazia = nada.",
                  L"portheon.ini is open. On the line %s= write the engine's variables separated by semicolons (BB_... and SHADPS4_BC_MODE only), for example BB_ANISO=16;BB_WINDOW_SIZE=1600x900. Save the file: it applies at the next PLAY. An empty line = nothing."), ek);
}
/* the controllers connected now, as the port sees them: its bb-gpu-capabilities.exe --gamepads ("GUID<tab>name") */
typedef struct { wchar_t guid[40], name[96]; } PadInfo;
static int ListPads(int g, PadInfo *out, int max)
{
    wchar_t d[MAX_PATH], exe[MAX_PATH], cmd[MAX_PATH + 32], bin[MAX_PATH]; PortDir(g, d); Join(bin, d, L"bin"); Join(exe, bin, L"bb-gpu-capabilities.exe");
    if (!FileExists(exe)) return -1;
    _snwprintf(cmd, MAX_PATH + 32, L"\"%s\" --gamepads", exe);
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE}; HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return -1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = {sizeof(si)}; si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; si.hStdOutput = wr; si.hStdError = wr;
    PROCESS_INFORMATION pi = {0}; int n = 0;
    BOOL ok = CreateProcessW(exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, bin, &si, &pi);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); return -1; }
    char buf[8192]; DWORD got = 0, k;
    while (got < sizeof(buf) - 1 && ReadFile(rd, buf + got, sizeof(buf) - 1 - got, &k, NULL) && k) got += k;
    buf[got] = 0; CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, 10000); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    for (char *line = buf; *line && n < max; ) {
        char *e = strchr(line, '\n'); if (e) *e = 0;
        char *tab = strchr(line, '\t');
        if (tab) {
            *tab = 0; char *cr = strchr(tab + 1, '\r'); if (cr) *cr = 0;
            MultiByteToWideChar(CP_UTF8, 0, line, -1, out[n].guid, 40); MultiByteToWideChar(CP_UTF8, 0, tab + 1, -1, out[n].name, 96);
            out[n].guid[39] = out[n].name[95] = 0; n++;
        }
        if (!e) break;
        line = e + 1;
    }
    return n;
}

/* ---------------------------------------------------------------- 3.5.3: a port file that is missing or blocked
   A port whose engine, DLLs or bundled Python went missing (an incomplete extraction, a file removed or blocked by a
   security program) failed to start with odd errors. Before each PLAY, and once when PORTHEON opens, every file a UFC 1-4
   port needs is checked, and the message names the file. */
static const wchar_t *const PORT_FILES[] = {L"bin\\sacredfire-game.exe", L"bin\\SDL3.dll", L"bin\\libc++.dll", L"bin\\libunwind.dll",
    L"bin\\libwinpthread-1.dll", L"bin\\bb-gpu-capabilities.exe", L"python\\python.exe", L"python\\python313.dll", L"python\\python313.zip",
    L"scripts\\prepare.py", L"scripts\\link_libc.py", L"scripts\\link_modules.py", L"scripts\\patches.py", L"scripts\\game_check.py",
    L"scripts\\merge_update.py", L"share\\aerolib.inl"};
/* 0: fine; 1: missing; 2: there but the antivirus refuses to open it. which: the file, relative to PORTHEON's folder */
static int PortFileProblem(int g, wchar_t *which, int n)
{
    if (!PS4G(g)) return 0;
    wchar_t d[MAX_PATH], f[MAX_PATH]; PortDir(g, d);
    for (int i = 0; i < N(PORT_FILES); i++) {
        Join(f, d, PORT_FILES[i]);
        int bad = 0;
        if (!FileExists(f)) bad = 1;
        else {
            HANDLE h = CreateFileW(f, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
            if (h == INVALID_HANDLE_VALUE) { DWORD e = GetLastError(); if (e == ERROR_VIRUS_INFECTED || e == ERROR_VIRUS_DELETED || e == ERROR_ACCESS_DENIED) bad = 2; }
            else CloseHandle(h);
        }
        if (bad) { _snwprintf(which, n, L"games\\%s\\%s", G[g].id, PORT_FILES[i]); which[n - 1] = 0; return bad; }
    }
    return 0;
}
static void AntivirusHelp(int g, int problem, const wchar_t *which, BOOL ask)
{
    (void)ask;
    Msg(C_BAD, L2(L"O %s não pode abrir: %s %s. Extraia de novo o pacote oficial do PORTHEON (github.com/" AUTHOR_NICK L"/PORTHEON) numa pasta nova. Se um programa de segurança marcou esse arquivo, pode ser um falso positivo: reporte ao fabricante dele e avise no Discord.",
                  L"%s cannot start: %s %s. Extract the official PORTHEON package (github.com/" AUTHOR_NICK L"/PORTHEON) again into a new folder. If a security program flagged that file, it may be a false positive: report it to its vendor and tell us on Discord."),
        G[g].name, which, problem == 2 ? L2(L"está bloqueado", L"is blocked") : L2(L"sumiu da pasta", L"is missing from the folder"));
}
/* once at start: the first installed port with a missing or blocked file says so on the status line */
static void CheckPortFiles(void)
{
    for (int g = 0; g < 4; g++) {
        if (!PortOk(g)) continue;
        wchar_t w[MAX_PATH]; int p = PortFileProblem(g, w, MAX_PATH);
        if (p) { AntivirusHelp(g, p, w, FALSE); return; }
    }
}
/* 3.5.3, Linux (Wine, Proton): the UFC 1-4 engines map the PS4's memory with two things Wine does not do yet. They
   import CreateFileMapping2 (Windows 10 1803), which Wine 9 has not got: the engine stops as it creates the game's memory.
   And they place 16 KiB PS4 pages over the address-space placeholders of VirtualAlloc2/MapViewOfFile3, which Wine only
   splits at 64 KiB (error 1132, ERROR_MAPPED_ALIGNMENT; the games' first map is 16 KiB at 0xfbffffc000). Checked on
   Wine 9.0; the engine's own memory probe says Wine 11 still wants 64 KiB. So on Linux PLAY explains it first. */
static BOOL LinuxNotice(int g)
{
    static BOOL told[NGAMES];
    if (!UnderWine() || !PS4G(g) || told[g] || PIni(L"linux_try", 0)) return TRUE;
    told[g] = TRUE;
    int r = MessageBoxW(g_wnd, L2(L"Linux (Wine/Proton): o UFC 1 a 4 ainda não abre aqui.\n\nO motor dos ports usa duas funções de memória do Windows que o Wine/Proton ainda não tem do jeito que o motor precisa (CreateFileMapping2, e páginas de 16 KB do PS4 nos espaços reservados de memória: o Wine só aceita 64 KB). Por isso o jogo fecha logo ao abrir, sem janela. A correção é no motor dos ports, não no PORTHEON.\n\nNo Linux funciona o UFC 3 pelo build nativo do GNSB (github.com/GNSB/UFC-3-PS4-PORT-LINUX-WINDOWS).\n\nTentar abrir mesmo assim?",
                                  L"Linux (Wine/Proton): UFC 1-4 do not start here yet.\n\nThe ports' engine uses two Windows memory functions that Wine/Proton do not provide the way it needs (CreateFileMapping2, and the PS4's 16 KB pages in reserved memory: Wine only takes 64 KB). So the game closes right away, with no window. The fix belongs in the ports' engine, not in PORTHEON.\n\nOn Linux, UFC 3 works through GNSB's native build (github.com/GNSB/UFC-3-PS4-PORT-LINUX-WINDOWS).\n\nTry to start it anyway?"),
                        L"PORTHEON", MB_YESNO | MB_ICONINFORMATION);
    if (r != IDYES) { Msg(C_ORANGE, L"%s", L2(L"Linux: o UFC 1 a 4 ainda não roda no Wine/Proton (o motor precisa de funções de memória que o Wine não tem). Veja Sobre ou o LEIA-ME.", L"Linux: UFC 1-4 do not run on Wine/Proton yet (the engine needs memory functions Wine lacks). See About or the README.")); return FALSE; }
    return TRUE;
}

#include "tune62.c"   /* 6.2: effects, the monitor's Hz, the tuning from the sessions */
/* ---------------------------------------------------------------- start and stop */
static DWORD WINAPI WaitThread(LPVOID p)
{
    HANDLE h = (HANDLE)p; WaitForSingleObject(h, INFINITE);
    DWORD code = 0; GetExitCodeProcess(h, &code); PostMessageW(g_wnd, WM_APP_EXITED, code, 0); return 0;
}
/* 4.3.6, UFC 2 online (the user: bring the community package's UFC 2 online into PORTHEON): the package's server,
   games\UFC2\tools\ufc3_server.py --game ufc2 (login, Ultimate Team, Fight Now, Ranked Championships, Online Rivals),
   on the v3.0 engine (WantV30Engine), which reads BB_ONLINE_SERVER = host:port. Like UFC 3: one PC hosts (PORTHEON starts
   the server there, every service on one TCP port, the fight's traffic relayed through it) and the friend joins its
   address. The choice is PORTHEON's (online_UFC2, join_UFC2): UFC 2's own launcher stays "offline", since its "host"
   would start UFC 3's server, and keeps the BB_ONLINE_SERVER it is started with. The server runs in a job that closes
   with PORTHEON, which stays open while it hosts (CloseOnPlay); its log and the Ultimate Team save are in
   %LOCALAPPDATA%\UFC2Native\online. 6.2: UFC 3 the same way, with the same package's server (--game ufc3, its files in
   games\UFC3\tools and UFC3Native\online) on the same engine, instead of UFC3.exe's older server and engine. */
#define UFC2_PORT 42230
static const wchar_t *ProfName(void);   /* extras.c */
static void Ufc2UpnpOpen(void); static void Ufc2UpnpClose(void);   /* online.c (4.3.7) */
static void FixesApply(int g); static void FixWarnings(int g, wchar_t out[1200]); static void StartFixesFetch(void);   /* remote.c (4.3.8) */
static BOOL PcVerdictBox(int g, BOOL atPlay);   /* checks.c (4.3.8) */
static void MakeReport(int g);   /* below (4.3.5); OnExited offers it (4.3.8) */
static void Ufc2ServerStop(void)
{
    if (g_u2srv) Ufc2UpnpClose();   /* 4.3.7: the router's port closes with the room */
    if (g_u2srv) { TerminateProcess(g_u2srv, 0); CloseHandle(g_u2srv); g_u2srv = NULL; }
    if (g_u2job) { CloseHandle(g_u2job); g_u2job = NULL; }
}
static BOOL Ufc2ServerStart(int g)
{
    if (g_u2srv && g_u2game == g && WaitForSingleObject(g_u2srv, 0) == WAIT_TIMEOUT) return TRUE;
    Ufc2ServerStop();
    g_u2game = g;
    wchar_t dir[MAX_PATH], py[MAX_PATH], script[MAX_PATH], data[MAX_PATH], state[MAX_PATH], log[MAX_PATH], cmd[MAX_PATH * 3];
    PortDir(g, dir); _snwprintf(py, MAX_PATH, L"%s\\python\\python.exe", dir); _snwprintf(script, MAX_PATH, L"%s\\tools\\ufc3_server.py", dir);
    LocalData(data, G[g].data); CreateDirectoryW(data, NULL); Join(data, data, L"online"); CreateDirectoryW(data, NULL);
    Join(state, data, L"ufc2-ultimate-team.json"); Join(log, data, L"server.log");
    /* Released (11-13, the user: "the online mode, take it whole", with a player hosting as today): the newer server files
       of the same GPL port (server ufc2-v3.8: FighterNet numbers, a 120 s search, looser Quick Fight pairing) sit beside
       the tested v3.4 ones, in tools-v38 (and tools-v38s, with the Ultimate Team store its authors had not tried in the
       game). online_srv38 in portheon.ini picks them: 0 = the v3.4 files as before (the default until a fight between
       two PCs is seen), 1 = v3.8, 2 = v3.8 with the store. Same command line, port, state file and log; the host's
       saved online data is copied once to online\pre-v38 before the first newer start. */
    {
        int v38 = PIni(L"online_srv38", 0); wchar_t alt[MAX_PATH];
        _snwprintf(alt, MAX_PATH, L"%s\\%s\\ufc3_server.py", dir, v38 == 2 ? L"tools-v38s" : L"tools-v38"); alt[MAX_PATH - 1] = 0;
        if (v38 >= 1 && v38 <= 2 && FileExists(alt)) {
            wcscpy(script, alt);
            wchar_t keep[MAX_PATH]; Join(keep, data, L"pre-v38");
            if (g == 1 && GetFileAttributesW(keep) == INVALID_FILE_ATTRIBUTES && CreateDirectoryW(keep, NULL)) {
                static const wchar_t *const f[] = {L"ufc2-players.json", L"ufc2-accounts.json", L"ufc2-ultimate-team.json"};
                for (int i = 0; i < 3; i++) { wchar_t a[MAX_PATH], b[MAX_PATH]; Join(a, data, f[i]); Join(b, keep, f[i]); if (FileExists(a)) CopyFileW(a, b, TRUE); }
            }
        }
    }
    if (!FileExists(py) || !FileExists(script)) { Msg(C_BAD, L2(L"Falta o servidor online do %s (a pasta tools ou o python do port). Extraia o PORTHEON de novo.", L"%s's online server is missing (the port's tools folder or python). Extract PORTHEON again."), G[g].name); return FALSE; }
    if (g == 1) _snwprintf(cmd, MAX_PATH * 3, L"\"%s\" -u \"%s\" --game ufc2 --bind 0.0.0.0 --single-port %d --state \"%s\"", py, script, UFC2_PORT, state);
    else _snwprintf(cmd, MAX_PATH * 3, L"\"%s\" -u \"%s\" --game ufc3 --bind 0.0.0.0 --single-port %d", py, script, UFC2_PORT);
    cmd[MAX_PATH * 3 - 1] = 0;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE out = CreateFileW(log, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, 0, NULL);
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi = {0};
    if (out != INVALID_HANDLE_VALUE) { si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = out; si.hStdError = out; si.hStdInput = NULL; }
    BOOL ok = CreateProcessW(py, cmd, NULL, NULL, out != INVALID_HANDLE_VALUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, data, &si, &pi);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    if (!ok) { DWORD e = GetLastError(); Msg(C_BAD, L2(L"Não consegui iniciar o servidor online do %s (erro %lu).", L"Could not start %s's online server (error %lu)."), G[g].name, e); return FALSE; }
    g_u2job = CreateJobObjectW(NULL, NULL);
    if (g_u2job) { JOBOBJECT_EXTENDED_LIMIT_INFORMATION li = {0}; li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                   SetInformationJobObject(g_u2job, JobObjectExtendedLimitInformation, &li, sizeof(li)); AssignProcessToJobObject(g_u2job, pi.hProcess); }
    ResumeThread(pi.hThread); CloseHandle(pi.hThread); g_u2srv = pi.hProcess;
    if (WaitForSingleObject(g_u2srv, 2500) != WAIT_TIMEOUT) {   /* it stopped at once: the port is taken, or Python failed (the log says) */
        CloseHandle(g_u2srv); g_u2srv = NULL; Ufc2ServerStop();
        Msg(C_BAD, L2(L"O servidor online do %s fechou ao iniciar (a porta %d pode estar em uso por outro programa). Detalhes: %s", L"%s's online server closed as it started (port %d may be in use by another program). Details: %s"), G[g].name, UFC2_PORT, log);
        return FALSE;
    }
    return TRUE;
}
/* the address the engine of UFC 2 or 3 is given (FALSE: no PLAY); called with the other environment of PLAY */
static BOOL Ufc2OnlineEnv(int g)
{
    int mode = OnlineMode(g); if (!mode) return TRUE;
    wchar_t v[MAX_PATH], cur[16], addr[96]; V30EnginePath(v);
    if (!FileExists(v)) { Msg(C_BAD, L2(L"O online do %s precisa de um arquivo que não está aqui. Extraia o PORTHEON de novo.", L"%s online needs a file that is not here. Extract PORTHEON again."), G[g].name); return FALSE; }
    GetStr(g, L"online_mode", cur, 16, L"offline"); if (wcscmp(cur, L"offline")) PutStr(g, L"online_mode", L"offline");
    /* the name online is the PORTHEON profile's (the port's own default, "Player 1", made every friend Player_1: the
       user's first test, 2026-11-03); the launcher hands it to the game as BB_USER_NAME */
    { wchar_t pl[40]; GetStr(g, L"player", pl, 40, L""); if (ProfName()[0] && wcscmp(pl, ProfName())) PutStr(g, L"player", ProfName()); }
    if (mode == 2) { if (!Ufc2ServerStart(g)) return FALSE; if (PIni(L"ufc2_upnp", 1)) Ufc2UpnpOpen(); _snwprintf(addr, 96, L"127.0.0.1:%d", UFC2_PORT); }   /* 6.1: ufc2_upnp=0 skips PORTHEON's router mapping */
    else {
        wchar_t j[80];
        if (g == 1) GetPrivateProfileStringW(L"PORTHEON", L"join_UFC2", L"", j, 80, g_pIni); else GetStr(2, L"online_server", j, 80, L"");
        if (!j[0]) { Msg(C_ORANGE, L"%s", L2(L"Entrar: digite o endereço que o seu amigo (o anfitrião) passou e aperte JOGAR.", L"Join: type the address your friend (the host) gave you and press PLAY.")); return FALSE; }
        if (wcschr(j, L':')) _snwprintf(addr, 96, L"%s", j); else _snwprintf(addr, 96, L"%s:%d", j, UFC2_PORT);
    }
    addr[95] = 0; EnvPush(L"BB_ONLINE_SERVER", addr);
    return TRUE;
}
/* ---------------------------------------------------------------- 5.2: room and power before PLAY (warnings only)
   What filled up or slowed down a game only showed after a crash: drive C: full while the shader cache was written
   (code 23), too little virtual memory for UFC 4's 6080 MiB of guest memory (error 1450, then code 3), a laptop on
   battery or on the Power saver plan (GPU and CPU clocks at half). Now PLAY says so first. Nothing here stops a game
   unless the player answers No (under 1 GB free). The thresholds are portheon.ini keys (disk_warn_gb, disk_ask_gb,
   commit_ps4_gb, commit_ps5_gb), so a test can raise them. */
static wchar_t g_playWarn[900];
static void PlayWarn(const wchar_t *t)
{
    size_t l = wcslen(g_playWarn); if (l + wcslen(t) + 2 >= 900) return;
    if (l) wcscat(g_playWarn, L" "); wcscat(g_playWarn, t);
}
/* bit 1: on battery; bit 2: Windows' battery/energy saver on; bit 4: the Power saver plan is the active one */
static int PowerState(void)
{
    int s = 0; SYSTEM_POWER_STATUS ps;
    if (GetSystemPowerStatus(&ps)) { if (ps.ACLineStatus == 0) s |= 1; if (ps.SystemStatusFlag == 1) s |= 2; }
    HMODULE pp = LoadLibraryExW(L"powrprof.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (pp) {
        typedef DWORD (WINAPI *PGAS)(HKEY, GUID **); PGAS f = (PGAS)(void *)GetProcAddress(pp, "PowerGetActiveScheme"); GUID *a = NULL;
        static const GUID saver = {0xa1841308, 0x3541, 0x4fab, {0xbc, 0x81, 0xf7, 0x15, 0x56, 0xf2, 0x0b, 0x4a}};   /* GUID_MAX_POWER_SAVINGS */
        if (f && f(NULL, &a) == 0 && a) { if (!memcmp(a, &saver, sizeof(GUID))) s |= 4; LocalFree(a); }
        FreeLibrary(pp);
    }
    return s;
}
static void PagefileOfferEx(int g, BOOL before);
/* Released (11-13): the programs holding the most memory right now, for PlayRoomCheck's question and for the words after
   an out-of-memory end. Private bytes (what a program charges against RAM + page file, the same account the check
   reads), the working set where Windows gives no private figure. Programs of the same name are one line ("chrome
   (23 processes)"); PORTHEON itself, Windows' own pseudo-processes and what Windows will not let be read are left out. */
typedef struct { wchar_t name[64]; ULONGLONG bytes; int n; } MemHog;
typedef struct { DWORD cb, faults; SIZE_T peakWs, ws, qpp, qp, qpnp, qnp, pagefile, peakPagefile, priv; } MemCounters;   /* PROCESS_MEMORY_COUNTERS_EX */
static int MemHogCmp(const void *a, const void *b) { ULONGLONG x = ((const MemHog *)a)->bytes, y = ((const MemHog *)b)->bytes; return x < y ? 1 : x > y ? -1 : 0; }
static int MemHogs(MemHog *out, int max)
{
    typedef BOOL (WINAPI *GPMI)(HANDLE, MemCounters *, DWORD);
    HMODULE ps = NULL; GPMI f = (GPMI)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "K32GetProcessMemoryInfo");
    if (!f) { ps = LoadLibraryExW(L"psapi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32); if (ps) f = (GPMI)(void *)GetProcAddress(ps, "GetProcessMemoryInfo"); }
    HANDLE sn = f ? CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) : INVALID_HANDLE_VALUE;
    if (sn == INVALID_HANDLE_VALUE) { if (ps) FreeLibrary(ps); return 0; }
    static MemHog all[384]; int n = 0; DWORD me = GetCurrentProcessId(); PROCESSENTRY32W pe = {sizeof(pe)};
    static const wchar_t *const skip[] = {L"System", L"Registry", L"Memory Compression", L"Secure System", L"[System Process]", L"System Idle Process", L"Idle"};
    if (Process32FirstW(sn, &pe)) do {
        if (pe.th32ProcessID == 0 || pe.th32ProcessID == 4 || pe.th32ProcessID == me) continue;
        BOOL sys = FALSE; for (int i = 0; i < (int)(sizeof(skip) / sizeof(skip[0])) && !sys; i++) sys = !_wcsicmp(pe.szExeFile, skip[i]);
        if (sys) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID); if (!h) continue;
        MemCounters mc; memset(&mc, 0, sizeof(mc)); mc.cb = sizeof(mc);
        ULONGLONG b = f(h, &mc, sizeof(mc)) ? (mc.priv ? mc.priv : mc.ws) : 0; CloseHandle(h);
        if (!b) continue;
        int k = 0; while (k < n && _wcsicmp(all[k].name, pe.szExeFile)) k++;
        if (k == n) { if (n >= 384) continue; wcsncpy(all[n].name, pe.szExeFile, 63); all[n].name[63] = 0; all[n].bytes = 0; all[n].n = 0; n++; }
        all[k].bytes += b; all[k].n++;
    } while (Process32NextW(sn, &pe));
    CloseHandle(sn); if (ps) FreeLibrary(ps);
    qsort(all, n, sizeof(all[0]), MemHogCmp);
    int m = 0; for (; m < n && m < max && all[m].bytes >= (100ull << 20); m++) out[m] = all[m];   /* under 100 MB is not what fills the memory */
    return m;
}
/* bytes as GB with one decimal, in the language's way ("4,2" in Portuguese) */
static void GbText(ULONGLONG bytes, wchar_t *out, int n)
{
    ULONGLONG t = (bytes * 10 + (1ull << 29)) >> 30;
    _snwprintf(out, n, L"%llu%lc%llu", t / 10, g_lang == LANG_PT ? L',' : L'.', t % 10); out[n - 1] = 0;
}
/* the heaviest programs as text: lines == TRUE one per line for a question ("  - chrome (23 processos): 4,2 GB"),
   else one sentence part ("chrome 4,2 GB, Discord 1,1 GB"); FALSE when nothing heavy could be read */
static BOOL MemHogText(wchar_t *out, int n, BOOL lines)
{
    MemHog h[4]; int k = MemHogs(h, 4), l = 0; out[0] = 0;
    for (int i = 0; i < k && l < n - 110; i++) {
        wchar_t nm[64], gb[24], cnt[48] = L""; wcscpy(nm, h[i].name); GbText(h[i].bytes, gb, 24);
        { size_t e = wcslen(nm); if (e > 4 && !_wcsicmp(nm + e - 4, L".exe")) nm[e - 4] = 0; }
        if (h[i].n > 1) { _snwprintf(cnt, 48, L2(L" (%d processos)", L" (%d processes)"), h[i].n); cnt[47] = 0; }
        l += _snwprintf(out + l, n - l, lines ? L"%s  - %s%s: %s GB" : L"%s%s%s %s GB", lines ? (i ? L"\n" : L"") : (i ? L", " : L""), nm, cnt, gb);
    }
    out[n - 1] = 0; return k > 0;
}
/* the memory free at this moment, as words for a message: RAM, and RAM + page file (what a game's reservation needs) */
static void MemNowText(wchar_t *out, int n)
{
    MEMORYSTATUSEX ms = {sizeof(ms)}; out[0] = 0; if (!GlobalMemoryStatusEx(&ms)) return;
    wchar_t ram[24], com[24]; GbText(ms.ullAvailPhys, ram, 24); GbText(ms.ullAvailPageFile, com, 24);
    _snwprintf(out, n, L2(L"Agora o PC tem %s GB de RAM livre e %s GB de memória livre para programas (RAM + arquivo de paginação).", L"Right now the PC has %s GB of RAM free and %s GB of memory free for programs (RAM + page file)."), ram, com);
    out[n - 1] = 0;
}
/* Released (11-13): memory really short for the game about to start: the question that names the heaviest programs.
   Try again measures again (the player closed something), Continue plays anyway, Cancel does not start the game.
   TRUE = go on. shortGb is the line below which it is asked (PlayRoomCheck). */
static BOOL MemShortAsk(int g, int shortGb, int needFree)
{
    for (;;) {
        MEMORYSTATUSEX ms = {sizeof(ms)}; if (!GlobalMemoryStatusEx(&ms)) return TRUE;
        if (ms.ullAvailPageFile >= ((ULONGLONG)shortGb << 30)) return TRUE;   /* enough now (after a Try again) */
        wchar_t hogs[520], ram[24], com[24], q[1500]; BOOL have = MemHogText(hogs, 520, TRUE);
        GbText(ms.ullAvailPhys, ram, 24); GbText(ms.ullAvailPageFile, com, 24);
        _snwprintf(q, 1500, L2(L"Pouca memória livre para o %s.\n\nO Windows tem só %s GB de memória livre para programas agora (RAM + arquivo de paginação; %s GB de RAM livre). O jogo reserva cerca de %d GB ao abrir e pode fechar na hora (código 3 ou 25).\n\n%s%s%sFeche o que puder e escolha:\nTentar novamente = medir de novo.\nContinuar = jogar mesmo assim.\nCancelar = não abrir o jogo.",
                               L"Little memory free for %s.\n\nWindows has only %s GB of memory free for programs right now (RAM + page file; %s GB of RAM free). The game reserves about %d GB when it starts and may close at once (code 3 or 25).\n\n%s%s%sClose what you can and choose:\nTry again = measure again.\nContinue = play anyway.\nCancel = do not start the game."),
                   G[g].name, com, ram, needFree, have ? L2(L"Programas que mais usam memória agora:\n", L"Programs using the most memory right now:\n") : L"", have ? hogs : L"", have ? L"\n\n" : L"");
        q[1499] = 0;
        int a = MessageBoxW(g_wnd, q, L"PORTHEON", MB_CANCELTRYCONTINUE | MB_ICONWARNING | MB_DEFBUTTON2);
        if (a == IDCONTINUE) return TRUE;
        if (a != IDTRYAGAIN) { Msg(C_ORANGE, L2(L"O %s não foi aberto: feche programas pesados (navegador, Discord) e aperte JOGAR de novo.", L"%s was not started: close heavy programs (browser, Discord) and press PLAY again."), G[g].name); return FALSE; }
    }
}
static ULONGLONG FreeGbAt(const wchar_t *dir) { ULARGE_INTEGER fb = {0}; return dir[0] && GetDiskFreeSpaceExW(dir, &fb, NULL, NULL) ? fb.QuadPart >> 30 : 0xffffffffull; }
static BOOL PlayRoomCheck(int g)   /* FALSE only when the player chose not to play */
{
    /* the disk the game writes to: %LOCALAPPDATA% for UFC 1-4 (shader cache, logs, the prepared eboot), the port's own
       folder for UFC 5, 6 and Undisputed 3 (their bin\_Logs and caches) */
    wchar_t d[MAX_PATH] = L"";
    if (PS4G(g)) GetEnvironmentVariableW(L"LOCALAPPDATA", d, MAX_PATH); else PortDir(g, d);
    ULONGLONG fr = FreeGbAt(d); wchar_t t[600], drv[4] = L"C:"; if (d[0] && d[1] == L':') { drv[0] = d[0]; }
    if (fr < (ULONGLONG)PIni(L"disk_ask_gb", 1)) {
        _snwprintf(t, 600, L2(L"O disco %s tem menos de 1 GB livre (%llu GB). O %s grava ali o cache de shaders e os logs: com o disco cheio ele fecha no meio da luta (código 23).\n\nJogar mesmo assim?",
                              L"Drive %s has less than 1 GB free (%llu GB). %s writes its shader cache and logs there: with the drive full it closes in the middle of a fight (code 23).\n\nPlay anyway?"), drv, fr, G[g].name); t[599] = 0;
        if (MessageBoxW(g_wnd, t, L"PORTHEON", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
            Msg(C_BAD, L2(L"Libere espaço no disco %s (5 GB ou mais) e jogue de novo.", L"Free space on drive %s (5 GB or more) and play again."), drv); return FALSE;
        }
    } else if (fr < (ULONGLONG)PIni(L"disk_warn_gb", 5)) {
        _snwprintf(t, 600, L2(L"Pouco espaço no disco %s (%llu GB livres): libere 5 GB ou mais, ou o jogo pode fechar ao gravar o cache de shaders.", L"Little room on drive %s (%llu GB free): free 5 GB or more, or the game may close while it writes its shader cache."), drv, fr);
        t[599] = 0; PlayWarn(t);
    }
    /* the commit limit (RAM + page file): only with a small page file, where PagefileOffer can help. 5.2: once per game
       for each RAM size (commit_offer_<id> = RAM in GB): a system-managed page file makes the commit limit itself grow
       and shrink between boots, so keyed on it the offer came back at many PLAYs */
    if (PS4G(g) || G[g].ps5) {
        MEMORYSTATUSEX ms = {sizeof(ms)};
        if (GlobalMemoryStatusEx(&ms)) {
            ULONGLONG commit = ms.ullTotalPageFile >> 30, page = ms.ullTotalPageFile > ms.ullTotalPhys ? (ms.ullTotalPageFile - ms.ullTotalPhys) >> 30 : 0;
            int need = G[g].ps5 ? PIni(L"commit_ps5_gb", 20) : PIni(L"commit_ps4_gb", 12), ramGb = (int)((ms.ullTotalPhys + (512ull << 20)) >> 30);
            BOOL offered = FALSE;
            if (commit < (ULONGLONG)need && page < 15 && GIni(L"commit_offer", g, -1) != ramGb) { GIniSet(L"commit_offer", g, ramGb); PagefileOfferEx(g, TRUE); offered = TRUE; }
            /* 6.1 (Bloodborne's launcher does the same): the memory free for programs RIGHT NOW (RAM + page file minus what
               is in use). A browser holding the commit makes the game fail to reserve its memory at once (UFC 4: 6080 MiB
               of guest memory, log of 2026-10-26; code 3). A system-managed page file can still grow, so this is only a
               warning. Not after the offer above (one warning is enough). 6.1 review: once per PC for each RAM size
               (free_warn_ram = RAM in GB in portheon.ini; 0 there says it again), not at every PLAY */
            ULONGLONG freeNow = ms.ullAvailPageFile >> 30; int needFree = G[g].ps5 ? PIni(L"commit_free_ps5_gb", 12) : PIni(L"commit_free_ps4_gb", 7);
            /* Released (11-13): really short (under half of that line; commit_short_ps4_gb / commit_short_ps5_gb in
               portheon.ini move it, so a test can raise it): the question with the heaviest programs' names, until Continue is answered once in this run of PORTHEON, at every
               PLAY it is so (MemShortAsk: try again, continue, cancel). Between the two lines the warning stays a line
               of text once per RAM size, now with the names too. */
            int shortGb = G[g].ps5 ? PIni(L"commit_short_ps5_gb", needFree / 2) : PIni(L"commit_short_ps4_gb", needFree / 2);
            static unsigned memGo;   /* review: one bit per game, set when the player answered Continue: asked once per session, a text line after that */
            if (ms.ullAvailPageFile < ((ULONGLONG)shortGb << 30) && g < 32 && (memGo & (1u << g))) { wchar_t com[24]; GbText(ms.ullAvailPageFile, com, 24); _snwprintf(t, 600, L2(L"Pouca memória livre para o %s (%s GB livres para programas): se o jogo fechar ao abrir, feche o navegador e outros programas pesados.", L"Little memory free for %s (%s GB free for programs): if the game closes when it starts, close the browser and other heavy programs."), G[g].name, com); t[599] = 0; PlayWarn(t); }
            else if (ms.ullAvailPageFile < ((ULONGLONG)shortGb << 30)) { if (!MemShortAsk(g, shortGb, needFree)) return FALSE; if (g < 32 && GlobalMemoryStatusEx(&ms) && ms.ullAvailPageFile < ((ULONGLONG)shortGb << 30)) memGo |= 1u << g; }
            else if (!offered && freeNow < (ULONGLONG)needFree && PIni(L"free_warn_ram", 0) != ramGb) {
                PIniSet(L"free_warn_ram", ramGb);
                _snwprintf(t, 600, L2(L"O Windows tem só %llu GB de memória livre para programas agora (RAM + arquivo de paginação); o %s reserva cerca de %d GB ao abrir e pode fechar na hora (código 3). Feche o navegador e outros programas pesados antes de jogar.",
                                      L"Windows has only %llu GB of memory free for programs right now (RAM + page file); %s reserves about %d GB when it starts and may close at once (code 3). Close the browser and other heavy programs before playing."),
                           freeNow, G[g].name, needFree);
                t[599] = 0; PlayWarn(t);
                { wchar_t hogs[300]; if (MemHogText(hogs, 300, FALSE)) { _snwprintf(t, 600, L2(L"Mais usam memória agora: %s.", L"Using the most memory now: %s."), hogs); t[599] = 0; PlayWarn(t); } }
            }
        }
    }
    /* 5.2: UFC 6 below 4 GB of video memory (its 8 GB profile fills almost 7) */
    if (UFC6G(g) && g_haveGpu && g_gpu.vram && (g_gpu.vram >> 20) < 3800) {
        _snwprintf(t, 600, L2(L"A placa tem só %llu MB de memória de vídeo: o UFC 6 pode fechar nas trocas de cena. Use o perfil Rápido.", L"The card has only %llu MB of video memory: UFC 6 may close at scene changes. Use the Fast profile."), (unsigned long long)(g_gpu.vram >> 20));
        t[599] = 0; PlayWarn(t);
    }
    /* 7.1 (ufc1-4 Check, U3-VRAM: a GTX 1050 with 2 GB ran out of video memory in UFC 3's first fight, already in Ultra
       performance; the 11-14 UFC 2 fights used about 2.5 GB on a 4 GB card): a 2 GB card is told before PLAY. Not an
       Intel card (its memory is shared, the figure says little). 7.1 review (player#10): nor an integrated GPU of any
       maker (Vulkan device type 1: an AMD APU's device-local heap is only the BIOS carve-out, shared memory too) */
    if (g >= 1 && PS4G(g) && g_haveGpu && g_gpu.vram && (g_gpu.vram >> 20) < 2600 && !GpuIsIntel() && g_gpu.type != 1) {
        _snwprintf(t, 600, L2(L"A placa tem só %llu MB de memória de vídeo: nas lutas o %s pode passar disso e fechar com o código 23. Use o modo Ultra desempenho e feche o navegador, o Discord e gravadores de tela enquanto joga.",
                              L"The card has only %llu MB of video memory: in fights %s can need more and close with code 23. Use Ultra performance mode and close the browser, Discord and screen recorders while you play."),
                   (unsigned long long)(g_gpu.vram >> 20), G[g].name);
        t[599] = 0; PlayWarn(t);
    }
    /* 5.2 (F3): a laptop on battery, or the Power saver plan: said, never switched (with close_on_play nothing could
       give the player's plan back) */
    if (PS4G(g)) {   /* max-perf 11-13: the last session's engine named another card than the best one (MpEngineGpu) */
        wchar_t k[48], other[200]; _snwprintf(k, 48, L"gpu_other_%s", G[g].id); k[47] = 0; GetPrivateProfileStringW(L"PORTHEON", k, L"", other, 200, g_pIni);
        if (other[0] && g_haveGpu) { wchar_t t[600]; _snwprintf(t, 600, L2(L"Na última vez o jogo rodou na placa \"%s\" e não na \"%s\". Em Configurações do Windows > Sistema > Tela > Elementos gráficos, escolha \"Alto desempenho\" para o jogo.",
                                                                           L"Last time the game ran on the \"%s\" card, not on the \"%s\". In Windows Settings > System > Display > Graphics, choose \"High performance\" for the game."), other, g_gpu.name); t[599] = 0; PlayWarn(t); }
    }
    int pw = PowerState();
    if (pw & 1) PlayWarn(L2(L"Notebook na bateria: ligue o carregador (na bateria a placa e o processador rodam bem mais devagar).", L"Laptop on battery: plug in the charger (on battery the graphics card and the processor run much slower)."));
    else if (pw & 2) PlayWarn(L2(L"A economia de bateria do Windows está ligada: desligue-a para o jogo ter o desempenho todo.", L"Windows' battery saver is on: turn it off so the game gets full performance."));
    if (pw & 4) {
        long long day = UnixNow() / 86400;
        if (PIni(L"power_warned_day", 0) != (int)day) {
            PIniSet(L"power_warned_day", (int)day);
            if (MessageBoxW(g_wnd, L2(L"O plano de energia do Windows é \"Economia de energia\": nele o processador e a placa de vídeo rodam bem mais devagar e o jogo perde FPS.\n\nSim: abrir as Opções de energia para escolher \"Equilibrado\" ou \"Alto desempenho\" (o jogo abre também).   Não: jogar assim.",
                                      L"Windows' power plan is \"Power saver\": on it the processor and the graphics card run much slower and the game loses FPS.\n\nYes: open Power Options to pick \"Balanced\" or \"High performance\" (the game starts too).   No: play like this."),
                            L"PORTHEON", MB_YESNO | MB_ICONINFORMATION) == IDYES)
                ShellExecuteW(g_wnd, L"open", L"control.exe", L"powercfg.cpl", NULL, SW_SHOWNORMAL);
        } else PlayWarn(L2(L"Plano de energia \"Economia de energia\": o jogo perde FPS.", L"\"Power saver\" power plan: the game loses FPS."));
    }
    return TRUE;
}
static BOOL StartPort(int g, BOOL play)
{
    if (SOON(g)) return FALSE;   /* 6.2 */
    wchar_t exe[MAX_PATH], dir[MAX_PATH], cmd[2 * MAX_PATH + 96]; PortExe(g, exe); PortDir(g, dir);
    if (!EXTG(g) && !FileExists(exe)) { Msg(C_BAD, T(S_NOTFOUND), G[g].exe); return FALSE; }   /* 6.0: Zelda's engine is chosen in ExtCommand */
    int arcEngine = 0; BOOL legacy = FALSE;
    if (play) g_playWarn[0] = 0;
    if (play && PS4G(g)) {
        wchar_t miss[64];
        if (CpuLacksPs4(miss, 64)) {
            /* 4.0: an Intel Core of the 2nd or 3rd generation has AVX but not BMI1, MOVBE and LZCNT: PORTHEON's
               compatibility mode translates those instructions of the game for it (checks.c, CpuLegacyOk) */
            if (!CpuLegacyOk(g)) { CpuNoMessage(g, miss); return FALSE; }
            legacy = TRUE;
        }
        /* 4.0: Windows, the video driver and every file the engine loads (checks.c), then the game's own files */
        if (!PcReady(g)) return FALSE;
        if (!GameFilesReady(g)) return FALSE;
        SanitizePs4(g);
        /* 4.3.1: a port that has no game language yet takes PORTHEON's (the port's launcher writes its own default,
           English, at the first start: UFC 1, 3 and 4 opened in English for a Portuguese player, 2026-10-29) */
        { wchar_t lang[8]; GetStr(g, L"language", lang, 8, L""); if (!lang[0]) { g_kvGame = -1; PutInt(g, L"language", LANGS[g_lang].ps4); } }
        if (g == 3) Ufc4Prepare();
        else { FaceFix(g); SceneRes(g); SyncExperimentPatches(g); WriteOwnPatches(g); }   /* 6.0 final: SceneRes, the 3D scene's size */
        WriteCam3p(g);   /* 4.3.2: after Ufc4Prepare, which switches every XML of UFC 4 off */
        if (IntelGuard(g)) Msg(C_SUB, L"%s", L2(L"Placa Intel: modo PS4 Pro, 2K/4K e mais de 60 FPS foram desligados (travavam a placa, código 23). Se o jogo fechar com código 3, atualize o driver da Intel.",
                                               L"Intel graphics: PS4 Pro mode, 2K/4K and more than 60 FPS were turned off (they stopped the GPU, code 23). If the game closes with code 3, update the Intel driver."));
        PresentStretch(g, FALSE);   /* Released: the engine file as it ships, before it is compared */
        arcEngine = EnsureEngine(g);
        if (arcEngine >= 0 && Wide21On(g) && !PresentStretch(g, TRUE))   /* real 21:9: the picture fills the wide window */
            Msg(C_SUB, L"%s", L2(L"21:9: não consegui preparar o motor para preencher a tela; a imagem vai sair com barras laterais.", L"21:9: could not prepare the engine to fill the screen; the picture will have side bars."));
        if (arcEngine < 0) { Msg(C_BAD, L2(L"Não consegui trocar o motor de vídeo do %s (o jogo ou um antivírus está usando o arquivo). Feche o jogo e tente de novo.", L"Could not switch the video engine of %s (the game or an antivirus holds the file). Close the game and try again."), G[g].name); return FALSE; }
        GIniSet(L"engine_ran", g, arcEngine);   /* 5.2: which engine this session runs (OnExited, LateExited, the report) */
        /* 5.2: an Intel processor with an Intel card keeps the new engine for its Arc fixes, but that engine lacks the
           SSE4a patch of the port's own: said once; a crash on those instructions moves the game to the port's own */
        if (arcEngine == 1 && (g == 2 || g == 3) && !CpuHasSse4a() && !GIni(L"sse4a_told", g, 0)) {
            GIniSet(L"sse4a_told", g, 1);
            wchar_t q[700]; _snwprintf(q, 700, L2(L"Processador Intel com placa Intel: o %s usa o motor novo, que tem as correções da Intel Arc, mas não traduz duas instruções que só os processadores AMD têm (SSE4a).\n\nSe o jogo fechar com o código 3 (\"0xc000001d\" no log), o PORTHEON passa sozinho para o motor do próprio port no próximo JOGAR.",
                                                  L"Intel processor with an Intel card: %s uses the new engine, which has the Intel Arc fixes but does not translate two instructions only AMD processors have (SSE4a).\n\nIf the game closes with code 3 (\"0xc000001d\" in the log), PORTHEON moves to the port's own engine by itself at the next PLAY."), G[g].name);
            q[699] = 0; MessageBoxW(g_wnd, q, L"PORTHEON", MB_OK | MB_ICONINFORMATION);
        }
        /* 3.5.3: every file the port needs, not only the engine (PortFileProblem) */
        { wchar_t which[MAX_PATH]; int prob = PortFileProblem(g, which, MAX_PATH); if (prob) { AntivirusHelp(g, prob, which, TRUE); return FALSE; } }
        if (!LinuxNotice(g)) return FALSE;
    }
    if (play && X360G(g) && !X3FilesReady(g)) return FALSE;   /* 5.2: UFC Undisputed 3's files and imports (checks.c) */
    if (play && !PlayRoomCheck(g)) return FALSE;              /* 5.2: disk, virtual memory, power (warnings) */
    /* 4.0.2: UFC 5's launcher starts the game with no window of its own (--headless). 4.2: UFC6.exe the same, told the
       interface's language for the texts it leaves in UFC6.ini */
    if (play && OWNINI(g)) _snwprintf(cmd, MAX_PATH + 64, L"\"%s\" --play --headless --ui-lang %s", exe, LANGS[g_lang].code);
    else if (X360G(g)) {   /* 5.0: UFC Undisputed 3: UFC3.exe --play (it waits for the game) or --configure, with the game folder */
        wchar_t gd[MAX_PATH]; GameDir(g, gd); X3Normalize(gd); StripSlash(gd); { size_t l = wcslen(gd); if (l && gd[l - 1] == L'\\' && l < MAX_PATH - 1) wcscat(gd, L"."); }   /* "D:\" would escape the quote */
        if (gd[0]) _snwprintf(cmd, 2 * MAX_PATH + 96, L"\"%s\" %s --game \"%s\"", exe, play ? L"--play" : L"--configure", gd);
        else _snwprintf(cmd, 2 * MAX_PATH + 96, L"\"%s\" %s", exe, play ? L"--play" : L"--configure");
        cmd[2 * MAX_PATH + 95] = 0;
    }
    else if (EXTG(g)) { if ((play && !ExtPreflight(g)) || !ExtCommand(g, play, exe, dir, cmd, 2 * MAX_PATH + 96)) { EnvPopAll(); return FALSE; } }   /* 6.0: extgames.c (6.1: ExtPreflight) */
    else _snwprintf(cmd, MAX_PATH + 32, play ? L"\"%s\" --play" : L"\"%s\"", exe);
    if (play) AllowSetForegroundWindow(ASFW_ANY);   /* so the game window can take the focus from PORTHEON (3.5.1: UFC 1-4 too) */
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi = {0};
    /* UFC 1: the fight's compute shader 0xb61e59a3b11b03ad is fixed by a corrected SPIR-V in the engine's shader patch
       folder, so it is no longer skipped. The engine runs on the classic single-threaded GPU path: with the Vulkan
       recording threads and the draw pipe, a fight froze when a submission handed to them never reached the GPU while
       the GPU command thread waited for it inside a readback (logs of 2026-10-19). ufc1_stable_engine=0 turns it off. */
    static const wchar_t *const envK[] = {L"BB_VK_RECORD_THREAD", L"BB_DRAW_PIPE", L"BB_READBACKS"}, *const envV[] = {L"0", L"0", L"1"};
    if (g == 0 && play) InstallUfc1ShaderPatches();
    BOOL stable = g == 0 && PIni(L"ufc1_stable_engine", 1);
    int nenv = g == 0 && PIni(L"ufc1_readbacks_relaxed", 0) ? 3 : 2;
    wchar_t oldV[3][256]; DWORD had[3] = {0, 0, 0};
    if (stable) for (int i = 0; i < nenv; i++) { had[i] = GetEnvironmentVariableW(envK[i], oldV[i], 256); SetEnvironmentVariableW(envK[i], envV[i]); }
    FILETIME started; GetSystemTimeAsFileTime(&started);
    /* UFC 2 on Intel graphics: the GPU hung on the pixel shader that draws the first-start video (/movie/ftue/,
       ps 0x10ed0260, code 23). ufc2_skip_ftue=0 in portheon.ini plays them. */
    BOOL skipFtue = play && g == 1 && GpuIsIntel() && PIni(L"ufc2_skip_ftue", 1);
    if (skipFtue) SetEnvironmentVariableW(L"BB_SKIP_MOVIES_MATCH", L"/movie/bootflow/,/movie/ftue/");
    /* UFC 2, 3 and 4 Career (3.3): write windows off (BB_WRITE_WINDOW=0), the UFC 3 port author's advice for crashes
       loading the next Career fight, and the cure for UFC 4's FPS falling after sparring. 3.5: two UFC 4 crash logs of
       2026-10-22 ("Guest fault ... at guest offset 0x12a58fd", RTX 4090 and MX450) had them on: both players had started
       UFC4.exe by itself, whose default is on. The 3.5 launchers of UFC 2, 3 and 4 default to off too, so PORTHEON says
       it either way: 0 with career_safe (the default), 1 only when it is turned off in Extras. */
    BOOL careerSafe = (g == 1 || g == 2) ? PIni(L"career_safe", 1) : g == 3 ? PIni(L"career_safe_ufc4", 1) : TRUE;
    wchar_t oldWw[16]; DWORD hadWw = 0; BOOL setWw = play && g >= 1 && g <= 3;
    if (setWw) { hadWw = GetEnvironmentVariableW(L"BB_WRITE_WINDOW", oldWw, 16); SetEnvironmentVariableW(L"BB_WRITE_WINDOW", careerSafe ? L"0" : L"1"); }
    wchar_t skipF[MAX_PATH] = L""; BOOL skipOn = FALSE;
    /* UFC 3 on Intel Arc (B580, driver 101.9150, log of 2026-10-22 00:12): the GPU got stuck on pixel shader a0caf33d
       in a Career training session (code 23). Intel cards start with it already skipped. */
    if (play && g == 2 && GpuIsIntel()) SkipFileEnsure(g, "00000000a0caf33d");
    /* 6.0 (players' reports of 2026-11-03..06: RX 5500 XT, RX 5600 XT x4 sessions, RX 5700 + the deepseek report): UFC 4 on
       every Radeon RX 5000 (RDNA1) stops the GPU on pixel shader f74dbf6a (one pass of a 6744-index mesh; the stall once
       recovered: a very long shader, not a fault), on the port's own engine, the new one and v3.0 alike, with
       alpha-to-coverage already on. The remote line matched "rx 5700" only and the learner refuses draws of 3000+ indices,
       so the others looped on "code 23, nothing left to change": these cards start with it skipped */
    if (play && g == 3 && GpuIsRdna1() && PIni(L"ufc4_rdna1_skip", 1)) {
        SkipFileEnsure(g, "00000000f74dbf6a");
        /* 6.1 (proxima-versao-relatorios-1108 ufc4-rx5500xt): the second RDNA1 hang, ps 10de37cd in a 4x-MSAA scene early
           in a fight, learned on an RX 5500 XT on 26.10.1: every RX 5000 starts with it skipped too */
        SkipFileEnsure(g, "0000000010de37cd");
    }
    if (play && !X360G(g)) FixesApply(g);   /* 4.3.8: the remote fix list's shaders for this card */
    if (play && PS4G(g) && G[g].data) { SkipFilePath(g, skipF); skipOn = FileExists(skipF); if (skipOn) SetEnvironmentVariableW(L"BB_SKIP_FILE", skipF); }
    if (play && PS4G(g)) { PadEnv(g); CpuEnv(g); GpuSafeEnv(g); SkinEnv(g); AsyncEnv(g); ProfileEnv(g); CheatsPrepare(g); if (legacy) LegacyEnv(g); }
    if (play && (g == 1 || g == 2) && !Ufc2OnlineEnv(g)) {   /* 4.3.6: what was set for this PLAY goes back */
        EnvPopAll(); if (skipOn) SetEnvironmentVariableW(L"BB_SKIP_FILE", NULL);
        if (setWw) SetEnvironmentVariableW(L"BB_WRITE_WINDOW", hadWw && hadWw < 16 ? oldWw : NULL);
        if (skipFtue) SetEnvironmentVariableW(L"BB_SKIP_MOVIES_MATCH", NULL);
        return FALSE;
    }
    /* 6.1: UFC 2 on the v3.0 engine (online, or Extras "Motor v3.0"): deferred image readbacks, as the v3.x launcher plays
       it (only the thread that touches a pending readback waits, not the GPU; reads still see the GPU's result). Its
       author: the tutorial fight on a Radeon Vega M GL at 1080p went from 26 to 37 FPS. The port's own UFC 2 engine does
       not have it. Log: "GPU: deferred image readbacks (BB_DEFERRED_READBACK=1)". ufc2_deferred_readback=0 turns it off;
       a value the player set in the environment wins. 6.1 review (owner's decision): this and BB_TOUCH_COMMIT=0 below stay
       the defaults for UFC 2 on the v3.0 engine, as the engine author's own launcher sets them; UFC 2 online is said to
       be "em teste" (in testing) in the news and the README. */
    if (play && g == 1 && WantV30Engine(1)) {
        wchar_t dr[8];
        if (!GetEnvironmentVariableW(L"BB_DEFERRED_READBACK", dr, 8)) EnvPush(L"BB_DEFERRED_READBACK", PIni(L"ufc2_deferred_readback", 1) ? L"1" : L"0");
    }
    /* 6.1: UFC 4 "as UFC4-Native-Plus plays it" (the v3.x launcher: completed-work GPU fences, UFC 3's shader patches
       off). OFF by default on every engine: the v3.x package says its UFC 4 was never run, and no UFC 4 dump has been
       tested with them in PORTHEON (FPS, the RX 5000 f74dbf6a hang and career stability are unknown either way).
       ufc4_native_plus=1 in portheon.ini turns them on for a test; the log then says "GPU: fences written when the GPU
       has finished the work before them (BB_HONEST_LABELS=1)". Never for UFC 3 (BB_PATCH_SHADERS carries its fixes). */
    if (play && g == 3 && PIni(L"ufc4_native_plus", 0)) { EnvPush(L"BB_HONEST_LABELS", L"1"); EnvPush(L"BB_PATCH_SHADERS", L"0"); }
    /* 6.1: UFC 2 online closed at the login (guest fault, code 3, at 0x63731b / 0x637180: logs of 2026-11-08 02:27-02:30,
       and 11-03 07:51, and v2.1 offline 10-29 21:52). That is zlib's inflate_fast, which keeps its locals in the System V
       red zone below rsp. The v3.0 engine commits guest memory on first touch (BB_TOUCH_COMMIT, ~15,000 handled faults
       per run). Windows writes each exception frame right below rsp and wipes those locals. Committed as the game maps it
       (what PORTHEON's own engines do): no fault and no crash (2 of 2 runs on 10-29 vs 6 of 11 with touch commit on).
       UFC 2 and, from 6.2, UFC 3 (same engine, same zlib; UFC 4 on "Motor v3.0" was never tested with it). v30_touch_commit=1 in
       portheon.ini goes back; a BB_TOUCH_COMMIT the player set stays. */
    if (play && (g == 1 || g == 2) && WantV30Engine(g) && !PIni(L"v30_touch_commit", 0)) {
        wchar_t tc[8]; if (!GetEnvironmentVariableW(L"BB_TOUCH_COMMIT", tc, 8)) EnvPush(L"BB_TOUCH_COMMIT", L"0");
    }
    /* 4.1: RivaTuner's Vulkan layer (MSI Afterburner's overlay) faulted inside UFC 4 (RTSSVkLayer64.dll+0x2a01, RTX 3070
       Ti, log of 2026-10-27): every game starts without it, as Godot's editor does. rtss_layer_off=0 keeps it. */
    /* max-perf 11-13: for UFC 1-4 also Epic's overlay layer, as UFC5.exe and UFC6.exe already do for their engines (it cannot
       attach to a game that is not an EOS one; a glob in the loader's own list, loader 1.3.234+, older loaders ignore the
       whole variable and keep DISABLE_RTSS_LAYER). keep_overlays=1 keeps it. reshade_off=1 in portheon.ini (a test, off by
       default) also leaves ReShade's global layer out of these engines, as UFC6.exe does for its own. */
    if (play && !X360G(g) && PIni(L"rtss_layer_off", 1)) {
        EnvPush(L"DISABLE_RTSS_LAYER", L"1");
        EnvPush(L"VK_LOADER_LAYERS_DISABLE", !PS4G(g) || GetInt(g, L"pini:keep_overlays", 0) ? L"VK_LAYER_RTSS" : PIni(L"reshade_off", 0) ? L"VK_LAYER_RTSS,*EOS_Overlay*,*EOSOverlay*,*reshade*,*ReShade*" : L"VK_LAYER_RTSS,*EOS_Overlay*,*EOSOverlay*");
    }
    if (play && !X360G(g) && !EXTG(g)) { HzApply(g); LearnEnv(g); }   /* 6.2 */
    if (play && PS4G(g)) {   /* 6.2: the engine's own texture filter, ultrawide window and HDR */
        if (GetInt(g, L"pgame:ultrawide", 0)) EnvPush(L"BB_WINDOW_SIZE", L"3440x1440");
        else if (Wide21On(g) && !GetInt(g, L"fullscreen", 1) && !GetEnvironmentVariableW(L"BB_WINDOW_SIZE", NULL, 0)) EnvPush(L"BB_WINDOW_SIZE", Wide21Window());   /* Released: a 21:9 window */
        if (GetInt(g, L"pgame:hdr", 0)) EnvPush(L"BB_HDR", L"1");
        {   static const wchar_t *const tv[5] = {NULL, L"fsr3", L"fsr4", L"fsr411", L"taa"}; int t = GetInt(g, L"pgame:tups", 0);   /* Released */
            if (t >= 1 && t <= 4 && !GetEnvironmentVariableW(L"BB_UPSCALER", NULL, 0)) EnvPush(L"BB_UPSCALER", tv[t]); }
    }
    if (play && g >= 1 && g <= 3 && GetInt(g, L"pgame:a2c", 0)) EnvPush(L"BB_ALPHA_TO_MASK", L"coverage");   /* Gpu23Escalate */
    if (play && PS4G(g)) {   /* Released: the FPS limits the ports' launchers do not have in their own table go straight to the engine */
        int fl = GetInt(g, L"fps_limit", 60);
        if ((fl == 75 || fl == 100 || fl == 165 || fl == 180) && !GetEnvironmentVariableW(L"BB_FPS_LIMIT", NULL, 0)) { wchar_t fv[8]; _snwprintf(fv, 8, L"%d", fl); fv[7] = 0; EnvPush(L"BB_FPS_LIMIT", fv); }
    }
    if (play && g >= 0 && g <= 2) { NpOptsEnv(g); Pad2Apply(g); }   /* Released: Native Plus's advanced options and Player 2's controller, UFC 1-3 (the last BB_ variables set) */
    if (play && !X360G(g)) FsrEnv(g);   /* 5.0: FSR 1; 6.2: the texture quality */
    if (play && (PS4G(g) || G[g].ps5)) PadMapEnv(g);   /* 6.2: the player's button layout */
    /* 5.2: Steam's implicit Vulkan layers off for the games (keep_overlays=1 keeps them). Fossilize records every
       pipeline the game creates, and an emulator creates thousands, each one a disk write while the game compiles;
       the overlay hooks every present. When PORTHEON runs from Steam (SteamAppId set) the player chose the overlay:
       only Fossilize goes off then. OBS's capture layer stays (players record and stream). */
    if (play && !GetInt(g, L"pini:keep_overlays", 0)) {
        EnvPush(L"DISABLE_VK_LAYER_VALVE_steam_fossilize_1", L"1");
        if (!GetEnvironmentVariableW(L"SteamAppId", NULL, 0)) EnvPush(L"DISABLE_VK_LAYER_VALVE_steam_overlay_1", L"1");
    }
    if (play) PadPause(TRUE);   /* 4.0: PORTHEON lets go of the controllers while a game runs (pad.c) */
    if (play) EnvPush(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", NULL);   /* 6.2: the vitrine's browser switches are not the game's */
    if (play && G[g].ps5) PutStr(g, L"last_code", NULL);   /* 4.3.6: the launcher writes it at the end (LateExited). 6.0 final: before the launcher starts and reads its ini */
    /* 6.0 final: the native Ocarina build is a console program: no console window over the game (it then logs to its
       logs\totk.log by itself) */
    if (play) MpPlayRecord(g);   /* max-perf 11-13: what this PLAY applies, one line each (play-<game>.txt); the GPU preference of UFC 1-4's engine */
    DWORD cflags = play && ZELDAG(g) && ZeldaEngine() == 3 ? CREATE_NO_WINDOW : 0;
    BOOL okp = CreateProcessW(exe, cmd, NULL, NULL, FALSE, cflags, NULL, dir, &si, &pi); DWORD perr = GetLastError();
    if (okp && play && (PS4G(g) || X360G(g))) { DWORD_PTR vm = VCacheMask(); if (vm) SetProcessAffinityMask(pi.hProcess, vm); }   /* 5.0: the V-Cache die (its children inherit it) */
    if (!okp && play) { PadPause(FALSE); Ufc2ServerStop(); HzRestore(); }
    EnvPopAll();
    if (skipOn) SetEnvironmentVariableW(L"BB_SKIP_FILE", NULL);
    if (setWw) SetEnvironmentVariableW(L"BB_WRITE_WINDOW", hadWw && hadWw < 16 ? oldWw : NULL);
    if (skipFtue) SetEnvironmentVariableW(L"BB_SKIP_MOVIES_MATCH", NULL);
    if (stable) for (int i = 0; i < nenv; i++) SetEnvironmentVariableW(envK[i], had[i] && had[i] < 256 ? oldV[i] : NULL);
    /* 225/226: an antivirus refused to run the file or removed it (a false positive is common with programs that have
       no digital signature, like the ports) */
    if (!okp && (perr == ERROR_VIRUS_INFECTED || perr == ERROR_VIRUS_DELETED)) {
        Msg(C_BAD, L2(L"O antivírus bloqueou o %s (erro %lu). Programas sem assinatura digital, como os ports, às vezes são marcados por engano. Restaure o arquivo na quarentena do antivírus e marque como permitido; o código-fonte do PORTHEON está na pasta src.",
                      L"An antivirus blocked %s (error %lu). Programs without a digital signature, like the ports, are sometimes flagged by mistake. Restore the file from the antivirus quarantine and allow it; PORTHEON's source code is in the src folder."), G[g].exe, perr);
        return FALSE;
    }
    if (!okp) { Msg(C_BAD, T(S_CANTSTART), G[g].exe, perr); return FALSE; }
    if (play) {
        g_proc = pi.hProcess; g_procGame = g; g_playStart = GetTickCount64();
        RunningMark(g);   /* 5.2: cleared at the end; still set at the next start = the session never ended (RunningLeft) */
        CloseHandle(CreateThread(NULL, 0, GOW3G(g) || NGPORTG(g) ? WaitTreeThread : WaitThread, g_proc, 0, NULL));   /* 6.0: God of War III's launcher starts the game as its child (6.1: Bloodborne's, inFAMOUS's too) */
        /* 4.3.2: nothing of PORTHEON ticks while the game runs ("only the game"): the 30-a-second effects timer (Animate
           and PadPoll already returned at once during a game) and the 6-hour update check; OnExited starts them again */
        KillTimer(g_wnd, 7); KillTimer(g_wnd, 9); VitStop(); YtDestroy();   /* 6.2: the vitrine stops and lets go of its video; YouTube's web view closes (its processes end) */
        if (g_view == V_LAUNCH) SetTimer(g_wnd, 13, 50, NULL);   /* 4.4.0: only the loading ring moves (LaunchTick) */
        MfgStart(g);   /* 4.3.3: the player's frame generator, when the switch is on. 4.4.0: before SelfQuiet, or it was
                          born below normal priority (a program started by a below-normal one inherits it) */
        SelfQuiet(TRUE);   /* 4.3.3 */
        LONG session = InterlockedIncrement(&g_watchSession); InterlockedExchange(&g_watchStop, 0);
        if (PS4G(g)) {   /* UFC 1-4: catch a freeze of the engine */
            WatchArgs *wa = (WatchArgs *)malloc(sizeof(WatchArgs));
            if (wa) { wa->game = g; wa->start = started; wa->session = session; g_frozenFile[0] = 0;
                      HANDLE th = CreateThread(NULL, 0, FreezeWatch, wa, 0, NULL); if (th) CloseHandle(th); else free(wa); }
        }
        /* 5.2: UFC Undisputed 3 too (FindEngine already knew its game exes; UFC3.exe --play waits for its game) */
        if ((PS4G(g) || X360G(g)) && PIni(L"engine_boost", 1)) { HANDLE th = CreateThread(NULL, 0, BoostThread, (LPVOID)(UINT_PTR)pi.dwProcessId, 0, NULL); if (th) CloseHandle(th); }
        FocusWatchStart();   /* 4.3.2: UFC 5 and 6 too, which PORTHEON minimized before their window existed (the focus
                                went to whatever was behind it, and their engines ignore an unfocused controller) */
        int fl = PS4G(g) ? GetInt(g, L"fps_limit", 60) : 60; wchar_t fn[48]; FpsName(fl, TRUE, fn, 48);
        if (arcEngine == 1) Msg(C_GOOD, L2(L"Iniciando o %s (motor novo: Carreira e Intel Arc)...", L"Starting %s (the new engine: Career and Intel Arc)..."), G[g].name);
        else if (arcEngine == 2) Msg(C_GOOD, L2(L"Iniciando o %s...", L"Starting %s..."), G[g].name);
        else if (arcEngine == 3) Msg(C_GOOD, L2(L"Iniciando o %s (motor Native Plus)...", L"Starting %s (the Native Plus engine)..."), G[g].name);
        else if (PS4G(g) && FpsExperimental(g, fl)) Msg(C_ORANGE, L2(L"Iniciando o %s %s (experimental): se a luta acelerar ou o jogo fechar, volte para 60.", L"Starting %s %s (experimental): if the fight speeds up or the game closes, go back to 60."), G[g].name, fn);
        else if (PS4G(g)) Msg(C_GOOD, L2(L"Iniciando o %s... Enquanto joga, deixe o foco na janela do jogo (sem Alt+Tab).", L"Starting %s... While you play, keep the focus on the game's window (no Alt+Tab)."), G[g].name);
        else if (X360G(g)) Msg(C_GOOD, L2(L"Iniciando o %s... Na primeira vez o port prepara os shaders por alguns minutos.", L"Starting %s... The first time, the port prepares its shaders for a few minutes."), G[g].name);
        else Msg(C_GOOD, T(S_STARTING), G[g].name);
        { wchar_t fw[1200]; FixWarnings(g, fw); if (fw[0]) Msg(C_ORANGE, L"%s", fw); }   /* 4.3.8: the remote fix list's warnings for this card */
        if (legacy) Msg(C_GOOD, L2(L"Iniciando o %s no modo de compatibilidade do processador (Intel Core de 2ª/3ª geração): a primeira preparação demora um pouco mais.",
                                   L"Starting %s in the processor compatibility mode (Intel Core 2nd/3rd generation): the first preparation takes a little longer."), G[g].name);
        /* 4.3.2: UFC 5 and 6 hold about 12 GB of RAM and fill an 8 GB card; with little left, Windows pages to the disk and
           what does not fit on the card is read from RAM, so the FPS falls fight after fight (tests of 2026-10-30: 28.9 ->
           22.2 fps from the first fight to the second while other programs held memory). Said while PORTHEON is in front. */
        if (G[g].ps5) {
            MEMORYSTATUSEX ms = {sizeof(ms)};
            if (GlobalMemoryStatusEx(&ms) && ms.ullTotalPhys < (24ull << 30) && ms.ullAvailPhys < (10ull << 30))
                Msg(C_ORANGE, L2(L"Pouca memória livre (%llu de %llu GB): feche o navegador, o Discord e outros programas antes de jogar. O %s usa cerca de 12 GB; sem folga, o FPS cai a cada luta.",
                                 L"Little free memory (%llu of %llu GB): close the browser, Discord and other programs before playing. %s uses about 12 GB; without room, the FPS drops with every fight."),
                    ms.ullAvailPhys >> 30, (ms.ullTotalPhys + (512ull << 20)) >> 30, G[g].name);
        }
        /* 5.2: PlayRoomCheck's warnings, after a warning already on the line or in place of the start message */
        if (SafeOn(g)) PlayWarn(L2(L"(Modo compatível ligado.)", L"(Compatible mode is on.)"));
        if (g_playWarn[0]) {
            if (g_msgColor == C_ORANGE || g_msgColor == C_BAD) { wchar_t m[1024]; _snwprintf(m, 1024, L"%s %s", g_msg, g_playWarn); m[1023] = 0; Msg(C_ORANGE, L"%s", m); }
            else Msg(C_ORANGE, L"%s", g_playWarn);
        }
    } else CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return TRUE;
}
static void SaveEdits(void);
static BOOL BadGameDirMsg(int g);
/* 4.1: PLAY in the window starts the game the way its desktop shortcut does: a new "PORTHEON.exe --play <id> --back"
   (a small window, no boot, no full screen) takes over and this one closes, with its art, fonts and full-screen window;
   after the game's normal end, --back opens PORTHEON again (an error keeps the explanation open, as for the shortcut).
   The user found the shortcut's start smoother (2026-10-28). light_play=0 in portheon.ini goes back. */
static HANDLE g_single; static BOOL g_backToUi, g_playInstance;   /* g_playInstance: this one was started with --play */
/* 4.4.0: the window that started the PLAY instance waits for that one's window (timer 14), then closes */
static ULONGLONG g_handoffAt;   /* HandoffTimer, after WNDCLASS_NAME */
static BOOL RelaunchSelf(const wchar_t *args)
{
    wchar_t exe[MAX_PATH], cmd[MAX_PATH + 64]; GetModuleFileNameW(NULL, exe, MAX_PATH);
    _snwprintf(cmd, MAX_PATH + 64, L"\"%s\"%s%s", exe, args[0] ? L" " : L"", args); cmd[MAX_PATH + 63] = 0;
    if (g_single) { ReleaseMutex(g_single); CloseHandle(g_single); g_single = NULL; }   /* the new one takes the lock */
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi;
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, 0, NULL, g_exeDir, &si, &pi)) return FALSE;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return TRUE;
}
static void MusicStop(BOOL fast);   /* music.c */
/* 6.1: the player's own games (library.c, ui_library.c): the custom game shown on the home screen (-1: a built-in
   game, g_sel, is shown) and the id of the custom game running (0: none) */
static int g_csel = -1, g_crun;
static void LibPlayToggle(void); static BOOL LibBusyMsg(void);
/* 6.1: before PORTHEON's own STOP of UFC 6, a line in the newest session log (bin\_Logs, UFC6.exe's): the next PLAY's
   check for a cut-off session (UFC6.exe LastSessionCut) then knows the player stopped it, and warns of nothing */
static void StopNoteUfc6(int g)
{
    if (!UFC6G(g)) return;
    wchar_t d[MAX_PATH], logs[MAX_PATH], pat[MAX_PATH], best[64] = L""; PortDir(g, d); Join(logs, d, L"bin\\_Logs"); Join(pat, logs, L"*.log");
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); if (h == INVALID_HANDLE_VALUE) return;
    do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcslen(fd.cFileName) < 64 && wcscmp(fd.cFileName, best) > 0) wcscpy(best, fd.cFileName); } while (FindNextFileW(h, &fd));
    FindClose(h); if (!best[0]) return;
    wchar_t p[MAX_PATH]; Join(p, logs, best);
    HANDLE f = CreateFileW(p, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    static const char line[] = "\r\n[PORTHEON] stopped by the player\r\n"; DWORD wr; WriteFile(f, line, sizeof(line) - 1, &wr, NULL); CloseHandle(f);
}
static void Play(void)
{
    if (g_csel < 0 && SOON(g_sel)) return;   /* 6.2 */
    if (g_csel >= 0) {   /* 6.1: a custom game is shown: start it, or stop it while it runs; never the built-in game running */
        if (g_proc && g_procGame >= 0) { Msg(C_ORANGE, L2(L"Feche o jogo que está aberto primeiro (%s).", L"Close the running game first (%s)."), G[g_procGame].name); return; }
        if (g_proc) { Msg(C_ORANGE, L"%s", L2(L"Feche o jogo que está aberto primeiro.", L"Close the running game first.")); return; }
        LibPlayToggle(); return;
    }
    if (!g_proc && LibBusyMsg()) return;                        /* 6.1: a custom game is running */
    if (!g_proc) ArenaParsecAtPlay(g_sel);                      /* 7.3 */
    if (g_proc) {   /* STOP: close the port and every process it started (the game engine) */
        /* 6.1: only processes created after their parent (ProcTree), and the engine held since it was found (Bloodborne's
           bb-probe.exe stays when "Bloodborne.exe --run" between them has left) */
        StopNoteUfc6(g_procGame);
        PNode nd[64]; int nk = ProcTree(GetProcessId(g_proc), nd, 64);
        if (g_engineH && WaitForSingleObject(g_engineH, 0) == WAIT_TIMEOUT) TerminateProcess(g_engineH, 1);
        for (int i = nk - 1; i >= 1; i--) { HANDLE h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, nd[i].pid); if (h) { if (ProcBornH(h) == nd[i].born) TerminateProcess(h, 1); CloseHandle(h); } }
        if (g_procGame >= 0 && NGPORTG(g_procGame) && g_playStart) {
            /* 6.1 review: Bloodborne/inFAMOUS: an engine left alone when the port's own launcher exited between two focus
               ticks (no live parent to follow): only processes whose program lies in this port's bin\ folder and that
               started after this PLAY */
            wchar_t pd[MAX_PATH], bin[MAX_PATH]; PortDir(g_procGame, pd); Join(bin, pd, L"bin\\");
            FILETIME nowf; GetSystemTimeAsFileTime(&nowf);
            ULONGLONG since = (((ULONGLONG)nowf.dwHighDateTime << 32) | nowf.dwLowDateTime) - (GetTickCount64() - g_playStart + 2000) * 10000ULL;
            HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
            size_t bl = wcslen(bin);
            if (sn != INVALID_HANDLE_VALUE) {
                for (BOOL ok = Process32FirstW(sn, &pe); ok; ok = Process32NextW(sn, &pe)) {
                    HANDLE h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID); if (!h) continue;
                    wchar_t img[MAX_PATH]; DWORD il = MAX_PATH; FILETIME c, e, k, u;
                    if (QueryFullProcessImageNameW(h, 0, img, &il) && il > bl && !_wcsnicmp(img, bin, bl) && GetProcessTimes(h, &c, &e, &k, &u)
                        && (((ULONGLONG)c.dwHighDateTime << 32) | c.dwLowDateTime) >= since) TerminateProcess(h, 1);
                    CloseHandle(h);
                }
                CloseHandle(sn);
            }
        }
        TerminateProcess(g_proc, 1);
        return;
    }
    SaveEdits();
    int st = Status(g_sel);
    if (st == S_NOPORT) { if (NGPORTG(g_sel)) ExtInstallMenu(g_sel); else Msg(C_BAD, T(S_NOTFOUND), G[g_sel].exe); return; }   /* 6.1: installed from inside */
    if (st == S_NOCPU && BBG(g_sel)) { wchar_t miss[64]; CpuLacksPs4(miss, 64); Msg(C_BAD, L2(L"Este processador não tem %s: o port do Bloodborne precisa de AVX (Intel Core de 2ª geração ou mais novo, ou AMD Ryzen/FX).", L"This processor lacks %s: the Bloodborne port needs AVX (Intel Core 2nd generation or newer, or AMD Ryzen/FX)."), miss); return; }
    if (st == S_PICK) { Msg(C_ORANGE, L"%s", L2(L"Escolha a pasta do jogo primeiro (veja \"Como instalar\").", L"Choose the game folder first (see \"How to install\").")); return; }
    if (BadGameDirMsg(g_sel)) return;
    if (st == S_NOCPU && (G[g_sel].ps5 || X360G(g_sel))) { wchar_t t[700]; NoAvx2Text(g_sel, t, 700); Msg(C_BAD, L"%s", t); return; }   /* 5.2: UFC Undisputed 3 too */
    if (!g_playInstance && !PcVerdictBox(g_sel, TRUE)) return;   /* 4.3.8: "Este PC roda?" before a game's first PLAY */
    SafePrompt(g_sel);   /* 5.2: the compatible mode after 2 crashes in a row (asked once: the answer is kept) */
    if (PS4G(g_sel) && GetInt(g_sel, L"ultra", 0)) Msg(C_SUB, L"%s", T(S_ULTRAON));
    MusicStop(TRUE);   /* 6.0: the game's music goes out at once: the game starts alone */
    if (!g_playInstance && PIni(L"light_play", 1) && !UnderWine()) {   /* 4.1: the shortcut's start, see RelaunchSelf (also
                                                                          for a shortcut started while this window was open) */
        wchar_t a[48]; _snwprintf(a, 48, L"--play %s --back", G[g_sel].id);
        /* 4.4.0: this window shows the game's picture and stays until the PLAY instance's window (the same picture) is
           up, so the screen goes straight from PORTHEON to the game's picture, as on a console. 6.0 final: the PLAY
           instance is started first and this window paints while it loads (the picture was built before it), the
           controllers are let go at once, and the first-PLAY shortcut is made after */
        if (RelaunchSelf(a)) {
            if (PIni(L"launch_splash", 1)) { LaunchShow(g_sel, TRUE); UpdateWindow(g_wnd); }
            PadPause(TRUE);
            AutoShortcut(g_sel);
            if (g_view == V_LAUNCH) { g_handoffAt = GetTickCount64(); SetTimer(g_wnd, 14, 60, NULL); }
            else DestroyWindow(g_wnd);
            return;
        }
        AutoShortcut(g_sel);
    }
    /* 3.5.1: UFC 1-4 minimize PORTHEON once their window shows and has the focus (FocusTimer); 4.3.2: UFC 5 and 6 too */
    if (StartPort(g_sel, TRUE)) AutoShortcut(g_sel);
}

/* what an exit code means, in words (3.5: "error 321" is how players read 3221225477, an access violation) */
static void ExitText(int g, DWORD code, wchar_t *out, int n)
{
    if (code == 3221225477u)
        _snwprintf(out, n, L2(L"O jogo fechou com o código 3221225477 (0xC0000005, um acesso inválido de memória). %s",
                              L"The game closed with code 3221225477 (0xC0000005, an access violation). %s"),
                   L2(L"Atualize o driver de vídeo e feche programas que desenham sobre o jogo (RivaTuner/MSI Afterburner, overlays, ReShade). Se repetir, clique em Logs e mande o log mais novo.",
                                 L"Update the video driver and close programs that draw over the game (RivaTuner/MSI Afterburner, overlays, ReShade). If it happens again, click Logs and send the newest log."));
    else if (code == 3221225781u || code == 3221225785u) {   /* 0xC0000135 a DLL is missing, 0xC0000139 a function in one */
        /* 4.0: which one, and what to do about it (checks.c): the video driver's Vulkan, an old Windows, or a port file */
        wchar_t why[600]; if (DllDiagnosis(g, why, 600)) _snwprintf(out, n, L"%s", why);
        else _snwprintf(out, n, L"%s", L2(L"O jogo não abriu: o Windows não achou um arquivo que o jogo usa (código 0xC0000135). Atualize o Windows (Windows Update) e o driver da placa de vídeo, e extraia o pacote do PORTHEON de novo numa pasta nova.",
                                          L"The game did not start: Windows did not find a file the game uses (code 0xC0000135). Update Windows (Windows Update) and the graphics driver, and extract the PORTHEON package again into a new folder."));
    }
    else if (code == 3221225501u)
        /* 4.0.2: the compatibility mode covers each port's own game version (its plan is made from that eboot.bin) */
        _snwprintf(out, n, CpuLacksPs4(NULL, 0)
            /* 4.0.3: the port makes the plan for a dump whose files are not the shipped plan's (cpu_compat.adapt_plan) */
            ? L2(L"O processador não tem uma instrução que o jogo usa (código 0xC000001D), num ponto que o modo de compatibilidade do PORTHEON não traduziu (%s %s, %s). Mande o log mais novo no Discord.",
                  L"The processor lacks an instruction the game uses (code 0xC000001D), at a spot PORTHEON's compatibility mode did not translate (%s %s, %s). Send the newest log on Discord.")
            : L2(L"O processador não tem uma instrução que o jogo usa (código 0xC000001D).%.0s%.0s%.0s", L"The processor lacks an instruction the game uses (code 0xC000001D).%.0s%.0s%.0s"),
            G[g].name, G[g].upd, G[g].tid);
    else if (code == 3221226505u)
        _snwprintf(out, n, L"%s", L2(L"O jogo encerrou por um erro interno (código 0xC0000409). Se repetir, clique em Logs e mande o log mais novo.", L"The game ended on an internal error (code 0xC0000409). If it happens again, click Logs and send the newest log."));
    else if (PS4G(g) && code == 3 && FpsExperimental(g, GetInt(g, L"fps_limit", 60))) {
        wchar_t fn[48]; FpsName(GetInt(g, L"fps_limit", 60), TRUE, fn, 48);
        _snwprintf(out, n, L2(L"O %s fechou com o código 3 %s, que é experimental%s. Volte para %s FPS em VÍDEO > Limite de FPS.",
                              L"%s closed with code 3 %s, which is experimental%s. Go back to %s FPS in VIDEO > FPS limit."), G[g].name, fn,
                   FpsFree(g) ? L"" : L2(L": acima de 60 o jogo conta o tempo da luta errado", L": above 60 the game counts fight time wrong"), FpsFree(g) ? L2(L"144 ou 60", L"144 or 60") : L"60");
    }
    else if (PS4G(g) && code == 3)
        _snwprintf(out, n, L"%s", L2(L"O jogo fechou com o código 3 (uma falha dentro do jogo). O PORTHEON já aplica as correções conhecidas (60 FPS por padrão, write windows desligadas, patch de rede do UFC 4). Se repetir no mesmo lugar, clique em Logs e mande o log mais novo.",
                                     L"The game closed with code 3 (a fault inside the game). PORTHEON already applies the known fixes (60 FPS by default, write windows off, UFC 4's network patch). If it happens again at the same spot, click Logs and send the newest log."));
    else if (PS4G(g) && code == 25) {   /* Released (11-13): the engine's own "out of memory" end, when its log did not say more (OnExited's LogNoCommit) */
        wchar_t now[260], hogs[300], who[360] = L""; MemNowText(now, 260);
        if (MemHogText(hogs, 300, FALSE)) { _snwprintf(who, 360, L2(L" Mais usam memória agora: %s.", L" Using the most memory now: %s."), hogs); who[359] = 0; }
        _snwprintf(out, n, L2(L"O %s fechou com o código 25: faltou memória para o jogo. %s Feche o navegador, o Discord e outros programas pesados e jogue de novo.%s",
                              L"%s closed with code 25: the game ran out of memory. %s Close the browser, Discord and other heavy programs and play again.%s"), G[g].name, now, who);
    }
    else if (PS4G(g) && code == 20)
        _snwprintf(out, n, L2(L"O jogo fechou com o código 20 (uma função do PS4 que o port não tem). %s", L"The game closed with code 20 (a PS4 function the port lacks). %s"),
                   (g == 2 || g == 3) && !WantArcEngine(g) ? L2(L"Em Extras, \"Motor de vídeo\" > \"Correção Intel Arc\" tem as funções que faltavam no UFC 4.", L"In Extras, \"Video engine\" > \"Intel Arc fix\" has the functions UFC 4 lacked.")
                                                          : L2(L"Clique em Logs e mande o log mais novo.", L"Click Logs and send the newest log."));
    else _snwprintf(out, n, T(S_CLOSEDERR), code);
    out[n - 1] = 0;
}
/* 4.2: why UFC 6 ended, from the word UFC6.exe left in UFC6.ini (last_reason), in PORTHEON's language; FALSE for a word
   it does not know (UFC6.exe's own text, last_session, is shown then) */
static BOOL Ufc6Reason(int g, const wchar_t *why, wchar_t *out, int n)
{
    const wchar_t *t = NULL;
    if (!wcscmp(why, L"nodevice")) t = L2(L"A placa de vídeo não tem o que o UFC 6 precisa (Vulkan 1.3 com os recursos do PS5). Atualize o driver da placa. Placas antigas, como as GTX 10 ou anteriores, não rodam.",
                                         L"The graphics card lacks what UFC 6 needs (Vulkan 1.3 with the PS5's features). Update the card's driver. Old cards, such as the GTX 10 series or older, cannot run it.");
    else if (!wcscmp(why, L"vram")) t = L2(L"O UFC 6 ficou sem memória de vídeo numa troca de cena (numa placa de 8 GB ele usa quase 7 GB). Feche o navegador, o Discord e programas de gravação antes de jogar; se repetir, mande o log desta sessão.",
                                          L"UFC 6 ran out of video memory at a scene change (on an 8 GB card it uses almost 7 GB). Close the browser, Discord and recording programs before playing; if it happens again, send this session's log.");
    else if (!wcscmp(why, L"rtx50")) t = L2(L"A RTX 50 travou ao preparar um gráfico do jogo (falha do compilador de shaders da NVIDIA). O PORTHEON anotou esse gráfico e vai pulá-lo daqui em diante: é só jogar de novo.",
                                           L"The RTX 50 card crashed while it prepared one of the game's graphics (a fault in NVIDIA's shader compiler). PORTHEON noted it and will skip it from now on: just play again.");
    else if (!wcscmp(why, L"driver")) t = L2(L"O driver da placa de vídeo travou ao preparar os gráficos do jogo. Reinstale o driver com instalação limpa (o mais novo, ou o anterior se você já tem o mais novo); se repetir, mande o log desta sessão.",
                                            L"The graphics driver crashed while it prepared the game's graphics. Reinstall the driver with a clean install (the newest, or the previous one if you already have the newest); if it happens again, send this session's log.");
    else if (!wcscmp(why, L"journal")) t = L2(L"A placa travou ao montar os efeitos guardados do UFC 6 na abertura. O PORTHEON apagou essa lista: na próxima vez o jogo prepara cada efeito quando precisar (pode engasgar um pouco no começo). É só jogar de novo; se fechar outra vez, mande o log desta sessão.",
                                             L"The graphics card crashed while it rebuilt UFC 6's saved effects at start. PORTHEON removed that list: next time the game prepares each effect when it needs it (it may stutter a little at first). Just play again; if it closes again, send this session's log.");   /* 5.1: UFC6.exe 1.16.0 */
    else if (!wcscmp(why, L"devlost")) t = L2(L"A placa de vídeo parou de responder (device lost). Atualize o driver e jogue de novo; se repetir, use o perfil Rápido e mande o log desta sessão.",
                                             L"The graphics card stopped responding (device lost). Update the driver and play again; if it happens again, use the Fast profile and send this session's log.");
    else if (!wcscmp(why, L"ram")) t = L2(L"Faltou memória RAM para o UFC 6 (ele usa cerca de 10 GB). Feche o navegador e outros programas pesados e deixe o Windows gerenciar o arquivo de paginação.",
                                         L"UFC 6 ran out of RAM (it uses about 10 GB). Close the browser and other heavy programs and let Windows manage the page file.");
    else if (!wcscmp(why, L"noengine")) t = L2(L"Falta o motor do UFC 6 (games\\UFC6\\bin\\UFC 6.exe). Um antivírus pode tê-lo removido: restaure-o ou extraia o PORTHEON de novo.",
                                              L"UFC 6's engine is missing (games\\UFC6\\bin\\UFC 6.exe). An antivirus may have removed it: restore it or extract PORTHEON again.");
    else if (!wcscmp(why, L"parsec")) t = L2(L"O modo Parsec não conseguiu preparar o motor de 2 jogadores (games\\UFC6\\bin\\UFC 6 Parsec.exe). Um antivírus pode tê-lo bloqueado; desligue o modo Parsec para jogar sozinho.",
                                            L"Parsec mode could not prepare the 2-player engine (games\\UFC6\\bin\\UFC 6 Parsec.exe). An antivirus may have blocked it; turn Parsec mode off to play alone.");
    else if (!wcscmp(why, L"novulkan")) t = L2(L"Nenhuma placa de vídeo com Vulkan foi encontrada. Instale o driver mais novo da sua placa (NVIDIA, AMD ou Intel).",
                                              L"No graphics card with Vulkan was found. Install your card's newest driver (NVIDIA, AMD or Intel).");
    else if (!wcscmp(why, L"nogame")) t = T(S_CHOOSEFIRST);
    else if (!wcscmp(why, L"noavx2")) { NoAvx2Text(g, out, n); return TRUE; }
    if (!t) return FALSE;
    wcsncpy(out, t, n); out[n - 1] = 0; return TRUE;
}
static void LoadEdits(void);
static void SetFullscreen(BOOL on);
/* 4.0.5: the engine could not reserve the game's memory ("cannot create 6080 MiB of guest memory (error 1450); is the
   page file large enough?", then a fault, code 3: UFC 4 on a laptop with an Intel UHD 620, log of 2026-10-26). Windows
   ran out of virtual memory (RAM + page file). PORTHEON offers what UFC5.exe does: a page file of 16 to 32 GB on C:
   (PORTHEON.exe --set-pagefile, as administrator), which takes effect after a restart. */
static int SetPagefile(void)
{
    HKEY k; int rc = 1;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management", 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
        static const wchar_t v[] = L"C:\\pagefile.sys 16384 32768\0";
        if (RegSetValueExW(k, L"PagingFiles", 0, REG_MULTI_SZ, (const BYTE *)v, sizeof(v)) == ERROR_SUCCESS) rc = 0;
        RegCloseKey(k);
    }
    return rc;
}
/* 5.2: before == TRUE is PlayRoomCheck's offer before the game starts (a small commit limit): the same page file, worded
   as a warning, and "No" just plays (it does not open Windows' window) */
static void PagefileOffer(int g) { PagefileOfferEx(g, FALSE); }
static void PagefileOfferEx(int g, BOOL before)
{
    MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms);
    unsigned long long ram = (ms.ullTotalPhys + (512ull << 20)) >> 30, virt = ms.ullTotalPageFile >> 30, page = ms.ullTotalPageFile > ms.ullTotalPhys ? (ms.ullTotalPageFile - ms.ullTotalPhys) >> 30 : 0;
    ULARGE_INTEGER fb = {0}; GetDiskFreeSpaceExW(L"C:\\", &fb, NULL, NULL); unsigned long long disk = fb.QuadPart >> 30;
    const wchar_t *manual = L2(L"Para fazer à mão: Painel de Controle > Sistema > Configurações avançadas do sistema > Desempenho > Avançado > Memória virtual > Alterar: 16384 a 32768 MB no C: (ou em outro disco com espaço), e reinicie o PC.",
                               L"To do it by hand: Control Panel > System > Advanced system settings > Performance > Advanced > Virtual memory > Change: 16384 to 32768 MB on C: (or on another drive with room), then restart the PC.");
    if (before) {
        if (page >= 15 || disk < 20) {   /* nothing PORTHEON can set: said, the game starts */
            wchar_t t[700]; _snwprintf(t, 700, L2(L"Pouca memória virtual (%llu GB de RAM, %llu GB no total): o %s pode fechar ao reservar a memória do jogo. %s", L"Little virtual memory (%llu GB of RAM, %llu GB in all): %s may close while it reserves the game's memory. %s"), ram, virt, G[g].name, manual);
            t[699] = 0; PlayWarn(t); return;
        }
        wchar_t q[800]; _snwprintf(q, 800, L2(L"Este PC tem pouca memória virtual para o %s (%llu GB de RAM e %llu GB de memória virtual no total): o jogo pode fechar ao reservar a memória dele (\"error 1450\").\n\nSim: o PORTHEON configura um arquivo de paginação de 16 a 32 GB no C: (pede administrador uma vez; vale depois de REINICIAR o PC). O jogo abre agora mesmo assim.\nNão: jogar sem mudar nada.",
                                              L"This PC has little virtual memory for %s (%llu GB of RAM and %llu GB of virtual memory in all): the game may close while it reserves its memory (\"error 1450\").\n\nYes: PORTHEON sets a page file of 16 to 32 GB on C: (it asks for administrator once; it takes effect after you RESTART the PC). The game starts now anyway.\nNo: play without changing anything."), G[g].name, ram, virt);
        q[799] = 0;
        if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONWARNING) != IDYES) return;
    } else {
    /* 6.1: the page file is already big and the v3.0 engine says how little was free when it stopped: other programs
       (a browser, Discord) held the memory, so a bigger page file is not the answer */
    if (page >= 15 && LogHas(g, "STOP: out of memory (")) {
        /* Released (11-13): what is free now and who holds the most */
        wchar_t now[260], hogs[300], who[360] = L""; MemNowText(now, 260);
        if (MemHogText(hogs, 300, FALSE)) { _snwprintf(who, 360, L2(L" Mais usam memória agora: %s.", L" Using the most memory now: %s."), hogs); who[359] = 0; }
        Msg(C_BAD, L2(L"O %s fechou por falta de memória (código 25): o arquivo de paginação já é grande (%llu GB de RAM, %llu GB de memória virtual no total), então outros programas estavam usando a memória. Feche o navegador, o Discord e outros programas pesados e jogue de novo. %s%s",
                      L"%s closed for lack of memory (code 25): the page file is already big (%llu GB of RAM, %llu GB of virtual memory in all), so other programs were using the memory. Close the browser, Discord and other heavy programs and play again. %s%s"),
            G[g].name, ram, virt, now, who);
        return;
    }
    { wchar_t now[260]; MemNowText(now, 260);   /* Released (11-13): the free memory now, beside the totals */
    Msg(C_BAD, L2(L"O %s fechou porque o Windows não deixou ele reservar a memória do jogo: falta memória virtual (este PC tem %llu GB de RAM e %llu GB de memória virtual no total). %s %s",
                  L"%s closed because Windows would not let it reserve the game's memory: virtual memory ran out (this PC has %llu GB of RAM and %llu GB of virtual memory in all). %s %s"),
        G[g].name, ram, virt, now, page >= 15 || disk < 20 ? manual : L""); }
    if (page >= 15 || disk < 20) return;   /* the page file is already big (too little RAM), or C: has no room for one */
    wchar_t q[700]; _snwprintf(q, 700, L2(L"O %s precisa de mais memória virtual.\n\nSim: o PORTHEON configura um arquivo de paginação de 16 a 32 GB no C: (pede administrador uma vez; vale depois de REINICIAR o PC).\nNão: abre a janela do Windows para você fazer à mão (Avançado > Memória virtual > Alterar).",
                                          L"%s needs more virtual memory.\n\nYes: PORTHEON sets a page file of 16 to 32 GB on C: (it asks for administrator once; it takes effect after you RESTART the PC).\nNo: opens the Windows window to do it by hand (Advanced > Virtual memory > Change)."), G[g].name);
    q[699] = 0;
    if (MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNO | MB_ICONWARNING) != IDYES) { ShellExecuteW(g_wnd, L"open", L"SystemPropertiesPerformance.exe", NULL, NULL, SW_SHOWNORMAL); return; }
    }
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL, exe, MAX_PATH);
    SHELLEXECUTEINFOW ei = {sizeof(ei)}; ei.fMask = SEE_MASK_NOCLOSEPROCESS; ei.hwnd = g_wnd; ei.lpVerb = L"runas"; ei.lpFile = exe; ei.lpParameters = L"--set-pagefile"; ei.nShow = SW_HIDE;
    DWORD rc = 1;
    if (ShellExecuteExW(&ei) && ei.hProcess) { WaitForSingleObject(ei.hProcess, 30000); GetExitCodeProcess(ei.hProcess, &rc); CloseHandle(ei.hProcess); }
    if (before) PlayWarn(rc == 0 ? L2(L"Arquivo de paginação de 16 a 32 GB configurado: vale depois de REINICIAR o PC.", L"A page file of 16 to 32 GB was set: it takes effect after you RESTART the PC.")
                                 : L2(L"Não deu para configurar a memória virtual (sem permissão de administrador).", L"Virtual memory could not be set (no administrator permission)."));
    else if (rc == 0) Msg(C_GOOD, L"%s", L2(L"Pronto: arquivo de paginação de 16 a 32 GB no C:. REINICIE o PC e jogue de novo.", L"Done: a page file of 16 to 32 GB on C:. RESTART the PC and play again."));
    else Msg(C_BAD, L"%s %s", L2(L"Não deu para configurar a memória virtual (sem permissão de administrador).", L"Virtual memory could not be set (no administrator permission)."), manual);
}
/* 7.1 review (launcher-ufc6/player#1, portheon-app/correctness#1, player#1, cross/contract#1; the ufc6 lane's request 4):
   UFC6.exe 1.29.0's note for this PLAY, play_note in UFC6.ini, already in the player's language: DLSS, the PS5 Pro mode
   or AMD FSR that did not run this time, why, and where to turn it on again (lead decisions D/E), the Visual C++ files,
   the title profile and the folder's path. UFC6.exe empties it at the next PLAY's first start and copies it into
   last_session only when the session failed, so PORTHEON shows it after every end: OnExited (after the end's text), a
   desktop shortcut's or the window's PLAY that ended normally (PORTHEON stays open, as after an error) and LateExited's
   normal end (told, as an error end is); then the key is emptied so it is said once. */
static BOOL Ufc6PlayNoteGet(int g, wchar_t *pn, int n)
{
    pn[0] = 0;
    if (g >= 0 && UFC6G(g)) GetStr(g, L"play_note", pn, n, L"");
    return pn[0] != 0;
}
static BOOL Ufc6PlayNotePending(int g) { wchar_t pn[8]; return Ufc6PlayNoteGet(g, pn, 8); }
/* the end's text (g_msg) and the note in one message (Msg keeps one): the note after it, orange after a normal end, red
   after an error (the report offer looks for C_BAD); not again when the text already holds it (last_session of a failed
   end). No room for both: the end's text is cut after its last whole sentence that leaves room for the note. */
static void Ufc6PlayNoteShow(int g)
{
    wchar_t pn[600], m[1024];
    if (!Ufc6PlayNoteGet(g, pn, 600)) return;
    PutStr(g, L"play_note", L"");
    if (wcsstr(g_msg, pn)) return;
    COLORREF c = g_msgColor == C_BAD ? C_BAD : C_ORANGE;
    size_t ln = wcslen(pn);
    wcsncpy(m, g_msg, 1023); m[1023] = 0;
    if (wcslen(m) + ln + 1 > 1023) {
        size_t room = ln + 1 < 1023 ? 1023 - ln - 1 : 0, cut = 0;
        for (size_t i = 0; i < room && m[i]; i++) if (m[i] == L'.' && (m[i + 1] == L' ' || !m[i + 1])) cut = i + 1;
        m[cut] = 0;
    }
    if (m[0]) wcscat(m, L" ");
    size_t l = wcslen(m); wcsncpy(m + l, pn, 1023 - l); m[1023] = 0;
    Msg(c, L"%s", m);
}
/* 3.5.2: a game started by its desktop shortcut (or the window's PLAY, light_play) that ended normally closes PORTHEON
   too; an error keeps it open. 7.1 review: so does a UFC 6 end with a note to show (the next instance would not know) */
static BOOL ShortcutEndCloses(int g, DWORD code, int lost)
{
    return g >= 0 && g == g_directPlay && (code == 0 || code == 1) && lost == 0 && !Ufc6PlayNotePending(g);
}
static void OnExited(DWORD code)
{
    int g = g_procGame, lost = PS4G(g_procGame) ? g_focusLost : 0, sw = 0; CloseHandle(g_proc); g_proc = NULL; g_procGame = -1;
    if (g >= 0 && PS4G(g) && (code == 0 || code == 1) && LogFrozenClosed(g)) code = 23;   /* 7.1 (U2-HANG F2): a frozen game the player closed */
    SetTimer(g_wnd, 7, 30, NULL); SetTimer(g_wnd, 9, 6 * 3600 * 1000, NULL);   /* 4.3.2: stopped while the game ran (StartPort); perf 11-13: 30 as in wWinMain */
    SelfQuiet(FALSE);   /* 4.3.3 */
    if (g_view == V_LAUNCH) { KillTimer(g_wnd, 13); g_view = V_HOME; }   /* 4.4.0: the game ended before its window showed */
    BOOL lostLate = lost && GetTickCount64() - g_focusLostAt < 4000;
    if (lostLate) lost--;   /* the last seconds: the crash's own dialog, not the player */
    /* 4.3.1: the player was in the game when it ended; otherwise the program the player went to keeps the foreground
       (g_quietUntil). 4.3.2: UFC 5 and 6 are followed too (FocusWatchStart for every game), so a key meant for that other
       program cannot start them again; a game whose window was never found still brings PORTHEON back */
    BOOL front = g < 0 || !g_gameWnd || g_gameFocused || lostLate;
    g_quietUntil = GetTickCount64() + 1500;
    FocusWatchStop(); g_focusLost = 0; KillTimer(g_wnd, 12);
    InterlockedExchange(&g_watchStop, 1);
    if (g_job) { CloseHandle(g_job); g_job = NULL; }
    Ufc2ServerStop();   /* 4.3.6: the UFC 2 room ends with the host's game */
    HzRestore(); LearnAfter(g);   /* 6.2 */
    ULONGLONG played = g_playStart ? GetTickCount64() - g_playStart : 0;
    if (g >= 0 && g_playStart) { PlayTimeAdd(g, played); SessionRecord(g, played, code); }   /* 6.2: the Atividade card */
    g_playStart = 0;
    RunningClear(g); StreakAfter(g, code, played);   /* 5.2: before the shortcut's early return below */
    /* 3.5.2: a game started by its desktop shortcut that ended normally closes PORTHEON too; an error keeps it open.
       7.1 review: a UFC 6 note to show keeps it open too (ShortcutEndCloses) */
    if (ShortcutEndCloses(g, code, lost)) { if (g_backToUi) RelaunchSelf(front ? L"--after-game" : L"--after-game --behind"); DestroyWindow(g_wnd); return; }
    g_directPlay = -1;
    if (front) {
        ShowWindow(g_wnd, SW_RESTORE);
        if (g_backToUi && PIni(L"ui_fullscreen", 1)) SetFullscreen(TRUE);   /* 4.1: an error shows the window as the player left it */
        SetForegroundWindow(g_wnd);
    } else {   /* 4.3.1: stays where it is (minimized during the game), its taskbar button blinking until the player opens it */
        if (!IsWindowVisible(g_wnd)) ShowWindow(g_wnd, SW_SHOWMINNOACTIVE);   /* single window: it was hidden during the game (FocusTimer) */
        FLASHWINFO fi = {sizeof(fi), g_wnd, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0}; FlashWindowEx(&fi);
    }
    g_kvGame = -1;
    PadPause(FALSE);   /* 4.0: the controllers drive PORTHEON again */
    wchar_t cal[400] = L"", txt[600]; int stuckWhy = 0;   /* 7.1 review: LearnStuckShader's reason */
    if (g >= 0 && G[g].ps5) {   /* 4.0.2: the launcher wrote what happened (4.2: UFC6.exe, last_reason and last_session in UFC6.ini) */
        wchar_t last[600], why[32]; GetStr(g, L"last_session", last, 600, L""); GetStr(g, L"last_reason", why, 32, L"");
        if ((!wcscmp(why, L"bench") || !wcscmp(why, L"warmup")) && last[0]) Msg(C_GOOD, L"%s", last);   /* 4.3.7: "Preparar o UFC 6" too */   /* 4.3.5: the performance test's result (UFC6.exe leaves it only when no error explains the end) */
        else if (code == 0 || code == 1) Msg(C_SUB, L"%s", T(S_CLOSED));
        else if (UFC6G(g) && !wcscmp(why, L"ram")) PagefileOffer(g);   /* 4.3.1: as UFC 1-4: on 8 GB of RAM the page file holds the rest of UFC 6's ~10 GB */
        else if (why[0] && UFC6G(g) && Ufc6Reason(g, why, txt, 600)) {   /* 4.3: UFC5.exe leaves its own text only */
            wchar_t note[600]; GetStr(g, L"repair_note", note, 600, L"");   /* 5.1: UFC6.exe 1.16.0's automatic repair step */
            Msg(C_BAD, L"%s%s", txt, note);
        }
        else if (last[0]) Msg(C_BAD, L"%s", last);
        else { ExitText(g, code, txt, 600); Msg(C_BAD, L"%s", txt); }
    }
    else if (g >= 0 && EXTG(g)) {   /* 6.0: Zelda's emulator or native build, God of War III's port */
        if (code == 0 || code == 1 || code == 0xC000013Au) Msg(C_SUB, L"%s", T(S_CLOSED));
        else { wchar_t ld[MAX_PATH]; if (!ExtLogsDir(g, ld)) wcscpy(ld, L"-");
               Msg(C_ORANGE, L2(L"O %s fechou (código %lu). Se não foi você que fechou, os logs ficam em %s (botão Logs): mande o mais novo no Discord.", L"%s closed (code %lu). If you did not close it, the logs are in %s (the Logs button): send the newest one on Discord."), G[g].name, code, ld); }
    }
    else if (g >= 0 && X360G(g)) {   /* 5.0: UFC3.exe's exit is the game's; its port writes ufc3.log and RELATORIO.md in the game folder */
        if (code == 0) Msg(C_SUB, L"%s", T(S_CLOSED));
        else { wchar_t gd[MAX_PATH]; GameDir(g, gd); X3Normalize(gd);
               Msg(C_ORANGE, L2(L"O %s fechou (código %lu). Se não foi você que fechou, abra o ufc3.log e o RELATORIO.md na pasta do jogo (%s): o port explica ali o que houve.",
                                L"%s closed (code %lu). If you did not close it, open ufc3.log and RELATORIO.md in the game folder (%s): the port explains there what happened."), G[g].name, code, gd); }
    }
    else if (code == 0 || code == 1) Msg(C_SUB, L"%s", T(S_CLOSED));
    else if (g >= 0 && PS4G(g) && LogNoCommit(g)) PagefileOffer(g);   /* 4.0.5: no virtual memory left; 6.1: the v3.0 engine's words too (LogNoCommit) */
    else if (g >= 0 && code == 2 && Code2Diagnosis(g, txt, 600)) Msg(C_BAD, L"%s", txt);   /* 4.0: "erro 2" */
    /* 4.0: a game folder with a broken file explains the crash better than the crash itself (checks.c) */
    else if (g >= 0 && GameFilesAfterCrash(g, txt, 600)) Msg(C_BAD, L"%s", txt);
    else if (g >= 0 && PS4G(g) && VulkanTooOldAfterExit(g, txt, 600)) Msg(C_BAD, L"%s", txt);   /* 4.0.3 (code 23); 4.3.1: any code, the log's own words */
    else if (g == 3 && LogHas(g, "mapping direct memory 0, size=") && LogHas(g, "failed (error 0x80020016)"))   /* 7.3: see Ufc4MemEnv */
        Msg(C_BAD, L2(L"O UFC 4 fechou ao carregar porque o arquivo samplegame\\data\\common\\config\\videoheap.cfg da pasta do jogo pede mais memória de vídeo (%d MB) do que cabe na memória do PS4 que o motor reserva. Isso acontece quando outro launcher (o do UFC4 Native Plus, por exemplo) mudou esse arquivo para 4K. O PORTHEON já reserva o que ele pede; se repetir, extraia de novo só esse arquivo do jogo, sem a mudança, e mande o relatório.",
                      L"UFC 4 closed while loading because the game folder's samplegame\\data\\common\\config\\videoheap.cfg asks for more video memory (%d MB) than fits in the PS4 memory the engine reserves. This happens when another launcher (UFC4 Native Plus's, for example) changed that file for 4K. PORTHEON already reserves what it asks for; if it happens again, extract that one game file again, without the change, and send the report."), g_u4HeapMb ? g_u4HeapMb : 4000);
    else if (g >= 0 && NpEngineFallback(g, code, played))   /* Released */
        Msg(C_BAD, L2(L"O %s fechou com o motor Native Plus (código %lu). O próximo JOGAR usa o motor de antes; para voltar ao Native Plus: EXTRAS > Motor de vídeo.",
                      L"%s closed on the Native Plus engine (code %lu). The next PLAY uses the engine from before; to go back to Native Plus: EXTRAS > Video engine."), G[g].name, code);
    else if (g >= 0 && NewEngineFallback(g, code, played))   /* 4.3.1 */
        Msg(C_BAD, L2(L"O %s fechou logo no começo com o motor novo (código %lu). O próximo JOGAR usa o motor do próprio port; para voltar ao novo: EXTRAS > Motor de vídeo.",
                      L"%s closed right at the start on the new engine (code %lu). The next PLAY uses the port's own engine; to go back to the new one: EXTRAS > Video engine."), G[g].name, code);
    /* 4.0.3: the card ran out of video memory (a GTX 1050 with 2 GB in UFC 3, already at 720p: "ErrorOutOfDeviceMemory") */
    else if (g >= 0 && PS4G(g) && code == 23 && LogHas(g, "ErrorOutOfDeviceMemory")) {
        int mb = g_haveGpu ? (int)(g_gpu.vram >> 20) : 0; BOOL was = GetInt(g, L"ultra", 0) != 0;
        if (!was) PutInt(g, L"ultra", 1);
        Msg(C_BAD, L2(L"O %s fechou com o código 23 porque a placa de vídeo ficou sem memória de vídeo (%d MB).%s Feche os programas que usam a placa enquanto joga (navegador, Discord, gravação de tela); se repetir, a placa tem memória de vídeo de menos para o jogo: placas de 2 GB ficam no limite.",
                      L"%s closed with code 23 because the graphics card ran out of video memory (%d MB).%s Close the programs that use the card while you play (browser, Discord, screen recording); if it happens again, the card has too little video memory for the game: 2 GB cards are at the limit."),
            G[g].name, mb, was ? L"" : L2(L" O próximo JOGAR usa o modo Ultra desempenho (720p, modo PS4).", L" The next PLAY uses Ultra performance mode (720p, PS4 mode)."));
    }
    /* 4.0.4: drive C: filled up while the engine wrote its shader cache: "WriteSpan: Assertion Failed! Failed to write to
       file, error = No space left on device", code 23 (this PC's UFC 4 on 2026-10-24, with 2.8 GB free) */
    else if (g >= 0 && PS4G(g) && code == 23 && LogHas(g, "No space left on device")) {
        /* Released (11-13): the drive the game's data is on (%LOCALAPPDATA%, not always C:) and how much it has free now */
        wchar_t la[MAX_PATH] = L"", drv[4] = L"C:", gb[24]; ULARGE_INTEGER fb = {0}; GetEnvironmentVariableW(L"LOCALAPPDATA", la, MAX_PATH);
        if (la[0] && la[1] == L':') drv[0] = la[0];
        if (!la[0] || !GetDiskFreeSpaceExW(la, &fb, NULL, NULL)) fb.QuadPart = 0;
        GbText(fb.QuadPart, gb, 24);
        Msg(C_BAD, L2(L"O %s fechou com o código 23 porque o disco %s ficou cheio enquanto o jogo gravava o cache de shaders (\"no space left on device\"): agora ele tem %s GB livres. Libere espaço no disco %s (5 GB ou mais) e jogue de novo.",
                      L"%s closed with code 23 because drive %s filled up while the game was writing its shader cache (\"no space left on device\"): it has %s GB free now. Free space on drive %s (5 GB or more) and play again."), G[g].name, drv, gb, drv);
    }
    else if (g >= 0 && PS4G(g) && code == 23 && LearnStuckShader(g, cal, &stuckWhy)) {
        /* 3.5.1: an experimental FPS goes back to 60 as well (very high rates also stopped GPUs, code 23) */
        BOOL fps = FpsExperimental(g, GetInt(g, L"fps_limit", 60)); if (fps) PutInt(g, L"fps_limit", 60);
        Msg(C_BAD, L2(L"O jogo fechou com o código 23: a placa de vídeo travou num efeito gráfico (shader %s). Ele fica desligado a partir do próximo JOGAR, para essa cena não travar a placa de novo.%s Se repetir, mande o log mais novo.",
                      L"The game closed with code 23: the graphics card got stuck on one graphics effect (shader %s). It is switched off from the next PLAY on, so that scene no longer stops the GPU.%s If it happens again, send the newest log."), cal,
            fps ? L2(L" O FPS experimental voltou para 60.", L" The experimental FPS is back at 60.") : L"");
    }
    else if (g >= 0 && PS4G(g) && code == 23) {
        /* code 23: the graphics card stopped (a GPU fault). Seen at the start of a UFC 4 fight with 720p + FSR 1.0
           (player's photo, 2026-10-20): the next PLAY goes without the upscaler and without PS4 Pro mode */
        BOOL fsr = GetInt(g, L"upscaler", 0) != 0, neo = GetInt(g, L"neo", 0) != 0;
        if (fsr) { PutInt(g, L"upscaler", 0); PutInt(g, L"sharpness", 0); }
        /* 7.1 (ufc1-4 Check: the 11-14 UFC 2 stop ran FSR 3.1 temporal, which no step turned off): the temporal upscaler too */
        if (GetInt(g, L"pgame:tups", 0)) { PutInt(g, L"pgame:tups", 0); fsr = TRUE; }
        if (neo) PutInt(g, L"neo", 0);
        if (GetInt(g, L"resolution", 1) > 1) { PutInt(g, L"resolution", 1); fsr = TRUE; }
        { int fl = GetInt(g, L"fps_limit", 60); if (fl == 0 || fl > 60) { PutInt(g, L"fps_limit", 60); neo = TRUE; } }
        if (GetInt(g, L"low_latency", 0) && !GetInt(g, L"parsec", 0)) PutInt(g, L"low_latency", 0);
        BOOL arcHint = (g == 2 || g == 3) && GpuIsIntel() && !WantArcEngine(g);
        const wchar_t *hint = arcHint ? L2(L" Em Extras, use o \"Motor novo\" (correção Intel Arc).", L" In Extras, use the \"New engine\" (Intel Arc fix).") : L"";
        /* 3.5.3: what PORTHEON changes by itself for the next PLAY (Gpu23Escalate) */
        int esc = Gpu23Escalate(g);
        const wchar_t *next = esc == 1 ? L2(L" O próximo JOGAR usa o \"Motor novo\" (o SacredFire mais recente, com correções de travamento da placa). Para voltar: EXTRAS > Motor de vídeo > Padrão do port.",
                                            L" The next PLAY uses the \"New engine\" (the newest SacredFire, with GPU hang fixes). To go back: EXTRAS > Video engine > The port's own.")
                            : esc == 2 ? L2(L" O próximo JOGAR usa o modo seguro de vídeo (EXTRAS), que grava os comandos da placa numa thread só.",
                                            L" The next PLAY uses the safe video mode (EXTRAS), which records the GPU work on a single thread.")
                            : esc == 3 ? L2(L" O próximo JOGAR desenha o cabelo com a cobertura da própria placa (alpha-to-coverage), em vez do pontilhado que parou as Radeon RX 5000.",
                                            L" The next PLAY draws the hair with the card's own alpha-to-coverage, instead of the dither that stopped Radeon RX 5000 cards.")
                            : esc == 4 && PIni(L"safe_offer", 1) ? L2(L" O próximo JOGAR oferece o modo compatível (720p, 60 FPS, sem FSR e outros ajustes mais seguros).",   /* 7.1 */
                                            L" The next PLAY offers the compatible mode (720p, 60 FPS, no FSR and other safer settings).") : L"";
        if (fsr || neo) Msg(C_BAD, L2(L"O jogo fechou com o código 23 (a placa de vídeo parou). O próximo JOGAR roda sem %s.%s Atualize também o driver da placa.%s",
                                      L"The game closed with code 23 (the graphics card stopped). The next PLAY runs without %s.%s Update your GPU driver too.%s"),
                        fsr && neo ? L2(L"FSR / 2K-4K / PS4 Pro / acima de 60 FPS", L"FSR / 2K-4K / PS4 Pro / over 60 FPS") : fsr ? L"FSR / 2K-4K" : L2(L"PS4 Pro / acima de 60 FPS", L"PS4 Pro / over 60 FPS"), next, hint);
        /* 6.0: a big draw stuck twice on the same shader is switched off (LearnStuckShader), so the text says so */
        else if (stuckWhy == 1) Msg(C_BAD, L2(L"O jogo fechou com o código 23 (a placa de vídeo parou num desenho grande, que o PORTHEON só desliga se a placa travar nele de novo, para não sumir um lutador ou a arena à toa).%s Jogue de novo; se repetir, clique em Logs e mande o log mais novo no Discord.%s",
                           L"The game closed with code 23 (the graphics card stopped on a big draw, which PORTHEON only switches off if the card stops on it again, so a fighter or the arena does not vanish for nothing).%s Play again; if it happens again, click Logs and send the newest log on Discord.%s"), next, hint);
        /* 7.1 review (correctness#5, player#5): a small draw left on (another shader stopped last time), and a stop with no
           draw named (no STUCK line, as a frozen game the player closed can be) get their own true text */
        else if (stuckWhy == 2) Msg(C_BAD, L2(L"O jogo fechou com o código 23 (a placa de vídeo parou num efeito diferente do da última parada; como as paradas mudam de lugar, o PORTHEON só desliga um efeito se a placa travar nele de novo).%s Jogue de novo; se repetir, clique em Logs e mande o log mais novo no Discord.%s",
                                         L"The game closed with code 23 (the graphics card stopped on a different effect than at the last stop; since the stops move around, PORTHEON only switches an effect off if the card stops on it again).%s Play again; if it happens again, click Logs and send the newest log on Discord.%s"), next, hint);
        else Msg(C_BAD, L2(L"O jogo fechou com o código 23 (a placa de vídeo parou).%s Jogue de novo; se repetir, clique em Logs e mande o log mais novo no Discord.%s",
                           L"The game closed with code 23 (the graphics card stopped).%s Play again; if it happens again, click Logs and send the newest log on Discord.%s"), next, hint);
    }
    /* 5.2: an Intel processor (no SSE4a) reached UFC 3/4's INSERTQ/EXTRQ on the new or the v3.0 engine, which do not
       patch them: any card, any time in the game. On Automatic the next PLAY takes the port's own engine
       (ufcN_old_engine, which WantArcEngine honours on Intel cards too); a hand-picked engine is named. Before
       Ufc3CareerCrash, so the two rules cannot take turns. */
    else if (g >= 1 && g <= 3 && code == 3 && !CpuHasSse4a() && GIni(L"engine_ran", g, 0) > 0 && LogHas(g, "exception 0xc000001d")) {
        int m = GetInt(g, L"pgame:engine", 0), ran = GIni(L"engine_ran", g, 0);
        if (g >= 2 && m == 0) PIniSet(g == 2 ? L"ufc3_old_engine" : L"ufc4_old_engine", 1);
        Msg(C_BAD, L2(L"O %s fechou com o código 3 numa instrução que só os processadores AMD têm (SSE4a, \"0xc000001d\"): o %s não a traduz num processador Intel, e o motor do próprio port traduz. %s",
                      L"%s closed with code 3 on an instruction only AMD processors have (SSE4a, \"0xc000001d\"): the %s does not translate it on an Intel processor, and the port's own engine does. %s"),
            G[g].name, ran == 3 ? L2(L"motor Native Plus", L"Native Plus engine") : ran == 2 ? L2(L"motor do online", L"online engine") : L2(L"motor novo", L"new engine"),
            g <= 2 && OnlineMode(g) ? L2(L"O online precisa desse motor: jogue offline neste PC.", L"Online needs that engine: play offline on this PC.")
            : g >= 2 && m == 0 ? L2(L"O próximo JOGAR usa o motor do próprio port.", L"The next PLAY uses the port's own engine.")
            : L2(L"Em EXTRAS > Motor de vídeo, escolha \"Padrão do port\".", L"In EXTRAS > Video engine, pick \"The port's own\"."));
    }
    else if (g == 2 && code == 3 && (sw = Ufc3CareerCrash()) != 0) {
        if (sw == 1) Msg(C_BAD, L"%s", L2(L"O UFC 3 fechou com o código 3 (uma falha dentro do jogo, como o crash da Carreira nas lutas de título e na última luta). O próximo JOGAR usa o motor novo, que tem as correções da Carreira do GNSB: continue de onde parou. Para voltar ao antigo: EXTRAS > Motor de vídeo > Padrão do port.",
                                          L"UFC 3 closed with code 3 (a fault inside the game, like the Career crash at title fights and the last fight). The next PLAY uses the new engine, which has GNSB's Career fixes: go on from where you stopped. To go back: EXTRAS > Video engine > The port's own."));
        else Msg(C_BAD, L"%s", L2(L"O UFC 3 fechou no crash conhecido da Carreira (thread \"UI Update\"). Em EXTRAS > Motor de vídeo, escolha \"Motor novo\": ele tem a correção do GNSB para esse crash.",
                                  L"UFC 3 closed on the known Career crash (thread \"UI Update\"). In EXTRAS > Video engine, pick \"New engine\": it has GNSB's fix for that crash."));
    }
    /* 4.0.3: the engine reports a processor instruction it cannot run as code 3 ("exception 0xc000001d" in its log) */
    else if (g >= 0 && PS4G(g) && code == 3 && LogHas(g, "exception 0xc000001d")) { ExitText(g, 3221225501u, txt, 600); Msg(C_BAD, L"%s", txt); }
    else if (g >= 0 && PS4G(g) && code == 3) {
        /* 3.5.3: code 3 is a fault inside the game. At an experimental FPS the next PLAY goes back to 60 by itself (UFC 1,
           2 and 4 count the fight's time per frame); otherwise the message says where it happened, for the report. */
        int fl = GetInt(g, L"fps_limit", 60); BOOL fps = FpsExperimental(g, fl);
        wchar_t where[200]; GuestFault(g, where, 200);
        if (fps) {
            wchar_t fn[48]; FpsName(fl, TRUE, fn, 48); PutInt(g, L"fps_limit", 60);
            Msg(C_BAD, L2(L"O %s fechou com o código 3 %s, que é experimental%s. O próximo JOGAR já volta para 60 FPS.",
                          L"%s closed with code 3 %s, which is experimental%s. The next PLAY goes back to 60 FPS by itself."), G[g].name, fn,
                FpsFree(g) ? L"" : L2(L": acima de 60 o jogo conta o tempo da luta errado", L": above 60 the game counts fight time wrong"));
        } else if (FpsFree(g) && fl > 60) {   /* 5.2: UFC 3 (7.2: and UFC 2) above 60 (RecValue follows the monitor): back to 60, and the automatic setup keeps 60 from now on */
            /* ufc3_fps60 lasts until the player raises the FPS limit above 60 by hand (DropdownMenu clears it) */
            wchar_t fn[48]; FpsName(fl, TRUE, fn, 48); PutInt(g, L"fps_limit", 60); PIniSet(Fps60Key(g), 1);
            Msg(C_BAD, L2(L"O %s fechou com o código 3 %s%s. O próximo JOGAR volta para 60 FPS; para subir de novo: VÍDEO > Limite de FPS.",
                          L"%s closed with code 3 %s%s. The next PLAY goes back to 60 FPS; to raise it again: VIDEO > FPS limit."), G[g].name, fn, where);
        } else
            Msg(C_BAD, L2(L"O %s fechou com o código 3, uma falha dentro do jogo%s. O PORTHEON já aplica as correções conhecidas (60 FPS, write windows desligadas, patch de rede do UFC 4). Se repetir no mesmo ponto: deixe o foco na janela do jogo, atualize o driver de vídeo%s e mande o log mais novo (botão Logs) no Discord: %s",
                          L"%s closed with code 3, a fault inside the game%s. PORTHEON already applies the known fixes (60 FPS, write windows off, UFC 4's network patch). If it happens again at the same spot: keep the focus on the game's window, update the video driver%s and send the newest log (Logs button) on Discord: %s"),
                G[g].name, where, (g == 2 || g == 3) && !WantArcEngine(g) ? L2(L", teste EXTRAS > Motor de vídeo > Motor novo", L", try EXTRAS > Video engine > New engine") : L"", URL_DISCORD);
    }
    else { ExitText(g >= 0 ? g : 0, code, txt, 600); Msg(C_BAD, L"%s", txt); }
    /* 3.5.1: a crash after the game's window lost the focus (FocusTimer, ForegroundHook) says so first */
    if (g >= 0 && PS4G(g) && code != 0 && code != 1 && lost > 0) {
        wchar_t m[1024]; wcsncpy(m, g_msg, 1024); m[1023] = 0;
        Msg(C_BAD, L2(L"Antes de fechar, a janela do %s ficou fora de foco %d vez(es) (Alt+Tab, clique em outro monitor ou outra janela): fora de foco o jogo pode fechar sozinho, então deixe o foco nele enquanto joga. %s",
                      L"Before it closed, the %s window lost the focus %d time(s) (Alt+Tab, a click on another monitor or window): out of focus the game can close by itself, so keep the focus on it while you play. %s"), G[g].name, lost, m);
    }
    /* 3.5.2: the shortcut this game's first PLAY made, said after a normal end (an error message comes first) */
    if (g >= 0 && g == g_shortcutNew && (code == 0 || code == 1))
        Msg(C_GOOD, L2(L"Atalho \"%s - PORTHEON\" criado na área de trabalho: da próxima vez, abra o jogo direto por ele.", L"Shortcut \"%s - PORTHEON\" made on the desktop: next time, start the game straight from it."), G[g].name);
    if (g == g_shortcutNew) g_shortcutNew = -1;
    /* 7.1 review: UFC6.exe's note for this PLAY, after every end's text (the shortcut's too), before the report offer */
    if (g >= 0 && G[g].ps5) Ufc6PlayNoteShow(g);
    /* 4.3.8: an error end offers the report for Discord (a .zip on the desktop, MakeReport) and the Discord page: most
       players never send a log, and each log is how an error gets fixed for everyone (the remote fix list) */
    if (g >= 0 && code != 0 && code != 1 && g_msgColor == C_BAD && PIni(L"report_offer", 1)) {
        wchar_t q[700]; _snwprintf(q, 700, L2(L"O %s fechou com erro.\n\nQuer gerar o relatório para mandar no Discord do PORTHEON? É um .zip na área de trabalho com os logs e o resumo do PC (sem o nome da sua conta do Windows). Com ele, o erro pode ser corrigido para todo mundo.\n\nSim: gera o .zip e abre o Discord.   Não: agora não.   Cancelar: não perguntar mais.",
                                     L"%s closed with an error.\n\nMake the report to send on PORTHEON's Discord? It is a .zip on the desktop with the logs and a summary of the PC (without your Windows account's name). With it, the error can be fixed for everyone.\n\nYes: makes the .zip and opens Discord.   No: not now.   Cancel: do not ask again."), G[g].name); q[699] = 0;
        int a = MessageBoxW(g_wnd, q, L"PORTHEON", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (a == IDYES) { MakeReport(g); ShellExecuteW(g_wnd, L"open", URL_DISCORD, NULL, NULL, SW_SHOWNORMAL); }
        else if (a == IDCANCEL) PIniSet(L"report_offer", 0);
    }
    LoadEdits();
}

/* ---------------------------------------------------------------- the other parts of PORTHEON */
static BOOL MakeShortcut(int g, BOOL quiet);
#include "detect.c"
#include "gamever.c"  /* 6.0: the version of the player's game files, and whether the update is in */
#include "extgames.c" /* 6.0: Zelda: Ocarina of Time (Switch 2) and God of War III */
#include "update.c"
#include "checks.c"
#include "remote.c"   /* 4.3.8: the remote fix list */
#include "steam.c"    /* 5.0: Add to the Steam library */
#include "library.c"  /* 6.1: the player's own games (Steam, Epic, emulators, any program): library.ini, start, watch */
#include "extras.c"
#include "pad.c"
#include "music.c"    /* 6.0: the chosen game's own music (sce_sys\snd0.at9 of the player's copy), as on a console */
#include "theme.c"    /* 6.0: PORTHEON's color (RGB in the interface code below goes through ThemeRGB) */
#include "video.c"    /* 6.2: Media Foundation player for the vitrine (the player's own videos) */
#include "ui.c"
#include "online.c"
#undef RGB
#define RGB(r, g, b) RGB0(r, g, b)   /* 6.0: the rest of portheon.c keeps the plain colors */

/* 4.3.2: while a game runs and PORTHEON is minimized, its pictures (the games' art, a 3840x2160 picture for UFC 5 and 6,
   their tiles and the window's back buffer, ~70 MB) are freed and its working set handed back to Windows, so the game has
   the memory (UFC 6 uses ~12 GB of a 16 GB PC). Everything is drawn again from the files when the window comes back. */
static void PlayPark(void)
{
    for (int i = 0; i < NGAMES; i++) ForgetArt(i);
    FreeBmp(&g_neutralBg);
    if (g_back) { DeleteObject(g_back); g_back = NULL; g_backW = g_backH = 0; }
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

/* ---------------------------------------------------------------- desktop shortcuts (3.5.2)
   A game's shortcut on the desktop starts PORTHEON with --play <id>. PORTHEON gets the game ready as PLAY does (patches,
   FPS, controllers, engine, the game window's focus) and starts it, shows itself only while the game opens, and closes
   after the game's normal end (an error keeps it open with the explanation). The shortcut is made at each game's first
   PLAY (desktop_shortcut=0 in portheon.ini, the Game tab's switch, turns that off) or by the Game tab's button. Its icon
   is the game's own icon0.png from the player's dump, saved as an .ico in %LOCALAPPDATA%\PORTHEON\icons; without one,
   the port's launcher's or PORTHEON's. */
static int GameById(const wchar_t *id) { for (int i = 0; i < NGAMES; i++) if (!_wcsicmp(id, G[i].id)) return i; return -1; }
static BOOL ShortcutPath(int g, wchar_t *out)
{
    PWSTR desk = NULL;
    if (FAILED(SHGetKnownFolderPath(&FOLDERID_Desktop, 0, NULL, &desk)) || !desk) return FALSE;
    _snwprintf(out, MAX_PATH, L"%s\\%s - PORTHEON.lnk", desk, G[g].name); out[MAX_PATH - 1] = 0;
    CoTaskMemFree(desk); return TRUE;
}
static BOOL GameIco(int g, wchar_t *ico)   /* the game's icon0.png as a 256x256 PNG icon file */
{
    LoadArt(g); if (!g_icon[g]) return FALSE;
    wchar_t d[MAX_PATH], f[64]; LocalData(d, L"PORTHEON"); Join(d, d, L"icons"); CreateDirectoryW(d, NULL);
    _snwprintf(f, 64, L"%s.ico", G[g].id); Join(ico, d, f);
    GpBitmap *bmp = NULL; if (GdipCreateBitmapFromScan0(256, 256, 0, 0x26200A /* PixelFormat32bppARGB */, NULL, &bmp) || !bmp) return FALSE;
    GpGraphics *gr = NULL; GdipGetImageGraphicsContext((GpImage *)bmp, &gr);
    if (gr) { GdipSetInterpolationMode(gr, 7); GdipSetPixelOffsetMode(gr, 4); GdipDrawImageRectI(gr, g_icon[g], 0, 0, 256, 256); GdipDeleteGraphics(gr); }
    static const CLSID png = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
    IStream *st = NULL; BOOL ok = FALSE;
    if (SUCCEEDED(CreateStreamOnHGlobal(NULL, TRUE, &st)) && st && !GdipSaveImageToStream((GpImage *)bmp, st, &png, NULL)) {
        HGLOBAL hg = NULL; STATSTG ss; GetHGlobalFromStream(st, &hg); st->lpVtbl->Stat(st, &ss, STATFLAG_NONAME);
        DWORD n = ss.cbSize.LowPart, off = 22, wr = 0; BYTE *p = hg ? (BYTE *)GlobalLock(hg) : NULL;
        /* ICONDIR (reserved, type 1 = icon, 1 image) + its entry (256x256 written as 0, 1 plane, 32 bits, size, offset) */
        BYTE hd[22] = {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 32, 0}; memcpy(hd + 14, &n, 4); memcpy(hd + 18, &off, 4);
        HANDLE h = p ? CreateFileW(ico, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL) : INVALID_HANDLE_VALUE;
        if (h != INVALID_HANDLE_VALUE) { ok = WriteFile(h, hd, 22, &wr, NULL) && WriteFile(h, p, n, &wr, NULL) && wr == n; CloseHandle(h); }
        if (p) GlobalUnlock(hg);
    }
    if (st) st->lpVtbl->Release(st);
    GdipDisposeImage((GpImage *)bmp);
    return ok;
}
/* 6.0 final: a PORTHEON.exe that changed in place (an update, a new install over an older one) shows the old logo on
   its shortcuts and taskbar button until Windows' icon cache is told (iconrefresh.h). Once per new exe (its size and
   time), in the background, so the start does not wait. */
#ifndef PORTHEON_TEST   /* never from a test build (as the shortcut below) */
#include "iconrefresh.h"
static DWORD WINAPI IconRefreshThread(LPVOID unused)
{
    (void)unused; CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL, exe, MAX_PATH); IconRefreshAll(exe);
    CoUninitialize(); return 0;
}
static void IconRefreshOnce(void)
{
    wchar_t exe[MAX_PATH], stamp[64], seen[64]; WIN32_FILE_ATTRIBUTE_DATA fa;
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (!GetFileAttributesExW(exe, GetFileExInfoStandard, &fa)) return;
    _snwprintf(stamp, 64, L"%lx-%lx%08lx", fa.nFileSizeLow, fa.ftLastWriteTime.dwHighDateTime, fa.ftLastWriteTime.dwLowDateTime); stamp[63] = 0;
    GetPrivateProfileStringW(L"PORTHEON", L"icon_stamp", L"", seen, 64, g_pIni);
    if (!wcscmp(seen, stamp)) return;
    WritePrivateProfileStringW(L"PORTHEON", L"icon_stamp", stamp, g_pIni);
    HANDLE t = CreateThread(NULL, 0, IconRefreshThread, NULL, 0, NULL); if (t) CloseHandle(t);
}
#endif
/* 5.0: PORTHEON's own shortcut on the desktop, with its logo and its taskbar ID (made once, at the first start of 5.0,
   when the desktop has none; deleting it is the player's choice and it is not made again) */
#define PORTHEON_AUMID L"XAndromedaXR.PORTHEON"
static BOOL PortheonShortcut(void)
{
    PWSTR desk = NULL; wchar_t lnk[MAX_PATH], exe[MAX_PATH]; BOOL ok = FALSE;
    if (FAILED(SHGetKnownFolderPath(&FOLDERID_Desktop, 0, NULL, &desk)) || !desk) return FALSE;
    _snwprintf(lnk, MAX_PATH, L"%s\\PORTHEON.lnk", desk); lnk[MAX_PATH - 1] = 0; CoTaskMemFree(desk);
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    IShellLinkW *sl = NULL; IPersistFile *pf = NULL; IPropertyStore *ps = NULL;
    static const GUID iidPs = {0x886d8eeb, 0x8cf2, 0x4446, {0x8d, 0x02, 0xcd, 0xba, 0x1d, 0xbd, 0xcf, 0x99}};
    static const PROPERTYKEY pkId = {{0x9f4c2855, 0x9f79, 0x4b39, {0xa8, 0xd0, 0xe1, 0xd4, 0x2d, 0xe1, 0xd5, 0xf3}}, 5};
    if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl)) && sl) {
        sl->lpVtbl->SetPath(sl, exe); sl->lpVtbl->SetWorkingDirectory(sl, g_exeDir); sl->lpVtbl->SetIconLocation(sl, exe, 0);
        sl->lpVtbl->SetDescription(sl, L2(L"PORTHEON: os ports do UFC para PC", L"PORTHEON: the UFC PC ports"));
        if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &iidPs, (void **)&ps)) && ps) {
            PROPVARIANT pv; memset(&pv, 0, sizeof(pv)); pv.vt = VT_LPWSTR; pv.pwszVal = (LPWSTR)PORTHEON_AUMID;
            ps->lpVtbl->SetValue(ps, &pkId, &pv); ps->lpVtbl->Commit(ps); ps->lpVtbl->Release(ps);
        }
        if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf)) && pf) { ok = SUCCEEDED(pf->lpVtbl->Save(pf, lnk, TRUE)); pf->lpVtbl->Release(pf); }
        sl->lpVtbl->Release(sl);
    }
    return ok;
}
static BOOL MakeShortcut(int g, BOOL quiet)
{
    if (g < 0 || g >= NGAMES || SOON(g)) return FALSE;
    wchar_t lnk[MAX_PATH], exe[MAX_PATH], args[48], ico[MAX_PATH], desc[160]; BOOL ok = FALSE;
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (ShortcutPath(g, lnk)) {
        _snwprintf(args, 48, L"--play %s", G[g].id);
        if (!GameIco(g, ico)) { PortExe(g, ico); if (!FileExists(ico) || ExtractIconExW(ico, -1, NULL, NULL, 0) == 0) wcscpy(ico, exe); }
        _snwprintf(desc, 160, L2(L"Abre o %s pelo PORTHEON", L"Starts %s through PORTHEON"), G[g].name);
        IShellLinkW *sl = NULL; IPersistFile *pf = NULL;
        if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl)) && sl) {
            sl->lpVtbl->SetPath(sl, exe); sl->lpVtbl->SetArguments(sl, args); sl->lpVtbl->SetWorkingDirectory(sl, g_exeDir);
            sl->lpVtbl->SetDescription(sl, desc); sl->lpVtbl->SetIconLocation(sl, ico, 0);
            if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf)) && pf) { ok = SUCCEEDED(pf->lpVtbl->Save(pf, lnk, TRUE)); pf->lpVtbl->Release(pf); }
            sl->lpVtbl->Release(sl);
        }
    }
    if (!quiet) {
        if (ok) Msg(C_GOOD, L2(L"Atalho \"%s - PORTHEON\" criado na área de trabalho: ele abre o jogo direto, sem passar pela biblioteca.", L"Shortcut \"%s - PORTHEON\" made on the desktop: it starts the game straight away."), G[g].name);
        else Msg(C_BAD, L"%s", L2(L"Não consegui criar o atalho na área de trabalho.", L"Could not make the desktop shortcut."));
    }
    return ok;
}
/* 5.0 (the user: "the option to put the images in Steam as the icon and everything"): a non-Steam shortcut that starts
   the game through PORTHEON (PORTHEON.exe --play <id>, as the desktop shortcut), with Steam's library pictures. The
   pictures: games\<id>\steam\grid|hero|logo|wide .png/.jpg and icon.ico when the package has them (UFC 2: the user's
   set), else the player's own cover from the dump (art cache <id>_pic.png as the wide and hero picture) and the game's
   icon. Steam rewrites its shortcut list when it exits, so it has to be closed first. */
static void SteamPic(int g, const wchar_t *base, wchar_t *out)
{
    static const wchar_t *const ext[] = {L".png", L".jpg", L".ico"}; wchar_t d[MAX_PATH], f[64]; PortDir(g, d); out[0] = 0;
    for (int i = 0; i < 3 && !out[0]; i++) { wchar_t p[MAX_PATH]; _snwprintf(f, 64, L"steam\\%s%s", base, ext[i]); Join(p, d, f); if (FileExists(p)) wcscpy(out, p); }
}
/* 7.1 (steam-3): what the button did, one line per click in %LOCALAPPDATA%\PORTHEON\steam.txt (MakeReport adds it):
   5.0-7.0 kept no trace, so no report could show the one-byte-short file. Kept under 64 KB. */
static void SteamTraceLog(int g, const wchar_t *trace)
{
    wchar_t d[MAX_PATH], f[MAX_PATH]; LocalData(d, L"PORTHEON"); CreateDirectoryW(d, NULL); Join(f, d, L"steam.txt");
    WIN32_FILE_ATTRIBUTE_DATA fa; BOOL big = GetFileAttributesExW(f, GetFileExInfoStandard, &fa) && (fa.nFileSizeHigh || fa.nFileSizeLow > (64u << 10));
    HANDLE h = CreateFileW(f, big ? GENERIC_WRITE : FILE_APPEND_DATA, FILE_SHARE_READ, NULL, big ? CREATE_ALWAYS : OPEN_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME t; GetLocalTime(&t); wchar_t line[640];
    _snwprintf(line, 640, L"%04d-%02d-%02d %02d:%02d:%02d PORTHEON %s, %s: %s\r\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, APP_VER, G[g].id, trace && trace[0] ? trace : L"-");
    line[639] = 0;
    char u[2048]; int n = WideCharToMultiByte(CP_UTF8, 0, line, -1, u, sizeof(u), NULL, NULL); DWORD w;
    if (n > 1) WriteFile(h, u, (DWORD)(n - 1), &w, NULL);
    CloseHandle(h);
}
static void AddToSteam(int g)
{
    if (g < 0 || g >= NGAMES) return;
    wchar_t sp[MAX_PATH];
    if (!SteamFindPath(sp, MAX_PATH)) { SteamTraceLog(g, L"not started: Steam was not found"); Msg(C_BAD, L"%s", L2(L"A Steam não foi encontrada neste PC.", L"Steam was not found on this PC.")); return; }
    if (SteamIsRunning()) { SteamTraceLog(g, L"not started: Steam is running"); Msg(C_ORANGE, L"%s", L2(L"Feche a Steam primeiro (clique com o botão direito no ícone dela perto do relógio > Sair): ela regrava a lista de jogos ao fechar e o jogo sumiria.",
                                                   L"Close Steam first (right-click its icon by the clock > Exit): it rewrites its game list when it exits and the game would vanish.")); return; }
    wchar_t exe[MAX_PATH], args[48], name[80], grid[MAX_PATH], hero[MAX_PATH], logo[MAX_PATH], wide[MAX_PATH], ico[MAX_PATH], err[300];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    _snwprintf(args, 48, L"--play %s", G[g].id); _snwprintf(name, 80, L"%s", G[g].card); name[79] = 0;
    SteamPic(g, L"grid", grid); SteamPic(g, L"hero", hero); SteamPic(g, L"logo", logo); SteamPic(g, L"wide", wide); SteamPic(g, L"icon", ico);
    if (!wide[0] || !hero[0]) { wchar_t pic[MAX_PATH]; LoadArt(g); ArtCachePath(g, L"pic", pic); if (FileExists(pic)) { if (!wide[0]) wcscpy(wide, pic); if (!hero[0]) wcscpy(hero, pic); } }
    if (!ico[0] && !GameIco(g, ico)) wcscpy(ico, exe);   /* 7.1: steam.c names an .exe as the icon, it no longer copies it */
    BOOL ok = SteamAddShortcut(name, exe, g_exeDir, args, grid, hero, logo, wide, ico, err, 300);
    SteamTraceLog(g, SteamLastTrace());
    /* 7.1 (steam-4): do not click again before Steam has been opened; how the first backup gives the old list back */
    if (ok) {
        wchar_t back[300] = L""; int rs = SteamLastRestored();
        if (rs > 0) _snwprintf(back, 300, L2(L" %d jogo(s) não-Steam que estavam na cópia de segurança e não estavam mais na lista voltaram para ela.", L" %d non-Steam game(s) that were in the backup and no longer in the list are back in it."), rs);
        back[299] = 0;
        /* the first backup exists only when Steam had a list before (state 1 valid, 2 repaired); it is the list as it was
           before PORTHEON's first write, kept for good.
           7.1 review (correctness#3, player#3, safety#1): said so only when the backup really is a list from before any
           PORTHEON entry (SteamLastBakKind 1): 5.0-7.0 overwrote it at every click, so for a 7.0 player who clicked
           twice it is PORTHEON's own file. Otherwise, when Steam had a list, the copy of the list as it was right before
           this click (.portheon.last.bak, written whole by steam.c) is named instead */
        int bk = SteamLastBakKind();
        const wchar_t *bak = bk == 1
            ? L2(L" A lista como era antes do primeiro clique do PORTHEON ficou guardada na pasta da Steam, em userdata > (número da sua conta) > config > shortcuts.vdf.portheon.bak. Para voltar a ela: feche a Steam, apague o shortcuts.vdf e renomeie essa cópia para shortcuts.vdf.",
                 L" The list as it was before PORTHEON's first click is kept in Steam's folder, in userdata > (your account's number) > config > shortcuts.vdf.portheon.bak. To go back to it: close Steam, delete shortcuts.vdf and rename that copy to shortcuts.vdf.")
            : SteamLastState() >= 1
            ? L2(L" A lista como estava logo antes deste clique ficou guardada na pasta da Steam, em userdata > (número da sua conta) > config > shortcuts.vdf.portheon.last.bak. Para voltar a ela: feche a Steam, apague o shortcuts.vdf e renomeie essa cópia para shortcuts.vdf.",
                 L" The list as it was right before this click is kept in Steam's folder, in userdata > (your account's number) > config > shortcuts.vdf.portheon.last.bak. To go back to it: close Steam, delete shortcuts.vdf and rename that copy to shortcuts.vdf.") : L"";
        Msg(C_GOOD, L2(L"\"%s\" foi adicionado à biblioteca da Steam, com capa e ícone. Abra a Steam: ele aparece em Biblioteca e no Big Picture e abre o jogo direto pelo PORTHEON. Não clique no botão de novo antes de abrir a Steam e conferir; para clicar outra vez, feche a Steam por completo (ícone perto do relógio > Sair).%s%s",
                       L"\"%s\" was added to the Steam library, with its cover and icon. Open Steam: it shows in the Library and in Big Picture and starts the game straight through PORTHEON. Do not click the button again before you open Steam and check; to click again, close Steam completely (its icon by the clock > Exit).%s%s"), name, back, bak);
    }
    else if (SteamLastState() == -1)   /* 7.1: a file PORTHEON cannot write back whole was left as it is */
        Msg(C_BAD, L2(L"O PORTHEON não mexeu na lista da Steam: o arquivo shortcuts.vdf dela não está num formato que o PORTHEON conheça, e gravar por cima poderia apagar os seus outros jogos não-Steam. Adicione pela própria Steam: Jogos > Adicionar um jogo não-Steam à biblioteca > Procurar > PORTHEON.exe; depois, em Propriedades > Opções de inicialização, escreva --play %s.",
                      L"PORTHEON left Steam's list alone: its shortcuts.vdf file is in a form PORTHEON does not know, and writing over it could erase your other non-Steam games. Add it in Steam itself: Games > Add a Non-Steam Game to My Library > Browse > PORTHEON.exe; then, in Properties > Launch options, type --play %s."), G[g].id);
    else Msg(C_BAD, L2(L"O PORTHEON não conseguiu adicionar o jogo à Steam: %s", L"Could not add it to Steam: %s"), err);
}
static void AutoShortcut(int g)   /* at a game's first PLAY (not the shortcut's own start, not under Wine) */
{
    wchar_t k[48]; _snwprintf(k, 48, L"shortcut_%s", G[g].id);
    if (g_directPlay >= 0 || UnderWine() || !PIni(L"desktop_shortcut", 1) || PIni(k, 0)) return;
    PIniSet(k, 1);   /* once: a shortcut the player deleted does not come back */
    if (MakeShortcut(g, TRUE)) g_shortcutNew = g;
}
/* the shortcut's own start: the game's page with PLAY pressed; an unready game stays on its page with the reason */
/* 4.3.6: which game PORTHEON leaves running (LateExited reads it at the next start). 5.2: also written when the player
   closes PORTHEON by hand during a game (WM_CLOSE: close_on_play=0, Wine, a UFC 2 online host, the 90 s UFC 3/4 watch)
   and when Windows ends the session, so that running_<id> is cleared by the game's real end and not counted as a crash
   by RunningLeft */
static void PendingWrite(void)
{
    if (!g_proc || g_procGame < 0) return;
    wchar_t b[32];
    WritePrivateProfileStringW(L"PORTHEON", L"pending_game", G[g_procGame].id, g_pIni);
    _snwprintf(b, 32, L"%lu", GetProcessId(g_proc)); WritePrivateProfileStringW(L"PORTHEON", L"pending_pid", b, g_pIni);
    _snwprintf(b, 32, L"%llu", ProcBorn(g_proc)); WritePrivateProfileStringW(L"PORTHEON", L"pending_born", b, g_pIni);
    _snwprintf(b, 32, L"%lld", UnixNow() - (long long)((GetTickCount64() - g_playStart) / 1000)); WritePrivateProfileStringW(L"PORTHEON", L"pending_t0", b, g_pIni);
}
/* 4.3.6: see CloseOnPlay. The game has the focus for 3 s: PORTHEON notes it and exits (the game's launcher keeps it) */
static void CloseTimer(void)
{
    if (!g_proc || g_procGame < 0) { KillTimer(g_wnd, 12); return; }
    GameWndName();   /* single window: until the engine has taken it */
    /* single window, verified 11-13: in 1 start of 10 (UFC 1) FocusTimer did not see the game in front at its one look,
       the game then had the foreground and PORTHEON never heard of it: it stayed minimized, with its taskbar button,
       20 s and more. This tick asks Windows again (one call), and a PORTHEON that minimized instead of hiding then
       hides as FocusTimer would have (this timer only runs where PORTHEON closes during the game: CloseOnPlay, not
       Zelda/God of War). */
    if (!g_gameFocused && g_gameWnd && GetForegroundWindow() == g_gameWnd) g_gameFocused = TRUE;
    if (g_gameFocused && IsWindowVisible(g_wnd) && IsIconic(g_wnd)) ShowWindow(g_wnd, SW_HIDE);
    if (!g_gameFocused || GetTickCount64() < g_closeAt) return;   /* a game that opened behind: the player clicks it first */
    KillTimer(g_wnd, 12);
    PendingWrite();
    InterlockedExchange(&g_watchStop, 1); FocusWatchStop();
    DestroyWindow(g_wnd);
}
static long long NewestWrite(int g)   /* when the game last wrote: UFC6.ini/UFC5.ini, or UFC 1-4's newest log (Unix time) */
{
    wchar_t f[MAX_PATH] = L"", pat[MAX_PATH], d[MAX_PATH]; FILETIME best = {0, 0}; WIN32_FIND_DATAW fd;
    if (G[g].ps5) { PsIni(g, f); HANDLE h = FindFirstFileW(f, &fd); if (h != INVALID_HANDLE_VALUE) { best = fd.ftLastWriteTime; FindClose(h); } }
    else if (X360G(g)) { GameDir(g, d); X3Normalize(d); Join(f, d, L"ufc3.log"); HANDLE h = FindFirstFileW(f, &fd); if (h != INVALID_HANDLE_VALUE) { best = fd.ftLastWriteTime; FindClose(h); } }   /* 5.0 */
    else if (G[g].data) {
        LocalData(d, G[g].data); Join(d, d, L"logs"); Join(pat, d, L"*.log"); HANDLE h = FindFirstFileW(pat, &fd);
        if (h != INVALID_HANDLE_VALUE) { do if (CompareFileTime(&fd.ftLastWriteTime, &best) > 0) best = fd.ftLastWriteTime; while (FindNextFileW(h, &fd)); FindClose(h); }
    }
    ULARGE_INTEGER u; u.LowPart = best.dwLowDateTime; u.HighPart = best.dwHighDateTime;
    return u.QuadPart ? (long long)(u.QuadPart / 10000000ull) - 11644473600ll : 0;
}
/* 4.3.6: a PORTHEON that closed during a game (CloseTimer) opens again: the game still running is followed as if
   PORTHEON had started it (STOP works, its end is told); a game that ended is told now, as OnExited would have */
/* 5.2: the end of a UFC 1-4 session from its log. The port's launcher writes the engine's real exit code there
   ("sacredfire-game.exe exited with code 3 (a fault; the report is above)", 47 normal ends and every crash in this PC's
   logs had it), so that line wins. Without it (the engine killed, a frozen PC) the engine's own words tell the ends
   PORTHEON acts on. TRUE when the log says anything; only a log written during the session counts (LateExited). */
static BOOL LogEndCode(int g, DWORD *code)
{
    char *t = ReadLogTail(g, 2u << 20); if (!t) return FALSE;
    const char *last = NULL; for (const char *m = t; (m = strstr(m, "sacredfire-game.exe exited with code ")) != NULL; m++) last = m;
    BOOL ok = TRUE;
    if (last) { *code = (DWORD)strtoul(last + 37, NULL, 10); if ((*code == 0 || *code == 1) && FrozenCloseText(t)) *code = 23; }   /* 7.1 (U2-HANG F2) */
    else if (strstr(t, "STOP: out of memory (")) *code = 25;   /* 6.1: the v3.0 engine's out-of-memory stop (_exit(25)) */
    else if (LogTextNoCommit(t)) *code = 3;
    else if (strstr(t, "STUCK:") || strstr(t, "ErrorOutOfDeviceMemory") || strstr(t, "No space left on device") || strstr(t, "Required Vulkan extension unavailable")) *code = 23;
    else if (strstr(t, "Guest fault:") || strstr(t, "Fault: exception")) *code = 3;
    else ok = FALSE;
    free(t); return ok;
}
/* 4.3.6. 5.2: returns 1 when the game is still open (PORTHEON follows it), 2 when its end was told (OnExited), 0 when
   there was nothing to tell; it also runs on a desktop shortcut's start (wWinMain), where the next CloseTimer used to
   overwrite pending_* before anything was said. The played time (the log's last write minus the start) goes to
   OnExited, so NewEngineFallback's 90 s rule and the crash streak see the real session. */
static BOOL LateEndQuiet(int g, DWORD code, const wchar_t *why)   /* 7.1 review: LateExited's "nothing to say" */
{
    return (code == 0 || code == 1) && wcscmp(why, L"bench") && wcscmp(why, L"warmup") && !(G[g].ps5 && Ufc6PlayNotePending(g));
}
static int LateExited(int *game)   /* 5.2: *game = the game told (or followed), -1 when nothing was */
{
    if (game) *game = -1;
    wchar_t id[16], bs[32]; GetPrivateProfileStringW(L"PORTHEON", L"pending_game", L"", id, 16, g_pIni);
    if (!id[0]) return 0;
    int g = GameById(id); DWORD pid = (DWORD)PIni(L"pending_pid", 0);
    GetPrivateProfileStringW(L"PORTHEON", L"pending_born", L"0", bs, 32, g_pIni); ULONGLONG born = _wcstoui64(bs, NULL, 10);
    GetPrivateProfileStringW(L"PORTHEON", L"pending_t0", L"0", bs, 32, g_pIni); long long t0 = _wtoi64(bs);
    PendingClear();
    if (g < 0 || g_proc) return 0;
    HANDLE h = pid ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid) : NULL;
    if (h && born && ProcBorn(h) == born && WaitForSingleObject(h, 0) == WAIT_TIMEOUT) {
        long long ran = t0 ? UnixNow() - t0 : 0; if (ran < 0) ran = 0;
        g_proc = h; g_procGame = g; g_playStart = GetTickCount64() - (ULONGLONG)ran * 1000; InterlockedIncrement(&g_watchSession);
        CloseHandle(CreateThread(NULL, 0, WaitThread, g_proc, 0, NULL));
        PadPause(TRUE);   /* the controllers stay with the game */
        if (g_wnd) { KillTimer(g_wnd, 7); KillTimer(g_wnd, 9); VitStop(); YtDestroy(); }   /* 6.0 final: nothing ticks while it follows the game (OnExited starts them again) */
        Msg(C_SUB, L2(L"O %s ainda está aberto.",L"%s is still open."), G[g].name);
        if (game) *game = g;
        return 1;
    }
    if (h) CloseHandle(h);
    long long end = NewestWrite(g);
    ULONGLONG playedMs = t0 && end > t0 && end - t0 < 24 * 3600 ? (ULONGLONG)(end - t0) * 1000 : 0;
    DWORD code = 0;
    if (G[g].ps5) code = (DWORD)GetInt(g, L"last_code", 0);
    else if (X360G(g) || EXTG(g)) { if (playedMs) { PlayTimeAdd(g, playedMs); SessionRecord(g, playedMs, 0); } RunningClear(g); StreakAfter(g, 0, playedMs); return 0; }   /* 5.0: UFC3.exe keeps no exit code: its end is just the game closing (6.0: Zelda, God of War III) */
    else if (!t0 || end >= t0) LogEndCode(g, &code);   /* UFC 1-4: an older log tells nothing about this session */
    wchar_t why[32] = L""; if (G[g].ps5) GetStr(g, L"last_reason", why, 32, L"");
    /* a normal end: nothing to say. 7.1 review: a UFC 6 end with UFC6.exe's note (play_note) is told through OnExited
       like an error end (return 2: the page stays with the message, a shortcut's start does not start the game at once,
       which would empty the note first) */
    if (LateEndQuiet(g, code, why)) {
        if (playedMs) { PlayTimeAdd(g, playedMs); SessionRecord(g, playedMs, code); }   /* 6.2 review: the Atividade card shows this session too */
        RunningClear(g); StreakAfter(g, code, playedMs);
        return 0;
    }
    /* OnExited adds the played time, clears running_<id> and counts the streak */
    g_proc = NULL; g_procGame = g; g_playStart = playedMs ? GetTickCount64() - playedMs : 0; g_directPlay = -1;
    OnExited(code);
    if (game) *game = g;
    return 2;
}
/* 5.2: a game whose running_<id> is still set, with no pending_* (LateExited cleared that game's) and nothing of it
   running: its session never reached an end (the PC froze or restarted in the game, a blue screen, PORTHEON killed).
   That counts as a crash for the compatible mode's offer, and is said once. 5.2: quiet (LateExited told an end or
   follows a game) counts it without the message, which would replace the one LateExited/OnExited showed. */
static void RunningLeft(BOOL quiet)
{
    for (int g = 0; g < NGAMES; g++) {
        if (!StreakGame(g) || !GIni(L"running", g, 0) || (g_proc && g_procGame == g)) continue;
        if (PS4G(g) ? FindProcess(L"sacredfire-game.exe") != 0 : (FindProcess(L"ufc3-launcher-l1.exe") || FindProcess(L"ufc3-original-v48.exe"))) continue;   /* still running elsewhere */
        RunningClear(g); StreakAfter(g, 0xFFFFFFFFu, 0);
        if (!quiet) Msg(C_ORANGE, L2(L"A última sessão do %s não terminou (o PC travou ou reiniciou durante o jogo, ou o PORTHEON foi fechado à força). Se acontecer de novo, o PORTHEON oferece o modo compatível.",
                         L"The last %s session did not end (the PC froze or restarted during the game, or PORTHEON was force-closed). If it happens again, PORTHEON offers the compatible mode."), G[g].name);
    }
}
static void DirectPlay(int g)
{
    if (g < 0 || SOON(g)) return;
    if (g != g_sel) Select(g);
    g_overlay = OV_NONE; g_directPlay = g;
    Play();
    if (!g_proc) { g_directPlay = -1; g_view = V_GAME; g_tab = TAB_OVERVIEW; LoadEdits(); }
    InvalidateRect(g_wnd, NULL, FALSE);
}

/* ---------------------------------------------------------------- the news of a new version (3.5.2)
   Shown once on the first start of each version, however it came (the updater, or a new zip extracted by hand). A first
   start gets the install guide instead, and a desktop shortcut's start leaves them for the next normal start. */
static void NewsOnce(void)
{
    wchar_t seen[32]; GetPrivateProfileStringW(L"PORTHEON", L"news_seen", L"", seen, 32, g_pIni);
    if (!wcscmp(seen, APP_VER)) return;
    WritePrivateProfileStringW(L"PORTHEON", L"news_seen", APP_VER, g_pIni);
    if (PIni(L"guide_seen", 0) && g_overlay == OV_NONE) g_overlay = OV_NEWS;
}

/* ---------------------------------------------------------------- start-up safety (3.5.2)
   A player's PORTHEON "opened and closed by itself". Two ways to that, now handled:
   - a PORTHEON that crashed vanished without a word: CrashReport writes %LOCALAPPDATA%\PORTHEON\crash.txt (code,
     module and offset, the PORTHEON addresses on the stack) and says where it is;
   - a PORTHEON stuck in the background (its window not answering, or no window) kept the single-instance lock, and
     every new start found it and quit at once, with nothing on screen: OtherInstance asks to close the stuck one. */
static DWORD WINAPI CrashBox(void *text) { MessageBoxW(NULL, (const wchar_t *)text, L"PORTHEON", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND); return 0; }
/* 6.1: 0x406D1388 is Windows' "set thread name" exception (MS_VC_EXCEPTION), raised by RaiseException to name a thread
   for a debugger: continuable and harmless. An RX 580 player's PORTHEON closed on it (proxima-versao-relatorios-1108),
   with a program on the PC (an overlay, a game booster, a debugger or cheat tool) breaking the usual handling; SDL3's
   gamepad threads (pad.c PadStart, g_padResync after a game) name theirs. ThreadNameVeh, first in line from start-up,
   swallows it; CrashReport does the same as a second net, and neither writes crash.txt for it. */
#define MS_VC_EXCEPTION 0x406D1388u
static LONG CALLBACK ThreadNameVeh(EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    return er->ExceptionCode == MS_VC_EXCEPTION && !(er->ExceptionFlags & EXCEPTION_NONCONTINUABLE) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}
static LONG WINAPI CrashReport(EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == MS_VC_EXCEPTION && !(ep->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE))
        return EXCEPTION_CONTINUE_EXECUTION;   /* 6.1: a thread's name, not a crash (above) */
    static volatile LONG once; if (InterlockedExchange(&once, 1)) return EXCEPTION_EXECUTE_HANDLER;
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    wchar_t d[MAX_PATH], f[MAX_PATH], mod[MAX_PATH] = L"?"; LocalData(d, L"PORTHEON"); CreateDirectoryW(d, NULL); Join(f, d, L"crash.txt");
    HMODULE hm = NULL; ULONG_PTR addr = (ULONG_PTR)er->ExceptionAddress, base = 0;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)er->ExceptionAddress, &hm) && hm) {
        GetModuleFileNameW(hm, mod, MAX_PATH); base = (ULONG_PTR)hm;
    }
    SYSTEMTIME t; GetLocalTime(&t); OSVERSIONINFOW ov = {sizeof(ov)};
    typedef LONG (WINAPI *RGV)(OSVERSIONINFOW *); RGV rgv = (RGV)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"); if (rgv) rgv(&ov);
    static char b[4096]; int n = snprintf(b, sizeof(b), "PORTHEON %ls crash, %04d-%02d-%02d %02d:%02d:%02d, Windows %lu.%lu.%lu\r\ncode 0x%08lX at %ls+0x%llX\r\nview %d, tab %d, overlay %d, game %d, playing %d\r\nPORTHEON addresses on the stack:",
                                 APP_VER, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, ov.dwMajorVersion, ov.dwMinorVersion, ov.dwBuildNumber,
                                 (unsigned long)er->ExceptionCode, PathFindFileNameW(mod), (unsigned long long)(addr - base), g_view, g_tab, g_overlay, g_sel, g_proc != NULL);
    {   /* return addresses inside PORTHEON.exe near the crash, to place it in this build */
        BYTE *exeBase = (BYTE *)GetModuleHandleW(NULL); IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(exeBase + ((IMAGE_DOS_HEADER *)exeBase)->e_lfanew);
        ULONG_PTR lo = (ULONG_PTR)exeBase, hi = lo + nt->OptionalHeader.SizeOfImage, sp = (ULONG_PTR)ep->ContextRecord->Rsp;
        MEMORY_BASIC_INFORMATION mb; int found = 0;
        if (VirtualQuery((void *)sp, &mb, sizeof(mb))) {
            ULONG_PTR end = (ULONG_PTR)mb.BaseAddress + mb.RegionSize;
            for (ULONG_PTR p = sp; p + 8 <= end && p < sp + 16384 && found < 24 && n > 0 && n < (int)sizeof(b) - 32; p += 8) {
                ULONG_PTR v = *(ULONG_PTR *)p; if (v > lo && v < hi) { n += snprintf(b + n, sizeof(b) - n, " +0x%llX", (unsigned long long)(v - lo)); found++; }
            }
        }
        if (n > 0 && n < (int)sizeof(b) - 4) n += snprintf(b + n, sizeof(b) - n, "\r\n");
    }
    HANDLE h = CreateFileW(f, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { DWORD wr; if (n > 0) WriteFile(h, b, (DWORD)n, &wr, NULL); CloseHandle(h); }
    static wchar_t m[800]; _snwprintf(m, 800, L2(L"O PORTHEON fechou por um erro (código 0x%08lX em %s).\n\nO relatório está em:\n%s\n\nMande esse arquivo no Discord para a gente corrigir: " URL_DISCORD,
                                                 L"PORTHEON closed on an error (code 0x%08lX in %s).\n\nThe report is at:\n%s\n\nSend that file on Discord so it gets fixed: " URL_DISCORD),
                                  (unsigned long)er->ExceptionCode, PathFindFileNameW(mod), f); m[799] = 0;
    /* the message from a thread of its own: a message box on the crashed thread would run its window's timers and
       paints again, crash there once more and vanish at once */
    HANDLE th = CreateThread(NULL, 0, CrashBox, m, 0, NULL);
    if (th) { WaitForSingleObject(th, INFINITE); CloseHandle(th); }
    return EXCEPTION_EXECUTE_HANDLER;
}
#ifdef PORTHEON_TEST   /* a test build next to the player's own PORTHEON: its own lock and window class */
#define MUTEX_NAME L"PORTHEON_Test_Single"
#define WNDCLASS_NAME L"PortheonTestWindow"
#else
#define MUTEX_NAME L"PORTHEON_Launcher_Single"
#define WNDCLASS_NAME L"PortheonLauncher"
#endif
static BOOL HasUpdaterWindow(DWORD pid)   /* PORTHEON.exe --apply-update installing an update: never closed */
{
    HWND u = FindWindowW(L"PortheonUpdate", NULL); DWORD up = 0; if (u) GetWindowThreadProcessId(u, &up);
    return u && up == pid;
}
/* 4.4.0: the window that started the PLAY instance (Play) waits for that one's window (timer 14), then closes */
typedef struct { DWORD self; BOOL found; } HandoffFind;
static BOOL CALLBACK FindOtherPortheon(HWND w, LPARAM lp)
{
    HandoffFind *f = (HandoffFind *)lp; wchar_t cls[64]; DWORD pid = 0;
    if (!IsWindowVisible(w) || !GetClassNameW(w, cls, 64) || wcscmp(cls, WNDCLASS_NAME)) return TRUE;
    GetWindowThreadProcessId(w, &pid);
    if (pid && pid != f->self) { f->found = TRUE; return FALSE; }
    return TRUE;
}
static void HandoffTimer(void)
{
    HandoffFind f = {GetCurrentProcessId(), FALSE}; EnumWindows(FindOtherPortheon, (LPARAM)&f);
    if (f.found) { KillTimer(g_wnd, 14); Sleep(150); DestroyWindow(g_wnd); return; }   /* its first frame is drawn by then */
    if (GetTickCount64() - g_handoffAt > 8000) { KillTimer(g_wnd, 14); DestroyWindow(g_wnd); }
}
/* another PORTHEON holds the single-instance lock: TRUE when this one should go on starting */
static BOOL OtherInstance(HANDLE single, const wchar_t *play)
{
    HWND o = NULL; DWORD_PTR r = 0; BOOL alive = FALSE;
    /* up to 8 s: the other one may still be starting (its splash, reading the graphics card) */
    for (ULONGLONG t0 = GetTickCount64(); !alive && GetTickCount64() - t0 < 8000; ) {
        o = FindWindowW(WNDCLASS_NAME, NULL);
        if (o) alive = SendMessageTimeoutW(o, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 2000, &r) != 0;
        if (!alive) Sleep(250);
    }
    if (alive) {   /* to the front, with the shortcut's game if there is one */
        if (play[0]) { COPYDATASTRUCT cd = {0x504C4159 /* "PLAY" */, (DWORD)((wcslen(play) + 1) * sizeof(wchar_t)), (void *)play}; SendMessageTimeoutW(o, WM_COPYDATA, 0, (LPARAM)&cd, SMTO_ABORTIFHUNG, 4000, &r); }
        ShowWindow(o, IsIconic(o) ? SW_RESTORE : SW_SHOW); SetForegroundWindow(o);
        return FALSE;
    }
    if (MessageBoxW(NULL, L2(L"Já existe um PORTHEON aberto, mas ele travou e não responde (por isso o PORTHEON parecia abrir e fechar sozinho).\n\nFechar o PORTHEON travado e abrir de novo?",
                             L"A PORTHEON is already open, but it is stuck and does not answer (that is why PORTHEON seemed to open and close by itself).\n\nClose the stuck PORTHEON and start again?"),
                    L"PORTHEON", MB_YESNO | MB_ICONWARNING | MB_SETFOREGROUND) != IDYES) return FALSE;
    DWORD stuck = 0; if (o) GetWindowThreadProcessId(o, &stuck);
    wchar_t me[MAX_PATH]; GetModuleFileNameW(NULL, me, MAX_PATH); const wchar_t *myName = PathFindFileNameW(me);
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32W pe = {sizeof(pe)};
    if (sn != INVALID_HANDLE_VALUE) {
        if (Process32FirstW(sn, &pe)) do {
            DWORD pid = pe.th32ProcessID;
            if (pid == GetCurrentProcessId() || (stuck ? pid != stuck : _wcsicmp(pe.szExeFile, myName)) || HasUpdaterWindow(pid)) continue;
            HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
            if (h) { TerminateProcess(h, 1); WaitForSingleObject(h, 3000); CloseHandle(h); }
        } while (Process32NextW(sn, &pe));
        CloseHandle(sn);
    }
    DWORD w = WaitForSingleObject(single, 5000);   /* the lock of the closed one comes to this one */
    return w == WAIT_OBJECT_0 || w == WAIT_ABANDONED;
}

/* ---------------------------------------------------------------- selection and actions */
static void LoadEdits(void)
{
    wchar_t b[MAX_PATH];
    GetStr(g_sel, L"game", b, MAX_PATH, L""); SetWindowTextW(g_editGame, b);
    SetWindowTextW(g_editPlayer, CurProf()->name);   /* 4.0: the name in the game is the profile's */
    if (g_sel == 2) { GetStr(2, L"online_server", b, MAX_PATH, L""); SetWindowTextW(g_editAddr, b); }
    if (g_sel == 1) { GetPrivateProfileStringW(L"PORTHEON", L"join_UFC2", L"", b, MAX_PATH, g_pIni); SetWindowTextW(g_editAddr, b); }   /* 4.3.6 */
}
static void SaveEdits(void)
{
    if (g_csel < 0 && SOON(g_sel)) return;   /* 6.2: nothing of GTA VI is saved */
    wchar_t b[MAX_PATH], cur[MAX_PATH];
    GetWindowTextW(g_editGame, b, MAX_PATH); StripSlash(b); if (X360G(g_sel)) X3Normalize(b); GetStr(g_sel, L"game", cur, MAX_PATH, L"");
    if (wcscmp(b, cur)) { PutStr(g_sel, L"game", b); ForgetArt(g_sel); }
    GetWindowTextW(g_editPlayer, b, 32);
    { wchar_t *s = b; while (*s == L' ') s++; memmove(b, s, (wcslen(s) + 1) * sizeof(wchar_t)); size_t l = wcslen(b); while (l && b[l - 1] == L' ') b[--l] = 0; }
    if (b[0] && wcscmp(b, CurProf()->name)) { wcsncpy(CurProf()->name, b, 31); CurProf()->name[31] = 0; ProfilesSave(); }
    if (G[g_sel].ps5) { wchar_t u[32]; GetStr(g_sel, L"user", u, 32, L""); if (wcscmp(u, CurProf()->name)) PutStr(g_sel, L"user", CurProf()->name); }   /* 4.0.2: UFC 5's name is the profile's too */
    if (g_sel == 2) {
        GetWindowTextW(g_editAddr, b, 64);
        { wchar_t *s = b; while (*s == L' ') s++; memmove(b, s, (wcslen(s) + 1) * sizeof(wchar_t)); size_t l = wcslen(b); while (l && b[l - 1] == L' ') b[--l] = 0; }
        GetStr(2, L"online_server", cur, MAX_PATH, L"");
        if (wcscmp(b, cur)) PutStr(2, L"online_server", b);
    }
    if (g_sel == 1) {   /* 4.3.6: UFC 2 online */
        GetWindowTextW(g_editAddr, b, 64);
        { wchar_t *s = b; while (*s == L' ') s++; memmove(b, s, (wcslen(s) + 1) * sizeof(wchar_t)); size_t l = wcslen(b); while (l && b[l - 1] == L' ') b[--l] = 0; }
        WritePrivateProfileStringW(L"PORTHEON", L"join_UFC2", b[0] ? b : NULL, g_pIni);
    }
}
static void Select(int g)
{
    if (g < 0 || g >= NGAMES || (g == g_sel && g_csel < 0)) return;   /* 6.1: from a custom game back to the same built-in one */
    SaveEdits(); FeatureChanged(g_csel >= 0 ? -1 : g_sel); g_sel = g; g_kvGame = -1; g_msg[0] = 0;
    if (g_csel >= 0) { g_csel = -1; PIniSet(L"selected_custom", 0); }   /* 6.1 */
    WritePrivateProfileStringW(L"PORTHEON", L"selected_game", G[g].id, g_pIni);
    LoadEdits();
    PadHapticGame(g);   /* 6.0: each game has its own feel on the controller (only when the controller moved the focus) */
    MusicFollow();      /* 6.0: the game's music, after a short pause on it */
    InvalidateRect(g_wnd, NULL, FALSE);
}

static BOOL PickFolder(wchar_t *path, const wchar_t *title)
{
    IFileOpenDialog *d = NULL; BOOL ok = FALSE;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&d))) return FALSE;
    DWORD o; d->lpVtbl->GetOptions(d, &o); d->lpVtbl->SetOptions(d, o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM); d->lpVtbl->SetTitle(d, title);
    if (path[0]) { IShellItem *si; if (SUCCEEDED(SHCreateItemFromParsingName(path, NULL, &IID_IShellItem, (void **)&si))) { d->lpVtbl->SetFolder(d, si); si->lpVtbl->Release(si); } }
    if (SUCCEEDED(d->lpVtbl->Show(d, g_wnd))) {
        IShellItem *it; if (SUCCEEDED(d->lpVtbl->GetResult(d, &it))) {
            PWSTR p; if (SUCCEEDED(it->lpVtbl->GetDisplayName(it, SIGDN_FILESYSPATH, &p))) { wcsncpy(path, p, MAX_PATH); CoTaskMemFree(p); ok = TRUE; }
            it->lpVtbl->Release(it);
        }
    }
    d->lpVtbl->Release(d); return ok;
}

/* 3.4.2: a whole drive or a Microsoft Store / Xbox app folder as the game folder. The port looks through the folder
   for the game and Windows refuses it entry into D:\WpSystem ("OSError: [WinError 1337] The security ID structure is
   invalid: 'D:\\WpSystem\\eboot.bin'", player's screenshot of 2026-10-22). Returns the message to show, or NULL. */
static const wchar_t *BadGameDir(const wchar_t *p)
{
    size_t n = wcslen(p);
    /* 4.0.3: "D:\\", "D:\." and the like are the drive too (the ports then searched the whole drive and stopped on
       D:\WpSystem, WinError 1337) */
    wchar_t full[MAX_PATH]; BOOL root = n && GetFullPathNameW(p, MAX_PATH, full, NULL) && PathIsRootW(full);
    if (n && (root || n <= 3 || (p[1] == L':' && n == 2)))
        return L2(L"Isso é o disco inteiro. Escolha a pasta do próprio jogo (a que tem eboot.bin, sce_sys e sce_module), por exemplo D:\\Jogos\\UFC 4.",
                  L"That is a whole drive. Pick the game's own folder (the one with eboot.bin, sce_sys and sce_module), for example D:\\Games\\UFC 4.");
    if (StrStrIW(p, L"\\WpSystem") || StrStrIW(p, L"\\WindowsApps") || StrStrIW(p, L"\\XboxGames"))
        return L2(L"Essa pasta é da Microsoft Store / app do Xbox e o Windows bloqueia. Copie o jogo para uma pasta normal (por exemplo D:\\Jogos\\UFC 4) e escolha ela.",
                  L"That folder belongs to the Microsoft Store / Xbox app and Windows locks it. Copy the game to a normal folder (for example D:\\Games\\UFC 4) and pick that one.");
    return NULL;
}
static BOOL BadGameDirMsg(int g) { wchar_t gp[MAX_PATH]; GetStr(g, L"game", gp, MAX_PATH, L""); const wchar_t *bad = gp[0] ? BadGameDir(gp) : NULL; if (bad) Msg(C_BAD, L"%s", bad); return bad != NULL; }
/* a folder chosen for game g (Browse, the guide, a drop or the scan): saved, checked, and said what it is */
static void SetGameFolder(int g, const wchar_t *p)
{
    /* 4.0.2: UFC 5 keeps the dump folder itself, even when the folder picked is one or two levels above it; and the
       folder of another UFC is not saved for g (it used to be, with only a warning, and PLAY then started the wrong
       files: the player's video of 2026-10-24, UFC 5 set to C:\GA\UFC 4) */
    wchar_t r[MAX_PATH], why[400];
    if (EXTG(g)) {   /* 6.0: Zelda's file (.nup/.nsp/.xci) or extracted folder; God of War III's PS4 dump */
        wcsncpy(r, p, MAX_PATH - 1); r[MAX_PATH - 1] = 0; StripSlash(r);
        PutStr(g, L"game", r); if (g == g_sel) SetWindowTextW(g_editGame, r);
        ForgetArt(g); g_stTick = 0;
        if (ExtGameOk(g, r)) Msg(C_GOOD, L2(L"%s pronto para jogar.", L"%s is ready to play."), G[g].name);
        else if (NGPORTG(g)) Msg(C_ORANGE, L"%s", NgPickText(g));   /* 6.1 */
        else Msg(C_ORANGE, L"%s", ZELDAG(g) ? L2(L"O Zelda não foi encontrado aí: escolha o arquivo do jogo (.nup, .nsp ou .xci) ou a pasta extraída (com exefs e romfs).", L"Zelda was not found there: choose the game file (.nup, .nsp or .xci) or the extracted folder (with exefs and romfs).")
                                            : L2(L"O God of War III não foi encontrado nessa pasta: escolha a pasta do jogo de PS4 extraído (CUSA01623, com eboot.bin, sce_sys e sce_module), já com o update 01.02.", L"God of War III was not found in that folder: choose the extracted PS4 game's folder (CUSA01623, with eboot.bin, sce_sys and sce_module), with update 01.02 merged in."));
        return;
    }
    if (X360G(g)) {   /* 5.0: UFC Undisputed 3: default.xex or an Xbox 360 .iso; a picked .iso file stands for its folder */
        wcsncpy(r, p, MAX_PATH - 1); r[MAX_PATH - 1] = 0; X3Normalize(r); StripSlash(r);
        const wchar_t *bad = BadGameDir(r);
        if (bad && !X3Folder(r)) { Msg(C_BAD, L"%s", bad); return; }
        PutStr(g, L"game", r); if (g == g_sel) SetWindowTextW(g_editGame, r);
        ForgetArt(g); g_stTick = 0;
        if (X3Folder(r)) Msg(C_GOOD, L2(L"%s pronto para jogar.", L"%s is ready to play."), G[g].name);
        else Msg(C_ORANGE, L"%s", L2(L"O UFC Undisputed 3 não foi encontrado nessa pasta: escolha a pasta do jogo extraído (a que tem o default.xex) ou a pasta onde está o .iso do Xbox 360.",
                                     L"UFC Undisputed 3 was not found in that folder: pick the extracted game's folder (the one with default.xex) or the folder that holds the Xbox 360 .iso."));
        return;
    }
    if (G[g].ps5 && PsFind(g, p, r, 2)) p = r;
    int det = DetectFolder(p, NULL, why, 400);
    if (det >= 0 && det != g && !(G[g].ps5 && PsDump(g, p))) {
        Msg(C_ORANGE, L2(L"Essa pasta parece ser do %s, não do %s. Confira a pasta.", L"That folder looks like %s, not %s. Check the folder."), G[det].name, G[g].name);
        return;
    }
    PutStr(g, L"game", p); if (g == g_sel) SetWindowTextW(g_editGame, p);
    ForgetArt(g);
    const wchar_t *bad = BadGameDir(p);
    if (bad) { Msg(C_BAD, L"%s", bad); return; }
    if (GameOkFor(g, p)) {
        if (why[0]) Msg(C_ORANGE, L"%s", why);
        else Msg(C_GOOD, L2(L"%s pronto para jogar.", L"%s is ready to play."), G[g].name);
    }
    else Msg(C_ORANGE, L"%s", why[0] ? why : L2(L"O jogo não foi encontrado nessa pasta: escolha a pasta que tem eboot.bin, sce_sys e sce_module (veja \"Como instalar\").",
                                                 L"The game was not found in that folder: pick the one with eboot.bin, sce_sys and sce_module (see \"How to install\")."));
}
static void Browse(void)
{
    wchar_t p[MAX_PATH], t[160]; GetStr(g_sel, L"game", p, MAX_PATH, L""); _snwprintf(t, 160, T(S_PICKGAME), G[g_sel].name);
    if (ZELDAG(g_sel)) {   /* 6.0: Zelda is a file (.nup/.nsp/.xci); "Cancel" goes on to a folder (an extracted dump) */
        wchar_t f[MAX_PATH] = L""; OPENFILENAMEW of = {sizeof(of)}; of.hwndOwner = g_wnd;
        of.lpstrFilter = L"Switch (*.nup;*.nsp;*.xci;*.nsz;*.xcz)\0*.nup;*.nsp;*.xci;*.nsz;*.xcz\0*.*\0*.*\0"; of.lpstrFile = f; of.nMaxFile = MAX_PATH;
        of.lpstrTitle = L2(L"Escolha o arquivo do Zelda: Ocarina of Time (Cancelar = escolher uma pasta extraída)", L"Choose the Zelda: Ocarina of Time file (Cancel = pick an extracted folder)");
        of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&of)) { SetGameFolder(g_sel, f); return; }
    }
    if (!PickFolder(p, t)) return;
    StripSlash(p); SetGameFolder(g_sel, p);
}
static void ChangePort(void)
{
    wchar_t p[MAX_PATH], t[200]; PortDir(g_sel, p); _snwprintf(t, 200, T(S_PICKPORT), G[g_sel].name, G[g_sel].exe);
    if (!PickFolder(p, t)) return;
    wchar_t e[MAX_PATH]; Join(e, p, G[g_sel].exe);
    if (!FileExists(e)) { Msg(C_BAD, T(S_NOTFOUND), G[g_sel].exe); return; }
    wchar_t k[32]; _snwprintf(k, 32, L"port_%s", G[g_sel].id); WritePrivateProfileStringW(L"PORTHEON", k, p, g_pIni);
    ReadVersion(g_sel); g_kvGame = -1; LoadEdits(); InvalidateRect(g_wnd, NULL, FALSE);
}
static void OpenLogs(void)
{
    wchar_t p[MAX_PATH];
    if (G[g_sel].ps5) { wchar_t d[MAX_PATH]; PortDir(g_sel, d); Join(p, d, L"bin\\_Logs"); }   /* 4.0.2: UFC5.exe's and the engine's logs */
    else if (X360G(g_sel)) { GameDir(g_sel, p); X3Normalize(p); }   /* 5.0: ufc3.log and RELATORIO.md are in the game folder */
    else if (EXTG(g_sel)) ExtLogsDir(g_sel, p);   /* 6.0 */
    else { wchar_t d[MAX_PATH]; LocalData(d, G[g_sel].data); Join(p, d, L"logs"); if (!DirExists(p)) wcscpy(p, d); }
    if (!DirExists(p)) { Msg(C_SUB, L"%s", T(S_NOLOGS)); return; }
    ShellExecuteW(g_wnd, L"open", p, NULL, NULL, SW_SHOWNORMAL);
}
/* 4.3.5: copies a log with the Windows profile path (it holds the account's name) written as %USERPROFILE% */
static BOOL CopyRedacted(const wchar_t *from, const wchar_t *to, const char *prof)
{
    HANDLE h = CreateFileW(from, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    LARGE_INTEGER sz; GetFileSizeEx(h, &sz); LONGLONG start = sz.QuadPart > (24 << 20) ? sz.QuadPart - (24 << 20) : 0;   /* the end of a huge log */
    LARGE_INTEGER pos; pos.QuadPart = start; SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
    DWORD want = (DWORD)(sz.QuadPart - start), n = 0; char *b = (char *)malloc((size_t)want + 1);
    BOOL ok = b && ReadFile(h, b, want, &n, NULL); CloseHandle(h);
    if (!ok) { free(b); return FALSE; }
    HANDLE o = CreateFileW(to, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (o == INVALID_HANDLE_VALUE) { free(b); return FALSE; }
    size_t pl = strlen(prof); DWORD i = 0, run = 0, w;
    if (n >= 2 && (unsigned char)b[0] == 0xFF && (unsigned char)b[1] == 0xFE) {   /* a UTF-16 file (the games' .ini) */
        wchar_t wp[MAX_PATH]; int wl = MultiByteToWideChar(CP_UTF8, 0, prof, -1, wp, MAX_PATH) - 1; DWORD step = (DWORD)wl * 2;
        for (i = 2, run = 0; i + 1 < n;) {
            if (wl > 3 && i + step <= n && !_wcsnicmp((const wchar_t *)(b + i), wp, wl)) {
                WriteFile(o, b + run, i - run, &w, NULL); WriteFile(o, L"%USERPROFILE%", 26, &w, NULL); i += step; run = i;
            } else i += 2;
        }
    } else
    while (i < n) {
        if (pl > 3 && i + pl <= n && !_strnicmp(b + i, prof, pl)) {
            WriteFile(o, b + run, i - run, &w, NULL); WriteFile(o, "%USERPROFILE%", 13, &w, NULL); i += (DWORD)pl; run = i;
        } else i++;
    }
    WriteFile(o, b + run, n - run, &w, NULL); CloseHandle(o); free(b); return TRUE;
}
/* 5.2: the report's helpers. ReportAdd copies one file into the report folder (text: through CopyRedacted; binary,
   a minidump: as it is) and adds it to tar's list; NewestFile finds the newest match of a pattern (maxSize 0: any
   size); HideIniGame writes sacredfire.ini's game= line as <hidden> (a folder name can carry a person's name); Ap
   appends to pc.txt. */
/* 5.2: a minidump holds module paths (C:\Users\<name>\...) in UTF-16 and sometimes in ANSI. Its strings have fixed
   lengths and offsets, so the account name inside the profile path is overwritten with '_' of the same length instead
   of %USERPROFILE%: the dump stays readable by a debugger. Only the profile path is found; the memory a dump carries
   can still hold other text, which the report's pc.txt says. */
static BOOL CopyDumpRedacted(const wchar_t *from, const wchar_t *to, const char *prof)
{
    DWORD n = 0; BYTE *b = ReadWhole(from, &n); if (!b) return FALSE;
    wchar_t wp[MAX_PATH]; int wl = MultiByteToWideChar(CP_UTF8, 0, prof, -1, wp, MAX_PATH) - 1;
    const wchar_t *sl = wl > 3 ? wcsrchr(wp, L'\\') : NULL; int nameAt = sl ? (int)(sl - wp) + 1 : wl, nameLen = wl - nameAt;
    if (nameLen > 0) {
        char ap[MAX_PATH]; int al = WideCharToMultiByte(CP_ACP, 0, wp, -1, ap, MAX_PATH, NULL, NULL) - 1;
        for (DWORD i = 0; i + (DWORD)wl * 2 <= n; i++)   /* UTF-16 (any alignment: a dump's strings are 2-aligned, its blobs not always) */
            if (!_wcsnicmp((const wchar_t *)(b + i), wp, wl)) { for (int k = 0; k < nameLen; k++) { b[i + (nameAt + k) * 2] = '_'; b[i + (nameAt + k) * 2 + 1] = 0; } i += (DWORD)wl * 2 - 1; }
        if (al == wl) for (DWORD i = 0; i + (DWORD)al <= n; i++)   /* ANSI, when it has one byte per character */
            if (!_strnicmp((const char *)(b + i), ap, al)) { memset(b + i + nameAt, '_', nameLen); i += (DWORD)al - 1; }
    }
    HANDLE o = CreateFileW(to, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (o == INVALID_HANDLE_VALUE) { free(b); return FALSE; }
    DWORD w = 0; BOOL ok = WriteFile(o, b, n, &w, NULL) && w == n; CloseHandle(o); free(b);
    if (!ok) DeleteFileW(to);
    return ok;
}
static BOOL DumpSmall(const wchar_t *p)
{
    WIN32_FILE_ATTRIBUTE_DATA a; return GetFileAttributesExW(p, GetFileExInfoStandard, &a) && !a.nFileSizeHigh && a.nFileSizeLow <= (8u << 20);
}
static BOOL ReportAdd(const wchar_t *dir, wchar_t *list, size_t cap, int *nf, const wchar_t *from, const char *prof, BOOL text)
{
    if (!from[0] || !FileExists(from)) return FALSE;
    const wchar_t *base = wcsrchr(from, L'\\'); base = base ? base + 1 : from;
    if (wcslen(list) + wcslen(base) + 4 >= cap) return FALSE;
    wchar_t to[MAX_PATH]; Join(to, dir, base);
    BOOL ok = text ? CopyRedacted(from, to, prof) : CopyDumpRedacted(from, to, prof);
    if (ok) { wcscat(list, L" \""); wcscat(list, base); wcscat(list, L"\""); (*nf)++; }
    return ok;
}
static BOOL NewestFile(const wchar_t *dir, const wchar_t *pattern, ULONGLONG maxSize, wchar_t *out)
{
    wchar_t pat[MAX_PATH]; Join(pat, dir, pattern); WIN32_FIND_DATAW fd; FILETIME bt = {0, 0}; out[0] = 0;
    HANDLE h = FindFirstFileW(pat, &fd); if (h == INVALID_HANDLE_VALUE) return FALSE;
    do {
        ULONGLONG sz = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (maxSize && sz > maxSize)) continue;
        if (CompareFileTime(&fd.ftLastWriteTime, &bt) > 0) { bt = fd.ftLastWriteTime; Join(out, dir, fd.cFileName); }
    } while (FindNextFileW(h, &fd));
    FindClose(h); return out[0] != 0;
}
/* 6.1: the newest Windows error report (Report.wer, UTF-16) of a PS5 engine: for a fail-fast (0xC0000409, the 11-08
   UFC 5 AMD start-up crashes of triagem2-1108) it is the only place the faulting DLL is named, since the engine's log
   is cut off. WER keeps one folder per crash, AppCrash_<exe>_<hash>_..., in the account's and the machine's
   ReportArchive / ReportQueue; dirPat is "AppCrash_UFC 5*" (UFC 5.exe and UFC 5 AMD.exe, never the launcher UFC5.exe)
   or "AppCrash_UFC 6*" (UFC 6.exe, its compat and Parsec builds). *when gets the report's write time. */
static BOOL NewestWer(const wchar_t *dirPat, wchar_t *out, FILETIME *when)
{
    wchar_t la[MAX_PATH] = L"", pd[MAX_PATH] = L"", roots[4][MAX_PATH]; int nr = 0; FILETIME bt = {0, 0}; out[0] = 0;
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", la, MAX_PATH)) {
        _snwprintf(roots[nr++], MAX_PATH, L"%s\\Microsoft\\Windows\\WER\\ReportArchive", la); _snwprintf(roots[nr++], MAX_PATH, L"%s\\Microsoft\\Windows\\WER\\ReportQueue", la);
    }
    if (GetEnvironmentVariableW(L"ProgramData", pd, MAX_PATH)) {
        _snwprintf(roots[nr++], MAX_PATH, L"%s\\Microsoft\\Windows\\WER\\ReportArchive", pd); _snwprintf(roots[nr++], MAX_PATH, L"%s\\Microsoft\\Windows\\WER\\ReportQueue", pd);
    }
    for (int r = 0; r < nr; r++) {
        roots[r][MAX_PATH - 1] = 0;
        wchar_t pat[MAX_PATH]; Join(pat, roots[r], dirPat); WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pat, &fd); if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            wchar_t sub[MAX_PATH], wer[MAX_PATH]; Join(sub, roots[r], fd.cFileName); Join(wer, sub, L"Report.wer");
            WIN32_FILE_ATTRIBUTE_DATA a;
            if (GetFileAttributesExW(wer, GetFileExInfoStandard, &a) && !a.nFileSizeHigh && a.nFileSizeLow <= (1u << 20) &&
                CompareFileTime(&a.ftLastWriteTime, &bt) > 0) { bt = a.ftLastWriteTime; wcscpy(out, wer); }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (when) *when = bt;
    return out[0] != 0;
}
static void HideIniGame(const wchar_t *path)
{
    DWORD n = 0; BYTE *b = ReadWhole(path, &n); if (!b) return;
    char *o = (char *)malloc((size_t)n * 3 + 64); if (!o) { free(b); return; }   /* a short game= line grows */
    DWORD i = 0, k = 0;
    while (i < n) {
        DWORD e = i; while (e < n && b[e] != '\n') e++;
        DWORD s = i; if (s == 0 && n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) { memcpy(o + k, b, 3); k += 3; s = 3; }
        if (e - s >= 5 && !memcmp(b + s, "game=", 5)) { memcpy(o + k, "game=<hidden>\r", 14); k += 14; if (e < n) o[k++] = '\n'; }
        else { memcpy(o + k, b + s, (e < n ? e + 1 : e) - s); k += (e < n ? e + 1 : e) - s; }
        i = e < n ? e + 1 : e;
    }
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, o, k, &w, NULL); CloseHandle(h); }
    free(o); free(b);
}
static void Ap(wchar_t *b, size_t cap, const wchar_t *fmt, ...)
{
    size_t l = wcslen(b); if (l + 1 >= cap) return;
    va_list ap; va_start(ap, fmt); _vsnwprintf(b + l, cap - l, fmt, ap); va_end(ap); b[cap - 1] = 0;
}
/* 7.1 (U5-7): the name of a file without its folder and its suffix: "UFC5_20261111_215900" of
   ...\UFC5_20261111_215900_motor.txt with suffix "_motor.txt"; FALSE when the name does not end so */
static BOOL NameStem(const wchar_t *path, const wchar_t *suffix, wchar_t *stem, int n)
{
    const wchar_t *b = wcsrchr(path, L'\\'); b = b ? b + 1 : path;
    size_t bl = wcslen(b), sl = wcslen(suffix);
    if (bl <= sl || _wcsicmp(b + bl - sl, suffix)) return FALSE;
    size_t l = bl - sl; if (l >= (size_t)n) return FALSE;
    wcsncpy(stem, b, l); stem[l] = 0; return TRUE;
}
/* 7.1 (U5-7, report -2204: the newest _motor.txt was a clean session of 01:40, the crash was at 21:59): the newest file
   of the pattern whose stem is the stem of one of the logs the report holds (UFC5.exe names a session's log, its
   _motor.txt and its .dmp after the same start time; UFC6.exe its log and its .dmp). *other: a file of the pattern
   from another session was there (left out). */
static BOOL NewestOfSession(const wchar_t *dir, const wchar_t *pattern, const wchar_t *suffix, wchar_t (*logs)[MAX_PATH], int nl, wchar_t *out, BOOL *other)
{
    wchar_t pat[MAX_PATH]; Join(pat, dir, pattern); WIN32_FIND_DATAW fd; FILETIME bt = {0, 0}; out[0] = 0; if (other) *other = FALSE;
    HANDLE h = FindFirstFileW(pat, &fd); if (h == INVALID_HANDLE_VALUE) return FALSE;
    do {
        wchar_t st[MAX_PATH]; if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !NameStem(fd.cFileName, suffix, st, MAX_PATH)) continue;
        BOOL mine = FALSE; for (int k = 0; k < nl && !mine; k++) { wchar_t ls[MAX_PATH]; mine = NameStem(logs[k], L".log", ls, MAX_PATH) && !_wcsicmp(ls, st); }
        if (!mine) { if (other) *other = TRUE; continue; }
        if (CompareFileTime(&fd.ftLastWriteTime, &bt) > 0) { bt = fd.ftLastWriteTime; Join(out, dir, fd.cFileName); }
    } while (FindNextFileW(h, &fd));
    FindClose(h); return out[0] != 0;
}
/* 7.1 (ufc6-1114 J: every new-engine report said "main, UFC 6.exe"): the engine a UFC 6 session ran, from its log's
   header (UFC6.exe 1.28.0: "Engine: new (...), PS5 Pro mode on (...)", "Engine: compatibility (...)", "Engine: the
   new engine was asked for (...) but is not used: ...", "Parsec mode: on the main engine's 2-player engine (...)";
   no "Engine:" line = the main engine). head: the log's first bytes, 0-terminated.
   7.1 review (correctness#7, cross/contract#6): UFC6.exe's "Engine: back to the main engine (compat_engine=0): ..." note
   comes before the real "Engine:" line of that restarted PLAY, so it is skipped. (cross/contract#7): what was asked for
   is UFC6.exe's "Upscaler: " line (1.29.0; its "DLSS: <mode> (dlss=" line in older logs), and what the engine did is its
   own first "FSR: not loaded:" / "FSR: on," / "FSR: runtime " / "DLSS: game window " line, said apart ("; engine: ") */
static void Ufc6EngineOf(const char *head, wchar_t *out, int n)
{
    const char *eng = NULL, *par = NULL, *ups = NULL, *dl = NULL, *fsrNo = NULL, *fsrOn = NULL, *fsrRt = NULL, *win = NULL;
    for (const char *l = head; l && *l; ) {
        if (!eng && !strncmp(l, "Engine: ", 8) && strncmp(l + 8, "back to ", 8)) eng = l;
        if (!par && !strncmp(l, "Parsec mode: on ", 16)) par = l + 16;
        if (!ups && !strncmp(l, "Upscaler: ", 10)) ups = l;
        if (!dl && !strncmp(l, "DLSS: ", 6) && strncmp(l + 6, "game window", 11)) dl = l;
        if (!fsrNo && !strncmp(l, "FSR: not loaded:", 16)) fsrNo = l;
        if (!fsrOn && !strncmp(l, "FSR: on,", 8)) fsrOn = l;
        if (!fsrRt && !strncmp(l, "FSR: runtime ", 13)) fsrRt = l;
        if (!win && !strncmp(l, "DLSS: game window ", 18)) win = l;
        const char *e = strchr(l, '\n'); l = e ? e + 1 : NULL;
    }
    if (!ups) ups = dl;
    const char *res = fsrNo ? fsrNo : fsrOn ? fsrOn : fsrRt ? fsrRt : win;
    const wchar_t *what = L"main, UFC 6.exe"; BOOL pro = FALSE, asked = FALSE;
    if (eng && !strncmp(eng + 8, "new ", 4)) { what = L"new, novo\\UFC 6 novo.exe"; const char *e = strchr(eng, '\n'), *p = strstr(eng, "PS5 Pro mode on"); pro = p && (!e || p < e); }
    else if (eng && !strncmp(eng + 8, "compatibility", 13)) what = L"compatibility, compat\\UFC 6 compat.exe";
    else { asked = eng && !strncmp(eng + 8, "the new engine was asked for", 28); if (par && !strncmp(par, "the main engine's 2-player engine", 33)) what = L"2-player, bin\\UFC 6 Parsec.exe"; }
    wchar_t up[140] = L"", rs[110] = L"";
    if (ups) { char u8[121]; size_t k = 0; while (k < 120 && ups[k] && ups[k] != '\r' && ups[k] != '\n') { u8[k] = ups[k]; k++; } u8[k] = 0; MultiByteToWideChar(CP_UTF8, 0, u8, -1, up, 140); up[139] = 0; }
    if (res) { char r8[101]; size_t k = 0; while (k < 100 && res[k] && res[k] != '\r' && res[k] != '\n') { r8[k] = res[k]; k++; } r8[k] = 0; MultiByteToWideChar(CP_UTF8, 0, r8, -1, rs, 110); rs[109] = 0; }
    _snwprintf(out, n, L"%s%s%s%s%s%s%s%s", what, eng && !wcsncmp(what, L"new", 3) ? (pro ? L", PS5 Pro mode on" : L", PS5 Pro mode off") : L"", asked ? L" (the new engine was asked for, not used)" : L"",
               par && wcsncmp(what, L"2-player", 8) ? L", Parsec mode" : L"", up[0] ? L"; log: " : L"", up, rs[0] ? L"; engine: " : L"", rs);
    out[n - 1] = 0;
}
static void Ufc6EngineOfLog(const wchar_t *log, wchar_t *out, int n)
{
    out[0] = 0; HANDLE h = CreateFileW(log, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    char *b = (char *)malloc((64u << 10) + 1); DWORD got = 0;
    if (b && ReadFile(h, b, 64u << 10, &got, NULL)) { b[got] = 0; Ufc6EngineOf(b, out, n); }
    free(b); CloseHandle(h);
}
/* 4.3.5: "Make a report for Discord": one .zip on the desktop with what a crash report needs - the game's three newest
   logs, its settings file and a summary of this PC, with the Windows account's name taken out of every path. Zipped by
   Windows' own tar.exe (Windows 10 1803 and newer) */
static void MakeReport(int g)
{
    wchar_t logs[MAX_PATH], el[MAX_PATH] = L"";
    if (G[g].ps5) {
        wchar_t d[MAX_PATH]; PortDir(g, d); Join(logs, d, L"bin\\_Logs");
        /* 7.1 review (correctness#6): UFC5.exe writes a session's log, its _motor.txt and its .dmp in <emulator>\_Logs
           (ufc5.c: emuDir\_Logs). With emulator= outside bin\ that is another folder (el), read for the newest logs too,
           so the 6.1 rule below finds the crash's own _motor.txt again. Inside the ps5 branch: the else-if chain below
           must not run for UFC 6 (its G[].data is NULL, LocalData would point logs at %LOCALAPPDATA%\(null)) */
        if (UFC5G(g)) {
            wchar_t ini[MAX_PATH], emu[MAX_PATH] = L""; const wchar_t *sec = PsIni(g, ini);
            GetPrivateProfileStringW(sec, L"emulator", L"", emu, MAX_PATH, ini);
            wchar_t *sl = wcsrchr(emu, L'\\');
            if (sl) { *sl = 0; Join(el, emu, L"_Logs"); if (!_wcsicmp(el, logs) || !DirExists(el)) el[0] = 0; }
        }
    }
    else if (X360G(g)) { GameDir(g, logs); X3Normalize(logs); if (!logs[0]) { Msg(C_SUB, L"%s", T(S_NOLOGS)); return; } }   /* 5.0: ufc3.log */
    else if (EXTG(g)) { if (!ExtLogsDir(g, logs)) { Msg(C_SUB, L"%s", T(S_NOLOGS)); return; } }   /* 6.0: the emulator's or the port's log folder */
    else { wchar_t d[MAX_PATH]; LocalData(d, G[g].data); Join(logs, d, L"logs"); if (!DirExists(logs)) wcscpy(logs, d); }
    wchar_t pat[MAX_PATH], newest[3][MAX_PATH] = {{0}}; FILETIME nt[3] = {{0}}; int nl = 0;
    /* 7.1 (critic 2D: a Zelda report held only .gitattributes, .gitignore and the recomp repository's AGENTS.md, the
       newest "*.*" of its root): the other games' folders are read for logs and text files only, never dotfiles */
    static const wchar_t *const pats[2] = {L"*.log", L"*.txt"};
    const wchar_t *dirs[2] = {logs, el};
    for (int di = 0; di < (el[0] ? 2 : 1); di++)
    for (int pi = 0; pi < (EXTG(g) ? 2 : 1); pi++) {
        Join(pat, dirs[di], pats[pi]); WIN32_FIND_DATAW fd; HANDLE fh = FindFirstFileW(pat, &fd);
        if (fh == INVALID_HANDLE_VALUE) continue;
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
            int at = nl < 3 ? nl : 3;
            while (at > 0 && CompareFileTime(&fd.ftLastWriteTime, &nt[at - 1]) > 0) at--;
            if (at >= 3) continue;
            for (int k = 2; k > at; k--) { wcscpy(newest[k], newest[k - 1]); nt[k] = nt[k - 1]; }
            Join(newest[at], dirs[di], fd.cFileName); nt[at] = fd.ftLastWriteTime; if (nl < 3) nl++;
        } while (FindNextFileW(fh, &fd));
        FindClose(fh);
    }
    if (!nl) { Msg(C_SUB, L"%s", T(S_NOLOGS)); return; }
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t tmp[MAX_PATH], dir[MAX_PATH], name[96]; GetTempPathW(MAX_PATH, tmp);
    _snwprintf(name, 96, L"PORTHEON-relatorio-%s-%04d%02d%02d-%02d%02d", G[g].id, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    Join(dir, tmp, name); CreateDirectoryW(dir, NULL);
    wchar_t profW[MAX_PATH] = L""; char prof[MAX_PATH * 3] = "";
    if (GetEnvironmentVariableW(L"USERPROFILE", profW, MAX_PATH)) WideCharToMultiByte(CP_UTF8, 0, profW, -1, prof, sizeof(prof), NULL, NULL);
    wchar_t list[4096] = L""; int nf = 0; BOOL dumped = FALSE, werIn = FALSE; FILETIME werAt = {0, 0};
    for (int k = 0; k < nl; k++) ReportAdd(dir, list, 4096, &nf, newest[k], prof, TRUE);
    if (ZELDAG(g)) {   /* 7.1: the recomp's other logs (ZeldaExtraLog), each once */
        wchar_t zf[MAX_PATH];
        for (int k = 0; ZeldaExtraLog(k, zf); k++) { BOOL dup = FALSE; for (int j = 0; j < nl; j++) if (!_wcsicmp(zf, newest[j])) dup = TRUE; if (!dup) ReportAdd(dir, list, 4096, &nf, zf, prof, TRUE); }
    }
    if (G[g].ps5) {   /* the game's settings (UFC5.ini / UFC6.ini) */
        wchar_t ini[MAX_PATH]; PsIni(g, ini); ReportAdd(dir, list, 4096, &nf, ini, prof, TRUE);
    }
    /* 5.1.1: UFC 5's newest engine log (*_motor.txt): the only file that shows a lost device's cause. 5.2: UFC 6's too,
       and the newest minidump either launcher wrote (UFC5.exe 1.32.0 / UFC6.exe 1.18.0), up to 8 MB so the zip stays
       under Discord's 10 MB free upload (CopyDumpRedacted). Only the newest one: a bigger newest dump is left out rather
       than an older, smaller one that belongs to another crash.
       7.1 (U5-7): only a _motor.txt or a .dmp of one of the sessions whose log is in the report (NewestOfSession: the
       same start time in the name); pc.txt says when one of another session was left out */
    int motorIn = 0, dumpOther = 0; BOOL motorOther = FALSE;
    if (G[g].ps5) {
        wchar_t best[MAX_PATH]; BOOL oth = FALSE;
        if (NewestOfSession(logs, L"*_motor.txt", L"_motor.txt", newest, nl, best, &oth)) motorIn += ReportAdd(dir, list, 4096, &nf, best, prof, TRUE);
        motorOther |= oth;
        if (NewestOfSession(logs, L"*.dmp", L".dmp", newest, nl, best, &oth) && DumpSmall(best)) dumped = ReportAdd(dir, list, 4096, &nf, best, prof, FALSE);
        dumpOther = oth;
        if (el[0]) {   /* 6.1: UFC5.exe writes the engine's _motor.txt next to the engine it runs (emulator= in UFC5.ini):
                          one set to an engine outside bin\ kept its _motor.txt out of the zip (triagem2-1108: the
                          reports asked for the _motor.txt the zip did not hold). 7.1 review: el's logs are among the
                          newest above, so its _motor.txt matches its own session's log; its .dmp too, when bin\_Logs
                          gave none (still one dump at most) */
            if (NewestOfSession(el, L"*_motor.txt", L"_motor.txt", newest, nl, best, &oth)) motorIn += ReportAdd(dir, list, 4096, &nf, best, prof, TRUE);
            motorOther |= oth;
            if (!dumped) { if (NewestOfSession(el, L"*.dmp", L".dmp", newest, nl, best, &oth) && DumpSmall(best)) dumped = ReportAdd(dir, list, 4096, &nf, best, prof, FALSE); dumpOther |= oth; }
        }
        /* 6.1: the newest Windows error report of this game's engine (NewestWer), through CopyRedacted like the logs */
        if (NewestWer(UFC5G(g) ? L"AppCrash_UFC 5*" : L"AppCrash_UFC 6*", best, &werAt)) werIn = ReportAdd(dir, list, 4096, &nf, best, prof, TRUE);
    }
    if (X360G(g)) {   /* 5.0: the UFC Undisputed 3 port's own report; 5.2: and its newest crash dump */
        wchar_t rel[MAX_PATH], best[MAX_PATH]; Join(rel, logs, L"RELATORIO.md"); ReportAdd(dir, list, 4096, &nf, rel, prof, TRUE);
        if (NewestFile(logs, L"ufc3-crash-*.dmp", 0, best) && DumpSmall(best)) dumped = ReportAdd(dir, list, 4096, &nf, best, prof, FALSE);
    }
    /* 5.2: what a UFC 1-4 crash needs to be reproduced: the freeze catcher's stacks, the port's settings (the game
       folder hidden), the shaders PORTHEON skips for this PC */
    if (PS4G(g)) {
        wchar_t best[MAX_PATH], sf[MAX_PATH], to[MAX_PATH];
        if (NewestFile(logs, L"*.stacks.txt", 0, best)) ReportAdd(dir, list, 4096, &nf, best, prof, TRUE);
        SfPath(g, sf); if (ReportAdd(dir, list, 4096, &nf, sf, prof, TRUE)) { Join(to, dir, L"sacredfire.ini"); HideIniGame(to); }
        SkipFilePath(g, sf); ReportAdd(dir, list, 4096, &nf, sf, prof, TRUE);
    }
    {   /* 5.2: PORTHEON's own: the FSR layer's log for this game and PORTHEON's last crash */
        wchar_t pd[MAX_PATH], f[MAX_PATH], nm[48]; LocalData(pd, L"PORTHEON");
        _snwprintf(nm, 48, L"fsr-%s.log", G[g].id); Join(f, pd, nm); ReportAdd(dir, list, 4096, &nf, f, prof, TRUE);
        Join(f, pd, L"crash.txt"); ReportAdd(dir, list, 4096, &nf, f, prof, TRUE);
        _snwprintf(nm, 48, L"play-%s.txt", G[g].id); Join(f, pd, nm); ReportAdd(dir, list, 4096, &nf, f, prof, TRUE);   /* max-perf 11-13: what the last PLAY applied */
        Join(f, pd, L"steam.txt"); ReportAdd(dir, list, 4096, &nf, f, prof, TRUE);   /* 7.1: what "Add to the Steam library" did (SteamTraceLog) */
    }
    {   /* this PC. 5.2: the processor and what it lacks, the driver, memory, disks, power, the engine that ran and the
           settings that matter, so a report can be reproduced without asking */
        static wchar_t pc[8000]; wchar_t sum[400]; PcSummary(sum, 400);
        OSVERSIONINFOW ov = {sizeof(ov)}; typedef LONG (WINAPI *RtlGV)(OSVERSIONINFOW *); RtlGV rgv = (RtlGV)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"); if (rgv) rgv(&ov);
        pc[0] = 0;
        Ap(pc, 8000, L"PORTHEON %s (Portheon %s)\r\nGame: %s (%s)\r\nGame version: %s\r\nWindows: %lu.%lu build %lu%s\r\nPC: %s\r\nGPU: %s, %llu MB\r\nLanguage: %d\r\n",
           APP_VER, APP_SHOW, G[g].name, G[g].id, g_ver[g][0] ? g_ver[g] : L"?", ov.dwMajorVersion, ov.dwMinorVersion, ov.dwBuildNumber, UnderWine() ? L" (Wine)" : L"", sum,
           g_haveGpu ? g_gpu.name : L"?", g_haveGpu ? (unsigned long long)(g_gpu.vram >> 20) : 0ull, g_lang);
        {   wchar_t m4[64] = L"", m5[160] = L"", m3[64] = L""; CpuLacksPs4(m4, 64); CpuPs5Ok(m5, 160); CpuX360Ok(m3, 64);
            Ap(pc, 8000, L"CPU: %s, %d threads\r\nCPU lacks: PS4 [%s] PS5 [%s] X360 [%s] SSE4a %s%s\r\n", g_cpuName, g_threads, m4[0] ? m4 : L"-", m5[0] ? m5 : L"-", m3[0] ? m3 : L"-",
               CpuHasSse4a() ? L"yes" : L"no", PIni(L"debug_no_sse4a", 0) ? L" (debug_no_sse4a)" : L""); }
        VulkanCheck();
        Ap(pc, 8000, L"Vulkan: %s, API %u.%u.%u, vendor %04x device %04x, driver %s (raw %08x, id %u), barycentrics %d%s%s\r\n", g_vkc.name[0] ? g_vkc.name : L"?",
           g_vkc.api >> 22, (g_vkc.api >> 12) & 0x3ff, g_vkc.api & 0xfff, g_vkc.vendor, g_vkc.devid, g_vkc.drv[0] ? g_vkc.drv : L"?", g_vkc.driver, g_vkc.drvId, g_vkc.bary,
           g_vkc.miss[0] ? L", missing " : L"", g_vkc.miss);
        {   MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms);
            Ap(pc, 8000, L"RAM: %llu MB, %llu MB free; commit limit %llu MB (%llu MB free), page file %llu MB\r\n", ms.ullTotalPhys >> 20, ms.ullAvailPhys >> 20,
               ms.ullTotalPageFile >> 20, ms.ullAvailPageFile >> 20, ms.ullTotalPageFile > ms.ullTotalPhys ? (ms.ullTotalPageFile - ms.ullTotalPhys) >> 20 : 0ull); }
        {   wchar_t la[MAX_PATH] = L"", gd[MAX_PATH] = L"", pdir[MAX_PATH]; GetEnvironmentVariableW(L"LOCALAPPDATA", la, MAX_PATH); GameDir(g, gd); PortDir(g, pdir);
            ULONGLONG c = FreeGbAt(L"C:\\"), l = FreeGbAt(la), gg = FreeGbAt(gd), pp = FreeGbAt(pdir);
            Ap(pc, 8000, L"Free disk: C: %lld GB, LOCALAPPDATA %lld GB, game folder %lld GB, port folder %lld GB\r\n",
               c == 0xffffffffull ? -1ll : (long long)c, l == 0xffffffffull ? -1ll : (long long)l, gg == 0xffffffffull ? -1ll : (long long)gg, pp == 0xffffffffull ? -1ll : (long long)pp);
            /* 7.1 (critic 6.6, ufc6-1114 B: a game path with a letter outside ASCII leaves UFC 6's title profile off, and
               the report's %USERPROFILE% redaction hides that letter): only yes / no, never the path */
            wchar_t up[MAX_PATH] = L""; GetEnvironmentVariableW(L"USERPROFILE", up, MAX_PATH);
            Ap(pc, 8000, L"Plain ASCII path (no accent or other non-ASCII letter): game folder %s, PORTHEON folder %s, user profile %s\r\n",
               !gd[0] ? L"-" : NonAscii(gd) ? L"no" : L"yes", NonAscii(g_exeDir) ? L"no" : L"yes", !up[0] ? L"-" : NonAscii(up) ? L"no" : L"yes"); }
        {   int pw = PowerState(); Ap(pc, 8000, L"Power: %s%s%s\r\n", pw & 1 ? L"battery" : L"AC", pw & 2 ? L", battery saver on" : L"", pw & 4 ? L", Power saver plan" : L""); }
        {   int ran = GIni(L"engine_ran", g, -1); wchar_t fx[MAX_PATH]; FixesPath(fx); WIN32_FILE_ATTRIBUTE_DATA fa; SYSTEMTIME ft = {0}; BOOL hf = GetFileAttributesExW(fx, GetFileExInfoStandard, &fa);
            if (hf) FileTimeToSystemTime(&fa.ftLastWriteTime, &ft);
            /* 6.1: engine_ran is set by the PS4 ports only, so UFC 5 / UFC 6 always said "-" although their engine ran
               (proxima-versao-relatorios-1108 ufc6-rtx5060, triagem2-1108): their launchers' own ini names it -
               UFC5.ini engine_last (main / amd, the build whose caches are in place) and emulator=, UFC6.ini
               compat_engine / parsec (the exe UFC6.exe's Play picks). Only file names: a path can hold the account. */
            wchar_t eng[480] = L"";
            if (G[g].ps5) {
                wchar_t ini[MAX_PATH]; const wchar_t *sec = PsIni(g, ini);
                if (UFC5G(g)) {
                    wchar_t last[16] = L"", emu[MAX_PATH] = L""; GetPrivateProfileStringW(sec, L"engine_last", L"", last, 16, ini); GetPrivateProfileStringW(sec, L"emulator", L"", emu, MAX_PATH, ini);
                    if (last[0] || emu[0])
                        _snwprintf(eng, 160, L"%s (UFC5.ini engine_last=%s, emulator %s)", !wcscmp(last, L"amd") ? L"UFC 5 AMD.exe" : !wcscmp(last, L"main") ? L"UFC 5.exe" : L"?",
                                   last[0] ? last : L"-", emu[0] ? PathFindFileNameW(emu) : L"-");
                } else {   /* 7.1 (J): from the newest log's own "Engine:" line (UFC6.exe's choice: new engine, compat, a fallback), the ini only as a fallback */
                    int ce = GetPrivateProfileIntW(sec, L"compat_engine", 0, ini), ps = GetPrivateProfileIntW(sec, L"parsec", 0, ini);
                    int dl = GetPrivateProfileIntW(sec, L"dlss", 0, ini), us = GetPrivateProfileIntW(sec, L"upscaler", 0, ini);
                    wchar_t fromLog[360] = L""; if (nl) Ufc6EngineOfLog(newest[0], fromLog, 360);   /* 7.1 review: 360 (the engine's line too) */
                    if (fromLog[0]) _snwprintf(eng, 480, L"%s (newest log %s; UFC6.ini compat_engine=%d, parsec=%d, dlss=%d, upscaler=%d)", fromLog, PathFindFileNameW(newest[0]), ce, ps, dl, us);
                    else _snwprintf(eng, 480, L"%s (no log read; UFC6.ini compat_engine=%d, parsec=%d, dlss=%d, upscaler=%d)", ce ? L"compatibility, compat\\UFC 6 compat.exe" : ps ? L"UFC 6 Parsec.exe" : L"main, UFC 6.exe", ce, ps, dl, us);
                }
                eng[479] = 0;
            }
            Ap(pc, 8000, L"Engine that ran: %s; V-Cache mask %llx; close_on_play %d; remote fixes %s\r\n",
               eng[0] ? eng : ran == 1 ? L"new (intel-arc)" : ran == 2 ? L"v3.0" : ran == 3 ? L"Native Plus 1.2.0" : ran == 0 ? L"the port's own" : L"-", (unsigned long long)VCacheMask(), PIni(L"close_on_play", 1),
               hf ? L"present" : L"none");
            if (werIn) {   /* 6.1: when the included Report.wer was written, to tell an old crash from this one */
                SYSTEMTIME wt; FileTimeToSystemTime(&werAt, &wt);
                Ap(pc, 8000, L"Windows error report: Report.wer included, written %04d-%02d-%02d %02d:%02d UTC\r\n", wt.wYear, wt.wMonth, wt.wDay, wt.wHour, wt.wMinute);
            }
            if (hf) Ap(pc, 8000, L"Remote fixes file: %04d-%02d-%02d %02d:%02d UTC\r\n", ft.wYear, ft.wMonth, ft.wDay, ft.wHour, ft.wMinute); }
        if (StreakGame(g)) Ap(pc, 8000, L"Crash streak %d, running %d, compatible mode %d (clean %d), offer pending %d\r\n", GIni(L"crash_streak", g, 0), GIni(L"running", g, 0), SafeOn(g), GIni(L"safe_clean", g, 0), GIni(L"safe_ask", g, 0));   /* 7.1 review: safe_ask (Gpu23Escalate) */
        if (dumped) Ap(pc, 8000, L"Minidump: included; the account name in the profile path is replaced by '_', other text from the game's memory may remain\r\n");
        else if (dumpOther) Ap(pc, 8000, L"Minidump: none of the included sessions; a dump of another session was left out\r\n");   /* 7.1 (U5-7) */
        if (UFC5G(g)) Ap(pc, 8000, L"Engine log (_motor.txt): %s\r\n", motorIn ? L"included, from one of the included sessions" : motorOther ? L"none from the included sessions (one from another session was left out)" : L"none");
        if (PS4G(g)) {
            /* 7.1 (U2-HANG F5): the temporal upscaler, the 3D scene size, the texture filter, the frame statistics, the
               skinning fix and the shader learning, which the 11-14 UFC 2 report could not show */
            static const wchar_t *const ks[] = {L"fps_limit", L"present_mode", L"resolution", L"display", L"ultra", L"neo", L"upscaler", L"low_latency", L"parsec", L"pini:cpu_all", L"pini:engine_boost",
                                                L"pini:async_pipelines", L"pgame:gpu_safe", L"pgame:engine", L"pgame:fsr", L"pgame:mfg", L"pgame:a2c", L"pini:ufc3_old_engine", L"pini:ufc4_old_engine",
                                                L"pini:ufc3_new_engine", L"pini:ufc4_new_engine", L"pini:legacy_cpu", L"pini:x3d_ccd",
                                                L"pgame:tups", L"pgame:scene", L"pgame:tex", L"pgame:frame_stats", L"pini:face_fix", L"pini:skip_learn", L"pini:ufc3_no_v30"};
            g_kvGame = -1; Ap(pc, 8000, L"Settings:");
            for (int i = 0; i < N(ks); i++) Ap(pc, 8000, L" %s=%d", ks[i], GetInt(g, ks[i], -1));
            Ap(pc, 8000, L"\r\n"); g_kvGame = -1;
        }
        wchar_t to[MAX_PATH]; Join(to, dir, L"pc.txt"); HANDLE o = CreateFileW(to, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (o != INVALID_HANDLE_VALUE) {
            static char u8[24000]; int len = WideCharToMultiByte(CP_UTF8, 0, pc, -1, u8, sizeof(u8), NULL, NULL); DWORD w;
            if (len > 1) WriteFile(o, u8, (DWORD)(len - 1), &w, NULL); CloseHandle(o);
            wcscat(list, L" \"pc.txt\""); nf++;
        }
    }
    wchar_t zip[MAX_PATH] = L"", *desk = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Desktop, 0, NULL, &desk)) && desk) { _snwprintf(zip, MAX_PATH, L"%s\\%s.zip", desk, name); CoTaskMemFree(desk); }
    wchar_t sys[MAX_PATH], tar[MAX_PATH]; static wchar_t cmd[8192]; GetSystemDirectoryW(sys, MAX_PATH); Join(tar, sys, L"tar.exe");
    _snwprintf(cmd, 8192, L"\"%s\" -a -c -f \"%s\" -C \"%s\"%s", tar, zip, dir, list); cmd[8191] = 0;
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi; DWORD code = 1;
    if (zip[0] && FileExists(tar) && CreateProcessW(tar, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, dir, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 60000); GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    if (code == 0 && FileExists(zip)) {
        wchar_t sel[MAX_PATH + 16]; _snwprintf(sel, MAX_PATH + 16, L"/select,\"%s\"", zip);
        ShellExecuteW(g_wnd, L"open", L"explorer.exe", sel, NULL, SW_SHOWNORMAL);
        Msg(C_GOOD, L2(L"Relatório pronto na área de trabalho: %s.zip (%d arquivos, sem o nome da sua conta do Windows). Mande ele no Discord do PORTHEON com o que aconteceu.",
                       L"Report ready on the desktop: %s.zip (%d files, without your Windows account's name). Send it on PORTHEON's Discord with what happened."), name, nf);
    } else {   /* no tar.exe (an old Windows): the folder itself */
        ShellExecuteW(g_wnd, L"open", dir, NULL, NULL, SW_SHOWNORMAL);
        Msg(C_SUB, L2(L"Os arquivos do relatório estão nesta pasta (%d arquivos): mande eles no Discord do PORTHEON.", L"The report's files are in this folder (%d files): send them on PORTHEON's Discord."), nf);
    }
}
/* 4.3.5: UFC 6's performance test: the next session counts its frames (UFC6.exe bench=1), and when the game closes
   UFC6.exe's message gives the fight's FPS and the preset for this PC */
static void Play(void);
static void StartBench(int g)
{
    if (!UFC6G(g)) return;
    PutInt(g, L"bench", 1);
    Play();
    if (!g_proc) { PutInt(g, L"bench", 0); return; }   /* it did not start: Play's message stays */
    Msg(C_SUB, L"%s", L2(L"Teste de desempenho: entre em Lutar agora, jogue uma luta por uns 2 minutos e feche o jogo durante a luta (Alt+F4). O PORTHEON mostra o FPS da luta e o preset certo para este PC.",
                         L"Performance test: go to Fight now, play a fight for about 2 minutes and close the game during the fight (Alt+F4). PORTHEON shows the fight's FPS and the right preset for this PC."));
}
/* 4.3.7, "Preparar o UFC 6": one session that plays itself to a CPU fight and closes (UFC6.exe warmup=1, Warmup), so
   the engine's shader journal already has the menu and the fight when the player starts */
static void StartWarmup(int g)
{
    if (!UFC6G(g)) return;
    PutInt(g, L"warmup", 1);
    Play();
    if (!g_proc) { PutInt(g, L"warmup", 0); return; }
    Msg(C_SUB, L"%s", L2(L"Preparando o UFC 6: o jogo vai sozinho até uma luta CPU contra CPU e fecha em uns 8 minutos. Não precisa apertar nada; pode usar outro programa enquanto isso.",
                         L"Preparing UFC 6: the game goes by itself to a CPU-vs-CPU fight and closes in about 8 minutes. No need to press anything; you can use another program meanwhile."));
}
static void OpenGameFolder(void)
{
    wchar_t d[MAX_PATH]; GameDir(g_sel, d);
    if (d[0] && DirExists(d)) ShellExecuteW(g_wnd, L"open", d, NULL, NULL, SW_SHOWNORMAL);
    else Msg(C_ORANGE, L"%s", T(S_CHOOSEFIRST));
}
/* 3.5.3: a link in the browser (4.4.0: the clipboard copy went with the PIX key and the phone) */
static void OpenUrl(const wchar_t *url) { ShellExecuteW(g_wnd, L"open", url, NULL, NULL, SW_SHOWNORMAL); }
/* a popup menu under a hit region; returns the chosen command (0: none) */
static int PopupAt(HMENU m, int hitId)
{
    RECT r = {0}; HitRect(hitId, &r);
    POINT pt = {r.left + g_ox, r.bottom + SC(2) + g_oy}; ClientToScreen(g_wnd, &pt);
    int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_wnd, NULL);
    DestroyMenu(m); return c;
}
static void BestSettings(void)
{
    if (G[g_sel].ps5) { PsAutoCfg(g_sel, TRUE); return; }
    if (!PS4G(g_sel)) return;
    DetectPc(); g_kvGame = -1; AutoConfig4(g_sel); g_kvGame = -1; wchar_t sum[400]; PcSummary(sum, 400);
    Msg(C_GOOD, L2(L"%s configurado para este PC (%s).", L"%s set up for this PC (%s)."), G[g_sel].name, sum); LoadEdits();
}
/* 4.3.3: what each graphics preset sets (p 1 Low .. 4 Ultra), per game; 0 = PORTHEON's automatic setup again */
static void GfxPreset(int g, int p)
{
    if (X360G(g)) return;   /* 5.1: UFC Undisputed 3's video is set in its own port (no preset row any more) */
    if (p == 0) { BestSettings(); return; }
    if (p == 5) { Msg(C_SUB, L"%s", L2(L"Personalizado: as opções ficam como você ajustou. Para um preset pronto, escolha de Muito baixo a Extremo.", L"Custom: the settings stay as you set them. For a ready-made preset, pick one from Very low to Extreme.")); return; }
    /* 7.3: seven steps, i = 0 Very low, 1 Low, 2 Medium, 3 High, 4 Ultra, 5 Max, 6 Extreme (6.2: Ultrawide = Ultra in a 3440x1440 window) */
    int i = p == 8 ? 0 : p == 6 ? 5 : p == 9 ? 6 : p == 7 ? 4 : p >= 1 && p <= 4 ? p : -1;
    if (i < 0) return;
    if (UFC5G(g)) {   /* UFC5.ini: resolution (RES5: 1 720p, 3 1080p, 4 2K, 5 4K; the window's size, full screen is the monitor's), the 3D size (percent of the game's own), the arena's crowd/shadows/seats, FXAA */
        static const int res[7] = {1, 1, 3, 3, 4, 5, 5}, rs[7] = {60, 80, 100, 100, 100, 100, 100}, crowd[7] = {1, 1, 1, 0, 0, 0, 0}, shadows[7] = {1, 1, 0, 0, 0, 0, 0}, seats[7] = {1, 1, 1, 0, 0, 0, 0}, fxaa[7] = {0, 0, 1, 1, 1, 1, 1};
        PutInt(g, L"resolution", res[i]); PutInt(g, L"render_scale", rs[i]); PutInt(g, L"crowd_off", crowd[i]); PutInt(g, L"shadows_off", shadows[i]);
        PutInt(g, L"seats_off", seats[i]); PutInt(g, L"fxaa", fxaa[i]);
    } else if (G[g].ps5) {   /* UFC6.ini: the 3D size (render_scale, percent of 1920x1080), the crowd, depth of field, FXAA.
                                Very low 640x360 without the crowd; Low 960x540 without the crowd; Medium 1280x720 with it;
                                High the game's 1920x1080 + FXAA; Ultra 2560x1440 with depth of field; Max 3840x2160; Extreme the
                                game's own 4K scale (3200x1800 in fights). The sizes above 1080p are drawn bigger and scaled down,
                                and UFC6.exe falls back by itself on a card with too little video memory (8 GB for 2K, 12 GB for 4K) */
        static const int rs[7] = {33, 50, 67, 100, 133, 200, 300}, crowd[7] = {1, 1, 0, 0, 0, 0, 0}, dof[7] = {1, 1, 1, 1, 0, 0, 0}, fxaa[7] = {0, 0, 0, 1, 1, 1, 1};
        PutInt(g, L"render_scale", rs[i]); PutInt(g, L"quality", rs[i] >= 100); PutInt(g, L"render_scale_75", 0);
        PutInt(g, L"crowd_off", crowd[i]); PutInt(g, L"dof_off", dof[i]); PutInt(g, L"fxaa", fxaa[i]);
    } else if (PS4G(g)) {   /* sacredfire.ini: render size (RES4), PS4 Pro mode, FSR 1.0 and its sharpness, crowd and blur patches */
        static const int res[7] = {0, 0, 1, 1, 2, 3, 3}, neo[7] = {0, 0, 0, 1, 1, 1, 1}, ups[7] = {1, 1, 0, 0, 1, 1, 1}, shp[7] = {2, 2, 1, 1, 2, 2, 2}, off[7] = {1, 1, 0, 0, 0, 0, 0};
        PutInt(g, L"resolution", res[i]); if (g != 0) PutInt(g, L"neo", neo[i]);
        PutInt(g, L"upscaler", ups[i]); PutInt(g, L"sharpness", shp[i]);
        if ((g == 1 || g == 2) && PatchState(g, L"crowd", -1) >= 0) PutInt(g, L"patch:crowd", off[i]);
        if (g <= 2 && PatchState(g, L"blur", -1) >= 0) PutInt(g, L"patch:blur", off[i]);
        if (g == 3) { PIniSet(L"ufc4_crowd_off", off[i]); PIniSet(L"ufc4_blur_off", off[i]); }
    }
    {   /* 6.2: the presets move the texture quality too; 7.3: and the effects quality at the two ends of the PS4 ports (Very low: low effects; Extreme: ultra) */
        static const int tx[7] = {1, 1, 2, 3, 4, 4, 4}, fxq[7] = {1, 0, 0, 0, 0, 0, 4};
        PutInt(g, L"pgame:tex", tx[i]); PutInt(g, L"pgame:fxq", PS4G(g) ? fxq[i] : 0);
    }
    if (UFC5G(g) || G[g].ps5) { if (p == 7) PutInt(g, L"resolution", 7); else if (G[g].ps5 && !UFC5G(g) && GetInt(g, L"resolution", 0) == 7) PutInt(g, L"resolution", 0); }
    else if (PS4G(g)) PutInt(g, L"pgame:ultrawide", p == 7);
    static const wchar_t *const pt[7] = {L"Muito baixo", L"Baixo", L"Médio", L"Alto (1080p)", L"Ultra (2K)", L"Máximo (4K)", L"Extremo"}, *const en[7] = {L"Very low", L"Low", L"Medium", L"High (1080p)", L"Ultra (2K)", L"Max (4K)", L"Extreme"};
    Msg(C_GOOD, L2(L"Preset gráfico %s aplicado ao %s: vale no próximo JOGAR. As opções abaixo mostram o que ele mudou; para voltar ao ajuste automático, escolha Automático.",
                   L"Graphics preset %s set for %s: it applies at the next PLAY. The options below show what it changed; to go back to the automatic setup, pick Automatic."),
        p == 7 ? L"Ultrawide (3440x1440)" : L2(pt[i], en[i]), G[g].name);
    LoadEdits(); InvalidateRect(g_wnd, NULL, FALSE);
}
/* 4.3.5: a setting the graphics preset also sets, changed by hand: the preset shows "Custom" instead of a name that is
   no longer true (the user's "Ultra, then the crowd off by hand"). Only each game's own preset settings count */
static void PresetTouched(int g, const wchar_t *key)
{
    static const wchar_t *const k5[] = {L"resolution", L"crowd_off", L"shadows_off", L"seats_off", L"fxaa", L"render_scale"};   /* 6.0: + the 3D size */
    static const wchar_t *const k6[] = {L"quality", L"render_scale_75", L"render_scale", L"crowd_off", L"dof_off", L"fxaa"};
    static const wchar_t *const k4[] = {L"resolution", L"neo", L"upscaler", L"sharpness", L"patch:crowd", L"patch:blur", L"pini:ufc4_crowd_off", L"pini:ufc4_blur_off"};
    const wchar_t *const *k; int nk;
    if (UFC5G(g)) { k = k5; nk = N(k5); } else if (G[g].ps5) { k = k6; nk = N(k6); } else if (PS4G(g)) { k = k4; nk = N(k4); } else return;
    if (GetInt(g, L"pgame:gfx_preset", 0) == 5) return;
    for (int i = 0; i < nk; i++) if (!wcscmp(key, k[i])) { PutInt(g, L"pgame:gfx_preset", 5); return; }
}
static void DropdownMenu(const Field *f, int hitId)
{
    HMENU m = CreatePopupMenu(); int cur = OptIndex(f, GetInt(g_sel, f->key, f->def));
    for (int i = 0; i < f->nopts; i++) AppendMenuW(m, MF_STRING | (i == cur ? MF_CHECKED : 0), 1 + i, OptName(&f->opts[i]));
    int c = PopupAt(m, hitId);
    if (c <= 0) return;
    DropdownApply(f, f->opts[c - 1].value);
}
static void DropdownApply(const Field *f, int val)   /* 6.2: also the home screen's quick options (home62.c) */
{
    PutInt(g_sel, f->key, val); PresetTouched(g_sel, f->key);
    /* Released: the one list replaces the first build's two switches and turns Automatic back on after a failed start */
    if (!wcscmp(f->key, L"dlss")) { PutInt(g_sel, L"new_engine", 0); PutInt(g_sel, L"pro_mode", 0); PutInt(g_sel, L"dlss_auto_off", 0); PutInt(g_sel, L"pro_job_n", 0); }
    /* 5.2: UFC 3 raised above 60 by hand: the automatic setup follows the monitor again (ufc3_fps60, set by a code 3 above
       60 in OnExited, would otherwise keep RecValue at 60 for good) */
    if (FpsFree(g_sel) && !wcscmp(f->key, L"fps_limit") && (val == 0 || val > 60) && PIni(Fps60Key(g_sel), 0)) PIniSet(Fps60Key(g_sel), 0);
    if (PS4G(g_sel) && !wcscmp(f->key, L"fps_limit") && FpsExperimental(g_sel, val)) {   /* 3.5.1: the experimental rates of UFC 1-4 */
        const wchar_t *nm = G[g_sel].name; wchar_t fn[48]; FpsName(val, FALSE, fn, 48);
        if (GetInt(g_sel, L"parsec", 0)) { PutInt(g_sel, L"fps_limit", 60); Msg(C_ORANGE, L2(L"Com o Modo Parsec ligado o %s fica em 60 FPS (o Parsec transmite a 60).", L"With Parsec mode on, %s stays at 60 FPS (Parsec streams at 60)."), nm); }
        else if (GpuIsIntel() && PIni(L"intel_guard", 1)) { PutInt(g_sel, L"fps_limit", 60); Msg(C_ORANGE, L2(L"Placa Intel: o %s fica em 60 FPS (acima disso a placa travava, código 23).", L"Intel graphics: %s stays at 60 FPS (above that the GPU stopped, code 23)."), nm); }
        else if (g_sel == 2) Msg(C_ORANGE, L2(L"%s no UFC 3 é experimental: em taxas muito altas a placa de vídeo pode travar (código 23). Se acontecer, o PORTHEON volta para 60 FPS sozinho.",
                                              L"%s in UFC 3 is experimental: at very high rates the graphics card may stop (code 23). If it does, PORTHEON goes back to 60 FPS by itself."), fn);
        else Msg(C_ORANGE, L2(L"%s no %s é experimental: o jogo conta o tempo por quadro, então a luta pode acelerar e o jogo pode fechar com código 3. Se acontecer, volte para 60 FPS.",
                              L"%s in %s is experimental: the game counts time per frame, so the fight may speed up and the game may close with code 3. If it does, go back to 60 FPS."), fn, nm);
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    if (!wcsncmp(f->key, L"patch:", 6) || !wcsncmp(f->key, L"pini:ufc4_", 10)) Msg(C_GOOD, L"%s", L2(L"Salvo: vale na próxima vez que o jogo abrir.", L"Saved: applies the next time the game starts."));
    else if (!wcscmp(f->key, L"pgame:gfx_preset")) GfxPreset(g_sel, val);   /* 4.3.3 */
    else if (!wcscmp(f->key, L"pgame:fxq")) FxQuality(g_sel, val);   /* 6.2 */
    else if (!wcscmp(f->key, L"pgame:engine")) {
        wchar_t arc[MAX_PATH], v30[MAX_PATH]; ArcEnginePath(arc); V30EnginePath(v30);
        if (g_sel == 2) PIniSet(L"ufc3_no_v30", 0);   /* 6.2: a new choice forgets the way back after a Career crash */
        if (g_sel >= 2) {
            PIniSet(g_sel == 2 ? L"ufc3_new_engine" : L"ufc4_new_engine", 0);   /* 3.5.3: a choice here replaces the automatic switch after a crash */
            PIniSet(g_sel == 2 ? L"ufc3_old_engine" : L"ufc4_old_engine", 0);   /* 4.3.1: and the way back after one (NewEngineFallback) */
        }
        if (g_sel == 3) PIniSet(L"ufc4_no_np", 0);   /* Released: a choice here forgets the way back from Native Plus */
        if (val == 4 || (val == 0 && WantNpEngine(g_sel)))
            Msg(C_SUB, L"%s", WantNpEngine(g_sel) ? L2(L"O próximo JOGAR usa o motor Native Plus. Se algo der errado, escolha outro aqui.", L"The next PLAY uses the Native Plus engine. If anything goes wrong, pick another one here.")
                                                  : L2(L"Esse motor não está nesta instalação.", L"That engine is not in this install."));
        else if (val == 3) Msg(C_SUB, L"%s", FileExists(v30) ? L2(L"O próximo JOGAR usa o motor do online. Se algo der errado, escolha outro aqui.", L"The next PLAY uses the online engine. If anything goes wrong, pick another one here.")
                                                         : L2(L"Esse motor não está nesta instalação.", L"That engine is not in this install."));
        else if (g_sel == 1) Msg(C_SUB, L"%s", L2(L"O próximo JOGAR usa o motor do próprio port.", L"The next PLAY uses the port's own engine."));
        else Msg(C_SUB, L"%s", !FileExists(arc) ? L2(L"O motor da correção Intel Arc não está nesta instalação (games\\engines\\intel-arc).", L"The Intel Arc engine is not in this install (games\\engines\\intel-arc).")
                         : WantArcEngine(g_sel) ? L2(L"O próximo JOGAR usa o motor novo (mais FPS, e as correções dos códigos 20 e 23, da Carreira e da Intel Arc).", L"The next PLAY uses the new engine (more FPS, and the fixes for codes 20 and 23, the Career and Intel Arc).")
                                                : L2(L"O próximo JOGAR usa o motor do próprio port.", L"The next PLAY uses the port's own engine."));
    }
    else if (!wcscmp(f->key, L"online")) OnlineChanged(g_sel, val);
    else if (!wcscmp(f->key, L"pgame:skin_mode"))   /* 3.5.3 */
        Msg(C_SUB, L"%s", val ? L2(L"Cor da pele: alternativa salva para o próximo JOGAR. É um teste: se a pele ficar certa, conte no Discord qual alternativa resolveu; se piorar, volte para Padrão.",
                                   L"Skin color: alternative saved for the next PLAY. It is a test: if the skin looks right, tell on Discord which alternative fixed it; if it gets worse, go back to Default.")
                              : L2(L"Cor da pele: padrão do motor.", L"Skin color: the engine's default."));
    InvalidateRect(g_wnd, NULL, FALSE);
}
static void ToggleSwitch(const Field *f)
{
    if (!wcscmp(f->key, L"parsec")) {
        BOOL on = !GetInt(g_sel, L"parsec", 0); ParsecSet(g_sel, on);
        { wchar_t apk[48]; ArenaParsecKey(g_sel, apk); WritePrivateProfileStringW(L"PORTHEON", apk, NULL, g_pIni); }   /* 7.3: the player's own choice now */
        if (on && !wcscmp(G[g_sel].id, L"UFC6")) Msg(C_SUB, L"%s", L2(L"Modo Parsec ligado: 2 jogadores (motor de 2 jogadores do PORTHEON), sem fila de quadros (Immediate) e controle com a janela fora de foco. Ligue o seu controle primeiro; no Parsec, compartilhe este PC com o seu amigo: o controle dele vira o Jogador 2.",
                                                                       L"Parsec mode on: 2 players (PORTHEON's 2-player engine), no frame queue (Immediate) and the controller out of focus. Connect your controller first; in Parsec, share this PC with your friend: their controller is Player 2."));
        else if (on && G[g_sel].ps5) Msg(C_SUB, L"%s", L2(L"Modo Parsec ligado: controle com a janela fora de foco, Mailbox e 60 Hz. No Parsec, compartilhe este PC com o seu amigo: ele joga com o controle dele no seu lugar (um jogador por vez neste motor).",
                                                     L"Parsec mode on: the controller out of focus, Mailbox and 60 Hz. In Parsec, share this PC with your friend: they play with their controller in your place (one player at a time on this engine)."));
        else if (on) Msg(C_SUB, L"%s", L2(L"Modo Parsec ligado: baixa latência, 60 FPS, Mailbox e controle com a janela fora de foco. No Parsec, compartilhe este PC com o seu amigo: o controle dele vira o Jogador 2.",
                                          L"Parsec mode on: low input latency, 60 FPS, Mailbox and the controller out of focus. In Parsec, share this PC with your friend: their controller is Player 2."));
        else if (G[g_sel].ps5) Msg(C_GOOD, L"%s", L2(L"Modo Parsec desligado: o controle volta a valer só com a janela do jogo em foco, e a apresentação e o vblank voltam aos do jogo.", L"Parsec mode off: the controller works only with the game's window in focus again, and the present mode and vblank are the game's own again."));
        else Msg(C_GOOD, L"%s", L2(L"Modo Parsec desligado: latência, limite de FPS e apresentação voltaram ao que eram.", L"Parsec mode off: latency, FPS limit and present mode are back to what they were."));
        InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    int v = !GetInt(g_sel, f->key, f->def); PutInt(g_sel, f->key, v); PresetTouched(g_sel, f->key);
    if (!wcscmp(f->key, L"no_time_dilation"))   /* 4.3.5 */
        Msg(v ? C_GOOD : C_SUB, L"%s", v ? L2(L"Sem câmera lenta contextual: a luta não desacelera nos momentos de esquiva e contra-ataque, mesmo com a opção ligada no jogo.", L"No contextual slow motion: the fight does not slow down at dodges and counters, even with the game's option on.")
                                          : L2(L"Câmera lenta contextual liberada: vale o que estiver no menu do jogo (Jogabilidade > Dilatação temporal contextual).", L"Contextual slow motion allowed: the game's menu decides (Gameplay > Contextual time dilation)."));
    if (!wcscmp(f->key, L"pgame:mfg")) {   /* 4.3.3 */
        wchar_t exe[MAX_PATH];
        if (v && !MfgExe(exe)) { Msg(C_ORANGE, L"%s", L2(L"MFG SUPER MOTION não está instalado: abri a página dele. Instale, configure do seu jeito e ele abre junto com o jogo.", L"MFG SUPER MOTION is not installed: its page is open. Install it, set it up your way, and it starts with the game.")); OpenUrl(MFG_URL); }
        else Msg(v ? C_SUB : C_GOOD, L"%s", v ? L2(L"Gerador de quadros ligado: o PORTHEON abre o MFG SUPER MOTION junto com o jogo. Ajuste multiplicador e gerador no próprio MFG.", L"Frame generator on: PORTHEON starts MFG SUPER MOTION with the game. Set the multiplier and generator in MFG itself.") : L2(L"Gerador de quadros desligado: o PORTHEON não abre mais o MFG SUPER MOTION com o jogo.", L"Frame generator off: PORTHEON no longer starts MFG SUPER MOTION with the game."));
    }
    if (!wcscmp(f->key, L"pini:ufc4_mouth_fix"))
        Msg(C_SUB, L"%s", v ? L2(L"Correção da boca ligada: a boca fica no lugar, mas alguns cabelos podem mostrar falhas na cabeça.", L"Mouth fix on: the mouth stays in place, but some hairstyles can show gaps on the head.")
                            : L2(L"Correção da boca desligada: o cabelo encaixa em todas as cabeças (do jeito do próprio jogo).", L"Mouth fix off: hair fits every head (the game's own way)."));
    if (!wcscmp(f->key, L"pini:career_safe_ufc4") && !v)
        Msg(C_ORANGE, L"%s", L2(L"FPS estável na Carreira desligado: as write windows voltam (mais FPS em alguns PCs, mas os crashes de código 3 do UFC 4 de 22/10 aconteceram com elas).",
                                L"Career steady FPS off: write windows are back (more FPS on some PCs, but UFC 4's code 3 crashes of 10/22 happened with them)."));
    if (PS4G(g_sel) && !wcscmp(f->key, L"ultra") && v) Msg(C_SUB, L"%s", T(S_ULTRAON));
    if (!wcscmp(f->key, L"pini:cpu_all"))   /* 3.5.3 */
        Msg(C_SUB, L"%s", v ? L2(L"Processador inteiro: o motor usa todas as threads menos duas para preparar os desenhos e um terço delas para as cópias de memória (vale no próximo JOGAR).",
                                 L"Whole processor: the engine uses every thread but two to prepare draws and a third of them for memory copies (from the next PLAY).")
                            : L2(L"Processador inteiro desligado: o motor volta à divisão dele (metade das threads para preparar os desenhos).", L"Whole processor off: the engine goes back to its own split (half the threads to prepare draws)."));
    if (!wcscmp(f->key, L"pgame:gpu_safe"))
        Msg(C_SUB, L"%s", v ? L2(L"Modo seguro de vídeo ligado: o motor grava os comandos da placa numa thread só (como o UFC 1). Um pouco menos de FPS em PCs fortes, menos travamentos da placa.",
                                 L"Safe video mode on: the engine records the GPU work on one thread (as UFC 1 does). A little less FPS on strong PCs, fewer GPU stops.")
                            : L2(L"Modo seguro de vídeo desligado.", L"Safe video mode off."));
    if (!wcscmp(f->key, L"pini:async_pipelines"))   /* 4.0.1 */
        Msg(C_SUB, L"%s", v ? L2(L"Shaders novos compilam ao lado do jogo (vale no próximo JOGAR): na 1ª vez de uma cena, um efeito pode aparecer um instante depois, em vez de a luta travar.",
                                 L"New shaders compile beside the game (from the next PLAY): the 1st time of a scene, an effect may show up a moment later instead of the fight freezing.")
                            : L2(L"Shaders compilam na hora, com o jogo esperando (como antes).", L"Shaders compile on the spot, with the game waiting (as before)."));
    InvalidateRect(g_wnd, NULL, FALSE);
}
/* "Advanced settings": the port's own window and the tools that are not everyday settings */
static void AdvMenu(void)
{
    HMENU m = CreatePopupMenu(); int g = g_sel;
    AppendMenuW(m, MF_STRING, 1, L2(L"Melhores configurações para este PC (automático)", L"Best settings for this PC (automatic)"));
    if (PS4G(g)) AppendMenuW(m, MF_STRING, 2, L2(L"Todas as opções (janela do port)", L"All options (the port's own window)"));
    if (X360G(g)) AppendMenuW(m, MF_STRING, 2, L2(L"Configurar o port", L"Configure the port"));   /* 5.0: UFC3.exe --configure */
    if (PS4G(g)) { wchar_t sf[MAX_PATH]; SkipFilePath(g, sf); AppendMenuW(m, MF_STRING | (FileExists(sf) ? 0 : MF_GRAYED), 3, L2(L"Religar os efeitos desligados depois de um código 23", L"Turn back on the effects switched off after a code 23")); }

    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, 4, L2(L"Trocar a pasta do port", L"Change the port folder"));
    AppendMenuW(m, MF_STRING, 5, L2(L"Abrir a pasta de dados (saves, logs, cache)", L"Open the data folder (saves, logs, cache)"));
    int c = PopupAt(m, H_ADV);
    if (c == 1) BestSettings();
    else if (c == 2) StartPort(g, FALSE);
    else if (c == 3) { wchar_t sf[MAX_PATH]; SkipFilePath(g, sf); DeleteFileW(sf); Msg(C_GOOD, L"%s", L2(L"Os efeitos voltam no próximo JOGAR.", L"The effects come back at the next PLAY.")); }
    else if (c == 4) ChangePort();
    else if (c == 5) {
        wchar_t d[MAX_PATH];
        if (PS4G(g)) LocalData(d, G[g].data); else if (X360G(g)) LocalData(d, L"UFC3MONS-P11-Camisas"); else PortDir(g, d);   /* 5.0: the port's settings and saves */
        if (DirExists(d)) ShellExecuteW(g_wnd, L"open", d, NULL, NULL, SW_SHOWNORMAL); else Msg(C_SUB, L"%s", T(S_NOLOGS));
    }

    InvalidateRect(g_wnd, NULL, FALSE);
}

/* ---------------------------------------------------------------- window */
static void MakeFonts(void)
{
    HFONT *all[] = {&g_fTitle, &g_fCard, &g_fText, &g_fSmall, &g_fSection, &g_fPlay, &g_fBtn, &g_fFoot, &g_fTag, &g_fHero, &g_fBrand, &g_fNav, &g_fH2, &g_fTile, &g_fBody};
    for (int i = 0; i < N(all); i++) if (*all[i]) { DeleteObject(*all[i]); *all[i] = NULL; }
    /* 4.0: the faces follow the language (lang.c: Japanese, Korean and Chinese need fonts with their characters) */
    const wchar_t *hd = FaceHead(), *bd = FaceBody();
    #define F(h, w, face) CreateFontW(-SC(h), 0, 0, 0, w, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, face)
    int hw = LangCjk() ? FW_BOLD : FW_SEMIBOLD;   /* 7.0: the Latin headings are Segoe UI Semibold (lang.c, HEAD_FACE); the CJK faces keep their bold */
    g_fTitle = F(40, hw, hd); g_fCard = F(21, hw, hd); g_fHero = F(56, hw, hd);
    g_fText = F(15, FW_NORMAL, bd); g_fSmall = F(13, FW_NORMAL, bd); g_fSection = F(13, hw, hd);
    g_fPlay = F(24, hw, hd); g_fBtn = F(15, hw, hd); g_fFoot = F(14, FW_NORMAL, bd);
    g_fTag = F(11, hw, hd); g_fBrand = F(22, hw, hd); g_fNav = F(13, hw, hd);
    g_fH2 = F(26, hw, hd); g_fTile = F(18, hw, hd); g_fBody = F(15, FW_NORMAL, bd);
    #undef F
    if (g_editGame) { SendMessageW(g_editGame, WM_SETFONT, (WPARAM)g_fText, TRUE); SendMessageW(g_editPlayer, WM_SETFONT, (WPARAM)g_fText, TRUE); SendMessageW(g_editAddr, WM_SETFONT, (WPARAM)g_fText, TRUE); }
    if (g_editProf) SendMessageW(g_editProf, WM_SETFONT, (WPARAM)g_fText, TRUE);
}
/* the scale of the whole window: from its client width in a window (aspect kept by WM_SIZING); in full screen the
   1280x800 frame fits the monitor and is centred (g_ox, g_oy) */
static void SetScaleFromClient(int cw, int ch)
{
    int d = (int)((double)cw * 96 / BASE_W + 0.5);
    if (g_full && ch > 0) { int d2 = (int)((double)ch * 96 / BASE_H); if (d2 < d) d = d2; }
    if (d < 40) d = 40;
    g_ox = g_full ? (cw - MulDiv(BASE_W, d, 96)) / 2 : 0; g_oy = g_full ? (ch - MulDiv(BASE_H, d, 96)) / 2 : 0;
    if (g_ox < 0) g_ox = 0; if (g_oy < 0) g_oy = 0;
    if (d != g_dpi) { g_dpi = d; MakeFonts(); ScaleChanged(); }
}
/* 4.0: full screen like a console (F11, the power menu, the settings): a borderless window over the whole monitor */
static WINDOWPLACEMENT g_wpPrev = {sizeof(WINDOWPLACEMENT)};
static void SetFullscreen(BOOL on)
{
    if (on == g_full) return;
    HWND w = g_wnd; DWORD style = (DWORD)GetWindowLongW(w, GWL_STYLE);
    if (on) {
        MONITORINFO mi = {sizeof(mi)};
        if (!GetWindowPlacement(w, &g_wpPrev) || !GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTOPRIMARY), &mi)) return;
#ifdef PORTHEON_TEST   /* PORTHEON_TEST_FULL=2560x1440: full screen as on a monitor of that size (UI shots) */
        {   /* 4.0.4: "1920x1080@-4000,0" also moves it off the screen, so the shots do not cover the desktop */
            wchar_t fs[48]; int fw, fh, fx, fy, k; if (GetEnvironmentVariableW(L"PORTHEON_TEST_FULL", fs, 48) && (k = swscanf(fs, L"%dx%d@%d,%d", &fw, &fh, &fx, &fy)) >= 2) {
                if (k == 4) { mi.rcMonitor.left = fx; mi.rcMonitor.top = fy; }
                mi.rcMonitor.right = mi.rcMonitor.left + fw; mi.rcMonitor.bottom = mi.rcMonitor.top + fh; } }
#endif
        g_full = TRUE;
        SetWindowLongW(w, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
        SetWindowPos(w, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        g_full = FALSE;
        SetWindowLongW(w, GWL_STYLE, (style & ~WS_POPUP) | ((WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX)));
        SetWindowPlacement(w, &g_wpPrev);
        SetWindowPos(w, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    PIniSet(L"ui_fullscreen", g_full);
    RECT cr; GetClientRect(w, &cr); SetScaleFromClient(cr.right, cr.bottom);
    InvalidateRect(w, NULL, FALSE);
}
static void OnDrop(HDROP hd)
{
    wchar_t p[MAX_PATH]; UINT n = DragQueryFileW(hd, 0xFFFFFFFF, NULL, 0);
    if (!n || !DragQueryFileW(hd, 0, p, MAX_PATH)) { DragFinish(hd); return; }
    DragFinish(hd); StripSlash(p);
    /* 5.0: an Xbox 360 .iso, or a folder with default.xex or an .iso, is UFC Undisputed 3's */
    if (FileExists(p) && !_wcsicmp(PathFindExtensionW(p), L".iso")) { X3Normalize(p); }
    if (DirExists(p) && X3Folder(p) && DetectFolder(p, NULL, NULL, 0) < 0) {
        Select(6); SetGameFolder(6, p); g_overlay = OV_NONE; g_view = V_GAME; g_tab = TAB_GAME; g_focus = -1; InvalidateRect(g_wnd, NULL, FALSE); return;
    }
    /* 6.0: a Switch file, or a folder with exefs\main, is Zelda's; a CUSA01623 dump is God of War III's */
    {   int eg = -1; wchar_t zf[MAX_PATH];
        if ((FileExists(p) && ZeldaExt(p)) || (DirExists(p) && ExtGameOk(7, p) && DetectFolder(p, NULL, NULL, 0) < 0)) eg = 7;
        else if (DirExists(p) && ExtGameOk(8, p)) eg = 8;
        else if (DirExists(p) && ExtGameOk(9, p)) eg = 9;     /* 6.1: Bloodborne */
        else if (DirExists(p) && ExtGameOk(10, p)) eg = 10;   /* 6.1: inFAMOUS Second Son */
        (void)zf;
        if (eg >= 0) { Select(eg); SetGameFolder(eg, p); g_overlay = OV_NONE; g_view = V_GAME; g_tab = TAB_GAME; g_focus = -1; InvalidateRect(g_wnd, NULL, FALSE); return; }
    }
    if (!DirExists(p)) {
        const wchar_t *ext = PathFindExtensionW(p);
        if (!_wcsicmp(ext, L".pkg") || !_wcsicmp(ext, L".iso") || !_wcsicmp(ext, L".rar") || !_wcsicmp(ext, L".zip") || !_wcsicmp(ext, L".7z"))
            Msg(C_ORANGE, L2(L"\"%s\" é um arquivo fechado: o port precisa da pasta já extraída (com eboot.bin, sce_sys e sce_module). Veja o passo a passo em \"Como instalar\".",
                             L"\"%s\" is a packed file: the port needs the extracted folder (with eboot.bin, sce_sys and sce_module). See the steps in \"How to install\"."), PathFindFileNameW(p));
        else Msg(C_ORANGE, L"%s", L2(L"Arraste a pasta do jogo (não um arquivo).", L"Drop the game's folder (not a file)."));
        OpenGuide(g_sel); return;
    }
    wchar_t why[400]; int g = DetectFolder(p, NULL, why, 400);
    if (g < 0) { Msg(C_ORANGE, L2(L"Não reconheci um UFC em \"%s\". Escolha a pasta que tem eboot.bin, sce_sys e sce_module (ou a pasta do Reborn).", L"No UFC found in \"%s\". Pick the folder with eboot.bin, sce_sys and sce_module (or the Reborn folder)."), PathFindFileNameW(p)); return; }

    Select(g); SetGameFolder(g, p); g_overlay = OV_NONE; g_view = V_GAME; g_tab = TAB_GAME; g_focus = -1; InvalidateRect(g_wnd, NULL, FALSE);
}

static LRESULT CALLBACK WndProc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_CREATE: g_wnd = w; return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { HRGN rg = CreateRectRgn(0, 0, 0, 0); if (GetUpdateRgn(w, rg, FALSE) == ERROR) { DeleteObject(rg); rg = NULL; }
                     PAINTSTRUCT ps; HDC dc = BeginPaint(w, &ps); Paint(dc, &ps.rcPaint, rg); EndPaint(w, &ps); if (rg) DeleteObject(rg); return 0; }
    case WM_CTLCOLOREDIT: { HDC dc = (HDC)wp; SetBkColor(dc, C_FIELD); SetTextColor(dc, C_TEXT); return (LRESULT)g_brField; }
    case WM_MOUSEWHEEL:   /* 6.2: a page whose options do not fit (PaintItems) */
        if (HomeWheel(GET_WHEEL_DELTA_WPARAM(wp))) return 0;   /* Released: the home's two pages (ui_input.c) */
        if (g_itMax > 0 && g_overlay == OV_NONE) { int r = g_itRow + (GET_WHEEL_DELTA_WPARAM(wp) < 0 ? 1 : -1); if (r >= 0 && r <= g_itMax) { g_itRow = r; g_lastInput = GetTickCount(); InvalidateRect(w, NULL, FALSE); } }
        return 0;
    case WM_MOUSEMOVE: {
        static LPARAM last; if (lp == last) return 0; last = lp;   /* only a real move takes the focus from the keyboard */
        g_kbNav = FALSE; g_lastInput = GetTickCount();
        int h = HitAt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (h != g_hot) { int old = g_hot; g_hot = h; HotChanged(old, h); TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, w, 0}; TrackMouseEvent(&t); }
        return 0;
    }
    case WM_MOUSELEAVE: if (g_hot != -1) { int old = g_hot; g_hot = -1; HotChanged(old, -1); } return 0;
    case WM_SETCURSOR: if (LOWORD(lp) == HTCLIENT && g_hot >= 0) { SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND)); return TRUE; } break;
    case WM_LBUTTONDBLCLK: {
        int h = HitAt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (h >= H_CARD && h < H_CARD + NGAMES && !g_overlay) { Select(h - H_CARD); OpenGame(); }
        return 0;
    }
    case WM_LBUTTONDOWN: SetFocus(w); g_kbNav = FALSE; g_lastInput = GetTickCount(); { int h = HitAt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); if (h >= 0) g_focus = h; OnClick(h); } return 0;
    case WM_RBUTTONDOWN: if (KeysCaptureMouse(L"Mouse:Right")) return 0; if (!g_overlay) Back(); return 0;   /* 4.0: like a controller's Circle; 6.0: or a key being chosen */
    case WM_MBUTTONDOWN: KeysCaptureMouse(L"Mouse:Middle"); return 0;   /* 6.0 */
    case WM_XBUTTONDOWN: if (KeysCaptureMouse(GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? L"Mouse:X1" : L"Mouse:X2")) return TRUE; if (GET_XBUTTON_WPARAM(wp) == XBUTTON1) Back(); return TRUE;
    case WM_SYSKEYDOWN: if (KeysCaptureKey(wp, lp) || CkCaptureKey(wp, lp)) return 0; break;   /* 6.0: Alt while a key is chosen */
    case WM_KEYDOWN:
        if (KeysCaptureKey(wp, lp) || CkCaptureKey(wp, lp)) return 0;   /* 6.0: the keyboard screen waits for a key */
        if ((wp == VK_RETURN || wp == VK_SPACE) && (lp & 0x40000000)) return 0;   /* 4.3.1: held since before (g_quietUntil) */
        if (wp == VK_F5) { for (int i = 0; i < NGAMES; i++) { ReadVersion(i); ForgetArt(i); } InvalidateRect(w, NULL, FALSE); }
        else if (wp == 'Q') NavKey(VK_PRIOR);
        else if (wp == 'E') NavKey(VK_NEXT);
        else NavKey((int)wp);
        return 0;
    case WM_COMMAND: if (HIWORD(wp) == EN_KILLFOCUS) { SaveEdits(); InvalidateRect(w, NULL, FALSE); } return 0;
    case WM_DROPFILES: OnDrop((HDROP)wp); return 0;
    case WM_COPYDATA: {   /* 3.5.2: a desktop shortcut started while PORTHEON was open (OtherInstance) */
        const COPYDATASTRUCT *cd = (const COPYDATASTRUCT *)lp;
        if (cd && cd->dwData == 0x504C4159 && cd->cbData >= sizeof(wchar_t) && cd->cbData <= 32 * sizeof(wchar_t)) {
            wchar_t id[33] = L""; memcpy(id, cd->lpData, cd->cbData); id[32] = 0;
            BOOL arena = FALSE; { wchar_t *a = wcsstr(id, L"@A"); if (a) { *a = 0; arena = TRUE; } }   /* 7.3: from the ARENA's host button */
            int dg = GameById(id); if (dg >= 0 && !SOON(dg)) PostMessageW(w, WM_APP_DIRECTPLAY, (WPARAM)dg, arena);   /* not inside SendMessage */
            return TRUE;
        }
        break;
    }
    case WM_APP_DIRECTPLAY: {
        int dg = (int)wp;
        if (lp && !g_proc) ArenaHostPrep(dg);   /* 7.3 */
        if (g_proc) Msg(C_ORANGE, L2(L"O %s já está aberto: feche esse jogo antes de abrir outro.", L"%s is already running: close it before starting another game."), G[g_procGame >= 0 ? g_procGame : dg].name);
        else { if (dg != g_sel || g_csel >= 0) Select(dg); g_overlay = OV_NONE; Play(); }   /* PORTHEON was open already: it stays open after the game (6.1: also with a custom game shown) */
        InvalidateRect(w, NULL, FALSE); return 0;
    }
    case WM_APP_EXITED: OnExited((DWORD)wp); return 0;
    case VIDEO_MSG: VitOnVideoFrame(); return 0;           /* 6.2: a new video frame (video.c) */
    case WM_APP_VITOPEN: VitOnOpened(wp, lp); return 0;   /* 6.2: the vitrine's video opened (or not) */
    case WM_APP_VITART: VitOnArt(wp, lp); return 0;       /* 6.2: the vitrine's pictures are ready */
    case WM_APP_YT: YtWndMsg(wp, lp); return 0;           /* 6.2: YouTube's web view (yt.c) */
    case WM_APP_LIBWATCH: LibOnWatch(wp, lp); return 0;   /* 6.1: a custom game started / ended (library.c) */
    case WM_APP_LIBSCAN: LibOnScan(wp, lp); return 0;     /* 6.1: the Add screen's Steam / Epic list was read */
    case WM_APP_UPDATE: OnUpdateFound((int)wp); InvalidateRect(w, NULL, FALSE); return 0;
    case WM_APP_DLDONE: OnDownloadDone(wp != 0); return 0;
    case WM_APP_EXTINST: ExtInstallMsg(wp, lp); return 0;   /* 6.1: extgames.c, installing Bloodborne's / inFAMOUS's port */
    case WM_APP_SCAN: OnScanDone((int)wp); return 0;
    case WM_APP_AUTOCFG:
        g_stTick = 0;
        if (lp >= 5) {   /* a PS5 game's launcher set itself up for this PC (1: hardware changed, 2: "Best settings"); 4.3: lp = 5 + its index - 4 */
            int pg = (int)lp - 1; if (pg < 4 || pg >= NGAMES || !G[pg].ps5) pg = 4;
            if (wp == 1 || wp == 2) { wchar_t sum[400]; PcSummary(sum, 400); Msg(C_GOOD, L2(L"%s configurado para este PC (%s).", L"%s set up for this PC (%s)."), G[pg].name, sum); if (g_sel == pg) LoadEdits(); }
            InvalidateRect(w, NULL, FALSE);
            return 0;
        }
        if (g_autoDone4) {
            wchar_t sum[400]; PcSummary(sum, 400);
            Msg(C_GOOD, L2(L"Melhores configurações escolhidas para este PC (UFC 1-4): %s", L"Best settings picked for this PC (UFC 1-4): %s"), sum);
            g_autoDone4 = 0; g_kvGame = -1; InvalidateRect(w, NULL, FALSE);
        }
        if (GpuIsArc() && !GpuHasRebar())   /* the one thing an Arc card cannot do without */
            Msg(C_ORANGE, L"%s", L2(L"Intel Arc sem Resizable BAR: ligue \"Re-Size BAR\" e \"Above 4G Decoding\" na BIOS, senão a placa perde muito FPS.",
                                    L"Intel Arc without Resizable BAR: turn on \"Re-Size BAR\" and \"Above 4G Decoding\" in the BIOS, or the card loses a lot of FPS."));
        return 0;
    case WM_APP_FROZEN:   /* the engine stopped getting GPU work done: say so and where the diagnosis went */
        if (!IsWindowVisible(w)) ShowWindow(w, SW_SHOWMINNOACTIVE);   /* single window: hidden during the game (FocusTimer): its button back, to be found */
        FlashWindow(w, TRUE);
        Msg(C_BAD, L2(L"O %s travou (a GPU parou de responder). O diagnóstico foi salvo na pasta de logs (%s). Clique em PARAR para fechar.",
                      L"%s froze (the GPU stopped answering). The diagnosis was saved to the logs folder (%s). Click STOP to close it."),
            G[wp < NGAMES ? wp : 0].name, PathFindFileNameW(g_frozenFile));
        InvalidateRect(w, NULL, FALSE); return 0;
    case WM_ACTIVATE: if (LOWORD(wp) != WA_INACTIVE) { g_kvGame = -1; for (int i = 0; i < NGAMES; i++) ReadVersion(i); LibReload(); InvalidateRect(w, NULL, FALSE); } break;   /* 6.1: LibReload */
    case WM_SIZING: {   /* the layout keeps its proportions: the dragged edge decides, the other follows */
        if (g_full) return TRUE;
        RECT *r = (RECT *)lp, nc = {0, 0, 0, 0}; UINT dpi = GetDpiForWindow(w);
        AdjustWindowRectExForDpi(&nc, (DWORD)GetWindowLongW(w, GWL_STYLE), FALSE, (DWORD)GetWindowLongW(w, GWL_EXSTYLE), dpi);
        int ncw = nc.right - nc.left, nch = nc.bottom - nc.top, cw = r->right - r->left - ncw, ch = r->bottom - r->top - nch;
        if (wp == WMSZ_TOP || wp == WMSZ_BOTTOM) cw = ch * BASE_W / BASE_H; else ch = cw * BASE_H / BASE_W;
        if (wp == WMSZ_LEFT || wp == WMSZ_TOPLEFT || wp == WMSZ_BOTTOMLEFT) r->left = r->right - cw - ncw; else r->right = r->left + cw + ncw;
        if (wp == WMSZ_TOP || wp == WMSZ_TOPLEFT || wp == WMSZ_TOPRIGHT) r->top = r->bottom - ch - nch; else r->bottom = r->top + ch + nch;
        return TRUE;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp; UINT dpi = GetDpiForWindow(w); if (!dpi) dpi = 96;
        RECT r = {0, 0, BASE_W * 55 / 100 * (int)dpi / 96, BASE_H * 55 / 100 * (int)dpi / 96};
        AdjustWindowRectExForDpi(&r, (DWORD)GetWindowLongW(w, GWL_STYLE), FALSE, (DWORD)GetWindowLongW(w, GWL_EXSTYLE), dpi);
        mm->ptMinTrackSize.x = r.right - r.left; mm->ptMinTrackSize.y = r.bottom - r.top;
        return 0;
    }
    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) { if (g_proc) PlayPark(); }   /* 4.3.2 */
        else if (LOWORD(lp) > 0) { SetScaleFromClient(LOWORD(lp), HIWORD(lp)); InvalidateRect(w, NULL, FALSE); }
        return 0;
    case WM_DPICHANGED: {
        RECT *r = (RECT *)lp; SetWindowPos(w, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE); return 0;
    }
    case WM_ENDSESSION: if (wp) PendingWrite(); return 0;   /* 5.2: Windows shuts down with a game running (see PendingWrite) */
    case WM_CLOSE: {
        PendingWrite();   /* 5.2: closed by hand during a game: the next start follows it (LateExited) */
        SaveEdits();
        WINDOWPLACEMENT wpl = {sizeof(wpl)};
        if (!IsIconic(w) && GetWindowPlacement(w, &wpl)) {
            UINT dpi = GetDpiForWindow(w); if (!dpi) dpi = 96;
            PIniSet(L"win_x", wpl.rcNormalPosition.left); PIniSet(L"win_y", wpl.rcNormalPosition.top);
            PIniSet(L"win_scale", g_dpi * 1000 / (int)dpi);
        }
        DestroyWindow(w); return 0;
    }
    case WM_TIMER:
        if (wp == 7) { Animate(); return 0; }
        if (wp == 9) { StartUpdateCheck(FALSE); return 0; }   /* every 6 hours while PORTHEON stays open */
        if (wp == 11) { FocusTimer(); return 0; }             /* 3.5.1: the game's window, to hand it the focus */
        if (wp == 12) { CloseTimer(); return 0; }             /* 4.3.6: PORTHEON closes while the game runs */
        if (wp == 13) { LaunchTick(); return 0; }             /* 4.4.0: the loading ring while the game opens */
        if (wp == 14) { HandoffTimer(); return 0; }           /* 4.4.0: the PLAY instance's window is up */
        if (wp == VIT_TIMER) { VitTimer(); return 0; }        /* 6.2: the live edit's frames (home62.c) */
        break;
    case WM_DESTROY: VitStop(); YtDestroy(); KillTimer(w, 7); KillTimer(w, 9); KillTimer(w, 13); KillTimer(w, 14); MusicQuit(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

/* dark context menus (uxtheme ordinals; ignored where unavailable) */
static void DarkMenus(void)
{
    HMODULE ux = LoadLibraryW(L"uxtheme.dll"); if (!ux) return;
    typedef int (WINAPI *SetMode)(int); typedef void (WINAPI *Flush)(void);
    SetMode sm = (SetMode)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(135)); Flush fl = (Flush)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(136));
    if (sm) sm(2); if (fl) fl();
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdline, int show)
{
    (void)prev;
    g_inst = inst;
    SetUnhandledExceptionFilter(CrashReport);   /* 3.5.2 */
    AddVectoredExceptionHandler(1, ThreadNameVeh);   /* 6.1: a thread-name exception never closes PORTHEON (CrashReport) */
    if (wcsstr(cmdline, L"--apply-update")) return ApplyUpdate(inst);   /* 3.5: PORTHEON installs its own update */
    if (wcsstr(cmdline, L"--set-pagefile")) return SetPagefile();      /* 4.0.5: elevated, from PagefileOffer */
    GetModuleFileNameW(NULL, g_exeDir, MAX_PATH); { wchar_t *e = wcsrchr(g_exeDir, L'\\'); if (e) *e = 0; }
    { wchar_t d[MAX_PATH]; LocalData(d, L"PORTHEON"); CreateDirectoryW(d, NULL); Join(g_pIni, d, L"portheon.ini"); }
    g_lang = LangLoad(g_pIni);   /* 4.0: ui_lang, or the old PT/EN switch, or Windows' own language */
    wchar_t play[16] = L"";   /* 3.5.2: --play <game id>, from a desktop shortcut */
    { const wchar_t *p = wcsstr(cmdline, L"--play"); if (p) { p += 6; while (*p == L' ' || *p == L'"') p++; int i = 0; while (*p && *p != L' ' && *p != L'"' && i < 15) play[i++] = *p++; play[i] = 0; } }
    BOOL arenaStart = play[0] && wcsstr(cmdline, L"--arena") != NULL;   /* 7.3: the ARENA's host button; "@A" carries it to a PORTHEON already open */
    if (arenaStart && wcslen(play) < 13) wcscat(play, L"@A");
    HANDLE single = CreateMutexW(NULL, TRUE, MUTEX_NAME); g_single = single;
    if (GetLastError() == ERROR_ALREADY_EXISTS && !OtherInstance(single, play)) return 0;
    { wchar_t *a = wcsstr(play, L"@A"); if (a) *a = 0; }
    g_playInstance = play[0] != 0; g_backToUi = g_playInstance && wcsstr(cmdline, L"--back") != NULL;   /* 4.1: started by the window's PLAY */
    /* 4.3.1: opened again after a game (RelaunchSelf in OnExited): the same quiet moment, and behind the program the
       player is in when the game had lost the focus to it */
    if (wcsstr(cmdline, L"--after-game")) { g_quietUntil = GetTickCount64() + 2000; if (wcsstr(cmdline, L"--behind")) show = SW_SHOWMINNOACTIVE; }
    int direct = play[0] ? GameById(play) : -1; if (direct >= 0 && SOON(direct)) direct = -1;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    {   /* 5.0 (the user: "it shows as UFC 6's icon"): PORTHEON's own identity on the taskbar. Started from a game's
           desktop shortcut, Windows showed that shortcut's picture (UFC 6) for PORTHEON's window; with its own ID the
           taskbar shows PORTHEON's logo, and the PORTHEON shortcut (PortheonShortcut) carries the same ID for pinning */
        HMODULE sh = GetModuleHandleW(L"shell32.dll"); typedef HRESULT (WINAPI *SetId)(PCWSTR);
        SetId f = sh ? (SetId)(void *)GetProcAddress(sh, "SetCurrentProcessExplicitAppUserModelID") : NULL; if (f) f(PORTHEON_AUMID);
    }
    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_STANDARD_CLASSES}; InitCommonControlsEx(&ic);
    GdiplusStartupInput gi = {1, NULL, FALSE, FALSE}; GdiplusStartup(&g_gdip, &gi, NULL);
    DarkMenus();

    { wchar_t o[MAX_PATH], l[MAX_PATH], pd[MAX_PATH]; Join(o, g_exeDir, L"PORTHEON.exe.old"); LocalData(pd, L"PORTHEON"); Join(l, pd, L"update-leftovers.txt");
      if (FileExists(o) || FileExists(l)) { HANDLE th = CreateThread(NULL, 0, DropOldExe, NULL, 0, NULL); if (th) CloseHandle(th); } }   /* 5.1: also what the last install left */
    wchar_t selId[16]; GetPrivateProfileStringW(L"PORTHEON", L"selected_game", L"UFC4", selId, 16, g_pIni);
    for (int i = 0; i < NGAMES; i++) { ReadVersion(i); if (!_wcsicmp(selId, G[i].id) && !SOON(i)) g_sel = i; }
    LibLoad(); if (direct >= 0) g_csel = -1;   /* 6.1: the player's own games (library.ini); a desktop shortcut's game is the one shown */

    g_imgBg = NULL; g_imgLogo = LoadPngRes(101); ThemeLoad(); SkinsLoad();   /* perf 11-13: HzScan() left this line: HzScanOnce, at the first list of options (tune62.c, BuildItems) */   /* 6.0: the new logo (the user's design); the circuit background is gone (the console's own look) */
    g_brField = CreateSolidBrush(C_FIELD);

    ProfilesLoad();   /* 4.0 */
    WNDCLASSEXW wc = {sizeof(wc)}; wc.lpfnWndProc = WndProc; wc.hInstance = inst; wc.lpszClassName = WNDCLASS_NAME; wc.style = CS_DBLCLKS;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW); wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    RegisterClassExW(&wc);
    /* 3.5: the window fits each player's monitor. It is resizable (proportions kept), never larger than 92% of the
       monitor's work area, centred, and comes back at the size and place it had. On a 1366x768 laptop the 3.4 window
       (1260x830 at 100%) was taller than the screen. */
    DWORD style = (WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX) | WS_CLIPCHILDREN, exstyle = WS_EX_ACCEPTFILES;
    POINT cur; GetCursorPos(&cur); HMONITOR mon = MonitorFromPoint(cur, MONITOR_DEFAULTTOPRIMARY);
    int sx = PIni(L"win_x", -99999), sy = PIni(L"win_y", -99999);
    if (sx != -99999) { POINT p = {sx + 60, sy + 20}; HMONITOR m2 = MonitorFromPoint(p, MONITOR_DEFAULTTONULL); if (m2) mon = m2; }   /* the monitor it was on */
    MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(mon, &mi);
    UINT dx = 96, dy = 96; HMODULE sh = LoadLibraryW(L"shcore.dll");
    if (sh) { typedef HRESULT (WINAPI *GDFM)(HMONITOR, int, UINT *, UINT *); GDFM f = (GDFM)(void *)GetProcAddress(sh, "GetDpiForMonitor"); if (f) f(mon, 0, &dx, &dy); }
    int waw = mi.rcWork.right - mi.rcWork.left, wah = mi.rcWork.bottom - mi.rcWork.top;
    RECT ncr = {0, 0, 0, 0}; AdjustWindowRectExForDpi(&ncr, style, FALSE, exstyle, dx);
    int ncw = ncr.right - ncr.left, nch = ncr.bottom - ncr.top;
    double sc = (double)dx / 96.0, sFitW = (0.92 * waw - ncw) / BASE_W, sFitH = (0.92 * wah - nch) / BASE_H;
    if (sFitW < sc) sc = sFitW;
    if (sFitH < sc) sc = sFitH;
    { int saved = PIni(L"win_scale", 0); if (saved >= 400) { double s2 = saved / 1000.0 * dx / 96.0; if (s2 < sc) sc = s2; } }
    if (sc < 0.55) sc = 0.55;
    g_dpi = (int)(96 * sc + 0.5);
    MakeFonts();
    /* 4.0: the opening is the boot animation inside the window (a desktop shortcut's start skips it) */
    g_view = direct < 0 && PIni(L"boot_anim", 1) ? V_BOOT : V_HOME;
    if (direct < 0 && g_view == V_HOME) OpenOnReady();   /* 5.0 */
    /* 4.4.0: PLAY (or a desktop shortcut) opens on the game's picture, as a console does, until the game's window shows;
       after the window's PLAY (--back) that window was already showing it: no fade from black */
    if (direct >= 0 && PIni(L"launch_splash", 1)) { g_view = V_LAUNCH; g_launchGame = direct; g_launchT0 = GetTickCount() - (g_backToUi ? 1000 : 0); }
    int ww = SC(BASE_W) + ncw, wh = SC(BASE_H) + nch;
    int x = mi.rcWork.left + (waw - ww) / 2, y = mi.rcWork.top + (wah - wh) / 2;
    if (sx != -99999 && sx >= mi.rcWork.left - 20 && sy >= mi.rcWork.top - 20 && sx + ww <= mi.rcWork.right + 20 && sy + wh <= mi.rcWork.bottom + 20) { x = sx; y = sy; }
    if (y < mi.rcWork.top) y = mi.rcWork.top;
    HWND w = CreateWindowExW(exstyle, wc.lpszClassName, L"Portheon " APP_SHOW, style, x, y, ww, wh, NULL, NULL, inst, NULL);   /* 7.1: the shown name (7.0: "PORTHEON - Ports & Mods"); nothing finds this window by its title */
    BOOL dark = TRUE; DwmSetWindowAttribute(w, 20, &dark, sizeof(dark));
    COLORREF cap = RGB(7, 9, 15); DwmSetWindowAttribute(w, 35 /* DWMWA_CAPTION_COLOR */, &cap, sizeof(cap));
    g_editGame = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, w, (HMENU)300, inst, NULL);
    g_editPlayer = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, w, (HMENU)301, inst, NULL);
    g_editAddr = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, w, (HMENU)302, inst, NULL);
    SendMessageW(g_editGame, WM_SETFONT, (WPARAM)g_fText, TRUE); SendMessageW(g_editPlayer, WM_SETFONT, (WPARAM)g_fText, TRUE); SendMessageW(g_editAddr, WM_SETFONT, (WPARAM)g_fText, TRUE);
    g_editProf = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, w, (HMENU)303, inst, NULL);
    SendMessageW(g_editProf, WM_SETFONT, (WPARAM)g_fText, TRUE); SendMessageW(g_editProf, EM_LIMITTEXT, 24, 0);
    SendMessageW(g_editPlayer, EM_LIMITTEXT, 24, 0); SendMessageW(g_editAddr, EM_LIMITTEXT, 60, 0);
    { RECT cr; GetClientRect(w, &cr); if (cr.right > 0) SetScaleFromClient(cr.right, cr.bottom); }
    LoadEdits();
    g_bootT0 = GetTickCount();
    /* 4.0.5: the first start sets every game up (DetectPc, AutoConfigAll below) before the window runs, and the first
       start of a new version waits on Windows looking at the new files: up to 8 to 10 s with nothing moving, black on the
       whole monitor now that PORTHEON opens in full screen. Those starts paint the boot's logo, name and subtitle already
       in place (its frame at 1.7 s), and the animation goes on from there. */
    {   wchar_t seen[32]; GetPrivateProfileStringW(L"PORTHEON", L"news_seen", L"", seen, 32, g_pIni);
        if (PIni(L"auto_rev", 0) < 33 || wcscmp(seen, APP_VER)) g_bootT0 -= 950; }   /* 4.3.6: 1.7 s of the 1.8x faster animation */
    /* 4.0.4: PORTHEON opens in full screen, like a console (the user asked for it). Those who had it in a window get it
       once; then the window button at the top (or F11) decides, and PORTHEON remembers. A desktop shortcut's start
       (straight into the game) stays in a window. Before ShowWindow, so the window does not flash at its old size. */
    if (PIni(L"ui_full_rev", 0) < 404) { PIniSet(L"ui_fullscreen", 1); PIniSet(L"ui_full_rev", 404); }
#ifndef PORTHEON_TEST   /* 5.0: PORTHEON's own desktop shortcut, once (never from a test build) */
    if (direct < 0 && !PIni(L"portheon_lnk", 0)) { PortheonShortcut(); PIniSet(L"portheon_lnk", 1); }
    IconRefreshOnce();   /* 6.0 final: the new logo on the shortcuts and the taskbar, not Windows' cached old one */
#endif
    if ((direct < 0 || g_view == V_LAUNCH) && PIni(L"ui_fullscreen", 1)) SetFullscreen(TRUE);
#ifdef PORTHEON_TEST
    if (TestQuiet()) { if (!g_full) SetWindowPos(w, NULL, -4000, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE); show = SW_SHOWNOACTIVATE; }
#endif
    ShowWindow(w, show); UpdateWindow(w);
    if (show == SW_SHOWMINNOACTIVE) { FLASHWINFO fi = {sizeof(fi), w, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0}; FlashWindowEx(&fi); }   /* --behind */
    else if (g_view == V_BOOT) Snd(SND_BOOT);
    SetTimer(w, 7, 30, NULL);                 /* effects; perf 11-13: 30, not 33: Windows rounds a timer up to its 15.6 ms steps, so 33 came every 47 ms (21 frames a second) and 30 comes every 31-32 (31 a second) */
    SetTimer(w, 9, 6 * 3600 * 1000, NULL);    /* update check while open */
    if (g_view == V_BOOT && direct < 0) g_guardMeanwhile = HomePrewarmBg;   /* perf 11-13: the home's background, while DetectPc waits for the Vulkan probe (ui_screens.c) */
    DetectPc(); g_guardMeanwhile = NULL;
#ifdef PORTHEON_TEST
    /* 4.3.8 test builds only: --selftest438 <file> writes the verdict of every game, the fix list applied to UFC 6 and
       the test channel's choice, then exits (no window shown, no game started) */
    { const wchar_t *st = wcsstr(cmdline, L"--selftest438 ");
      if (st) {
          wchar_t sp[MAX_PATH]; const wchar_t *q = st + 14; while (*q == L' ' || *q == L'"') q++; wcsncpy(sp, q, MAX_PATH - 1); sp[MAX_PATH - 1] = 0; { wchar_t *e = wcschr(sp, L'"'); if (e) *e = 0; } FILE *f = _wfopen(sp, L"w, ccs=UTF-8");
          if (f) {
              for (int g = 0; g < NGAMES; g++) { if (SOON(g)) continue; wchar_t v[2400]; int lv = PcVerdict(g, v, 2400); fwprintf(f, L"== %s: level %d\n%s\n\n", G[g].name, lv, v); }
              VulkanCheck(); fwprintf(f, L"vk: vendor %04x device %04x driver %08x (%s, id %u) bary %d\n", g_vkc.vendor, g_vkc.devid, g_vkc.driver, g_vkc.drv, g_vkc.drvId, g_vkc.bary);   /* 5.2: the decoded driver */
              FixesApply(4); { wchar_t d[MAX_PATH], rf[MAX_PATH]; PortDir(4, d); Join(rf, d, L"bin\\portheon-remote-skip.txt"); FILE *r = _wfopen(rf, L"rb"); char b[600] = ""; if (r) { b[fread(b, 1, 599, r)] = 0; fclose(r); } fwprintf(f, L"ufc6 remote skip: %S\n", b); }
              { wchar_t w[1200]; FixWarnings(4, w); fwprintf(f, L"ufc6 warnings: %s\n", w); }
              DWORD status = 0, len = 0; char *buf = HttpGet(L"https://" UPD_API_HOST L"/repos/XAndromedaXR/PORTHEON/releases?per_page=15", TRUE, &status, NULL, 0, &len);
              char tag[32] = ""; char *rel = buf && status == 200 ? BetaRelease(buf) : NULL; if (rel) JsonStr(rel, "tag_name", tag, 32);
              fwprintf(f, L"beta: http %lu, newest release %S\n", status, tag); free(buf);
              fclose(f);
          }
          return 0;
      } }
#endif
    MigrateOldInstall();   /* 5.1: UFC 5 / 6 saves from an older PORTHEON folder, before UFC5/6.exe --auto-config writes their ini */
    for (int g = 0; g < 4; g++) if (CAM3P_XML[g]) Cam3pMigrate(g);   /* 4.3.2: the camera switch shows a hand-installed mod */
    {   /* 3.3: every installed UFC 1-4 gets the steady setup once; 3.5 changes only what its fixes need (no reset) */
        BOOL redo = PIni(L"auto_rev", 0) < 33;
        g_autoDone4 = AutoConfigAll(redo);
        if (redo) WritePrivateProfileStringW(L"PORTHEON", L"auto_rev", L"35", g_pIni);
        else if (PIni(L"auto_rev", 0) < 35) {
            if (PcTier() < 2 || g_oldCpu) PIniSet(L"ufc4_crowd_off", 1);   /* the UFC 4 crowd switch is new: weak PCs start with it off */
            WritePrivateProfileStringW(L"PORTHEON", L"auto_rev", L"35", g_pIni);
        }
        if (PIni(L"auto_rev", 0) < 351) {   /* 3.5.1: the controller out of focus starts off again (3.5 had it on, Parsec mode set it for good) */
            WritePrivateProfileStringW(L"PORTHEON", L"pad_background", NULL, g_pIni);
            WritePrivateProfileStringW(L"PORTHEON", L"auto_rev", L"351", g_pIni);
        }
        if (PIni(L"auto_rev", 0) < 353) {   /* 3.5.3: motion blur off for everyone (UFC 3 and UFC 4; it stays a choice in Extras) */
            if (PortOk(2) && PatchState(2, L"blur", -1) >= 0) PatchState(2, L"blur", 1);
            PIniSet(L"ufc4_blur_off", 1);
            WritePrivateProfileStringW(L"PORTHEON", L"auto_rev", L"353", g_pIni);
        }
        if (PIni(L"auto_rev", 0) < 404) {   /* 4.0.4: the camera motion blur goes too, and UFC 1 and 2 have the switch: off once for everyone */
            for (int g = 0; g < 3; g++) if (PortOk(g) && PatchState(g, L"blur", -1) >= 0) PatchState(g, L"blur", 1);
            PIniSet(L"ufc4_blur_off", 1);
            WritePrivateProfileStringW(L"PORTHEON", L"auto_rev", L"404", g_pIni);
        }
        if (PIni(L"auto_rev", 0) < 410) {   /* 4.1: Immediate (no VSync) once for everyone, Parsec mode's Mailbox kept */
            for (int g = 0; g < 4; g++) {
                if (!PortOk(g)) continue;
                if (!GetInt(g, L"parsec", 0)) PutInt(g, L"present_mode", 2);
                else { wchar_t pk[48]; _snwprintf(pk, 48, L"parsec_%s_present_mode", G[g].id); WritePrivateProfileStringW(L"PORTHEON", pk, L"2", g_pIni); }   /* what Parsec mode's off brings back */
            }
            WritePrivateProfileStringW(L"PORTHEON", L"auto_rev", L"410", g_pIni);
        }
        for (int g = 0; g < 4; g++) SanitizePs4(g);
        /* 4.0.2; 4.3: UFC 6 and UFC 5. 4.3.2: not in an instance that starts a game right away (the window's PLAY or a
           shortcut): UFC5.exe/UFC6.exe --auto-config would run next to the game's start and write the ini it reads;
           the interface does it when it opens */
        if (direct < 0) for (int g = 4; g < NGAMES; g++) if (G[g].ps5 && PortOk(g)) PsAutoCfg(g, FALSE);
    }
    if (direct < 0) CheckPortFiles();   /* 3.5.3: a missing or blocked port file says so right away. 6.0 final: only in the window (the started port is checked at its PLAY) */
    PostMessageW(w, WM_APP_AUTOCFG, 0, 0);
    UpdateResult();
    InvalidateRect(w, NULL, FALSE);   /* the PC was read after the first paint: draw it all again */
    if (PIni(L"update_check", 1) && direct < 0) StartUpdateCheck(FALSE);   /* 4.3.2: not next to a game's start */
    StartFixesFetch();   /* 4.3.8: the remote fix list, for the next PLAY */
#ifdef PORTHEON_TEST   /* PORTHEON_TEST_NOSTART=1 with --play: the loading picture only, no game started (UI tests) */
    if (direct >= 0 && GetEnvironmentVariableW(L"PORTHEON_TEST_NOSTART", NULL, 0)) { SetTimer(w, 13, 50, NULL); direct = -2; }
#endif
    /* 5.2: a desktop shortcut's start tells the last session's end first (LateExited): with an end to tell, or that game
       still open, the game's page stays with it instead of starting a game (PLAY starts it, with what the end changed) */
    if (direct >= 0) {
        /* 5.2: the page of the game LateExited told (not the shortcut's, which may be another game), and its message
           kept: Select empties g_msg when the game changes */
        int lg = -1, late = LateExited(&lg); RunningLeft(late != 0);
        if (late) {
            static wchar_t keep[1024]; COLORREF kc = g_msgColor; DWORD ka = g_msgAt; wcscpy(keep, g_msg);
            Select(lg >= 0 ? lg : direct); g_view = V_GAME; g_tab = TAB_OVERVIEW; LoadEdits();
            wcscpy(g_msg, keep); g_msgColor = kc; g_msgAt = ka; InvalidateRect(w, NULL, FALSE);
        }
        else { if (g_view != V_LAUNCH) g_view = V_HOME; if (arenaStart) ArenaHostPrep(direct); DirectPlay(direct); }   /* 3.5.2: a desktop shortcut */
    }
    else { NewsOnce(); FirstRunGuide(); RunningLeft(LateExited(NULL) != 0); LearnPendingAll(); if (!PIni(L"scan_62", 0)) { PIniSet(L"scan_62", 1); StartScan(FALSE); } }   /* 4.3.6: a game PORTHEON closed for (CloseOnPlay) */
    PadStart();   /* 4.0: the controllers drive PORTHEON */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (g_single) CloseHandle(g_single);
    return 0;
}
