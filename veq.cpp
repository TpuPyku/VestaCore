// veq.cpp - VestaCore: the fine equalizer (see veq.h).
#include "veq.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

const int kEqFreq[EQ_BANDS] = { 31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

static int Clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static int Q28(double v)
{
    return (int)floor(v * (1 << 28) + 0.5);
}

static void Peak(EqCoef* c, double hz, double db, int rate)   // RBJ's peaking filter, Q 1.41 (an octave)
{
    double a = pow(10.0, db / 40), w = 2 * 3.14159265358979 * hz / rate, cs = cos(w), al = sin(w) / (2 * 1.41);
    double a0 = 1 + al / a;
    c->b0 = Q28((1 + al * a) / a0);
    c->b1 = Q28(-2 * cs / a0);
    c->b2 = Q28((1 - al * a) / a0);
    c->a1 = c->b1;
    c->a2 = Q28((1 - al / a) / a0);
}

static void Make(EqBank* s, const int* gains, int rate)
{
    int top = 0;
    s->flat = true;
    s->n = 0;
    while (s->n < EQ_BANDS && kEqFreq[s->n] < rate * 45 / 100)
        s->n++;
    for (int i = 0; i < s->n; i++) {
        int g = Clamp(gains[i], -EQ_MAX, EQ_MAX);
        Peak(&s->c[i], kEqFreq[i], g / 10.0, rate);
        s->flat &= g == 0;
        top = g > top ? g : top;
    }
    s->pre = (int)floor(32768 * pow(10.0, -top / 200.0) + 0.5);   // (the boosts' headroom)
}

void EqInit(Eq* e, const int* gains, int rate, int chans)
{
    memset(e, 0, sizeof(*e));
    e->rate = rate;
    e->chans = Clamp(chans, 1, 2);
    Make(&e->set[0], gains, rate);
}

void EqChange(Eq* e, const int* gains)
{
    long next = !e->cur;
    Make(&e->set[next], gains, e->rate);
    InterlockedExchange((LONG*)&e->cur, next);
}

// One band, one channel, one sample (Q8 in and out). The rounding's remainder fed into the next sum (err): without it
// the bass filters' poles, close to 1, hold a small constant forever (a dead band: an impulse at 22050 Hz left 30).
static int Band(const EqCoef& c, int x, int* x1, int* x2, int* y1, int* y2, int* err)
{
    __int64 acc = (__int64)c.b0 * x + (__int64)c.b1 * *x1 + (__int64)c.b2 * *x2 - (__int64)c.a1 * *y1 -
                  (__int64)c.a2 * *y2 + *err;
    int y = (int)(acc >> 28);
    *err = (int)(acc - ((__int64)y << 28));
    *x2 = *x1;
    *x1 = x;
    *y2 = *y1;
    *y1 = y;
    return y;
}

void EqRun(Eq* e, short* pcm, int frames)
{
    const EqBank& s = e->set[e->cur];
    if (s.flat)
        return;
    for (int f = 0; f < frames; f++) {
        for (int ch = 0; ch < e->chans; ch++) {
            short* p = pcm + f * e->chans + ch;
            int v = (*p * s.pre) >> 7;   // Q8, the headroom taken
            for (int b = 0; b < s.n; b++)
                v = Band(s.c[b], v, &e->x1[b][ch], &e->x2[b][ch], &e->y1[b][ch], &e->y2[b][ch], &e->err[b][ch]);
            *p = (short)Clamp((v + 128) >> 8, -32768, 32767);
        }
    }
}

int EqGainsText(const int* gains, char* out, int cap)
{
    int n = 0;
    for (int i = 0; i < EQ_BANDS; i++) {
        int k = _snprintf(out + n, cap - n, i ? ",%d" : "%d", gains[i]);
        if (k < 0 || n + k >= cap)
            return -1;
        n += k;
    }
    return n;
}

bool EqGainsParse(const char* s, int* gains)
{
    for (int i = 0; i < EQ_BANDS; i++) {
        char* end;
        long v = strtol(s, &end, 10);
        if (end == s || (i < EQ_BANDS - 1 ? *end != ',' : *end && *end != '\t' && *end != '\r' && *end != '\n'))
            return false;
        gains[i] = (int)v;
        s = end + (i < EQ_BANDS - 1);
    }
    return true;
}
