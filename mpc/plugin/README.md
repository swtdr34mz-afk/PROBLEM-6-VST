# PROPHET 67 for the MPC

The PROPHET 67 synth as a plugin for Akai MPC standalone hardware: the same engine as the PROPHET 67 VST3 and the
M-Vave FM-1 firmware, built with Akai's MPC Plugin SDK. A 7-voice analogue-style poly after the Sequential
Prophet-6: two oscillators (triangle - saw - pulse, sub octave, noise, hard sync, LOW FREQ), poly mod, a resonant
high-pass into a creamy 4-pole low-pass, filter and amp envelopes, VINTAGE and SPREAD, two LFOs (LFO 1 into the
audio range; on S+H at the top, the Prophet-6's noise), distortion, chorus, delay, five reverbs (ROOM, HALL, PLATE,
SPRING, SPACE) and an arpeggiator on the MPC's tempo.

## Installieren (kurz auf Deutsch)

1. MPC OS **3.10.1 oder neuer**. Auf der MPC: **Preferences → Enhancements (Beta) → Enable Enhancements** und
   **External Plugin Support** einschalten.
2. Den Ordner **`swtdr34mz - VST - PROPHET 67`** (aus dem Download `PROPHET67-MPC.zip`) auf einen USB-Stick, eine
   SD-Karte oder das interne Laufwerk der MPC kopieren, in einen Ordner namens **`VST2 Plugins`** ganz oben auf dem
   Laufwerk: `VST2 Plugins/swtdr34mz - VST - PROPHET 67/`.
3. Die MPC neu starten. PROPHET 67 steht dann im Plugin-Browser unter **External Plugins** (als Plugin-Programm
   laden).

Die MPC XL, Live III (64 Bit) und die 32-Bit-Geräte (X, X SE, Live, Live II, One, One+, Key 61, Key 37, Force)
nehmen denselben Ordner; jedes Gerät lädt seine eigene Datei darin.

## On the screen

Six pages (the MPC's six function keys), each in rows of four (one Q-Link group each):

| Page | Rows |
|---|---|
| OSC | oscillator 1 (FREQ, SHAPE, PW, SUB) · oscillator 2 (FREQ, FINE, SHAPE, PW) · the mixer (OSC 1, OSC 2, NOISE, OSC 2 MODE) · GLIDE, UNISON, BEND RANGE, AMP VELOCITY |
| FILTER | low-pass (CUTOFF, RESONANCE, ENV AMT, KEYBOARD) · high-pass (CUTOFF, RESO, ENV AMT, KEYBOARD) · the velocities · the filter envelope |
| AMP | the amp envelope · VINTAGE, SPREAD, VOLUME, QUALITY · aftertouch (DEST, AMOUNT, CURVE, SMOOTH) |
| LFO+PM | LFO 1 · both LFOs' SYNC and RETRIG · LFO 2 · poly mod |
| FX | distortion and chorus · the reverb · SPACE's own (shown on SPACE) |
| DLY+ARP | the delay's times, REPEATS, MIX · its modulation, TONE, SYNC · the arp (on, MODE, RATE, OCTAVES · GATE, HOLD, SWING) |

The 16 factory programs are the plugin's presets (the MPC's preset list); the MPC saves the sound with the project,
and presets of your own with its own preset save. Every value reads as on the FM-1 and in the VST.

**MIDI:** notes and velocity from the pads or keys, pitch bend (BEND RANGE), poly and channel pressure (the
AFTERTOUCH page), CC 1 mod wheel (more LFO 1), CC 64 sustain, CC 74 cutoff, CC 120 / 121 / 123.

## CPU

The MPC is a small ARM computer, and this is a full analogue-style poly with five effects. **QUALITY** chooses:

- **HIGH** (the default on the MPC XL and Live III): the low-pass at twice the rate, as in the VST: its full top end,
  no aliasing from its saturation.
- **ECO** (the default on the 32-bit MPCs): the low-pass at the rate: about 45 % less for the voices, a little less
  air at the very top of a bright, resonant sound.

As a guide, on a desktop computer seven voices of POLY PAD cost about 3.7 % of a core in HIGH and 2.0 % in ECO; on
the MPC expect several times that. Voices that have finished cost nothing, and effects turned to nought cost little.
If the MPC's CPU meter runs high, use ECO, fewer notes at once, or bounce the track.

## Not in this version

The LOW SPLIT and the scales of the VST (the MPC's pads and keyboard modes do that job), the VST's panel (the MPC
draws its own screen from `Plugin Skins/`), and the FM-1's sequencer (the MPC's sequencer is the one to use).

## Build

See `mpc/build.py` in the repository: it makes the SDK project from the shared engine and runs the SDK's build. On
GitHub, every push builds it (Actions → the run → Artifacts → `PROPHET67-MPC`).

Third-party plugins run on the MPC without protection: a fault in one can stop the MPC. PROPHET 67's engine is
tested on every build (`tests/engine_tests.cpp`, `mpc/test/mpc_host_test.cpp`), but as with any new plugin, save
your projects before trying it.

Licence: MPL-2.0 (the PROPHET 67 code); the SDK's framework is MIT. Prophet-6 is a trademark of Sequential LLC, MPC
of inMusic Brands, VST of Steinberg Media Technologies GmbH; named only to describe.
