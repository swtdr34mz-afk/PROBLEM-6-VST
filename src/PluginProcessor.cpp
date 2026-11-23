/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Sections.h"

/* --------------------------------------------------------------- the scales (the firmware's, app.c) --- */
namespace {
struct Scale {
    const char *name;
    int n;
    int st[7];
};
const Scale SCALES[] = {
    {"MAJOR", 7, {0, 2, 4, 5, 7, 9, 11}},   {"MINOR", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"DORIAN", 7, {0, 2, 3, 5, 7, 9, 10}},  {"PHRYGIAN", 7, {0, 1, 3, 5, 7, 8, 10}},
    {"LYDIAN", 7, {0, 2, 4, 6, 7, 9, 11}},  {"MIXOLYD", 7, {0, 2, 4, 5, 7, 9, 10}},
    {"HARM MIN", 7, {0, 2, 3, 5, 7, 8, 11}}, {"PENTA MAJ", 5, {0, 2, 4, 7, 9}},
    {"PENTA MIN", 5, {0, 3, 5, 7, 10}},     {"BLUES", 6, {0, 3, 5, 6, 7, 10}},
    {"ARABIC", 7, {0, 1, 4, 5, 7, 8, 11}},
};
const int NSC = (int)(sizeof SCALES / sizeof SCALES[0]);
const char *const NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
bool isWhite(int n)
{
    int c = n % 12;
    return c == 0 || c == 2 || c == 4 || c == 5 || c == 7 || c == 9 || c == 11;
}
int snap(int n, int rootNote, int scale)        /* to the scale's note at or below n */
{
    const Scale &sc = SCALES[(scale - 1) % NSC];
    int rel = ((n - rootNote) % 12 + 12) % 12, best = 0;
    for (int i = 0; i < sc.n; i++)
        if (sc.st[i] <= rel)
            best = sc.st[i];
    return n - rel + best;
}
juce::String noteName(int n) { return juce::String(NOTE[n % 12]) + juce::String(n / 12 - 1); }
} // namespace

/* ------------------------------------------------------------------------------- parameters --- */
juce::AudioProcessorValueTreeState::ParameterLayout Prophet67Processor::createLayout(std::atomic<bool> *dlSynced)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < NPARAM; i++) {
        const param_t &d = PARAMS[i];
        juce::String sec(sectionOf(i)), name = sec == d.name ? sec : sec + " " + d.name;
        if (i == P_O2_MODE)
            name = "OSC 2 MODE";             /* (MIXER's OSC 2 is the level) */
        int def = FACTORY[0].p[i];
        if (d.kind == PK_CHOICE) {
            juce::StringArray ch;
            for (int k = 0; k <= d.max; k++)
                ch.add(d.choice[k]);
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{paramId(i), 1}, name, ch, def));
        } else {
            layout.add(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID{paramId(i), 1}, name, 0, d.max, def,
                juce::AudioParameterIntAttributes().withStringFromValueFunction([i, dlSynced](int v, int) {
                    char t[24];
                    if (PARAMS[i].kind == PK_DTIME && dlSynced->load())
                        return juce::String(DL_SYNC_NAME[v * NDLS / 128]);
                    param_text(i, v, t);
                    return juce::String(t);
                })));
        }
    }
    auto choice = [&](const char *id, const char *name, juce::StringArray ch, int def) {
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, 1}, name, ch, def));
    };
    auto integer = [&](const char *id, const char *name, int lo, int hi, int def, std::function<juce::String(int)> txt) {
        layout.add(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{id, 1}, name, lo, hi, def,
            juce::AudioParameterIntAttributes().withStringFromValueFunction([txt](int v, int) { return txt(v); })));
    };
    auto num = [](int v) { return juce::String(v); };
    juce::StringArray rates{"1/8", "1/8T", "1/16", "1/16T", "1/32"};
    juce::StringArray scales{"CHROMA"};
    for (int i = 0; i < NSC; i++)
        scales.add(SCALES[i].name);
    juce::StringArray keys;
    for (int i = 0; i < 12; i++)
        keys.add(NOTE[i]);
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"arp_on", 1}, "ARP ON", false));
    choice("arp_mode", "ARP MODE", {"UP", "DOWN", "UP-DN", "RANDOM", "PLAYED"}, 0);
    choice("arp_rate", "ARP RATE", rates, 2);
    integer("arp_oct", "ARP OCTAVES", 1, 4, 1, num);
    integer("arp_gate", "ARP GATE", 5, 100, 50, [](int v) { return juce::String(v) + "%"; });
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"arp_hold", 1}, "ARP HOLD", false));
    integer("swing", "SWING", 0, 50, 0, [](int v) { return juce::String(v) + "%"; });
    choice("split", "SPLIT", {"OFF", "-1 OCT", "-2 OCT", "-3 OCT"}, 0);
    integer("split_point", "SPLIT POINT", 24, 96, 60, [](int v) { return noteName(v); });
    choice("scale_l", "SCALE L", scales, 0);
    choice("scale_r", "SCALE R", scales, 0);
    choice("key", "KEY", keys, 0);
    choice("follow", "FOLLOW", {"KEY", "ROOT", "WHITE"}, 1);
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"low_hold", 1}, "LOW HOLD", false));
    integer("balance", "BALANCE", -50, 50, 0, [](int v) { return (v > 0 ? "+" : "") + juce::String(v); });
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"volume", 1}, "VOLUME", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return v <= 0.0001f ? juce::String("-inf") : juce::String(20.0f * std::log10(v * v * 2.0f), 1) + " dB"; })));
    return layout;
}

Prophet67Processor::Prophet67Processor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PROPHET67", createLayout(&dlSynced))
{
    for (int i = 0; i < 128; i++)
        mapped[i] = -1;
}

bool Prophet67Processor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void Prophet67Processor::prepareToPlay(double sampleRate, int)
{
    rate = sampleRate;
    engine.prepare(sampleRate);
    patched = false;
    arpSent = false;
    for (int i = 0; i < 128; i++) {
        mapped[i] = -1;
        held[i] = latched[i] = false;
    }
    root = -1;
}

void Prophet67Processor::setCurrentProgram(int index)
{
    if (index < 0 || index >= NPRESETS)
        return;
    program = index;
    for (int i = 0; i < NPARAM; i++)
        if (auto *p = apvts.getParameter(paramId(i)))
            p->setValueNotifyingHost(p->convertTo0to1((float)FACTORY[index].p[i]));
}

const juce::String Prophet67Processor::getProgramName(int index)
{
    return index >= 0 && index < NPRESETS ? juce::String(FACTORY[index].name) : juce::String();
}

void Prophet67Processor::readGlobals()
{
    auto v = [&](const char *id) { return apvts.getRawParameterValue(id)->load(); };
    g.arpOn = v("arp_on") > 0.5f;
    g.arpHold = v("arp_hold") > 0.5f;
    g.arpMode = (int)v("arp_mode");
    g.arpRate = (int)v("arp_rate");
    g.arpOct = (int)v("arp_oct");
    g.arpGate = (int)v("arp_gate");
    g.swing = (int)v("swing");
    g.split = (int)v("split");
    g.point = (int)v("split_point");
    g.scaleL = (int)v("scale_l");
    g.scaleR = (int)v("scale_r");
    g.key = (int)v("key");
    g.follow = (int)v("follow");
    g.lowHold = v("low_hold") > 0.5f;
    g.balance = (int)v("balance");
    g.volume = v("volume");
}

/* ------------------------------------------------------------------------------ the split --- */
int Prophet67Processor::keyMap(int note) const
{
    if (isLeft(note)) {
        int n = note - 12 * g.split;
        return g.scaleL ? snap(n, g.key, g.scaleL) : n;
    }
    if (!g.scaleR)
        return note;
    int base = g.split > 0 && g.follow != 0 && root >= 0 ? root : g.key;
    if (g.split > 0 && g.follow == 2) {  /* WHITE: the scale on the white keys from the split point up */
        const Scale &sc = SCALES[(g.scaleR - 1) % NSC];
        if (!isWhite(note))
            return -1;
        int first = g.point;
        while (!isWhite(first))
            first++;
        int d = 0;
        for (int j = first; j < note; j++)
            d += isWhite(j) ? 1 : 0;
        int b0 = first - (((first - base) % 12 + 12) % 12);
        if (first - b0 > 6)
            b0 += 12;
        return b0 + 12 * (d / sc.n) + sc.st[d % sc.n];
    }
    return snap(note, base, g.scaleR);
}

float Prophet67Processor::zoneGain(int note) const
{
    float b = (float)g.balance / 50.0f;
    if (g.split == 0)
        return 1.0f;
    return isLeft(note) ? (b > 0.0f ? 1.0f - b : 1.0f) : (b < 0.0f ? 1.0f + b : 1.0f);
}

void Prophet67Processor::rootUpdate()
{
    for (int n = 0; n < 128; n++)
        if (isLeft(n) && mapped[n] >= 0) {
            root = mapped[n] % 12;
            return;
        }
}

void Prophet67Processor::stopNote(int note)
{
    if (mapped[note] >= 0) {
        if (engine.arpOn())
            engine.arpKey(mapped[note], false, 0.0f);
        else
            engine.noteOff(note);
    }
    mapped[note] = -1;
    latched[note] = false;
}

void Prophet67Processor::noteDown(int note, float vel)
{
    if (g.lowHold && isLeft(note)) {     /* a new note on the left, none held there: the held ones go */
        bool anyHeld = false, anyLatched = false;
        for (int n = 0; n < 128; n++) {
            anyHeld |= held[n] && isLeft(n);
            anyLatched |= latched[n];
        }
        if (!anyHeld && anyLatched)
            for (int n = 0; n < 128; n++)
                if (latched[n])
                    stopNote(n);
    }
    if (mapped[note] >= 0)
        stopNote(note);
    held[note] = true;
    int m = keyMap(note);
    if (m < 0 || m > 127)
        return;
    mapped[note] = m;
    if (engine.arpOn())
        engine.arpKey(m, true, vel);
    else
        engine.noteOn(note, m, vel, zoneGain(note));
    if (isLeft(note)) {
        if (g.lowHold)
            latched[note] = true;
        rootUpdate();
    }
}

void Prophet67Processor::noteUp(int note)
{
    held[note] = false;
    if (latched[note] && g.lowHold)
        return;                          /* LOW HOLD: it stays */
    stopNote(note);
    rootUpdate();
}

void Prophet67Processor::handleMidi(const juce::MidiMessage &m)
{
    if (m.isNoteOn())
        noteDown(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteUp(m.getNoteNumber());
    else if (m.isPitchWheel())
        engine.bend((float)(m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())
        engine.pressAll((float)m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())
        engine.press(m.getNoteNumber(), (float)m.getAfterTouchValue() / 127.0f);
    else if (m.isSustainPedalOn())
        engine.sustain(true);
    else if (m.isSustainPedalOff())
        engine.sustain(false);
    else if (m.isController()) {
        int cc = m.getControllerNumber(), v = m.getControllerValue();
        if (cc == 1)
            engine.modWheel((float)v / 127.0f);
        else if (cc == 74)
            engine.cutoffCC(v);
        else if (cc == 120 || cc == 123) {
            engine.allOff();
            for (int n = 0; n < 128; n++) {
                mapped[n] = -1;
                held[n] = latched[n] = false;
            }
        }
    } else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allOff();
}

/* ------------------------------------------------------------------------------ the block --- */
void Prophet67Processor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    float *L = buffer.getWritePointer(0), *R = buffer.getWritePointer(buffer.getNumChannels() > 1 ? 1 : 0);

    /* the sound: the parameters, when they change */
    {
        uint8_t p[NPARAM];
        bool change = !patched;
        for (int i = 0; i < NPARAM; i++) {
            float v = apvts.getRawParameterValue(paramId(i))->load();
            int iv = (int)(v + 0.5f);
            p[i] = (uint8_t)juce::jlimit(0, (int)PARAMS[i].max, iv);
            change |= p[i] != sent[i];
        }
        dlSynced.store(p[P_DL_SYNC] != 0);
        if (change) {
            engine.patch(p);
            std::memcpy(sent, p, sizeof sent);
            patched = true;
        }
    }
    readGlobals();
    if (!arpSent || g.arpOn != gSent.arpOn || g.arpMode != gSent.arpMode || g.arpRate != gSent.arpRate
        || g.arpOct != gSent.arpOct || g.arpGate != gSent.arpGate) {
        if (g.arpOn != gSent.arpOn || !arpSent) {   /* the arp on or off: what sounds lets go */
            engine.allOff();
            for (int i = 0; i < 128; i++)
                mapped[i] = -1, latched[i] = false;
        }
        engine.arp(g.arpOn, g.arpMode, g.arpRate, g.arpOct, (float)g.arpGate * 0.01f);
    }
    if (!arpSent || g.arpHold != gSent.arpHold)
        engine.arpHold(g.arpHold);
    if (arpSent && gSent.lowHold && !g.lowHold)   /* LOW HOLD off: what only it held goes */
        for (int i = 0; i < 128; i++)
            if (latched[i] && !held[i])
                stopNote(i);
    arpSent = true;
    gSent = g;

    /* the host's tempo and position */
    float bpm = 120.0f;
    if (auto *ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm())
                bpm = (float)*b;
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                    engine.clockPos(*ppq);
        }
    }
    engine.tempo(bpm, (float)g.swing * 0.01f);

    /* MIDI on time: the block rendered up to each event */
    int done = 0;
    for (const auto meta : midi) {
        int at = juce::jlimit(0, n, meta.samplePosition);
        if (at > done) {
            engine.render(L + done, R + done, at - done);
            done = at;
        }
        handleMidi(meta.getMessage());
    }
    if (n > done)
        engine.render(L + done, R + done, n - done);
    float gain = g.volume * g.volume * 2.0f;   /* (0.7: about 0 dB) */
    buffer.applyGain(0, 0, n, gain);
    if (buffer.getNumChannels() > 1)
        buffer.applyGain(1, 0, n, gain);
    for (int c = 2; c < buffer.getNumChannels(); c++)
        buffer.clear(c, 0, n);
}

/* ------------------------------------------------------------------------------ state --- */
void Prophet67Processor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    state.setProperty("program", program, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Prophet67Processor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType())) {
        auto state = juce::ValueTree::fromXml(*xml);
        program = (int)state.getProperty("program", 0);
        apvts.replaceState(state);
    }
}

juce::AudioProcessorEditor *Prophet67Processor::createEditor() { return new Prophet67Editor(*this); }

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new Prophet67Processor(); }
