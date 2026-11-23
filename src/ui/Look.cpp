/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
#include "Look.h"
#include "BinaryData.h"

namespace p67
{
static juce::Typeface::Ptr face(Face f)
{
    static juce::Typeface::Ptr t[4] = {
        juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansSemiBold_ttf, BinaryData::FiraSansSemiBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansBold_ttf, BinaryData::FiraSansBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansExtraBold_ttf, BinaryData::FiraSansExtraBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor(BinaryData::AnonymousProBold_ttf, BinaryData::AnonymousProBold_ttfSize),
    };
    return t[(int)f];
}

juce::Font font(Face f, float height) { return juce::Font(juce::FontOptions(face(f)).withHeight(height)); }

Look::Look()
{
    using namespace juce;
    setColour(ResizableWindow::backgroundColourId, col::panelBottom);
    setColour(PopupMenu::backgroundColourId, Colour(0xff26272a));
    setColour(PopupMenu::textColourId, col::legend);
    setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xffb3241a));
    setColour(PopupMenu::highlightedTextColourId, Colours::white);
    setColour(PopupMenu::headerTextColourId, col::dim);
}

juce::Typeface::Ptr Look::getTypefaceForFont(const juce::Font &) { return face(Face::label); }

void Look::drawPopupMenuBackground(juce::Graphics &g, int w, int h)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(col::frame.withAlpha(0.6f));
    g.drawRect(0, 0, w, h, 1);
}

/* ------------------------------------------------------------------------------- the knob --- */
void Look::drawRotarySlider(juce::Graphics &g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider &s)
{
    using namespace juce;
    const float d = (float)jmin(w, h), R = d * 0.5f;
    const Point<float> c((float)x + (float)w * 0.5f, (float)y + (float)h * 0.5f);
    const float ang = a0 + pos * (a1 - a0);
    const float rb = R * 0.92f;

    /* its shadow on the panel */
    g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.65f), c.x, c.y + rb * 0.3f, Colours::transparentBlack, c.x,
                                     c.y + rb * 1.5f, true));
    g.fillEllipse(Rectangle<float>(rb * 2.7f, rb * 2.7f).withCentre(c.translated(0.0f, rb * 0.22f)));

    /* the knurled skirt */
    auto skirt = Rectangle<float>(rb * 2.0f, rb * 2.0f).withCentre(c);
    g.setGradientFill(ColourGradient(Colour(0xff2c2d30), c.x, c.y - rb, Colour(0xff050506), c.x, c.y + rb, false));
    g.fillEllipse(skirt);
    const int ribs = d > 60.0f ? 44 : 30;
    for (int i = 0; i < ribs; i++) {
        float a = MathConstants<float>::twoPi * (float)i / (float)ribs;
        float lit = 0.5f + 0.5f * std::cos(a + 0.8f);   /* lit from the top left */
        auto p0 = c.getPointOnCircumference(rb * 0.80f, a), p1 = c.getPointOnCircumference(rb * 0.985f, a);
        g.setColour(i & 1 ? Colours::white.withAlpha(0.03f + 0.07f * lit) : Colours::black.withAlpha(0.7f));
        g.drawLine({p0, p1}, jmax(1.0f, d * 0.025f));
    }
    g.setColour(Colours::black);
    g.drawEllipse(skirt, 1.0f);

    /* the cap, with a soft sheen, and the white line */
    const float rc = rb * 0.76f;
    auto cap = Rectangle<float>(rc * 2.0f, rc * 2.0f).withCentre(c);
    g.setGradientFill(ColourGradient(Colour(0xff2a2b2e), c.x - rc * 0.6f, c.y - rc, Colour(0xff0b0b0c), c.x + rc * 0.5f, c.y + rc, false));
    g.fillEllipse(cap);
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.10f), c.x - rc * 0.3f, c.y - rc * 0.6f, Colours::transparentWhite,
                                     c.x, c.y + rc * 0.2f, true));
    g.fillEllipse(cap.reduced(rc * 0.1f));
    g.setColour(Colours::black);
    g.drawEllipse(cap, 1.0f);
    g.setColour(Colours::white.withAlpha(0.08f));
    g.drawEllipse(cap.reduced(1.0f), 0.8f);

    auto p0 = c.getPointOnCircumference(rc * 0.30f, ang), p1 = c.getPointOnCircumference(rc * 0.97f, ang);
    g.setColour(Colour(0xfff6f4ee));
    g.drawLine({p0, p1}, jmax(1.6f, d * 0.045f));

    if (s.isMouseButtonDown()) {
        g.setColour(Colours::white.withAlpha(0.05f));
        g.fillEllipse(skirt);
    }
}

/* --------------------------------------------------------------------------- the pieces --- */
void drawLed(juce::Graphics &g, juce::Point<float> c, float r, float level)
{
    using namespace juce;
    level = jlimit(0.0f, 1.0f, level);
    if (level > 0.02f) {
        g.setGradientFill(ColourGradient(col::redLed.withAlpha(0.6f * level), c.x, c.y, Colours::transparentBlack, c.x + r * 3.0f, c.y, true));
        g.fillEllipse(Rectangle<float>(r * 6.0f, r * 6.0f).withCentre(c));
    }
    auto body = Rectangle<float>(r * 2.0f, r * 2.0f).withCentre(c);
    g.setColour(Colours::black.withAlpha(0.8f));
    g.fillEllipse(body.expanded(0.8f));
    g.setColour(col::ledOff.interpolatedWith(col::redLed, level));
    g.fillEllipse(body);
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.12f + 0.5f * level), c.x - r * 0.3f, c.y - r * 0.4f,
                                     Colours::transparentWhite, c.x, c.y + r * 0.6f, true));
    g.fillEllipse(body.reduced(r * 0.2f));
}

void drawButton(juce::Graphics &g, juce::Rectangle<float> r, bool on, bool down, bool over, bool light)
{
    using namespace juce;
    g.setColour(Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(r.translated(0.0f, 1.5f).expanded(1.0f), 2.5f);
    Colour top = light ? Colour(0xffd4d4cf) : col::buttonTop, bottom = light ? Colour(0xffa3a39e) : col::buttonBottom;
    if (down) {
        top = top.darker(0.25f);
        bottom = bottom.darker(0.25f);
    }
    g.setGradientFill(ColourGradient(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 2.5f);
    if (on) {                            /* the red light in the button */
        auto lamp = light ? r.reduced(r.getWidth() * 0.32f, r.getHeight() * 0.36f).withY(r.getY() + r.getHeight() * 0.18f) : r.reduced(3.0f, 3.0f);
        g.setGradientFill(ColourGradient(col::redLed.withAlpha(0.5f), lamp.getCentreX(), lamp.getCentreY(), Colours::transparentBlack,
                                         lamp.getCentreX() + lamp.getWidth(), lamp.getCentreY(), true));
        g.fillRect(lamp.expanded(lamp.getWidth() * 0.6f, lamp.getHeight() * 0.9f));
        g.setGradientFill(ColourGradient(Colour(0xffff6a4a), lamp.getCentreX(), lamp.getY(), Colour(0xffd81c10), lamp.getCentreX(),
                                         lamp.getBottom(), false));
        g.fillRoundedRectangle(lamp, 1.5f);
    }
    if (over && !down) {
        g.setColour(Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(r, 2.5f);
    }
    g.setColour(Colours::white.withAlpha(light ? 0.5f : 0.12f));
    g.drawLine(r.getX() + 2.0f, r.getY() + 0.8f, r.getRight() - 2.0f, r.getY() + 0.8f, 1.0f);
    g.setColour(Colours::black.withAlpha(0.9f));
    g.drawRoundedRectangle(r, 2.5f, 1.0f);
}

void drawLegend(juce::Graphics &g, const juce::String &t, juce::Rectangle<float> r, float size, juce::Justification j)
{
    g.setColour(col::legend);
    g.setFont(font(Face::label, size).withExtraKerningFactor(0.01f));
    g.drawFittedText(t, r.reduced(2.0f, 0.0f).toNearestInt(), j, 2, 0.7f);
}

/* ------------------------------------------------------------------------- seven segments --- */
static int segMask(juce::juce_wchar ch)
{
    enum { A = 1, B = 2, C = 4, D = 8, E = 16, F = 32, G = 64 };
    switch (juce::CharacterFunctions::toUpperCase(ch)) {
    case '0': case 'O': return A | B | C | D | E | F;
    case '1': return B | C;
    case '2': case 'Z': return A | B | G | E | D;
    case '3': return A | B | G | C | D;
    case '4': return F | G | B | C;
    case '5': case 'S': return A | F | G | C | D;
    case '6': return A | F | G | E | D | C;
    case '7': return A | B | C;
    case '8': return A | B | C | D | E | F | G;
    case '9': return A | B | C | D | F | G;
    case 'A': return A | B | C | E | F | G;
    case 'B': return C | D | E | F | G;
    case 'C': return A | D | E | F;
    case 'D': return B | C | D | E | G;
    case 'E': return A | D | E | F | G;
    case 'F': return A | E | F | G;
    case 'G': return A | C | D | E | F;
    case 'H': case 'K': case 'X': return B | C | E | F | G;
    case 'I': return E | F;
    case 'J': return B | C | D | E;
    case 'L': return D | E | F;
    case 'M': case 'N': return A | B | C | E | F;
    case 'P': return A | B | E | F | G;
    case 'Q': return A | B | C | F | G;
    case 'R': return E | G;
    case 'T': return D | E | F | G;
    case 'U': case 'V': case 'W': return B | C | D | E | F;
    case 'Y': return B | C | D | F | G;
    case '-': case '+': return G;
    case '_': return D;
    case '=': return D | G;
    case '\'': return F;
    default: return 0;
    }
}

static juce::Path segment(float x0, float y0, float x1, float y1, float t)   /* a bar with pointed ends */
{
    juce::Path p;
    float h = t * 0.5f;
    if (y0 == y1) {
        p.startNewSubPath(x0, y0);
        p.lineTo(x0 + h, y0 - h);
        p.lineTo(x1 - h, y0 - h);
        p.lineTo(x1, y0);
        p.lineTo(x1 - h, y0 + h);
        p.lineTo(x0 + h, y0 + h);
    } else {
        p.startNewSubPath(x0, y0);
        p.lineTo(x0 + h, y0 + h);
        p.lineTo(x0 + h, y1 - h);
        p.lineTo(x0, y1);
        p.lineTo(x0 - h, y1 - h);
        p.lineTo(x0 - h, y0 + h);
    }
    p.closeSubPath();
    return p;
}

void drawSegWindow(juce::Graphics &g, juce::Rectangle<float> r)
{
    using namespace juce;
    g.setColour(Colour(0xff0a0a0b));
    g.fillRoundedRectangle(r.expanded(3.0f), 3.0f);
    g.setColour(Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(r.expanded(3.0f), 3.0f, 1.0f);
    g.setGradientFill(ColourGradient(Colour(0xff230604), r.getX(), r.getY(), Colour(0xff120303), r.getX(), r.getBottom(), false));
    g.fillRect(r);
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.06f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(),
                                     r.getY() + r.getHeight() * 0.45f, false));
    g.fillRect(r.withHeight(r.getHeight() * 0.45f));
}

void drawSegText(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &text, int n)
{
    using namespace juce;
    /* the characters and their points (a '.' lights the one before) */
    std::vector<std::pair<int, bool>> cs;
    for (auto ch : text) {
        if (ch == '.' && !cs.empty() && !cs.back().second)
            cs.back().second = true;
        else
            cs.push_back({segMask(ch), false});
    }
    while ((int)cs.size() < n)
        cs.push_back({0, false});
    cs.resize((size_t)n);

    auto in = r.reduced(r.getHeight() * 0.16f, r.getHeight() * 0.15f);
    float cw = in.getWidth() / (float)n, w = cw * 0.72f, h = in.getHeight(), t = jmin(w * 0.2f, h * 0.12f), gap = t * 0.18f;
    for (int i = 0; i < n; i++) {
        float x = in.getX() + cw * (float)i + (cw - w) * 0.3f, y = in.getY();
        Path segs[7] = {
            segment(x + gap, y + t * 0.5f, x + w - gap, y + t * 0.5f, t),                       /* a */
            segment(x + w - t * 0.5f, y + t * 0.5f + gap, x + w - t * 0.5f, y + h * 0.5f - gap, t), /* b */
            segment(x + w - t * 0.5f, y + h * 0.5f + gap, x + w - t * 0.5f, y + h - t * 0.5f - gap, t), /* c */
            segment(x + gap, y + h - t * 0.5f, x + w - gap, y + h - t * 0.5f, t),               /* d */
            segment(x + t * 0.5f, y + h * 0.5f + gap, x + t * 0.5f, y + h - t * 0.5f - gap, t), /* e */
            segment(x + t * 0.5f, y + t * 0.5f + gap, x + t * 0.5f, y + h * 0.5f - gap, t),     /* f */
            segment(x + gap, y + h * 0.5f, x + w - gap, y + h * 0.5f, t),                       /* g */
        };
        auto skew = AffineTransform::translation(-x, -(y + h)).sheared(-0.09f, 0.0f).translated(x, y + h);
        for (int s = 0; s < 7; s++) {
            segs[s].applyTransform(skew);
            bool lit = (cs[(size_t)i].first >> s) & 1;
            if (lit) {
                g.setColour(col::segOn.withAlpha(0.28f));
                g.strokePath(segs[s], PathStrokeType(t * 0.9f, PathStrokeType::mitered, PathStrokeType::rounded));
            }
            g.setColour(lit ? col::segOn : col::segOff);
            g.fillPath(segs[s]);
        }
        auto dp = Rectangle<float>(t, t).withCentre({x + w + t * 0.55f, y + h - t * 0.5f});
        g.setColour(cs[(size_t)i].second ? col::segOn : col::segOff);
        g.fillEllipse(dp);
    }
}
/* ------------------------------------------------------------------------ fourteen segments --- */
namespace {
enum : unsigned {
    SA = 1u << 0, SB = 1u << 1, SC = 1u << 2, SD = 1u << 3, SE = 1u << 4, SF = 1u << 5, SG1 = 1u << 6, SG2 = 1u << 7,
    SH = 1u << 8, SI = 1u << 9, SJ = 1u << 10, SK = 1u << 11, SL = 1u << 12, SM = 1u << 13,
    SVL = 1u << 14, SVR = 1u << 15,      /* (two strokes of my own for V, drawn only when lit) */
};
unsigned seg14(juce::juce_wchar ch)
{
    switch (juce::CharacterFunctions::toUpperCase(ch)) {
    case '0': case 'O': return SA | SB | SC | SD | SE | SF;
    case '1': return SB | SC | SJ;
    case '2': return SA | SB | SG1 | SG2 | SE | SD;
    case '3': return SA | SB | SG2 | SC | SD;
    case '4': return SF | SG1 | SG2 | SB | SC;
    case '5': return SA | SF | SG1 | SG2 | SC | SD;
    case '6': return SA | SF | SE | SD | SC | SG1 | SG2;
    case '7': return SA | SB | SC;
    case '8': return SA | SB | SC | SD | SE | SF | SG1 | SG2;
    case '9': return SA | SB | SC | SD | SF | SG1 | SG2;
    case 'A': return SA | SB | SC | SE | SF | SG1 | SG2;
    case 'B': return SA | SB | SC | SD | SI | SL | SG2;
    case 'C': return SA | SD | SE | SF;
    case 'D': return SA | SB | SC | SD | SI | SL;
    case 'E': return SA | SD | SE | SF | SG1;
    case 'F': return SA | SE | SF | SG1;
    case 'G': return SA | SC | SD | SE | SF | SG2;
    case 'H': return SB | SC | SE | SF | SG1 | SG2;
    case 'I': return SA | SD | SI | SL;
    case 'J': return SB | SC | SD | SE;
    case 'K': return SE | SF | SG1 | SJ | SM;
    case 'L': return SD | SE | SF;
    case 'M': return SB | SC | SE | SF | SH | SJ;
    case 'N': return SB | SC | SE | SF | SH | SM;
    case 'P': return SA | SB | SE | SF | SG1 | SG2;
    case 'Q': return SA | SB | SC | SD | SE | SF | SM;
    case 'R': return SA | SB | SE | SF | SG1 | SG2 | SM;
    case 'S': return SA | SC | SD | SF | SG1 | SG2;
    case 'T': return SA | SI | SL;
    case 'U': return SB | SC | SD | SE | SF;
    case 'V': return SVL | SVR;
    case 'W': return SB | SC | SE | SF | SK | SM;
    case 'X': return SH | SJ | SK | SM;
    case 'Y': return SH | SJ | SL;
    case 'Z': return SA | SD | SJ | SK;
    case '+': return SG1 | SG2 | SI | SL;
    case '-': return SG1 | SG2;
    case '/': return SJ | SK;
    case '_': return SD;
    default: return 0;
    }
}
} // namespace

void drawSeg14Text(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &text, int n)
{
    using namespace juce;
    std::vector<std::pair<unsigned, bool>> cs;
    for (auto ch : text) {
        if (ch == '.' && !cs.empty() && !cs.back().second)
            cs.back().second = true;
        else
            cs.push_back({seg14(ch), false});
    }
    while ((int)cs.size() < n)
        cs.push_back({0u, false});
    cs.resize((size_t)n);

    auto in = r.reduced(r.getHeight() * 0.18f, r.getHeight() * 0.16f);
    float cw = in.getWidth() / (float)n, w = cw * 0.74f, h = in.getHeight(), t = jmin(w * 0.15f, h * 0.09f), gap = t * 0.25f;
    for (int i = 0; i < n; i++) {
        float x = in.getX() + cw * (float)i + (cw - w) * 0.3f, y = in.getY(), cx = x + w * 0.5f, cy = y + h * 0.5f;
        auto skew = AffineTransform::translation(-x, -(y + h)).sheared(-0.09f, 0.0f).translated(x, y + h);
        auto line = [&](float x0, float y0, float x1, float y1) {
            Path p;
            p.startNewSubPath(x0, y0);
            p.lineTo(x1, y1);
            Path s;
            PathStrokeType(t * 0.78f, PathStrokeType::mitered, PathStrokeType::butt).createStrokedPath(s, p);
            return s;
        };
        float in2 = t * 1.1f;
        Path segs[16] = {
            segment(x + gap, y + t * 0.5f, x + w - gap, y + t * 0.5f, t),
            segment(x + w - t * 0.5f, y + t * 0.5f + gap, x + w - t * 0.5f, cy - gap, t),
            segment(x + w - t * 0.5f, cy + gap, x + w - t * 0.5f, y + h - t * 0.5f - gap, t),
            segment(x + gap, y + h - t * 0.5f, x + w - gap, y + h - t * 0.5f, t),
            segment(x + t * 0.5f, cy + gap, x + t * 0.5f, y + h - t * 0.5f - gap, t),
            segment(x + t * 0.5f, y + t * 0.5f + gap, x + t * 0.5f, cy - gap, t),
            segment(x + gap, cy, cx - gap, cy, t),
            segment(cx + gap, cy, x + w - gap, cy, t),
            line(x + in2, y + in2, cx - t * 0.6f, cy - t * 0.7f),
            segment(cx, y + t * 0.5f + gap * 2.0f, cx, cy - gap * 2.0f, t * 0.85f),
            line(x + w - in2, y + in2, cx + t * 0.6f, cy - t * 0.7f),
            line(x + in2, y + h - in2, cx - t * 0.6f, cy + t * 0.7f),
            segment(cx, cy + gap * 2.0f, cx, y + h - t * 0.5f - gap * 2.0f, t * 0.85f),
            line(x + w - in2, y + h - in2, cx + t * 0.6f, cy + t * 0.7f),
            line(x + t * 0.5f, y + t, cx, y + h - t * 0.6f),
            line(x + w - t * 0.5f, y + t, cx, y + h - t * 0.6f),
        };
        unsigned m = cs[(size_t)i].first;
        for (int s = 0; s < 16; s++) {
            bool lit = (m >> s) & 1u;
            if (s >= 14 && !lit)
                continue;
            segs[s].applyTransform(skew);
            if (lit) {
                g.setColour(col::segOn.withAlpha(0.25f));
                g.strokePath(segs[s], PathStrokeType(t * 0.8f, PathStrokeType::mitered, PathStrokeType::rounded));
            }
            g.setColour(lit ? col::segOn : col::segOff);
            g.fillPath(segs[s]);
        }
        auto dp = Rectangle<float>(t, t).withCentre({x + w + t * 0.7f, y + h - t * 0.5f});
        g.setColour(cs[(size_t)i].second ? col::segOn : col::segOff);
        g.fillEllipse(dp);
    }
}
} // namespace p67
