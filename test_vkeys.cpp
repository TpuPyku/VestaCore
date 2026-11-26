// vkeys (the on-screen keyboard's text and cursor): typing and deleting at the cursor, Shift, the field's view and
// the finger's cursor, the keys' characters. PC build (test_vkeys.bat); exit code: the number of failed checks.
#include "vkeys.h"
#include <stdio.h>
#include <string.h>

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static void EditChecks()
{
    VKeys k;
    VKeysOpen(&k, L"abc", 6);
    Check(!wcscmp(k.text, L"abc") && k.cur == 3 && k.view == 0 && !k.shift && !k.en, "open: the text, the cursor at its end");
    VKeysType(&k, L'd');
    Check(!wcscmp(k.text, L"abcd") && k.cur == 4, "type at the end");
    k.cur = 1;
    VKeysType(&k, L'X');
    Check(!wcscmp(k.text, L"aXbcd") && k.cur == 2, "type at the cursor, the cursor after it");
    k.shift = true;
    VKeysType(&k, L'q');
    Check(!wcscmp(k.text, L"aXQbcd") && k.cur == 3 && !k.shift, "Shift: a capital, once");
    VKeysType(&k, L'z');
    Check(!wcscmp(k.text, L"aXQbcd"), "full (cap 6): nothing typed");
    VKeysBack(&k);
    Check(!wcscmp(k.text, L"aXbcd") && k.cur == 2, "backspace: the character before the cursor");
    k.cur = 0;
    VKeysBack(&k);
    Check(!wcscmp(k.text, L"aXbcd") && k.cur == 0, "backspace at the start: nothing");
    k.cur = 99;
    VKeysType(&k, L'e');
    Check(!wcscmp(k.text, L"aXbcde") && k.cur == 6, "a cursor past the end: at the end");
    k.cur = -5;
    VKeysBack(&k);
    Check(!wcscmp(k.text, L"aXbcde") && k.cur == 0, "a cursor before the start: at the start");
    VKeysOpen(&k, L"ab", 6);
    k.cur = 1;
    VKeysBack(&k);
    Check(!wcscmp(k.text, L"b") && k.cur == 0, "backspace after the first character");
    VKeysOpen(&k, L"", VKEYS_MAX - 2);
    Check(k.cap == VKEYS_MAX - 2, "cap: under the buffer's, as asked");
    VKeysOpen(&k, L"", VKEYS_MAX - 1);
    Check(k.cap == VKEYS_MAX - 1, "cap: the buffer's own, less its end");
    VKeysOpen(&k, L"", VKEYS_MAX);
    Check(k.cap == VKEYS_MAX - 1, "cap: no more than the buffer");
    VKeysOpen(&k, L"", 1000);
    Check(k.cap == VKEYS_MAX - 1, "cap: a big one cut to the buffer");
    VKeysOpen(&k, L"0123456789", 5);
    Check(!wcscmp(k.text, L"01234") && k.cur == 5, "open: a longer text cut to cap");
}

static void ShiftChecks()
{
    VKeys k;
    VKeysOpen(&k, L"", 10);
    k.shift = true;
    VKeysType(&k, L'й');
    Check(!wcscmp(k.text, L"Й") && !k.shift, "Shift: a capital Russian letter, once");
    VKeysType(&k, L'й');
    Check(!wcscmp(k.text, L"Йй"), "after it: small again");
}

static void KeyChecks()
{
    VKeys k;
    VKeysOpen(&k, L"", 10);
    Check(VKeysChar(&k, 0, 0) == L'ё' && VKeysChar(&k, 0, 1) == L'1' && VKeysChar(&k, 1, 0) == L'й' &&
          VKeysChar(&k, 1, 4) == L'е' && VKeysChar(&k, 1, 5) == L'н' && VKeysChar(&k, 3, 9) == L'.', "the Russian keys as a PC's");
    Check(VKeysRowLen(&k, 0) == 13 && VKeysRowLen(&k, 1) == 13 && VKeysRowLen(&k, 2) == 11 && VKeysRowLen(&k, 3) == 10 &&
          VKeysRowLen(&k, 4) == 0 && VKeysRowLen(&k, -1) == 0, "rows of 13, 13, 11, 10");
    k.shift = true;
    Check(VKeysChar(&k, 0, 1) == L'!' && VKeysChar(&k, 1, 0) == L'Й' && VKeysChar(&k, 0, 0) == L'Ё' &&
          VKeysChar(&k, 3, 9) == L',', "Shift as a PC's: 1 - !, letters - capitals, . - ,");
    k.shift = false;
    k.en = true;
    Check(VKeysChar(&k, 1, 0) == L'q' && VKeysChar(&k, 3, 0) == L'z' && VKeysChar(&k, 0, 0) == L'`' &&
          VKeysChar(&k, 1, 12) == L'\\', "the Latin keys as a PC's");
    k.shift = true;
    Check(VKeysChar(&k, 1, 10) == L'{' && VKeysChar(&k, 2, 9) == L':' && VKeysChar(&k, 3, 9) == L'?', "EN Shift: [ - {, ; - :, / - ?");
}

static void FieldChecks()
{
    HDC dc = CreateCompatibleDC(NULL);
    HFONT f = CreateFont(-26, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Tahoma");
    HGDIOBJ of = SelectObject(dc, f);
    VKeys k;
    VKeysOpen(&k, L"abc", 50);
    int x = VKeysLayout(&k, dc, 200);
    Check(k.view == 0 && k.visN == 3 && x == k.dx[2] && x > 0, "short: all shown, the cursor after the last one");
    VKeysCursorAt(&k, k.dx[0] + 1, 200);
    Check(k.cur == 1, "the finger after the first character: the cursor there");
    VKeysCursorAt(&k, -20, 200);
    Check(k.cur == 0, "the finger left of the text: at its start");
    VKeysCursorAt(&k, 190, 200);
    Check(k.cur == 3, "the finger right of the text: at its end");
    VKeysCursorAt(&k, 198, 200);
    Check(k.cur == 3, "the finger at the right edge, all shown: at the end, not past it");

    wchar_t w[41];
    for (int i = 0; i < 40; i++)
        w[i] = L'w';
    w[40] = 0;
    VKeysOpen(&k, w, 50);
    x = VKeysLayout(&k, dc, 200);
    Check(k.view > 0 && k.visN < 40 && k.view + k.visN == 40 && x <= 200, "long: the end shown, the cursor in sight");
    int view = k.view;
    VKeysCursorAt(&k, 2, 200);
    Check(k.cur == view - 1, "the finger at the field's left edge: one back past the view");
    VKeysLayout(&k, dc, 200);
    Check(k.view == view - 1, "the view follows the cursor back");
    k.cur = 0;
    x = VKeysLayout(&k, dc, 200);
    Check(k.view == 0 && x == 0 && k.view + k.visN < 40, "the cursor at the start: the start shown");
    VKeysCursorAt(&k, 198, 200);
    Check(k.cur == k.visN + 1, "the finger at the right edge: one on past the view");
    VKeysLayout(&k, dc, 200);
    Check(k.view == 1, "the view one on");
    VKeysCursorAt(&k, 2, 200);
    Check(k.cur == 0, "the view one on, the finger at the left edge: the start");
    k.cur = 0;
    VKeysLayout(&k, dc, 200);
    VKeysCursorAt(&k, 192, 200);
    Check(k.cur <= k.visN, "8 px from the right edge: not past the view yet");
    VKeysCursorAt(&k, 100, 200);
    Check(k.cur > 0 && k.cur < k.visN, "the finger in the middle, more on the right: the gap there");

    // a text of different letters: the shown part measured from the view on
    VKeysOpen(&k, L"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiWWWWWWWWWW", 60);
    x = VKeysLayout(&k, dc, 200);
    SIZE sz;
    GetTextExtentPoint32(dc, k.text + k.view, k.visN, &sz);
    Check(k.view > 0 && x == k.dx[k.visN - 1] && x == sz.cx, "the end shown: its own width, the cursor after it");
    int at = k.view + 2;
    VKeysCursorAt(&k, 8, 200);
    Check(k.cur > k.view - 1, "8 px from the left edge: not back past the view");
    k.cur = at - 1;
    x = VKeysLayout(&k, dc, 200);
    Check(x == k.dx[k.cur - k.view - 1], "the cursor's x: after the characters shown before it");
    k.cur = k.view + 1;
    x = VKeysLayout(&k, dc, 200);
    Check(x == k.dx[0] && x > 0, "the cursor after the first shown: its right edge");

    SelectObject(dc, of);
    DeleteObject(f);
    DeleteDC(dc);
}

// The other characters (a long press: a bubble): with them each layout holds every printable ASCII character; ё only
// once; Shift's swap gives the key's own as the first other.
static void AltChecks()
{
    VKeys k;
    VKeysOpen(&k, L"", 10);
    for (int lang = 0; lang < 2; lang++) {
        k.en = lang == 1;
        bool seen[128] = { false };
        int yo = 0;
        for (int r = 0; r < VKEYS_ROWS; r++)
            for (int c = 0; c < VKeysRowLen(&k, r); c++) {
                wchar_t o[2], m = VKeysChar(&k, r, c);
                int n = VKeysAlts(&k, r, c, o);
                for (int i = -1; i < n; i++) {
                    wchar_t ch = i < 0 ? m : o[i];
                    if (ch < 128)
                        seen[ch] = true;
                    yo += ch == L'ё';
                }
            }
        bool ascii = true;
        for (int ch = 0x21; ch < 0x7F; ch++)
            if (!seen[ch] && !(ch >= L'a' && ch <= L'z') && !(ch >= L'A' && ch <= L'Z')) {
                ascii = false;
                printf("  %s: no '%c'\n", lang ? "EN" : "RU", ch);
            }
        Check(ascii, "every printable ASCII character in the layout");
        Check(yo == (lang ? 0 : 1), "ё: once, its own key");
    }
    k.en = false;
    wchar_t o[2];
    Check(VKeysAlts(&k, 0, 1, o) == 1 && o[0] == L'!', "1: !");
    Check(VKeysAlts(&k, 0, 2, o) == 2 && o[0] == L'"' && o[1] == L'@', "2: \" and @");
    Check(VKeysAlts(&k, 1, 0, o) == 0, "й: none");
    Check(VKeysAltsFor(&k, L'.', o) == 2 && o[0] == L',' && o[1] == L'…', "by the key's character: . - , …");
    Check(VKeysAltsFor(&k, L' ', o) == 0 && VKeysAltsFor(&k, L'q', o) == 0, "the space, another layout's key: none");
    k.shift = true;
    Check(VKeysAlts(&k, 0, 1, o) == 1 && o[0] == L'1', "Shift: 1 shows !, its other is 1");
    Check(VKeysAltsFor(&k, L'"', o) == 2 && o[0] == L'2' && o[1] == L'@', "Shift: found by the shown sign");
    Check(VKeysAlts(&k, 3, 7, o) == 2 && o[0] == L'<' && o[1] == L'«', "Shift on a letter: its others as they are");
    k.shift = false;
    k.en = true;
    Check(VKeysAlts(&k, 1, 2, o) == 1 && o[0] == L'€', "EN e: €");
}

// The bubble: centred over the key, kept on the screen; the finger picks the cell under it, out of it the nearest end.
static void BubbleChecks()
{
    Check(VKeysBubbleX(100, 56, 2, 56, 800) == 72, "centred over the key");
    Check(VKeysBubbleX(0, 56, 2, 56, 800) == 0, "at the left edge: kept on the screen");
    Check(VKeysBubbleX(780, 56, 2, 56, 800) == 688, "at the right edge: kept on the screen");
    Check(VKeysBubbleX(744, 56, 1, 56, 800) == 744, "one cell at the right edge: its own place");
    Check(VKeysBubbleSel(80, 72, 56, 2) == 0 && VKeysBubbleSel(127, 72, 56, 2) == 0, "over the first cell");
    Check(VKeysBubbleSel(128, 72, 56, 2) == 1 && VKeysBubbleSel(183, 72, 56, 2) == 1, "over the second");
    Check(VKeysBubbleSel(10, 72, 56, 2) == 0 && VKeysBubbleSel(500, 72, 56, 2) == 1, "out of it: the nearest end");
    Check(VKeysBubbleSel(184, 72, 56, 2) == 1, "just past its right edge: the last");
}

int main()
{
    EditChecks();
    ShiftChecks();
    KeyChecks();
    FieldChecks();
    AltChecks();
    BubbleChecks();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
