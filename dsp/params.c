/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The parameters: their names, kinds and choices, and how their values read on the screen (engine.h). */
#include "engine.h"

static const char *const C_O2M[NO2M] = {"NORMAL", "SYNC", "LOW FRQ", "LF FIXED"};
static const char *const C_KT[NKT] = {"OFF", "HALF", "FULL"};
static const char *const C_LS[NLS] = {"TRI", "SAW", "RAMP", "SQUARE", "S+H"};
static const char *const C_LD[NLD] = {"FREQ 1+2", "FREQ 1", "FREQ 2", "PW 1+2", "LPF", "HPF", "AMP"};
static const char *const C_PMO[8] = {"OFF", "FREQ", "SHAPE", "FRQ+SHP", "PW", "FRQ+PW", "SHP+PW", "ALL"};
static const char *const C_PMF[NPMF] = {"OFF", "LPF", "HPF", "LPF+HPF"};
static const char *const C_RV[NRV] = {"ROOM", "HALL", "PLATE", "SPRING", "SPACE"};
static const char *const C_UNI[2] = {"OFF", "ON"};
static const char *const C_AT[NAT] = {"OFF", "FILTER", "SWELL", "POWER", "VIBRATO", "LFO 1", "PITCH", "POLY MOD"};
static const char *const C_LSY[NLSY] = {"OFF", "4 BARS", "2 BARS", "1 BAR", "1/2", "1/4", "1/4T", "1/8", "1/8T",
                                        "1/16", "1/16T", "1/32"};
const uint16_t LSY_TICKS[NLSY] = {0, 384, 192, 96, 48, 24, 16, 12, 8, 6, 4, 3};

const param_t PARAMS[NPARAM] = {
    [P_O1_FREQ] = {"FREQ", PK_SEMI, 48, 0},
    [P_O1_SHAPE] = {"SHAPE", PK_KNOB, 127, 0},
    [P_O1_PW] = {"PW", PK_PCT, 127, 0},
    [P_O1_SUB] = {"SUB OCT", PK_KNOB, 127, 0},
    [P_O2_FREQ] = {"FREQ", PK_SEMI, 48, 0},
    [P_O2_FINE] = {"FINE", PK_FINE, 127, 0},
    [P_O2_SHAPE] = {"SHAPE", PK_KNOB, 127, 0},
    [P_O2_PW] = {"PW", PK_PCT, 127, 0},
    [P_MIX_O1] = {"OSC 1", PK_KNOB, 127, 0},
    [P_MIX_O2] = {"OSC 2", PK_KNOB, 127, 0},
    [P_NOISE] = {"NOISE", PK_KNOB, 127, 0},
    [P_O2_MODE] = {"OSC 2", PK_CHOICE, NO2M - 1, C_O2M},
    [P_CUTOFF] = {"CUTOFF", PK_HZ, 255, 0},   /* fine: half a semitone a detent */
    [P_RESO] = {"RESONANCE", PK_KNOB, 127, 0},
    [P_ENV_AMT] = {"ENV AMT", PK_BIPOLAR, 127, 0},
    [P_HPF] = {"HP CUTOFF", PK_KNOB, 127, 0},
    [P_HP_RESO] = {"HP RESO", PK_KNOB, 127, 0},
    [P_KEYTRK] = {"KEYBOARD", PK_CHOICE, NKT - 1, C_KT},
    [P_FVEL] = {"VELOCITY", PK_KNOB, 127, 0},
    [P_HP_ENV] = {"HP ENV", PK_BIPOLAR, 127, 0},
    [P_FA] = {"ATTACK", PK_TIME, 127, 0},
    [P_FD] = {"DECAY", PK_TIME, 127, 0},
    [P_FS] = {"SUSTAIN", PK_KNOB, 127, 0},
    [P_FR] = {"RELEASE", PK_TIME, 127, 0},
    [P_AA] = {"ATTACK", PK_TIME, 127, 0},
    [P_AD] = {"DECAY", PK_TIME, 127, 0},
    [P_AS] = {"SUSTAIN", PK_KNOB, 127, 0},
    [P_AR] = {"RELEASE", PK_TIME, 127, 0},
    [P_LFO_RATE] = {"RATE", PK_KNOB, 127, 0},
    [P_LFO_AMT] = {"AMOUNT", PK_KNOB, 127, 0},
    [P_LFO_SHAPE] = {"SHAPE", PK_CHOICE, NLS - 1, C_LS},
    [P_LFO_DEST] = {"DEST", PK_CHOICE, NLD - 1, C_LD},
    [P_PM_FENV] = {"FILT ENV", PK_BIPOLAR, 127, 0},
    [P_PM_OSC2] = {"OSC 2", PK_BIPOLAR, 127, 0},
    [P_PM_DOSC] = {"OSC 1 TO", PK_CHOICE, 7, C_PMO},
    [P_PM_DFILT] = {"FILTER", PK_CHOICE, NPMF - 1, C_PMF},
    [P_CH_MIX] = {"CHORUS", PK_KNOB, 127, 0},
    [P_CH_RATE] = {"RATE", PK_KNOB, 127, 0},
    [P_CH_DEPTH] = {"DEPTH", PK_KNOB, 127, 0},
    [P_DIST] = {"DISTORT", PK_KNOB, 127, 0},
    [P_RV_TYPE] = {"TYPE", PK_CHOICE, NRV - 1, C_RV},
    [P_RV_MIX] = {"MIX", PK_KNOB, 127, 0},
    [P_RV_DECAY] = {"DECAY", PK_KNOB, 127, 0},
    [P_RV_TONE] = {"TONE", PK_BIPOLAR, 127, 0},
    [P_SP_MOD] = {"MODULATE", PK_KNOB, 127, 0},
    [P_SP_SHIM] = {"SHIMMER", PK_KNOB, 127, 0},
    [P_SP_SWELL] = {"SWELL", PK_KNOB, 127, 0},
    [P_SP_PRE] = {"PREDELAY", PK_KNOB, 127, 0},
    [P_GLIDE] = {"GLIDE", PK_KNOB, 127, 0},
    [P_AVEL] = {"VELOCITY", PK_KNOB, 127, 0},
    [P_UNISON] = {"UNISON", PK_CHOICE, 1, C_UNI},
    [P_BEND] = {"BEND", PK_KNOB, 12, 0},
    [P_VINTAGE] = {"VINTAGE", PK_KNOB, 127, 0},
    [P_SPREAD] = {"SPREAD", PK_KNOB, 127, 0},
    [P_L2_RATE] = {"RATE", PK_KNOB, 127, 0},
    [P_L2_AMT] = {"AMOUNT", PK_KNOB, 127, 0},
    [P_L2_SHAPE] = {"SHAPE", PK_CHOICE, NLS - 1, C_LS},
    [P_L2_DEST] = {"DEST", PK_CHOICE, NLD - 1, C_LD},
    [P_L1_SYNC] = {"SYNC 1", PK_CHOICE, NLSY - 1, C_LSY},
    [P_L2_SYNC] = {"SYNC 2", PK_CHOICE, NLSY - 1, C_LSY},
    [P_L1_RETRIG] = {"RETRIG 1", PK_CHOICE, 1, C_UNI},
    [P_L2_RETRIG] = {"RETRIG 2", PK_CHOICE, 1, C_UNI},
    [P_AT_DEST] = {"DEST", PK_CHOICE, NAT - 1, C_AT},
    [P_AT_AMT] = {"AMOUNT", PK_BIPOLAR, 127, 0},
    [P_AT_CURVE] = {"CURVE", PK_BIPOLAR, 127, 0},
    [P_AT_SMOOTH] = {"SMOOTH", PK_KNOB, 127, 0},
    [P_DL_TIME_L] = {"TIME L", PK_DTIME, 127, 0},
    [P_DL_TIME_R] = {"TIME R", PK_DTIME, 127, 0},
    [P_DL_FB] = {"REPEATS", PK_KNOB, 127, 0},
    [P_DL_MIX] = {"MIX", PK_KNOB, 127, 0},
    [P_DL_DEPTH] = {"DEPTH", PK_KNOB, 127, 0},
    [P_DL_RATE] = {"RATE", PK_KNOB, 127, 0},
    [P_DL_TONE] = {"TONE", PK_BIPOLAR, 127, 0},
    [P_DL_SYNC] = {"SYNC", PK_CHOICE, NDLSYNC - 1, C_UNI},
};

/* the delay's times: 20 ms .. 1.2 s (fx.c dl_seconds); synced, the notes of the clock */
const uint8_t DL_SYNC_TICKS[NDLS] = {3, 4, 6, 8, 9, 12, 16, 18, 24, 32, 36, 48};
const char *const DL_SYNC_NAME[NDLS] = {"1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4",
                                        "1/2T", "1/4.", "1/2"};

static char *put_int(char *o, int v)
{
    char t[8];
    int n = 0;
    if (v < 0) {
        *o++ = '-';
        v = -v;
    }
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 7);
    while (n)
        *o++ = t[--n];
    *o = 0;
    return o;
}

static void put_str(char *o, const char *s)
{
    while (*s)
        *o++ = *s++;
    *o = 0;
}

/* the envelope times as the engine reads them (engine.c env_seconds): 1 ms .. 20 s */
static void put_time(char *o, int v)
{
    /* 0.001 * 20000^(v/127), in ms below a second */
    float t = 1.0f, k = 1.0811f;           /* 20000^(1/127) */
    int i, ms;
    for (i = 0; i < v; i++)
        t *= k;
    ms = (int)(t + 0.5f);
    if (ms < 1000) {
        o = put_int(o, ms);
        put_str(o, "ms");
    } else {
        int ds = (ms + 50) / 100;
        o = put_int(o, ds / 10);
        if (ds < 100) {
            *o++ = '.';
            o = put_int(o, ds % 10);
        }
        put_str(o, "s");
    }
}

void param_text(int p, int v, char *o)
{
    const param_t *d = &PARAMS[p];
    if (v > d->max)
        v = d->max;
    switch (d->kind) {
    case PK_CHOICE:
        put_str(o, d->choice[v]);
        return;
    case PK_BIPOLAR:
        if (v > 64)
            *o++ = '+';
        put_int(o, v - 64);
        return;
    case PK_SEMI:                        /* -24 .. +24 semitones */
        if (v > 24)
            *o++ = '+';
        put_int(o, v - 24);
        return;
    case PK_FINE:                        /* -50 .. +50 cents */
        if (v > 64)
            *o++ = '+';
        o = put_int(o, ((v - 64) * 50) / 63);
        put_str(o, "c");
        return;
    case PK_TIME:
        put_time(o, v);
        return;
    case PK_DTIME: {                     /* 20 ms * 60^(v/127) */
        float t = 20.0f;
        int i;
        for (i = 0; i < v; i++)
            t *= 1.032770f;
        o = put_int(o, (int)(t + 0.5f));
        put_str(o, "ms");
        return;
    }
    case PK_PCT:                         /* pulse width 50 .. 95 % */
        o = put_int(o, 50 + (v * 45) / 127);
        put_str(o, "%");
        return;
    case PK_HZ: {                        /* 20 Hz .. 20 kHz, 0..255 (engine.c cutoff_oct) */
        float hz = 20.0f;
        int i;
        for (i = 0; i < v; i++)
            hz *= 1.027461f;             /* 1000^(1/255) */
        if (hz < 1000.0f) {
            put_int(o, (int)hz);
        } else {
            int dk = (int)(hz / 100.0f + 0.5f);
            o = put_int(o, dk / 10);
            if (dk < 100) {
                *o++ = '.';
                o = put_int(o, dk % 10);
            }
            put_str(o, "k");
        }
        return;
    }
    default:
        put_int(o, v);
    }
}
