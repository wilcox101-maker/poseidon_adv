/*
 * gz_display_ili9341.cpp — ILI9341V 4-wire SPI back end (ES3C28P), behind the
 * gz_display.h API. Three-board port, completion orders #88 §3.4 / §3.6.
 *
 * Driver: the vendored esp_lcd_st77916 in its plain-SPI mode
 * (use_qspi_interface = 0: 8-bit commands, D/C on its own GPIO), fed
 * gz_panel_tables.h's ILI9341V table (lcdwiki vendor init for the ES3C28P:
 * inversion on, MADCTL 0x08 portrait BGR, RGB565). The panel's reset line is
 * tied to the S3's EN, so reset_gpio_num = -1 and the driver sends a software
 * reset (0x01 + 120 ms) instead — both paths checked in the driver source.
 *
 * Backlight: GPIO45, high = on, LEDC PWM like the 1.85B; dark until the first
 * frame. (GPIO45 is a strapping pin; the vendor's design wires it so, and we
 * only drive it after boot.)
 *
 * Pins: gz_board_es3c28p.h (lcdwiki).
 */
#include "gz_display.h"
#include "gz_board.h"

#if GZ_BOARD_DISPLAY == 3   /* ILI9341V boards (ES3C28P) */

#include "gz_panel_tables.h"
#include "gz_st77916_rev.h"     /* GZ_ST77916_REV_UNKNOWN for panel_rev() */

#include <Arduino.h>
#include <stddef.h>
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st77916.h"

static_assert(sizeof(gz_lcd_init_cmd_t) == sizeof(st77916_lcd_init_cmd_t), "init entry size");
static_assert(offsetof(gz_lcd_init_cmd_t, cmd)        == offsetof(st77916_lcd_init_cmd_t, cmd), "cmd");
static_assert(offsetof(gz_lcd_init_cmd_t, data)       == offsetof(st77916_lcd_init_cmd_t, data), "data");
static_assert(offsetof(gz_lcd_init_cmd_t, data_bytes) == offsetof(st77916_lcd_init_cmd_t, data_bytes), "bytes");
static_assert(offsetof(gz_lcd_init_cmd_t, delay_ms)   == offsetof(st77916_lcd_init_cmd_t, delay_ms), "delay");

#define GZ_LCD_HOST          SPI2_HOST
#define GZ_LCD_CMD_BITS      8
#define GZ_LCD_PARAM_BITS    8
#define GZ_LCD_TRANS_QUEUE   10
#define GZ_LCD_MAX_XFER      (GZ_LCD_WIDTH * GZ_LCD_BUF_LINES * 2)
#define GZ_BL_LEDC_HZ        20000u
#define GZ_BL_LEDC_BITS      10u

static esp_lcd_panel_handle_t s_panel = nullptr;
static gz_display_done_cb_t   s_done_cb  = nullptr;
static void                  *s_done_ctx = nullptr;

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

    /* Backlight attached but dark until the first frame. */
    ledcAttach(GZ_PIN_LCD_BL, GZ_BL_LEDC_HZ, GZ_BL_LEDC_BITS);
    ledcWrite(GZ_PIN_LCD_BL, 0);

    spi_bus_config_t bus = {};
    bus.mosi_io_num     = GZ_PIN_LCD_MOSI;
    bus.miso_io_num     = GZ_PIN_LCD_MISO;
    bus.sclk_io_num     = GZ_PIN_LCD_SCK;
    bus.quadwp_io_num   = -1;
    bus.quadhd_io_num   = -1;
    bus.max_transfer_sz = GZ_LCD_MAX_XFER;
    if (spi_bus_initialize(GZ_LCD_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        Serial.println("!! LCD: spi_bus_initialize failed");
        return false;
    }

    esp_lcd_panel_io_handle_t io = nullptr;
    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.cs_gpio_num         = GZ_PIN_LCD_CS;
    io_cfg.dc_gpio_num         = GZ_PIN_LCD_DC;
    io_cfg.spi_mode            = 0;
    io_cfg.pclk_hz             = GZ_LCD_SPI_HZ;
    io_cfg.trans_queue_depth   = GZ_LCD_TRANS_QUEUE;
    io_cfg.on_color_trans_done = on_color_trans_done;
    io_cfg.lcd_cmd_bits        = GZ_LCD_CMD_BITS;
    io_cfg.lcd_param_bits      = GZ_LCD_PARAM_BITS;
    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)GZ_LCD_HOST, &io_cfg, &io) != ESP_OK) {
        Serial.println("!! LCD: esp_lcd_new_panel_io_spi failed");
        return false;
    }

    size_t n = 0;
    const gz_lcd_init_cmd_t *table = gz_panel_init_table(GZ_PANEL_ILI9341V, &n);
    st77916_vendor_config_t vendor = {};
    vendor.init_cmds      = (const st77916_lcd_init_cmd_t *)table;
    vendor.init_cmds_size = (uint16_t)n;
    vendor.flags.use_qspi_interface = 0;          /* plain 4-wire SPI */

    esp_lcd_panel_dev_config_t panel = {};
    panel.reset_gpio_num = -1;                    /* RST is on EN: software reset */
    panel.rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR;
    panel.bits_per_pixel = 16;
    panel.vendor_config  = &vendor;
    if (esp_lcd_new_panel_st77916(io, &panel, &s_panel) != ESP_OK) {
        Serial.println("!! LCD: ILI9341V panel create failed");
        return false;
    }
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_disp_on_off(s_panel, true);
    Serial.printf("  LCD: ILI9341V %ux%u SPI @ %u MHz up (backlight off until first frame)\n",
                  (unsigned)GZ_LCD_WIDTH, (unsigned)GZ_LCD_HEIGHT,
                  (unsigned)(GZ_LCD_SPI_HZ / 1000000u));
    return true;
}

int gz_display_panel_rev(void) { return GZ_ST77916_REV_UNKNOWN; }   /* no probe here */

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

#endif /* GZ_BOARD_DISPLAY == 3 */
