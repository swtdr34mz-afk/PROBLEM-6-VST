# Übergabe: PROPHETIC 6, das MPC-Plugin von PROPHET 67


> **26.11.2026: umbenannt in PROBLEM 6** (wegen der Marke „Prophet“ von Sequential). Ordner auf der MPC jetzt
> `swtdr34mz - VST - PROBLEM 6`, Dateien `VST/problem-6-arm32.so` / `-arm64.so`, Release-Zips `PROBLEM-6-<v>-mpc-*.zip`.
> **Gleich geblieben:** die uid `Pr6c` und die Katalog-id `prophetic-6` (eine id darf sich nie ändern). Die Katalog-Zips
> räumen den alten Ordner `swtdr34mz - VST - PROPHETIC 6` beim Installieren weg (`--retire`). Danach im Katalog
> `catalog/plugins/prophetic-6.json` = `mpc/catalog/prophetic-6.json` (neuer Name, `asset_pattern` `PROBLEM-6-*`).
> Weiter unten steht noch der alte Name: gemeint ist dasselbe Plugin.
Stand: 24.11.2026, Mittag. Für eine neue Unterhaltung: dieses Protokoll anhängen oder einfügen, dazu das Repository
`swtdr34mz-afk/PROBLEM-6-VST` verbinden.

## Worum es geht

- **PROPHET 67** ist Toms Synth-Plugin im Prophet-6-Look: VST3 für Windows/Cubase (und Mac), gebaut mit JUCE 8.0.9.
  Dieselbe Engine läuft auch als Firmware auf dem M-Vave FM-1.
- **PROPHETIC 6** ist dieselbe Engine als Plugin für Akai-MPCs (Tom hat eine **MPC XL** mit 16 Q-Links), gebaut mit
  Akais MPC Plugin SDK. Der Name ist bewusst anders, weil der Open-MPC-Katalog keine Herstellernamen im Plugin-Namen
  erlaubt.
- Autor/Hersteller überall: **swtdr34mz**. Toms Tag (SWTDR34MZ-Logo, `res/swtdr34mz-tag.png`) sitzt im VST rechts
  unten (statt des alten Badges) und auf jeder MPC-Seite rechts unten.
- Ziel: PROPHETIC 6 im **OpenMPC Plugin Manager** (Katalog `open-mpc/mpc-vst-plugins`).

## Stand (Mittag, 24.11.)

- **Release `v1.0.0` ist veröffentlicht** (PR #3 gemergt). MPC-Zip `PROPHETIC-6-1.0.0-mpc-sdk.zip`,
  SHA-256 `4677a096bf0d11c89acb801c63c2ab2ee6dd4afe7d1ed72c199e16f0fce00644`.
- **Katalog-PR ist offen:** open-mpc/mpc-vst-plugins **#1158** („catalog: add PROPHETIC 6“), aus Toms Fork
  `swtdr34mz-afk/PROPHETIC-6`, Branch `add-prophetic-6`. Wartet auf die Betreuer (Checks müssen sie erst freigeben).
  `catalog_check.py` wurde nicht lokal ausgeführt; falls die Reviewer die Ausgabe wollen, nachreichen.
- **Fertig, aber noch nicht im Repo:** QUICK heißt jetzt **HOME**; LFO-Seite nur noch LFO 1 + LFO 2 (POLY MOD ist auf
  HOME), dadurch Maßstab 2. Tom hat das Bild abgenommen. Liegt als Patch `0001-…patch` bei: auf einem Branch mit
  `git am` einspielen, PR, danach Release `v1.0.1` (der Katalog nimmt neue Releases selbst auf).

## Stand davor

- `main` enthält: Audit/Optimierung der Engine (PR #1), MPC-Plugin mit Skin (PR #2). **PR #3 muss Tom noch mergen**
  (Claude darf es nicht): CI hängt die Zips an ein auf GitHub veröffentlichtes Release, LFO-Seite übereinander,
  grauer INIT-Schalter, `tested.json`, dieses Protokoll.
- **v1.0.4 auf der MPC XL (MPC OS 3.10.1) getestet, 25.11.2026: funktioniert** (vorher 1.0.0 am 24.11.: „spielt sich hervorragend“) (Stand vor INIT und der neuen
  LFO-Seite; die nach dem Release kurz nachprüfen). Eingetragen in `tested.json` (Gen2).
- CI (`.github/workflows/build.yml`): Engine-Tests, MPC-Build (SDK gepinnt auf `a5949cf6fe2c…`, arm32 + arm64 +
  native), Akais Validator (zuletzt **0 Fehler, 0 Warnungen**), eigener Host-Test (16 Checks), Windows-Build mit
  pluginval (Strictness 8). Artifact `PROPHETIC6-MPC` = der fertige MPC-Ordner.

## Das MPC-Plugin

- Ordner auf der MPC: `VST2 Plugins/swtdr34mz - VST - PROPHETIC 6/` (den alten `… - PROPHET 67` löschen; neue uid
  `Pr6c`, Dateien `VST/prophetic-6-arm32.so` / `-arm64.so`).
- Code: `mpc/plugin/src/Prophet67Mpc.cpp` (SDK-Plugin), Engine geteilt mit dem VST (`src/P67Engine.*`, `dsp/`),
  LOW SPLIT geteilt über `src/KeySplit.h`. `mpc/build.py` baut das SDK-Projekt aus dem Repo.
- **95 Parameter, Reihenfolge fest** (neue nur hinten anhängen): 0 Volume; 1..74 die Sound-Parameter (`p0..p73`);
  75..77 HP Velocity, HP Keyboard, FEnv Velocity; 78..84 Arp On, Mode, Rate, Octaves, Gate, Hold, Swing; 85 Quality
  (HIGH/ECO); 86..93 LOW SPLIT (Split, Point, Scale L, Scale R, Key, Follow, Low Hold, Balance).
- 95 mit **94 INIT**: ein Druck (grauer Schalter auf HOME) setzt den Klang auf das INIT-Programm (Arp, Split,
  QUALITY, VOLUME bleiben), dann geht INIT wieder auf aus.
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
| HOME (früher QUICK) | LPF, MIXER, FX MIX · VINTAGE & SPREAD, POLY MOD, ARP · INIT (grau)/HOLD/GLIDE/UNISON/BEND, AFTERTOUCH, QUALITY, LOW SPLIT (nur SPLIT, POINT, LOW HOLD) | CUTOFF, RES, ENV AMOUNT, HPF CUTOFF · OSC1, OSC2, SUB, NOISE · VINTAGE, SPREAD, REVERB, DELAY · CHORUS, DISTORT, ARP ON/OFF, QUALITY |
| OSC | OSC 1, OSC 2 · MIXER | OSC 1 · OSC 2 · MIXER |
| FLT/ENV | HPF, FILTER ENV · LPF, AMP ENV | HPF · FILTER ENV · LPF · AMP ENV |
| LFO | LFO 1 · LFO 2 (übereinander, Maßstab 2; POLY MOD nur auf HOME) | LFO 1 · LFO 2 · beide DEST + RETRIG |
| FX | EFFECTS (alle 5 Effekte inkl. DELAY und DLY MOD, wie im VST) | CHORUS · REVERB · DELAY · DLY MOD + SYNC |

Offene Kleinigkeiten: SPACE (4 Regler) hat keine Q-Links; SWING (aus CLOCK) ist auf keiner Seite; LOW SPLITs
Scales/Key/Follow/Balance nur in der Parameterliste.

## Touch-Bedienung (25.11.2026, nach Toms Test auf der MPC XL)

- QUALITY: ein Button (HIGH / ECO, LEDs darüber) statt nur der LED-Liste.
- LOW SPLIT: POINT zeigt die Note in MPC-Schreibweise (C1 = 36, also 60 = C3; Standard bleibt 60 = C3). Das VST nennt
  dieselbe Note C4.
- LFO: je LFO ein WAVE- und ein DEST-Button, die die LEDs durchschalten; die Abschnitte fast seitenbreit.
- FX: statt der roten Siebensegment-Anzeige ein ruhiges, scharfes Display (Effektname ausgeschrieben, Wert als Zahl bzw.
  Name des Reverb-Typs). Nur auf der MPC, das VST bleibt rot.
- `mpc/skin/CMakeLists.txt` kennt jetzt auch `src/P6Import.cpp` (sonst baut der Skin-Renderer nicht mehr).

## Release und Katalog: nächste Schritte

0. PR #3 mergen (GitHub → Pull requests → #3 → Merge).
1. **Release:** Auf GitHub → Releases → „Draft a new release“ → neuer Tag **`v1.0.0`** auf `main` → veröffentlichen
   (Titel frei, z. B. „PROPHET 67 1.0.0 / PROPHETIC 6 für die MPC“). Die CI hängt die Windows-/Mac-Zips und
   **`PROPHETIC-6-1.0.0-mpc-sdk.zip`** an und schreibt dessen SHA-256 ins Log. (Tags und Releases kann Claude aus
   der Cloud-Sitzung nicht anlegen.)
2. **Katalog-PR** (mit Toms GitHub-Konto): `open-mpc/mpc-vst-plugins` forken → Branch → Datei
   `catalog/plugins/prophetic-6.json` = `mpc/catalog/prophetic-6.json` aus diesem Repo (kein `asset_pattern`: der
   Katalog wandelt das SDK-Zip selbst um). Screenshot-URL zeigt auf `mpc/catalog/screenshot.png` am Tag `v1.0.0`
   (Screenshot ggf. auf die neue HOME-Seite aktualisieren, **vor** dem Release). PR-Vorlage ausfüllen: Ausgabe von
   `tools/catalog_check.py <zip> --catalog` und SHA-256 des Zips, „offline only“ bzw. Gerätetest angeben.
3. **Gerätetest:** `tested.json` ist eingetragen (MPC XL, Gen2, OS 3.10.1) → nach dem Release „Verified Gen2“. Weitere
   Geräte dort ergänzen.
4. Branding-Hinweis: „PROPHETIC 6“ ist nah an „Prophet-6“; die Reviewer könnten nachfragen. Kein Hersteller-Logo im
   Skin (geprüft), Summary sagt „Prophet-6-style“.

## Neue Version und Katalog-Zips

- Die MPC-Version kommt beim Release **aus dem Tag** (die CI schreibt sie in `mpc/plugin/plugin.json`). v1.0.2 und
  v1.0.3 hatten noch kein MPC-Zip, weil plugin.json nicht passte.
- Die CI macht mit den Werkzeugen des Katalogs (`open-mpc/mpc-vst-plugins` gepinnt auf `aa81004…`, `tools/sdk_release.py`)
  `PROPHETIC-6-<v>-mpc-armv7.zip` und `-mpc-aarch64.zip`, prüft sie mit `catalog_check.py --catalog` und schreibt
  Ausgabe + SHA-256 in die Job-Zusammenfassung des `mpc`-Jobs. Ans Release kommen beide plus `-mpc-sdk.zip`.
- **Im Katalog seit 25.11.2026** (open-mpc/mpc-vst-plugins#1158 gemergt). Bei jedem Release: neuen Gerätetest in
  `tested.json` eintragen (nur wenn wirklich auf der MPC getestet) und den Screenshot-Pin in `catalog/plugins/prophetic-6.json`
  des Katalogs auf den neuen Tag ziehen (eigener kleiner PR dort).
- Katalog-Eintrag: `asset_pattern` = `PROPHETIC-6-*-mpc-armv7.zip` (das aarch64-Zip findet der Katalog daneben).
- Review in #1158 (24.11. abends): Summary mit „A port of the PROPHET 67 firmware for the M-VAVE FM-1“, asset_pattern,
  catalog_check-Ausgabe + SHA-256 im PR. Der Name PROPHETIC 6 darf bleiben (Korrektur der Betreuer).

## Arbeitsregeln (wie bisher)

- Auf Branches arbeiten, PRs, kein direkter Push auf `main`.
- Keine Binaries Dritter lokal ausführen; Akais SDK-Build und pluginval laufen nur in der CI.
- Vor öffentlichen oder nicht umkehrbaren Schritten Tom fragen.
