/* lang.c - part of portheon.c (4.0): PORTHEON in 16 languages.
 * The code keeps writing each text twice, in Brazilian Portuguese and in English (L2(pt, en)); every other language
 * is a table keyed by the English text (lang_data.inc: one row per text, the translations in LANGS order from Spanish
 * on). A text without a row, or with an empty cell, is shown in English. The language is chosen in the system
 * settings; on the first start it follows Windows' own display language. The fonts follow it too (MakeFonts): the
 * Japanese, Korean and Chinese texts need fonts that have their characters. */
enum { LANG_PT, LANG_EN, LANG_ES, LANG_FR, LANG_DE, LANG_IT, LANG_NL, LANG_PL, LANG_RU, LANG_UK, LANG_TR, LANG_ID, LANG_JA, LANG_KO, LANG_ZHS, LANG_ZHT, NLANG };
typedef struct { const wchar_t *code, *name; int ps4; } LangInfo;
/* name: the language in itself; ps4: the PS4 system language a game is started with when the player picks this one
   (the game's own text and voices, where the game has that language: otherwise the game shows English) */
static const LangInfo LANGS[NLANG] = {
    {L"pt-BR", L"Português (Brasil)", 17}, {L"en", L"English", 1}, {L"es", L"Español", 20}, {L"fr", L"Français", 2},
    {L"de", L"Deutsch", 4}, {L"it", L"Italiano", 5}, {L"nl", L"Nederlands", 6}, {L"pl", L"Polski", 16},
    {L"ru", L"Русский", 8}, {L"uk", L"Українська", 8}, {L"tr", L"Türkçe", 19}, {L"id", L"Bahasa Indonesia", 29},
    {L"ja", L"日本語", 0}, {L"ko", L"한국어", 9}, {L"zh-Hans", L"简体中文", 11}, {L"zh-Hant", L"繁體中文", 10}};
/* one row: the English text, then es fr de it nl pl ru uk tr id ja ko zh-Hans zh-Hant (NULL: not translated) */
typedef struct { const wchar_t *en, *tr[NLANG - 2]; } TrRow;
static const TrRow TROWS[] = {
#include "lang_data.inc"
};
#define NTROWS (int)(sizeof(TROWS) / sizeof(TROWS[0]))
static int g_trHash[8192];   /* open addressing: row index + 1, 0 = empty */
static uint32_t TrKey(const wchar_t *s) { uint32_t h = 2166136261u; while (*s) { h ^= (uint32_t)*s++; h *= 16777619u; } return h; }
static void TrInit(void)
{
    static BOOL done; if (done) return; done = TRUE;
    for (int i = 0; i < NTROWS; i++) {
        uint32_t k = TrKey(TROWS[i].en) & 8191;
        while (g_trHash[k]) k = (k + 1) & 8191;
        g_trHash[k] = i + 1;
    }
}
static const wchar_t *TR(const wchar_t *pt, const wchar_t *en)
{
    if (g_lang == LANG_PT) return pt;
    if (g_lang == LANG_EN || g_lang < 0 || g_lang >= NLANG) return en;
    TrInit();
    for (uint32_t k = TrKey(en) & 8191; g_trHash[k]; k = (k + 1) & 8191) {
        const TrRow *r = &TROWS[g_trHash[k] - 1];
        if (!wcscmp(r->en, en)) { const wchar_t *t = r->tr[g_lang - 2]; return t && t[0] ? t : en; }
    }
    return en;
}
static BOOL LangCjk(void) { return g_lang >= LANG_JA; }
/* the faces for this language: headings, body text. 7.0 (the console's look): the Latin headings in Segoe UI Semibold
   (HEAD_FACE; 6.2 was L"Bahnschrift" at FW_BOLD: put that name back here and FW_BOLD in MakeFonts' hw to undo) */
#define HEAD_FACE L"Segoe UI"
static const wchar_t *FaceHead(void) { return g_lang == LANG_JA ? L"Yu Gothic UI" : g_lang == LANG_KO ? L"Malgun Gothic" : g_lang == LANG_ZHS ? L"Microsoft YaHei UI" : g_lang == LANG_ZHT ? L"Microsoft JhengHei UI" : HEAD_FACE; }
static const wchar_t *FaceBody(void) { return g_lang == LANG_JA ? L"Yu Gothic UI" : g_lang == LANG_KO ? L"Malgun Gothic" : g_lang == LANG_ZHS ? L"Microsoft YaHei UI" : g_lang == LANG_ZHT ? L"Microsoft JhengHei UI" : L"Segoe UI"; }
/* portheon.ini: ui_lang=<code>; before 4.0 language=1 meant PT-BR and 0 English; with neither, Windows' language */
static int LangFromWindows(void)
{
    LANGID l = GetUserDefaultUILanguage(); int p = PRIMARYLANGID(l), s = SUBLANGID(l);
    switch (p) {
    case LANG_PORTUGUESE: return LANG_PT; case LANG_SPANISH: return LANG_ES; case LANG_FRENCH: return LANG_FR; case LANG_GERMAN: return LANG_DE;
    case LANG_ITALIAN: return LANG_IT; case LANG_DUTCH: return LANG_NL; case LANG_POLISH: return LANG_PL; case LANG_RUSSIAN: return LANG_RU;
    case LANG_UKRAINIAN: return LANG_UK; case LANG_TURKISH: return LANG_TR; case LANG_INDONESIAN: return LANG_ID; case LANG_JAPANESE: return LANG_JA;
    case LANG_KOREAN: return LANG_KO; case LANG_CHINESE: return (s == SUBLANG_CHINESE_TRADITIONAL || s == SUBLANG_CHINESE_HONGKONG || s == SUBLANG_CHINESE_MACAU) ? LANG_ZHT : LANG_ZHS;
    default: return LANG_EN;
    }
}
static int LangLoad(const wchar_t *ini)
{
    wchar_t c[16]; GetPrivateProfileStringW(L"PORTHEON", L"ui_lang", L"", c, 16, ini);
    for (int i = 0; i < NLANG; i++) if (c[0] && !_wcsicmp(c, LANGS[i].code)) return i;
    int old = GetPrivateProfileIntW(L"PORTHEON", L"language", -1, ini);
    if (old == 1) return LANG_PT;
    if (old == 0) return LANG_EN;
    return LangFromWindows();
}
static void LangSave(int l)
{
    WritePrivateProfileStringW(L"PORTHEON", L"ui_lang", LANGS[l].code, g_pIni);
    WritePrivateProfileStringW(L"PORTHEON", L"language", l == LANG_PT ? L"1" : L"0", g_pIni);   /* what older versions read */
}
