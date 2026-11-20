/*
 * saltyjack_eth — W5500 SPI Ethernet driver + bring-up screen.
 * See saltyjack_eth.h. Authorized testing only.
 */
#include "saltyjack_eth.h"
#include <ETH.h>
#include "../../app.h"
#include "../../ui.h"
#include "../../input.h"
#include "../../sd_helper.h"
#include "saltyjack.h"
#include "saltyjack_style.h"

/* Shared-HSPI pin map (docs/saltyjack-networking.md "How-to B"). */
#define SJ_W5500_CS        1
#define SJ_W5500_IRQ       2
#define SJ_W5500_RST      (-1)   /* RST open, module pull-up */
#define SJ_W5500_PHY_ADDR  1
#define SJ_W5500_SPI_MHZ   20    /* conservative on a bus shared with the SD */

static bool s_started = false;

bool sj_eth_begin(void)
{
    if (s_started) return true;
    sd_prepare_bus();                 /* HSPI pins up even with no card seated */
    /* Reuse the SD's HSPI SPIClass so the W5500 and microSD share one bus
     * (CS-multiplexed) instead of double-initialising the SPI host. */
    s_started = ETH.begin(ETH_PHY_W5500, SJ_W5500_PHY_ADDR,
                          SJ_W5500_CS, SJ_W5500_IRQ, SJ_W5500_RST,
                          sd_get_spi(), SJ_W5500_SPI_MHZ);
    return s_started;
}

void      sj_eth_end(void)      { if (s_started) { ETH.end(); s_started = false; } }
bool      sj_eth_started(void)  { return s_started; }
bool      sj_eth_link_up(void)  { return s_started && ETH.linkUp(); }
bool      sj_eth_has_ip(void)   { return s_started && ETH.localIP() != IPAddress(0, 0, 0, 0); }
IPAddress sj_eth_ip(void)       { return s_started ? ETH.localIP()   : IPAddress(0, 0, 0, 0); }
IPAddress sj_eth_gateway(void)  { return s_started ? ETH.gatewayIP() : IPAddress(0, 0, 0, 0); }
IPAddress sj_eth_subnet(void)   { return s_started ? ETH.subnetMask(): IPAddress(0, 0, 0, 0); }
String    sj_eth_mac(void)      { return s_started ? ETH.macAddress(): String("--"); }

/* ===== bring-up / status screen (SaltyJack ▸ Ethernet) ===== */

static void sj_eth_draw(void)
{
    auto &d = M5Cardputer.Display;
    sj_frame("Ethernet W5500");

    d.setTextColor(SJ_FG_DIM, SJ_BG);
    d.setCursor(SJ_CONTENT_X, BODY_Y + 14);
    d.print("W5500 on shared HSPI");

    d.setTextColor(sj_eth_started() ? SJ_ACCENT : SJ_WARN, SJ_BG);
    d.setCursor(SJ_CONTENT_X, BODY_Y + 28);
    d.print(sj_eth_started() ? "driver: up" : "driver: init FAILED");

    d.setTextColor(sj_eth_link_up() ? SJ_ACCENT : SJ_FG_DIM, SJ_BG);
    d.setCursor(SJ_CONTENT_X, BODY_Y + 38);
    d.print(sj_eth_link_up() ? "link:   UP" : "link:   down");

    d.setTextColor(SJ_FG, SJ_BG);
    d.setCursor(SJ_CONTENT_X, BODY_Y + 50);
    d.print("IP   "); d.print(sj_eth_ip().toString().c_str());
    d.setCursor(SJ_CONTENT_X, BODY_Y + 60);
    d.print("GW   "); d.print(sj_eth_gateway().toString().c_str());
    d.setCursor(SJ_CONTENT_X, BODY_Y + 70);
    d.print("MAC  "); d.print(sj_eth_mac().c_str());

    d.setTextColor(SJ_WARN, SJ_BG);
    d.setCursor(SJ_CONTENT_X, BODY_Y + 88);
    d.print("authorized testing only");

    sj_footer("r=retry  `=back");
}

static void sj_eth_wait_dhcp(void)
{
    for (int i = 0; i < 24 && !sj_eth_has_ip(); i++) delay(250);  /* ~6 s */
}

void feat_saltyjack_ethernet(void)
{
    sj_eth_begin();
    sj_eth_wait_dhcp();
    sj_eth_draw();

    while (true) {
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(30); continue; }
        if (k == PK_ESC) break;
        if (k == 'r' || k == 'R') {
            sj_eth_end();
            sj_eth_begin();
            sj_eth_wait_dhcp();
            sj_eth_draw();
        }
    }
}
