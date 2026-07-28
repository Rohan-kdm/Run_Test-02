idf_component_register(
    SRCS "opruss_boot_logo.c" "OPRUSS_logo_black_bg.c" "ui.c" "waveshare_rgb_lcd_port.c" "main.c" "spiffs.c" "lvgl_port.c" "web_download.c" "sd_card.c" "ch422g.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi
                                esp_event
                                nvs_flash
                                esp_netif
                                esp_lcd 
                                driver esp_lcd_touch esp_lcd_touch_gt911 lvgl__lvgl esp_psram esp_timer spiffs esp_http_client esp_http_server fatfs sdmmc esp_driver_spi driver)
    

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
