/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
#include "PluginProcessor.h"
#include "PanelDialog.h"
#include "P6Import.h"
#include "PluginEditor.h"
#include "Sections.h"

/* LOW SPLIT's scales and the rest: KeySplit.h (shared with the MPC plugin) */
using namespace p67split;
namespace {
juce::String noteName(int n) { return juce::String(NOTE[n % 12]) + juce::String(n / 12 - 2); }   /* 60: C3 (as Cubase and the MPC) */
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
    /* HIGH-PASS VELOCITY and KEYBOARD: the plugin's own (on the FM-1, the high-pass follows the low-pass's) */
    integer("hp_vel", "HIGH-PASS VELOCITY", 0, 127, FACTORY[0].p[P_FVEL], num);
    choice("hp_keytrk", "HIGH-PASS KEYBOARD", {"OFF", "HALF", "FULL"}, FACTORY[0].p[P_KEYTRK]);
    integer("fenv_vel", "FILTER ENV VELOCITY", 0, 127, 0, num);   /* velocity to the filter envelope (both filters); off in the factory programs */
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
    loadUserPresets();
}

/* ------------------------------------------------------------------------- user presets --- */
juce::File Prophet67Processor::presetFolder()
{
    /* PROBLEM 6 (until 1.0.8 PROPHET 67): the old folder's programs come along once, the old folder left as it was */
    auto docs = pdlg::docsOrAppData();
    auto now = docs.getChildFile("PROBLEM 6").getChildFile("Presets"), old = docs.getChildFile("PROPHET 67").getChildFile("Presets");
    if (!now.isDirectory() && old.isDirectory())
        old.copyDirectoryTo(now);
    return now;
}

void Prophet67Processor::loadUserPresets()
{
    userPresets.clear();
    auto files = presetFolder().findChildFiles(juce::File::findFiles, false, "*.p67");
    files.sort();
    auto p6 = p6Folder().findChildFiles(juce::File::findFiles, false, "*.p67");   /* after WRITE's: "000 NAME.p67" */
    p6.sort();
    files.addArray(p6);
    for (auto &f : files)
        if (auto xml = juce::XmlDocument::parse(f)) {
            if (!xml->hasTagName("PROPHET67PRESET"))
                continue;
            UserPreset u;
            u.name = xml->getStringAttribute("name", f.getFileNameWithoutExtension());
            for (int i = 0; i < NPARAM; i++)   /* (a parameter the file lacks: the INIT program's) */
                u.p[i] = (uint8_t)juce::jlimit(0, (int)PARAMS[i].max, xml->getIntAttribute("p" + juce::String(i), FACTORY[NPRESETS - 1].p[i]));
            u.hpVel = juce::jlimit(0, 127, xml->getIntAttribute("hp_vel", u.p[P_FVEL]));
            u.hpKt = juce::jlimit(0, 2, xml->getIntAttribute("hp_keytrk", u.p[P_KEYTRK]));
            u.fenvVel = juce::jlimit(0, 127, xml->getIntAttribute("fenv_vel", 0));
            u.p6 = f.getParentDirectory() == p6Folder() ? juce::jlimit(-1, 999, xml->getIntAttribute("p6_program", -1)) : -1;
            userPresets.push_back(u);
        }
}

int Prophet67Processor::writePreset(const juce::String &name)
{
    auto clean = name.trim().toUpperCase().substring(0, NAME_LEN - 1);
    if (clean.isEmpty())
        return -1;
    juce::XmlElement xml("PROPHET67PRESET");
    xml.setAttribute("name", clean);
    xml.setAttribute("version", 1);
    for (int i = 0; i < NPARAM; i++)
        xml.setAttribute("p" + juce::String(i), (int)std::lround(apvts.getRawParameterValue(paramId(i))->load()));
    xml.setAttribute("hp_vel", (int)std::lround(apvts.getRawParameterValue("hp_vel")->load()));
    xml.setAttribute("hp_keytrk", (int)std::lround(apvts.getRawParameterValue("hp_keytrk")->load()));
    xml.setAttribute("fenv_vel", (int)std::lround(apvts.getRawParameterValue("fenv_vel")->load()));
    auto folder = presetFolder();
    folder.createDirectory();
    auto file = folder.getChildFile(juce::File::createLegalFileName(clean) + ".p67");
    if (!xml.writeTo(file))
        return -1;
    loadUserPresets();
    for (size_t i = 0; i < userPresets.size(); i++)
        if (userPresets[i].name == clean && userPresets[i].p6 < 0) {
            program = NFACTORY + (int)i;
            updateHostDisplay(ChangeDetails().withProgramChanged(true));
            return program;
        }
    return -1;
}

int Prophet67Processor::importP6(const juce::File &syx)
{
    juce::MemoryBlock mb;
    if (!syx.loadFileAsData(mb))
        return 0;
    auto progs = p6ReadSysex((const uint8_t *)mb.getData(), mb.getSize());
    auto folder = p6Folder();
    folder.createDirectory();
    int n = 0, edit = 0;
    for (auto &pr : progs) {
        int num = pr.number >= 0 ? pr.number : 900 + juce::jmin(99, edit++);   /* an edit buffer: 900.. */
        auto name = juce::String(pr.name).toUpperCase();
        juce::XmlElement xml("PROPHET67PRESET");
        xml.setAttribute("name", name);
        xml.setAttribute("version", 1);
        xml.setAttribute("p6_program", num);   /* ("p6" alone is a parameter's) */
        for (int i = 0; i < NPARAM; i++)
            xml.setAttribute("p" + juce::String(i), (int)pr.p[i]);
        xml.setAttribute("hp_vel", pr.hpVel);
        xml.setAttribute("hp_keytrk", pr.hpKt);
        xml.setAttribute("fenv_vel", pr.fenvVel);
        /* the same program number again (a second file, or the same file twice): the newer in its place */
        for (auto &old : folder.findChildFiles(juce::File::findFiles, false, juce::String(num).paddedLeft('0', 3) + " *.p67"))
            old.deleteFile();
        auto file = folder.getChildFile(juce::String(num).paddedLeft('0', 3) + " " + juce::File::createLegalFileName(name) + ".p67");
        if (xml.writeTo(file))
            n++;
    }
    loadUserPresets();
    updateHostDisplay(ChangeDetails().withProgramChanged(true));
    return n;
}

const uint8_t *Prophet67Processor::programData(int index) const
{
    if (index >= 0 && index < NPRESETS)
        return FACTORY[index].p;
    if (index >= NPRESETS && index < NFACTORY)
        return VST_PRESETS[index - NPRESETS].p;
    if (index >= NFACTORY && index < NFACTORY + (int)userPresets.size())
        return userPresets[(size_t)(index - NFACTORY)].p;
    return nullptr;
}

bool Prophet67Processor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void Prophet67Processor::prepareToPlay(double sampleRate, int)
{
    rate = sampleRate;
    uiMidi.reset(sampleRate);
    engine.prepare(sampleRate);
    patched = false;
    hpVelSent = hpKtSent = fenvVelSent = -1;
    arpSent = false;
    split.reset();
    pedal = susSent = false;
    lastGain = -1.0f;
}

void Prophet67Processor::setCurrentProgram(int index)
{
    auto *d = programData(index);
    if (!d)
        return;
    program = index;
    for (int i = 0; i < NPARAM; i++)
        if (auto *p = apvts.getParameter(paramId(i)))
            p->setValueNotifyingHost(p->convertTo0to1((float)d[i]));
    int hv = d[P_FVEL], hk = d[P_KEYTRK];      /* the high-pass's: a program of your own keeps them, else as the low-pass's */
    if (index >= NFACTORY) {
        hv = userPresets[(size_t)(index - NFACTORY)].hpVel;
        hk = userPresets[(size_t)(index - NFACTORY)].hpKt;
    }
    if (auto *p = apvts.getParameter("hp_vel"))
        p->setValueNotifyingHost(p->convertTo0to1((float)hv));
    if (auto *p = apvts.getParameter("hp_keytrk"))
        p->setValueNotifyingHost(p->convertTo0to1((float)hk));
    if (auto *p = apvts.getParameter("fenv_vel"))
        p->setValueNotifyingHost(p->convertTo0to1(index >= NFACTORY ? (float)userPresets[(size_t)(index - NFACTORY)].fenvVel : 0.0f));
}

const juce::String Prophet67Processor::getProgramName(int index)
{
    if (index >= 0 && index < NPRESETS)
        return juce::String(FACTORY[index].name, (size_t)NAME_LEN);   /* (a name of 12 has no nought) */
    if (index >= NPRESETS && index < NFACTORY)
        return juce::String(VST_PRESETS[index - NPRESETS].name, (size_t)NAME_LEN);
    if (index >= NFACTORY && index < NFACTORY + (int)userPresets.size())
        return userPresets[(size_t)(index - NFACTORY)].name;
    return {};
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
    sp.split = (int)v("split");
    sp.point = (int)v("split_point");
    sp.scaleL = (int)v("scale_l");
    sp.scaleR = (int)v("scale_r");
    sp.key = (int)v("key");
    sp.follow = (int)v("follow");
    sp.lowHold = v("low_hold") > 0.5f;
    sp.balance = (int)v("balance");
    g.volume = v("volume");
}

void Prophet67Processor::handleMidi(const juce::MidiMessage &m)
{
    if (m.isNoteOn())
        split.noteDown(engine, m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        split.noteUp(engine, m.getNoteNumber());
    else if (m.isPitchWheel())
        engine.bend((float)(m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())
        engine.pressAll((float)m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())
        engine.press(m.getNoteNumber(), (float)m.getAfterTouchValue() / 127.0f);
    else if (m.isSustainPedalOn() || m.isSustainPedalOff()) {
        pedal = m.isSustainPedalOn();
        sustainUpdate();
    }
    else if (m.isController()) {
        int cc = m.getControllerNumber(), v = m.getControllerValue();
        if (cc == 1)
            engine.modWheel((float)v / 127.0f);
        else if (cc == 74)   /* brightness: around the CUTOFF knob, 64 the knob as it is (FL Studio sends Mod X as
                              * CC 74 with every note, 64 unless drawn; an MPE key's slide too), +-4.5 octaves */
            engine.cutoffMod((float)(juce::jlimit(0, 127, v) - 64) * (4.5f / 63.0f));
        else if (cc == 120 || cc == 123) {   /* ALL SOUND OFF: silent at once; ALL NOTES OFF: let go (their release) */
            if (cc == 120)
                engine.panic();
            else
                engine.allOff();
            split.forget(true);
        } else if (cc == 121) {          /* RESET ALL CONTROLLERS: the wheels, the pedal, the pressure, CC 74 */
            engine.bend(0.0f);
            engine.modWheel(0.0f);
            pedal = false;                 /* (HOLD, if on, still holds) */
            sustainUpdate();
            engine.pressAll(0.0f);
            engine.cutoffMod(0.0f);
        }
    } else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allOff();
}

/* the sustain: the pedal (CC 64), or HOLD while the arp is off (with the arp on, HOLD is the arp's own) */
void Prophet67Processor::sustainUpdate()
{
    bool want = pedal || (g.arpHold && !g.arpOn);
    if (want != susSent) {
        engine.sustain(want);
        susSent = want;
    }
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
        int hv = (int)(apvts.getRawParameterValue("hp_vel")->load() + 0.5f), hk = (int)(apvts.getRawParameterValue("hp_keytrk")->load() + 0.5f),
            fv = (int)(apvts.getRawParameterValue("fenv_vel")->load() + 0.5f);
        if (hv != hpVelSent || hk != hpKtSent || fv != fenvVelSent) {
            engine.hpExtra(hv, hk, fv);
            hpVelSent = hv;
            hpKtSent = hk;
            fenvVelSent = fv;
            change = true;
        }
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
            split.forget(false);
        }
        engine.arp(g.arpOn, g.arpMode, g.arpRate, g.arpOct, (float)g.arpGate * 0.01f);
    }
    if (!arpSent || g.arpHold != gSent.arpHold)
        engine.arpHold(g.arpHold);
    sustainUpdate();                       /* HOLD without the arp: the notes held, as by the pedal */
    split.set(engine, sp);                 /* (LOW HOLD off: what only it held goes) */
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
    hostBpm.store(bpm, std::memory_order_relaxed);

    uiMidi.removeNextBlockOfMessages(midi, n);
    keyboardState.processNextMidiBuffer(midi, 0, n, true);   /* the editor's keyboard joins the MIDI in */

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
    for (int v = 0; v < 7; v++)
        voiceLevel[v].store(engine.voiceEnv(v), std::memory_order_relaxed);
    arpSteps.store(engine.arpSteps(), std::memory_order_relaxed);
    float gain = g.volume * g.volume * 2.0f;   /* (0.7: about 0 dB) */
    if (lastGain < 0.0f)
        lastGain = gain;
    /* VOLUME moves in a ramp over the block, never a step */
    buffer.applyGainRamp(0, 0, n, lastGain, gain);
    if (buffer.getNumChannels() > 1)
        buffer.applyGainRamp(1, 0, n, lastGain, gain);
    lastGain = gain;
    for (int c = 2; c < buffer.getNumChannels(); c++)
        buffer.clear(c, 0, n);
}

/* ------------------------------------------------------------------------------ state --- */
void Prophet67Processor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    state.setProperty("program", program, nullptr);
    state.setProperty("uiScale", (double)uiScale.load(), nullptr);
    state.setProperty("uiPage", uiPage.load(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Prophet67Processor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType())) {
        auto state = juce::ValueTree::fromXml(*xml);
        program = (int)state.getProperty("program", 0);
        uiScale = (float)(double)state.getProperty("uiScale", 0.82);
        uiPage = (int)state.getProperty("uiPage", 0);
        apvts.replaceState(state);
    }
}

juce::AudioProcessorEditor *Prophet67Processor::createEditor() { return new Prophet67Editor(*this); }

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new Prophet67Processor(); }
