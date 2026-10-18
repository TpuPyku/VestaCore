// vnight.h - VestaCore: the night mode shared by VestaHome and OSMapsCE.
//
// One value, HKEY_CURRENT_USER\Software\Vesta "Night" (DWORD 0 / 1): the program that switches the mode writes it,
// the others look at it about once a second and follow. The unit's registry is reset on boot - the mode then starts
// from each program's own setting until one of them switches it.
#ifndef VNIGHT_H
#define VNIGHT_H

#include <windows.h>

int  NightShared();            // 0 day, 1 night, -1 nobody set it yet
void NightShare(bool night);   // this program switched it

#endif
