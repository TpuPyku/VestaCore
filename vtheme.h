// vtheme.h - VestaCore: VestaHome's look for the programs beside it (VestaFiles first, the user 2026-11-27: "in
// VestaHome's style, with its themes"). The colours: the built-in ones, then VestaHome's Skin\skin.ini, the accent
// chosen in its "Тема" (screen.accent of VestaHome.ini, a Lada paint code), then VHContent\Skin\user_skin.ini (its
// lines win, its accent over the chosen one - as VestaHome does). Its font: Fonts\ in VestaHome's folder.
#ifndef VTHEME_H
#define VTHEME_H

#include <windows.h>

struct VTheme {
    COLORREF ink, panel, panel2, line, text, muted, dim, accent, off, ok, stop;
    bool     userAccent;          // user_skin.ini set the accent (the chosen one then not applied)
    wchar_t  vh[MAX_PATH];        // VestaHome's folder ("\ResidentFlash\VestaHome\"), "" not found
};

void VThemeDefaults(VTheme* t);   // VestaHome's built-in colours (Марс)
// A skin file's text (UTF-8, its BOM allowed): its [colors] lines over t. user: user_skin.ini (its accent noted).
void VThemeColors(VTheme* t, const char* text, bool user);
// The accent of a Lada paint code (VestaHome's "Тема"): false - not one of them (t not changed).
bool VThemeAccent(VTheme* t, int code);
int  VThemeAccentCode(const char* ini);   // VestaHome.ini's screen.accent (its text); 0 - not there
// All of it from VestaHome's folder vh (ending in '\'; "" - the built-in colours only) and the user's skin folders
// (VHContent\Skin on \ResidentFlash, then the cards; the first found).
void VThemeLoad(VTheme* t, const wchar_t* vh);
// VestaHome's folder: \ResidentFlash\VestaHome\, \SDMMC\VestaHome\ (as its stub looks), the PC: VestaHome\ beside
// this exe. false - none (out "").
bool VThemeFindVh(wchar_t* out);

#endif
