// vcan3.h - VestaCore: the CAN panel's COM3 frames decoded (the user 2026-11-27: "будем писать декодер"; first for
// VestaHome's live view while it records COM3, then GPS without the panel). The CAN module sends 16-byte frames
// 00 00 00 00 | type | code | data[8] | sum[2], sum = (0x10000 - the first 14 bytes' sum) & 0xFFFF big-endian
// (can_panel.exe 1.3.6 / 1.3.6.1, PROJECT.md). The fields known from its dispatcher; what each means is checked
// against recordings with marks. Pure: no files, no ports.
#ifndef VCAN3_H
#define VCAN3_H

// what is open (type 3 0x50): data[6] 0x20, data[6] 0x08, data[7] 0x02 (the rear doors, one flag), data[4] 0x08, data[7] 0x08
enum { VCO_FL = 1, VCO_FR = 2, VCO_REAR = 4, VCO_TRUNK = 8, VCO_HOOD = 16 };
// the lights (type 5 0xde data[0], as it comes; the fogs' two and the turns' two by the panel's pictures' order -
// checked by vhcan6 for the beams and the hazard lights only)
enum { VCL_HIGH = 1, VCL_LOW = 2, VCL_SIDE = 4, VCL_FOGF = 8, VCL_FOGR = 16, VCL_LEFT = 32, VCL_RIGHT = 64 };

struct VCan3 {
    unsigned char win[16];    // the bytes not yet a frame
    int           have;
    unsigned long frames;     // frames with a right sum
    unsigned long skipped;    // bytes dropped finding them
    // the last value of each field; has* false - not seen yet (can_panel 1.3.6.1's reading, PROJECT.md "Что идёт по CAN")
    bool hasSpeed;  int  speed10;     // type 2 0x17: km/h x 10 (other cars; not this one's)
    // type 6 0x46 the trip computer, each x 0.1: l/100 km, km, l used, km/h
    bool hasTrip;   int  use10, trip10, used10, avg10;
    bool hasBody;   int  ign, open;   // type 3 0x50: the ignition data[0] & 7 (5 on, 4 off), VCO_* open
    bool hasLight;  int  light;       // VCL_*
    bool hasHand;   bool hand;        // type 4 0xf8 data[0] & 8: the handbrake up
    bool hasRpm;    int  rpm;         // type 1 0x86 (d0 << 8 | d1) >> 3
    bool hasAir;    int  airC;        // type 3 0xb7 data[0] - 40
    bool hasCool;   int  coolC;       // type 5 0xda data[0] - 40 (0 not taken)
    bool hasFuel;   int  fuel2, rangeKm;   // type 6 0xfb: data[3] in 0.5 l, (d1 & 15) << 8 | d2 km
    bool hasClima;  int  climaT, fan; // type 6 0x99: data[1] (8 LOW, 10 HI, else / 2 degrees), data[3] >> 4 (15 off, 0 auto)
    // type 0 0xc6: the steering wheel, raw data[0] << 8 | data[1]; degrees = (raw - 32767) / 15, raw 0 - 0
    // (can_panel 1.3.6.1's @ANG builder VA 0x17f2c, 2026-11-27; not 0x46); wheelN: frames of it so far
    bool hasWheel;  int  wheel;  unsigned long wheelN;
    bool hasCar;    int  carKmh100;   // type 5 0xd7: the car's speed, km/h x 100 (the panel's "@,%03d" line)
    bool hasPos;    long lat, lon;    // type 4 0xa2: 1/600000 degree, signed (posN: frames of it so far)
    unsigned long posN;
    bool hasMove;   int  kn100, course100, day, month, year;   // type 4 0xa6: knots x 100, degrees x 100, the date
    bool hasUtc;    unsigned long utcMs;                         // type 4 0xa8: ms since midnight UTC
};

void VCan3Init(VCan3* c);
bool VCan3Valid(const unsigned char* f);           // 16 bytes: the zero head and the sum
void VCan3Frame(VCan3* c, const unsigned char* f); // one valid frame into the fields
int  VCan3Feed(VCan3* c, const unsigned char* p, int n);   // bytes as read; the frames found in them

// The lines can_panel writes to VSP4 / VSP5 / COM7 (COM4 / COM5 for the readers), for the navigators when the
// panel is off (the user 2026-11-27). Each "...*XX" + CR LF (XX: the XOR after the first char); false - not known yet
// or no room. RMC's course in degrees (the panel's is x 1/127, wrong); GGA: fix 1, 12 satellites, as the panel's.
bool VCan3Nmea(char* out, int cap, const char* body);
bool VCan3Ang(const VCan3* c, char* out, int cap);   // "@ANG,<degrees .1>,"
bool VCan3Car(const VCan3* c, char* out, int cap);   // "@,<km/h 3 digits>,2.34" (2.34: the panel's when no module version)
bool VCan3Rmc(const VCan3* c, char* out, int cap);
bool VCan3Gga(const VCan3* c, char* out, int cap);

#endif
