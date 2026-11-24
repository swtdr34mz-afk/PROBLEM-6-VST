/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* PROPHET 67's voices (engine.h): oscillators, mixer, filters, envelopes, poly mod, vintage and spread, the voice
 * allocation, and the render that runs them on both cores and hands the sum to the effects (fx.c).
 *
 * Rates: the oscillators, filters and envelopes per sample; pitches, cutoffs and their coefficients per chunk of CH
 * samples (0.18 ms), except where oscillator 2 modulates them through poly mod: then per sample, as on the real thing,
 * where oscillator 2 is an audio-rate source.
 *
 * The oscillators are band-limited with a polynomial BLEP applied as events: each step in a waveform (a saw's reset,
 * a pulse's two edges, a hard-sync reset) is found where it falls between two samples and smoothed over those two
 * samples (the output runs one sample late for that). Hard sync and audio-rate pulse-width modulation stay clean. */
#define RAM_INLINE_MATH                  /* (voices_render runs from RAM: core.h) */
#include "core.h"
#include "plat.h"

#if defined P67_PLUGIN && !defined LADDER_LINEAR
#define LADDER_OTA                       /* the low-pass's stages saturate each (ladder, ota) */
#endif
#ifndef SMOOTH_K
#ifdef P67_PLUGIN
#define SMOOTH_K 0.14f                   /* (the plugin: 90 % in 11 ms; a host's automation moves in coarser steps
                                          * than a knob's detents) */
#else
#define SMOOTH_K 0.25f
#endif
#endif
#define CH 16u                           /* a chunk (0.36 ms): pitches and coefficient targets; the coefficients
                                          * themselves ramp per sample */
#ifdef P67_PLUGIN
#define HAAS 128u                        /* the spread's per-voice delay line (a power of two; room for 192 kHz) */
#else
#define HAAS 32u                         /* the spread's per-voice delay line (a power of two) */
#endif

/* ------------------------------------------------------------- the patch --- */
/* eng_patch's numbers (main loop writes, the render reads; each a single float, no torn values) */
static struct {
    float o1_semi, o2_semi, o1_shape, o2_shape, o1_pw, o2_pw;
    float mix1, mix2, sub, noise;
    uint8_t o2_mode;
    float cut_oct, res_k, env_oct, hp_oct, hp_k, keytrk, fvel, hp_env_oct, hp_keytrk, hp_fvel, fenv_vel;
    float fs, as;                        /* sustain levels */
    float lfo_hz[2], lfo_amt[2];
    uint8_t lfo_shape[2], lfo_dest[2], lfo_sync[2], lfo_retrig[2];
    uint8_t lfo_noise;                   /* LFO 1 on S+H with FREQUENCY at the top: the Prophet-6's hidden sixth
                                          * shape, noise (the plugin's) */
    float pm_fenv, pm_osc2;              /* -1..1, squared with their sign */
    uint8_t pm_dosc, pm_dfilt;
    float glide_k, avel, bend;
    uint8_t unison;
    float vint, spread;
    uint8_t at_dest;
    float at_amt, at_curve, at_k, at_kr;   /* AFTERTOUCH: amount -1..1, the curve's exponent, the smoothing (held,
                                            * and let go: never faster than 150 ms, so a release never jumps) */
} P;
#ifdef P67_PLUGIN
/* the plugin: the patch's continuous numbers as the voices get them, gliding to eng_patch's in about 4 ms (a host's
 * automation moves in steps; none is heard as a step): Q. The per-voice character from VINTAGE and SPREAD glides the
 * same way (VT, the targets; the voice's own fields, where they are) */
static struct {
    float mix1, mix2, sub, noise, o1_shape, o2_shape, o1_pw, o2_pw, pm_fenv, pm_osc2, lfo_amt[2], avel, fvel, hp_fvel,
        fenv_vel, hp_env_oct, at_amt, modw, bend;
    uint8_t init;
} Q;
#define PQ Q
#define PQ_BEND Q.bend
#else
#define PQ P
#define PQ_BEND bend_v
#endif

/* ------------------------------------------------------------ vintage, spread --- */
/* Each voice's own character. Hand-set rather than random: every column has its mean near 0 and no voice is extreme
 * in everything, so a chord stays in tune with itself and no single voice sticks out, at any VINTAGE. */
static const float V_T1[NVOICE] = {0.6f, -0.8f, 0.2f, 1.0f, -0.4f, -0.1f, -0.5f};
static const float V_T2[NVOICE] = {-0.5f, 0.3f, 0.9f, -0.2f, -1.0f, 0.7f, -0.2f};
static const float V_CUT[NVOICE] = {0.4f, -0.9f, 0.1f, 0.7f, -0.3f, 1.0f, -1.0f};
static const float V_RES[NVOICE] = {-0.3f, 0.6f, -0.8f, 0.2f, 0.9f, -0.5f, -0.1f};
static const float V_ATT[NVOICE] = {0.5f, -0.2f, 1.0f, -0.7f, 0.1f, -0.9f, 0.2f};
static const float V_DEC[NVOICE] = {-0.6f, 0.8f, -0.1f, 0.4f, -0.9f, 0.3f, 0.1f};
static const float V_REL[NVOICE] = {0.2f, -0.5f, 0.7f, -1.0f, 0.6f, 0.1f, -0.1f};
static const float V_AMP[NVOICE] = {-0.4f, 0.2f, 0.6f, -0.8f, 0.3f, -0.1f, 0.2f};
static const float V_PW[NVOICE] = {0.7f, -0.3f, -0.6f, 0.2f, 0.9f, -0.8f, -0.1f};
static const float V_SHP[NVOICE] = {-0.2f, 0.5f, 0.3f, -0.7f, 0.1f, 0.8f, -0.8f};
/* SPREAD: where each voice stands (the voices are taken in turn, so neighbours in a chord land apart), its detune
 * (cents at full), its colour (cutoff, semitones at full) */
static const float S_PAN[NVOICE] = {-0.95f, 0.9f, -0.35f, 0.45f, -0.7f, 0.75f, 0.05f};
static const float S_DET[NVOICE] = {-3.0f, 3.5f, -1.5f, 1.2f, -2.5f, 2.2f, 0.4f};
static const float S_COL[NVOICE] = {0.35f, -0.45f, 0.2f, -0.15f, 0.5f, -0.3f, 0.0f};
/* UNISON: each voice's place in the stack (times the detune) */
static const float U_DET[NVOICE] = {0.0f, -1.0f, 1.0f, -0.55f, 0.55f, -0.25f, 0.3f};

/* ------------------------------------------------------------- a voice --- */
#ifdef P67_PLUGIN
/* the plugin's oscillators: band-limited by a table, a windowed sinc's step (BLEP) and its ramp (BLAMP) over 16
 * samples, each corner of a wave placed to 1/1024 of a sample (P67Engine.cpp makes the tables): the aliasing about
 * -80 dB where the FM-1's two-sample BLEP leaves -35 to -45, the top flat to 16 kHz. The output runs BL_H samples
 * late (0.18 ms), into a ring that takes each correction where it falls */
#define BL_H 8
#define BL_OS 1024
#define BL_N (2 * BL_H * BL_OS + 2)      /* (a table: t = -BL_H .. BL_H, and one more for the interpolation) */
#define ORING 32u
#endif
typedef struct {
    float ph, prev;                      /* phase 0..1, the output a sample late (the BLEP's second half) */
#ifdef P67_PLUGIN
    float ring[ORING];
    uint32_t rp;
#endif
} osc_t;
enum { ES_IDLE, ES_ATT, ES_DEC, ES_REL };
typedef struct {
    float v;
    uint8_t st;
} env_t;
typedef struct {
    int16_t id;                          /* the note's id (-1: none) */
    int8_t note;
    uint8_t gate, sus;                   /* the key held; let go but the pedal holds it */
    float vel, pitch, target;
    osc_t o1, o2;
    float flip;                          /* the sub-octave's half (0 / 1) */
    env_t fe, ae;
    float s1, s2, s3, s4;                /* the ladder */
    float h1, h2;                        /* the high-pass */
    float dr1, dr2, dt1, dt2;            /* VINTAGE's drift, now and where it heads (cents) */
    uint16_t dr_n;                       /* chunks until a new heading */
    uint32_t seed, age;
    /* a voice taken while it sounds: it fades out in STEAL samples, then plays this */
    int16_t pid;
    int8_t pnote;
    uint8_t pend;
    float pvel, fade;
    /* its character from VINTAGE and SPREAD (eng_patch) */
    float c1, c2, ccut, cres, ka_f, kd_f, kr_f, ka_a, kd_a, kr_a, camp, cpw, cshape, cpw2, cshape2, cdrive;
    float n1, n2, ncut;                  /* VINTAGE, per note: each keystroke lands a little differently */
    /* the filters' coefficients where the last chunk ended: each chunk moves from there to its own, sample by sample,
     * so a cutoff never steps (lG, lig, lG4, lk, lc: the low-pass; ha1, ha2, hg, hk: the high-pass) */
    float lG, lig, lG4, lk, lc, ha1, ha2, hg, hk;
    uint8_t cinit;
    float gain, pgain;                   /* its level (the split's BALANCE), the waiting note's */
    float press, ptgt;                   /* AFTERTOUCH: now (smoothed), where it heads (curved) */
    float gl, gr;
    uint8_t hd, far;                     /* the spread's delay on the far side (samples), which side (0 L, 1 R) */
    float hdf;                           /* (the plugin: that delay in fractions of a sample, gliding) */
    uint8_t hw;
    float hbuf[HAAS];
#ifdef P67_PLUGIN
    float ou0[5], ou1[5], od0[5], od1[5], odp;   /* the low-pass's 2x: the half-bands up and down */
    float subd[16];                      /* the sub-octave, delayed as oscillator 1 is */
    float g1, g2, gsub, gnoise, gout, ga[5];   /* the mixer's levels and the voice's, per sample (gliding: no step at a
                                          * chunk's edge); gok: set (a voice from silence starts there) */
    uint8_t gok;
#endif
} voice_t;
static voice_t V[NVOICE];
#ifdef P67_PLUGIN
typedef struct {                         /* a voice's character as eng_patch sets it (q_glide takes the voice there) */
    float c1, c2, ccut, cres, camp, cpw, cshape, cpw2, cshape2, cdrive, gl, gr, hdf;
    uint8_t far;
} vchar_t;
static vchar_t VT[NVOICE];
#define CV (&VT[i])
#else
#define CV v
#endif
#define ENV_END 3.2e-4f                  /* -70 dB: a release ends here */
#define STEAL 200.0f                     /* a stolen voice's fade, samples (4.5 ms, a smoothstep curve) */

static uint32_t notes_n, steals_n, age_n, rr;
static float on_gain = 1.0f;            /* the level of the note being started (EV_ON's) */
static float chan_press;                 /* MIDI channel pressure, curved (every note) */
static uint8_t sustain;
static float bend_v, mod_v;
static int cc_cut = -1;
static float lvl_out;
static uint8_t duck_on;
#ifdef P67_PLUGIN
static float dcb_x[2], dcb_y[2];
static uint8_t q_eco;                    /* (eng_quality) */
#endif
static float duck_g = 1.0f, gain_s;

/* unison: the notes held, the last on top */
#define USTACK 16
static int16_t u_id[USTACK];
static int8_t u_note[USTACK];
static float u_vel[USTACK];
static int u_n;

/* the LFO, shared: its value per chunk of the block (worked out before the voices) */
/* the two LFOs, shared by the voices: per chunk of the block, what each destination gets from them (worked out before
 * the voices). Free, or SYNCed: then their phase is the clock's position (so they stay on the beat, and start with the
 * sequencer). RETRIG: a new note played with no other key down starts them from the top */
static float lfo_ph[2], lfo_sh[2], lfo_now[2], lfo_off[2];
static uint32_t lfo_seed = 0x1234567u;
static uint8_t lfo_reset;                /* a note asked for a RETRIG (render) */
static float blk_mod[NBLK / CH][NLD];    /* each destination's LFO, amounts in */
static float blk_trem[NBLK / CH];        /* AMP: the gain, 0..1 */
static float blk_l1[NBLK / CH], blk_vib[NBLK / CH];   /* LFO 1 at full amount; AFTERTOUCH's vibrato (5.5 Hz) */
static float vib_ph;
/* a fast LFO (LFO 1 above 40 Hz, or its noise): worked out per sample, not per chunk, and through the CV's own
 * bandwidth (two poles at 1.6 kHz: the DAC's smoothing and the analogue control path into the filter), so an
 * audio-rate LFO is clean and the noise reaches the cutoff as a smooth, dense wander, not as a raw hiss. What each
 * destination gets, per sample (blk_ls; blk_ltrem: AMP's gain; blk_l1s: LFO 1 at full amount, for AFTERTOUCH) */
static uint8_t lfo_fast[2];
static float lfo_cv[2][2];
static float blk_ls[NLD][NBLK], blk_ltrem[NBLK], blk_l1s[NBLK];
static uint32_t blk_lfd;
static float blk_zero[NBLK];             /* (nought: a destination no fast LFO reaches) */                 /* the destinations a fast LFO reaches (a mask of 1 << LD_*) */
/* the filter's knobs (and CC 74), as they reach the voices: a knob moves in detents, so each detent's step is spread
 * over 2.5 ms, per chunk; the envelopes, LFOs and poly mod are never smoothed */
static float blk_cut[NBLK / CH], blk_res[NBLK / CH], blk_env[NBLK / CH], blk_hp[NBLK / CH], blk_hpk[NBLK / CH];
static struct {
    float cut, res, env, hp, hpk;        /* the second stage (what the voices get) */
    float c1, r1, e1, h1, k1;            /* the first */
    uint8_t init;
} SM;

/* ------------------------------------------------------- the events in --- */
/* notes from the main loop and the 1 kHz tick come through a queue; the render takes them at the start of a block */
enum { EV_ON, EV_OFF, EV_SUS, EV_ALLOFF, EV_PANIC, EV_PRESS, EV_PRESS_ALL };
typedef struct {
    uint8_t type;
    int8_t note;
    int16_t id;
    float vel, gain;
} ev_t;
#ifdef P67_PLUGIN                        /* (a host may send hundreds at one sample: every note off of a panic, a
                                          * chord with its pressure and controllers; none may be lost) */
#define NEV 2048u
#else
#define NEV 64u
#endif
static ev_t evq[NEV];
static volatile uint32_t ev_w, ev_r;

static void ev_put2(int type, int id, int note, float vel, float gain)
{
    uint32_t f = plat_lock();
    if (ev_w - ev_r < NEV) {
        ev_t *e = &evq[ev_w % NEV];
        e->type = (uint8_t)type;
        e->id = (int16_t)id;
        e->note = (int8_t)note;
        e->vel = vel;
        e->gain = gain;
        ev_w++;
    }
    plat_unlock(f);
}
static void ev_put(int type, int id, int note, float vel) { ev_put2(type, id, note, vel, 1.0f); }

/* ------------------------------------------------------------- the patch --- */
static float env_seconds(int v) { return 0.001f * fm_exp2f((float)v * (14.2877f / 127.0f)); }   /* 1 ms .. 20 s */
static float cutoff_oct(float v) { return 4.321928f + v * (9.965784f / 255.0f); }   /* log2: 20 Hz .. 20 kHz, 0..255 */
static float sq_sign(float x) { return x * fm_fabsf(x); }

#ifdef P67_PLUGIN
static int hp_extra_set;
static float hp_extra_vel, hp_extra_kt, fenv_extra_vel;
#endif
void eng_patch(const uint8_t *p)
{
    int i;
    float a, s;
    P.o1_semi = (float)p[P_O1_FREQ] - 24.0f;
    P.o2_semi = (float)p[P_O2_FREQ] - 24.0f + bip(p[P_O2_FINE]) * 0.5f;
    P.o1_shape = u7(p[P_O1_SHAPE]);
    P.o2_shape = u7(p[P_O2_SHAPE]);
    P.o1_pw = 0.5f + 0.45f * u7(p[P_O1_PW]);
    P.o2_pw = 0.5f + 0.45f * u7(p[P_O2_PW]);
    P.mix1 = u7(p[P_MIX_O1]);
    P.mix2 = u7(p[P_MIX_O2]);
    P.sub = u7(p[P_O1_SUB]);
    P.noise = u7(p[P_NOISE]) * u7(p[P_NOISE]);
    P.o2_mode = p[P_O2_MODE];
    P.cut_oct = cutoff_oct((float)p[P_CUTOFF]);
    a = u7(p[P_RESO]);
    P.res_k = 4.25f * a * (0.6f + 0.4f * a);        /* self-oscillation from about 115 */
    P.env_oct = sq_sign(bip(p[P_ENV_AMT])) * 9.0f;
    P.hp_oct = 3.3219f + (float)p[P_HPF] * (9.2288f / 127.0f);   /* 10 Hz (out of the way) .. 6 kHz */
    P.hp_k = 2.0f - 1.85f * u7(p[P_HP_RESO]);
    P.keytrk = p[P_KEYTRK] == KT_FULL ? 1.0f : p[P_KEYTRK] == KT_HALF ? 0.5f : 0.0f;
    P.fvel = u7(p[P_FVEL]);
#ifdef P67_PLUGIN
    P.hp_keytrk = hp_extra_set ? hp_extra_kt : P.keytrk;   /* the plugin's HIGH-PASS VELOCITY and KEYBOARD (else as the LP's) */
    P.hp_fvel = hp_extra_set ? hp_extra_vel : P.fvel;
    P.fenv_vel = hp_extra_set ? fenv_extra_vel : 0.0f;
#else
    P.fenv_vel = 0.0f;
    P.hp_keytrk = P.keytrk;
    P.hp_fvel = P.fvel;
#endif
    P.hp_env_oct = sq_sign(bip(p[P_HP_ENV])) * 7.0f;
    P.fs = u7(p[P_FS]);
    P.as = u7(p[P_AS]);
#ifdef P67_PLUGIN
    /* LFO 1 as the Prophet-6's, whose digital LFO reaches into the audio range: as before up to 112 (14 Hz), then
     * steeper, 280 Hz at 126 and 350 Hz at 127; on S+H at 127 it turns to noise (lfo_block) */
    P.lfo_hz[0] = p[P_LFO_RATE] <= 112 ? 0.05f * fm_exp2f((float)p[P_LFO_RATE] * (9.2288f / 127.0f))
                                       : 14.09f * fm_exp2f((float)(p[P_LFO_RATE] - 112) * 0.308f);
    P.lfo_noise = p[P_LFO_SHAPE] == LS_SH && p[P_LFO_RATE] >= 127 && !p[P_L1_SYNC];
#else
    P.lfo_hz[0] = 0.05f * fm_exp2f((float)p[P_LFO_RATE] * (9.2288f / 127.0f));   /* 0.05 .. 30 Hz */
    P.lfo_noise = 0;
#endif
    P.lfo_amt[0] = u7(p[P_LFO_AMT]) * u7(p[P_LFO_AMT]);
    P.lfo_shape[0] = p[P_LFO_SHAPE];
    P.lfo_dest[0] = p[P_LFO_DEST];
    P.lfo_hz[1] = 0.05f * fm_exp2f((float)p[P_L2_RATE] * (9.2288f / 127.0f));
    P.lfo_amt[1] = u7(p[P_L2_AMT]) * u7(p[P_L2_AMT]);
    P.lfo_shape[1] = p[P_L2_SHAPE];
    P.lfo_dest[1] = p[P_L2_DEST];
    P.lfo_sync[0] = p[P_L1_SYNC];
    P.lfo_sync[1] = p[P_L2_SYNC];
    P.lfo_retrig[0] = p[P_L1_RETRIG];
    P.lfo_retrig[1] = p[P_L2_RETRIG];
    P.pm_fenv = sq_sign(bip(p[P_PM_FENV]));
    P.pm_osc2 = sq_sign(bip(p[P_PM_OSC2]));
    P.pm_dosc = p[P_PM_DOSC];
    P.pm_dfilt = p[P_PM_DFILT];
    P.glide_k = p[P_GLIDE] ? 1.0f - fm_expf(-(float)CH / (env_seconds(p[P_GLIDE] / 2 + 20) * 0.25f * SR)) : 1.0f;
    P.avel = u7(p[P_AVEL]);
    P.bend = (float)p[P_BEND];
    if (p[P_UNISON] != P.unison && notes_n)
        ev_put(EV_ALLOFF, 0, 0, 0.0f);   /* UNISON switched: the notes held let go (their release), so none is
                                          * left with no key to end it */
    P.unison = p[P_UNISON];
    P.at_dest = p[P_AT_DEST] < NAT ? p[P_AT_DEST] : AT_OFF;
    P.at_amt = bip(p[P_AT_AMT]);
    P.at_curve = fm_exp2f(-bip(p[P_AT_CURVE]) * 1.6f);   /* - soft (early), + hard (late): x^0.33 .. x^3 */
    /* SMOOTH: how fast the pressure reaches the sound, 3 ms .. 0.4 s (per chunk of 16 samples) */
    P.at_k = 1.0f - fm_expf(-16.0f / ((0.003f + 0.4f * u7(p[P_AT_SMOOTH]) * u7(p[P_AT_SMOOTH])) * SR));
    P.at_kr = fm_minf(P.at_k, 1.0f - fm_expf(-16.0f / (0.15f * SR)));
    s = u7(p[P_SPREAD]);
    a = u7(p[P_VINTAGE]);
    P.vint = a;                          /* VINTAGE: straight, so the middle is already clearly there */
    P.spread = s;
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &V[i];
        float pan = S_PAN[i] * s * (0.6f + 0.4f * s), th = (pan + 1.0f) * 0.7853982f;
        /* at the top: each oscillator up to 12 / 15 cents off (and a few more per note and in its drift) (a voice's two beat against each other), the filter up
         * to 0.6 octave, resonance 40 %, the envelopes' times 55 %, the level 3 dB, the waves' width and shape, how
         * hard the filter's input is driven: an old poly whose voices were each calibrated a different year */
        float tf = 1.0f + 0.55f * a * V_ATT[i], td = 1.0f + 0.50f * a * V_DEC[i], tr = 1.0f + 0.55f * a * V_REL[i];
        CV->c1 = a * 12.0f * V_T1[i] + s * S_DET[i];                     /* cents */
        CV->c2 = a * 15.0f * V_T2[i] + s * S_DET[i] * 0.8f;
        CV->ccut = a * 0.6f * V_CUT[i] + s * S_COL[i] * (1.0f / 12.0f);   /* octaves */
        CV->cres = 1.0f + 0.4f * a * V_RES[i];
        CV->camp = 1.0f + 0.35f * a * V_AMP[i];                           /* about +-3 dB */
        CV->cpw = 0.12f * a * V_PW[i];
        CV->cshape = 0.10f * a * V_SHP[i];
        CV->cpw2 = 0.10f * a * V_PW[NVOICE - 1 - i];
        CV->cshape2 = 0.08f * a * V_SHP[NVOICE - 1 - i];
        CV->cdrive = 1.4f * (1.0f + 0.45f * a * V_DEC[i]);
        /* the envelopes: an RC's curve. Attack aims past the top (1.3) and stops at 1; decay and release fall to
         * where they go; the times are where a decay is down 40 dB */
        v->ka_f = k_of(env_seconds(p[P_FA]) * tf * 0.6824f);
        v->kd_f = k_of(env_seconds(p[P_FD]) * td * 0.2171f);
        v->kr_f = k_of(env_seconds(p[P_FR]) * tr * 0.2171f);
        v->ka_a = k_of(env_seconds(p[P_AA]) * tf * 0.6824f);
        v->kd_a = k_of(env_seconds(p[P_AD]) * td * 0.2171f);
        v->kr_a = k_of(env_seconds(p[P_AR]) * tr * 0.2171f);
        CV->gl = fm_cosf(th);
        CV->gr = fm_sinf(th);
        CV->far = pan < 0.0f ? 1u : 0u;   /* the side away from it hears it a little later */
#ifdef P67_PLUGIN
        CV->hdf = fm_fabsf(pan) * 16.0f * s * SRS;   /* (up to 0.36 ms, at any rate; gliding: no click) */
#else
        v->hd = (uint8_t)(fm_fabsf(pan) * 16.0f * s);
#endif
    }
    fx_patch(p);
}

/* ------------------------------------------------------------- the voices --- */
static void env_gate(env_t *e) { e->st = ES_ATT; }   /* from where it is, as an analogue envelope */
static void env_rel(env_t *e)
{
    if (e->st != ES_IDLE)
        e->st = ES_REL;
}

static int any_gate(void)
{
    int i;
    for (i = 0; i < NVOICE; i++)
        if (V[i].gate)
            return 1;
    return 0;
}

static void voice_start(voice_t *v, int id, int note, float vel, int retrig, float gain)
{
    if (retrig && !any_gate())
        lfo_reset = 1;                   /* (LFO RETRIG: a fresh phrase) */
    v->id = (int16_t)id;
    v->note = (int8_t)note;
    v->target = (float)note;
    if (P.glide_k >= 1.0f || v->pitch == 0.0f)   /* (with GLIDE each voice slides from its last note) */
        v->pitch = v->target;
    v->vel = vel;
    v->gain = gain;
    if (retrig)
        v->ptgt = 0.0f;                  /* a new key starts unpressed: what is left glides away, never jumps */
    v->n1 = rnd_f(&v->seed) * 5.0f * P.vint;   /* VINTAGE: this keystroke's own few cents and colour */
    v->n2 = rnd_f(&v->seed) * 5.0f * P.vint;
    v->ncut = rnd_f(&v->seed) * 0.12f * P.vint;
    v->gate = 1;
    v->sus = 0;
    if (retrig) {
        env_gate(&v->fe);
        env_gate(&v->ae);
    }
    v->age = ++age_n;
    notes_n++;
}

/* voices_render runs from RAM, and reaches voice_start in the flash through this */
#ifdef P67_PLUGIN
#define voice_start_far voice_start
#else
static void (*volatile voice_start_far)(voice_t *, int, int, float, int, float) = voice_start;
#endif

static int voice_find(int id)
{
    int i;
    for (i = 0; i < NVOICE; i++)
        if (V[i].id == id && (V[i].gate || V[i].sus || V[i].pend))
            return i;
    return -1;
}

static void unison_play(int retrig)
{
    int i;
    for (i = 0; i < NVOICE; i++)
        voice_start(&V[i], u_id[u_n - 1], u_note[u_n - 1], u_vel[u_n - 1], retrig, on_gain);
}


void voice_on(int id, int note, float vel)
{
    int i, best = -1;
    if (note < 0 || note > 127)
        return;
    if (P.unison) {
        int held = 0;
        for (i = 0; i < u_n; i++)
            if (u_id[i] == id)
                break;
        if (i < u_n) {                   /* the same note again: to the top */
            for (; i + 1 < u_n; i++) {
                u_id[i] = u_id[i + 1];
                u_note[i] = u_note[i + 1];
                u_vel[i] = u_vel[i + 1];
            }
            u_n--;
        }
        held = u_n > 0;
        if (u_n == USTACK) {
            for (i = 0; i + 1 < u_n; i++) {
                u_id[i] = u_id[i + 1];
                u_note[i] = u_note[i + 1];
                u_vel[i] = u_vel[i + 1];
            }
            u_n--;
        }
        u_id[u_n] = (int16_t)id;
        u_note[u_n] = (int8_t)note;
        u_vel[u_n] = vel;
        u_n++;
        unison_play(!held);              /* legato while another key is held */
        return;
    }
    i = voice_find(id);
    if (i >= 0) {                        /* struck again */
        voice_start(&V[i], id, note, vel, 1, on_gain);
        return;
    }
    /* the same note still ringing out: that voice again (its own character, its pressure, its filter and phase go on
     * from where they are: no jump, no click) */
    for (i = 0; i < NVOICE; i++)
        if (V[i].note == note && !V[i].gate && !V[i].sus && !V[i].pend && V[i].fade >= 1.0f
            && V[i].ae.st != ES_IDLE) {
            rr = (uint32_t)(i + 1) % NVOICE;
            voice_start(&V[i], id, note, vel, 1, on_gain);
            return;
        }
    /* a free voice, taking them in turn (so each note meets another voice's character, and a chord spreads) */
    for (i = 0; i < NVOICE; i++) {
        int k = (int)((rr + (uint32_t)i) % NVOICE);
        if (V[k].ae.st == ES_IDLE && !V[k].pend) {
            best = k;
            break;
        }
    }
    if (best < 0) {                      /* none: the quietest of those let go */
        float lo = 2.0f;
        for (i = 0; i < NVOICE; i++)
            if (!V[i].gate && !V[i].sus && !V[i].pend && V[i].ae.v < lo) {
                lo = V[i].ae.v;
                best = i;
            }
    }
    if (best < 0) {                      /* none let go: the oldest note */
        uint32_t old = 0xFFFFFFFFu;
        for (i = 0; i < NVOICE; i++)
            if (!V[i].pend && V[i].age < old) {
                old = V[i].age;
                best = i;
            }
    }
    if (best < 0)
        return;
    rr = (uint32_t)(best + 1) % NVOICE;
    if (V[best].ae.st != ES_IDLE && V[best].ae.v > ENV_END) {   /* still sounding: out smoothly, then the new note */
        V[best].pend = 1;
        if (V[best].fade >= 1.0f)
            V[best].fade = 0.99999f;
        V[best].pid = (int16_t)id;
        V[best].pnote = (int8_t)note;
        V[best].pvel = vel;
        V[best].pgain = on_gain;
        V[best].gate = 0;
        V[best].id = -1;
        steals_n++;
        return;
    }
    voice_start(&V[best], id, note, vel, 1, on_gain);
}

void voice_off(int id)
{
    int i;
    if (P.unison) {
        for (i = 0; i < u_n; i++)
            if (u_id[i] == id)
                break;
        if (i == u_n)
            return;
        for (; i + 1 < u_n; i++) {
            u_id[i] = u_id[i + 1];
            u_note[i] = u_note[i + 1];
            u_vel[i] = u_vel[i + 1];
        }
        u_n--;
        if (u_n > 0) {                   /* back to the note still held */
            for (i = 0; i < NVOICE; i++) {
                V[i].id = u_id[u_n - 1];
                V[i].note = u_note[u_n - 1];
                V[i].target = (float)u_note[u_n - 1];
            }
            return;
        }
        for (i = 0; i < NVOICE; i++) {
            V[i].gate = 0;
            if (sustain)
                V[i].sus = 1;
            else {
                env_rel(&V[i].fe);
                env_rel(&V[i].ae);
            }
        }
        return;
    }
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &V[i];
        if (v->pend && v->pid == id) {   /* let go before it started: it never will */
            v->pend = 0;
            continue;
        }
        if (v->id != id || !v->gate)
            continue;
        v->gate = 0;
        v->ptgt = 0.0f;                  /* the pressure eases off with the release (P.at_kr) */
        if (sustain) {
            v->sus = 1;
        } else {
            env_rel(&v->fe);
            env_rel(&v->ae);
        }
    }
}

static void sus_set(int on)
{
    int i;
    sustain = (uint8_t)on;
    if (on)
        return;
    for (i = 0; i < NVOICE; i++)
        if (V[i].sus) {
            V[i].sus = 0;
            env_rel(&V[i].fe);
            env_rel(&V[i].ae);
        }
}

static void all_off(void)
{
    int i;
    u_n = 0;
    for (i = 0; i < NVOICE; i++) {
        V[i].gate = V[i].sus = V[i].pend = 0;
        env_rel(&V[i].fe);
        env_rel(&V[i].ae);
    }
}

/* --------------------------------------------------------- the oscillators --- */
/* The waveform: TRIANGLE (shape 0) - SAW (0.5) - PULSE (1), crossfaded, the pulse's DC taken out. A step of height J
 * d samples before the current sample is smoothed over the sample before (prev) and this one (cur). */
typedef struct {
    float wt, ws, wp, pw, dc;
} wave_t;

INL void wave_set(wave_t *w, float shape, float pw)
{
    shape = fm_clampf(shape, 0.0f, 1.0f);
    w->pw = fm_clampf(pw, 0.03f, 0.97f);
    if (shape < 0.5f) {
        w->ws = shape * 2.0f;
        w->wt = 1.0f - w->ws;
        w->wp = 0.0f;
    } else {
        w->wp = (shape - 0.5f) * 2.0f;
        w->ws = 1.0f - w->wp;
        w->wt = 0.0f;
    }
    w->dc = w->wp * (2.0f * w->pw - 1.0f);
}

INL float wave_at(const wave_t *w, float ph)
{
    float y = w->ws * (2.0f * ph - 1.0f) - w->dc;
    if (w->wt > 0.0f)
        y += w->wt * (1.0f - 4.0f * fm_fabsf(ph - 0.5f));
    if (w->wp > 0.0f)
        y += ph < w->pw ? w->wp : -w->wp;
    return y;
}

/* the filter's input stage: a soft clip without a division (a cubic, flat at +-1.5 where it reaches 1) */
INL float sat(float x)
{
    x = fm_clampf(x, -1.5f, 1.5f);
    return x - 0.148148f * x * x * x;
}

#define BLEP(J, d)                                                                                                      \
    do {                                                                                                                \
        float d_ = (d), e_ = 1.0f - d_;                                                                                 \
        pc += (J) * d_ * d_ * 0.5f;                                                                                     \
        cc -= (J) * e_ * e_ * 0.5f;                                                                                     \
    } while (0)

#ifdef P67_PLUGIN
static const float *bl_tab;              /* the BLEP (bl_tab) and the BLAMP's residual (bl_tab + BL_N) */

/* a step of height J (bl_tab, the step itself taken out) or a change of slope J (per sample: the ramp's residual,
 * bl_tab + BL_N), d samples before this one: into the 16 samples around it */
INL void bl_add(osc_t *o, float J, float d, int ramp)
{
    const float *tb = bl_tab + (ramp ? BL_N : 0);
    float di = d * (float)BL_OS, fr;
    int i0 = (int)di, j;
    if (i0 >= BL_OS)
        i0 = BL_OS - 1;
    fr = di - (float)i0;
    for (j = -BL_H; j < BL_H; j++) {
        const float *q = tb + (j + BL_H) * BL_OS + i0;
        float b = q[0] + fr * (q[1] - q[0]);
        if (!ramp && j >= 0)
            b -= 1.0f;
        o->ring[(o->rp + (uint32_t)j) & (ORING - 1u)] += J * b;
    }
}

/* the waveform's slope (per sample) at a phase: the triangle's part (the saw's is the same everywhere) */
INL float tri_slope(const wave_t *w, float ph, float inc) { return w->wt * (ph < 0.5f ? 4.0f : -4.0f) * inc; }

INL float osc_tick(osc_t *o, const wave_t *w, float inc, float reset, int *wrapped, float *wd)
{
    float old = o->ph, ph = old + inc, out;
    uint32_t r;
    *wrapped = 0;
    if (reset >= 0.0f) {                 /* synced: from where it got to straight back to the start */
        float pr = old + inc * (1.0f - reset);
        if (pr >= 1.0f)
            pr -= 1.0f;
        bl_add(o, wave_at(w, 0.0f) - wave_at(w, pr), reset, 0);
        if (w->wt > 0.0f)
            bl_add(o, tri_slope(w, 0.0f, inc) - tri_slope(w, pr, inc), reset, 1);
        ph = inc * reset;
        *wrapped = 1;
        *wd = reset;
    } else {
        if (ph >= 1.0f) {
            ph -= 1.0f;
            *wrapped = 1;
            *wd = ph / inc;
            if (*wd > 0.999f)
                *wd = 0.999f;
            bl_add(o, 2.0f * w->wp - 2.0f * w->ws, *wd, 0);   /* the saw falls, the pulse rises */
            if (w->wt > 0.0f)
                bl_add(o, 8.0f * w->wt * inc, *wd, 1);        /* the triangle's corner at the bottom */
            if (w->wp > 0.0f && ph >= w->pw)                    /* and the pulse already fell again (very short) */
                bl_add(o, -2.0f * w->wp, fm_minf((ph - w->pw) / inc, 0.999f), 0);
        } else {
            if (w->wp > 0.0f && old < w->pw && ph >= w->pw)
                bl_add(o, -2.0f * w->wp, fm_minf((ph - w->pw) / inc, 0.999f), 0);
            if (w->wt > 0.0f && old < 0.5f && ph >= 0.5f)    /* its corner at the top */
                bl_add(o, -8.0f * w->wt * inc, fm_minf((ph - 0.5f) / inc, 0.999f), 1);
        }
    }
    o->ph = ph;
    r = o->rp & (ORING - 1u);
    o->ring[r] += wave_at(w, ph);
    r = (o->rp - (uint32_t)BL_H) & (ORING - 1u);
    out = o->ring[r];
    o->ring[r] = 0.0f;
    o->rp++;
    return out;
}
#else
/* one sample of an oscillator. reset: hard sync, d samples ago (-1: none). wrapped: set when its phase came round
 * (and how long ago, in *wd) */
INL float osc_tick(osc_t *o, const wave_t *w, float inc, float reset, int *wrapped, float *wd)
{
    float old = o->ph, ph = old + inc, pc = 0.0f, cc = 0.0f, cur, out;
    *wrapped = 0;
    if (reset >= 0.0f) {                 /* synced: from where it got to straight back to the start */
        float pr = old + inc * (1.0f - reset);
        if (pr >= 1.0f)
            pr -= 1.0f;
        BLEP(wave_at(w, 0.0f) - wave_at(w, pr), reset);
        ph = inc * reset;
        *wrapped = 1;
        *wd = reset;
    } else {
        if (ph >= 1.0f) {
            ph -= 1.0f;
            *wrapped = 1;
            *wd = ph / inc;
            if (*wd > 0.999f)
                *wd = 0.999f;
            BLEP(2.0f * w->wp - 2.0f * w->ws, *wd);     /* the saw falls, the pulse rises */
            if (w->wp > 0.0f && ph >= w->pw)            /* and the pulse already fell again (very short) */
                BLEP(-2.0f * w->wp, fm_minf((ph - w->pw) / inc, 0.999f));
        } else if (w->wp > 0.0f && old < w->pw && ph >= w->pw) {
            BLEP(-2.0f * w->wp, fm_minf((ph - w->pw) / inc, 0.999f));
        }
    }
    o->ph = ph;
    cur = wave_at(w, ph);
    out = o->prev + pc;
    o->prev = cur + cc;
    return out;
}
#endif

/* ------------------------------------------------------------- the render --- */
static float vb_l[2][NBLK], vb_r[2][NBLK];   /* the voices of each core, summed */
static uint32_t blk_n;
static uint8_t blk_fm, blk_pms, blk_pmp, blk_pml, blk_pmh;   /* poly mod by oscillator 2, per sample, to ... */

INL float noise_seed_f(uint32_t *s) { return rnd_f(s); }

/* The envelopes, per sample: v += (target - v) * k, capped at 1. Their stages change per chunk (env_advance): the
 * attack aims past the top (1.3) and turns to decay at 1, the release ends at -70 dB (where nothing is heard: below
 * that a voice only costs time) */
INL void env_targets(const voice_t *v, float *ft, float *fk, float *at, float *ak)
{
    switch (v->fe.st) {
    case ES_ATT: *ft = 1.3f; *fk = v->ka_f; break;
    case ES_DEC: *ft = P.fs; *fk = v->kd_f; break;
    case ES_REL: *ft = 0.0f; *fk = v->kr_f; break;
    default: *ft = 0.0f; *fk = 0.0f;
    }
    switch (v->ae.st) {
    case ES_ATT: *at = 1.3f; *ak = v->ka_a; break;
    case ES_DEC: *at = P.as; *ak = v->kd_a; break;
    case ES_REL: *at = 0.0f; *ak = v->kr_a; break;
    default: *at = 0.0f; *ak = 0.0f;
    }
}

INL void env_advance(voice_t *v)
{
    if (v->fe.st == ES_ATT && v->fe.v >= 0.9999f)
        v->fe.st = ES_DEC;
    if (v->ae.st == ES_ATT && v->ae.v >= 0.9999f)
        v->ae.st = ES_DEC;
    if (v->ae.st == ES_REL && v->ae.v < ENV_END) {
        v->ae.v = v->fe.v = 0.0f;
        v->ae.st = v->fe.st = ES_IDLE;
    }
}

#if defined P67_PLUGIN && defined LADDER_OTA
/* a transconductor's curve: L tanh(x / L), L the level it saturates at (a Padé tanh, flat at +-3: no division by
 * anything near nought, slope 1 at the middle) */
#ifndef OTA_L
#define OTA_L 1.2f
#endif
INL float ota(float x)
{
    float t = x * (1.0f / OTA_L), t2;
    if (t > 3.0f)
        return OTA_L;
    if (t < -3.0f)
        return -OTA_L;
    t2 = t * t;
    return OTA_L * t * (27.0f + t2) / (27.0f + 9.0f * t2);
}
#endif

/* the low-pass's loop, one tick: the feedback solved without a delay, the input saturating, four one-poles */
INL float ladder(voice_t *v, float x, float qG, float qig, float qG4, float kres, float drv, float idrv)
{
    float G2 = qG * qG, S, u, rs, y1, y2, y3, y4;
    S = (G2 * qG * v->s1 + G2 * v->s2 + qG * v->s3 + v->s4) * qig;
    u = (x - kres * S) * qG4;
    u = sat(u * drv) * idrv;
#if defined P67_PLUGIN && defined LADDER_OTA
    /* the plugin: each stage a transconductor, as the Prophet-6's (the Prophet-5's design): what drives it is its
     * input less its output, through a soft curve (ota), so a stage pushed hard slews instead of following, and the
     * resonance rounds off as it rises: the filter's cream. At the levels a voice plays at it is very nearly linear */
    rs = ota(u - v->s1) * qG;
    y1 = rs + v->s1;
    v->s1 = y1 + rs;
    rs = ota(y1 - v->s2) * qG;
    y2 = rs + v->s2;
    v->s2 = y2 + rs;
    rs = ota(y2 - v->s3) * qG;
    y3 = rs + v->s3;
    v->s3 = y3 + rs;
    rs = ota(y3 - v->s4) * qG;
    y4 = rs + v->s4;
    v->s4 = y4 + rs;
#else
    rs = (u - v->s1) * qG;
    y1 = rs + v->s1;
    v->s1 = y1 + rs;
    rs = (y1 - v->s2) * qG;
    y2 = rs + v->s2;
    v->s2 = y2 + rs;
    rs = (y2 - v->s3) * qG;
    y3 = rs + v->s3;
    v->s3 = y3 + rs;
    rs = (y3 - v->s4) * qG;
    y4 = rs + v->s4;
    v->s4 = y4 + rs;
#endif
    return y4;
}

#ifdef P67_PLUGIN
/* the low-pass's 2x: a polyphase IIR half-band (two paths of four first-order allpasses, the even coefficients one
 * path, the odd the other; elliptic, after de Soras' design: -91 dB from 24.7 kHz at 88.2, flat to 19.4 kHz). Up: the
 * two paths give the pair; down: half of the even path's sample and the odd path's last one */
static const float HB_A0[4] = {0.0472898531f, 0.3385179585f, 0.6552730688f, 0.8745126132f};
static const float HB_A1[4] = {0.1727806110f, 0.5069776792f, 0.7764899192f, 0.9589457184f};
INL float hb_ap4(float x, float *t, const float *a)
{
    float y0 = a[0] * (x - t[1]) + t[0], y1 = a[1] * (y0 - t[2]) + t[1], y2 = a[2] * (y1 - t[3]) + t[2],
          y3 = a[3] * (y2 - t[4]) + t[3];
    t[0] = x;
    t[1] = y0;
    t[2] = y1;
    t[3] = y2;
    t[4] = y3;
    return y3;
}
#endif

HOT static void voices_render(const uint8_t *list, int nv, float *restrict L, float *restrict R)
{
    uint32_t n = blk_n, c, s, k;
    int j;
    /* what is tuned per sample or per chunk at 44.1 kHz, at the rate now: the steal's fade (4.5 ms), the drift's
     * glide, the noise's level in the audible band (white noise at a higher rate spreads its power wider) */
    const float steal_d = 1.0f / (STEAL * SRS), drift_k = k_sr(0.0016f);
    const float noise_g = SRS == 1.0f ? 1.0f : fm_sqrtf(SRS);
#ifdef P67_PLUGIN
    const int lp_os = SR < 70000.0f && !q_eco ? 2 : 1;   /* the low-pass's rate: twice, below 70 kHz (unless ECO) */
#else
    const int lp_os = 1;
#endif
    const float w_lp = 3.1415927f / (SR * (float)lp_os);
#ifdef P67_PLUGIN
    const float kg_v = 1.0f - fm_expf(-1.0f / (0.001f * SR));   /* (the levels' glide: 2 x 1 ms) */
#endif
    for (s = 0; s < n; s++)
        L[s] = R[s] = 0.0f;
    for (j = 0; j < nv; j++) {
        int i = list[j];
        voice_t *v = &V[i];
        if (v->ae.st == ES_IDLE && !v->pend)
            continue;
        for (c = 0; c * CH < n; c++) {
            const float *lm = blk_mod[c];
            float lmv[NLD], pp, pmfe, pmo2, at_cut = 0.0f, at_amp = 1.0f, at_det = 0.0f, at_drv = 1.0f, at_pm = 1.0f;
            float p, inc1, inc2, sh1, pw1, cut, hp, G, gh, kh, a1, a2, kres, comp, pm_s, pm_p, pm_l, pm_h, pm_f;
            float vel_f, famt, hamt, fen, ug, ig, G4, drv, idrv, dG, dig, dG4, dk, dc, dgh, dkh, da1, da2, ft, fk, at, ak;
            int hp_on, pm_sp;
            float m1, m2, msub, mnoise, sh2b, pw2b, at_f1, at_f2, at_pw, at_lp, at_hp;
            const float *lsp[NLD];
            wave_t w1, w2;
            /* a fast LFO 1 (per sample: blk_ls), and AFTERTOUCH's share of it to its destination */
            uint32_t lfd = blk_lfd;
            int at_l1 = P.at_dest == AT_LFO && lfo_fast[0] && P.lfo_dest[0] < NLD && P.lfo_dest[0] != LD_AMP;
            int lowf = P.o2_mode == O2M_LOW || P.o2_mode == O2M_LOWFIX;
            int sync = P.o2_mode == O2M_SYNC;
            int o2_used = (PQ.mix2 > 0.0f && !lowf) || sync || blk_fm || blk_pms || blk_pmp || blk_pml || blk_pmh;
#ifdef P67_PLUGIN
            (void)o2_used;
#endif
            /* VINTAGE's drift: a slow wander, a new heading now and then */
            if (P.vint > 0.0f) {
                if (v->dr_n == 0) {
                    v->dt1 = rnd_f(&v->seed) * 10.0f * P.vint;
                    v->dt2 = rnd_f(&v->seed) * 10.0f * P.vint;
                    v->dr_n = (uint16_t)((float)(1100u + (rnd_u(&v->seed) & 2047u)) * SRS);   /* 0.2 .. 0.57 s */
                }
                v->dr_n--;
                v->dr1 += (v->dt1 - v->dr1) * drift_k;
                v->dr2 += (v->dt2 - v->dr2) * drift_k;
            } else {
                v->dr1 = v->dr2 = 0.0f;
            }
            v->pitch += (v->target - v->pitch) * P.glide_k;
            p = v->pitch + PQ_BEND * P.bend;
            /* AFTERTOUCH: the key's pressure (or the channel's), smoothed, to where it goes */
            v->press += (ri_maxf(v->ptgt, chan_press) - v->press) * (v->gate || v->sus ? P.at_k : P.at_kr);
            pp = v->press * PQ.at_amt;
            switch (P.at_dest) {
            case AT_FILTER: at_cut = 4.0f * pp; break;
            case AT_SWELL: at_amp = ri_maxf(0.0f, 1.0f + 1.5f * pp); at_cut = 0.7f * pp; break;
            case AT_POWER:               /* (OFFWORLD's: brighter, louder, wider, driven) */
                at_cut = 1.5f * pp;
                at_amp = ri_maxf(0.0f, 1.0f + 0.8f * pp);
                at_det = 12.0f * pp;
                at_drv = ri_maxf(0.3f, 1.0f + 1.6f * pp);
                break;
            case AT_VIBRATO: p += 0.45f * pp * blk_vib[c]; break;
            case AT_PITCH: p += 2.0f * pp; break;
            case AT_POLYMOD: at_pm = ri_maxf(0.0f, 1.0f + 2.0f * pp); break;
            default: break;
            }
            pmfe = PQ.pm_fenv * at_pm;   /* (AFTERTOUCH to POLY MOD: both its amounts) */
            pmo2 = PQ.pm_osc2 * at_pm;
            if (P.at_dest == AT_LFO) {   /* more (or less) of LFO 1, to its destination */
                int d;
                for (d = 0; d < NLD; d++)
                    lmv[d] = lm[d];
                if (P.lfo_dest[0] < NLD && P.lfo_dest[0] != LD_AMP)
                    lmv[P.lfo_dest[0]] += pp * blk_l1[c];
                lm = lmv;
            }
            if (P.unison)
                p += U_DET[i] * (0.08f + 0.22f * P.spread);
            inc1 = hz_of(p + P.o1_semi + (v->c1 + v->n1 + v->dr1) * 0.01f + (lm[LD_FREQ] + lm[LD_FREQ1]) * 12.0f)
                   * (1.0f / SR);
            {
                float p2 = P.o2_semi + (v->c2 + v->n2 + v->dr2 + at_det) * 0.01f + (lm[LD_FREQ] + lm[LD_FREQ2]) * 12.0f;
                if (lowf)                /* LOW FREQ: an LFO's rates (1 Hz at middle C), with the keys or without */
                    inc2 = ex2((p2 + (P.o2_mode == O2M_LOW ? p - 60.0f : 0.0f)) * (1.0f / 12.0f)) * (1.0f / SR);
                else
                    inc2 = hz_of(p + p2) * (1.0f / SR);
            }
            /* poly mod by the filter envelope (this chunk) */
            fen = v->fe.v;
            sh1 = PQ.o1_shape + v->cshape;
            pw1 = PQ.o1_pw + v->cpw;
            if (P.pm_dosc & PMO_FREQ)
                inc1 *= ex2(pmfe * fen * 3.0f);   /* up to 3 octaves */
            if (P.pm_dosc & PMO_SHAPE)
                sh1 += pmfe * fen;
            if (P.pm_dosc & PMO_PW)
                pw1 += pmfe * fen * 0.45f;
            pw1 += lm[LD_PW] * 0.42f;
            inc1 = fm_minf(inc1, 0.45f);
            inc2 = fm_minf(inc2, 0.45f);
            /* the filters this chunk */
            vel_f = 1.0f - PQ.fvel + PQ.fvel * v->vel;
            vel_f *= 1.0f - PQ.fenv_vel + PQ.fenv_vel * v->vel;   /* (FILTER ENVELOPE's VELOCITY, the plugin's: both filters) */
            famt = blk_env[c] * vel_f;
            hamt = PQ.hp_env_oct * (1.0f - PQ.hp_fvel + PQ.hp_fvel * v->vel) * (1.0f - PQ.fenv_vel + PQ.fenv_vel * v->vel);
            cut = blk_cut[c] + v->ccut + v->ncut + famt * fen + P.keytrk * (p - 60.0f) * (1.0f / 12.0f);
            hp = blk_hp[c] + hamt * fen + P.hp_keytrk * (p - 60.0f) * (1.0f / 12.0f);
            if (P.pm_dfilt == PMF_LPF || P.pm_dfilt == PMF_BOTH)
                cut += pmfe * fen * 7.0f;
            if (P.pm_dfilt == PMF_HPF || P.pm_dfilt == PMF_BOTH)
                hp += pmfe * fen * 5.0f;
            cut += lm[LD_LPF] * 4.0f + at_cut;
            hp += lm[LD_HPF] * 3.0f;
            /* this chunk's coefficients (where they are to be at its end) ... */
            {
                float tk = blk_res[c] * v->cres, tg, tG, tig, tG4, tgh, tkh, ta1, tc, inv = 1.0f / (float)CH;
                tc = 1.0f + tk * 0.32f;   /* some of the bass the resonance takes, back (not all: a Prophet's) */
#if defined P67_PLUGIN && defined LADDER_OTA
                /* (the stages' saturation holds a self-oscillating filter a little low, as on an uncalibrated
                 * analogue one: calibrated, as the Prophet-6 is, from where it starts to sing to the top) */
                cut += (0.0135f + 1.26e-6f * fm_minf(ex2(cut), 17500.0f)) * fm_clampf((tk - 3.7f) * (1.0f / 0.55f), 0.0f, 1.0f);
#endif
                tg = tan_w(w_lp * fm_clampf(ex2(cut), 12.0f, 17500.0f));
                tig = 1.0f / (1.0f + tg);
                tG = tg * tig;
                tG4 = 1.0f / (1.0f + tk * tG * tG * tG * tG);
                tgh = tan_w(3.1415927f / SR * fm_clampf(ex2(hp), 8.0f, 12000.0f));
                tkh = blk_hpk[c];
                ta1 = 1.0f / (1.0f + tgh * (tgh + tkh));
                if (!v->cinit) {         /* a voice starting from silence: from here */
                    v->lG = tG;
                    v->lig = tig;
                    v->lG4 = tG4;
                    v->lk = tk;
                    v->lc = tc;
                    v->hg = tgh;
                    v->hk = tkh;
                    v->ha1 = ta1;
                    v->ha2 = tgh * ta1;
                    v->cinit = 1;
                }
                /* ... and the steps from where they are, one a sample */
                G = v->lG;
                ig = v->lig;
                G4 = v->lG4;
                kres = v->lk;
                comp = v->lc;
                gh = v->hg;
                kh = v->hk;
                a1 = v->ha1;
                a2 = v->ha2;
                dG = (tG - G) * inv;
                dig = (tig - ig) * inv;
                dG4 = (tG4 - G4) * inv;
                dk = (tk - kres) * inv;
                dc = (tc - comp) * inv;
                dgh = (tgh - gh) * inv;
                dkh = (tkh - kh) * inv;
                da1 = (ta1 - a1) * inv;
                da2 = (tgh * ta1 - a2) * inv;
                v->lG = tG;
                v->lig = tig;
                v->lG4 = tG4;
                v->lk = tk;
                v->lc = tc;
                v->hg = tgh;
                v->hk = tkh;
                v->ha1 = ta1;
                v->ha2 = tgh * ta1;
            }
            /* poly mod by oscillator 2, per sample */
            pm_f = blk_fm ? pmo2 * 2.0f : 0.0f;           /* octaves of FM */
            pm_s = blk_pms ? pmo2 : 0.0f;
            pm_p = blk_pmp ? pmo2 * 0.45f : 0.0f;
            pm_l = blk_pml ? pmo2 * 5.0f : 0.0f;
            pm_h = blk_pmh ? pmo2 * 4.0f : 0.0f;
            if (at_l1)
                lfd |= 1u << P.lfo_dest[0];
            {                            /* (each destination's fast LFO: its line, or nought) */
                int d;
                for (d = 0; d < NLD; d++)
                    lsp[d] = (blk_lfd & (1u << d)) && d != LD_AMP ? blk_ls[d] : blk_zero;
                at_f1 = at_l1 && (P.lfo_dest[0] == LD_FREQ || P.lfo_dest[0] == LD_FREQ1) ? 1.0f : 0.0f;
                at_f2 = at_l1 && (P.lfo_dest[0] == LD_FREQ || P.lfo_dest[0] == LD_FREQ2) ? 1.0f : 0.0f;
                at_pw = at_l1 && P.lfo_dest[0] == LD_PW ? 1.0f : 0.0f;
                at_lp = at_l1 && P.lfo_dest[0] == LD_LPF ? 1.0f : 0.0f;
                at_hp = at_l1 && P.lfo_dest[0] == LD_HPF ? 1.0f : 0.0f;
            }
            sh2b = PQ.o2_shape + v->cshape2;
            pw2b = PQ.o2_pw + v->cpw2 + lm[LD_PW] * 0.42f;
            wave_set(&w2, sh2b, pw2b);
            wave_set(&w1, sh1, pw1);
            drv = v->cdrive * at_drv;
            idrv = 1.0f / drv;
            ug = v->camp * blk_trem[c] * at_amp * v->gain
                 * (1.0f - PQ.avel + PQ.avel * v->vel);
            hp_on = hp > 4.6f || pm_h != 0.0f || (lfd & (1u << LD_HPF));
            pm_sp = pm_s != 0.0f || pm_p != 0.0f;
            m1 = 0.5f * PQ.mix1;
            m2 = lowf ? 0.0f : 0.5f * PQ.mix2;
            msub = 0.5f * PQ.sub;
            mnoise = 0.7f * PQ.noise * noise_g;
            env_targets(v, &ft, &fk, &at, &ak);
#ifdef P67_PLUGIN
            if (!v->gok) {
                v->g1 = m1;
                v->g2 = m2;
                v->gsub = msub;
                v->gnoise = mnoise;
                v->gout = ug;
                v->ga[0] = m1;
                v->ga[1] = m2;
                v->ga[2] = msub;
                v->ga[3] = mnoise;
                v->ga[4] = ug;
                v->gok = 1;
            }
#endif
            for (s = 0; s < CH && c * CH + s < n; s++) {
                float o1, o2, sub, x, y4, t, hv1, hv2, hv3, out, ae;
                float lf1 = 0.0f, lf2 = 0.0f, lpw = 0.0f, llp = 0.0f, lhp = 0.0f, ltr = 1.0f, i2 = inc2;
                int wr1, wr2;
                float wd1 = 0.0f, wd2 = 0.0f;
                k = c * CH + s;
                if (lfd) {               /* a fast LFO, this sample (its amounts as the chunk's: LFO 1's 12
                                          * semitones, 0.42 of PW, 4 octaves of LPF, 3 of HPF) */
                    float at1 = at_l1 ? pp * blk_l1s[k] : 0.0f;
                    lf1 = lsp[LD_FREQ][k] + lsp[LD_FREQ1][k] + at_f1 * at1;   /* octaves */
                    lf2 = lsp[LD_FREQ][k] + lsp[LD_FREQ2][k] + at_f2 * at1;
                    lpw = (lsp[LD_PW][k] + at_pw * at1) * 0.42f;
                    llp = (lsp[LD_LPF][k] + at_lp * at1) * 4.0f;
                    lhp = (lsp[LD_HPF][k] + at_hp * at1) * 3.0f;
                    if (lfd & (1u << LD_AMP))
                        ltr = blk_ltrem[k];
                    if (lf2 != 0.0f)
                        i2 = fm_minf(inc2 * ex2(lf2), 0.45f);
                    if (lpw != 0.0f)
                        wave_set(&w2, sh2b, pw2b + lpw);
                }
                G += dG;                 /* the filters' coefficients, a step a sample */
                ig += dig;
                G4 += dG4;
                kres += dk;
                comp += dc;
                gh += dgh;
                kh += dkh;
                a1 += da1;
                a2 += da2;
                /* the envelopes: one step each toward where their stage goes (set per chunk) */
                v->fe.v = fm_minf(v->fe.v + (ft - v->fe.v) * fk, 1.0f);
                v->ae.v = fm_minf(v->ae.v + (at - v->ae.v) * ak, 1.0f);
                ae = v->ae.v;
                if (v->fade < 1.0f) {    /* taken (or let go at once): out along a smooth curve, then silent */
                    v->fade -= steal_d;
                    if (v->fade <= 0.0f) {
                        v->fade = 1.0f;
                        v->fe.v = v->ae.v = 0.0f;
                        ae = 0.0f;
                        if (v->pend) {   /* the note it was taken for (in the flash: through a pointer) */
                            v->pend = 0;
                            voice_start_far(v, v->pid, v->pnote, v->pvel, 1, v->pgain);
                        } else {
                            v->fe.st = v->ae.st = ES_IDLE;
                        }
                        env_targets(v, &ft, &fk, &at, &ak);
                    } else {
                        ae *= v->fade * v->fade * (3.0f - 2.0f * v->fade);   /* (smoothstep: no corner at either end) */
                    }
                }
                /* oscillator 2 first: it is poly mod's source and the sync's master */
#ifdef P67_PLUGIN
                if (1) {                 /* (the plugin: always, so its ring never holds a stale wave) */
#else
                if (o2_used) {
#endif
                    o2 = osc_tick(&v->o2, &w2, i2, -1.0f, &wr2, &wd2);
                } else {                 /* not heard, not used: it only runs on (free-running, as a VCO) */
                    o2 = 0.0f;
                    wr2 = 0;
                    v->o2.ph += i2;
                    if (v->o2.ph >= 1.0f)
                        v->o2.ph -= 1.0f;
                }
                if (pm_sp || lpw != 0.0f)
                    wave_set(&w1, sh1 + pm_s * o2, pw1 + pm_p * o2 + lpw);
                o1 = osc_tick(&v->o1, &w1, pm_f != 0.0f || lf1 != 0.0f ? fm_minf(inc1 * ex2(pm_f * o2 + lf1), 0.45f) : inc1,
                              (sync && wr2) ? wd2 : -1.0f, &wr1, &wd1);
                if (wr1)
                    v->flip = 1.0f - v->flip;
                t = (v->o1.ph + v->flip) * 0.5f;     /* the sub-octave: a triangle an octave down */
                sub = 1.0f - 4.0f * fm_fabsf(t - 0.5f);
#ifdef P67_PLUGIN
                v->subd[v->o1.rp & 15u] = sub;       /* (as late as oscillator 1, less the one sample it always was) */
                sub = v->subd[(v->o1.rp - (uint32_t)(BL_H - 1)) & 15u];
#endif
#ifdef P67_PLUGIN
                v->ga[0] += (m1 - v->ga[0]) * kg_v;   /* (two stages: an S-curve, no corner) */
                v->ga[1] += (m2 - v->ga[1]) * kg_v;
                v->ga[2] += (msub - v->ga[2]) * kg_v;
                v->ga[3] += (mnoise - v->ga[3]) * kg_v;
                v->ga[4] += (ug - v->ga[4]) * kg_v;
                v->g1 += (v->ga[0] - v->g1) * kg_v;
                v->g2 += (v->ga[1] - v->g2) * kg_v;
                v->gsub += (v->ga[2] - v->gsub) * kg_v;
                v->gnoise += (v->ga[3] - v->gnoise) * kg_v;
                v->gout += (v->ga[4] - v->gout) * kg_v;
                x = v->g1 * o1 + v->g2 * o2 + v->gsub * sub;
                if (v->gnoise > 1e-7f)
                    x += v->gnoise * noise_seed_f(&v->seed);
#else
                x = m1 * o1 + m2 * o2 + msub * sub;
                if (mnoise > 0.0f)
                    x += mnoise * noise_seed_f(&v->seed);
#endif
                /* the high-pass (2-pole, state variable) */
                if (hp_on) {             /* (at its bottom, below 24 Hz, it is left out) */
                    float qa1 = a1, qa2 = a2, qg = gh;
                    if (pm_h != 0.0f || lhp != 0.0f) {   /* oscillator 2 or a fast LFO moving it: this sample */
                        qg = tan_w(3.1415927f / SR * fm_clampf(ex2(hp + pm_h * o2 + lhp), 8.0f, 12000.0f));
                        qa1 = 1.0f / (1.0f + qg * (qg + kh));
                        qa2 = qg * qa1;
                    }
                    hv3 = x - v->h2;
                    hv1 = qa1 * v->h1 + qa2 * hv3;
                    hv2 = v->h2 + qg * hv1;
                    v->h1 = 2.0f * hv1 - v->h1;
                    v->h2 = 2.0f * hv2 - v->h2;
                    x = x - kh * hv1 - hv2;
                }
                /* the low-pass: four one-poles in a loop, solved without a delay; the input saturates. In the
                 * plugin below 70 kHz it runs at twice the rate (lp_os): its response the analogue one up to the top
                 * of the band, its saturation without aliasing */
                {
                    float qG = G, qig = ig, qG4 = G4;
                    if (pm_l != 0.0f || llp != 0.0f) {   /* oscillator 2 or a fast LFO moving it: this sample */
                        float gs = tan_w(w_lp * fm_clampf(ex2(cut + pm_l * o2 + llp), 12.0f, 17500.0f));
                        qig = 1.0f / (1.0f + gs);
                        qG = gs * qig;
                        qG4 = 1.0f / (1.0f + kres * qG * qG * qG * qG);
                    }
#ifdef P67_PLUGIN
                    if (lp_os == 2) {
                        float e0 = hb_ap4(x, v->ou0, HB_A0), e1 = hb_ap4(x, v->ou1, HB_A1), z0, z1;
                        z0 = ladder(v, e0, qG, qig, qG4, kres, drv, idrv);
                        z1 = ladder(v, e1, qG, qig, qG4, kres, drv, idrv);
                        y4 = 0.5f * (hb_ap4(z0, v->od0, HB_A0) + hb_ap4(v->odp, v->od1, HB_A1));
                        v->odp = z1;
                    } else
#endif
                        y4 = ladder(v, x, qG, qig, qG4, kres, drv, idrv);
                }
#ifdef P67_PLUGIN
                out = y4 * comp * ae * v->gout * ltr * 0.9f;
#else
                out = y4 * comp * ae * ug * ltr * 0.9f;
#endif
                /* its place: a pan, and on the far side a fraction of a millisecond late */
#ifdef P67_PLUGIN
                if (0) {                 /* (the plugin: always through the line, so it is never stale when SPREAD
                                          * comes up from nought) */
#else
                if (v->hd == 0) {
#endif
                    L[k] += out * v->gl;
                    R[k] += out * v->gr;
                    continue;
                }
                v->hbuf[v->hw & (HAAS - 1u)] = out;
                {
#ifdef P67_PLUGIN                        /* (between two samples: the delay glides) */
                    uint32_t hi = (uint32_t)v->hdf;
                    float hf = v->hdf - (float)hi, h0 = v->hbuf[(v->hw - hi) & (HAAS - 1u)],
                          late = h0 + hf * (v->hbuf[(v->hw - hi - 1u) & (HAAS - 1u)] - h0);
#else
                    float late = v->hbuf[(v->hw - v->hd) & (HAAS - 1u)];
#endif
                    if (v->far) {
                        L[k] += out * v->gl;
                        R[k] += late * v->gr;
                    } else {
                        L[k] += late * v->gl;
                        R[k] += out * v->gr;
                    }
                }
                v->hw++;
            }
            env_advance(v);
            if (v->ae.st == ES_IDLE && !v->pend && v->fade >= 1.0f)
                break;                   /* finished: the rest of the block is silence for it */
        }
        if (v->ae.st == ES_IDLE && !v->pend) {
            v->cinit = 0;                /* silent: the next note's coefficients start where it starts */
#ifdef P67_PLUGIN
            v->gok = 0;
#endif
        }
        v->s1 = flush_dn(v->s1);
        v->s2 = flush_dn(v->s2);
        v->s3 = flush_dn(v->s3);
        v->s4 = flush_dn(v->s4);
        v->h1 = flush_dn(v->h1);
        v->h2 = flush_dn(v->h2);
#ifdef P67_PLUGIN
        {
            int q;
            for (q = 0; q < 5; q++) {
                v->ou0[q] = flush_dn(v->ou0[q]);
                v->ou1[q] = flush_dn(v->ou1[q]);
                v->od0[q] = flush_dn(v->od0[q]);
                v->od1[q] = flush_dn(v->od1[q]);
            }
            v->odp = flush_dn(v->odp);
        }
#endif
    }
}

/* The share-out, each block: the voices sounding (only those), some here and the rest on the second core, so that
 * both finish together: this core also runs the effects after, which cost about fx_weight() voices */
static uint8_t act[NVOICE], act_n, act_k;
static void job_voices(uint32_t arg)
{
    (void)arg;
    PLAT_FAR(voices_render)(act + act_k, act_n - act_k, vb_l[1], vb_r[1]);
}

/* the load, measured: this core's render and the second core's share, in % of a block's time (peaks over ~1 s) */
static uint32_t ld_c1, ld_c2, ld_c1_pk, ld_c2_pk, ld_n;
static uint32_t ld_c1_max, ld_c2_max;

static float lfo_shape_at(int k, float ph)
{
    switch (P.lfo_shape[k]) {
    case LS_SAW: return 2.0f * ph - 1.0f;
    case LS_RAMP: return 1.0f - 2.0f * ph;
    case LS_SQUARE: return ph < 0.5f ? 1.0f : -1.0f;
    case LS_SH: return lfo_sh[k];
    default: return 1.0f - 4.0f * fm_fabsf(ph - 0.5f);
    }
}

#ifdef P67_PLUGIN
/* Q toward P (and the wheels toward where they are), and each voice's character toward VT: once a block, about
 * 4 ms; close enough, exactly there (so what is off is off) */
#define QG(f, t) Q.f = (fm_fabsf((t) - Q.f) < 1e-6f || !Q.init) ? (t) : Q.f + ((t) - Q.f) * k
static void q_glide(uint32_t n)
{
    float k = 1.0f - fm_expf(-(float)n / (0.004f * SR));
    int i;
    QG(mix1, P.mix1);
    QG(mix2, P.mix2);
    QG(sub, P.sub);
    QG(noise, P.noise);
    QG(o1_shape, P.o1_shape);
    QG(o2_shape, P.o2_shape);
    QG(o1_pw, P.o1_pw);
    QG(o2_pw, P.o2_pw);
    QG(pm_fenv, P.pm_fenv);
    QG(pm_osc2, P.pm_osc2);
    QG(lfo_amt[0], P.lfo_amt[0]);
    QG(lfo_amt[1], P.lfo_amt[1]);
    QG(avel, P.avel);
    QG(fvel, P.fvel);
    QG(hp_fvel, P.hp_fvel);
    QG(fenv_vel, P.fenv_vel);
    QG(hp_env_oct, P.hp_env_oct);
    QG(at_amt, P.at_amt);
    {
        float kw = 1.0f - fm_expf(-(float)n / (0.003f * SR));   /* the wheels: 3 ms (a 7-bit mod wheel in no steps) */
        float t = mod_v;
        Q.modw = (fm_fabsf(t - Q.modw) < 1e-6f || !Q.init) ? t : Q.modw + (t - Q.modw) * kw;
        t = bend_v;
        Q.bend = (fm_fabsf(t - Q.bend) < 1e-6f || !Q.init) ? t : Q.bend + (t - Q.bend) * kw;
    }
    for (i = 0; i < NVOICE; i++) {       /* each voice's character (a silent one: at once) */
        voice_t *v = &V[i];
        const vchar_t *t = &VT[i];
        float kv = (v->ae.st == ES_IDLE && !v->pend) || !Q.init ? 1.0f : k;
#define VG(f) v->f += (t->f - v->f) * kv
        VG(c1);
        VG(c2);
        VG(ccut);
        VG(cres);
        VG(camp);
        VG(cpw);
        VG(cshape);
        VG(cpw2);
        VG(cshape2);
        VG(cdrive);
        VG(gl);
        VG(gr);
        VG(hdf);
#undef VG
        v->far = t->far;
    }
    Q.init = 1;
}
#undef QG
#endif

static void lfo_block(uint32_t n)
{
    uint32_t c;
    int k, d;
    float amt[2], m = mod_v, pos = clock_pos(), spt = clock_spt();
#ifdef P67_PLUGIN
    m = Q.modw;
#endif
    amt[0] = fm_minf(1.0f, PQ.lfo_amt[0] + m * m * 0.6f);   /* the mod wheel adds to LFO 1, as the Prophet's */
    amt[1] = PQ.lfo_amt[1];
    for (k = 0; k < 2; k++) {
        if (lfo_reset && P.lfo_retrig[k]) {
            lfo_ph[k] = 0.0f;
            if (P.lfo_sync[k])           /* synced: the clock's position becomes its start */
                lfo_off[k] = -pos / (float)LSY_TICKS[P.lfo_sync[k]];
        }
    }
    lfo_reset = 0;
    /* the fast ones, per sample */
    blk_lfd = 0;
    for (k = 0; k < 2; k++) {
        int fast = k == 0 && !P.lfo_sync[0] && (P.lfo_noise || P.lfo_hz[0] > 40.0f);
        uint32_t s;
        float a, b, ng, inc, *dst;
        if (!fast) {
            lfo_fast[k] = 0;
            continue;
        }
        if (!lfo_fast[k])                /* (just turned fast: the CV starts where the LFO is) */
            lfo_cv[k][0] = lfo_cv[k][1] = lfo_now[k];
        lfo_fast[k] = 1;
        a = 1.0f - fm_expf(-6.2831853f * 1600.0f / SR);
        b = 1.0f - a;
        /* white noise through the CV's two poles keeps a fraction of its power, a^4 (1 + b^2) / (1 - b^2)^3: made
         * up, so the noise moves the cutoff as far as a full-scale S+H would (rms 0.58), at any sample rate */
        ng = 1.0f / fm_sqrtf(a * a * a * a * (1.0f + b * b) / ((1.0f - b * b) * (1.0f - b * b) * (1.0f - b * b)));
        inc = P.lfo_hz[k] / SR;
        dst = P.lfo_dest[k] == LD_AMP ? blk_ltrem : blk_ls[P.lfo_dest[k] < NLD ? P.lfo_dest[k] : 0];
        if (P.lfo_dest[k] == LD_AMP) {
            if (!(blk_lfd & (1u << LD_AMP)))
                for (s = 0; s < n; s++)
                    blk_ltrem[s] = 1.0f;
            blk_lfd |= 1u << LD_AMP;
        } else {
            uint32_t bit = 1u << (P.lfo_dest[k] < NLD ? P.lfo_dest[k] : 0);
            if (!(blk_lfd & bit))
                for (s = 0; s < n; s++)
                    dst[s] = 0.0f;
            blk_lfd |= bit;
        }
        for (s = 0; s < n; s++) {
            float x, y;
            if (P.lfo_noise && k == 0) {
                x = rnd_f(&lfo_seed) * ng;   /* the noise: a new value every sample */
            } else {
                float ph = lfo_ph[k] + inc;
                if (ph >= 1.0f) {
                    ph -= 1.0f;
                    lfo_sh[k] = rnd_f(&lfo_seed);
                }
                lfo_ph[k] = ph;
                x = lfo_shape_at(k, ph);
            }
            lfo_cv[k][0] += (x - lfo_cv[k][0]) * a;
            lfo_cv[k][1] += (lfo_cv[k][0] - lfo_cv[k][1]) * a;
            y = fm_clampf(lfo_cv[k][1], -2.0f, 2.0f);
            if (k == 0)
                blk_l1s[s] = y;
            if (P.lfo_dest[k] == LD_AMP)
                dst[s] *= 1.0f - amt[k] * fm_clampf(0.5f - 0.5f * y, 0.0f, 1.0f);
            else
                dst[s] += amt[k] * y;
        }
        lfo_now[k] = fm_clampf(lfo_cv[k][1], -1.0f, 1.0f);
        lfo_cv[k][0] = flush_dn(lfo_cv[k][0]);
        lfo_cv[k][1] = flush_dn(lfo_cv[k][1]);
    }
    for (c = 0; c * CH < n; c++) {
        float *md = blk_mod[c], trem = 1.0f;
        for (d = 0; d < NLD; d++)
            md[d] = 0.0f;
        for (k = 0; k < 2; k++) {
            float ph, v, prev = lfo_ph[k];
            if (lfo_fast[k]) {           /* (per sample, above) */
                if (k == 0)
                    blk_l1[c] = 0.0f;
                continue;
            }
            if (P.lfo_sync[k] && P.lfo_sync[k] < NLSY) {
                float cyc = (float)LSY_TICKS[P.lfo_sync[k]];
                ph = (pos + (float)(c * CH) / spt) / cyc + lfo_off[k];
                ph -= fm_floorf(ph);
            } else {
                ph = prev + P.lfo_hz[k] * (float)CH / SR;
                if (ph >= 1.0f)
                    ph -= 1.0f;
            }
            if (ph < prev)               /* came round: S+H takes a new value */
                lfo_sh[k] = rnd_f(&lfo_seed);
            lfo_ph[k] = ph;
            v = lfo_shape_at(k, ph);
            lfo_now[k] = v;
            if (k == 0)
                blk_l1[c] = v;
            if (P.lfo_dest[k] == LD_AMP)
                trem *= 1.0f - amt[k] * (0.5f - 0.5f * v);
            else
                md[P.lfo_dest[k] < NLD ? P.lfo_dest[k] : 0] += amt[k] * v;
        }
        blk_trem[c] = trem;
        vib_ph += 5.5f * (float)CH / SR;
        if (vib_ph >= 1.0f)
            vib_ph -= 1.0f;
        blk_vib[c] = 1.0f - 4.0f * fm_fabsf(vib_ph - 0.5f);
        {
#ifdef P67_PLUGIN
            const float ke = k_sr(0.07f);  /* (ENV AMT: its steps the largest, up to 9 octaves over 64: 22 ms) */
#else
            const float ke = k_sr(SMOOTH_K);
#endif
            const float k = k_sr(SMOOTH_K);   /* two stages, each a chunk's share: a detent's step becomes an S-curve
                                          * (no corner to hear at its start), 90 % there in about 6 ms */
            float tc = P.cut_oct + (cc_cut >= 0 ? cutoff_oct((float)cc_cut * (255.0f / 127.0f)) - P.cut_oct : 0.0f);
            if (!SM.init) {
                SM.cut = tc;
                SM.res = P.res_k;
                SM.env = P.env_oct;
                SM.hp = P.hp_oct;
                SM.hpk = P.hp_k;
                SM.c1 = tc;
                SM.r1 = P.res_k;
                SM.e1 = P.env_oct;
                SM.h1 = P.hp_oct;
                SM.k1 = P.hp_k;
                SM.init = 1;
            }
            SM.c1 += (tc - SM.c1) * k;
            SM.r1 += (P.res_k - SM.r1) * k;
            SM.e1 += (P.env_oct - SM.e1) * ke;
            SM.h1 += (P.hp_oct - SM.h1) * k;
            SM.k1 += (P.hp_k - SM.k1) * k;
            SM.cut += (SM.c1 - SM.cut) * k;
            SM.res += (SM.r1 - SM.res) * k;
            SM.env += (SM.e1 - SM.env) * ke;
            SM.hp += (SM.h1 - SM.hp) * k;
            SM.hpk += (SM.k1 - SM.hpk) * k;
            blk_cut[c] = SM.cut;
            blk_res[c] = SM.res;
            blk_env[c] = SM.env;
            blk_hp[c] = SM.hp;
            blk_hpk[c] = SM.hpk;
        }
    }
}

#ifdef P67_PLUGIN
void eng_render(float *ol, float *orr, uint32_t n, float gain)
#else
void eng_render(int32_t *lr, uint32_t n, float gain)
#endif
{
    uint32_t s, t0, t1, t2;
    int on2;
    float lvl = 0.0f, kg = k_sr(0.004f), kduck = k_sr(0.003f);
#ifdef P67_PLUGIN
    const float dcb_r = 1.0f - 6.2831853f * 4.0f / SR;
#endif
    if (n > NBLK)
        n = NBLK;
    while (ev_r != ev_w) {               /* the notes that came */
        ev_t e = evq[ev_r % NEV];
        ev_r++;
        switch (e.type) {
        case EV_ON:
            on_gain = e.gain;
            voice_on(e.id, e.note, e.vel);
            on_gain = 1.0f;
            break;
        case EV_PRESS: {
            int i;
            for (i = 0; i < NVOICE; i++)
                if (V[i].id == e.id && (V[i].gate || V[i].sus))
                    V[i].ptgt = e.vel;
            break;
        }
        case EV_PRESS_ALL: chan_press = e.vel; break;
        case EV_OFF: voice_off(e.id); break;
        case EV_SUS: sus_set(e.note); break;
        case EV_ALLOFF: all_off(); break;
        default: {
            int i;
            all_off();
            for (i = 0; i < NVOICE; i++) {
                V[i].ae.v = V[i].fe.v = 0.0f;
                V[i].ae.st = V[i].fe.st = ES_IDLE;
                V[i].s1 = V[i].s2 = V[i].s3 = V[i].s4 = V[i].h1 = V[i].h2 = 0.0f;
#ifdef P67_PLUGIN
                std::memset(V[i].ou0, 0, sizeof V[i].ou0);
                std::memset(V[i].ou1, 0, sizeof V[i].ou1);
                std::memset(V[i].od0, 0, sizeof V[i].od0);
                std::memset(V[i].od1, 0, sizeof V[i].od1);
                V[i].odp = 0.0f;
#endif
            }
            fx_clear();
            clock_panic();
        }
        }
    }
    clock_block(n);                      /* the sequencer's and the arp's notes for this block */
#ifdef P67_PLUGIN
    q_glide(n);
#endif
    blk_n = n;
    lfo_block(n);
    blk_fm = (P.pm_dosc & PMO_FREQ) && PQ.pm_osc2 != 0.0f;
    blk_pms = (P.pm_dosc & PMO_SHAPE) && PQ.pm_osc2 != 0.0f;
    blk_pmp = (P.pm_dosc & PMO_PW) && PQ.pm_osc2 != 0.0f;
    blk_pml = (P.pm_dfilt == PMF_LPF || P.pm_dfilt == PMF_BOTH) && PQ.pm_osc2 != 0.0f;
    blk_pmh = (P.pm_dfilt == PMF_HPF || P.pm_dfilt == PMF_BOTH) && PQ.pm_osc2 != 0.0f;
    {
        int i, k;
        act_n = 0;
        for (i = 0; i < NVOICE; i++)
            if (V[i].ae.st != ES_IDLE || V[i].pend)
                act[act_n++] = (uint8_t)i;
        k = (int)(((float)act_n - fx_weight()) * 0.5f + 0.5f);
        act_k = (uint8_t)(k < 0 ? 0 : k > act_n ? act_n : k);
    }
    t0 = plat_ticks24();
    on2 = plat_cpu2_on() && act_k < act_n && plat_cpu2_run(job_voices, 0);
    if (!on2)
        act_k = act_n;                   /* one core: all of them here */
    PLAT_FAR(voices_render)(act, act_k, vb_l[0], vb_r[0]);
    t1 = plat_ticks24();
    if (on2 && plat_cpu2_wait() != 0) {
        PLAT_FAR(voices_render)(act + act_k, act_n - act_k, vb_l[1], vb_r[1]);   /* (given up: done here) */
        on2 = 0;
    }
    if (!on2 && act_k == act_n)
        for (s = 0; s < n; s++)
            vb_l[1][s] = vb_r[1][s] = 0.0f;
    t2 = plat_ticks24();
    for (s = 0; s < n; s++) {
        vb_l[0][s] += vb_l[1][s];
        vb_r[0][s] += vb_r[1][s];
    }
    fx_process(vb_l[0], vb_r[0], n);
    for (s = 0; s < n; s++) {
        float l = vb_l[0][s], r = vb_r[0][s], a;
#ifdef P67_PLUGIN
        {                                /* the outputs AC-coupled, as the Prophet-6's (4 Hz: what the filter's drive
                                          * leaves of DC goes; nothing heard is touched) */
            float yl = l - dcb_x[0] + dcb_r * dcb_y[0], yr = r - dcb_x[1] + dcb_r * dcb_y[1];
            dcb_x[0] = l;
            dcb_x[1] = r;
            dcb_y[0] = l = flush_dn(yl);
            dcb_y[1] = r = flush_dn(yr);
        }
#endif
        /* the output: soft above -3 dB, never over */
        a = fm_fabsf(l);
        if (a > 0.7f)
            l = (l > 0.0f ? 1.0f : -1.0f) * (0.7f + 0.3f * fm_tanhf((a - 0.7f) * 3.3333f));
        a = fm_fabsf(r);
        if (a > 0.7f)
            r = (r > 0.0f ? 1.0f : -1.0f) * (0.7f + 0.3f * fm_tanhf((a - 0.7f) * 3.3333f));
        lvl = fm_maxf(lvl, fm_fabsf(l) + fm_fabsf(r));
        gain_s += (gain - gain_s) * kg;
        duck_g += ((duck_on ? 0.0f : 1.0f) - duck_g) * kduck;
#ifdef P67_PLUGIN
        ol[s] = l * gain_s * duck_g;
        orr[s] = r * gain_s * duck_g;
    }
    /* (the meter's fall: 0.97 a block of 128 at 44.1 kHz, the same per second at any block and rate) */
    lvl_out = lvl * 0.5f > lvl_out ? lvl * 0.5f : lvl_out * fm_exp2f(-0.0439433f * (float)n / (128.0f * SRS));
#else
        lr[2u * s] = (int32_t)(l * gain_s * duck_g);
        lr[2u * s + 1u] = (int32_t)(r * gain_s * duck_g);
    }
    lvl_out = lvl * 0.5f > lvl_out ? lvl * 0.5f : lvl_out * 0.97f;
#endif
    {                                    /* the load: this core busy (less its wait); the second core's job at least
                                          * until this core stopped waiting for it */
        uint32_t t3 = plat_ticks24(), blk = n * 1000000u / 44100u * PLAT_TICKS_PER_US;
        uint32_t c1 = (t1 - t0) + (t3 - t2), c2 = on2 ? t2 - t0 : 0u;
        if (blk) {
            c1 = c1 * 100u / blk;
            c2 = c2 * 100u / blk;
            ld_c1 = c1;
            ld_c2 = c2;
            if (c1 > ld_c1_max)
                ld_c1_max = c1;
            if (c2 > ld_c2_max)
                ld_c2_max = c2;
            if (++ld_n >= 344u) {        /* about a second */
                ld_c1_pk = ld_c1_max;
                ld_c2_pk = ld_c2_max;
                ld_c1_max = ld_c2_max = 0;
                ld_n = 0;
            }
        }
    }
}

void eng_load(uint32_t *c1, uint32_t *c2, uint32_t *c1_peak, uint32_t *c2_peak)
{
    *c1 = ld_c1;
    *c2 = ld_c2;
    *c1_peak = ld_c1_pk;
    *c2_peak = ld_c2_pk;
}

/* -------------------------------------------------------------- the calls --- */
void eng_init(void)
{
    int i;
    for (i = 0; i < NVOICE; i++) {
        V[i].id = -1;
        V[i].fade = 1.0f;
        V[i].seed = 0x9E3779B9u * (uint32_t)(i + 1);
        V[i].o1.ph = 0.13f * (float)i;   /* free-running: they start wherever */
        V[i].o2.ph = 0.37f * (float)i - (float)(int)(0.37f * (float)i);
        V[i].dr_n = (uint16_t)(i * 97);
    }
#ifdef P67_PLUGIN
    bl_tab = p67_blep_tables();
#endif
    fx_init();
    clock_init();
}

void eng_note_on(int id, int note, float vel) { ev_put(EV_ON, id, note, vel); }
void eng_note_on_gain(int id, int note, float vel, float gain) { ev_put2(EV_ON, id, note, vel, gain); }
static float at_curve(float p) { return p <= 0.0f ? 0.0f : fm_powf(fm_minf(p, 1.0f), P.at_curve); }
void eng_press(int id, float p) { ev_put(EV_PRESS, id, 0, at_curve(p)); }
void eng_press_all(float p) { ev_put(EV_PRESS_ALL, 0, 0, at_curve(p)); }
float eng_voice_press(int v) { return v >= 0 && v < NVOICE ? V[v].press : 0.0f; }
void eng_note_off(int id) { ev_put(EV_OFF, id, 0, 0.0f); }
void eng_sustain(int on) { ev_put(EV_SUS, 0, on ? 1 : 0, 0.0f); }
void eng_all_off(void) { ev_put(EV_ALLOFF, 0, 0, 0.0f); }
void eng_panic(void) { ev_put(EV_PANIC, 0, 0, 0.0f); }
void eng_bend(float v) { bend_v = v; }
void eng_mod(float v) { mod_v = v; }
void eng_cutoff_cc(int v) { cc_cut = v; }
#ifdef P67_PLUGIN
void eng_hp_extra(int vel, int keytrk, int fenvVel)   /* (taken at the next patch) */
{
    fenv_extra_vel = (float)fenvVel * (1.0f / 127.0f);
    hp_extra_set = 1;
    hp_extra_vel = (float)vel * (1.0f / 127.0f);
    hp_extra_kt = keytrk == KT_FULL ? 1.0f : keytrk == KT_HALF ? 0.5f : 0.0f;
}
#endif
#ifdef P67_PLUGIN
void eng_quality(int high) { q_eco = (uint8_t)(high ? 0 : 1); }
#endif
void eng_duck(int on) { duck_on = (uint8_t)(on != 0); }
int eng_ducked(void) { return duck_g < 0.001f; }

void eng_overload(void)                  /* the quietest voice let go at once */
{
    int i, q = -1;
    float lo = 2.0f;
    for (i = 0; i < NVOICE; i++)
        if (V[i].ae.st != ES_IDLE && !V[i].gate && V[i].ae.v < lo) {
            lo = V[i].ae.v;
            q = i;
        }
    if (q >= 0 && V[q].fade >= 1.0f)
        V[q].fade = 0.99999f;            /* out along the steal's curve (4.5 ms), never cut */
}

float eng_voice_env(int v) { return v >= 0 && v < NVOICE ? V[v].ae.v : 0.0f; }
int eng_voice_note(int v) { return v >= 0 && v < NVOICE && V[v].ae.st != ES_IDLE ? V[v].note : -1; }
int eng_voices(void)
{
    int i, k = 0;
    for (i = 0; i < NVOICE; i++)
        if (V[i].ae.st != ES_IDLE)
            k++;
    return k;
}
uint32_t eng_notes(void) { return notes_n; }
uint32_t eng_steals(void) { return steals_n; }
float eng_level(void) { return lvl_out; }
float eng_tail(void) { return fx_tail(); }
float eng_lfo(int which) { return lfo_now[which & 1]; }
