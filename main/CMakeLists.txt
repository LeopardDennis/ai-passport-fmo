set(FMO_COMPACT_FONT "${CMAKE_CURRENT_BINARY_DIR}/fmo_channel_font_compact.c")
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../tools/compact_font.py"
        "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/fmo_channel_font.c" "${FMO_COMPACT_FONT}"
        COMMAND_ERROR_IS_FATAL ANY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/../tools/compact_font.py"
        "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/fmo_channel_font.c")
endif()

idf_component_register(
    SRCS "main.c"
         "fmo_ui.c"
         "fmo_wifi_font.c"
         "fmo_clock.c"
         "fmo_audio.c"
         "fmo_pcm.c"
         "fmo_audio_meter.c"
         "../assets/fonts/fmo_callsign_bold_14.c"
         "../assets/fonts/fmo_callsign_bold_20.c"
         "../assets/fonts/fmo_callsign_bold_32.c"
         "../assets/fonts/fmo_grid_2.c"
         "../assets/fonts/fmo_grid_4.c"
         "../assets/fonts/fmo_grid_6.c"
         "../assets/fonts/fmo_grid_8.c"
         "../assets/fonts/fmo_grid_10.c"
         "../assets/fonts/fmo_grid_12.c"
         "../assets/fonts/fmo_grid_14.c"
         "fmo_controls.c"
         "fmo_stations.c"
         "fmo_wifi_qr.c"
         "${FMO_COMPACT_FONT}"
         "fmo_monitor_state.c"
         "fmo_text.c"
         "fmo_display_policy.c"
         "fmo_storage.c"
         "fmo_storage_policy.c"
         "fmo_network.c"
         "fmo_location.c"
         "fmo_location_json.c"
         "fmo_geocoder.c"
         "fmo_ws_rx.c"
         "fmo_provision.c"
         "fmo_credentials.c"
         "fmo_endpoint.c"
         "fmo_wifi_profiles.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "demo_radio.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_event esp_hw_support esp_netif esp_timer esp_wifi
             esp_websocket_client esp_http_client mbedtls esp_http_server esp_app_format esp_partition json nvs_flash
)
