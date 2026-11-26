// vkeys.h - VestaCore: the on-screen keyboard's text and cursor (VestaHome's keyboard; the user 2026-10-29: OSMapsCE's
// search too). The text is edited at the cursor; the finger puts and drags the cursor anywhere on the field, the
// field shows the text from `view`, as far as the cursor needs. Each program draws the keys and the field its own way;
// the keys are a PC's, 4 rows of 13, 13, 11, 10 (VKeysChar), then Shift, the language, the space and backspace.
#ifndef VKEYS_H
#define VKEYS_H

#include <windows.h>

enum { VKEYS_MAX = 400, VKEYS_ROWS = 4, VKEYS_COLS = 13 };

struct VKeys {
    wchar_t text[VKEYS_MAX];
    int     cap;               // characters at most (< VKEYS_MAX)
    int     cur, view, visN;   // the cursor, the first character shown, how many are shown
    int     dx[VKEYS_MAX];     // the shown characters' right edges from the field's left (the finger's map)
    bool    en, shift;         // the Latin keys; Shift (one character, like a phone's keyboard)
};

void    VKeysOpen(VKeys* k, const wchar_t* text, int cap);   // the cursor at the text's end, Russian, no Shift
wchar_t VKeysChar(const VKeys* k, int row, int col);         // a key as shown (Shift as a PC's: capitals, 1: !)
int     VKeysRowLen(const VKeys* k, int row);                // keys in a row (0: no such row)
// A key's other characters (a long press: a bubble, the finger picks one): 0..2 into out[2].
int     VKeysAlts(const VKeys* k, int row, int col, wchar_t* out);
int     VKeysAltsFor(const VKeys* k, wchar_t shown, wchar_t* out);   // of the key showing `shown`
// The bubble of n cells over a key: its left x (centred, kept on the screen); the cell under the finger.
int     VKeysBubbleX(int keyX, int keyW, int n, int cellW, int screenW);
int     VKeysBubbleSel(int fingerX, int bubbleX, int cellW, int n);
void    VKeysType(VKeys* k, wchar_t ch);                     // at the cursor (Shift: a capital, Shift off)
void    VKeysBack(VKeys* k);                                 // the character before the cursor
void    VKeysCursorAt(VKeys* k, int x, int fieldW);          // the finger at x from the field's left (the last layout)
int     VKeysLayout(VKeys* k, HDC dc, int fieldW);           // the field (dc: its font): view, visN, dx; the cursor's x

#endif
