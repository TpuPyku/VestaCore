// sample.cpp - an example add-on for VestaHome (VestaHome/docs/plugin-api.md, API 1). Free to copy as the start of
// your own. A program of its own (Windows CE ARM and the desktop, for testing next to VestaHomePC.exe): a hidden
// window, hello to VestaHome by WM_COPYDATA, then a widget "Счётчик" on the main screen, two described screens with
// every kind of item (the first with a button in the bottom bar), a screen of its own window, and the speed and the
// night from VestaHome's data. Put it in \ResidentFlash\VHContent\VHPlugins\Sample\Sample.exe: VestaHome starts it.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "vhap.h"   // VestaCore: VhapAddon (VestaHome found, hello, its commands, bye), VhapSend, VhapField
enum { kFindMs = 3000 };

static void Receive(const wchar_t* cmd);
static VhapAddon s_addon = { NULL, NULL, L"id=Sample\tname=Пример\tver=1.0", Receive };   // id = its folder's name
static HWND s_own;   // our own screen's window
static int  s_count, s_step = 1, s_mode, s_speed = -1;
static bool s_light, s_night;

static void Send(const wchar_t* fmt, ...)   // one command to VestaHome: "verb\tkey=value..."
{
    va_list a;
    va_start(a, fmt);
    VhapSendV(s_addon.vh, s_addon.self, fmt, a);
    va_end(a);
}

static bool Field(const wchar_t* cmd, const wchar_t* key, wchar_t* out, int cap)
{
    return VhapField(cmd, key, out, cap);
}

static void Values()   // the values that change: on the screen and on the widget
{
    Send(L"item.set\tscreen=main\tid=count\tvalue=%d", s_count);
    Send(L"item.set\tscreen=main\tid=step\tvalue=%d", s_step);
    if (s_speed >= 0)
        Send(L"item.set\tscreen=about\tid=speed\tvalue=%d км/ч", s_speed);
    else
        Send(L"item.set\tscreen=about\tid=speed\tvalue=нет GPS");
    Send(L"widget.set\tid=count\tvalue=%d\tunit=раз\ttext=шаг %d\ttext2=%s\tstate=%s", s_count, s_step,
         s_light ? L"подсветка включена" : L"подсветка выключена", s_count >= 10 ? L"warn" : L"ok");
}

static void Declare()   // after "welcome": the widget and the screens
{
    Send(L"widget\tid=count\ttitle=Пример\ticon=star\tsize=1\tscreen=main");
    Send(L"screen\tid=main\ttitle=Пример\ticon=star\tnav=1");   // with a button in the bottom bar
    Send(L"item\tscreen=main\tid=plus\ttype=button\tlabel=Плюс шаг");
    Send(L"item\tscreen=main\tid=count\ttype=value\tlabel=Счётчик\tsub=нажатия кнопки выше");
    Send(L"item\tscreen=main\tid=step\ttype=stepper\tlabel=Шаг\tvalue=1");
    Send(L"item\tscreen=main\tid=light\ttype=toggle\tlabel=Подсветка\tsub=просто переключатель\tvalue=0");
    Send(L"item\tscreen=main\tid=mode\ttype=seg\tlabel=Режим\toptions=Эко|Норма|Спорт\tvalue=0");
    Send(L"item\tscreen=main\tid=about\ttype=row\tlabel=О дополнении\tsub=второй экран\tlink=about");
    Send(L"screen\tid=own\ttitle=Своё окно\town=1");
    Send(L"window\tscreen=own\thwnd=%08X", (unsigned)(UINT_PTR)s_own);
    Send(L"screen\tid=about\ttitle=О дополнении");
    Send(L"item\tscreen=about\tid=ver\ttype=value\tlabel=Версия\tvalue=1.0");
    Send(L"item\tscreen=about\tid=api\ttype=value\tlabel=API\tvalue=1");
    Send(L"item\tscreen=about\tid=reset\ttype=button\tlabel=Сбросить счётчик");
    Send(L"item\tscreen=about\tid=speed\ttype=value\tlabel=Скорость (подписка gps)\tvalue=…");
    Send(L"item\tscreen=about\tid=ownwin\ttype=row\tlabel=Своё окно\tsub=дополнение рисует само\tlink=own");
    Send(L"subscribe\twhat=gps,night");
    Values();
}

static void Receive(const wchar_t* cmd)
{
    wchar_t id[32], v[32];
    if (!wcsncmp(cmd, L"welcome", 7)) {
        Declare();
    } else if (!wcsncmp(cmd, L"quit", 4)) {
        DestroyWindow(s_addon.self);
    } else if (!wcsncmp(cmd, L"event", 5)) {
        Field(cmd, L"id", id, 32);
        Field(cmd, L"value", v, 32);
        if (!wcscmp(id, L"plus")) {
            s_count += s_step;
            Send(L"notify\ttext=Счётчик: %d", s_count);
        } else if (!wcscmp(id, L"step")) {
            s_step += _wtoi(v);
            s_step = s_step < 1 ? 1 : s_step > 10 ? 10 : s_step;
        } else if (!wcscmp(id, L"light")) {
            s_light = v[0] == L'1';
        } else if (!wcscmp(id, L"mode")) {
            s_mode = _wtoi(v);
        } else if (!wcscmp(id, L"reset")) {
            s_count = 0;
        }
        Values();
    } else if (!wcsncmp(cmd, L"screen.open", 11)) {
        Values();   // fresh values when shown
    } else if (!wcsncmp(cmd, L"gps", 3)) {
        s_speed = Field(cmd, L"speed", v, 32) ? _wtoi(v) : -1;
        Send(L"item.set\tscreen=about\tid=speed\tvalue=%s", s_speed >= 0 ? v : L"нет GPS");
    } else if (!wcsncmp(cmd, L"night", 5)) {
        Field(cmd, L"value", v, 32);
        s_night = v[0] == L'1';
        if (s_own)
            InvalidateRect(s_own, NULL, TRUE);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (VhapAddonMsg(&s_addon, msg, wp, lp))   // its commands to Receive, hello again when VestaHome restarted, bye
        return msg == WM_COPYDATA;
    return DefWindowProc(hwnd, msg, wp, lp);
}

// The screen of our own window: drawn by us (VestaHome only shows it in front); a tap - back to VestaHome.
static LRESULT CALLBACK OwnProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r;
        GetClientRect(hwnd, &r);
        HBRUSH b = CreateSolidBrush(s_night ? RGB(10, 20, 40) : RGB(30, 60, 120));
        FillRect(dc, &r, b);
        DeleteObject(b);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        wchar_t t[160];
        _snwprintf(t, 160, L"Своё окно дополнения\n\nсчётчик %d · %s\n\nнажмите — назад в VestaHome", s_count,
                   s_night ? L"ночь" : L"день");
        DrawText(dc, t, -1, &r, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONUP:   // back: hidden, VestaHome told
        ShowWindow(hwnd, SW_HIDE);
        Send(L"back");
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

#ifdef UNDER_CE
int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int)
#else
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int)
#endif
{
    if (FindWindow(L"VHSample_Wnd", NULL))
        return 0;   // one copy
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.lpszClassName = L"VHSample_Wnd";
    RegisterClass(&wc);
    s_addon.self = CreateWindow(L"VHSample_Wnd", L"VestaHome sample add-on", WS_POPUP, 0, 0, 1, 1, NULL, NULL, inst,
                                NULL);
    if (!s_addon.self)
        return 1;
    wc.lpfnWndProc = OwnProc;   // the own screen's window: hidden until VestaHome shows it
    wc.lpszClassName = L"VHSampleOwn_Wnd";
    RegisterClass(&wc);
    s_own = CreateWindow(L"VHSampleOwn_Wnd", L"Sample own screen", WS_POPUP, 0, 0, 800, 480, NULL, NULL, inst, NULL);
    VhapHello(&s_addon);
    SetTimer(s_addon.self, VHAP_TIMER_FIND, kFindMs, NULL);
    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return 0;
}
