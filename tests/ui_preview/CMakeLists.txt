cmake_minimum_required(VERSION 3.16)
project(fmo_ui_preview C CXX ASM)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory(../../managed_components/lvgl__lvgl lvgl)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../sdkconfig.defaults")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../sdkconfig.defaults" FMO_POOL_LINE
    REGEX "^CONFIG_LV_MEM_SIZE_KILOBYTES=[0-9]+$")
if(NOT FMO_POOL_LINE MATCHES "=([0-9]+)$")
    message(FATAL_ERROR "Missing firmware LVGL memory budget")
endif()
math(EXPR FMO_POOL_BYTES "${CMAKE_MATCH_1} * 1024")
target_compile_definitions(lvgl PUBLIC LV_MEM_SIZE=${FMO_POOL_BYTES})
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(FMO_COMPACT_FONT "${CMAKE_CURRENT_BINARY_DIR}/fmo_channel_font_compact.c")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/compact_font.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/fonts/fmo_channel_font.c" "${FMO_COMPACT_FONT}"
    COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/compact_font.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/fonts/fmo_channel_font.c")
add_executable(fmo_ui_preview ../test_fmo_ui.c ../../assets/fonts/fmo_callsign_bold_14.c ../../assets/fonts/fmo_callsign_bold_20.c ../../assets/fonts/fmo_callsign_bold_32.c ../../main/fmo_ui.c ../../main/fmo_wifi_font.c ../../main/fmo_audio_meter.c ../../main/fmo_clock.c ../../main/fmo_controls.c ../../main/fmo_wifi_qr.c ../../main/fmo_monitor_state.c ../../main/fmo_text.c "${FMO_COMPACT_FONT}")
target_include_directories(fmo_ui_preview PRIVATE ../../main)
target_link_libraries(fmo_ui_preview PRIVATE lvgl m)
enable_testing()
add_test(NAME fmo_ui COMMAND fmo_ui_preview)
foreach(state IN ITEMS cross idle empty lan muted lastheard offline setup setup_info network network_retry network_back wifi_saved wifi_current wifi_empty wifi_offline wifi_long long rare brackets symbols error meter_low meter_mid meter_high)
    add_test(NAME fmo_ui_${state} COMMAND fmo_ui_preview ${state})
endforeach()
