# Übergabe: PROPHETIC 6, das MPC-Plugin von PROPHET 67

Stand: 24.11.2026. Für eine neue Unterhaltung: dieses Protokoll anhängen oder einfügen, dazu das Repository
`swtdr34mz-afk/PROPHET-67-VST` verbinden.

## Worum es geht

- **PROPHET 67** ist Toms Synth-Plugin im Prophet-6-Look: VST3 für Windows/Cubase (und Mac), gebaut mit JUCE 8.0.9.
  Dieselbe Engine läuft auch als Firmware auf dem M-Vave FM-1.
- **PROPHETIC 6** ist dieselbe Engine als Plugin für Akai-MPCs (Tom hat eine **MPC XL** mit 16 Q-Links), gebaut mit
  Akais MPC Plugin SDK. Der Name ist bewusst anders, weil der Open-MPC-Katalog keine Herstellernamen im Plugin-Namen
  erlaubt.
- Autor/Hersteller überall: **swtdr34mz**. Toms Tag (SWTDR34MZ-Logo, `res/swtdr34mz-tag.png`) sitzt im VST rechts
  unten (statt des alten Badges) und auf jeder MPC-Seite rechts unten.
- Ziel: PROPHETIC 6 im **OpenMPC Plugin Manager** (Katalog `open-mpc/mpc-vst-plugins`).

## Stand

- `main` enthält alles: Audit/Optimierung der Engine (PR #1), MPC-Plugin mit Skin (PR #2). PR #3 (CI hängt die Zips an
  ein auf GitHub veröffentlichtes Release) ist offen/gerade gemerged.
- Tom hat PROPHET 67 bereits auf der MPC laufen gehabt (alte Version). Die neue Version mit den neuen Seiten ist auf
  der MPC **noch nicht getestet**.
- CI (`.github/workflows/build.yml`): Engine-Tests, MPC-Build (SDK gepinnt auf `a5949cf6fe2c…`, arm32 + arm64 +
  native), Akais Validator (zuletzt **0 Fehler, 0 Warnungen**), eigener Host-Test (16 Checks), Windows-Build mit
  pluginval (Strictness 8). Artifact `PROPHETIC6-MPC` = der fertige MPC-Ordner.

## Das MPC-Plugin

- Ordner auf der MPC: `VST2 Plugins/swtdr34mz - VST - PROPHETIC 6/` (den alten `… - PROPHET 67` löschen; neue uid
  `Pr6c`, Dateien `VST/prophetic-6-arm32.so` / `-arm64.so`).
- Code: `mpc/plugin/src/Prophet67Mpc.cpp` (SDK-Plugin), Engine geteilt mit dem VST (`src/P67Engine.*`, `dsp/`),
  LOW SPLIT geteilt über `src/KeySplit.h`. `mpc/build.py` baut das SDK-Projekt aus dem Repo.
- **94 Parameter, Reihenfolge fest** (neue nur hinten anhängen): 0 Volume; 1..74 die Sound-Parameter (`p0..p73`);
  75..77 HP Velocity, HP Keyboard, FEnv Velocity; 78..84 Arp On, Mode, Rate, Octaves, Gate, Hold, Swing; 85 Quality
  (HIGH/ECO); 86..93 LOW SPLIT (Split, Point, Scale L, Scale R, Key, Follow, Low Hold, Balance).
- 16 Werksprogramme. Tempo und Programme kommen von der MPC.

## Die Oberfläche (Skin)

- Gezeichnet mit dem **Code des VST** (Potis, Buttons, LEDs, Displays, Sektionsrahmen aus `src/ui/`), Sektionen
  innen wie im VST, nur größer. Keine Tastatur, keine Schieberegler.
- Werkzeuge: `mpc/skin/render_skin.cpp` (JUCE, zeichnet die Seiten und schneidet jeden Regler in jedem Wert aus) und
  `mpc/skin/make_skin.py` (macht daraus `mpc/plugin/skins/`: TUI.json, Q-Links.json, PNGs). Das Ergebnis liegt im
  Repo; neu erzeugen nur, wenn sich Seiten ändern (Anleitung oben in `mpc/skin/CMakeLists.txt`; Pillow für die
  256-Farben-Komprimierung).
- Bedienung wie im VST: Potis ziehen oder per Q-Link, Doppeltipp = großes Werte-Fenster; Buttons per Tipp nach den
  VST-Regeln; LED-Listen per Tipp; FX: SELECT und LEDs wählen den Effekt.

| Seite | Inhalt (Zeilen) | Q-Links (Reihe oben → unten; 13–16 ist oben) |
|---|---|---|
| QUICK | LPF, MIXER, FX MIX · VINTAGE & SPREAD, POLY MOD, ARP · HOLD/GLIDE/UNISON/BEND, AFTERTOUCH, QUALITY, LOW SPLIT (nur SPLIT, POINT, LOW HOLD) | CUTOFF, RES, ENV AMOUNT, HPF CUTOFF · OSC1, OSC2, SUB, NOISE · VINTAGE, SPREAD, REVERB, DELAY · CHORUS, DISTORT, ARP ON/OFF, QUALITY |
| OSC | OSC 1, OSC 2 · MIXER | OSC 1 · OSC 2 · MIXER |
| FLT/ENV | HPF, FILTER ENV · LPF, AMP ENV | HPF · FILTER ENV · LPF · AMP ENV |
| LFO | POLY MOD, LFO 1, LFO 2 | POLY MOD · LFO 1 · LFO 2 · beide DEST |
| FX | EFFECTS (alle 5 Effekte inkl. DELAY und DLY MOD, wie im VST) | CHORUS · REVERB · DELAY · DLY MOD + SYNC |

Offene Kleinigkeiten: SPACE (4 Regler) hat keine Q-Links; SWING (aus CLOCK) ist auf keiner Seite; LOW SPLITs
Scales/Key/Follow/Balance nur in der Parameterliste.

## Release und Katalog: nächste Schritte

1. **Release:** Auf GitHub → Releases → „Draft a new release“ → neuer Tag **`v1.0.0`** auf `main` → veröffentlichen
   (Titel frei, z. B. „PROPHET 67 1.0.0 / PROPHETIC 6 für die MPC“). Die CI hängt die Windows-/Mac-Zips und
   **`PROPHETIC-6-1.0.0-mpc-sdk.zip`** an und schreibt dessen SHA-256 ins Log. (Tags und Releases kann Claude aus
   der Cloud-Sitzung nicht anlegen.)
2. **Katalog-PR** (mit Toms GitHub-Konto): `open-mpc/mpc-vst-plugins` forken → Branch → Datei
   `catalog/plugins/prophetic-6.json` = `mpc/catalog/prophetic-6.json` aus diesem Repo (kein `asset_pattern`: der
   Katalog wandelt das SDK-Zip selbst um). Screenshot-URL zeigt auf `mpc/catalog/screenshot.png` am Tag `v1.0.0`
   (Screenshot ggf. auf die neue QUICK-Seite aktualisieren, **vor** dem Release). PR-Vorlage ausfüllen: Ausgabe von
   `tools/catalog_check.py <zip> --catalog` und SHA-256 des Zips, „offline only“ bzw. Gerätetest angeben.
3. **Gerätetest** auf der MPC XL, dann `tested.json` im Repo (Gerät, MPC-OS-Version, Datum) → Stufe „Verified“.
4. Branding-Hinweis: „PROPHETIC 6“ ist nah an „Prophet-6“; die Reviewer könnten nachfragen. Kein Hersteller-Logo im
   Skin (geprüft), Summary sagt „Prophet-6-style“.

## Arbeitsregeln (wie bisher)

- Auf Branches arbeiten, PRs, kein direkter Push auf `main`.
- Keine Binaries Dritter lokal ausführen; Akais SDK-Build und pluginval laufen nur in der CI.
- Vor öffentlichen oder nicht umkehrbaren Schritten Tom fragen.
