/*
 * gz_display_co5300.cpp — CO5300 QSPI AMOLED back end (AMOLED-2.06), behind
 * the gz_display.h API. Three-board port, completion orders #88 §3.4 / §3.5.
 *
 * Driver: the vendored esp_lcd_st77916 in QSPI mode (same 0x02 command /
 * 0x32 pixel framing the CO5300 uses), fed gz_panel_tables.h's CO5300 table
 * (vendor sequence from Arduino_GFX's co5300_init_operations, as used by
 * Waveshare's AMOLED-2.06 demo). The panel RAM is 480 wide with the visible
 * 410 columns offset by GZ_LCD_COL_GAP (22), so set_gap shifts every window.
 * The CO5300 wants even window x / width; gz_ui.cpp's rounder already sends
 * full-width, even-row strips (410 and 22 are both even).
 *
 * No backlight: "backlight" is the panel's DCS brightness (0x51), written over
 * the same QSPI IO. The table leaves it at 0x00, so the panel is dark until
 * gz_ui.cpp raises it after the first flush — no flash at boot.
 *
 * Pins: gz_board_amoled206.h (Waveshare wiki). TE (GPIO13) is not used yet.
 */
#include "gz_display.h"
#include "gz_board.h"

#if GZ_BOARD_DISPLAY == 2   /* CO5300 boards (AMOLED-2.06) */

#include "gz_panel_tables.h"
#include "gz_st77916_rev.h"     /* GZ_ST77916_REV_UNKNOWN for panel_rev() */

#include <Arduino.h>
#include <stddef.h>
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st77916.h"

/* gz_lcd_init_cmd_t must be a drop-in for the driver's table entry. */
static_assert(sizeof(gz_lcd_init_cmd_t) == sizeof(st77916_lcd_init_cmd_t), "init entry size");
static_assert(offsetof(gz_lcd_init_cmd_t, cmd)        == offsetof(st77916_lcd_init_cmd_t, cmd), "cmd");
static_assert(offsetof(gz_lcd_init_cmd_t, data)       == offsetof(st77916_lcd_init_cmd_t, data), "data");
static_assert(offsetof(gz_lcd_init_cmd_t, data_bytes) == offsetof(st77916_lcd_init_cmd_t, data_bytes), "bytes");
static_assert(offsetof(gz_lcd_init_cmd_t, delay_ms)   == offsetof(st77916_lcd_init_cmd_t, delay_ms), "delay");

#define GZ_LCD_HOST          SPI2_HOST
#define GZ_LCD_CMD_BITS      32
#define GZ_LCD_PARAM_BITS    8
#define GZ_LCD_TRANS_QUEUE   10
#define GZ_LCD_MAX_XFER      (GZ_LCD_WIDTH * GZ_LCD_BUF_LINES * 2)
#define GZ_QSPI_OPCODE_WRITE 0x02u

static esp_lcd_panel_handle_t    s_panel = nullptr;
static esp_lcd_panel_io_handle_t s_io    = nullptr;
static gz_display_done_cb_t      s_done_cb  = nullptr;
static void                     *s_done_ctx = nullptr;

static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                          esp_lcd_panel_io_event_data_t *edata,
                                          void *user_ctx)
{
    (void)io; (void)edata; (void)user_ctx;
    return s_done_cb ? s_done_cb(s_done_ctx) : false;
}

bool gz_display_init(gz_display_done_cb_t on_done, void *ctx)
{
    s_done_cb  = on_done;
    s_done_ctx = ctx;

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

    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.cs_gpio_num         = GZ_PIN_LCD_CS;
    io_cfg.dc_gpio_num         = -1;
    io_cfg.spi_mode            = 0;
    io_cfg.pclk_hz             = GZ_LCD_QSPI_HZ;
    io_cfg.trans_queue_depth   = GZ_LCD_TRANS_QUEUE;
    io_cfg.on_color_trans_done = on_color_trans_done;
    io_cfg.lcd_cmd_bits        = GZ_LCD_CMD_BITS;
    io_cfg.lcd_param_bits      = GZ_LCD_PARAM_BITS;
    io_cfg.flags.quad_mode     = 1;
    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)GZ_LCD_HOST, &io_cfg, &s_io) != ESP_OK) {
        Serial.println("!! LCD: esp_lcd_new_panel_io_spi failed");
        return false;
    }

    size_t n = 0;
    const gz_lcd_init_cmd_t *table = gz_panel_init_table(GZ_PANEL_CO5300, &n);
    st77916_vendor_config_t vendor = {};
    vendor.init_cmds      = (const st77916_lcd_init_cmd_t *)table;
    vendor.init_cmds_size = (uint16_t)n;
    vendor.flags.use_qspi_interface = 1;

    esp_lcd_panel_dev_config_t panel = {};
    panel.reset_gpio_num = GZ_PIN_LCD_RST;
    panel.rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB;
    panel.bits_per_pixel = 16;
    panel.vendor_config  = &vendor;
    if (esp_lcd_new_panel_st77916(s_io, &panel, &s_panel) != ESP_OK) {
        Serial.println("!! LCD: CO5300 panel create failed");
        return false;
    }
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_set_gap(s_panel, GZ_LCD_COL_GAP, 0);
    esp_lcd_panel_disp_on_off(s_panel, true);
    Serial.printf("  LCD: CO5300 %ux%u AMOLED QSPI @ %u MHz up, col gap %u "
                  "(brightness 0 until first frame)\n",
                  (unsigned)GZ_LCD_WIDTH, (unsigned)GZ_LCD_HEIGHT,
                  (unsigned)(GZ_LCD_QSPI_HZ / 1000000u), (unsigned)GZ_LCD_COL_GAP);
    return true;
}

int gz_display_panel_rev(void) { return GZ_ST77916_REV_UNKNOWN; }   /* no probe here */

void gz_display_draw(int x1, int y1, int x2_excl, int y2_excl, const void *px)
{
    if (s_panel) esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2_excl, y2_excl, px);
}

/* AMOLED brightness: DCS 0x51 over the QSPI IO (0x02 write-command framing). */
void gz_display_backlight(uint8_t pct)
{
    if (!s_io) return;
    uint8_t b = gz_panel_brightness_byte(pct);
    int cmd = (int)((GZ_QSPI_OPCODE_WRITE << 24) | ((uint32_t)GZ_DCS_BRIGHTNESS << 8));
    esp_lcd_panel_io_tx_param(s_io, cmd, &b, 1);
}

#endif /* GZ_BOARD_DISPLAY == 2 */
