// test_vtouch.cpp - vtouch.cpp on synthetic traces of a resistive panel: a still finger with noise, steady drags,
// single far points, strays at the press and the release, quick flicks. Exit code: the failed checks.
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include "vtouch.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static unsigned s_seed;
static int Noise(int n)   // -n..n, the same every run
{
    s_seed = s_seed * 1103515245 + 12345;
    return (int)((s_seed >> 16) % (2 * n + 1)) - n;
}

// A finger held still at (300, 200) for ms with +-n px of the panel's wander, a point every 10 ms; the most the
// state reached (VT_DRAG once a drag)
static int Still(VTouch* t, int n, int ms, int spikeEvery)
{
    int most = VT_PRESS;
    VTouchDown(t, 1000, 300 + Noise(n), 200 + Noise(n));
    for (int i = 1; i * 10 <= ms; i++) {
        int x = 300 + Noise(n), y = 200 + Noise(n);
        if (spikeEvery && i % spikeEvery == 0)
            x += 90;   // a single far point
        int s = VTouchMove(t, 1000 + i * 10, x, y);
        most = s > most ? s : most;
    }
    return most;
}

static void TestTap()
{
    VTouch t;
    double vx, vy;
    s_seed = 1;
    Check(Still(&t, 6, 600, 0) == VT_PRESS, "a still finger with +-6 px of wander stays a press");
    Check(!VTouchUp(&t, 1610, &vx, &vy), "  and is released as a tap");
    Check(abs(t.px - 300) <= 6 && abs(t.py - 200) <= 6, "  at the finger's place (within its wander)");
    Check(Still(&t, 10, 600, 0) == VT_PRESS, "+-10 px of wander: still a press");
    Check(Still(&t, 6, 600, 7) == VT_PRESS, "single far points (90 px) meanwhile: still a press");
    Check(abs(t.x - 300) <= 4, "  the point shown does not go to them");
    VTouchDown(&t, 1000, 300, 200);
    Check(!VTouchUp(&t, 1050, &vx, &vy) && vx == 0 && vy == 0, "a tap with no moves: a press, no speed");
}

static void TestSettle()
{
    VTouch t;
    VTouchDown(&t, 1000, 300, 200);   // the panel's point wanders 30 px slowly in the first 90 ms
    int s = VT_PRESS;
    for (int i = 1; i <= 9; i++)
        s = VTouchMove(&t, 1000 + i * 10, 300 + i * 3, 200);
    for (int i = 10; i <= 40; i++)
        s = VTouchMove(&t, 1000 + i * 10, 327, 200);
    Check(s == VT_PRESS, "a wander of 27 px within the settling: still a press");
    Check(abs(t.px - 327) <= 4, "  its point followed the finger while it settled");
    s = VT_PRESS;
    for (int i = 41; i <= 50 && s == VT_PRESS; i++)
        s = VTouchMove(&t, 1000 + i * 10, 327 + (i - 40) * 4, 200);
    Check(s == VT_DRAG, "then past 20 px from the settled point: a drag");
    Check(abs(t.dx) <= 6 && t.dy == 0, "  starting from where it began (no jump)");
}

static void TestFlick()
{
    VTouch t;
    VTouchDown(&t, 1000, 300, 200);
    int s = VT_PRESS, at = 0;
    for (int i = 1; i <= 8 && s == VT_PRESS; i++) {
        s = VTouchMove(&t, 1000 + i * 10, 300 + i * 12, 200);
        at = i * 10;
    }
    Check(s == VT_DRAG && at < kVTouchSettleMs, "a quick flick (1.2 px/ms) is a drag before the settling ends");
}

// A steady drag east at v px/ms, a point every step ms, from (100, 300); the shown point's largest lag behind the
// finger once the drag began; *from: the shown point where it began (the map's offset is measured from it)
static double Drag(VTouch* t, double v, int step, int ms, int* from)
{
    VTouchDown(t, 1000, 100, 300);
    double lag = 0;
    *from = -1;
    for (int i = 1; i * step <= ms; i++) {
        int x = 100 + (int)(v * i * step);
        if (VTouchMove(t, 1000 + i * step, x, 300) != VT_DRAG)
            continue;
        if (*from < 0)
            *from = t->x - t->dx;
        double l = x - t->x;
        lag = l > lag ? l : lag;
    }
    return lag;
}

static void TestDrag()
{
    VTouch t;
    int from;
    double lag = Drag(&t, 1.0, 10, 400, &from);
    Check(t.state == VT_DRAG, "a steady drag (1 px/ms) is a drag");
    Check(lag <= 2 * 10, "  the point shown within 2 points of the finger (20 px at 1 px/ms)");
    Check(t.dx == t.x - from && t.dy == 0, "  the offset: the shown point's way since the drag began");
    for (int i = 41; i <= 60; i++)   // the finger stops: the point shown catches up
        VTouchMove(&t, 1000 + i * 10, 500, 300);
    Check(abs(t.x - 500) <= 1, "  stopped: the point shown catches the finger up");
    lag = Drag(&t, 0.3, 15, 600, &from);
    Check(lag <= 2.5 * 15 * 0.3, "a slow drag (0.3 px/ms, a point every 15 ms) within 2.5 points");
}

static void TestSpike()
{
    VTouch t;
    VTouchDown(&t, 1000, 100, 300);
    int most = 0;
    for (int i = 1; i <= 40; i++) {
        int y = 300 + (i == 25 ? 150 : 0) + (i == 32 ? -40 : 0);   // a far single point, a nearer one
        VTouchMove(&t, 1000 + i * 10, 100 + i * 10, y);
        int d = abs(t.y - 300);
        most = d > most ? d : most;
    }
    Check(t.state == VT_DRAG && most <= 1, "single far points across a drag: the shown point stays on the line");
    VTouchDown(&t, 1000, 100, 300);
    most = 0;
    for (int i = 1; i <= 40; i++) {
        int y = 300 + (i == 25 ? 200 : i == 26 ? -200 : 0);   // two far points in a row, far from each other too
        VTouchMove(&t, 1000 + i * 10, 100 + i * 10, y);
        int d = abs(t.y - 300);
        most = d > most ? d : most;
    }
    Check(t.state == VT_DRAG && most <= 1, "two far points apart in a row: both dropped");
}

static void TestPressStray()
{
    VTouch t;
    VTouchDown(&t, 1000, 700, 50);   // the press's first point is a stray, the finger at (300, 200)
    for (int i = 1; i <= 30; i++)
        VTouchMove(&t, 1000 + i * 10, 300, 200);
    double vx, vy;
    Check(t.state == VT_PRESS && !VTouchUp(&t, 1310, &vx, &vy), "a stray press point: still a tap");
    Check(abs(t.px - 300) <= 1 && abs(t.py - 200) <= 1, "  at the finger's place");
    VTouchDown(&t, 1000, 300, 450);   // a stray press point straight below the finger
    for (int i = 1; i <= 30; i++)
        VTouchMove(&t, 1000 + i * 10, 300, 200);
    Check(t.state == VT_PRESS && abs(t.py - 200) <= 1, "a stray press point straight below: still a tap, its place");
}

// A swipe east at 1 px/ms (a point every 10 ms), the release at ms + 5; stray: one more point before the release
static bool Swipe(VTouch* t, int ms, int strayX, int strayY, double* vx, double* vy)
{
    VTouchDown(t, 1000, 100, 300);
    for (int i = 1; i * 10 <= ms; i++)
        VTouchMove(t, 1000 + i * 10, 100 + i * 10, 300);
    if (strayX)
        VTouchMove(t, 1000 + ms + 3, strayX, strayY);
    return VTouchUp(t, 1000 + ms + 5, vx, vy);
}

static void TestRelease()
{
    VTouch t;
    double vx, vy, sx, sy;
    Check(Swipe(&t, 300, 0, 0, &vx, &vy) && fabs(vx - 1) < 0.01 && fabs(vy) < 0.01, "a swipe's speed: 1 px/ms east");
    Check(Swipe(&t, 300, 160, 420, &sx, &sy) && fabs(sx - vx) < 0.01 && fabs(sy - vy) < 0.01,
          "a far stray point before the release: the same speed");
    Check(Swipe(&t, 300, 395, 305, &sx, &sy) && fabs(sx - vx) < 0.01 && fabs(sy - vy) < 0.01,
          "a near stray point before the release: the same speed");
    Check(Swipe(&t, 300, 0, 0, &vx, &vy) && t.x >= 360 && t.x <= 400, "the release's place: the last shown point");
    VTouchDown(&t, 1000, 100, 300);   // a swipe, then the finger held still (no points) before the release
    for (int i = 1; i <= 30; i++)
        VTouchMove(&t, 1000 + i * 10, 100 + i * 10, 300);
    Check(VTouchUp(&t, 1400, &vx, &vy) && vx == 0 && vy == 0, "held still 100 ms before the release: no speed");
    VTouchDown(&t, 1000, 100, 300);   // slow, then quick: the last 100 ms' speed
    int x = 100;
    for (int i = 1; i <= 20; i++)
        VTouchMove(&t, 1000 + i * 10, x += 1, 300);
    for (int i = 21; i <= 40; i++)
        VTouchMove(&t, 1000 + i * 10, x += 20, 300);
    Check(VTouchUp(&t, 1405, &vx, &vy) && fabs(vx - 2) < 0.05, "slow, then quick: the speed is the last 100 ms' (2)");
    VTouchDown(&t, 1000, 100, 300);   // the finger stands on (the panel's points keep coming), then lifts
    for (int i = 1; i <= 30; i++)
        VTouchMove(&t, 1000 + i * 10, 100 + (i < 15 ? i * 10 : 140), 300);
    Check(VTouchUp(&t, 1305, &vx, &vy) && fabs(vx) < 0.05, "the finger stopped with its points coming: no speed");
}

static void TestJump()
{
    VTouch t;   // a drag, then the thread busy 200 ms (its points merged): the finger really is far
    VTouchDown(&t, 1000, 100, 300);
    for (int i = 1; i <= 20; i++)
        VTouchMove(&t, 1000 + i * 10, 100 + i * 10, 300);
    VTouchMove(&t, 1400, 500, 300);
    VTouchMove(&t, 1410, 510, 300);
    for (int i = 0; i < 20; i++)
        VTouchMove(&t, 1420 + i * 10, 520, 300);
    Check(t.state == VT_DRAG && abs(t.x - 520) <= 1, "a far point after a pause, confirmed by the next: taken");
    VTouchDown(&t, 1000, 100, 300);   // a drag, then 300 px in 10 ms (quicker than a finger) and on from there
    for (int i = 1; i <= 20; i++)
        VTouchMove(&t, 1000 + i * 10, 100 + i * 10, 300);
    for (int i = 21; i <= 40; i++)
        VTouchMove(&t, 1000 + i * 10, 600 + (i - 21), 300);
    Check(t.state == VT_DRAG && abs(t.x - 619) <= 4, "a far jump within a drag, confirmed by the next: taken (on, slowly: smoothed)");
    double vx, vy;
    VTouchDown(&t, 1000, 100, 300);   // a flick of two points: a drag; too few points for a speed
    VTouchMove(&t, 1010, 160, 300);
    Check(VTouchMove(&t, 1020, 220, 300) == VT_DRAG, "a flick of two points (60 px each): a drag");
    Check(VTouchUp(&t, 1025, &vx, &vy) && vx == 0 && vy == 0, "  released: no speed from the press's point alone");
}

// A drag east at 1 px/ms from (100, 300) for ms (a point every 10 ms); spikeAt: that point's own place instead
static void DragEast(VTouch* t, int ms, int spikeAt, int sx, int sy, int* most)
{
    VTouchDown(t, 1000, 100, 300);
    *most = 0;
    for (int i = 1; i * 10 <= ms; i++) {
        bool s = i == spikeAt;
        VTouchMove(t, 1000 + i * 10, s ? sx : 100 + i * 10, s ? sy : 300);
        int d = abs(t->y - 300) + (t->state == VT_DRAG && t->x < 100 + (i - 3) * 10 ? 100 + (i - 3) * 10 - t->x : 0);
        *most = d > *most ? d : *most;
    }
}

static void TestEdges()
{
    VTouch t;
    double vx, vy;
    VTouchDown(&t, 1000, 300, 200);   // no wander: the point shown is the finger's, to the px
    for (int i = 1; i <= 5; i++)
        VTouchMove(&t, 1000 + i * 10, 300, 200);
    Check(t.x == 300 && t.y == 200 && t.px == 300 && t.py == 200, "a press without wander: its point, to the px");
    VTouchDown(&t, 1000, 300, 200);   // a step of 6 px right after the press: smoothed (a still finger)
    VTouchMove(&t, 1010, 306, 200);
    VTouchMove(&t, 1020, 306, 200);
    Check(t.x > 300 && t.x <= 302, "a 6 px step while the finger is still: the point shown goes 1-2 px at first");
    VTouchDown(&t, 1000, 300, 200);   // exactly 20 px after the settling: a press; 21: a drag
    for (int i = 1; i <= 12; i++)
        VTouchMove(&t, 1000 + i * 10, 300, 200);
    for (int i = 13; i <= 16; i++)
        VTouchMove(&t, 1000 + i * 10, 320, 200);
    Check(t.state == VT_PRESS, "20 px from the settled point: still a press");
    for (int i = 17; i <= 18; i++)
        VTouchMove(&t, 1000 + i * 10, 321, 200);
    Check(t.state == VT_DRAG && t.dx == 0, "21 px: a drag, from there");
    VTouchDown(&t, 1000, 300, 200);   // 30 px just as the settling ends (100 ms): a drag, not the press's wander
    VTouchMove(&t, 1095, 330, 200);
    Check(VTouchMove(&t, 1100, 330, 200) == VT_DRAG, "30 px at 100 ms: the settling is over - a drag");
    VTouchDown(&t, 1000, 100, 300);   // a drag down-right at 45 degrees
    for (int i = 1; i <= 30; i++)
        VTouchMove(&t, 1000 + i * 10, 100 + i * 8, 300 + i * 8);
    Check(t.state == VT_DRAG && t.dy > 100 && abs(t.dx - t.dy) <= 1, "a diagonal drag: both ways alike");
    VTouchDown(&t, 1000, 100, 300);   // points at the same ms (GetTickCount's steps): as if 1 ms apart
    for (int i = 1; i <= 10; i++)
        VTouchMove(&t, 1000 + i * 10, 100 + i * 10, 300);
    VTouch u = t;
    VTouchMove(&t, 1100, 205, 300);
    VTouchMove(&u, 1101, 205, 300);
    Check(t.x == u.x && t.x > 150 && t.x <= 205, "points at the same ms: as 1 ms apart");
    Swipe(&t, 300, 0, 0, &vx, &vy);   // a new press after a drag: its own points, no offset
    VTouchDown(&t, 2000, 600, 100);
    Check(t.dx == 0 && t.dy == 0, "a new press: no offset left from the drag before");
    VTouchMove(&t, 2010, 606, 100);
    Check(t.x == 600 && t.y == 100, "  its first points are its own (a single step: the median's)");
}

static void TestStrays()
{
    VTouch t;
    int most;
    DragEast(&t, 400, 25, 260, 300, &most);   // a point 90 px back (near enough to be taken): the median's
    Check(t.state == VT_DRAG && most <= 1, "a single point back across a drag: the shown point does not go back");
    DragEast(&t, 400, 25, 350, 400, &most);   // a point 100 px aside, near enough to be taken
    Check(t.state == VT_DRAG && most <= 1, "a single near point aside: the median's");
    VTouchDown(&t, 1000, 100, 300);   // two single far points at the same place 100 ms apart: both dropped
    most = 0;
    for (int i = 1; i <= 40; i++) {
        bool s = i == 20 || i == 30;
        VTouchMove(&t, 1000 + i * 10, s ? 700 : 100 + i * 10, s ? 50 : 300);
        most = abs(t.y - 300) > most ? abs(t.y - 300) : most;
    }
    Check(most <= 1, "two single far points at one place, 100 ms apart: both dropped");
    VTouchDown(&t, 1000, 100, 300);   // two far points in a row on one side, far from each other: both dropped
    most = 0;
    for (int i = 1; i <= 40; i++) {
        int x = 100 + i * 10, y = 300;
        if (i >= 20 && i <= 22) {   // (three: the second and the third would be taken once one waits)
            x += (i - 20) * 250;
            y = 500;
        }
        VTouchMove(&t, 1000 + i * 10, x, y);
        most = abs(t.y - 300) > most ? abs(t.y - 300) : most;
    }
    Check(most <= 1, "two far points apart on one side: both dropped");
    VTouchDown(&t, 1000, 300, 200);   // a still press, a far stray, then the finger 30 px on: taken at once
    for (int i = 1; i <= 15; i++)
        VTouchMove(&t, 1000 + i * 10, 300, 200);
    VTouchMove(&t, 1160, 700, 450);
    VTouchMove(&t, 1170, 330, 200);
    VTouchMove(&t, 1180, 330, 200);
    Check(t.x > 310, "the point after a dropped stray is taken");
    VTouchDown(&t, 1000, 300, 200);   // after the settling a quick far move (confirmed): a drag, not a new press
    VTouchMove(&t, 1050, 300, 200);
    VTouchMove(&t, 1100, 800, 200);
    VTouchMove(&t, 1105, 805, 200);
    VTouchMove(&t, 1110, 810, 200);
    Check(t.state == VT_DRAG, "a confirmed far move once the press settled: a drag");
    VTouchDown(&t, 1000, 100, 300);   // a drag, a confirmed jump and one point after it: the jump shown
    for (int i = 1; i <= 20; i++)
        VTouchMove(&t, 1000 + i * 10, 100 + i * 10, 300);
    VTouchMove(&t, 1210, 600, 300);
    VTouchMove(&t, 1220, 650, 300);
    Check(t.x > 400, "a confirmed jump and one point after it: shown");
}

static void TestSpeed()
{
    VTouch t;
    double vx, vy, sx, sy;
    Swipe(&t, 300, 0, 0, &vx, &vy);
    VTouchDown(&t, 1000, 100, 300);   // a near stray at the same ms as the last point: left out too
    for (int i = 1; i <= 30; i++)
        VTouchMove(&t, 1000 + i * 10, 100 + i * 10, 300);
    VTouchMove(&t, 1300, 395, 305);
    Check(VTouchUp(&t, 1305, &sx, &sy) && fabs(sx - vx) < 0.01 && fabs(sy - vy) < 0.01,
          "a near stray at the last point's ms: the same speed");
    VTouchDown(&t, 1000, 100, 300);   // two points at one ms just before the release: the speed kept
    for (int i = 1; i <= 29; i++)
        VTouchMove(&t, 1000 + i * 10, 100 + i * 10, 300);
    VTouchMove(&t, 1290, 395, 300);
    VTouchMove(&t, 1300, 400, 300);
    Check(VTouchUp(&t, 1305, &sx, &sy) && sx > 0.9, "two points at one ms before the release: the speed kept");
    VTouchDown(&t, 1000, 100, 300);   // 1 px/ms at uneven times: the clean points keep theirs
    DWORD now = 1000;
    s_seed = 7;
    for (int i = 0; i < 36; i++) {
        now += 12 + Noise(7);
        VTouchMove(&t, now, 100 + (int)(now - 1000), 300);
    }
    Check(VTouchUp(&t, now + 5, &vx, &vy) && fabs(vx - 1) < 0.01, "points at uneven times: the speed still 1 px/ms");
    VTouchDown(&t, 1000, 100, 300);   // the last confirmed point quicker: counted
    int x = 100;
    for (int i = 1; i <= 30; i++)
        VTouchMove(&t, 1000 + i * 10, x += 10, 300);
    for (int i = 31; i <= 33; i++)
        VTouchMove(&t, 1000 + i * 10, x += 30, 300);
    Check(VTouchUp(&t, 1335, &vx, &vy) && vx > 1.2, "the newest confirmed point counts in the speed");
    VTouchDown(&t, 1000, 100, 300);   // a point every 25 ms: the speed over 100 ms exactly, back to the still part
    static const int xs[15] = { 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 150, 175, 200, 225, 250 };
    for (int i = 0; i < 15; i++)
        VTouchMove(&t, 1025 + i * 25, xs[i], 300);
    Check(VTouchUp(&t, 1380, &vx, &vy) && fabs(vx - 1) < 0.01, "the speed's 100 ms: the point 100 ms back counts");
    VTouchDown(&t, 1000, 100, 300);   // a short flick quicker at its start: its first clean point counts
    static const int fx[5] = { 140, 160, 180, 200, 220 };
    for (int i = 0; i < 5; i++)
        VTouchMove(&t, 1010 + i * 10, fx[i], 300);
    Check(VTouchUp(&t, 1055, &vx, &vy) && fabs(vx - 80.0 / 30) < 0.01, "a short flick: from its first clean point");
    VTouchDown(&t, 1000, 100, 300);   // a point every 60 ms (1 px/ms): two clean points within the 100 ms
    for (int i = 1; i <= 8; i++)
        VTouchMove(&t, 1000 + i * 60, 100 + i * 60, 300);
    Check(VTouchUp(&t, 1490, &vx, &vy) && fabs(vx - 1) < 0.01, "points 60 ms apart: the speed of the last two");
    Swipe(&t, 300, 0, 0, &vx, &vy);   // released 80 ms after the last point: still a swipe
    VTouchDown(&t, 1000, 100, 300);
    for (int i = 1; i <= 30; i++)
        VTouchMove(&t, 1000 + i * 10, 100 + i * 10, 300);
    Check(VTouchUp(&t, 1380, &sx, &sy) && fabs(sx - vx) < 0.01, "released 80 ms after the last point: its speed");
}

int main()
{
    TestTap();
    TestSettle();
    TestFlick();
    TestDrag();
    TestSpike();
    TestPressStray();
    TestRelease();
    TestJump();
    TestEdges();
    TestStrays();
    TestSpeed();
    printf("test_vtouch: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
