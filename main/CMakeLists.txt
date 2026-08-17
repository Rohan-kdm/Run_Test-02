idf_component_register(
    SRCS "OPRUSS_logo_black_bg.c" "fan_controller.c" "settings_nvs.c" "fan_logo.c" "air_quality_model.c" "fan_controller.c" "lgfx_wrapper.cpp" "aq_voice_engine.c" "settings_nvs.c" "opruss_boot_logo.c" "air_quality_model.c" "aq_voice_engine.c" "sd_context.c" "ai_client.c" "opruss_mqtt.c"  "ai_router.c" "ui.c" "main.c" "spiffs.c" "lvgl_port.c" "web_download.c" "sd_card.c" "ch422g.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi
                                esp_event
                                nvs_flash
                                esp_netif
                                esp_lcd 
                                driver esp_lcd_touch esp_lcd_touch_gt911 lvgl__lvgl LovyanGFX esp_psram esp_timer spiffs esp_http_client esp_http_server mqtt fatfs sdmmc esp_driver_spi driver mbedtls)
    

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
