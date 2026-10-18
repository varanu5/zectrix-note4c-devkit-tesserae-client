# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 varanu5 <https://github.com/varanu5>
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/secrets.h")
    message(FATAL_ERROR "Remove secrets.h. Configure credentials through the device setup portal.")
endif()

idf_component_register(
    SRCS "main.c" "config_store.c" "epd_driver.c" "display_update.c" "power.c" "framebuf.c"
         "relay.c" "relay_crypto.c" "relay_wire.c" "rest_handler.c" "heartbeat.c" "wifi_manager.c" "wake_align.c"
         "log_capture.c" "log_ring.c" "diag.c" "provisioning.c"
         "provision_form.c" "setup_identity.c" "screen.c" "splash.c" "fb2bpp.c"
         "setup_request.c" "portal_view.c" "vendor/qrcodegen.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_spi esp_adc esp_timer esp_wifi esp_netif
             esp_event esp_http_client esp_http_server nvs_flash
             cjson mbedtls lwip esp_hw_support esp_psram)

idf_build_get_property(note4c_version PROJECT_VER)
target_compile_definitions(${COMPONENT_LIB} PRIVATE FW_VERSION="${note4c_version}")

foreach(screen setup waiting register lowbatt revoked)
    target_add_binary_data(${COMPONENT_LIB} "assets/splash_${screen}.bin" BINARY)
endforeach()
