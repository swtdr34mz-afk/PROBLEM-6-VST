/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
#include "PluginEditor.h"
#include "PanelDialog.h"
#include "WindowSize.h"

using namespace p67;

namespace {
constexpr int X0 = 56, X1 = 1504;        /* the panel's working width */
constexpr int KW = 58, BW = 50;
bool bipolarKind(int i)
{
    int k = PARAMS[i].kind;
    return k == PK_BIPOLAR || k == PK_SEMI || k == PK_FINE;
}          /* a knob's and a button's width */
const char *const FX_NAME[5] = {"Chorus", "Reverb", "Space", "Delay", "Dly Mod"};
const char *const FX_CODE[5] = {"CHO", "REV", "SPC", "DLY", "MOD"};

/* ------------------------------------------------------------------- EFFECTS: SELECT and LEDs --- */
class FxSelect : public juce::Component
{
public:
    explicit FxSelect(Prophet67Panel &p) : panel(p) { setRepaintsOnMouseActivity(true); }
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        auto b = r.removeFromLeft((float)BW);
        auto lg = b.removeFromBottom(14.0f);
        drawButton(g, b.withSizeKeepingCentre(36.0f, 19.0f).withY(b.getY() + 34.0f), false, down, isMouseOver());
        drawLegend(g, "SELECT", lg);
        for (int i = 0; i < 5; i++) {
            auto it = item(i);
            drawLed(g, {it.getX() + 4.5f, it.getCentreY()}, 3.0f, panel.fx == i ? 1.0f : 0.0f);
            g.setColour(col::legend);
            g.setFont(font(Face::label, 10.5f));
            g.drawText(FX_NAME[i], it.withTrimmedLeft(12.0f), juce::Justification::centredLeft, false);
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        if (e.position.x < (float)BW) {
            down = true;
            panel.selectFx((panel.fx + 1) % 5);
        } else
            for (int i = 0; i < 5; i++)
                if (item(i).contains(e.position))
                    panel.selectFx(i);
        repaint();
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        down = false;
        repaint();
    }

private:
    juce::Rectangle<float> item(int i) const { return {(float)BW + 4.0f, 14.0f + 13.0f * (float)i, (float)getWidth() - BW - 4.0f, 13.0f}; }
    Prophet67Panel &panel;
    bool down = false;
};

/* ---------------------------------------------------------- the program buttons, TENS SELECT --- */
class ProgButton : public juce::Component
{
public:
    ProgButton(Prophet67Panel &p, int digit) : panel(p), digit(digit) { setRepaintsOnMouseActivity(true); }
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lg = r.removeFromBottom(14.0f);
        int prog = panel.tens * 10 + digit;
        bool on = panel.proc.getCurrentProgram() == prog, there = prog < panel.proc.programCount();
        drawKeycap(g, r.withSizeKeepingCentre(44.0f, r.getHeight() - 6.0f), on, true, down, isMouseOver() && there);
        drawLegend(g, juce::String(digit), lg, 11.0f);
    }
    void mouseDown(const juce::MouseEvent &) override
    {
        int prog = panel.tens * 10 + digit;
        down = true;
        if (prog < panel.proc.programCount())
            panel.proc.setCurrentProgram(prog);
        panel.repaint();
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        down = false;
        repaint();
    }

private:
    Prophet67Panel &panel;
    int digit;
    bool down = false;
};

class TensButton : public juce::Component
{
public:
    explicit TensButton(Prophet67Panel &p) : panel(p) { setRepaintsOnMouseActivity(true); }
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        drawLegend(g, "TENS", r.removeFromTop(14.0f));
        auto lg = r.removeFromBottom(14.0f);
        drawKeycap(g, r.withSizeKeepingCentre(44.0f, r.getHeight() - 6.0f), false, false, down, isMouseOver());
        drawLegend(g, "SELECT", lg);
    }
    void mouseDown(const juce::MouseEvent &) override
    {
        down = true;
        panel.tens = (panel.tens + 1) % ((panel.proc.programCount() + 9) / 10);
        panel.repaint();
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        down = false;
        repaint();
    }

private:
    Prophet67Panel &panel;
    bool down = false;
};

/* INIT: a grey key; the INIT program (a saw, the filter open), to start a sound from */
class InitKey : public juce::Component
{
public:
    explicit InitKey(Prophet67Panel &p) : panel(p) { setRepaintsOnMouseActivity(true); }
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lg = r.removeFromBottom(14.0f);
        drawKeycap(g, r.withSizeKeepingCentre(44.0f, r.getHeight() - 6.0f), false, false, down, isMouseOver());
        drawLegend(g, "INIT", lg);
    }
    void mouseDown(const juce::MouseEvent &) override
    {
        down = true;
        panel.proc.setCurrentProgram(NPRESETS - 1);   /* (the factory's last: INIT) */
        panel.repaint();
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        down = false;
        repaint();
    }

private:
    Prophet67Panel &panel;
    bool down = false;
};

/* PREV / NEXT: under the name, a program back or on (in the menu's order, round at the ends); held, they run on */
class StepKey : public juce::Component, private juce::Timer
{
public:
    StepKey(Prophet67Panel &p, int d) : panel(p), dir(d) { setRepaintsOnMouseActivity(true); }
    void paint(juce::Graphics &g) override   /* a small dark panel button, as SELECT or SYNC, an arrow on it, its legend below */
    {
        auto r = getLocalBounds().toFloat();
        auto lg = r.removeFromBottom(13.0f);
        auto b = r.withSizeKeepingCentre(34.0f, 17.0f);
        drawButton(g, b, false, down, isMouseOver());
        juce::Path tri;
        float cx = b.getCentreX(), cy = b.getCentreY() + (down ? 0.5f : 0.0f), w = 3.2f, h = 3.6f;
        tri.addTriangle(cx - dir * w, cy - h, cx - dir * w, cy + h, cx + dir * w, cy);
        g.setColour(col::legend.withAlpha(0.85f));
        g.fillPath(tri);
        drawLegend(g, dir < 0 ? "PREV" : "NEXT", lg, 9.5f);
    }
    void mouseDown(const juce::MouseEvent &) override
    {
        down = true;
        step();
        startTimer(450);
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        down = false;
        stopTimer();
        repaint();
    }

private:
    void timerCallback() override
    {
        step();
        startTimer(110);
    }
    void step()
    {
        int n = panel.proc.programCount();
        if (n <= 0)
            return;
        int cur = panel.proc.getCurrentProgram();
        panel.proc.setCurrentProgram(((cur + dir) % n + n) % n);
        panel.repaint();
    }
    Prophet67Panel &panel;
    int dir;
    bool down = false;
};

/* WRITE: the red key; asks a name, keeps the sound as a program of your own */
class WriteKey : public juce::Component
{
public:
    explicit WriteKey(Prophet67Panel &p) : panel(p) { setRepaintsOnMouseActivity(true); }
    void paint(juce::Graphics &g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        auto lg = r.removeFromBottom(14.0f);
        auto key = r.withSizeKeepingCentre(44.0f, r.getHeight() - 6.0f).translated(0.0f, down ? 1.2f : 0.0f);
        g.setColour(Colour(0xff050505));
        g.fillRoundedRectangle(key.expanded(2.5f), 3.5f);
        float split = key.getY() + key.getHeight() * 0.64f;
        g.setGradientFill(ColourGradient(Colour(0xffb81d12), key.getX(), split, Colour(0xff8e140c), key.getX(), key.getBottom(), false));
        g.fillRoundedRectangle(key.withTop(split - 3.0f), 2.0f);
        g.setGradientFill(ColourGradient(Colour(0xffe8392a), key.getX(), key.getY(), Colour(0xffcc2a1c), key.getX(), split, false));
        g.fillRoundedRectangle(key.withBottom(split), 2.0f);
        g.setColour(Colours::white.withAlpha(0.35f));
        g.drawLine(key.getX() + 1.5f, split, key.getRight() - 1.5f, split, 1.0f);
        g.drawLine(key.getX() + 2.0f, key.getY() + 0.8f, key.getRight() - 2.0f, key.getY() + 0.8f, 1.0f);
        auto c = Point<float>(key.getCentreX(), key.getY() + (split - key.getY()) * 0.42f);
        g.setColour(Colour(0xff5a0c07));
        g.fillEllipse(Rectangle<float>(11.0f, 11.0f).withCentre(c));
        g.setColour(Colour(0xff9a1a10));
        g.fillEllipse(Rectangle<float>(8.0f, 8.0f).withCentre(c));
        if (isMouseOver() && !down) {
            g.setColour(Colours::white.withAlpha(0.07f));
            g.fillRoundedRectangle(key, 2.0f);
        }
        drawLegend(g, "WRITE", lg);
    }
    void mouseDown(const juce::MouseEvent &) override
    {
        down = true;
        repaint();
    }
    void mouseUp(const juce::MouseEvent &e) override
    {
        down = false;
        repaint();
        if (getLocalBounds().contains(e.getPosition()))
            ask();
    }

private:
    void ask()
    {
        auto *w = new juce::AlertWindow("WRITE PROGRAM", "Keep the sound as a program of your own.\nThe same name again replaces it.",
                                        juce::MessageBoxIconType::NoIcon, this);
        w->setLookAndFeel(&getLookAndFeel());
        w->addTextEditor("name", panel.proc.getProgramName(panel.proc.getCurrentProgram()), "NAME (up to 11 characters)");
        if (auto *t = w->getTextEditor("name")) {
            t->setInputRestrictions(NAME_LEN - 1);
            t->selectAll();
        }
        w->addButton("WRITE", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<Prophet67Panel> self(&panel);
        pdlg::showInside(panel, w);
        w->enterModalState(true, juce::ModalCallbackFunction::create([self, w](int r) {
                               if (self && r == 1) {
                                   int at = self->proc.writePreset(w->getTextEditorContents("name"));
                                   if (at < 0)
                                       pdlg::message(*self, "WRITE",
                                                                              "The program could not be written to\n"
                                                                                  + Prophet67Processor::presetFolder().getFullPathName());
                                   else
                                       self->tens = at / 10;
                                   self->repaint();
                               }
                           }),
                           true);
    }
    Prophet67Panel &panel;
    bool down = false;
};

} // namespace

/* =============================================================================== the panel === */
Prophet67Panel::Prophet67Panel(Prophet67Processor &p) : proc(p)
{
    setLookAndFeel(&look);
    setOpaque(true);
    fx = juce::jlimit(0, 4, proc.uiPage.load());
    tens = proc.getCurrentProgram() / 10;
    build();
    selectFx(fx);
    startTimerHz(15);
}

Prophet67Panel::~Prophet67Panel()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void Prophet67Panel::build()
{
    auto &s = proc.apvts;
    auto P = [](int i) { return Prophet67Processor::paramId(i); };
    auto add = [&](juce::Component *c, juce::Rectangle<int> r) {
        controls.add(c);
        addAndMakeVisible(c);
        c->setBounds(r);
        return c;
    };

    /* a row of sections, each a run of items, spread over the panel's width */
    struct Item {
        int w;
        std::function<void(int x, int y)> place;
    };
    struct Sec {
        const char *title;
        std::vector<Item> items;
        int fixed = 0;                   /* a width of its own (the envelopes, one above the other) */
    };
    auto row = [&](int y, std::vector<Sec> secs) {   /* fixed gaps; the room left widens the sections, spreading their items */
        const float gap = 8.0f;
        float total = 0.0f;
        std::vector<float> nat, w;
        float flexible = 0.0f;
        for (auto &sc : secs) {
            float t = sc.title ? 20.0f : 0.0f;
            for (auto &it : sc.items)
                t += (float)it.w;
            nat.push_back(t);
            total += sc.fixed ? (float)sc.fixed : t;
            if (sc.title && !sc.fixed)
                flexible += t;
        }
        float extra = juce::jmax(0.0f, (float)(X1 - X0) - total - gap * (float)(secs.size() - 1));
        float x = (float)X0;
        for (size_t i = 0; i < secs.size(); i++) {
            float sw = secs[i].fixed ? (float)secs[i].fixed : nat[i] + (secs[i].title ? extra * nat[i] / flexible : 0.0f);
            int sx = (int)std::lround(x);
            if (secs[i].title)
                sections.push_back({{sx, y, (int)std::lround(sw), 120}, secs[i].title});
            float inner = nat[i] - (secs[i].title ? 20.0f : 0.0f), spare = sw - nat[i];
            float step = secs[i].items.empty() ? 0.0f : spare / (float)secs[i].items.size();
            float cx = (float)sx + (secs[i].title ? 10.0f : 0.0f) + step * 0.5f;
            juce::ignoreUnused(inner);
            for (auto &it : secs[i].items) {
                it.place((int)std::lround(cx), y);
                cx += (float)it.w + step;
            }
            x += sw + gap;
        }
    };
    auto K = [&](int i, const char *leg) {
        return Item{KW, [=, &s](int x, int y) {
                        auto *k = new Knob(s, P(i), leg, this);
                        k->getProperties().set("bipolar", bipolarKind(i));
                        add(k, {x, y + 22, KW, 80});
                    }};
    };
    auto BIGK = [&](int i, const char *leg) {   /* the low-pass CUTOFF: larger, the knob most played */
        return Item{80, [=, &s](int x, int y) { add(new Knob(s, P(i), leg, this), {x, y + 16, 80, 92}); }};
    };
    auto GK = [&](const char *id, const char *leg) { return Item{KW, [=, &s](int x, int y) { add(new Knob(s, id, leg, this), {x, y + 22, KW, 80}); }}; };
    auto L = [&](juce::String id, juce::StringArray names, int w, float rowH = 13.0f) {
        return Item{w, [=, &s](int x, int y) {
                        auto *l = new LedList(s, id, this, names);
                        l->rowH = rowH;
                        int h = juce::jmax(74, (int)std::ceil(rowH * (float)names.size()) + 4);
                        add(l, {x, y + 55 - h / 2, w, h});
                    }};
    };
    auto B = [&](juce::String id, const char *leg, int bit = 1) {
        return Item{BW, [=, &s](int x, int y) { add(new p67::Button(s, id, leg, this, bit), {x, y + 48, BW, 54}); }};
    };
    auto BR = [&](juce::String id, const char *leg, p67::Button::IsOn on, p67::Button::Next next) {
        return Item{BW, [=, &s](int x, int y) { add(new p67::Button(s, id, leg, this, on, next), {x, y + 48, BW, 54}); }};
    };
    /* VELOCITY as the P6's button: lit when it acts; off keeps the amount for when it comes back on */
    auto VEL = [&](juce::String id) {
        auto mem = std::make_shared<int>(100);
        return BR(id, "VELOCITY", [](int v) { return v > 0; }, [mem](int v) {
            if (v > 0) {
                *mem = v;
                return 0;
            }
            return *mem;
        });
    };
    /* KEYBOARD as the P6's: a button stepping OFF, HALF, FULL, its LEDs above */
    auto KB = [&](juce::String id) {
        return Item{BW + 6, [=, &s](int x, int y) {
                        auto *b = new p67::Button(s, id, "KEYBOARD", this, [](int) { return false; }, [](int v) { return (v + 1) % NKT; });
                        b->leds = {"Half", "Full"};
                        add(b, {x, y + 34, BW + 6, 70});
                    }};
    };
    auto SHAPE = [&](int i) {   /* with the waves around it: triangle, saw, pulse */
        return Item{80, [=, &s](int x, int y) {
                        auto *k = new Knob(s, P(i), "SHAPE", this);
                        k->getProperties().set("shapes", true);
                        add(k, {x, y + 18, 80, 84});
                    }};
    };

    /* OSC 2's mode as the P6's buttons: SYNC (osc 1 to osc 2), LOW FREQ, and KEYBOARD (LOW FREQ following the keys) */
    auto isSync = [](int v) { return v == O2M_SYNC; };
    auto sync = [](int v) { return v == O2M_SYNC ? O2M_NORMAL : O2M_SYNC; };
    auto isLow = [](int v) { return v == O2M_LOW || v == O2M_LOWFIX; };
    auto low = [](int v) { return v == O2M_LOW || v == O2M_LOWFIX ? O2M_NORMAL : O2M_LOW; };
    auto isKeyb = [](int v) { return v != O2M_LOWFIX; };
    auto keyb = [](int v) { return v == O2M_LOW ? O2M_LOWFIX : v == O2M_LOWFIX ? O2M_LOW : v; };
    const int fxSets[5][4] = {{P_CH_MIX, P_CH_RATE, P_CH_DEPTH, -1},
                              {P_RV_TYPE, P_RV_MIX, P_RV_DECAY, P_RV_TONE},
                              {P_SP_MOD, P_SP_SHIM, P_SP_SWELL, P_SP_PRE},
                              {P_DL_TIME_L, P_DL_TIME_R, P_DL_FB, P_DL_MIX},
                              {P_DL_DEPTH, P_DL_RATE, P_DL_TONE, -1}};
    const char *fxLegend[5][4] = {{"MIX", "RATE", "DEPTH", ""},
                                  {"TYPE", "MIX", "DECAY", "TONE"},
                                  {"MODULATE", "SHIMMER", "SWELL", "PRE-DELAY"},
                                  {"TIME L", "TIME R", "REPEATS", "MIX"},
                                  {"DEPTH", "RATE", "TONE", ""}};
    auto wideKnob = [&](int i, const char *leg, int w) {
        return Item{w, [=, &s](int x, int y) { add(new Knob(s, P(i), leg, this), {x, y + 22, w, 80}); }};
    };

    const int ENV_W = 306;               /* both envelopes: the same width, their knobs one above the other */

    /* ---- row 1: MASTER VOL, EFFECTS, CLOCK, ARPEGGIATOR, VINTAGE & VOICE SPREAD, DISTORT ---- */
    row(22, {
        {nullptr, {Item{60, [&](int x, int y) { add(new Knob(s, "volume", "MASTER VOL", this), {x, y + 22, 60, 80}); }}}},
        {"EFFECTS", {Item{106, [&](int x, int y) { add(new FxSelect(*this), {x, y + 22, 106, 82}); }},
                     Item{84, [&](int x, int y) {
                              add(new SegDisplay(3, [this] { return juce::String(FX_CODE[fx]); }), {x + 4, y + 26, 76, 36});
                              fxDisplay = add(new SegDisplay(3, [this] { return fxValueText(); }), {x + 4, y + 64, 76, 36});
                          }},
                     Item{KW * 4, [&](int x, int y) {
                              for (int f = 0; f < 5; f++)
                                  for (int k = 0; k < 4; k++)
                                      if (fxSets[f][k] >= 0) {
                                          auto *kn = new Knob(s, P(fxSets[f][k]), fxLegend[f][k], this);
                                          kn->getProperties().set("bipolar", bipolarKind(fxSets[f][k]));
                                          fxControls.push_back({add(kn, {x + KW * k, y + 22, KW, 80}), f});
                                      }
                          }},
                     Item{BW, [&](int x, int y) { fxControls.push_back({add(new p67::Button(s, P(P_DL_SYNC), "SYNC", this), {x, y + 48, BW, 54}), 3}); }}}},
        {"ARPEGGIATOR", {B("arp_on", "ON/OFF"), GK("arp_rate", "VALUE"), L("arp_rate", {"1/8", "1/8 T", "1/16", "1/16 T", "1/32"}, 46),
                         GK("arp_oct", "OCTAVES"), L("arp_oct", {"1", "2", "3", "4"}, 26), GK("arp_mode", "MODE"),
                         L("arp_mode", {"Up", "Down", "Up/Down", "Random", "Played"}, 58), GK("arp_gate", "GATE")}},
        {"VINTAGE & VOICE SPREAD", {wideKnob(P_VINTAGE, "VINTAGE", 64), wideKnob(P_SPREAD, "VOICE SPREAD", 72)}},
        {"DISTORT", {K(P_DIST, "AMOUNT")}},
    });

    /* ---- row 2: POLY MOD, LFO 1, LFO 2, AFTERTOUCH ---- */
    auto lfo = [&](int l) {
        return Sec{l ? "LFO 2" : "LFO 1",
                   {K(l ? P_L2_RATE : P_LFO_RATE, "FREQUENCY"), K(l ? P_L2_SYNC : P_L1_SYNC, "SYNC"),
                    L(P(l ? P_L2_SHAPE : P_LFO_SHAPE), {"Triangle", "Sawtooth", "Ramp", "Square", "Random"}, 64),
                    K(l ? P_L2_AMT : P_LFO_AMT, "INITIAL AMT"),
                    L(P(l ? P_L2_DEST : P_LFO_DEST), {"Freq 1+2", "Freq 1", "Freq 2", "PW 1+2", "Low-pass", "High-pass", "Amp"}, 66, 10.5f),
                    B(P(l ? P_L2_RETRIG : P_L1_RETRIG), "RETRIG")}};
    };
    row(148, {
        {"POLY MOD", {K(P_PM_FENV, "FILTER ENV"), K(P_PM_OSC2, "OSC 2"), B(P(P_PM_DOSC), "FREQ 1", PMO_FREQ),
                      B(P(P_PM_DOSC), "SHAPE 1", PMO_SHAPE), B(P(P_PM_DOSC), "PW 1", PMO_PW),
                      Item{BW, [&](int x, int y) {
                               auto *b = new p67::Button(s, P(P_PM_DFILT), "FILTER", this, [](int) { return false; },
                                                         [](int v) { return (v + 1) % 4; });
                               b->leds = {"LP", "HP"};
                               add(b, {x, y + 34, BW, 70});
                           }}}},
        lfo(0),
        lfo(1),
        {"AFTERTOUCH", {K(P_AT_AMT, "AMOUNT"), K(P_AT_CURVE, "CURVE"), K(P_AT_SMOOTH, "SMOOTH"), K(P_AT_DEST, "DEST"),
                        L(P(P_AT_DEST), {"Off", "Filter", "Swell", "Power", "Vibrato", "LFO 1", "Pitch", "Poly Mod"}, 66, 10.0f)}},
    });

    /* ---- row 3: OSCILLATOR 1, OSCILLATOR 2, LOW SPLIT, FILTER ENVELOPE ---- */
    row(274, {
        {"OSCILLATOR 1", {K(P_O1_FREQ, "FREQUENCY"), BR(P(P_O2_MODE), "SYNC", isSync, sync), SHAPE(P_O1_SHAPE),
                          K(P_O1_PW, "PULSE WIDTH")}},
        {"OSCILLATOR 2", {K(P_O2_FREQ, "FREQUENCY"), K(P_O2_FINE, "FINE"), SHAPE(P_O2_SHAPE), K(P_O2_PW, "PULSE WIDTH"),
                          Item{BW + 6, [&](int x, int y) {   /* LOW FREQ above KEYBOARD */
                              add(new p67::Button(s, P(P_O2_MODE), "LOW FREQ", this, isLow, low), {x, y + 16, BW + 6, 48});
                              add(new p67::Button(s, P(P_O2_MODE), "KEYBOARD", this, isKeyb, keyb), {x, y + 64, BW + 6, 48});
                          }}}},
        {"LOW SPLIT", {GK("split", "SPLIT"), L("split", {"Off", "-1 Oct", "-2 Oct", "-3 Oct"}, 42), Item{KW, [&](int x, int y) {   /* POINT: its note on a small display above the knob */
                           pointDisplay = add(new SegDisplay(3, [this] {
                               auto *p = proc.apvts.getParameter("split_point");
                               return p->getCurrentValueAsText();
                           }, true), {x + 5, y + 12, KW - 10, 24});
                           add(new Knob(s, "split_point", "POINT", this), {x, y + 36, KW, 66});
                       }},
                       B("low_hold", "LOW HOLD")}},
        {"FILTER ENVELOPE", {VEL("fenv_vel"), K(P_FA, "ATTACK"), K(P_FD, "DECAY"), K(P_FS, "SUSTAIN"), K(P_FR, "RELEASE")}, ENV_W},
    });

    /* ---- row 4: MIXER, LOW-PASS FILTER, HIGH-PASS FILTER, AMPLIFIER ENVELOPE ---- */
    row(400, {
        {"MIXER", {K(P_MIX_O1, "OSC 1"), K(P_MIX_O2, "OSC 2"), K(P_O1_SUB, "SUB OCTAVE"), K(P_NOISE, "NOISE")}},
        {"LOW-PASS FILTER", {BIGK(P_CUTOFF, "CUTOFF"), K(P_RESO, "RESONANCE"), K(P_ENV_AMT, "ENV AMOUNT"), VEL(P(P_FVEL)), KB(P(P_KEYTRK))}},
        {"HIGH-PASS FILTER", {K(P_HPF, "CUTOFF"), K(P_HP_RESO, "RESONANCE"), K(P_HP_ENV, "ENV AMOUNT"), VEL("hp_vel"), KB("hp_keytrk")}},
        {"AMPLIFIER ENVELOPE", {VEL(P(P_AVEL)), K(P_AA, "ATTACK"), K(P_AD, "DECAY"), K(P_AS, "SUSTAIN"), K(P_AR, "RELEASE")}, ENV_W},
    });

    /* ---- the program strip ---- */
    for (auto [id, leg, x] : {std::tuple<const char *, const char *, int>{"arp_hold", "HOLD", 68}, {"", "UNISON", 206}}) {
        auto *b = new p67::Button(s, juce::String(id).isEmpty() ? P(P_UNISON) : juce::String(id), leg, this);
        b->key = true;
        add(b, {x, 540, 54, 80});
    }
    add(new Knob(s, P(P_GLIDE), "GLIDE AMT", this), {134, 538, KW, 80});
    add(new Knob(s, P(P_BEND), "BEND RANGE", this), {268, 538, 64, 80});
    auto *prog = new SegDisplay(3, [this] { return juce::String(proc.getCurrentProgram()).paddedLeft('0', 3); });
    prog->onClick = [this] { presetMenu(); };
    add(prog, {342, 546, 112, 52});
    add(new TensButton(*this), {462, 528, 52, 94});
    for (int d = 0; d < 10; d++)
        progButtons.add(add(new ProgButton(*this, d), {530 + 50 * d, 540, 50, 80}));
    add(new InitKey(*this), {1034, 540, 50, 80});
    auto *name = new SegDisplay(12, [this] {
        auto n = proc.getProgramName(proc.getCurrentProgram()).toUpperCase();
        return shownEdited ? n.paddedRight(' ', 12).substring(0, 12) + "." : n;
    }, true);
    name->onClick = [this] { presetMenu(); };
    add(new WriteKey(*this), {1090, 540, 50, 80});
    nameDisplay = add(name, {1150, 546, 230, 52});
    add(new StepKey(*this, -1), {1222, 601, 40, 33});
    add(new StepKey(*this, 1), {1268, 601, 40, 33});
}

void Prophet67Panel::selectFx(int f)
{
    fx = f;
    proc.uiPage = f;
    fxShown = nullptr;
    for (auto &[c, ef] : fxControls)
        c->setVisible(ef == f);
    repaint(juce::Rectangle<int>(X0, 400, X1 - X0, 124));
}

juce::String Prophet67Panel::fxValueText() const
{
    auto *p = fxShown;
    if (!p)
        for (auto &[c, ef] : fxControls)
            if (ef == fx)
                if (auto *kn = dynamic_cast<Knob *>(c)) {
                    p = &kn->param;
                    break;
                }
    if (!p)
        return {};
    int v = (int)std::lround(p->convertFrom0to1(p->getValue()));
    if (dynamic_cast<juce::AudioParameterChoice *>(p))
        return p->getCurrentValueAsText().toUpperCase().substring(0, 3);
    return juce::String(v).paddedLeft('0', 3);
}

/* ------------------------------------------------------------------------------ painting --- */
static void paintWood(juce::Graphics &g, juce::Rectangle<float> r, int seed)
{
    using namespace juce;
    ColourGradient base(Colour(0xff3a1f10), r.getX(), 0.0f, Colour(0xff3a1f10), r.getRight(), 0.0f, false);
    base.addColour(0.4, Colour(0xff6a3c1f));
    base.addColour(0.7, Colour(0xff57311a));
    g.setGradientFill(base);
    g.fillRect(r);
    Random rnd(seed);
    for (int i = 0; i < 30; i++) {
        float x0 = r.getX() + rnd.nextFloat() * r.getWidth(), amp = 1.0f + rnd.nextFloat() * 3.5f,
              f = 0.004f + rnd.nextFloat() * 0.01f, ph = rnd.nextFloat() * 6.28f;
        Path p;
        p.startNewSubPath(x0, r.getY());
        for (float y = r.getY(); y <= r.getBottom(); y += 12.0f)
            p.lineTo(x0 + amp * std::sin(y * f + ph) + 1.2f * std::sin(y * f * 3.7f + ph * 2.0f), y);
        bool dark = rnd.nextFloat() < 0.7f;
        g.setColour(dark ? Colour(0xff1e0f06).withAlpha(0.16f + 0.2f * rnd.nextFloat())
                         : Colour(0xff9a6a42).withAlpha(0.10f + 0.12f * rnd.nextFloat()));
        g.strokePath(p, PathStrokeType(0.5f + rnd.nextFloat() * 1.6f));
    }
}

void Prophet67Panel::paintSection(juce::Graphics &g, const Section &s) { drawSection(g, s.r, s.title); }

void Prophet67Panel::paintBackground(juce::Graphics &g)
{
    using namespace juce;
    auto all = Rectangle<float>(0.0f, 0.0f, (float)W, (float)H);
    auto panel = all.reduced(40.0f, 0.0f);
    drawPanelFace(g, panel, 9000);

    paintWood(g, all.withWidth(40.0f), 1);
    paintWood(g, all.withLeft((float)W - 40.0f), 2);
    for (float x : {40.0f, (float)W - 40.0f}) {
        bool left = x < 100.0f;
        g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.5f), x, 0.0f, Colours::transparentBlack, x + (left ? 10.0f : -10.0f), 0.0f, false));
        g.fillRect(left ? Rectangle<float>(x, 0.0f, 10.0f, (float)H) : Rectangle<float>(x - 10.0f, 0.0f, 10.0f, (float)H));
    }

    for (auto &s : sections)
        paintSection(g, s);

    /* the strip's legends */
    drawLegend(g, "PROGRAM", Rectangle<float>(342.0f, 600.0f, 112.0f, 14.0f));
    drawLegend(g, "PROGRAM NAME", Rectangle<float>(1150.0f, 532.0f, 230.0f, 12.0f), 9.5f);

    /* the maker's tag, where a badge would be */
    drawTag(g, Rectangle<float>(1390.0f, 553.0f, 114.0f, 40.0f));
}

void Prophet67Panel::paint(juce::Graphics &g)
{
    float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (sc != backgroundScale || background.isNull()) {
        backgroundScale = sc;
        background = juce::Image(juce::Image::RGB, juce::jmax(1, (int)std::ceil(W * sc)), juce::jmax(1, (int)std::ceil(H * sc)), true);
        juce::Graphics bg(background);
        bg.addTransform(juce::AffineTransform::scale(sc));
        paintBackground(bg);
    }
    g.drawImage(background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

}

/* ------------------------------------------------------------------------------- the rest --- */
void Prophet67Panel::touched(juce::RangedAudioParameter &p)
{
    shown = &p;
    shownAt = juce::Time::getMillisecondCounter();
    for (auto &[c, ef] : fxControls)
        if (auto *kn = dynamic_cast<Knob *>(c); kn && &kn->param == &p) {
            fxShown = &p;
            if (fxDisplay)
                fxDisplay->repaint();
        }
    if (pointDisplay && p.getParameterID() == "split_point")
        pointDisplay->repaint();
}

void Prophet67Panel::timerCallback()
{
    if (++tick % 3 == 0) {
        bool e = false;
        int prog = proc.getCurrentProgram();
        if (auto *d = proc.programData(prog))
            for (int i = 0; i < NPARAM && !e; i++)
                e = (int)std::lround(proc.apvts.getRawParameterValue(Prophet67Processor::paramId(i))->load()) != d[i];
        if (e != shownEdited || prog != shownProgram) {
            shownEdited = e;
            if (prog != shownProgram)
                tens = prog / 10;
            shownProgram = prog;
            repaint(juce::Rectangle<int>(330, 530, 1060, 100));
        }
        if (fxDisplay)
            fxDisplay->repaint();
    }
    if (pointDisplay) {                  /* (POINT turned by the host, automation, a MIDI learn) */
        int pt = (int)proc.apvts.getRawParameterValue("split_point")->load();
        if (pt != pointShown) {
            pointShown = pt;
            pointDisplay->repaint();
        }
    }
}

void Prophet67Panel::presetMenu()
{
    juce::PopupMenu m;
    auto item = [this](juce::PopupMenu &menu, int i) {
        menu.addItem(i + 1, juce::String(i).paddedLeft('0', 3) + "   " + proc.getProgramName(i), true, i == proc.getCurrentProgram());
    };
    m.addSectionHeader("FACTORY");
    for (int i = 0; i < NPRESETS; i++)
        item(m, i);
    m.addSectionHeader("SOUND LIBRARY");        /* the VST's own 50, by kind */
    for (int g = 0; g < NVST_GROUPS; g++) {
        juce::PopupMenu sub;
        int end = g + 1 < NVST_GROUPS ? VST_GROUPS[g + 1].first : NVST_PRESETS;
        bool here = false;
        for (int k = VST_GROUPS[g].first; k < end; k++) {
            item(sub, NPRESETS + k);
            here |= proc.getCurrentProgram() == NPRESETS + k;
        }
        m.addSubMenu(VST_GROUPS[g].name, sub, true, nullptr, here);
    }
    m.addSectionHeader("YOUR PROGRAMS");
    bool own = false;
    for (int i = Prophet67Processor::NFACTORY; i < proc.programCount(); i++)
        if (proc.userPresets[(size_t)(i - Prophet67Processor::NFACTORY)].p6 < 0) {
            item(m, i);
            own = true;
        }
    if (!own)
        m.addItem(-1, "(WRITE keeps the sound as a program of your own)", false, false);
    /* LOAD P6 SYSEX's: by the Prophet-6's banks, numbered as on it */
    juce::PopupMenu bank[10];
    bool bankHere[10] = {}, any = false;
    for (int i = Prophet67Processor::NFACTORY; i < proc.programCount(); i++) {
        int num = proc.userPresets[(size_t)(i - Prophet67Processor::NFACTORY)].p6;
        if (num < 0)
            continue;
        int b = juce::jlimit(0, 9, num / 100);
        bank[b].addItem(i + 1, juce::String(num).paddedLeft('0', 3) + "   " + proc.getProgramName(i), true, i == proc.getCurrentProgram());
        bankHere[b] |= i == proc.getCurrentProgram();
        any = true;
    }
    if (any) {
        m.addSectionHeader("P6 LIBRARY");
        for (int b = 0; b < 10; b++)
            if (bank[b].getNumItems() > 0)
                m.addSubMenu(b == 9 ? juce::String("EDIT BUFFER") : "BANK " + juce::String(b) + "   (" + juce::String(b * 100).paddedLeft('0', 3) + "-"
                                 + juce::String(b * 100 + 99) + ")", bank[b], true, nullptr, bankHere[b]);
    }
    m.addSeparator();
    m.addItem(LOAD_P6, "LOAD P6 SYSEX...");
    juce::Component::SafePointer<Prophet67Panel> self(this);
    auto area = nameDisplay ? nameDisplay->getScreenBounds() : getScreenBounds();
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(area).withMinimumWidth(240), [self](int r) {
        if (!self)
            return;
        if (r == LOAD_P6)
            self->loadP6();
        else if (r > 0) {
            self->proc.setCurrentProgram(r - 1);
            self->repaint();
        }
    });
}

void Prophet67Panel::loadP6()
{
    chooser = std::make_unique<juce::FileChooser>("LOAD P6 SYSEX: a Prophet-6 program file",
                                                  juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.syx");
    juce::Component::SafePointer<Prophet67Panel> self(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [self](const juce::FileChooser &fc) {
        if (!self)
            return;
        auto f = fc.getResult();
        if (f == juce::File())
            return;
        int n = self->proc.importP6(f);
        juce::String msg = n > 0 ? juce::String(n) + " Prophet-6 programs are now in the program menu, under P6 LIBRARY.\n\n"
                                       "They are kept in Documents/PROBLEM 6/Presets/P6 LIBRARY, as programs of your own."
                                 : juce::String("No Prophet-6 programs were found in this file.");
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "LOAD P6 SYSEX", msg);
        self->repaint();
    });
}

/* ============================================================================== the editor === */
Prophet67Editor::Prophet67Editor(Prophet67Processor &p) : AudioProcessorEditor(&p), proc(p), panel(p)
{
    addAndMakeVisible(panel);
    panel.setBounds(0, 0, Prophet67Panel::W, Prophet67Panel::H);
    const double W = Prophet67Panel::W, H = Prophet67Panel::H;
    /* the size it was left at, read before the limits below resize the empty window; a new instance (still at the
     * default size): the size last used anywhere */
    const float kept = proc.uiScale.load();
    setResizable(true, true);
    setResizeLimits((int)(W * 0.5), (int)(H * 0.5), (int)(W * 1.6), (int)(H * 1.6));
    getConstrainer()->setFixedAspectRatio(W / H);
    float s = juce::jlimit(0.5f, 1.6f, std::abs(kept - 0.82f) < 1e-4f ? (float)winsize::last("PROBLEM-6", 0.82) : kept);
    sized = true;
    setSize((int)std::lround(W * s), (int)std::lround(H * s));
}

void Prophet67Editor::resized()
{
    float s = (float)getWidth() / (float)Prophet67Panel::W;
    panel.setTransform(juce::AffineTransform::scale(s));
    if (!sized)
        return;
    proc.uiScale = s;
    winsize::remember("PROBLEM-6", (double)s);
}
