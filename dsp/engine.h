/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* PROPHET 67's sound engine: a 7-voice analogue-style poly in the manner of the Sequential Prophet-6.
 *
 *   voice     two oscillators with a continuously variable shape (triangle - saw - pulse) and pulse width, a triangle
 *             sub-octave under oscillator 1, white noise; oscillator 1 hard-synced to oscillator 2, oscillator 2's
 *             LOW FREQ mode and keyboard tracking; a 2-pole resonant high-pass into a 4-pole resonant low-pass that
 *             self-oscillates (zero-delay-feedback ladder with a saturating input); a filter and an amp ADSR with
 *             analogue (RC) curves
 *   poly mod  filter envelope and oscillator 2 (both bipolar, per voice, oscillator 2 at audio rate) to oscillator 1's
 *             frequency, shape and pulse width and the low- and high-pass cutoffs
 *   vintage   each voice its own fixed character (tuning, drift, cutoff, resonance, envelope times, level, width)
 *   spread    each voice its own place in the stereo field, a little detune and colour of its own
 *   effects   analogue-style distortion, a BBD chorus (FX A); room, hall, plate, spring and SPACE, an ambient reverb
 *             after the Meris Mercury7 (FX B)
 *   clock     the step sequencer (16 bars, chords on each step) and the arpeggiator, run in the render
 *
 * All parameters are bytes 0..127 (or a choice), in a patch of NPARAM; eng_patch turns them into the engine's numbers.
 * The main loop calls eng_*; eng_render runs in the audio interrupt. Single precision only. */
#pragma once
#include <stdint.h>

#define ENG_SR 44100.0f
#define NVOICE 7

/* ---------------------------------------------------------------- parameters --- */
enum {
    P_O1_FREQ, P_O1_SHAPE, P_O1_PW, P_O1_SUB,          /* OSC 1 */
    P_O2_FREQ, P_O2_FINE, P_O2_SHAPE, P_O2_PW,         /* OSC 2 */
    P_MIX_O1, P_MIX_O2, P_NOISE, P_O2_MODE,            /* MIXER */
    P_CUTOFF, P_RESO, P_ENV_AMT, P_HPF,                /* FILTER (HOME) */
    P_HP_RESO, P_KEYTRK, P_FVEL, P_HP_ENV,             /* FILTER 2 */
    P_FA, P_FD, P_FS, P_FR,                            /* FILTER ENV */
    P_AA, P_AD, P_AS, P_AR,                            /* AMP ENV */
    P_LFO_RATE, P_LFO_AMT, P_LFO_SHAPE, P_LFO_DEST,    /* LFO */
    P_PM_FENV, P_PM_OSC2, P_PM_DOSC, P_PM_DFILT,       /* POLY MOD */
    P_CH_MIX, P_CH_RATE, P_CH_DEPTH, P_DIST,           /* FX A */
    P_RV_TYPE, P_RV_MIX, P_RV_DECAY, P_RV_TONE,        /* FX B */
    P_SP_MOD, P_SP_SHIM, P_SP_SWELL, P_SP_PRE,         /* SPACE */
    P_GLIDE, P_AVEL, P_UNISON, P_BEND,                 /* VOICE */
    P_VINTAGE, P_SPREAD,                               /* SELECT, ALGORITHM */
    P_L2_RATE, P_L2_AMT, P_L2_SHAPE, P_L2_DEST,        /* LFO 2 (v0.2: added after, so older presets load) */
    P_L1_SYNC, P_L2_SYNC, P_L1_RETRIG, P_L2_RETRIG,    /* LFO SYNC */
    P_AT_DEST, P_AT_AMT, P_AT_CURVE, P_AT_SMOOTH,      /* AFTERTOUCH (v0.5) */
    P_DL_TIME_L, P_DL_TIME_R, P_DL_FB, P_DL_MIX,       /* DELAY (v0.6) */
    P_DL_DEPTH, P_DL_RATE, P_DL_TONE, P_DL_SYNC,       /* DELAY MOD */
    NPARAM
};
typedef struct {
    const char *name;                  /* on the screen */
    uint8_t kind;                      /* PK_* */
    uint8_t max;                       /* the largest value (127 for a knob, n - 1 for a choice) */
    const char *const *choice;         /* PK_CHOICE: the names */
} param_t;
enum { PK_KNOB, PK_BIPOLAR, PK_SEMI, PK_FINE, PK_TIME, PK_CHOICE, PK_PCT, PK_HZ, PK_DTIME };
extern const param_t PARAMS[NPARAM];
void param_text(int p, int v, char *out);     /* the value as shown (up to 8 characters) */

/* the choices */
enum { O2M_NORMAL, O2M_SYNC, O2M_LOW, O2M_LOWFIX, NO2M };      /* oscillator 2: as is, osc 1 synced to it, LOW FREQ,
                                                                * LOW FREQ without the keyboard */
enum { KT_OFF, KT_HALF, KT_FULL, NKT };
enum { LS_TRI, LS_SAW, LS_RAMP, LS_SQUARE, LS_SH, NLS };
enum { LD_FREQ, LD_FREQ1, LD_FREQ2, LD_PW, LD_LPF, LD_HPF, LD_AMP, NLD };
enum { LSY_OFF, LSY_4BAR, LSY_2BAR, LSY_1BAR, LSY_2, LSY_4, LSY_4T, LSY_8, LSY_8T, LSY_16, LSY_16T, LSY_32, NLSY };
extern const uint16_t LSY_TICKS[NLSY];   /* a cycle, in clock ticks (24 a quarter) */
#define PMO_FREQ 1u                    /* poly mod's oscillator destinations, a mask (the choice's value) */
#define PMO_SHAPE 2u
#define PMO_PW 4u
enum { PMF_OFF, PMF_LPF, PMF_HPF, PMF_BOTH, NPMF };
enum { RV_ROOM, RV_HALL, RV_PLATE, RV_SPRING, RV_SPACE, NRV };
/* AFTERTOUCH: what pressing harder does (OFFWORLD's FILTER, SWELL, POWER, VIBRATO, and the Prophet-6's own) */
/* DELAY SYNC: the times free (ms), or notes of the clock */
enum { DLS_OFF, DLS_ON, NDLSYNC };
#define NDLS 12
extern const uint8_t DL_SYNC_TICKS[NDLS];   /* synced: TIME's 128 positions over these notes (in clock ticks) */
extern const char *const DL_SYNC_NAME[NDLS];
enum { AT_OFF, AT_FILTER, AT_SWELL, AT_POWER, AT_VIBRATO, AT_LFO, AT_PITCH, AT_POLYMOD, NAT };

/* the presets */
#define NAME_LEN 12
typedef struct {
    char name[NAME_LEN];
    uint8_t p[NPARAM];
} patch_t;
#define NPRESETS 16
extern const patch_t FACTORY[NPRESETS];

/* --------------------------------------------------------------- the engine --- */
void eng_init(void);
void eng_patch(const uint8_t *p);       /* the sound (a patch's NPARAM bytes); any time */
/* notes: id names the note (the same id again strikes it again), note a MIDI note, vel 0..1 */
void eng_note_on(int id, int note, float vel);
void eng_note_on_gain(int id, int note, float vel, float gain);   /* with its own level (the split's BALANCE) */
void eng_press(int id, float p);        /* a note's pressure 0..1 (the keys, MIDI poly pressure) */
void eng_press_all(float p);            /* every note's (MIDI channel pressure) */
void eng_note_off(int id);
void eng_sustain(int on);               /* the pedal */
void eng_bend(float v);                 /* -1..1 (the patch's BEND range) */
void eng_mod(float v);                  /* the mod wheel 0..1: more LFO */
void eng_cutoff_cc(int v);              /* MIDI CC 74 (-1: the patch's own) */
#ifdef P67_PLUGIN
void eng_hp_extra(int vel, int keytrk); /* the plugin's own HIGH-PASS VELOCITY (0..127) and KEYBOARD (KT_*), at the next patch */
#endif
void eng_all_off(void);                 /* every note let go */
void eng_panic(void);                   /* silence at once */
void eng_overload(void);                /* the render ran late: let the quietest voice go now */
void eng_render(int32_t *lr, uint32_t n, float gain);
void eng_duck(int on);                  /* a flash write is coming: fade out (and back in) */
int eng_ducked(void);

/* the screen and the tests */
float eng_voice_env(int v);             /* voice v's amp envelope 0..1 (its LED) */
float eng_voice_press(int v);           /* its pressure now 0..1 (smoothed) */
int eng_voice_note(int v);              /* the note it plays (-1: none) */
int eng_voices(void);                   /* voices sounding */
uint32_t eng_notes(void);               /* notes started since the start */
uint32_t eng_steals(void);              /* voices taken from a note still sounding */
float eng_level(void);                  /* the output 0..1, smoothed */
float eng_tail(void);                   /* FX B's tail level 0..1 (the screen) */
void eng_load(uint32_t *c1, uint32_t *c2, uint32_t *c1_peak, uint32_t *c2_peak);   /* the cores' load, % (peaks: the last second) */
float eng_lfo(int which);               /* LFO 1 (0) or 2 (1) now, -1..1 */

/* ---------------------------------------------------- the clock, sequencer and arp --- */
/* A 24-per-quarter clock runs in the render: from BPM, or MIDI clock coming in. The sequencer and the arp step on it,
 * on time to within an audio block (2.9 ms). */
#define SEQ_BARS 16
#define SEQ_STEPS (SEQ_BARS * 16)
#define SEQ_POLY 7                      /* notes a step holds */
#define SN_NONE 0                       /* an empty slot */
#define SN_TIE 1                        /* note slot 0 = TIE: the step holds the step before's notes on */
typedef struct {
    uint8_t note[SEQ_STEPS][SEQ_POLY];  /* 0: none, 1: tie (slot 0), else the MIDI note */
} seq_t;
enum { RATE_8, RATE_8T, RATE_16, RATE_16T, RATE_32, NRATE };
void eng_seq_bind(const seq_t *s);      /* the sequence the sequencer plays */
void eng_seq_set(int bars, int rate, float gate, int transpose);
void eng_play(int on);                  /* the sequencer from the top / stopped */
int eng_playing(void);
int eng_seq_step(void);                 /* the step playing (-1: stopped) */
int eng_seq_nearest(void);              /* the step a note played now belongs to (quantised), -1 stopped */
void eng_tempo(float bpm, float swing); /* BPM, swing 0..0.5 */
float eng_bpm(void);                    /* the tempo now (heard in MIDI clock, if it comes) */
enum { CLK_TICK, CLK_START, CLK_STOP, CLK_CONT };
void eng_clock(int what);               /* MIDI real time in (from the 1 kHz tick) */
int eng_clock_ext(void);                /* 1: MIDI clock is coming in */
uint32_t eng_beats(void);               /* quarter notes since the start (the screen) */

enum { ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM, ARP_PLAYED, NARP };
void eng_arp(int on, int mode, int rate, int octaves, float gate);
void eng_arp_key(int note, int down, float vel);   /* a key held for the arp (the arp plays them) */
void eng_arp_hold(int on);              /* HOLD: the notes stay after the keys are let go (until a new chord) */
int eng_arp_on(void);
int eng_arp_note(void);                 /* the arp's last note (-1 none) */
uint32_t eng_arp_steps(void);
