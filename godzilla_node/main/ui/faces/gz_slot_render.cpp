/*
 * gz_slot_render.cpp — shared slot renderer helpers (watch-faces orders, unit 5).
 * See gz_slot_render.h. The title / subject / value-format logic mirrors the
 * Command face's pods (gz_face_command.cpp) so every slotted face reads the
 * same honest live fields.
 *
 * Firmware-only (LVGL). Compiled on every godzilla env.
 */
#include "gz_slot_render.h"
#include "gz_face_subjects.h"

#include <stdio.h>

const char *gz_slot_comp_title(gz_comp_id_t c)
{
    switch (c) {
    case GZ_COMP_STATUS:   return "NODE";
    case GZ_COMP_CONTACTS: return "CONTACT";
    case GZ_COMP_PUBLISH:  return "PUBLISH";
    case GZ_COMP_CHANNEL:  return "CHAN";
    case GZ_COMP_IMU:      return "TILT";
    case GZ_COMP_BATTERY:  return "BATT";
    case GZ_COMP_RECON:    return "RECON";
    case GZ_COMP_DECODE:   return "DECODE";
    case GZ_COMP_CLOCK:    return "CLOCK";
    default:               return "";
    }
}

lv_subject_t *gz_slot_field_subject(gz_subj_field_t f)
{
    gz_subjects_t *s = gz_subjects();
    switch (f) {
    case GZ_SUBJ_HEAP_KB:   return &s->heap_kb;
    case GZ_SUBJ_CONTACTS:  return &s->contacts;
    case GZ_SUBJ_RSSI_BEST: return &s->rssi_best;
    case GZ_SUBJ_PUB_AGE_S: return &s->pub_age_s;
    case GZ_SUBJ_CHANNEL:   return &s->channel;
    case GZ_SUBJ_IMU_TILT:  return &s->imu_tilt;
    case GZ_SUBJ_BATT_PCT:  return &s->batt_pct;
    case GZ_SUBJ_RECON_ON:  return &s->recon_on;
    default:                return nullptr;
    }
}

void gz_slot_field_text(gz_subj_field_t f, char *out, size_t n)
{
    lv_subject_t *subj = gz_slot_field_subject(f);
    int v = subj ? (int)lv_subject_get_int(subj) : 0;
    switch (f) {
    case GZ_SUBJ_HEAP_KB:   snprintf(out, n, "%dk", v);             break;
    case GZ_SUBJ_CONTACTS:  snprintf(out, n, "%d", v);             break;
    case GZ_SUBJ_RSSI_BEST: snprintf(out, n, "%ddBm", v);          break;
    case GZ_SUBJ_PUB_AGE_S: if (v < 0) snprintf(out, n, "--");
                            else       snprintf(out, n, "%ds", v);  break;
    case GZ_SUBJ_CHANNEL:   snprintf(out, n, "%d", v);             break;
    case GZ_SUBJ_IMU_TILT:  snprintf(out, n, "%d\xC2\xB0", v);     break;
    case GZ_SUBJ_BATT_PCT:  if (v < 0) snprintf(out, n, "--");
                            else       snprintf(out, n, "%d%%", v); break;
    case GZ_SUBJ_RECON_ON:  snprintf(out, n, v ? "ON" : "off");    break;
    default:                snprintf(out, n, "--");                break;
    }
}
