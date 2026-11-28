// vcan3.cpp - VestaCore: the CAN panel's COM3 frames (vcan3.h).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vcan3.h"

void VCan3Init(VCan3* c)
{
    memset(c, 0, sizeof(*c));
}

bool VCan3Valid(const unsigned char* f)
{
    if (f[0] | f[1] | f[2] | f[3])
        return false;
    unsigned s = 0;
    for (int i = 4; i < 14; i++)
        s += f[i];
    return ((0x10000 - s) & 0xFFFF) == (unsigned)(f[14] << 8 | f[15]);
}

static long Be32(const unsigned char* p)
{
    return (long)((unsigned long)p[0] << 24 | (unsigned long)p[1] << 16 | (unsigned long)p[2] << 8 | p[3]);
}

static void Gps(VCan3* c, int code, const unsigned char* d)
{
    if (code == 0xa2) {
        c->hasPos = true;
        c->posN++;
        c->lat = Be32(d);
        c->lon = Be32(d + 4);
    } else if (code == 0xa6) {
        int date = d[6] << 8 | d[7];
        c->hasMove = true;
        c->kn100 = d[0] << 8 | d[1];
        c->course100 = d[2] << 8 | d[3];
        c->day = date >> 11;
        c->month = date >> 7 & 15;
        c->year = date & 127;
    } else if (code == 0xa8) {
        c->hasUtc = true;
        c->utcMs = (unsigned long)Be32(d);
    }
}

// One function a frame type, each knowing its codes only (can_panel 1.3.6.1's dispatcher; vcan3.h)
static void Type012(VCan3* c, int type, int code, const unsigned char* d)   // the wheel, the revs, the speed
{
    if (type == 0 && code == 0xc6) {
        c->hasWheel = true;
        c->wheel = d[0] << 8 | d[1];
        c->wheelN++;
    } else if (type == 1 && code == 0x86) {
        c->hasRpm = true;
        c->rpm = (d[0] << 8 | d[1]) >> 3;
    } else if (type == 2 && code == 0x17) {   // (other cars)
        c->hasSpeed = true;
        c->speed10 = (d[3] << 4 | d[4] >> 4) * 10;
    }
}

static void Type3(VCan3* c, int, int code, const unsigned char* d)   // the ignition and what is open, the air outside
{
    if (code == 0x50) {
        c->hasBody = true;
        c->ign = d[0] & 7;
        c->open = (d[6] & 0x20 ? VCO_FL : 0) | (d[6] & 0x08 ? VCO_FR : 0) | (d[7] & 0x02 ? VCO_REAR : 0) |
                  (d[4] & 0x08 ? VCO_TRUNK : 0) | (d[7] & 0x08 ? VCO_HOOD : 0);
    } else if (code == 0xb7) {
        c->hasAir = true;
        c->airC = d[0] - 40;
    }
}

static void Type4(VCan3* c, int, int code, const unsigned char* d)   // GPS, the handbrake
{
    if (code != 0xf8) {
        Gps(c, code, d);
        return;
    }
    c->hasHand = true;
    c->hand = (d[0] & 8) != 0;
}

static void Type5(VCan3* c, int, int code, const unsigned char* d)   // the lights, the car's speed, the coolant
{
    if (code == 0xde) {
        c->hasLight = true;
        c->light = d[0];
    } else if (code == 0xd7) {
        c->hasCar = true;
        c->carKmh100 = d[0] << 8 | d[1];
    } else if (code == 0xda && d[0]) {
        c->hasCool = true;
        c->coolC = d[0] - 40;
    }
}

static void Type6(VCan3* c, int, int code, const unsigned char* d)   // the trip computer, the fuel, the climate
{
    if (code == 0x46) {
        c->hasTrip = true;
        c->use10 = (d[0] & 0x3F) << 8 | d[1];
        c->trip10 = (d[2] & 0x7F) << 9 | d[3] << 1 | d[4] >> 7;
        c->used10 = (d[4] & 0x3F) << 8 | d[5];
        c->avg10 = d[6] << 4 | d[7] >> 4;
    } else if (code == 0xfb) {
        c->hasFuel = true;
        c->fuel2 = d[3];
        c->rangeKm = (d[1] & 0x0F) << 8 | d[2];
    } else if (code == 0x99) {
        c->hasClima = true;
        c->climaT = d[1];
        c->fan = d[3] >> 4;
    }
}

void VCan3Frame(VCan3* c, const unsigned char* f)
{
    static void (*const types[7])(VCan3*, int, int, const unsigned char*) = { Type012, Type012, Type012, Type3,
                                                                               Type4, Type5, Type6 };
    if (f[4] < 7)   // (type 7: the units' diagnostics - not read)
        types[f[4]](c, f[4], f[5], f + 6);
}

int VCan3Feed(VCan3* c, const unsigned char* p, int n)
{
    int found = 0;
    for (int i = 0; i < n; i++) {
        c->win[c->have++] = p[i];
        if (c->have < 16)
            continue;
        if (VCan3Valid(c->win)) {
            VCan3Frame(c, c->win);
            c->frames++;
            found++;
            c->have = 0;
        } else {   // one byte on: the next try starts a byte later
            memmove(c->win, c->win + 1, 15);
            c->have = 15;
            c->skipped++;
        }
    }
    return found;
}

bool VCan3Nmea(char* out, int cap, const char* body)
{
    int n = (int)strlen(body);
    if (n + 6 > cap)
        return false;
    unsigned x = 0;
    for (int i = 1; i < n; i++)
        x ^= (unsigned char)body[i];
    sprintf(out, "%s*%02X\r\n", body, x);
    return true;
}

bool VCan3Ang(const VCan3* c, char* out, int cap)
{
    char b[32];
    if (!c->hasWheel)
        return false;
    sprintf(b, "@ANG,%.1f,", c->wheel ? (c->wheel - 32767) / 15.0 : 0.0);
    return VCan3Nmea(out, cap, b);
}

bool VCan3Car(const VCan3* c, char* out, int cap)
{
    char b[32];
    if (!c->hasCar)
        return false;
    sprintf(b, "@,%03d,2.34", c->carKmh100 / 100);
    return VCan3Nmea(out, cap, b);
}

// "hhmmss.00,ddmm.mmmm,N,dddmm.mmmm,E" (1/600000 degree: a minute is 10000)
static void TimePos(const VCan3* c, char* b)
{
    unsigned long s = c->utcMs / 1000;
    unsigned long la = (unsigned long)labs(c->lat), lo = (unsigned long)labs(c->lon);
    sprintf(b, "%02lu%02lu%02lu.00,%02lu%02lu.%04lu,%c,%03lu%02lu.%04lu,%c", s / 3600, s / 60 % 60, s % 60,
            la / 600000, la % 600000 / 10000, la % 10000, c->lat < 0 ? 'S' : 'N',
            lo / 600000, lo % 600000 / 10000, lo % 10000, c->lon < 0 ? 'W' : 'E');
}

bool VCan3Rmc(const VCan3* c, char* out, int cap)
{
    char tp[64], b[128];
    if (!c->hasPos || !c->hasUtc || !c->hasMove)
        return false;
    TimePos(c, tp);
    char* comma = strchr(tp, ',');   // the time, then the position
    *comma = 0;
    sprintf(b, "$GPRMC,%s,A,%s,%d.%02d,%d.%02d,%02d%02d%02d,,,A", tp, comma + 1, c->kn100 / 100, c->kn100 % 100,
            c->course100 / 100, c->course100 % 100, c->day, c->month, c->year);
    return VCan3Nmea(out, cap, b);
}

bool VCan3Gga(const VCan3* c, char* out, int cap)
{
    char tp[64], b[128];
    if (!c->hasPos || !c->hasUtc)
        return false;
    TimePos(c, tp);
    sprintf(b, "$GPGGA,%s,1,12,1.0,0.0,M,0.0,M,,", tp);
    return VCan3Nmea(out, cap, b);
}
