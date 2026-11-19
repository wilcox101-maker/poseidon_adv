/*
 * gz_face_subjects.cpp — see gz_face_subjects.h. Firmware-only (LVGL observer).
 *
 * A guard flag keeps the setters safe to call before gz_subjects_init() runs
 * on the UI task: a producer tick that fires during bring-up is dropped rather
 * than writing an uninitialised subject.
 */
#include "gz_face_subjects.h"

static gz_subjects_t s_subj;
static bool          s_ready = false;

gz_subjects_t *gz_subjects(void) { return &s_subj; }

void gz_subjects_init(void)
{
    lv_subject_init_int(&s_subj.heap_kb,   0);
    lv_subject_init_int(&s_subj.psram_kb,  0);
    lv_subject_init_int(&s_subj.uptime_s,  0);
    lv_subject_init_int(&s_subj.ble_sent,  0);
    lv_subject_init_int(&s_subj.wifi_sent, 0);
    lv_subject_init_int(&s_subj.pub_age_s, -1);
    lv_subject_init_int(&s_subj.channel,   0);
    lv_subject_init_int(&s_subj.contacts,  0);
    lv_subject_init_int(&s_subj.rssi_best, 0);
    lv_subject_init_int(&s_subj.imu_tilt,  0);
    lv_subject_init_int(&s_subj.batt_pct,  -1);
    lv_subject_init_int(&s_subj.recon_on,  0);
    s_ready = true;
}

static inline void set(lv_subject_t *subj, int v)
{
    if (s_ready) lv_subject_set_int(subj, (int32_t)v);
}

void gz_subj_set_heap_kb(int v)   { set(&s_subj.heap_kb,   v); }
void gz_subj_set_psram_kb(int v)  { set(&s_subj.psram_kb,  v); }
void gz_subj_set_uptime_s(int v)  { set(&s_subj.uptime_s,  v); }
void gz_subj_set_ble_sent(int v)  { set(&s_subj.ble_sent,  v); }
void gz_subj_set_wifi_sent(int v) { set(&s_subj.wifi_sent, v); }
void gz_subj_set_pub_age_s(int v) { set(&s_subj.pub_age_s, v); }
void gz_subj_set_channel(int v)   { set(&s_subj.channel,   v); }
void gz_subj_set_contacts(int v)  { set(&s_subj.contacts,  v); }
void gz_subj_set_rssi_best(int v) { set(&s_subj.rssi_best, v); }
void gz_subj_set_imu_tilt(int v)  { set(&s_subj.imu_tilt,  v); }
void gz_subj_set_batt_pct(int v)  { set(&s_subj.batt_pct,  v); }
void gz_subj_set_recon_on(int v)  { set(&s_subj.recon_on,  v); }
