/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The firmware's engine (dsp/engine.c, fx.c, clock.c: C) compiled as one C++ class: its file-scope state becomes the
 * members of an instance (the 'static' storage class is taken away while they are read in), its functions member
 * functions. The code is the firmware's, unchanged but for its P67_PLUGIN guards (the rate is the host's: sr_). */
#include "P67Engine.h"
#include "plat.h"
#include "fastmath.h"
#include <cstring>

#define P67_IN_CLASS
struct P67Impl {
    float sr_ = 44100.0f;
#define static
#include "core.h"
#include "engine.c"
#include "fx.c"
#include "clock.c"
#undef static
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
void P67Engine::allOff() { d->eng_all_off(); }
void P67Engine::panic() { d->eng_panic(); }
void P67Engine::tempo(float bpm, float swing) { d->eng_tempo(bpm, swing); }
void P67Engine::clockPos(double ppq) { d->eng_clock_pos(ppq); }
void P67Engine::arp(bool on, int mode, int rate, int octaves, float gate) { d->eng_arp(on ? 1 : 0, mode, rate, octaves, gate); }
void P67Engine::arpKey(int note, bool down, float vel) { d->eng_arp_key(note, down ? 1 : 0, vel); }
void P67Engine::arpHold(bool on) { d->eng_arp_hold(on ? 1 : 0); }
bool P67Engine::arpOn() const { return d->eng_arp_on() != 0; }
int P67Engine::voices() const { return d->eng_voices(); }
float P67Engine::voiceEnv(int v) const { return d->eng_voice_env(v); }

void P67Engine::render(float *left, float *right, int n)
{
    int32_t buf[2 * 128];
    const float g = 8388607.0f;          /* (the engine writes 24-bit integers at this gain) */
    while (n > 0) {                      /* in blocks of 128, as on the FM-1 */
        int k = n < 128 ? n : 128, i;
        d->eng_render(buf, (uint32_t)k, g);
        for (i = 0; i < k; i++) {
            left[i] = (float)buf[2 * i] * (1.0f / 8388607.0f);
            right[i] = (float)buf[2 * i + 1] * (1.0f / 8388607.0f);
        }
        left += k;
        right += k;
        n -= k;
    }
}
