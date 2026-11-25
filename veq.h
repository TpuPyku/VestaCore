// veq.h - VestaCore: the "fine" equalizer (the user 2026-11-25: the stock one has 3 bands in the MCU and jumps too far
// at one step - "coarse"; ours 10 bands in 0.5 dB steps, for the sound the unit's processor plays itself: VestaHome's
// music and internet radio, VestaAuto's Android Auto). 10 peaking filters (RBJ biquads, Q 1.41 - an octave each) at
// 31 Hz ... 16 kHz, each -12..+12 dB; integer only when filtering (coefficients Q28, samples Q8, 64-bit sums - SMULL
// on the unit's ARM); the boosts' headroom taken off the input so nothing clips; all bands at 0 - the sound untouched.
// Interleaved 16-bit PCM, 1 or 2 channels. The gains may change while another thread filters (EqChange swaps a whole
// set of coefficients at once).
#ifndef VESTA_EQ_H
#define VESTA_EQ_H

enum { EQ_BANDS = 10, EQ_MAX = 120 };   // gains in tenths of a dB: -120..120

struct EqCoef {
    int b0, b1, b2, a1, a2;   // Q28
};
struct EqBank {
    EqCoef c[EQ_BANDS];
    int    pre;               // the input's scale, Q15 (32768 = 1)
    int    n;                 // the bands in use (those below 0.45 of the rate)
    bool   flat;
};
struct Eq {
    EqBank         set[2];
    volatile long cur;        // the set in use
    int           rate, chans;
    int           x1[EQ_BANDS][2], x2[EQ_BANDS][2], y1[EQ_BANDS][2], y2[EQ_BANDS][2];   // per band, per channel (Q8)
    int           err[EQ_BANDS][2];   // the rounding's remainder (Q28)
};

extern const int kEqFreq[EQ_BANDS];   // Hz

// gains: EQ_BANDS values in tenths of a dB (clamped to +-EQ_MAX); rate (Hz) and channels (1, 2) of the sound.
void EqInit(Eq* e, const int* gains, int rate, int chans);
void EqChange(Eq* e, const int* gains);   // new gains (the filters' state kept: no click)
// Filters n frames of interleaved 16-bit samples in place.
void EqRun(Eq* e, short* pcm, int frames);
// The gains as text "a,b,..." (VestaHome tells the add-ons) and back; false - not 10 numbers.
int  EqGainsText(const int* gains, char* out, int cap);
bool EqGainsParse(const char* s, int* gains);

#endif
