# PROPHETIC 6: PROPHET 67 for the MPC

The PROPHET 67 synth as a plugin for Akai MPC standalone hardware, named PROPHETIC 6 there: the same engine as the PROPHET 67 VST3 and the
M-Vave FM-1 firmware, built with Akai's MPC Plugin SDK. A 7-voice analogue-style poly after the Sequential
Prophet-6: two oscillators (triangle - saw - pulse, sub octave, noise, hard sync, LOW FREQ), poly mod, a resonant
high-pass into a creamy 4-pole low-pass, filter and amp envelopes, VINTAGE and SPREAD, two LFOs (LFO 1 into the
audio range; on S+H at the top, the Prophet-6's noise), distortion, chorus, delay, five reverbs (ROOM, HALL, PLATE,
SPRING, SPACE), an arpeggiator on the MPC's tempo and the LOW SPLIT with its scales.

## Installieren (kurz auf Deutsch)

1. MPC OS **3.10.1 oder neuer**. Auf der MPC: **Preferences → Enhancements (Beta) → Enable Enhancements** und
   **External Plugin Support** einschalten.
2. Den Ordner **`swtdr34mz - VST - PROPHETIC 6`** (aus dem Download `PROPHETIC6-MPC.zip`) auf einen USB-Stick, eine
   SD-Karte oder das interne Laufwerk der MPC kopieren, in einen Ordner namens **`VST2 Plugins`** ganz oben auf dem
   Laufwerk: `VST2 Plugins/swtdr34mz - VST - PROPHETIC 6/`.
3. Die MPC neu starten. PROPHETIC 6 steht dann im Plugin-Browser unter **External Plugins** (als Plugin-Programm
   laden).

Die MPC XL, Live III (64 Bit) und die 32-Bit-Geräte (X, X SE, Live, Live II, One, One+, Key 61, Key 37, Force)
nehmen denselben Ordner; jedes Gerät lädt seine eigene Datei darin.

## On the screen

The pages are drawn with the VST's own code: its knobs, buttons, LEDs, displays and section frames, laid out as on
the VST, just larger. Every control works as in the VST: turn a knob (drag it up and down, or its Q-Link; a double
tap opens the MPC's large editor, or the list for a choice), tap a button (POLY MOD's FREQ 1 / SHAPE 1 / PW 1, OSC 2's
SYNC, LOW FREQ and KEYBOARD, the filters' KEYBOARD, the VELOCITY buttons, RETRIG, ARP ON/OFF, HOLD, UNISON), tap an LED
to choose (the LFOs' shapes and destinations, AFTERTOUCH's DEST, the arp's rate, octaves and mode, QUALITY).

| Page | On it | The Q-Links (top row first) |
|---|---|---|
| QUICK | LOW-PASS FILTER, MIXER, FX MIX · VINTAGE & VOICE SPREAD, POLY MOD, ARPEGGIATOR · INIT (grey: the INIT program's sound), HOLD, GLIDE, UNISON, BEND RANGE, AFTERTOUCH, QUALITY, LOW SPLIT (SPLIT, POINT, LOW HOLD) | CUTOFF, RESONANCE, ENV AMOUNT, HP CUTOFF · OSC 1, OSC 2, SUB, NOISE · VINTAGE, VOICE SPREAD, REVERB, DELAY · CHORUS, DISTORT, ARP ON/OFF, QUALITY |
| OSC | OSCILLATOR 1, OSCILLATOR 2 · MIXER | OSC 1 · OSC 2 · MIXER |
| FLT/ENV | HIGH-PASS FILTER, FILTER ENVELOPE · LOW-PASS FILTER, AMPLIFIER ENVELOPE | HIGH-PASS · FILTER ENV · LOW-PASS · AMP ENV |
| LFO | POLY MOD · LFO 1 · LFO 2 | POLY MOD · LFO 1 · LFO 2 · their DESTs |
| FX | EFFECTS: SELECT and its LEDs choose CHORUS, REVERB, SPACE, DELAY or DLY MOD, as on the VST | CHORUS · REVERB · DELAY · DLY MOD and SYNC |

The tempo and the programs are the MPC's own (the factory programs are in its preset list); the keyboard is its pads.
LOW SPLIT's scales, KEY, FOLLOW and BALANCE are not on the pages (at their defaults: an octave split); they are in
the MPC's parameter list.

The 16 factory programs are the plugin's presets (the MPC's preset list); the MPC saves the sound with the project,
and presets of your own with its own preset save. Every value reads as on the FM-1 and in the VST.

**MIDI:** notes and velocity from the pads or keys, pitch bend (BEND RANGE), poly and channel pressure (AFTERTOUCH), CC 1 mod wheel (more LFO 1), CC 64 sustain, CC 74 cutoff, CC 120 / 121 / 123.

## CPU

The MPC is a small ARM computer, and this is a full analogue-style poly with five effects. **QUALITY** (on the QUICK page) chooses:

- **HIGH** (the default on the MPC XL and Live III): the low-pass at twice the rate, as in the VST: its full top end,
  no aliasing from its saturation.
- **ECO** (the default on the 32-bit MPCs): the low-pass at the rate: about 45 % less for the voices, a little less
  air at the very top of a bright, resonant sound.

As a guide, on a desktop computer seven voices of POLY PAD cost about 3.7 % of a core in HIGH and 2.0 % in ECO; on
the MPC expect several times that. Voices that have finished cost nothing, and effects turned to nought cost little.
If the MPC's CPU meter runs high, use ECO, fewer notes at once, or bounce the track.

## Not in this version

The FM-1's sequencer (the MPC's sequencer is the one to use), and the VST's own programs of your own (WRITE): the
MPC keeps presets with its own preset save.

## Build

See `mpc/build.py` in the repository: it makes the SDK project from the shared engine and runs the SDK's build. On
GitHub, every push builds it (Actions → the run → Artifacts → `PROPHETIC6-MPC`).

Third-party plugins run on the MPC without protection: a fault in one can stop the MPC. PROPHET 67's engine is
tested on every build (`tests/engine_tests.cpp`, `mpc/test/mpc_host_test.cpp`), but as with any new plugin, save
your projects before trying it.

Licence: MPL-2.0 (the PROPHET 67 code); the SDK's framework is MIT. Prophet-6 is a trademark of Sequential LLC, MPC
of inMusic Brands, VST of Steinberg Media Technologies GmbH; named only to describe.
