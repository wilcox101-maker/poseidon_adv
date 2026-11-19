/*
 * gz_display.cpp — ST77916 QSPI bring-up for the GODZILLA node (P2a).
 *
 * Sequence follows Waveshare's own 1.85B Arduino demo
 * (waveshareteam/ESP32-S3-Touch-LCD-1.85B, examples/Arduino-V3.2.0/examples/
 * 01_lvgl_demo/Display_ST77916.cpp, QSPI_Init), with three deliberate changes:
 *   - the 5 MHz probe IO is DELETED before the real IO is created (the demo
 *     leaves two SPI devices on one CS line);
 *   - an unknown panel revision falls back to the component's built-in table
 *     with a loud log line instead of silently proceeding;
 *   - 40 MHz instead of 80 MHz until the bench says otherwise.
 */
#include "gz_display.h"
#include "gz_board.h"
#include "gz_st77916_rev.h"
#include "gz_st77916_tables.h"

#include <Arduino.h>
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st77916.h"

#define GZ_LCD_HOST          SPI2_HOST
#define GZ_LCD_CMD_BITS      32
#define GZ_LCD_PARAM_BITS    8
#define GZ_LCD_TRANS_QUEUE   10
/* Largest single QSPI transfer = one LVGL partial buffer (RGB565). esp_lcd
 * splits anything larger into chunks of this size. */
#define GZ_LCD_MAX_XFER      (GZ_LCD_WIDTH * GZ_LCD_BUF_LINES * 2)

#define GZ_LCD_OPCODE_READ   0x0Bu
#define GZ_BL_LEDC_HZ        20000u
#define GZ_BL_LEDC_BITS      10u

static esp_lcd_panel_handle_t s_panel = nullptr;
static int                    s_rev   = GZ_ST77916_REV_UNKNOWN;
static gz_display_done_cb_t   s_done_cb  = nullptr;
static void                  *s_done_ctx = nullptr;

static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                          esp_lcd_panel_io_event_data_t *edata,
                                          void *user_ctx)
{
    (void)io; (void)edata; (void)user_ctx;
    return s_done_cb ? s_done_cb(s_done_ctx) : false;
}

static esp_lcd_panel_io_handle_t make_io(uint32_t hz, bool with_cb)
{
    esp_lcd_panel_io_spi_config_t cfg = {};
    cfg.cs_gpio_num       = GZ_PIN_LCD_CS;
    cfg.dc_gpio_num       = -1;
    cfg.spi_mode          = 0;
    cfg.pclk_hz           = hz;
    cfg.trans_queue_depth = GZ_LCD_TRANS_QUEUE;
    cfg.on_color_trans_done = with_cb ? on_color_trans_done : nullptr;
    cfg.user_ctx          = nullptr;
    cfg.lcd_cmd_bits      = GZ_LCD_CMD_BITS;
    cfg.lcd_param_bits    = GZ_LCD_PARAM_BITS;
    cfg.flags.quad_mode   = 1;
    esp_lcd_panel_io_handle_t io = nullptr;
    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)GZ_LCD_HOST, &cfg, &io) != ESP_OK)
        return nullptr;
    return io;
}

bool gz_display_init(gz_display_done_cb_t on_done, void *ctx)
{
    s_done_cb  = on_done;
    s_done_ctx = ctx;

    /* Backlight attached but dark until the first frame. */
    ledcAttach(GZ_PIN_LCD_BL, GZ_BL_LEDC_HZ, GZ_BL_LEDC_BITS);
    ledcWrite(GZ_PIN_LCD_BL, 0);

    /* Hard reset so the controller answers the probe. */
    pinMode(GZ_PIN_LCD_RST, OUTPUT);
    digitalWrite(GZ_PIN_LCD_RST, LOW);
    delay(10);
    digitalWrite(GZ_PIN_LCD_RST, HIGH);
    delay(50);

    spi_bus_config_t bus = {};
    bus.data0_io_num    = GZ_PIN_LCD_DATA0;
    bus.data1_io_num    = GZ_PIN_LCD_DATA1;
    bus.sclk_io_num     = GZ_PIN_LCD_PCLK;
    bus.data2_io_num    = GZ_PIN_LCD_DATA2;
    bus.data3_io_num    = GZ_PIN_LCD_DATA3;
    bus.data4_io_num    = -1;
    bus.data5_io_num    = -1;
    bus.data6_io_num    = -1;
    bus.data7_io_num    = -1;
    bus.max_transfer_sz = GZ_LCD_MAX_XFER;
    bus.flags           = SPICOMMON_BUSFLAG_MASTER;
    if (spi_bus_initialize(GZ_LCD_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        Serial.println("!! LCD: spi_bus_initialize failed");
        return false;
    }

    /* Revision probe: read reg 0x04 at a slow clock, then drop that IO. */
    uint8_t reg04[4] = { 0, 0, 0, 0 };
    esp_lcd_panel_io_handle_t probe = make_io(GZ_LCD_PROBE_HZ, false);
    if (probe) {
        int cmd = (int)((GZ_LCD_OPCODE_READ << 24) | (0x04u << 8));
        if (esp_lcd_panel_io_rx_param(probe, cmd, reg04, sizeof reg04) != ESP_OK)
            Serial.println("!! LCD: reg 0x04 read failed");
        esp_lcd_panel_io_del(probe);
    }
    s_rev = gz_st77916_panel_rev(reg04);
    Serial.printf("  LCD: reg04 = %02X %02X %02X %02X -> panel rev %d\n",
                  reg04[0], reg04[1], reg04[2], reg04[3], s_rev);

    esp_lcd_panel_io_handle_t io = make_io(GZ_LCD_QSPI_HZ, true);
    if (!io) {
        Serial.println("!! LCD: esp_lcd_new_panel_io_spi failed");
        return false;
    }

    st77916_vendor_config_t vendor = {};
    vendor.flags.use_qspi_interface = 1;
    if (s_rev == GZ_ST77916_REV_1) {
        vendor.init_cmds      = gz_st77916_init_v1;
        vendor.init_cmds_size = (uint16_t)gz_st77916_init_v1_len;
    } else if (s_rev == GZ_ST77916_REV_2) {
        vendor.init_cmds      = gz_st77916_init_v2;
        vendor.init_cmds_size = (uint16_t)gz_st77916_init_v2_len;
    } else {
        Serial.println("!! LCD: unknown panel revision — using the driver's "
                       "built-in init table; report the reg04 bytes above");
    }

    esp_lcd_panel_dev_config_t panel = {};
    panel.reset_gpio_num  = GZ_PIN_LCD_RST;
    panel.rgb_ele_order   = LCD_RGB_ELEMENT_ORDER_RGB;
    panel.bits_per_pixel  = 16;
    panel.vendor_config   = &vendor;
    if (esp_lcd_new_panel_st77916(io, &panel, &s_panel) != ESP_OK) {
        Serial.println("!! LCD: esp_lcd_new_panel_st77916 failed");
        return false;
    }
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_disp_on_off(s_panel, true);
    Serial.printf("  LCD: ST77916 %ux%u QSPI @ %u MHz up (backlight off until first frame)\n",
                  (unsigned)GZ_LCD_WIDTH, (unsigned)GZ_LCD_HEIGHT,
                  (unsigned)(GZ_LCD_QSPI_HZ / 1000000u));
    return true;
}

int gz_display_panel_rev(void) { return s_rev; }

void gz_display_draw(int x1, int y1, int x2_excl, int y2_excl, const void *px)
{
    if (s_panel) esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2_excl, y2_excl, px);
}

void gz_display_backlight(uint8_t pct)
{
    if (pct > 100) pct = 100;
    uint32_t max = (1u << GZ_BL_LEDC_BITS) - 1u;
    ledcWrite(GZ_PIN_LCD_BL, (max * pct) / 100u);
}
