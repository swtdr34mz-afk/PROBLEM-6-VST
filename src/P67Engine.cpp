/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The firmware's engine (dsp/engine.c, fx.c, clock.c: C) compiled as one C++ class: its file-scope state becomes the
 * members of an instance (the 'static' storage class is taken away while they are read in), its functions member
 * functions. The code is the firmware's, unchanged but for its P67_PLUGIN guards (the rate is the host's: sr_). */
#include "P67Engine.h"
#include "plat.h"
#include "fastmath.h"
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

/* the oscillators' tables (engine.c bl_add): a Kaiser-windowed sinc (cutoff 0.9 of Nyquist, beta 8) over 16 samples,
 * integrated once (the band-limited step) and its residual once more (the band-limited ramp's), at 1024 points a
 * sample. Made once, shared by every instance (read only) */
static const float *p67_blep_tables()
{
    static const std::vector<float> t = [] {
        const int H = 8, OS = 1024, N = 2 * H * OS + 2;
        const double fc = 0.9, beta = 8.0, pi = 3.14159265358979323846;
        auto i0 = [](double x) {         /* the modified Bessel function I0, by its series */
            double s = 1, term = 1;
            for (int k = 1; k < 60; k++) {
                term *= (x / (2 * k)) * (x / (2 * k));
                s += term;
            }
            return s;
        };
        std::vector<double> h(N), B(N), R(N);
        for (int i = 0; i < N; i++) {
            double x = -H + (double)i / OS, r = x / H, w = std::fabs(r) <= 1 ? i0(beta * std::sqrt(1 - r * r)) / i0(beta) : 0;
            double sc = x == 0 ? 1.0 : std::sin(pi * fc * x) / (pi * fc * x);
            h[i] = fc * sc * w;
        }
        double acc = 0;                  /* the step: the kernel's integral (trapezoids), 0 .. 1 */
        B[0] = 0;
        for (int i = 1; i < N; i++)
            B[i] = (acc += 0.5 * (h[i - 1] + h[i]) / OS);
        double total = B[2 * H * OS];
        for (int i = 0; i < N; i++)
            B[i] = B[i] / total;
        acc = 0;                         /* the ramp's residual: the integral of the step less the ideal step */
        R[0] = 0;
        for (int i = 1; i < N; i++) {
            double x0 = -H + (double)(i - 1) / OS, x1 = -H + (double)i / OS;
            double e0 = B[i - 1] - (x0 >= 0 ? 1.0 : 0.0), e1 = B[i] - (x1 >= 0 ? 1.0 : 0.0);
            if (x0 < 0 && x1 >= 0)       /* (the step between: each side its own) */
                e0 = B[i - 1];
            R[i] = (acc += 0.5 * (e0 + e1) / OS);
        }
        double drift = R[2 * H * OS];    /* (nought by symmetry, but for rounding: so it ends exactly at 0) */
        std::vector<float> out(2 * N);
        for (int i = 0; i < N; i++) {
            out[i] = (float)B[i];
            out[N + i] = (float)(R[i] - drift * std::min(1.0, (double)i / (2 * H * OS)));
        }
        out[N - 1] = 1.0f;
        out[2 * N - 1] = 0.0f;
        return out;
    }();
    return t.data();
}

#define P67_IN_CLASS
struct P67Impl {
    float sr_ = 44100.0f;
#define static
#include "core.h"
#include "engine.c"
#include "fx.c"
#include "clock.c"
#undef static
    /* the engine runs in whole chunks (CH samples), always on the same grid, whatever the host's blocks: what a
     * chunk made beyond what the host asked for waits here for the next call. So the sound never depends on the
     * buffer size or on where MIDI splits a block, and a note lands on the next grid point (at most CH - 1 samples:
     * 0.34 ms at 44.1 kHz); no latency is added */
    float gl[CH] = {}, gr[CH] = {};
    unsigned gpos = CH;
};
P67Engine::P67Engine() : d(new P67Impl()) {}
P67Engine::~P67Engine() = default;

void P67Engine::prepare(double sampleRate)
{
    d.reset(new P67Impl());              /* (value-initialised: all zero, then the members' own starting values) */
    d->sr_ = (float)sampleRate;
    d->eng_init();
}
void P67Engine::patch(const uint8_t *p) { d->eng_patch(p); }
void P67Engine::noteOn(int id, int note, float vel, float gain) { d->eng_note_on_gain(id, note, vel, gain); }
void P67Engine::noteOff(int id) { d->eng_note_off(id); }
void P67Engine::press(int id, float p) { d->eng_press(id, p); }
void P67Engine::pressAll(float p) { d->eng_press_all(p); }
void P67Engine::sustain(bool on) { d->eng_sustain(on ? 1 : 0); }
void P67Engine::bend(float v) { d->eng_bend(v); }
void P67Engine::modWheel(float v) { d->eng_mod(v); }
void P67Engine::cutoffCC(int v) { d->eng_cutoff_cc(v); }
void P67Engine::hpExtra(int vel, int keytrk, int fenvVel) { d->eng_hp_extra(vel, keytrk, fenvVel); }
void P67Engine::allOff() { d->eng_all_off(); }
void P67Engine::panic() { d->eng_panic(); }
void P67Engine::tempo(float bpm, float swing) { d->eng_tempo(bpm, swing); }
void P67Engine::clockPos(double ppq) { d->eng_clock_pos(ppq); }
void P67Engine::arp(bool on, int mode, int rate, int octaves, float gate) { d->eng_arp(on ? 1 : 0, mode, rate, octaves, gate); }
void P67Engine::arpKey(int note, bool down, float vel) { d->eng_arp_key(note, down ? 1 : 0, vel); }
void P67Engine::arpHold(bool on) { d->eng_arp_hold(on ? 1 : 0); }
bool P67Engine::arpOn() const { return d->eng_arp_on() != 0; }
uint32_t P67Engine::arpSteps() const { return d->eng_arp_steps(); }
int P67Engine::voices() const { return d->eng_voices(); }
float P67Engine::voiceEnv(int v) const { return d->eng_voice_env(v); }

void P67Engine::render(float *left, float *right, int n)
{
    P67Impl &e = *d;
    while (n > 0) {
        if (e.gpos >= CH) {              /* the next chunk */
            e.eng_render(e.gl, e.gr, CH, 1.0f);
            e.gpos = 0;
        }
        int k = (int)(CH - e.gpos) < n ? (int)(CH - e.gpos) : n;
        std::memcpy(left, e.gl + e.gpos, sizeof(float) * (size_t)k);
        std::memcpy(right, e.gr + e.gpos, sizeof(float) * (size_t)k);
        e.gpos += (unsigned)k;
        left += k;
        right += k;
        n -= k;
    }
}
