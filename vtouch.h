// vtouch.h - VestaCore: one finger on the unit's resistive panel, filtered (the user 2026-11-23: "the map jumps under
// the finger"; in Android Auto it does not). The panel's point wanders a few px while pressed, stray points come at
// the press and at the release, single far points meanwhile. Pure: no window, no clock (times are the caller's ms).
//
//   raw point -> a far jump held till the next point confirms it (a single far point dropped)
//             -> the median of the last 3 (per axis): the finger's clean point, one point late
//             -> the 1-euro smoothing (heavy when still, light when moving): the point shown
//
// A press settles for kVTouchSettleMs (its point follows the finger meanwhile: the panel's wander is not a move), then
// becomes a drag past kVTouchDragPx from it; the drag's offset starts at 0 where it became one (no jump). The release
// ignores the pen-up point and the last unconfirmed one; its speed is the clean points' over the last 100 ms.
#ifndef VTOUCH_H
#define VTOUCH_H

#include <windows.h>

// The press's settling and the drag's threshold (as VestaHome's kSettleMs / kDragPx). The unit's own system starts
// a drag at 30 px (its firmware registry: HKLM\System\GWE\Commctrl\DragMetrics CXDrag = CYDrag = 0x1e): 20 px with
// the settling is within reason - raise it to 30 here if taps still turn into drags on the unit.
enum { kVTouchSettleMs = 100, kVTouchDragPx = 20 };

enum { VT_UP, VT_PRESS, VT_DRAG };   // VTouch.state
enum { kVTouchKeep = 8 };            // the clean points kept for the release's speed

struct VTouch {
    int    state;
    int    x, y;      // the point shown (smoothed)
    int    px, py;    // the press's point (a tap's, a long press's): settled
    int    dx, dy;    // VT_DRAG: the shown point's way since the drag began
    // inside
    DWORD  t0, tRaw;                    // pressed at; the last raw point's time
    int    x0, y0;                      // the first clean point: a quick flick measured from it
    int    ox, oy;                      // the shown point where the drag began
    DWORD  tl;                          // the last accepted raw point
    int    lx, ly;
    bool   pend;                        // a far point waiting for the next to confirm it
    DWORD  tq;
    int    qx, qy;
    DWORD  rt[3];                       // the last 3 accepted points (the newest last)
    int    rx[3], ry[3];
    DWORD  mt[kVTouchKeep];             // the clean points (a ring, the newest at (nm - 1) % kVTouchKeep)
    int    mx[kVTouchKeep], my[kVTouchKeep];
    int    nm;
    DWORD  tf;                          // the smoothing's last time, its point and speed (px/ms)
    double fx, fy, fv;
};

void VTouchDown(VTouch* t, DWORD now, int x, int y);
int  VTouchMove(VTouch* t, DWORD now, int x, int y);   // the state after this point (VT_PRESS or VT_DRAG)
// The finger lifted (its own point not used): true - it was a drag, *vx, *vy its speed (px/ms; 0 when it stood the
// last 80 ms); false - a press (a tap or a long one, at px, py)
bool VTouchUp(VTouch* t, DWORD now, double* vx, double* vy);

#endif
