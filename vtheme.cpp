// vtheme.cpp - VestaCore: VestaHome's look for the programs beside it (vtheme.h).
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "vtheme.h"

void VThemeDefaults(VTheme* t)   // (VestaHome's skin.ini as installed, vh_draw.inc's built-in)
{
    memset(t, 0, sizeof(*t));
    t->ink = RGB(0x0e, 0x10, 0x13);
    t->panel = RGB(0x1a, 0x1d, 0x22);
    t->panel2 = RGB(0x23, 0x27, 0x2e);
    t->line = RGB(0x2a, 0x2f, 0x37);
    t->text = RGB(0xf5, 0xf3, 0xee);
    t->muted = RGB(0xaa, 0xb0, 0xb8);
    t->dim = RGB(0x6c, 0x72, 0x7c);
    t->accent = RGB(0xff, 0x6b, 0x1a);
    t->off = RGB(0x3d, 0x42, 0x4a);
    t->ok = RGB(0x2f, 0xbf, 0x71);
    t->stop = RGB(0xe8, 0x48, 0x3b);
}

static COLORREF* ColorOf(VTheme* t, const char* key, int n)
{
    struct Key { const char* k; size_t off; };
    static const Key keys[] = {
        { "ink", offsetof(VTheme, ink) },       { "panel", offsetof(VTheme, panel) },
        { "panel2", offsetof(VTheme, panel2) }, { "line", offsetof(VTheme, line) },
        { "text", offsetof(VTheme, text) },     { "muted", offsetof(VTheme, muted) },
        { "dim", offsetof(VTheme, dim) },       { "accent", offsetof(VTheme, accent) },
        { "off", offsetof(VTheme, off) },       { "ok", offsetof(VTheme, ok) },
        { "stop", offsetof(VTheme, stop) } };
    for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++)
        if ((int)strlen(keys[i].k) == n && !_strnicmp(keys[i].k, key, n))
            return (COLORREF*)((char*)t + keys[i].off);
    return NULL;
}

static int HexDigit(char ch)   // -1: not one
{
    int l = ch | 32;
    return ch >= '0' && ch <= '9' ? ch - '0' : l >= 'a' && l <= 'f' ? l - 'a' + 10 : -1;
}

static bool Hex6(const char* v, int n, COLORREF* c)   // "RRGGBB" / "#RRGGBB", n its length
{
    if (n && *v == '#') {
        v++;
        n--;
    }
    if (n != 6)
        return false;
    unsigned long x = 0;
    for (int i = 0; i < 6; i++) {
        int d = HexDigit(v[i]);
        if (d < 0)
            return false;
        x = x * 16 + d;
    }
    *c = RGB(x >> 16, (x >> 8) & 255, x & 255);
    return true;
}

static const char* Word(const char* s, const char* e, int* n)   // the trimmed part of [s, e): its start, length
{
    while (s < e && (*s == ' ' || *s == '\t'))
        s++;
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
        e--;
    *n = (int)(e - s);
    return s;
}

static void ColorLine(VTheme* t, const char* s, const char* e, bool user)   // "name = RRGGBB"
{
    const char* eq = (const char*)memchr(s, '=', e - s);
    if (!eq)
        return;
    int nk, nv;
    const char* k = Word(s, eq, &nk);
    const char* v = Word(eq + 1, e, &nv);
    COLORREF* c = ColorOf(t, k, nk);
    if (c && Hex6(v, nv, c) && user && c == &t->accent)
        t->userAccent = true;
}

// One line [s, e): a section's line sets *colors; in [colors] a colour.
static void SkinLine(VTheme* t, const char* s, const char* e, bool user, bool* colors)
{
    const char* semi = (const char*)memchr(s, ';', e - s);
    int n;
    const char* w = Word(s, semi ? semi : e, &n);
    if (n && *w == '[')
        *colors = n == 8 && !_strnicmp(w, "[colors]", 8);
    else if (*colors)
        ColorLine(t, w, w + n, user);
}

void VThemeColors(VTheme* t, const char* text, bool user)
{
    if (!strncmp(text, "\xef\xbb\xbf", 3))
        text += 3;
    bool colors = true;   // (the lines before any section: VestaHome's SkinFile starts in [colors] too)
    while (*text) {
        const char* e = strchr(text, '\n');
        if (!e)
            e = text + strlen(text);
        SkinLine(t, text, e, user, &colors);
        text = *e ? e + 1 : e;
    }
}

struct Paint { int code; COLORREF c; };
static const Paint kPaints[] = {   // (VestaHome's kAccents, vh_settings.inc: 130 Марс - the skin's own)
    { 130, RGB(0xff, 0x6b, 0x1a) }, { 195, RGB(0xdc, 0x3c, 0x3c) }, { 247, RGB(0xc3, 0xb8, 0x9c) },
    { 372, RGB(0x72, 0xc0, 0x59) }, { 476, RGB(0x4f, 0x7a, 0xca) }, { 619, RGB(0xab, 0xba, 0xd8) },
    { 691, RGB(0xee, 0xee, 0xee) } };

bool VThemeAccent(VTheme* t, int code)
{
    for (int i = 0; i < (int)(sizeof(kPaints) / sizeof(kPaints[0])); i++)
        if (kPaints[i].code == code) {
            t->accent = kPaints[i].c;
            return true;
        }
    return false;
}

int VThemeAccentCode(const char* ini)
{
    for (const char* p = ini; (p = strstr(p, "screen.accent=")) != NULL; p++)
        if (p == ini || p[-1] == '\n')
            return atoi(p + 14);
    return 0;
}

static char* ReadText(const wchar_t* path)   // the file with a zero after it; NULL none (64 KB at most)
{
    HANDLE f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return NULL;
    DWORD size = GetFileSize(f, NULL), got = 0;
    char* buf = size < 65536 ? (char*)malloc(size + 1) : NULL;
    if (buf && !ReadFile(f, buf, size, &got, NULL))
        got = 0;
    CloseHandle(f);
    if (buf)
        buf[got] = 0;
    return buf;
}

static void ColorsOfFile(VTheme* t, const wchar_t* dir, const wchar_t* name, bool user)
{
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", dir, name);
    path[MAX_PATH - 1] = 0;
    char* text = ReadText(path);
    if (text)
        VThemeColors(t, text, user);
    free(text);
}

void VThemeLoad(VTheme* t, const wchar_t* vh)
{
    VThemeDefaults(t);
    wcsncpy(t->vh, vh, MAX_PATH - 1);
    if (!vh[0])
        return;
    ColorsOfFile(t, vh, L"Skin\\skin.ini", false);
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sVestaHome.ini", vh);
    path[MAX_PATH - 1] = 0;
    char* ini = ReadText(path);
    int code = ini ? VThemeAccentCode(ini) : 0;
    free(ini);
    COLORREF skin = t->accent;
    wchar_t udirs[2][MAX_PATH];   // the card's first: the partition's wins (VestaHome's LoadSkin)
#ifdef UNDER_CE
    wcscpy(udirs[0], L"\\SDMemory\\VHContent\\Skin\\");
    wcscpy(udirs[1], L"\\ResidentFlash\\VHContent\\Skin\\");
#else
    _snwprintf(udirs[0], MAX_PATH, L"%sSDMemory\\VHContent\\Skin\\", vh);
    _snwprintf(udirs[1], MAX_PATH, L"%sVHContent\\Skin\\", vh);
    udirs[0][MAX_PATH - 1] = udirs[1][MAX_PATH - 1] = 0;
#endif
    for (int i = 0; i < 2; i++)
        ColorsOfFile(t, udirs[i], L"user_skin.ini", true);
    if (!t->userAccent && (code == 130 || !VThemeAccent(t, code)))   // Марс / none: the skin's
        t->accent = skin;
}

static int VhPlaces(wchar_t (*dirs)[MAX_PATH])   // where VestaHome may be, in order
{
#ifdef UNDER_CE
    wcscpy(dirs[0], L"\\ResidentFlash\\VestaHome\\");
    wcscpy(dirs[1], L"\\SDMMC\\VestaHome\\");
    return 2;
#else
    DWORD n = GetModuleFileName(NULL, dirs[0], MAX_PATH);
    wchar_t* s = n && n < MAX_PATH ? wcsrchr(dirs[0], L'\\') : NULL;
    if (!s || (s - dirs[0]) + 12 >= MAX_PATH)
        return 0;
    wcscpy(s + 1, L"VestaHome\\");
    return 1;
#endif
}

bool VThemeFindVh(wchar_t* out)   // (its skin there)
{
    wchar_t dirs[2][MAX_PATH], p[MAX_PATH];
    int n = VhPlaces(dirs);
    for (int i = 0; i < n; i++) {
        _snwprintf(p, MAX_PATH, L"%sSkin\\skin.ini", dirs[i]);
        p[MAX_PATH - 1] = 0;
        if (GetFileAttributes(p) != 0xFFFFFFFF) {
            wcscpy(out, dirs[i]);
            return true;
        }
    }
    out[0] = 0;
    return false;
}
