# PROPHET 67 — plugin

The [PROPHET 67](https://github.com/swtdr34mz-afk/PROPHET-67) polysynth for the M-VAVE FM-1, on the computer: a
**VST3** for Windows and the Mac, an **AU** for the Mac (Logic, GarageBand, and Ableton Live if you prefer it), and a
**standalone app** for both. By **swtdr34mz**.

The sound is the firmware's own engine — the same C code, built once per plugin instance — so a patch sounds the same
on the FM-1 and in the DAW: seven voices, POLY MOD, VINTAGE, VOICE SPREAD, the two LFOs, the reverbs (ROOM, HALL,
PLATE, SPRING, SPACE with shimmer and swell), the chorus and distortion, the stereo mod delay, aftertouch, the arp and
the LOW SPLIT.

Unofficial: not made, endorsed or supported by Sequential or M-VAVE.

## Download

[Actions](https://github.com/swtdr34mz-afk/PROPHET-67-VST/actions) → the newest green run → **Artifacts** at the
bottom: `PROPHET67-Windows` or `PROPHET67-macOS` (a GitHub sign-in is needed for artifacts). A release (a tag `v1.0`
and so on) carries the same zips under [Releases](https://github.com/swtdr34mz-afk/PROPHET-67-VST/releases).

## Installieren (kurz auf Deutsch)

**Windows (Cubase):**
1. Zip entpacken.
2. Den Ordner `PROPHET 67.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
3. Cubase starten (oder *Studio → VST-PlugIn-Manager → Aktualisieren*). PROPHET 67 steht unter den Instrumenten,
   Hersteller swtdr34mz.
4. `PROPHET 67.exe` ist die Standalone-App, sie läuft von überall. Audio- und MIDI-Gerät stellt man unter *Options*
   ein.

Windows SmartScreen kann beim ersten Start der App warnen (sie ist nicht signiert): *Weitere Informationen →
Trotzdem ausführen*.

**Mac (Ableton Live):**
1. Zip entpacken.
2. `PROPHET 67.vst3` nach `~/Library/Audio/Plug-Ins/VST3/` kopieren, `PROPHET 67.component` (AU) nach
   `~/Library/Audio/Plug-Ins/Components/`, die App `PROPHET 67.app` nach *Programme*.
   (Im Finder: *Gehe zu → Gehe zum Ordner …* und `~/Library/Audio/Plug-Ins` eingeben.)
3. Die Dateien sind nicht von Apple beglaubigt (notarisiert). Einmal im Terminal freigeben:
   ```
   xattr -cr ~/Library/Audio/Plug-Ins/VST3/"PROPHET 67.vst3"
   xattr -cr ~/Library/Audio/Plug-Ins/Components/"PROPHET 67.component"
   xattr -cr /Applications/"PROPHET 67.app"
   ```
4. Ableton: *Einstellungen → Plug-Ins → VST3-Plug-In-Systemordner verwenden: an* (für AU: *Audio Units: an*), dann
   *Neu scannen*. PROPHET 67 steht unter *Plug-Ins → swtdr34mz*.

Läuft auf Apple Silicon und Intel (macOS 10.13 und neuer), Windows 10/11 64-bit.

## In the plugin

- **All 74 sound parameters** are host parameters: automatable, named by section ("FILTER CUTOFF", "DELAY TIME L", …),
  shown in the firmware's units, saved with the project.
- **The 16 factory presets** are the host's programs (and the menu in the editor). A preset sets the sound's
  parameters, as on the FM-1; the plugin settings below stay.
- **WRITE** (the red key next to 9) keeps the sound as a program of your own: give it a name, and it comes after
  the factory programs (016, 017, …; TENS SELECT, the keys and the name display's menu reach them). They are files in
  `Documents/PROPHET 67/Presets` (one `.p67` each), shared by every project and the standalone app; the same name
  again replaces one, and deleting a file there removes it.
- **Plugin settings** (also automatable): ARP (on, MODE, RATE, OCTAVES, GATE, HOLD), SWING, LOW SPLIT (SPLIT, POINT,
  SCALE L, SCALE R, KEY, FOLLOW, LOW HOLD, BALANCE) and VOLUME.
- **The host's tempo** drives the arp, the synced LFOs and the synced delay; while the host plays, they lock to its
  bar position (an LFO on 1 BAR starts each bar on the bar).
- **MIDI in:** notes with velocity, poly aftertouch and channel pressure (to the AFTERTOUCH page's DEST), pitch bend
  (BEND semitones), CC 1 mod wheel (more LFO 1), CC 64 sustain, CC 74 cutoff, CC 120 / 123 all off.
- **LOW SPLIT** works on the MIDI notes as on the FM-1's keys: below POINT the left hand is transposed by SPLIT and
  snapped to SCALE L; above it SCALE R follows KEY, the left hand's ROOT, or lies on the WHITE keys; LOW HOLD keeps the
  left hand's chord; BALANCE sets the two hands' levels.
- **The panel** is laid out and drawn after the Prophet-6's: charcoal panel, white section frames, black knobs, buttons
  that light red, LED lists, red seven-segment displays (CLOCK shows the host's tempo; EFFECTS works as on the P6:
  SELECT an effect, its knobs appear, the lower display shows the value), the program strip (program display, TENS
  SELECT, 0-9, the program's name) and a four-octave keyboard with pitch and mod wheels. Under the name, the control
  in hand shows its name and value. Double-click a knob for its default, SHIFT-drag for fine. Drag the window's corner
  to size it (kept with the project).

**Not in this version:** the FM-1's 16-bar step sequencer — in a DAW the piano roll does that job; it may come later.
The FM-1's CPU limits do not apply, so all seven voices always play at full quality.

## Build

CMake 3.22 and a C++17 compiler (Visual Studio 2022, Xcode). JUCE 8.0.9 is fetched by CMake.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The results are under `build/Prophet67_artefacts/Release/`. GitHub Actions (`.github/workflows/build.yml`) builds
Windows and the Mac (universal) on every push.

`dsp/` is the firmware's `dsp/` with small `P67_PLUGIN` guards (any sample rate: the delay lines are scaled to it and
sized for up to 96 kHz; no second core; no RAM placement). `src/P67Engine.cpp` builds those C files as the members of one
C++ class, so each plugin instance has its own voices and effects. `shim/plat.h` stands in for the FM-1 platform.

## Licence

The PROPHET 67 code is under the [Mozilla Public License 2.0](LICENSE). The plugin is built with
[JUCE](https://juce.com) 8, used under the AGPLv3 (JUCE's open-source licence), so the plugin binaries are distributed
under the AGPLv3 with this source. Sequential and Prophet-6 are trademarks of Sequential LLC, named here only to
describe the inspiration. VST is a trademark of Steinberg Media Technologies GmbH.
