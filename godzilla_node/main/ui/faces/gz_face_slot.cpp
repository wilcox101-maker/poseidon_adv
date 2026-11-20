/*
 * gz_face_slot.cpp — see gz_face_slot.h. Field -> subject -> text, the one
 * honest binding the slotted faces share (watch-faces orders, unit 5).
 */
#include "gz_face_slot.h"
#include "gz_face_subjects.h"

#include <stdio.h>

lv_subject_t *gz_face_subject(gz_subj_field_t f)
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

void gz_face_field_text(gz_subj_field_t f, char *out, size_t n)
{
    lv_subject_t *subj = gz_face_subject(f);
    int v = subj ? (int)lv_subject_get_int(subj) : 0;
    switch (f) {
    case GZ_SUBJ_HEAP_KB:   snprintf(out, n, "%dk", v);                        break;
    case GZ_SUBJ_CONTACTS:  snprintf(out, n, "%d", v);                         break;
    case GZ_SUBJ_RSSI_BEST: snprintf(out, n, "%d", v);                         break;
    case GZ_SUBJ_PUB_AGE_S:
        if (v < 0) { snprintf(out, n, "--"); } else { snprintf(out, n, "%ds", v); }
        break;
    case GZ_SUBJ_CHANNEL:   snprintf(out, n, "%d", v);                         break;
    case GZ_SUBJ_IMU_TILT:  snprintf(out, n, "%d\xC2\xB0", v);                 break;
    case GZ_SUBJ_BATT_PCT:
        if (v < 0) { snprintf(out, n, "--"); } else { snprintf(out, n, "%d%%", v); }
        break;
    case GZ_SUBJ_RECON_ON:  snprintf(out, n, v ? "ON" : "off");               break;
    default:                snprintf(out, n, "--");                           break;
    }
}

const char *gz_face_comp_title(gz_comp_id_t c)
{
    switch (c) {
    case GZ_COMP_STATUS:   return "Node";
    case GZ_COMP_CONTACTS: return "Contacts";
    case GZ_COMP_PUBLISH:  return "Publish";
    case GZ_COMP_CHANNEL:  return "Chan";
    case GZ_COMP_IMU:      return "Tilt";
    case GZ_COMP_BATTERY:  return "Batt";
    case GZ_COMP_RECON:    return "Recon";
    case GZ_COMP_DECODE:   return "Decode";
    case GZ_COMP_CLOCK:    return "Clock";
    default:               return "";
    }
}
