#include "settings_nvs.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "NVS_SETTINGS";

static nvs_handle_t settings_handle = 0;

static bool settings_initialized = false;


/*
 * ============================================================
 * INITIALIZATION
 * ============================================================
 */

void settings_nvs_init(void)
{
    if (settings_initialized)
        return;

    esp_err_t err;

    err = nvs_open(
        "opruss_settings",
        NVS_READWRITE,
        &settings_handle
    );

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to open settings NVS: %s",
            esp_err_to_name(err)
        );

        settings_initialized = false;
        return;
    }

    settings_initialized = true;

    ESP_LOGI(
        TAG,
        "OPRUSS settings NVS initialized"
    );
}


/*
 * ============================================================
 * WIFI
 * ============================================================
 */

bool settings_save_wifi(
    const char *ssid,
    const char *password)
{
    if (!settings_initialized ||
        ssid == NULL ||
        password == NULL)
    {
        return false;
    }

    esp_err_t err;

    err = nvs_set_str(
        settings_handle,
        "wifi_ssid",
        ssid
    );

    if (err != ESP_OK)
        return false;


    err = nvs_set_str(
        settings_handle,
        "wifi_pass",
        password
    );

    if (err != ESP_OK)
        return false;


    err = nvs_set_u8(
        settings_handle,
        "wifi_saved",
        1
    );

    if (err != ESP_OK)
        return false;


    err = nvs_commit(
        settings_handle
    );

    if (err != ESP_OK)
        return false;


    ESP_LOGI(
        TAG,
        "WiFi credentials saved for SSID: %s",
        ssid
    );

    return true;
}


bool settings_load_wifi(
    char *ssid,
    size_t ssid_len,
    char *password,
    size_t password_len)
{
    if (!settings_initialized ||
        ssid == NULL ||
        password == NULL)
    {
        return false;
    }

    uint8_t saved = 0;

    if (nvs_get_u8(
            settings_handle,
            "wifi_saved",
            &saved) != ESP_OK)
    {
        return false;
    }

    if (!saved)
        return false;


    size_t len = ssid_len;

    if (nvs_get_str(
            settings_handle,
            "wifi_ssid",
            ssid,
            &len) != ESP_OK)
    {
        return false;
    }


    len = password_len;

    if (nvs_get_str(
            settings_handle,
            "wifi_pass",
            password,
            &len) != ESP_OK)
    {
        return false;
    }


    return true;
}


bool settings_wifi_exists(void)
{
    if (!settings_initialized)
        return false;

    uint8_t saved = 0;

    if (nvs_get_u8(
            settings_handle,
            "wifi_saved",
            &saved) != ESP_OK)
    {
        return false;
    }

    return saved != 0;
}


bool settings_clear_wifi(void)
{
    if (!settings_initialized)
        return false;

    nvs_erase_key(
        settings_handle,
        "wifi_ssid"
    );

    nvs_erase_key(
        settings_handle,
        "wifi_pass"
    );

    nvs_erase_key(
        settings_handle,
        "wifi_saved"
    );

    return
        nvs_commit(settings_handle) == ESP_OK;
}


/*
 * ============================================================
 * ALERT EMAIL
 * ============================================================
 */

bool settings_save_alert_email(
    const char *email)
{
    if (!settings_initialized ||
        email == NULL)
    {
        return false;
    }

    if (nvs_set_str(
            settings_handle,
            "alert_email",
            email) != ESP_OK)
    {
        return false;
    }

    return
        nvs_commit(settings_handle) == ESP_OK;
}


bool settings_load_alert_email(
    char *email,
    size_t len)
{
    if (!settings_initialized ||
        email == NULL)
    {
        return false;
    }

    size_t required = len;

    return
        nvs_get_str(
            settings_handle,
            "alert_email",
            email,
            &required
        ) == ESP_OK;
}


bool settings_save_email_verified(
    bool verified)
{
    if (!settings_initialized)
        return false;

    if (nvs_set_u8(
            settings_handle,
            "email_verified",
            verified ? 1 : 0) != ESP_OK)
    {
        return false;
    }

    return
        nvs_commit(settings_handle) == ESP_OK;
}


bool settings_load_email_verified(
    bool *verified)
{
    if (!settings_initialized ||
        verified == NULL)
    {
        return false;
    }

    uint8_t value;

    if (nvs_get_u8(
            settings_handle,
            "email_verified",
            &value) != ESP_OK)
    {
        return false;
    }

    *verified = (value != 0);

    return true;
}

bool settings_save_email_change_timestamp(
    int64_t timestamp)
{
    if (!settings_initialized)
        return false;

    esp_err_t err = nvs_set_i64(
        settings_handle,
        "email_chg_at",
        timestamp
    );

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to save email timestamp: %s",
            esp_err_to_name(err)
        );
        return false;
    }

    err = nvs_commit(settings_handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to commit email timestamp: %s",
            esp_err_to_name(err)
        );
        return false;
    }

    return true;
}

bool settings_load_email_change_timestamp(
    int64_t *timestamp)
{
    if (!settings_initialized ||
        timestamp == NULL)
    {
        return false;
    }

    esp_err_t err = nvs_get_i64(
        settings_handle,
        "email_chg_at",
        timestamp
    );

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        *timestamp = 0;
        return false;
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to load email timestamp: %s",
            esp_err_to_name(err)
        );
        return false;
    }

    return true;
}

/*
 * ============================================================
 * FAN
 * ============================================================
 */

bool settings_save_fan_config(
    bool manual_mode,
    int speed_level)
{
    if (!settings_initialized)
        return false;

    if (nvs_set_u8(
            settings_handle,
            "fan_manual",
            manual_mode ? 1 : 0) != ESP_OK)
    {
        return false;
    }

    if (nvs_set_i32(
            settings_handle,
            "fan_speed",
            speed_level) != ESP_OK)
    {
        return false;
    }

    if (nvs_commit(
            settings_handle) != ESP_OK)
    {
        return false;
    }

    ESP_LOGI(
        TAG,
        "Fan saved: %s / level %d",
        manual_mode ? "MANUAL" : "AUTO",
        speed_level
    );

    return true;
}


bool settings_load_fan_config(
    bool *manual_mode,
    int *speed_level)
{
    if (!settings_initialized ||
        manual_mode == NULL ||
        speed_level == NULL)
    {
        return false;
    }

    uint8_t mode;

    if (nvs_get_u8(
            settings_handle,
            "fan_manual",
            &mode) != ESP_OK)
    {
        return false;
    }

    if (nvs_get_i32(
            settings_handle,
            "fan_speed",
            speed_level) != ESP_OK)
    {
        return false;
    }

    *manual_mode = (mode != 0);

    return true;
}


/*
 * ============================================================
 * PURIFIER
 * ============================================================
 */

bool settings_save_purifier_status(
    bool state,
    int filter_life)
{
    if (!settings_initialized)
        return false;

    if (nvs_set_u8(
            settings_handle,
            "purifier_on",
            state ? 1 : 0) != ESP_OK)
    {
        return false;
    }

    if (nvs_set_i32(
            settings_handle,
            "filter_life",
            filter_life) != ESP_OK)
    {
        return false;
    }

    if (nvs_commit(
            settings_handle) != ESP_OK)
    {
        return false;
    }

    ESP_LOGI(
        TAG,
        "Purifier saved: state=%d filter=%d",
        state,
        filter_life
    );

    return true;
}


bool settings_load_purifier_status(
    bool *state,
    int *filter_life)
{
    if (!settings_initialized ||
        state == NULL ||
        filter_life == NULL)
    {
        return false;
    }

    uint8_t value;

    if (nvs_get_u8(
            settings_handle,
            "purifier_on",
            &value) != ESP_OK)
    {
        return false;
    }

    if (nvs_get_i32(
            settings_handle,
            "filter_life",
            filter_life) != ESP_OK)
    {
        return false;
    }

    *state = (value != 0);

    return true;
}


/*
 * ============================================================
 * BRIGHTNESS
 * ============================================================
 */

// bool settings_save_brightness(
//     int brightness)
// {
//     if (!settings_initialized)
//         return false;

//     if (nvs_set_i32(
//             settings_handle,
//             "brightness",
//             brightness) != ESP_OK)
//     {
//         return false;
//     }

//     return
//         nvs_commit(settings_handle) == ESP_OK;
// }


// bool settings_load_brightness(
//     int *brightness)
// {
//     if (!settings_initialized ||
//         brightness == NULL)
//     {
//         return false;
//     }

//     return
//         nvs_get_i32(
//             settings_handle,
//             "brightness",
//             brightness
//         ) == ESP_OK;
// }


/*
 * ============================================================
 * SCREEN 3
 * ============================================================
 *
 * selected_range:
 *
 * 0 = Today
 * 1 = Yesterday
 * 2 = Last Week
 * 3 = Last Month
 * 4 = Custom
 *
 * Adjust these numbers if your UI uses a different mapping.
 */

bool settings_save_screen3_state(
    bool live_mode,
    int selected_range)
{
    if (!settings_initialized)
        return false;

    if (nvs_set_u8(
            settings_handle,
            "s3_live",
            live_mode ? 1 : 0) != ESP_OK)
    {
        return false;
    }

    if (nvs_set_i32(
            settings_handle,
            "s3_range",
            selected_range) != ESP_OK)
    {
        return false;
    }

    return
        nvs_commit(settings_handle) == ESP_OK;
}


bool settings_load_screen3_state(
    bool *live_mode,
    int *selected_range)
{
    if (!settings_initialized ||
        live_mode == NULL ||
        selected_range == NULL)
    {
        return false;
    }

    uint8_t live;

    if (nvs_get_u8(
            settings_handle,
            "s3_live",
            &live) != ESP_OK)
    {
        return false;
    }

    if (nvs_get_i32(
            settings_handle,
            "s3_range",
            selected_range) != ESP_OK)
    {
        return false;
    }

    *live_mode = (live != 0);

    return true;
}

/*
 * ============================================================
 * SCREEN 3 CUSTOM RANGE
 * ============================================================
 *
 * Date format:
 * YYYYMMDD
 *
 * Example:
 * 20260801
 */

bool settings_save_screen3_custom_range(
    int start_day,
    int end_day)
{
    if (!settings_initialized)
        return false;

    if (start_day <= 0 || end_day <= 0)
        return false;

    if (nvs_set_u32(
            settings_handle,
            "s3_custom_start",
            (uint32_t)start_day) != ESP_OK)
    {
        return false;
    }

    if (nvs_set_u32(
            settings_handle,
            "s3_custom_end",
            (uint32_t)end_day) != ESP_OK)
    {
        return false;
    }

    return
        nvs_commit(settings_handle) == ESP_OK;
}


bool settings_load_screen3_custom_range(
    int *start_day,
    int *end_day)
{
    if (!settings_initialized ||
        start_day == NULL ||
        end_day == NULL)
    {
        return false;
    }

    uint32_t start = 0;
    uint32_t end = 0;

    if (nvs_get_u32(
            settings_handle,
            "s3_custom_start",
            &start) != ESP_OK)
    {
        return false;
    }

    if (nvs_get_u32(
            settings_handle,
            "s3_custom_end",
            &end) != ESP_OK)
    {
        return false;
    }

    *start_day = (int)start;
    *end_day = (int)end;

    return true;
}


/*
 * ============================================================
 * SLEEP STATE
 * ============================================================
 */

bool settings_save_sleep_state(
    bool active,
    bool schedule_mode,
    uint64_t wake_epoch,
    int timer_minutes)
{
    if (!settings_initialized)
        return false;

    if (nvs_set_u8(
            settings_handle,
            "sleep_active",
            active ? 1 : 0) != ESP_OK)
    {
        return false;
    }

    if (nvs_set_u8(
            settings_handle,
            "sleep_sched",
            schedule_mode ? 1 : 0) != ESP_OK)
    {
        return false;
    }

    if (nvs_set_u64(
            settings_handle,
            "sleep_wake",
            wake_epoch) != ESP_OK)
    {
        return false;
    }

    if (nvs_set_i32(
            settings_handle,
            "sleep_min",
            timer_minutes) != ESP_OK)
    {
        return false;
    }

    return
        nvs_commit(settings_handle) == ESP_OK;
}


bool settings_load_sleep_state(
    bool *active,
    bool *schedule_mode,
    uint64_t *wake_epoch,
    int *timer_minutes)
{
    if (!settings_initialized ||
        active == NULL ||
        schedule_mode == NULL ||
        wake_epoch == NULL ||
        timer_minutes == NULL)
    {
        return false;
    }

    uint8_t active_value;
    uint8_t schedule_value;

    if (nvs_get_u8(
            settings_handle,
            "sleep_active",
            &active_value) != ESP_OK)
    {
        return false;
    }

    if (nvs_get_u8(
            settings_handle,
            "sleep_sched",
            &schedule_value) != ESP_OK)
    {
        return false;
    }

    if (nvs_get_u64(
            settings_handle,
            "sleep_wake",
            wake_epoch) != ESP_OK)
    {
        return false;
    }

    if (nvs_get_i32(
            settings_handle,
            "sleep_min",
            timer_minutes) != ESP_OK)
    {
        return false;
    }

    *active = active_value != 0;
    *schedule_mode = schedule_value != 0;

    return true;
}


bool settings_clear_sleep_state(void)
{
    if (!settings_initialized)
        return false;

    nvs_erase_key(settings_handle, "sleep_active");
    nvs_erase_key(settings_handle, "sleep_sched");
    nvs_erase_key(settings_handle, "sleep_wake");
    nvs_erase_key(settings_handle, "sleep_min");

    return
        nvs_commit(settings_handle) == ESP_OK;
}


/*
 * ============================================================
 * DEVICE ID
 * ============================================================
 */

bool settings_save_device_id(
    const char *device_id)
{
    if (!settings_initialized ||
        device_id == NULL)
    {
        return false;
    }

    if (nvs_set_str(
            settings_handle,
            "device_id",
            device_id) != ESP_OK)
    {
        return false;
    }

    return
        nvs_commit(settings_handle) == ESP_OK;
}


bool settings_load_device_id(
    char *device_id,
    size_t len)
{
    if (!settings_initialized ||
        device_id == NULL)
    {
        return false;
    }

    size_t required = len;

    return
        nvs_get_str(
            settings_handle,
            "device_id",
            device_id,
            &required
        ) == ESP_OK;
}


/*
 * ============================================================
 * LAST SENSOR SNAPSHOT
 * ============================================================
 */

bool settings_save_sensor_snapshot(
    const settings_sensor_snapshot_t *snapshot)
{
    if (!settings_initialized ||
        snapshot == NULL)
    {
        return false;
    }

    if (nvs_set_blob(
            settings_handle,
            "sensor_snapshot",
            snapshot,
            sizeof(settings_sensor_snapshot_t)
        ) != ESP_OK)
    {
        return false;
    }

    return
        nvs_commit(settings_handle) == ESP_OK;
}


bool settings_load_sensor_snapshot(
    settings_sensor_snapshot_t *snapshot)
{
    if (!settings_initialized ||
        snapshot == NULL)
    {
        return false;
    }

    size_t size =
        sizeof(settings_sensor_snapshot_t);

    return
        nvs_get_blob(
            settings_handle,
            "sensor_snapshot",
            snapshot,
            &size
        ) == ESP_OK;
}