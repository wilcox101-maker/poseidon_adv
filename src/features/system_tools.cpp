/*
 * system_tools — saved WiFi connect, file browser, clock, settings.
 */
#include "app.h"
#include "../theme.h"
#include "ui.h"
#include "input.h"
#include "radio.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <SD.h>
#include "../sd_helper.h"
#include <Preferences.h>
#include "../heap_budget.h"
#include <time.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include "../wifi_types.h"

/* ========== Saved WiFi ========== */

static Preferences s_prefs;

void feat_wifi_connect(void)
{
    radio_switch(RADIO_WIFI);
    s_prefs.begin("poseidon", false);

    ui_clear_body();
    auto &d = M5Cardputer.Display;
    d.setTextColor(T_ACCENT, T_BG);
    d.setCursor(4, BODY_Y + 2); d.print("WIFI CONNECT");
    d.drawFastHLine(4, BODY_Y + 12, 100, T_ACCENT);
    d.setTextColor(T_FG, T_BG);
    String ssid = s_prefs.getString("ssid", "");
    String pass = s_prefs.getString("pass", "");
    bool scanned_ssid = false;
    /* A freshly selected scan result must win over stale credentials from a
     * previous failed attempt. Require an explicit password for this target
     * instead of silently retrying an old value. */
    if (g_last_selected_valid && g_last_selected_ap.ssid[0]) {
        ssid = g_last_selected_ap.ssid;
        pass = "";
        scanned_ssid = true;
    }

    if (ssid.length() > 0) {
        d.setCursor(4, BODY_Y + 22); d.printf("saved: %s", ssid.c_str());
        d.setCursor(4, BODY_Y + 34);
        d.print((scanned_ssid || pass.length() == 0)
                ? "[C] enter password  [N] new"
                : "[C] connect  [N] new  [X] forget");
    } else {
        d.setCursor(4, BODY_Y + 22); d.print("no saved network");
        d.setCursor(4, BODY_Y + 34); d.print("[N] add new");
    }
    ui_draw_footer("letter=go  `=back");

    while (true) {
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(20); continue; }
        if (k == PK_ESC) { s_prefs.end(); return; }
        if ((k == 'n' || k == 'N')) {
            char s[33], p[65];
            if (!input_line("ssid:", s, sizeof(s))) continue;
            if (!input_line("password:", p, sizeof(p))) continue;
            s_prefs.putString("ssid", s);
            s_prefs.putString("pass", p);
            ssid = s; pass = p;
            ui_toast("saved", T_GOOD, 600);
            break;
        }
        if ((k == 'x' || k == 'X') && ssid.length() > 0) {
            s_prefs.remove("ssid");
            s_prefs.remove("pass");
            s_prefs.end();
            ui_toast("forgotten", T_WARN, 600);
            return;
        }
        if ((k == 'c' || k == 'C') && ssid.length() > 0) {
            if (pass.length() == 0) {
                char p[65];
                if (!input_line("password:", p, sizeof(p))) continue;
                pass = p;
                s_prefs.putString("ssid", ssid);
                s_prefs.putString("pass", pass);
                scanned_ssid = false;
            }
            break;
        }
    }

    /* Scan and most WiFi features use the raw lean-IDF driver. Calling
     * Arduino WiFi.begin() after that path can double-create the STA netif
     * and reboot on Cardputer-Adv. Configure and connect through the same
     * raw-IDF state instead. */
    if (!wifi_lean_sta_init()) {
        s_prefs.end();
        ui_toast("WiFi init failed", T_BAD, 1500);
        return;
    }
    wifi_config_t sta_cfg = {};
    strncpy((char *)sta_cfg.sta.ssid, ssid.c_str(), sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, pass.c_str(), sizeof(sta_cfg.sta.password) - 1);
    Serial.printf("[wifi_connect] ssid='%s' pass_len=%u\n",
                  ssid.c_str(), (unsigned)pass.length());
    esp_wifi_disconnect();
    esp_err_t cfg_rc = esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_err_t conn_rc = (cfg_rc == ESP_OK) ? esp_wifi_connect() : cfg_rc;
    Serial.printf("[wifi_connect] set_config=%s connect=%s\n",
                  esp_err_to_name(cfg_rc), esp_err_to_name(conn_rc));
    if (cfg_rc != ESP_OK || conn_rc != ESP_OK) {
        s_prefs.end();
        ui_toast("WiFi connect failed", T_BAD, 1500);
        return;
    }
    ui_clear_body();
    d.setTextColor(T_WARN, T_BG);
    d.setCursor(4, BODY_Y + 2); d.printf("connecting to %s", ssid.c_str());

    uint32_t deadline = millis() + 15000;
    uint32_t next_status_log = 0;
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info = {};
    bool connected = false;
    while (!connected && millis() < deadline) {
        wifi_ap_record_t ap_info = {};
        connected = esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
        if (sta_netif) {
            esp_netif_get_ip_info(sta_netif, &ip_info);
            connected = connected && ip_info.ip.addr != 0;
        }
        if (millis() >= next_status_log) {
            next_status_log = millis() + 1000;
            Serial.printf("[wifi_connect] associated=%d ip=%s\n",
                          connected ? 1 : 0,
                          ip_info.ip.addr ? IPAddress(ip_info.ip.addr).toString().c_str() : "0.0.0.0");
        }
        d.fillRect(0, BODY_Y + 22, SCR_W, 12, T_BG);
        d.setCursor(4, BODY_Y + 22);
        d.setTextColor(T_DIM, T_BG);
        d.printf("%ds", (int)((deadline - millis()) / 1000));
        if (input_poll() == PK_ESC) { WiFi.disconnect(); s_prefs.end(); return; }
        delay(250);
    }

    d.fillRect(0, BODY_Y + 22, SCR_W, 60, T_BG);
    if (connected) {
        Serial.println("[wifi_connect] connected");
        d.setTextColor(T_GOOD, T_BG);
        d.setCursor(4, BODY_Y + 22); d.print("CONNECTED");
        d.setTextColor(T_FG, T_BG);
        d.setCursor(4, BODY_Y + 34); d.printf("IP : %s", IPAddress(ip_info.ip.addr).toString().c_str());
        d.setCursor(4, BODY_Y + 46); d.printf("GW : %s", IPAddress(ip_info.gw.addr).toString().c_str());
        d.setCursor(4, BODY_Y + 58); d.printf("MASK: %s", IPAddress(ip_info.netmask.addr).toString().c_str());
    } else {
        wifi_ap_record_t ap_info = {};
        esp_err_t ap_rc = esp_wifi_sta_get_ap_info(&ap_info);
        Serial.printf("[wifi_connect] failed ap_info=%s\n", esp_err_to_name(ap_rc));
        /* Do not keep a credential that just failed association. The next
         * Connect attempt must ask for a fresh password instead of retrying
         * stale input indefinitely. */
        s_prefs.remove("pass");
        d.setTextColor(T_BAD, T_BG);
        d.setCursor(4, BODY_Y + 22); d.print("FAILED");
    }
    s_prefs.end();
    ui_draw_footer("`=back");
    while (true) {
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(20); continue; }
        if (k == PK_ESC) break;
    }
}

/* ========== File browser ========== */

#define FB_MAX 32
struct fb_entry_t { char name[32]; bool is_dir; uint32_t size; };
static fb_entry_t s_fb[FB_MAX];
static int        s_fb_count = 0;

static void fb_list(const char *path)
{
    s_fb_count = 0;
    File root = SD.open(path);
    if (!root || !root.isDirectory()) return;
    File f = root.openNextFile();
    while (f && s_fb_count < FB_MAX) {
        fb_entry_t &e = s_fb[s_fb_count++];
        strncpy(e.name, f.name(), sizeof(e.name) - 1);
        e.name[sizeof(e.name) - 1] = '\0';
        e.is_dir = f.isDirectory();
        e.size   = (uint32_t)f.size();
        f = root.openNextFile();
    }
}

static void fb_view_file(const char *filepath, const char *filename)
{
    File f = SD.open(filepath, FILE_READ);
    if (!f) { ui_toast("cant open file", T_BAD, 1000); return; }

    auto &d = M5Cardputer.Display;
    const int rows = 8;
    int line_offset = 0;
    bool hex_mode = false;

    /* Check if binary extension */
    const char *dot = strrchr(filename, '.');
    if (dot && (!strcasecmp(dot, ".bin") || !strcasecmp(dot, ".pcap") || !strcasecmp(dot, ".dat"))) {
        hex_mode = true;
    }

    auto render = [&]() {
        ui_clear_body();
        d.setTextColor(T_ACCENT, T_BG);
        d.setCursor(4, BODY_Y + 2);
        d.printf("VIEW: %.22s (%s)", filename, hex_mode ? "HEX" : "TXT");
        d.drawFastHLine(4, BODY_Y + 12, SCR_W - 8, T_ACCENT);

        if (!hex_mode) {
            /* Text mode: seek to line */
            f.seek(0);
            int cur_line = 0;
            String l;
            while (cur_line < line_offset && f.available()) {
                l = f.readStringUntil('\n');
                cur_line++;
            }
            int drawn = 0;
            while (drawn < rows && f.available()) {
                l = f.readStringUntil('\n');
                l.trim();
                int y = BODY_Y + 16 + drawn * 11;
                d.setTextColor(T_DIM, T_BG);
                d.setCursor(2, y);
                d.printf("%-3d", cur_line + drawn + 1);
                d.setTextColor(T_FG, T_BG);
                d.setCursor(24, y);
                d.printf("%.34s", l.c_str());
                drawn++;
            }
            if (drawn == 0 && line_offset == 0) {
                d.setTextColor(T_DIM, T_BG);
                d.setCursor(4, BODY_Y + 24);
                d.print("(empty file)");
            }
        } else {
            /* Hex mode */
            f.seek(line_offset * 8);
            for (int r = 0; r < rows && f.available(); r++) {
                uint32_t pos = (uint32_t)f.position();
                uint8_t buf[8];
                int n = f.read(buf, 8);
                int y = BODY_Y + 16 + r * 11;
                d.setTextColor(T_DIM, T_BG);
                d.setCursor(2, y);
                d.printf("%04X:", (unsigned)pos);
                d.setTextColor(T_ACCENT, T_BG);
                d.setCursor(34, y);
                for (int i = 0; i < n; i++) d.printf("%02X ", buf[i]);
                d.setTextColor(T_FG, T_BG);
                d.setCursor(180, y);
                for (int i = 0; i < n; i++) {
                    char c = (buf[i] >= 32 && buf[i] < 127) ? (char)buf[i] : '.';
                    d.printf("%c", c);
                }
            }
        }
        ui_draw_footer(";/.=scroll  H=hex/txt  `=back");
    };

    render();
    while (true) {
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(20); continue; }
        if (k == PK_ESC) break;
        if (k == ';' || k == PK_UP) {
            if (line_offset > 0) { line_offset--; render(); }
        } else if (k == '.' || k == PK_DOWN) {
            line_offset++; render();
        } else if (k == 'h' || k == 'H') {
            hex_mode = !hex_mode;
            line_offset = 0;
            render();
        }
    }
    f.close();
}

void feat_file_browser(void)
{
    if (!sd_mount()) { ui_toast("SD mount fail", T_BAD, 1500); return; }
    char path[128] = "/poseidon";
    fb_list(path);
    int cursor = 0;

    auto draw = [&]() {
        auto &d = M5Cardputer.Display;
        ui_clear_body();
        ui_header(path, "FILES");
        if (s_fb_count == 0) {
            d.setTextColor(T_DIM, T_BG);
            d.setCursor(4, BODY_Y + 24);
            d.print("(empty)");
            return;
        }
        int rows = 8;
        int first = cursor - rows / 2;
        if (first < 0) first = 0;
        if (first + rows > s_fb_count) first = max(0, s_fb_count - rows);
        for (int r = 0; r < rows && first + r < s_fb_count; ++r) {
            const fb_entry_t &e = s_fb[first + r];
            int y = BODY_Y + 20 + r * 11;
            bool sel = (first + r == cursor);
            const uint16_t bg = sel ? T_SEL_BG : T_BG;
            d.fillRect(0, y - 1, SCR_W, 11, bg);
            ui_label(6, y, 24, e.is_dir ? T_ACCENT : T_DIM, bg, e.is_dir ? "DIR" : "");
            ui_label(34, y, 144, sel ? T_FG : T_DIM, bg, e.name);
            char size[24];
            snprintf(size, sizeof(size), "%luB", (unsigned long)e.size);
            ui_label(184, y, 48, T_DIM, bg, e.is_dir ? "" : size);
        }
        ui_scrollbar(237, BODY_Y + 19, 88, first, rows, s_fb_count);
    };

    draw();
    ui_draw_footer(";/.=move  ENTER=view/open  D=del  `=up");
    while (true) {
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(20); continue; }
        if (k == PK_ESC) {
            /* Go up if not at root. */
            char *slash = strrchr(path, '/');
            if (slash && slash != path) { *slash = '\0'; fb_list(path); cursor = 0; draw(); }
            else return;
        } else if (k == ';' || k == PK_UP)    { if (cursor > 0) cursor--; draw(); }
        else if (k == '.' || k == PK_DOWN)    { if (cursor + 1 < s_fb_count) cursor++; draw(); }
        else if (k == PK_ENTER && s_fb_count > 0) {
            const fb_entry_t &e = s_fb[cursor];
            if (e.is_dir) {
                size_t l = strlen(path);
                if (l + 1 + strlen(e.name) + 1 < sizeof(path)) {
                    if (path[l - 1] != '/') { path[l++] = '/'; path[l] = 0; }
                    strcat(path, e.name);
                    fb_list(path); cursor = 0; draw();
                }
            } else {
                char full[192];
                snprintf(full, sizeof(full), "%s/%s", path, e.name);
                fb_view_file(full, e.name);
                draw();
            }
        } else if ((k == 'd' || k == 'D') && s_fb_count > 0) {
            const fb_entry_t &e = s_fb[cursor];
            char full[192];
            snprintf(full, sizeof(full), "%s/%s", path, e.name);
            if (!ui_confirm("DELETE FILE", e.name)) { draw(); continue; }
            if (SD.remove(full)) { ui_toast("deleted", T_GOOD, 500); fb_list(path); if (cursor >= s_fb_count) cursor = s_fb_count - 1; draw(); }
            else { ui_toast("fail", T_BAD, 500); }
        }
    }
}

/* ========== Clock ========== */

void feat_clock(void)
{
    /* Uses GPS time if the LoRa-GNSS hat is present, else uptime. */
    ui_clear_body();
    ui_draw_footer("`=back");
    auto &d = M5Cardputer.Display;

    d.setTextColor(T_ACCENT, T_BG);
    d.setCursor(4, BODY_Y + 2); d.print("CLOCK");
    d.drawFastHLine(4, BODY_Y + 12, 50, T_ACCENT);
    d.setTextColor(T_DIM, T_BG);
    d.setCursor(SCR_W / 2 - 20, BODY_Y + 70);
    d.print("uptime");

    char shown[16] = "";
    uint32_t last = 0;
    while (true) {
        if (millis() - last > 500) {
            last = millis();

            /* Big uptime. */
            uint32_t s = millis() / 1000;
            uint32_t h = s / 3600, m = (s % 3600) / 60, sec = s % 60;
            char buf[16];
            snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
                     (unsigned long)h, (unsigned long)m, (unsigned long)sec);
            if (strcmp(buf, shown) != 0) {
                strcpy(shown, buf);
                d.setTextColor(T_FG, T_BG);
                d.setTextSize(3);
                int w = d.textWidth(buf);
                d.setCursor((SCR_W - w) / 2, BODY_Y + 30);
                d.print(buf);
                d.setTextSize(1);
            }
            ui_draw_status(radio_name(), "clock");
        }
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(40); continue; }
        if (k == PK_ESC) break;
    }
}

/* Live internal-heap census: free / largest contiguous / lifetime low-water
 * mark, plus a manual reclaim. The same numbers the portal + rf_preflight log
 * to serial, viewable on-device during the heap calibration pass. */
void feat_heap_census(void)
{
    ui_clear_body();
    ui_draw_footer("R=reclaim  `=back");
    auto &d = M5Cardputer.Display;

    d.setTextColor(T_ACCENT, T_BG);
    d.setCursor(4, BODY_Y + 2); d.print("HEAP internal");
    d.drawFastHLine(4, BODY_Y + 12, 90, T_ACCENT);

    char shown[64] = "";
    uint32_t last  = 0;
    while (true) {
        if (millis() - last > 400) {
            last = millis();
            unsigned fr  = (unsigned)heap_free_internal();
            unsigned lg  = (unsigned)heap_largest_internal();
            unsigned mn  = (unsigned)heap_min_ever_internal();
            char buf[64];
            snprintf(buf, sizeof(buf), "%u/%u/%u", fr, lg, mn);
            if (strcmp(buf, shown) != 0) {
                strcpy(shown, buf);
                d.setTextColor(T_FG, T_BG);
                d.setCursor(4, BODY_Y + 26); d.printf("free     %6u", fr);
                d.setCursor(4, BODY_Y + 40); d.printf("largest  %6u", lg);
                d.setCursor(4, BODY_Y + 54); d.printf("min ever %6u", mn);
            }
            ui_draw_status(radio_name(), "heap");
        }
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(40); continue; }
        if (k == PK_ESC) break;
        if (k == 'r' || k == 'R') { heap_reclaim_all(); shown[0] = '\0'; }
    }
}

/* ========== Settings (basic) ========== */

#include "../tx_arm.h"

void feat_settings(void)
{
    auto draw = []() {
        ui_force_clear_body();
        ui_header("SETTINGS", "DEVICE");
        const char *rows[] = {
            "[W] Saved WiFi", "[C] Clear credentials log",
            "[F] Clear saved preferences", "[S] Format SD card",
            "[R] Reboot", "[L] Return to Launcher"
        };
        for (int r = 0; r < 6; ++r)
            ui_label(8, BODY_Y + 21 + r * 14, 224, T_FG, T_BG, rows[r]);
        char txrow[36];
        snprintf(txrow, sizeof(txrow), "[T] TX transmit  %s",
                 tx_armed() ? "ARMED" : "SAFE");
        ui_label(8, BODY_Y + 21 + 6 * 14, 224,
                 tx_armed() ? T_BAD : T_GOOD, T_BG, txrow);
        ui_draw_footer("Letter select  ` back");
    };
    draw();
    while (true) {
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(20); continue; }
        if (k == PK_ESC) return;
        if (k == 'w' || k == 'W') { extern void feat_wifi_connect(); feat_wifi_connect(); return; }
        if (k == 'c' || k == 'C') {
            if (!ui_confirm("CLEAR LOG", "Delete the saved credentials log? This cannot be undone.")) { draw(); continue; }
            bool removed = false;
            if (sd_mount()) {
                if (SD.exists(SD_CREDS_PATH)) removed = SD.remove(SD_CREDS_PATH) || removed;
                if (SD.exists("/poseidon/creds.log"))
                    removed = SD.remove("/poseidon/creds.log") || removed;
            }
            ui_toast(removed ? "cleared" : "no creds log",
                     removed ? T_GOOD : T_DIM, 600);
            return;
        }
        if (k == 'f' || k == 'F') {
            if (!ui_confirm("CLEAR PREFERENCES", "Clear saved network preferences? Display and theme choices are retained.")) { draw(); continue; }
            Preferences p; p.begin("poseidon", false); p.clear(); p.end();
            ui_toast("prefs cleared", T_GOOD, 600); return;
        }
        if (k == 's' || k == 'S') {
            if (!ui_confirm("FORMAT SD", "Erase the SD card and all its files? This cannot be undone.")) { draw(); continue; }
            ui_toast("formatting...", T_WARN, 300);
            if (sd_format()) ui_toast("SD formatted", T_GOOD, 900);
            else             ui_toast("SD format fail", T_BAD, 1200);
            return;
        }
        if (k == 'r' || k == 'R') {
            if (ui_confirm("REBOOT", "Restart POSEIDON now?")) ESP.restart();
            draw();
        }
        if (k == 'l' || k == 'L') {
            /* bmorcelli's Launcher uses a custom bootloader that decides
             * which partition to run from the RTC reset-reason:
             *   POWERON_RESET  -> app0/test (Launcher)
             *   SW_CPU_RESET   -> app1/ota_0 (this app)
             * Per the project's own wiki ("impossible to return to the
             * Launcher unless you change the other application to it"),
             * there is no software-only way to force POWERON_RESET, so
             * we can't boot to Launcher from here. Tell the user what
             * the actual escape is. Only show the hint when running
             * under the Launcher partition scheme — otherwise it's
             * nonsense. */
            const esp_partition_t *p = esp_partition_find_first(
                ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, NULL);
            if (p) ui_toast("unplug USB to return", T_WARN, 1500);
            else   ui_toast("no launcher slot",    T_BAD,  900);
            return;
        }
        if (k == 't' || k == 'T') {
            if (tx_armed()) {
                tx_arm_set(false);
                ui_toast("TX SAFE", T_GOOD, 700);
            } else if (ui_confirm("ARM TRANSMIT",
                       "Enable transmitting? Deauth, jammers, beacon spam and other TX features will be able to send frames. Authorized use only.")) {
                tx_arm_set(true);
                ui_toast("TX ARMED", T_BAD, 900);
            }
            draw();
        }
    }
}

/* ===== Screensaver toggle =====
 * NVS-persistent. Default on, 120 s idle threshold. Flips with toast. */
#include "../screensaver.h"

void feat_screensaver_toggle(void)
{
    bool now_on = !screensaver_enabled();
    screensaver_enabled_set(now_on);
    ui_toast(now_on ? "Screensaver ON" : "Screensaver OFF",
             now_on ? T_GOOD : T_WARN, 900);
}

/* ===== Layout toggle =====
 * Flips the menu render style between the dense Terminal list and the
 * big-card Carousel layout. Persists to NVS via menu_style_set. The
 * change takes effect the next time we re-enter run_submenu, which
 * happens when this feature returns. */
#include "../menu.h"

void feat_menu_style_toggle(void)
{
    menu_style_t cur  = menu_style_get();
    menu_style_t next = (cur == MENU_STYLE_TERMINAL) ? MENU_STYLE_CAROUSEL
                                                     : MENU_STYLE_TERMINAL;
    menu_style_set(next);
    ui_toast(next == MENU_STYLE_CAROUSEL ? "Carousel layout"
                                         : "Terminal layout",
             T_GOOD, 900);
}

/* ===== Ambient Preview =====
 * Live preview of the active theme's procedural ambient motion. Hit ESC
 * to exit. 'a' / 'A' toggles the NVS-backed enable flag so the user can
 * disable ambient globally if it's distracting under menus. */
extern void ui_ambient_tick(int x, int y, int w, int h);
extern bool ui_ambient_enabled(void);
extern void ui_ambient_enabled_set(bool on);

void feat_ambient_preview(void)
{
    auto &d = M5Cardputer.Display;
    d.fillScreen(T_BG);
    ui_status_invalidate();
    ui_draw_status("AMBIENT", "preview");
    ui_draw_footer("[A] toggle on/off  [ESC] exit");

    while (true) {
        uint16_t k = input_poll();
        if (k == PK_ESC) break;
        if (k == 'a' || k == 'A') {
            ui_ambient_enabled_set(!ui_ambient_enabled());
            ui_toast(ui_ambient_enabled() ? "ambient ON" : "ambient OFF",
                     ui_ambient_enabled() ? T_GOOD : T_WARN, 700);
        }
        d.fillRect(0, BODY_Y, SCR_W, BODY_H, T_BG);
        ui_ambient_tick(0, BODY_Y, SCR_W, BODY_H);
        d.setTextColor(T_ACCENT, T_BG);
        d.setCursor(4, BODY_Y + 2);
        d.print(theme().name);
        d.setTextColor(T_DIM, T_BG);
        d.setCursor(4, BODY_Y + 12);
        d.print(!ui_motion_enabled() ? "Paused by reduced motion"
               : ui_ambient_enabled() ? "ambient: on" : "ambient: off");
        delay(33);
    }
    ui_force_clear_body();
}
