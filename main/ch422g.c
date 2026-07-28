#include "ch422g.h"

#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define CH422G_I2C_PORT I2C_NUM_0
#define CH422G_CONFIG_ADDRESS 0x24
#define CH422G_OUTPUT_ADDRESS 0x38
#define CH422G_CONFIG_OUTPUT 0x01
#define CH422G_SAFE_IDLE_OUTPUTS 0x1E

static SemaphoreHandle_t s_lock;
static uint8_t s_outputs = CH422G_SAFE_IDLE_OUTPUTS;
static bool s_initialized;

static esp_err_t write_outputs_locked(void)
{
    return i2c_master_write_to_device(CH422G_I2C_PORT, CH422G_OUTPUT_ADDRESS,
                                      &s_outputs, sizeof(s_outputs), pdMS_TO_TICKS(1000));
}

esp_err_t ch422g_init(void)
{
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
        if (s_lock == NULL) return ESP_ERR_NO_MEM;
    }
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) return ESP_FAIL;
    if (s_initialized) {
        xSemaphoreGive(s_lock);
        return ESP_OK;
    }

    uint8_t config = CH422G_CONFIG_OUTPUT;
    esp_err_t err = i2c_master_write_to_device(CH422G_I2C_PORT, CH422G_CONFIG_ADDRESS,
                                                &config, sizeof(config), pdMS_TO_TICKS(1000));
    if (err == ESP_OK) err = write_outputs_locked();
    if (err == ESP_OK) s_initialized = true;
    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t ch422g_update_outputs(void)
{
    if (!s_initialized || s_lock == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) return ESP_FAIL;
    esp_err_t err = write_outputs_locked();
    xSemaphoreGive(s_lock);
    return err;
}

static esp_err_t set_output_bit(ch422g_io_t io, bool high)
{
    if (!s_initialized || s_lock == NULL || io > CH422G_IO_USB_SEL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) return ESP_FAIL;

    const uint8_t mask = (uint8_t)(1U << io);
    const uint8_t previous = s_outputs;
    if (high) s_outputs |= mask;
    else s_outputs &= (uint8_t)~mask;

    esp_err_t err = write_outputs_locked();
    if (err != ESP_OK) s_outputs = previous;
    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t ch422g_set_bit(ch422g_io_t io) { return set_output_bit(io, true); }
esp_err_t ch422g_clear_bit(ch422g_io_t io) { return set_output_bit(io, false); }
uint8_t ch422g_get_outputs(void) { return s_outputs; }
esp_err_t ch422g_set_backlight(bool enabled) { return enabled ? ch422g_set_bit(CH422G_IO_BACKLIGHT) : ch422g_clear_bit(CH422G_IO_BACKLIGHT); }
esp_err_t ch422g_set_sd_selected(bool selected) { return selected ? ch422g_clear_bit(CH422G_IO_SD_CS) : ch422g_set_bit(CH422G_IO_SD_CS); }
