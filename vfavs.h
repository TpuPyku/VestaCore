// vfavs.h - VestaCore: the favourite places (the user 2026-10-31): short names ("Дом", "Работа") and their points.
// OSMapsCE keeps them (favorites.txt beside its exe), VestaHome reads the first ones into its map widget. The file:
// UTF-8 lines "name<TAB>lat<TAB>lon<TAB>sign", '#' a comment (no sign - an older file: by the name).
// The sign is the mark's on the map (the user 2026-11-02: Дом, Работа and a few more, changeable).
#ifndef VFAVS_H
#define VFAVS_H

#include <windows.h>

enum { VFAV_MAX = 12, VFAV_NAME = 24 };
enum { VFAV_STAR, VFAV_HOME, VFAV_WORK, VFAV_FLAG, VFAV_HEART, VFAV_FUEL, VFAV_ICONS };

struct VFav { wchar_t name[VFAV_NAME]; double lat, lon; int icon; };
struct VFavs { VFav f[VFAV_MAX]; int n; };

void VFavsClear(VFavs* f);
int  VFavsAdd(VFavs* f, const wchar_t* name, double lat, double lon);   // its index (the same name: replaced, its sign
                                                                        // kept; a new one's sign by the name); -1 none
int  VFavsRename(VFavs* f, int i, const wchar_t* name);   // its index (another of that name goes); -1 not renamed
void VFavsDel(VFavs* f, int i);
bool VFavsLoad(VFavs* f, const wchar_t* path);   // false: no file (none then)
bool VFavsSave(const VFavs* f, const wchar_t* path);

#endif
