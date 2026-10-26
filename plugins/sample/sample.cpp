// sample.cpp - an example add-on for VestaHome (VestaHome/docs/plugin-api.md, API 1). Free to copy as the start of
// your own. A program of its own (Windows CE ARM and the desktop, for testing next to VestaHomePC.exe): a hidden
// window, hello to VestaHome by WM_COPYDATA, then a widget "Счётчик" on the main screen and two described screens
// with every kind of item. Put it in VestaHome\Plugins\Sample\Sample.exe: VestaHome starts it.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#define VHAP_MAGIC 0x56484150   // 'VHAP'
static const wchar_t* kId = L"Sample";   // = its folder's name in Plugins
enum { TIMER_FIND = 1, kFindMs = 3000 };

static HWND s_self, s_vh;   // our window, VestaHome's
static int  s_count, s_step = 1, s_mode;
static bool s_light;

static void Send(const wchar_t* fmt, ...)   // one command: "verb\tkey=value..."
{
    if (!s_vh || !IsWindow(s_vh)) {
        s_vh = NULL;
        return;
    }
    wchar_t s[512];
    va_list a;
    va_start(a, fmt);
    int n = _vsnwprintf(s, 511, fmt, a);
    va_end(a);
    s[511] = 0;
    if (n < 0)
        n = (int)wcslen(s);
    COPYDATASTRUCT c;
    c.dwData = VHAP_MAGIC;
    c.cbData = n * sizeof(wchar_t);
    c.lpData = s;
    SendMessage(s_vh, WM_COPYDATA, (WPARAM)s_self, (LPARAM)&c);
}

static bool Field(const wchar_t* cmd, const wchar_t* key, wchar_t* out, int cap)
{
    out[0] = 0;
    int kl = (int)wcslen(key);
    for (const wchar_t* p = wcschr(cmd, L'\t'); p; p = wcschr(p + 1, L'\t')) {
        if (wcsncmp(p + 1, key, kl) || p[1 + kl] != L'=')
            continue;
        const wchar_t* v = p + 2 + kl;
        int n = 0;
        while (v[n] && v[n] != L'\t' && n < cap - 1) {
            out[n] = v[n];
            n++;
        }
        out[n] = 0;
        return true;
    }
    return false;
}

static void Values()   // the values that change: on the screen and on the widget
{
    Send(L"item.set\tscreen=main\tid=count\tvalue=%d", s_count);
    Send(L"item.set\tscreen=main\tid=step\tvalue=%d", s_step);
    Send(L"widget.set\tid=count\tvalue=%d\tunit=раз\ttext=шаг %d\ttext2=%s\tstate=%s", s_count, s_step,
         s_light ? L"подсветка включена" : L"подсветка выключена", s_count >= 10 ? L"warn" : L"ok");
}

static void Declare()   // after "welcome": the widget and the screens
{
    Send(L"widget\tid=count\ttitle=Пример\ticon=star\tsize=1\tscreen=main");
    Send(L"screen\tid=main\ttitle=Пример дополнения");
    Send(L"item\tscreen=main\tid=plus\ttype=button\tlabel=Плюс шаг");
    Send(L"item\tscreen=main\tid=count\ttype=value\tlabel=Счётчик\tsub=нажатия кнопки выше");
    Send(L"item\tscreen=main\tid=step\ttype=stepper\tlabel=Шаг\tvalue=1");
    Send(L"item\tscreen=main\tid=light\ttype=toggle\tlabel=Подсветка\tsub=просто переключатель\tvalue=0");
    Send(L"item\tscreen=main\tid=mode\ttype=seg\tlabel=Режим\toptions=Эко|Норма|Спорт\tvalue=0");
    Send(L"item\tscreen=main\tid=about\ttype=row\tlabel=О дополнении\tsub=второй экран\tlink=about");
    Send(L"screen\tid=about\ttitle=О дополнении");
    Send(L"item\tscreen=about\tid=ver\ttype=value\tlabel=Версия\tvalue=1.0");
    Send(L"item\tscreen=about\tid=api\ttype=value\tlabel=API\tvalue=1");
    Send(L"item\tscreen=about\tid=reset\ttype=button\tlabel=Сбросить счётчик");
    Values();
}

static void Hello()   // VestaHome found (again): who we are
{
    s_vh = FindWindow(L"VestaHome_Wnd", NULL);
    if (s_vh)
        Send(L"hello\tid=%s\tname=Пример\tver=1.0\tapi=1\thwnd=%08X", kId, (unsigned)(UINT_PTR)s_self);
}

static void Receive(const wchar_t* cmd)
{
    wchar_t id[32], v[32];
    if (!wcsncmp(cmd, L"welcome", 7)) {
        Declare();
    } else if (!wcsncmp(cmd, L"quit", 4)) {
        DestroyWindow(s_self);
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
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_COPYDATA: {
        const COPYDATASTRUCT* c = (const COPYDATASTRUCT*)lp;
        if (!c || c->dwData != VHAP_MAGIC || !c->lpData)
            return FALSE;
        wchar_t cmd[512];
        int n = (int)(c->cbData / sizeof(wchar_t));
        n = n > 511 ? 511 : n;
        memcpy(cmd, c->lpData, n * sizeof(wchar_t));
        cmd[n] = 0;
        Receive(cmd);
        return TRUE;
    }
    case WM_TIMER:   // VestaHome not there yet, or restarted: say hello again
        if (!s_vh || !IsWindow(s_vh))
            Hello();
        return 0;
    case WM_DESTROY:
        Send(L"bye");
        PostQuitMessage(0);
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
    s_self = CreateWindow(L"VHSample_Wnd", L"VestaHome sample add-on", WS_POPUP, 0, 0, 1, 1, NULL, NULL, inst, NULL);
    if (!s_self)
        return 1;
    Hello();
    SetTimer(s_self, TIMER_FIND, kFindMs, NULL);
    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return 0;
}
