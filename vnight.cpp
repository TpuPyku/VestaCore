// vnight.cpp - VestaCore: the night mode shared by VestaHome and OSMapsCE (see vnight.h).
#include "vnight.h"

int NightShared()
{
    HKEY k;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Vesta", 0, KEY_READ, &k) != ERROR_SUCCESS)
        return -1;
    DWORD v = 0, size = sizeof(v), type = 0;
    LONG r = RegQueryValueEx(k, L"Night", NULL, &type, (BYTE*)&v, &size);
    RegCloseKey(k);
    return r == ERROR_SUCCESS && type == REG_DWORD ? (v ? 1 : 0) : -1;
}

void NightShare(bool night)
{
    HKEY k;
    DWORD disp;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Vesta", 0, NULL, 0, KEY_WRITE, NULL, &k, &disp) != ERROR_SUCCESS)
        return;
    DWORD v = night ? 1 : 0;
    RegSetValueEx(k, L"Night", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    RegCloseKey(k);
}
