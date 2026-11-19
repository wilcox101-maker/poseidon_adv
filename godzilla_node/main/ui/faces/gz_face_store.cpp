/*
 * gz_face_store.cpp — watch-faces settings in NVS via Arduino Preferences.
 * See gz_face_store.h. Firmware-only (needs the ESP32 NVS); the pure codec and
 * the board-caps sanitiser it calls are host-tested in test_godzilla_faces.
 */
#include "gz_face_store.h"
#include "gz_face_caps.h"    /* GZ_FACE_BOARD_CAPS for this board */

#include <Preferences.h>

static const char *GZ_FACE_NS  = "gzui";
static const char *GZ_FACE_KEY = "cfg";

void gz_face_store_load(gz_ui_settings_t *s)
{
    gz_ui_settings_defaults(s);

    Preferences prefs;
    if (prefs.begin(GZ_FACE_NS, /*readOnly=*/true)) {
        uint8_t blob[GZ_SETTINGS_WIRE_LEN];
        size_t n = prefs.getBytes(GZ_FACE_KEY, blob, sizeof blob);
        prefs.end();
        /* decode rejects anything that isn't exactly our blob -> keeps defaults */
        if (n == (size_t)GZ_SETTINGS_WIRE_LEN)
            gz_ui_settings_decode(blob, (int)n, s);
    }

    gz_ui_settings_sanitise(s, GZ_FACE_BOARD_CAPS);
}

void gz_face_store_save(const gz_ui_settings_t *s)
{
    uint8_t blob[GZ_SETTINGS_WIRE_LEN];
    int n = gz_ui_settings_encode(s, blob);

    Preferences prefs;
    if (prefs.begin(GZ_FACE_NS, /*readOnly=*/false)) {
        prefs.putBytes(GZ_FACE_KEY, blob, (size_t)n);
        prefs.end();
    }
}
