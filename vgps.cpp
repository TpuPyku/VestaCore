// vgps.cpp - VestaCore GPS (see vgps.h): NMEA + the CAN module's '@' lines from the unit's port, one
// program reading it and publishing to the others through shared memory.
#include "vgps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ---- shared memory: the reader's state and its last checked lines.
// The reader writes under a sequence lock: seq odd while writing, even when done; a copy is taken only
// between two equal even values.
#define VGPS_SHM_NAME  L"VestaGPS"
#define VGPS_MUTEX     L"VestaGPS_Owner"
#define VGPS_MAGIC     0x53504756   // "VGPS"
enum { kShmLines = 64, kShmLineLen = 100 };

struct GpsShm {
    DWORD         magic, version;
    volatile LONG seq;              // state's sequence lock
    DWORD         beat;             // GetTickCount() of the last publish
    GpsState      state;            // the reader's state (owner = 1 there)
    volatile LONG lineSeq;          // lines written so far
    char          lines[kShmLines][kShmLineLen];
};

static CRITICAL_SECTION s_cs;
static GpsState         s_state;
static GpsConfig        s_cfg;
static wchar_t          s_cfgPort[16], s_cfgApp[24], s_cfgKill[32], s_cfgDir[MAX_PATH];
static HANDLE           s_thread, s_wake, s_mutex, s_shmMap;
static GpsShm*          s_shm;
static volatile bool    s_stop, s_reopen, s_scanReq;
static wchar_t          s_port[16];
static DWORD            s_baud;
static DWORD            s_lastNotify;
static volatile LONG    s_notifyPosted;
static char             s_line[128];
static int              s_lineLen;

static const DWORD      kReadPauseMs = 100;  // let the driver collect a chunk instead of spinning on bytes

// Where the reader's CPU time goes (HU 2026-10-18: the thread took ~80 % of the CPU), logged once a minute.
struct ReadStats {
    DWORD start, polls, pollMs, reads, empty, readMs, parseMs, bytes;
};
static ReadStats s_rs;

static DWORD ThreadMs()   // this thread's kernel + user time; the low parts are enough for differences
{
    FILETIME c, e, k, u;
    if (!GetThreadTimes(GetCurrentThread(), &c, &e, &k, &u))
        return 0;
    return (k.dwLowDateTime + u.dwLowDateTime) / 10000;
}

static void Logf(const char* fmt, ...)
{
    if (!s_cfg.log)
        return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    s_cfg.log(buf);
}

// At most one message a second, and never a second one while the window hasn't read the state yet.
static void Notify(bool force)
{
    DWORD now = GetTickCount();
    if (!s_cfg.notify || (!force && now - s_lastNotify < 1000))
        return;
    if (InterlockedExchange((LONG*)&s_notifyPosted, 1))
        return;
    s_lastNotify = now;
    if (!PostMessage(s_cfg.notify, s_cfg.msg, 0, 0))
        s_notifyPosted = 0;
}

// ---- publishing (the reader) and copying (the others)
static void Publish()   // under s_cs
{
    if (!s_shm)
        return;
    InterlockedIncrement((LONG*)&s_shm->seq);   // odd: being written
    s_shm->state = s_state;
    s_shm->beat = GetTickCount();
    InterlockedIncrement((LONG*)&s_shm->seq);   // even: done
}

static void PublishLine(const char* s)
{
    if (!s_shm)
        return;
    LONG n = InterlockedIncrement((LONG*)&s_shm->lineSeq);
    char* dst = s_shm->lines[(n - 1) % kShmLines];
    strncpy(dst, s, kShmLineLen - 1);
    dst[kShmLineLen - 1] = 0;
}

static int HexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// "5549.3932" + 'N' -> 55.823220
static double NmeaDeg(const char* v, char hemi)
{
    double raw = atof(v);
    int deg = (int)(raw / 100);
    double d = deg + (raw - deg * 100) / 60.0;
    return (hemi == 'S' || hemi == 'W') ? -d : d;
}

static void BadLine()
{
    EnterCriticalSection(&s_cs);
    s_state.badLines++;
    LeaveCriticalSection(&s_cs);
}

// "@,<speed>,<version>" and "@ANG,<angle>," as G3NavHMI reads them: exactly 3 fields (FUN_0041cdb0/cf40).
static void ParseAt(char* s)
{
    char* f[8];
    int n = 0;
    f[n++] = s + 1;
    for (char* p = s + 1; *p && n < 8; p++)
        if (*p == ',') {
            *p = 0;
            f[n++] = p + 1;
        }
    if (n != 3)
        return;
    EnterCriticalSection(&s_cs);
    if (!f[0][0] && f[1][0] >= '0' && f[1][0] <= '9') {
        s_state.carSpeed = atoi(f[1]);
        s_state.carTick = GetTickCount();
        strncpy(s_state.canVer, f[2], sizeof(s_state.canVer) - 1);
        s_state.canVer[sizeof(s_state.canVer) - 1] = 0;
    } else if (!strcmp(f[0], "ANG") && f[1][0] && strcmp(f[1], "null")) {
        double a = atof(f[1]);
        s_state.angle10 = (int)(a * 10 + (a < 0 ? -0.5 : 0.5));
        s_state.angTick = GetTickCount();
    }
    LeaveCriticalSection(&s_cs);
}

// One line ('$' or '@'). The port may carry other data between sentences: everything must be printable;
// a checksum is checked when present ("*hh", XOR between the first character and '*'). A '$' line without
// it is still accepted when it has all its fields (as GpsGate's filter does). Returns true for a good one.
static bool ParseSentence(char* s)
{
    if (s[0] != '$' && s[0] != '@')
        return false;
    for (const char* p = s; *p; p++)
        if ((unsigned char)*p < 0x20 || (unsigned char)*p > 0x7E) {
            BadLine();
            return false;
        }
    char* star = strrchr(s, '*');
    bool checked = star != NULL;
    if (star) {
        unsigned char sum = 0;
        for (char* p = s + 1; p < star; p++)
            sum ^= (unsigned char)*p;
        int hi = HexDigit(star[1]), lo = HexDigit(star[2]);
        if (hi < 0 || lo < 0 || sum != hi * 16 + lo) {
            BadLine();
            return false;
        }
        // monitors get the whole checked line (with its "*hh"); the CAN module's lines are short
        if (s_cfg.onLine)
            s_cfg.onLine(s);
        PublishLine(s);
    } else if (s[0] == '@') {
        BadLine();   // the CAN module always sends a checksum: a line without one was cut
        return false;
    }

    EnterCriticalSection(&s_cs);
    strncpy(s_state.last, s, sizeof(s_state.last) - 1);
    s_state.lines++;
    s_state.dataTick = GetTickCount();
    LeaveCriticalSection(&s_cs);
    if (star)
        *star = 0;

    if (s[0] == '@') {
        ParseAt(s);
        Notify(false);
        return true;
    }

    char* f[24];
    int n = 0;
    f[n++] = s + 1;
    for (char* p = s + 1; *p && n < 24; p++)
        if (*p == ',') {
            *p = 0;
            f[n++] = p + 1;
        }
    if (strlen(f[0]) != 5)
        return true;
    const char* type = f[0] + 2;   // after the talker id (GP, GN, GL...)
    if (!checked && ((!strcmp(type, "RMC") && n < 12) || (!strcmp(type, "GGA") && n < 14)))
        return false;   // a cut sentence without a checksum

    EnterCriticalSection(&s_cs);
    bool fixChanged = false;
    if (!strcmp(type, "RMC") && n >= 9) {
        bool valid = f[2][0] == 'A' && f[3][0] && f[5][0];
        if (valid) {
            s_state.lat = NmeaDeg(f[3], f[4][0]);
            s_state.lon = NmeaDeg(f[5], f[6][0]);
            s_state.speedKmh = atof(f[7]) * 1.852;
            if (f[8][0])
                s_state.course = atof(f[8]);
            s_state.fixTick = s_state.dataTick;
            if (n >= 10 && strlen(f[9]) == 6 && strlen(f[1]) >= 6) {
                int d = atoi(f[9]);   // ddmmyy
                s_state.date = 20000000 + (d % 100) * 10000 + (d / 100 % 100) * 100 + d / 10000;
                s_state.time = atoi(f[1]);
            }
        }
        fixChanged = (s_state.valid != 0) != valid;
        s_state.valid = valid;
    } else if (!strcmp(type, "GGA") && n >= 10) {
        s_state.sats = atoi(f[7]);
        if (f[9][0])
            s_state.alt = atof(f[9]);
    }
    bool nowValid = s_state.valid != 0;
    LeaveCriticalSection(&s_cs);
    if (fixChanged)
        Logf("gps: fix %s", nowValid ? "acquired" : "lost");
    Notify(fixChanged);
    return true;
}

// Raw bytes in, returns the number of good sentences.
static int Feed(const char* buf, DWORD len)
{
    int good = 0;
    for (DWORD i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '$' || c == '@')
            s_lineLen = 0;
        if (c == '\r' || c == '\n') {
            s_line[s_lineLen] = 0;
            if (s_lineLen > 1 && ParseSentence(s_line))
                good++;
            s_lineLen = 0;
        } else if (s_lineLen < (int)sizeof(s_line) - 1) {
            s_line[s_lineLen++] = c;
        }
    }
    EnterCriticalSection(&s_cs);
    Publish();
    LeaveCriticalSection(&s_cs);
    return good;
}

static void SetStatus(int status, const wchar_t* port, DWORD baud)
{
    EnterCriticalSection(&s_cs);
    s_state.status = status;
    wcsncpy(s_state.port, port, 15);
    s_state.port[15] = 0;
    s_state.baud = baud;
    if (status != GPS_OPEN)
        s_state.valid = 0;
    Publish();
    LeaveCriticalSection(&s_cs);
    Notify(true);
}

// On the Vesta MMC the port also belongs to the shell: read-only, left as the shell set it up (no DTR/RTS,
// no timeouts, no purge); only the speed is changed, and only if it differs (baud = 0: never).
static HANDLE OpenPort(const wchar_t* port, DWORD baud, DWORD* err)
{
    HANDLE h = CreateFile(port, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        *err = GetLastError();
        return h;
    }
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (baud && GetCommState(h, &dcb) && dcb.BaudRate != baud) {
        Logf("gps: %S speed %lu -> %lu", port, dcb.BaudRate, baud);
        dcb.BaudRate = baud;
        SetCommState(h, &dcb);
    }
    *err = 0;
    return h;
}

// What is already in the driver queue, waiting up to waitMs (polling: comm timeouts are the shell's too).
// A queue that says it has bytes but reads none counts as empty: no spinning on it.
static int ReadAvailable(HANDLE h, char* buf, DWORD size, DWORD waitMs)
{
    DWORD start = GetTickCount();
    for (;;) {
        DWORD errors = 0;
        COMSTAT st;
        memset(&st, 0, sizeof(st));
        DWORD t = ThreadMs();
        BOOL ok = ClearCommError(h, &errors, &st);
        s_rs.polls++;
        s_rs.pollMs += ThreadMs() - t;
        if (!ok)
            return -1;
        if (st.cbInQue) {
            DWORD got = 0;
            t = ThreadMs();
            ok = ReadFile(h, buf, st.cbInQue < size ? st.cbInQue : size, &got, NULL);
            s_rs.reads++;
            s_rs.readMs += ThreadMs() - t;
            if (!ok)
                return -1;
            if (got)
                return (int)got;
            s_rs.empty++;
        }
        if (s_stop || s_reopen || s_scanReq || GetTickCount() - start >= waitMs)
            return 0;
        Sleep(100);
    }
}

static bool IsBusyError(DWORD err)
{
    return err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION || err == ERROR_BUSY;
}

// Listens to a port with its current settings; true if it talks valid NMEA. The speed is never changed
// while searching: other ports of the unit serve the car (MCU, CAN).
static bool ProbePort(const wchar_t* port, DWORD* baud)
{
    DWORD err;
    HANDLE h = OpenPort(port, 0, &err);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    *baud = GetCommState(h, &dcb) ? dcb.BaudRate : 0;
    SetStatus(GPS_SCANNING, port, *baud);
    int good = 0;
    s_lineLen = 0;
    DWORD start = GetTickCount();
    while (!s_stop && GetTickCount() - start < 3000 && good < 2) {
        char buf[256];
        int got = ReadAvailable(h, buf, sizeof(buf), 3000 - (GetTickCount() - start));
        if (got < 0)
            break;
        good += Feed(buf, got);
    }
    CloseHandle(h);
    return good >= 2;
}

static void Scan()
{
    Logf("gps: scanning ports");
    wchar_t first[16];
    EnterCriticalSection(&s_cs);
    wcscpy(first, s_port);
    LeaveCriticalSection(&s_cs);
    for (int i = 0; i <= 9 && !s_stop; i++) {
        wchar_t port[16];
        if (i == 0)
            wcscpy(port, first);
        else
            _snwprintf(port, 16, L"COM%d:", i);
        if (i > 0 && !wcscmp(port, first))
            continue;
        DWORD baud = 0;
        if (ProbePort(port, &baud)) {
            Logf("gps: found NMEA on %S %lu", port, baud);
            EnterCriticalSection(&s_cs);
            wcscpy(s_port, port);
            s_baud = baud;
            s_state.found = 1;
            s_state.baud = baud;
            LeaveCriticalSection(&s_cs);
            if (s_cfg.notify)
                PostMessage(s_cfg.notify, s_cfg.msg, 1, 0);
            return;
        }
    }
    Logf("gps: no NMEA found");
    EnterCriticalSection(&s_cs);
    s_state.found = -1;
    LeaveCriticalSection(&s_cs);
    if (s_cfg.notify)
        PostMessage(s_cfg.notify, s_cfg.msg, 1, 0);
}

#ifndef UNDER_CE
// PC test build only: a ".nmea" port plays a recording from exeDir, one RMC per 250 ms (4x real time).
static void PlayFile(const wchar_t* name)
{
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", s_cfgDir, name);
    path[MAX_PATH - 1] = 0;
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD len = h != INVALID_HANDLE_VALUE ? GetFileSize(h, NULL) : 0;
    char* data = len && len != 0xFFFFFFFF ? (char*)malloc(len) : NULL;
    DWORD got = 0;
    if (data && !ReadFile(h, data, len, &got, NULL))
        got = 0;
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
    if (!data || got != len) {
        free(data);
        SetStatus(GPS_NO_PORT, name, 0);
        WaitForSingleObject(s_wake, 3000);
        return;
    }
    SetStatus(GPS_OPEN, name, 0);
    s_lineLen = 0;
    while (!s_stop && !s_reopen && !s_scanReq) {
        DWORD start = 0;
        for (DWORD i = 0; i < len && !s_stop && !s_reopen && !s_scanReq; i++) {
            if (data[i] != '\n')
                continue;
            EnterCriticalSection(&s_cs);
            s_state.bytes += i + 1 - start;
            LeaveCriticalSection(&s_cs);
            if (s_cfg.onRaw)
                s_cfg.onRaw(data + start, i + 1 - start);
            Feed(data + start, i + 1 - start);
            if (i - start > 6 && !strncmp(data + start + 3, "RMC", 3))
                WaitForSingleObject(s_wake, 250);
            start = i + 1;
        }
    }
    free(data);
}
#endif

// This program reads the port (it holds the mutex): the old single-program loop, publishing as it goes.
static void ReadPortLoop()
{
    DWORD lastErr = 0xFFFFFFFF;
    while (!s_stop) {
        if (s_scanReq) {
            s_scanReq = false;
            Scan();
            continue;
        }
        wchar_t port[16];
        EnterCriticalSection(&s_cs);
        wcscpy(port, s_port);
        DWORD baud = s_baud;
        s_reopen = false;
        LeaveCriticalSection(&s_cs);
#ifndef UNDER_CE
        if (wcsstr(port, L".nmea")) {
            PlayFile(port);
            continue;
        }
#endif
        DWORD err = 0;
        HANDLE h = OpenPort(port, baud, &err);
        if (h == INVALID_HANDLE_VALUE && IsBusyError(err) && s_cfg.kill && s_cfgKill[0]) {
            if (s_cfg.kill(s_cfgKill))
                Sleep(1000);
            h = OpenPort(port, baud, &err);
        }
        if (h == INVALID_HANDLE_VALUE) {
            if (err != lastErr)
                Logf("gps: cannot open %S (%lu), retrying", port, err);
            lastErr = err;
            SetStatus(IsBusyError(err) ? GPS_BUSY : GPS_NO_PORT, port, baud);
            WaitForSingleObject(s_wake, 3000);
            continue;
        }
        Logf("gps: %S %lu opened", port, baud);
        lastErr = 0;
        SetStatus(GPS_OPEN, port, baud);
        s_lineLen = 0;
        DWORD lastData = GetTickCount();
        while (!s_stop && !s_reopen && !s_scanReq) {
            char buf[2048];
            int got = ReadAvailable(h, buf, sizeof(buf), 1000);
            if (got < 0) {
                Logf("gps: read error (%lu)", GetLastError());
                break;
            }
            if (got == 0) {
                if (GetTickCount() - lastData > 15000) {
                    Logf("gps: no data for 15 s, reopening");
                    break;
                }
                continue;
            }
            lastData = GetTickCount();
            EnterCriticalSection(&s_cs);
            s_state.bytes += got;
            LeaveCriticalSection(&s_cs);
            DWORD t = ThreadMs();
            if (s_cfg.onRaw)
                s_cfg.onRaw(buf, got);
            Feed(buf, got);
            s_rs.parseMs += ThreadMs() - t;
            s_rs.bytes += got;
            if (lastData - s_rs.start >= 60000) {
                if (s_rs.start)
                    Logf("gps: 60 s: polls %lu (%lu ms), reads %lu, empty %lu (%lu ms), parse %lu ms, %lu bytes",
                         s_rs.polls, s_rs.pollMs, s_rs.reads, s_rs.empty, s_rs.readMs, s_rs.parseMs, s_rs.bytes);
                memset(&s_rs, 0, sizeof(s_rs));
                s_rs.start = lastData;
            }
            Sleep(kReadPauseMs);
        }
        CloseHandle(h);
        SetStatus(GPS_NO_PORT, port, baud);
        if (!s_stop && !s_reopen && !s_scanReq)
            WaitForSingleObject(s_wake, 1000);
    }
}

// Another program reads the port: copy its state and pass its new lines on, until the mutex is free.
// true = the mutex is ours now (taken here, once), false = stopping.
static bool FollowLoop()
{
    LONG seenLines = s_shm ? s_shm->lineSeq : 0;
    bool logged = false;
    while (!s_stop) {
        DWORD w = WaitForSingleObject(s_mutex, 0);
        if (w == WAIT_OBJECT_0 || w == WAIT_ABANDONED)
            return true;   // the reader is gone: this program takes the port (the mutex is held now)
        if (s_shm && s_shm->magic == VGPS_MAGIC) {
            GpsState st;
            LONG a, b;
            int tries = 0;
            do {
                a = s_shm->seq;
                st = s_shm->state;
                b = s_shm->seq;
            } while ((a != b || (a & 1)) && ++tries < 10);
            if (a == b && !(a & 1)) {
                if (!logged) {
                    Logf("gps: port read by %S, sharing its data", st.ownerApp);
                    logged = true;
                }
                EnterCriticalSection(&s_cs);
                int found = s_state.found;
                s_state = st;
                s_state.owner = 0;
                s_state.found = found;
                LeaveCriticalSection(&s_cs);
            }
            LONG n = s_shm->lineSeq;
            if (n - seenLines > kShmLines)
                seenLines = n - kShmLines;
            for (; seenLines < n; seenLines++) {
                char line[kShmLineLen];
                strncpy(line, s_shm->lines[seenLines % kShmLines], kShmLineLen - 1);
                line[kShmLineLen - 1] = 0;
                if (s_cfg.onLine)
                    s_cfg.onLine(line);
            }
            Notify(false);
        }
        if (s_scanReq) {   // the port is not ours to probe
            s_scanReq = false;
            EnterCriticalSection(&s_cs);
            s_state.found = -1;
            LeaveCriticalSection(&s_cs);
            Logf("gps: no scan, the port is read by another program");
            if (s_cfg.notify)
                PostMessage(s_cfg.notify, s_cfg.msg, 1, 0);
        }
        WaitForSingleObject(s_wake, 200);
    }
    return false;
}

static DWORD WINAPI GpsThread(LPVOID)
{
    Logf("gps: thread id %08X", (unsigned)GetCurrentThreadId());
    while (!s_stop) {
        // no mutex (could not be created): read the port alone, as before
        DWORD w = s_mutex ? WaitForSingleObject(s_mutex, 0) : WAIT_OBJECT_0;
        if (w != WAIT_OBJECT_0 && w != WAIT_ABANDONED && !FollowLoop())
            break;   // stopping, the mutex was never taken
        if (s_stop) {
            if (s_mutex)
                ReleaseMutex(s_mutex);
            break;
        }
        Logf("gps: this program reads the port");
        EnterCriticalSection(&s_cs);
        s_state.owner = 1;
        wcsncpy(s_state.ownerApp, s_cfgApp, 23);
        s_state.ownerApp[23] = 0;
        if (s_shm) {
            s_shm->magic = VGPS_MAGIC;
            s_shm->version = 1;
        }
        Publish();
        LeaveCriticalSection(&s_cs);
        ReadPortLoop();
        EnterCriticalSection(&s_cs);
        s_state.status = GPS_NO_PORT;
        s_state.valid = 0;
        Publish();   // the others see the port closed until one of them takes it
        LeaveCriticalSection(&s_cs);
        if (s_mutex)
            ReleaseMutex(s_mutex);
    }
    return 0;
}

void GpsStart(const GpsConfig* cfg)
{
    InitializeCriticalSection(&s_cs);
    memset(&s_state, 0, sizeof(s_state));
    s_cfg = *cfg;
    // keep our own copies of the strings
    wcsncpy(s_cfgPort, cfg->port ? cfg->port : L"COM5:", 15);
    wcsncpy(s_cfgApp, cfg->app ? cfg->app : L"?", 23);
    wcsncpy(s_cfgKill, cfg->killExe ? cfg->killExe : L"", 31);
    wcsncpy(s_cfgDir, cfg->exeDir ? cfg->exeDir : L"", MAX_PATH - 1);
    wcscpy(s_port, s_cfgPort);
    s_baud = cfg->baud;
    s_mutex = CreateMutex(NULL, FALSE, VGPS_MUTEX);
    s_shmMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(GpsShm), VGPS_SHM_NAME);
    if (s_shmMap)
        s_shm = (GpsShm*)MapViewOfFile(s_shmMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(GpsShm));
    if (!s_mutex)
        Logf("gps: no mutex (%lu), reading the port alone", GetLastError());
    s_wake = CreateEvent(NULL, FALSE, FALSE, NULL);
    s_thread = CreateThread(NULL, 0, GpsThread, NULL, 0, NULL);
}

void GpsStop()
{
    if (!s_thread)
        return;
    s_stop = true;
    SetEvent(s_wake);
    WaitForSingleObject(s_thread, 3000);
    CloseHandle(s_thread);
    s_thread = NULL;
    if (s_shm)
        UnmapViewOfFile(s_shm);
    if (s_shmMap)
        CloseHandle(s_shmMap);
    if (s_mutex)
        CloseHandle(s_mutex);
    s_shm = NULL;
}

void GpsConfigure(const wchar_t* port, DWORD baud)
{
    if (!s_thread)
        return;
    EnterCriticalSection(&s_cs);
    wcsncpy(s_port, port, 15);
    s_port[15] = 0;
    s_baud = baud;
    s_state.lines = s_state.badLines = 0;
    s_state.last[0] = 0;
    s_state.found = 0;
    s_reopen = true;
    LeaveCriticalSection(&s_cs);
    SetEvent(s_wake);
}

void GpsScan()
{
    if (!s_thread)
        return;
    EnterCriticalSection(&s_cs);
    s_state.lines = s_state.badLines = 0;
    s_state.last[0] = 0;
    s_state.found = 0;
    LeaveCriticalSection(&s_cs);
    s_scanReq = true;
    SetEvent(s_wake);
}

void GpsGet(GpsState* s)
{
    if (!s_thread) {
        memset(s, 0, sizeof(*s));
        return;
    }
    EnterCriticalSection(&s_cs);
    *s = s_state;
    LeaveCriticalSection(&s_cs);
    s_notifyPosted = 0;
}
