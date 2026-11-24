/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The MPC skin's pictures, from the VST3's own panel: Prophet67Panel (src/PluginEditor.cpp) with its controls
 * (src/ui/Controls.cpp) and its look (src/ui/Look.cpp), drawn at the MPC's width (1560 -> 1280: 0.8205), so the MPC
 * shows the same panel, pixel for pixel as the VST3 at that size. What it writes, into the folder given:
 *
 *   panel.png          the tab's background (1280 x 628): the panel without its controls (their legends kept)
 *   c<NN>_*.png        each control cut out of the drawn panel at each value it can take: a knob's film strip (one
 *                      frame a value, top to bottom), a button's look in each run of values that looks and acts the
 *                      same (with the value a press gives, the VST's own rule), an LED list's rows lit and unlit,
 *                      EFFECTS' SELECT, its LEDs, its two displays
 *   manifest.json      what each is: the parameter, where it sits (MPC pixels), its frames; for make_skin.py
 *
 * Nothing here decides how anything looks: it only draws the VST's panel and cuts it up. */
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace {
constexpr float S = 1280.0f / (float)Prophet67Panel::W;   /* the MPC's 1280 across */
constexpr int TW = 1280, TH = 628;                          /* a tab */

Rectangle<int> toMpc(Rectangle<float> r)
{
    /* (the edges rounded, not widened: controls side by side meet without overlapping) */
    int x0 = (int)std::lround(r.getX() * S), y0 = (int)std::lround(r.getY() * S);
    int x1 = (int)std::lround(r.getRight() * S), y1 = (int)std::lround(r.getBottom() * S);
    x0 = jlimit(0, TW, x0), x1 = jlimit(0, TW, x1), y0 = jlimit(0, TH, y0), y1 = jlimit(0, TH, y1);
    return {x0, y0, x1 - x0, y1 - y0};
}

var rectVar(Rectangle<int> r) { return Array<var>{r.getX(), r.getY(), r.getWidth(), r.getHeight()}; }

struct Strip {                           /* frames of one size, stacked */
    Rectangle<int> r;
    int frames = 0;
    Image img;
    std::vector<bool> done;
    void init(Rectangle<int> rr, int n)
    {
        r = rr;
        frames = n;
        img = Image(Image::ARGB, jmax(1, r.getWidth()), jmax(1, r.getHeight() * n), true);
        done.assign((size_t)n, false);
    }
    void grab(const Image &full, int frame)
    {
        if (frame < 0 || frame >= frames || done[(size_t)frame])
            return;
        Graphics g(img);
        g.drawImageAt(full.getClippedImage(r), 0, frame * r.getHeight());
        done[(size_t)frame] = true;
    }
    bool complete() const { return std::all_of(done.begin(), done.end(), [](bool b) { return b; }); }
};

struct Ctl {
    enum Kind { knob, button, leds } kind;
    Component *c = nullptr;
    RangedAudioParameter *p = nullptr;
    String id, legend;
    int fx = -1, steps = 2, lo = 0;
    Strip strip;                         /* knob */
    struct Range {
        int a, b, next;
        Strip s;
    };
    std::vector<Range> ranges;           /* button */
    std::vector<Strip> on, off;          /* leds: each row */
};

int stepsOf(RangedAudioParameter &p, int &lo)
{
    auto r = p.getNormalisableRange();
    if (dynamic_cast<AudioParameterFloat *>(&p)) {   /* VOLUME: 128 places */
        lo = 0;
        return 128;
    }
    lo = (int)r.start;
    return (int)r.end - (int)r.start + 1;
}

constexpr int MAXF = 128;               /* a film strip's most frames (CUTOFF's 256 values: one frame each two) */
int framesOf(int steps) { return jmin(steps, MAXF); }
int indexOf(int steps, int frame)        /* the value a frame shows */
{
    int f = framesOf(steps);
    return f == steps ? frame : (int)std::lround((double)frame * (steps - 1) / (f - 1));
}

void setIndex(RangedAudioParameter &p, int steps, int i)
{
    i = jlimit(0, steps - 1, i);
    p.setValueNotifyingHost(steps > 1 ? (float)i / (float)(steps - 1) : 0.0f);
}

bool writePng(const Image &img, const File &f)
{
    f.deleteFile();
    FileOutputStream out(f);
    PNGImageFormat png;
    return out.openedOk() && png.writeImageToStream(img, out);
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: p67_skin_render <out folder>\n");
        return 2;
    }
    ScopedJuceInitialiser_GUI gui;
    File dir = File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    dir.createDirectory();

    Prophet67Processor proc;
    auto panel = std::make_unique<Prophet67Panel>(proc);
    panel->setBounds(0, 0, Prophet67Panel::W, Prophet67Panel::H);
    panel->tens = 1;                     /* (no program button lit: programs are the MPC's own) */
    auto snap = [&] { return panel->createComponentSnapshot(panel->getLocalBounds(), true, S); };

    /* ---- what is on the panel ---- */
    std::vector<std::unique_ptr<Ctl>> ctls;
    Component *fxSel = nullptr, *fxCode = nullptr, *fxValue = nullptr, *bpm = nullptr, *prog = nullptr, *name = nullptr;
    for (auto *c : panel->getChildren()) {
        auto ctl = std::make_unique<Ctl>();
        ctl->c = c;
        if (auto *k = dynamic_cast<p67::Knob *>(c)) {
            ctl->kind = Ctl::knob;
            ctl->p = &k->param;
            ctl->legend = k->legend;
        } else if (auto *b = dynamic_cast<p67::Button *>(c)) {
            ctl->kind = Ctl::button;
            ctl->p = &b->param;
        } else if (auto *l = dynamic_cast<p67::LedList *>(c)) {
            ctl->kind = Ctl::leds;
            ctl->p = &l->param;
        } else {
            if (String(typeid(*c).name()).contains("FxSelect"))
                fxSel = c;
            else if (dynamic_cast<p67::SegDisplay *>(c)) {
                auto r = c->getBounds();
                if (r.getWidth() == 76)
                    (r.getY() < 60 ? fxCode : fxValue) = c;
                else if (r.getWidth() == 84)
                    bpm = c;
                else if (r.getWidth() == 112)
                    prog = c;
                else
                    name = c;
            }
            continue;
        }
        ctl->id = ctl->p->paramID;
        ctl->steps = stepsOf(*ctl->p, ctl->lo);
        ctls.push_back(std::move(ctl));
    }
    if (!fxSel || !fxCode || !fxValue || !bpm || !prog || !name) {
        std::fprintf(stderr, "the panel is not as expected (EFFECTS, the displays)\n");
        return 1;
    }
    for (int f = 0; f < 5; f++) {        /* EFFECTS' knobs (and DELAY's SYNC): those shown for one effect only */
        panel->selectFx(f);
        for (auto &c : ctls)
            if (c->c->isVisible() && c->fx < 0) {
                bool always = true;
                for (int g = 0; g < 5 && always; g++) {
                    panel->selectFx(g);
                    always = c->c->isVisible();
                }
                panel->selectFx(f);
                if (!always)
                    c->fx = f;
            }
    }

    /* ---- the strips and images to fill ---- */
    p67::Knob *fxFirst[5] = {};
    for (auto &c : ctls) {
        auto b = c->c->getBounds().toFloat();
        if (c->kind == Ctl::knob) {
            auto *k = static_cast<p67::Knob *>(c->c);
            if (c->fx < 0 && k->legend.isNotEmpty())   /* the legend is the background's; the strip only the knob */
                b.removeFromBottom(14.0f * (k->legend.contains("\n") ? 2.0f : 1.0f));
            c->strip.init(toMpc(b), framesOf(c->steps));
            if (c->fx >= 0 && (!fxFirst[c->fx] || k->getX() < fxFirst[c->fx]->getX()))
                fxFirst[c->fx] = k;
        } else if (c->kind == Ctl::button) {
            auto *bt = static_cast<p67::Button *>(c->c);
            int mask = (1 << bt->leds.size()) - 1;
            for (int i = 0; i < c->steps; i++) {
                int v = c->lo + i, nx = jlimit(0, c->steps - 1, bt->next(v) - c->lo);
                bool on = bt->isOn(v);
                if (!c->ranges.empty()) {
                    auto &last = c->ranges.back();
                    int lv = c->lo + last.a;
                    if (last.next == nx && bt->isOn(lv) == on && (lv & mask) == (v & mask)) {
                        last.b = i;
                        continue;
                    }
                }
                c->ranges.push_back({i, i, nx, {}});
                c->ranges.back().s.init(toMpc(b), 1);
            }
        } else {
            auto *l = static_cast<p67::LedList *>(c->c);
            for (int i = 0; i < l->names.size(); i++) {
                auto r = toMpc(l->item(i) + b.getPosition());
                c->on.emplace_back();
                c->on.back().init(r, 1);
                c->off.emplace_back();
                c->off.back().init(r, 1);
            }
        }
    }
    int fxSteps[5];
    Strip fxValueStrip[5], fxCodeStrip, selectStrip;
    std::vector<Strip> selOn(5), selOff(5);
    fxCodeStrip.init(toMpc(fxCode->getBounds().toFloat()), 5);
    selectStrip.init(toMpc(fxSel->getBounds().toFloat().withWidth(50.0f)), 1);
    for (int i = 0; i < 5; i++) {        /* (FxSelect::item) */
        auto r = toMpc(Rectangle<float>(54.0f, 14.0f + 13.0f * (float)i, (float)fxSel->getWidth() - 54.0f, 13.0f)
                       + fxSel->getBounds().toFloat().getPosition());
        selOn[(size_t)i].init(r, 1);
        selOff[(size_t)i].init(r, 1);
    }
    for (int f = 0; f < 5; f++) {
        int lo;
        fxSteps[f] = stepsOf(fxFirst[f]->param, lo);
        fxValueStrip[f].init(toMpc(fxValue->getBounds().toFloat()), fxSteps[f]);
    }

    /* ---- the background: the panel without its controls ---- */
    for (auto &c : ctls)
        c->c->setVisible(false);
    for (auto *c : {fxSel, fxCode, fxValue, bpm, prog, name})
        c->setVisible(false);
    Image bg = snap().getClippedImage({0, 0, TW, TH}).createCopy();
    {
        Graphics g(bg);
        g.addTransform(AffineTransform::scale(S));
        for (auto &c : ctls)             /* the knobs' legends (an effect's go with its knobs) */
            if (c->kind == Ctl::knob && c->fx < 0) {
                auto *k = static_cast<p67::Knob *>(c->c);
                if (k->legend.isEmpty())
                    continue;
                auto r = k->getBounds().toFloat();
                p67::drawLegend(g, k->legend, r.removeFromBottom(14.0f * (k->legend.contains("\n") ? 2.0f : 1.0f)), 10.0f);
            }
        for (auto *c : {bpm, prog}) {    /* the tempo and the program are the MPC's: their windows dark */
            auto r = c->getBounds().toFloat().reduced(3.0f);
            p67::drawSegWindow(g, r);
            p67::drawSegText(g, r, "", 3);
        }
        auto r = name->getBounds().toFloat().reduced(3.0f);
        p67::drawSegWindow(g, r);
        p67::drawSeg14Text(g, r, "PROPHET 67", 12);
    }
    writePng(bg, dir.getChildFile("panel.png"));

    /* ---- every control at every value: the controls not of EFFECTS ---- */
    for (auto &c : ctls)
        c->c->setVisible(c->fx < 0);
    for (int i = 0; i < 128; i++) {
        for (auto &c : ctls)
            if (c->fx < 0)
                setIndex(*c->p, c->steps, indexOf(c->steps, i));
        auto full = snap();
        for (auto &c : ctls) {
            if (c->fx >= 0 || i >= framesOf(c->steps))
                continue;
            if (c->kind == Ctl::knob)
                c->strip.grab(full, i);
            else if (c->kind == Ctl::button) {
                for (auto &r : c->ranges)
                    if (r.a == i)
                        r.s.grab(full, 0);
            } else
                for (size_t k = 0; k < c->on.size(); k++)
                    (k == (size_t)i ? c->on[k] : c->off[k]).grab(full, 0);
        }
    }
    /* (an LED row never off at the lowest value: off at another) */
    for (auto &c : ctls)
        if (c->kind == Ctl::leds && c->fx < 0 && !c->off.empty() && !c->off[0].complete()) {
            setIndex(*c->p, c->steps, 1);
            c->off[0].grab(snap(), 0);
        }

    /* ---- EFFECTS: each effect's controls, SELECT, its LEDs, the displays ---- */
    for (auto &c : ctls)
        if (c->fx < 0)
            c->c->setVisible(false);
    for (auto *c : {fxSel, fxCode, fxValue})
        c->setVisible(true);
    for (int f = 0; f < 5; f++) {
        panel->selectFx(f);
        for (auto &c : ctls)
            if (c->fx >= 0)
                c->c->setVisible(c->fx == f);
        for (int i = 0; i < 128; i++) {
            for (auto &c : ctls)
                if (c->fx == f)
                    setIndex(*c->p, c->steps, indexOf(c->steps, i));
            auto full = snap();
            for (auto &c : ctls) {
                if (c->fx != f || i >= framesOf(c->steps))
                    continue;
                if (c->kind == Ctl::knob)
                    c->strip.grab(full, i);
                else if (c->kind == Ctl::button)
                    for (auto &r : c->ranges)
                        if (r.a == i)
                            r.s.grab(full, 0);
            }
            if (i < fxSteps[f])
                fxValueStrip[f].grab(full, i);
            if (i == 0) {
                fxCodeStrip.grab(full, f);
                selectStrip.grab(full, 0);
                selOn[(size_t)f].grab(full, 0);
                selOff[(size_t)((f + 1) % 5)].grab(full, 0);
            }
        }
    }

    /* ---- the files and the manifest ---- */
    int n = 0;
    auto save = [&](Strip &s, const String &what) {
        if (!s.complete())
            std::fprintf(stderr, "warning: %s has frames not drawn\n", what.toRawUTF8());
        String f = "c" + String(n++).paddedLeft('0', 3) + "_" + what.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789_") + ".png";
        writePng(s.img, dir.getChildFile(f));
        return f;
    };
    Array<var> list;
    for (auto &c : ctls) {
        auto *o = new DynamicObject();
        o->setProperty("id", c->id);
        o->setProperty("fx", c->fx);
        o->setProperty("steps", c->steps);
        o->setProperty("frames", framesOf(c->steps));
        o->setProperty("choice", dynamic_cast<AudioParameterChoice *>(c->p) != nullptr || dynamic_cast<AudioParameterBool *>(c->p) != nullptr);
        Array<var> names;                /* a choice's names (the MPC plugin's are the same) */
        if (auto *ch = dynamic_cast<AudioParameterChoice *>(c->p))
            for (auto &t : ch->choices)
                names.add(t);
        else if (dynamic_cast<AudioParameterBool *>(c->p))
            names = {"OFF", "ON"};
        else if (c->steps <= 8)
            for (int i = 0; i < c->steps; i++)
                names.add(String(c->lo + i));
        o->setProperty("names", names);
        if (c->kind == Ctl::knob) {
            o->setProperty("kind", "knob");
            o->setProperty("legend", c->legend);
            o->setProperty("bipolar", (bool)c->c->getProperties()["bipolar"]);
            o->setProperty("rect", rectVar(c->strip.r));
            o->setProperty("frame", rectVar(toMpc(c->c->getBounds().toFloat())));   /* (with its legend) */
            o->setProperty("strip", save(c->strip, "knob_" + c->id));
        } else if (c->kind == Ctl::button) {
            o->setProperty("kind", "button");
            o->setProperty("rect", rectVar(c->ranges.front().s.r));
            Array<var> rs;
            for (auto &r : c->ranges) {
                auto *ro = new DynamicObject();
                ro->setProperty("a", r.a);
                ro->setProperty("b", r.b);
                ro->setProperty("next", r.next);
                ro->setProperty("image", save(r.s, "button_" + c->id + "_" + String(r.a)));
                rs.add(var(ro));
            }
            o->setProperty("ranges", rs);
        } else {
            o->setProperty("kind", "leds");
            Array<var> rows;
            for (size_t k = 0; k < c->on.size(); k++) {
                auto *ro = new DynamicObject();
                ro->setProperty("rect", rectVar(c->on[k].r));
                ro->setProperty("on", save(c->on[k], "led_" + c->id + "_" + String((int)k) + "_on"));
                ro->setProperty("off", save(c->off[k], "led_" + c->id + "_" + String((int)k) + "_off"));
                rows.add(var(ro));
            }
            o->setProperty("rows", rows);
        }
        list.add(var(o));
    }
    auto *fx = new DynamicObject();
    fx->setProperty("select", rectVar(selectStrip.r));
    fx->setProperty("selectImage", save(selectStrip, "fx_select"));
    Array<var> rows;
    for (int i = 0; i < 5; i++) {
        auto *ro = new DynamicObject();
        ro->setProperty("rect", rectVar(selOn[(size_t)i].r));
        ro->setProperty("on", save(selOn[(size_t)i], "fx_led_" + String(i) + "_on"));
        ro->setProperty("off", save(selOff[(size_t)i], "fx_led_" + String(i) + "_off"));
        rows.add(var(ro));
    }
    fx->setProperty("rows", rows);
    fx->setProperty("code", rectVar(fxCodeStrip.r));
    fx->setProperty("codeStrip", save(fxCodeStrip, "fx_code"));
    Array<var> codes;                    /* (each effect's alone, for its own FX page) */
    for (int f = 0; f < 5; f++) {
        Strip one;
        one.init(fxCodeStrip.r, 1);
        Graphics g(one.img);
        g.drawImageAt(fxCodeStrip.img.getClippedImage(fxCodeStrip.r.withPosition(0, f * fxCodeStrip.r.getHeight())), 0, 0);
        one.done[0] = true;
        codes.add(save(one, "fx_code_" + String(f)));
    }
    fx->setProperty("codes", codes);
    fx->setProperty("value", rectVar(fxValueStrip[0].r));
    Array<var> vals;
    for (int f = 0; f < 5; f++) {
        auto *vo = new DynamicObject();
        vo->setProperty("id", fxFirst[f]->param.paramID);
        vo->setProperty("steps", fxSteps[f]);
        vo->setProperty("strip", save(fxValueStrip[f], "fx_value_" + String(f)));
        vals.add(var(vo));
    }
    fx->setProperty("values", vals);
    auto *root = new DynamicObject();
    root->setProperty("scale", S);
    root->setProperty("background", "panel.png");
    root->setProperty("controls", list);
    root->setProperty("effects", var(fx));
    dir.getChildFile("manifest.json").replaceWithText(JSON::toString(var(root)));
    std::printf("%d controls, %d pictures, in %s\n", (int)ctls.size(), n + 1, dir.getFullPathName().toRawUTF8());
    panel = nullptr;
    return 0;
}
