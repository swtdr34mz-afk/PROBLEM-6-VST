/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The sixteen factory presets: classic analogue poly sounds in the Prophet manner. The names are our own.
 *
 * Reading a row: envelope times 0..127 run 1 ms .. 20 s (40: 22 ms, 60: 0.1 s, 80: 0.5 s, 100: 2.4 s); CUTOFF 0..255, 20 Hz ..
 * 20 kHz (100: 300 Hz, 140: 900 Hz, 180: 2.7 kHz); SHAPE 0 triangle, 64 saw, 127 pulse; FREQ 24 is the note (12: an octave
 * down, 31: a fifth up); bipolar knobs are 0 at 64. Poly mod's oscillator destinations: 1 FREQ, 2 SHAPE, 4 PW (added). */
#include "engine.h"

/*                 O1 FRQ SHP PW SUB   O2 FRQ FIN SHP PW   MIX O1 O2 NOI MODE   CUT RES ENV HPF   HPR KEY FVEL HPE
 *                 FA FD FS FR   AA AD AS AR   LFO RATE AMT SHP DST   PM FENV OSC2 DOSC DFLT   CH MIX RATE DPTH DIST
 *                 RV TYP MIX DEC TONE   SP MOD SHIM SWL PRE   GLIDE AVEL UNI BEND   VINTAGE SPREAD
 *                 (then, where a preset uses them: LFO 2 RATE AMT SHP DST   SYNC 1 SYNC 2 RETRIG 1 RETRIG 2;
 *                 and named: the AFTERTOUCH) */
const patch_t FACTORY[NPRESETS] = {
    /* the classic: two saws a little apart, slow and warm, wide */
    {"POLY PAD", {24, 64, 0, 20, 24, 72, 64, 0, 110, 100, 0, O2M_NORMAL, 120, 22, 84, 0, 0, KT_HALF, 30, 64,
                  76, 86, 72, 86, 74, 90, 112, 88, 35, 0, LS_TRI, LD_FREQ, 64, 64, 0, PMF_OFF, 50, 30, 60, 0,
                  RV_HALL, 40, 70, 64, 30, 0, 0, 20, 0, 40, 0, 2, 50, 80,
        [P_AT_DEST] = AT_FILTER, [P_AT_AMT] = 96, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 85, [P_DL_TIME_R] = 95, [P_DL_FB] = 50, [P_DL_MIX] = 0, [P_DL_DEPTH] = 30, [P_DL_RATE] = 50, [P_DL_TONE] = 64, [P_DL_SYNC] = 0}},
    /* a brass section: the filter swells open and settles */
    {"BRASS 6", {24, 64, 0, 0, 24, 69, 64, 0, 115, 105, 0, O2M_NORMAL, 88, 16, 106, 0, 0, KT_HALF, 60, 64,
                 52, 72, 56, 62, 30, 80, 116, 60, 60, 0, LS_TRI, LD_FREQ, 64, 64, 0, PMF_OFF, 22, 30, 50, 6,
                 RV_ROOM, 25, 60, 64, 30, 0, 0, 20, 0, 50, 0, 2, 42, 55,
        [P_AT_DEST] = AT_POWER, [P_AT_AMT] = 100, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 85, [P_DL_TIME_R] = 95, [P_DL_FB] = 50, [P_DL_MIX] = 0, [P_DL_DEPTH] = 30, [P_DL_RATE] = 50, [P_DL_TONE] = 64, [P_DL_SYNC] = 0}},
    /* hard sync, swept by the filter envelope through poly mod: the lead */
    {"SYNC LEAD", {24, 64, 0, 0, 24, 64, 64, 0, 120, 0, 0, O2M_SYNC, 172, 28, 80, 0, 0, KT_FULL, 40, 64,
                   0, 76, 40, 52, 0, 80, 120, 52, 72, 0, LS_TRI, LD_FREQ, 104, 64, PMO_FREQ, PMF_OFF, 0, 30, 50, 18,
                   RV_PLATE, 25, 60, 70, 30, 0, 0, 20, 30, 40, 0, 2, 30, 20,
        [P_AT_DEST] = AT_VIBRATO, [P_AT_AMT] = 100, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 77, [P_DL_TIME_R] = 90, [P_DL_FB] = 55, [P_DL_MIX] = 35, [P_DL_DEPTH] = 25, [P_DL_RATE] = 45, [P_DL_TONE] = 64, [P_DL_SYNC] = 1}},
    /* a saw and a square an octave down with the sub: round, a little driven */
    {"FAT BASS", {12, 64, 0, 90, 12, 66, 127, 0, 120, 95, 0, O2M_NORMAL, 76, 42, 100, 0, 0, KT_HALF, 70, 64,
                  0, 62, 22, 42, 0, 70, 110, 36, 60, 0, LS_TRI, LD_FREQ, 64, 64, 0, PMF_OFF, 0, 30, 50, 26,
                  RV_ROOM, 0, 40, 64, 30, 0, 0, 20, 0, 40, 0, 2, 25, 10,
        [P_AT_DEST] = AT_FILTER, [P_AT_AMT] = 100, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 85, [P_DL_TIME_R] = 95, [P_DL_FB] = 50, [P_DL_MIX] = 0, [P_DL_DEPTH] = 30, [P_DL_RATE] = 50, [P_DL_TONE] = 64, [P_DL_SYNC] = 0}},
    /* oscillator 2 a twelfth and a bit up, frequency-modulating oscillator 1 at audio rate: bells */
    {"PM BELLS", {24, 0, 0, 0, 43, 64, 0, 0, 120, 0, 0, O2M_NORMAL, 192, 10, 74, 0, 0, KT_FULL, 50, 64,
                  0, 86, 0, 92, 0, 96, 0, 96, 50, 0, LS_TRI, LD_FREQ, 64, 104, PMO_FREQ, PMF_OFF, 30, 25, 50, 0,
                  RV_HALL, 45, 82, 74, 30, 0, 0, 20, 0, 60, 0, 2, 20, 72,
        [P_AT_DEST] = AT_POLYMOD, [P_AT_AMT] = 100, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 85, [P_DL_TIME_R] = 95, [P_DL_FB] = 50, [P_DL_MIX] = 0, [P_DL_DEPTH] = 30, [P_DL_RATE] = 50, [P_DL_TONE] = 64, [P_DL_SYNC] = 0}},
    /* triangles an octave apart, the chorus moving: soft keys */
    {"SOFT KEYS", {24, 10, 0, 0, 36, 64, 0, 0, 120, 55, 0, O2M_NORMAL, 140, 6, 72, 0, 0, KT_HALF, 60, 64,
                   0, 80, 30, 70, 0, 90, 40, 70, 40, 0, LS_TRI, LD_FREQ, 64, 64, 0, PMF_OFF, 70, 35, 70, 0,
                   RV_ROOM, 30, 60, 60, 30, 0, 0, 20, 0, 70, 0, 2, 45, 60,
        [P_AT_DEST] = AT_SWELL, [P_AT_AMT] = 90, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 85, [P_DL_TIME_R] = 95, [P_DL_FB] = 50, [P_DL_MIX] = 0, [P_DL_DEPTH] = 30, [P_DL_RATE] = 50, [P_DL_TONE] = 64, [P_DL_SYNC] = 0}},
    /* a pulse breathing (the LFO on its width) and a saw, the chorus deep: strings */
    {"STRINGS", {24, 127, 60, 0, 24, 72, 64, 0, 105, 95, 0, O2M_NORMAL, 144, 10, 70, 30, 0, KT_HALF, 20, 64,
                 80, 80, 90, 90, 86, 90, 116, 92, 76, 22, LS_TRI, LD_PW, 64, 64, 0, PMF_OFF, 90, 40, 80, 0,
                 RV_HALL, 35, 72, 64, 30, 0, 0, 20, 0, 30, 0, 2, 55, 100, 70, 12, LS_TRI, LD_FREQ, LSY_OFF, LSY_OFF, 0, 0,
        [P_AT_DEST] = AT_SWELL, [P_AT_AMT] = 100, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 85, [P_DL_TIME_R] = 95, [P_DL_FB] = 50, [P_DL_MIX] = 0, [P_DL_DEPTH] = 30, [P_DL_RATE] = 50, [P_DL_TONE] = 64, [P_DL_SYNC] = 0}},
    /* a resonant sweep, slow up and slower down, the LFO turning the filter */
    {"SWEEP PAD", {24, 64, 0, 10, 24, 74, 64, 0, 110, 105, 0, O2M_NORMAL, 70, 72, 92, 0, 0, KT_HALF, 20, 64,
                   108, 110, 40, 100, 96, 100, 120, 100, 24, 30, LS_TRI, LD_LPF, 64, 64, 0, PMF_OFF, 40, 30, 60, 0,
                   RV_HALL, 50, 92, 60, 30, 0, 0, 20, 0, 30, 0, 2, 62, 110, 0, 0, LS_TRI, LD_FREQ, LSY_2BAR, LSY_OFF, 0, 0,
        [P_AT_DEST] = AT_FILTER, [P_AT_AMT] = 90, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 85, [P_DL_TIME_R] = 95, [P_DL_FB] = 50, [P_DL_MIX] = 0, [P_DL_DEPTH] = 30, [P_DL_RATE] = 50, [P_DL_TONE] = 64, [P_DL_SYNC] = 0}},
    /* a short pluck, resonant, harder hits brighter */
    {"PLUCK", {24, 64, 0, 0, 24, 70, 127, 20, 115, 80, 0, O2M_NORMAL, 80, 46, 106, 0, 0, KT_FULL, 85, 64,
               0, 60, 0, 60, 0, 76, 0, 70, 40, 0, LS_TRI, LD_FREQ, 64, 64, 0, PMF_OFF, 20, 30, 50, 0,
               RV_PLATE, 35, 66, 64, 30, 0, 0, 20, 0, 70, 0, 2, 35, 70,
        [P_AT_DEST] = AT_FILTER, [P_AT_AMT] = 90, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 77, [P_DL_TIME_R] = 77, [P_DL_FB] = 60, [P_DL_MIX] = 28, [P_DL_DEPTH] = 30, [P_DL_RATE] = 40, [P_DL_TONE] = 60, [P_DL_SYNC] = 1}},
    /* oscillator 2 slowed to an LFO (LOW FREQ, with the keys) moving oscillator 1's shape and the filter: a growl */
    {"PM GROWL", {24, 64, 40, 30, 36, 64, 0, 0, 120, 0, 0, O2M_LOW, 110, 56, 86, 0, 0, KT_HALF, 40, 64,
                  0, 80, 50, 50, 0, 80, 116, 46, 60, 0, LS_TRI, LD_FREQ, 64, 100, PMO_SHAPE, PMF_LPF, 0, 30, 50, 30,
                  RV_ROOM, 20, 50, 64, 30, 0, 0, 20, 0, 40, 0, 2, 30, 30, 0, 40, LS_SH, LD_LPF, LSY_OFF, LSY_16, 0, 1,
        [P_AT_DEST] = AT_POLYMOD, [P_AT_AMT] = 110, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 85, [P_DL_TIME_R] = 95, [P_DL_FB] = 50, [P_DL_MIX] = 0, [P_DL_DEPTH] = 30, [P_DL_RATE] = 50, [P_DL_TONE] = 64, [P_DL_SYNC] = 0}},
    /* the SPACE reverb: swelling, shimmering, endless */
    {"SPACE DRIFT", {24, 40, 0, 0, 36, 66, 0, 0, 92, 62, 0, O2M_NORMAL, 116, 24, 76, 10, 0, KT_HALF, 20, 64,
                     100, 104, 62, 104, 100, 104, 116, 108, 20, 10, LS_TRI, LD_FREQ, 64, 64, 0, PMF_OFF, 30, 25, 60, 0,
                     RV_SPACE, 70, 112, 70, 70, 50, 60, 30, 0, 30, 0, 2, 50, 120, 20, 30, LS_TRI, LD_LPF, LSY_OFF, LSY_OFF, 0, 0,
        [P_AT_DEST] = AT_SWELL, [P_AT_AMT] = 90, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 100, [P_DL_TIME_R] = 112, [P_DL_FB] = 70, [P_DL_MIX] = 20, [P_DL_DEPTH] = 60, [P_DL_RATE] = 30, [P_DL_TONE] = 50, [P_DL_SYNC] = 0}},
    /* every voice on one note, spread wide, a saw an octave up: the big lead */
    {"UNISON LEAD", {24, 64, 0, 30, 36, 68, 64, 0, 120, 90, 0, O2M_NORMAL, 150, 34, 86, 0, 0, KT_FULL, 40, 64,
                     0, 70, 60, 50, 0, 80, 120, 48, 72, 0, LS_TRI, LD_FREQ, 64, 64, 0, PMF_OFF, 0, 30, 50, 30,
                     RV_PLATE, 25, 64, 64, 30, 0, 0, 20, 22, 40, 1, 2, 40, 60,
        [P_AT_DEST] = AT_VIBRATO, [P_AT_AMT] = 100, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 85, [P_DL_TIME_R] = 95, [P_DL_FB] = 45, [P_DL_MIX] = 25, [P_DL_DEPTH] = 35, [P_DL_RATE] = 50, [P_DL_TONE] = 64, [P_DL_SYNC] = 0}},
    /* narrow pulses, the lows cut: a clav */
    {"CLAV", {24, 127, 100, 0, 36, 64, 127, 80, 120, 60, 0, O2M_NORMAL, 160, 20, 92, 50, 20, KT_FULL, 80, 64,
              0, 50, 0, 40, 0, 60, 0, 36, 60, 0, LS_TRI, LD_FREQ, 64, 64, 0, PMF_OFF, 0, 30, 50, 10,
              RV_ROOM, 20, 50, 70, 30, 0, 0, 20, 0, 80, 0, 2, 20, 40,
        [P_AT_DEST] = AT_FILTER, [P_AT_AMT] = 90, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 85, [P_DL_TIME_R] = 95, [P_DL_FB] = 50, [P_DL_MIX] = 0, [P_DL_DEPTH] = 30, [P_DL_RATE] = 50, [P_DL_TONE] = 64, [P_DL_SYNC] = 0}},
    /* synced to a twelfth-and-a-bit, the filter envelope sweeping oscillator 1's pitch and shape, a shimmering space */
    {"GLASS PAD", {24, 0, 0, 0, 43, 64, 0, 0, 115, 0, 0, O2M_SYNC, 200, 10, 64, 20, 0, KT_HALF, 20, 64,
                   86, 100, 40, 100, 82, 100, 110, 100, 30, 0, LS_TRI, LD_FREQ, 114, 64, PMO_FREQ | PMO_SHAPE, PMF_OFF,
                   20, 25, 50, 0, RV_SPACE, 55, 96, 74, 50, 80, 30, 20, 0, 30, 0, 2, 30, 100,
        [P_AT_DEST] = AT_FILTER, [P_AT_AMT] = 90, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 90, [P_DL_TIME_R] = 104, [P_DL_FB] = 55, [P_DL_MIX] = 18, [P_DL_DEPTH] = 40, [P_DL_RATE] = 35, [P_DL_TONE] = 60, [P_DL_SYNC] = 0}},
    /* an old poly's keys: every voice its own (VINTAGE high) */
    {"VINTAGE KEYS", {24, 64, 0, 0, 24, 66, 110, 30, 115, 90, 0, O2M_NORMAL, 132, 26, 88, 0, 0, KT_HALF, 50, 64,
                      0, 78, 30, 70, 0, 85, 80, 72, 50, 0, LS_TRI, LD_FREQ, 64, 64, 0, PMF_OFF, 25, 30, 50, 0,
                      RV_ROOM, 30, 60, 64, 30, 0, 0, 20, 0, 60, 0, 2, 110, 70,
        [P_AT_DEST] = AT_FILTER, [P_AT_AMT] = 90, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 85, [P_DL_TIME_R] = 95, [P_DL_FB] = 50, [P_DL_MIX] = 0, [P_DL_DEPTH] = 30, [P_DL_RATE] = 50, [P_DL_TONE] = 64, [P_DL_SYNC] = 0}},
    /* a saw, the filter open: to start from */
    {"INIT", {24, 64, 0, 0, 24, 64, 64, 0, 120, 0, 0, O2M_NORMAL, 255, 0, 64, 0, 0, KT_OFF, 0, 64,
              0, 60, 127, 40, 0, 60, 127, 40, 60, 0, LS_TRI, LD_FREQ, 64, 64, 0, PMF_OFF, 0, 30, 50, 0,
              RV_HALL, 0, 60, 64, 30, 0, 0, 20, 0, 40, 0, 2, 0, 0,
        [P_AT_DEST] = AT_FILTER, [P_AT_AMT] = 96, [P_AT_CURVE] = 64, [P_AT_SMOOTH] = 30,
        [P_DL_TIME_L] = 85, [P_DL_TIME_R] = 95, [P_DL_FB] = 50, [P_DL_MIX] = 0, [P_DL_DEPTH] = 30, [P_DL_RATE] = 50, [P_DL_TONE] = 64, [P_DL_SYNC] = 0}},
};
