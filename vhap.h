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

// A WM_COPYDATA's lParam as a command: its text (with a zero, cut to cap) in out; false - not a command of ours.
bool VhapCommand(LPARAM lp, wchar_t* out, int cap);

// An add-on's hidden window (plugin-api.md 2-3): VestaHome found and greeted, its commands, the goodbye.
enum { VHAP_TIMER_FIND = 0x5648 };   // the add-on's timer (every few seconds): VestaHome looked for again when gone
struct VhapAddon {
    HWND           self, vh;                  // the add-on's window; VestaHome's, found
    const wchar_t* hello;                     // "id=..\tname=..\tver=.." (api and hwnd are added)
    void          (*on)(const wchar_t* cmd);  // a command from VestaHome
};
void VhapHello(VhapAddon* a);   // VestaHome looked for; found: hello
// WM_COPYDATA - a command to on; WM_TIMER VHAP_TIMER_FIND - hello when VestaHome is not there; WM_DESTROY - bye, the
// message loop quits. true: handled (WndProc returns TRUE / 0 as for these messages).
bool VhapAddonMsg(VhapAddon* a, UINT msg, WPARAM wp, LPARAM lp);

// A field of a command: "key=value" between tabs; false (out "") when it is not there.
bool VhapField(const wchar_t* cmd, const wchar_t* key, wchar_t* out, int cap);

#endif
