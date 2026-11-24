/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The tables mpc/gen/gen_data.cpp makes at build time (p67_data.cpp): PARAMS, FACTORY and the sync tables for the
 * engine (declared in engine.h), and every parameter's every value as the firmware shows it. */
#pragma once
#include <cstdint>
extern "C" {
#include "engine.h"
}

#define P67_TEXT_LEN 10
extern const char P67_TEXT[NPARAM][128][P67_TEXT_LEN];
