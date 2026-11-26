// vkeys.cpp - VestaCore: the on-screen keyboard's text and cursor (see vkeys.h; moved from VestaHome 2026-10-29).
#include "vkeys.h"
#include <string.h>

// The keys as a PC's (the user 2026-11-26): Russian ЙЦУКЕН with ё left of 1, English QWERTY; rows of 13, 13, 11 and
// 10. Each key: its own character, then its second and third (' ' none) - a long press shows them in a bubble, the
// finger picks one. With them each layout holds every printable ASCII character (and № « » — … ° €).
struct KeyRow { const wchar_t* own; const wchar_t* second; const wchar_t* third; };
static const KeyRow kRu[VKEYS_ROWS] = {
    { L"ё1234567890-=", L"`!\"№;%:?*()_+", L"~ @#$ ^&  °— " },
    { L"йцукенгшщзхъ\\", L"          []/", L"          {}|" },
    { L"фывапролджэ", L"          '", L"           " },
    { L"ячсмитьбю.", L"       <>,", L"       «»…" } };
static const KeyRow kEn[VKEYS_ROWS] = {
    { L"`1234567890-=", L"~!@#$%^&*()_+", L"          °— " },
    { L"qwertyuiop[]\\", L"  €       {}|", L"             " },
    { L"asdfghjkl;'", L"         :\"", L"           " },
    { L"zxcvbnm,./", L"       <>?", L"       «»…" } };

void VKeysOpen(VKeys* k, const wchar_t* text, int cap)
{
    memset(k, 0, sizeof(*k));
    k->cap = cap < VKEYS_MAX - 1 ? cap : VKEYS_MAX - 1;
    wcsncpy(k->text, text, k->cap);
    k->text[k->cap] = 0;
    k->cur = (int)wcslen(k->text);
}

int VKeysRowLen(const VKeys* k, int row)
{
    return row >= 0 && row < VKEYS_ROWS ? (int)wcslen((k->en ? kEn : kRu)[row].own) : 0;
}

static wchar_t Upper(wchar_t c)
{
    CharUpperBuff(&c, 1);
    return c;
}

// Shift as a PC's: a letter's capital; a sign gives its second (1: !, [: {) - and its own is then the second.
static bool Swapped(const VKeys* k, int row, int col)
{
    const KeyRow& r = (k->en ? kEn : kRu)[row];
    return k->shift && Upper(r.own[col]) == r.own[col] && r.second[col] != L' ';
}

wchar_t VKeysChar(const VKeys* k, int row, int col)
{
    const KeyRow& r = (k->en ? kEn : kRu)[row];
    if (Swapped(k, row, col))
        return r.second[col];
    return k->shift ? Upper(r.own[col]) : r.own[col];
}

int VKeysAlts(const VKeys* k, int row, int col, wchar_t* out)
{
    const KeyRow& r = (k->en ? kEn : kRu)[row];
    wchar_t two[2] = { Swapped(k, row, col) ? r.own[col] : r.second[col], r.third[col] };
    int n = 0;
    for (int i = 0; i < 2; i++)
        if (two[i] != L' ')
            out[n++] = two[i];
    return n;
}

int VKeysAltsFor(const VKeys* k, wchar_t shown, wchar_t* out)
{
    for (int r = 0; r < VKEYS_ROWS; r++)
        for (int c = 0; c < VKeysRowLen(k, r); c++)
            if (VKeysChar(k, r, c) == shown)
                return VKeysAlts(k, r, c, out);
    return 0;
}

int VKeysBubbleX(int keyX, int keyW, int n, int cellW, int screenW)
{
    int x = keyX + keyW / 2 - n * cellW / 2;
    return x < 0 ? 0 : x > screenW - n * cellW ? screenW - n * cellW : x;
}

int VKeysBubbleSel(int fingerX, int bubbleX, int cellW, int n)
{
    int i = fingerX < bubbleX ? 0 : (fingerX - bubbleX) / cellW;
    return i < n ? i : n - 1;
}

static int Cursor(const VKeys* k, int n)   // the cursor within the text
{
    return k->cur < 0 ? 0 : k->cur > n ? n : k->cur;
}

void VKeysType(VKeys* k, wchar_t ch)
{
    int n = (int)wcslen(k->text);
    if (n >= k->cap)
        return;
    if (k->shift) {   // one-shot, like a phone keyboard
        CharUpperBuff(&ch, 1);
        k->shift = false;
    }
    int c = Cursor(k, n);
    memmove(k->text + c + 1, k->text + c, (n - c + 1) * sizeof(wchar_t));
    k->text[c] = ch;
    k->cur = c + 1;
}

void VKeysBack(VKeys* k)
{
    int n = (int)wcslen(k->text);
    int c = Cursor(k, n);
    if (c > 0)
        memmove(k->text + c - 1, k->text + c, (n - c + 1) * sizeof(wchar_t));
    k->cur = c > 0 ? c - 1 : 0;
}

// The cursor to the nearest gap between the shown characters; at the field's ends one further (the field's layout
// keeps the cursor in sight: the view follows it).
void VKeysCursorAt(VKeys* k, int x, int fieldW)
{
    int n = (int)wcslen(k->text);
    if (x < 8 && k->view > 0) {
        k->cur = k->view - 1;
        return;
    }
    if (x > fieldW - 8 && k->view + k->visN < n) {
        k->cur = k->view + k->visN + 1;
        return;
    }
    int best = 0, bd = x < 0 ? -x : x;
    for (int i = 1; i <= k->visN; i++) {
        int d = x - k->dx[i - 1];
        d = d < 0 ? -d : d;
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    k->cur = k->view + best;
}

int VKeysLayout(VKeys* k, HDC dc, int fieldW)
{
    int n = (int)wcslen(k->text);
    k->cur = Cursor(k, n);
    if (k->view > k->cur)
        k->view = k->cur;
    for (;;) {
        SIZE sz;
        int fit = 0;
        if (!GetTextExtentExPoint(dc, k->text + k->view, n - k->view, fieldW, &fit, k->dx, &sz))
            fit = 0;
        k->visN = fit < 0 ? 0 : fit;
        if (k->cur - k->view <= k->visN || k->view >= k->cur)
            break;
        k->view++;   // the cursor beyond the field: one character on
    }
    int i = k->cur - k->view;
    return i > 0 ? k->dx[i - 1] : 0;
}
