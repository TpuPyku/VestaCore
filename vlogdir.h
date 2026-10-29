// vlogdir.h - VestaCore: where the programs keep their logs and records on the head unit.
//
// Only removable media: the SD card or a USB disk (folder VestaLogs on it); never the unit's built-in flash
// (\SDMMC, \ResidentFlash, \StorageFlash), which would wear out under logs. Without a card or a disk there
// is no log. On another device (no Vesta shell's folder): its first storage card, whatever its name, else the
// exe's folder (exeDir, when given). Looked up again every 30 s, a card may be put in later. PC test builds: the
// exe's folder.
#ifndef VLOGDIR_H
#define VLOGDIR_H

#include <windows.h>

// dir gets the folder with a trailing '\' (MAX_PATH); false = nowhere to log now.
bool LogDir(wchar_t* dir, const wchar_t* exeDir);

#endif
