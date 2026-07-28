#ifndef BOOT_VIDEO_H
#define BOOT_VIDEO_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Plays a pre-loaded MJPEG boot video from the SD card using PSRAM and Hardware JPEG decoding.
 * 
 * @param filepath The path to the .mjpeg file (e.g., "/sdcard/boot.mjpeg")
 */
void play_boot_video(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // BOOT_VIDEO_H

// In header Add
//#include "boot_video.h"

/*void app_main() {
    // ... NVS, WiFi, Time init ...

    ESP_LOGI(MAIN_TAG, "Initializing SD Card...");
    esp_err_t sd_ret = waveshare_sd_card_init();
    if (sd_ret == ESP_OK) ESP_LOGI(MAIN_TAG, "SD Card Mounted Successfully");
    else ESP_LOGE(MAIN_TAG, "SD Card Mount Failed: %s", esp_err_to_name(sd_ret));

    waveshare_esp32_s3_rgb_lcd_init(); 
    wavesahre_rgb_lcd_bl_off();

    // ==========================================
    // 🎥 PLAY OPRUSS BOOT VIDEO
    // ==========================================
    if (sd_ret == ESP_OK) {
        // The display will turn on during playback inside this function
        play_boot_video("/sdcard/boot.mjpeg"); 
    }

    // Now initialize LVGL UI.
    if (lvgl_port_lock(-1)) {
        spiffs_init();
        ui_init();
        lvgl_port_unlock();
    }
    
    xTaskCreatePinnedToCore(sensor_processing_task, "sensor_processing", 4096, NULL, 5, NULL, 1);

    wavesahre_rgb_lcd_bl_on(); // Failsafe ensure backlight is on for LVGL
}*/