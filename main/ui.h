#ifndef UI_H
#define UI_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>
#include "air_quality_model.h"


bool ui_get_normalized_air_data(
    normalized_air_data_t *out
);

bool ui_get_caqi(
    float *caqi
);

typedef struct {
    float pm25;

    float temp;
    float humidity;

    float pressure;
    float gas;
    float tvoc;
    float eco2;

    float pm10;      // -1 if unavailable
    int fan_speed;
    int filter_life;

    int aqi;
    int fan;     // Add this line
    int filter;

    // time_t timestamp;
} sensor_data_t;


extern sensor_data_t real_data;
extern bool use_mock_data;
extern bool wifi_is_connected;
extern lv_obj_t *ui_CheckboxStats;
extern lv_obj_t *ui_CheckboxRaw;

void ui_init(void);
void ui_push_sensor_data(sensor_data_t *incoming);
bool ui_has_sensor_data(void);


void ui_restore_alert_email(const char *email);

void ui_restore_email_verified(bool verified);


void ui_restore_email_timestamp(
    int64_t timestamp
);

void wifi_set_pending_credentials(
    const char *ssid,
    const char *password
);

void ui_restore_purifier_state(
    bool state,
    int filter_life
);

int ui_get_fan_rpm(void);
const char *ui_get_fan_mode(void);
const char *ui_get_fan_speed_name(void);
bool ui_is_sensing_enabled(void);

#endif // UI_H