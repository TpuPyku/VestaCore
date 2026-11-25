// vrvc.cpp - the rear view camera (see vrvc.h; VestaHome without the stock shell, VestaLaunch over a navigator).
// What G3NavHMI does (Ghidra, G3NavHMI.orig.exe and
// desay_sdk_api.dll / RVS_algBackingTrace.dll of firmware 4120, 2026-10-17):
//  - CRvcDlg::paint: SV_VideoInputDisable, SV_VideoSelectChannel(0, &signal) (0 = the camera, 2 = AUX video),
//    SV_VideoInputEnable, SV_TouchDisable (SV_TouchEnable when the reverse ends), SV_BacklightOn; the SV_Video* calls are ExtEscape 0x1B6A1..0x1B6A6 to the display driver;
//    SV_VideoShowPip(alpha, colour key) = 0x1B6A3: the video shows where the screen has the key colour, the shell's
//    views use 0x101 = RGB(1, 1, 0) (the same colour VestaLaunch finds on the shell's empty navigation screen).
//  - CSkinRearViewCtrl: RVS_setTrackRear(2100), RVS_setWheelbase(1510), RVS_setStartLen(340), RVS_setMaxLen(3400),
//    RVS_setParamFilePath(PARAMS.TXT: 8 doubles, the camera's ground -> screen homography), RVS_backingTraceInit;
//    per frame RVS_locationLine(left, right, 27) (the static lines) and, with the steering angle,
//    RVS_setSteerAngle(wheel degrees * 0.0649, i.e. the road wheels', within +-45) + RVS_backingTrace(left, right,
//    27): int (x, y) screen points. Zones by point index: < 4 near, < 13 middle, the rest far. Colours: red
//    (255,0,0), yellow (243,255,0), green (32,255,0); without the angle all blue (49,130,172). The trajectory is
//    drawn when the wheel is turned more than 5 degrees: 8 px red near, then 5 px yellow (red past 341 degrees).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vrvc.h"

#ifdef UNDER_CE
#define PROC(m, n) GetProcAddress(m, L##n)
#else
#define PROC(m, n) GetProcAddress(m, n)
#endif

typedef DWORD    (*RevFn)(DWORD*);
typedef BOOL     (*VoidFn)(void);
typedef BOOL     (*SelFn)(DWORD, DWORD*);
typedef BOOL     (*PipFn)(DWORD, DWORD);
typedef BOOL     (*BlFn)(DWORD);
typedef void     (*IntFn)(int);
typedef void     (*PathFn)(const char*);
typedef int      (*InitFn)(void);
typedef unsigned (*LineFn)(int*, int*, unsigned);
typedef void     (*AngleFn)(float);

static struct {
    RevFn  reverse;
    VoidFn inEnable, inDisable, hidePip, touchOn, touchOff;
    SelFn  select;
    PipFn  showPip;
    BlFn   backlightOn;
} D;
static struct {
    IntFn   trackRear, wheelbase, startLen, maxLen;
    PathFn  paramPath;
    InitFn  init;
    LineFn  location, trace;
    AngleFn angle;
} R;
static int      s_sdk;   // 0 not tried, 1 ready, -1 none
static int      s_rvs;
static RvcLogFn s_log;
static const COLORREF kKey = RGB(1, 1, 0);   // ShowPip's 0x101
enum { kPoints = 27, kNear = 4, kMid = 13, kBuf = 60 };

#define GETP(mod, st, field, name) (*(FARPROC*)&st.field = PROC(mod, name), st.field != NULL)

bool RvcInit(RvcLogFn log)
{
    s_log = log;
    if (s_sdk)
        return s_sdk > 0;
    s_sdk = -1;
#ifdef UNDER_CE
    HMODULE m = LoadLibrary(L"desay_sdk_api.dll");
    if (!m)
        return false;
    bool ok = GETP(m, D, reverse, "SV_VehicleGetReverseStatus") & GETP(m, D, inEnable, "SV_VideoInputEnable") &
              GETP(m, D, inDisable, "SV_VideoInputDisable") & GETP(m, D, hidePip, "SV_VideoHidePip") &
              GETP(m, D, select, "SV_VideoSelectChannel") & GETP(m, D, showPip, "SV_VideoShowPip");
    GETP(m, D, backlightOn, "SV_BacklightOn");
    GETP(m, D, touchOn, "SV_TouchEnable");
    GETP(m, D, touchOff, "SV_TouchDisable");
    if (!ok) {
        if (s_log)
            s_log(L"камера: в desay_sdk_api.dll нет функций видео");
        return false;
    }
    s_sdk = 1;
    // the touch screen is off only while the camera shows: after a crash in reverse it would stay off
    if (D.touchOn && RvcReverse() == 0)
        D.touchOn();
#endif
    return s_sdk > 0;
}

int RvcReverse()
{
    if (s_sdk <= 0)
        return -1;
    DWORD v = 1;
    if (D.reverse(&v) != 0)
        return -1;
    return v == 0 ? 1 : 0;   // GPIO 0x5d is 0 in reverse (checked on the unit 2026-10-16)
}

COLORREF RvcKey()
{
    return kKey;
}

int RvcInputs(int found, int in[2])
{
    in[0] = found == 1 ? 1 : 0;
    in[1] = 1 - in[0];
    return 2;
}

static int s_input = -1;   // the input the camera was found on

int RvcVideo(bool on)
{
    if (s_sdk <= 0)
        return -1;
    DWORD signal = 0;
    if (on) {
        D.inDisable();
        int in[2], n = RvcInputs(s_input, in), got[2] = { -1, -1 };
        for (int i = 0; i < n && !signal; i++) {   // (the driver waits 200 ms after each)
            D.select((DWORD)in[i], &signal);
            got[i] = (int)signal;
            if (signal)
                s_input = in[i];
        }
        if (!signal)
            D.select((DWORD)in[0], &signal);   // none anywhere: back to the first
        D.inEnable();
        D.showPip(0xFF, kKey);
        if (D.backlightOn)
            D.backlightOn(0);
        if (D.touchOff)   // as the shell: no taps through the camera picture
            D.touchOff();
        if (s_log)
            s_log(L"камера: видео включено, сигнал %u (вход %d: %d, вход %d: %d)", (unsigned)signal, in[0], got[0],
                  in[1], got[1]);
    } else {
        D.hidePip();
        D.inDisable();
        if (D.touchOn)
            D.touchOn();
        if (s_log)
            s_log(L"камера: видео выключено");
    }
    return (int)signal;
}

static bool LoadRvs()
{
    if (s_rvs)
        return s_rvs > 0;
    s_rvs = -1;
#ifdef UNDER_CE
    HMODULE m = LoadLibrary(L"\\ResidentFlash\\navigation\\app\\RVS_algBackingTrace.dll");
    if (!m) {
        if (s_log)
            s_log(L"камера: нет RVS_algBackingTrace.dll (%u)", (unsigned)GetLastError());
        return false;
    }
    bool ok = GETP(m, R, trackRear, "RVS_setTrackRear") & GETP(m, R, wheelbase, "RVS_setWheelbase") &
              GETP(m, R, startLen, "RVS_setStartLen") & GETP(m, R, maxLen, "RVS_setMaxLen") &
              GETP(m, R, paramPath, "RVS_setParamFilePath") & GETP(m, R, init, "RVS_backingTraceInit") &
              GETP(m, R, location, "RVS_locationLine") & GETP(m, R, trace, "RVS_backingTrace") &
              GETP(m, R, angle, "RVS_setSteerAngle");
    if (!ok)
        return false;
    // the shell's numbers (CSkinRearViewCtrl's constructor)
    R.trackRear(2100);
    R.wheelbase(1510);
    R.startLen(340);
    R.maxLen(3400);
    static const char kParams[] = "\\ResidentFlash\\navigation\\app\\Config\\PARAMS.TXT";
    R.paramPath(kParams);
    if (!R.init()) {
        if (s_log)
            s_log(L"камера: RVS_backingTraceInit не прочитал PARAMS.TXT");
        return false;
    }
    s_rvs = 1;
#endif
    return s_rvs > 0;
}

static bool OnScreen(const int* p)
{
    return p[0] > 0 && p[0] <= 800 && p[1] > 0 && p[1] <= 480;
}

struct Segs {
    RvcSeg* s;
    int     n, cap;
};

static void Add(Segs* o, int x0, int y0, int x1, int y1, COLORREF c, int w, bool outline)
{
    int a[2] = { x0, y0 }, b[2] = { x1, y1 };
    if (!OnScreen(a) || !OnScreen(b) || o->n >= o->cap)
        return;
    RvcSeg& s = o->s[o->n++];
    s.x0 = x0;
    s.y0 = y0;
    s.x1 = x1;
    s.y1 = y1;
    s.c = c;
    s.w = w;
    s.outline = outline;
}

static void AddPts(Segs* o, const int* a, const int* b, COLORREF c, int w, bool outline)
{
    Add(o, a[0], a[1], b[0], b[1], c, w, outline);
}

static const COLORREF kRed = RGB(255, 0, 0), kYellow = RGB(243, 255, 0), kGreen = RGB(32, 255, 0),
                      kBlue = RGB(49, 130, 172);

static COLORREF Zone(int k, bool valid)   // the static lines' colour of segment k (near to far)
{
    return !valid ? kBlue : k < kNear ? kRed : k < kMid ? kYellow : kGreen;
}

// A short tick inward from both sides at point k: an eighth of the corridor's width there.
static void Ticks(Segs* o, const int* L, const int* R, int k, COLORREF c, int w, bool outline)
{
    int len = (R[k * 2] - L[k * 2]) / 8;
    Add(o, L[k * 2], L[k * 2 + 1], L[k * 2] + len, L[k * 2 + 1], c, w, outline);
    Add(o, R[k * 2], R[k * 2 + 1], R[k * 2] - len, R[k * 2 + 1], c, w, outline);
}

static void Sides(Segs* o, const int* L, const int* R, int k, COLORREF c, int w, bool outline)
{
    AddPts(o, L + k * 2, L + (k + 1) * 2, c, w, outline);
    AddPts(o, R + k * 2, R + (k + 1) * 2, c, w, outline);
}

static COLORREF StockColor(bool nearPiece, bool valid)   // the shell's: the near piece red, blue without the angle
{
    return valid && nearPiece ? kRed : kBlue;
}

// Ticks at three points (both sides), each its colour.
static void TicksAt(Segs* o, const int* L, const int* R, int n, const int* at, const COLORREF* c, bool outline)
{
    for (int i = 0; i < 3; i++)
        if (at[i] < n)
            Ticks(o, L, R, at[i], c[i], 6, outline);
}

static void StockSegs(Segs* o, const int* L, const int* R, int n, bool valid)
{   // three pieces with gaps (the segment before kNear, kMid), ticks at their starts
    for (int k = 0; k + 1 < n; k++)
        if (k + 1 != kNear && k + 1 != kMid)
            Sides(o, L, R, k, StockColor(k < kNear, valid), 8, false);
    int at[3] = { 0, kNear, kMid };
    COLORREF c[3] = { StockColor(true, valid), kBlue, kBlue };
    TicksAt(o, L, R, n, at, c, false);
}

static void ZoneSides(Segs* o, const int* L, const int* R, int n, bool valid, int w, bool outline)
{
    for (int k = 0; k + 1 < n; k++)
        Sides(o, L, R, k, Zone(k, valid), w, outline);
}

static void ModernSegs(Segs* o, const int* L, const int* R, int n, bool valid)
{   // open, outlined, ticks at the zones' ends in their colours
    ZoneSides(o, L, R, n, valid, 7, true);
    int at[3] = { kNear, kMid, n - 1 };
    COLORREF c[3] = { Zone(kNear - 1, valid), Zone(kMid - 1, valid), Zone(n - 2, valid) };
    TicksAt(o, L, R, n, at, c, true);
}

static void ClassicSegs(Segs* o, const int* L, const int* R, int n, bool valid)
{   // by zone with bars across at the zones' ends
    ZoneSides(o, L, R, n, valid, 4, false);
    for (int k = 0; k + 1 < n; k++)
        if (k == 0 || k + 1 == kNear || k + 1 == kMid || k + 2 == n)
            AddPts(o, L + (k + 1) * 2, R + (k + 1) * 2, Zone(k, valid), 3, false);
}

int RvcStaticSegs(const int* L, const int* R, int n, bool valid, int style, RvcSeg* out, int cap)
{
    Segs o = { out, 0, cap };
    if (style == RVC_LINES_STOCK)
        StockSegs(&o, L, R, n, valid);
    else if (style == RVC_LINES_MODERN || style == RVC_LINES_TRACKS)
        ModernSegs(&o, L, R, n, valid);
    else
        ClassicSegs(&o, L, R, n, valid);
    return o.n;
}

static void Strips(Segs* o, const int* P, int k, COLORREF c)   // a wheel's path: two thin strips 6 px apart
{
    for (int d = -3; d <= 3; d += 6)
        Add(o, P[k * 2] + d, P[k * 2 + 1], P[k * 2 + 2] + d, P[k * 2 + 3], c, 3, true);
}

int RvcTraceSegs(const int* L, const int* R, int n, int mag, int style, RvcSeg* out, int cap)
{
    Segs o = { out, 0, cap };
    bool modern = style == RVC_LINES_MODERN || style == RVC_LINES_TRACKS;
    for (int k = 0; k + 1 < n; k++) {
        bool nearZone = k < kNear;
        COLORREF c = nearZone || mag >= 341 ? kRed : kYellow;   // (341: the wheel near its end)
        if (style == RVC_LINES_TRACKS) {
            Strips(&o, L, k, c);
            Strips(&o, R, k, c);
        } else {
            Sides(&o, L, R, k, c, nearZone ? 8 : modern ? 6 : 5, modern);
        }
    }
    return o.n;
}

void RvcDrawSegs(HDC dc, const RvcSeg* s, int n)
{
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < n; i++) {
            if (!pass && !s[i].outline)
                continue;
            HPEN pen = CreatePen(PS_SOLID, pass ? s[i].w : s[i].w + 4, pass ? s[i].c : RGB(16, 16, 16));
            HGDIOBJ op = SelectObject(dc, pen);
            POINT pt[2] = { { s[i].x0, s[i].y0 }, { s[i].x1, s[i].y1 } };
            Polyline(dc, pt, 2);
            SelectObject(dc, op);
            DeleteObject(pen);
        }
}

void RvcDrawLines(HDC dc, int angle10, bool valid, int style)
{
    if (!LoadRvs())
        return;
    static int L[kBuf * 2], Rt[kBuf * 2];
    static RvcSeg segs[kBuf * 8];
    unsigned n = R.location(L, Rt, kPoints);
    RvcDrawSegs(dc, segs, RvcStaticSegs(L, Rt, (int)n, valid, style, segs, kBuf * 8));
    int deg = (angle10 >= 0 ? angle10 + 5 : angle10 - 5) / 10;
    if (!valid || (deg <= 5 && deg >= -5))
        return;
    R.angle((float)(deg * 0.0649));   // the road wheels' angle
    static int TL[kBuf * 2], TR[kBuf * 2];
    n = R.trace(TL, TR, kPoints);
    RvcDrawSegs(dc, segs, RvcTraceSegs(TL, TR, (int)n, deg < 0 ? -deg : deg, style, segs, kBuf * 8));
}
