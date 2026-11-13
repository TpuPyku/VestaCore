// vhap.cpp - VestaCore: the add-ons' protocol. See vhap.h.
#include "vhap.h"
#include <stdio.h>
#include <string.h>

LRESULT VhapSendV(HWND to, HWND from, const wchar_t* fmt, va_list a)
{
    enum { kMax = 511 };
    if (!IsWindow(to))
        return 0;
    wchar_t s[kMax];
    int n = _vsnwprintf(s, kMax, fmt, a);
    if (n < 0)   // longer: its first kMax characters
        n = kMax;
    COPYDATASTRUCT c;
    c.dwData = VHAP_MAGIC;
    c.cbData = n * sizeof(wchar_t);
    c.lpData = s;
    return SendMessage(to, WM_COPYDATA, (WPARAM)from, (LPARAM)&c);
}

LRESULT VhapSend(HWND to, HWND from, const wchar_t* fmt, ...)
{
    va_list a;
    va_start(a, fmt);
    LRESULT r = VhapSendV(to, from, fmt, a);
    va_end(a);
    return r;
}

bool VhapField(const wchar_t* cmd, const wchar_t* key, wchar_t* out, int cap)
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

bool VhapCommand(LPARAM lp, wchar_t* out, int cap)
{
    const COPYDATASTRUCT* c = (const COPYDATASTRUCT*)lp;
    if (!c || c->dwData != VHAP_MAGIC || !c->lpData)
        return false;
    int n = (int)(c->cbData / sizeof(wchar_t));
    n = n > cap - 1 ? cap - 1 : n;
    memcpy(out, c->lpData, n * sizeof(wchar_t));
    out[n] = 0;
    return true;
}

void VhapHello(VhapAddon* a)
{
    a->vh = FindWindow(L"VestaHome_Wnd", NULL);
    if (a->vh)
        VhapSend(a->vh, a->self, L"hello\t%s\tapi=1\thwnd=%08X", a->hello, (unsigned)(UINT_PTR)a->self);
}

bool VhapAddonMsg(VhapAddon* a, UINT msg, WPARAM wp, LPARAM lp)
{
    wchar_t cmd[512];
    switch (msg) {
    case WM_COPYDATA:
        if (!VhapCommand(lp, cmd, 512))
            return false;
        a->on(cmd);
        return true;
    case WM_TIMER:
        if (wp != VHAP_TIMER_FIND)
            return false;
        if (!a->vh || !IsWindow(a->vh))
            VhapHello(a);
        return true;
    case WM_DESTROY:
        VhapSend(a->vh, a->self, L"bye");
        PostQuitMessage(0);
        return true;
    }
    return false;
}
