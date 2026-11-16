/*
 * theme.h — curated palettes; POSEIDON uses the Deepwater visual system.
 *
 *   POSEIDON cyberpunk: cyan / magenta / purple on black with strategic
 *                       magenta splashes. The default. Designed for
 *                       readability (T_DIM is medium ice-cyan, not grey).
 *   MATRIX hacker:      bright phosphor green on pure black. Souped-up
 *                       cinematic rain in the ambient layer.
 *   E-INK paper:        black on white. Daytime / outdoor / minimal.
 *                       Ambient is a no-op so the page stays clean.
 *   SYNTHWAVE smooth:   vaporwave on midnight grape bg — hot magenta +
 *                       cyan, pastel highlights. 80s night-sky vibe;
 *                       distinct from POSEIDON because bg is purple,
 *                       not black.
 *   PHANTOM violet:     deep violet / lavender — visually cohesive with
 *                       the user's phantom-* suite (launcher, rf, mobile).
 *   BLOOD tactical:     Mr Robot fsociety red-on-black. Aggressive,
 *                       urgent; reads as "device is actively attacking".
 *   AMBER CRT:          80s phosphor amber terminal.
 *   NIGHT CITY:         vivid cyberpunk electric yellow + cyan.
 *   SOLARIS:            solar gold & warm plasma on charcoal.
 *   NORDIC FROST:       glacial ice-cyan & silver on arctic navy.
 *   GHOST PURPLE:       neon electric purple on obsidian.
 */
#pragma once

#include <Arduino.h>

struct poseidon_theme_t {
    const char *name;
    uint16_t bg;          /* main background */
    uint16_t fg;          /* primary text */
    uint16_t accent;      /* titles, highlights */
    uint16_t accent2;     /* secondary accent (borders, rules) */
    uint16_t warn;        /* warnings */
    uint16_t bad;         /* errors, attacks */
    uint16_t good;        /* success, captures */
    uint16_t dim;         /* muted text, hints */
    uint16_t sel_bg;      /* selected row background */
    uint16_t sel_border;  /* selected row border */
    uint16_t status_bg;   /* status bar gradient top */
    uint16_t status_bg2;  /* status bar gradient bottom */
    uint16_t footer_bg;   /* footer gradient */
    uint16_t rule;        /* divider lines */
};

enum theme_id_t {
    THEME_POSEIDON = 0,   /* Deepwater navy / ice / cyan — default */
    THEME_MATRIX,         /* souped-up hacker green-on-black */
    THEME_EINK,           /* paper white, daytime / minimal */
    THEME_SYNTHWAVE,      /* vaporwave magenta + cyan on midnight grape */
    THEME_PHANTOM,        /* deep violet — matches phantom-* suite */
    THEME_BLOOD,          /* fsociety red on pure black */
    THEME_AMBER_CRT,      /* 80s phosphor amber terminal */
    THEME_NIGHT_CITY,     /* vivid cyberpunk electric yellow + cyan */
    THEME_SOLARIS,        /* solar gold & warm plasma on charcoal */
    THEME_NORDIC_FROST,   /* glacial ice-cyan & silver on arctic navy */
    THEME_GHOST_PURPLE,   /* neon electric purple on obsidian */
    THEME__COUNT
};

void theme_init(void);               /* load from NVS on boot */
bool theme_set(theme_id_t id);       /* apply + persist; false on storage failure */
void theme_preview(theme_id_t id);   /* apply in RAM only — no NVS write */
theme_id_t theme_current_id(void);
const poseidon_theme_t &theme(void);

/* Convenience — replaces COL_* macros. */
#define T_BG       (theme().bg)
#define T_FG       (theme().fg)
#define T_ACCENT   (theme().accent)
#define T_ACCENT2  (theme().accent2)
#define T_WARN     (theme().warn)
#define T_BAD      (theme().bad)
#define T_GOOD     (theme().good)
#define T_DIM      (theme().dim)
#define T_SEL_BG   (theme().sel_bg)
#define T_SEL_BD   (theme().sel_border)
