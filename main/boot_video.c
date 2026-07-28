#include "boot_video.h"
#include <stdio.h>
#include <sys/stat.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jpeg_decoder.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "BOOT_VID";

// External references to your display driver handles
extern esp_lcd_panel_handle_t panel_handle; 
extern void wavesahre_rgb_lcd_bl_on(void); // Keeping your original spelling

void play_boot_video(const char* filepath) {
    ESP_LOGI(TAG, "Starting OPRUSS Boot Video Engine");

    // 1. Get File Size
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ESP_LOGE(TAG, "Video file not found: %s", filepath);
        return;
    }
    
    size_t file_size = st.st_size;
    ESP_LOGI(TAG, "File Size: %.2f MB", (float)file_size / (1024*1024));

    if(file_size > (3 * 1024 * 1024)) {
        ESP_LOGE(TAG, "File exceeds 3MB PSRAM limit!");
        return;
    }

    // 2. Allocate PSRAM for Entire File & Framebuffer
    uint8_t *video_file_buf = heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    uint16_t *decode_frame_buf = heap_caps_malloc(800 * 480 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);

    if (!video_file_buf || !decode_frame_buf) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffers");
        if(video_file_buf) free(video_file_buf);
        if(decode_frame_buf) free(decode_frame_buf);
        return;
    }

    // 3. Fast Sequential Read from SD
    ESP_LOGI(TAG, "Pre-loading video to PSRAM...");
    FILE *f = fopen(filepath, "rb");
    if (f) {
        fread(video_file_buf, 1, file_size, f);
        fclose(f);
    }
    ESP_LOGI(TAG, "Pre-load complete. Commencing playback.");

    // Turn backlight on right before pushing frames
    wavesahre_rgb_lcd_bl_on();

    // 4. Initialize Hardware JPEG Decoder
    jpeg_dec_io_t *jpeg_io = esp_jpeg_dec_alloc_default();
    jpeg_dec_header_info_t header_info;
    
    // 5. MJPEG Parsing Loop (Search for SOI 0xFF 0xD8 and EOI 0xFF 0xD9)
    size_t offset = 0;
    while (offset < file_size - 1) {
        if (video_file_buf[offset] == 0xFF && video_file_buf[offset + 1] == 0xD8) {
            size_t start = offset;
            size_t end = start;
            
            // Find end of current JPEG frame
            while (end < file_size - 1) {
                if (video_file_buf[end] == 0xFF && video_file_buf[end + 1] == 0xD9) {
                    end += 2;
                    break;
                }
                end++;
            }

            size_t frame_len = end - start;
            
            // Hardware decode the frame directly from PSRAM
            esp_jpeg_dec_parse_header(jpeg_io, video_file_buf + start, frame_len, &header_info);
            esp_jpeg_dec_process(jpeg_io, video_file_buf + start, frame_len, (uint8_t*)decode_frame_buf, 800 * 480 * 2);

            // Push directly to LCD
            esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 800, 480, decode_frame_buf);

            // ~33ms delay for ~30fps playback of the OPRUSS animation
            vTaskDelay(pdMS_TO_TICKS(33)); 
            
            offset = end;
        } else {
            offset++;
        }
    }

    // 6. Cleanup Memory to give it back to LVGL
    esp_jpeg_dec_free(jpeg_io);
    heap_caps_free(video_file_buf);
    heap_caps_free(decode_frame_buf);
    
    ESP_LOGI(TAG, "Video complete.");
}