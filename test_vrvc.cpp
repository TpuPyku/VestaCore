// test_vrvc.cpp - which video input the camera is looked for on (vrvc.h RvcInputs), on the PC.
#include <windows.h>
#include <stdio.h>
#include "vrvc.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}


// The guide lines' styles (2026-11-25, the user: one of four in "Car"): segments from the corridor's points, near to far.
static int s_L[54], s_R[54];
static void Corridor(int n)   // a straight corridor: x 100..300 near (y 470) narrowing to 300..500 far
{
    for (int k = 0; k < n; k++) {
        s_L[k * 2] = 100 + k * 8;
        s_L[k * 2 + 1] = 470 - k * 10;
        s_R[k * 2] = 700 - k * 8;
        s_R[k * 2 + 1] = 470 - k * 10;
    }
}

static int Count(const RvcSeg* s, int n, COLORREF c)
{
    int k = 0;
    for (int i = 0; i < n; i++)
        k += s[i].c == c;
    return k;
}

static bool Across(const RvcSeg& s)   // a bar from one side to the other
{
    return s.x1 - s.x0 > 100 || s.x0 - s.x1 > 100;
}

static void TestStyles()
{
    RvcSeg s[200];
    Corridor(27);
    int n = RvcStaticSegs(s_L, s_R, 27, true, RVC_LINES_CLASSIC, s, 200);
    int bars = 0;
    for (int i = 0; i < n; i++)
        bars += Across(s[i]);
    Check(n == 26 * 2 + 4 && bars == 4, "classic: both sides and 4 bars across (the zones' ends)");
    Check(Count(s, n, RGB(255, 0, 0)) && Count(s, n, RGB(243, 255, 0)) && Count(s, n, RGB(32, 255, 0)),
          "classic: red, yellow, green zones");
    Check(s[0].w == 4 && !s[0].outline, "classic: thin, no outline");
    n = RvcStaticSegs(s_L, s_R, 27, false, RVC_LINES_CLASSIC, s, 200);
    Check(Count(s, n, RGB(49, 130, 172)) == n, "no steering angle: all blue, as the shell's");

    n = RvcStaticSegs(s_L, s_R, 27, true, RVC_LINES_STOCK, s, 200);
    bars = 0;
    int ticks = 0;
    for (int i = 0; i < n; i++) {
        bars += Across(s[i]);
        ticks += s[i].y0 == s[i].y1 && !Across(s[i]);
    }
    Check(bars == 0 && ticks == 6, "stock: no bars across, a short tick inward at each of the 3 pieces' near ends");
    Check(Count(s, n, RGB(49, 130, 172)) && Count(s, n, RGB(255, 0, 0)) && !Count(s, n, RGB(243, 255, 0)),
          "stock: blue pieces, the near one red");
    Check(n < 26 * 2 + 6, "stock: gaps between the pieces");
    Check(s[0].w == 8, "stock: thick");

    n = RvcStaticSegs(s_L, s_R, 27, true, RVC_LINES_MODERN, s, 200);
    bars = ticks = 0;
    for (int i = 0; i < n; i++) {
        bars += Across(s[i]);
        ticks += s[i].y0 == s[i].y1 && !Across(s[i]);
    }
    Check(bars == 0 && ticks == 6, "modern: open, ticks at the zones' ends instead of bars");
    Check(s[0].outline && s[0].w >= 6, "modern: thick with a dark outline");
    Check(Count(s, n, RGB(32, 255, 0)) > 0, "modern: the zones' colours");
    Check(RvcStaticSegs(s_L, s_R, 27, true, RVC_LINES_TRACKS, s, 200) == n, "tracks: the static lines as modern");
    Check(RvcStaticSegs(s_L, s_R, 27, true, 9, s, 200) == RvcStaticSegs(s_L, s_R, 27, true, RVC_LINES_CLASSIC, s, 200),
          "a stray style: classic");
    Check(RvcStaticSegs(s_L, s_R, 27, true, RVC_LINES_CLASSIC, s, 10) == 10, "no more than the room");
    s_L[0] = -5;
    n = RvcStaticSegs(s_L, s_R, 27, true, RVC_LINES_CLASSIC, s, 200);
    Check(n == 26 * 2 + 4 && s[0].x0 == 0, "an end off the screen: cut at its edge (was: left out)");
    // HU photo 2026-11-26: the right line's near piece missing - its near point past x 800, the piece dropped
    Corridor(27);
    s_R[0] = 860;
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_CLASSIC, s, 200);
    Check(n == 52 && s[1].x0 == 799 && s[1].y0 == 466 && s[1].x1 == 692 && s[1].y1 == 460,
          "past the right edge: cut at x 799 on the line");
    s_R[1] = 540;
    s_R[0] = 700;
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_CLASSIC, s, 200);
    Check(n == 52 && s[1].y0 == 479 && s[1].x0 == 694, "below the bottom: cut at y 479");
    s_R[0] = 900;
    s_R[2] = 850;
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_CLASSIC, s, 200);
    Check(n == 51 && s[1].x0 == s_L[2], "all of it off the screen: left out");
    Corridor(27);
    s_L[0] = -40;
    s_L[2] = -10;
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_CLASSIC, s, 200);
    Check(n == 51 && s[0].x0 == s_R[0], "all of it left of the screen: left out");
    s_L[0] = -5;
    s_L[2] = -5;
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_CLASSIC, s, 200);
    Check(n == 51 && s[0].x0 == s_R[0], "upright left of the screen: left out");
    Corridor(27);
    s_L[0] = 900;
    s_L[2] = 900;
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_CLASSIC, s, 200);
    Check(n == 51 && s[0].x0 == s_R[0], "upright right of the screen: left out");
    Corridor(27);
    s_L[1] = -30;
    s_L[3] = -20;
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_CLASSIC, s, 200);
    Check(n == 51 && s[0].x0 == s_R[0], "above the screen: left out");
    Corridor(27);
    s_L[1] = 0;
    s_L[3] = 0;
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_CLASSIC, s, 200);
    Check(n == 52 && s[0].y0 == 0 && s[0].y1 == 0 && s[0].x1 == 108, "along the top edge: kept");
    Corridor(27);

    Corridor(27);
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_CLASSIC, s, 200);
    Check(n == 52 && s[0].c == RGB(255, 0, 0) && s[0].w == 8 && s[n - 1].c == RGB(243, 255, 0) && s[n - 1].w == 5,
          "trajectory classic: near red and thick, then yellow");
    n = RvcTraceSegs(s_L, s_R, 27, 400, RVC_LINES_CLASSIC, s, 200);
    Check(Count(s, n, RGB(255, 0, 0)) == n, "trajectory: the wheel near its end - all red");
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_MODERN, s, 200);
    Check(n == 52 && s[0].outline, "trajectory modern: outlined");
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_TRACKS, s, 200);
    Check(n == 104 && s[0].x0 != s[1].x0 && s[0].w < 6, "trajectory tracks: each line two thin strips");
}

static void TestExact()   // where each look puts its lines (the corridor of Corridor: y 470 - 10 k)
{
    RvcSeg s[200];
    Corridor(27);
    int n = RvcStaticSegs(s_L, s_R, 27, true, RVC_LINES_CLASSIC, s, 200);
    int ys[4], b = 0;
    for (int i = 0; i < n && b < 4; i++)
        if (Across(s[i])) {
            Check(s[i].w == 3 && !s[i].outline && s[i].y0 == s[i].y1, "classic: a bar 3 px, level, no outline");
            ys[b++] = s[i].y0;
        }
    Check(b == 4 && ys[0] == 460 && ys[1] == 430 && ys[2] == 340 && ys[3] == 210,
          "classic: bars at points 1, 4 (near), 13 (middle), 26 (far)");
    n = RvcStaticSegs(s_L, s_R, 27, true, RVC_LINES_STOCK, s, 200);
    Check(n == 24 * 2 + 6, "stock: 24 segments a side (gaps before points 4 and 13), 6 ticks");
    Check(s[4].c == RGB(255, 0, 0) && s[4].y0 == 450 && s[6].c == RGB(49, 130, 172) && s[6].y0 == 430 &&
          s[6].w == 8 && !s[6].outline, "stock: the near piece red to point 3, the next starts at point 4 blue");
    Check(s[48].x0 == 100 && s[48].x1 == 175 && s[48].y0 == 470 && s[48].c == RGB(255, 0, 0) && s[48].w == 6 &&
          s[49].x0 == 700 && s[49].x1 == 625, "stock: the near piece's ticks, an eighth of the width inward, red");
    Check(s[50].y0 == 430 && s[52].y0 == 340 && s[50].c == RGB(49, 130, 172), "stock: ticks at points 4 and 13, blue");
    Check(!s[48].outline && !s[50].outline, "stock: ticks without an outline");
    bool gap12 = true, from13 = false;
    for (int i = 0; i < 48; i++) {
        gap12 &= s[i].y0 != 350;   // (point 12: its segment to 13 left out)
        from13 |= s[i].y0 == 340;
    }
    Check(gap12 && from13, "stock: the gap before point 13");
    n = RvcStaticSegs(s_L, s_R, 27, true, RVC_LINES_CLASSIC, s, 200);
    COLORREF z[27];   // the left side's colour by segment
    for (int i = 0; i < n; i++)
        if (!Across(s[i]) && s[i].x0 < 400)
            z[(470 - s[i].y0) / 10] = s[i].c;
    Check(z[3] == RGB(255, 0, 0) && z[4] == RGB(243, 255, 0) && z[12] == RGB(243, 255, 0) && z[13] == RGB(32, 255, 0),
          "classic: red to segment 3, yellow 4-12, green from 13");
    n = RvcStaticSegs(s_L, s_R, 27, true, RVC_LINES_MODERN, s, 200);
    Check(n == 26 * 2 + 6 && s[0].w == 7 && s[0].outline, "modern: 26 a side, 7 px outlined, 6 ticks");
    Check(s[52].y0 == 430 && s[52].x1 - s[52].x0 == 67 && s[52].c == RGB(255, 0, 0) && s[52].w == 6 && s[52].outline,
          "modern: the near zone's tick at point 4, red");
    Check(s[54].y0 == 340 && s[54].c == RGB(243, 255, 0) && s[56].y0 == 210 && s[56].c == RGB(32, 255, 0),
          "modern: the middle's at 13 yellow, the far end's at 26 green");
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_TRACKS, s, 200);
    Check(s[0].x0 == 97 && s[1].x0 == 103 && s[0].x1 == 105 && s[0].y0 == 470 && s[0].y1 == 460,
          "tracks: the strips 3 px either side of the path");
    Check(s[0].w == 3 && s[0].outline && s[2].x0 == 697, "tracks: 3 px outlined, then the right wheel");
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_CLASSIC, s, 200);
    Check(s[6].c == RGB(255, 0, 0) && s[6].w == 8 && s[8].c == RGB(243, 255, 0) && s[8].w == 5 && !s[8].outline,
          "trajectory: segments 0-3 near (red, 8), from 4 yellow 5");
    n = RvcTraceSegs(s_L, s_R, 27, 341, RVC_LINES_CLASSIC, s, 200);
    Check(s[n - 1].c == RGB(255, 0, 0), "trajectory: 341 degrees - red to the end");
    n = RvcTraceSegs(s_L, s_R, 27, 340, RVC_LINES_CLASSIC, s, 200);
    Check(s[n - 1].c == RGB(243, 255, 0), "trajectory: 340 - still yellow far");
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_MODERN, s, 200);
    Check(s[8].w == 6 && s[0].w == 8, "trajectory modern: 6 px far, 8 near");
}

static bool Same(const RvcSeg* a, const RvcSeg* b, int n)   // field by field (the struct's padding is garbage)
{
    for (int i = 0; i < n; i++)
        if (a[i].x0 != b[i].x0 || a[i].y0 != b[i].y0 || a[i].x1 != b[i].x1 || a[i].y1 != b[i].y1 || a[i].c != b[i].c ||
            a[i].w != b[i].w || a[i].outline != b[i].outline)
            return false;
    return true;
}

static void TestTraceOnly()   // only the trajectory (2026-11-26, the user: a look without the static lines)
{
    RvcSeg s[200], m[200];
    Corridor(27);
    Check(RvcStaticSegs(s_L, s_R, 27, true, RVC_LINES_TRACE, s, 200) == 0, "trace only: no static lines");
    int n = RvcStaticSegs(s_L, s_R, 27, false, RVC_LINES_TRACE, s, 200);
    int k = RvcStaticSegs(s_L, s_R, 27, false, RVC_LINES_MODERN, m, 200);
    Check(n == k && n > 0 && Same(s, m, n), "trace only, no angle: modern's blue corridor");
    n = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_TRACE, s, 200);
    k = RvcTraceSegs(s_L, s_R, 27, 100, RVC_LINES_MODERN, m, 200);
    Check(n == k && Same(s, m, n), "trace only: the trajectory as modern's");
    Check(RvcTraceShown(0, true, RVC_LINES_TRACE), "trace only: the wheel straight - the trajectory still");
    Check(!RvcTraceShown(30, false, RVC_LINES_TRACE), "no angle: no trajectory");
    Check(!RvcTraceShown(5, true, RVC_LINES_CLASSIC) && !RvcTraceShown(-5, true, RVC_LINES_MODERN),
          "the other looks: within 5 degrees none");
    Check(RvcTraceShown(6, true, RVC_LINES_CLASSIC) && RvcTraceShown(-6, true, RVC_LINES_TRACKS), "past 5: shown");
}

static void TestDraw()   // the segments drawn: an outlined one dark around, the plain one without
{
    HDC dc = CreateCompatibleDC(NULL);
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = 100;
    bi.bmiHeader.biHeight = 100;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ ob = SelectObject(dc, bmp);
    RECT rc = { 0, 0, 100, 100 };
    FillRect(dc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    RvcSeg s[2] = { { 10, 20, 90, 20, RGB(255, 0, 0), 4, true }, { 10, 70, 90, 70, RGB(0, 0, 255), 4, false } };
    RvcDrawSegs(dc, s, 2);
    Check(GetPixel(dc, 50, 20) == RGB(255, 0, 0) && GetPixel(dc, 50, 70) == RGB(0, 0, 255), "draw: the lines' colours");
    Check(GetPixel(dc, 50, 23) == RGB(16, 16, 16), "draw: the outlined one dark beside it");
    Check(GetPixel(dc, 50, 73) == RGB(255, 255, 255), "draw: the plain one - nothing beside it");
    Check(GetPixel(dc, 50, 26) == RGB(255, 255, 255) && GetPixel(dc, 50, 14) == RGB(255, 255, 255),
          "draw: the outline 2 px each side, no more");
    SelectObject(dc, ob);
    DeleteObject(bmp);
    DeleteDC(dc);
}

int main()
{
    int in[2];
    Check(RvcInputs(-1, in) == 2 && in[0] == 0 && in[1] == 1, "nothing found yet: the rear camera's input 0, then 1");
    Check(RvcInputs(0, in) == 2 && in[0] == 0 && in[1] == 1, "found on 0: 0 first");
    Check(RvcInputs(1, in) == 2 && in[0] == 1 && in[1] == 0,
          "found on 1 (the shell's camera type 3 - a 4125 Vesta 2021, 2026-11-23): 1 first, then 0");
    Check(RvcInputs(7, in) == 2 && in[0] == 0 && in[1] == 1, "a stray number: as nothing found");
    TestStyles();
    TestExact();
    TestTraceOnly();
    TestDraw();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
