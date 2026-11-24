/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PROPHET 67) */
/* The VST's own presets, after the sixteen factory programs (src/vst_presets.c, made by tools/make_vst_presets.py). */
#pragma once
#include "engine.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NVST_PRESETS 50
extern const patch_t VST_PRESETS[NVST_PRESETS];
#define NVST_GROUPS 8
typedef struct {
    const char *name;
    int first;                           /* its first preset (VST_PRESETS) */
} vst_group_t;
extern const vst_group_t VST_GROUPS[NVST_GROUPS];   /* in order: brass, strings, pads, bass, leads, keys, plucks, effects */
#ifdef __cplusplus
}
#endif
