// vversion.h - VestaCore: a program's version "<big releases>.<significant changes>.<the build's date YYMMDD>"
// (the user 2026-10-23: daily builds, e.g. "1.03.261023"); the first two as the program gives them ("1.03", "0.5"),
// raised by hand. The date is __DATE__ of the file that includes this one: include it where the program's main file
// is (VestaHome's one translation unit, launch.cpp, OSMapsCE's main.cpp).
#ifndef VVERSION_H
#define VVERSION_H

#include <windows.h>
#include <string.h>
#include <stdlib.h>

static const wchar_t* AppVersion(const wchar_t* release)   // "1.03" -> "1.03.261023"
{
    static wchar_t v[24];
    if (!v[0]) {
        static const char kMon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
        const char* d = __DATE__;   // "Oct 23 2026"
        int m = 1;
        for (int i = 0; i < 12; i++)
            if (!strncmp(kMon + i * 3, d, 3))
                m = i + 1;
        _snwprintf(v, 24, L"%s.%c%c%02d%02d", release, d[9], d[10], m, atoi(d + 4));
        v[23] = 0;
    }
    return v;
}

#endif
