#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 swtdr34mz (PROPHET 67)
"""The VST's own fifty presets (after the sixteen factory programs it shares with the FM-1 and the MPC), written as
src/vst_presets.c. Each preset is the INIT-like defaults below with what makes it its own; the names and the sounds
are our own, in the manner of the classic analogue poly: brass, strings, pads, basses, leads, keys, plucks, effects.

    python3 tools/make_vst_presets.py            (then rebuild; tests/engine_tests.cpp plays every one)

Reading the values: envelope times 0..127 run 1 ms .. 20 s (40: 22 ms, 60: 0.1 s, 70: 0.22 s, 80: 0.5 s, 90: 1.1 s,
100: 2.4 s, 110: 5 s); CUTOFF 0..255, 20 Hz .. 20 kHz (100: 300 Hz, 120: 520 Hz, 140: 900 Hz, 160: 1.6 kHz, 180:
2.7 kHz); SHAPE 0 triangle, 64 saw, 127 pulse (PW 0: square, 127: 95 %); FREQ 24 the note (12 an octave down, 31 a
fifth up, 36 an octave up); bipolar knobs 0 at 64; LFO amounts are squared (to pitch, 127 is an octave: 12 a gentle
vibrato); LFO rate 50: 0.6 Hz, 60: 1 Hz, 70: 1.6 Hz, 80: 2.7 Hz, 90: 4.6 Hz.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "vst_presets.c")
# each preset's level: its mixer (oscillators, sub, noise) scaled to bring it to the others' loudness, measured through
# the engine (the loudest 400 ms of a phrase; tools/vst_presets_levels.json, kept with the presets)
LEVELS = os.path.join(HERE, "vst_presets_levels.json")
MIXER = ("MIX_O1", "MIX_O2", "NOISE", "O1_SUB")

BASE = {
    "O1_FREQ": 24, "O1_SHAPE": 64, "O1_PW": 0, "O1_SUB": 0,
    "O2_FREQ": 24, "O2_FINE": 68, "O2_SHAPE": 64, "O2_PW": 0,
    "MIX_O1": 112, "MIX_O2": 100, "NOISE": 0, "O2_MODE": "O2M_NORMAL",
    "CUTOFF": 140, "RESO": 10, "ENV_AMT": 84, "HPF": 0,
    "HP_RESO": 0, "KEYTRK": "KT_HALF", "FVEL": 40, "HP_ENV": 64,
    "FA": 0, "FD": 76, "FS": 60, "FR": 62,
    "AA": 0, "AD": 76, "AS": 120, "AR": 62,
    "LFO_RATE": 60, "LFO_AMT": 0, "LFO_SHAPE": "LS_TRI", "LFO_DEST": "LD_FREQ",
    "PM_FENV": 64, "PM_OSC2": 64, "PM_DOSC": 0, "PM_DFILT": "PMF_OFF",
    "CH_MIX": 0, "CH_RATE": 30, "CH_DEPTH": 50, "DIST": 0,
    "RV_TYPE": "RV_HALL", "RV_MIX": 25, "RV_DECAY": 70, "RV_TONE": 64,
    "SP_MOD": 30, "SP_SHIM": 0, "SP_SWELL": 0, "SP_PRE": 20,
    "GLIDE": 0, "AVEL": 50, "UNISON": 0, "BEND": 2,
    "VINTAGE": 40, "SPREAD": 60,
    "L2_RATE": 60, "L2_AMT": 0, "L2_SHAPE": "LS_TRI", "L2_DEST": "LD_FREQ",
    "L1_SYNC": "LSY_OFF", "L2_SYNC": "LSY_OFF", "L1_RETRIG": 0, "L2_RETRIG": 0,
    "AT_DEST": "AT_FILTER", "AT_AMT": 96, "AT_CURVE": 64, "AT_SMOOTH": 30,
    "DL_TIME_L": 85, "DL_TIME_R": 95, "DL_FB": 50, "DL_MIX": 0,
    "DL_DEPTH": 30, "DL_RATE": 50, "DL_TONE": 64, "DL_SYNC": 0,
}

# (name, what it is, the values that make it; the category leads how render_presets plays it)
P = [
    # ---------------------------------------------------------------- brass
    ("SOFT BRASS", "two saws, the filter swelling open and settling: a mellow section", dict(
        CUTOFF=96, RESO=14, ENV_AMT=100, FA=50, FD=78, FS=58, FR=64, AA=30, AD=80, AS=118, AR=64,
        FVEL=60, CH_MIX=20, RV_TYPE="RV_ROOM", RV_MIX=22, VINTAGE=50, SPREAD=50, AT_DEST="AT_POWER", AT_AMT=100)),
    ("BRASS STAB", "short and punchy, harder hits brighter", dict(
        CUTOFF=92, RESO=18, ENV_AMT=108, FA=30, FD=68, FS=34, FR=54, AA=0, AD=70, AS=96, AR=50,
        FVEL=80, AVEL=60, DIST=10, RV_TYPE="RV_PLATE", RV_MIX=20, AT_DEST="AT_POWER")),
    ("LOW BRASS", "a saw and a saw an octave down with the sub: trombones", dict(
        O2_FREQ=12, O1_SUB=40, MIX_O2=96, CUTOFF=84, RESO=10, ENV_AMT=104, FA=54, FD=80, FS=56, FR=64,
        AA=36, AD=80, AS=118, AR=62, FVEL=60, RV_MIX=20, AT_DEST="AT_POWER")),
    ("FIFTH BRASS", "a saw and its fifth: a fanfare in one finger", dict(
        O2_FREQ=31, O2_FINE=66, CUTOFF=98, RESO=12, ENV_AMT=100, FA=46, FD=78, FS=60, FR=64, AA=26, AS=118,
        FVEL=60, CH_MIX=25, RV_MIX=26, AT_DEST="AT_POWER")),
    ("BRASS PAD", "brass that takes its time: slow swell, long tail, wide", dict(
        CUTOFF=90, RESO=12, ENV_AMT=96, FA=90, FD=100, FS=70, FR=92, AA=80, AD=90, AS=120, AR=92,
        CH_MIX=50, CH_DEPTH=60, RV_MIX=40, RV_DECAY=82, SPREAD=100, VINTAGE=50, AT_DEST="AT_SWELL")),
    ("SYNTH HORNS", "a square and a saw, a touch of resonance, vibrato on pressure", dict(
        O1_SHAPE=127, O1_PW=20, O2_FINE=70, CUTOFF=100, RESO=22, ENV_AMT=98, FA=40, FD=76, FS=64, FR=62,
        AA=20, AS=120, FVEL=50, LFO_RATE=78, CH_MIX=25, RV_TYPE="RV_PLATE", RV_MIX=24, AT_DEST="AT_VIBRATO")),
    # ---------------------------------------------------------------- strings
    ("ANA STRINGS", "two saws apart, the chorus deep, the lows thinned: the string machine", dict(
        O2_FINE=72, CUTOFF=150, RESO=4, ENV_AMT=70, HPF=30, FA=70, FD=90, FS=100, FR=90,
        AA=76, AD=90, AS=120, AR=88, CH_MIX=80, CH_RATE=40, CH_DEPTH=80, RV_MIX=35, RV_DECAY=74,
        SPREAD=100, VINTAGE=60, AT_DEST="AT_SWELL")),
    ("LOW STRINGS", "cellos and basses: a saw and its octave below", dict(
        O2_FREQ=12, O2_FINE=70, CUTOFF=128, RESO=6, ENV_AMT=72, HPF=10, FA=70, FD=90, FS=96, FR=86,
        AA=70, AS=120, AR=86, CH_MIX=70, CH_DEPTH=70, RV_MIX=32, SPREAD=90, AT_DEST="AT_SWELL")),
    ("SLOW STRINGS", "strings fading in and out slowly, the hall long", dict(
        O2_FINE=71, CUTOFF=140, RESO=4, ENV_AMT=70, HPF=24, FA=96, FD=100, FS=100, FR=100,
        AA=96, AD=96, AS=120, AR=100, CH_MIX=75, CH_DEPTH=80, RV_MIX=50, RV_DECAY=88, SPREAD=110, AT_DEST="AT_SWELL")),
    ("PWM STRINGS", "two pulses breathing at two speeds: lush", dict(
        O1_SHAPE=127, O1_PW=50, O2_SHAPE=127, O2_PW=70, O2_FINE=70, CUTOFF=150, RESO=6, ENV_AMT=70, HPF=25,
        FA=72, FD=90, FS=100, FR=90, AA=74, AS=120, AR=90,
        LFO_RATE=50, LFO_AMT=34, LFO_DEST="LD_PW", L2_RATE=38, L2_AMT=26, L2_DEST="LD_PW",
        CH_MIX=60, CH_DEPTH=70, RV_MIX=36, SPREAD=100, AT_DEST="AT_SWELL")),
    ("OCT STRINGS", "a saw and its octave above, bright, for high lines", dict(
        O2_FREQ=36, MIX_O2=80, CUTOFF=156, RESO=4, ENV_AMT=70, HPF=28, FA=70, FD=90, FS=100, FR=88,
        AA=72, AS=120, AR=88, CH_MIX=80, CH_DEPTH=80, RV_MIX=36, SPREAD=100, AT_DEST="AT_SWELL")),
    # ---------------------------------------------------------------- pads
    ("WARM PAD", "a soft saw and a saw apart, slow, the hall wide", dict(
        O1_SHAPE=40, O2_FINE=70, CUTOFF=112, RESO=12, ENV_AMT=82, FA=92, FD=100, FS=80, FR=96,
        AA=90, AD=90, AS=120, AR=100, CH_MIX=50, RV_MIX=50, RV_DECAY=90, SPREAD=100, VINTAGE=50, AT_DEST="AT_SWELL")),
    ("DARK PAD", "low and round, an octave down and the sub, the filter mostly shut", dict(
        O2_FREQ=12, O1_SUB=50, CUTOFF=86, RESO=22, ENV_AMT=80, FA=96, FD=100, FS=90, FR=100,
        AA=96, AD=90, AS=120, AR=100, CH_MIX=40, RV_MIX=55, RV_DECAY=96, RV_TONE=50, SPREAD=90)),
    ("RANDOM PAD", "sample and hold on the filter, a little resonance, echoes", dict(
        CUTOFF=120, RESO=40, ENV_AMT=70, FA=80, FD=90, FS=100, FR=96, AA=80, AD=90, AS=120, AR=96,
        LFO_RATE=70, LFO_AMT=36, LFO_SHAPE="LS_SH", LFO_DEST="LD_LPF",
        CH_MIX=40, RV_MIX=50, RV_DECAY=86, DL_MIX=22, DL_SYNC=1, DL_TIME_L=77, DL_TIME_R=90, SPREAD=90)),
    ("VOX PAD", "a pulse and a saw through a narrow band, a slow vibrato: voices", dict(
        O1_SHAPE=100, O1_PW=40, O2_FINE=70, CUTOFF=116, RESO=36, ENV_AMT=74, HPF=30, HP_RESO=40,
        FA=86, FD=96, FS=96, FR=94, AA=90, AD=90, AS=120, AR=96, LFO_RATE=64, LFO_AMT=10,
        CH_MIX=70, CH_DEPTH=70, RV_MIX=55, RV_DECAY=88, SPREAD=100, AT_DEST="AT_SWELL")),
    ("SHIMMER PAD", "a triangle and a saw an octave up into SPACE: shimmering, endless", dict(
        O1_SHAPE=0, O2_FREQ=36, MIX_O2=70, CUTOFF=130, RESO=10, ENV_AMT=76, FA=100, FD=104, FS=90, FR=104,
        AA=100, AD=100, AS=120, AR=106, RV_TYPE="RV_SPACE", RV_MIX=70, RV_DECAY=110, RV_TONE=70,
        SP_MOD=40, SP_SHIM=80, SP_SWELL=50, SP_PRE=30, SPREAD=110, AT_DEST="AT_SWELL")),
    ("SLOW SWEEP", "the filter opening slowly as the note is held, resonant", dict(
        CUTOFF=70, RESO=34, ENV_AMT=110, FA=104, FD=110, FS=110, FR=100, AA=70, AD=90, AS=120, AR=96,
        CH_MIX=40, RV_MIX=45, RV_DECAY=86, SPREAD=100)),
    ("BREATH PAD", "saws with breath (noise), the lows cut, a plate", dict(
        NOISE=40, CUTOFF=130, RESO=20, ENV_AMT=76, HPF=40, FA=90, FD=96, FS=90, FR=96,
        AA=96, AD=90, AS=120, AR=98, CH_MIX=60, RV_TYPE="RV_PLATE", RV_MIX=45, RV_DECAY=80, SPREAD=100)),
    ("POLYMOD PAD", "oscillator 2 a fifth up bending oscillator 1, the envelope on the filter: alive", dict(
        O2_FREQ=31, MIX_O2=70, PM_OSC2=78, PM_DOSC=1, CUTOFF=110, RESO=18, ENV_AMT=88, FA=84, FD=96, FS=80, FR=96,
        AA=84, AD=90, AS=120, AR=96, CH_MIX=45, RV_MIX=45, RV_DECAY=84, SPREAD=100, AT_DEST="AT_POLYMOD")),
    # ---------------------------------------------------------------- bass
    ("ROUND BASS", "a saw and a square an octave down with the sub: round, a little driven", dict(
        O1_FREQ=12, O2_FREQ=12, O2_SHAPE=127, O2_FINE=66, O1_SUB=80, CUTOFF=80, RESO=20, ENV_AMT=98,
        FD=66, FS=30, FR=44, AD=70, AS=120, AR=44, FVEL=60, DIST=10, RV_MIX=0, VINTAGE=30, SPREAD=10)),
    ("SYNC BASS", "oscillator 1 synced, swept by the envelope: a biting bass", dict(
        O1_FREQ=19, O2_FREQ=12, O2_MODE="O2M_SYNC", MIX_O2=60, PM_FENV=100, PM_DOSC=1, CUTOFF=118, RESO=16,
        ENV_AMT=90, FD=60, FS=40, FR=44, AD=70, AS=120, AR=40, FVEL=60, DIST=16, RV_MIX=0, SPREAD=10)),
    ("PLUCK BASS", "short and resonant, harder hits brighter", dict(
        O1_FREQ=12, O2_FREQ=12, O2_FINE=66, CUTOFF=70, RESO=36, ENV_AMT=110, FD=56, FS=0, FR=40,
        AD=72, AS=80, AR=40, FVEL=80, AVEL=60, RV_MIX=0, SPREAD=10)),
    ("SUB BASS", "a triangle and the sub: the lowest, clean", dict(
        O1_FREQ=12, O1_SHAPE=0, O1_SUB=100, MIX_O2=0, CUTOFF=90, RESO=0, ENV_AMT=70, FD=70, FS=60,
        AD=70, AS=120, AR=40, KEYTRK="KT_FULL", RV_MIX=0, SPREAD=0, VINTAGE=20)),
    ("RESO BASS", "a square, the resonance high, a glide: squelch", dict(
        O1_FREQ=12, O1_SHAPE=127, MIX_O2=0, CUTOFF=66, RESO=80, ENV_AMT=110, FD=60, FS=10, FR=40,
        AD=72, AS=110, AR=40, KEYTRK="KT_FULL", FVEL=70, GLIDE=20, DIST=20, RV_MIX=0, SPREAD=0)),
    ("UNISON BASS", "every voice on the note: huge", dict(
        O1_FREQ=12, O2_FREQ=12, O2_FINE=68, UNISON=1, CUTOFF=86, RESO=16, ENV_AMT=96, FD=70, FS=40, FR=44,
        AD=72, AS=120, AR=44, GLIDE=15, DIST=20, VINTAGE=60, RV_MIX=0, AT_DEST="AT_FILTER")),
    ("OCTAVE BASS", "a saw and its octave: a bass that cuts through", dict(
        O1_FREQ=12, O2_FREQ=24, CUTOFF=96, RESO=22, ENV_AMT=96, FD=62, FS=30, FR=44, AD=76, AS=100, AR=44,
        FVEL=60, DIST=10, RV_MIX=0, SPREAD=10)),
    ("FM BASS", "oscillator 2 bending oscillator 1 at audio rate, the envelope too: a metal edge", dict(
        O1_FREQ=12, O1_SHAPE=0, O2_FREQ=24, O2_SHAPE=0, MIX_O2=20, PM_OSC2=92, PM_FENV=84, PM_DOSC=1,
        CUTOFF=110, RESO=10, ENV_AMT=96, FD=64, FS=30, FR=44, AD=72, AS=110, AR=44, FVEL=60, RV_MIX=0, SPREAD=10)),
    # ---------------------------------------------------------------- leads
    ("SAW LEAD", "two saws, a glide, a delay: the classic lead", dict(
        O2_FINE=68, CUTOFF=150, RESO=24, ENV_AMT=84, FD=80, FS=70, FR=56, AD=80, AS=120, AR=56,
        KEYTRK="KT_FULL", GLIDE=20, AT_DEST="AT_VIBRATO", DL_MIX=25, DL_SYNC=1, DL_TIME_L=82, DL_TIME_R=82,
        RV_TYPE="RV_PLATE", RV_MIX=25)),
    ("SQUARE LEAD", "a square and its octave, hollow, a glide", dict(
        O1_SHAPE=127, O2_SHAPE=127, O2_FREQ=36, MIX_O2=60, CUTOFF=140, RESO=20, ENV_AMT=80, FD=80, FS=80,
        AS=120, KEYTRK="KT_FULL", GLIDE=30, AT_DEST="AT_VIBRATO", DL_MIX=20, DL_SYNC=1, DL_TIME_L=77, DL_TIME_R=77)),
    ("GLIDE LEAD", "every voice on one note, a long glide", dict(
        UNISON=1, CUTOFF=156, RESO=30, ENV_AMT=84, FD=84, FS=80, GLIDE=60, KEYTRK="KT_FULL", VINTAGE=60,
        AT_DEST="AT_VIBRATO", DL_MIX=30, DL_SYNC=1, DL_TIME_L=82, DL_TIME_R=90, RV_TYPE="RV_PLATE", RV_MIX=25)),
    ("FIFTH LEAD", "a saw and its fifth, a little driven", dict(
        O2_FREQ=31, O2_FINE=66, CUTOFF=160, RESO=18, ENV_AMT=80, FD=80, FS=80, KEYTRK="KT_FULL", GLIDE=16,
        DIST=20, AT_DEST="AT_VIBRATO", DL_MIX=22, DL_SYNC=1, DL_TIME_L=82, DL_TIME_R=82)),
    ("SCREAM LEAD", "the resonance high, driven: it sings", dict(
        CUTOFF=120, RESO=90, ENV_AMT=90, FD=90, FS=80, KEYTRK="KT_FULL", GLIDE=16, DIST=40,
        AT_DEST="AT_FILTER", AT_AMT=100, RV_TYPE="RV_PLATE", RV_MIX=24, DL_MIX=18)),
    ("SOFT LEAD", "triangles and a breath, a gentle vibrato: a flute", dict(
        O1_SHAPE=0, O2_SHAPE=0, O2_FREQ=36, MIX_O2=30, NOISE=20, CUTOFF=150, RESO=10, ENV_AMT=74, FA=30,
        FD=80, FS=100, AA=40, AS=120, AR=62, LFO_RATE=76, LFO_AMT=12, KEYTRK="KT_FULL",
        AT_DEST="AT_VIBRATO", RV_MIX=30)),
    # ---------------------------------------------------------------- keys
    ("E-PIANO", "triangles, the upper one bending the lower: tines, the chorus slow", dict(
        O1_SHAPE=0, O2_SHAPE=0, O2_FREQ=36, MIX_O2=40, PM_OSC2=76, PM_DOSC=1, CUTOFF=140, RESO=4, ENV_AMT=80,
        FD=86, FS=20, FR=70, AD=96, AS=0, AR=70, FVEL=70, AVEL=80, CH_MIX=60, CH_RATE=30, RV_TYPE="RV_ROOM", RV_MIX=25)),
    ("ORGAN", "triangles an octave apart and the sub, no envelope: drawbars", dict(
        O1_SHAPE=0, O2_SHAPE=0, O2_FREQ=36, O2_FINE=64, O1_SUB=60, MIX_O2=96, CUTOFF=200, RESO=0, ENV_AMT=64,
        FD=70, FS=127, FR=40, AD=70, AS=127, AR=40, KEYTRK="KT_FULL", AVEL=0, FVEL=0,
        CH_MIX=60, CH_RATE=70, CH_DEPTH=60, RV_TYPE="RV_ROOM", RV_MIX=24, VINTAGE=30)),
    ("HARPSICHORD", "a narrow pulse and a saw an octave up, the lows cut", dict(
        O1_SHAPE=127, O1_PW=100, O2_FREQ=36, O2_FINE=66, MIX_O2=80, CUTOFF=160, RESO=10, ENV_AMT=90, HPF=40,
        FD=64, FS=20, FR=60, AD=80, AS=0, AR=60, KEYTRK="KT_FULL", FVEL=40, RV_TYPE="RV_ROOM", RV_MIX=30)),
    ("POLY KEYS", "saws, the filter closing as the key is held: an analogue poly's keys", dict(
        CUTOFF=120, RESO=14, ENV_AMT=96, FD=76, FS=30, FR=70, AD=90, AS=70, AR=70, FVEL=60, AVEL=60,
        CH_MIX=40, RV_MIX=26)),
    ("BELL KEYS", "oscillator 2 a twelfth up bending oscillator 1: bells", dict(
        O1_SHAPE=0, O2_SHAPE=0, O2_FREQ=43, O2_FINE=66, MIX_O2=0, PM_OSC2=100, PM_DOSC=1, CUTOFF=192, RESO=8,
        ENV_AMT=74, FD=86, FS=0, FR=92, AD=100, AS=0, AR=96, KEYTRK="KT_FULL", FVEL=50, RV_MIX=40, RV_DECAY=82)),
    ("BARK KEYS", "a pulse, resonant and driven, velocity bright: a reedy electric piano", dict(
        O1_SHAPE=100, O1_PW=70, MIX_O2=60, CUTOFF=120, RESO=26, ENV_AMT=90, FD=70, FS=20, FR=60,
        AD=86, AS=40, AR=60, FVEL=90, AVEL=70, DIST=30, CH_MIX=40, RV_TYPE="RV_ROOM", RV_MIX=24)),
    # ---------------------------------------------------------------- plucks, stabs
    ("SOFT PLUCK", "saws, resonant, short, echoing in time", dict(
        CUTOFF=90, RESO=30, ENV_AMT=104, FD=62, FS=0, FR=62, AD=74, AS=0, AR=70, FVEL=60,
        DL_MIX=30, DL_SYNC=1, DL_TIME_L=82, DL_TIME_R=90, RV_TYPE="RV_PLATE", RV_MIX=30, SPREAD=80)),
    ("MARIMBA", "a triangle and its two octaves, struck: wood", dict(
        O1_SHAPE=0, O2_SHAPE=0, O2_FREQ=48, MIX_O2=40, CUTOFF=140, RESO=30, ENV_AMT=90, FD=54, FS=0, FR=56,
        AD=70, AS=0, AR=60, KEYTRK="KT_FULL", FVEL=70, AVEL=70, RV_TYPE="RV_ROOM", RV_MIX=25)),
    ("KALIMBA", "a triangle bent by its twelfth, short: a thumb piano", dict(
        O1_SHAPE=0, O2_SHAPE=0, O2_FREQ=43, MIX_O2=0, PM_OSC2=86, PM_FENV=80, PM_DOSC=1, CUTOFF=170, RESO=6,
        ENV_AMT=70, FD=60, FS=0, FR=60, AD=74, AS=0, AR=64, KEYTRK="KT_FULL", FVEL=50, AVEL=70,
        DL_MIX=25, DL_SYNC=1, DL_TIME_L=77, DL_TIME_R=86, RV_MIX=28)),
    ("PIZZICATO", "saws apart, plucked, a hall: plucked strings", dict(
        O2_FINE=70, CUTOFF=126, RESO=10, ENV_AMT=96, HPF=20, FD=60, FS=0, FR=60, AD=70, AS=0, AR=60,
        FVEL=60, AVEL=40, CH_MIX=50, RV_MIX=35, RV_DECAY=74, SPREAD=100)),
    ("HOUSE STAB", "a pulse and its octave, resonant and short: the stab", dict(
        O1_SHAPE=127, O2_SHAPE=127, O2_FREQ=36, MIX_O2=90, CUTOFF=120, RESO=30, ENV_AMT=90, FD=62, FS=0, FR=54,
        AD=70, AS=0, AR=54, FVEL=60, RV_TYPE="RV_PLATE", RV_MIX=25, DL_MIX=20, DL_SYNC=1, DL_TIME_L=82, DL_TIME_R=82)),
    ("SEQ PLUCK", "for the arp: a tight, resonant pluck, a dotted echo", dict(
        CUTOFF=80, RESO=50, ENV_AMT=110, FD=56, FS=0, FR=50, AD=66, AS=0, AR=54, KEYTRK="KT_FULL", FVEL=70,
        DL_MIX=35, DL_SYNC=1, DL_TIME_L=88, DL_TIME_R=88, DL_FB=60, RV_MIX=20)),
    ("SYNC STAB", "synced, the envelope sweeping its pitch, short: a bright stab", dict(
        O1_FREQ=31, O2_MODE="O2M_SYNC", MIX_O2=40, PM_FENV=110, PM_DOSC=1, CUTOFF=170, RESO=10, ENV_AMT=74,
        FD=60, FS=0, FR=50, AD=66, AS=0, AR=50, FVEL=50, DIST=15, RV_TYPE="RV_PLATE", RV_MIX=22)),
    # ---------------------------------------------------------------- effects, drones, drums
    ("DRONE", "a fifth, slowly moving filter and pitch, the SPACE long: for holding", dict(
        O2_FREQ=31, CUTOFF=100, RESO=50, ENV_AMT=64, FA=100, FS=127, FR=110, AA=100, AS=127, AR=110,
        LFO_RATE=30, LFO_AMT=40, LFO_DEST="LD_LPF", L2_RATE=40, L2_AMT=8, L2_SHAPE="LS_SH",
        RV_TYPE="RV_SPACE", RV_MIX=70, RV_DECAY=120, SP_MOD=60, SP_SHIM=30, SP_SWELL=40, SPREAD=110)),
    ("RANDOM BLIPS", "sample and hold on the filter in sixteenths, resonant: the computer thinking", dict(
        CUTOFF=130, RESO=80, ENV_AMT=64, LFO_AMT=90, LFO_SHAPE="LS_SH", LFO_DEST="LD_LPF", L1_SYNC="LSY_16",
        L1_RETRIG=1, AS=120, KEYTRK="KT_FULL", DL_MIX=40, DL_SYNC=1, DL_TIME_L=82, DL_TIME_R=90, RV_MIX=26)),
    ("WIND", "noise through a resonant filter moving slowly: weather", dict(
        MIX_O1=0, MIX_O2=0, NOISE=127, CUTOFF=150, RESO=62, ENV_AMT=64, KEYTRK="KT_FULL",
        AA=90, AS=127, AR=100, LFO_RATE=30, LFO_AMT=40, LFO_DEST="LD_LPF", L2_RATE=22, L2_AMT=30, L2_DEST="LD_AMP",
        RV_MIX=50, RV_DECAY=90, SPREAD=100)),
    ("SYNTH TOM", "a triangle falling in pitch with a little noise: an analogue tom", dict(
        O1_FREQ=12, O1_SHAPE=0, O1_SUB=70, MIX_O2=0, NOISE=30, PM_FENV=110, PM_DOSC=1, CUTOFF=140, RESO=10,
        ENV_AMT=80, FD=62, FS=0, FR=62, AD=74, AS=0, AR=62, KEYTRK="KT_FULL", FVEL=50, AVEL=40, RV_TYPE="RV_ROOM", RV_MIX=25,
        SPREAD=20)),
]


GROUPS = [("BRASS", "SOFT BRASS"), ("STRINGS", "ANA STRINGS"), ("PADS", "WARM PAD"), ("BASS", "ROUND BASS"),
          ("LEADS", "SAW LEAD"), ("KEYS", "E-PIANO"), ("PLUCKS & STABS", "SOFT PLUCK"), ("EFFECTS & DRUMS", "DRONE")]


def main():
    assert len(P) == 50, len(P)
    names = set()
    rows = []
    levels = json.load(open(LEVELS)) if os.path.isfile(LEVELS) else {}
    for name, what, vals in P:
        assert len(name) <= 12 and name == name.upper() and name not in names, name
        names.add(name)
        unknown = set(vals) - set(BASE)
        assert not unknown, (name, unknown)
        v = dict(BASE)
        v.update(vals)
        g = levels.get(name, 1.0)
        for k in MIXER:
            v[k] = max(0, min(127, int(round(v[k] * g))))
        body = ", ".join("[P_%s] = %s" % (k, v[k]) for k in BASE)
        rows.append("    /* %s */\n    {\"%s\", {%s}}," % (what, name, body))
    with open(OUT, "w") as f:
        f.write("/* SPDX-License-Identifier: MPL-2.0\n * Copyright (C) 2026 swtdr34mz (PROPHET 67) */\n")
        f.write("/* The VST's own fifty presets, after the sixteen factory programs (generated by tools/make_vst_presets.py:\n"
                " * edit there). The names and the sounds are our own. */\n#include \"engine.h\"\n#include \"vst_presets.h\"\n\n")
        f.write("const patch_t VST_PRESETS[NVST_PRESETS] = {\n%s\n};\n" % "\n".join(rows))
        index = [n for n, _w, _v in P]
        groups = ", ".join('{"%s", %d}' % (g, index.index(first)) for g, first in GROUPS)
        assert len(GROUPS) == 8
        f.write("\n/* the menu's groups: a name and the first preset */\nconst vst_group_t VST_GROUPS[NVST_GROUPS] = {%s};\n" % groups)
    print("%d presets -> %s" % (len(P), os.path.normpath(OUT)))


if __name__ == "__main__":
    main()
