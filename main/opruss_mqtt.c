#include "opruss_mqtt.h"
#include "aq_voice_engine.h"
#include "ui.h"                 
#include "lvgl.h"  

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ai_router.h"
#include "ai_client.h"

#include "sd_context.h"

static const char *TAG = "MQTT";

#define MQTT_BROKER_URI "mqtt://broker.hivemq.com:1883"
// #define g_topic_command  "opruss/command"
// #define g_topic_response "opruss/response"
// #define g_topic_livedata "opruss/livedata"

// Device-specific topic buffers
static char g_topic_command[64] = {0};
static char g_topic_response[64] = {0};
static char g_topic_livedata[64] = {0};
static char g_device_id[13] = {0};  // 12 hex chars + null
static char g_topic_request_data[64] = {0};

static esp_mqtt_client_handle_t mqtt_handle = NULL;

static bool mqtt_connected = false;

// Current fan status
static char g_fan_mode[16] = "Unknown";
static char g_fan_speed[16] = "Unknown";
static int g_fan_rpm = 0;
static char g_topic_historical[64] = {0};

extern lv_obj_t *voice_response_lbl;
extern lv_obj_t *voice_status_lbl;
extern SemaphoreHandle_t sensor_mutex;
extern bool fan_manual_mode;
extern int fan_speed_level;
extern const int fan_speed_rpm[];

typedef struct {
    char question[256];
    char session_id[16];
} cloud_task_args_t;


// Forward declaration
static void mqtt_cloud_response_cb(const char *response, bool success, void *user_data);
static void mqtt_cloud_task(void *pv);
static bool g_msg_from_app = false;

// Generate device ID from MAC
static void generate_device_id(void) {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(g_device_id, sizeof(g_device_id), 
             "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // snprintf(g_topic_command, sizeof(g_topic_command), "opruss/%s/command", g_device_id);
    snprintf(g_topic_command, sizeof(g_topic_command), "opruss/%s/command/+", g_device_id);
    snprintf(g_topic_response, sizeof(g_topic_response), "opruss/%s/response", g_device_id);
    snprintf(g_topic_livedata, sizeof(g_topic_livedata), "opruss/%s/livedata", g_device_id);
    snprintf(g_topic_request_data, sizeof(g_topic_request_data), "opruss/%s/request_data", g_device_id);
    
    snprintf(g_topic_historical, sizeof(g_topic_historical), "opruss/%s/historical", g_device_id);
    
    ESP_LOGI(TAG, "Device ID: %s", g_device_id);
}

// NEW: Function to send the AI response to a specific user's channel
void mqtt_publish_private_response(const char *response, const char *session_id) {
    if (!mqtt_connected || response == NULL || session_id == NULL) return;
    char private_topic[128];
    snprintf(private_topic, sizeof(private_topic), "opruss/%s/response/%s", g_device_id, session_id);
    esp_mqtt_client_publish(mqtt_handle, private_topic, response, 0, 1, 0);
}

const char *mqtt_get_device_id(void) {
    return g_device_id;
}

// Wrapper to call cloud from a safe task
// static void mqtt_cloud_task(void *pv) {
//     char *question = (char *)pv;
    
//     // Call cloud and wait for callback to publish response
//     if (!ai_client_ask(question, mqtt_cloud_response_cb, NULL)) {
//         // If cloud fails, send fallback message
//         mqtt_publish_response("Cloud AI is thinking...");
//     }
    
//     // Wait for HTTP response to complete
//     vTaskDelay(pdMS_TO_TICKS(15000));
    
//     free(question);
//     vTaskDelete(NULL);
// }

// // Cloud response callback for MQTT
// static void mqtt_cloud_response_cb(const char *response, bool success, void *user_data) {
//     if (response && mqtt_connected) {
//         mqtt_publish_response(response);
//     } else if (!success && mqtt_connected) {
//         mqtt_publish_response("Cloud AI unavailable. Try again.");
//     }
// }

// Cloud response callback for MQTT
static void mqtt_cloud_response_cb(const char *response, bool success, void *user_data) {
    char *session_id = (char *)user_data; // Extract the session ID from the callback
    
    if (response && mqtt_connected) {
        mqtt_publish_private_response(response, session_id);
    } else if (!success && mqtt_connected) {
        mqtt_publish_private_response("Cloud AI unavailable. Try again.", session_id);
    }
    
    // Free the allocated memory now that the response is sent
    if (session_id) free(session_id);
}

// Wrapper to call cloud from a safe task
static void mqtt_cloud_task(void *pv) {
    cloud_task_args_t *args = (cloud_task_args_t *)pv;
    
    // Create a copy of the session ID to pass to the callback
    char *session_id_copy = strdup(args->session_id);
    
    // Call cloud and wait for callback to publish response
    if (!ai_client_ask(args->question, mqtt_cloud_response_cb, session_id_copy)) {
        // If cloud fails, send fallback message
        mqtt_publish_private_response("Cloud AI is thinking...", args->session_id);
        free(session_id_copy);
    }
    
    // Wait for HTTP response to complete
    vTaskDelay(pdMS_TO_TICKS(15000));
    
    free(args); // Free the struct memory
    vTaskDelete(NULL);
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, 
                                int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to MQTT broker");
            mqtt_connected = true;
            ESP_LOGI(TAG, "Subscribing to: %s", g_topic_command);
            esp_mqtt_client_subscribe(mqtt_handle, g_topic_command, 1);
            esp_mqtt_client_subscribe(mqtt_handle, g_topic_request_data, 1);
            esp_mqtt_client_subscribe(mqtt_handle, g_topic_historical, 1);
            mqtt_publish_response("OPRUSS AI is connected to the Air Purifier and Monitoring System and it is online!");
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            mqtt_connected = false;
            break;
            
        case MQTT_EVENT_DATA: {
            char topic[64] = {0};
            char payload[256] = {0};
            
            snprintf(topic, sizeof(topic), "%.*s", event->topic_len, event->topic);
            snprintf(payload, sizeof(payload), "%.*s", event->data_len, event->data);
            
            ESP_LOGI(TAG, "Received: topic=%s, payload=%s", topic, payload);
        
            // Build the base command topic to check against
            char base_cmd_topic[64];
            snprintf(base_cmd_topic, sizeof(base_cmd_topic), "opruss/%s/command/", g_device_id);

            // Check if the incoming topic starts with the base command topic
            if (strncmp(topic, base_cmd_topic, strlen(base_cmd_topic)) == 0) {
                
                // Extract the Session ID from the end of the topic
                char *session_id = topic + strlen(base_cmd_topic);
                
                g_msg_from_app = true;

                if (wifi_is_connected) {
                    // Allocate the struct to pass to FreeRTOS
                    cloud_task_args_t *args = malloc(sizeof(cloud_task_args_t));
                    
                    if (args) {
                        strncpy(args->question, payload, sizeof(args->question) - 1);
                        args->question[sizeof(args->question) - 1] = '\0';
                        
                        strncpy(args->session_id, session_id, sizeof(args->session_id) - 1);
                        args->session_id[sizeof(args->session_id) - 1] = '\0';

                        if (xTaskCreate(mqtt_cloud_task, "ai_cloud", 8192, args, 5, NULL) != pdPASS) {
                            mqtt_publish_private_response("System memory is full right now. Please try again.", session_id);
                            free(args);
                        }
                    } else {
                        mqtt_publish_private_response("Unable to allocate memory for AI request.", session_id);
                    }
                } else {
                    /* Offline AI fallback */
                    char response[512] = {0};
                    ai_router_ask(payload, response, sizeof(response));
                    mqtt_publish_private_response(response, session_id);
                }

                g_msg_from_app = false;
            }
            else if (strcmp(topic, g_topic_request_data) == 0) {
                ESP_LOGI(TAG, "Data requested by app");
                if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    sensor_data_t s = real_data;
                    xSemaphoreGive(sensor_mutex);
                    
                    // Get fan status
                    const char *fan_mode_str = fan_manual_mode ? "MANUAL" : "AUTO";
                    const char *fan_speed_str = "OFF";
                    if (fan_speed_level == 1) fan_speed_str = "LOW";
                    else if (fan_speed_level == 2) fan_speed_str = "MED";
                    else if (fan_speed_level == 3) fan_speed_str = "HIGH";
                    int fan_rpm = fan_manual_mode ? fan_speed_rpm[fan_speed_level] : (int)s.fan_speed;
                    
                    char p[384];
                    snprintf(p, sizeof(p), 
                        "{\"aqi\":%d,\"pm25\":%.1f,\"pm10\":%.1f,\"temp\":%.1f,\"humidity\":%.1f,\"tvoc\":%.1f,\"fan_mode\":\"%s\",\"fan_speed\":\"%s\",\"fan_rpm\":%d}",
                        s.aqi, (double)s.pm25, (double)s.pm10, (double)s.temp, (double)s.humidity, (double)s.tvoc,
                        fan_mode_str, fan_speed_str, fan_rpm);
                    
                    esp_mqtt_client_publish(mqtt_handle, g_topic_livedata, p, 0, 1, 0);
                    ESP_LOGI(TAG, "Published livedata on request");
                }
            }

            else if (strcmp(topic, g_topic_historical) == 0) {
                // App requests historical data summary
                char summary[1024] = {0};
                if (sd_context_get_today_summary(summary, sizeof(summary))) {
                    esp_mqtt_client_publish(mqtt_handle, g_topic_response, summary, 0, 1, 0);
                    // esp_mqtt_client_publish(mqtt_handle, g_topic_livedata, summary, 0, 1, 0);
                    ESP_LOGI(TAG, "Published historical summary");
                }

            }
        }
        break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;
            
        default:
            break;
    }
}


void mqtt_client_init(void) {
    generate_device_id();

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        // The Keep-Alive Heartbeat! Pings HiveMQ every 45 seconds so it never sleeps.
        .session.keepalive = 45, 
    };
    
    mqtt_handle = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_handle);
    
    ESP_LOGI(TAG, "MQTT client started for device: %s", g_device_id);
}

void mqtt_set_fan_status(const char *mode,
                         const char *speed,
                         int rpm)
{
    if (mode != NULL) {
        snprintf(g_fan_mode, sizeof(g_fan_mode), "%s", mode);
    }

    if (speed != NULL) {
        snprintf(g_fan_speed, sizeof(g_fan_speed), "%s", speed);
    }

    g_fan_rpm = rpm;
}

static void mqtt_update_fan_status(void)
{
    const char *mode = ui_get_fan_mode();
    const char *speed = ui_get_fan_speed_name();
    int rpm = ui_get_fan_rpm();

    mqtt_set_fan_status(
        mode,
        speed,
        rpm
    );

    ESP_LOGI(TAG,
             "Fan status: mode=%s speed=%s rpm=%d",
             g_fan_mode,
             g_fan_speed,
             g_fan_rpm);
}
// void mqtt_publish_sensor_data(int aqi, float pm25, float pm10, float temp, float humidity, float tvoc) {
//     if (!mqtt_connected) return;
//     char payload[256];
//     snprintf(payload, sizeof(payload),
//         "{\"aqi\":%d,\"pm25\":%.1f,\"pm10\":%.1f,\"temp\":%.1f,\"humidity\":%.1f,\"tvoc\":%.1f}",
//         aqi, (double)pm25, (double)pm10, (double)temp, (double)humidity, (double)tvoc);
//     esp_mqtt_client_publish(mqtt_handle, g_topic_livedata, payload, 0, 1, 0);
// }

void mqtt_publish_sensor_data(int aqi,
                              float pm25,
                              float pm10,
                              float temp,
                              float humidity,
                              float tvoc)
{
    if (!mqtt_connected) return;

    char payload[512];

    mqtt_update_fan_status();

    snprintf(payload, sizeof(payload),
        "{"
        "\"aqi\":%d,"
        "\"pm25\":%.1f,"
        "\"pm10\":%.1f,"
        "\"temp\":%.1f,"
        "\"humidity\":%.1f,"
        "\"tvoc\":%.1f,"
        "\"fan_mode\":\"%s\","
        "\"fan_speed\":\"%s\","
        "\"fan_rpm\":%d"
        "}",
        aqi,
        (double)pm25,
        (double)pm10,
        (double)temp,
        (double)humidity,
        (double)tvoc,
        g_fan_mode,
        g_fan_speed,
        g_fan_rpm
    );

    ESP_LOGI(TAG, "Publishing livedata: %s", payload);

    esp_mqtt_client_publish(
        mqtt_handle,
        g_topic_livedata,
        payload,
        0,
        1,
        0
    );
}

void mqtt_publish_response(const char *response) {
    if (!mqtt_connected || response == NULL) return;
    esp_mqtt_client_publish(mqtt_handle, g_topic_response, response, 0, 1, 0);
}

bool mqtt_is_connected(void) {
    return mqtt_connected;}