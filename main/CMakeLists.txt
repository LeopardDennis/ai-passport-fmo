idf_component_register(
    SRCS "main.c"
         "fmo_monitor_state.c"
         "fmo_network.c"
         "fmo_ws_rx.c"
         "fmo_provision.c"
         "fmo_credentials.c"
         "fmo_wifi_profiles.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "demo_radio.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_event esp_hw_support esp_netif esp_timer esp_wifi
             esp_websocket_client esp_http_server json nvs_flash
)
