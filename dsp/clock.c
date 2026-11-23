/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The clock, the step sequencer and the arpeggiator (engine.h). They run in the render, before the voices: a block's
 * ticks are counted, steps and arp notes go straight to the voices, so they land within a block (2.9 ms) of their time
 * and never drift. The clock is 24 ticks a quarter note, from BPM, or from MIDI clock while it comes in (START, STOP
 * and CONTINUE followed). */
#include "core.h"
#include "plat.h"

static const uint8_t RATE_TICKS[NRATE] = {12, 8, 6, 4, 3};

/* ------------------------------------------------------------- the clock --- */
static float bpm = 120.0f, swing;
static float acc;                        /* samples into the current tick */
static float spt = 918.75f;              /* samples a tick */
static uint32_t ticks;                   /* since the start */
static volatile uint32_t ext_in;         /* MIDI clocks come (the 1 kHz tick), the render takes them */
static uint32_t ext_taken, ext_last_blk, blk_count;
static float ext_spt;                    /* samples between MIDI clocks, smoothed */
static float since_ext;                  /* samples since the last one */
static volatile uint8_t ext_cmd;         /* a START / STOP / CONTINUE waiting */

/* ------------------------------------------------------------ the sequencer --- */
static const seq_t *seq;
static uint8_t playing, bars = 1, rate = RATE_16;
static float gate = 0.5f;
static int8_t transpose;
static int16_t step = -1;                /* the step playing */
static uint8_t step_tick;                /* ticks into it */
static uint8_t on_n;                     /* the step's notes sounding */
static int8_t on_note[SEQ_POLY];
static float on_left;                    /* samples until they are let go (< 0: held) */
static float swing_wait = -1.0f;         /* a swung step waits this many samples (-1: none waiting) */
static int16_t swing_step;

/* -------------------------------------------------------------- the arp --- */
#define AK 16
static uint8_t arp_on_, arp_mode, arp_rate = RATE_16, arp_oct = 1, arp_hold;
static float arp_gate = 0.5f;
static int8_t ak_note[AK];               /* the keys held for it, in the order played */
static float ak_vel[AK];
static uint8_t ak_n, ak_down;            /* how many it plays, how many fingers are still down */
static uint8_t arp_tick;
static int16_t arp_idx;
static int8_t arp_last = -1, arp_dir = 1, arp_shown = -1;   /* sounding; the last played (the screen) */
static float arp_left = -1.0f;
static uint32_t arp_steps_n, arp_seed = 0xA5A5F00Du;

void clock_init(void) {}

static void seq_release(void)
{
    uint8_t i;
    for (i = 0; i < on_n; i++)
        voice_off(SRC_SEQ | on_note[i]);
    on_n = 0;
    on_left = -1.0f;
}

static void arp_release(void)
{
    if (arp_last >= 0)
        voice_off(SRC_ARP | arp_last);
    arp_last = -1;
    arp_left = -1.0f;
}

void clock_panic(void)
{
    on_n = 0;
    arp_last = -1;
    on_left = arp_left = -1.0f;
}

static float spt_now(void) { return eng_clock_ext() && ext_spt > 0.0f ? ext_spt : spt; }
float clock_spt(void) { return spt_now(); }
float clock_pos(void)
{
    float f = eng_clock_ext() && ext_spt > 0.0f ? since_ext / ext_spt : acc / spt;
    return (float)ticks + fm_minf(f, 0.999f);
}

/* a step of the sequencer: its notes on (a TIE holds the notes before) */
static void seq_fire(int st)
{
    const uint8_t *nt = seq->note[st];
    float len = spt_now() * (float)RATE_TICKS[rate];
    uint8_t i;
    if (nt[0] == SN_TIE) {
        if (on_n)
            on_left = len * gate + len;  /* longer: through this step too */
        return;
    }
    seq_release();
    for (i = 0; i < SEQ_POLY; i++) {
        int note = nt[i];
        if (note < 12)
            continue;
        note += transpose;
        if (note < 0 || note > 127)
            continue;
        voice_on(SRC_SEQ | note, note, 0.8f);
        on_note[on_n++] = (int8_t)note;
    }
    on_left = len * gate;
}

static int seq_len(void) { return (int)bars * 16; }

static void seq_tick(void)
{
    if (!playing || !seq)
        return;
    if (step < 0 || ++step_tick >= RATE_TICKS[rate]) {
        step_tick = 0;
        step = (int16_t)(step + 1 >= seq_len() ? 0 : step + 1);
        if ((step & 1) && swing > 0.0f) {   /* the odd steps a little late */
            swing_wait = swing * spt_now() * (float)RATE_TICKS[rate];
            swing_step = step;
        } else {
            seq_fire(step);
        }
    }
}

/* ---------------------------------------------------------------- the arp --- */
static int arp_order(int8_t *out)        /* the notes it walks, one octave (sorted, or as played) */
{
    int i, j, n = ak_n;
    for (i = 0; i < n; i++)
        out[i] = ak_note[i];
    if (arp_mode != ARP_PLAYED)
        for (i = 1; i < n; i++) {
            int8_t t = out[i];
            for (j = i; j > 0 && out[j - 1] > t; j--)
                out[j] = out[j - 1];
            out[j] = t;
        }
    return n;
}

static void arp_next(void)
{
    int8_t ord[AK];
    int n = arp_order(ord), total, k, note;
    float vel = 0.8f;
    arp_release();
    if (n == 0)
        return;
    total = n * arp_oct;
    switch (arp_mode) {
    case ARP_DOWN:
        arp_idx = (int16_t)(arp_idx <= 0 || arp_idx > total ? total - 1 : arp_idx - 1);
        k = arp_idx;
        break;
    case ARP_UPDOWN:
        if (total == 1) {
            k = 0;
            break;
        }
        arp_idx += arp_dir;
        if (arp_idx >= total) {
            arp_idx = (int16_t)(total - 2);
            arp_dir = -1;
        } else if (arp_idx < 0) {
            arp_idx = 1;
            arp_dir = 1;
        }
        k = arp_idx;
        break;
    case ARP_RANDOM:
        k = (int)(rnd_u(&arp_seed) % (uint32_t)total);
        break;
    default:                             /* UP, PLAYED */
        arp_idx = (int16_t)(arp_idx + 1 >= total || arp_idx < 0 ? 0 : arp_idx + 1);
        k = arp_idx;
    }
    if (k < 0 || k >= total)
        k = 0;
    note = ord[k % n] + 12 * (k / n);
    {
        int j;
        for (j = 0; j < ak_n; j++)       /* its key's velocity */
            if (ak_note[j] == ord[k % n])
                vel = ak_vel[j];
    }
    if (note > 127)
        note -= 12;
    voice_on(SRC_ARP | note, note, vel);
    arp_last = arp_shown = (int8_t)note;
    arp_left = spt_now() * (float)RATE_TICKS[arp_rate] * arp_gate;
    arp_steps_n++;
}

static void arp_tick_(void)
{
    if (!arp_on_)
        return;
    if (ak_n == 0) {
        if (arp_last >= 0)
            arp_release();
        arp_tick = 0;
        return;
    }
    if (++arp_tick >= RATE_TICKS[arp_rate]) {
        arp_tick = 0;
        arp_next();
    }
}

static void tick(void)
{
    ticks++;
    seq_tick();
    arp_tick_();
}

/* ----------------------------------------------------------- the block --- */
void clock_block(uint32_t n)
{
    float fn = (float)n;
    uint32_t in = ext_in;
    blk_count++;
    /* MIDI's START / STOP / CONTINUE */
    if (ext_cmd) {
        uint8_t c = ext_cmd;
        ext_cmd = 0;
        if (c == CLK_STOP + 1) {
            playing = 0;
            seq_release();
            step = -1;
        } else {
            if (c == CLK_START + 1) {
                step = -1;
                step_tick = 0;
                arp_tick = 0;
            }
            playing = seq != 0;
        }
    }
    if (in != ext_taken) {               /* MIDI clock: its ticks are the ticks */
        uint32_t k = in - ext_taken;
        if (k > 24u)
            k = 24u;
        ext_taken = in;
        if (since_ext > 0.0f && since_ext < SR * 0.5f) {
            float gap = since_ext / (float)k;
            ext_spt = ext_spt > 0.0f ? ext_spt + (gap - ext_spt) * 0.1f : gap;
            bpm = SR * 60.0f / (24.0f * ext_spt);
        }
        since_ext = 0.0f;
        ext_last_blk = blk_count;
        while (k--)
            tick();
    } else if (!eng_clock_ext()) {       /* our own */
        spt = SR * 60.0f / (24.0f * bpm);
        acc += fn;
        while (acc >= spt) {
            acc -= spt;
            tick();
        }
    }
    since_ext += fn;
    /* the gates */
    if (swing_wait >= 0.0f) {
        swing_wait -= fn;
        if (swing_wait < 0.0f && playing && seq)
            seq_fire(swing_step);
    }
    if (on_left >= 0.0f) {
        on_left -= fn;
        if (on_left < 0.0f)
            seq_release();
    }
    if (arp_left >= 0.0f) {
        arp_left -= fn;
        if (arp_left < 0.0f)
            arp_release();
    }
}

/* ---------------------------------------------------------------- the calls --- */
void eng_seq_bind(const seq_t *s) { seq = s; }
void eng_seq_set(int b, int r, float g, int tr)
{
    bars = (uint8_t)(b < 1 ? 1 : b > SEQ_BARS ? SEQ_BARS : b);
    rate = (uint8_t)(r >= 0 && r < NRATE ? r : RATE_16);
    gate = fm_clampf(g, 0.05f, 1.0f);
    transpose = (int8_t)tr;
}
void eng_play(int on)
{
    uint32_t f = plat_lock();
    if (on) {
        step = -1;
        step_tick = 0;
        acc = spt;                       /* the first step at once */
        playing = 1;
    } else {
        playing = 0;
        swing_wait = -1.0f;
        seq_release();
        step = -1;
    }
    plat_unlock(f);
}
int eng_playing(void) { return playing; }
int eng_seq_step(void) { return playing ? step : -1; }
int eng_seq_nearest(void)
{
    int st = step;
    if (!playing || st < 0)
        return -1;
    if (step_tick * 2 >= RATE_TICKS[rate])   /* past half the step: the next one */
        st = st + 1 >= seq_len() ? 0 : st + 1;
    return st;
}
void eng_tempo(float b, float sw)
{
    bpm = fm_clampf(b, 30.0f, 300.0f);
    swing = fm_clampf(sw, 0.0f, 0.5f);
}
float eng_bpm(void) { return bpm; }
void eng_clock(int what)
{
    if (what == CLK_TICK)
        ext_in++;
    else
        ext_cmd = (uint8_t)(what + 1);
}
int eng_clock_ext(void) { return ext_last_blk && blk_count - ext_last_blk < 172u; }   /* a clock within 0.5 s */
uint32_t eng_beats(void) { return ticks / 24u; }

void eng_arp(int on, int mode, int r, int oct, float g)
{
    uint32_t f = plat_lock();
    if (!on && arp_on_)
        arp_release();
    if (on && !arp_on_) {
        arp_tick = (uint8_t)(RATE_TICKS[r >= 0 && r < NRATE ? r : RATE_16] - 1);   /* the first note at once */
        arp_idx = -1;
        arp_dir = 1;
    }
    arp_on_ = (uint8_t)(on != 0);
    arp_mode = (uint8_t)(mode >= 0 && mode < NARP ? mode : ARP_UP);
    arp_rate = (uint8_t)(r >= 0 && r < NRATE ? r : RATE_16);
    arp_oct = (uint8_t)(oct < 1 ? 1 : oct > 4 ? 4 : oct);
    arp_gate = fm_clampf(g, 0.05f, 1.0f);
    if (!on) {
        ak_n = ak_down = 0;
    }
    plat_unlock(f);
}

void eng_arp_key(int note, int down, float vel)
{
    uint32_t f = plat_lock();
    int i;
    if (down) {
        if (ak_down == 0 && arp_hold)    /* HOLD: a new chord after every finger was off */
            ak_n = 0;
        if (ak_n == 0) {                 /* the first note: at once */
            arp_tick = (uint8_t)(RATE_TICKS[arp_rate] - 1);
            arp_idx = -1;
            arp_dir = 1;
        }
        ak_down++;
        for (i = 0; i < ak_n; i++)
            if (ak_note[i] == note)
                break;
        if (i == ak_n && ak_n < AK) {
            ak_note[ak_n] = (int8_t)note;
            ak_vel[ak_n] = vel;
            ak_n++;
        }
    } else {
        if (ak_down)
            ak_down--;
        if (!arp_hold) {
            for (i = 0; i < ak_n; i++)
                if (ak_note[i] == note) {
                    for (; i + 1 < ak_n; i++) {
                        ak_note[i] = ak_note[i + 1];
                        ak_vel[i] = ak_vel[i + 1];
                    }
                    ak_n--;
                    break;
                }
        }
    }
    plat_unlock(f);
}

void eng_arp_hold(int on)
{
    uint32_t f = plat_lock();
    arp_hold = (uint8_t)(on != 0);
    if (!on && ak_down == 0)
        ak_n = 0;
    plat_unlock(f);
}
int eng_arp_on(void) { return arp_on_; }
int eng_arp_note(void) { return arp_on_ ? arp_shown : -1; }
uint32_t eng_arp_steps(void) { return arp_steps_n; }

#ifdef P67_PLUGIN
/* the plugin: the host's transport is the clock (its position in quarter notes, while it plays) */
void eng_clock_pos(double ppq)
{
    double t = ppq * 24.0;
    uint32_t want = (uint32_t)t;
    if (want != ticks && (want > ticks + 1u || want + 1u < ticks)) {   /* (moved: the host jumped or started) */
        ticks = want;
        acc = (float)(t - (double)want) * spt;
    }
}
#endif
