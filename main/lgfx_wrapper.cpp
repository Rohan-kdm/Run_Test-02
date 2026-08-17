#include "lgfx_port.hpp"
#include "lvgl.h"
#include "esp_log.h"
#include "ch422g.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_lcd_touch_gt911.h"
#include "driver/i2c_master.h"

// Global Instances
static LGFX lcd;
static esp_lcd_touch_handle_t tp_handle = NULL;

extern "C" i2c_master_bus_handle_t shared_i2c_bus;

extern "C" {

    // ============================================================
    // HARDWARE CONTROL FUNCTIONS
    // ============================================================
    void waveshare_esp32_s3_touch_reset(void) {
        ch422g_clear_bit(CH422G_IO_TOUCH_RST); 
        esp_rom_delay_us(100 * 1000);
        
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.pin_bit_mask = (1ULL << 4);
        io_conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&io_conf);
        gpio_set_level(GPIO_NUM_4, 0);
        
        esp_rom_delay_us(100 * 1000);
        ch422g_set_bit(CH422G_IO_TOUCH_RST);   
        esp_rom_delay_us(200 * 1000);
    }

    esp_err_t wavesahre_rgb_lcd_bl_on(void) {
        return ch422g_set_bit(CH422G_IO_BACKLIGHT);
    }

    esp_err_t wavesahre_rgb_lcd_bl_off(void) {
        return ch422g_clear_bit(CH422G_IO_BACKLIGHT);
    }

    // ============================================================
    // HYBRID INITIALIZATION (LGFX Display + ESP_LCD Touch)
    // ============================================================
    void lgfx_hardware_init(void) {
        // 1. Init LovyanGFX (Display Only)
        lcd.init();
        ESP_LOGI("LGFX", "LovyanGFX RGB Display Initialized");

        // 2. Init ESP-LCD (Touch Only - Uses strictly Modern I2C)
        if (shared_i2c_bus != NULL) {
            esp_lcd_panel_io_handle_t tp_io_handle = NULL;
            esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();

            tp_io_config.scl_speed_hz = 400000; // setting the scl frequency to modern i2c clock speed
            
            // THE FIX: Use _v2() and pass the modern handle directly!
            if (esp_lcd_new_panel_io_i2c_v2(shared_i2c_bus, &tp_io_config, &tp_io_handle) == ESP_OK) {
                
                esp_lcd_touch_config_t tp_cfg = {}; 
                tp_cfg.x_max = 800;
                tp_cfg.y_max = 480;
                tp_cfg.rst_gpio_num = (gpio_num_t)-1;
                tp_cfg.int_gpio_num = (gpio_num_t)-1;
                tp_cfg.levels.reset = 0;
                tp_cfg.levels.interrupt = 0;
                tp_cfg.flags.swap_xy = 0;
                tp_cfg.flags.mirror_x = 0;
                tp_cfg.flags.mirror_y = 0;
                tp_cfg.driver_data = NULL;
                
                esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle);
                ESP_LOGI("LGFX", "Modern I2C GT911 Touch Initialized successfully");
            } else {
                ESP_LOGE("LGFX", "Failed to initialize Touch IO");
            }
        }
    }

    // ============================================================
    // LVGL CALLBACKS
    // ============================================================
    void lgfx_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);
        
        // Let LovyanGFX handle the heavy DMA lifting
        lcd.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t *)&color_map->full);
        lv_disp_flush_ready(drv);
    }

    void lgfx_touch_read_cb(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
        if (!tp_handle) {
            data->state = LV_INDEV_STATE_REL;
            return;
        }

        uint16_t x, y;
        uint8_t touchpad_cnt = 0;
        
        // Read via the modern ESP-LCD driver
        esp_lcd_touch_read_data(tp_handle);
        bool pressed = esp_lcd_touch_get_coordinates(tp_handle, &x, &y, NULL, &touchpad_cnt, 1);
        
        if (pressed && touchpad_cnt > 0) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = x;
            data->point.y = y;
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
    }

} // End of extern "C"