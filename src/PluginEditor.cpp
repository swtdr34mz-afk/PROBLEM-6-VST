/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
#include "PluginEditor.h"

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

/* ---------------------------------------------------------------------------- the keyboard --- */
class Keys : public juce::MidiKeyboardComponent
{
public:
    explicit Keys(juce::MidiKeyboardState &s) : MidiKeyboardComponent(s, horizontalKeyboard)
    {
        setBlackNoteLengthProportion(0.62f);
        setBlackNoteWidthProportion(0.62f);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
        setScrollButtonsVisible(false);
        setColour(whiteNoteColourId, juce::Colour(0xff0c0c0d));   /* (the bed between the keys) */
        setColour(shadowColourId, juce::Colours::transparentBlack);
    }
    void drawWhiteNote(int, juce::Graphics &g, juce::Rectangle<float> a, bool down, bool, juce::Colour, juce::Colour) override
    {
        using namespace juce;
        auto r = a.reduced(0.8f, 0.0f);
        float lip = down ? 3.0f : 9.0f;
        g.setGradientFill(ColourGradient(down ? Colour(0xffd9d6cc) : Colour(0xfff7f5ef), r.getX(), r.getY(),
                                         down ? Colour(0xffc9c6bb) : Colour(0xffe6e3da), r.getX(), r.getBottom(), false));
        g.fillRect(r.withTrimmedBottom(lip));
        g.setGradientFill(ColourGradient(Colour(0xffd2cfc5), r.getX(), r.getBottom() - lip, Colour(0xffa9a69c), r.getX(), r.getBottom(), false));
        g.fillRect(r.withTop(r.getBottom() - lip));
        g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.45f), r.getX(), r.getY(), Colours::transparentBlack, r.getX(),
                                         r.getY() + 14.0f, false));
        g.fillRect(r.withHeight(14.0f));
        if (down) {
            g.setColour(Colours::black.withAlpha(0.08f));
            g.fillRect(r);
        }
    }
    void drawBlackNote(int, juce::Graphics &g, juce::Rectangle<float> a, bool down, bool, juce::Colour) override
    {
        using namespace juce;
        g.setColour(Colours::black.withAlpha(0.5f));
        g.fillRect(a.translated(2.0f, 3.0f));
        g.setColour(Colour(0xff050505));
        g.fillRect(a);
        auto top = a.reduced(a.getWidth() * 0.14f, 0.0f).withTrimmedBottom(down ? a.getHeight() * 0.05f : a.getHeight() * 0.12f);
        g.setGradientFill(ColourGradient(Colour(0xff1b1b1d), top.getX(), top.getY(), Colour(0xff2e2e31), top.getX(), top.getBottom(), false));
        g.fillRect(top);
        g.setColour(Colours::white.withAlpha(0.16f));
        g.fillRect(top.withWidth(1.5f).translated(1.0f, 0.0f));
        g.setColour(Colours::white.withAlpha(0.08f));
        g.fillRect(top.withTop(top.getBottom() - 2.0f));
    }
};

/* ------------------------------------------------------------------- the pitch and mod wheels --- */
class Wheel : public juce::Component
{
public:
    Wheel(bool spring, std::function<void(float)> send) : spring(spring), send(std::move(send)) { pos = spring ? 0.5f : 0.0f; }
    void paint(juce::Graphics &g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        auto well = r.withTrimmedBottom(18.0f);
        g.setColour(Colour(0xff030303));
        g.fillRoundedRectangle(well, 4.0f);
        auto glow = well.reduced(3.0f);           /* the red light in the well */
        g.setGradientFill(ColourGradient(col::redLed.withAlpha(0.75f), glow.getCentreX(), glow.getBottom() - 6.0f,
                                         Colours::transparentBlack, glow.getCentreX(), glow.getY() + glow.getHeight() * 0.25f, false));
        g.fillRoundedRectangle(glow, 3.0f);
        auto wheel = well.reduced(6.0f, 8.0f);
        float h = wheel.getHeight();
        for (float y = 0.0f; y < h; y += 1.0f) {  /* a cylinder, its ribs turning with the position */
            float t = (y / h) * 2.0f - 1.0f, shade = std::sqrt(jmax(0.0f, 1.0f - t * t));
            float rib = std::fmod((std::asin(jlimit(-1.0f, 1.0f, t)) * 9.0f + pos * 18.0f) + 100.0f, 1.0f);
            float v = 0.05f + 0.20f * shade * (rib < 0.5f ? 1.0f : 0.55f);
            g.setColour(Colour::fromFloatRGBA(v, v, v * 1.02f, 1.0f));
            g.fillRect(wheel.getX(), wheel.getY() + y, wheel.getWidth(), 1.0f);
        }
        g.setColour(Colours::black);
        g.drawRect(wheel, 1.0f);
        float my = wheel.getBottom() - h * pos;   /* the marker */
        g.setColour(Colours::white.withAlpha(0.6f));
        g.fillRect(wheel.getX() + 2.0f, my - 1.0f, wheel.getWidth() - 4.0f, 2.0f);
        drawLegend(g, spring ? "PITCH" : "MOD", r.removeFromBottom(14.0f), 9.5f);
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        start = pos;
        startY = e.position.y;
    }
    void mouseDrag(const juce::MouseEvent &e) override
    {
        set(juce::jlimit(0.0f, 1.0f, start - (e.position.y - startY) / ((float)getHeight() * 0.8f)));
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        if (spring)
            set(0.5f);
    }

private:
    void set(float p)
    {
        if (p == pos)
            return;
        pos = p;
        send(p);
        repaint();
    }
    bool spring;
    std::function<void(float)> send;
    float pos, start = 0.0f, startY = 0.0f;
};

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
        bool on = panel.proc.getCurrentProgram() == prog, there = prog < panel.proc.getNumPrograms();
        drawKeycap(g, r.withSizeKeepingCentre(44.0f, r.getHeight() - 6.0f), on, true, down, isMouseOver() && there);
        drawLegend(g, juce::String(digit), lg, 11.0f);
    }
    void mouseDown(const juce::MouseEvent &) override
    {
        int prog = panel.tens * 10 + digit;
        down = true;
        if (prog < panel.proc.getNumPrograms())
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
        panel.tens = (panel.tens + 1) % ((panel.proc.getNumPrograms() + 9) / 10);
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
        w->enterModalState(true, juce::ModalCallbackFunction::create([self, w](int r) {
                               if (self && r == 1) {
                                   int at = self->proc.writePreset(w->getTextEditorContents("name"));
                                   if (at < 0)
                                       juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "WRITE",
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

float textWidth(const juce::Font &f, const juce::String &t)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText(f, t, 0.0f, 0.0f);
    return ga.getBoundingBox(0, -1, true).getWidth();
}
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

    const int ENV_W = 340;               /* both envelopes: the same width, their knobs one above the other */

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
        {"CLOCK", {Item{96, [&](int x, int y) {
                       bpmDisplay = add(new SegDisplay(3, [this] { return juce::String(juce::roundToInt(proc.hostBpm.load())); }),
                                        {x + 6, y + 34, 84, 44});
                   }},
                   GK("swing", "SWING")}},
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

    /* ---- row 3: OSCILLATOR 1, OSCILLATOR 2, LOW SPLIT, AMPLIFIER ENVELOPE ---- */
    row(274, {
        {"OSCILLATOR 1", {K(P_O1_FREQ, "FREQUENCY"), BR(P(P_O2_MODE), "SYNC", isSync, sync), K(P_O1_SHAPE, "SHAPE"),
                          K(P_O1_PW, "PULSE WIDTH")}},
        {"OSCILLATOR 2", {K(P_O2_FREQ, "FREQUENCY"), K(P_O2_FINE, "FINE"), K(P_O2_SHAPE, "SHAPE"), K(P_O2_PW, "PULSE WIDTH"),
                          Item{BW + 6, [&](int x, int y) {   /* LOW FREQ above KEYBOARD */
                              add(new p67::Button(s, P(P_O2_MODE), "LOW FREQ", this, isLow, low), {x, y + 16, BW + 6, 48});
                              add(new p67::Button(s, P(P_O2_MODE), "KEYBOARD", this, isKeyb, keyb), {x, y + 64, BW + 6, 48});
                          }}}},
        {"LOW SPLIT", {GK("split", "SPLIT"), L("split", {"Off", "-1 Oct", "-2 Oct", "-3 Oct"}, 42), GK("split_point", "POINT"),
                       GK("scale_l", "SCALE L"), GK("scale_r", "SCALE R"), GK("key", "KEY"), GK("follow", "FOLLOW"),
                       B("low_hold", "LOW HOLD"), Item{KW, [&](int x, int y) {
                           auto *k = new Knob(s, "balance", "BALANCE", this);
                           k->getProperties().set("bipolar", true);
                           add(k, {x, y + 22, KW, 80});
                       }}}},
        {"AMPLIFIER ENVELOPE", {K(P_AVEL, "VELOCITY"), K(P_AA, "ATTACK"), K(P_AD, "DECAY"), K(P_AS, "SUSTAIN"), K(P_AR, "RELEASE")}, ENV_W},
    });

    /* ---- row 4: MIXER, LOW-PASS FILTER, HIGH-PASS FILTER, FILTER ENVELOPE ---- */
    row(400, {
        {"MIXER", {K(P_MIX_O1, "OSC 1"), K(P_MIX_O2, "OSC 2"), K(P_O1_SUB, "SUB OCTAVE"), K(P_NOISE, "NOISE")}},
        {"LOW-PASS FILTER", {K(P_CUTOFF, "CUTOFF"), K(P_RESO, "RESONANCE"), K(P_ENV_AMT, "ENV AMOUNT"),
                             BR(P(P_KEYTRK), "KEYBOARD", [](int) { return false; }, [](int v) { return (v + 1) % NKT; }),
                             L(P(P_KEYTRK), {"Off", "Half", "Full"}, 44)}},
        {"HIGH-PASS FILTER", {K(P_HPF, "CUTOFF"), K(P_HP_RESO, "RESONANCE"), K(P_HP_ENV, "ENV AMOUNT")}},
        {"FILTER ENVELOPE", {K(P_FVEL, "VELOCITY"), K(P_FA, "ATTACK"), K(P_FD, "DECAY"), K(P_FS, "SUSTAIN"), K(P_FR, "RELEASE")}, ENV_W},
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
    add(prog, {376, 546, 112, 52});
    add(new TensButton(*this), {500, 528, 52, 94});
    for (int d = 0; d < 10; d++)
        progButtons.add(add(new ProgButton(*this, d), {574 + 50 * d, 540, 50, 80}));
    auto *name = new SegDisplay(12, [this] {
        auto n = proc.getProgramName(proc.getCurrentProgram()).toUpperCase();
        return shownEdited ? n.paddedRight(' ', 12).substring(0, 12) + "." : n;
    }, true);
    name->onClick = [this] { presetMenu(); };
    add(new WriteKey(*this), {1090, 540, 50, 80});
    nameDisplay = add(name, {1150, 546, 230, 52});

    /* ---- the keyboard and the wheels ---- */
    auto *k = new Keys(proc.keyboardState);
    keys.add(k);
    addAndMakeVisible(k);
    k->setBounds(168, 648, 1336, 244);
    k->setAvailableRange(36, 84);
    k->setKeyWidth(1336.0f / 29.0f);
    k->setLowestVisibleKey(36);
    auto *pitch = keys.add(new Wheel(true, [this](float v) {
        auto m = juce::MidiMessage::pitchWheel(1, juce::jlimit(0, 16383, (int)std::lround(v * 16383.0f)));
        m.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        proc.uiMidi.addMessageToQueue(m);
    }));
    auto *mod = keys.add(new Wheel(false, [this](float v) {
        auto m = juce::MidiMessage::controllerEvent(1, 1, juce::jlimit(0, 127, (int)std::lround(v * 127.0f)));
        m.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        proc.uiMidi.addMessageToQueue(m);
    }));
    addAndMakeVisible(pitch);
    addAndMakeVisible(mod);
    pitch->setBounds(72, 690, 34, 170);
    mod->setBounds(116, 690, 34, 170);
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

void Prophet67Panel::paintSection(juce::Graphics &g, const Section &s)
{
    using namespace juce;
    auto r = s.r.toFloat().reduced(0.5f);
    auto f = font(Face::title, 11.5f).withExtraKerningFactor(0.06f);
    float tw = textWidth(f, s.title), cx = r.getCentreX(), gl = cx - tw * 0.5f - 6.0f, gr = cx + tw * 0.5f + 6.0f, rad = 6.0f;
    Path p;
    p.startNewSubPath(gr, r.getY());
    p.lineTo(r.getRight() - rad, r.getY());
    p.quadraticTo(r.getRight(), r.getY(), r.getRight(), r.getY() + rad);
    p.lineTo(r.getRight(), r.getBottom() - rad);
    p.quadraticTo(r.getRight(), r.getBottom(), r.getRight() - rad, r.getBottom());
    p.lineTo(r.getX() + rad, r.getBottom());
    p.quadraticTo(r.getX(), r.getBottom(), r.getX(), r.getBottom() - rad);
    p.lineTo(r.getX(), r.getY() + rad);
    p.quadraticTo(r.getX(), r.getY(), r.getX() + rad, r.getY());
    p.lineTo(gl, r.getY());
    g.setColour(col::frame);
    g.strokePath(p, PathStrokeType(1.4f));
    g.setColour(col::legend);
    g.setFont(f);
    g.drawText(s.title, Rectangle<float>(gl, r.getY() - 8.0f, gr - gl, 16.0f), Justification::centred, false);
}

void Prophet67Panel::paintBackground(juce::Graphics &g)
{
    using namespace juce;
    auto all = Rectangle<float>(0.0f, 0.0f, (float)W, (float)H);
    auto panel = all.reduced(40.0f, 0.0f);
    g.setGradientFill(ColourGradient(col::panelTop, 0.0f, 0.0f, col::panelBottom, 0.0f, 640.0f, false));
    g.fillRect(panel.withBottom(640.0f));
    Random rnd(67);                      /* a matte finish */
    for (int i = 0; i < 9000; i++) {
        float x = panel.getX() + rnd.nextFloat() * panel.getWidth(), y = rnd.nextFloat() * 640.0f;
        g.setColour((rnd.nextBool() ? Colours::white : Colours::black).withAlpha(0.025f + 0.03f * rnd.nextFloat()));
        g.fillRect(x, y, 1.0f, 1.0f);
    }
    /* the keyboard's bed and the wheels' block */
    g.setColour(Colour(0xff0f0f10));
    g.fillRect(panel.withTop(640.0f));
    g.setGradientFill(ColourGradient(Colour(0xff1d1e20), 56.0f, 648.0f, Colour(0xff111112), 56.0f, 892.0f, false));
    g.fillRect(Rectangle<float>(56.0f, 648.0f, 106.0f, 244.0f));
    g.setColour(Colours::black.withAlpha(0.7f));
    g.fillRect(Rectangle<float>(40.0f, 638.0f, panel.getWidth(), 4.0f));
    g.setColour(Colours::white.withAlpha(0.06f));
    g.drawHorizontalLine(637, 40.0f, (float)W - 40.0f);

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
    drawLegend(g, "PROGRAM", Rectangle<float>(376.0f, 600.0f, 112.0f, 14.0f));
    drawLegend(g, "PROGRAM NAME", Rectangle<float>(1150.0f, 532.0f, 230.0f, 12.0f), 9.5f);

    /* the badge */
    auto badge = Rectangle<float>(1392.0f, 553.0f, 110.0f, 40.0f);
    g.setColour(Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(badge.translated(0.0f, 2.0f), 3.0f);
    g.setGradientFill(ColourGradient(Colour(0xffc9cacb), badge.getX(), badge.getY(), Colour(0xff8f9092), badge.getX(), badge.getBottom(), false));
    g.fillRoundedRectangle(badge, 3.0f);
    Random brush(6);
    for (int i = 0; i < 60; i++) {
        g.setColour((brush.nextBool() ? Colours::white : Colours::black).withAlpha(0.05f));
        g.drawHorizontalLine((int)(badge.getY() + brush.nextFloat() * badge.getHeight()), badge.getX() + 2.0f, badge.getRight() - 2.0f);
    }
    g.setColour(Colour(0xff4d4e50));
    g.drawRoundedRectangle(badge, 3.0f, 1.0f);
    g.setColour(Colour(0xff2b2c2e));
    g.setFont(font(Face::logo, 19.0f).withExtraKerningFactor(0.02f));
    g.drawText("prophet 67", badge.reduced(8.0f, 0.0f), Justification::centred, false);
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

    /* the readout: what is in hand, else nothing (as on the panel) */
    if (shown && juce::Time::getMillisecondCounter() - shownAt < 3000) {
        g.setColour(col::legend);
        g.setFont(font(Face::label, 11.0f));
        g.drawText(shown->getName(40) + ":  " + shown->getCurrentValueAsText(), readoutR, juce::Justification::centred, true);
    }
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
    repaint(readoutR.expanded(2));
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
            repaint(juce::Rectangle<int>(360, 530, 1000, 100));
        }
        if (bpmDisplay)
            bpmDisplay->repaint();
        if (fxDisplay)
            fxDisplay->repaint();
    }
    auto age = juce::Time::getMillisecondCounter() - shownAt;
    if (shown && age > 3000 && age < 3300)
        repaint(readoutR.expanded(2));
}

void Prophet67Panel::presetMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader("FACTORY");
    for (int i = 0; i < proc.getNumPrograms(); i++) {
        if (i == NPRESETS)
            m.addSectionHeader("YOUR PROGRAMS");
        m.addItem(i + 1, juce::String(i).paddedLeft('0', 3) + "   " + proc.getProgramName(i), true, i == proc.getCurrentProgram());
    }
    if (proc.userPresets.empty())
        m.addItem(-1, "(WRITE keeps the sound as a program of your own)", false, false);
    juce::Component::SafePointer<Prophet67Panel> self(this);
    auto area = nameDisplay ? nameDisplay->getScreenBounds() : getScreenBounds();
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(area).withMinimumWidth(240), [self](int r) {
        if (self && r > 0) {
            self->proc.setCurrentProgram(r - 1);
            self->repaint();
        }
    });
}

/* ============================================================================== the editor === */
Prophet67Editor::Prophet67Editor(Prophet67Processor &p) : AudioProcessorEditor(&p), proc(p), panel(p)
{
    addAndMakeVisible(panel);
    panel.setBounds(0, 0, Prophet67Panel::W, Prophet67Panel::H);
    const double W = Prophet67Panel::W, H = Prophet67Panel::H;
    setResizable(true, true);
    setResizeLimits((int)(W * 0.5), (int)(H * 0.5), (int)(W * 1.6), (int)(H * 1.6));
    getConstrainer()->setFixedAspectRatio(W / H);
    float s = juce::jlimit(0.5f, 1.6f, proc.uiScale.load());
    setSize((int)std::lround(W * s), (int)std::lround(H * s));
}

void Prophet67Editor::resized()
{
    float s = (float)getWidth() / (float)Prophet67Panel::W;
    panel.setTransform(juce::AffineTransform::scale(s));
    proc.uiScale = s;
}
