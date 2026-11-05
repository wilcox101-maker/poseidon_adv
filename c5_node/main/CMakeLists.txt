idf_component_register(
    SRCS "main.c" "proto.c" "wifi_scanner.c" "zb_sniffer.c" "wifi_attacker.c"
         "pmkid_capture.c" "hs_capture.c" "probe_sniffer.c" "deauth_detect.c"
         "clients_hunt.c" "spectrum.c" "led_fx.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif nvs_flash esp_event ieee802154 esp_timer esp_driver_rmt
)

# Sanity-check bypass is a direct binary patch to ESP-IDF's libnet80211.a,
# applied to the toolchain install itself (not to anything built here) via
# tools/patch_libnet80211.sh — see c5_node/README.md's Building section,
# which this must run before. The wifi_sanity_override.c source file and
# its --wrap / --allow-multiple-definition flags are gone — neither
# approach worked because esp_wifi_80211_tx and its call to the sanity
# check live in the same object file, so the linker already treated the
# symbol as resolved internally.
