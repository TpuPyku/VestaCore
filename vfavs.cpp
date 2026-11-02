// vfavs.cpp - VestaCore: the favourite places (vfavs.h).
#include "vfavs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void VFavsClear(VFavs* f)
{
    memset(f, 0, sizeof(*f));
}

static int IconOf(const wchar_t* name)   // a new one's sign: Дом, Работа, Дача - theirs, else the star
{
    static const struct { const wchar_t* name; int icon; } kBy[] = {
        { L"Дом", VFAV_HOME }, { L"дом", VFAV_HOME }, { L"Работа", VFAV_WORK }, { L"работа", VFAV_WORK },
        { L"Дача", VFAV_COTTAGE }, { L"дача", VFAV_COTTAGE } };
    for (int i = 0; i < (int)(sizeof(kBy) / sizeof(kBy[0])); i++)
        if (!wcscmp(name, kBy[i].name))
            return kBy[i].icon;
    return VFAV_STAR;
}

static void NameCopy(wchar_t* to, const wchar_t* name)
{
    wcsncpy(to, name, VFAV_NAME - 1);
    to[VFAV_NAME - 1] = 0;
}

static int Find(const VFavs* f, const wchar_t* name)   // (the name as kept: cut)
{
    wchar_t v[VFAV_NAME];
    NameCopy(v, name);
    for (int i = 0; i < f->n; i++)
        if (!wcscmp(f->f[i].name, v))
            return i;
    return -1;
}

int VFavsAdd(VFavs* f, const wchar_t* name, double lat, double lon)
{
    if (!name || !name[0])
        return -1;
    int i = Find(f, name);
    if (i < 0) {
        if (f->n >= VFAV_MAX)
            return -1;
        i = f->n++;
        NameCopy(f->f[i].name, name);
        f->f[i].icon = IconOf(f->f[i].name);
    }
    f->f[i].lat = lat;
    f->f[i].lon = lon;
    return i;
}

int VFavsRename(VFavs* f, int i, const wchar_t* name)
{
    if (i < 0 || i >= f->n || !name || !name[0])
        return -1;
    int other = Find(f, name);
    if (other >= 0 && other != i) {
        VFavsDel(f, other);
        i -= other < i;
    }
    NameCopy(f->f[i].name, name);
    return i;
}

void VFavsDel(VFavs* f, int i)
{
    if (i < 0 || i >= f->n)
        return;
    memmove(f->f + i, f->f + i + 1, (f->n - i - 1) * sizeof(VFav));
    f->n--;
}

static int SignOf(const char* tab, int byName)   // the sign's column ("<TAB>n"); none: by the name; bad: the star
{
    if (!tab)
        return byName;
    int icon = atoi(tab + 1);
    return icon >= 0 && icon < VFAV_ICONS ? icon : VFAV_STAR;
}

static void TakeLine(VFavs* f, char* s)   // "name<TAB>lat<TAB>lon[<TAB>sign]"; anything else left out
{
    char* t1 = strchr(s, '\t');
    char* t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t2 || t1 == s || *s == '#')
        return;
    *t1 = 0;
    wchar_t name[VFAV_NAME * 2];
    int i = MultiByteToWideChar(CP_UTF8, 0, s, -1, name, VFAV_NAME * 2) > 0 ?
            VFavsAdd(f, name, atof(t1 + 1), atof(t2 + 1)) : -1;
    if (i >= 0)
        f->f[i].icon = SignOf(strchr(t2 + 1, '\t'), f->f[i].icon);
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
        int len = _snprintf(line, sizeof(line), "%s\t%.6f\t%.6f\t%d\r\n", name, f->f[i].lat, f->f[i].lon,
                            f->f[i].icon);
        DWORD put = 0;
        ok = len > 0 && WriteFile(h, line, len, &put, NULL) && put == (DWORD)len;
    }
    CloseHandle(h);
    return ok;
}
