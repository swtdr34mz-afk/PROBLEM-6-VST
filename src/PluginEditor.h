/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The editor: a Prophet-6's front panel, drawn at 1560 x 640 and scaled to the window (drag the corner). Four rows
 * of sections as on the P6 (POLY MOD, ARPEGGIATOR, AFTERTOUCH; the oscillators, VINTAGE, MIXER, HIGH-PASS
 * FILTER, FILTER ENVELOPE; the two LFOs, LOW-PASS FILTER, MISC PARAMETERS; DISTORT, EFFECTS, AMPLIFIER ENVELOPE,
 * LOW SPLIT), the program strip (HOLD, GLIDE AMT, UNISON, the program display, TENS SELECT, 0-9, INIT, WRITE, the name); no keyboard
 * or wheels (played from the DAW or a MIDI keyboard). */
#pragma once
#include "PluginProcessor.h"
#include "ui/Controls.h"

class Prophet67Panel : public juce::Component, public p67::Touch, private juce::Timer
{
public:
    static constexpr int W = 1560, H = 640;
    explicit Prophet67Panel(Prophet67Processor &);
    ~Prophet67Panel() override;
    void paint(juce::Graphics &) override;
    void touched(juce::RangedAudioParameter &) override;

    Prophet67Processor &proc;
    int fx = 0;                          /* the effect EFFECTS shows (SELECT) */
    int tens = 0;                        /* TENS SELECT */
    void selectFx(int f);
    void presetMenu();
    void loadP6();                     /* LOAD P6 SYSEX (the program menu) */
    std::unique_ptr<juce::FileChooser> chooser;
    static constexpr int LOAD_P6 = 100000;
    
    juce::String fxValueText() const;

private:
    struct Section {
        juce::Rectangle<int> r;
        juce::String title;
    };
    void timerCallback() override;
    void build();
    void paintBackground(juce::Graphics &);
    void paintSection(juce::Graphics &, const Section &);

    p67::Look look;
    juce::OwnedArray<juce::Component> controls;
    std::vector<Section> sections;
    std::vector<std::pair<juce::Component *, int>> fxControls;   /* EFFECTS: a knob and the effect it is for */
    juce::Component *fxDisplay = nullptr, *nameDisplay = nullptr;
    juce::Array<juce::Component *> progButtons;

    juce::Image background;
    float backgroundScale = 0.0f;

    juce::RangedAudioParameter *shown = nullptr, *fxShown = nullptr;
    juce::uint32 shownAt = 0;
    int shownProgram = -1;
    bool shownEdited = false;
    int tick = 0;
    juce::Component *pointDisplay = nullptr;   /* LOW SPLIT POINT's note, above its knob */
    int pointShown = -1;
};

class Prophet67Editor : public juce::AudioProcessorEditor
{
public:
    explicit Prophet67Editor(Prophet67Processor &);
    void resized() override;
    bool sized = false;                      /* (resizes during the constructor keep nothing) */

private:
    Prophet67Processor &proc;
    juce::SharedResourcePointer<p67::UiResources> resources; // (before the panel: freed after it)
    Prophet67Panel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Prophet67Editor)
};
