/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The editor, plain and functional: the FM-1's pages as tabs, every parameter a knob, a menu or a switch. */
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class P67Look : public juce::LookAndFeel_V4
{
public:
    P67Look();
    void drawRotarySlider(juce::Graphics &, int x, int y, int w, int h, float pos, float a0, float a1,
                          juce::Slider &) override;
    static const juce::Colour panel, box, red, text, dim;
};

/* one parameter: a knob, a menu or a switch, with its name */
class P67Control : public juce::Component
{
public:
    P67Control(juce::AudioProcessorValueTreeState &s, const juce::String &id, const juce::String &label);
    void resized() override;
    void refresh();                      /* the knob's text again (a synced delay's notes) */

private:
    juce::Label label;
    std::unique_ptr<juce::Slider> knob;
    std::unique_ptr<juce::ComboBox> menu;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> menuA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleA;
};

/* a section: its red name over a row of controls */
class P67Group : public juce::Component
{
public:
    P67Group(const juce::String &title) : title(title) {}
    void add(std::unique_ptr<P67Control> c);
    void paint(juce::Graphics &) override;
    void resized() override;
    juce::OwnedArray<P67Control> controls;

private:
    juce::String title;
};

/* a tab: the sections stacked */
class P67Page : public juce::Component
{
public:
    P67Group &group(const juce::String &title);
    void resized() override;
    juce::OwnedArray<P67Group> groups;
};

class Prophet67Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Prophet67Editor(Prophet67Processor &);
    ~Prophet67Editor() override;
    void paint(juce::Graphics &) override;
    void resized() override;

private:
    void timerCallback() override;
    Prophet67Processor &proc;
    P67Look look;
    juce::ComboBox preset;
    juce::TextButton prev{"<"}, next{">"};
    juce::OwnedArray<P67Control> header;
    juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
    juce::OwnedArray<P67Page> pages;
    int shownProgram = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Prophet67Editor)
};
