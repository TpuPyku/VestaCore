// vgps.h - VestaCore GPS: the head unit's NMEA port (Lada Vesta MMC: COM5 = a copy of the CAN module's
// stream, 115200), shared between the programs that use it (VestaHome, OSMapsCE, YaMapsCE).
//
// One program reads the port, the others read what it publishes: the first one to take the named mutex
// "VestaGPS_Owner" opens the port and puts its state and the last checked lines into the shared memory
// "VestaGPS"; the others copy them from there. When the reader closes (or dies - the mutex is then
// abandoned), the next program takes the port over. Every program works alone as well.
//
// The stream also carries the CAN module's own sentences (G3NavHMI's rn2::CanMessageParser):
// "@,<speed km/h>,<CAN module version>*hh" and "@ANG,<steering angle | null>,*hh"; both are parsed here.
//
// Plain Win32 (COREDLL on CE), no C++11. The port is opened read-only and left as the shell set it up.
#ifndef VGPS_H
#define VGPS_H

#include <windows.h>

enum { GPS_NO_PORT = 0, GPS_BUSY, GPS_OPEN, GPS_SCANNING };

struct GpsState {
    int     status;          // GPS_* (of the program that reads the port)
    wchar_t port[16];        // port being used / probed
    DWORD   baud;
    int     lines, badLines; // sentences with a good / bad checksum since (re)configure
    char    last[84];        // last good sentence, for settings screens
    int     found;           // scan result: 1 found, -1 nothing, 0 no scan
    int     valid;           // last RMC had status 'A'
    double  lon, lat;
    double  speedKmh, course, alt;
    int     sats;
    DWORD   fixTick;         // GetTickCount() of the last valid fix
    DWORD   dataTick;        // GetTickCount() of the last sentence
    int     date, time;      // yyyymmdd, hhmmss (UTC) of the last valid RMC, 0 = unknown
    DWORD   bytes;           // read from the port since start
    int     carSpeed;        // "@,<km/h>": the car's speed from the CAN module
    int     angle10;         // "@ANG": steering angle, tenths of a degree
    DWORD   carTick, angTick;   // when they came (0 = never)
    char    canVer[8];       // the CAN module's firmware ("1.19")
    int     owner;           // 1 = this program reads the port, 0 = another one does (shared)
    wchar_t ownerApp[24];    // the program reading the port
};

// Called by the module (from its thread): a log line; whether to close a program holding the port.
typedef void (*GpsLogFn)(const char* line);
typedef bool (*GpsKillFn)(const wchar_t* exe);
// Taps for monitors: every checked line ('$' or '@', checksum ok, without CR/LF), and - only while this
// program reads the port - the raw bytes as read. Called from the module's thread.
typedef void (*GpsLineFn)(const char* line);
typedef void (*GpsRawFn)(const char* buf, DWORD len);

struct GpsConfig {
    HWND           notify;        // gets msg (wParam 1 = a scan finished) at most once a second
    UINT           msg;
    const wchar_t* port;          // "COM5:"; PC test build: "<file>.nmea" from exeDir plays a recording
    DWORD          baud;          // 0 = leave the port's speed alone
    const wchar_t* app;           // this program's name, shown to the others ("VestaHome")
    const wchar_t* killExe;       // close this program when it holds the port (NULL = never)
    GpsKillFn      kill;
    GpsLogFn       log;           // NULL = no log
    const wchar_t* exeDir;        // with a trailing '\', for the PC's .nmea recordings
    GpsLineFn      onLine;        // NULL = none
    GpsRawFn       onRaw;         // NULL = none
};

void GpsStart(const GpsConfig* cfg);
void GpsStop();
void GpsGet(GpsState* s);                             // also re-arms the notify message
void GpsConfigure(const wchar_t* port, DWORD baud);   // reopen with new settings (used when reading the port)
void GpsScan();                                       // look for NMEA on COM1..COM9 (only when reading the port)

#endif
