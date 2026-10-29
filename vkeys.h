// vkeys.h - VestaCore: the on-screen keyboard's text and cursor (VestaHome's keyboard; the user 2026-10-29: OSMapsCE's
// search too). The text is edited at the cursor; the finger puts and drags the cursor anywhere on the field, the
// field shows the text from `view`, as far as the cursor needs. Each program draws the keys and the field its own way;
// the keys are 4 rows of 12 (VKeysChar), then Shift, the language, the space and backspace.
#ifndef VKEYS_H
#define VKEYS_H

#include <windows.h>

enum { VKEYS_MAX = 400, VKEYS_ROWS = 4, VKEYS_COLS = 12 };

struct VKeys {
    wchar_t text[VKEYS_MAX];
    int     cap;               // characters at most (< VKEYS_MAX)
    int     cur, view, visN;   // the cursor, the first character shown, how many are shown
    int     dx[VKEYS_MAX];     // the shown characters' right edges from the field's left (the finger's map)
    bool    en, shift;         // the Latin keys; Shift (one character, like a phone's keyboard)
};

void    VKeysOpen(VKeys* k, const wchar_t* text, int cap);   // the cursor at the text's end, Russian, no Shift
wchar_t VKeysChar(const VKeys* k, int row, int col);         // a key as shown (Shift: the digits' row - symbols, capitals)
int     VKeysRowLen(const VKeys* k, int row);                // keys in a row (0: no such row)
void    VKeysType(VKeys* k, wchar_t ch);                     // at the cursor (Shift: a capital, Shift off)
void    VKeysBack(VKeys* k);                                 // the character before the cursor
void    VKeysCursorAt(VKeys* k, int x, int fieldW);          // the finger at x from the field's left (the last layout)
int     VKeysLayout(VKeys* k, HDC dc, int fieldW);           // the field (dc: its font): view, visN, dx; the cursor's x

#endif
