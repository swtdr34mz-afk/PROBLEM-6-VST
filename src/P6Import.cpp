/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* LOAD P6 SYSEX (P6Import.h). The Prophet-6's program is 1024 bytes, sent "packed MS bit" (each 7 bytes after a byte
 * of their top bits). Where its parameters sit in those bytes (not in the order of the manual's NRPN table): */
#include "P6Import.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
enum {
    B_O1_FREQ = 0, B_O2_FREQ = 1, B_O2_FINE = 2, B_O1_SHAPE = 3, B_O2_SHAPE = 4, B_O1_PW = 5, B_O2_PW = 6,
    B_O1_LEVEL = 7, B_O2_LEVEL = 8, B_SUB = 9, B_NOISE = 10, B_SYNC = 11, B_O2_KBD = 12, B_O2_LOFREQ = 13,
    B_GLIDE_RATE = 14, B_GLIDE_ON = 16, B_BEND = 17, B_SLOP = 18,
    B_LP_FREQ = 19, B_LP_RES = 20, B_LP_KBD = 21, B_LP_VEL = 22, B_HP_FREQ = 23, B_HP_RES = 24, B_HP_KBD = 25,
    B_HP_VEL = 26, B_VOLUME = 27, B_SPREAD = 28, B_LP_ENV = 29, B_HP_ENV = 30, 
    B_FA = 35, B_AA = 36, B_FD = 37, B_AD = 38, B_FS = 39, B_AS = 40, B_FR = 41, B_AR = 42, B_AMP_VEL = 43,
    B_FXA_TYPE = 44, B_FXB_TYPE = 45, B_FX_ON = 46, B_FXA_MIX = 48, B_FXB_MIX = 49, B_FXA_P1 = 50, B_FXB_P1 = 51,
    B_FXA_P2 = 52, B_FXB_P2 = 53, B_FXA_SYNC = 54, B_FXB_SYNC = 55, B_DIST = 58,
    B_LFO_FREQ = 59, B_LFO_SYNC = 61, B_LFO_SHAPE = 62, B_LFO_AMT = 63,
    B_LFO_O1 = 64, B_LFO_O2 = 65, B_LFO_PW = 66, B_LFO_LP = 67, B_LFO_HP = 68, B_LFO_AMP = 69,
    B_AT_AMT = 70, B_AT_O1 = 71, B_AT_O2 = 72, B_AT_LP = 73, B_AT_AMP = 74, B_AT_HP = 75, B_AT_LFO = 76,
    B_PM_FENV = 77, B_PM_OSC2 = 78, B_PM_FREQ = 79, B_PM_SHAPE = 80, B_PM_PW = 81, B_PM_LP = 82, B_PM_HP = 83,
    B_UNISON = 84,
    B_NAME = 107, NAME_CHARS = 20,
    P6_BYTES = 1024,
};
/* FX A's types (0..5) and FX B's (0..9) */
enum { FX_OFF, FX_BBD, FX_DDL, FX_CHORUS, FX_PHASER1, FX_PHASER2, FX_HALL, FX_ROOM, FX_PLATE, FX_SPRING };

int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
int lround_(double v) { return (int)std::lround(v); }

/* a Prophet-6 knob centred at 127 (0..254) to ours centred at 64 (0..127): -1..1 */
double centred(int v) { return std::max(-1.0, std::min(1.0, (v - 127) / 127.0)); }
int bipolar(double s) { return clamp(64 + lround_(s * 63.0), 0, 127); }
/* our ENV AMT, POLY MOD and LFO AMOUNT are squared (fine near nought): the square root keeps the Prophet-6's travel */
int bipolarSq(double s) { return bipolar(s < 0 ? -std::sqrt(-s) : std::sqrt(s)); }
/* the filter envelopes: the Prophet-6's reach further (about 13 octaves at the end) than ours (9): kept to scale */
int envAmount(int v)
{
    double s = centred(v) * (13.0 / 9.0);
    return bipolarSq(std::max(-1.0, std::min(1.0, s)));
}

/* the low-pass: the Prophet-6's 0..164 about 30 Hz .. 15 kHz, to ours (20 Hz * 1000^(v / 255), engine.c) */
int lpCutoff(int v)
{
    double hz = 30.0 * std::pow(2.0, clamp(v, 0, 164) * (9.0 / 164.0));
    return clamp(lround_(std::log2(hz / 20.0) * (255.0 / std::log2(1000.0))), 0, 255);
}
/* the high-pass: gentle over most of its travel (about 40 Hz at 0, 240 Hz at 120, 1.1 kHz at 140, 20 kHz at the top);
 * ours 10 Hz .. 6 kHz over 0..127 (engine.c) */
int hpCutoff(int v)
{
    if (v <= 0)
        return 0;
    double t = clamp(v, 0, 164) / 164.0;
    double hz = 40.0 * std::pow(500.0, t * t * t * t);
    return clamp(lround_((std::log2(hz) - 3.3219) * (127.0 / 9.2288)), 0, 127);
}
/* the LFO: the Prophet-6's 0..254 about 0.03 .. 500 Hz; ours 0.05 Hz * 2^(v * 9.2288 / 127) */
int lfoRate(int v)
{
    double hz = 0.03 * std::pow(500.0 / 0.03, clamp(v, 0, 254) / 254.0);
    return clamp(lround_(std::log2(hz / 0.05) * (127.0 / 9.2288)), 0, 127);
}
/* pulse width: the Prophet-6's square at the centre (127), narrower either way; ours square at 0, 95 % at 127 */
int pulseWidth(int v) { return clamp(lround_(std::fabs(clamp(v, 0, 255) - 127.5) / 127.5 * 127.0), 0, 127); }

/* an LFO's destination from the Prophet-6's switches, the first (want 0) or the second (want 1) */
int lfoDest(const uint8_t *u, int want)
{
    int d[6], n = 0;
    if (u[B_LFO_O1] && u[B_LFO_O2])
        d[n++] = LD_FREQ;
    else if (u[B_LFO_O1])
        d[n++] = LD_FREQ1;
    else if (u[B_LFO_O2])
        d[n++] = LD_FREQ2;
    if (u[B_LFO_PW])
        d[n++] = LD_PW;
    if (u[B_LFO_LP])
        d[n++] = LD_LPF;
    if (u[B_LFO_HP])
        d[n++] = LD_HPF;
    if (u[B_LFO_AMP])
        d[n++] = LD_AMP;
    return want < n ? d[want] : -1;
}

void fxSlot(int type, int mix, int p1, int p2, int sync, uint8_t *p)
{
    switch (type) {
    case FX_BBD:
    case FX_DDL:
        p[P_DL_MIX] = (uint8_t)clamp(mix, 0, 127);
        p[P_DL_TIME_L] = (uint8_t)clamp(p1 / 2, 0, 127);
        p[P_DL_TIME_R] = (uint8_t)clamp(p1 / 2 + 6, 0, 127);
        p[P_DL_FB] = (uint8_t)clamp(p2, 0, 127);
        p[P_DL_SYNC] = (uint8_t)(sync ? DLS_ON : DLS_OFF);
        p[P_DL_TONE] = (uint8_t)(type == FX_BBD ? 40 : 64);   /* the bucket brigade darker */
        p[P_DL_DEPTH] = (uint8_t)(type == FX_BBD ? 20 : 0);
        break;
    case FX_CHORUS:
    case FX_PHASER1:
    case FX_PHASER2:                       /* (ours has no phaser: a chorus in its place) */
        p[P_CH_MIX] = (uint8_t)clamp(mix, 0, 127);
        p[P_CH_RATE] = (uint8_t)clamp(p1 / 2, 0, 127);
        p[P_CH_DEPTH] = (uint8_t)clamp(p2, 0, 127);
        break;
    case FX_HALL:
    case FX_ROOM:
    case FX_PLATE:
    case FX_SPRING:
        p[P_RV_TYPE] = (uint8_t)(type == FX_HALL ? RV_HALL : type == FX_ROOM ? RV_ROOM : type == FX_PLATE ? RV_PLATE : RV_SPRING);
        p[P_RV_MIX] = (uint8_t)clamp(mix, 0, 127);
        p[P_RV_DECAY] = (uint8_t)clamp(p1 / 2, 0, 127);
        p[P_RV_TONE] = (uint8_t)clamp(p2, 0, 127);
        break;
    default:
        break;
    }
}

std::vector<uint8_t> unpack(const uint8_t *b, size_t n)
{
    std::vector<uint8_t> out;
    out.reserve(P6_BYTES);
    for (size_t k = 0; k < n; k += 8) {
        uint8_t ms = b[k];
        for (size_t j = 1; j < 8 && k + j < n; j++)
            out.push_back((uint8_t)((b[k + j] & 0x7f) | (((ms >> (j - 1)) & 1) << 7)));
    }
    return out;
}
} // namespace

void p6Convert(const uint8_t *src, size_t n, P6Program &o)
{
    uint8_t u[P6_BYTES] = {};
    std::memcpy(u, src, std::min(n, (size_t)P6_BYTES));
    uint8_t *p = o.p;
    std::memcpy(p, FACTORY[NPRESETS - 1].p, NPARAM);    /* the INIT program, for what the Prophet-6 has not */

    /* the name */
    std::string nm;
    for (int i = 0; i < NAME_CHARS; i++) {
        char c = (char)u[B_NAME + i];
        nm += (c >= 32 && c < 127) ? c : ' ';
    }
    while (!nm.empty() && nm.back() == ' ')
        nm.pop_back();
    while (!nm.empty() && nm.front() == ' ')
        nm.erase(nm.begin());
    o.name = nm.empty() ? "P6 PROGRAM" : nm;

    /* the oscillators (FREQ: C0 .. C5, 24 the note both) */
    p[P_O1_FREQ] = (uint8_t)clamp(u[B_O1_FREQ], 0, 48);
    p[P_O2_FREQ] = (uint8_t)clamp(u[B_O2_FREQ], 0, 48);
    p[P_O2_FINE] = (uint8_t)bipolar(centred(u[B_O2_FINE]));
    p[P_O1_SHAPE] = (uint8_t)clamp(u[B_O1_SHAPE] / 2, 0, 127);   /* both: triangle, saw at the centre, pulse */
    p[P_O2_SHAPE] = (uint8_t)clamp(u[B_O2_SHAPE] / 2, 0, 127);
    p[P_O1_PW] = (uint8_t)pulseWidth(u[B_O1_PW]);
    p[P_O2_PW] = (uint8_t)pulseWidth(u[B_O2_PW]);
    p[P_O2_MODE] = (uint8_t)(u[B_SYNC] ? O2M_SYNC : u[B_O2_LOFREQ] ? (u[B_O2_KBD] ? O2M_LOW : O2M_LOWFIX) : O2M_NORMAL);

    /* the mixer, at the program's VOLUME (its VCA's ENV AMOUNT not: ours has no level of its own beneath the envelope,
     * and the Prophet-6's programs with it low play through the LFO on the amp) */
    double lv = clamp(u[B_VOLUME], 0, 127) / 127.0 * 0.85;   /* (0.85: to the loudness of our own programs) */
    auto mix = [&](int v) { return (uint8_t)clamp(lround_(clamp(v, 0, 127) * lv), 0, 127); };
    p[P_MIX_O1] = mix(u[B_O1_LEVEL]);
    p[P_MIX_O2] = mix(u[B_O2_LEVEL]);
    p[P_O1_SUB] = mix(u[B_SUB]);
    p[P_NOISE] = mix(u[B_NOISE]);

    /* the filters */
    p[P_CUTOFF] = (uint8_t)lpCutoff(u[B_LP_FREQ]);
    p[P_RESO] = (uint8_t)clamp(u[B_LP_RES] / 2, 0, 127);
    p[P_ENV_AMT] = (uint8_t)envAmount(u[B_LP_ENV]);
    /* the filter played on its own (nothing in the mixer, the resonance singing): a breath of noise to start it */
    if (!p[P_MIX_O1] && !p[P_MIX_O2] && !p[P_O1_SUB] && !p[P_NOISE] && p[P_RESO] >= 110)
        p[P_NOISE] = 3;
    p[P_KEYTRK] = (uint8_t)clamp(u[B_LP_KBD], 0, KT_FULL);
    p[P_FVEL] = 0;                                   /* the Prophet-6's VELOCITY: on the filter envelope's amount */
    p[P_HPF] = (uint8_t)hpCutoff(u[B_HP_FREQ]);
    p[P_HP_RESO] = (uint8_t)clamp(u[B_HP_RES] / 2, 0, 127);
    p[P_HP_ENV] = (uint8_t)envAmount(u[B_HP_ENV]);
    o.hpKt = clamp(u[B_HP_KBD], 0, KT_FULL);
    o.hpVel = 0;
    o.fenvVel = (u[B_LP_VEL] || u[B_HP_VEL]) ? 100 : 0;

    /* the envelopes (0..127 both) */
    p[P_FA] = (uint8_t)clamp(u[B_FA], 0, 127);
    p[P_FD] = (uint8_t)clamp(u[B_FD], 0, 127);
    p[P_FS] = (uint8_t)clamp(u[B_FS], 0, 127);
    p[P_FR] = (uint8_t)clamp(u[B_FR], 0, 127);
    p[P_AA] = (uint8_t)clamp(u[B_AA], 0, 127);
    p[P_AD] = (uint8_t)clamp(u[B_AD], 0, 127);
    p[P_AS] = (uint8_t)clamp(u[B_AS], 0, 127);
    p[P_AR] = (uint8_t)clamp(u[B_AR], 0, 127);
    p[P_AVEL] = (uint8_t)(u[B_AMP_VEL] ? 90 : 0);

    /* the LFO: its first destination on LFO 1, a second on LFO 2 (the same rate and shape) */
    int rate = lfoRate(u[B_LFO_FREQ]);
    int amt = clamp(lround_(std::sqrt(clamp(u[B_LFO_AMT], 0, 255) / 255.0) * 127.0), 0, 127);
    int shape = clamp(u[B_LFO_SHAPE], 0, NLS - 1);
    int sync = u[B_LFO_SYNC] ? clamp(1 + u[B_LFO_FREQ] * (NLSY - 2) / 255, 1, NLSY - 1) : LSY_OFF;
    int d1 = lfoDest(u, 0), d2 = lfoDest(u, 1);
    p[P_LFO_RATE] = p[P_L2_RATE] = (uint8_t)rate;
    p[P_LFO_SHAPE] = p[P_L2_SHAPE] = (uint8_t)shape;
    p[P_L1_SYNC] = p[P_L2_SYNC] = (uint8_t)sync;
    p[P_L1_RETRIG] = p[P_L2_RETRIG] = 0;
    p[P_LFO_AMT] = (uint8_t)(d1 >= 0 ? amt : 0);
    p[P_LFO_DEST] = (uint8_t)(d1 >= 0 ? d1 : LD_FREQ);
    p[P_L2_AMT] = (uint8_t)(d2 >= 0 ? amt : 0);
    p[P_L2_DEST] = (uint8_t)(d2 >= 0 ? d2 : LD_FREQ);

    /* poly mod */
    p[P_PM_FENV] = (uint8_t)bipolarSq(centred(u[B_PM_FENV]));
    p[P_PM_OSC2] = (uint8_t)bipolarSq(centred(u[B_PM_OSC2]));
    p[P_PM_DOSC] = (uint8_t)((u[B_PM_FREQ] ? PMO_FREQ : 0) | (u[B_PM_SHAPE] ? PMO_SHAPE : 0) | (u[B_PM_PW] ? PMO_PW : 0));
    p[P_PM_DFILT] = (uint8_t)(u[B_PM_LP] && u[B_PM_HP] ? PMF_BOTH : u[B_PM_LP] ? PMF_LPF : u[B_PM_HP] ? PMF_HPF : PMF_OFF);

    /* aftertouch: one destination, the nearest */
    p[P_AT_DEST] = (uint8_t)(u[B_AT_LP] ? AT_FILTER : u[B_AT_LFO] ? AT_LFO : u[B_AT_AMP] ? AT_SWELL
                             : (u[B_AT_O1] || u[B_AT_O2]) ? AT_PITCH : u[B_AT_HP] ? AT_FILTER : AT_OFF);
    p[P_AT_AMT] = (uint8_t)bipolar(centred(u[B_AT_AMT]));
    if (u[B_AT_AMT] == 127)
        p[P_AT_DEST] = AT_OFF;

    /* the voice */
    p[P_GLIDE] = (uint8_t)(u[B_GLIDE_ON] ? clamp(u[B_GLIDE_RATE], 0, 127) : 0);
    p[P_UNISON] = (uint8_t)(u[B_UNISON] ? 1 : 0);
    p[P_BEND] = (uint8_t)clamp(u[B_BEND], 0, 12);
    p[P_VINTAGE] = (uint8_t)clamp(u[B_SLOP], 0, 127);
    p[P_SPREAD] = (uint8_t)clamp(u[B_SPREAD], 0, 127);
    p[P_DIST] = (uint8_t)clamp(u[B_DIST], 0, 127);

    /* the effects: off unless the Prophet-6's are on and say so */
    p[P_CH_MIX] = 0;
    p[P_RV_MIX] = 0;
    p[P_DL_MIX] = 0;
    if (u[B_FX_ON]) {
        fxSlot(u[B_FXA_TYPE], u[B_FXA_MIX], u[B_FXA_P1], u[B_FXA_P2], u[B_FXA_SYNC], p);
        fxSlot(u[B_FXB_TYPE], u[B_FXB_MIX], u[B_FXB_P1], u[B_FXB_P2], u[B_FXB_SYNC], p);
    }

    for (int i = 0; i < NPARAM; i++)
        if (p[i] > PARAMS[i].max)
            p[i] = PARAMS[i].max;
}

std::vector<P6Program> p6ReadSysex(const uint8_t *d, size_t len)
{
    std::vector<P6Program> out;
    for (size_t i = 0; i + 4 < len; i++) {
        if (d[i] != 0xF0 || d[i + 1] != 0x01 || d[i + 2] != 0x2D)
            continue;
        size_t end = i + 1;
        while (end < len && d[end] != 0xF7)
            end++;
        if (end >= len)
            break;
        size_t body = 0;
        int number = -1;
        if (d[i + 3] == 0x02 && i + 6 <= end) {          /* a program: bank, program */
            number = d[i + 4] * 100 + d[i + 5];
            body = i + 6;
        } else if (d[i + 3] == 0x03) {                   /* the edit buffer */
            body = i + 4;
        }
        if (body) {
            auto u = unpack(d + body, end - body);
            if (u.size() >= 127) {
                P6Program pr;
                pr.number = number;
                p6Convert(u.data(), u.size(), pr);
                out.push_back(pr);
            }
        }
        i = end;
    }
    return out;
}
