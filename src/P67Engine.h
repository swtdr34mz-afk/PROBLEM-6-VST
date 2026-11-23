/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* PROPHET 67's engine for the plugin: the FM-1 firmware's sound engine (dsp/, the same code), one instance per
 * plugin. P67Engine.cpp builds the firmware's C engine as one class, so every instance has its own voices, effects and
 * clock. All calls from the audio thread. */
#pragma once
#include <cstdint>
#include <memory>

extern "C" {
#include "engine.h"
}

struct P67Impl;

class P67Engine
{
public:
    P67Engine();
    ~P67Engine();
    void prepare(double sampleRate);         /* a fresh engine at this rate */
    void patch(const uint8_t *p);            /* the sound: NPARAM bytes */
    void noteOn(int id, int note, float vel, float gain);
    void noteOff(int id);
    void press(int id, float p);
    void pressAll(float p);
    void sustain(bool on);
    void bend(float v);
    void modWheel(float v);
    void cutoffCC(int v);
    void allOff();
    void panic();
    void tempo(float bpm, float swing);
    void clockPos(double ppq);
    void arp(bool on, int mode, int rate, int octaves, float gate);
    void arpKey(int note, bool down, float vel);
    void arpHold(bool on);
    bool arpOn() const;
    void render(float *left, float *right, int n);   /* n frames, any count */
    int voices() const;
    float voiceEnv(int v) const;

private:
    std::unique_ptr<P67Impl> d;
};
