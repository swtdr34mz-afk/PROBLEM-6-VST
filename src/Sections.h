/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The sections of the 74 parameters, as on the FM-1's pages (dsp/engine.h, groups of four). */
#pragma once

inline const char *sectionOf(int i)
{
    static const struct {
        int first;
        const char *name;
    } S[] = {
        {0, "OSC 1"},       {4, "OSC 2"},       {8, "MIXER"},      {12, "FILTER"},    {16, "FILTER 2"},
        {20, "FILTER ENV"}, {24, "AMP ENV"},    {28, "LFO 1"},     {32, "POLY MOD"},  {36, "CHORUS"},
        {40, "REVERB"},     {44, "SPACE"},      {48, "VOICE"},     {52, "VINTAGE"},   {53, "SPREAD"},
        {54, "LFO 2"},      {58, "LFO SYNC"},   {62, "AFTERTOUCH"}, {66, "DELAY"},    {70, "DELAY MOD"},
    };
    const char *n = S[0].name;
    for (const auto &s : S)
        if (i >= s.first)
            n = s.name;
    return n;
}
