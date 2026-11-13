// test_vhap.cpp - vhap.cpp on the PC: a command's fields, a command sent to a real window (what it receives, the
// answer coming back). Exit code: the failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "vhap.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static void TestField()
{
    wchar_t v[16];
    const wchar_t* cmd = L"route\tlat=53.2\tlon=50.1\tname=Дом";
    Check(VhapField(cmd, L"lat", v, 16) && !wcscmp(v, L"53.2"), "a field in the middle");
    Check(VhapField(cmd, L"name", v, 16) && !wcscmp(v, L"Дом"), "the last field");
    Check(!VhapField(cmd, L"zoom", v, 16) && !v[0], "a missing field: false, empty");
    Check(!VhapField(cmd, L"la", v, 16), "a key's beginning is not the key");
    Check(!VhapField(cmd, L"route", v, 16), "the command's name is not a field");
    Check(VhapField(L"x\tk=", L"k", v, 16) && !v[0], "an empty value");
    Check(VhapField(L"x\tk=abcdef", L"k", v, 4) && !wcscmp(v, L"abc"), "a long value cut to the room");
    Check(VhapField(L"x\t\tk=v", L"k", v, 16) && !wcscmp(v, L"v"), "after an empty field");
}

static wchar_t s_got[1024];
static DWORD   s_gotMagic;
static HWND    s_gotFrom;
static int     s_gotLen;   // characters

static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_COPYDATA) {
        const COPYDATASTRUCT* c = (const COPYDATASTRUCT*)l;
        s_gotMagic = (DWORD)c->dwData;
        s_gotFrom = (HWND)w;
        int n = (int)(c->cbData / sizeof(wchar_t));
        s_gotLen = n;
        memcpy(s_got, c->lpData, n * sizeof(wchar_t));
        s_got[n] = 0;
        return 2;
    }
    return DefWindowProc(h, m, w, l);
}

static void TestSend()
{
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = Proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"VhapTest";
    RegisterClass(&wc);
    HWND to = CreateWindow(L"VhapTest", L"", 0, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    HWND from = CreateWindow(L"VhapTest", L"", 0, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    LRESULT r = VhapSend(to, from, L"key\tcode=%d\tst=%d", 8, 1);
    Check(r == 2, "the receiver's answer comes back");
    Check(s_gotMagic == VHAP_MAGIC, "the magic");
    Check(s_gotFrom == from, "the sender's window");
    Check(!wcscmp(s_got, L"key\tcode=8\tst=1"), "the text, without its zero");
    wchar_t lng[701];
    for (int i = 0; i < 700; i++)
        lng[i] = L'a' + i % 26;
    lng[700] = 0;
    VhapSend(to, from, L"%s", lng);
    Check(wcslen(s_got) == 511 && !wcsncmp(s_got, lng, 511), "a long command: its first 511 characters");
    VhapSend(to, from, L"%s", L"");
    Check(s_gotLen == 0, "an empty command: no characters");
    DestroyWindow(to);
    Check(VhapSend(to, from, L"hello") == 0, "a window gone: 0, nothing sent");
    DestroyWindow(from);
}

static void TestCommand()   // a WM_COPYDATA's lParam read back as a command
{
    wchar_t text[] = L"event\tid=gate", out[64];
    COPYDATASTRUCT c = { VHAP_MAGIC, 13 * sizeof(wchar_t), text };
    Check(VhapCommand((LPARAM)&c, out, 64) && !wcscmp(out, L"event\tid=gate"), "command: the text with its zero");
    Check(VhapCommand((LPARAM)&c, out, 6) && !wcscmp(out, L"event"), "command: cut to the room");
    COPYDATASTRUCT other = { 1234, 13 * sizeof(wchar_t), text };
    Check(!VhapCommand((LPARAM)&other, out, 64), "command: another magic - not ours");
    COPYDATASTRUCT empty = { VHAP_MAGIC, 0, NULL };
    Check(!VhapCommand((LPARAM)&empty, out, 64) && !VhapCommand(0, out, 64), "command: no data - not ours");
    COPYDATASTRUCT six = { VHAP_MAGIC, 6 * sizeof(wchar_t), text };
    out[6] = L'#';
    Check(VhapCommand((LPARAM)&six, out, 6) && !wcscmp(out, L"event") && out[6] == L'#',
          "command: as long as the room - cut by one for the zero, nothing written past it");
}

static wchar_t s_on[64];
static void On(const wchar_t* cmd)
{
    wcscpy(s_on, cmd);
}

static void TestAddon()   // an add-on's hidden window: a made-up VestaHome (its class) gets what it sends
{
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = Proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"VestaHome_Wnd";
    RegisterClass(&wc);
    HWND self = CreateWindow(L"VhapTest", L"", 0, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    VhapAddon a = { self, NULL, L"id=x\tname=Икс\tver=1", On };
    Check(VhapAddonMsg(&a, WM_TIMER, VHAP_TIMER_FIND, 0) && !a.vh, "addon: no VestaHome - none found");
    HWND vh = CreateWindow(L"VestaHome_Wnd", L"", 0, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    wchar_t want[80];
    swprintf(want, L"hello\tid=x\tname=Икс\tver=1\tapi=1\thwnd=%08X", (unsigned)(UINT_PTR)self);
    Check(VhapAddonMsg(&a, WM_TIMER, VHAP_TIMER_FIND, 0) && a.vh == vh && !wcscmp(s_got, want),
          "addon: the find timer - VestaHome found, hello with the api and our window");
    s_got[0] = 0;
    Check(VhapAddonMsg(&a, WM_TIMER, VHAP_TIMER_FIND, 0) && !s_got[0], "addon: VestaHome there - no hello again");
    Check(!VhapAddonMsg(&a, WM_TIMER, 7, 0), "addon: another timer - not ours");
    DestroyWindow(vh);   // VestaHome restarted: its old window gone, a new one
    vh = CreateWindow(L"VestaHome_Wnd", L"", 0, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    Check(VhapAddonMsg(&a, WM_TIMER, VHAP_TIMER_FIND, 0) && a.vh == vh && !wcsncmp(s_got, L"hello", 5),
          "addon: VestaHome restarted - found again, hello again");
    wchar_t text[] = L"welcome\tapi=1";
    COPYDATASTRUCT c = { VHAP_MAGIC, 13 * sizeof(wchar_t), text };
    Check(VhapAddonMsg(&a, WM_COPYDATA, 0, (LPARAM)&c) && !wcscmp(s_on, L"welcome\tapi=1"), "addon: a command to on");
    COPYDATASTRUCT other = { 1, 13 * sizeof(wchar_t), text };
    s_on[0] = 0;
    Check(!VhapAddonMsg(&a, WM_COPYDATA, 0, (LPARAM)&other) && !s_on[0], "addon: another WM_COPYDATA - not ours");
    Check(VhapAddonMsg(&a, WM_DESTROY, 0, 0) && !wcscmp(s_got, L"bye"), "addon: closing - bye");
    MSG m;
    bool quit = false;
    while (!quit && PeekMessage(&m, NULL, 0, 0, PM_REMOVE))
        quit = m.message == WM_QUIT;
    Check(quit && m.wParam == 0, "addon: closing - the loop quits, exit code 0");
    Check(!VhapAddonMsg(&a, WM_PAINT, 0, 0), "addon: other messages - not ours");
    DestroyWindow(vh);
    DestroyWindow(self);
}

int main()
{
    TestField();
    TestSend();
    TestCommand();
    TestAddon();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
