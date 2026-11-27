// vcan3.h - VestaCore: the CAN panel's COM3 frames decoded (the user 2026-11-27: "будем писать декодер"; first for
// VestaHome's live view while it records COM3, then GPS without the panel). The CAN module sends 16-byte frames
// 00 00 00 00 | type | code | data[8] | sum[2], sum = (0x10000 - the first 14 bytes' sum) & 0xFFFF big-endian
// (can_panel.exe 1.3.6 / 1.3.6.1, PROJECT.md). The fields known from its dispatcher; what each means is checked
// against recordings with marks. Pure: no files, no ports.
#ifndef VCAN3_H
#define VCAN3_H

struct VCan3 {
    unsigned char win[16];    // the bytes not yet a frame
    int           have;
    unsigned long frames;     // frames with a right sum
    unsigned long skipped;    // bytes dropped finding them
    // the last value of each field; has* false - not seen yet
    bool hasSpeed;  int  speed10;     // type 2 0x17: km/h x 10
    bool hasSteer;  int  steer;       // type 6 0x46: the signed 16-bit angle, raw
    bool hasGear;   int  gear;        // type 3 0x50: data[0] & 7
    bool hasDoors;  int  doors;       // type 5 0xde: data[0] bits
    bool hasStatus; int  status;      // type 0 0xc6: data[0] << 8 | data[1]
    bool hasPos;    long lat, lon;    // type 4 0xa2: 1/600000 degree, signed
    bool hasMove;   int  kn100, course100, day, month, year;   // type 4 0xa6: knots x 100, degrees x 100, the date
    bool hasUtc;    unsigned long utcMs;                         // type 4 0xa8: ms since midnight UTC
};

void VCan3Init(VCan3* c);
bool VCan3Valid(const unsigned char* f);           // 16 bytes: the zero head and the sum
void VCan3Frame(VCan3* c, const unsigned char* f); // one valid frame into the fields
int  VCan3Feed(VCan3* c, const unsigned char* p, int n);   // bytes as read; the frames found in them

#endif
