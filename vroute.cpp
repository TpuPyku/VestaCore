// vroute.cpp - VestaCore: the route's line shared between programs (vroute.h).
#include "vroute.h"

#define VROUTE_SHM   L"VestaRouteLine"
#define VROUTE_MAGIC 0x54524F56   // "VORT"

struct RouteShm {
    DWORD         magic;
    volatile LONG serial;   // odd while written
    int           n;
    int           pt[VROUTE_MAX][2];   // latitude, longitude, millionths of a degree
};

static RouteShm* Map()   // kept for the program's life (the line stays while any program has it open)
{
    static RouteShm* s_shm;
    if (!s_shm) {
        HANDLE h = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(RouteShm), VROUTE_SHM);
        if (h)
            s_shm = (RouteShm*)MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(RouteShm));
    }
    return s_shm;
}

void VRoutePublish(const double* lonLat, int n)
{
    RouteShm* s = Map();
    if (!s || n < 0)
        return;
    InterlockedIncrement((LONG*)&s->serial);   // odd: being written
    int m = n <= VROUTE_MAX ? n : VROUTE_MAX;
    for (int i = 0; i < m; i++) {
        int k = n <= VROUTE_MAX ? i : (int)((double)i * (n - 1) / (m - 1));
        s->pt[i][0] = (int)(lonLat[k * 2 + 1] * 1e6 + (lonLat[k * 2 + 1] < 0 ? -0.5 : 0.5));
        s->pt[i][1] = (int)(lonLat[k * 2] * 1e6 + (lonLat[k * 2] < 0 ? -0.5 : 0.5));
    }
    s->n = m;
    s->magic = VROUTE_MAGIC;
    InterlockedIncrement((LONG*)&s->serial);   // even: done
}

int VRouteRead(int* latE6, int* lonE6, int cap, LONG* serial)
{
    RouteShm* s = Map();
    if (!s || s->magic != VROUTE_MAGIC)
        return 0;
    for (int tries = 0; tries < 20; tries++) {
        LONG a = s->serial;
        if (a & 1) {
            Sleep(5);
            continue;
        }
        int n = s->n < cap ? s->n : cap;
        for (int i = 0; i < n; i++) {
            latE6[i] = s->pt[i][0];
            lonE6[i] = s->pt[i][1];
        }
        if (s->serial == a) {
            if (serial)
                *serial = a;
            return n;
        }
    }
    return 0;
}
