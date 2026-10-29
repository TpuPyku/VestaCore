// vkeys.cpp - VestaCore: the on-screen keyboard's text and cursor (see vkeys.h; moved from VestaHome 2026-10-29).
#include "vkeys.h"
#include <string.h>

static const wchar_t* kRu[VKEYS_ROWS] = { L"1234567890-_", L"йцукенгшщзхъ", L"фывапролджэё", L"ячсмитьбю.,!" };
static const wchar_t* kEn[VKEYS_ROWS] = { L"1234567890-_", L"qwertyuiop()", L"asdfghjkl:;\"", L"zxcvbnm.,/\\=" };
static const wchar_t  kSym[] = L"!@#$%&*+?|~'";   // the digits' row with Shift (the configs: # @ & ? % + |)

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
    return row >= 0 && row < VKEYS_ROWS ? (int)wcslen((k->en ? kEn : kRu)[row]) : 0;
}

wchar_t VKeysChar(const VKeys* k, int row, int col)
{
    wchar_t ch[2] = { row == 0 && k->shift ? kSym[col] : (k->en ? kEn : kRu)[row][col], 0 };
    if (k->shift)
        CharUpperBuff(ch, 1);
    return ch[0];
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
