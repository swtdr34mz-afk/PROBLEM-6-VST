/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* PROPHET 67's engine tests: the engine (src/P67Engine.*, dsp/) driven as the plugin drives it (the block split at
 * each MIDI event, any buffer size), offline, without JUCE.
 *
 *   engine_tests            all tests; exit code 0 when all pass
 *   engine_tests bench      the CPU figures only
 *   engine_tests render <preset> <sr> <out.wav>   a preset's demo phrase, to listen to
 *
 * Built by CMake (-DP67_TESTS=ON) or by hand: see tests/CMakeLists.txt. */
#include "P67Engine.h"
#include "vst_presets.h"
#include "P6Import.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <functional>
#include <string>
#include <vector>
#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

namespace {
/* ------------------------------------------------------------------ a phrase: events at sample times --- */
struct Ev {
    int at;                                  /* sample */
    enum { ON, OFF, BEND, MOD, PRESS, PRESS_ALL, SUS, PATCH, ALLOFF } type;
    int a;
    float f;
    std::vector<uint8_t> patch;
};
using Phrase = std::vector<Ev>;
struct Patch {
    uint8_t p[NPARAM];
    int hpVel = 0, hpKt = 0, fenvVel = 0;
};
Patch preset(int i)                          /* 0..15 the factory's, 16.. the VST's own (vst_presets.c) */
{
    Patch pt;
    std::memcpy(pt.p, i < NPRESETS ? FACTORY[i].p : VST_PRESETS[i - NPRESETS].p, NPARAM);
    pt.hpVel = pt.p[P_FVEL];
    pt.hpKt = pt.p[P_KEYTRK];
    return pt;
}
Patch dry(Patch pt)                          /* the effects off */
{
    pt.p[P_RV_MIX] = pt.p[P_CH_MIX] = pt.p[P_DL_MIX] = pt.p[P_DIST] = 0;
    return pt;
}

struct Out {
    std::vector<float> L, R;
    int sr;
    double seconds;                          /* the render's own time */
    int voicesAtEnd;
};

/* renders as PluginProcessor::processBlock does: blocks of 'buf' (a pattern of sizes, cycled), each split at its
 * events */
Out render(const Patch &pt, int sr, int len, const Phrase &ph, std::vector<int> bufs = {128},
           std::function<void(P67Engine &, int)> perBlock = nullptr)
{
#if defined(__SSE__) || defined(_M_X64)
    _mm_setcsr(_mm_getcsr() | 0x8040);       /* flush to zero, denormals are zero: as the plugin runs (JUCE's
                                              * ScopedNoDenormals in processBlock) */
#endif
    Out o;
    o.sr = sr;
    o.L.assign(len, 0.0f);
    o.R.assign(len, 0.0f);
    P67Engine e;
    e.prepare(sr);
    e.hpExtra(pt.hpVel, pt.hpKt, pt.fenvVel);
    e.patch(pt.p);
    size_t ei = 0, bi = 0;
    int pos = 0;
    auto t0 = std::chrono::steady_clock::now();
    while (pos < len) {
        int n = std::min(bufs[bi++ % bufs.size()], len - pos);
        if (perBlock)
            perBlock(e, pos);
        int done = 0;
        while (ei < ph.size() && ph[ei].at < pos + n) {
            const Ev &ev = ph[ei++];
            int at = std::max(0, ev.at - pos);
            if (at > done) {
                e.render(&o.L[pos + done], &o.R[pos + done], at - done);
                done = at;
            }
            switch (ev.type) {
            case Ev::ON: e.noteOn(ev.a, ev.a, ev.f, 1.0f); break;
            case Ev::OFF: e.noteOff(ev.a); break;
            case Ev::BEND: e.bend(ev.f); break;
            case Ev::MOD: e.modWheel(ev.f); break;
            case Ev::PRESS: e.press(ev.a, ev.f); break;
            case Ev::PRESS_ALL: e.pressAll(ev.f); break;
            case Ev::SUS: e.sustain(ev.a != 0); break;
            case Ev::PATCH: e.patch(ev.patch.data()); break;
            case Ev::ALLOFF: e.allOff(); break;
            }
        }
        if (n > done)
            e.render(&o.L[pos + done], &o.R[pos + done], n - done);
        pos += n;
    }
    o.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    o.voicesAtEnd = e.voices();
    return o;
}

int S(double sec, int sr) { return (int)std::lround(sec * sr); }

/* a phrase every preset can show itself in: a chord held, a melody over it, bend, the mod wheel, pressure, the
 * pedal, then silence for the tails */
Phrase demo(int sr)
{
    Phrase ph;
    auto on = [&](double t, int n, float v) { ph.push_back({S(t, sr), Ev::ON, n, v, {}}); };
    auto off = [&](double t, int n) { ph.push_back({S(t, sr), Ev::OFF, n, 0, {}}); };
    for (int n : {48, 55, 60, 64})
        on(0.0, n, 0.75f);
    for (int n : {48, 55, 60, 64})
        off(1.5, n);
    int mel[] = {72, 74, 76, 79, 77, 76, 74, 72};
    for (int i = 0; i < 8; i++) {
        on(1.6 + i * 0.18, mel[i], 0.5f + 0.06f * i);
        off(1.6 + i * 0.18 + 0.15, mel[i]);
    }
    for (int i = 0; i <= 20; i++)
        ph.push_back({S(3.2 + i * 0.01, sr), Ev::BEND, 0, (float)i / 20.0f, {}});
    for (int i = 0; i <= 20; i++)
        ph.push_back({S(3.6 + i * 0.01, sr), Ev::BEND, 0, 1.0f - (float)i / 20.0f, {}});
    on(3.1, 67, 0.9f);
    for (int i = 0; i <= 30; i++)
        ph.push_back({S(3.1 + i * 0.02, sr), Ev::PRESS, 67, (float)i / 30.0f, {}});
    for (int i = 0; i <= 30; i++)
        ph.push_back({S(3.2 + i * 0.02, sr), Ev::MOD, 0, (float)i / 30.0f, {}});
    ph.push_back({S(3.9, sr), Ev::SUS, 1, 0, {}});
    off(4.0, 67);
    on(4.1, 43, 0.8f);
    off(4.2, 43);
    ph.push_back({S(4.6, sr), Ev::SUS, 0, 0, {}});
    ph.push_back({S(4.6, sr), Ev::MOD, 0, 0.0f, {}});
    std::stable_sort(ph.begin(), ph.end(), [](const Ev &a, const Ev &b) { return a.at < b.at; });
    return ph;
}

/* ------------------------------------------------------------------------------------ measures --- */
double rms(const std::vector<float> &x, int a = 0, int b = -1)
{
    if (b < 0)
        b = (int)x.size();
    double s = 0;
    for (int i = a; i < b; i++)
        s += (double)x[i] * x[i];
    return std::sqrt(s / std::max(1, b - a));
}
double peak(const Out &o)
{
    double p = 0;
    for (size_t i = 0; i < o.L.size(); i++)
        p = std::max(p, (double)std::max(std::fabs(o.L[i]), std::fabs(o.R[i])));
    return p;
}
bool finite(const Out &o)
{
    for (size_t i = 0; i < o.L.size(); i++)
        if (!std::isfinite(o.L[i]) || !std::isfinite(o.R[i]))
            return false;
    return true;
}
double diffDb(const Out &a, const Out &b)    /* the difference of two renders, dB below the first */
{
    double d = 0, s = 0;
    size_t n = std::min(a.L.size(), b.L.size());
    for (size_t i = 0; i < n; i++) {
        d += (double)(a.L[i] - b.L[i]) * (a.L[i] - b.L[i]) + (double)(a.R[i] - b.R[i]) * (a.R[i] - b.R[i]);
        s += (double)a.L[i] * a.L[i] + (double)a.R[i] * a.R[i];
    }
    if (d == 0)
        return -400;
    return 10 * std::log10(d / std::max(s, 1e-30));
}
/* the energy above fc (a 4th-order high-pass, by two biquads), relative to the whole: what clicks and zipper add */
double hfRatioDb(const std::vector<float> &x, int sr, double fc, int a = 0, int b = -1)
{
    if (b < 0)
        b = (int)x.size();
    double w = std::tan(M_PI * fc / sr), q[2] = {0.5412, 1.3066}, e = 0, t = 0;
    std::vector<double> y(x.begin() + a, x.begin() + b);
    for (double qq : q) {
        double k = w, n = 1 / (1 + k / qq + k * k), b0 = n, b1 = -2 * n, b2 = n, a1 = 2 * (k * k - 1) * n,
               a2 = (1 - k / qq + k * k) * n, z1 = 0, z2 = 0;
        for (double &v : y) {
            double o = b0 * v + z1;
            z1 = b1 * v - a1 * o + z2;
            z2 = b2 * v - a2 * o;
            v = o;
        }
    }
    for (int i = a; i < b; i++)
        t += (double)x[i] * x[i];
    for (size_t i = y.size() / 10; i < y.size(); i++)
        e += y[i] * y[i];
    return 10 * std::log10(std::max(e, 1e-30) / std::max(t, 1e-30));
}
/* zipper: a parameter automated in steps of 10 ms makes the high end pulse at 100 Hz. The energy above 5 kHz in
 * 1 ms frames, its spectrum: the share at 100, 200, 300 Hz against its mean (a smooth change has none there) */
double zipperDb(const std::vector<float> &x, int sr, int a, int b)
{
    std::vector<float> hp(x.begin() + a, x.begin() + b);
    {   /* high-pass, 2nd order at 5 kHz, twice */
        double w = std::tan(M_PI * 5000.0 / sr), q = 0.7071;
        for (int pass = 0; pass < 2; pass++) {
            double k = w, nn = 1 / (1 + k / q + k * k), b0 = nn, b1 = -2 * nn, b2 = nn, a1 = 2 * (k * k - 1) * nn,
                   a2 = (1 - k / q + k * k) * nn, z1 = 0, z2 = 0;
            for (float &v : hp) {
                double o = b0 * v + z1;
                z1 = b1 * v - a1 * o + z2;
                z2 = b2 * v - a2 * o;
                v = (float)o;
            }
        }
    }
    int fl = sr / 1000, nf = (int)hp.size() / fl;
    std::vector<double> e(nf);
    double mean = 0;
    for (int i = 0; i < nf; i++) {
        double s2 = 0;
        for (int j = 0; j < fl; j++)
            s2 += (double)hp[i * fl + j] * hp[i * fl + j];
        e[i] = s2;
        mean += s2;
    }
    mean /= std::max(1, nf);
    if (mean <= 1e-30)
        return -200;
    double lines = 0;
    for (double f : {100.0, 200.0, 300.0}) {
        double re = 0, im = 0;
        for (int i = 0; i < nf; i++) {
            double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / (nf - 1)), ph = 2 * M_PI * f * i / 1000.0;
            re += w * (e[i] - mean) * std::cos(ph);
            im += w * (e[i] - mean) * std::sin(ph);
        }
        lines += (re * re + im * im) / ((double)nf * nf * 0.25 * 0.375 * 4);
    }
    return 10 * std::log10(std::max(lines, 1e-30) / (mean * mean));
}

/* the strongest frequency in a stretch (zero crossings of a band-limited signal are too fragile: a DFT peak, refined
 * by a parabola) */
double pitchHz(const std::vector<float> &x, int sr, int a, int n, double lo, double hi)
{
    double best = 0, bf = 0;
    auto mag = [&](double f) {
        double re = 0, im = 0;
        for (int i = 0; i < n; i++) {
            double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / (n - 1)), ph = 2 * M_PI * f * i / sr;
            re += w * x[a + i] * std::cos(ph);
            im += w * x[a + i] * std::sin(ph);
        }
        return re * re + im * im;
    };
    for (double f = lo; f <= hi; f *= 1.002) {
        double m = mag(f);
        if (m > best)
            best = m, bf = f;
    }
    for (double step = bf * 0.001; step > bf * 1e-7; step *= 0.5) {   /* golden enough: climb */
        double m1 = mag(bf - step), m2 = mag(bf + step);
        if (m1 > best)
            best = m1, bf -= step;
        else if (m2 > best)
            best = m2, bf += step;
    }
    return bf;
}
double cents(double f, double ref) { return 1200.0 * std::log2(f / ref); }

void wav(const char *fn, const Out &o)
{
    FILE *f = std::fopen(fn, "wb");
    if (!f)
        return;
    int n = (int)o.L.size(), bytes = n * 8;
    auto w32 = [&](int v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](short v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f);
    w32(36 + bytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16);
    w16(3);
    w16(2);
    w32(o.sr);
    w32(o.sr * 8);
    w16(8);
    w16(32);
    std::fwrite("data", 1, 4, f);
    w32(bytes);
    for (int i = 0; i < n; i++) {
        std::fwrite(&o.L[i], 4, 1, f);
        std::fwrite(&o.R[i], 4, 1, f);
    }
    std::fclose(f);
}

/* ------------------------------------------------------------------------------------ the tests --- */
int failures = 0, checks = 0;
void check(bool ok, const char *what, const std::string &detail = "")
{
    checks++;
    if (!ok)
        failures++;
    std::printf("  [%s] %s%s%s\n", ok ? " ok " : "FAIL", what, detail.empty() ? "" : "  ", detail.c_str());
}
std::string fmt(const char *f, ...)
{
    char b[512];
    va_list a;
    va_start(a, f);
    std::vsnprintf(b, sizeof b, f, a);
    va_end(a);
    return b;
}

const int RATES[] = {44100, 48000, 88200, 96000, 192000};

/* every preset, every rate: finite, bounded, silent at the end, every voice back */
void testPresets()
{
    std::puts("presets at every rate (finite, bounded, the voices end)");
    for (int sr : RATES)
        for (int i = 0; i < NPRESETS + NVST_PRESETS; i++) {
            Patch pt = preset(i);
            Out o = render(pt, sr, S(7.0, sr), demo(sr));
            Patch d = dry(pt);
            Out od = render(d, sr, S(24.0, sr), demo(sr), {512});   /* (the longest release: 12 s to -70 dB) */
            double tail = rms(od.L, S(23.5, sr));
            bool ok = finite(o) && peak(o) < 2.0 && od.voicesAtEnd == 0 && tail < 1e-5;
            if (!ok || sr == 44100)
                check(ok, fmt("%-12.12s %6d", i < NPRESETS ? FACTORY[i].name : VST_PRESETS[i - NPRESETS].name, sr).c_str(),
                      fmt("peak %.2f  voices left %d  dry tail %.1f dB", peak(o), od.voicesAtEnd, 20 * std::log10(tail + 1e-12)));
        }
}

/* the same phrase at any buffer size (and split by events anywhere) sounds the same */
void testBuffers()
{
    std::puts("buffer sizes (the output must not depend on them)");
    for (int sr : {44100, 96000})
        for (int i : {0, 6, 7, 9, 10}) {
            Patch pt = preset(i);
            Out ref = render(pt, sr, S(5.0, sr), demo(sr), {128});
            double worst = -400;
            for (auto b : std::vector<std::vector<int>>{{1}, {7}, {16}, {32}, {64}, {100}, {441}, {512}, {1024}, {2048},
                                                        {13, 256, 3, 77, 1000, 64}}) {
                Out o = render(pt, sr, S(5.0, sr), demo(sr), b);
                worst = std::max(worst, diffDb(ref, o));
            }
            check(worst < -120, fmt("%-12s %6d", FACTORY[i].name, sr).c_str(), fmt("worst difference %.0f dB", worst));
        }
}

/* the rates: a note's pitch, the LFO's speed, the glide's and the envelopes' times are the same at any rate */
void testRates()
{
    std::puts("sample rates (the sound must not depend on them)");
    /* pitch: osc 1 alone, a saw, the filter open */
    for (int sr : RATES) {
        Patch pt = dry(preset(NPRESETS - 1));
        pt.p[P_CUTOFF] = 255;
        pt.p[P_RESO] = 0;
        pt.p[P_MIX_O2] = 0;
        pt.p[P_O1_SUB] = 0;
        pt.p[P_VINTAGE] = 0;
        pt.p[P_SPREAD] = 0;
        pt.p[P_O1_SHAPE] = 0;    /* triangle: its fundamental clear */
        Phrase ph = {{0, Ev::ON, 69, 0.8f, {}}};
        Out o = render(pt, sr, S(1.0, sr), ph);
        double f = pitchHz(o.L, sr, S(0.3, sr), S(0.5, sr), 400, 480);
        check(std::fabs(cents(f, 440.0)) < 0.5, fmt("A4 at %6d", sr).c_str(), fmt("%.3f Hz (%+.2f cents)", f, cents(f, 440.0)));
    }
    /* LFO 1 (triangle, to osc pitch, 60): its rate, from the pitch's wobble: measured as the frequency of the pitch
     * modulation via the AMP destination instead (level is easier): AMP, triangle, rate 80 */
    std::vector<double> lfoHz;
    for (int sr : RATES) {
        Patch pt = dry(preset(NPRESETS - 1));
        pt.p[P_LFO_RATE] = 90;
        pt.p[P_LFO_AMT] = 127;
        pt.p[P_LFO_SHAPE] = LS_TRI;
        pt.p[P_LFO_DEST] = LD_AMP;
        pt.p[P_CUTOFF] = 255;
        pt.p[P_VINTAGE] = 0;
        pt.p[P_SPREAD] = 0;
        Phrase ph = {{0, Ev::ON, 69, 0.8f, {}}};
        Out o = render(pt, sr, S(3.0, sr), ph, {100});
        std::vector<float> env(o.L.size());
        double a = 0, k = 1 - std::exp(-2 * M_PI * 40.0 / sr);
        for (size_t i = 0; i < env.size(); i++)
            env[i] = (float)(a += (std::fabs(o.L[i]) - a) * k);
        lfoHz.push_back(pitchHz(env, sr, S(0.5, sr), S(2.4, sr), 2.0, 8.0));
    }
    double lo = *std::min_element(lfoHz.begin(), lfoHz.end()), hi = *std::max_element(lfoHz.begin(), lfoHz.end());
    /* the effects: a preset's level and its tail (the reverb's and the delay's, after the phrase) at every rate */
    for (int i : {0, 2, 10}) {
        double lv[5], tl[5];
        int k = 0;
        for (int sr : RATES) {
            Out o = render(preset(i), sr, S(7.0, sr), demo(sr), {256});
            lv[k] = 20 * std::log10(rms(o.L) + 1e-12);
            tl[k] = 20 * std::log10(rms(o.L, S(5.0, sr)) + 1e-12);
            k++;
        }
        double dl = *std::max_element(lv, lv + 5) - *std::min_element(lv, lv + 5);
        double dt = *std::max_element(tl, tl + 5) - *std::min_element(tl, tl + 5);
        check(dl < 1.0 && dt < 2.5, fmt("%-12s level and tail at every rate", FACTORY[i].name).c_str(),
              fmt("level within %.2f dB, tail within %.2f dB", dl, dt));
    }
    double nominal = 0.05 * std::pow(2.0, 90 * 9.2288 / 127.0);
    check(cents(hi, lo) < 2.0 && std::fabs(cents(lo, nominal)) < 5.0, "LFO 1 rate the same at every rate, as set",
          fmt("%.4f .. %.4f Hz (set: %.4f)", lo, hi, nominal));
}

/* automation: each continuous parameter swept while a chord holds; what the steps add above 7 kHz (the patch itself
 * has nothing there: triangles, the filter low) */
void testZipper(bool print)
{
    std::puts("automation (zipper and clicks while a parameter moves)");
    int params[] = {P_O1_SHAPE, P_O2_SHAPE, P_O1_PW, P_O2_PW, P_O1_SUB, P_MIX_O1, P_MIX_O2, P_NOISE, P_CUTOFF, P_RESO,
                    P_ENV_AMT, P_HPF, P_LFO_AMT, P_PM_OSC2, P_DIST, P_CH_MIX, P_RV_MIX, P_DL_MIX, P_SP_PRE, P_VINTAGE,
                    P_SPREAD, P_O2_FINE};
    int sr = 48000;
    for (int prm : params) {
        Patch pt = dry(preset(NPRESETS - 1));
        pt.p[P_O1_SHAPE] = pt.p[P_O2_SHAPE] = 0;
        pt.p[P_CUTOFF] = 110;
        pt.p[P_RESO] = 20;
        pt.p[P_ENV_AMT] = 64;
        pt.p[P_VINTAGE] = 0;
        pt.p[P_SPREAD] = 0;
        pt.p[P_AS] = 127;
        pt.p[P_RV_DECAY] = 40;
        if (prm == P_PM_OSC2)
            pt.p[P_PM_DOSC] = PMO_FREQ;
        if (prm == P_SP_PRE)
            pt.p[P_RV_TYPE] = RV_SPACE, pt.p[P_RV_MIX] = 60;
        Phrase ph;                       /* one note, A2: its beats (110 Hz and up) clear of the steps' 100, 200, 300 */
        ph.push_back({0, Ev::ON, 45, 0.8f, {}});
        Patch moving = pt;
        int lo = 0, hi = 127;
        if (prm == P_CUTOFF)
            lo = 60, hi = 160;
        if (prm == P_RESO)
            hi = 100;
        if (prm == P_DIST)
            hi = 60;
        for (int i = 0; i <= 200; i++) {     /* there and back in 2 s, a value every 10 ms (a host's automation) */
            double t = 0.5 + i * 0.01;
            int v = i <= 100 ? lo + (hi - lo) * i / 100 : hi - (hi - lo) * (i - 100) / 100;
            moving.p[prm] = (uint8_t)v;
            ph.push_back({S(t, sr), Ev::PATCH, 0, 0, std::vector<uint8_t>(moving.p, moving.p + NPARAM)});
        }
        Out o = render(pt, sr, S(3.0, sr), ph, {256});
        double hf = hfRatioDb(o.L, sr, 7000, S(0.5, sr), S(2.5, sr)), zp = zipperDb(o.L, sr, S(0.5, sr), S(2.5, sr));
        if (print)
            std::printf("    %-3d %-10s  HF %6.1f dB  zipper %6.1f dB\n", prm, PARAMS[prm].name, hf, zp);
        check(zp < -30 || hf < -90, fmt("sweep %-10s", PARAMS[prm].name).c_str(), fmt("zipper %.1f dB (HF %.1f dB)", zp, hf));
    }
}

/* the voices: a storm of notes, the pedal, unison on and off, the arp: nothing sticks, nothing breaks */
void testFuzz()
{
    std::puts("fuzz (random notes, pedal, unison, the arp; every voice must end)");
    for (int sr : {44100, 96000})
        for (int seed = 1; seed <= 6; seed++) {
            uint32_t r = 0x12345u * seed;
            auto rnd = [&]() { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return r; };
            Patch pt = dry(preset(seed % NPRESETS));
            Phrase ph;
            bool down[128] = {};
            Patch cur = pt;
            for (int t = 0; t < S(6.0, sr); t += 1 + (int)(rnd() % (uint32_t)(sr / 200))) {
                int k = rnd() % 100;
                int n = 24 + rnd() % 80;
                if (k < 40) {
                    ph.push_back({t, Ev::ON, n, 0.1f + (rnd() % 90) / 100.0f, {}});
                    down[n] = true;
                } else if (k < 75) {
                    ph.push_back({t, Ev::OFF, n, 0, {}});
                    down[n] = false;
                } else if (k < 80) {
                    ph.push_back({t, Ev::SUS, (int)(rnd() & 1), 0, {}});
                } else if (k < 83) {
                    if (!getenv("NOUNI"))
                        cur.p[P_UNISON] ^= 1;
                    ph.push_back({t, Ev::PATCH, 0, 0, std::vector<uint8_t>(cur.p, cur.p + NPARAM)});
                } else if (k < 86) {
                    cur.p[P_GLIDE] = rnd() % 128;
                    cur.p[P_LFO_RATE] = rnd() % 128;
                    cur.p[P_LFO_SHAPE] = rnd() % NLS;
                    cur.p[P_LFO_DEST] = rnd() % NLD;
                    cur.p[P_LFO_AMT] = rnd() % 128;
                    ph.push_back({t, Ev::PATCH, 0, 0, std::vector<uint8_t>(cur.p, cur.p + NPARAM)});
                } else if (k < 92) {
                    ph.push_back({t, Ev::BEND, 0, ((int)(rnd() % 201) - 100) / 100.0f, {}});
                } else if (k < 96) {
                    ph.push_back({t, Ev::PRESS, n, (rnd() % 100) / 100.0f, {}});
                } else {
                    ph.push_back({t, Ev::MOD, 0, (rnd() % 100) / 100.0f, {}});
                }
            }
            int end = S(6.0, sr);
            ph.push_back({end, Ev::SUS, 0, 0, {}});
            for (int n = 0; n < 128; n++)
                ph.push_back({end, Ev::OFF, n, 0, {}});
            Out o = render(pt, sr, S(30.0, sr), ph, {64, 333, 17});
            double tail = rms(o.L, S(29.0, sr));
            bool ok = finite(o) && peak(o) < 2.0 && o.voicesAtEnd == 0 && tail < 1e-5;
            check(ok, fmt("seed %d %6d", seed, sr).c_str(),
                  fmt("peak %.2f  voices left %d  tail %.1f dB", peak(o), o.voicesAtEnd, 20 * std::log10(tail + 1e-12)));
        }
}

/* the effects switched while a chord holds: the reverb's type, each effect off and on again: no click (the high end in
 * 2 ms frames around each switch, against the frames before it) */
void testFxSwitch()
{
    std::puts("effects switched under a held chord (no click)");
    int sr = 48000;
    Patch pt = preset(0);
    pt.p[P_O1_SHAPE] = pt.p[P_O2_SHAPE] = 0;
    pt.p[P_CUTOFF] = 110;
    pt.p[P_AS] = 127;
    pt.p[P_RV_MIX] = 80;
    pt.p[P_DL_MIX] = 60;
    pt.p[P_CH_MIX] = 60;
    Phrase ph;
    for (int n : {48, 55, 60})
        ph.push_back({0, Ev::ON, n, 0.8f, {}});
    Patch q = pt;
    std::vector<std::pair<double, const char *>> sw;
    double t = 1.0;
    auto at = [&](const char *what) {
        ph.push_back({S(t, sr), Ev::PATCH, 0, 0, std::vector<uint8_t>(q.p, q.p + NPARAM)});
        sw.push_back({t, what});
        t += 0.6;
    };
    if (getenv("SAMEPATCH")) {
        for (int i = 0; i < 6; i++) at("same patch");
    }
    for (int ty = 0; ty < NRV; ty++) {
        q.p[P_RV_TYPE] = (uint8_t)((pt.p[P_RV_TYPE] + 1 + ty) % NRV);
        at("reverb type");
    }
    q.p[P_RV_MIX] = 0;  at("reverb off");
    q.p[P_RV_MIX] = 80; at("reverb on");
    q.p[P_DL_MIX] = 0;  at("delay off");
    q.p[P_DL_MIX] = 60; at("delay on");
    q.p[P_CH_MIX] = 0;  at("chorus off");
    q.p[P_CH_MIX] = 60; at("chorus on");
    q.p[P_DIST] = 60;   at("distortion on");
    Out o = render(pt, sr, S(t + 0.5, sr), ph);
    if (getenv("DUMP"))
        wav("fxswitch.wav", o);
    std::vector<float> hp(o.L);
    {
        double w = std::tan(M_PI * 6000.0 / sr), qq = 0.7071;
        for (int pass = 0; pass < 2; pass++) {
            double k = w, nn = 1 / (1 + k / qq + k * k), b0 = nn, b1 = -2 * nn, b2 = nn, a1 = 2 * (k * k - 1) * nn,
                   a2 = (1 - k / qq + k * k) * nn, z1 = 0, z2 = 0;
            for (float &v : hp) {
                double oo = b0 * v + z1;
                z1 = b1 * v - a1 * oo + z2;
                z2 = b2 * v - a2 * oo;
                v = (float)oo;
            }
        }
    }
    for (auto &w : sw) {
        int c = S(w.first, sr), fl = S(0.002, sr);
        /* a click: a burst in the first 30 ms above both the sound before and the sound it settles to */
        double before = 0, around = 0, after = 0;
        for (int i = c - S(0.25, sr); i < c - fl; i++)
            before = std::max(before, (double)std::fabs(hp[i]));
        for (int i = c; i < c + S(0.03, sr); i++)
            around = std::max(around, (double)std::fabs(hp[i]));
        for (int i = c + S(0.25, sr); i < c + S(0.55, sr); i++)
            after = std::max(after, (double)std::fabs(hp[i]));
        double r = 20 * std::log10((around + 1e-9) / (std::max(before, after) + 1e-9));
        check(r < 3.0 || around < 1e-4, fmt("%-14s at %.1f s", w.second, w.first).c_str(),
              fmt("burst %+.1f dB over the sound before and after", r));
    }
}

/* UNISON switched while keys are held (both ways), the arp started and stopped under held keys: when the keys are
 * let go, every voice ends */
void testSwitches()
{
    std::puts("switching while notes are held (unison, the arp)");
    for (int dir = 0; dir < 2; dir++) {
        int sr = 48000;
        Patch pt = dry(preset(NPRESETS - 1));
        pt.p[P_UNISON] = (uint8_t)dir;
        Patch sw = pt;
        sw.p[P_UNISON] = (uint8_t)(1 - dir);
        Phrase ph;
        for (int n : {48, 52, 55})
            ph.push_back({S(0.1, sr), Ev::ON, n, 0.8f, {}});
        ph.push_back({S(0.5, sr), Ev::PATCH, 0, 0, std::vector<uint8_t>(sw.p, sw.p + NPARAM)});
        for (int n : {48, 52, 55})
            ph.push_back({S(1.0, sr), Ev::OFF, n, 0, {}});
        Out o = render(pt, sr, S(4.0, sr), ph);
        check(o.voicesAtEnd == 0 && rms(o.L, S(3.5, sr)) < 1e-6, dir ? "unison on -> off, keys held" : "unison off -> on, keys held",
              fmt("voices left %d", o.voicesAtEnd));
    }
    {   /* the arp: keys in, the arp on and off under them */
        int sr = 48000;
        Patch pt = dry(preset(NPRESETS - 1));
        Phrase ph;
        Out o = render(pt, sr, S(5.0, sr), ph, {128}, [&](P67Engine &e, int pos) {
            if (pos == 0) e.arp(true, ARP_UPDOWN, RATE_16, 2, 0.5f);
            if (pos == 128 * 10) { e.arpKey(60, true, 0.8f); e.arpKey(64, true, 0.8f); e.arpKey(67, true, 0.8f); }
            if (pos == 128 * 600) e.arp(false, ARP_UP, RATE_16, 1, 0.5f);
            if (pos == 128 * 700) e.arp(true, ARP_UP, RATE_16, 1, 0.5f);
            if (pos == 128 * 800) { e.arpKey(60, false, 0); e.arpKey(64, false, 0); e.arpKey(67, false, 0); }
        });
        check(o.voicesAtEnd == 0 && rms(o.L, S(4.5, sr)) < 1e-6, "the arp on, off, on under held keys", fmt("voices left %d", o.voicesAtEnd));
    }
}

/* the filter: self-oscillating, keyboard tracking full, it plays in tune; and its tone at the top is clean */
void testFilter()
{
    std::puts("the filter (self-oscillation in tune across the keys)");
    for (int sr : {44100, 96000}) {
        double worst = 0;
        std::string line;
        for (int note : {36, 48, 60, 72, 84, 96}) {
            Patch pt = dry(preset(NPRESETS - 1));
            pt.p[P_MIX_O1] = pt.p[P_MIX_O2] = pt.p[P_O1_SUB] = pt.p[P_NOISE] = 0;
            pt.p[P_RESO] = 127;
            pt.p[P_KEYTRK] = KT_FULL;
            pt.p[P_ENV_AMT] = 64;
            pt.p[P_VINTAGE] = pt.p[P_SPREAD] = 0;
            pt.p[P_CUTOFF] = 150;
            pt.p[P_NOISE] = 2;       /* a breath, to start it */
            Phrase ph = {{0, Ev::ON, note, 0.8f, {}}};
            Out o = render(pt, sr, S(1.5, sr), ph);
            double base = 20.0 * std::pow(2.0, 150 * 9.965784 / 255.0) * std::pow(2.0, (note - 60) / 12.0);
            double f = pitchHz(o.L, sr, S(0.8, sr), S(0.5, sr), base / 1.5, std::min(base * 1.5, sr * 0.45));
            double c = cents(f, base);
            line += fmt("%d:%+.0f ", note, c);
            worst = std::max(worst, std::fabs(c));
        }
        check(worst < 5, fmt("self-oscillation %6d (cents off the cutoff)", sr).c_str(), line);
    }
}

void bench()
{
    std::puts("CPU (7 voices, the heaviest patch: poly mod, noise LFO, all effects), % of real time");
    for (int sr : RATES) {
        Patch pt = preset(9);
        pt.p[P_LFO_RATE] = 127;
        pt.p[P_LFO_SHAPE] = LS_SH;
        pt.p[P_LFO_DEST] = LD_LPF;
        pt.p[P_LFO_AMT] = 60;
        pt.p[P_RV_TYPE] = RV_SPACE;
        pt.p[P_RV_MIX] = 60;
        pt.p[P_SP_SHIM] = 60;
        pt.p[P_DL_MIX] = 50;
        pt.p[P_CH_MIX] = 60;
        pt.p[P_DIST] = 40;
        pt.p[P_PM_DFILT] = PMF_BOTH;
        pt.p[P_PM_OSC2] = 90;
        Phrase ph;
        for (int n : {36, 43, 48, 55, 60, 64, 67})
            ph.push_back({0, Ev::ON, n, 0.8f, {}});
        double best = 1e9;
        for (int rep = 0; rep < 3; rep++) {
            Out o = render(pt, sr, S(4.0, sr), ph, {256});
            best = std::min(best, o.seconds / 4.0 * 100.0);
        }
        std::printf("    %6d Hz: %.2f %%\n", sr, best);
    }
}
} // namespace

/* LOAD P6 SYSEX: a made-up Prophet-6 program (no factory data here), packed as the Prophet-6 sends it, read back */
static void testP6Import()
{
    std::printf("\nP6 SYSEX\n");
    uint8_t u[1024] = {};
    u[0] = 12; u[1] = 24; u[2] = 127;            /* osc 1 an octave down, osc 2 the note, fine at the centre */
    u[3] = 127; u[4] = 254; u[6] = 127;          /* saw; pulse, square */
    u[7] = 127; u[8] = 100; u[27] = 127;         /* levels, VOLUME */
    u[19] = 82; u[20] = 0; u[29] = 254; u[21] = 2;   /* cutoff halfway, the envelope full up, full tracking */
    u[40] = 127; u[38] = 60;                     /* amp sustain, decay */
    u[59] = 127; u[63] = 128; u[64] = 1; u[67] = 1;  /* LFO on osc 1 and the low-pass */
    u[46] = 1; u[45] = 6; u[49] = 50; u[51] = 200;   /* FX on: B a hall */
    u[84] = 1;                                   /* unison */
    const char *nm = "Test Brass";
    for (int i = 0; nm[i]; i++) u[107 + i] = (uint8_t)nm[i];
    for (int i = (int)std::strlen(nm); i < 20; i++) u[107 + i] = ' ';
    std::vector<uint8_t> syx = {0xF0, 0x01, 0x2D, 0x02, 3, 42};
    for (int k = 0; k < 1024; k += 7) {
        uint8_t ms = 0;
        for (int j = 0; j < 7 && k + j < 1024; j++) ms |= (uint8_t)((u[k + j] >> 7) << j);
        syx.push_back(ms);
        for (int j = 0; j < 7 && k + j < 1024; j++) syx.push_back(u[k + j] & 0x7f);
    }
    syx.push_back(0xF7);
    std::vector<uint8_t> two = syx;               /* and an edit buffer after it */
    two.insert(two.end(), {0xF0, 0x01, 0x2D, 0x03});
    two.insert(two.end(), syx.begin() + 6, syx.end());
    auto progs = p6ReadSysex(two.data(), two.size());
    check(progs.size() == 2, "two programs read", fmt("%d", (int)progs.size()));
    if (progs.size() != 2)
        return;
    const P6Program &a = progs[0];
    check(a.number == 342 && a.name == "Test Brass" && progs[1].number == -1, "number and name",
          fmt("%d '%s'", a.number, a.name.c_str()));
    check(a.p[P_O1_FREQ] == 12 && a.p[P_O2_FREQ] == 24 && a.p[P_O2_FINE] == 64, "oscillator pitches");
    check(a.p[P_O1_SHAPE] == 63 && a.p[P_O2_SHAPE] == 127 && a.p[P_O2_PW] <= 1, "shapes: saw, a square pulse");
    check(a.p[P_ENV_AMT] == 127 && a.p[P_KEYTRK] == KT_FULL && a.p[P_CUTOFF] > 80 && a.p[P_CUTOFF] < 200, "the filter",
          fmt("cutoff %d env %d", a.p[P_CUTOFF], a.p[P_ENV_AMT]));
    check(a.p[P_LFO_DEST] == LD_FREQ1 && a.p[P_L2_DEST] == LD_LPF && a.p[P_LFO_AMT] > 60 && a.p[P_L2_AMT] == a.p[P_LFO_AMT],
          "the LFO's two destinations on LFO 1 and 2");
    check(a.p[P_RV_TYPE] == RV_HALL && a.p[P_RV_MIX] == 50 && a.p[P_CH_MIX] == 0 && a.p[P_DL_MIX] == 0, "FX B a hall, nothing else");
    check(a.p[P_UNISON] == 1 && a.p[P_AS] == 127, "unison, the amp envelope");
    bool inRange = true;
    for (auto &pr : progs)
        for (int i = 0; i < NPARAM; i++)
            inRange &= pr.p[i] <= PARAMS[i].max;
    check(inRange, "every value in its range");
    Patch pt;
    std::memcpy(pt.p, a.p, NPARAM);
    pt.hpKt = a.hpKt;
    pt.fenvVel = a.fenvVel;
    Out o = render(pt, 44100, S(2.0, 44100), {{0, Ev::ON, 48, 0.8f, {}}, {S(1.0, 44100), Ev::OFF, 48, 0, {}}});
    double r = rms(o.L, 0);
    check(r > 1e-3 && r < 1.0 && std::isfinite(r), "it plays", fmt("rms %.4f", r));
}

int main(int argc, char **argv)
{
    if (argc > 1 && !std::strcmp(argv[1], "bench")) {
        bench();
        return 0;
    }
    if (argc > 7 && !std::strcmp(argv[1], "tone")) {   /* tone <sr> <cutoff> <reso> <shape> <note> <out> [drive...] */
        int sr = std::atoi(argv[2]);
        Patch pt = dry(preset(NPRESETS - 1));
        pt.p[P_CUTOFF] = (uint8_t)std::atoi(argv[3]);
        pt.p[P_RESO] = (uint8_t)std::atoi(argv[4]);
        pt.p[P_O1_SHAPE] = (uint8_t)std::atoi(argv[5]);
        pt.p[P_MIX_O2] = pt.p[P_O1_SUB] = 0;
        pt.p[P_ENV_AMT] = 64;
        pt.p[P_VINTAGE] = pt.p[P_SPREAD] = 0;
        pt.p[P_AS] = 127;
        Phrase ph = {{0, Ev::ON, std::atoi(argv[6]), 0.8f, {}}};
        Out o = render(pt, sr, S(1.5, sr), ph);
        wav(argv[7], o);
        return 0;
    }
    if (argc > 4 && !std::strcmp(argv[1], "selfosc")) {   /* selfosc <sr> <note> <out>: the filter alone, ringing */
        int sr = std::atoi(argv[2]);
        Patch pt = dry(preset(NPRESETS - 1));
        pt.p[P_MIX_O1] = pt.p[P_MIX_O2] = pt.p[P_O1_SUB] = 0;
        pt.p[P_NOISE] = 2;
        pt.p[P_RESO] = 127;
        pt.p[P_KEYTRK] = KT_FULL;
        pt.p[P_ENV_AMT] = 64;
        pt.p[P_VINTAGE] = pt.p[P_SPREAD] = 0;
        pt.p[P_CUTOFF] = 150;
        Phrase ph = {{0, Ev::ON, std::atoi(argv[3]), 0.8f, {}}};
        Out o = render(pt, sr, S(1.5, sr), ph);
        wav(argv[4], o);
        return 0;
    }
    if (argc > 4 && !std::strcmp(argv[1], "render")) {
        int sr = std::atoi(argv[3]);
        Patch pt = preset(std::atoi(argv[2]) % NPRESETS);
        if (argc > 5) {                      /* render <preset> <sr> <out> dry|nochorus|norev|nodelay */
            std::string m = argv[5];
            if (m == "dry") pt = dry(pt);
            if (m == "nochorus") pt.p[P_CH_MIX] = 0;
            if (m == "norev") pt.p[P_RV_MIX] = 0;
            if (m == "nodelay") pt.p[P_DL_MIX] = 0;
        }
        Out o = render(pt, sr, S(7.0, sr), demo(sr));
        wav(argv[4], o);
        return 0;
    }
    if (argc > 1) {                          /* one test by name */
        std::string t = argv[1];
        if (t == "presets") testPresets();
        else if (t == "buffers") testBuffers();
        else if (t == "rates") testRates();
        else if (t == "zipper") testZipper(true);
        else if (t == "fuzz") testFuzz();
        else if (t == "filter") testFilter();
        else if (t == "switches") testSwitches();
        else if (t == "fxswitch") testFxSwitch();
        else if (t == "p6") testP6Import();
        std::printf("\n%d checks, %d failed\n", checks, failures);
        return failures ? 1 : 0;
    }
    testPresets();
    testBuffers();
    testRates();
    testZipper(false);
    testFuzz();
    testSwitches();
    testFxSwitch();
    testFilter();
    testP6Import();
    bench();
    std::printf("\n%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
