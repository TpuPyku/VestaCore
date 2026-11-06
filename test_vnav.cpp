// test_vnav.cpp - vnav.cpp on the PC: what OSMapsCE sends comes out the same in VestaHome, the instruction's parts, the
// sign's pixels. Exit code: the failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "vnav.h"
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

static VNav Sample()
{
    VNav n;
    memset(&n, 0, sizeof(n));
    n.on = true;
    n.turn = -90;
    wcscpy(n.dist, L"350 м");
    wcscpy(n.street, L"ул. Льва Толстого");
    wcscpy(n.eta, L"14:41");
    return n;
}

static void TestRoundTrip()
{
    wchar_t cmd[256];
    VNav a = Sample(), b;
    VNavFormat(a, cmd, 256);
    Check(!wcsncmp(cmd, L"nav\t", 4), "the verb first");
    memset(&b, 0xFF, sizeof(b));
    VNavParse(cmd, &b);
    Check(b.on, "on comes through");
    Check(b.turn == -90, "a left turn comes through");
    Check(!wcscmp(b.dist, L"350 м"), "the distance comes through");
    Check(!wcscmp(b.street, L"ул. Льва Толстого"), "the street comes through");
    Check(!wcscmp(b.eta, L"14:41"), "the arrival comes through");
    a.turn = VNAV_FINISH;
    VNavFormat(a, cmd, 256);
    VNavParse(cmd, &b);
    Check(b.turn == VNAV_FINISH, "the finish comes through");
}

static void TestOff()
{
    wchar_t cmd[256];
    VNav a = Sample(), b = Sample();
    a.on = false;
    VNavFormat(a, cmd, 256);
    Check(!wcsncmp(cmd, L"nav\t", 4) && !wcsstr(cmd, L"Толстого"), "off: the verb, no leftovers sent");
    VNavParse(cmd, &b);
    Check(!b.on && !b.dist[0] && !b.street[0] && !b.eta[0] && !b.turn, "off: nothing left of the last manoeuvre");
    b = Sample();
    VNavParse(L"widget\ton=1\tturn=5", &b);
    Check(!b.on, "another command is not a route");
}

static void TestDirtyText()
{
    wchar_t cmd[256], v[16];
    VNav a = Sample(), b;
    wcscpy(a.street, L"Новая\tул\r\nдом");
    wcscpy(a.eta, L"12:00");
    VNavFormat(a, cmd, 256);
    VNavParse(cmd, &b);
    Check(!wcscmp(b.street, L"Новая ул  дом"), "a tab and line breaks in the text: spaces");
    Check(!wcscmp(b.eta, L"12:00"), "the field after them survives");
    Check(!wcschr(cmd, L'\n') && !wcschr(cmd, L'\r'), "no line breaks sent");
    Check(VhapField(cmd, L"on", v, 16) && !wcscmp(v, L"1"), "the protocol's own reader sees the fields");
    wchar_t tiny[12];
    VNavFormat(Sample(), tiny, 12);
    Check(wcslen(tiny) == 11, "a short buffer: cut and closed");
    VNavParse(L"nav\ton=1\tstreet=12345678901234567890123456789012345678901234567890123456789012345678901234567890", &b);
    Check(wcslen(b.street) == 63, "a long street: cut to the field");
    VNavParse(L"nav\ton=1\tdist=12345678901234567890\tstreet=x\teta=12345678901234567890", &b);
    Check(wcslen(b.dist) == 15 && !wcscmp(b.street, L"x"), "a long distance: cut, the street untouched");
    Check(wcslen(b.eta) == 7, "a long arrival: cut to the field");
}

static void TestSplit()
{
    wchar_t verb[32], street[32];
    VNavSplit(L"Поверните направо: Тверская", verb, street, 32);
    Check(!wcscmp(verb, L"направо"), "the verb without its first word");
    Check(!wcscmp(street, L"Тверская"), "the street after the colon");
    VNavSplit(L"Держитесь левее: МКАД", verb, street, 32);
    Check(!wcscmp(verb, L"Держитесь левее"), "another instruction stays whole");
    VNavSplit(L"Вы приехали", verb, street, 32);
    Check(!verb[0] && !wcscmp(street, L"Вы приехали"), "no colon: all of it the street");
    wchar_t v2[6], s2[6];
    VNavSplit(L"Поверните направо: Тверская", v2, s2, 6);
    Check(!wcscmp(v2, L"напра") && !wcscmp(s2, L"Тверс"), "short outs: cut and closed");
}

enum { kCanvas = 152 };

// Where the sign's colour is on a canvas: pixels on the box's left and right halves, and outside the box.
static void SignPixels(int turn, int box, int* left, int* right, int* outside)
{
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = kCanvas;
    bi.bmiHeader.biHeight = -kCanvas;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ ob = SelectObject(dc, bmp);
    memset(bits, 0, kCanvas * kCanvas * 4);
    RECT r = { 30, 30, 30 + box, 30 + box };
    HGDIOBJ pen0 = GetCurrentObject(dc, OBJ_PEN);
    VNavDrawTurn(dc, r, turn, RGB(255, 255, 255));
    GdiFlush();
    Check(GetCurrentObject(dc, OBJ_PEN) == pen0, "the pen given back");
    *left = *right = *outside = 0;
    for (int y = 0; y < kCanvas; y++)
        for (int x = 0; x < kCanvas; x++)
            if (((DWORD*)bits)[y * kCanvas + x] & 0xFFFFFF) {
                if (x < r.left || x >= r.right || y < r.top || y >= r.bottom)
                    ++*outside;
                else if (x < (r.left + r.right) / 2)
                    ++*left;
                else
                    ++*right;
            }
    SelectObject(dc, ob);
    DeleteObject(bmp);
    DeleteDC(dc);
}

static void TestSign()
{
    int l, r, o;
    SignPixels(90, 76, &l, &r, &o);
    Check(r > l + 50, "a right turn leans right");
    Check(o == 0, "a right turn stays in its box");
    SignPixels(-90, 76, &l, &r, &o);
    Check(l > r + 50, "a left turn leans left");
    SignPixels(VNAV_FINISH, 76, &l, &r, &o);
    Check(l + r > 200 && o == 0, "the finish is drawn in its box");
    SignPixels(VNAV_ROUND, 76, &l, &r, &o);
    Check(l + r > 200 && o == 0, "a roundabout is drawn in its box");
    SignPixels(180, 76, &l, &r, &o);
    Check(l + r > 200 && o == 0, "a U-turn is drawn in its box");
    SignPixels(90, 28, &l, &r, &o);
    Check(r > l && o == 0, "a small box: scaled");
}

int main()
{
    TestRoundTrip();
    TestOff();
    TestDirtyText();
    TestSplit();
    TestSign();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
