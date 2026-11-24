/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* PROPHET 67's effects, in the Prophet-6's order: the analogue distortion, then FX A (a BBD-style stereo chorus), then
 * FX B, a reverb: ROOM, HALL, PLATE and SPRING as the Prophet-6's unit B has, and SPACE, an ambient reverb after the
 * Meris Mercury7 (modulated, diffuse, very long, with SWELL, SHIMMER and PREDELAY).
 *
 * The reverbs share one core: input diffusion (allpasses), eight delay lines with a Householder feedback matrix, each
 * line damped and gently modulated. The types differ in their line lengths, diffusion, damping, modulation and what
 * comes before the lines (a spring's chirping dispersion; SPACE's swell, shimmer and long predelay). One type runs at a
 * time, so they share the memory. */
#include "core.h"
#include "plat.h"

/* ------------------------------------------------------------ parameters --- */
static struct {
    float dist, dist_g, dist_c;          /* DISTORT: amount, drive, level after */
    float ch_mix, ch_hz, ch_depth;
    uint8_t type;
    float mix, t60, damp_hz, low_hz, size, diff, mod, mod_hz, lap_g;
    float shim, swell_k, pre;
    float dl_mix, dl_fb, dl_depth, dl_hz, dl_tk;
    uint8_t dl_vl, dl_vr, dl_sync;
} F;
static uint8_t type_now = 0xFF;
#ifdef P67_PLUGIN
/* the plugin: the levels and depths as the effects get them, gliding to the patch's in about 4 ms (FS), the
 * predelay in fractions of a sample gliding in 30 ms, and a new reverb type crossfaded: the old one out in 15 ms,
 * the lines cleared and set, the new one in (rv_xf) */
static struct {
    float dist, dist_g, dist_c, ch_mix, ch_depth, dl_mix, dl_fb, dl_depth, mix, shim, pre;
    uint8_t init;
} FS, FA;                                /* (FA: the glide's first stage) */
static uint8_t type_want = 0xFF, rv_live, rv_bq;
static float rv_b0, rv_b1, rv_b2, rv_a1, rv_a2, rv_z1, rv_z2;   /* the reverb's input filter above 60 kHz */
static struct {
    float t60, damp_hz, low_hz, size, diff, mod, lap_g, mod_hz, shim, swell_k, pre;
} FT, FO;                                /* the type's numbers: wanted, and the ones sounding */
static float rv_xf = 1.0f;
#define FQ FS
#define RV_NOW type_now
#else
#define FQ F
#define RV_NOW F.type
#endif

/* --------------------------------------------------------------- memory --- */
#ifdef P67_PLUGIN
#define CHN 4096u
#else
#define CHN 1024u
#endif                        /* chorus lines (23 ms) */
static float ch_l[CHN] PLAT_POOL, ch_r[CHN] PLAT_POOL;
static uint32_t ch_w;
static float ch_ph, ch_lpl, ch_lpr;

/* The reverb's core (diffusers, lines, shimmer) runs at half the rate, 22.05 kHz, as the classic digital reverbs
 * did: everything it keeps is below 10 kHz anyway (its damping), and it costs half. Its input is decimated (a 3-tap
 * half-band), its output interpolated, then rounded off. The predelay, the spring's dispersion and the swell run at
 * the full rate. Lengths below are in the core's samples. */
#define RSR (SR * 0.5f)
#define NL 8
#ifdef P67_PLUGIN                        /* (room for 192 kHz) */
#define LMAX 9600u
#else
#define LMAX 2200u
#endif                       /* a line at most (100 ms) */
static float rv_line[NL][LMAX] PLAT_POOL;
static uint32_t rv_len[NL], rv_w[NL];
static float rv_lp[NL], rv_g[NL], rv_mph[NL];
static const uint16_t LBASE[NL] = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116};   /* mutually prime-ish */
static const float LMHZ[NL] = {0.31f, 0.43f, 0.37f, 0.53f, 0.29f, 0.47f, 0.41f, 0.59f};

#define NAP 4
#ifdef P67_PLUGIN
#define APMAX 2400u
#else
#define APMAX 560u
#endif
static float ap_buf[NAP][APMAX] PLAT_POOL;
static uint32_t ap_len[NAP], ap_w[NAP];
static const uint16_t APBASE[NAP] = {142, 107, 379, 277};

/* inside the loop, one allpass per line: every echo smeared into many, so no single mode rings (a plate's density,
 * a hall's smoothness) */
#ifdef P67_PLUGIN
#define LAPMAX 1400u
#else
#define LAPMAX 320u
#endif
#ifdef P67_PLUGIN
#define LAP2MAX 840u
#else
#define LAP2MAX 192u
#endif
static float lap_buf[NL][LAPMAX] PLAT_POOL, lap2_buf[NL][LAP2MAX] PLAT_POOL;
static uint32_t lap_len[NL], lap_w[NL], lap2_len[NL], lap2_w[NL];
static const uint16_t LAPBASE[NL] = {113, 149, 173, 197, 223, 251, 277, 307};
static const uint16_t LAP2BASE[NL] = {67, 41, 89, 53, 97, 61, 79, 47};

#ifdef P67_PLUGIN
#define PREN 65536u
#else
#define PREN 8192u
#endif                       /* predelay: up to 185 ms */
static float pre_buf[PREN] PLAT_POOL;
static uint32_t pre_w, pre_d;

#ifdef P67_PLUGIN
#define SHN 16384u
#else
#define SHN 2048u
#endif                        /* the shimmer's pitch shifter */
static float sh_buf[SHN] PLAT_POOL;
static uint32_t sh_w;
static float sh_d, sh_fb;

static float sp_x[12];

/* THE DELAY, after the Walrus Meraki (its idea, not its code): two lines, each its own time (L, R), REPEATS through a
 * warm path (TONE's low-pass, the lows thinned, a soft saturation that lets the top end run away gently), a MOD of
 * the times (DEPTH, RATE; the two sides a quarter apart, and a slow analogue wander), times that glide when changed
 * (a tape's pitch bend, never a click). Like the reverb's core it runs at 22.05 kHz, its lines 16-bit: 1.2 s each */
#ifdef P67_PLUGIN
#define DLN 131072u
#else
#define DLN 26624u
#endif
#ifdef P67_PLUGIN
static float dl_buf[2][DLN];             /* (the plugin: floats, no 16-bit floor under the repeats) */
#define DL_Q 1.0f
#else
static int16_t dl_buf[2][DLN] PLAT_POOL;
#define DL_Q 16384.0f
#endif
static uint32_t dl_w, dl_wn;
static float dl_d[2], dl_lp[2], dl_hp[2], dl_xp[2], dl_y[2], dl_wet[2], dl_ph, dl_wow[2], dl_wowt[2], dl_wt[2], dl_lvl;
static uint32_t dl_seed = 0x2468ACE1u;
static float dc_x1, dc_x2, core_l, core_r;   /* the rate change */                   /* the spring's dispersion: first-order allpasses */
static float sw_g = 1.0f, sw_fast, sw_slow;   /* SWELL */
static float hp_in, lp_out_l, lp_out_r, tail;
static float dist_lpl, dist_lpr;
static uint32_t mod_c;

/* the lines' safety: unity for the levels a reverb lives at, rounding off above 1 and never past 2 (a held SPACE
 * fed again and again stays bounded) */
INL float soft2(float x)
{
    float h = x * 0.5f, h2;
    if (x < 0.8f && x > -0.8f)           /* (nearly always: as it is) */
        return x;
    if (h > 3.0f)
        return 2.0f;
    if (h < -3.0f)
        return -2.0f;
    h2 = h * h;
    return 2.0f * h * (27.0f + h2) / (27.0f + 9.0f * h2);
}

/* ---------------------------------------------------------- the patch --- */
static const float LAPS[NRV] = {1.0f, 1.0f, 1.8f, 1.0f, 1.6f};   /* the in-loop diffusion's sizes */
#ifndef P67_PLUGIN
static void rv_clear(void);
static void fx_type(uint8_t t);
static void fx_gains(void);
#endif

void fx_patch(const uint8_t *p)
{
    float d = u7(p[P_DIST]), dv = u7(p[P_RV_DECAY]), tone = bip(p[P_RV_TONE]);
    static const float T_LO[NRV] = {0.25f, 0.9f, 0.5f, 0.7f, 2.0f}, T_HI[NRV] = {2.5f, 9.0f, 6.0f, 4.0f, 70.0f};
    static const float SIZE[NRV] = {0.42f, 1.0f, 0.85f, 0.38f, 2.6f};
    static const float DIFF[NRV] = {0.62f, 0.68f, 0.62f, 0.45f, 0.72f};
    static const float DAMP[NRV] = {5200.0f, 6000.0f, 5800.0f, 4200.0f, 7000.0f};
    static const float LOWC[NRV] = {120.0f, 80.0f, 110.0f, 220.0f, 60.0f};
    static const float LAPG[NRV] = {0.0f, 0.45f, 0.65f, 0.0f, 0.5f};    /* the in-loop diffusion */
    static const float PRE[NRV] = {0.004f, 0.022f, 0.008f, 0.0f, 0.0f};
    uint8_t t = p[P_RV_TYPE] < NRV ? p[P_RV_TYPE] : RV_HALL;
    F.dist = d;
    F.dist_g = 1.0f + 14.0f * d * d;
    F.dist_c = 1.0f / (1.0f + 2.5f * d);
    F.ch_mix = u7(p[P_CH_MIX]);
    F.ch_hz = 0.08f * fm_exp2f((float)p[P_CH_RATE] * (6.0f / 127.0f));   /* 0.08 .. 5 Hz */
    F.ch_depth = u7(p[P_CH_DEPTH]);
    F.dl_mix = u7(p[P_DL_MIX]);
    F.dl_fb = 1.08f * fm_powf(u7(p[P_DL_FB]), 0.8f);   /* past 0.95 at the top: it runs away, held by the saturation */
    F.dl_depth = u7(p[P_DL_DEPTH]) * u7(p[P_DL_DEPTH]) * 0.004f * RSR;   /* up to 4 ms */
    F.dl_hz = 0.05f * fm_exp2f((float)p[P_DL_RATE] * (6.64f / 127.0f));   /* 0.05 .. 5 Hz */
    F.dl_tk = 1.0f - fm_expf(-6.2831853f * 2600.0f * fm_exp2f(bip(p[P_DL_TONE]) * 1.5f) / RSR);   /* 0.9 .. 7 kHz */
    F.dl_vl = p[P_DL_TIME_L];
    F.dl_vr = p[P_DL_TIME_R];
    F.dl_sync = p[P_DL_SYNC];
    F.type = t;
    F.mix = u7(p[P_RV_MIX]);
    F.t60 = T_LO[t] * fm_exp2f(dv * fm_log2f(T_HI[t] / T_LO[t]));
    if (t == RV_SPACE && p[P_RV_DECAY] >= 127)
        F.t60 = 1e6f;                    /* SPACE at the top: it holds (only the damping takes the highs) */
    if (t == RV_SPACE && p[P_SP_SHIM] && F.t60 > 30.0f)
        F.t60 = 30.0f;                   /* (with the shimmer feeding it, a hold would only grow) */
    F.damp_hz = DAMP[t] * fm_exp2f(tone * 1.6f);
    F.low_hz = LOWC[t] * fm_exp2f(-tone * 0.8f);
    F.size = SIZE[t];
    F.diff = DIFF[t];
    F.mod = (t == RV_SPACE ? 2.0f + 22.0f * u7(p[P_SP_MOD]) : t == RV_HALL ? 3.0f : t == RV_PLATE ? 12.0f : 0.6f)
            * SRS;                       /* (the lines' wobble: core samples at 22.05 kHz, the same time at any rate) */
    F.lap_g = LAPG[t];
    F.mod_hz = t == RV_SPACE ? 0.25f + 1.2f * u7(p[P_SP_MOD]) : 0.5f;
    F.shim = t == RV_SPACE ? u7(p[P_SP_SHIM]) * 0.45f : 0.0f;
    F.swell_k = t == RV_SPACE && p[P_SP_SWELL] ? k_of(0.03f + 3.0f * u7(p[P_SP_SWELL]) * u7(p[P_SP_SWELL])) : 1.0f;
    F.pre = t == RV_SPACE ? 0.18f * u7(p[P_SP_PRE]) : PRE[t];
#ifdef P67_PLUGIN
    type_want = t;
    if (type_now == 0xFF)                /* (the first: at once) */
        fx_type(t);
    /* what belongs to the type waits with it: the old type fades out with its own (a line's wobble or diffusion
     * changed under the sound it holds would click) */
    FT.t60 = F.t60, FT.damp_hz = F.damp_hz, FT.low_hz = F.low_hz, FT.size = F.size, FT.diff = F.diff;
    FT.mod = F.mod, FT.lap_g = F.lap_g, FT.mod_hz = F.mod_hz, FT.shim = F.shim, FT.swell_k = F.swell_k, FT.pre = F.pre;
    if (type_want != type_now) {
        F.t60 = FO.t60, F.damp_hz = FO.damp_hz, F.low_hz = FO.low_hz, F.size = FO.size, F.diff = FO.diff;
        F.mod = FO.mod, F.lap_g = FO.lap_g, F.mod_hz = FO.mod_hz, F.shim = FO.shim, F.swell_k = FO.swell_k, F.pre = FO.pre;
    } else {
        FO = FT;
    }
#else
    if (t != type_now)
        fx_type(t);
#endif
    fx_gains();
    pre_d = (uint32_t)(F.pre * SR);
    if (pre_d > PREN - 2u)
        pre_d = PREN - 2u;
}

/* a reverb type's line lengths (the lines cleared) */
static void fx_type(uint8_t t)
{
    {                                    /* a new type: its line lengths (the render clears the lines) */
        int i;
        for (i = 0; i < NL; i++) {
            uint32_t L = (uint32_t)((float)LBASE[i] * F.size * 0.5f * SRS);
            if (L > LMAX - 20u)
                L = LMAX - 20u;
            rv_len[i] = L;
        }
        for (i = 0; i < NAP; i++) {
            uint32_t L = (uint32_t)((float)APBASE[i] * 0.5f * SRS * (t == RV_SPACE ? 2.4f : t == RV_PLATE ? 1.7f : 1.0f));
            ap_len[i] = L < APMAX ? L : APMAX - 1u;
        }
        for (i = 0; i < NL; i++) {
            uint32_t L = (uint32_t)((float)LAPBASE[i] * 0.5f * SRS * LAPS[t]);
            lap_len[i] = L < LAPMAX ? L : LAPMAX - 1u;
            L = (uint32_t)((float)LAP2BASE[i] * 0.5f * SRS * LAPS[t]);
            lap2_len[i] = L < LAP2MAX ? L : LAP2MAX - 1u;
        }
        type_now = t;
        rv_clear();                      /* (the delay's repeats go on) */
    }
}

static void fx_gains(void)
{
    int i;
    for (i = 0; i < NL; i++)             /* each line loses 60 dB in T60 */
        rv_g[i] = fm_exp2f(-9.965784f * (float)rv_len[i] / (F.t60 * RSR));
}

static void rv_clear(void)
{
    uint32_t i, k;
    for (k = 0; k < NL; k++) {
        for (i = 0; i < LMAX; i++)
            rv_line[k][i] = 0.0f;
        rv_lp[k] = 0.0f;
    }
    for (k = 0; k < NAP; k++)
        for (i = 0; i < APMAX; i++)
            ap_buf[k][i] = 0.0f;
    for (k = 0; k < NL; k++) {
        for (i = 0; i < LAPMAX; i++)
            lap_buf[k][i] = 0.0f;
        for (i = 0; i < LAP2MAX; i++)
            lap2_buf[k][i] = 0.0f;
    }
    for (i = 0; i < PREN; i++)
        pre_buf[i] = 0.0f;
    for (i = 0; i < SHN; i++)
        sh_buf[i] = 0.0f;
    sh_fb = tail = 0.0f;
}

void fx_clear(void)
{
    uint32_t i;
    rv_clear();
    for (i = 0; i < CHN; i++)
        ch_l[i] = ch_r[i] = 0.0f;
    for (i = 0; i < DLN; i++)
        dl_buf[0][i] = dl_buf[1][i] = 0;
    for (i = 0; i < 12u; i++)
        sp_x[i] = 0.0f;
    sh_fb = tail = 0.0f;
}

void fx_init(void)
{
#ifdef P67_PLUGIN
    rv_bq = SR > 60000.0f;
    if (rv_bq) {                         /* a 2-pole low-pass, -6 dB at 11 kHz, -3 dB at 8 kHz (Q 0.5) */
        float w = fm_tanf(3.1415927f * 8000.0f / SR), q = 0.5f, nn = 1.0f / (1.0f + w / q + w * w);
        rv_b0 = w * w * nn;
        rv_b1 = 2.0f * rv_b0;
        rv_b2 = rv_b0;
        rv_a1 = 2.0f * (w * w - 1.0f) * nn;
        rv_a2 = (1.0f - w / q + w * w) * nn;
    }
#endif
    uint8_t p[NPARAM] = {0};
    p[P_RV_TYPE] = RV_HALL;
    p[P_RV_TONE] = 64;
    fx_patch(p);
}

float fx_tail(void) { return tail; }

/* ----------------------------------------------------------- the pieces --- */
INL float rd_frac(const float *b, uint32_t mask_or_len, uint32_t w, float d, int pow2)
{
    float rp = (float)w - d;
    int32_t i;
    float f, a, c;
    uint32_t i0, i1;
    if (rp < 0.0f)
        rp += (float)mask_or_len + (pow2 ? 1.0f : 0.0f);
    i = (int32_t)rp;
    f = rp - (float)i;
    if (pow2) {
        i0 = (uint32_t)i & mask_or_len;
        i1 = (i0 + 1u) & mask_or_len;
    } else {
        i0 = (uint32_t)i;
        if (i0 >= mask_or_len)
            i0 -= mask_or_len;
        i1 = i0 + 1u >= mask_or_len ? 0u : i0 + 1u;
    }
    a = b[i0];
    c = b[i1];
    return a + (c - a) * f;
}

/* the shimmer: an octave up, two heads sliding through a window, crossfaded */
INL float shimmer(float x)
{
    const float D = fm_minf(1200.0f * SRS, (float)SHN * 0.45f);
    float d2, w1, w2, y;
    sh_buf[sh_w & (SHN - 1u)] = x;
    sh_d -= 1.0f;                         /* an octave up: the delay shrinks a sample a sample */
    if (sh_d < 0.0f)
        sh_d += D;
    d2 = sh_d + D * 0.5f;
    if (d2 >= D)
        d2 -= D;
    w1 = 1.0f - fm_fabsf(2.0f * sh_d / D - 1.0f);
    w2 = 1.0f - w1;
    y = w1 * rd_frac(sh_buf, SHN - 1u, sh_w, sh_d + 1.0f, 1) + w2 * rd_frac(sh_buf, SHN - 1u, sh_w, d2 + 1.0f, 1);
    sh_w++;
    return y;
}

/* ------------------------------------------------------------- the block --- */
/* (the effects run from the flash: their loops are small and stay in its cache) */
/* the reverb asleep: nothing going in and its tail gone below -100 dB for a second; it wakes with the next sound
 * (a held SPACE at DECAY 127 never sleeps) */
static uint8_t rv_sleep;
static uint32_t rv_quiet;
static float blk_in;

float fx_weight(void)                    /* in voices' worth of work (measured: engine.c's share-out uses it) */
{
    float w = 0.15f;
    if (F.dist > 0.0f)
        w += 0.15f;
    if (F.ch_mix > 0.0f)
        w += 0.35f;
    if (F.dl_mix > 0.0f)
        w += 0.45f;
    if (F.mix > 0.0f && !rv_sleep)
        w += F.type == RV_PLATE ? 2.4f : F.type == RV_SPACE ? (F.shim > 0.0f ? 2.6f : 2.2f)
             : F.type == RV_HALL ? 1.9f : 1.5f;
    return w;
}

void fx_process(float *l, float *r, uint32_t n)
{
    uint32_t s, k;
    float kd = 1.0f - fm_expf(-6.2831853f * fm_minf(F.damp_hz, 9500.0f) / RSR);
    float kl = 1.0f - fm_expf(-6.2831853f * F.low_hz / SR);
    float kdist = 1.0f - fm_expf(-6.2831853f * 9000.0f / SR);
    float ch_k = 1.0f - fm_expf(-6.2831853f * 7500.0f / SR);
    float wet_g = FQ.mix * 1.25f, dry_g = 1.0f - FQ.mix * FQ.mix * 0.55f;   /* (the plugin: per sample) */
    float tl = 0.0f;
    float mod_now[NL];
    /* tuned per sample at 44.1 kHz (the delay's and the reverb's cores at 22.05 kHz), the same at any rate: the
     * delay's glides and its lows, SWELL's followers, the reverb's output smoothing, the spring's dispersion */
    const float k_wow = k_sr(0.0003f), k_dglide = k_sr(0.0006f), k_dhp = k_sr(0.023f);
    const float k_swa = k_sr(0.05f), k_swr = k_sr(0.0006f), k_sws = k_sr(0.00008f), k_out = k_sr(0.75f);
    const float sp_a = SRS == 1.0f ? 0.62f : (1.0f - fm_tanf(3.1415927f * 3234.0f / SR)) / (1.0f + fm_tanf(3.1415927f * 3234.0f / SR));
    const uint32_t wow_n = (uint32_t)(2048.0f * SRS);
#ifdef P67_PLUGIN
    float xf_step = 1.0f / (0.015f * SR), xf_in = 1.0f / (0.04f * SR), pre_now, fs_k, fs_kd;
    {
        float kp = FS.init ? 1.0f - fm_expf(-(float)n / (0.03f * SR)) : 1.0f;
        fs_k = FS.init ? 1.0f - fm_expf(-1.0f / (0.002f * SR)) : 1.0f;   /* (the levels: per sample, below) */
        fs_kd = FS.init ? 1.0f - fm_expf(-1.0f / (0.005f * SR)) : 1.0f;
        FS.pre += (fm_minf(F.pre * SR, (float)PREN - 3.0f) - FS.pre) * kp;
        if (!FS.init) {
            FS.dist = F.dist, FS.dist_g = F.dist_g, FS.dist_c = F.dist_c, FS.ch_mix = F.ch_mix, FS.ch_depth = F.ch_depth;
            FS.dl_mix = F.dl_mix, FS.dl_fb = F.dl_fb, FS.dl_depth = F.dl_depth, FS.mix = F.mix, FS.shim = F.shim;
            FA = FS;
        }
        FS.init = 1;
    }
    if (type_want != type_now && rv_xf <= 0.0f) {   /* the old type gone: the new one's lines and numbers */
        F.t60 = FT.t60, F.damp_hz = FT.damp_hz, F.low_hz = FT.low_hz, F.size = FT.size, F.diff = FT.diff;
        F.mod = FT.mod, F.lap_g = FT.lap_g, F.mod_hz = FT.mod_hz, F.shim = FT.shim, F.swell_k = FT.swell_k, F.pre = FT.pre;
        FO = FT;
        fx_type(type_want);
        fx_gains();
        FS.pre = fm_minf(F.pre * SR, (float)PREN - 3.0f);   /* (the lines are empty: the predelay at once) */
    }
    pre_now = FS.pre;
    /* the reverb turned off and faded out: its lines emptied, so when it comes back it starts from silence and
     * never plays what it held from long ago (once, as it goes off; the chorus and the delay always run) */
    if (FS.mix <= 0.0f && rv_live) {
        rv_clear();
        lp_out_l = lp_out_r = core_l = core_r = 0.0f;
        rv_live = 0;
    }
    if (FS.mix > 0.0f)
        rv_live = 1;
#endif
    /* the lines' wobble, once a block (2.9 ms: fine for a few samples of slow movement) */
    for (k = 0; k < NL; k++) {
        rv_mph[k] += LMHZ[k] * F.mod_hz * (float)n / SR;
        if (rv_mph[k] >= 1.0f)
            rv_mph[k] -= 1.0f;
        mod_now[k] = F.mod * 0.5f * (1.0f + fm_sin_turns(rv_mph[k])) * 0.5f + 1.0f;
    }
    mod_c++;
    {                                    /* the delay's times (synced: the clock's notes, as it is now) */
        int c;
        for (c = 0; c < 2; c++) {
            uint8_t v = c ? F.dl_vr : F.dl_vl;
            float sec = F.dl_sync ? (float)DL_SYNC_TICKS[v * NDLS / 128u] * clock_spt() / SR
                                  : 0.02f * fm_exp2f((float)v * (5.9069f / 127.0f));   /* 20 ms .. 1.2 s */
            dl_wt[c] = fm_clampf(sec * RSR, 8.0f, (float)DLN - 160.0f);
            if (dl_d[c] <= 0.0f)
                dl_d[c] = dl_wt[c];
        }
    }
    blk_in = 0.0f;
    for (s = 0; s < n; s++)
        blk_in = fm_maxf(blk_in, fm_fabsf(l[s]) + fm_fabsf(r[s]));
    if (FQ.dl_mix > 0.0f)                 /* the delay's repeats go into the reverb too */
        blk_in = fm_maxf(blk_in, dl_lvl);
    dl_lvl = 0.0f;
    if (rv_sleep && blk_in > 1e-5f)
        rv_sleep = 0;                    /* something to reverberate: awake */
    for (s = 0; s < n; s++) {
        float L = l[s], R = r[s], x, sum, out_l, out_r, fb[NL];
#ifdef P67_PLUGIN
        float wg;
#define FG(f) FA.f += (F.f - FA.f) * fs_k, FS.f += (FA.f - FS.f) * fs_k   /* (two stages: an S-curve, no corner) */
#undef FG
#define FG(f) FA.f += (F.f - FA.f) * fs_kd, FS.f += (FA.f - FS.f) * fs_kd   /* (the distortion: 2 x 5 ms) */
        FG(dist);
        FG(dist_g);
        FG(dist_c);
#undef FG
#define FG(f) FA.f += (F.f - FA.f) * fs_k, FS.f += (FA.f - FS.f) * fs_k
        FG(ch_mix);
        FG(ch_depth);
        FG(dl_mix);
        FG(dl_fb);
        FG(dl_depth);
        FG(mix);
        FG(shim);
#undef FG
        if (fm_fabsf(F.mix - FS.mix) < 1e-6f) FS.mix = FA.mix = F.mix;   /* (close enough: there, so what is off is off) */
        if (fm_fabsf(F.dist - FS.dist) < 1e-6f) FS.dist = FA.dist = F.dist;
        if (fm_fabsf(F.ch_mix - FS.ch_mix) < 1e-6f) FS.ch_mix = FA.ch_mix = F.ch_mix;
        if (fm_fabsf(F.dl_mix - FS.dl_mix) < 1e-6f) FS.dl_mix = FA.dl_mix = F.dl_mix;
        wet_g = FS.mix * 1.25f;
        dry_g = 1.0f - FS.mix * FS.mix * 0.55f;
        if (type_want != type_now)
            rv_xf = fm_maxf(0.0f, rv_xf - xf_step);
        else if (rv_xf < 1.0f)
            rv_xf = fm_minf(1.0f, rv_xf + xf_in);   /* (the new type in a little slower: 40 ms) */
        wg = wet_g * rv_xf * rv_xf * (3.0f - 2.0f * rv_xf);   /* (smoothstep: no corner at either end) */
#else
        const float wg = wet_g;
#endif
        /* the analogue distortion */
        if (FQ.dist > 0.0f) {
#ifdef P67_PLUGIN
            float L0 = L, R0 = R, w = fm_minf(1.0f, FQ.dist * 127.0f);   /* (from off to the first step: blended in,
                                                                          * not switched; from there as it was) */
#endif
            L = fm_tanhf(L * FQ.dist_g) * FQ.dist_c;
            R = fm_tanhf(R * FQ.dist_g) * FQ.dist_c;
            dist_lpl += (L - dist_lpl) * kdist;
            dist_lpr += (R - dist_lpr) * kdist;
            L = L + (dist_lpl - L) * FQ.dist;   /* the fizz rounded off as it gets harder */
            R = R + (dist_lpr - R) * FQ.dist;
#ifdef P67_PLUGIN
            L = L0 + (L - L0) * w;
            R = R0 + (R - R0) * w;
#endif
        }
        /* FX A: the chorus, two bucket brigades swept by one triangle a quarter apart, their highs dulled */
#ifdef P67_PLUGIN
        if (1) {                         /* (the plugin: always running, so it fades in from what it holds) */
#else
        if (FQ.ch_mix > 0.0f) {
#endif
            float tl_ = ch_ph, tr_ = ch_ph + 0.25f, dl, dr, wl, wr;
            if (tr_ >= 1.0f)
                tr_ -= 1.0f;
            tl_ = 1.0f - 4.0f * fm_fabsf(tl_ - 0.5f);
            tr_ = 1.0f - 4.0f * fm_fabsf(tr_ - 0.5f);
            dl = (0.0065f + 0.0042f * FQ.ch_depth * tl_) * SR;
            dr = (0.0065f + 0.0042f * FQ.ch_depth * tr_) * SR;
            ch_l[ch_w & (CHN - 1u)] = L;
            ch_r[ch_w & (CHN - 1u)] = R;
            wl = rd_frac(ch_l, CHN - 1u, ch_w, dl, 1);
            wr = rd_frac(ch_r, CHN - 1u, ch_w, dr, 1);
            ch_w++;
            ch_lpl += (wl - ch_lpl) * ch_k;
            ch_lpr += (wr - ch_lpr) * ch_k;
            L = L * (1.0f - 0.35f * FQ.ch_mix) + ch_lpr * FQ.ch_mix * 0.85f;   /* crossed: wider */
            R = R * (1.0f - 0.35f * FQ.ch_mix) + ch_lpl * FQ.ch_mix * 0.85f;
            ch_ph += F.ch_hz / SR;
            if (ch_ph >= 1.0f)
                ch_ph -= 1.0f;
        }
#ifdef P67_PLUGIN
        if (1) {                         /* THE DELAY (the plugin: always running, as the hardware's) */
#else
        if (FQ.dl_mix > 0.0f) {           /* THE DELAY */
#endif
            float in[2];
            int c;
            in[0] = L;
            in[1] = R;
            if (s & 1u) {                /* its core, every second sample */
                dl_ph += F.dl_hz * 2.0f / SR;
                if (dl_ph >= 1.0f)
                    dl_ph -= 1.0f;
                for (c = 0; c < 2; c++) {
                    float x = 0.5f * (in[c] + dl_xp[c]), d, rp, f, y, a, b;
                    int32_t i0, q;
                    uint32_t j0, j1;
                    if (dl_wn == (uint32_t)c * wow_n)   /* the analogue wander: a new heading now and then (0.19 s) */
                        dl_wowt[c] = rnd_f(&dl_seed) * 0.00015f * RSR;
                    dl_wow[c] += (dl_wowt[c] - dl_wow[c]) * k_wow;   /* (gliding there: no step) */
                    dl_d[c] += (dl_wt[c] - dl_d[c]) * k_dglide;   /* a new time: glides there (~80 ms) */
                    d = dl_d[c] + dl_wow[c] + FQ.dl_depth * (1.0f + fm_sin_turns(dl_ph + 0.25f * (float)c)) * 0.5f;
                    rp = (float)dl_w - d;
                    if (rp < 0.0f)
                        rp += (float)DLN;
                    i0 = (int32_t)rp;
                    f = rp - (float)i0;
                    j0 = (uint32_t)i0 % DLN;
                    j1 = j0 + 1u >= DLN ? 0u : j0 + 1u;
                    a = (float)dl_buf[c][j0];
                    b = (float)dl_buf[c][j1];
                    y = (a + (b - a) * f) * (1.0f / DL_Q);
                    dl_lp[c] += (y - dl_lp[c]) * F.dl_tk;   /* TONE: the repeats darken as they go */
                    dl_hp[c] += (dl_lp[c] - dl_hp[c]) * k_dhp;   /* the lows thinned (80 Hz) */
                    f = (dl_lp[c] - dl_hp[c]) * FQ.dl_fb;
                    f = f / (1.0f + 0.25f * fm_fabsf(f));   /* warm: it rounds off as it builds up */
#ifdef P67_PLUGIN
                    (void)q;
                    dl_buf[c][dl_w] = fm_clampf(x + f, -2.0f, 2.0f);
#else
                    q = (int32_t)((x + f) * 16384.0f);
                    dl_buf[c][dl_w] = (int16_t)(q > 32767 ? 32767 : q < -32767 ? -32767 : q);
#endif
                    dl_wet[c] = 0.5f * (dl_lp[c] + dl_y[c]);   /* (half way: the rate back up, as the reverb) */
                    dl_y[c] = dl_lp[c];
                    dl_lvl = fm_maxf(dl_lvl, fm_fabsf(dl_lp[c]));
                    dl_xp[c] = in[c];
                }
                dl_w = dl_w + 1u >= DLN ? 0u : dl_w + 1u;
                dl_wn = dl_wn + 1u >= 2u * wow_n ? 0u : dl_wn + 1u;
            } else {
                dl_xp[0] = L;
                dl_xp[1] = R;
                dl_wet[0] = dl_y[0];
                dl_wet[1] = dl_y[1];
            }
            L = L * (1.0f - 0.45f * FQ.dl_mix) + dl_wet[0] * FQ.dl_mix * 1.1f;
            R = R * (1.0f - 0.45f * FQ.dl_mix) + dl_wet[1] * FQ.dl_mix * 1.1f;
        }
        if (FQ.mix <= 0.0f || rv_sleep) {
            l[s] = L * (FQ.mix > 0.0f ? dry_g : 1.0f);
            r[s] = R * (FQ.mix > 0.0f ? dry_g : 1.0f);
            continue;
        }
        /* FX B: the reverb's input: mono, the lows cut, predelayed */
        x = (L + R) * 0.5f;
        hp_in += (x - hp_in) * kl;
        x -= hp_in;
        if (RV_NOW == RV_SPACE) {        /* SWELL: after each new note the reverb fades in */
            float a = fm_fabsf(x);
            sw_fast += (a - sw_fast) * (a > sw_fast ? k_swa : k_swr);
            sw_slow += (a - sw_slow) * k_sws;
            if (F.swell_k < 1.0f && sw_fast > sw_slow * 2.2f + 0.02f && sw_g > 0.6f)
                sw_g = 0.0f;
            sw_g += (1.0f - sw_g) * F.swell_k;
            x = x * sw_g + sh_fb;        /* and the shimmer's octave goes round again */
        }
        pre_buf[pre_w & (PREN - 1u)] = x;
#ifdef P67_PLUGIN
        {
            uint32_t pi = (uint32_t)pre_now;
            float pf = pre_now - (float)pi, p0 = pre_buf[(pre_w - pi) & (PREN - 1u)];
            x = p0 + pf * (pre_buf[(pre_w - pi - 1u) & (PREN - 1u)] - p0);
        }
#else
        x = pre_buf[(pre_w - pre_d) & (PREN - 1u)];
#endif
        pre_w++;
        if (RV_NOW == RV_SPRING) {       /* a spring: dispersion, its chirp */
            for (k = 0; k < 12u; k++) {
                float y = -sp_a * x + sp_x[k];
                sp_x[k] = x + sp_a * y;
                x = y;
            }
        }
        /* down to the core's rate: a half-band of three taps over this sample and the two before; the core runs on
         * every second sample, the output in between interpolated */
        {
#ifdef P67_PLUGIN
            float xd;
            if (rv_bq) {                 /* (above 60 kHz: the 44.1 kHz half-band's response, cos^2 of the
                                          * frequency, as a biquad at the rate: the reverb fed the same at any rate) */
                float y = rv_b0 * x + rv_z1;
                rv_z1 = rv_b1 * x - rv_a1 * y + rv_z2;
                rv_z2 = rv_b2 * x - rv_a2 * y;
                xd = y;
            } else {
                xd = 0.25f * (x + dc_x2) + 0.5f * dc_x1;
            }
#else
            float xd = 0.25f * (x + dc_x2) + 0.5f * dc_x1;
#endif
            dc_x2 = dc_x1;
            dc_x1 = x;
            if (!(s & 1u)) {
                lp_out_l += (core_l - lp_out_l) * k_out;   /* the core's latest */
                lp_out_r += (core_r - lp_out_r) * k_out;
                tl = fm_maxf(tl, fm_fabsf(lp_out_l) + fm_fabsf(lp_out_r));
                l[s] = L * dry_g + lp_out_l * wg;
                r[s] = R * dry_g + lp_out_r * wg;
                continue;
            }
            x = xd;
        }
        for (k = 0; k < NAP; k++) {      /* diffusion: a burst of echoes into a wash */
            uint32_t ri = ap_w[k] >= ap_len[k] ? ap_w[k] - ap_len[k] : ap_w[k] + APMAX - ap_len[k];
            float dly = ap_buf[k][ri], v = x + F.diff * dly;
            ap_buf[k][ap_w[k]] = v;
            x = dly - F.diff * v;
            ap_w[k] = ap_w[k] + 1u >= APMAX ? 0u : ap_w[k] + 1u;
        }
        /* the eight lines: read (wobbling), damp, mix (Householder), write back with the input */
        sum = 0.0f;
        for (k = 0; k < NL; k++) {
            float d = (float)rv_len[k] - mod_now[k], y;
            y = rd_frac(rv_line[k], LMAX, rv_w[k], d, 0);
            rv_lp[k] += (y - rv_lp[k]) * kd;
            y = rv_lp[k];
            if (F.lap_g > 0.0f) {        /* the line's own diffusion */
                uint32_t ri = lap_w[k] >= lap_len[k] ? lap_w[k] - lap_len[k] : lap_w[k] + LAPMAX - lap_len[k];
                float dly = lap_buf[k][ri], v = y + F.lap_g * dly;
                lap_buf[k][lap_w[k]] = v;
                y = dly - F.lap_g * v;
                lap_w[k] = lap_w[k] + 1u >= LAPMAX ? 0u : lap_w[k] + 1u;
                if (RV_NOW != RV_PLATE)  /* (a second stage: the plate's density; the others are smooth without) */
                    goto lap_done;
                ri = lap2_w[k] >= lap2_len[k] ? lap2_w[k] - lap2_len[k] : lap2_w[k] + LAP2MAX - lap2_len[k];
                dly = lap2_buf[k][ri];
                v = y - F.lap_g * dly;   /* (the other sign: the two together colour nothing) */
                lap2_buf[k][lap2_w[k]] = v;
                y = dly + F.lap_g * v;
                lap2_w[k] = lap2_w[k] + 1u >= LAP2MAX ? 0u : lap2_w[k] + 1u;
            }
        lap_done:
            fb[k] = y * rv_g[k];
            sum += fb[k];
        }
        sum *= 0.25f;                    /* 2 / N */
        out_l = fb[0] - fb[2] + fb[4] - fb[6] + 0.5f * (fb[1] + fb[5]);
        out_r = fb[1] - fb[3] + fb[5] - fb[7] + 0.5f * (fb[2] + fb[6]);
        for (k = 0; k < NL; k++) {
            float in = (k & 1u) ? -x : x;
            rv_line[k][rv_w[k]] = soft2(fb[k] - sum + in * 0.35f);
            rv_w[k] = rv_w[k] + 1u >= LMAX ? 0u : rv_w[k] + 1u;
        }
        out_l *= 0.42f;
        out_r *= 0.42f;
        if (RV_NOW == RV_SPACE && FQ.shim > 0.0f)
            sh_fb = shimmer((out_l + out_r) * 0.5f) * FQ.shim;
        else
            sh_fb = 0.0f;
        lp_out_l += (0.5f * (out_l + core_l) - lp_out_l) * k_out;   /* half way from the last to this (a half
                                                                       * sample late: linear interpolation) */
        lp_out_r += (0.5f * (out_r + core_r) - lp_out_r) * k_out;
        core_l = out_l;
        core_r = out_r;
        tl = fm_maxf(tl, fm_fabsf(lp_out_l) + fm_fabsf(lp_out_r));
        l[s] = L * dry_g + lp_out_l * wg;
        r[s] = R * dry_g + lp_out_r * wg;
    }
#ifdef P67_PLUGIN                        /* (any block size: its fall and the second of quiet in samples) */
    tail = tl > tail ? tl : tail * fm_exp2f(-0.0291463f * (float)n / (128.0f * SRS));
    if (FQ.mix > 0.0f && !rv_sleep && tail < 1e-5f && blk_in < 1e-5f) {   /* going quiet */
        rv_quiet += n;
        if (rv_quiet > (uint32_t)(344.0f * 128.0f * SRS))
            rv_sleep = 1;
#else
    tail = tl > tail ? tl : tail * 0.98f;
    if (FQ.mix > 0.0f && !rv_sleep && tail < 1e-5f && blk_in < 1e-5f) {   /* going quiet */
        if (++rv_quiet > 344u)
            rv_sleep = 1;                /* (what is left in its lines is below -100 dB: it wakes from that) */
#endif
    } else {
        rv_quiet = 0;
    }
    for (k = 0; k < NL; k++)
        rv_lp[k] = flush_dn(rv_lp[k]);
    hp_in = flush_dn(hp_in);
    sh_fb = flush_dn(sh_fb);
}
