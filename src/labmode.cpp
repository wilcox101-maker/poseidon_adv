/*
 * labmode — guided educational exercises. See labmode.h.
 *
 * The bundled lessons are PLACEHOLDER content to exercise the UI; the real
 * curriculum is a product decision (TBD).
 */
#include "labmode.h"
#include <string.h>

typedef struct { const char *text; lab_event_t trig; } lab_step_t;
typedef struct { const char *title; const lab_step_t *steps; int n; } lab_lesson_t;

static const lab_step_t L1[] = {
    { "Start a WiFi scan",         LAB_EV_SCAN_STARTED },
    { "Select a target AP",        LAB_EV_AP_SELECTED },
    { "Capture a 4-way handshake", LAB_EV_HANDSHAKE_CAPTURED },
    { "Save the capture to SD",    LAB_EV_SAVED_TO_SD },
};
static const lab_step_t L2[] = {
    { "Start a WiFi scan",      LAB_EV_SCAN_STARTED },
    { "Run PMKID capture",      LAB_EV_PMKID_CAPTURED },
    { "Save the capture to SD", LAB_EV_SAVED_TO_SD },
};

static const lab_lesson_t LESSONS[] = {
    { "WPA handshake 101", L1, (int)(sizeof(L1) / sizeof(L1[0])) },
    { "PMKID in the wild", L2, (int)(sizeof(L2) / sizeof(L2[0])) },
};
static const int LESSON_N = (int)(sizeof(LESSONS) / sizeof(LESSONS[0]));

static int  s_cur = 0;
static bool s_done[LAB_MAX_STEPS];

int labmode_lesson_count(void) { return LESSON_N; }
int labmode_current(void)      { return s_cur; }

const char *labmode_lesson_title(int l)
{
    if (l < 0 || l >= LESSON_N) return "";
    return LESSONS[l].title;
}

void labmode_select(int l)
{
    if (l < 0)          l = 0;
    if (l >= LESSON_N)  l = LESSON_N - 1;
    s_cur = l;
    memset(s_done, 0, sizeof(s_done));
}

int labmode_step_count(void) { return LESSONS[s_cur].n; }

const char *labmode_step_text(int i)
{
    if (i < 0 || i >= LESSONS[s_cur].n) return "";
    return LESSONS[s_cur].steps[i].text;
}

bool labmode_step_done(int i)
{
    if (i < 0 || i >= LAB_MAX_STEPS) return false;
    return s_done[i];
}

int labmode_steps_done(void)
{
    int c = 0;
    for (int i = 0; i < LESSONS[s_cur].n; ++i) if (s_done[i]) c++;
    return c;
}

void labmode_mark(int i, bool done)
{
    if (i < 0 || i >= LAB_MAX_STEPS) return;
    s_done[i] = done;
}

bool labmode_notify(lab_event_t ev)
{
    if (ev == LAB_EV_NONE) return false;
    for (int i = 0; i < LESSONS[s_cur].n; ++i) {
        if (!s_done[i] && LESSONS[s_cur].steps[i].trig == ev) {
            s_done[i] = true;
            return true;
        }
    }
    return false;
}

void labmode_reset(void) { memset(s_done, 0, sizeof(s_done)); }
