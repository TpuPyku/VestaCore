// test_vfavs.cpp - the favourite places (vfavs.cpp) on the PC: the list, the file, the names (test_vfavs.bat).
#include <stdio.h>
#include <string.h>
#include "vfavs.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static bool Near(double a, double b)
{
    return a - b < 1e-6 && b - a < 1e-6;
}

int main()
{
    VFavs f;
    VFavsClear(&f);
    Check(f.n == 0 && VFavsAdd(&f, L"Дом", 55.75, 37.62) == 0 && VFavsAdd(&f, L"Работа", 55.80, 37.50) == 1 &&
          f.n == 2 && !wcscmp(f.f[1].name, L"Работа") && Near(f.f[1].lat, 55.80), "favs: added in order");
    Check(VFavsAdd(&f, L"Дом", 55.70, 37.60) == 0 && f.n == 2 && Near(f.f[0].lat, 55.70),
          "favs: the same name - its place replaced, where it was");
    Check(VFavsAdd(&f, L"", 55.0, 37.0) == -1 && f.n == 2, "favs: no name - not added");
    VFavsAdd(&f, L"Очень длинное название для короткого имени", 1, 2);
    Check(f.n == 3 && wcslen(f.f[2].name) == VFAV_NAME - 1, "favs: a long name cut");
    for (int i = 0; i < 20; i++) {
        wchar_t nm[8];
        _snwprintf(nm, 8, L"Т%d", i);
        VFavsAdd(&f, nm, i, i);
    }
    Check(f.n == VFAV_MAX && VFavsAdd(&f, L"ещё", 1, 1) == -1, "favs: full - not added");
    VFavsDel(&f, 1);
    Check(f.n == VFAV_MAX - 1 && !wcscmp(f.f[0].name, L"Дом") && wcscmp(f.f[1].name, L"Работа") != 0,
          "favs: deleted, the rest moved up");
    VFavsDel(&f, 99);
    VFavsDel(&f, f.n);
    VFavsDel(&f, -1);
    Check(f.n == VFAV_MAX - 1, "favs: no such - nothing");
    VFav last = f.f[f.n - 1];
    VFavsDel(&f, 0);
    Check(f.n == VFAV_MAX - 2 && !wcscmp(f.f[f.n - 1].name, last.name) && wcscmp(f.f[0].name, L"Дом") != 0,
          "favs: the first deleted, the last moved up too");
    VFavsDel(&f, f.n - 1);
    Check(f.n == VFAV_MAX - 3 && wcscmp(f.f[f.n - 1].name, last.name) != 0, "favs: the last deleted");
    VFavsClear(&f);
    Check(VFavsAdd(&f, L"A", 1, 2) == 0 && f.n == 1, "favs: a one-letter name");
    VFavsAdd(&f, L"Дом", 55.70, 37.60);

    const wchar_t* path = L"obj\\favorites.txt";
    SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);   // (a run before left it as it was)
    DeleteFileW(path);
    Check(VFavsSave(&f, path) && VFavsSave(&f, path), "favs: saved, and again over it");
    VFavs g;
    Check(VFavsLoad(&g, path) && g.n == f.n && !wcscmp(g.f[1].name, L"Дом") && Near(g.f[1].lat, 55.70) &&
          Near(g.f[1].lon, 37.60), "favs: read back as written (UTF-8, the places to 1e-6)");
    FILE* t = _wfopen(path, L"wb");
    fputs("\xEF\xBB\xBF# a comment\r\n#x\t1\t2\r\n\xD0\x94\xD0\xBE\xD0\xBC\t55.1\t37.2\r\nbad line\r\n\t1\t2\r\nX\t1\r\n", t);
    fclose(t);
    Check(VFavsLoad(&g, path) && g.n == 1 && !wcscmp(g.f[0].name, L"Дом") && Near(g.f[0].lon, 37.2),
          "favs: written by hand - a BOM, a comment, the bad lines left out");
    Check(!VFavsLoad(&g, L"obj\\none.txt") && g.n == 0, "favs: no file - none");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
