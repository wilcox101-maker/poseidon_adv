/*
 * gps.cpp — minimal NMEA parser (GGA + RMC).
 */
#include "gps.h"
#include "gps_nmea.h"
#include <HardwareSerial.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <time.h>

static HardwareSerial s_uart(1);
static gps_fix_t s_fix = {};
static portMUX_TYPE s_fix_mux = portMUX_INITIALIZER_UNLOCKED;
static char s_line[128];
static int  s_line_len = 0;
static gps_diag_t s_diag = {};

const gps_diag_t &gps_diag(void) { return s_diag; }

static const uint32_t BAUD_CYCLE[] = { 9600, 115200, 38400, 4800, 19200, 57600 };
static size_t  s_baud_idx = 0;
static uint32_t s_baud    = GPS_BAUD;

static bool s_started = false;
static TaskHandle_t s_gps_task = nullptr;
static volatile bool s_pause_poll = false;
static volatile bool s_gps_stop  = false;
static volatile bool s_gps_alive = false;

bool gps_begin(void)
{
    if (s_started) return true;
    /* CAP-LoRa1262 reference software enables the Cardputer's external 5V
     * rail before opening the ATGM336H receiver. Make that requirement
     * explicit instead of relying on the board-default power configuration. */
    M5.Power.setExtOutput(true);
    /* Match Bruce's Cardputer GPS startup: release the shared RX pad before
     * attaching UART1 so a prior peripheral owner cannot hold it. */
    pinMode(GPS_UART_RX_PIN, INPUT);
    s_uart.begin(s_baud, SERIAL_8N1, GPS_UART_RX_PIN, GPS_UART_TX_PIN);
    s_started = true;
    return true;
}

void gps_end(void)
{
    if (!s_started) return;
    /* Stop the poller before tearing down the UART so it can't read a
     * half-closed port. Force-deleting the task could fire mid-gps_poll()
     * (UART read / newlib lock) and leak a lock or corrupt HardwareSerial.
     * Instead signal a cooperative stop and bounded-join on the task's
     * self-delete before ending the UART, then null the handle so
     * gps_ensure_running respawns it. */
    s_pause_poll = true;
    s_gps_stop   = true;
    uint32_t d = millis() + 500;
    while (s_gps_alive && millis() < d) delay(5);
    s_gps_task   = nullptr;
    s_pause_poll = false;
    s_uart.end();
    /* gps-002: HardwareSerial::end() does not release the pin mode —
     * the UART driver leaves TX in OUTPUT (driven LOW or HIGH depending
     * on idle state) and RX with whatever pull was last set. Hydra park
     * re-claims GPIO 13 (GPS UART TX / CC1101 CS) and drives it high;
     * leaving the UART driver on that pad fights the chip-select.
     * Tri-state both ends so the HSPI park paths can drive cleanly.
     * GPIO 15 (GPS UART RX) is not a Hydra pin. */
    pinMode(GPS_UART_TX_PIN, INPUT);
    pinMode(GPS_UART_RX_PIN, INPUT);
    s_started = false;
}

uint32_t gps_current_baud(void) { return s_baud; }

bool gps_is_up(void) { return s_started; }

/* sys-015 / OPSEC: GPS is OFF by default. NVS namespace "gps", key
 * "enabled" (uint8_t). Cached on first read so menu pages can poll
 * cheaply; gps_set_user_enabled invalidates and persists. */
static bool s_user_enabled_cache    = false;
static bool s_user_enabled_loaded   = false;

bool gps_user_enabled(void)
{
    if (!s_user_enabled_loaded) {
        Preferences p;
        if (p.begin("gps", false)) {
            s_user_enabled_cache = p.getUChar("enabled", 0) ? true : false;
            p.end();
        }
        s_user_enabled_loaded = true;
    }
    return s_user_enabled_cache;
}

void gps_set_user_enabled(bool on)
{
    s_user_enabled_cache  = on;
    s_user_enabled_loaded = true;
    Preferences p;
    if (p.begin("gps", false)) {
        p.putUChar("enabled", on ? 1 : 0);
        p.end();
    }
}

/* Polling task — same as the one previously spawned by main.cpp
 * setup(), now owned by gps.cpp so gps_ensure_running can lazy-
 * spawn it post-boot when the user opens the GPS / Wardrive menu. */
static void gps_task_fn(void *_)
{
    s_gps_alive = true;
    while (!s_gps_stop) {
        gps_poll();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    s_gps_alive = false;
    vTaskDelete(nullptr);
}

bool gps_ensure_running(void)
{
    /* Opening the GPS or Wardrive menu IS the opt-in event — record
     * it so future cold-boots also have GPS up. */
    gps_set_user_enabled(true);
    if (!s_started) gps_begin();
    if (!s_gps_task) {
        s_gps_stop = false;  /* clear any prior cooperative-stop request */
        xTaskCreate(gps_task_fn, "gps", 2304, nullptr, 2, &s_gps_task);
    }
    return s_started;
}

uint32_t gps_cycle_baud(void)
{
    s_pause_poll = true;  /* pause gps_task to avoid UART read during teardown */
    delay(20);
    s_baud_idx = (s_baud_idx + 1) % (sizeof(BAUD_CYCLE) / sizeof(BAUD_CYCLE[0]));
    s_baud = BAUD_CYCLE[s_baud_idx];
    s_uart.end();
    s_started = false;
    s_uart.begin(s_baud, SERIAL_8N1, GPS_UART_RX_PIN, GPS_UART_TX_PIN);
    s_started = true;
    s_diag = {};
    s_line_len = 0;
    s_pause_poll = false;
    Serial.printf("[gps] baud -> %lu\n", (unsigned long)s_baud);
    return s_baud;
}

/* Returns a live reference — fields may tear under concurrent poller
 * updates. Use gps_snapshot() for a race-free multi-field read. */
const gps_fix_t &gps_get(void) { return s_fix; }

bool gps_snapshot(gps_fix_t *out)
{
    if (!out) return false;
    portENTER_CRITICAL(&s_fix_mux);
    bool ok = s_fix.valid;
    if (ok) *out = s_fix;
    portEXIT_CRITICAL(&s_fix_mux);
    return ok;
}

/* ---- helpers ---- */

static double nmea_to_degrees(const char *s, char hemi)
{
    /* NMEA format: DDMM.mmmm or DDDMM.mmmm */
    if (!s || !*s) return 0.0;
    const char *dot = strchr(s, '.');
    int pre = dot ? (int)(dot - s) : (int)strlen(s);
    if (pre < 4) return 0.0;
    int deg_digits = pre - 2;
    /* deg_buf holds up to 3 digits + NUL; a malformed/over-long field
     * (many digits, or no dot) would otherwise overrun it. Reject. */
    if (deg_digits > 3) return 0.0;
    char deg_buf[4] = {0};
    memcpy(deg_buf, s, deg_digits);
    double deg = atof(deg_buf);
    double min = atof(s + deg_digits);
    double out = deg + min / 60.0;
    if (hemi == 'S' || hemi == 'W') out = -out;
    return out;
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* Verify the NMEA *HH checksum: XOR of all bytes between '$' and '*'.
 * Rejects sentences with no '*' delimiter or malformed hex digits. */
static bool nmea_checksum_ok(const char *line)
{
    if (!line || line[0] != '$') return false;
    uint8_t sum = 0;
    const char *p = line + 1;
    while (*p && *p != '*') sum ^= (uint8_t)*p++;
    if (*p != '*') return false;
    int hi = hex_nibble(p[1]);
    int lo = (hi < 0) ? -1 : hex_nibble(p[2]);
    if (hi < 0 || lo < 0) return false;
    return sum == (uint8_t)((hi << 4) | lo);
}

static int split_csv(char *line, char **fields, int max)
{
    int n = 0;
    fields[n++] = line;
    for (char *p = line; *p && n < max; ++p) {
        if (*p == ',') { *p = '\0'; fields[n++] = p + 1; }
        if (*p == '*') { *p = '\0'; break; }
    }
    return n;
}

static void invalidate_fix(void)
{
    gps_fix_t fix = s_fix;
    fix.valid = false;
    fix.time_ms = millis();
    portENTER_CRITICAL(&s_fix_mux);
    s_fix = fix;
    portEXIT_CRITICAL(&s_fix_mux);
}

static void parse_gga(char *line)
{
    char *f[16] = {0};
    int n = split_csv(line, f, 16);
    if (n < 10) return;
    int fix_quality = atoi(f[6]);
    s_diag.last_gga_quality = fix_quality;
    if (fix_quality < 1) {
        invalidate_fix();
        return;
    }
    gps_fix_t fix = s_fix;
    fix.lat_deg = nmea_to_degrees(f[2], f[3][0]);
    fix.lon_deg = nmea_to_degrees(f[4], f[5][0]);
    fix.sats    = (uint8_t)atoi(f[7]);
    fix.hdop    = (float)atof(f[8]);
    fix.alt_m   = (float)atof(f[9]);
    strncpy(fix.utc, f[1], sizeof(fix.utc) - 1);
    fix.valid   = true;
    fix.time_ms = millis();
    portENTER_CRITICAL(&s_fix_mux);
    s_fix = fix;
    portEXIT_CRITICAL(&s_fix_mux);
}

static void parse_rmc(char *line)
{
    char *f[16] = {0};
    int n = split_csv(line, f, 16);
    if (n < 10) return;
    if (f[2][0] != 'A') {
        s_diag.last_rmc_status = f[2][0];
        invalidate_fix();
        return;  /* A = active, V = void */
    }
    s_diag.last_rmc_status = f[2][0];
    gps_fix_t fix = s_fix;
    fix.lat_deg    = nmea_to_degrees(f[3], f[4][0]);
    fix.lon_deg    = nmea_to_degrees(f[5], f[6][0]);
    fix.speed_kts  = (float)atof(f[7]);
    fix.course_deg = (float)atof(f[8]);
    strncpy(fix.date, f[9], sizeof(fix.date) - 1);
    /* Seed the ESP system clock from GPS UTC so FAT timestamps and future
     * capture filenames are meaningful even when WiFi/NTP is unavailable. */
    if (strlen(f[1]) >= 6 && strlen(f[9]) == 6) {
        struct tm utc = {};
        utc.tm_hour = (f[1][0] - '0') * 10 + (f[1][1] - '0');
        utc.tm_min  = (f[1][2] - '0') * 10 + (f[1][3] - '0');
        utc.tm_sec  = (f[1][4] - '0') * 10 + (f[1][5] - '0');
        utc.tm_mday = (f[9][0] - '0') * 10 + (f[9][1] - '0');
        utc.tm_mon  = (f[9][2] - '0') * 10 + (f[9][3] - '0') - 1;
        utc.tm_year = 100 + (f[9][4] - '0') * 10 + (f[9][5] - '0');
        setenv("TZ", "UTC0", 1);
        tzset();
        time_t epoch = mktime(&utc);
        if (epoch > 1577836800) {
            struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
            settimeofday(&tv, nullptr);
        }
    }
    /* Do NOT set s_fix.valid here. RMC's "A" status only means the
     * GPS module's NMEA layer is happy, not that we have a 3D fix —
     * a 2D fix can report 'A' while GGA reports fix-quality=0. The
     * prior code overwrote `alt_m` from a stale 3D fix with garbage
     * because RMC flipped `valid=true` regardless. Let GGA gate
     * validity via fix-quality≥1 (parse_gga does this correctly). */
    fix.time_ms = millis();
    portENTER_CRITICAL(&s_fix_mux);
    s_fix = fix;
    portEXIT_CRITICAL(&s_fix_mux);
}

static void process_line(char *line)
{
    s_diag.lines++;
    strncpy(s_diag.last, line, sizeof(s_diag.last) - 1);
    s_diag.last[sizeof(s_diag.last) - 1] = '\0';
    /* Capture the receiver's raw validity fields before the shared helper
     * can reject a no-fix sentence. */
    char probe[128];
    strncpy(probe, line, sizeof(probe) - 1);
    probe[sizeof(probe) - 1] = '\0';
    char *fields[16] = {0};
    int field_count = split_csv(probe, fields, 16);
    if ((strncmp(line, "$GPGGA", 6) == 0 || strncmp(line, "$GNGGA", 6) == 0) && field_count > 6) {
        strncpy(s_diag.last_gga, line, sizeof(s_diag.last_gga) - 1);
        s_diag.last_gga[sizeof(s_diag.last_gga) - 1] = '\0';
        s_diag.last_gga_quality = atoi(fields[6]);
    } else if ((strncmp(line, "$GPRMC", 6) == 0 || strncmp(line, "$GNRMC", 6) == 0) && field_count > 2) {
        strncpy(s_diag.last_rmc, line, sizeof(s_diag.last_rmc) - 1);
        s_diag.last_rmc[sizeof(s_diag.last_rmc) - 1] = '\0';
        s_diag.last_rmc_status = fields[2][0];
    }
    /* Drop sentences whose *HH checksum doesn't match — RF noise / baud
     * glitches can otherwise yield a valid-looking but wrong fix. */
    if (!nmea_checksum_ok(line)) return;
    if (strncmp(line, "$GPGGA", 6) == 0 || strncmp(line, "$GNGGA", 6) == 0) {
        s_diag.gga++;
        if (!gps_nmea_sentence_has_fix(line)) {
            invalidate_fix();
            return;
        }
        parse_gga(line);
    } else if (strncmp(line, "$GPRMC", 6) == 0 || strncmp(line, "$GNRMC", 6) == 0) {
        s_diag.rmc++;
        if (!gps_nmea_sentence_has_fix(line)) {
            invalidate_fix();
            return;
        }
        parse_rmc(line);
    }
}

void gps_poll(void)
{
    if (s_pause_poll || !s_started) return;
    /* POS-AUDIT-301 / gps-001: cap byte drain per call. At 115200 baud
     * with 8 sentences/sec the UART backs ~1500 B/call, blocking ~10 ms.
     * Triton calls every loop iteration and that throughput murdered
     * the cooperative tick. Drain ≤256 B per invocation — the next
     * call (within ms) picks up where this one stopped, total throughput
     * unchanged but no single call blocks more than ~2 ms. */
    int drained = 0;
    while (s_uart.available() && drained < 256) {
        int c = s_uart.read();
        if (c < 0) break;
        s_diag.bytes++;
        drained++;
        if (c == '\r') continue;
        if (c == '\n') {
            s_line[s_line_len] = '\0';
            if (s_line_len > 6) process_line(s_line);
            s_line_len = 0;
            continue;
        }
        if (s_line_len + 1 < (int)sizeof(s_line)) {
            s_line[s_line_len++] = (char)c;
        } else {
            /* gps-003: surface the overflow so diag screen can show a
             * GPS line that wedged longer than 127 bytes (typically a
             * baud mismatch hammering garbage). */
            s_diag.overflows++;
            s_line_len = 0;
        }
    }

    /* Age out stale fixes after 10 seconds without an update. */
    if (s_fix.valid && millis() - s_fix.time_ms > 10000) {
        portENTER_CRITICAL(&s_fix_mux);
        s_fix.valid = false;
        portEXIT_CRITICAL(&s_fix_mux);
    }
}
