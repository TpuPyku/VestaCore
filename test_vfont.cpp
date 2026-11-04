// test_vfont.cpp - vfont.cpp on the PC: the font made is what was asked, unsmoothed. Exit code: the failed checks.
#include <windows.h>
#include <stdio.h>
#include "vfont.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

int main()
{
    HFONT f = VFontMake(L"Tahoma", 18, FW_BOLD);
    LOGFONT lf;
    Check(f && GetObject(f, sizeof(lf), &lf) == sizeof(lf), "a font");
    Check(lf.lfHeight == -18, "the character height (negative: without the leading)");
    Check(lf.lfWeight == FW_BOLD, "the weight");
    Check(lf.lfQuality == NONANTIALIASED_QUALITY, "no smoothing");
    Check(lf.lfCharSet == DEFAULT_CHARSET, "the character set");
    Check(!wcscmp(lf.lfFaceName, L"Tahoma"), "the face");
    Check(!lf.lfWidth && !lf.lfItalic && !lf.lfUnderline && !lf.lfStrikeOut, "plain: no width, italic, lines");
    DeleteObject(f);
    const wchar_t* longest = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcde";   // LF_FACESIZE - 1 characters
    f = VFontMake(longest, 12, FW_NORMAL);
    Check(f && GetObject(f, sizeof(lf), &lf) == sizeof(lf) && !wcscmp(lf.lfFaceName, longest),
          "the longest face name whole");
    DeleteObject(f);
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
