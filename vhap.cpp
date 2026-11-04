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
