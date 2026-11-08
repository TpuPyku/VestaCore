// vroute.h - VestaCore: the route's line shared between programs (the user 2026-11-08: GpsSim drives along the route
// built in OSMapsCE). OSMapsCE publishes each new route's line into the shared memory "VestaRouteLine"; others read it.
// Plain Win32 (COREDLL on CE), no C++11.
#ifndef VROUTE_H
#define VROUTE_H

#include <windows.h>

enum { VROUTE_MAX = 16000 };   // points (a 1000 km route has some 10 000)

// The line (n points, lon / lat in degrees, as pairs) - every point kept while n <= VROUTE_MAX, else evenly fewer.
void VRoutePublish(const double* lonLat, int n);
// The line published last: up to cap points, latitude / longitude in millionths of a degree; its number (0 - none),
// *serial - which publication it is (a new route: another serial).
int VRouteRead(int* latE6, int* lonE6, int cap, LONG* serial);

#endif
