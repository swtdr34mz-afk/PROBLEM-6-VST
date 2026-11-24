/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The MPC skin's pictures, drawn with the VST3's own code: its controls (src/ui/Controls.cpp: the knobs, the
 * buttons, the LED lists, the seven-segment displays), its sections and panel face (src/ui/Look.cpp), laid out as
 * the VST lays them out (the same items, sizes and spacing as Prophet67Panel::build), on the MPC's pages:
 *
 *   QUICK    LOW-PASS FILTER, MIXER, FX MIX / VINTAGE & VOICE SPREAD, POLY MOD, ARPEGGIATOR / HOLD, GLIDE, UNISON,
 *            BEND (and INIT), AFTERTOUCH, QUALITY, LOW SPLIT
 *   OSC      OSCILLATOR 1, OSCILLATOR 2 / MIXER
 *   FLT/ENV  HIGH-PASS FILTER, FILTER ENVELOPE / LOW-PASS FILTER, AMPLIFIER ENVELOPE
 *   LFO      POLY MOD / LFO 1 / LFO 2
 *   FX       EFFECTS (SELECT, its LEDs and displays, each effect's knobs, DELAY's SYNC)
 *
 * Each page is drawn as large as it fits the MPC's 1280 x 628 (at most twice the VST's size), and cut up: its
 * background (the face, the frames, the knobs' legends), and every control at every value it can take (a knob's film
 * strip, a button's look for each run of values that looks and acts alike, with the value a press gives by the VST's
 * own rule; an LED list's rows lit and unlit; EFFECTS' SELECT, LEDs and displays). manifest.json says what each is,
 * for make_skin.py.
 *
 *   p67_skin_render <out folder> [--vst-panel] */
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using namespace p67;

namespace {
constexpr int TW = 1280, TH = 628;       /* a page on the MPC */
constexpr int KW = 58, BW = 50;          /* (PluginEditor.cpp) */
const char *const FX_NAME[5] = {"Chorus", "Reverb", "Space", "Delay", "Dly Mod"};
const char *const FX_CODE[5] = {"CHO", "REV", "SPC", "DLY", "MOD"};

bool bipolarKind(int i)
{
    int k = PARAMS[i].kind;
    return k == PK_BIPOLAR || k == PK_SEMI || k == PK_FINE;
}

/* QUALITY and INIT: the MPC plugin's own (not in the VST), parameters for their controls to show */
class Extra : public AudioProcessor
{
public:
    Extra() : apvts(*this, nullptr, "EXTRA", layout()) {}
    static AudioProcessorValueTreeState::ParameterLayout layout()
    {
        AudioProcessorValueTreeState::ParameterLayout l;
        l.add(std::make_unique<AudioParameterChoice>(ParameterID{"quality", 1}, "QUALITY", StringArray{"HIGH", "ECO"}, 0));
        l.add(std::make_unique<AudioParameterBool>(ParameterID{"init", 1}, "INIT", false));
        return l;
    }
    const String getName() const override { return "extra"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(AudioBuffer<float> &, MidiBuffer &) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    AudioProcessorEditor *createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String &) override {}
    void getStateInformation(MemoryBlock &) override {}
    void setStateInformation(const void *, int) override {}
    AudioProcessorValueTreeState apvts;
};

/* a page: the panel's face, its sections' frames, the knobs' legends; the controls are its children */
struct Page : Component {
    String name;
    float scale = 1.0f;
    int fx = 0;                          /* EFFECTS: the effect shown */
    std::vector<std::pair<Rectangle<int>, String>> sections;
    bool legends = false;                /* (the background: the controls hidden, their legends drawn here) */
    void paint(Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        drawPanelFace(g, r, (int)(9000.0f * r.getWidth() * r.getHeight() / (1480.0f * 640.0f)));
        for (auto &[sr, t] : sections)
            drawSection(g, sr, t);
        /* the maker's tag, bottom right: 200 x 24 on the MPC's screen */
        drawTag(g, Rectangle<float>(1280.0f - 18.0f - 200.0f, 628.0f - 14.0f - 24.0f, 200.0f, 24.0f) / scale);
        if (legends)
            for (auto *c : getChildren())
                if (auto *k = dynamic_cast<Knob *>(c); k && k->legend.isNotEmpty() && !k->getProperties().contains("fx")) {
                    auto b = k->getBounds().toFloat();
                    drawLegend(g, k->legend, b.removeFromBottom(14.0f * (k->legend.contains("\n") ? 2.0f : 1.0f)), 10.0f);
                }
    }
};

/* EFFECTS' SELECT and its LEDs (as PluginEditor.cpp's FxSelect) */
struct FxSel : Component {
    int *fx;
    explicit FxSel(int *f) : fx(f) {}
    static Rectangle<float> item(int i, float w) { return {(float)BW + 4.0f, 14.0f + 13.0f * (float)i, w - BW - 4.0f, 13.0f}; }
    void paint(Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        auto b = r.removeFromLeft((float)BW);
        auto lg = b.removeFromBottom(14.0f);
        drawButton(g, b.withSizeKeepingCentre(36.0f, 19.0f).withY(b.getY() + 34.0f), false, false, false);
        drawLegend(g, "SELECT", lg);
        for (int i = 0; i < 5; i++) {
            auto it = item(i, (float)getWidth());
            drawLed(g, {it.getX() + 4.5f, it.getCentreY()}, 3.0f, *fx == i ? 1.0f : 0.0f);
            g.setColour(col::legend);
            g.setFont(font(Face::label, 10.5f));
            g.drawText(FX_NAME[i], it.withTrimmedLeft(12.0f), Justification::centredLeft, false);
        }
    }
};

/* the items of a row, as Prophet67Panel::build makes them */
struct Item {
    int w;
    std::function<void(int x, int y)> place;
};
struct Sec {
    const char *title;                   /* nullptr: no frame (the program strip's controls) */
    std::vector<Item> items;
    int fixed = 0;
};

struct Builder {
    Prophet67Processor &proc;
    Extra &extra;
    Page &page;

    APVTS &apvtsOf(const String &id) { return id == "quality" || id == "init" ? extra.apvts : proc.apvts; }
    Component *add(Component *c, Rectangle<int> r)
    {
        page.addAndMakeVisible(c);
        c->setBounds(r);
        return c;
    }
    static String P(int i) { return Prophet67Processor::paramId(i); }

    /* a row of sections at their own widths, side by side 8 apart, centred on the page's width */
    void row(int y, std::vector<Sec> secs)
    {
        const float gap = 8.0f;
        std::vector<float> w;
        float total = 0.0f;
        for (auto &sc : secs) {
            float t = sc.title ? 20.0f : 0.0f;
            for (auto &it : sc.items)
                t += (float)it.w;
            w.push_back(sc.fixed ? (float)sc.fixed : t);
            total += w.back();
        }
        total += gap * (float)(secs.size() - 1);
        float x = std::round(((float)page.getWidth() - total) * 0.5f);
        for (size_t i = 0; i < secs.size(); i++) {
            int sx = (int)std::lround(x);
            if (secs[i].title)
                page.sections.push_back({{sx, y, (int)std::lround(w[i]), 120}, secs[i].title});
            float nat = secs[i].title ? 20.0f : 0.0f;
            for (auto &it : secs[i].items)
                nat += (float)it.w;
            float step = secs[i].items.empty() ? 0.0f : (w[i] - nat) / (float)secs[i].items.size();
            float cx = (float)sx + (secs[i].title ? 10.0f : 0.0f) + step * 0.5f;
            for (auto &it : secs[i].items) {
                it.place((int)std::lround(cx), y);
                cx += (float)it.w + step;
            }
            x += w[i] + gap;
        }
    }

    Item K(int i, const char *leg)
    {
        return {KW, [=](int x, int y) {
                    auto *k = new Knob(proc.apvts, P(i), leg, nullptr);
                    k->getProperties().set("bipolar", bipolarKind(i));
                    add(k, {x, y + 22, KW, 80});
                }};
    }
    Item GK(const char *id, const char *leg, int w = KW)
    {
        return {w, [=](int x, int y) { add(new Knob(proc.apvts, id, leg, nullptr), {x, y + 22, w, 80}); }};
    }
    Item L(String id, StringArray names, int w, float rowH = 13.0f)
    {
        return {w, [=](int x, int y) {
                    auto *l = new LedList(apvtsOf(id), id, nullptr, names);
                    l->rowH = rowH;
                    int h = jmax(74, (int)std::ceil(rowH * (float)names.size()) + 4);
                    add(l, {x, y + 55 - h / 2, w, h});
                }};
    }
    Item B(String id, const char *leg, int bit = 1)
    {
        return {BW, [=](int x, int y) { add(new p67::Button(proc.apvts, id, leg, nullptr, bit), {x, y + 48, BW, 54}); }};
    }
    Item BR(String id, const char *leg, p67::Button::IsOn on, p67::Button::Next next)
    {
        return {BW, [=](int x, int y) { add(new p67::Button(proc.apvts, id, leg, nullptr, on, next), {x, y + 48, BW, 54}); }};
    }
    Item VEL(String id)
    {
        auto mem = std::make_shared<int>(100);
        return BR(id, "VELOCITY", [](int v) { return v > 0; }, [mem](int v) {
            if (v > 0) {
                *mem = v;
                return 0;
            }
            return *mem;
        });
    }
    Item KB(String id)
    {
        return {BW + 6, [=](int x, int y) {
                    auto *b = new p67::Button(proc.apvts, id, "KEYBOARD", nullptr, [](int) { return false; }, [](int v) { return (v + 1) % NKT; });
                    b->leds = {"Half", "Full"};
                    add(b, {x, y + 34, BW + 6, 70});
                }};
    }
    Item SHAPE(int i)
    {
        return {80, [=](int x, int y) {
                    auto *k = new Knob(proc.apvts, P(i), "SHAPE", nullptr);
                    k->getProperties().set("shapes", true);
                    add(k, {x, y + 18, 80, 84});
                }};
    }
    Item wideKnob(int i, const char *leg, int w)
    {
        return {w, [=](int x, int y) { add(new Knob(proc.apvts, P(i), leg, nullptr), {x, y + 22, w, 80}); }};
    }
    Item keyButton(String id, const char *leg, bool grey = false)   /* the program strip's keys (HOLD, UNISON; INIT grey) */
    {
        return {54, [=](int x, int y) {
                    auto *b = new p67::Button(apvtsOf(id), id, leg, nullptr);
                    b->key = true;
                    b->grey = grey;
                    add(b, {x, y + 22, 54, 80});
                }};
    }

    /* the sections, as the VST's */
    Sec lowPass() { return {"LOW-PASS FILTER", {K(P_CUTOFF, "CUTOFF"), K(P_RESO, "RESONANCE"), K(P_ENV_AMT, "ENV AMOUNT"), VEL(P(P_FVEL)), KB(P(P_KEYTRK))}}; }
    Sec highPass() { return {"HIGH-PASS FILTER", {K(P_HPF, "CUTOFF"), K(P_HP_RESO, "RESONANCE"), K(P_HP_ENV, "ENV AMOUNT"), VEL("hp_vel"), KB("hp_keytrk")}}; }
    Sec mixer() { return {"MIXER", {K(P_MIX_O1, "OSC 1"), K(P_MIX_O2, "OSC 2"), K(P_O1_SUB, "SUB OCTAVE"), K(P_NOISE, "NOISE")}}; }
    Sec polyMod()
    {
        return {"POLY MOD", {K(P_PM_FENV, "FILTER ENV"), K(P_PM_OSC2, "OSC 2"), B(P(P_PM_DOSC), "FREQ 1", PMO_FREQ),
                             B(P(P_PM_DOSC), "SHAPE 1", PMO_SHAPE), B(P(P_PM_DOSC), "PW 1", PMO_PW),
                             Item{BW, [this](int x, int y) {
                                      auto *b = new p67::Button(proc.apvts, P(P_PM_DFILT), "FILTER", nullptr, [](int) { return false; },
                                                                [](int v) { return (v + 1) % 4; });
                                      b->leds = {"LP", "HP"};
                                      add(b, {x, y + 34, BW, 70});
                                  }}}};
    }
    Sec lfo(int l)
    {
        return {l ? "LFO 2" : "LFO 1",
                {K(l ? P_L2_RATE : P_LFO_RATE, "FREQUENCY"), K(l ? P_L2_SYNC : P_L1_SYNC, "SYNC"),
                 L(P(l ? P_L2_SHAPE : P_LFO_SHAPE), {"Triangle", "Sawtooth", "Ramp", "Square", "Random"}, 64),
                 K(l ? P_L2_AMT : P_LFO_AMT, "INITIAL AMT"),
                 L(P(l ? P_L2_DEST : P_LFO_DEST), {"Freq 1+2", "Freq 1", "Freq 2", "PW 1+2", "Low-pass", "High-pass", "Amp"}, 66, 10.5f),
                 B(P(l ? P_L2_RETRIG : P_L1_RETRIG), "RETRIG")}};
    }
    Sec afterTouch()
    {
        return {"AFTERTOUCH", {K(P_AT_AMT, "AMOUNT"), K(P_AT_CURVE, "CURVE"), K(P_AT_SMOOTH, "SMOOTH"), K(P_AT_DEST, "DEST"),
                               L(P(P_AT_DEST), {"Off", "Filter", "Swell", "Power", "Vibrato", "LFO 1", "Pitch", "Poly Mod"}, 66, 10.0f)}};
    }
    Sec lowSplit()
    {
        /* (the MPC's: SPLIT, POINT and LOW HOLD; the scales, KEY, FOLLOW and BALANCE stay at their defaults) */
        return {"LOW SPLIT", {GK("split", "SPLIT"), L("split", {"Off", "-1 Oct", "-2 Oct", "-3 Oct"}, 42), GK("split_point", "POINT"),
                              B("low_hold", "LOW HOLD")}};
    }
    Sec arp()
    {
        return {"ARPEGGIATOR", {B("arp_on", "ON/OFF"), GK("arp_rate", "VALUE"), L("arp_rate", {"1/8", "1/8 T", "1/16", "1/16 T", "1/32"}, 46),
                                GK("arp_oct", "OCTAVES"), L("arp_oct", {"1", "2", "3", "4"}, 26), GK("arp_mode", "MODE"),
                                L("arp_mode", {"Up", "Down", "Up/Down", "Random", "Played"}, 58), GK("arp_gate", "GATE")}};
    }
    Sec osc1()
    {
        return {"OSCILLATOR 1", {K(P_O1_FREQ, "FREQUENCY"), BR(P(P_O2_MODE), "SYNC", [](int v) { return v == O2M_SYNC; },
                                                               [](int v) { return v == O2M_SYNC ? O2M_NORMAL : O2M_SYNC; }),
                                 SHAPE(P_O1_SHAPE), K(P_O1_PW, "PULSE WIDTH")}};
    }
    Sec osc2()
    {
        return {"OSCILLATOR 2", {K(P_O2_FREQ, "FREQUENCY"), K(P_O2_FINE, "FINE"), SHAPE(P_O2_SHAPE), K(P_O2_PW, "PULSE WIDTH"),
                                 Item{BW + 6, [this](int x, int y) {   /* LOW FREQ above KEYBOARD */
                                          add(new p67::Button(proc.apvts, P(P_O2_MODE), "LOW FREQ", nullptr,
                                                              [](int v) { return v == O2M_LOW || v == O2M_LOWFIX; },
                                                              [](int v) { return v == O2M_LOW || v == O2M_LOWFIX ? O2M_NORMAL : O2M_LOW; }),
                                              {x, y + 16, BW + 6, 48});
                                          add(new p67::Button(proc.apvts, P(P_O2_MODE), "KEYBOARD", nullptr, [](int v) { return v != O2M_LOWFIX; },
                                                              [](int v) { return v == O2M_LOW ? O2M_LOWFIX : v == O2M_LOWFIX ? O2M_LOW : v; }),
                                              {x, y + 64, BW + 6, 48});
                                      }}}};
    }
    Sec envelope(bool amp)
    {
        if (amp)
            return {"AMPLIFIER ENVELOPE", {VEL(P(P_AVEL)), K(P_AA, "ATTACK"), K(P_AD, "DECAY"), K(P_AS, "SUSTAIN"), K(P_AR, "RELEASE")}, 306};
        return {"FILTER ENVELOPE", {VEL("fenv_vel"), K(P_FA, "ATTACK"), K(P_FD, "DECAY"), K(P_FS, "SUSTAIN"), K(P_FR, "RELEASE")}, 306};
    }
    Sec effects()
    {
        static const int sets[5][4] = {{P_CH_MIX, P_CH_RATE, P_CH_DEPTH, -1}, {P_RV_TYPE, P_RV_MIX, P_RV_DECAY, P_RV_TONE},
                                       {P_SP_MOD, P_SP_SHIM, P_SP_SWELL, P_SP_PRE}, {P_DL_TIME_L, P_DL_TIME_R, P_DL_FB, P_DL_MIX},
                                       {P_DL_DEPTH, P_DL_RATE, P_DL_TONE, -1}};
        static const char *legs[5][4] = {{"MIX", "RATE", "DEPTH", ""}, {"TYPE", "MIX", "DECAY", "TONE"},
                                         {"MODULATE", "SHIMMER", "SWELL", "PRE-DELAY"}, {"TIME L", "TIME R", "REPEATS", "MIX"},
                                         {"DEPTH", "RATE", "TONE", ""}};
        return {"EFFECTS",
                {Item{106, [this](int x, int y) { add(new FxSel(&page.fx), {x, y + 22, 106, 82})->getProperties().set("role", "fxselect"); }},
                 Item{84, [this](int x, int y) {
                     add(new SegDisplay(3, [this] { return String(FX_CODE[page.fx]); }), {x + 4, y + 26, 76, 36})
                         ->getProperties().set("role", "fxcode");
                     add(new SegDisplay(3, [this] {   /* (fxValueText: the effect's first knob) */
                             auto *p = proc.apvts.getParameter(P(sets[page.fx][0]));
                             int v = (int)std::lround(p->convertFrom0to1(p->getValue()));
                             if (dynamic_cast<AudioParameterChoice *>(p))
                                 return p->getCurrentValueAsText().toUpperCase().substring(0, 3);
                             return String(v).paddedLeft('0', 3);
                         }),
                         {x + 4, y + 64, 76, 36})
                         ->getProperties().set("role", "fxvalue");
                 }},
                 Item{KW * 4, [this](int x, int y) {
                     for (int f = 0; f < 5; f++)
                         for (int k = 0; k < 4; k++)
                             if (sets[f][k] >= 0) {
                                 auto *kn = new Knob(proc.apvts, P(sets[f][k]), legs[f][k], nullptr);
                                 kn->getProperties().set("bipolar", bipolarKind(sets[f][k]));
                                 kn->getProperties().set("fx", f);
                                 add(kn, {x + KW * k, y + 22, KW, 80});
                             }
                 }},
                 Item{BW, [this](int x, int y) {
                     add(new p67::Button(proc.apvts, P(P_DL_SYNC), "SYNC", nullptr), {x, y + 48, BW, 54})->getProperties().set("fx", 3);
                 }}}};
    }
};

/* ---------------------------------------------------------------- cutting a page up --- */
struct Strip {                           /* frames of one size, stacked */
    Rectangle<int> r;
    int frames = 0;
    Image img;
    std::vector<bool> done;
    void init(Rectangle<int> rr, int n)
    {
        r = rr;
        frames = n;
        img = Image(Image::ARGB, jmax(1, r.getWidth()), jmax(1, r.getHeight() * n), true);
        done.assign((size_t)n, false);
    }
    void grab(const Image &full, int frame)
    {
        if (frame < 0 || frame >= frames || done[(size_t)frame])
            return;
        Graphics g(img);
        g.drawImageAt(full.getClippedImage(r), 0, frame * r.getHeight());
        done[(size_t)frame] = true;
    }
    bool complete() const { return std::all_of(done.begin(), done.end(), [](bool b) { return b; }); }
};

/* a film strip's frames: one a value, at most 128 (CUTOFF's 256 values: one frame each two), and the strip at most
 * 9800 pixels tall (as the SDK's own knobs; a picture much taller than that is more than some MPCs' graphics take) */
constexpr int MAXF = 128, MAXH = 9800;
int framesOf(int steps, int h = 1) { return jmax(2, jmin(steps, MAXF, MAXH / jmax(1, h))); }
int indexOf(int steps, int frames, int frame)
{
    return frames == steps ? frame : (int)std::lround((double)frame * (steps - 1) / (frames - 1));
}

int stepsOf(RangedAudioParameter &p, int &lo)
{
    auto r = p.getNormalisableRange();
    if (dynamic_cast<AudioParameterFloat *>(&p)) {   /* (a float: 128 places) */
        lo = 0;
        return 128;
    }
    lo = (int)r.start;
    return (int)r.end - (int)r.start + 1;
}

void setIndex(RangedAudioParameter &p, int steps, int i)
{
    i = jlimit(0, steps - 1, i);
    p.setValueNotifyingHost(steps > 1 ? (float)i / (float)(steps - 1) : 0.0f);
}

var rectVar(Rectangle<int> r) { return Array<var>{r.getX(), r.getY(), r.getWidth(), r.getHeight()}; }

bool writePng(const Image &img, const File &f)
{
    f.deleteFile();
    FileOutputStream out(f);
    PNGImageFormat png;
    return out.openedOk() && png.writeImageToStream(img, out);
}

struct Ctl {
    enum Kind { knob, button, leds } kind;
    Component *c = nullptr;
    RangedAudioParameter *p = nullptr;
    String id;
    int fx = -1, steps = 2, lo = 0, frames = 2;
    Strip strip, legend;                 /* (an effect's knob: its legend comes and goes with it) */
    struct Range {
        int a, b, next;
        Strip s;
    };
    std::vector<Range> ranges;
    std::vector<Strip> on, off;
};

var cutPage(Page &page, const File &dir)
{
    const float S = page.scale;
    auto toMpc = [S](Rectangle<float> r) {   /* (edges rounded: neighbours meet without overlapping) */
        int x0 = (int)std::lround(r.getX() * S), y0 = (int)std::lround(r.getY() * S);
        int x1 = (int)std::lround(r.getRight() * S), y1 = (int)std::lround(r.getBottom() * S);
        x0 = jlimit(0, TW, x0), x1 = jlimit(0, TW, x1), y0 = jlimit(0, TH, y0), y1 = jlimit(0, TH, y1);
        return Rectangle<int>{x0, y0, x1 - x0, y1 - y0};
    };
    auto snap = [&] {
        Image full(Image::ARGB, TW, TH, true);
        Graphics g(full);
        g.drawImageAt(page.createComponentSnapshot(page.getLocalBounds(), true, S), 0, 0);
        return full;
    };
    String tag = page.name.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789");
    int n = 0;
    auto save = [&](Strip &s, const String &what) {
        if (!s.complete())
            std::fprintf(stderr, "warning: %s %s has frames not drawn\n", tag.toRawUTF8(), what.toRawUTF8());
        String f = tag + "_" + String(n++).paddedLeft('0', 3) + "_" + what.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789_") + ".png";
        writePng(s.img, dir.getChildFile(f));
        return f;
    };

    std::vector<std::unique_ptr<Ctl>> ctls;
    Component *fxSel = nullptr, *fxCode = nullptr, *fxValue = nullptr;
    for (auto *c : page.getChildren()) {
        auto role = c->getProperties()["role"].toString();
        if (role == "fxselect")
            fxSel = c;
        else if (role == "fxcode")
            fxCode = c;
        else if (role == "fxvalue")
            fxValue = c;
        auto ctl = std::make_unique<Ctl>();
        ctl->c = c;
        ctl->fx = c->getProperties().getWithDefault("fx", -1);
        if (auto *k = dynamic_cast<Knob *>(c)) {
            ctl->kind = Ctl::knob;
            ctl->p = &k->param;
        } else if (auto *b = dynamic_cast<p67::Button *>(c)) {
            ctl->kind = Ctl::button;
            ctl->p = &b->param;
        } else if (auto *l = dynamic_cast<LedList *>(c)) {
            ctl->kind = Ctl::leds;
            ctl->p = &l->param;
        } else
            continue;
        ctl->id = ctl->p->paramID;
        ctl->steps = stepsOf(*ctl->p, ctl->lo);
        ctls.push_back(std::move(ctl));
    }

    /* what to fill */
    Knob *fxFirst[5] = {};
    for (auto &c : ctls) {
        auto b = c->c->getBounds().toFloat();
        if (c->kind == Ctl::knob) {
            auto *k = static_cast<Knob *>(c->c);
            if (k->legend.isNotEmpty()) {   /* the legend is the background's (an effect's its own); the strip only the knob */
                auto lg = b.removeFromBottom(14.0f * (k->legend.contains("\n") ? 2.0f : 1.0f));
                if (c->fx >= 0)
                    c->legend.init(toMpc(lg), 1);
            }
            auto r = toMpc(b);
            c->frames = framesOf(c->steps, r.getHeight());
            c->strip.init(r, c->frames);
            if (c->fx >= 0 && (!fxFirst[c->fx] || k->getX() < fxFirst[c->fx]->getX()))
                fxFirst[c->fx] = k;
        } else if (c->kind == Ctl::button) {
            auto *bt = static_cast<p67::Button *>(c->c);
            int mask = (1 << bt->leds.size()) - 1;
            c->frames = c->steps;
            for (int i = 0; i < c->steps; i++) {
                int v = c->lo + i, nx = jlimit(0, c->steps - 1, bt->next(v) - c->lo);
                bool on = bt->isOn(v);
                if (!c->ranges.empty()) {
                    auto &last = c->ranges.back();
                    int lv = c->lo + last.a;
                    if (last.next == nx && bt->isOn(lv) == on && (lv & mask) == (v & mask)) {
                        last.b = i;
                        continue;
                    }
                }
                c->ranges.push_back({i, i, nx, {}});
                c->ranges.back().s.init(toMpc(b), 1);
            }
        } else {
            auto *l = static_cast<LedList *>(c->c);
            c->frames = c->steps;
            for (int i = 0; i < l->names.size(); i++) {
                auto r = toMpc(l->item(i) + b.getPosition());
                c->on.emplace_back();
                c->on.back().init(r, 1);
                c->off.emplace_back();
                c->off.back().init(r, 1);
            }
        }
    }
    bool hasFx = fxSel && fxCode && fxValue;
    int fxSteps[5] = {};
    Strip fxValueStrip[5], fxCodeStrip, selectStrip;
    std::vector<Strip> selOn(5), selOff(5);
    if (hasFx) {
        fxCodeStrip.init(toMpc(fxCode->getBounds().toFloat()), 5);
        selectStrip.init(toMpc(fxSel->getBounds().toFloat().withWidth((float)BW)), 1);
        for (int i = 0; i < 5; i++) {
            auto r = toMpc(FxSel::item(i, (float)fxSel->getWidth()) + fxSel->getBounds().toFloat().getPosition());
            selOn[(size_t)i].init(r, 1);
            selOff[(size_t)i].init(r, 1);
        }
        for (int f = 0; f < 5; f++) {
            int lo;
            fxSteps[f] = stepsOf(fxFirst[f]->param, lo);
            fxValueStrip[f].init(toMpc(fxValue->getBounds().toFloat()), fxSteps[f]);
        }
    }

    /* the background */
    for (auto *c : page.getChildren())
        c->setVisible(false);
    page.legends = true;
    Image bg = snap();
    page.legends = false;
    String bgName = tag + "_panel.png";
    writePng(bg, dir.getChildFile(bgName));

    /* every control at every value: those not of EFFECTS, then each effect's */
    for (auto &c : ctls)
        c->c->setVisible(c->fx < 0);
    auto grabAll = [&](const Image &full, int i, int f) {
        for (auto &c : ctls) {
            if (c->fx != f || i >= c->frames)
                continue;
            if (c->kind == Ctl::knob) {
                c->strip.grab(full, i);
                if (c->fx >= 0)
                    c->legend.grab(full, 0);
            }
            else if (c->kind == Ctl::button) {
                for (auto &r : c->ranges)
                    if (r.a == i)
                        r.s.grab(full, 0);
            } else
                for (size_t k = 0; k < c->on.size(); k++)
                    (k == (size_t)i ? c->on[k] : c->off[k]).grab(full, 0);
        }
    };
    for (int i = 0; i < MAXF; i++) {
        for (auto &c : ctls)
            if (c->fx < 0)
                setIndex(*c->p, c->steps, indexOf(c->steps, c->frames, i));
        grabAll(snap(), i, -1);
    }
    for (auto &c : ctls)                 /* (an LED row never off at the lowest value: off at another) */
        if (c->kind == Ctl::leds && !c->off.empty() && !c->off[0].complete()) {
            setIndex(*c->p, c->steps, 1);
            c->off[0].grab(snap(), 0);
        }
    if (hasFx) {
        for (auto &c : ctls)
            if (c->fx < 0)
                c->c->setVisible(false);
        for (auto *c : {fxSel, fxCode, fxValue})
            c->setVisible(true);
        for (int f = 0; f < 5; f++) {
            page.fx = f;
            for (auto &c : ctls)
                if (c->fx >= 0)
                    c->c->setVisible(c->fx == f);
            for (int i = 0; i < MAXF; i++) {
                for (auto &c : ctls)
                    if (c->fx == f)
                        setIndex(*c->p, c->steps, indexOf(c->steps, c->frames, i));
                auto full = snap();
                grabAll(full, i, f);
                if (i < fxSteps[f])
                    fxValueStrip[f].grab(full, i);
                if (i == 0) {
                    fxCodeStrip.grab(full, f);
                    selectStrip.grab(full, 0);
                    selOn[(size_t)f].grab(full, 0);
                    selOff[(size_t)((f + 1) % 5)].grab(full, 0);
                }
            }
        }
    }

    /* the files and what they are */
    Array<var> list;
    for (auto &c : ctls) {
        auto *o = new DynamicObject();
        o->setProperty("id", c->id);
        o->setProperty("fx", c->fx);
        o->setProperty("steps", c->steps);
        o->setProperty("frames", c->frames);
        Array<var> names;                /* a choice's names (the MPC plugin's are the same) */
        if (auto *ch = dynamic_cast<AudioParameterChoice *>(c->p))
            for (auto &t : ch->choices)
                names.add(t);
        else if (dynamic_cast<AudioParameterBool *>(c->p))
            names = {"OFF", "ON"};
        else if (c->steps <= 8)
            for (int i = 0; i < c->steps; i++)
                names.add(String(c->lo + i));
        o->setProperty("names", names);
        if (c->kind == Ctl::knob) {
            o->setProperty("kind", "knob");
            o->setProperty("bipolar", (bool)c->c->getProperties()["bipolar"]);
            o->setProperty("rect", rectVar(c->strip.r));
            o->setProperty("frame", rectVar(toMpc(c->c->getBounds().toFloat())));   /* (with its legend) */
            o->setProperty("strip", save(c->strip, "knob_" + c->id));
            if (c->fx >= 0) {
                o->setProperty("legendRect", rectVar(c->legend.r));
                o->setProperty("legendImage", save(c->legend, "legend_" + c->id));
            }
        } else if (c->kind == Ctl::button) {
            o->setProperty("kind", "button");
            o->setProperty("rect", rectVar(c->ranges.front().s.r));
            Array<var> rs;
            for (auto &r : c->ranges) {
                auto *ro = new DynamicObject();
                ro->setProperty("a", r.a);
                ro->setProperty("b", r.b);
                ro->setProperty("next", r.next);
                ro->setProperty("image", save(r.s, "button_" + c->id + "_" + String(r.a)));
                rs.add(var(ro));
            }
            o->setProperty("ranges", rs);
        } else {
            o->setProperty("kind", "leds");
            Array<var> rows;
            for (size_t k = 0; k < c->on.size(); k++) {
                auto *ro = new DynamicObject();
                ro->setProperty("rect", rectVar(c->on[k].r));
                ro->setProperty("on", save(c->on[k], "led_" + c->id + "_" + String((int)k) + "_on"));
                ro->setProperty("off", save(c->off[k], "led_" + c->id + "_" + String((int)k) + "_off"));
                rows.add(var(ro));
            }
            o->setProperty("rows", rows);
        }
        list.add(var(o));
    }
    auto *po = new DynamicObject();
    po->setProperty("name", page.name);
    po->setProperty("scale", S);
    po->setProperty("background", bgName);
    po->setProperty("controls", list);
    if (hasFx) {
        auto *fx = new DynamicObject();
        fx->setProperty("select", rectVar(selectStrip.r));
        fx->setProperty("selectImage", save(selectStrip, "fx_select"));
        Array<var> rows;
        for (int i = 0; i < 5; i++) {
            auto *ro = new DynamicObject();
            ro->setProperty("rect", rectVar(selOn[(size_t)i].r));
            ro->setProperty("on", save(selOn[(size_t)i], "fx_led_" + String(i) + "_on"));
            ro->setProperty("off", save(selOff[(size_t)i], "fx_led_" + String(i) + "_off"));
            rows.add(var(ro));
        }
        fx->setProperty("rows", rows);
        fx->setProperty("code", rectVar(fxCodeStrip.r));
        fx->setProperty("codeStrip", save(fxCodeStrip, "fx_code"));
        fx->setProperty("value", rectVar(fxValueStrip[0].r));
        Array<var> vals;
        for (int f = 0; f < 5; f++) {
            auto *vo = new DynamicObject();
            vo->setProperty("id", fxFirst[f]->param.paramID);
            vo->setProperty("steps", fxSteps[f]);
            vo->setProperty("strip", save(fxValueStrip[f], "fx_value_" + String(f)));
            vals.add(var(vo));
        }
        fx->setProperty("values", vals);
        po->setProperty("effects", var(fx));
    }
    std::printf("%-7s scale %.3f, %d controls, %d pictures\n", page.name.toRawUTF8(), S, (int)ctls.size(), n + 1);
    return var(po);
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: p67_skin_render <out folder>\n");
        return 2;
    }
    ScopedJuceInitialiser_GUI gui;
    File dir = File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    dir.createDirectory();
    Prophet67Processor proc;
    Extra extra;
    Look look;
    if (argc > 2 && String(argv[2]) == "--vst-panel") {   /* (a look at the VST's own panel, for comparing) */
        Prophet67Panel panel(proc);
        panel.setBounds(0, 0, Prophet67Panel::W, Prophet67Panel::H);
        writePng(panel.createComponentSnapshot(panel.getLocalBounds(), true, 1.0f), dir.getChildFile("vst_panel.png"));
        return 0;
    }

    /* the pages: rows of sections (y as on the VST: rows 126 apart, a section 120 high), in the VST's units */
    struct Def {
        const char *name;
        int rows;
        std::function<void(Builder &)> build;
    };
    std::vector<Def> defs = {
        {"QUICK", 3, [](Builder &b) {
             b.row(0, {b.lowPass(), b.mixer(),
                       {"FX MIX", {b.K(P_DIST, "DISTORT"), b.K(P_CH_MIX, "CHORUS"), b.K(P_RV_MIX, "REVERB"), b.K(P_DL_MIX, "DELAY")}}});
             b.row(126, {{"VINTAGE & VOICE SPREAD", {b.wideKnob(P_VINTAGE, "VINTAGE", 64), b.wideKnob(P_SPREAD, "VOICE SPREAD", 72)}},
                         b.polyMod(), b.arp()});
             b.row(252, {{nullptr, {b.keyButton("init", "INIT", true), b.keyButton("arp_hold", "HOLD"), b.K(P_GLIDE, "GLIDE AMT"), b.keyButton(Builder::P(P_UNISON), "UNISON"),
                                    b.wideKnob(P_BEND, "BEND RANGE", 64)}},
                         b.afterTouch(), {"QUALITY", {b.L("quality", {"High", "Eco"}, 60)}}, b.lowSplit()});
         }},
        {"OSC", 2, [](Builder &b) {
             b.row(0, {b.osc1(), b.osc2()});
             b.row(126, {b.mixer()});
         }},
        {"FLT/ENV", 2, [](Builder &b) {
             b.row(0, {b.highPass(), b.envelope(false)});
             b.row(126, {b.lowPass(), b.envelope(true)});
         }},
        {"LFO", 3, [](Builder &b) {
             b.row(0, {b.polyMod()});
             b.row(126, {b.lfo(0)});
             b.row(252, {b.lfo(1)});
         }},
        {"FX", 1, [](Builder &b) { b.row(0, {b.effects()}); }},
    };

    Array<var> pages;
    for (auto &d : defs) {
        /* how wide its rows are: the page as large as fits (24 round them), at most twice the VST's size */
        float w = 0.0f;
        {
            Page probe;
            probe.setSize(4000, 4000);
            Builder pb{proc, extra, probe};
            d.build(pb);
            int x0 = 4000, x1 = 0;
            for (auto &s : probe.sections)
                x0 = jmin(x0, s.first.getX()), x1 = jmax(x1, s.first.getRight());
            for (auto *c : probe.getChildren())
                x0 = jmin(x0, c->getX()), x1 = jmax(x1, c->getRight());
            w = (float)(x1 - x0);
            probe.deleteAllChildren();
        }
        float h = 120.0f + 126.0f * (float)(d.rows - 1);
        float S = jmin((float)TW / (w + 48.0f), (float)TH / (h + 48.0f), 2.0f);

        Page page;
        page.name = d.name;
        page.scale = S;
        page.setLookAndFeel(&look);
        int ph = (int)std::ceil((float)TH / S);
        page.setSize((int)std::ceil((float)TW / S), ph);
        Builder b{proc, extra, page};
        d.build(b);
        int top = (int)std::lround(((float)ph - h) * 0.5f);   /* (the rows in the middle) */
        for (auto *c : page.getChildren())
            c->setTopLeftPosition(c->getX(), c->getY() + top);
        for (auto &s : page.sections)
            s.first.translate(0, top);
        pages.add(cutPage(page, dir));
        page.deleteAllChildren();
        page.setLookAndFeel(nullptr);
    }
    auto *root = new DynamicObject();
    root->setProperty("pages", pages);
    dir.getChildFile("manifest.json").replaceWithText(JSON::toString(var(root)));
    return 0;
}
