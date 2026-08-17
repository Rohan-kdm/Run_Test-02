// #include "ai_client.h"
// #include <stdio.h>
// #include <string.h>
// #include <stdlib.h>
// #include <ctype.h>
// #include "esp_log.h"
// #include "esp_http_client.h"
// #include "esp_crt_bundle.h"
// #include "cJSON.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/semphr.h"
// #include "sd_context.h"
// #include "ui.h" 

// static const char *TAG = "AI_CLIENT";

// // Vercel endpoint
// // #define AI_API_URL "https://opruss-ai.vercel.app/api/chat"
// #define GROQ_API_KEY "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"
// #define GROQ_URL "https://api.groq.com/openai/v1/chat/completions"
// #define REQUEST_TIMEOUT_MS 15000
// #define MAX_RESPONSE_SIZE 2048

// typedef struct {
//     char question[512];
//     ai_client_callback_t callback;
//     void *user_data;
// } ai_request_t;

// static bool g_busy = false;
// static char g_error[256] = {0};
// static SemaphoreHandle_t g_mutex = NULL;
// extern sensor_data_t real_data;
// extern SemaphoreHandle_t sensor_mutex;

// // Background HTTP task
// static void ai_client_task(void *pv) {
//     ai_request_t *req = (ai_request_t *)pv;

//     // Get current sensor data for context
//     char context[512] = {0};
//     if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
//         sensor_data_t s = real_data;
//         xSemaphoreGive(sensor_mutex);
//         snprintf(context, sizeof(context),
//             "Current sensor readings - AQI: %d, PM2.5: %.1f, Temp: %.1fC, Humidity: %.1f%%, TVOC: %.1f ppb.",
//             s.aqi, (double)s.pm25, (double)s.temp, (double)s.humidity, (double)s.tvoc);
//     }

//     // Build Groq-format JSON
//     cJSON *root = cJSON_CreateObject();
//     cJSON_AddStringToObject(root, "model", "llama-3.1-8b-instant");
//     cJSON_AddNumberToObject(root, "max_tokens", 300);
    
//     cJSON *messages = cJSON_CreateArray();
    
//     // System message
//     cJSON *sys = cJSON_CreateObject();
//     cJSON_AddStringToObject(sys, "role", "system");
//     cJSON_AddStringToObject(sys, "content", "You are OPRUSS AI. Answer based on sensor data provided. Be concise.");
//     cJSON_AddItemToArray(messages, sys);
    
//     // User message with context
//     cJSON *usr = cJSON_CreateObject();
//     cJSON_AddStringToObject(usr, "role", "user");
//     // char prompt[2048];
//     // snprintf(prompt, sizeof(prompt), "%s\n\nQuestion: %s", context, req->question);
//     char prompt[1024];
//     snprintf(prompt, sizeof(prompt), "%.400s\n\nQuestion: %.400s", context, req->question);
//     cJSON_AddStringToObject(usr, "content", prompt);
//     cJSON_AddItemToArray(messages, usr);
    
//     cJSON_AddItemToObject(root, "messages", messages);
//     char *json_str = cJSON_PrintUnformatted(root);
//     cJSON_Delete(root);
    
//     char *response = calloc(1, MAX_RESPONSE_SIZE);
//     bool success = false;
//     if (!response) { free(req); free(json_str); vTaskDelete(NULL); return; }
    
//     ESP_LOGI(TAG, "Sending: %s", req->question);

//     // ADD THIS - config was deleted
//     esp_http_client_config_t config = {
//         .url = GROQ_URL,
//         .method = HTTP_METHOD_POST,
//         .timeout_ms = REQUEST_TIMEOUT_MS,
//         .crt_bundle_attach = esp_crt_bundle_attach,
//     };
    
//     esp_http_client_handle_t client = esp_http_client_init(&config);
//     esp_http_client_set_header(client, "Authorization", "Bearer " GROQ_API_KEY);
//     esp_http_client_set_header(client, "Content-Type", "application/json");
//     esp_http_client_set_post_field(client, json_str, strlen(json_str));
//     esp_err_t err = esp_http_client_perform(client);
//     int status = esp_http_client_get_status_code(client);
//     ESP_LOGI(TAG, "HTTP Status: %d, Err: %s", status, esp_err_to_name(err));
    
//     if (err == ESP_OK && status == 200) {
//         // Get content length (may be -1 for chunked)
//         int content_length = esp_http_client_get_content_length(client);
//         ESP_LOGI(TAG, "Content-Length: %d", content_length);
        
//         // Allocate based on content_length if known, else use max
//         int buf_size = (content_length > 0 && content_length < MAX_RESPONSE_SIZE) 
//                         ? content_length + 1 : MAX_RESPONSE_SIZE;
//         char *body = calloc(1, buf_size);
//         int total = 0;
        
//         if (body) {
//             while (total < buf_size - 1) {
//                 int len = esp_http_client_read(client, body + total, buf_size - total - 1);
//                 if (len <= 0) break;
//                 total += len;
//             }
//             body[total] = '\0';
            
//             if (total > 0) {
//                 ESP_LOGI(TAG, "Body (%d bytes): %.200s", total, body);
//                 strncpy(response, body, MAX_RESPONSE_SIZE - 1);
                
//                 // Parse Groq JSON
//                 cJSON *r = cJSON_Parse(body);
//                 if (r) {
//                     cJSON *choices = cJSON_GetObjectItem(r, "choices");
//                     if (choices && cJSON_GetArraySize(choices) > 0) {
//                         cJSON *c = cJSON_GetArrayItem(choices, 0);
//                         cJSON *msg = cJSON_GetObjectItem(c, "message");
//                         cJSON *txt = cJSON_GetObjectItem(msg, "content");
//                         if (txt && txt->valuestring) {
//                             strncpy(response, txt->valuestring, MAX_RESPONSE_SIZE - 1);
//                             success = true;
//                         }
//                     }
//                     cJSON_Delete(r);
//                 }
//             }
//             free(body);
//         }
//     }
//     esp_http_client_cleanup(client);
//     free(json_str);
    
//     xSemaphoreTake(g_mutex, portMAX_DELAY);
//     g_busy = false;
//     xSemaphoreGive(g_mutex);
    
//     if (req->callback) req->callback(response, success, req->user_data);
//     free(response);
//     free(req);
//     vTaskDelete(NULL);
// }

// void ai_client_init(void) {
//     if (g_mutex == NULL) {
//         g_mutex = xSemaphoreCreateMutex();
//         if (g_mutex == NULL) {
//             ESP_LOGE(TAG, "Failed to create mutex");
//         }
//     }
//     ESP_LOGI(TAG, "AI Client initialized");
// }

// bool ai_client_ask(const char *question, ai_client_callback_t callback, void *user_data) {
//     if (!question || strlen(question) == 0) return false;
    
//     xSemaphoreTake(g_mutex, portMAX_DELAY);
//     if (g_busy) {
//         xSemaphoreGive(g_mutex);
//         ESP_LOGW(TAG, "Request already in progress");
//         return false;
//     }
//     g_busy = true;
//     g_error[0] = '\0';
//     xSemaphoreGive(g_mutex);
    
//     ai_request_t *req = malloc(sizeof(ai_request_t));
//     if (!req) {
//         xSemaphoreTake(g_mutex, portMAX_DELAY);
//         g_busy = false;
//         xSemaphoreGive(g_mutex);
//         return false;
//     }
    
//     strncpy(req->question, question, sizeof(req->question)-1);
//     req->question[sizeof(req->question)-1] = '\0';
//     req->callback = callback;
//     req->user_data = user_data;
    
//     if (xTaskCreate(ai_client_task, "ai_client", 8192, req, 5, NULL) != pdPASS) {
//         free(req);
//         xSemaphoreTake(g_mutex, portMAX_DELAY);
//         g_busy = false;
//         xSemaphoreGive(g_mutex);
//         return false;
//     }
    
//     return true;
// }

// bool ai_client_is_busy(void) {
//     xSemaphoreTake(g_mutex, portMAX_DELAY);
//     bool busy = g_busy;
//     xSemaphoreGive(g_mutex);
//     return busy;
// }

// const char *ai_client_get_error(void) {
//     xSemaphoreTake(g_mutex, portMAX_DELAY);
//     static char err[256];
//     strncpy(err, g_error, sizeof(err)-1);
//     xSemaphoreGive(g_mutex);
//     return err;
// }

#include "ai_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sd_context.h"
#include "ui.h" 
#include "ch422g.h" // Required to toggle SD Card SPI lines
#include "esp_heap_caps.h"

static const char *TAG = "AI_CLIENT";

// Groq API Configuration
#define GROQ_API_KEY "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"
#define GROQ_URL "https://api.groq.com/openai/v1/chat/completions"
#define REQUEST_TIMEOUT_MS 30000
#define MAX_RESPONSE_SIZE 2048
#define MAX_CONTEXT_SIZE 3072 // Increased buffer to hold CSV data
#define AI_WORKER_STACK_SIZE 10240

typedef struct {
    char question[512];
    ai_client_callback_t callback;
    void *user_data;
} ai_request_t;

static bool g_busy = false;
static char g_error[256] = {0};
static SemaphoreHandle_t g_mutex = NULL;
extern sensor_data_t real_data;
extern SemaphoreHandle_t sensor_mutex;
extern SemaphoreHandle_t sd_mutex;
static QueueHandle_t g_ai_queue = NULL;

// Helper to fetch the latest CSV lines from SD Card
static void append_csv_context(char *context_buf, size_t max_len) {
    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        ch422g_set_sd_selected(true);
        vTaskDelay(pdMS_TO_TICKS(10));

        DIR *dir = opendir("/sdcard");
        char latest_file[64] = {0};
        
        if (dir) {
            struct dirent *ent;
            while ((ent = readdir(dir)) != NULL) {
                // Find latest date CSV (Assuming YYYYMMDD.csv format)
                if (strstr(ent->d_name, ".csv") || strstr(ent->d_name, ".CSV")) {
                    if (strcmp(ent->d_name, latest_file) > 0) {
                        strncpy(latest_file, ent->d_name, sizeof(latest_file) - 1);
                    }
                }
            }
            closedir(dir);
        }

        if (strlen(latest_file) > 0) {
            char filepath[128];
            snprintf(filepath, sizeof(filepath), "/sdcard/%s", latest_file);
            
            FILE *f = fopen(filepath, "r");
            if (f) {
                char lines[6][128] = {0};
                int count = 0;
                char temp[128];
                
                // Read and discard CSV header row
                fgets(temp, sizeof(temp), f);
                
                // Read circularly to keep only the last 5 logs
                while (fgets(temp, sizeof(temp), f)) {
                    strncpy(lines[count % 5], temp, 127);
                    count++;
                }
                fclose(f);
                
                size_t len = strlen(context_buf);
                snprintf(context_buf + len, max_len - len, 
                         "\n\nRecent History (%s) [Date,Time,PM2.5,Temp,Hum,Press,Gas,TVOC,eCO2,PM10,Fan,Filter,AQI]:\n", 
                         latest_file);
                
                int start = (count < 5) ? 0 : (count % 5);
                int num_lines = (count < 5) ? count : 5;
                
                // Append rows to prompt context
                for (int i = 0; i < num_lines; i++) {
                    len = strlen(context_buf);
                    snprintf(context_buf + len, max_len - len, "%s", lines[(start + i) % 5]);
                }
            }
        }
        
        ch422g_set_sd_selected(false);
        xSemaphoreGive(sd_mutex);
    }
}

static bool build_ai_context(char *context, size_t context_size)
{
    if (!context || context_size == 0) {
        return false;
    }

    context[0] = '\0';

    /*
     * =========================================================
     * 1. REALTIME SENSOR DATA
     * =========================================================
     */

    sensor_data_t s = {0};

    // bool sensor_valid = false;

    if (xSemaphoreTake(
            sensor_mutex,
            pdMS_TO_TICKS(100)) == pdTRUE) {

        s = real_data;

        xSemaphoreGive(sensor_mutex);

        /*
         * AQI is calculated from PM2.5 in main.c.
         * PM2.5 > 0 is therefore a useful indication
         * that valid sensor data has arrived.
         */
        // if (s.pm25 > 0.0f || s.aqi > 0) {
        //     sensor_valid = true;
        // }
    }

    bool sensor_received = ui_has_sensor_data();
    
    const char *fan_mode = ui_get_fan_mode();
    const char *fan_speed = ui_get_fan_speed_name();
    int fan_rpm = ui_get_fan_rpm();

    if (fan_mode == NULL)
        fan_mode = "UNKNOWN";

    if (fan_speed == NULL)
        fan_speed = "UNKNOWN";


    snprintf(
        context,
        context_size,

        "REALTIME SENSOR DATA:\n"
        "Sensor packet received: %s\n"
        "AQI: %d\n"
        "PM2.5: %.1f ug/m3\n"
        "PM10: %.1f ug/m3\n"
        "Temperature: %.1f C\n"
        "Humidity: %.1f %%\n"
        "TVOC: %.1f ppb\n\n"

        "FAN STATUS:\n"
        "Mode: %s\n"
        "Speed: %s\n"
        "RPM: %d\n",

        sensor_received ? "YES" : "NO",

        s.aqi,
        (double)s.pm25,
        (double)s.pm10,
        (double)s.temp,
        (double)s.humidity,
        (double)s.tvoc,

        fan_mode,
        fan_speed,
        fan_rpm
    );


    /*
     * =========================================================
     * 2. TODAY'S HISTORICAL SUMMARY
     * =========================================================
     */

    size_t used = strlen(context);

    if (used < context_size - 1) {

        char today_summary[700] = {0};

        if (sd_context_get_today_summary(
                today_summary,
                sizeof(today_summary))) {

            snprintf(
                context + used,
                context_size - used,

                "\nHISTORICAL DATA - TODAY:\n"
                "%s\n",

                today_summary
            );

        } else {

            snprintf(
                context + used,
                context_size - used,

                "\nHISTORICAL DATA - TODAY:\n"
                "Today's historical summary is unavailable.\n"
            );
        }
    }


    /*
     * =========================================================
     * 3. TODAY vs YESTERDAY
     * =========================================================
     */

    used = strlen(context);

    if (used < context_size - 1) {

        char comparison[1200] = {0};

        if (sd_context_compare_days(
                comparison,
                sizeof(comparison))) {

            snprintf(
                context + used,
                context_size - used,

                "\nHISTORICAL DATA - DAY COMPARISON:\n"
                "%s\n",

                comparison
            );



        } 
        used = strlen(context);

        if (used < context_size - 1) {

            snprintf(
                context + used,
                context_size - used,

                "\nIMPORTANT HISTORICAL DATA RULES:\n"
                "Historical SD-card data is authoritative.\n"
                "A historical value of 0 is a valid recorded value.\n"
                "Never replace a historical zero with another value.\n"
                "Never invent or estimate historical sensor values.\n"
                "If a historical parameter is not provided, report it "
                "as unavailable.\n"
                "The DAY-TO-DAY AIR QUALITY COMPARISON is calculated "
                "by the OPRUSS device software and must be treated "
                "as authoritative.\n"
                "Do not replace the supplied comparison values with "
                "invented values.\n"
            );
        }
    }


    /*
     * =========================================================
     * 4. Debug logging
     * =========================================================
     */

    ESP_LOGI(
        TAG,
        "AI context prepared (%d bytes)",
        (int)strlen(context)
    );

    ESP_LOGI(
        TAG,
        "AI context preview:\n%.1200s",
        context
    );

    return true;
}

static bool should_include_history(const char *question) {
    if (!question) return false;
    
    // Convert question to lowercase for simple substring matching
    char lower_q[512];
    int i = 0;
    while (question[i] && i < sizeof(lower_q) - 1) {
        lower_q[i] = tolower((unsigned char)question[i]);
        i++;
    }
    lower_q[i] = '\0';
    
    // Skip heavy CSV context for pure greetings or identity questions
    if (strstr(lower_q, "hi") || strstr(lower_q, "hello") || 
        strstr(lower_q, "what is your name") || strstr(lower_q, "who are you")) {
        // Keep context out unless they specifically ask about status/history/aqi
        if (!strstr(lower_q, "status") && !strstr(lower_q, "history") && !strstr(lower_q, "aqi")) {
            return false;
        }
    }
    
    return true;
}

static void ai_worker_task(void *pv) {
     ESP_LOGI(
        TAG,
        "AI worker started. Initial stack watermark: %u bytes",
        (unsigned)uxTaskGetStackHighWaterMark(NULL)
    );

    ai_request_t req;

    // Infinite loop: task stays alive forever
    while (1) {
        // Wait here indefinitely until a request arrives in the queue
        if (xQueueReceive(g_ai_queue, &req, portMAX_DELAY) == pdTRUE) {

            ESP_LOGI(
                TAG,
                "AI request received: %s",
                req.question
            );

            ESP_LOGI(
                TAG,
                "Stack watermark before processing: %u bytes",
                (unsigned)uxTaskGetStackHighWaterMark(NULL)
            );
            
            // 1. Allocate context buffer safely
            char *context = calloc(1, MAX_CONTEXT_SIZE);
            if (!context) {
                ESP_LOGE(TAG, "Failed to allocate memory for context");
                if (req.callback) req.callback("Memory Error", false, req.user_data);
                continue; // Skip to the next item in the queue instead of deleting the task
            }

            // // 2. Get current sensor data for realtime context
            // if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            //     sensor_data_t s = real_data;
            //     xSemaphoreGive(sensor_mutex);
            //     snprintf(context, MAX_CONTEXT_SIZE,
            //         "Current Realtime - AQI: %d, PM2.5: %.1f, Temp: %.1fC, Humidity: %.1f%%, TVOC: %.1f ppb.",
            //         s.aqi, (double)s.pm25, (double)s.temp, (double)s.humidity, (double)s.tvoc);
            // }

            // // 3. Read SD Card and append historical CSV Data
            // append_csv_context(context, MAX_CONTEXT_SIZE);

            /*
            * ==========================================================
            * BUILD COMPLETE AI CONTEXT
            * ==========================================================
            */

            if (!build_ai_context(context, MAX_CONTEXT_SIZE)) {

                snprintf(
                    context,
                    MAX_CONTEXT_SIZE,
                    "No sensor or historical data is currently available."
                );
            }

            ESP_LOGI(
                TAG,
                "Stack watermark before cJSON: %u bytes",
                (unsigned)uxTaskGetStackHighWaterMark(NULL)
            );

            /*
            * ==========================================================
            * REALTIME SENSOR CONTEXT
            *
            * TEMPORARILY NO SD ACCESS.
            * We are isolating the stack problem first.
            * ==========================================================
            */

            // sensor_data_t s = {0};

            // if (xSemaphoreTake(
            //         sensor_mutex,
            //         pdMS_TO_TICKS(100)
            //     ) == pdTRUE) {

            //     s = real_data;

            //     xSemaphoreGive(sensor_mutex);

            //     const char *fan_mode = ui_get_fan_mode();
            //     const char *fan_speed = ui_get_fan_speed_name();
            //     int fan_rpm = ui_get_fan_rpm();

            //     if (fan_mode == NULL)
            //         fan_mode = "UNKNOWN";

            //     if (fan_speed == NULL)
            //         fan_speed = "UNKNOWN";

            //     snprintf(
            //         context,
            //         MAX_CONTEXT_SIZE,

            //         "REALTIME SENSOR DATA:\n"
            //         "AQI: %d\n"
            //         "PM2.5: %.1f ug/m3\n"
            //         "PM10: %.1f ug/m3\n"
            //         "Temperature: %.1f C\n"
            //         "Humidity: %.1f %%\n"
            //         "TVOC: %.1f ppb\n"

            //         "FAN STATUS:\n"
            //         "Mode: %s\n"
            //         "Speed: %s\n"
            //         "RPM: %d\n",

            //         s.aqi,
            //         (double)s.pm25,
            //         (double)s.pm10,
            //         (double)s.temp,
            //         (double)s.humidity,
            //         (double)s.tvoc,

            //         fan_mode,
            //         fan_speed,
            //         fan_rpm
            //     );

            // } else {

            //     snprintf(
            //         context,
            //         MAX_CONTEXT_SIZE,

            //         "REALTIME SENSOR DATA:\n"
            //         "Sensor data temporarily unavailable.\n"
            //     );
            // }
            // Build Groq-format JSON Payload
            cJSON *root = cJSON_CreateObject();
            if (!root) {

                ESP_LOGE(
                    TAG,
                    "Failed to allocate Groq JSON root"
                );

                free(context);

                if (req.callback) {
                    req.callback(
                        "AI memory allocation error",
                        false,
                        req.user_data
                    );
                }

                continue;
            }
            cJSON_AddStringToObject(root, "model", "llama-3.1-8b-instant");
            cJSON_AddNumberToObject(root, "max_tokens", 300);
            
            cJSON *messages = cJSON_CreateArray();

            if (!messages) {

                ESP_LOGE(
                    TAG,
                    "Failed to allocate messages JSON"
                );

                cJSON_Delete(root);
                free(context);

                if (req.callback) {
                    req.callback(
                        "AI memory allocation error",
                        false,
                        req.user_data
                    );
                }

                continue;
            }
            
            // System message
            // System message
            cJSON *sys = cJSON_CreateObject();
            if (!sys) {

                ESP_LOGE(
                    TAG,
                    "Failed to allocate system JSON"
                );

                cJSON_Delete(root);
                free(context);

                if (req.callback) {
                    req.callback(
                        "AI memory allocation error",
                        false,
                        req.user_data
                    );
                }

                continue;
            }
            cJSON_AddStringToObject(sys, "role", "system");
            cJSON_AddStringToObject(sys, "content", 
                "You are OPRUSS AI, the official AI assistant "
                "of the OPRUSS air-quality monitoring system. "

                "OPRUSS is a brand name and must not be expanded "
                "into an acronym. "

                "You have access to realtime sensor readings and "
                "historical air-quality statistics calculated directly "
                "from the device SD card. "

                "Answer questions using ONLY the information provided in "
                "the device context. "

                "The historical data in the Data Context is authoritative. "

                "When the user asks about yesterday, today, previous days, "
                "comparisons, trends, averages, increases, decreases, "
                "best days, worst days, or historical air quality, "
                "use the historical data provided in the context. "

                "Do not claim that historical data is unavailable when "
                "historical data is present in the context. "

                "Do not invent, estimate, or hallucinate sensor values. "

                "If realtime values are unavailable, clearly distinguish "
                "that from historical values. "

                "When comparing historical values, use the exact values "
                "provided in the DAY-TO-DAY AIR QUALITY COMPARISON section. "
                "Do not calculate or substitute different historical values. "
                "State the actual supplied values and percentage change "
                "when available. "

                "For AQI and PM2.5, explain whether the change represents "
                "an improvement or deterioration. "

                "IMPORTANT DATA RULES: "

                "Historical data from the SD card is authoritative. "
                "If a historical value is explicitly provided, use that "
                "exact value. "

                "A historical value of zero is a valid recorded value. "
                "Never replace zero with another value. "

                "Never invent, estimate, guess, or hallucinate sensor values. "

                "If a requested historical value is not present in the "
                "context, say that the data is unavailable. "

                "Do not use general knowledge to fill missing device data. "

                "The DAY-TO-DAY AIR QUALITY COMPARISON section is calculated "
                "by the OPRUSS device software. Treat its values as "
                "authoritative and use them directly. "

                "Do not recalculate or replace the supplied comparison "
                "values with invented values. "

                "REALTIME SENSOR DATA RULES: "

                "Always report the realtime sensor values exactly as "
                "provided in the device context. Do not replace or hide "
                "zero values. "

                "The field 'Sensor packet received' indicates whether "
                "the device has actually received a sensor data packet. "

                "If Sensor packet received is NO, the displayed realtime "
                "values may be default or initialization values. Report "
                "those values, but explain that they may not represent "
                "actual environmental measurements and recommend checking "
                "sensor connections and initialization. "

                "If Sensor packet received is YES, treat the supplied "
                "realtime values as actual sensor readings, even if one "
                "or more values are zero. Do not assume a zero value means "
                "the sensor is disconnected. "

                "Do not invent replacement values for zero readings. "

                "PM10 values of -1 indicate unavailable PM10 data and "
                "must not be interpreted as a real negative measurement. "

                "When the user asks about the fan, use the supplied "
                "FAN STATUS values. Never invent fan mode, speed, or RPM. "



                "Be concise, clear, and conversational.");
            cJSON_AddItemToArray(messages, sys);
            
            // User message with appended CSV context
            cJSON *usr = cJSON_CreateObject();
            if (!usr) {

                ESP_LOGE(
                    TAG,
                    "Failed to allocate user JSON"
                );

                cJSON_Delete(root);
                free(context);

                if (req.callback) {
                    req.callback(
                        "AI memory allocation error",
                        false,
                        req.user_data
                    );
                }

                continue;
            }
            cJSON_AddStringToObject(usr, "role", "user");
            
            char *prompt = calloc(1, MAX_CONTEXT_SIZE + 512);
            if (prompt) {
                snprintf(
                    prompt,
                    MAX_CONTEXT_SIZE + 512,

                    "DATA CONTEXT:\n"
                    "====================\n"
                    "%s\n"
                    "====================\n\n"
                    "USER QUESTION:\n"
                    "%s",

                    context,
                    req.question
                );
                cJSON_AddStringToObject(usr, "content", prompt);
                free(prompt);
            } else {
                cJSON_AddStringToObject(usr, "content", req.question);
            }
            cJSON_AddItemToArray(messages, usr);
            
            cJSON_AddItemToObject(root, "messages", messages);
            char *json_str = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            free(context); // Free context buffer early to save RAM
            
            // Send HTTP POST to Groq
            char *response = calloc(1, MAX_RESPONSE_SIZE);
            if (!response) {

                ESP_LOGE(
                    TAG,
                    "Failed to allocate AI response buffer"
                );

                if (json_str) {
                    free(json_str);
                }

                if (req.callback) {
                    req.callback(
                        "AI response memory allocation failed",
                        false,
                        req.user_data
                    );
                }

                continue;
            }
            bool success = false;
            
            if (response && json_str) {
                ESP_LOGI(TAG, "Sending robust context to Groq API...");

                esp_http_client_config_t config = {
                    .url = GROQ_URL,
                    .method = HTTP_METHOD_POST,
                    .timeout_ms = 30000, // INCREASED TO 30 SECONDS
                    .crt_bundle_attach = esp_crt_bundle_attach,
                };
                
                ESP_LOGI(
                    TAG,
                    "Stack watermark before HTTP: %u bytes",
                    (unsigned)uxTaskGetStackHighWaterMark(NULL)
                );

                ESP_LOGI(
                    TAG,
                    "Free heap before HTTP: %u",
                    (unsigned)esp_get_free_heap_size()
                );
                
                esp_http_client_handle_t client = esp_http_client_init(&config);
                if (!client) {

                        ESP_LOGE(
                            TAG,
                            "Failed to initialize HTTP client"
                        );

                        free(response);

                        if (json_str) {
                            free(json_str);
                        }

                        if (req.callback) {
                            req.callback(
                                "HTTP client initialization failed",
                                false,
                                req.user_data
                            );
                        }

                        continue;
                    }
                esp_http_client_set_header(client, "Authorization", "Bearer " GROQ_API_KEY);
                esp_http_client_set_header(client, "Content-Type", "application/json");
                // esp_http_client_set_post_field(client, json_str, strlen(json_str));
                
                // Manually open the connection and send the payload
                esp_err_t err = esp_http_client_open(client, strlen(json_str));
                if (err == ESP_OK) {
                    esp_http_client_write(client, json_str, strlen(json_str));
                    esp_http_client_fetch_headers(client);
                    
                    int status = esp_http_client_get_status_code(client);
                    ESP_LOGI(TAG, "HTTP Status: %d", status);
                    
                    if (status == 200) {
                        char *body = calloc(1, MAX_RESPONSE_SIZE);
                        int total = 0;
                        
                        if (body) {
                            // Read chunks until the stream is empty or buffer is full
                            while (total < MAX_RESPONSE_SIZE - 1) {
                                int len = esp_http_client_read(client, body + total, MAX_RESPONSE_SIZE - total - 1);
                                if (len <= 0) break; // End of stream
                                total += len;
                            }
                            body[total] = '\0';
                            
                            // Parse the complete JSON
                            if (total > 0) {
                                cJSON *r = cJSON_Parse(body);
                                if (r) {
                                    cJSON *choices = cJSON_GetObjectItem(r, "choices");
                                    if (choices && cJSON_GetArraySize(choices) > 0) {
                                        cJSON *c = cJSON_GetArrayItem(choices, 0);
                                        cJSON *msg = cJSON_GetObjectItem(c, "message");
                                        cJSON *txt = cJSON_GetObjectItem(msg, "content");
                                        if (txt && txt->valuestring) {
                                            strncpy(response, txt->valuestring, MAX_RESPONSE_SIZE - 1);
                                            success = true;
                                        }
                                    }
                                    cJSON_Delete(r);
                                }
                            }
                            free(body);
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
                }
                
                esp_http_client_cleanup(client);
                ESP_LOGI(
                    TAG,
                    "Free heap after HTTP: %u",
                    (unsigned)esp_get_free_heap_size()
                );
                ESP_LOGI(
                    TAG,
                    "Stack watermark after HTTP: %u bytes",
                    (unsigned)uxTaskGetStackHighWaterMark(NULL)
                );
            }
            
            
            if (json_str) free(json_str);
            
            // 6. Execute Callback and Clean up final memory
            if (req.callback) {
                req.callback(response ? response : "API Error", success, req.user_data);
            }
            if (response) free(response);
            
            // Notice: There is no vTaskDelete(NULL) here. The code loops back to the top!
        }
    }
}

bool ai_client_ask(const char *question, ai_client_callback_t callback, void *user_data) {
    if (!question || !g_ai_queue) return false;
    
    ai_request_t new_req;
    strncpy(new_req.question, question, sizeof(new_req.question) - 1);
    new_req.question[sizeof(new_req.question) - 1] = '\0';
    new_req.callback = callback;
    new_req.user_data = user_data;
    
    // Send the request to the queue (wait up to 100ms if queue is full)
    if (xQueueSend(g_ai_queue, &new_req, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(TAG, "AI Queue is full! Dropping request.");
        return false;
    }
    // if (g_mutex == NULL) {
    //     g_mutex = xSemaphoreCreateMutex();
    // }
    
    // if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    //     return false;
    // }
    
    // if (g_busy) {
    //     xSemaphoreGive(g_mutex);
    //     ESP_LOGW(TAG, "AI client is busy processing another request.");
    //     return false;
    // }
    // g_busy = true;
    // xSemaphoreGive(g_mutex);
    
    // ai_request_t *req = malloc(sizeof(ai_request_t));
    // if (!req) {
    //     xSemaphoreTake(g_mutex, portMAX_DELAY);
    //     g_busy = false;
    //     xSemaphoreGive(g_mutex);
    //     return false;
    // }
    
    // strncpy(req->question, question, sizeof(req->question) - 1);
    // req->question[sizeof(req->question) - 1] = '\0';
    // req->callback = callback;
    // req->user_data = user_data;
    
    // BaseType_t ret = xTaskCreate(ai_client_task, "ai_client_task", 4096, req, 5, NULL);
    // if (ret != pdPASS) {
    //     free(req);
    //     xSemaphoreTake(g_mutex, portMAX_DELAY);
    //     g_busy = false;
    //     xSemaphoreGive(g_mutex);
    //     return false;
    // }
    
    return true;
}

void ai_client_init(void) {
    // Create a queue that can hold up to 5 pending questions
    g_ai_queue = xQueueCreate(5, sizeof(ai_request_t));
    
    BaseType_t ret = xTaskCreate(
        ai_worker_task,
        "ai_worker_task",
        AI_WORKER_STACK_SIZE,
        NULL,
        5,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(
            TAG,
            "Failed to create ai_worker_task"
        );
    } else {
        ESP_LOGI(
            TAG,
            "AI worker created with %d bytes stack",
            AI_WORKER_STACK_SIZE
        );
    }
    
    ESP_LOGI(TAG, "AI Client initialized with Queue.");
}