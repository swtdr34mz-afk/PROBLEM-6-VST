/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* PROPHET 67 as a plugin (VST3, AU) and a standalone app: the FM-1 firmware's engine (P67Engine.h), its parameters for
 * the host (automation, saved with the project), its 16 presets as the host's programs, MIDI in (notes, velocity,
 * channel and poly pressure, pitch bend, mod wheel, sustain, CC 74), the host's tempo and position for the arp and
 * the synced LFOs and delay, and the firmware's LOW SPLIT on the MIDI notes. */
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "P67Engine.h"
#include "KeySplit.h"
#include "vst_presets.h"

class Prophet67Processor : public juce::AudioProcessor
{
public:
    Prophet67Processor();
    ~Prophet67Processor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PROBLEM 6"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    /* the programs: the 16 factory programs (the FM-1's and the MPC's), the VST's own 50, then the user's own */
    static constexpr int NFACTORY = NPRESETS + NVST_PRESETS;
    /* A VST3 host (Cubase, FL Studio) sizes its program parameter when the plugin opens: a patch saved later lay
     * past that size, so the host answered the save with another patch. The VST3 host is told a fixed number
     * of slots instead; the slots past the real patches are empty. */
    static constexpr int HOST_PROGRAMS = 1024;
    int getNumPrograms() override { return wrapperType == wrapperType_VST3 ? juce::jmax(HOST_PROGRAMS, programCount()) : programCount(); }
    int programCount() const { return NFACTORY + (int)userPresets.size(); }
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String &) override {}

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    juce::MidiKeyboardState keyboardState;   /* the editor's keyboard */
    juce::MidiMessageCollector uiMidi;        /* the editor's wheels */
    std::atomic<float> voiceLevel[8] = {};    /* each voice's amp envelope, for the editor's LEDs */
    std::atomic<uint32_t> arpSteps{0};      /* for the editor: the arp's steps, the host's tempo */
    std::atomic<float> hostBpm{120.0f};
    std::atomic<float> uiScale{0.82f};        /* the editor's size, kept with the project */
    std::atomic<int> uiPage{0};
    std::atomic<bool> dlSynced{false};

    /* the user's own programs (WRITE): files in Documents/PROBLEM 6/Presets, after the 16 factory programs */
    struct UserPreset {
        juce::String name;
        uint8_t p[NPARAM];
        int hpVel = 0, hpKt = 0, fenvVel = 0;
        int p6 = -1;                      /* LOAD P6 SYSEX's: the Prophet-6's program number (else -1, WRITE's) */
    };
    std::vector<UserPreset> userPresets;
    static juce::File presetFolder();
    void loadUserPresets();
    /* the folder read again (after an import): the sound playing keeps its number */
    void rescanUser()
    {
        auto cur = getProgramName(program);
        loadUserPresets();
        if (getProgramName(program) != cur)
            for (int i = programCount(); --i >= 0;)
                if (getProgramName(i) == cur) {
                    program = i;
                    break;
                }
        updateHostDisplay(ChangeDetails().withProgramChanged(true));
    }
    int writePreset(const juce::String &name);   /* the sound as it is now, kept under this name: its program number */
    /* LOAD P6 SYSEX: a Prophet-6 SysEx file's programs, written as the user's own (Presets/P6 LIBRARY): how many */
    int importP6(const juce::File &syx);
    static juce::File p6Folder() { return presetFolder().getChildFile("P6 LIBRARY"); }
    const uint8_t *programData(int index) const;   /* DELAY SYNC on: TIME shows notes (for the host's and the editor's text) */
    juce::AudioProcessorValueTreeState apvts;
    static juce::String paramId(int i) { return "p" + juce::String(i); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout(std::atomic<bool> *dlSynced);

private:
    P67Engine engine;
    uint8_t sent[NPARAM] = {};
    bool patched = false;
    int hpVelSent = -1, hpKtSent = -1, fenvVelSent = -1;
    int program = 0;
    double rate = 44100.0;
    float lastGain = -1.0f;              /* VOLUME as the last block ended (its ramp) */

    /* the plugin's own settings, read each block */
    struct Globals {
        bool arpOn = false, arpHold = false;
        int arpMode = 0, arpRate = 2, arpOct = 1, arpGate = 50, swing = 0;
        float volume = 0.7f;
    } g, gSent;
    p67split::Settings sp;               /* LOW SPLIT's, read each block */
    bool arpSent = false;
    void readGlobals();
    bool pedal = false, susSent = false;  /* the sustain pedal; the sustain the engine has */
    void sustainUpdate();

    p67split::KeySplit split;            /* the LOW SPLIT on MIDI notes (KeySplit.h) */
    void handleMidi(const juce::MidiMessage &m);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Prophet67Processor)
};
