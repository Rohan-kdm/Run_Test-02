#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    CH422G_IO_TOUCH_RST = 1, /* EXIO1, active low */
    CH422G_IO_BACKLIGHT = 2, /* EXIO2 / DISP */
    CH422G_IO_LCD_RST = 3,   /* EXIO3, active low */
    CH422G_IO_SD_CS = 4,     /* EXIO4, active low */
    CH422G_IO_USB_SEL = 5,   /* EXIO5, low selects USB */
} ch422g_io_t;

esp_err_t ch422g_init(void);
esp_err_t ch422g_update_outputs(void);
esp_err_t ch422g_set_bit(ch422g_io_t io);
esp_err_t ch422g_clear_bit(ch422g_io_t io);
uint8_t ch422g_get_outputs(void);
esp_err_t ch422g_set_backlight(bool enabled);
esp_err_t ch422g_set_sd_selected(bool selected);
