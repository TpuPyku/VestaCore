// vlogdir.cpp - VestaCore: the log folder on removable media (see vlogdir.h).
#include "vlogdir.h"
#include <stdio.h>
#include <string.h>

bool LogDir(wchar_t* dir, const wchar_t* exeDir)
{
#ifdef UNDER_CE
    static wchar_t s_dir[MAX_PATH];
    static DWORD   s_tick;
    static bool    s_ok, s_tried;
    (void)exeDir;
    if (!s_tried || GetTickCount() - s_tick > 30000) {
        static const wchar_t* vols[4] = { L"\\SDMemory", L"\\USB Disk", L"\\USBDisk", L"\\USBDevice" };
        s_tried = true;
        s_tick = GetTickCount();
        s_ok = false;
        for (int i = 0; i < 4 && !s_ok; i++) {
            DWORD a = GetFileAttributes(vols[i]);
            if (a == 0xFFFFFFFF || !(a & FILE_ATTRIBUTE_DIRECTORY))
                continue;
            _snwprintf(s_dir, MAX_PATH, L"%s\\VestaLogs\\", vols[i]);
            s_dir[MAX_PATH - 1] = 0;
            CreateDirectory(s_dir, NULL);
            DWORD b = GetFileAttributes(s_dir);
            s_ok = b != 0xFFFFFFFF && (b & FILE_ATTRIBUTE_DIRECTORY);
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
