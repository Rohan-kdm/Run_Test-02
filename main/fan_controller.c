#include "fan_controller.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "FAN_CTRL";

#define FAN_RPM_MAX 2750 
#define FAN_RPM_MIN 600  

static fan_mode_t current_mode = FAN_MODE_AUTO;
static fan_output_t current_output = { .rpm1 = 0, .rpm2 = 0 };

// Smoothing history variables
static float last_D_total = 0.0f;
static float last_D_fan1 = 0.0f;
static float last_D_fan2 = 0.0f;

#define SMOOTHING_FACTOR 0.15f 

static void apply_output(void)
{
    // Placeholder: Will call update_dimmer_hardware(rpm1, rpm2) here later
    // ESP_LOGI(TAG, "OUTPUT: FAN1=%d RPM | FAN2=%d RPM", current_output.rpm1, current_output.rpm2);
}

void fan_controller_init(void)
{
    current_mode = FAN_MODE_AUTO;
    current_output.rpm1 = 0;
    current_output.rpm2 = 0;
    apply_output();
    ESP_LOGI(TAG, "Fan controller initialized");
}

void fan_controller_set_mode(fan_mode_t mode)
{
    current_mode = mode;
    ESP_LOGI(TAG, "Mode changed to %s", mode == FAN_MODE_AUTO ? "AUTO" : "MANUAL");
}

fan_mode_t fan_controller_get_mode(void)
{
    return current_mode;
}

void fan_controller_set_manual_rpms(int rpm1, int rpm2)
{
    if (rpm1 > FAN_RPM_MAX) rpm1 = FAN_RPM_MAX;
    if (rpm1 < 0) rpm1 = 0;
    if (rpm2 > FAN_RPM_MAX) rpm2 = FAN_RPM_MAX;
    if (rpm2 < 0) rpm2 = 0;

    current_output.rpm1 = rpm1;
    current_output.rpm2 = rpm2;

    apply_output();
    ESP_LOGI(TAG, "MANUAL OVERRIDE: Fan1=%d RPM, Fan2=%d RPM", rpm1, rpm2);
}

void fan_controller_update_auto(float pm25_n, float pm10_n, float tvoc_n, float temp_n, float hum_n)
{
    if (current_mode != FAN_MODE_AUTO) return;

    float raw_D_total = (0.40f * pm25_n) + (0.35f * pm10_n) + (0.15f * tvoc_n) + (0.05f * temp_n) + (0.05f * hum_n);
    float raw_H = ((0.40f * pm25_n) + (0.35f * pm10_n)) / 0.75f;
    float raw_L = ((0.15f * tvoc_n) + (0.05f * temp_n) + (0.05f * hum_n)) / 0.25f;
    float raw_delta = 0.05f * (raw_H - raw_L);

    float D_total = (SMOOTHING_FACTOR * raw_D_total) + ((1.0f - SMOOTHING_FACTOR) * last_D_total);
    last_D_total = D_total; 

    if (D_total < 0.05f) {
        current_output.rpm1 = 0;
        current_output.rpm2 = 0;
        last_D_fan1 = 0.0f; 
        last_D_fan2 = 0.0f;
    } 
    else {
        float raw_D_fan1 = D_total + raw_delta;
        float raw_D_fan2 = D_total - raw_delta;

        float D_fan1 = (SMOOTHING_FACTOR * raw_D_fan1) + ((1.0f - SMOOTHING_FACTOR) * last_D_fan1);
        float D_fan2 = (SMOOTHING_FACTOR * raw_D_fan2) + ((1.0f - SMOOTHING_FACTOR) * last_D_fan2);

        if (D_fan1 > 1.0f) D_fan1 = 1.0f;
        if (D_fan1 < 0.0f) D_fan1 = 0.0f;
        if (D_fan2 > 1.0f) D_fan2 = 1.0f;
        if (D_fan2 < 0.0f) D_fan2 = 0.0f;

        last_D_fan1 = D_fan1;
        last_D_fan2 = D_fan2;

        current_output.rpm1 = FAN_RPM_MIN + (int)(D_fan1 * (FAN_RPM_MAX - FAN_RPM_MIN));
        current_output.rpm2 = FAN_RPM_MIN + (int)(D_fan2 * (FAN_RPM_MAX - FAN_RPM_MIN));
        
        ESP_LOGI(TAG, "60cm Stack Auto | Master: %.2f | Fan1: %d RPM | Fan2: %d RPM", 
                 (double)D_total, current_output.rpm1, current_output.rpm2);
    }
    apply_output();
}

fan_output_t fan_controller_get_output(void)
{
    return current_output;
}

void fan_controller_all_off(void)
{
    current_output.rpm1 = 0;
    current_output.rpm2 = 0;
    apply_output();
}