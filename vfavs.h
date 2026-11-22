// vfavs.h - VestaCore: the favourite places (the user 2026-10-31): short names ("Дом", "Работа") and their points.
// OSMapsCE keeps them (favorites.txt beside its exe), VestaHome reads the first ones into its map widget. The file:
// UTF-8 lines "name<TAB>lat<TAB>lon<TAB>sign[<TAB>address]", '#' a comment (no sign - an older file: the star).
// The sign is the mark's on the map (the user 2026-11-02: Дом, Работа and a few more, changeable).
#ifndef VFAVS_H
#define VFAVS_H

#include <windows.h>

enum { VFAV_MAX = 12, VFAV_NAME = 24, VFAV_ADDR = 80 };
enum { VFAV_STAR, VFAV_HOME, VFAV_WORK, VFAV_FLAG, VFAV_HEART, VFAV_FUEL, VFAV_COTTAGE, VFAV_FLATS, VFAV_SHOP,
       VFAV_HOSPITAL, VFAV_SCHOOL, VFAV_PARKING, VFAV_ICONS };   // (their numbers are in the files: added at the end)

// addr - its address (the user 2026-11-22: the search finds a favourite by its street too), "" not known yet; the
// file's fifth column
struct VFav { wchar_t name[VFAV_NAME]; double lat, lon; int icon; wchar_t addr[VFAV_ADDR]; };
struct VFavs { VFav f[VFAV_MAX]; int n; };

void VFavsClear(VFavs* f);
int  VFavsAdd(VFavs* f, const wchar_t* name, double lat, double lon);   // its index (always a new one, the star);
                                                                        // -1 none (no name, full)
int  VFavsRename(VFavs* f, int i, const wchar_t* name);   // its index (names may repeat); -1 not renamed
void VFavsDel(VFavs* f, int i);
int  VFavsMove(VFavs* f, int i, int dir);   // dir -1 up, 1 down: its new index; -1 not moved (the end)
bool VFavsLoad(VFavs* f, const wchar_t* path);   // false: no file (none then)
bool VFavsSave(const VFavs* f, const wchar_t* path);
void VFavsSetAddr(VFavs* f, int i, const wchar_t* addr);
// The favourites with every word of the query in their name or address (the case aside, ё as е): their indexes into
// idx in their order, cap at most; their count (0: none, or an empty query). The user 2026-11-21: OSMapsCE's search
// finds them too; 2026-11-22: by the street as well.
int  VFavsFind(const VFavs* f, const wchar_t* query, int* idx, int cap);

#endif
