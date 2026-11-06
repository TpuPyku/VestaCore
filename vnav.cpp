// vnav.cpp - VestaCore: see vnav.h.
#include "vnav.h"
#include "vhap.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const double kPi = 3.14159265358979323846;

// src into dst of cap characters, cut and closed
static void Copy(wchar_t* dst, const wchar_t* src, int cap)
{
    wcsncpy(dst, src, cap - 1);
    dst[cap - 1] = 0;
}

// A text as a field's value: what would end the field becomes a space
static void Clean(wchar_t* dst, const wchar_t* src, int cap)
{
    Copy(dst, src, cap);
    for (; *dst; dst++)
        if (*dst == L'\t' || *dst == L'\r' || *dst == L'\n')
            *dst = L' ';
}

void VNavFormat(const VNav& n, wchar_t* out, int cap)
{
    if (!n.on) {
        Copy(out, L"nav\ton=0", cap);
        return;
    }
    wchar_t dist[16], street[64], eta[8];
    Clean(dist, n.dist, 16);
    Clean(street, n.street, 64);
    Clean(eta, n.eta, 8);
    _snwprintf(out, cap, L"nav\ton=1\tturn=%d\tdist=%s\tstreet=%s\teta=%s", n.turn, dist, street, eta);
    out[cap - 1] = 0;
}

void VNavParse(const wchar_t* cmd, VNav* n)
{
    memset(n, 0, sizeof(*n));
    wchar_t v[16];
    if (wcsncmp(cmd, L"nav\t", 4) || !VhapField(cmd, L"on", v, 16) || v[0] != L'1')
        return;
    n->on = true;
    if (VhapField(cmd, L"turn", v, 16))
        n->turn = _wtoi(v);
    VhapField(cmd, L"dist", n->dist, 16);
    VhapField(cmd, L"street", n->street, 64);
    VhapField(cmd, L"eta", n->eta, 8);
}

// "Поверните направо: Тверская" -> "направо", "Тверская": the sign says the rest
void VNavSplit(const wchar_t* text, wchar_t* verb, wchar_t* street, int cap)
{
    static const wchar_t kTurn[] = L"Поверните ";
    const wchar_t* colon = wcsstr(text, L": ");
    if (!colon) {
        verb[0] = 0;
        Copy(street, text, cap);
        return;
    }
    Copy(street, colon + 2, cap);
    if (!wcsncmp(text, kTurn, 10))
        text += 10;
    int n = (int)(colon - text);
    if (n > cap - 1)
        n = cap - 1;
    wcsncpy(verb, text, n);
    verb[n] = 0;
}

// Drawn for a 76 px box; a smaller one: everything scaled (Q: the length times the box over 76)
void VNavDrawTurn(HDC dc, const RECT& r, int turn, COLORREF col)
{
    int q = r.right - r.left;
#define Q(v) ((v) * q / 76)
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2 + Q(4);
    HPEN pen = CreatePen(PS_SOLID, Q(7) < 3 ? 3 : Q(7), col);
    HBRUSH br = CreateSolidBrush(col);
    HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, br);
    if (turn == VNAV_FINISH) {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, cx - Q(20), cy - Q(24), cx + Q(20), cy + Q(16));
        SelectObject(dc, br);
        Ellipse(dc, cx - Q(6), cy - Q(10), cx + Q(6), cy + Q(2));
    } else {
        double a;
        int hx, hy;
        if (turn == VNAV_ROUND) {
            SelectObject(dc, GetStockObject(NULL_BRUSH));
            Ellipse(dc, cx - Q(14), cy - Q(8), cx + Q(14), cy + Q(20));
            POINT st[2] = { { cx, cy - Q(8) }, { cx, cy - Q(26) } };
            Polyline(dc, st, 2);
            SelectObject(dc, br);
            a = 0;
            hx = cx;
            hy = cy - Q(26);
        } else if (turn >= 170 || turn <= -170) {   // U-turn: up and back down on the turn's side
            int k = turn > 0 ? 1 : -1;
            POINT u[4] = { { cx - Q(12) * k, r.bottom - Q(10) }, { cx - Q(12) * k, r.top + Q(20) },
                           { cx + Q(12) * k, r.top + Q(20) }, { cx + Q(12) * k, cy + Q(4) } };
            Polyline(dc, u, 4);
            a = kPi;
            hx = cx + Q(12) * k;
            hy = cy + Q(4);
        } else {
            a = turn * kPi / 180.0;
            POINT st[3] = { { cx, r.bottom - Q(10) }, { cx, cy },
                            { cx + (int)(sin(a) * Q(22)), cy - (int)(cos(a) * Q(22)) } };
            Polyline(dc, st, 3);
            hx = st[2].x;
            hy = st[2].y;
        }
        // the head: a triangle pointing along a
        double c = cos(a), sn = sin(a);
        POINT h[3] = { { hx + (int)(sn * Q(14)), hy - (int)(c * Q(14)) },
                       { hx + (int)(c * Q(12) - sn * Q(4)), hy + (int)(sn * Q(12) + c * Q(4)) },
                       { hx - (int)(c * Q(12) + sn * Q(4)), hy - (int)(sn * Q(12) - c * Q(4)) } };
        HPEN thin = CreatePen(PS_SOLID, 1, col);
        SelectObject(dc, thin);
        Polygon(dc, h, 3);
        SelectObject(dc, pen);
        DeleteObject(thin);
    }
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
    DeleteObject(pen);
#undef Q
}
