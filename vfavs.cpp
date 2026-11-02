// vfavs.cpp - VestaCore: the favourite places (vfavs.h).
#include "vfavs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void VFavsClear(VFavs* f)
{
    memset(f, 0, sizeof(*f));
}

int VFavsAdd(VFavs* f, const wchar_t* name, double lat, double lon)
{
    if (!name || !name[0])
        return -1;
    VFav v;
    wcsncpy(v.name, name, VFAV_NAME - 1);
    v.name[VFAV_NAME - 1] = 0;
    v.lat = lat;
    v.lon = lon;
    for (int i = 0; i < f->n; i++)
        if (!wcscmp(f->f[i].name, v.name)) {
            f->f[i] = v;
            return i;
        }
    if (f->n >= VFAV_MAX)
        return -1;
    f->f[f->n] = v;
    return f->n++;
}

void VFavsDel(VFavs* f, int i)
{
    if (i < 0 || i >= f->n)
        return;
    memmove(f->f + i, f->f + i + 1, (f->n - i - 1) * sizeof(VFav));
    f->n--;
}

static void TakeLine(VFavs* f, char* s)   // "name<TAB>lat<TAB>lon"; anything else left out
{
    char* t1 = strchr(s, '\t');
    char* t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t2 || t1 == s || *s == '#')
        return;
    *t1 = 0;
    wchar_t name[VFAV_NAME * 2];
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, name, VFAV_NAME * 2) > 0)
        VFavsAdd(f, name, atof(t1 + 1), atof(t2 + 1));
}

bool VFavsLoad(VFavs* f, const wchar_t* path)
{
    VFavsClear(f);
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    char buf[4096];
    DWORD got = 0;
    ReadFile(h, buf, sizeof(buf) - 1, &got, NULL);
    CloseHandle(h);
    buf[got] = 0;
    char* s = buf + (got >= 3 && !memcmp(buf, "\xEF\xBB\xBF", 3) ? 3 : 0);
    while (s && *s) {
        char* nl = strchr(s, '\n');
        if (nl)
            *nl = 0;
        char* cr = strchr(s, '\r');
        if (cr)
            *cr = 0;
        TakeLine(f, s);
        s = nl ? nl + 1 : NULL;
    }
    return true;
}

bool VFavsSave(const VFavs* f, const wchar_t* path)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    bool ok = true;
    for (int i = 0; i < f->n && ok; i++) {
        char name[VFAV_NAME * 4], line[VFAV_NAME * 4 + 64];
        WideCharToMultiByte(CP_UTF8, 0, f->f[i].name, -1, name, sizeof(name), NULL, NULL);
        int len = _snprintf(line, sizeof(line), "%s\t%.6f\t%.6f\r\n", name, f->f[i].lat, f->f[i].lon);
        DWORD put = 0;
        ok = len > 0 && WriteFile(h, line, len, &put, NULL) && put == (DWORD)len;
    }
    CloseHandle(h);
    return ok;
}
