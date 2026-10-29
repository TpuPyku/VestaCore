// vlogdir.cpp - VestaCore: the log folder on removable media (see vlogdir.h).
#include "vlogdir.h"
#include <stdio.h>
#include <string.h>

#ifdef UNDER_CE
static bool MakeLogs(const wchar_t* vol, wchar_t* dir)   // vol\VestaLogs\ made there; false - cannot
{
    DWORD a = GetFileAttributes(vol);
    if (a == 0xFFFFFFFF || !(a & FILE_ATTRIBUTE_DIRECTORY))
        return false;
    _snwprintf(dir, MAX_PATH, L"%s\\VestaLogs\\", vol);
    dir[MAX_PATH - 1] = 0;
    CreateDirectory(dir, NULL);
    DWORD b = GetFileAttributes(dir);
    return b != 0xFFFFFFFF && (b & FILE_ATTRIBUTE_DIRECTORY);
}

// Another device than the Vesta's head unit (the user 2026-10-29: OSMapsCE on a Shturmann NSL 500 - its card is
// "\SDMMC", the Vesta's built-in one's name): the first storage card of the root (FILE_ATTRIBUTE_TEMPORARY)
static bool AnyCard(wchar_t* dir)
{
    WIN32_FIND_DATA fd;
    HANDLE ff = FindFirstFile(L"\\*", &fd);
    if (ff == INVALID_HANDLE_VALUE)
        return false;
    bool ok = false;
    do {
        wchar_t vol[MAX_PATH];
        _snwprintf(vol, MAX_PATH, L"\\%s", fd.cFileName);
        ok = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (fd.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) &&
             MakeLogs(vol, dir);
    } while (!ok && FindNextFile(ff, &fd));
    FindClose(ff);
    return ok;
}
#endif

bool LogDir(wchar_t* dir, const wchar_t* exeDir)
{
#ifdef UNDER_CE
    static wchar_t s_dir[MAX_PATH];
    static DWORD   s_tick;
    static bool    s_ok, s_tried;
    if (!s_tried || GetTickCount() - s_tick > 30000) {
        static const wchar_t* vols[4] = { L"\\SDMemory", L"\\USB Disk", L"\\USBDisk", L"\\USBDevice" };
        s_tried = true;
        s_tick = GetTickCount();
        s_ok = false;
        for (int i = 0; i < 4 && !s_ok; i++)
            s_ok = MakeLogs(vols[i], s_dir);
        // the Vesta's unit (its shell's folder there): never its built-in flash (\SDMMC...) - elsewhere any card
        // else the exe's folder (the user 2026-10-29: a Shturmann NSL 500 gave no log - its card is not marked
        // removable)
        if (!s_ok && GetFileAttributes(L"\\ResidentFlash\\navigation\\app") == 0xFFFFFFFF) {
            s_ok = AnyCard(s_dir);
            if (!s_ok && exeDir && exeDir[0]) {
                wcsncpy(s_dir, exeDir, MAX_PATH - 1);
                s_ok = true;
            }
        }
    }
    if (s_ok)
        wcscpy(dir, s_dir);
    return s_ok;
#else
    wcsncpy(dir, exeDir ? exeDir : L"", MAX_PATH - 1);
    dir[MAX_PATH - 1] = 0;
    return true;
#endif
}
