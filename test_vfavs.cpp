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
    Check(VFavsAdd(&f, L"Дом", 55.70, 37.60) == 2 && f.n == 3 && Near(f.f[0].lat, 55.75) && Near(f.f[2].lat, 55.70),
          "favs: the same name - a new place, the old one kept (the user 2026-11-03: a new Дом took the old one's)");
    VFavsDel(&f, 2);
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

    // the marks' signs (the user 2026-11-02: changeable; 2026-11-03: not tied to the name)
    VFavsClear(&f);
    VFavsAdd(&f, L"Дом", 1, 1);
    VFavsAdd(&f, L"Дача", 2, 2);
    Check(f.f[0].icon == VFAV_STAR && f.f[1].icon == VFAV_STAR, "favs: a new one - the star, whatever its name");
    f.f[0].icon = VFAV_HOME;
    Check(VFavsAdd(&f, L"Дом", 3, 3) == 2 && f.n == 3 && f.f[0].icon == VFAV_HOME && Near(f.f[0].lat, 1) &&
          f.f[2].icon == VFAV_STAR, "favs: a second Дом - a new one, the first as it was");
    Check(VFavsRename(&f, 1, L"Дом") == 1 && f.n == 3 && !wcscmp(f.f[1].name, L"Дом") && Near(f.f[1].lat, 2) &&
          !wcscmp(f.f[0].name, L"Дом"), "favs: renamed as another's name - both kept");
    Check(VFavsRename(&f, 5, L"X") == -1 && VFavsRename(&f, f.n, L"X") == -1 && VFavsRename(&f, 0, L"") == -1 &&
          VFavsRename(&f, -1, L"X") == -1 && !wcscmp(f.f[0].name, L"Дом"), "favs: no such / no name - not renamed");
    Check(VFavsRename(&f, 1, L"Я") == 1 && !wcscmp(f.f[1].name, L"Я") && f.f[0].icon == VFAV_HOME,
          "favs: renamed to one letter, its sign kept");
    Check(VFavsRename(&f, 2, L"Очень длинное название для короткого имени") == 2 && wcslen(f.f[2].name) == VFAV_NAME - 1,
          "favs: renamed to a long name - cut");
    f.f[1].icon = VFAV_FUEL;
    Check(VFavsSave(&f, path) && VFavsLoad(&g, path) && g.n == 3 && g.f[0].icon == VFAV_HOME && g.f[1].icon ==
          VFAV_FUEL && g.f[2].icon == VFAV_STAR, "favs: the signs read back as written");
    t = _wfopen(path, L"wb");
    fputs("\xD0\x94\xD0\xBE\xD0\xBC\t1\t2\r\nA\t1\t2\t4\r\nB\t1\t2\t99\r\nC\t1\t2\t-1\r\nD\t1\t2\t11\nE\t1\t2\t12\n"
          "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\t1\t2\t3\n", t);
    fclose(t);
    Check(VFavsLoad(&g, path) && g.n == 6 && g.f[0].icon == VFAV_STAR && g.f[1].icon == 4 && g.f[2].icon ==
          VFAV_STAR && g.f[3].icon == VFAV_STAR && g.f[4].icon == VFAV_ICONS - 1 && g.f[5].icon == VFAV_STAR,
          "favs: an older file (no sign) - the star; a bad sign - the star; a name too long - left out");

    // the order (the user 2026-11-14: up and down in the settings; VestaHome's widget takes the first ones)
    VFavsClear(&f);
    VFavsAdd(&f, L"A", 1, 1);
    VFavsAdd(&f, L"B", 2, 2);
    VFavsAdd(&f, L"C", 3, 3);
    f.f[1].icon = VFAV_HOME;
    Check(VFavsMove(&f, 1, -1) == 0 && !wcscmp(f.f[0].name, L"B") && f.f[0].icon == VFAV_HOME && Near(f.f[0].lat, 2) &&
          !wcscmp(f.f[1].name, L"A") && Near(f.f[1].lat, 1) && f.n == 3, "favs: moved up, its place and sign with it");
    Check(VFavsMove(&f, 1, 1) == 2 && !wcscmp(f.f[2].name, L"A") && !wcscmp(f.f[1].name, L"C"), "favs: moved down");
    Check(VFavsMove(&f, 0, -1) == -1 && VFavsMove(&f, 2, 1) == -1 && VFavsMove(&f, -1, 1) == -1 &&
          VFavsMove(&f, 3, -1) == -1 && !wcscmp(f.f[0].name, L"B") && !wcscmp(f.f[1].name, L"C") &&
          !wcscmp(f.f[2].name, L"A"), "favs: the first up, the last down, no such - not moved");
    Check(VFavsMove(&f, 0, 1) == 1 && !wcscmp(f.f[0].name, L"C") && !wcscmp(f.f[1].name, L"B"), "favs: the first down");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
