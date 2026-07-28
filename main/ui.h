#ifndef UI_H
#define UI_H

#include "lvgl.h"
#include <stdbool.h>

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

    // time_t timestamp;
} sensor_data_t;

extern sensor_data_t real_data;
extern bool use_mock_data;
extern bool wifi_is_connected;

void ui_init(void);
void ui_push_sensor_data(sensor_data_t *incoming);

#endif // UI_H