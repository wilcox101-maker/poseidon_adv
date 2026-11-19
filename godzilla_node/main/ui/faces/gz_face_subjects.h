/*
 * gz_face_subjects.h — lv_subject wrappers over the node's live state
 * (watch-faces orders, unit 1).
 *
 * The drivers already compute everything the status screen shows; the faces
 * must not re-poll it. So each scalar is published once as an `lv_subject`
 * (LVGL 9.2 observer): the loop-side producers call the setters below, and a
 * face binds a label/arc to `&gz_subjects()->field` and self-updates when the
 * value changes. One owner writes, many widgets read — no polling, no copies.
 *
 * Unit 1 wires the subjects and their setters but nothing reads them yet (no
 * face is built on them until unit 2+), so the displayed UI is unchanged. All
 * values are plain ints (scaled where noted) to stay on the most stable slice
 * of the observer API; strings (node id, last decode) join when a face needs
 * them. Requires LV_USE_OBSERVER (enabled in lv_conf.h).
 */
#pragma once

#include <lvgl.h>

typedef struct {
    lv_subject_t heap_kb;       /* free internal heap, KiB                    */
    lv_subject_t psram_kb;      /* free PSRAM, KiB                            */
    lv_subject_t uptime_s;      /* seconds since boot                         */
    lv_subject_t ble_sent;      /* BLE frames published                       */
    lv_subject_t wifi_sent;     /* Wi-Fi frames published                     */
    lv_subject_t pub_age_s;     /* age of newest publish, s (-1 = never)      */
    lv_subject_t channel;       /* Wi-Fi hop channel 1-13                     */
    lv_subject_t contacts;      /* live contact count                         */
    lv_subject_t rssi_best;     /* strongest contact RSSI, dBm                */
    lv_subject_t imu_tilt;      /* tilt angle, whole degrees (IMU boards)     */
    lv_subject_t batt_pct;      /* battery %, 0-100 (powered boards; -1 n/a)  */
    lv_subject_t recon_on;      /* 0/1 Recon recording to SD                  */
} gz_subjects_t;

/* The single process-wide instance. */
gz_subjects_t *gz_subjects(void);

/* Call once on the UI task before any face is built. */
void gz_subjects_init(void);

/* Loop-side producers call these; a no-op before init. Each only notifies
 * observers when the value actually changes (lv_subject_set_int does). */
void gz_subj_set_heap_kb(int v);
void gz_subj_set_psram_kb(int v);
void gz_subj_set_uptime_s(int v);
void gz_subj_set_ble_sent(int v);
void gz_subj_set_wifi_sent(int v);
void gz_subj_set_pub_age_s(int v);
void gz_subj_set_channel(int v);
void gz_subj_set_contacts(int v);
void gz_subj_set_rssi_best(int v);
void gz_subj_set_imu_tilt(int v);
void gz_subj_set_batt_pct(int v);
void gz_subj_set_recon_on(int v);
