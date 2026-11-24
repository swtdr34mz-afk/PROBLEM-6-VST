// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 swtdr34mz (PROPHET 67)
//
// PROPHET 67 for the MPC: the same engine as the VST3 and the FM-1 firmware (dsp/, src/P67Engine.*), hosted by the
// MPC Plugin SDK's framework (mpc_plugin.h). mpc/build.py copies the engine's files beside this one and generates
// p67_data.cpp (the parameters, the 16 factory programs, every value's text) before the SDK builds it.
//
// Signal flow (as in the VST): 7 voices, each two oscillators (triangle-saw-pulse, sub, noise, sync, LOW FREQ), poly
// mod, a 2-pole high-pass into the 4-pole low-pass (twice the rate in HIGH quality), filter and amp envelopes,
// VINTAGE and SPREAD; two LFOs (LFO 1 to audio rate, S+H at the top is noise); then distortion, chorus, delay and
// reverb (ROOM, HALL, PLATE, SPRING, SPACE). The arp runs on the MPC's tempo and locks to its bar while it plays.
//
// Parameters (their ids never change; new ones only at the end):
//   0        VOLUME (the host nudges parameter 0 on every prepare: harmless here, it ramps)
//   1..74    the 74 sound parameters, as bytes 0..127 or choices, exactly the FM-1's / the VST's
//   75..77   HP VELOCITY, HP KEYBOARD, FILTER ENVELOPE VELOCITY (the VST's own)
//   78..84   ARP on, MODE, RATE, OCTAVES, GATE, HOLD, SWING (settings: the programs leave them)
//   85       QUALITY: HIGH (the low-pass at 2x) or ECO (less CPU; for the 32-bit MPCs and big projects)
//
// MIDI in: notes and velocity, pitch bend (BEND RANGE), poly and channel pressure (AFTERTOUCH), CC 1 mod wheel,
// CC 64 sustain, CC 74 cutoff, CC 120 all sound off, CC 121 reset controllers, CC 123 all notes off.

#include "mpc_plugin.h"
#include "P67Engine.h"
#include "p67_data.h"

#include <cmath>
#include <cstring>

namespace {

// the MPC's screen names: short (about 12 characters), each its own (ui.json finds controls by them)
const char *const NAMES[NPARAM] = {
    "Osc1 Freq",   "Osc1 Shape",  "Osc1 PW",     "Sub Oct",                         // OSC 1
    "Osc2 Freq",   "Osc2 Fine",   "Osc2 Shape",  "Osc2 PW",                         // OSC 2
    "Mix Osc1",    "Mix Osc2",    "Noise",       "Osc2 Mode",                       // MIXER
    "Cutoff",      "Resonance",   "LP Env Amt",  "HP Cutoff",                       // FILTER
    "HP Reso",     "LP Keyboard", "LP Velocity", "HP Env Amt",                      // FILTER 2
    "F Attack",    "F Decay",     "F Sustain",   "F Release",                       // FILTER ENV
    "A Attack",    "A Decay",     "A Sustain",   "A Release",                       // AMP ENV
    "LFO1 Freq",   "LFO1 Amount", "LFO1 Shape",  "LFO1 Dest",                       // LFO 1
    "PM Filt Env", "PM Osc2",     "PM Osc1 To",  "PM Filter",                       // POLY MOD
    "Chorus Mix",  "Chorus Rate", "Chorus Dpth", "Distortion",                      // FX A
    "Rev Type",    "Rev Mix",     "Rev Decay",   "Rev Tone",                        // FX B
    "Space Mod",   "Shimmer",     "Swell",       "Predelay",                        // SPACE
    "Glide",       "Amp Veloc",   "Unison",      "Bend Range",                      // VOICE
    "Vintage",     "Spread",                                                        // VINTAGE, SPREAD
    "LFO2 Freq",   "LFO2 Amount", "LFO2 Shape",  "LFO2 Dest",                       // LFO 2
    "LFO1 Sync",   "LFO2 Sync",   "LFO1 Retrig", "LFO2 Retrig",                     // LFO SYNC
    "AT Dest",     "AT Amount",   "AT Curve",    "AT Smooth",                       // AFTERTOUCH
    "Delay L",     "Delay R",     "Repeats",     "Delay Mix",                       // DELAY
    "Dly Depth",   "Dly Rate",    "Dly Tone",    "Delay Sync",                      // DELAY MOD
};

enum {
    kVolume = 0,
    kSound = 1,                          // + P_* (the 74)
    kHpVel = kSound + NPARAM,
    kHpKey,
    kFenvVel,
    kArpOn,
    kArpMode,
    kArpRate,
    kArpOct,
    kArpGate,
    kArpHold,
    kSwing,
    kQuality,
    kNumParams
};
static_assert(kNumParams == 86, "the parameters' order is permanent");

class Prophet67Mpc : public mpc::Plugin
{
public:
    Prophet67Mpc() : mpc::Plugin(makeInfo())
    {
        addParam(mpc::floatParam("vol", "Volume", 0.0f, 1.0f, 0.7f, "", 2));
        for (int i = 0; i < NPARAM; i++) {
            std::string id = "p" + std::to_string(i);
            const uint8_t def = FACTORY[0].p[i];
            if (PARAMS[i].kind == PK_CHOICE) {
                std::vector<std::string> ch;
                for (int k = 0; k <= PARAMS[i].max; k++)
                    ch.push_back(PARAMS[i].choice[k]);
                addParam(mpc::choiceParam(id.c_str(), NAMES[i], ch, def));
            } else {
                addParam(mpc::intParam(id.c_str(), NAMES[i], 0, PARAMS[i].max, def));
            }
        }
        addParam(mpc::intParam("hp_vel", "HP Velocity", 0, 127, FACTORY[0].p[P_FVEL]));
        addParam(mpc::choiceParam("hp_keytrk", "HP Keyboard", {"OFF", "HALF", "FULL"}, FACTORY[0].p[P_KEYTRK]));
        addParam(mpc::intParam("fenv_vel", "FEnv Veloc", 0, 127, 0));
        addParam(mpc::boolParam("arp_on", "Arp", false));
        addParam(mpc::choiceParam("arp_mode", "Arp Mode", {"UP", "DOWN", "UP-DN", "RANDOM", "PLAYED"}, 0));
        addParam(mpc::choiceParam("arp_rate", "Arp Rate", {"1/8", "1/8T", "1/16", "1/16T", "1/32"}, 2));
        addParam(mpc::intParam("arp_oct", "Arp Octaves", 1, 4, 1));
        addParam(mpc::intParam("arp_gate", "Arp Gate", 5, 100, 50, "%"));
        addParam(mpc::boolParam("arp_hold", "Arp Hold", false));
        addParam(mpc::intParam("swing", "Swing", 0, 50, 0, "%"));
#if defined(__arm__)                     /* (the 32-bit MPCs: X, Live, Live II, One, Key, Force: ECO to start with) */
        addParam(mpc::choiceParam("quality", "Quality", {"HIGH", "ECO"}, 1));
#else
        addParam(mpc::choiceParam("quality", "Quality", {"HIGH", "ECO"}, 0));
#endif

        // the 16 factory programs: VOLUME, the sound, the filters' VELOCITY and KEYBOARD as on the FM-1 (the
        // high-pass's as the low-pass's) and the envelope's VELOCITY off; the arp, SWING and QUALITY they leave
        for (int k = 0; k < NPRESETS; k++) {
            std::vector<float> v;
            v.push_back(0.7f);
            for (int i = 0; i < NPARAM; i++)
                v.push_back((float)FACTORY[k].p[i]);
            v.push_back((float)FACTORY[k].p[P_FVEL]);
            v.push_back((float)FACTORY[k].p[P_KEYTRK]);
            v.push_back(0.0f);
            addProgram(std::string(FACTORY[k].name, strnlen(FACTORY[k].name, NAME_LEN)).c_str(), v);   /* (no nought at 12) */
        }
        engine.prepare(44100.0);         // (the MPC's rate: made here, on the UI thread, never in process)
        rate = 44100.0;
    }

    void prepare(double sampleRate, int) override
    {
        if (sampleRate != rate) {        // (only if the rate really changes: the MPC's is fixed at 44.1 kHz)
            engine.prepare(sampleRate);
            rate = sampleRate;
        }
        forceSend();
    }

    void reset() override              // resume, or a program / state loaded: the voices let go at once
    {
        engine.panic();
        for (int n = 0; n < 128; n++)
            held[n] = false;
        forceSend();
    }

    void process(mpc::ProcessContext &ctx) override
    {
        const int n = ctx.numFrames;
        float *L = ctx.outputs[0], *R = ctx.numOutputs > 1 ? ctx.outputs[1] : ctx.outputs[0];
        sync();
        /* the MPC's tempo, and its position while it plays (the arp, the synced LFOs and delay lock to its bar) */
        float bpm = ctx.transport.valid && ctx.transport.tempoValid ? (float)ctx.transport.bpm : 120.0f;
        engine.tempo(bpm, (float)getInt(kSwing) * 0.01f);
        if (ctx.transport.valid && ctx.transport.playing && ctx.transport.ppqValid)
            engine.clockPos(ctx.transport.ppqPos);
        /* the block rendered up to each MIDI event (the engine lands it on its 16-sample grid) */
        int done = 0;
        for (int i = 0; i < ctx.numMidiEvents; i++) {
            const mpc::MidiEvent &e = ctx.midi[i];
            int at = mpc::clamp(e.sampleOffset, 0, n);
            if (at > done) {
                engine.render(L + done, R + done, at - done);
                done = at;
            }
            midi(e);
        }
        if (n > done)
            engine.render(L + done, R + done, n - done);
        /* VOLUME, a ramp over the block (0.7: about 0 dB) */
        float v = (float)get(kVolume), g = v * v * 2.0f;
        if (lastGain < 0.0f)
            lastGain = g;
        float step = (g - lastGain) / (float)(n > 0 ? n : 1), gg = lastGain;
        for (int s = 0; s < n; s++) {
            gg += step;
            L[s] *= gg;
            if (R != L)
                R[s] *= gg;
        }
        lastGain = g;
    }

    bool formatValue(int index, float plain, std::string &out) override
    {
        int v = (int)std::lround(plain);
        if (index == kVolume) {
            char t[16];
            if (plain <= 0.0001f)
                out = "-inf dB";
            else {
                std::snprintf(t, sizeof t, "%.1f dB", 20.0 * std::log10(plain * plain * 2.0));
                out = t;
            }
            return true;
        }
        if (index >= kSound && index < kSound + NPARAM) {
            int p = index - kSound;
            v = mpc::clamp(v, 0, (int)PARAMS[p].max);
            if ((p == P_DL_TIME_L || p == P_DL_TIME_R) && getInt(kSound + P_DL_SYNC))
                out = DL_SYNC_NAME[v * NDLS / 128];   /* DELAY SYNC on: the times are notes */
            else
                out = P67_TEXT[p][v];
            return true;
        }
        if (index == kArpGate || index == kSwing) {
            out = std::to_string(v) + "%";
            return true;
        }
        return false;
    }

private:
    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo i;
        i.name = "PROPHET 67";
        i.vendor = "swtdr34mz";
        i.uniqueId = MPC_FOURCC('P', 'h', '6', '7');
        i.version = 1000;
        i.isInstrument = true;
        i.numInputs = 0;
        i.numOutputs = 2;
        i.tailSamples = 44100 * 8;   /* (the longest reverb's tail, about) */
        return i;
    }

    void forceSend()
    {
        patched = false;
        arpSent = false;
        lastGain = -1.0f;
    }

    /* the parameters to the engine, when they change (all of them read as atomics, safe here) */
    void sync()
    {
        uint8_t p[NPARAM];
        bool change = !patched;
        for (int i = 0; i < NPARAM; i++) {
            int v = mpc::clamp(getInt(kSound + i), 0, (int)PARAMS[i].max);
            p[i] = (uint8_t)v;
            change |= p[i] != sent[i];
        }
        int hv = getInt(kHpVel), hk = getInt(kHpKey), fv = getInt(kFenvVel);
        if (!patched || hv != hpVel || hk != hpKey || fv != fenvVel) {
            engine.hpExtra(hv, hk, fv);
            hpVel = hv;
            hpKey = hk;
            fenvVel = fv;
            change = true;
        }
        bool high = getInt(kQuality) == 0;
        if (!patched || high != qHigh) {
            engine.quality(high);
            qHigh = high;
        }
        if (change) {
            engine.patch(p);
            std::memcpy(sent, p, sizeof sent);
            patched = true;
        }
        bool on = getBool(kArpOn), hold = getBool(kArpHold);
        int mode = getInt(kArpMode), rt = getInt(kArpRate), oct = getInt(kArpOct), gate = getInt(kArpGate);
        if (!arpSent || on != arpOn || mode != arpMode || rt != arpRate || oct != arpOct || gate != arpGate) {
            if (!arpSent || on != arpOn) {   /* the arp on or off: what sounds lets go */
                engine.allOff();
                for (int k = 0; k < 128; k++)
                    held[k] = false;
            }
            engine.arp(on, mode, rt, oct, (float)gate * 0.01f);
        }
        if (!arpSent || hold != arpHold)
            engine.arpHold(hold);
        arpOn = on;
        arpMode = mode;
        arpRate = rt;
        arpOct = oct;
        arpGate = gate;
        arpHold = hold;
        arpSent = true;
    }

    void midi(const mpc::MidiEvent &e)
    {
        const uint8_t st = e.status();
        if (e.isNoteOn()) {
            int k = e.note();
            if (held[k])             /* (the same key again: let it go first) */
                noteUp(k);
            held[k] = true;
            if (arpOn)
                engine.arpKey(k, true, (float)e.velocity() / 127.0f);
            else
                engine.noteOn(k, k, (float)e.velocity() / 127.0f, 1.0f);
        } else if (e.isNoteOff()) {
            noteUp(e.note());
        } else if (e.isPitchBend()) {
            engine.bend((float)e.pitchBend() / 8192.0f);
        } else if (st == 0xA0) {         /* poly pressure */
            engine.press(e.data[1], (float)e.data[2] / 127.0f);
        } else if (e.isAftertouch()) {   /* channel pressure */
            engine.pressAll((float)e.data[1] / 127.0f);
        } else if (e.isController()) {
            int cc = e.controller(), v = e.controllerValue();
            if (cc == 1)
                engine.modWheel((float)v / 127.0f);
            else if (cc == 64)
                engine.sustain(v >= 64);
            else if (cc == 74)
                engine.cutoffCC(v);
            else if (cc == 120 || cc == 123) {   /* ALL SOUND OFF: silent at once; ALL NOTES OFF: their release */
                if (cc == 120)
                    engine.panic();
                else
                    engine.allOff();
                for (int k = 0; k < 128; k++)
                    held[k] = false;
            } else if (cc == 121) {      /* RESET ALL CONTROLLERS */
                engine.bend(0.0f);
                engine.modWheel(0.0f);
                engine.sustain(false);
                engine.pressAll(0.0f);
                engine.cutoffCC(-1);
            }
        }
    }

    void noteUp(int k)
    {
        if (!held[k])
            return;
        held[k] = false;
        if (arpOn)
            engine.arpKey(k, false, 0.0f);
        else
            engine.noteOff(k);
    }

    P67Engine engine;
    double rate = 0.0;
    uint8_t sent[NPARAM] = {};
    bool patched = false, qHigh = true;
    int hpVel = -1, hpKey = -1, fenvVel = -1;
    bool arpSent = false, arpOn = false, arpHold = false;
    int arpMode = 0, arpRate = 2, arpOct = 1, arpGate = 50;
    bool held[128] = {};
    float lastGain = -1.0f;
};

} // namespace

MPC_PLUGIN_ENTRY(Prophet67Mpc)
