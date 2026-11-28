// test_vcan3.cpp - vcan3.cpp on the PC: frames made here (every field, a broken sum, bytes split between reads,
// junk before them), then - on the author's PC, MAIN beside the repos - the tester's real recording of 2026-11-27
// (can_panel 1.3.6.1, the car parked: 19292 frames, 2 bytes before the first). Exit code: the failed checks.
#include <stdio.h>
#include <string.h>
#include "vcan3.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

// a valid frame: type, code, data[8]
static void Make(unsigned char* f, int type, int code, const unsigned char* d)
{
    memset(f, 0, 16);
    f[4] = (unsigned char)type;
    f[5] = (unsigned char)code;
    memcpy(f + 6, d, 8);
    unsigned s = 0;
    for (int i = 0; i < 14; i++)
        s += f[i];
    s = (0x10000 - s) & 0xFFFF;
    f[14] = (unsigned char)(s >> 8);
    f[15] = (unsigned char)s;
}

static void TestValid()
{
    unsigned char f[16], d[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    Make(f, 3, 0x50, d);
    Check(VCan3Valid(f), "valid: a made frame");
    f[15] ^= 1;
    Check(!VCan3Valid(f), "valid: the sum's low byte wrong");
    f[15] ^= 1;
    f[14] ^= 1;
    Check(!VCan3Valid(f), "valid: the sum's high byte wrong");
    f[14] ^= 1;
    for (int i = 0; i < 4; i++) {
        unsigned char g[16];
        memcpy(g, f, 16);
        g[i] = 1;   // (the sum is over the bytes after the head: only the head is wrong)
        Check(!VCan3Valid(g), "valid: the head's 4 zero bytes, each");
    }
    unsigned char z[16] = { 0 };
    Check(VCan3Valid(z), "valid: all zero (sum 0x10000 & 0xFFFF = 0)");
}

static void TestFields()
{
    VCan3 c;
    VCan3Init(&c);
    Check(!c.hasSpeed && !c.hasTrip && !c.hasBody && !c.hasLight && !c.hasWheel && !c.hasPos && !c.hasMove &&
          !c.hasUtc && !c.hasHand && !c.hasRpm && !c.hasAir && !c.hasCool && !c.hasFuel && !c.hasClima &&
          c.frames == 0, "init: nothing seen");
    unsigned char f[16];
    unsigned char sp[8] = { 0, 0, 0, 0x12, 0x34, 0, 0, 0 };   // (0x12 << 4 | 0x34 >> 4) = 0x123 = 291 km/h
    Make(f, 2, 0x17, sp);
    VCan3Frame(&c, f);
    Check(c.hasSpeed && c.speed10 == 2910, "0x17: speed (data[3] << 4 | data[4] >> 4) x 10");
    // 0x46 the trip computer: the average use ((d0 & 0x3f) << 8 | d1), the trip ((d2 & 0x7f) << 9 | d3 << 1 |
    // d4 >> 7), the fuel used ((d4 & 0x3f) << 8 | d5), the average speed (d6 << 4 | d7 >> 4), each x 0.1
    unsigned char tr[8] = { 0xC0 | 0x01, 0x02, 0x80 | 0x03, 0x04, 0x80 | 0x40 | 0x05, 0x06, 0x07, 0x8F };
    Make(f, 6, 0x46, tr);
    VCan3Frame(&c, f);
    Check(c.hasTrip && c.use10 == 0x102 && c.trip10 == (3 << 9 | 4 << 1 | 1) && c.used10 == 0x506 && c.avg10 == 0x78,
          "0x46: the trip computer's four, the bits beside each masked");
    unsigned char bo[8] = { 0xC5, 0, 0, 0, 0x08, 0, 0x20 | 0x08, 0x02 | 0x08 };
    Make(f, 3, 0x50, bo);
    VCan3Frame(&c, f);
    Check(c.hasBody && c.ign == 5 && c.open == (VCO_FL | VCO_FR | VCO_REAR | VCO_TRUNK | VCO_HOOD),
          "0x50: the ignition data[0] & 7, all open");
    unsigned char bo2[8] = { 0x04, 0xFF, 0xFF, 0xFF, 0xF7, 0xFF, 0xD7, 0xF5 };   // every other bit set
    Make(f, 3, 0x50, bo2);
    VCan3Frame(&c, f);
    Check(c.ign == 4 && c.open == 0, "0x50: the other bits are no door");
    unsigned char bo3[8] = { 0, 0, 0, 0, 0, 0, 0x20, 0 };
    Make(f, 3, 0x50, bo3);
    VCan3Frame(&c, f);
    Check(c.open == VCO_FL, "0x50: the front left alone");
    bo3[6] = 0x08;
    Make(f, 3, 0x50, bo3);
    VCan3Frame(&c, f);
    Check(c.open == VCO_FR, "0x50: the front right alone");
    bo3[6] = 0;
    bo3[7] = 0x02;
    Make(f, 3, 0x50, bo3);
    VCan3Frame(&c, f);
    Check(c.open == VCO_REAR, "0x50: the rear alone");
    bo3[7] = 0x08;
    Make(f, 3, 0x50, bo3);
    VCan3Frame(&c, f);
    Check(c.open == VCO_HOOD, "0x50: the hood alone");
    bo3[7] = 0;
    bo3[4] = 0x08;
    Make(f, 3, 0x50, bo3);
    VCan3Frame(&c, f);
    Check(c.open == VCO_TRUNK, "0x50: the trunk alone");
    unsigned char dr[8] = { 0x62, 0xFF, 0, 0, 0, 0, 0, 0 };   // (vhcan6: the hazard lights with the low beam)
    Make(f, 5, 0xde, dr);
    VCan3Frame(&c, f);
    Check(c.hasLight && c.light == (VCL_LOW | VCL_LEFT | VCL_RIGHT), "0xde: the lights = data[0]");
    unsigned char hb[8] = { 0x58, 0xC0, 0, 0, 0, 0, 0, 0 };
    Make(f, 4, 0xf8, hb);
    VCan3Frame(&c, f);
    Check(c.hasHand && c.hand, "0xf8: the handbrake up, data[0] & 8 (vhcan6 0x58)");
    hb[0] = 0x54;
    Make(f, 4, 0xf8, hb);
    VCan3Frame(&c, f);
    Check(c.hasHand && !c.hand, "0xf8: down (0x54)");
    hb[0] = 0xF7;
    Make(f, 4, 0xf8, hb);
    VCan3Frame(&c, f);
    Check(!c.hand, "0xf8: every bit but 0x08 - down");
    unsigned char rp[8] = { 0x5D, 0xC7, 0, 0, 0, 0, 0, 0 };   // (0x5dc7 >> 3 = 3000)
    Make(f, 1, 0x86, rp);
    VCan3Frame(&c, f);
    Check(c.hasRpm && c.rpm == 3000, "0x86: the revs (d0 << 8 | d1) >> 3");
    unsigned char ai[8] = { 0x24, 0, 0, 0, 0, 0, 0, 0 };
    Make(f, 3, 0xb7, ai);
    VCan3Frame(&c, f);
    Check(c.hasAir && c.airC == -4, "0xb7: outside data[0] - 40");
    unsigned char co[8] = { 0, 0x6F, 0, 0, 0, 0, 0, 0 };
    Make(f, 5, 0xda, co);
    VCan3Frame(&c, f);
    Check(!c.hasCool, "0xda: data[0] 0 - not taken (the panel's)");
    co[0] = 0x4E;
    Make(f, 5, 0xda, co);
    VCan3Frame(&c, f);
    Check(c.hasCool && c.coolC == 38, "0xda: the coolant data[0] - 40");
    co[0] = 0;
    Make(f, 5, 0xda, co);
    VCan3Frame(&c, f);
    Check(c.coolC == 38, "0xda: a 0 later keeps the last");
    unsigned char fu[8] = { 0x1F, 0xF2, 0x8F, 0x5E, 0xFF, 0, 0, 0xFF };
    Make(f, 6, 0xfb, fu);
    VCan3Frame(&c, f);
    Check(c.hasFuel && c.fuel2 == 94 && c.rangeKm == 0x28F, "0xfb: the fuel d3 (0.5 l), the range (d1 & 15) << 8 | d2");
    unsigned char cl[8] = { 0, 0x2C, 0, 0x5F, 0, 0, 0, 0 };
    Make(f, 6, 0x99, cl);
    VCan3Frame(&c, f);
    Check(c.hasClima && c.climaT == 0x2C && c.fan == 5, "0x99: the climate's temperature d1, the fan d3 >> 4");
    unsigned char ss[8] = { 0x80, 0x05, 0, 0, 0, 0, 0, 0 };
    Make(f, 0, 0xc6, ss);
    VCan3Frame(&c, f);
    Check(c.hasWheel && c.wheel == 0x8005 && c.wheelN == 1, "0xc6: the wheel raw = data[0] << 8 | data[1], counted");
    unsigned char cs[8] = { 0x04, 0xD2, 0, 0, 0, 0, 0, 0 };
    Make(f, 5, 0xd7, cs);
    VCan3Frame(&c, f);
    Check(c.hasCar && c.carKmh100 == 1234, "0xd7: the car's speed, km/h x 100 = data[0] << 8 | data[1]");
    unsigned char po[8] = { 0x02, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFE };
    Make(f, 4, 0xa2, po);
    VCan3Frame(&c, f);
    Check(c.hasPos && c.lat == 0x02000001L && c.lon == -2, "0xa2: lat, lon int32 big-endian, signed");
    unsigned char mv[8] = { 0x01, 0x02, 0x8C, 0x9F, 0, 5, (27 << 3) | (11 >> 1), (11 & 1) << 7 | 26 };
    Make(f, 4, 0xa6, mv);
    VCan3Frame(&c, f);
    Check(c.hasMove && c.kn100 == 0x102 && c.course100 == 35999, "0xa6: knots x 100, course x 100 unsigned");
    Check(c.day == 27 && c.month == 11 && c.year == 26, "0xa6: the date's bits 15..11, 10..7, 6..0");
    unsigned char ut[8] = { 0x05, 0x26, 0x5B, 0xFF, 0, 0, 0, 0 };
    Make(f, 4, 0xa8, ut);
    VCan3Frame(&c, f);
    Check(c.hasUtc && c.utcMs == 0x05265BFFUL, "0xa8: UTC ms big-endian");
    unsigned char no[8] = { 9, 9, 9, 9, 9, 9, 9, 9 };
    VCan3 d = c;
    Make(f, 2, 0x18, no);   // a code not decoded, and a known code under another type
    VCan3Frame(&c, f);
    static const int other[][2] = { { 3, 0x17 }, { 6, 0x53 }, { 5, 0x46 }, { 0, 0xd7 }, { 0, 0xde }, { 0, 0x86 },
                                    { 1, 0xc6 }, { 4, 0x50 }, { 0, 0xf8 }, { 0, 0xb7 }, { 0, 0xda }, { 0, 0xfb },
                                    { 0, 0x99 }, { 4, 0x86 } };
    for (int i = 0; i < 14; i++) {
        Make(f, other[i][0], other[i][1], no);
        VCan3Frame(&c, f);
    }
    Check(!memcmp(&c, &d, sizeof(c)), "unknown codes, known codes under another type: nothing changed");
}

static void TestFeed()
{
    VCan3 c;
    VCan3Init(&c);
    unsigned char s[3 + 16 * 3], d[8] = { 0xC2, 0, 0, 0, 0, 0, 0, 0 };
    s[0] = 0xFF;
    s[1] = 0x1B;
    s[2] = 0x00;   // junk, its last byte a zero as a head's
    Make(s + 3, 3, 0x50, d);
    Make(s + 19, 5, 0xde, d);
    Make(s + 35, 0, 0xc6, d);
    int n1 = VCan3Feed(&c, s, 10);           // the junk and part of the first frame
    int n2 = VCan3Feed(&c, s + 10, 20);      // the first frame's end and part of the second
    int n3 = VCan3Feed(&c, s + 30, sizeof(s) - 30);
    Check(n1 == 0 && n2 == 1 && n3 == 2, "feed: frames across the reads");
    Check(c.frames == 3 && c.skipped == 3, "feed: 3 frames, the 3 junk bytes skipped");
    Check(c.ign == 2 && c.light == 0xC2 && c.wheel == 0xC200, "feed: each frame decoded");
    unsigned char bad[16];
    memcpy(bad, s + 3, 16);
    bad[15] ^= 0x10;
    VCan3Feed(&c, bad, 16);
    Check(c.frames == 3, "feed: a broken sum is no frame");
    VCan3Feed(&c, s + 3, 16);   // the 16 broken bytes go one by one before this frame is found
    Check(c.frames == 4 && c.skipped == 3 + 16, "feed: found again after a broken one");
    Check(VCan3Feed(&c, s, 0) == 0, "feed: nothing read");
}

// The tester's recordings: chunks [ms u32 LE][len u16 LE][bytes] (VestaHome canrec.h); the bytes run together
// into b, 0 - no MAIN
static int Load(const char* name, unsigned char* b, int cap)
{
    char p[160];
    sprintf(p, "..\\MAIN\\HU_logs\\canpanel logs\\%s", name);
    FILE* f = fopen(p, "rb");
    if (!f) {
        printf("(no MAIN: the real recording skipped)\n");
        return 0;
    }
    static unsigned char r[400000];
    int len = (int)fread(r, 1, sizeof(r), f), n = 0;
    fclose(f);
    for (int i = 0; i + 6 <= len && n < cap;) {
        int k = r[i + 4] | r[i + 5] << 8;
        memcpy(b + n, r + i + 6, k);
        n += k;
        i += 6 + k;
    }
    return n;
}

static void TestReal()
{
    static unsigned char b[400000];
    int len = Load("4123\\VestaLogs\\com3_261127_214806.bin", b, sizeof(b));
    if (!len)
        return;
    VCan3 c;
    VCan3Init(&c);
    int found = VCan3Feed(&c, b, len);
    Check(c.frames == 19292 && found == 19292 && c.skipped == 2, "real: 19292 frames, 2 bytes before the first");
    Check(c.hasPos && c.lat != 0 && c.lon != 0, "real: a position");
    Check(c.hasMove && c.kn100 == 0 && c.day == 27 && c.month == 11 && c.year == 26, "real: parked, 27.11.26");
    Check(c.hasBody && c.ign == 5 && c.hasTrip && c.trip10 == 951 && c.hasLight && c.light == 0,
          "real: the ignition on, the trip 95.1 km, no lights");
    Check(!c.hasSpeed, "real: no 0x17 while parked");
    char s[64];
    Check(c.hasWheel && c.wheel == 0x8005 && VCan3Ang(&c, s, 64) && !strncmp(s, "@ANG,0.4,", 9),
          "real: the wheel straight - @ANG 0.4");
}

// vhcan6 (2026-11-27): the doors one by one as the user was asked, the hazard lights, the high beam, the handbrake
// up and down, then the ignition on
static void TestReal6()
{
    static unsigned char b[400000];
    int len = Load("vhcan6\\com3_261127_231454.bin", b, sizeof(b));
    if (!len)
        return;
    VCan3 c;
    VCan3Init(&c);
    int order[8], no = 0, lights = 0, hands = 0, open = 0;
    bool hand = false;
    for (int i = 0; i < len; i++) {
        VCan3Feed(&c, b + i, 1);
        int added = c.open & ~open;   // (each door's first opening; the front left opened twice)
        if (added && no < 8)
            order[no++] = added;
        open |= c.open;
        lights |= c.light;
        hands += c.hasHand && c.hand != hand;
        hand = c.hand;
    }
    Check(no == 5 && order[0] == VCO_FL && order[1] == VCO_FR && order[2] == VCO_REAR && order[3] == VCO_HOOD &&
          order[4] == VCO_TRUNK, "vhcan6: the doors in the asked order");
    Check(lights == (VCL_HIGH | VCL_LOW | VCL_LEFT | VCL_RIGHT), "vhcan6: the lights seen");
    Check(hands == 7, "vhcan6: the handbrake up, then down and up three times");
    Check(c.ign == 5 && c.airC == -3 && c.coolC == 38 && c.fuel2 == 94 && c.rangeKm == 656 && c.rpm == 0,
          "vhcan6: the end - the ignition on, -3, 38, 47 l, 656 km, the engine off");
}

// The lines can_panel writes to COM4 / COM5 / COM7, made here (the user 2026-11-27: GPS and the wheel without it)
static void TestNmea()
{
    char s[128];
    Check(VCan3Nmea(s, 128, "@ANG,31.0,") && !strcmp(s, "@ANG,31.0,*54\r\n"), "nmea: the panel's own line, its sum");
    Check(VCan3Nmea(s, 128, "@,000,2.34") && !strcmp(s, "@,000,2.34*2B\r\n"), "nmea: the sum after the first char");
    Check(!VCan3Nmea(s, 10, "@ANG,31.0,"), "nmea: no room - nothing");
    VCan3 c;
    VCan3Init(&c);
    Check(!VCan3Ang(&c, s, 128) && !VCan3Car(&c, s, 128) && !VCan3Rmc(&c, s, 128) && !VCan3Gga(&c, s, 128),
          "nothing seen: no lines");
    c.hasWheel = true;
    c.wheel = 33231;   // (the tester's car parked: the panel said @ANG,31.0 just after)
    Check(VCan3Ang(&c, s, 128) && !strcmp(s, "@ANG,30.9,*5C\r\n"), "@ANG: (raw - 32767) / 15");
    c.wheel = 28267;
    Check(VCan3Ang(&c, s, 128) && !strcmp(s, "@ANG,-300.0,*48\r\n"), "@ANG: to the left, negative");
    c.wheel = 32766;
    Check(VCan3Ang(&c, s, 128) && !strcmp(s, "@ANG,-0.1,*4A\r\n"), "@ANG: a tenth");
    c.wheel = 0;
    Check(VCan3Ang(&c, s, 128) && !strcmp(s, "@ANG,0.0,*66\r\n"), "@ANG: raw 0 - 0 (as the panel)");
    c.hasCar = true;
    c.carKmh100 = 1299;
    Check(VCan3Car(&c, s, 128) && !strcmp(s, "@,012,2.34*28\r\n"), "@,: the car's km/h, whole, and the module's 2.34");
    c.hasPos = c.hasMove = c.hasUtc = true;
    c.lat = 55 * 600000L + 451234;
    c.lon = 37 * 600000L + 375000;
    c.utcMs = 45296000;   // 12:34:56
    c.kn100 = 1050;
    c.course100 = 27050;
    c.day = 27;
    c.month = 11;
    c.year = 26;
    Check(VCan3Rmc(&c, s, 128) && !strcmp(s, "$GPRMC,123456.00,A,5545.1234,N,03737.5000,E,10.50,270.50,271126,,,A*6C\r\n"),
          "RMC: position ddmm.mmmm, knots, the course in degrees (not the panel's x/127), the date");
    Check(VCan3Gga(&c, s, 128) && !strcmp(s, "$GPGGA,123456.00,5545.1234,N,03737.5000,E,1,12,1.0,0.0,M,0.0,M,,*59\r\n"),
          "GGA: a fix, 12 satellites (as the panel)");
    c.lat = -300000;
    c.lon = -600001;
    c.utcMs = 1999;
    c.kn100 = c.course100 = 0;
    c.day = c.month = 1;
    c.year = 25;
    Check(VCan3Rmc(&c, s, 128) && !strcmp(s, "$GPRMC,000001.00,A,0030.0000,S,00100.0001,W,0.00,0.00,010125,,,A*54\r\n"),
          "RMC: south, west, the seconds whole, the zeros");
    c.hasUtc = false;
    Check(!VCan3Rmc(&c, s, 128) && !VCan3Gga(&c, s, 128), "no time yet: no position lines");
    c.hasUtc = true;
    c.hasMove = false;
    Check(!VCan3Rmc(&c, s, 128) && VCan3Gga(&c, s, 128), "no speed / date yet: GGA only");
    c.hasMove = true;
    Check(!VCan3Rmc(&c, s, 40), "RMC: no room - nothing");
    Check(VCan3Nmea(s, 16, "@ANG,31.0,") && !VCan3Nmea(s, 15, "@ANG,31.0,"), "nmea: the room exact (*XX, CR LF, 0)");
    c.carKmh100 = 10000;
    Check(VCan3Car(&c, s, 128) && !strcmp(s, "@,100,2.34*2A\r\n"), "@,: 100 km/h");
    c.lat = -(55 * 600000L + 599999);
    c.lon = 37 * 600000L + 599999;
    c.utcMs = 3600000;   // 01:00:00
    c.kn100 = 10000;
    c.course100 = 35999;
    c.day = 31;
    c.month = 12;
    c.year = 99;
    Check(VCan3Rmc(&c, s, 128) && !strcmp(s, "$GPRMC,010000.00,A,5559.9999,S,03759.9999,E,100.00,359.99,311299,,,A*49\r\n"),
          "RMC: the minutes' edges, an hour, 100 knots");
    c.lat = c.lon = 0;
    Check(VCan3Gga(&c, s, 128) && !strcmp(s, "$GPGGA,010000.00,0000.0000,N,00000.0000,E,1,12,1.0,0.0,M,0.0,M,,*5F\r\n"),
          "GGA: 0, 0 - north, east");
    c.lat = 56 * 600000L;
    c.lon = 38 * 600000L + 10000;
    Check(VCan3Gga(&c, s, 128) && !strcmp(s, "$GPGGA,010000.00,5600.0000,N,03801.0000,E,1,12,1.0,0.0,M,0.0,M,,*56\r\n"),
          "GGA: whole degrees, a whole minute");
    c.lon = 38 * 600000L;
    Check(VCan3Gga(&c, s, 128) && !strcmp(s, "$GPGGA,010000.00,5600.0000,N,03800.0000,E,1,12,1.0,0.0,M,0.0,M,,*57\r\n"),
          "GGA: whole degrees");
}

int main()
{
    TestValid();
    TestFields();
    TestFeed();
    TestReal();
    TestReal6();
    TestNmea();
    printf("test_vcan3: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
