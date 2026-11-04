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

int main()
{
    TestField();
    TestSend();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
