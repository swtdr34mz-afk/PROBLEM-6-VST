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

#ifndef SMOOTH_K
#define SMOOTH_K 0.25f
#endif
#define CH 16u                           /* a chunk (0.36 ms): pitches and coefficient targets; the coefficients
                                          * themselves ramp per sample */
#define HAAS 32u                         /* the spread's per-voice delay line (a power of two) */

/* ------------------------------------------------------------- the patch --- */
/* eng_patch's numbers (main loop writes, the render reads; each a single float, no torn values) */
static struct {
    float o1_semi, o2_semi, o1_shape, o2_shape, o1_pw, o2_pw;
    float mix1, mix2, sub, noise;
    uint8_t o2_mode;
    float cut_oct, res_k, env_oct, hp_oct, hp_k, keytrk, fvel, hp_env_oct;
    float fs, as;                        /* sustain levels */
    float lfo_hz[2], lfo_amt[2];
    uint8_t lfo_shape[2], lfo_dest[2], lfo_sync[2], lfo_retrig[2];
    float pm_fenv, pm_osc2;              /* -1..1, squared with their sign */
    uint8_t pm_dosc, pm_dfilt;
    float glide_k, avel, bend;
    uint8_t unison;
    float vint, spread;
    uint8_t at_dest;
    float at_amt, at_curve, at_k, at_kr;   /* AFTERTOUCH: amount -1..1, the curve's exponent, the smoothing (held,
                                            * and let go: never faster than 150 ms, so a release never jumps) */
} P;

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
typedef struct {
    float ph, prev;                      /* phase 0..1, the output a sample late (the BLEP's second half) */
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
    uint8_t hw;
    float hbuf[HAAS];
} voice_t;
static voice_t V[NVOICE];
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
#define NEV 64u
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
    P.hp_env_oct = sq_sign(bip(p[P_HP_ENV])) * 7.0f;
    P.fs = u7(p[P_FS]);
    P.as = u7(p[P_AS]);
    P.lfo_hz[0] = 0.05f * fm_exp2f((float)p[P_LFO_RATE] * (9.2288f / 127.0f));   /* 0.05 .. 30 Hz */
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
        v->c1 = a * 12.0f * V_T1[i] + s * S_DET[i];                     /* cents */
        v->c2 = a * 15.0f * V_T2[i] + s * S_DET[i] * 0.8f;
        v->ccut = a * 0.6f * V_CUT[i] + s * S_COL[i] * (1.0f / 12.0f);   /* octaves */
        v->cres = 1.0f + 0.4f * a * V_RES[i];
        v->camp = 1.0f + 0.35f * a * V_AMP[i];                           /* about +-3 dB */
        v->cpw = 0.12f * a * V_PW[i];
        v->cshape = 0.10f * a * V_SHP[i];
        v->cpw2 = 0.10f * a * V_PW[NVOICE - 1 - i];
        v->cshape2 = 0.08f * a * V_SHP[NVOICE - 1 - i];
        v->cdrive = 1.4f * (1.0f + 0.45f * a * V_DEC[i]);
        /* the envelopes: an RC's curve. Attack aims past the top (1.3) and stops at 1; decay and release fall to
         * where they go; the times are where a decay is down 40 dB */
        v->ka_f = k_of(env_seconds(p[P_FA]) * tf * 0.6824f);
        v->kd_f = k_of(env_seconds(p[P_FD]) * td * 0.2171f);
        v->kr_f = k_of(env_seconds(p[P_FR]) * tr * 0.2171f);
        v->ka_a = k_of(env_seconds(p[P_AA]) * tf * 0.6824f);
        v->kd_a = k_of(env_seconds(p[P_AD]) * td * 0.2171f);
        v->kr_a = k_of(env_seconds(p[P_AR]) * tr * 0.2171f);
        v->gl = fm_cosf(th);
        v->gr = fm_sinf(th);
        v->far = pan < 0.0f ? 1u : 0u;   /* the side away from it hears it a little later */
        v->hd = (uint8_t)(fm_fabsf(pan) * 16.0f * s);
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

HOT static void voices_render(const uint8_t *list, int nv, float *restrict L, float *restrict R)
{
    uint32_t n = blk_n, c, s, k;
    int j;
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
            float m1, m2, msub, mnoise;
            wave_t w1, w2;
            int lowf = P.o2_mode == O2M_LOW || P.o2_mode == O2M_LOWFIX;
            int sync = P.o2_mode == O2M_SYNC;
            int o2_used = (P.mix2 > 0.0f && !lowf) || sync || blk_fm || blk_pms || blk_pmp || blk_pml || blk_pmh;
            /* VINTAGE's drift: a slow wander, a new heading now and then */
            if (P.vint > 0.0f) {
                if (v->dr_n == 0) {
                    v->dt1 = rnd_f(&v->seed) * 10.0f * P.vint;
                    v->dt2 = rnd_f(&v->seed) * 10.0f * P.vint;
                    v->dr_n = (uint16_t)(1100u + (rnd_u(&v->seed) & 2047u));   /* 0.2 .. 0.57 s */
                }
                v->dr_n--;
                v->dr1 += (v->dt1 - v->dr1) * 0.0016f;
                v->dr2 += (v->dt2 - v->dr2) * 0.0016f;
            } else {
                v->dr1 = v->dr2 = 0.0f;
            }
            v->pitch += (v->target - v->pitch) * P.glide_k;
            p = v->pitch + bend_v * P.bend;
            /* AFTERTOUCH: the key's pressure (or the channel's), smoothed, to where it goes */
            v->press += (ri_maxf(v->ptgt, chan_press) - v->press) * (v->gate || v->sus ? P.at_k : P.at_kr);
            pp = v->press * P.at_amt;
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
            pmfe = P.pm_fenv * at_pm;   /* (AFTERTOUCH to POLY MOD: both its amounts) */
            pmo2 = P.pm_osc2 * at_pm;
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
            sh1 = P.o1_shape + v->cshape;
            pw1 = P.o1_pw + v->cpw;
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
            vel_f = 1.0f - P.fvel + P.fvel * v->vel;
            famt = blk_env[c] * vel_f;
            hamt = P.hp_env_oct * vel_f;
            cut = blk_cut[c] + v->ccut + v->ncut + famt * fen + P.keytrk * (p - 60.0f) * (1.0f / 12.0f);
            hp = blk_hp[c] + hamt * fen + P.keytrk * (p - 60.0f) * (1.0f / 12.0f);
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
                tg = tan_w(3.1415927f / SR * fm_clampf(ex2(cut), 12.0f, 17500.0f));
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
            wave_set(&w2, P.o2_shape + v->cshape2, P.o2_pw + v->cpw2 + lm[LD_PW] * 0.42f);
            wave_set(&w1, sh1, pw1);
            drv = v->cdrive * at_drv;
            idrv = 1.0f / drv;
            ug = v->camp * blk_trem[c] * at_amp * v->gain
                 * (1.0f - P.avel + P.avel * v->vel);
            hp_on = hp > 4.6f || pm_h != 0.0f;
            pm_sp = pm_s != 0.0f || pm_p != 0.0f;
            m1 = 0.5f * P.mix1;
            m2 = lowf ? 0.0f : 0.5f * P.mix2;
            msub = 0.5f * P.sub;
            mnoise = 0.7f * P.noise;
            env_targets(v, &ft, &fk, &at, &ak);
            for (s = 0; s < CH && c * CH + s < n; s++) {
                float o1, o2, sub, x, u, S, y1, y2, y3, y4, t, hv1, hv2, hv3, out, rs, ae, stG;
                int wr1, wr2;
                float wd1 = 0.0f, wd2 = 0.0f;
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
                    v->fade -= 1.0f / STEAL;
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
                if (o2_used) {
                    o2 = osc_tick(&v->o2, &w2, inc2, -1.0f, &wr2, &wd2);
                } else {                 /* not heard, not used: it only runs on (free-running, as a VCO) */
                    o2 = 0.0f;
                    wr2 = 0;
                    v->o2.ph += inc2;
                    if (v->o2.ph >= 1.0f)
                        v->o2.ph -= 1.0f;
                }
                if (pm_sp)
                    wave_set(&w1, sh1 + pm_s * o2, pw1 + pm_p * o2);
                o1 = osc_tick(&v->o1, &w1, pm_f != 0.0f ? fm_minf(inc1 * ex2(pm_f * o2), 0.45f) : inc1,
                              (sync && wr2) ? wd2 : -1.0f, &wr1, &wd1);
                if (wr1)
                    v->flip = 1.0f - v->flip;
                t = (v->o1.ph + v->flip) * 0.5f;     /* the sub-octave: a triangle an octave down */
                sub = 1.0f - 4.0f * fm_fabsf(t - 0.5f);
                x = m1 * o1 + m2 * o2 + msub * sub;
                if (mnoise > 0.0f)
                    x += mnoise * noise_seed_f(&v->seed);
                /* the high-pass (2-pole, state variable) */
                if (hp_on) {             /* (at its bottom, below 24 Hz, it is left out) */
                    float qa1 = a1, qa2 = a2, qg = gh;
                    if (pm_h != 0.0f) {  /* oscillator 2 moving it: worked out this sample */
                        qg = tan_w(3.1415927f / SR * fm_clampf(ex2(hp + pm_h * o2), 8.0f, 12000.0f));
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
                /* the low-pass: four one-poles in a loop, solved without a delay; the input saturates */
                {
                    float qG = G, qig = ig, qG4 = G4, G2;
                    if (pm_l != 0.0f) {  /* oscillator 2 moving it: worked out this sample */
                        float gs = tan_w(3.1415927f / SR * fm_clampf(ex2(cut + pm_l * o2), 12.0f, 17500.0f));
                        qig = 1.0f / (1.0f + gs);
                        qG = gs * qig;
                        qG4 = 1.0f / (1.0f + kres * qG * qG * qG * qG);
                    }
                    G2 = qG * qG;
                    S = (G2 * qG * v->s1 + G2 * v->s2 + qG * v->s3 + v->s4) * qig;
                    u = (x - kres * S) * qG4;
                    stG = qG;
                }
                u = sat(u * drv) * idrv;
                rs = (u - v->s1) * stG;
                y1 = rs + v->s1;
                v->s1 = y1 + rs;
                rs = (y1 - v->s2) * stG;
                y2 = rs + v->s2;
                v->s2 = y2 + rs;
                rs = (y2 - v->s3) * stG;
                y3 = rs + v->s3;
                v->s3 = y3 + rs;
                rs = (y3 - v->s4) * stG;
                y4 = rs + v->s4;
                v->s4 = y4 + rs;
                out = y4 * comp * ae * ug * 0.9f;
                /* its place: a pan, and on the far side a fraction of a millisecond late */
                k = c * CH + s;
                if (v->hd == 0) {
                    L[k] += out * v->gl;
                    R[k] += out * v->gr;
                    continue;
                }
                v->hbuf[v->hw & (HAAS - 1u)] = out;
                {
                    float late = v->hbuf[(v->hw - v->hd) & (HAAS - 1u)];
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
        if (v->ae.st == ES_IDLE && !v->pend)
            v->cinit = 0;                /* silent: the next note's coefficients start where it starts */
        v->s1 = flush_dn(v->s1);
        v->s2 = flush_dn(v->s2);
        v->s3 = flush_dn(v->s3);
        v->s4 = flush_dn(v->s4);
        v->h1 = flush_dn(v->h1);
        v->h2 = flush_dn(v->h2);
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

static void lfo_block(uint32_t n)
{
    uint32_t c;
    int k, d;
    float amt[2], m = mod_v, pos = clock_pos(), spt = clock_spt();
    amt[0] = fm_minf(1.0f, P.lfo_amt[0] + m * m * 0.6f);   /* the mod wheel adds to LFO 1, as the Prophet's */
    amt[1] = P.lfo_amt[1];
    for (k = 0; k < 2; k++) {
        if (lfo_reset && P.lfo_retrig[k]) {
            lfo_ph[k] = 0.0f;
            if (P.lfo_sync[k])           /* synced: the clock's position becomes its start */
                lfo_off[k] = -pos / (float)LSY_TICKS[P.lfo_sync[k]];
        }
    }
    lfo_reset = 0;
    for (c = 0; c * CH < n; c++) {
        float *md = blk_mod[c], trem = 1.0f;
        for (d = 0; d < NLD; d++)
            md[d] = 0.0f;
        for (k = 0; k < 2; k++) {
            float ph, v, prev = lfo_ph[k];
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
            const float k = SMOOTH_K;    /* two stages, each a chunk's share: a detent's step becomes an S-curve
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
            SM.e1 += (P.env_oct - SM.e1) * k;
            SM.h1 += (P.hp_oct - SM.h1) * k;
            SM.k1 += (P.hp_k - SM.k1) * k;
            SM.cut += (SM.c1 - SM.cut) * k;
            SM.res += (SM.r1 - SM.res) * k;
            SM.env += (SM.e1 - SM.env) * k;
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

void eng_render(int32_t *lr, uint32_t n, float gain)
{
    uint32_t s, t0, t1, t2;
    int on2;
    float lvl = 0.0f;
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
            }
            fx_clear();
            clock_panic();
        }
        }
    }
    clock_block(n);                      /* the sequencer's and the arp's notes for this block */
    blk_n = n;
    lfo_block(n);
    blk_fm = (P.pm_dosc & PMO_FREQ) && P.pm_osc2 != 0.0f;
    blk_pms = (P.pm_dosc & PMO_SHAPE) && P.pm_osc2 != 0.0f;
    blk_pmp = (P.pm_dosc & PMO_PW) && P.pm_osc2 != 0.0f;
    blk_pml = (P.pm_dfilt == PMF_LPF || P.pm_dfilt == PMF_BOTH) && P.pm_osc2 != 0.0f;
    blk_pmh = (P.pm_dfilt == PMF_HPF || P.pm_dfilt == PMF_BOTH) && P.pm_osc2 != 0.0f;
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
        /* the output: soft above -3 dB, never over */
        a = fm_fabsf(l);
        if (a > 0.7f)
            l = (l > 0.0f ? 1.0f : -1.0f) * (0.7f + 0.3f * fm_tanhf((a - 0.7f) * 3.3333f));
        a = fm_fabsf(r);
        if (a > 0.7f)
            r = (r > 0.0f ? 1.0f : -1.0f) * (0.7f + 0.3f * fm_tanhf((a - 0.7f) * 3.3333f));
        lvl = fm_maxf(lvl, fm_fabsf(l) + fm_fabsf(r));
        gain_s += (gain - gain_s) * 0.004f;
        duck_g += ((duck_on ? 0.0f : 1.0f) - duck_g) * 0.003f;
        lr[2u * s] = (int32_t)(l * gain_s * duck_g);
        lr[2u * s + 1u] = (int32_t)(r * gain_s * duck_g);
    }
    lvl_out = lvl * 0.5f > lvl_out ? lvl * 0.5f : lvl_out * 0.97f;
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
