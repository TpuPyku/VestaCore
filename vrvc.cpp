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

int RvcVideo(bool on)
{
    if (s_sdk <= 0)
        return -1;
    DWORD signal = 0;
    if (on) {
        D.inDisable();
        D.select(0, &signal);   // 0 = the camera
        D.inEnable();
        D.showPip(0xFF, kKey);
        if (D.backlightOn)
            D.backlightOn(0);
        if (D.touchOff)   // as the shell: no taps through the camera picture
            D.touchOff();
        if (s_log)
            s_log(L"камера: видео включено, сигнал %u", (unsigned)signal);
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

static void Seg(HDC dc, const int* a, const int* b, COLORREF c, int width)
{
    if (!OnScreen(a) || !OnScreen(b))
        return;
    HPEN pen = CreatePen(PS_SOLID, width, c);
    HGDIOBJ op = SelectObject(dc, pen);
    POINT pt[2] = { { a[0], a[1] }, { b[0], b[1] } };
    Polyline(dc, pt, 2);
    SelectObject(dc, op);
    DeleteObject(pen);
}

void RvcDrawLines(HDC dc, int angle10, bool valid)
{
    if (!LoadRvs())
        return;
    static int L[kBuf * 2], Rt[kBuf * 2];
    unsigned n = R.location(L, Rt, kPoints);
    // the static lines: by distance zone (red, yellow, green) with bars across at the zones' ends; blue without
    // the steering angle, as the shell's
    static const COLORREF kZone[3] = { RGB(255, 0, 0), RGB(243, 255, 0), RGB(32, 255, 0) };
    for (unsigned k = 0; k + 1 < n; k++) {
        COLORREF c = !valid ? RGB(49, 130, 172) : kZone[k < kNear ? 0 : k < kMid ? 1 : 2];
        Seg(dc, L + k * 2, L + (k + 1) * 2, c, 4);
        Seg(dc, Rt + k * 2, Rt + (k + 1) * 2, c, 4);
        if (k == 0 || k + 1 == kNear || k + 1 == kMid || k + 2 == n)
            Seg(dc, L + (k + 1) * 2, Rt + (k + 1) * 2, c, 3);
    }
    int deg = (angle10 >= 0 ? angle10 + 5 : angle10 - 5) / 10;
    if (!valid || (deg <= 5 && deg >= -5))
        return;
    R.angle((float)(deg * 0.0649));   // the road wheels' angle
    static int TL[kBuf * 2], TR[kBuf * 2];
    n = R.trace(TL, TR, kPoints);
    int mag = deg < 0 ? -deg : deg;
    for (unsigned k = 0; k + 1 < n; k++) {
        bool nearZone = k < kNear;
        COLORREF c = nearZone || mag >= 341 ? RGB(255, 0, 0) : RGB(243, 255, 0);
        Seg(dc, TL + k * 2, TL + (k + 1) * 2, c, nearZone ? 8 : 5);
        Seg(dc, TR + k * 2, TR + (k + 1) * 2, c, nearZone ? 8 : 5);
    }
}
