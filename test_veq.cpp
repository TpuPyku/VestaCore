// test_veq.cpp - veq.cpp on the PC: the fine equalizer's response, its headroom, untouched when flat.
#include "veq.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

enum { kRate = 44100, kN = 44100 };
static short s_pcm[kN * 2];

static void Sine(double hz, double amp)   // stereo: the sine left, silence right
{
    for (int i = 0; i < kN; i++) {
        s_pcm[i * 2] = (short)(amp * sin(2 * 3.14159265358979 * hz * i / kRate));
        s_pcm[i * 2 + 1] = 0;
    }
}

static double Level(int ch)   // the RMS of the last half second, of one channel
{
    double s = 0;
    for (int i = kN / 2; i < kN; i++)
        s += (double)s_pcm[i * 2 + ch] * s_pcm[i * 2 + ch];
    return sqrt(s / (kN / 2));
}

static double Db(double a, double b)
{
    return 20 * log10(a / b);
}

static double Through(const int* g, double hz, double amp)   // the gain (dB) of the equalizer at hz
{
    Eq e;
    EqInit(&e, g, kRate, 2);
    Sine(hz, amp);
    double in = Level(0);
    EqRun(&e, s_pcm, kN);
    return Db(Level(0), in);
}

static void TestFlat()
{
    int g[EQ_BANDS] = { 0 };
    Eq e;
    EqInit(&e, g, kRate, 2);
    Sine(440, 20000);
    static short copy[kN * 2];
    memcpy(copy, s_pcm, sizeof(copy));
    EqRun(&e, s_pcm, kN);
    Check(!memcmp(copy, s_pcm, sizeof(copy)), "flat: the sound untouched");
}

static void TestBands()
{
    int g[EQ_BANDS] = { 0 };
    g[5] = 60;   // 1 kHz +6 dB: the headroom takes 6 dB off everything
    double at = Through(g, 1000, 8000), away = Through(g, 100, 8000);
    Check(fabs(at - away - 6) < 0.6, "+6 dB at 1 kHz: 6 dB above the away bands");
    Check(fabs(at) < 0.6 && fabs(away + 6) < 0.6, "+6 dB: the headroom - the peak at 0, the rest -6");
    g[5] = -120;
    at = Through(g, 1000, 8000);
    away = Through(g, 100, 8000);
    Check(fabs(at + 12) < 1.5 && fabs(away) < 0.5, "-12 dB at 1 kHz: cut there only, no headroom for cuts");
    memset(g, 0, sizeof(g));
    g[0] = 120;
    at = Through(g, 31, 8000);
    Check(fabs(at) < 1.0 && Through(g, 4000, 8000) < -11, "31 Hz +12: the bass up against the rest");
    for (int i = 0; i < EQ_BANDS; i++)
        g[i] = -60;   // every band down: no headroom, all 6 dB lower (not taken as flat)
    Check(Through(g, 1000, 8000) < -5, "all bands -6: lower everywhere (the neighbours add up)");
    memset(g, 0, sizeof(g));
    g[5] = -120;
    Eq e1;
    EqInit(&e1, g, kRate, 1);
    short one[4] = { 30000, 0, 0, 0 };
    EqRun(&e1, one, 4);
    Check(one[0] != 30000, "the first sample is filtered too");
    memset(g, 0, sizeof(g));
    g[0] = 5;   // half a dB: a fine step
    double half = Through(g, 4000, 8000) - Through(g, 31, 8000);
    Check(half < -0.3 && half > -0.7, "0.5 dB: a step of half a decibel");
}

static void TestSafe()
{
    int g[EQ_BANDS];
    for (int i = 0; i < EQ_BANDS; i++)
        g[i] = 500;   // (clamped to 12 dB)
    Eq e;
    EqInit(&e, g, kRate, 2);
    Sine(1000, 32767);
    EqRun(&e, s_pcm, kN);
    bool smooth = true;   // no wrap-around: neighbours never jump by more than a sine's step can
    for (int i = 1; i < kN; i++)
        smooth &= abs(s_pcm[i * 2] - s_pcm[i * 2 - 2]) < 12000;
    Check(smooth, "all at the most, a full-scale sine: no wrap-around");
    Check(Level(1) == 0, "the right channel silent stays silent");
    Eq z;
    EqInit(&z, g, 22050, 1);
    Check(z.set[z.cur].n == 9, "22050 Hz: 16 kHz above its limit - left out");
    short imp[4000] = { 0 };
    imp[0] = 30000;
    EqRun(&z, imp, 4000);
    Check(imp[3999] == 0 && imp[3998] == 0, "an impulse dies away (stable at the most)");
    int flat[EQ_BANDS] = { 0 };
    EqChange(&e, flat);
    Sine(440, 20000);
    static short copy[kN * 2];
    memcpy(copy, s_pcm, sizeof(copy));
    EqRun(&e, s_pcm, kN);
    Check(!memcmp(copy, s_pcm, sizeof(copy)), "changed to flat while running: untouched again");
}

static void TestText()
{
    int g[EQ_BANDS] = { -120, -5, 0, 5, 10, 20, 30, 40, 50, 120 }, b[EQ_BANDS];
    char t[120];
    Check(EqGainsText(g, t, sizeof(t)) > 0 && !strcmp(t, "-120,-5,0,5,10,20,30,40,50,120"), "text: written");
    Check(EqGainsParse(t, b) && !memcmp(g, b, sizeof(g)), "text: read back");
    Check(!EqGainsParse("1,2,3", b), "text: too few - none");
    Check(!EqGainsParse("1,2,3,4,5,6,7,8,9,x", b), "text: not a number - none");
    Check(EqGainsText(g, t, 10) < 0, "text: no room - none");
}

int main()
{
    TestFlat();
    TestBands();
    TestSafe();
    TestText();
    printf("test_veq: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
