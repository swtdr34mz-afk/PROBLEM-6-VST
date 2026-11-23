/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The plugin's stand-in for fm1-base's platform (include/plat.h): what the engine (dsp/) asks of it. On a computer
 * the engine runs in the host's audio thread, on one core, with no flash and no second core. */
#pragma once
#include <stdint.h>

#define PLAT_POOL
#define PLAT_FAR(fn) fn
#define PLAT_TICKS_PER_US 24u

static inline uint32_t plat_lock(void) { return 0; }    /* (every engine call is made on the audio thread) */
static inline void plat_unlock(uint32_t f) { (void)f; }
static inline int plat_cpu2_on(void) { return 0; }
#define plat_cpu2_run(fn, arg) 0
static inline int plat_cpu2_wait(void) { return 0; }
static inline uint32_t plat_ticks24(void) { return 0; }  /* (the load meter: not on a computer) */
