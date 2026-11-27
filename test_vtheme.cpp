// VestaHome's look for the programs beside it (vtheme.h) on the PC: the skin's colours, the accent, the order.
#include <stdio.h>
#include <string.h>
#include "vtheme.h"

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static void TestColors()
{
    VTheme t;
    VThemeDefaults(&t);
    Check(t.ink == RGB(0x0e, 0x10, 0x13) && t.accent == RGB(0xff, 0x6b, 0x1a) && !t.userAccent, "defaults");
    VThemeColors(&t, "\xef\xbb\xbf; a comment\r\n[colors]\r\nink     = 010203   ; background\r\n"
                     "panel = #0A0B0C\r\ntext=FFFFFF\r\nbogus = 123456\r\nmuted = 12345\r\n"
                     "[images]\r\nline = 999999\r\n", false);
    Check(t.ink == RGB(1, 2, 3), "colors: a line, its comment cut, the BOM");
    Check(t.panel == RGB(10, 11, 12), "colors: #RRGGBB");
    Check(t.text == RGB(255, 255, 255), "colors: no spaces");
    Check(t.muted == RGB(0xaa, 0xb0, 0xb8), "colors: a short value ignored");
    Check(t.line == RGB(0x2a, 0x2f, 0x37), "colors: only in [colors]");
    Check(!t.userAccent, "colors: no accent, not noted");
    VThemeColors(&t, "accent = 00FF00\n", false);
    Check(t.accent == RGB(0, 255, 0) && !t.userAccent, "colors: the skin's accent, no section line (colors first)");
    VThemeColors(&t, "[colors]\npanel2=111111\nok=222222\nstop=333333\noff=444444\ndim=555555\n", true);
    Check(t.panel2 == RGB(0x11, 0x11, 0x11) && t.ok == RGB(0x22, 0x22, 0x22) && t.stop == RGB(0x33, 0x33, 0x33) &&
          t.off == RGB(0x44, 0x44, 0x44) && t.dim == RGB(0x55, 0x55, 0x55) && !t.userAccent, "colors: the rest");
    VThemeColors(&t, "[colors]\naccent = 12345\naccent = 00000g\n", true);
    Check(!t.userAccent && t.accent == RGB(0, 255, 0), "colors: a broken user accent is not the user's");
    VThemeColors(&t, "[colors]\nACCENT = 0000FF\n", true);
    Check(t.accent == RGB(0, 0, 255) && t.userAccent, "colors: the user's accent noted, any case");

    VThemeDefaults(&t);
    VThemeColors(&t, "\xef\xbb\xbfink = 09afAF", false);
    Check(t.ink == RGB(0x09, 0xaf, 0xaf), "colors: the BOM, all the digits' edges, no line end");
    VThemeColors(&t, "\tpanel\t=\t102030\t \r\ntext = 0g0000\nmuted = 0:0000\ndim = 0`0000\nline = 0G0000\n", false);
    Check(t.panel == RGB(0x10, 0x20, 0x30), "colors: tabs and spaces trimmed");
    Check(t.text == RGB(0xf5, 0xf3, 0xee) && t.muted == RGB(0xaa, 0xb0, 0xb8) && t.dim == RGB(0x6c, 0x72, 0x7c) &&
          t.line == RGB(0x2a, 0x2f, 0x37), "colors: not hex digits ignored");
}

static void TestAccent()
{
    VTheme t;
    VThemeDefaults(&t);
    Check(VThemeAccent(&t, 476) && t.accent == RGB(0x4f, 0x7a, 0xca), "accent: Дайвинг");
    Check(VThemeAccent(&t, 130) && t.accent == RGB(0xff, 0x6b, 0x1a), "accent: Марс");
    Check(!VThemeAccent(&t, 999) && t.accent == RGB(0xff, 0x6b, 0x1a), "accent: an unknown code");
    struct { int code; COLORREF c; } paints[] = {
        { 195, RGB(0xdc, 0x3c, 0x3c) }, { 247, RGB(0xc3, 0xb8, 0x9c) }, { 372, RGB(0x72, 0xc0, 0x59) },
        { 619, RGB(0xab, 0xba, 0xd8) }, { 691, RGB(0xee, 0xee, 0xee) }, { 130, RGB(0xff, 0x6b, 0x1a) } };
    bool all = true;
    for (int i = 0; i < 6; i++)
        all = all && VThemeAccent(&t, paints[i].code) && t.accent == paints[i].c;
    Check(all, "accent: every paint (Марс over another)");
    Check(!VThemeAccent(&t, 248) && !VThemeAccent(&t, 692), "accent: only the codes");
    Check(VThemeAccentCode("a=1\r\nscreen.accent=372\r\nb=2\r\n") == 372, "accent code: a line");
    Check(VThemeAccentCode("screen.accent=619\n") == 619, "accent code: the first line");
    Check(VThemeAccentCode("x.screen.accent=619\n") == 0, "accent code: the key's own line only");
    Check(VThemeAccentCode("a=1\n") == 0, "accent code: none");
}

static void Write(const char* path, const char* text)
{
    FILE* f = fopen(path, "wb");
    fwrite(text, 1, strlen(text), f);
    fclose(f);
}

static void TestLoad()
{
    CreateDirectoryA("obj\\vh", NULL);
    CreateDirectoryA("obj\\vh\\Skin", NULL);
    Write("obj\\vh\\Skin\\skin.ini", "[colors]\nink = 111111\naccent = 222222\n");
    Write("obj\\vh\\VestaHome.ini", "screen.accent=195\n");
    VTheme t;
    VThemeLoad(&t, L"obj\\vh\\");
    Check(t.ink == RGB(0x11, 0x11, 0x11), "load: the skin");
    Check(t.accent == RGB(0xdc, 0x3c, 0x3c), "load: the chosen accent over the skin's");
    Check(!wcscmp(t.vh, L"obj\\vh\\"), "load: the folder kept");
    Write("obj\\vh\\VestaHome.ini", "screen.accent=130\n");
    VThemeLoad(&t, L"obj\\vh\\");
    Check(t.accent == RGB(0x22, 0x22, 0x22), "load: Марс is the skin's own accent");
    // the user's skins (the PC: in VestaHome's folder): the card's, then the partition's over it
    CreateDirectoryA("obj\\vh\\VHContent", NULL);
    CreateDirectoryA("obj\\vh\\VHContent\\Skin", NULL);
    CreateDirectoryA("obj\\vh\\SDMemory", NULL);
    CreateDirectoryA("obj\\vh\\SDMemory\\VHContent", NULL);
    CreateDirectoryA("obj\\vh\\SDMemory\\VHContent\\Skin", NULL);
    Write("obj\\vh\\SDMemory\\VHContent\\Skin\\user_skin.ini", "[colors]\npanel = 333333\ntext = 444444\n");
    Write("obj\\vh\\VHContent\\Skin\\user_skin.ini", "[colors]\ntext = 555555\naccent = 666666\n");
    Write("obj\\vh\\VestaHome.ini", "screen.accent=195\n");
    VThemeLoad(&t, L"obj\\vh\\");
    Check(t.panel == RGB(0x33, 0x33, 0x33), "load: the card's user skin");
    Check(t.text == RGB(0x55, 0x55, 0x55), "load: the partition's wins");
    Check(t.accent == RGB(0x66, 0x66, 0x66) && t.userAccent, "load: the user's accent over the chosen one");
    DeleteFileA("obj\\vh\\VHContent\\Skin\\user_skin.ini");
    DeleteFileA("obj\\vh\\SDMemory\\VHContent\\Skin\\user_skin.ini");
    VThemeLoad(&t, L"");
    Check(t.ink == RGB(0x0e, 0x10, 0x13) && !t.vh[0], "load: no folder, the built-in");
}

static void TestFind()   // the PC: VestaHome\ beside the exe (obj\), with its skin
{
    wchar_t out[MAX_PATH] = L"x";
    DeleteFileA("obj\\VestaHome\\Skin\\skin.ini");
    Check(!VThemeFindVh(out) && !out[0], "find: none");
    CreateDirectoryA("obj\\VestaHome", NULL);
    CreateDirectoryA("obj\\VestaHome\\Skin", NULL);
    Write("obj\\VestaHome\\Skin\\skin.ini", "[colors]\n");
    int n = (int)wcslen(out);
    Check(VThemeFindVh(out) && (n = (int)wcslen(out)) > 14 && !wcscmp(out + n - 14, L"obj\\VestaHome\\"),
          "find: beside the exe");
}

int main()
{
    TestColors();
    TestAccent();
    TestLoad();
    TestFind();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
