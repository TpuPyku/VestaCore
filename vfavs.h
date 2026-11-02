// vfavs.h - VestaCore: the favourite places (the user 2026-10-31): short names ("Дом", "Работа") and their points.
// OSMapsCE keeps them (favorites.txt beside its exe), VestaHome reads the first ones into its map widget. The file:
// UTF-8 lines "name<TAB>lat<TAB>lon", '#' a comment.
#ifndef VFAVS_H
#define VFAVS_H

#include <windows.h>

enum { VFAV_MAX = 12, VFAV_NAME = 24 };

struct VFav { wchar_t name[VFAV_NAME]; double lat, lon; };
struct VFavs { VFav f[VFAV_MAX]; int n; };

void VFavsClear(VFavs* f);
int  VFavsAdd(VFavs* f, const wchar_t* name, double lat, double lon);   // its index (the same name: replaced); -1 none
void VFavsDel(VFavs* f, int i);
bool VFavsLoad(VFavs* f, const wchar_t* path);   // false: no file (none then)
bool VFavsSave(const VFavs* f, const wchar_t* path);

#endif
