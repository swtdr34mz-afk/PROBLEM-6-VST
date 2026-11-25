/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
#pragma once
/* LOAD P6 SYSEX: programs from a Prophet-6 SysEx file (a program dump, a bank, or the whole 500), turned into PROPHET 67
 * programs as near as its panel allows. Nothing of the file is kept in the plugin: the programs are written as the
 * user's own files (PluginProcessor::importP6). No JUCE here, so the tests can use it as it is. */
extern "C" {
#include "engine.h"
}
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct P6Program {
    int number = 0;               /* the Prophet-6's: bank * 100 + program (0..499); an edit buffer: -1 */
    std::string name;             /* as on the Prophet-6 (up to 20 characters, trimmed) */
    uint8_t p[NPARAM] = {};       /* the PROPHET 67 program */
    int hpVel = 0, hpKt = 0, fenvVel = 0;   /* the plugin's own (kept with a user program) */
};

/* every program in the file (its program dumps F0 01 2D 02 .. and edit buffers F0 01 2D 03 ..), in the file's order */
std::vector<P6Program> p6ReadSysex(const uint8_t *data, size_t len);

/* one program's 1024 unpacked bytes, converted */
void p6Convert(const uint8_t *u, size_t n, P6Program &out);
