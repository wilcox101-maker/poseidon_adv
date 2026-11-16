/*
 * tx_arm — master transmit-arm switch. See tx_arm.h.
 */
#include "tx_arm.h"
#include "ui.h"
#include "theme.h"

#include <Preferences.h>

static bool s_loaded = false;
static bool s_armed  = false;

bool tx_armed(void)
{
    if (!s_loaded) {
        Preferences p;
        if (p.begin("ptx", true)) {
            s_armed = p.getBool("armed", false);
            p.end();
        } else {
            Serial.println("[TXARM] preferences unavailable; defaulting SAFE");
            s_armed = false;
        }
        s_loaded = true;
    }
    return s_armed;
}

void tx_arm_set(bool on)
{
    Preferences p;
    if (!p.begin("ptx", false)) {
        Serial.println("[TXARM] cannot save arm state");
        ui_toast("Cannot save TX state", T_BAD, 900);
        return;
    }
    const bool saved = p.putBool("armed", on) != 0;
    p.end();
    if (!saved) {
        Serial.println("[TXARM] arm-state write failed");
        ui_toast("Cannot save TX state", T_BAD, 900);
        return;
    }
    s_armed  = on;
    s_loaded = true;
    Serial.printf("[TXARM] %s\n", on ? "ARMED" : "SAFE");
}

bool tx_arm_require(void)
{
    if (tx_armed()) return true;
    ui_toast("TX disabled\nArm it in Settings", T_WARN, 1200);
    return false;
}
