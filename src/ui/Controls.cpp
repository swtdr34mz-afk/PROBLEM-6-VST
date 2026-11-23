/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
#include "Controls.h"

namespace p67
{
static const float LEGEND_H = 14.0f;

/* ---------------------------------------------------------------------------------- knob --- */
Knob::Knob(APVTS &s, const juce::String &id, const juce::String &legend, Touch *t)
    : param(*s.getParameter(id)), legend(legend), touch(t)
{
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    bool choice = dynamic_cast<juce::AudioParameterChoice *>(&param) != nullptr;
    setMouseDragSensitivity(choice ? 120 : 240);
    setVelocityModeParameters(0.35, 1, 0.0, true, juce::ModifierKeys::shiftModifier);   /* SHIFT: fine */
    att = std::make_unique<APVTS::SliderAttachment>(s, id, *this);
    setDoubleClickReturnValue(true, param.convertFrom0to1(param.getDefaultValue()));
    onValueChange = [this] {
        if (touch && (isMouseButtonDown() || isMouseOver()))
            touch->touched(param);
    };
}

void Knob::mouseEnter(const juce::MouseEvent &e)
{
    juce::Slider::mouseEnter(e);
    if (touch)
        touch->touched(param);
}

void Knob::paint(juce::Graphics &g)
{
    auto r = getLocalBounds().toFloat();
    auto lg = legend.isEmpty() ? juce::Rectangle<float>() : r.removeFromBottom(LEGEND_H * (legend.contains("\n") ? 2.0f : 1.0f));
    float d = juce::jmin(r.getWidth(), r.getHeight());
    auto rp = getRotaryParameters();
    if ((bool)getProperties()["shapes"]) {   /* the waves around it: triangle at the left, saw at the top, pulse at the right */
        using namespace juce;
        auto c = r.getCentre();
        float R = d * 0.5f, w = 7.5f, h = 4.5f;
        auto wave = [&](Point<float> at, int kind) {
            Path p;
            float x0 = at.x - w, y0 = at.y;   /* (a wave 2w wide, centred on at) */
            if (kind == 0) {
                p.startNewSubPath(x0, y0 + h * 0.5f);
                for (int i = 0; i < 3; i++)
                    p.lineTo(x0 + w * (0.33f + 0.66f * (float)i), y0 + (i % 2 ? h : -h));
                p.lineTo(x0 + w * 2.0f, y0);
            } else if (kind == 1) {
                p.startNewSubPath(x0, y0 + h);
                p.lineTo(x0 + w * 0.8f, y0 - h);
                p.lineTo(x0 + w * 0.8f, y0 + h);
                p.lineTo(x0 + w * 1.6f, y0 - h);
                p.lineTo(x0 + w * 1.6f, y0 + h);
            } else {
                p.startNewSubPath(x0, y0 + h);
                p.lineTo(x0 + w * 0.5f, y0 + h);
                p.lineTo(x0 + w * 0.5f, y0 - h);
                p.lineTo(x0 + w * 1.1f, y0 - h);
                p.lineTo(x0 + w * 1.1f, y0 + h);
                p.lineTo(x0 + w * 1.6f, y0 + h);
                p.lineTo(x0 + w * 1.6f, y0 - h);
                p.lineTo(x0 + w * 2.0f, y0 - h);
            }
            g.setColour(col::legend);
            g.strokePath(p, PathStrokeType(1.2f, PathStrokeType::mitered, PathStrokeType::square));
        };
        wave(c.getPointOnCircumference(R * 0.92f, 4.40f), 0);
        wave(c.getPointOnCircumference(R * 0.86f, 0.0f), 1);
        wave(c.getPointOnCircumference(R * 0.92f, 1.88f), 2);
        d *= 0.74f;
    }
    auto k = r.withSizeKeepingCentre(d, d).toNearestInt();
    getLookAndFeel().drawRotarySlider(g, k.getX(), k.getY(), k.getWidth(), k.getHeight(),
                                      (float)valueToProportionOfLength(getValue()), rp.startAngleRadians,
                                      rp.endAngleRadians, *this);
    if (!lg.isEmpty())
        drawLegend(g, legend, lg, 10.0f);
}

/* ---------------------------------------------------------------------------- param view --- */
ParamView::ParamView(APVTS &s, const juce::String &id, const juce::String &legend, Touch *t)
    : param(*s.getParameter(id)), legend(legend), touch(t), att(param, [this](float v) {
          value = (int)std::lround(v);
          repaint();
      })
{
    auto r = param.getNormalisableRange();
    lo = (int)r.start;
    hi = (int)r.end;
    att.sendInitialUpdate();
    setRepaintsOnMouseActivity(true);
}

void ParamView::mouseEnter(const juce::MouseEvent &)
{
    if (touch)
        touch->touched(param);
}

void ParamView::set(int v)
{
    att.setValueAsCompleteGesture((float)juce::jlimit(lo, hi, v));
    if (touch)
        touch->touched(param);
}

/* ------------------------------------------------------------------------------ LED list --- */
LedList::LedList(APVTS &s, const juce::String &id, Touch *t, juce::StringArray n) : ParamView(s, id, {}, t), names(std::move(n))
{
}

juce::Rectangle<float> LedList::item(int i) const
{
    auto r = getLocalBounds().toFloat();
    float top = r.getY() + (r.getHeight() - rowH * (float)names.size()) * 0.5f;
    return {r.getX(), top + rowH * (float)i, r.getWidth(), rowH};
}

void LedList::paint(juce::Graphics &g)
{
    for (int i = 0; i < names.size(); i++) {
        auto r = item(i);
        drawLed(g, {r.getX() + 4.5f, r.getCentreY()}, 3.0f, value - lo == i ? 1.0f : 0.0f);
        g.setColour(col::legend);
        g.setFont(font(Face::label, juce::jmin(10.5f, rowH * 0.82f)));
        g.drawText(names[i], r.withTrimmedLeft(12.0f), juce::Justification::centredLeft, false);
    }
}

void LedList::mouseDown(const juce::MouseEvent &e)
{
    for (int i = 0; i < names.size(); i++)
        if (item(i).contains(e.position)) {
            set(lo + i);
            return;
        }
}

/* -------------------------------------------------------------------------------- button --- */
Button::Button(APVTS &s, const juce::String &id, const juce::String &legend, Touch *t, IsOn on, Next nx)
    : ParamView(s, id, legend, t), isOn(std::move(on)), next(std::move(nx))
{
}

Button::Button(APVTS &s, const juce::String &id, const juce::String &legend, Touch *t, int bit)
    : Button(s, id, legend, t, [bit](int v) { return (v & bit) != 0; }, [bit](int v) { return v ^ bit; })
{
}

void Button::paint(juce::Graphics &g)
{
    auto r = getLocalBounds().toFloat();
    auto lg = legend.isEmpty() ? juce::Rectangle<float>() : r.removeFromBottom(LEGEND_H * (legend.contains("\n") ? 2.0f : 1.0f));
    if (leds.size() > 0) {               /* the LEDs above, each with its name */
        auto top = r.removeFromTop(16.0f);
        float w = top.getWidth() / (float)leds.size();
        for (int i = 0; i < leds.size(); i++) {
            auto c = top.withX(top.getX() + w * (float)i).withWidth(w);
            g.setColour(col::legend);
            g.setFont(font(Face::label, 8.5f));
            g.drawText(leds[i], c.withTrimmedBottom(7.0f), juce::Justification::centredBottom, false);
            drawLed(g, {c.getCentreX(), c.getBottom() - 3.0f}, 2.6f, (value >> i) & 1 ? 1.0f : 0.0f);
        }
    }
    if (key) {
        drawKeycap(g, r.withSizeKeepingCentre(juce::jmin(r.getWidth() - 10.0f, 40.0f), r.getHeight() - 6.0f), isOn(value), true, pressed,
                   isMouseOver(), true);
        if (!lg.isEmpty())
            drawLegend(g, legend, lg);
        return;
    }
    auto b = r.withSizeKeepingCentre(juce::jmin(r.getWidth() - 6.0f, 36.0f), juce::jmin(r.getHeight() - 4.0f, 19.0f));
    drawButton(g, b, leds.isEmpty() && isOn(value), pressed, isMouseOver());
    if (!lg.isEmpty())
        drawLegend(g, legend, lg);
}

void Button::mouseDown(const juce::MouseEvent &)
{
    pressed = true;
    set(next(value));
}

void Button::mouseUp(const juce::MouseEvent &)
{
    pressed = false;
    repaint();
}
} // namespace p67
