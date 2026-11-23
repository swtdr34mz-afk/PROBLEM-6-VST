/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
#include "PluginEditor.h"
#include "Sections.h"

/* ------------------------------------------------------------------------------- the look --- */
const juce::Colour P67Look::panel{0xff15171a}, P67Look::box{0xff202328}, P67Look::red{0xffd8312b},
    P67Look::text{0xffe8e6e1}, P67Look::dim{0xff8b8f96};

P67Look::P67Look()
{
    setColour(juce::ResizableWindow::backgroundColourId, panel);
    setColour(juce::Label::textColourId, text);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::rotarySliderFillColourId, red);
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff3a3e45));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff2a2e34));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3a3e45));
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::arrowColourId, red);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff202328));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, red);
    setColour(juce::ToggleButton::textColourId, text);
    setColour(juce::ToggleButton::tickColourId, red);
    setColour(juce::ToggleButton::tickDisabledColourId, dim);
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2e34));
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TabbedButtonBar::tabTextColourId, dim);
    setColour(juce::TabbedButtonBar::frontTextColourId, text);
    setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
}

void P67Look::drawRotarySlider(juce::Graphics &g, int x, int y, int w, int h, float pos, float a0, float a1,
                               juce::Slider &)
{
    auto r = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(4.0f);
    float d = juce::jmin(r.getWidth(), r.getHeight());
    auto c = r.withSizeKeepingCentre(d, d);
    float ang = a0 + pos * (a1 - a0), rad = d * 0.5f;
    juce::Path arc;
    arc.addCentredArc(c.getCentreX(), c.getCentreY(), rad - 2.0f, rad - 2.0f, 0.0f, a0, a1, true);
    g.setColour(findColour(juce::Slider::rotarySliderOutlineColourId));
    g.strokePath(arc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val;
    val.addCentredArc(c.getCentreX(), c.getCentreY(), rad - 2.0f, rad - 2.0f, 0.0f, a0, ang, true);
    g.setColour(red);
    g.strokePath(val, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto knob = c.reduced(d * 0.2f);
    g.setColour(juce::Colour(0xff0d0e10));
    g.fillEllipse(knob);
    g.setColour(juce::Colour(0xff3a3e45));
    g.drawEllipse(knob, 1.0f);
    juce::Point<float> p1 = c.getCentre().getPointOnCircumference(knob.getWidth() * 0.15f, ang),
                       p2 = c.getCentre().getPointOnCircumference(knob.getWidth() * 0.48f, ang);
    g.setColour(text);
    g.drawLine({p1, p2}, 2.0f);
}

/* ---------------------------------------------------------------------------- a control --- */
P67Control::P67Control(juce::AudioProcessorValueTreeState &s, const juce::String &id, const juce::String &name)
{
    auto *p = s.getParameter(id);
    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, P67Look::dim);
    addAndMakeVisible(label);
    if (dynamic_cast<juce::AudioParameterBool *>(p)) {
        toggle = std::make_unique<juce::ToggleButton>("ON");
        addAndMakeVisible(*toggle);
        toggleA = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(s, id, *toggle);
    } else if (auto *c = dynamic_cast<juce::AudioParameterChoice *>(p)) {
        menu = std::make_unique<juce::ComboBox>();
        menu->addItemList(c->choices, 1);
        menu->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*menu);
        menuA = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(s, id, *menu);
    } else {
        knob = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 18);
        knob->setMouseDragSensitivity(220);
        knob->setVelocityBasedMode(false);
        addAndMakeVisible(*knob);
        knobA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(s, id, *knob);
        if (p)
            knob->setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
    }
}

void P67Control::resized()
{
    auto r = getLocalBounds();
    label.setBounds(r.removeFromTop(16));
    if (knob)
        knob->setBounds(r);
    if (menu)
        menu->setBounds(r.withSizeKeepingCentre(juce::jmin(r.getWidth() - 4, 100), 24));
    if (toggle)
        toggle->setBounds(r.withSizeKeepingCentre(60, 24));
}

void P67Control::refresh()
{
    if (knob)
        knob->updateText();
}

/* ------------------------------------------------------------------------------ a section --- */
void P67Group::add(std::unique_ptr<P67Control> c)
{
    addAndMakeVisible(*c);
    controls.add(c.release());
}

void P67Group::paint(juce::Graphics &g)
{
    auto r = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(P67Look::box);
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(P67Look::red);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(title, r.removeFromTop(22.0f).reduced(10.0f, 0.0f), juce::Justification::centredLeft);
}

void P67Group::resized()
{
    auto r = getLocalBounds().reduced(8, 4);
    r.removeFromTop(20);
    int n = controls.size(), w = n ? juce::jmin(110, r.getWidth() / n) : 0;
    for (auto *c : controls)
        c->setBounds(r.removeFromLeft(w));
}

/* -------------------------------------------------------------------------------- a tab --- */
P67Group &P67Page::group(const juce::String &title)
{
    auto *g = groups.add(new P67Group(title));
    addAndMakeVisible(g);
    return *g;
}

void P67Page::resized()
{
    auto r = getLocalBounds().reduced(8);
    int rows = groups.size(), h = rows ? juce::jmin(124, r.getHeight() / rows) : 0;
    for (auto *g : groups) {
        int w = juce::jmin(r.getWidth(), 16 + 110 * g->controls.size());
        g->setBounds(r.removeFromTop(h).withWidth(w));
    }
}

/* ----------------------------------------------------------------------------- the editor --- */
Prophet67Editor::Prophet67Editor(Prophet67Processor &p) : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&look);
    auto &s = proc.apvts;
    auto P = [&](int i) { return Prophet67Processor::paramId(i); };
    auto ctl = [&](const juce::String &id, const juce::String &name) { return std::make_unique<P67Control>(s, id, name); };
    auto label = [](int i) {
        juce::String n(PARAMS[i].name);
        return i == P_O2_MODE ? juce::String("OSC 2 MODE") : n;
    };
    auto page = [&](const juce::String &name) -> P67Page & {
        auto *pg = pages.add(new P67Page());
        tabs.addTab(name, P67Look::panel, pg, false);
        return *pg;
    };
    auto params = [&](P67Page &pg, const juce::String &title, int first, int count) {
        auto &g = pg.group(title);
        for (int i = first; i < first + count; i++)
            g.add(ctl(P(i), label(i)));
    };
    auto globals = [&](P67Page &pg, const juce::String &title, std::initializer_list<std::pair<const char *, const char *>> ids) {
        auto &g = pg.group(title);
        for (auto &[id, name] : ids)
            g.add(ctl(id, name));
    };

    auto &osc = page("OSC");
    params(osc, "OSCILLATOR 1", P_O1_FREQ, 4);
    params(osc, "OSCILLATOR 2", P_O2_FREQ, 4);
    params(osc, "MIXER", P_MIX_O1, 4);
    auto &flt = page("FILTER");
    params(flt, "LOW-PASS FILTER", P_CUTOFF, 4);
    params(flt, "HIGH-PASS / KEYBOARD", P_HPF, 4);
    auto &env = page("ENV");
    params(env, "FILTER ENVELOPE", P_FA, 4);
    params(env, "AMP ENVELOPE", P_AA, 4);
    auto &mod = page("MOD");
    params(mod, "LFO 1", P_LFO_RATE, 4);
    params(mod, "LFO 2", P_L2_RATE, 4);
    params(mod, "LFO SYNC", P_L1_SYNC, 4);
    params(mod, "POLY MOD", P_PM_FENV, 4);
    auto &voice = page("VOICE");
    params(voice, "VOICE", P_GLIDE, 4);
    params(voice, "AFTERTOUCH", P_AT_DEST, 4);
    auto &fx = page("FX");
    params(fx, "REVERB", P_RV_TYPE, 4);
    params(fx, "SPACE REVERB", P_SP_MOD, 4);
    params(fx, "CHORUS / DISTORTION", P_CH_MIX, 4);
    auto &dl = page("DELAY");
    params(dl, "DELAY", P_DL_TIME_L, 4);
    params(dl, "DELAY MOD", P_DL_DEPTH, 4);
    auto &arp = page("ARP / SPLIT");
    globals(arp, "ARPEGGIATOR", {{"arp_on", "ARP"}, {"arp_mode", "MODE"}, {"arp_rate", "RATE"}, {"arp_oct", "OCTAVES"},
                                 {"arp_gate", "GATE"}, {"arp_hold", "HOLD"}, {"swing", "SWING"}});
    globals(arp, "LOW SPLIT", {{"split", "SPLIT"}, {"split_point", "POINT"}, {"scale_l", "SCALE L"}, {"scale_r", "SCALE R"}});
    globals(arp, "SPLIT KEYS", {{"key", "KEY"}, {"follow", "FOLLOW"}, {"low_hold", "LOW HOLD"}, {"balance", "BALANCE"}});
    tabs.setTabBarDepth(30);
    tabs.setOutline(0);
    addAndMakeVisible(tabs);

    header.add(new P67Control(s, P(P_VINTAGE), "VINTAGE"));
    header.add(new P67Control(s, P(P_SPREAD), "SPREAD"));
    header.add(new P67Control(s, "volume", "VOLUME"));
    for (auto *h : header)
        addAndMakeVisible(h);

    for (int i = 0; i < proc.getNumPrograms(); i++)
        preset.addItem(juce::String(i + 1).paddedLeft('0', 2) + "  " + proc.getProgramName(i), i + 1);
    preset.onChange = [this] {
        int i = preset.getSelectedId() - 1;
        if (i >= 0 && i != proc.getCurrentProgram()) {
            proc.setCurrentProgram(i);
            shownProgram = i;
        }
    };
    prev.onClick = [this] {
        int n = proc.getNumPrograms();
        preset.setSelectedId((proc.getCurrentProgram() + n - 1) % n + 1);
    };
    next.onClick = [this] { preset.setSelectedId((proc.getCurrentProgram() + 1) % proc.getNumPrograms() + 1); };
    addAndMakeVisible(preset);
    addAndMakeVisible(prev);
    addAndMakeVisible(next);
    preset.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification);
    shownProgram = proc.getCurrentProgram();

    setSize(920, 600);
    startTimerHz(10);
}

Prophet67Editor::~Prophet67Editor() { setLookAndFeel(nullptr); }

void Prophet67Editor::paint(juce::Graphics &g)
{
    g.fillAll(P67Look::panel);
    auto top = getLocalBounds().removeFromTop(92).reduced(16, 0);
    g.setColour(P67Look::text);
    g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    g.drawText("PROPHET 67", top.removeFromLeft(230), juce::Justification::centredLeft);
    g.setColour(P67Look::red);
    g.fillRect(16, 90, getWidth() - 32, 2);
    g.setColour(P67Look::dim);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("7-VOICE POLYPHONIC SYNTHESIZER", 16, 64, 300, 14, juce::Justification::centredLeft);
}

void Prophet67Editor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop(92).reduced(16, 4);
    top.removeFromLeft(240);
    auto pr = top.removeFromLeft(300).withSizeKeepingCentre(300, 28);
    prev.setBounds(pr.removeFromLeft(28));
    next.setBounds(pr.removeFromRight(28));
    preset.setBounds(pr.reduced(4, 0));
    for (int i = header.size(); --i >= 0;)
        header[i]->setBounds(top.removeFromRight(84));
    tabs.setBounds(r.reduced(8, 6));
}

void Prophet67Editor::timerCallback()
{
    if (proc.getCurrentProgram() != shownProgram) {    /* the host changed the program */
        shownProgram = proc.getCurrentProgram();
        preset.setSelectedId(shownProgram + 1, juce::dontSendNotification);
    }
    for (auto *pg : pages)                             /* the delay's times as notes or ms */
        for (auto *g : pg->groups)
            for (auto *c : g->controls)
                c->refresh();
}
