// vnav.h - VestaCore: the route's next manoeuvre as OSMapsCE tells it to VestaHome (docs/plugin-api.md, "nav"), and its
// sign. OSMapsCE formats, VestaHome parses: one shape, one place.
#ifndef VNAV_H
#define VNAV_H

#include <windows.h>

struct VNav {
    bool    on;           // a route is being followed; off: the rest is empty
    int     turn;         // degrees (-180..180, + right), VNAV_FINISH, VNAV_ROUND, VNAV_STOP
    wchar_t dist[16];     // to the manoeuvre, ready to show: "350 m"
    wchar_t street[64];   // what comes there
    wchar_t eta[8];       // the arrival: "14:41"
};
enum { VNAV_STOP = 997, VNAV_ROUND = 998, VNAV_FINISH = 999 };

// "nav<TAB>on=1<TAB>turn=90..." into out; a tab or a line break inside a text becomes a space (they end a field).
void VNavFormat(const VNav& n, wchar_t* out, int cap);
// A "nav" command into n (on false for anything else). Fields too long are cut.
void VNavParse(const wchar_t* cmd, VNav* n);
// A routing instruction into what to do and where (see vnav.cpp); no colon: verb empty, all of it the street. Each out
// has cap characters.
void VNavSplit(const wchar_t* text, wchar_t* verb, wchar_t* street, int cap);
// The sign in r: a stem from below and an arrow the way of the turn; the finish - a ring with a dot, a roundabout -
// a ring with the way out up.
void VNavDrawTurn(HDC dc, const RECT& r, int turn, COLORREF col);

#endif
