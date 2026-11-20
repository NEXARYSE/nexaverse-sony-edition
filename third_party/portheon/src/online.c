/* online.c - part of portheon.c (included after ui.c): playing with friends (3.5).
 *  - UFC 3: the port's own online (its private server, tools\ufc3_server.py, which UFC3.exe hosts during the session):
 *    Host = this PC creates the room (the friend joins its address; UFC3.exe asks the router to open the ports by UPnP and
 *    writes the Internet address to UFC3Native\online-internet.txt), Join = the friend's address. Both enter
 *    Online > Quick Fight. Outside the same network: the same VPN on both PCs (Tailscale, Radmin VPN, ZeroTier).
 *  - UFC 1, 2 and 4: no server of their own exists; online with a friend is Parsec (the friend sees this PC's game and
 *    plays as Player 2), with PORTHEON's low-latency Parsec mode.
 *  - 4.3.6, UFC 2: the community package's own UFC 2 server too, hosted by PORTHEON (portheon.c, Ufc2OnlineEnv): the
 *    same Host / Join as UFC 3, on the v3.0 engine, port 42230 (TCP, and UDP for the ping). 4.3.7: PORTHEON asks the
 *    router to open that port by UPnP while it hosts (Ufc2UpnpOpen), as UFC3.exe does for UFC 3.
 * 4.0: shown in the online center (ui_screens.c, PaintOnline) and in each game's hub. */
typedef struct { wchar_t ip[16], name[80]; int kind; } HostAddr;   /* kind: 0 VPN, 1 home network, 2 other */
static HostAddr g_addr[12]; static int g_naddr;
static void ScanAddrs(void)
{
    g_naddr = 0;
    ULONG size = 32768; IP_ADAPTER_ADDRESSES *list = NULL;
    for (int tries = 0; tries < 3; tries++) {
        list = (IP_ADAPTER_ADDRESSES *)malloc(size); if (!list) return;
        ULONG r = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, list, &size);
        if (r == NO_ERROR) break;
        free(list); list = NULL; if (r != ERROR_BUFFER_OVERFLOW) return;
    }
    if (!list) return;
    HostAddr tmp[12]; int n = 0;
    for (IP_ADAPTER_ADDRESSES *a = list; a && n < 12; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (IP_ADAPTER_UNICAST_ADDRESS *u = a->FirstUnicastAddress; u && n < 12; u = u->Next) {
            struct sockaddr_in *sin = (struct sockaddr_in *)u->Address.lpSockaddr; if (!sin || sin->sin_family != AF_INET) continue;
            BYTE *ip = (BYTE *)&sin->sin_addr;
            if (ip[0] == 127 || (ip[0] == 169 && ip[1] == 254)) continue;
            HostAddr *h = &tmp[n++]; _snwprintf(h->ip, 16, L"%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
            const wchar_t *fn = a->FriendlyName ? a->FriendlyName : L"";
            if (ip[0] == 100 && ip[1] >= 64 && ip[1] < 128) { h->kind = 0; wcscpy(h->name, L"Tailscale"); }
            else if (ip[0] == 26) { h->kind = 0; wcscpy(h->name, L"Radmin VPN"); }
            else if (ip[0] == 25) { h->kind = 0; wcscpy(h->name, L"Hamachi"); }
            else if (StrStrIW(fn, L"ZeroTier") || StrStrIW(fn, L"Tailscale") || StrStrIW(fn, L"Radmin") || StrStrIW(fn, L"WireGuard")) { h->kind = 0; wcsncpy(h->name, fn, 79); h->name[79] = 0; }
            else if (ip[0] == 10 || (ip[0] == 192 && ip[1] == 168) || (ip[0] == 172 && ip[1] >= 16 && ip[1] < 32)) { h->kind = 1; _snwprintf(h->name, 80, L"%s (%s)", L2(L"rede de casa", L"home network"), fn); }
            else { h->kind = 2; wcsncpy(h->name, fn, 79); h->name[79] = 0; }
        }
    }
    free(list);
    for (int k = 0; k <= 2; k++) for (int i = 0; i < n; i++) if (tmp[i].kind == k) g_addr[g_naddr++] = tmp[i];
}
static void PublicAddr(wchar_t *out, int n)   /* what UFC3.exe's host server found (UPnP), or "" */
{
    wchar_t p[MAX_PATH], d[MAX_PATH]; LocalData(d, G[2].data); Join(p, d, L"online-internet.txt"); out[0] = 0;
    HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL); if (h == INVALID_HANDLE_VALUE) return;
    char b[512] = ""; DWORD got = 0; ReadFile(h, b, sizeof(b) - 1, &got, NULL); CloseHandle(h); b[got] = 0;
    char *e = strpbrk(b, "\r\n"); if (e) *e = 0;
    MultiByteToWideChar(CP_UTF8, 0, b, -1, out, n); out[n - 1] = 0;
}
/* 4.3.7, UFC 2 online without touching the router (the user): while this PC hosts, Windows' UPnP NAT API asks the
   router to forward port 42230 (TCP and UDP) to this PC, and the Internet address it reports goes to
   UFC2Native\online\online-internet.txt for the Online tab (PublicAddrG). A router without UPnP (or with it off) leaves
   the VPN / port-forward way. The mapping is removed when the room ends. The three interfaces are declared here (the
   toolchain has no natupnp.h): IUPnPNAT, IStaticPortMappingCollection and IStaticPortMapping, IDispatch-based. */
typedef struct UpMap UpMap; typedef struct UpColl UpColl; typedef struct UpNat UpNat;
typedef struct { HRESULT (STDMETHODCALLTYPE *QueryInterface)(UpMap *, REFIID, void **); ULONG (STDMETHODCALLTYPE *AddRef)(UpMap *); ULONG (STDMETHODCALLTYPE *Release)(UpMap *);
    void *GetTypeInfoCount, *GetTypeInfo, *GetIDsOfNames, *Invoke;
    HRESULT (STDMETHODCALLTYPE *get_ExternalIPAddress)(UpMap *, BSTR *); } UpMapVtbl;
struct UpMap { const UpMapVtbl *v; };
typedef struct { HRESULT (STDMETHODCALLTYPE *QueryInterface)(UpColl *, REFIID, void **); ULONG (STDMETHODCALLTYPE *AddRef)(UpColl *); ULONG (STDMETHODCALLTYPE *Release)(UpColl *);
    void *GetTypeInfoCount, *GetTypeInfo, *GetIDsOfNames, *Invoke, *get__NewEnum;
    HRESULT (STDMETHODCALLTYPE *get_Item)(UpColl *, long, BSTR, UpMap **); HRESULT (STDMETHODCALLTYPE *get_Count)(UpColl *, long *);
    HRESULT (STDMETHODCALLTYPE *Remove)(UpColl *, long, BSTR);
    HRESULT (STDMETHODCALLTYPE *Add)(UpColl *, long, BSTR, long, BSTR, VARIANT_BOOL, BSTR, UpMap **); } UpCollVtbl;
struct UpColl { const UpCollVtbl *v; };
typedef struct { HRESULT (STDMETHODCALLTYPE *QueryInterface)(UpNat *, REFIID, void **); ULONG (STDMETHODCALLTYPE *AddRef)(UpNat *); ULONG (STDMETHODCALLTYPE *Release)(UpNat *);
    void *GetTypeInfoCount, *GetTypeInfo, *GetIDsOfNames, *Invoke;
    HRESULT (STDMETHODCALLTYPE *get_StaticPortMappingCollection)(UpNat *, UpColl **); } UpNatVtbl;
struct UpNat { const UpNatVtbl *v; };
static const GUID CLSID_UpnpNat_ = {0xAE1E00AA, 0x3FD5, 0x403C, {0x8A, 0x27, 0x2B, 0xBD, 0xC3, 0x0C, 0xD0, 0xE1}};
static const GUID IID_UpnpNat_ = {0xB171C812, 0xCC76, 0x485A, {0x94, 0xD8, 0xB6, 0xB3, 0xA2, 0x79, 0x4E, 0x99}};
static wchar_t g_u2lan[32];   /* the address the mapping points at (this PC's, HostAddress) */
static void Ufc2InternetFile(int g, wchar_t *out) { wchar_t d[MAX_PATH]; LocalData(d, G[g].data); Join(d, d, L"online"); Join(out, d, L"online-internet.txt"); }
static UpColl *UpnpCollection(UpNat **nat)
{
    UpColl *c = NULL; *nat = NULL;
    if (FAILED(CoCreateInstance(&CLSID_UpnpNat_, NULL, CLSCTX_INPROC_SERVER, &IID_UpnpNat_, (void **)nat)) || !*nat) return NULL;
    if (FAILED((*nat)->v->get_StaticPortMappingCollection(*nat, &c))) c = NULL;   /* NULL as well: no UPnP router answered */
    return c;
}
static DWORD WINAPI Ufc2UpnpThread(LPVOID open)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    UpNat *nat; UpColl *c = UpnpCollection(&nat); wchar_t ext[64] = L"";
    if (c) {
        BSTR tcp = SysAllocString(L"TCP"), udp = SysAllocString(L"UDP");
        c->v->Remove(c, UFC2_PORT, tcp); c->v->Remove(c, UFC2_PORT, udp);   /* a mapping left by an earlier room */
        if (open) {
            BSTR lan = SysAllocString(g_u2lan), desc = SysAllocString(L"PORTHEON online"); UpMap *m = NULL;
            if (SUCCEEDED(c->v->Add(c, UFC2_PORT, tcp, UFC2_PORT, lan, VARIANT_TRUE, desc, &m)) && m) {
                BSTR ip = NULL; if (SUCCEEDED(m->v->get_ExternalIPAddress(m, &ip)) && ip) { wcsncpy(ext, ip, 63); ext[63] = 0; SysFreeString(ip); }
                m->v->Release(m);
            }
            m = NULL; if (SUCCEEDED(c->v->Add(c, UFC2_PORT, udp, UFC2_PORT, lan, VARIANT_TRUE, desc, &m)) && m) m->v->Release(m);
            SysFreeString(lan); SysFreeString(desc);
        }
        SysFreeString(tcp); SysFreeString(udp); c->v->Release(c);
    }
    if (nat) nat->v->Release(nat);
    if (open) {   /* "" when no router opened it: the Online tab says so */
        wchar_t f[MAX_PATH]; Ufc2InternetFile(g_u2game, f);
        HANDLE h = CreateFileW(f, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) { char b[80]; int n = WideCharToMultiByte(CP_UTF8, 0, ext, -1, b, 80, NULL, NULL); DWORD w; if (n > 1) WriteFile(h, b, n - 1, &w, NULL); CloseHandle(h); }
    }
    CoUninitialize();
    return 0;
}
static void HostAddress(wchar_t *out, int n);
static void Ufc2UpnpOpen(void)
{
    HostAddress(g_u2lan, 32); if (!g_u2lan[0]) return;
    HANDLE t = CreateThread(NULL, 0, Ufc2UpnpThread, (LPVOID)1, 0, NULL); if (t) CloseHandle(t);
}
static void Ufc2UpnpClose(void)
{
    HANDLE t = CreateThread(NULL, 0, Ufc2UpnpThread, NULL, 0, NULL);
    if (t) { WaitForSingleObject(t, 4000); CloseHandle(t); }   /* PORTHEON may be closing: a few seconds at most */
}
static void PublicAddrG(int g, wchar_t *out, int n)   /* the Internet address the host's router reported (Ufc2UpnpThread, UFC 2 and 3) */
{
    if (g != 1 && g != 2) { PublicAddr(out, n); return; }
    wchar_t p[MAX_PATH]; Ufc2InternetFile(g, p); out[0] = 0;
    HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL); if (h == INVALID_HANDLE_VALUE) return;
    char b[128] = ""; DWORD got = 0; ReadFile(h, b, sizeof(b) - 1, &got, NULL); CloseHandle(h); b[got] = 0;
    MultiByteToWideChar(CP_UTF8, 0, b, -1, out, n); out[n - 1] = 0;
}
static void HostAddress(wchar_t *out, int n)
{
    GetStr(2, L"host_address", out, n, L"");
    if (!g_naddr) ScanAddrs();
    BOOL here = FALSE; for (int i = 0; i < g_naddr; i++) if (!wcscmp(out, g_addr[i].ip)) here = TRUE;
    if ((!out[0] || !here) && g_naddr) { wcsncpy(out, g_addr[0].ip, n); out[n - 1] = 0; PutStr(2, L"host_address", out); }
}
static void OnlineChanged(int g, int mode)
{
    if (g == 1 || g == 2) {   /* 4.3.6: UFC 2; 6.2: UFC 3, the same way */
        if (mode == 2) {
            ScanAddrs(); wchar_t a[32]; HostAddress(a, 32);
            Msg(C_SUB, L2(L"Hospedar: o seu amigo escolhe \"Entrar\" e digita %s. Os dois entram no Online do jogo. Fora da mesma rede: a mesma VPN nos dois PCs (Radmin/ZeroTier/Tailscale) ou a porta 42230 aberta no roteador.",
                          L"Host: your friend picks \"Join\" and types %s. Both go to the game's Online. Outside the same network: the same VPN on both PCs (Radmin/ZeroTier/Tailscale) or port 42230 open on the router."), a[0] ? a : L"?");
        }
        else if (mode == 1) { Msg(C_SUB, L"%s", L2(L"Entrar: digite o endereço que o seu amigo (o anfitrião) passou e aperte JOGAR.", L"Join: type the address your friend (the host) gave you and press PLAY.")); SetFocus(g_editAddr); }
        else Msg(C_SUB, L2(L"Offline: o %s usa o motor escolhido em Extras, sem servidor.", L"Offline: %s uses the engine chosen in Extras, with no server."), G[g].name);
        return;
    }
}
static void HostIpMenu(void)
{
    ScanAddrs();
    HMENU m = CreatePopupMenu(); wchar_t cur[32]; GetStr(2, L"host_address", cur, 32, L"");
    for (int i = 0; i < g_naddr; i++) { wchar_t t[128]; _snwprintf(t, 128, L"%s   (%s)", g_addr[i].ip, g_addr[i].name); AppendMenuW(m, MF_STRING | (!wcscmp(cur, g_addr[i].ip) ? MF_CHECKED : 0), 1 + i, t); }
    if (!g_naddr) AppendMenuW(m, MF_STRING | MF_GRAYED, 99, L2(L"Nenhuma rede conectada", L"No network connected"));
    int c = PopupAt(m, H_HOSTIP);
    if (c >= 1 && c <= g_naddr) PutStr(2, L"host_address", g_addr[c - 1].ip);
    InvalidateRect(g_wnd, NULL, FALSE);
}
static void CopyHostIp(void)
{
    wchar_t a[32], pub[200], t[300], inv[520]; HostAddress(a, 32); PublicAddrG(g_sel, pub, 200);
    BOOL usePub = pub[0] && iswdigit(pub[0]);
    _snwprintf(t, 300, L"%s", usePub ? pub : a);
    /* 6.2: a ready invitation, to paste in Discord, Steam or any chat */
    _snwprintf(inv, 520, L2(L"Bora lutar no %s pelo PORTHEON! Abra %s > Online > Entrar e use este endereço: %s", L"Let's fight in %s on PORTHEON! Open %s > Online > Join and use this address: %s"), G[g_sel].name, G[g_sel].name, t); inv[519] = 0;
    if (OpenClipboard(g_wnd)) {
        EmptyClipboard(); size_t bytes = (wcslen(inv) + 1) * sizeof(wchar_t); HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hg) { memcpy(GlobalLock(hg), inv, bytes); GlobalUnlock(hg); SetClipboardData(CF_UNICODETEXT, hg); }
        CloseClipboard();
    }
    Msg(C_GOOD, L2(L"Convite copiado com o endereço %s: cole no Discord, na Steam ou em qualquer conversa.", L"Invitation copied with the address %s: paste it in Discord, Steam or any chat."), t);
}

/* the address this PC hosts on, with the ones it can choose from and a copy button */
static void HostIpWidget(HDC dc, GpGraphics *g, int x, int y, int w)
{
    wchar_t a[32]; HostAddress(a, 32);
    const wchar_t *kind = L""; for (int i = 0; i < g_naddr; i++) if (!wcscmp(a, g_addr[i].ip)) kind = g_addr[i].name;
    Text(dc, g_fSmall, C_SUB, x + SC(2), y + SC(1), L2(L"Endereço deste PC (o amigo digita este)", L"This PC's address (your friend types it)"));
    int cw = SC(110); RECT r = {x, y + SC(20), x + w - cw - SC(10), y + SC(58)}; BOOL hot = g_hot == H_HOSTIP;
    Box(g, r, SC(10), A(hot ? RGB(30, 22, 48) : C_FIELD, 255), A(hot ? C_CYAN : C_FIELDB, 255), 1.0f);
    wchar_t t[160]; _snwprintf(t, 160, L"%s   %s", a[0] ? a : L"?", kind);
    RECT tr = {r.left + SC(16), r.top, r.right - SC(40), r.bottom}; TextR(dc, g_fText, C_TEXT, tr, t, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    Chevron(g, r.right - SC(22), (r.top + r.bottom) / 2, SC(6), 0, A(hot ? C_CYAN : C_SUB, 255));
    AddHit(r.left, r.top, r.right, r.bottom, H_HOSTIP);
    RECT cb = {r.right + SC(10), r.top, x + w, r.bottom}; Button(dc, g, cb, L2(L"Copiar", L"Copy"), H_COPYIP, 0);
}
/* Player 1's controller (UFC 1-4): the port's "gamepad" setting, the GUID of one controller (empty: the first one
   connected). With Parsec the host picks their own pad here, so the friend's virtual pad is Player 2. */
static void PadPickWidget(HDC dc, GpGraphics *g, int x, int y, int w)
{
    wchar_t guid[64], name[128], k[48], t[200]; GetStr(g_sel, L"gamepad", guid, 64, L"");
    _snwprintf(k, 48, L"padname_%s", G[g_sel].id); GetPrivateProfileStringW(L"PORTHEON", k, L"", name, 128, g_pIni);
    if (!guid[0]) _snwprintf(t, 200, L"%s", L2(L"Automático (o primeiro controle conectado)", L"Automatic (the first controller connected)"));
    else _snwprintf(t, 200, L"%s", name[0] ? name : guid);
    Text(dc, g_fSmall, C_SUB, x + SC(2), y + SC(1), T(S_PAD1));
    RECT r = {x, y + SC(20), x + w, y + SC(58)}; BOOL hot = g_hot == H_PADPICK;
    Box(g, r, SC(10), A(hot ? RGB(30, 22, 48) : C_FIELD, 255), A(hot ? C_CYAN : C_FIELDB, 255), 1.0f);
    RECT tr = {r.left + SC(16), r.top, r.right - SC(40), r.bottom}; TextR(dc, g_fText, C_TEXT, tr, t, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    Chevron(g, r.right - SC(22), (r.top + r.bottom) / 2, SC(6), 0, A(hot ? C_CYAN : C_SUB, 255));
    AddHit(r.left, r.top, r.right, r.bottom, H_PADPICK);
}
static BOOL BbportSetPort2(int g, const char *val); static void Pad2Keys(int g, wchar_t *k, wchar_t *kn);   /* Released: below */
static void PadPickMenu(void)
{
    SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_WAIT));
    PadInfo pads[16]; int n = ListPads(g_sel, pads, 16);
    HMENU m = CreatePopupMenu(); wchar_t cur[64]; GetStr(g_sel, L"gamepad", cur, 64, L"");
    AppendMenuW(m, MF_STRING | (!cur[0] ? MF_CHECKED : 0), 1, L2(L"Automático (o primeiro controle conectado)", L"Automatic (the first controller connected)"));
    if (n > 0) AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    for (int i = 0; i < n; i++) {
        wchar_t t[180]; size_t gl = wcslen(pads[i].guid);
        _snwprintf(t, 180, L"%s   (%s)", pads[i].name, gl > 8 ? pads[i].guid + gl - 8 : pads[i].guid);
        AppendMenuW(m, MF_STRING | (!wcscmp(cur, pads[i].guid) ? MF_CHECKED : 0), 2 + i, t);
    }
    if (n == 0) AppendMenuW(m, MF_STRING | MF_GRAYED, 99, L2(L"Nenhum controle conectado agora", L"No controller connected now"));
    if (n < 0) AppendMenuW(m, MF_STRING | MF_GRAYED, 99, L2(L"A ferramenta de controles do port não está na pasta bin", L"The port's controller tool is not in its bin folder"));
    int c = PopupAt(m, H_PADPICK);
    wchar_t k[48]; _snwprintf(k, 48, L"padname_%s", G[g_sel].id);
    if (c == 1) { PutStr(g_sel, L"gamepad", L""); WritePrivateProfileStringW(L"PORTHEON", k, NULL, g_pIni); Msg(C_SUB, L"%s", L2(L"Jogador 1: o primeiro controle conectado.", L"Player 1: the first controller connected.")); }
    else if (c >= 2 && c < 2 + n) {
        PutStr(g_sel, L"gamepad", pads[c - 2].guid); WritePrivateProfileStringW(L"PORTHEON", k, pads[c - 2].name, g_pIni);
        Msg(C_GOOD, L2(L"Jogador 1: %s. Os outros controles (inclusive o do amigo no Parsec) viram Jogador 2, 3 e 4.", L"Player 1: %s. The other controllers (the Parsec friend's too) become Players 2, 3 and 4."), pads[c - 2].name);
        if (g_sel >= 0 && g_sel <= 2) {   /* Released: the same controller was Player 2's choice: that one goes back to Automatic */
            wchar_t k2[48], kn2[48], v2[64]; Pad2Keys(g_sel, k2, kn2); GetPrivateProfileStringW(L"PORTHEON", k2, L"", v2, 64, g_pIni);
            if (v2[0] && !_wcsicmp(v2, pads[c - 2].guid)) {
                WritePrivateProfileStringW(L"PORTHEON", k2, NULL, g_pIni); WritePrivateProfileStringW(L"PORTHEON", kn2, NULL, g_pIni); BbportSetPort2(g_sel, NULL);
                Msg(C_GOOD, L2(L"Jogador 1: %s. Ele era o controle escolhido para o Jogador 2: o Jogador 2 voltou para Automático.", L"Player 1: %s. It was the controller chosen for Player 2: Player 2 is back on Automatic."), pads[c - 2].name);
            }
        }
    }
    InvalidateRect(g_wnd, NULL, FALSE);
}
/* Released (2026-11-13): Player 2's controller for UFC 1-3, as UFC4 Native Plus's launcher sets it
   (scripts\setup_controllers.py, GPL-2.0, Abdullah Tech): the line port.2=<the controller's GUID> in the engine's
   bbport.ini (every SacredFire engine here logs "Runtime: controller port %d -> %s"). The choice is kept in
   portheon.ini (pad2_<id>, pad2name_<id>) and written again at each PLAY; Automatic takes the line out. Only a
   controller whose GUID differs from Player 1's can be bound: two pads of the same model share one GUID. */
static BOOL BbportSetPort2(int g, const char *val)   /* NULL: no port.2 line */
{
    wchar_t p[MAX_PATH]; if (!BbportPath(g, p)) return FALSE;
    DWORD n; char *b = ReadText(p, &n);
    if (!b && !val) return TRUE;
    wchar_t d[MAX_PATH]; wcscpy(d, p); *wcsrchr(d, L'\\') = 0; CreateDirectoryW(d, NULL);
    size_t cap = (size_t)n + 512; char *o = (char *)malloc(cap); if (!o) { free(b); return FALSE; }
    size_t k = 0; BOOL had = FALSE, same = FALSE;
    if (b) for (char *l = b; l && *l;) {
        char *e = strchr(l, '\n'); size_t len = e ? (size_t)(e - l + 1) : strlen(l);
        const char *s = l; while (*s == ' ' || *s == '\t') s++;
        if (!strncmp(s, "port.2=", 7)) {
            size_t vl = strcspn(s + 7, "\r\n"); had = TRUE;
            if (val && strlen(val) == vl && !memcmp(s + 7, val, vl)) same = TRUE;
        } else { memcpy(o + k, l, len); k += len; if (!e) { o[k++] = '\r'; o[k++] = '\n'; } }
        l = e ? e + 1 : NULL;
    }
    if ((val && same) || (!val && !had)) { free(o); free(b); return TRUE; }   /* already as wanted */
    if (val) k += (size_t)snprintf(o + k, cap - k, "port.2=%s\r\n", val);
    HANDLE f = CreateFileW(p, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    BOOL ok = f != INVALID_HANDLE_VALUE; if (ok) { DWORD w; ok = WriteFile(f, o, (DWORD)k, &w, NULL) && w == k; CloseHandle(f); }
    free(o); free(b); return ok;
}
static void Pad2Keys(int g, wchar_t *k, wchar_t *kn) { _snwprintf(k, 48, L"pad2_%s", G[g].id); k[47] = 0; _snwprintf(kn, 48, L"pad2name_%s", G[g].id); kn[47] = 0; }
static void Pad2Apply(int g)   /* PLAY: the chosen controller's line is in bbport.ini */
{
    if (g < 0 || g > 2) return;
    wchar_t k[48], kn[48], v[64]; Pad2Keys(g, k, kn); GetPrivateProfileStringW(L"PORTHEON", k, L"", v, 64, g_pIni);
    if (!v[0]) return;
    /* only while it holds, as Native Plus's own script checks before it binds: two controllers or more connected, the
       chosen one among them, and not Player 1's (the "gamepad" setting or the first listed); never in Parsec mode (the
       friend's controller takes the next free port). Otherwise no line for this PLAY; the choice stays saved */
    PadInfo pads[16]; int n = GetInt(g, L"parsec", 0) ? 0 : ListPads(g, pads, 16); BOOL here = FALSE;
    wchar_t p1[64]; GetStr(g, L"gamepad", p1, 64, L"");
    for (int i = 0; i < n; i++) if (!_wcsicmp(v, pads[i].guid)) here = TRUE;
    if (n < 2 || !here || !_wcsicmp(v, p1[0] ? p1 : pads[0].guid)) { BbportSetPort2(g, NULL); return; }
    char a[200]; WideCharToMultiByte(CP_UTF8, 0, v, -1, a, sizeof(a), NULL, NULL); BbportSetPort2(g, a);
}
static void Pad2PickWidget(HDC dc, GpGraphics *g, int x, int y, int w)
{
    wchar_t k[48], kn[48], guid[64], name[128], t[200]; Pad2Keys(g_sel, k, kn);
    GetPrivateProfileStringW(L"PORTHEON", k, L"", guid, 64, g_pIni); GetPrivateProfileStringW(L"PORTHEON", kn, L"", name, 128, g_pIni);
    if (!guid[0]) _snwprintf(t, 200, L"%s", L2(L"Automático (o próximo controle conectado)", L"Automatic (the next controller connected)"));
    else _snwprintf(t, 200, L"%s", name[0] ? name : guid);
    t[199] = 0;
    Text(dc, g_fSmall, C_SUB, x + SC(2), y + SC(1), L2(L"Controle do Jogador 2", L"Player 2 controller"));
    RECT r = {x, y + SC(20), x + w, y + SC(58)}; BOOL hot = g_hot == H_PADPICK2;
    Box(g, r, SC(10), A(hot ? RGB(30, 22, 48) : C_FIELD, 255), A(hot ? C_CYAN : C_FIELDB, 255), 1.0f);
    RECT tr = {r.left + SC(16), r.top, r.right - SC(40), r.bottom}; TextR(dc, g_fText, C_TEXT, tr, t, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    Chevron(g, r.right - SC(22), (r.top + r.bottom) / 2, SC(6), 0, A(hot ? C_CYAN : C_SUB, 255));
    AddHit(r.left, r.top, r.right, r.bottom, H_PADPICK2);
}
static void Pad2PickMenu(void)
{
    if (g_sel < 0 || g_sel > 2) return;
    SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_WAIT));
    PadInfo pads[16]; int n = ListPads(g_sel, pads, 16);
    wchar_t k[48], kn[48], cur[64], p1[64]; Pad2Keys(g_sel, k, kn);
    GetPrivateProfileStringW(L"PORTHEON", k, L"", cur, 64, g_pIni); GetStr(g_sel, L"gamepad", p1, 64, L"");
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (!cur[0] ? MF_CHECKED : 0), 1, L2(L"Automático (o próximo controle conectado)", L"Automatic (the next controller connected)"));
    if (n > 0) AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    for (int i = 0; i < n; i++) {
        wchar_t t[180]; size_t gl = wcslen(pads[i].guid);
        _snwprintf(t, 180, L"%s   (%s)", pads[i].name, gl > 8 ? pads[i].guid + gl - 8 : pads[i].guid); t[179] = 0;
        AppendMenuW(m, MF_STRING | (!_wcsicmp(cur, pads[i].guid) ? MF_CHECKED : 0), 2 + i, t);
    }
    if (n == 0) AppendMenuW(m, MF_STRING | MF_GRAYED, 99, L2(L"Nenhum controle conectado agora", L"No controller connected now"));
    if (n < 0) AppendMenuW(m, MF_STRING | MF_GRAYED, 99, L2(L"A ferramenta de controles do port não está na pasta bin", L"The port's controller tool is not in its bin folder"));
    int c = PopupAt(m, H_PADPICK2);
    if (c == 1) {
        WritePrivateProfileStringW(L"PORTHEON", k, NULL, g_pIni); WritePrivateProfileStringW(L"PORTHEON", kn, NULL, g_pIni);
        if (BbportSetPort2(g_sel, NULL)) Msg(C_SUB, L"%s", L2(L"Jogador 2: o próximo controle conectado depois do Jogador 1.", L"Player 2: the next controller connected after Player 1's."));
        else Msg(C_BAD, L"%s", L2(L"Não consegui gravar o bbport.ini do jogo (pasta %LOCALAPPDATA%). Feche o jogo e tente de novo.", L"Could not write the game's bbport.ini (the %LOCALAPPDATA% folder). Close the game and try again."));
    } else if (c >= 2 && c < 2 + n) {
        /* Player 1's GUID: the chosen one, or the first controller listed */
        const wchar_t *g1 = p1[0] ? p1 : pads[0].guid;
        if (!_wcsicmp(pads[c - 2].guid, g1))
            Msg(C_ORANGE, L"%s", L2(L"Esse controle é o do Jogador 1, ou é do mesmo modelo dele (os dois têm o mesmo código): o motor não consegue separar. Deixe em Automático: o segundo controle conectado vira o Jogador 2.",
                                    L"That controller is Player 1's, or the same model as it (both have the same code): the engine cannot tell them apart. Leave it on Automatic: the second controller connected becomes Player 2."));
        else {
            char a[200]; WideCharToMultiByte(CP_UTF8, 0, pads[c - 2].guid, -1, a, sizeof(a), NULL, NULL);
            WritePrivateProfileStringW(L"PORTHEON", k, pads[c - 2].guid, g_pIni); WritePrivateProfileStringW(L"PORTHEON", kn, pads[c - 2].name, g_pIni);
            if (BbportSetPort2(g_sel, a)) Msg(C_GOOD, L2(L"Jogador 2: %s. Vale no próximo JOGAR.", L"Player 2: %s. It applies at the next PLAY."), pads[c - 2].name);
            else Msg(C_BAD, L"%s", L2(L"Não consegui gravar o bbport.ini do jogo (pasta %LOCALAPPDATA%). Feche o jogo e tente de novo.", L"Could not write the game's bbport.ini (the %LOCALAPPDATA% folder). Close the game and try again."));
        }
    }
    InvalidateRect(g_wnd, NULL, FALSE);
}
/* 7.2 (2026-11-16, the owner: "put it inside PORTHEON for Portheon Update 7.2"): PORTHEON ARENA, the online fights app
   (public queue, rooms and invites over a public message server, the fight through Parsec), ships in arena\ next to
   PORTHEON.exe. Its own source and notes: Desktop\ufc 6\portheon-arena (README.md). It runs on its own (WebView2), keeps
   its data in %LOCALAPPDATA%\PORTHEON ARENA and gets the game as "--game ufcN"; its host button runs PORTHEON.exe --play.
   Not UFC 5: its PS5 engine has a single player, so there is no versus through Parsec. */
/* ARENA_GAME(g): portheon.c */
static void ArenaOpen(int g)
{
    wchar_t exe[MAX_PATH], dir[MAX_PATH], args[32] = L"";
    /* 7.3: on Linux (Wine/Proton) the ARENA's window needs Microsoft's WebView2 and hosting needs Parsec for Windows,
       neither of which is there: said plainly instead of a window that never opens */
    if (UnderWine()) { Msg(C_ORANGE, L"%s", L2(L"Linux (Wine/Proton): o PORTHEON ARENA ainda não abre aqui. Ele usa o WebView2 da Microsoft e, para hospedar, o Parsec de Windows, que o Wine/Proton não tem. No Windows ele funciona normalmente.", L"Linux (Wine/Proton): PORTHEON ARENA does not open here yet. It uses Microsoft's WebView2 and, to host, Parsec for Windows, which Wine/Proton lack. On Windows it works normally.")); return; }
    _snwprintf(dir, MAX_PATH, L"%s\\arena", g_exeDir); dir[MAX_PATH - 1] = 0;
    _snwprintf(exe, MAX_PATH, L"%s\\PORTHEON ARENA.exe", dir); exe[MAX_PATH - 1] = 0;
    if (!FileExists(exe)) { Msg(C_BAD, L"%s", L2(L"O PORTHEON ARENA não está na pasta arena do PORTHEON. Baixe o PORTHEON de novo (a instalação ficou incompleta).", L"PORTHEON ARENA is not in PORTHEON's arena folder. Download PORTHEON again (the install is incomplete).")); return; }
    if (g >= 0 && g < NGAMES && ARENA_GAME(g)) { _snwprintf(args, 32, L"--game %s", G[g].id); args[31] = 0; }
    if ((INT_PTR)ShellExecuteW(g_wnd, L"open", exe, args[0] ? args : NULL, dir, SW_SHOWNORMAL) > 32)
        Msg(C_GOOD, L"%s", L2(L"Abrindo o PORTHEON ARENA...", L"Opening PORTHEON ARENA..."));
    else Msg(C_BAD, L"%s", L2(L"O Windows não deixou abrir o PORTHEON ARENA (antivírus?). Abra arena\\PORTHEON ARENA.exe na pasta do PORTHEON.", L"Windows did not let PORTHEON ARENA open (antivirus?). Open arena\\PORTHEON ARENA.exe in PORTHEON's folder."));
}
static int ArenaItems(int g, Item *it, int n)   /* on top of the game's Online page and the online center */
{
    if (!ARENA_GAME(g)) return n;
    n = AddI(it, n, IT_HEAD, NULL, NULL, L2(L"PORTHEON ARENA  ·  LUTAS ONLINE COM A COMUNIDADE", L"PORTHEON ARENA  ·  ONLINE FIGHTS WITH THE COMMUNITY"), 0, 3);
    n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Abrir o PORTHEON ARENA", L"Open PORTHEON ARENA"), H_ARENA, 1);
    n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Ache adversários sem combinar endereço: Fila pública (procura alguém jogando o mesmo UFC), Salas e Convite. A luta passa pelo Parsec, que o ARENA ajuda a instalar (precisa de uma conta grátis do Parsec). Escolha o servidor da sua região (Brasil, Estados Unidos, Europa, Rússia, Coreia / Ásia). Quem hospeda salva o link de convite do Parsec (Parsec > Computers > Share) no perfil do ARENA e abre o jogo pelo botão da luta, que liga o Modo Parsec sozinho; quem entra clica em \"Entrar na luta\". O Peer ID só funciona entre amigos do Parsec.",
                                            L"Find opponents without trading addresses: Public queue (someone playing the same UFC), Rooms and Invite. The fight goes through Parsec, which the ARENA helps you install (a free Parsec account is needed). Pick your region's server (Brazil, United States, Europe, Russia, Korea / Asia). The host saves their Parsec Share link (Parsec > Computers > Share) in the ARENA profile and opens the game with the fight's button, which turns Parsec mode on by itself; the guest clicks \"Join the fight\". A Peer ID only works between Parsec friends."), 0, 3);
    if (UFC6G(g)) n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"UFC 6: quem hospeda liga o \"Modo Parsec\" (abaixo) antes da luta: é ele que dá ao UFC 6 o segundo jogador.", L"UFC 6: the host turns on \"Parsec mode\" (below) before the fight: it gives UFC 6 its second player."), 0, 3);
    return n;
}
static int OnlineItems(int g, Item *it, int n)
{
    static wchar_t pubNote[600];
    n = ArenaItems(g, it, n);   /* 7.2 */
    if (g == 1 || g == 2) {   /* 4.3.6 UFC 2, 6.2 UFC 3: the server PORTHEON hosts, then Parsec */
        int mode = OnlineMode(g);
        n = AddI(it, n, IT_HEAD, NULL, NULL, L2(L"MODO ONLINE DO PORTHEON  ·  © 2026 MARCELO MIYAKE (XANDROMEDAXR)", L"PORTHEON ONLINE MODE  ·  © 2026 MARCELO MIYAKE (XANDROMEDAXR)"), 0, 3);
        n = AddI(it, n, IT_DROP, &F3_ONLINE, NULL, NULL, 0, 1);
        if (mode == 2) n = AddI(it, n, IT_HOSTIP, NULL, NULL, NULL, 0, 2);
        if (mode == 2) {   /* Released 11-13: the host picks the server files (tools / tools-v38 / tools-v38s), only when the newer ones are in this install */
            wchar_t d38[MAX_PATH], s38[MAX_PATH]; PortDir(g, d38); _snwprintf(s38, MAX_PATH, L"%s\\tools-v38\\ufc3_server.py", d38); s38[MAX_PATH - 1] = 0;
            if (FileExists(s38)) {
                n = AddI(it, n, IT_DROP, &F_SRV38, NULL, NULL, 0, 2);
                n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Versão do servidor (vale para quem hospeda): a nova traz os números de verdade no FighterNet, busca de luta por 120 s e pareamento mais solto na Luta rápida. Foi vista entrando no jogo neste PC, mas ainda não numa luta entre dois PCs; se algo falhar, volte para a Padrão. Os dados do online são copiados para a pasta pre-v38 antes da primeira vez.",
                                                        L"Server version (the host's choice): the new one brings real FighterNet numbers, a 120 s fight search and looser Quick Fight pairing. It was seen logging the game in on one PC, not yet in a fight between two PCs; if something fails, go back to Standard. The online data is copied to the pre-v38 folder before the first time."), 0, 3);
            }
        }
        if (mode == 1) n = AddI(it, n, IT_EDIT, NULL, g_editAddr, L2(L"Endereço do anfitrião (o que o amigo passou)", L"The host's address (the one your friend gave you)"), 0, 2);
        if (mode == 2) {   /* 4.3.7: what the router opened the last time this PC hosted */
            wchar_t pub[200]; PublicAddrG(g, pub, 200);
            if (pub[0]) _snwprintf(pubNote, 600, L2(L"Internet (da última vez que você hospedou, o roteador abriu a porta sozinho): %s  (o botão Copiar já copia este)", L"Internet (the last time you hosted, the router opened the port by itself): %s  (the Copy button copies this one)"), pub);
            else _snwprintf(pubNote, 600, L"%s", L2(L"Ao hospedar, o PORTHEON pede ao roteador para abrir a porta sozinho (UPnP). Se o roteador não deixar, use a mesma VPN nos dois PCs.", L"When you host, PORTHEON asks the router to open the port by itself (UPnP). If the router does not allow it, use the same VPN on both PCs."));
            pubNote[599] = 0; n = AddI(it, n, IT_NOTE, NULL, NULL, pubNote, 0, 3);
        }
        if (g == 2) n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Online do UFC 3 entre amigos (novo modo online do PORTHEON, em teste): um hospeda (\"Hospedar\" e JOGAR) e o outro entra (\"Entrar\" com o endereço do anfitrião). Os dois vão em Online > Luta rápida e procuram ao mesmo tempo. Os dois precisam da mesma versão do PORTHEON e do jogo (1.14).",
                                                            L"UFC 3 online between friends (PORTHEON's new online mode, in testing): one hosts (\"Host\" and PLAY) and the other joins (\"Join\" with the host's address). Both go to Online > Quick Fight and search at the same time. Both need the same PORTHEON and game version (1.14)."), 0, 3);
        else
        n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Online do UFC 2 entre amigos (novo modo online do PORTHEON, em teste): login, Ultimate Team (lutadores e moedas), Luta online, Campeonatos Ranqueados (com o painel de divisão e pontos), convites dentro do jogo e Online Rivals. Um hospeda (\"Hospedar\" e JOGAR) e o outro entra (\"Entrar\" com o endereço do anfitrião). Os dois precisam da mesma versão do PORTHEON. Pacotes, loja e placares do Ultimate Team ainda não existem.",
                                                L"UFC 2 online between friends (PORTHEON's new online mode, in testing): login, Ultimate Team (fighters and coins), online fights, Ranked Championships (with the division and points hub), in-game invites and Online Rivals. One hosts (\"Host\" and PLAY) and the other joins (\"Join\" with the host's address). Both need the same PORTHEON version. Ultimate Team packs, store and leaderboards are not there yet."), 0, 3);
        n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Fora da mesma rede: o PORTHEON pede ao roteador do anfitrião para abrir a porta 42230 sozinho (UPnP) e mostra o endereço da internet para passar ao amigo. Se o roteador não deixar, os dois entram na mesma VPN gratuita (Radmin VPN, ZeroTier ou Tailscale) e usam o endereço dela, ou o anfitrião abre a porta 42230 (TCP e UDP) à mão. A luta passa pelo servidor do anfitrião, então só ele precisa da porta. Se o Windows perguntar, permita o python.exe na rede. O PORTHEON fica aberto enquanto você hospeda (o servidor fecha junto com o jogo).",
                                                L"Outside the same network: PORTHEON asks the host's router to open port 42230 by itself (UPnP) and shows the Internet address to give the friend. If the router does not allow it, both join the same free VPN (Radmin VPN, ZeroTier or Tailscale) and use its address, or the host opens port 42230 (TCP and UDP) by hand. The fight goes through the host's server, so only the host needs the port. If Windows asks, allow python.exe on the network. PORTHEON stays open while you host (the server closes with the game)."), 0, 3);
        n = AddI(it, n, IT_HEAD, NULL, NULL, L2(L"OU PELO PARSEC", L"OR THROUGH PARSEC"), 0, 3);
        n = AddI(it, n, IT_SWITCH, &F4_PARSEC, NULL, NULL, 0, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Baixar o Parsec (grátis)", L"Get Parsec (free)"), H_PARSECDL, 1);
        n = AddI(it, n, IT_PADPICK, NULL, NULL, NULL, 0, 2);
    } else if (PS4G(g)) {
        n = AddI(it, n, IT_HEAD, NULL, NULL, L2(L"ONLINE PELO PARSEC", L"ONLINE THROUGH PARSEC"), 0, 3);
        n = AddI(it, n, IT_SWITCH, &F4_PARSEC, NULL, NULL, 0, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Baixar o Parsec (grátis)", L"Get Parsec (free)"), H_PARSECDL, 1);
        n = AddI(it, n, IT_PADPICK, NULL, NULL, NULL, 0, 2);
        n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"1) Os dois instalam o Parsec e criam conta.  2) Conecte o SEU controle, escolha ele em \"Controle do Jogador 1\" (acima) e ligue o \"Modo Parsec\".  3) Aperte JOGAR e, no Parsec, compartilhe o PC com o amigo (Share / convite).  4) O controle dele (virtual, do Parsec) vira o Jogador 2.",
                                                L"1) Both install Parsec and make an account.  2) Connect YOUR controller, pick it in \"Player 1 controller\" (above) and turn on \"Parsec mode\".  3) Press PLAY and, in Parsec, share the PC with your friend (Share / invite).  4) Their (virtual, Parsec) controller becomes Player 2."), 0, 3);
        n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Controles sem bug: no Modo Parsec o jogo deixa um jogador por controle e continua lendo os controles mesmo com a janela do Parsec na frente. Menor atraso: 1 quadro na fila, 60 FPS fixos e Mailbox. No Parsec do anfitrião: Settings > Host, resolução 1920x1080, 60 FPS, 10 a 20 Mbps; o amigo: decodificador Hardware, H.265 e VSync desligado. Os dois no cabo de rede.",
                                                L"Controllers without trouble: in Parsec mode the game keeps one player per controller and keeps reading controllers even with Parsec's window in front. Lowest delay: 1 frame queued, a steady 60 FPS and Mailbox. Host's Parsec: Settings > Host, 1920x1080, 60 FPS, 10-20 Mbps; the friend: Hardware decoder, H.265, VSync off. Both on a network cable."), 0, 3);
        n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Por que não é o online do próprio jogo: os servidores da EA destes jogos não atendem o port, e o servidor próprio do port existe só para o UFC 2 e o UFC 3. O Parsec serve para as lutas entre dois jogadores (a Carreira é de um jogador só).",
                                                L"Why it is not the game's own online: EA's servers for these games do not answer the port, and the port's own server exists only for UFC 2 and UFC 3. Parsec is for two-player fights (Career is single-player)."), 0, 3);
    } else if (G[g].ps5) {   /* 4.3.1: UFC 5 and 6 through Parsec (UFC5.exe 1.17.0 / UFC6.exe 1.3.0 read "parsec" in their ini) */
        n = AddI(it, n, IT_HEAD, NULL, NULL, L2(L"ONLINE PELO PARSEC", L"ONLINE THROUGH PARSEC"), 0, 3);
        n = AddI(it, n, IT_SWITCH, &F4_PARSEC, NULL, NULL, 0, 1);
        n = AddI(it, n, IT_FBUTTON, NULL, NULL, L2(L"Baixar o Parsec (grátis)", L"Get Parsec (free)"), H_PARSECDL, 1);
        n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Modo Parsec no UFC 5 e no UFC 6: o jogo continua lendo os controles com a janela do Parsec (ou outra) na frente, sem rasgar a imagem (Mailbox) e a 60 Hz, o ritmo da transmissão do Parsec. 1) Os dois instalam o Parsec e criam conta.  2) Ligue o \"Modo Parsec\" e aperte JOGAR.  3) No Parsec, compartilhe este PC com o amigo (Share / convite).",
                                                    L"Parsec mode in UFC 5 and UFC 6: the game keeps reading the controllers with Parsec's window (or another one) in front, without tearing (Mailbox) and at 60 Hz, the pace of Parsec's stream. 1) Both install Parsec and make an account.  2) Turn on \"Parsec mode\" and press PLAY.  3) In Parsec, share this PC with your friend (Share / invite)."), 0, 3);
        /* 4.3.5: UFC 6's Parsec mode has a second PS5 user and pad since UFC6.exe 1.7.0 (the Danteh3-lab 2-player hooks):
           player 2 is the next gamepad that connects, a friend's through Parsec or a second one on this PC (local versus) */
        if (UFC6G(g)) n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"UFC 6: dois jogadores ao mesmo tempo. Com o Modo Parsec ligado, o próximo controle que se conectar depois do seu vira o Jogador 2: o do amigo pelo Parsec ou um segundo controle neste PC (luta local). Conecte o controle 1 primeiro, depois o 2, e aperte JOGAR.",
                                                    L"UFC 6: two players at the same time. With Parsec mode on, the next controller that connects after yours becomes Player 2: your friend's through Parsec or a second controller on this PC (local versus). Connect controller 1 first, then controller 2, and press PLAY."), 0, 3);
        else n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Um jogador por vez: os motores de PS5 do UFC 5 e do UFC 6 têm um usuário de PS5 só, então pelo Parsec o amigo joga com o controle dele no seu lugar (revezando). Luta entre dois jogadores ao mesmo tempo é nos UFC 1 a 4. No Parsec do anfitrião: Settings > Host, 1920x1080, 60 FPS, 10 a 20 Mbps; o amigo: decodificador Hardware e VSync desligado.",
                                                    L"One player at a time: UFC 5's and UFC 6's PS5 engines have a single PS5 user, so through Parsec your friend plays with their controller in your place (taking turns). Fights between two players at the same time are in UFC 1 to 4. Host's Parsec: Settings > Host, 1920x1080, 60 FPS, 10-20 Mbps; the friend: Hardware decoder, VSync off."), 0, 3);
    } else if (EXTG(g)) {   /* 6.0: Zelda and God of War III */
        n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"Este jogo não tem modo online no PORTHEON. Para jogar com um amigo a distância, use o Parsec compartilhando a tela deste PC.",
                                                L"This game has no online mode in PORTHEON. To play with a friend far away, use Parsec, sharing this PC's screen."), 0, 3);
    } else if (X360G(g)) {   /* 5.0: UFC Undisputed 3 */
        n = AddI(it, n, IT_NOTE, NULL, NULL, L2(L"UFC Undisputed 3: o PORTHEON não tem online para este jogo (os servidores da THQ fecharam). Os controles do port ficam em \"Configurar o port\", na Visão geral do jogo.",
                                                L"UFC Undisputed 3: PORTHEON has no online for this game (THQ's servers closed). The port's controllers are set in \"Configure the port\", on the game's Overview."), 0, 3);
    }
    return n;
}
