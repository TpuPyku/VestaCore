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
    Check(!c.hasSpeed && !c.hasSteer && !c.hasGear && !c.hasDoors && !c.hasStatus && !c.hasPos && !c.hasMove &&
          !c.hasUtc && c.frames == 0, "init: nothing seen");
    unsigned char f[16];
    unsigned char sp[8] = { 0, 0, 0, 0x12, 0x34, 0, 0, 0 };   // (0x12 << 4 | 0x34 >> 4) = 0x123 = 291 km/h
    Make(f, 2, 0x17, sp);
    VCan3Frame(&c, f);
    Check(c.hasSpeed && c.speed10 == 2910, "0x17: speed (data[3] << 4 | data[4] >> 4) x 10");
    unsigned char st[8] = { 0, 0, 0x40, 0x00, 0x80, 0, 0, 0 };   // bit 15 set, bit 0 set: -32767
    Make(f, 6, 0x46, st);
    VCan3Frame(&c, f);
    Check(c.hasSteer && c.steer == -32767, "0x46: the angle, signed");
    unsigned char st2[8] = { 0, 0, 0x01, 0xDB, 0x80, 0, 0, 0 };   // the recording's 951
    Make(f, 6, 0x46, st2);
    VCan3Frame(&c, f);
    Check(c.steer == (1 << 9 | 0xDB << 1 | 1), "0x46: (d2 & 0x7F) << 9 | d3 << 1 | d4 >> 7");
    unsigned char ge[8] = { 0xC5, 0, 0, 0, 0, 0, 0, 0 };
    Make(f, 3, 0x50, ge);
    VCan3Frame(&c, f);
    Check(c.hasGear && c.gear == 5, "0x50: gear = data[0] & 7");
    unsigned char dr[8] = { 0x81, 0, 0, 0, 0, 0, 0, 0 };
    Make(f, 5, 0xde, dr);
    VCan3Frame(&c, f);
    Check(c.hasDoors && c.doors == 0x81, "0xde: doors = data[0]");
    unsigned char ss[8] = { 0x80, 0x05, 0, 0, 0, 0, 0, 0 };
    Make(f, 0, 0xc6, ss);
    VCan3Frame(&c, f);
    Check(c.hasStatus && c.status == 0x8005, "0xc6: status = data[0] << 8 | data[1]");
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
    static const int other[][2] = { { 3, 0x17 }, { 6, 0x53 }, { 5, 0x46 }, { 5, 0xd7 }, { 0, 0xde }, { 0, 0x86 },
                                    { 1, 0xc6 }, { 4, 0x50 } };
    for (int i = 0; i < 8; i++) {
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
    Check(c.gear == 2 && c.doors == 0xC2 && c.status == 0xC200, "feed: each frame decoded");
    unsigned char bad[16];
    memcpy(bad, s + 3, 16);
    bad[15] ^= 0x10;
    VCan3Feed(&c, bad, 16);
    Check(c.frames == 3, "feed: a broken sum is no frame");
    VCan3Feed(&c, s + 3, 16);   // the 16 broken bytes go one by one before this frame is found
    Check(c.frames == 4 && c.skipped == 3 + 16, "feed: found again after a broken one");
    Check(VCan3Feed(&c, s, 0) == 0, "feed: nothing read");
}

// The tester's recording: chunks [ms u32 LE][len u16 LE][bytes] (VestaHome canrec.h)
static void TestReal()
{
    FILE* f = fopen("..\\MAIN\\HU_logs\\canpanel logs\\4123\\VestaLogs\\com3_261127_214806.bin", "rb");
    if (!f) {
        printf("(no MAIN: the real recording skipped)\n");
        return;
    }
    static unsigned char b[400000];
    int len = (int)fread(b, 1, sizeof(b), f);
    fclose(f);
    VCan3 c;
    VCan3Init(&c);
    int found = 0;
    for (int i = 0; i + 6 <= len;) {
        int n = b[i + 4] | b[i + 5] << 8;
        found += VCan3Feed(&c, b + i + 6, n);
        i += 6 + n;
    }
    Check(c.frames == 19292 && found == 19292 && c.skipped == 2, "real: 19292 frames, 2 bytes before the first");
    Check(c.hasPos && c.lat != 0 && c.lon != 0, "real: a position");
    Check(c.hasMove && c.kn100 == 0 && c.day == 27 && c.month == 11 && c.year == 26, "real: parked, 27.11.26");
    Check(c.hasGear && c.gear == 5 && c.hasSteer && c.steer == 951 && c.hasDoors && c.doors == 0,
          "real: gear 5, the wheel 951, doors 0");
    Check(!c.hasSpeed, "real: no 0x17 while parked");
}

int main()
{
    TestValid();
    TestFields();
    TestFeed();
    TestReal();
    printf("test_vcan3: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
