#ifndef SETTINGS_NVS_H
#define SETTINGS_NVS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================
 * INITIALIZATION
 * ============================================================ */

void settings_nvs_init(void);


/* ============================================================
 * WIFI
 * ============================================================ */

bool settings_save_wifi(
    const char *ssid,
    const char *password
);

bool settings_load_wifi(
    char *ssid,
    size_t ssid_len,
    char *password,
    size_t password_len
);

bool settings_wifi_exists(void);

bool settings_clear_wifi(void);


/* ============================================================
 * ALERT EMAIL
 * ============================================================ */

bool settings_save_alert_email(
    const char *email
);

bool settings_load_alert_email(
    char *email,
    size_t len
);


bool settings_save_email_verified(
    bool verified
);

bool settings_load_email_verified(
    bool *verified
);

bool settings_save_email_change_timestamp(
    int64_t timestamp
);

bool settings_load_email_change_timestamp(
    int64_t *timestamp
);


/* ============================================================
 * FAN
 * ============================================================ */

bool settings_save_fan_config(
    bool manual_mode,
    int speed_level
);

bool settings_load_fan_config(
    bool *manual_mode,
    int *speed_level
);


/* ============================================================
 * PURIFIER
 * ============================================================ */

bool settings_save_purifier_status(
    bool state,
    int filter_life
);

bool settings_load_purifier_status(
    bool *state,
    int *filter_life
);

/*
 * ============================================================
 * SCREEN 3
 * ============================================================
 */

bool settings_save_screen3_state(
    bool live_mode,
    int selected_range
);

bool settings_load_screen3_state(
    bool *live_mode,
    int *selected_range
);

bool settings_save_screen3_custom_range(
    int start_day,
    int end_day
);

bool settings_load_screen3_custom_range(
    int *start_day,
    int *end_day
);


/* ============================================================
 * SLEEP TIMER
 * ============================================================ */

// bool settings_save_sleep_timer(
//     int minutes,
//     bool schedule_mode,
//     int wake_hour,
//     int wake_min
// );

// bool settings_load_sleep_timer(
//     int *minutes,
//     bool *schedule_mode,
//     int *wake_hour,
//     int *wake_min
// );

/*
 * ============================================================
 * SLEEP / POWER MANAGEMENT
 * ============================================================
 */

bool settings_save_sleep_state(
    bool active,
    bool schedule_mode,
    uint64_t wake_epoch,
    int timer_minutes
);

bool settings_load_sleep_state(
    bool *active,
    bool *schedule_mode,
    uint64_t *wake_epoch,
    int *timer_minutes
);

bool settings_clear_sleep_state(void);


/* ============================================================
 * DEVICE ID
 * ============================================================ */

bool settings_save_device_id(
    const char *device_id
);

bool settings_load_device_id(
    char *device_id,
    size_t len
);

/*
 * ============================================================
 * LAST SENSOR SNAPSHOT
 *
 * Used only for restoring the UI immediately after reboot.
 * Historical data remains on SD card.
 * ============================================================
 */

typedef struct
{
    int aqi;
    float pm25;
    float pm10;
    float tvoc;
    float temp;
    float humidity;
    float pressure;
    float eco2;
    int fan_speed;
    int filter_life;

} settings_sensor_snapshot_t;


bool settings_save_sensor_snapshot(
    const settings_sensor_snapshot_t *snapshot
);

bool settings_load_sensor_snapshot(
    settings_sensor_snapshot_t *snapshot
);



/* ============================================================
 * OPTIONAL: DISPLAY SETTINGS
 * ============================================================ */

// bool settings_save_brightness(
//     int brightness
// );

// bool settings_load_brightness(
//     int *brightness
// );

#endif