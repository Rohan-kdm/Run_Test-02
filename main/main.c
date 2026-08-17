
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "waveshare_rgb_lcd_port.h"
#include "ui.h"
#include "settings_nvs.h"
#include "fan_controller.h"

#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_http_client.h"
#include <dirent.h>
#include "spiffs.h"
#include "sd_card.h"
#include "web_download.h"
#include "ch422g.h"
#include "esp_wifi_types.h"
#include "opruss_mqtt.h"
#include "ai_client.h"

static const char *MAIN_TAG = "MAIN_SYSTEM_CORE";
static bool s_logger_ready = true;
SemaphoreHandle_t sensor_mutex = NULL;

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

SemaphoreHandle_t sd_mutex = NULL;
SemaphoreHandle_t spi_mutex = NULL;

// --- Alert Configuration --- whatsapp and email
#define WHATSAPP_PHONE   "+919137626649"  // Replace with recipient phone number
#define CALLMEBOT_API_KEY "YOUR_API_KEY"  // Replace with CallMeBot API Key
#define GOOGLE_SCRIPT_ID "AKfycbyO1K9LUPdSyduRDw6yIWTrKnQefGzuNcIviqFgYWLhElY0qRoMseE9hZBttGrBjG1rgQ" //no https link, just the alphanumerical value in between hettps and /s

// Safety Thresholds
#define THRESHOLD_TEMP_MAX  32.0f // Celsius
#define THRESHOLD_PM25_MAX  60.0f // ug/m3 (CPCB 24h Standard)
#define THRESHOLD_TVOC_MAX  300.0f // ppb

// sensor_data_t latest_sensor_data = {0};

static uint32_t last_alert_time = 0;
#define ALERT_COOLDOWN_MS  (24 * 60 * 60 * 1000) // 15-minute alert cooldown to prevent API spam

extern bool wifi_is_connected;
extern bool ui_is_sensing_enabled(void);

extern void lgfx_hardware_init(void);

    // 1. Reset the Touch IC securely via CH422G
extern void waveshare_esp32_s3_touch_reset(void);

// Alert email configuration (shared with ui.c)
char alert_email[128] = "";
bool email_verified = false;

void ui_restore_alert_email(const char *email)
{
    if (email == NULL)
        return;

    strncpy(
        alert_email,
        email,
        sizeof(alert_email) - 1
    );

    alert_email[
        sizeof(alert_email) - 1
    ] = '\0';

    ESP_LOGI(
        "SETTINGS",
        "Restored alert email: %s",
        alert_email
    );
}

void ui_restore_email_verified(bool verified)
{
    email_verified = verified;

    ESP_LOGI(
        "SETTINGS",
        "Restored email verification: %s",
        verified ? "VERIFIED" : "NOT VERIFIED"
    );
}

// Fan control variables (shared across files)
bool fan_manual_mode = false;
int fan_speed_level = 1;

/* Wi-Fi credentials currently being tested */
static char pending_wifi_ssid[33] = {0};
static char pending_wifi_password[65] = {0};

// --- CPCB Indian AQI Calculation for PM2.5 ---
static int calculate_cpcb_pm25_aqi(float pm25) {
    if (pm25 <= 30.0f) {
        return (int)((50.0f / 30.0f) * pm25);
    } else if (pm25 <= 60.0f) {
        return (int)(51 + ((49.0f / 30.0f) * (pm25 - 30.0f)));
    } else if (pm25 <= 90.0f) {
        return (int)(101 + ((99.0f / 30.0f) * (pm25 - 60.0f)));
    } else if (pm25 <= 120.0f) {
        return (int)(201 + ((99.0f / 30.0f) * (pm25 - 90.0f)));
    } else if (pm25 <= 250.0f) {
        return (int)(301 + ((99.0f / 130.0f) * (pm25 - 120.0f)));
    } else {
        return (int)(401 + ((99.0f / 100.0f) * (pm25 - 250.0f)));
    }
}

void wifi_set_pending_credentials(
    const char *ssid,
    const char *password)
{
    if (ssid == NULL || password == NULL)
        return;

    strncpy(
        pending_wifi_ssid,
        ssid,
        sizeof(pending_wifi_ssid) - 1
    );

    pending_wifi_ssid[
        sizeof(pending_wifi_ssid) - 1
    ] = '\0';

    strncpy(
        pending_wifi_password,
        password,
        sizeof(pending_wifi_password) - 1
    );

    pending_wifi_password[
        sizeof(pending_wifi_password) - 1
    ] = '\0';

    ESP_LOGI(
        "WIFI",
        "Pending WiFi credentials updated for SSID: %s",
        pending_wifi_ssid
    );
}

// --- WhatsApp Alert via CallMeBot ---
static void send_whatsapp_alert(const char *message) {
    if (!wifi_is_connected) return;

    char url[512];
    // Encode message string
    snprintf(url, sizeof(url),
             "https://api.callmebot.com/whatsapp.php?phone=%s&text=%s&apikey=%s",
             WHATSAPP_PHONE, message, CALLMEBOT_API_KEY);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(MAIN_TAG, "WhatsApp Alert Sent Successfully");
    } else {
        ESP_LOGE(MAIN_TAG, "WhatsApp Alert Failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

// --- Email Alert via Google Apps Script ---
static void send_email_alert(const char *subject, const char *body) {
    if (!wifi_is_connected) return;

    // Check if alert email is set
    if (!email_verified || strlen(alert_email) == 0) {
        ESP_LOGW(MAIN_TAG, "Alert email not set - cannot send");
        return;
    }

    // char url[256];
    // snprintf(url, sizeof(url), "https://script.google.com/macros/s/%s/exec", GOOGLE_SCRIPT_ID);
    // Replaced the old Google Script URL with the Catalyst Endpoint
    // Corrected to send-alert! 
    char url[256] = "https://project-rainfall-60082868759.development.catalystserverless.in/server/project_rainfall_function/api/send-alert";

    char payload[1024];
    snprintf(payload, sizeof(payload), 
             "{\"recipient\":\"%s\", \"subject\":\"%s\", \"body\":\"%s\"}", 
             alert_email, subject, body);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 8000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(MAIN_TAG, "Email Alert Sent Successfully");
    } else {
        ESP_LOGE(MAIN_TAG, "Email Alert Failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

// --- Alert Checker ---
// --- Alert Checker ---
static void check_sensor_alerts(sensor_data_t *data) {
    uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
    if (now - last_alert_time < ALERT_COOLDOWN_MS && last_alert_time != 0) {
        return; // In cooldown period
    }

    bool breached = false;
    char alert_msg[256] = "OPRUSS ALERT: ";

    if (data->temp > THRESHOLD_TEMP_MAX) {
        char temp_str[64];
        snprintf(temp_str, sizeof(temp_str), "Temp %.1fC (>%.1f) | ", data->temp, THRESHOLD_TEMP_MAX);
        strcat(alert_msg, temp_str);
        breached = true;
    }
    if (data->pm25 > THRESHOLD_PM25_MAX) {
        char pm_str[64];
        snprintf(pm_str, sizeof(pm_str), "PM2.5 %.1fug/m3 (>%.1f) | ", data->pm25, THRESHOLD_PM25_MAX);
        strcat(alert_msg, pm_str);
        breached = true;
    }
    if (data->tvoc > THRESHOLD_TVOC_MAX) {
        char tvoc_str[64];
        snprintf(tvoc_str, sizeof(tvoc_str), "TVOC %.0fppb (>%.0f) | ", data->tvoc, THRESHOLD_TVOC_MAX);
        strcat(alert_msg, tvoc_str);
        breached = true;
    }

    if (breached) {
        // Remove the trailing " | " for a clean final string
        size_t len = strlen(alert_msg);
        if (len > 3) {
            alert_msg[len - 3] = '\0';
        }

        last_alert_time = now;
        ESP_LOGW(MAIN_TAG, "Limit Breached! Dispatching Alerts: %s", alert_msg);
        
        send_whatsapp_alert(alert_msg);
        send_email_alert("OPRUSS Environmental Alert", alert_msg);
    }
}

// --- ESP-NOW Receive Callback ---
static void espnow_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    char packet_str[128] = {0};
    if (data_len >= sizeof(packet_str)) data_len = sizeof(packet_str) - 1;
    memcpy(packet_str, data, data_len);

    // Parsing CSV string: "PM2.5,Temp,Humidity,Pressure,Gas,TVOC,eCO2"
    sensor_data_t incoming = {0};
    incoming.pm10 = -1.0f; // Missing PM10 flag

    int parsed = sscanf(packet_str, "%f,%f,%f,%f,%f,%f,%f",
                        &incoming.pm25, &incoming.temp, &incoming.humidity,
                        &incoming.pressure, &incoming.gas, &incoming.tvoc, &incoming.eco2);

    if (parsed >= 4) { // Valid payload received
        if (!ui_is_sensing_enabled())
        {
            return;
        }
        incoming.aqi = calculate_cpcb_pm25_aqi(incoming.pm25);

        if (xSemaphoreTake(sensor_mutex, portMAX_DELAY))
        {
            real_data = incoming;

            xSemaphoreGive(sensor_mutex);
        }
        
        // Forward parsed data to LVGL UI
        // ui_push_sensor_data(&incoming);

        // // Check limits for WhatsApp / Email Webhooks
        check_sensor_alerts(&incoming);

        ESP_LOGI(MAIN_TAG, "Recv Sensor Data - PM2.5: %.1f | Temp: %.1fC | Hum: %.1f%% | AQI: %d",
                 incoming.pm25, incoming.temp, incoming.humidity, incoming.aqi);
    } else {
        ESP_LOGE(MAIN_TAG, "Failed to parse CSV payload: %s", packet_str);
    }
}

// --- Wi-Fi Event Handler ---

// static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
//         esp_wifi_connect();
//     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         wifi_event_data_t *event = (wifi_event_data_t *)event_data;
//         wifi_is_connected = false;
        
//         if (event->reason == WIFI_REASON_AUTH_FAIL) {
//             ESP_LOGW("WIFI", "Password incorrect!");
//         } else {
//             ESP_LOGW("WIFI", "Disconnected (reason: %d). Reconnecting...", event->reason);
//         }
//         esp_wifi_connect();
//     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
//         ESP_LOGI("WIFI", "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
//         wifi_is_connected = true;
//     }
// }
// static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
//         esp_wifi_connect();
//     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         wifi_is_connected = false;
//         ESP_LOGW("WIFI", "Disconnected. Reconnecting...");
//         esp_wifi_connect();
//     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
//         ESP_LOGI("WIFI", "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
//         wifi_is_connected = true;
//     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
//         wifi_is_connected = false;
//         ESP_LOGW("WIFI", "Disconnected. Reason: %d", disconn->reason);
//     }
// }
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        wifi_is_connected = false;
        ESP_LOGW("WIFI", "Disconnected. Reason: %d", disconn->reason);
        // Retry after delay
        // vTaskDelay(pdMS_TO_TICKS(5000));
        esp_wifi_connect();
        // DON'T auto-reconnect - user will manually connect from Settings
    }
    else if (
        event_base == IP_EVENT &&
        event_id == IP_EVENT_STA_GOT_IP
    )
    {
        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)event_data;

        ESP_LOGI(
            "WIFI",
            "Connected! IP: " IPSTR,
            IP2STR(&event->ip_info.ip)
        );

        wifi_is_connected = true;


        /* ========================================================
        * SAVE WIFI CREDENTIALS AFTER SUCCESSFUL CONNECTION
        * ======================================================== */

        if (strlen(pending_wifi_ssid) > 0)
        {
            bool saved =
                settings_save_wifi(
                    pending_wifi_ssid,
                    pending_wifi_password
                );

            if (saved)
            {
                ESP_LOGI(
                    "WIFI",
                    "WiFi credentials saved to NVS"
                );
            }
            else
            {
                ESP_LOGE(
                    "WIFI",
                    "Failed to save WiFi credentials to NVS"
                );
            }
        }


        /* ========================================================
        * START MQTT ONLY AFTER GOT_IP
        * ======================================================== */

        mqtt_client_init();
    }
}

void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    // wifi_config_t wifi_config = {
    //     .sta = {
    //         .ssid = "OPRUSS OFFICE",
    //         .password = "Water@2024",
    //         .threshold.authmode = WIFI_AUTH_WPA2_PSK,
    //         .bssid_set = false,
    //     },
    // };

    wifi_config_t wifi_config = {0};

    char saved_ssid[33] = {0};
    char saved_password[65] = {0};

    bool saved_wifi =
        settings_load_wifi(
            saved_ssid,
            sizeof(saved_ssid),
            saved_password,
            sizeof(saved_password)
        );

    if (saved_wifi)
    {
        ESP_LOGI(
            "WIFI",
            "Saved WiFi found: %s",
            saved_ssid
        );

        strncpy(
            (char *)wifi_config.sta.ssid,
            saved_ssid,
            sizeof(wifi_config.sta.ssid) - 1
        );

        strncpy(
            (char *)wifi_config.sta.password,
            saved_password,
            sizeof(wifi_config.sta.password) - 1
        );

        wifi_config.sta.threshold.authmode =
            WIFI_AUTH_WPA2_PSK;

        wifi_config.sta.bssid_set = false;
    }
    else
    {
        ESP_LOGI(
            "WIFI",
            "No saved WiFi credentials"
        );

        /*
        * Leave credentials empty.
        *
        * User will connect from Settings.
        */
    }

    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    if (saved_wifi)
    {
        ESP_ERROR_CHECK(
            esp_wifi_set_config(
                WIFI_IF_STA,
                &wifi_config
            )
        );
    }
    ESP_ERROR_CHECK(esp_wifi_start());

    // Added these lines to Disable power saving (esp was entering power saving and wasnt sending report via mail if idf.py monitor is not running)
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));  // Disable power save mode
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));

        // ===== Print LCD Wi-Fi MAC =====
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));

    ESP_LOGI(MAIN_TAG, "LCD MAC: %02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2],
            mac[3], mac[4], mac[5]);
    // ==============================

    // Initialize ESP-NOW on top of STA interface
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
}

void time_sync_init(void) {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    setenv("TZ", "IST-5:30", 1);
    tzset();
}
static void sd_logging_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(60000); // 1-minute interval

    // Averaging buffers
    float pm25_accum = 0, temp_accum = 0, hum_accum = 0;
    float pressure_accum = 0, gas_accum = 0, tvoc_accum = 0;
    float eco2_accum = 0, pm10_accum = 0;
    int fan_speed_accum = 0, filter_life_accum = 0, aqi_accum = 0;
    int sample_count = 0;

    ESP_LOGI("LOGGER", "SD Logging Task STARTED");

    while (1)
    {
        if (!ui_is_sensing_enabled())
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        // vTaskDelayUntil(&last_wake_time, interval);
        // Collect samples every 10 seconds, log average every 60 seconds
        for (int i = 0; i < 6; i++)
        {
            vTaskDelayUntil(
                &last_wake_time,
                pdMS_TO_TICKS(10000)
            );

            // AQ7 may have entered OFF state while
            // the 60-second logging cycle was running.
            if (!ui_is_sensing_enabled())
            {
                // Discard the incomplete logging cycle.
                pm25_accum = temp_accum = hum_accum = pressure_accum = gas_accum = 0;
                tvoc_accum = eco2_accum = pm10_accum = 0;
                fan_speed_accum = filter_life_accum = aqi_accum = 0;
                sample_count = 0;

                continue;
            }

            sensor_data_t current_data = {0};
            if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                current_data = real_data;
                xSemaphoreGive(sensor_mutex);
            }
            
            // Accumulate for averaging
            pm25_accum += current_data.pm25;
            temp_accum += current_data.temp;
            hum_accum += current_data.humidity;
            pressure_accum += current_data.pressure;
            gas_accum += current_data.gas;
            tvoc_accum += current_data.tvoc;
            eco2_accum += current_data.eco2;
            pm10_accum += current_data.pm10;
            fan_speed_accum += current_data.fan_speed;
            filter_life_accum += current_data.filter_life;
            aqi_accum += current_data.aqi;
            sample_count++;
        }

        // Calculate averages
        // if (sample_count == 0) continue;
        // AQ7 may have entered OFF state during the
        // sampling cycle. Do not write partial data.
        if (!ui_is_sensing_enabled())
        {
            pm25_accum = temp_accum = hum_accum = pressure_accum = gas_accum = 0;
            tvoc_accum = eco2_accum = pm10_accum = 0;
            fan_speed_accum = filter_life_accum = aqi_accum = 0;
            sample_count = 0;

            continue;
        }

        // Calculate averages
        if (sample_count == 0)
            continue;
        
        float avg_pm25 = pm25_accum / sample_count;
        float avg_temp = temp_accum / sample_count;
        float avg_hum = hum_accum / sample_count;
        float avg_pressure = pressure_accum / sample_count;
        float avg_gas = gas_accum / sample_count;
        float avg_tvoc = tvoc_accum / sample_count;
        float avg_eco2 = eco2_accum / sample_count;
        float avg_pm10 = pm10_accum / sample_count;
        int avg_fan = fan_speed_accum / sample_count;
        int avg_filter = filter_life_accum / sample_count;
        int avg_aqi = aqi_accum / sample_count;

        // Reset accumulators
        pm25_accum = temp_accum = hum_accum = pressure_accum = gas_accum = 0;
        tvoc_accum = eco2_accum = pm10_accum = 0;
        fan_speed_accum = filter_life_accum = aqi_accum = 0;
        sample_count = 0;

        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        // Don't log with default 1970 timestamp if NTP hasn't synced yet
        if (timeinfo.tm_year < (2020 - 1900)) {
            ESP_LOGW("LOGGER", "Time not synced yet; skipping log cycle");
            continue;
        }

        // Generate daily filename: YYYYMMDD.csv
        int file_date = (timeinfo.tm_year + 1900) * 10000 + (timeinfo.tm_mon + 1) * 100 + timeinfo.tm_mday;
        char filepath[64];
        snprintf(filepath, sizeof(filepath), "/sdcard/%08d.csv", file_date);

        // if (xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        //     ESP_LOGW("LOGGER", "SPI mutex timeout, skipping log cycle");
        //     continue;
        // }

        // ONLY use sd_mutex, NOT spi_mutex
        if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(2000)) == pdTRUE)
        {

            if (!ui_is_sensing_enabled())
            {
                xSemaphoreGive(sd_mutex);
                continue;
            }
            
            // Select SD card via CH422G
            ch422g_set_sd_selected(true);
            vTaskDelay(pdMS_TO_TICKS(10));  

            // if (!s_logger_ready) {
            //     ch422g_set_sd_selected(false);
            //     xSemaphoreGive(sd_mutex);
            //     vTaskDelay(pdMS_TO_TICKS(1000));
            //     continue;
            // }

            struct stat st;
            bool file_exists = (stat(filepath, &st) == 0);

            // Open file with retry logic using the correct variable name 'filepath'
            FILE *f = NULL;
            int retries = 3;
            while (retries > 0 && f == NULL) {
                f = fopen(filepath, "a");
                if (f == NULL) {
                    retries--;
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }

            if (f == NULL) {
                ESP_LOGE("LOGGER", "Failed to open %s after retries (errno=%d)", filepath, errno);
                ch422g_set_sd_selected(false);
                xSemaphoreGive(sd_mutex);
                // xSemaphoreGive(spi_mutex); 
                continue;
            }
            

            // Write CSV header if file is newly created or empty
            if (!file_exists || st.st_size == 0) {
                fprintf(f, "Date,Time,PM2.5,Temp,Humidity,Pressure,Gas,TVOC,eCO2,PM10,Fan Speed,Filter Life,AQI\n");
            }

            sensor_data_t current_data = {0};
            if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                current_data = real_data;
                xSemaphoreGive(sensor_mutex);
            }

            char date_str[16], time_str[16];
            strftime(date_str, sizeof(date_str), "%Y-%m-%d", &timeinfo);
            strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);

            fprintf(f, "%s,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%d,%d,%d\n",
                    date_str, time_str,
                    current_data.pm25, current_data.temp, current_data.humidity,
                    current_data.pressure, current_data.gas, current_data.tvoc,
                    current_data.eco2, current_data.pm10, current_data.fan_speed,
                    current_data.filter_life, current_data.aqi);

            fclose(f);
            ESP_LOGI("LOGGER", "Appended sensor data to %s", filepath);
            
            ch422g_set_sd_selected(false);
            xSemaphoreGive(sd_mutex);
            // xSemaphoreGive(spi_mutex); 
        }
        else {
            ESP_LOGW("LOGGER", "SD mutex timeout, skipping log cycle");
        }
    }
}

static void sensor_processing_task(void *pvParameters)
{
    sensor_data_t local_data;
    sensor_data_t previous = {0};

    while (1)
    {
        if (!ui_is_sensing_enabled())
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            local_data = real_data;
            xSemaphoreGive(sensor_mutex);

            if (memcmp(&local_data, &previous, sizeof(sensor_data_t)) != 0)
            {
                previous = local_data;

                ui_push_sensor_data(&local_data);

                check_sensor_alerts(&local_data);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void app_main() {

    spi_mutex = xSemaphoreCreateMutex();
    if (spi_mutex == NULL) {
        ESP_LOGE("MAIN", "Failed to create SPI mutex");
        return;
    }

    sd_mutex = xSemaphoreCreateMutex();
    if (sd_mutex == NULL) {
        ESP_LOGE("MAIN", "Failed to create SD mutex");
        return;
    }

    sensor_mutex = xSemaphoreCreateMutex();
    if (sensor_mutex == NULL) {
        ESP_LOGE("MAIN", "Failed to create sensor mutex");
        return;
    }

    // Reset I2C bus before initialization
    gpio_reset_pin(GPIO_NUM_8);
    gpio_reset_pin(GPIO_NUM_9);
    vTaskDelay(pdMS_TO_TICKS(200));

    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    settings_nvs_init();

    bool saved_purifier_state = true;
    int saved_filter_life = 100;

    if (settings_load_purifier_status(
            &saved_purifier_state,
            &saved_filter_life))
    {
        ui_restore_purifier_state(
            saved_purifier_state,
            saved_filter_life
        );
    }

    /* ============================================================
    * RESTORE PERSISTENT SETTINGS
    * ============================================================ */

    /* ---------------- EMAIL ---------------- */

    char saved_email[128] = {0};

    if (settings_load_alert_email(
            saved_email,
            sizeof(saved_email)))
    {
        // strncpy(
        //     alert_email,
        //     saved_email,
        //     sizeof(alert_email) - 1
        // );

        // alert_email[
        //     sizeof(alert_email) - 1
        // ] = '\0';
        ui_restore_alert_email(saved_email);
    }


    bool saved_email_verified = false;

    if (settings_load_email_verified(
            &saved_email_verified))
    {
        ui_restore_email_verified(
            saved_email_verified
        );
    }


    int64_t saved_email_timestamp = 0;

    if (settings_load_email_change_timestamp(
            &saved_email_timestamp))
    {
        /*
        * email_change_timestamp lives in ui.c,
        * so restore it through a UI function.
        
        * We will add that function shortly.
        */
        ui_restore_email_timestamp(
         saved_email_timestamp
        );
    }



    /* ---------------- FAN ---------------- */

    bool saved_fan_manual = false;
    int saved_fan_speed = 1;

    if (settings_load_fan_config(
            &saved_fan_manual,
            &saved_fan_speed))
    {
        fan_manual_mode =
            saved_fan_manual;

        fan_speed_level =
            saved_fan_speed;

        /* Safety clamp */
        if (fan_speed_level < 0)
            fan_speed_level = 0;

        if (fan_speed_level > 3)
            fan_speed_level = 3;
    }

    /* ============================================================
    * INITIALIZE FAN CONTROLLER
    * ============================================================ */

    fan_controller_init();

    /*
    * Restore persisted fan mode into the new controller.
    */
    fan_controller_set_mode(
        saved_fan_manual
            ? FAN_MODE_MANUAL
            : FAN_MODE_AUTO
    );

    /*
    * If the saved mode is MANUAL,
    * restore the saved fan level.
    */
    if (saved_fan_manual)
    {
        extern const int fan_speed_rpm[];
        fan_controller_set_manual_rpms(
            fan_speed_rpm[fan_speed_level],
            fan_speed_rpm[fan_speed_level]
        );
    }


    /* ---------------- SENSOR SNAPSHOT ---------------- */

    settings_sensor_snapshot_t snapshot;

    if (settings_load_sensor_snapshot(&snapshot))
    {
        real_data.aqi =
            snapshot.aqi;

        real_data.pm25 =
            snapshot.pm25;

        real_data.pm10 =
            snapshot.pm10;

        real_data.tvoc =
            snapshot.tvoc;

        real_data.temp =
            snapshot.temp;

        real_data.humidity =
            snapshot.humidity;

        real_data.pressure =
            snapshot.pressure;

        real_data.eco2 =
            snapshot.eco2;

        real_data.fan_speed =
            snapshot.fan_speed;

        real_data.filter_life =
            snapshot.filter_life;
    }

    wifi_init_sta();
    time_sync_init();

    ESP_LOGI(MAIN_TAG, "Initializing SD Card...");

    esp_err_t sd_ret = waveshare_sd_card_init();
    DIR *dir = opendir("/sdcard");

    if (dir) {
        ESP_LOGI("TEST", "SD OK immediately after mount");

        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            ESP_LOGI("TEST", "%s", ent->d_name);
        }

        closedir(dir);
    } else {
        ESP_LOGE("TEST", "Cannot open SD immediately");
    }

    if (sd_ret == ESP_OK)
    {
        ESP_LOGI(MAIN_TAG, "SD Card Mounted Successfully");
        start_webserver();  
        xTaskCreatePinnedToCore(
            sd_logging_task,
            "sd_logging_task",
            4096,
            NULL,
            4,
            NULL,
            1
        );
    }
    else
    {
        ESP_LOGE(MAIN_TAG,
                "SD Card Mount Failed: %s",
                esp_err_to_name(sd_ret));
    }
            

    // 1. Initialize the I2C Bus FIRST
    extern esp_err_t i2c_master_init(void);
    i2c_master_init();

    // 2. Reset the Touch IC securely via CH422G
    extern void waveshare_esp32_s3_touch_reset(void);
    waveshare_esp32_s3_touch_reset();

    // 3. Initialize LovyanGFX Hardware (Now it will find the I2C bus!)
    extern void lgfx_hardware_init(void);
    lgfx_hardware_init();

    // Turn backlight off during boot to avoid white flash
    extern esp_err_t wavesahre_rgb_lcd_bl_off(void);
    wavesahre_rgb_lcd_bl_off();

    // 4. Initialize LVGL with the new port
    lvgl_port_init();

    if (lvgl_port_lock(-1)) {
        spiffs_init();
        ui_init();
        lvgl_port_unlock();
    }
    
    // sensor_mutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(
        sensor_processing_task,
        "sensor_processing",
        4096,
        NULL,
        5,
        NULL,
        1
    );

    // Turn backlight on once UI frame is ready
    extern esp_err_t wavesahre_rgb_lcd_bl_on(void);
    wavesahre_rgb_lcd_bl_on();

    // ===== ADD MQTT =====
    ai_client_init();
    // mqtt_client_init();
}

bool spi_take(void) {
    return xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(1000)) == pdTRUE;
}

void spi_give(void) {
    xSemaphoreGive(spi_mutex);
}