/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The panel's controls, each tied to one host parameter: a knob (also on a list of choices, stepping), a column of
 * LED choices (click one), a button (a switch, one bit of a parameter, or a rule of its own), a button that steps
 * through a choice with LEDs above it (POLY MOD's FILTER: LP, HP), and a seven-segment display. Touching any of them
 * reports it, for the readout under the program display. */
#pragma once
#include "Look.h"

namespace p67
{
struct Touch {
    virtual ~Touch() = default;
    virtual void touched(juce::RangedAudioParameter &) = 0;
};

using APVTS = juce::AudioProcessorValueTreeState;

class Knob : public juce::Slider
{
public:
    Knob(APVTS &, const juce::String &id, const juce::String &legend, Touch *);
    void paint(juce::Graphics &) override;
    void mouseEnter(const juce::MouseEvent &) override;
    juce::RangedAudioParameter &param;
    juce::String legend;

private:
    Touch *touch;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

/* a parameter with its attachment, for the controls drawn by hand */
class ParamView : public juce::Component
{
public:
    ParamView(APVTS &, const juce::String &id, const juce::String &legend, Touch *);
    void mouseEnter(const juce::MouseEvent &) override;
    juce::RangedAudioParameter &param;
    int value = 0;

protected:
    void set(int v);                     /* a gesture: the value (in the parameter's own units) */
    int lo = 0, hi = 1;
    juce::String legend;
    Touch *touch;
    juce::ParameterAttachment att;
};

class LedList : public ParamView
{
public:
    LedList(APVTS &, const juce::String &id, Touch *, juce::StringArray names);
    void paint(juce::Graphics &) override;
    void mouseDown(const juce::MouseEvent &) override;
    float rowH = 13.0f;
    juce::StringArray names;
    juce::Rectangle<float> item(int i) const;   /* (public: the MPC skin's renderer, mpc/skin/, crops each row) */
};

class Button : public ParamView
{
public:
    using IsOn = std::function<bool(int)>;
    using Next = std::function<int(int)>;
    Button(APVTS &, const juce::String &id, const juce::String &legend, Touch *, IsOn, Next);
    Button(APVTS &, const juce::String &id, const juce::String &legend, Touch *, int bit = 1);   /* a bit, toggled */
    void paint(juce::Graphics &) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    juce::StringArray leds;              /* LEDs above (bit i: LED i), as POLY MOD's FILTER: LP, HP */
    bool key = false;                    /* drawn as a dark P6 key with its LED (GLIDE, UNISON) */
    bool grey = false;                   /* with key: a grey key without an LED, as the program buttons (the MPC's INIT) */
    IsOn isOn;                           /* (public: the MPC skin's renderer, mpc/skin/, follows the same rules) */
    Next next;

private:
    bool pressed = false;
};

/* a seven-segment display of anything */
class SegDisplay : public juce::Component
{
public:
    SegDisplay(int chars, std::function<juce::String()> text, bool fourteen = false)
        : n(chars), text(std::move(text)), fourteen(fourteen)
    {
    }
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat().reduced(3.0f);
        drawSegWindow(g, r);
        if (fourteen)
            drawSeg14Text(g, r, text(), n);
        else
            drawSegText(g, r, text(), n);
    }
    std::function<void()> onClick;
    void mouseDown(const juce::MouseEvent &) override
    {
        if (onClick)
            onClick();
    }

private:
    int n;
    std::function<juce::String()> text;
    bool fourteen;
};
} // namespace p67
