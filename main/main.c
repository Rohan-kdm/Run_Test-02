// /*
//  * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
//  *
//  * SPDX-License-Identifier: CC0-1.0
//  */

// #include "waveshare_rgb_lcd_port.h"
// #include "ui.h"

// // --- Networking Includes ---
// #include "esp_wifi.h"
// #include "esp_event.h"
// #include "nvs_flash.h"
// #include "esp_log.h"
// #include "esp_netif.h"
// #include "esp_sntp.h"
// #include <time.h>
// #include <sys/time.h>
// #include "spiffs.h"

// // --- Wi-Fi Credentials ---
// #define WIFI_SSID "OPRUSS OFFICE"
// #define WIFI_PASS "Water@2024"


// // Link to the global UI flag defined in ui.c
// extern bool wifi_is_connected; 

// // --- Wi-Fi Event Handler ---
// static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
//         esp_wifi_connect();
//     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         wifi_is_connected = false; // UI turns Red "Disconnected"
//         ESP_LOGW(MAIN_TAG, "Wi-Fi disconnected. Reconnecting...");
//         esp_wifi_connect();
//     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
//         ESP_LOGI(MAIN_TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
//         wifi_is_connected = true; // UI turns Green "Connected"
//     }
// }

// // --- Wi-Fi Initialization ---
// void wifi_init_sta(void) {
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     esp_event_handler_instance_t instance_any_id;
//     esp_event_handler_instance_t instance_got_ip;
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid = WIFI_SSID,
//             .password = WIFI_PASS,
//             .threshold.authmode = WIFI_AUTH_WPA2_PSK,
//         },
//     };
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());
// }
// // --- NTP Time Sync Initialization ---
// void time_sync_init(void) {
//     ESP_LOGI("TIME", "Initializing SNTP");
//     esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
//     esp_sntp_setservername(0, "pool.ntp.org"); // Global NTP server
//     esp_sntp_init();
    
//     // Set Timezone to Indian Standard Time (UTC+5:30)
//     // Change "IST-5:30" to your specific timezone string if needed
//     setenv("TZ", "IST-5:30", 1);
//     tzset();
// }
// void app_main()
// {
//     // 1. Initialize NVS (Required for Wi-Fi to store calibration/credentials)
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);

//     // 2. Start Wi-Fi
//     ESP_LOGI(MAIN_TAG, "Initializing Wi-Fi Station");
//     wifi_init_sta();
//     // 2.1 Start Time Sync
//     time_sync_init();
//     // 3. Initialize LCD
//     waveshare_esp32_s3_rgb_lcd_init(); 
//     wavesahre_rgb_lcd_bl_off();
    
//     ESP_LOGI(MAIN_TAG, "Initializing LVGL Engine");
    
//     // Lock the mutex due to the LVGL APIs are not thread-safe
//     if (lvgl_port_lock(-1)) {
//         spiffs_init();
//         ui_init();
//         lvgl_port_unlock();
//     }
//     wavesahre_rgb_lcd_bl_on();
//     /* while (1) {
//         vTaskDelay(pdMS_TO_TICKS(1000));
//        }*/
// }
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "waveshare_rgb_lcd_port.h"
#include "ui.h"

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

static const char *MAIN_TAG = "MAIN_SYS";
SemaphoreHandle_t sensor_mutex = NULL;

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

SemaphoreHandle_t sd_mutex = NULL;

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
#define ALERT_COOLDOWN_MS  (15 * 60 * 1000) // 15-minute alert cooldown to prevent API spam

extern bool wifi_is_connected;

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

    char url[256];
    snprintf(url, sizeof(url), "https://script.google.com/macros/s/%s/exec", GOOGLE_SCRIPT_ID);

    char payload[512];
    snprintf(payload, sizeof(payload), "{\"subject\":\"%s\", \"body\":\"%s\"}", subject, body);

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
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_is_connected = false;
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // wifi_is_connected = true;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI("WIFI",
                "IP: " IPSTR,
                IP2STR(&event->ip_info.ip));

        wifi_is_connected = true;
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

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "OPRUSS OFFICE",
            .password = "Water@2024",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

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

static void sensor_processing_task(void *pvParameters)
{
    sensor_data_t local_data;
    sensor_data_t previous = {0};

    while (1)
    {
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
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    sensor_mutex = xSemaphoreCreateMutex();

    if (sensor_mutex == NULL)
    {
        ESP_LOGE(MAIN_TAG, "Failed to create sensor mutex");
        return;
    }

    sd_mutex = xSemaphoreCreateMutex();

    if (sd_mutex == NULL)
    {
        ESP_LOGE(MAIN_TAG, "Failed to create SD mutex");
        return;
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
    }
    else
    {
        ESP_LOGE(MAIN_TAG,
                "SD Card Mount Failed: %s",
                esp_err_to_name(sd_ret));
    }

    waveshare_esp32_s3_rgb_lcd_init(); 
    wavesahre_rgb_lcd_bl_off(); // Off during boot to avoid white flash
    
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

    wavesahre_rgb_lcd_bl_on(); // On once UI frame is ready
}