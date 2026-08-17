#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    FAN_MODE_MANUAL = 0,
    FAN_MODE_AUTO = 1
} fan_mode_t;

typedef struct
{
    int rpm1;
    int rpm2;
} fan_output_t;

void fan_controller_init(void);
void fan_controller_set_mode(fan_mode_t mode);
fan_mode_t fan_controller_get_mode(void);
void fan_controller_set_manual_rpms(int rpm1, int rpm2);
void fan_controller_update_auto(float pm25_n, float pm10_n, float tvoc_n, float temp_n, float hum_n);
fan_output_t fan_controller_get_output(void);
void fan_controller_all_off(void);

#ifdef __cplusplus
}
#endif

#endif