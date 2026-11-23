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
/* seen from above: white scale ticks around it, the black ribbed body, a chrome ring, a dark spun-metal cap and its
 * white line */
void Look::drawRotarySlider(juce::Graphics &g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider &s)
{
    using namespace juce;
    const float d = (float)jmin(w, h), R = d * 0.5f;
    const Point<float> c((float)x + (float)w * 0.5f, (float)y + (float)h * 0.5f);
    const float ang = a0 + pos * (a1 - a0);
    const float rb = R * 0.66f;

    /* the scale */
    g.setColour(col::legend);
    for (int i = 0; i <= 10; i++) {
        float a = a0 + (a1 - a0) * (float)i / 10.0f;
        g.drawLine(Line<float>(c.getPointOnCircumference(R * 0.77f, a), c.getPointOnCircumference(R * 0.89f, a)), jmax(1.0f, d * 0.022f));
    }
    if ((bool)s.getProperties()["bipolar"]) {
        g.setFont(font(Face::title, jmax(8.0f, d * 0.17f)));
        auto pm = c.getPointOnCircumference(R * 0.86f, a0 - 0.36f), pp = c.getPointOnCircumference(R * 0.86f, a1 + 0.36f);
        g.drawText("-", Rectangle<float>(10.0f, 10.0f).withCentre(pm), Justification::centred, false);
        g.drawText("+", Rectangle<float>(10.0f, 10.0f).withCentre(pp), Justification::centred, false);
    }

    /* its shadow, the ribbed body */
    g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.7f), c.x, c.y + rb * 0.3f, Colours::transparentBlack, c.x, c.y + rb * 1.45f, true));
    g.fillEllipse(Rectangle<float>(rb * 2.6f, rb * 2.6f).withCentre(c.translated(rb * 0.08f, rb * 0.2f)));
    auto body = Rectangle<float>(rb * 2.0f, rb * 2.0f).withCentre(c);
    g.setColour(Colour(0xff070708));
    g.fillEllipse(body);
    const int ribs = 40;
    for (int i = 0; i < ribs; i++) {
        float a = MathConstants<float>::twoPi * (float)i / (float)ribs;
        float lit = 0.5f + 0.5f * std::cos(a + 0.9f);
        g.setColour(Colours::white.withAlpha(0.04f + 0.10f * lit));
        g.drawLine(Line<float>(c.getPointOnCircumference(rb * 0.90f, a), c.getPointOnCircumference(rb * 0.995f, a)), jmax(0.8f, d * 0.016f));
    }

    /* the chrome ring: light along a band, as turned metal catches it */
    const float ro = rb * 0.89f, ri = rb * 0.70f;
    const int slices = 72;
    for (int i = 0; i < slices; i++) {
        float a = MathConstants<float>::twoPi * (float)i / (float)slices, a2 = a + MathConstants<float>::twoPi / (float)slices + 0.01f;
        float k = 0.5f + 0.5f * std::cos(2.0f * (a + 0.75f));          /* two bright sides */
        float v = 0.38f + 0.55f * std::pow(k, 1.6f);
        Path p;
        p.addCentredArc(c.x, c.y, ro, ro, 0.0f, a, a2, true);
        p.lineTo(c.getPointOnCircumference(ri, a2));
        p.addCentredArc(c.x, c.y, ri, ri, 0.0f, a2, a, false);
        p.closeSubPath();
        g.setColour(Colour::fromFloatRGBA(v, v, v * 1.02f, 1.0f));
        g.fillPath(p);
    }
    g.setColour(Colours::black.withAlpha(0.6f));
    g.drawEllipse(Rectangle<float>(ro * 2.0f, ro * 2.0f).withCentre(c), 0.8f);

    /* the cap: dark spun metal */
    for (int i = 0; i < slices; i++) {
        float a = MathConstants<float>::twoPi * (float)i / (float)slices, a2 = a + MathConstants<float>::twoPi / (float)slices + 0.01f;
        float k = 0.5f + 0.5f * std::cos(2.0f * (a + 0.75f));
        float v = 0.10f + 0.16f * std::pow(k, 2.0f);
        Path p;
        p.addPieSegment(Rectangle<float>(ri * 2.0f, ri * 2.0f).withCentre(c), a, a2, 0.0f);
        g.setColour(Colour::fromFloatRGBA(v, v, v * 1.03f, 1.0f));
        g.fillPath(p);
    }
    g.setColour(Colours::black.withAlpha(0.8f));
    g.drawEllipse(Rectangle<float>(ri * 2.0f, ri * 2.0f).withCentre(c), 0.8f);

    auto p0 = c.getPointOnCircumference(ri * 0.18f, ang), p1 = c.getPointOnCircumference(ri * 0.96f, ang);
    g.setColour(Colour(0xfff4f2ec));
    g.drawLine({p0, p1}, jmax(1.4f, d * 0.035f));
}

/* --------------------------------------------------------------------------- the pieces --- */
void drawLed(juce::Graphics &g, juce::Point<float> c, float r, float level)   /* a small square red LED */
{
    using namespace juce;
    level = jlimit(0.0f, 1.0f, level);
    auto body = Rectangle<float>(r * 2.6f, r * 1.9f).withCentre(c);
    if (level > 0.02f) {
        g.setGradientFill(ColourGradient(Colour(0xffff3a1a).withAlpha(0.6f * level), c.x, c.y, Colours::transparentBlack, c.x + r * 3.4f, c.y, true));
        g.fillEllipse(Rectangle<float>(r * 7.0f, r * 6.0f).withCentre(c));
    }
    g.setColour(Colours::black.withAlpha(0.85f));
    g.fillRoundedRectangle(body.expanded(1.0f), 1.5f);
    g.setGradientFill(ColourGradient(Colour(0xff5a1510).interpolatedWith(Colour(0xffffb070), level), c.x, c.y,
                                     Colour(0xff3a0c09).interpolatedWith(Colour(0xffff2a10), level), body.getRight(), body.getBottom(), true));
    g.fillRoundedRectangle(body, 1.2f);
    g.setColour(Colours::white.withAlpha(0.10f + 0.2f * level));
    g.fillRect(body.reduced(body.getWidth() * 0.2f, 0.0f).withHeight(body.getHeight() * 0.3f).translated(0.0f, 0.6f));
}

void drawButton(juce::Graphics &g, juce::Rectangle<float> r, bool on, bool down, bool over, bool)
{
    using namespace juce;
    if (on) {                            /* its light spills onto the panel */
        g.setGradientFill(ColourGradient(Colour(0xffff3010).withAlpha(0.45f), r.getCentreX(), r.getCentreY(), Colours::transparentBlack,
                                         r.getRight() + r.getWidth() * 0.35f, r.getCentreY(), true));
        g.fillEllipse(r.expanded(r.getWidth() * 0.45f, r.getHeight() * 1.0f));
    }
    g.setColour(Colours::black.withAlpha(0.7f));
    g.fillRoundedRectangle(r.translated(0.8f, 2.0f).expanded(0.5f), 3.5f);
    g.setColour(Colour(0xff050505));     /* the black rim */
    g.fillRoundedRectangle(r, 3.5f);
    auto face = r.reduced(1.6f).translated(0.0f, down ? 0.8f : 0.0f);
    if (on) {
        ColourGradient lit(Colour(0xffffb45a), face.getCentreX(), face.getCentreY(), Colour(0xffe2230c), face.getRight(), face.getBottom(), true);
        lit.addColour(0.45, Colour(0xffff6a26));
        g.setGradientFill(lit);
    } else
        g.setGradientFill(ColourGradient(Colour(0xff2b2c2f), face.getX(), face.getY(), Colour(0xff111113), face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle(face, 2.5f);
    g.setColour(Colours::white.withAlpha(on ? 0.25f : 0.10f));
    g.drawLine(face.getX() + 2.5f, face.getY() + 0.8f, face.getRight() - 2.5f, face.getY() + 0.8f, 1.0f);
    if (over && !down) {
        g.setColour(Colours::white.withAlpha(0.05f));
        g.fillRoundedRectangle(face, 2.5f);
    }
}

/* a Prophet-6 key (the program keys, BANK and TENS SELECT; dark: GLIDE, UNISON): upright, sunk in a black gap, its
 * top face above, its sloping front face below, a round red LED in the top face */
void drawKeycap(juce::Graphics &g, juce::Rectangle<float> r, bool led, bool hasLed, bool down, bool over, bool dark)
{
    using namespace juce;
    g.setColour(Colour(0xff050505));     /* the gap around it */
    g.fillRoundedRectangle(r.expanded(2.5f), 3.5f);
    auto key = r.translated(0.0f, down ? 1.2f : 0.0f);
    Colour topA = dark ? Colour(0xff2e2f32) : Colour(0xffcfcfcb), topB = dark ? Colour(0xff222326) : Colour(0xffbdbdb9);
    Colour frontA = dark ? Colour(0xff1a1b1d) : Colour(0xffb0b0ac), frontB = dark ? Colour(0xff101112) : Colour(0xff9d9d99);
    float split = key.getY() + key.getHeight() * (down ? 0.70f : 0.64f);
    auto front = key.withTop(split);
    auto top = key.withBottom(split);
    g.setGradientFill(ColourGradient(frontA, front.getX(), front.getY(), frontB, front.getX(), front.getBottom(), false));
    g.fillRoundedRectangle(front, 2.0f);
    g.fillRect(front.withHeight(3.0f));
    g.setGradientFill(ColourGradient(topA, top.getX(), top.getY(), topB, top.getX(), top.getBottom(), false));
    g.fillRoundedRectangle(top, 2.0f);
    g.fillRect(top.withTop(top.getBottom() - 3.0f));
    g.setColour(Colours::white.withAlpha(dark ? 0.10f : 0.55f));          /* the edge between the faces catches the light */
    g.drawLine(key.getX() + 1.5f, split, key.getRight() - 1.5f, split, 1.0f);
    g.setColour(Colours::black.withAlpha(dark ? 0.6f : 0.18f));
    g.drawLine(key.getX() + 1.5f, split + 1.0f, key.getRight() - 1.5f, split + 1.0f, 1.0f);
    g.setColour(Colours::white.withAlpha(dark ? 0.12f : 0.6f));
    g.drawLine(key.getX() + 2.0f, key.getY() + 0.8f, key.getRight() - 2.0f, key.getY() + 0.8f, 1.0f);
    g.setColour(Colours::black.withAlpha(0.5f));
    g.drawRoundedRectangle(key, 2.0f, 1.0f);
    if (over && !down) {
        g.setColour(Colours::white.withAlpha(0.07f));
        g.fillRoundedRectangle(key, 2.0f);
    }
    if (hasLed) {
        auto c = Point<float>(top.getCentreX(), top.getY() + top.getHeight() * 0.42f);
        float lr = jmin(key.getWidth() * 0.10f, 4.2f);
        if (led) {
            g.setGradientFill(ColourGradient(Colour(0xffff2a10).withAlpha(0.65f), c.x, c.y, Colours::transparentBlack, c.x + lr * 3.8f, c.y, true));
            g.fillEllipse(Rectangle<float>(lr * 7.6f, lr * 7.6f).withCentre(c));
        }
        g.setColour(dark ? Colour(0xff0a0a0a) : Colour(0xff4a4a48));
        g.fillEllipse(Rectangle<float>(lr * 2.7f, lr * 2.7f).withCentre(c));
        g.setGradientFill(ColourGradient(led ? Colour(0xffffb080) : Colour(0xff8a1c14), c.x - lr * 0.3f, c.y - lr * 0.35f,
                                         led ? Colour(0xffe8200c) : Colour(0xff40100c), c.x + lr, c.y + lr, true));
        g.fillEllipse(Rectangle<float>(lr * 2.0f, lr * 2.0f).withCentre(c));
    }
}

void drawLegend(juce::Graphics &g, const juce::String &t, juce::Rectangle<float> r, float size, juce::Justification j)
{
    g.setColour(col::legend);
    g.setFont(font(Face::title, size).withExtraKerningFactor(0.01f));
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
            g.setColour(lit ? col::segOn : col::segOff.withAlpha(0.55f));
            g.fillPath(segs[s]);
        }
        auto dp = Rectangle<float>(t, t).withCentre({x + w + t * 0.7f, y + h - t * 0.5f});
        g.setColour(cs[(size_t)i].second ? col::segOn : col::segOff);
        g.fillEllipse(dp);
    }
}
} // namespace p67
