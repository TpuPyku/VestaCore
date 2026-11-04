// vhap.h - VestaCore: the add-ons' protocol (VestaHome/docs/plugin-api.md): commands are WM_COPYDATA with dwData
// VHAP_MAGIC and UTF-16 text without its zero, "name\tkey=value\tkey=value...". Used by VestaHome (its add-ons),
// OSMapsCE (an add-on of VestaHome) and the sample add-on.
#ifndef VHAP_H
#define VHAP_H

#include <windows.h>
#include <stdarg.h>

#define VHAP_MAGIC 0x56484150   // 'VHAP'

// A command (printf-style, 511 characters at most) to `to`, from `from`: the receiver's answer; 0 - `to` is not a
// window (nothing sent).
LRESULT VhapSend(HWND to, HWND from, const wchar_t* fmt, ...);
LRESULT VhapSendV(HWND to, HWND from, const wchar_t* fmt, va_list a);

// A field of a command: "key=value" between tabs; false (out "") when it is not there.
bool VhapField(const wchar_t* cmd, const wchar_t* key, wchar_t* out, int cap);

#endif
