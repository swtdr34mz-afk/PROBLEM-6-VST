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

    const juce::String getName() const override { return "PROPHET 67"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return NPRESETS; }
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
    std::atomic<bool> dlSynced{false};   /* DELAY SYNC on: TIME shows notes (for the host's and the editor's text) */
    juce::AudioProcessorValueTreeState apvts;
    static juce::String paramId(int i) { return "p" + juce::String(i); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout(std::atomic<bool> *dlSynced);

private:
    P67Engine engine;
    uint8_t sent[NPARAM] = {};
    bool patched = false;
    int program = 0;
    double rate = 44100.0;

    /* the plugin's own settings, read each block */
    struct Globals {
        bool arpOn = false, arpHold = false, lowHold = false;
        int arpMode = 0, arpRate = 2, arpOct = 1, arpGate = 50, swing = 0;
        int split = 0, point = 60, scaleL = 0, scaleR = 0, key = 0, follow = 1, balance = 0;
        float volume = 0.7f;
    } g, gSent;
    bool arpSent = false;
    void readGlobals();

    /* the LOW SPLIT on MIDI notes (the firmware's logic, app.c key_map) */
    int mapped[128];                     /* the note each incoming note plays (-1 none) */
    bool held[128] = {}, latched[128] = {};
    int root = -1;
    int keyMap(int note) const;
    bool isLeft(int note) const { return g.split > 0 && note < g.point; }
    float zoneGain(int note) const;
    void noteDown(int note, float vel);
    void noteUp(int note);
    void stopNote(int note);
    void rootUpdate();
    void handleMidi(const juce::MidiMessage &m);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Prophet67Processor)
};
