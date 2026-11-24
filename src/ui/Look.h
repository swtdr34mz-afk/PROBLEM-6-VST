/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The look of a Prophet-6: a charcoal panel with white legends and white section frames (the name set in the frame's
 * top line), black knurled knobs with a white line, small dark buttons that light red, round red LEDs, red
 * seven-segment displays, walnut end cheeks, light grey program buttons and an ivory keyboard. The fonts are embedded
 * (Fira Sans, Anonymous Pro: SIL OFL, fonts/). */
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace p67
{
namespace col
{
const juce::Colour panelTop{0xff252629}, panelBottom{0xff1c1d1f}, frame{0xffd8d7d2}, legend{0xfff1f0ea},
    dim{0xffa9a9a4}, redLed{0xffff2d1c}, ledOff{0xff4a1612}, segOn{0xffff2a18}, segOff{0xff3a0d0a},
    buttonTop{0xff2a2b2e}, buttonBottom{0xff121315}, progButton{0xffc3c3be};
}

enum class Face { label, title, logo, mono };
juce::Font font(Face f, float height);

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider(juce::Graphics &, int x, int y, int w, int h, float pos, float a0, float a1,
                          juce::Slider &) override;
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font &) override;
    juce::Font getPopupMenuFont() override { return font(Face::label, 15.0f); }
    void drawPopupMenuBackground(juce::Graphics &, int w, int h) override;
};

void drawLed(juce::Graphics &, juce::Point<float> c, float r, float level);
/* a small Prophet button: dark, its face lit red when on (light: the grey program buttons) */
void drawButton(juce::Graphics &, juce::Rectangle<float> r, bool on, bool down, bool over, bool light = false);
/* a red seven-segment display: its window, then n characters of text (what seven segments can show) */
void drawKeycap(juce::Graphics &, juce::Rectangle<float> r, bool led, bool hasLed, bool down, bool over, bool dark = false);
void drawSegWindow(juce::Graphics &, juce::Rectangle<float> r);
void drawSegText(juce::Graphics &, juce::Rectangle<float> r, const juce::String &text, int n);
/* the same with fourteen segments (letters that read) */
void drawSeg14Text(juce::Graphics &, juce::Rectangle<float> r, const juce::String &text, int n);
/* a section's white frame with its name set in the top line */
void drawSection(juce::Graphics &, juce::Rectangle<int> r, const juce::String &title);
/* the panel's charcoal face, matte (dots specks over the area, the same for the same seed) */
void drawPanelFace(juce::Graphics &, juce::Rectangle<float> r, int dots, int seed = 67);
float textWidth(const juce::Font &, const juce::String &);
/* the maker's tag (res/swtdr34mz-tag.png), in the legends' white, as large as fits r, centred */
void drawTag(juce::Graphics &, juce::Rectangle<float> r);
void drawLegend(juce::Graphics &, const juce::String &, juce::Rectangle<float>, float size = 10.5f,
                juce::Justification j = juce::Justification::centred);
} // namespace p67
