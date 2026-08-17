#include "ch422g.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#define CH422G_CONFIG_ADDRESS 0x24
#define CH422G_OUTPUT_ADDRESS 0x38
#define CH422G_CONFIG_OUTPUT 0x01
#define CH422G_SAFE_IDLE_OUTPUTS 0x0E 

extern i2c_master_bus_handle_t shared_i2c_bus;

static i2c_master_dev_handle_t ch422g_conf_dev = NULL;
static i2c_master_dev_handle_t ch422g_out_dev = NULL;

static SemaphoreHandle_t s_lock;
static uint8_t s_outputs = CH422G_SAFE_IDLE_OUTPUTS;
static bool s_initialized;

static esp_err_t write_outputs_locked(void)
{
    return i2c_master_transmit(ch422g_out_dev, &s_outputs, 1, -1);
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

    if (shared_i2c_bus == NULL) {
        ESP_LOGE("CH422G", "I2C Bus not initialized!");
        xSemaphoreGive(s_lock);
        return ESP_ERR_INVALID_STATE;
    }

    // Register the two CH422G addresses to the modern bus
    if (ch422g_conf_dev == NULL) {
        i2c_device_config_t conf_cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = CH422G_CONFIG_ADDRESS, .scl_speed_hz = 400000 };
        i2c_master_bus_add_device(shared_i2c_bus, &conf_cfg, &ch422g_conf_dev);

        i2c_device_config_t out_cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = CH422G_OUTPUT_ADDRESS, .scl_speed_hz = 400000 };
        i2c_master_bus_add_device(shared_i2c_bus, &out_cfg, &ch422g_out_dev);
    }

    // Send the config byte
    uint8_t config = CH422G_CONFIG_OUTPUT;
    esp_err_t err = i2c_master_transmit(ch422g_conf_dev, &config, 1, -1);
    
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
    if (!s_initialized || s_lock == NULL) return ESP_ERR_INVALID_STATE;
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