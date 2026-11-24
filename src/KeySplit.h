/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* LOW SPLIT on MIDI notes (the firmware's logic, app.c key_map), for the VST3 and the MPC plugin alike: below the
 * split point the notes go down SPLIT octaves (and to SCALE L), above it to SCALE R, rooted on KEY or (FOLLOW) on the
 * note held on the left, or (WHITE) the scale laid on the white keys; BALANCE between the two sides; LOW HOLD keeps
 * the left side's notes until a new one is played there. No JUCE: plain C++ on P67Engine. */
#pragma once
#include "P67Engine.h"

namespace p67split
{
struct Scale {
    const char *name;
    int n;
    int st[7];
};
inline const Scale SCALES[] = {
    {"MAJOR", 7, {0, 2, 4, 5, 7, 9, 11}},   {"MINOR", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"DORIAN", 7, {0, 2, 3, 5, 7, 9, 10}},  {"PHRYGIAN", 7, {0, 1, 3, 5, 7, 8, 10}},
    {"LYDIAN", 7, {0, 2, 4, 6, 7, 9, 11}},  {"MIXOLYD", 7, {0, 2, 4, 5, 7, 9, 10}},
    {"HARM MIN", 7, {0, 2, 3, 5, 7, 8, 11}}, {"PENTA MAJ", 5, {0, 2, 4, 7, 9}},
    {"PENTA MIN", 5, {0, 3, 5, 7, 10}},     {"BLUES", 6, {0, 3, 5, 6, 7, 10}},
    {"ARABIC", 7, {0, 1, 4, 5, 7, 8, 11}},
};
constexpr int NSC = (int)(sizeof SCALES / sizeof SCALES[0]);
inline const char *const NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
constexpr const char *SPLIT_NAME[4] = {"OFF", "-1 OCT", "-2 OCT", "-3 OCT"};
constexpr const char *FOLLOW_NAME[3] = {"KEY", "ROOT", "WHITE"};

inline bool isWhite(int n)
{
    int c = n % 12;
    return c == 0 || c == 2 || c == 4 || c == 5 || c == 7 || c == 9 || c == 11;
}

inline int snap(int n, int rootNote, int scale)   /* to the scale's note at or below n */
{
    const Scale &sc = SCALES[(scale - 1) % NSC];
    int rel = ((n - rootNote) % 12 + 12) % 12, best = 0;
    for (int i = 0; i < sc.n; i++)
        if (sc.st[i] <= rel)
            best = sc.st[i];
    return n - rel + best;
}

struct Settings {
    int split = 0, point = 60, scaleL = 0, scaleR = 0, key = 0, follow = 1, balance = 0;   /* scale 0: CHROMA */
    bool lowHold = false;
};

class KeySplit
{
public:
    Settings g;

    KeySplit() { reset(); }

    void reset()                         /* everything forgotten (prepare) */
    {
        forget(true);
        root = -1;
    }
    /* the notes let go elsewhere (the arp on or off: alsoHeld false; ALL NOTES / SOUND OFF: true) */
    void forget(bool alsoHeld)
    {
        for (int n = 0; n < 128; n++) {
            mapped[n] = -1;
            latched[n] = false;
            if (alsoHeld)
                held[n] = false;
        }
    }

    /* the settings for this block; LOW HOLD off lets go of what only it held */
    void set(P67Engine &e, const Settings &s)
    {
        bool wasHold = g.lowHold;
        g = s;
        if (wasHold && !g.lowHold)
            for (int i = 0; i < 128; i++)
                if (latched[i] && !held[i])
                    stopNote(e, i);
    }

    bool isLeft(int note) const { return g.split > 0 && note < g.point; }

    int keyMap(int note) const
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

    float zoneGain(int note) const
    {
        float b = (float)g.balance / 50.0f;
        if (g.split == 0)
            return 1.0f;
        return isLeft(note) ? (b > 0.0f ? 1.0f - b : 1.0f) : (b < 0.0f ? 1.0f + b : 1.0f);
    }

    void noteDown(P67Engine &e, int note, float vel)
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
                        stopNote(e, n);
        }
        if (mapped[note] >= 0)
            stopNote(e, note);
        held[note] = true;
        int m = keyMap(note);
        if (m < 0 || m > 127)
            return;
        mapped[note] = m;
        if (e.arpOn())
            e.arpKey(m, true, vel);
        else
            e.noteOn(note, m, vel, zoneGain(note));
        if (isLeft(note)) {
            if (g.lowHold)
                latched[note] = true;
            rootUpdate();
        }
    }

    void noteUp(P67Engine &e, int note)
    {
        held[note] = false;
        if (latched[note] && g.lowHold)
            return;                          /* LOW HOLD: it stays */
        stopNote(e, note);
        rootUpdate();
    }

private:
    int mapped[128];                     /* the note each incoming note plays (-1 none) */
    bool held[128] = {}, latched[128] = {};
    int root = -1;

    void rootUpdate()
    {
        for (int n = 0; n < 128; n++)
            if (isLeft(n) && mapped[n] >= 0) {
                root = mapped[n] % 12;
                return;
            }
    }

    void stopNote(P67Engine &e, int note)
    {
        if (mapped[note] >= 0) {
            if (e.arpOn())
                e.arpKey(mapped[note], false, 0.0f);
            else
                e.noteOff(note);
        }
        mapped[note] = -1;
        latched[note] = false;
    }
};
} // namespace p67split
