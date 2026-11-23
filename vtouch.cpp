// vtouch.cpp - one finger on a resistive panel, filtered. See vtouch.h.
#include "vtouch.h"
#include <math.h>

// A raw point farther from the last one than kJumpPx + kFastPxMs * the ms between them waits for the next to confirm
// it: the finger's quickest flick is ~1.5 m/s, 8 px/ms on the unit's 7" (0.19 mm a px); a stray at the press or the
// release lands hundreds of px away. Nearer single points are the median's.
enum { kJumpPx = 40, kFlickPx = 40, kStillMs = 80, kWindowMs = 100 };
static const double kFastPxMs = 8;
// The 1-euro smoothing's cutoff (cycles a ms): 1 Hz for a still finger (the wander of +-6 px shown as +-1), up by
// kBeta a px/ms of speed (30 Hz at 1 px/ms: the point shown ~3 px behind the clean one) - its speed itself smoothed
// at 5 Hz (it follows a start within ~3 points).
static const double kMinCut = 0.001, kBeta = 0.03, kSpeedCut = 0.005, kTwoPi = 6.28318530718;

static bool Near(DWORD ms, int ax, int ay, int bx, int by)
{
    double r = kJumpPx + kFastPxMs * ms, dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy <= r * r;
}

static bool Past(int ax, int ay, int bx, int by, int r)
{
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by) > r * r;
}

static int Median3(const int* v)
{
    int a = v[0], b = v[1], c = v[2];
    if (a > b) {
        int s = a;
        a = b;
        b = s;
    }
    return c < a ? a : c > b ? b : c;
}

static void AddClean(VTouch* t, DWORD tm, int x, int y)
{
    int i = t->nm % kVTouchKeep;
    t->mt[i] = tm;
    t->mx[i] = x;
    t->my[i] = y;
    t->nm++;
}

static void Restart(VTouch* t, DWORD now, int x, int y)   // every stage from this point
{
    t->tl = t->tf = now;
    t->lx = t->x = t->px = t->x0 = x;
    t->ly = t->y = t->py = t->y0 = y;
    t->fx = x;
    t->fy = y;
    t->fv = 0;
    t->pend = false;
    for (int i = 0; i < 3; i++) {
        t->rt[i] = now;
        t->rx[i] = x;
        t->ry[i] = y;
    }
    t->nm = 0;
}

void VTouchDown(VTouch* t, DWORD now, int x, int y)
{
    t->state = VT_PRESS;
    t->dx = t->dy = 0;
    t->t0 = t->tRaw = now;
    Restart(t, now, x, y);
}

static double Alpha(double cut, double ms)
{
    return 1 / (1 + 1 / (kTwoPi * cut * ms));
}

static void Smooth(VTouch* t, DWORD now, int mx, int my)   // the 1-euro filter over the clean points
{
    double ms = now - t->tf > 0 ? (double)(now - t->tf) : 1;
    double vx = (mx - t->fx) / ms, vy = (my - t->fy) / ms;
    t->fv += Alpha(kSpeedCut, ms) * (sqrt(vx * vx + vy * vy) - t->fv);
    double a = Alpha(kMinCut + kBeta * t->fv, ms);
    t->fx += a * (mx - t->fx);
    t->fy += a * (my - t->fy);
    t->tf = now;
    t->x = (int)floor(t->fx + 0.5);
    t->y = (int)floor(t->fy + 0.5);
}

static void Classify(VTouch* t, DWORD now, int mx, int my)   // a press, a drag
{
    if (t->state == VT_PRESS) {
        bool settling = now - t->t0 < (DWORD)kVTouchSettleMs;
        if (settling ? !Past(mx, my, t->x0, t->y0, kFlickPx) : !Past(mx, my, t->px, t->py, kVTouchDragPx)) {
            if (settling) {   // (the press's point follows the finger while it settles)
                t->px = mx;
                t->py = my;
            }
            return;
        }
        t->state = VT_DRAG;
        t->ox = t->x;
        t->oy = t->y;
    }
    t->dx = t->x - t->ox;
    t->dy = t->y - t->oy;
}

static void Accept(VTouch* t, DWORD now, int x, int y)   // a raw point taken: the median, the smoothing, the state
{
    for (int i = 0; i < 2; i++) {
        t->rt[i] = t->rt[i + 1];
        t->rx[i] = t->rx[i + 1];
        t->ry[i] = t->ry[i + 1];
    }
    t->rt[2] = t->tl = now;
    t->rx[2] = t->lx = x;
    t->ry[2] = t->ly = y;
    int mx = Median3(t->rx), my = Median3(t->ry);
    AddClean(t, t->rt[1], mx, my);   // (the median is one point late: the middle point's time)
    Smooth(t, now, mx, my);
    Classify(t, now, mx, my);
}

static void Hold(VTouch* t, DWORD now, int x, int y)
{
    t->pend = true;
    t->tq = now;
    t->qx = x;
    t->qy = y;
}

static bool Held(VTouch* t, DWORD now, int x, int y)   // true: the point waits (or is dropped); false: take it
{
    bool fromLast = Near(now - t->tl, x, y, t->lx, t->ly);
    if (!t->pend) {
        if (!fromLast)
            Hold(t, now, x, y);
        return !fromLast;
    }
    t->pend = false;
    if (fromLast)   // the waiting one was a single far point: dropped
        return false;
    if (!Near(now - t->tq, x, y, t->qx, t->qy)) {   // far from both: this one waits instead
        Hold(t, now, x, y);
        return true;
    }
    // two far points together: the finger is there - while the press settles, its first points were strays
    if (t->state == VT_PRESS && t->tq - t->t0 < (DWORD)kVTouchSettleMs)
        Restart(t, t->tq, t->qx, t->qy);
    else
        Accept(t, t->tq, t->qx, t->qy);
    return false;
}

int VTouchMove(VTouch* t, DWORD now, int x, int y)
{
    t->tRaw = now;
    if (!Held(t, now, x, y))
        Accept(t, now, x, y);
    return t->state;
}

// The clean points' speed over the last kWindowMs, of the first n of them (the ring's newer ones left out)
static void Speed(const VTouch* t, int n, double* vx, double* vy)
{
    int last = (n - 1) % kVTouchKeep, first = last, kept = n - (t->nm > kVTouchKeep ? t->nm - kVTouchKeep : 0);
    for (int k = 1; k < kept; k++) {
        int i = (n - 1 - k) % kVTouchKeep;
        if (t->mt[last] - t->mt[i] > (DWORD)kWindowMs)
            break;
        first = i;
    }
    DWORD ms = t->mt[last] - t->mt[first];
    if (!ms)   // (one point, or only the press's)
        return;
    *vx = (t->mx[last] - t->mx[first]) / (double)ms;
    *vy = (t->my[last] - t->my[first]) / (double)ms;
}

bool VTouchUp(VTouch* t, DWORD now, double* vx, double* vy)
{
    *vx = *vy = 0;
    bool drag = t->state == VT_DRAG;
    t->state = VT_UP;
    if (drag && now - t->tRaw <= (DWORD)kStillMs)
        Speed(t, t->nm - 1, vx, vy);   // (without the newest: its last point had no next to confirm it; a drag has 2+)
    return drag;
}
