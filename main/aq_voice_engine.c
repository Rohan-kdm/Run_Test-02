// #include "aq_voice_engine.h"

// #include <ctype.h>
// #include <dirent.h>
// #include <errno.h>
// #include <math.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/stat.h>
// #include <time.h>
// #include "ch422g.h"

// #include "ui.h"

// #include "esp_log.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/semphr.h"
// #include "freertos/task.h"

// static const char *TAG = "VOICE_ENGINE";

// #define CSV_LINE_SIZE 512
// #define MAX_DAYS 366

// // External variables
// extern sensor_data_t real_data;
// extern SemaphoreHandle_t sensor_mutex;
// extern SemaphoreHandle_t sd_mutex;
// extern bool fan_manual_mode;
// extern int fan_speed_level;
// extern const int fan_speed_rpm[];

// extern lv_obj_t *voice_response_lbl;
// extern lv_obj_t *voice_status_lbl;

// extern void voice_start_thinking(void);
// extern void voice_stop_animation(void);
// extern void mqtt_publish_response(const char *response);   // ADD THIS LINE

// // Forward declarations
// static void voice_response_update_cb(void *data);

// // ==========================================
// // HELPERS
// // ==========================================

// static bool str_contains(const char *text, const char *needle) {
//     if (text == NULL || needle == NULL) return false;
//     char lower_text[256], lower_needle[128];
//     size_t i;
//     for (i = 0; text[i] && i < sizeof(lower_text) - 1; i++) lower_text[i] = (char)tolower((unsigned char)text[i]);
//     lower_text[i] = '\0';
//     for (i = 0; needle[i] && i < sizeof(lower_needle) - 1; i++) lower_needle[i] = (char)tolower((unsigned char)needle[i]);
//     lower_needle[i] = '\0';
//     return strstr(lower_text, lower_needle) != NULL;
// }

// static const char *aqi_category(float aqi) {
//     if (aqi <= 50.0f) return "Good";
//     if (aqi <= 100.0f) return "Satisfactory";
//     if (aqi <= 200.0f) return "Moderate";
//     if (aqi <= 300.0f) return "Poor";
//     if (aqi <= 400.0f) return "Very Poor";
//     return "Severe";
// }

// // ==========================================
// // COMMAND CLASSIFICATION
// // ==========================================

// voice_command_t aq_voice_classify(const char *text) {
//     if (text == NULL) return VOICE_CMD_UNKNOWN;

//     if (str_contains(text, "predict") || str_contains(text, "forecast") || 
//         str_contains(text, "next hour") || str_contains(text, "will aqi"))
//         return VOICE_CMD_PREDICT_AQI;
    
//     if (str_contains(text, "worst") || str_contains(text, "highest") || 
//         str_contains(text, "maximum") || str_contains(text, "peak"))
//         return VOICE_CMD_WORST_READING;
        
//     if (str_contains(text, "trend") || str_contains(text, "improving") || 
//         str_contains(text, "worsening") || str_contains(text, "over time"))
//         return VOICE_CMD_TREND;
        
//     if (str_contains(text, "recommend") || str_contains(text, "should i") || 
//         str_contains(text, "what to do") || str_contains(text, "advice"))
//         return VOICE_CMD_RECOMMENDATION;
        
//     if (str_contains(text, "drows") || str_contains(text, "headache") || 
//         str_contains(text, "health") || str_contains(text, "risk"))
//         return VOICE_CMD_HEALTH_RISK;
        
//     if (str_contains(text, "yesterday") || str_contains(text, "compare") || 
//         str_contains(text, "percent"))
//         return VOICE_CMD_COMPARE_DAYS;
        
//     if (str_contains(text, "current aqi") || str_contains(text, "air quality now") || 
//         str_contains(text, "status"))
//         return VOICE_CMD_CURRENT_AQI;
        
//     if (str_contains(text, "fan status") || str_contains(text, "fan speed"))
//         return VOICE_CMD_FAN_STATUS;
        
//     if (str_contains(text, "turn on fan") || str_contains(text, "fan on"))
//         return VOICE_CMD_FAN_ON;
        
//     if (str_contains(text, "turn off fan") || str_contains(text, "fan off"))
//         return VOICE_CMD_FAN_OFF;
        
//     if (str_contains(text, "export") || str_contains(text, "report"))
//         return VOICE_CMD_EXPORT;
        
//     return VOICE_CMD_UNKNOWN;
// }

// // ==========================================
// // CSV DATA ANALYSIS
// // ==========================================

// typedef struct {
//     char date[16];
//     double sum_aqi;
//     double sum_pm25;
//     int samples;
//     float max_aqi;
//     float max_pm25;
//     char worst_time[128];
// } day_stats_t;

// typedef struct {
//     voice_command_t cmd;
// } voice_task_params_t;

// static bool parse_csv_row(char *line, float *pm25, float *aqi, char *date_out, char *time_out) {
//     char *fields[13] = {0};
//     int count = 0;
//     char *save = NULL;
    
//     for (char *f = strtok_r(line, ",", &save); f && count < 13; f = strtok_r(NULL, ",", &save)) {
//         while (*f == ' ' || *f == '\t') f++;
//         size_t len = strlen(f);
//         while (len > 0 && (f[len-1] == '\r' || f[len-1] == '\n')) f[--len] = '\0';
//         fields[count++] = f;
//     }
    
//     if (count < 8) return false;
//     if (strncmp(fields[0], "Date", 4) == 0 || strncmp(fields[0], "OPRUSS", 6) == 0) return false;
    
//     strncpy(date_out, fields[0], 15);
//     date_out[15] = '\0';
//     if (count >= 13) {
//         snprintf(time_out, 31, "%s", fields[1]);
//     } else {
//         time_out[0] = '\0';
//     }
    
//     int pm25_idx = (count >= 13) ? 2 : 1;
//     int aqi_idx = (count >= 13) ? 12 : -1;
    
//     *pm25 = (float)atof(fields[pm25_idx]);
    
//     if (aqi_idx > 0 && aqi_idx < count) {
//         *aqi = (float)atof(fields[aqi_idx]);
//     } else {
//         float p = *pm25;
//         if (p <= 30) *aqi = (50.0f / 30.0f) * p;
//         else if (p <= 60) *aqi = 51 + (49.0f / 30.0f) * (p - 30);
//         else if (p <= 90) *aqi = 101 + (99.0f / 30.0f) * (p - 60);
//         else if (p <= 120) *aqi = 201 + (99.0f / 30.0f) * (p - 90);
//         else if (p <= 250) *aqi = 301 + (99.0f / 130.0f) * (p - 120);
//         else *aqi = 401 + (99.0f / 100.0f) * (p - 250);
//     }
    
//     return true;
// }

// static bool analyze_csv(const char *filepath, day_stats_t *today, day_stats_t *yesterday, 
//                         float *worst_aqi, float *worst_pm25, char *worst_time) {
//     FILE *f = fopen(filepath, "r");
//     if (!f) return false;
    
//     char line[CSV_LINE_SIZE];
//     int total_samples = 0;
//     *worst_aqi = 0;
    
//     while (fgets(line, sizeof(line), f)) {
//         float pm25, aqi;
//         char date[16], time_str[32];
        
//         if (!parse_csv_row(line, &pm25, &aqi, date, time_str)) continue;
        
//         if (today) {
//             today->sum_aqi += aqi;
//             today->sum_pm25 += pm25;
//             today->samples++;
//             snprintf(today->date, sizeof(today->date), "%s", date);
//         }
        
//         if (aqi > *worst_aqi) {
//             *worst_aqi = aqi;
//             *worst_pm25 = pm25;
//             snprintf(worst_time, 128, "%s %s", date, time_str);
//         }
        
//         total_samples++;
//     }
    
//     fclose(f);
//     return total_samples > 0;
// }

// static bool analyze_two_days(const char *dir_path, char *answer, size_t answer_size) {
//     DIR *dir = opendir(dir_path);
//     if (!dir) {
//         snprintf(answer, answer_size, "Cannot access data storage.");
//         return false;
//     }
    
//     char files[31][64];
//     int file_count = 0;
//     struct dirent *entry;
    
//     while ((entry = readdir(dir)) && file_count < 30) {
//         if (strstr(entry->d_name, ".csv") && strlen(entry->d_name) == 12) {
//             strncpy(files[file_count], entry->d_name, 63);
//             files[file_count][63] = '\0';
//             file_count++;
//         }
//     }
//     closedir(dir);
    
//     if (file_count < 2) {
//         snprintf(answer, answer_size, "Need at least 2 days of data for comparison.");
//         return false;
//     }
    
//     for (int i = 0; i < file_count - 1; i++) {
//         for (int j = i + 1; j < file_count; j++) {
//             if (strcmp(files[i], files[j]) > 0) {
//                 char tmp[64];
//                 strcpy(tmp, files[i]);
//                 strcpy(files[i], files[j]);
//                 strcpy(files[j], tmp);
//             }
//         }
//     }
    
//     day_stats_t yesterday_data = {0}, today_data = {0};
//     float worst_aqi = 0, worst_pm25 = 0;
//     char worst_time[64] = "";
    
//     char today_path[128], yesterday_path[128];
//     snprintf(today_path, sizeof(today_path), "%s/%s", dir_path, files[file_count - 1]);
//     snprintf(yesterday_path, sizeof(yesterday_path), "%s/%s", dir_path, files[file_count - 2]);
    
//     analyze_csv(today_path, &today_data, NULL, &worst_aqi, &worst_pm25, worst_time);
    
//     FILE *yf = fopen(yesterday_path, "r");
//     if (yf) {
//         char line[CSV_LINE_SIZE];
//         while (fgets(line, sizeof(line), yf)) {
//             float pm25, aqi;
//             char date[16], time_str[32];
//             if (parse_csv_row(line, &pm25, &aqi, date, time_str)) {
//                 yesterday_data.sum_aqi += aqi;
//                 yesterday_data.samples++;
//                 snprintf(yesterday_data.date, sizeof(yesterday_data.date), "%s", date);
//             }
//         }
//         fclose(yf);
//     }
    
//     float today_avg = today_data.samples > 0 ? (float)(today_data.sum_aqi / today_data.samples) : 0;
//     float yesterday_avg = yesterday_data.samples > 0 ? (float)(yesterday_data.sum_aqi / yesterday_data.samples) : 0;
    
//     if (yesterday_avg > 0) {
//         float change = ((today_avg - yesterday_avg) / yesterday_avg) * 100.0f;
//         const char *dir = change > 0 ? "increased" : "decreased";
//         snprintf(answer, answer_size,
//             "Yesterday: AQI %.1f on %s. Today: AQI %.1f on %s. Pollution %s by %.1f%%.",
//             yesterday_avg, yesterday_data.date, today_avg, today_data.date, dir, fabsf(change));
//     } else {
//         snprintf(answer, answer_size,
//             "Today's AQI: %.1f. Not enough yesterday data for comparison.", today_avg);
//     }
//     return true;
// }

// // ==========================================
// // BACKGROUND PROCESSING TASK
// // ==========================================

// static void voice_processing_task(void *pv) {
//     voice_task_params_t *params = (voice_task_params_t *)pv;
//     char response[512] = {0};
    
//     if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
//         ch422g_set_sd_selected(true);
//         vTaskDelay(pdMS_TO_TICKS(10));
        
//         switch (params->cmd) {
//             case VOICE_CMD_TREND:
//             case VOICE_CMD_COMPARE_DAYS:
//                 analyze_two_days("/sdcard", response, sizeof(response));
//                 break;
//             case VOICE_CMD_WORST_READING: {
//                 time_t now;
//                 struct tm tm;
//                 time(&now);
//                 localtime_r(&now, &tm);
//                 int date_int = (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday;
//                 char filepath[64];
//                 snprintf(filepath, sizeof(filepath), "/sdcard/%08d.csv", date_int);
//                 float worst_aqi = 0, worst_pm25 = 0;
//                 char worst_time[64] = "";
//                 if (analyze_csv(filepath, NULL, NULL, &worst_aqi, &worst_pm25, worst_time) && worst_aqi > 0) {
//                     snprintf(response, sizeof(response), "Worst today: AQI %.1f, PM2.5 %.1f at %s",
//                              (double)worst_aqi, (double)worst_pm25, worst_time);
//                 } else {
//                     snprintf(response, sizeof(response), "No data available for today.");
//                 }
//                 break;
//             }
//             default:
//                 snprintf(response, sizeof(response), "Processing complete.");
//                 break;
//         }
        
//         ch422g_set_sd_selected(false);
//         xSemaphoreGive(sd_mutex);
//     } else {
//         snprintf(response, sizeof(response), "SD card busy. Try again.");
//     }
    
//     lv_async_call(voice_response_update_cb, strdup(response));
    
//     // Also publish to MQTT so mobile app gets the response
//     mqtt_publish_response(response);
    
//     voice_stop_animation();
//     free(params);
//     vTaskDelete(NULL);

// static void voice_response_update_cb(void *data) {
//     char *response = (char *)data;
    
//     if (voice_response_lbl) {
//         lv_label_set_text(voice_response_lbl, response);
//     }
//     if (voice_status_lbl) {
//         lv_label_set_text(voice_status_lbl, "Ready");
//     }
//     free(response);
// }

// // ==========================================
// // MAIN PROCESSING FUNCTION
// // ==========================================

// void aq_voice_engine_init(void) {
//     ESP_LOGI(TAG, "Voice engine initialized");
// }

// bool aq_voice_process(const char *voice_text, char *response, size_t response_size) {
//     if (cmd == VOICE_CMD_TREND || cmd == VOICE_CMD_COMPARE_DAYS || cmd == VOICE_CMD_WORST_READING) {
//         snprintf(response, response_size, 
//             "For detailed analysis, please use the touch screen on the device.");
//         return true;
//     }

//     if (voice_text == NULL || response == NULL || response_size == 0) return false;

//     // Only animate if called from UI thread (check if voice_orb exists)
//     // if (voice_orb != NULL) {
//     //     voice_start_thinking();
//     // }
    
//     response[0] = '\0';
//     bool success = false;
//     voice_command_t cmd = aq_voice_classify(voice_text);
    
//     ESP_LOGI(TAG, "Command: '%s' -> type %d", voice_text, cmd);
    
//     // Heavy commands - use background task
//     if (cmd == VOICE_CMD_TREND || cmd == VOICE_CMD_COMPARE_DAYS || cmd == VOICE_CMD_WORST_READING) {
//         voice_task_params_t *task_params = malloc(sizeof(voice_task_params_t));
//         if (task_params) {
//             task_params->cmd = cmd;
//             xTaskCreate(voice_processing_task, "voice_task", 8192, task_params, 5, NULL);
//         }
//         snprintf(response, response_size, "Analyzing data...");
//         return true;
//     }
    
//     // Light commands - process directly
//     switch (cmd) {
//         case VOICE_CMD_PREDICT_AQI:
//             if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
//                 sensor_data_t s = real_data;
//                 xSemaphoreGive(sensor_mutex);
                
//                 int predicted_aqi = s.aqi;
//                 const char *direction = "stable";
//                 const char *reason = "current levels are steady";
                
//                 if (s.pm25 > 60) {
//                     predicted_aqi = s.aqi * 1.1;
//                     direction = "increase";
//                     reason = "PM2.5 is elevated and may rise further";
//                 } else if (s.pm25 < 20) {
//                     predicted_aqi = s.aqi * 0.9;
//                     direction = "improve slightly";
//                     reason = "PM2.5 is low with good ventilation";
//                 }
                
//                 snprintf(response, response_size,
//                     "Prediction: AQI may %s to around %d in next hour. %s. Current AQI: %d, PM2.5: %.1f ug/m3, Temp: %.1fC.",
//                     direction, predicted_aqi, reason, s.aqi, (double)s.pm25, (double)s.temp);
//                 success = true;
//             }
//             break;
            
//         case VOICE_CMD_CURRENT_AQI:
//             if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
//                 sensor_data_t s = real_data;
//                 xSemaphoreGive(sensor_mutex);
//                 snprintf(response, response_size,
//                     "Current AQI is %d (%s). PM2.5: %.1f ug/m3, Temp: %.1fC, Humidity: %.1f%%, TVOC: %.1f ppb.",
//                     s.aqi, aqi_category(s.aqi),
//                     (double)s.pm25, (double)s.temp, (double)s.humidity, (double)s.tvoc);
//                 success = true;
//             }
//             break;
            
//         case VOICE_CMD_RECOMMENDATION:
//             if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
//                 sensor_data_t s = real_data;
//                 xSemaphoreGive(sensor_mutex);
//                 snprintf(response, response_size,
//                     "AQI is %d (%s). %s Keep windows closed when pollution is high. "
//                     "Use air purifier if available. Limit outdoor activity when AQI exceeds 100.",
//                     s.aqi, aqi_category(s.aqi),
//                     s.aqi <= 100 ? "Air quality is acceptable." : "Take precautions!");
//                 success = true;
//             }
//             break;
            
//         case VOICE_CMD_HEALTH_RISK:
//             if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
//                 sensor_data_t s = real_data;
//                 xSemaphoreGive(sensor_mutex);
                
//                 if (s.aqi <= 50) {
//                     snprintf(response, response_size,
//                         "AQI %d (%s). Air quality is good. No significant health risks.", s.aqi, aqi_category(s.aqi));
//                 } else if (s.aqi <= 100) {
//                     snprintf(response, response_size,
//                         "AQI %d (%s). Sensitive individuals may experience mild effects. "
//                         "Consider reducing prolonged outdoor exertion.", s.aqi, aqi_category(s.aqi));
//                 } else {
//                     snprintf(response, response_size,
//                         "AQI %d (%s). PM2.5: %.1f. Risk of respiratory issues, drowsiness, "
//                         "and headaches. Keep windows closed and use filtration.", s.aqi, aqi_category(s.aqi), (double)s.pm25);
//                 }
//                 success = true;
//             }
//             break;
            
//         case VOICE_CMD_FAN_STATUS:
//             snprintf(response, response_size, "Fan is %s at speed %d.",
//                 fan_manual_mode ? "Manual" : "Auto", fan_speed_level);
//             success = true;
//             break;
            
//         case VOICE_CMD_FAN_ON:
//             fan_speed_level = 1;
//             fan_manual_mode = true;
//             snprintf(response, response_size, "Fan turned ON at speed 1.");
//             success = true;
//             break;
            
//         case VOICE_CMD_FAN_OFF:
//             fan_speed_level = 0;
//             fan_manual_mode = true;
//             snprintf(response, response_size, "Fan turned OFF.");
//             success = true;
//             break;
            
//         case VOICE_CMD_EXPORT:
//             snprintf(response, response_size, "Export is available from Screen 3. Tap Export button to download reports.");
//             success = true;
//             break;
            
//         default:
//             snprintf(response, response_size,
//                 "I can tell you: current AQI, predictions, trends, health risks, day comparisons, and fan control.");
//             break;
//     }
    
//     // if (voice_orb != NULL) {
//     //     voice_stop_animation();
//     // }
//     return success;
// }
#include "aq_voice_engine.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "ch422g.h"
#include "ui.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "VOICE_ENGINE";
#define CSV_LINE_SIZE 512

extern sensor_data_t real_data;
extern SemaphoreHandle_t sensor_mutex;
extern SemaphoreHandle_t sd_mutex;
extern bool fan_manual_mode;
extern int fan_speed_level;
extern const int fan_speed_rpm[];
extern lv_obj_t *voice_response_lbl;
extern lv_obj_t *voice_status_lbl;

typedef struct {
    voice_command_t cmd;
} voice_task_params_t;

static void voice_processing_task(void *pv);
static void voice_response_update_cb(void *data);
static bool analyze_two_days(const char *dir_path, char *answer, size_t answer_size);
static bool analyze_csv(const char *filepath, float *worst_aqi, float *worst_pm25, char *worst_time);
static bool parse_csv_row(char *line, float *pm25, float *aqi, char *date_out, char *time_out);

extern void voice_processing_task(void *pv);

static bool str_contains(const char *text, const char *needle) {
    if (!text || !needle) return false;
    char lt[256], ln[128]; size_t i;
    for(i=0; text[i]&&i<255; i++) lt[i]=tolower((unsigned char)text[i]);
    lt[i]=0;
    for(i=0; needle[i]&&i<127; i++) ln[i]=tolower((unsigned char)needle[i]);
    ln[i]=0;
    return strstr(lt, ln) != NULL;
}


voice_command_t aq_voice_classify(const char *text) {
    if (!text) return VOICE_CMD_UNKNOWN;
    
    // Check specific matches BEFORE general ones
    if(str_contains(text,"fan status")||str_contains(text,"fan speed")) return VOICE_CMD_FAN_STATUS;
    if(str_contains(text,"turn on fan")||str_contains(text,"fan on")) return VOICE_CMD_FAN_ON;
    if(str_contains(text,"turn off fan")||str_contains(text,"fan off")) return VOICE_CMD_FAN_OFF;
    if(str_contains(text,"current aqi")||str_contains(text,"air quality now")) return VOICE_CMD_CURRENT_AQI;
    if(str_contains(text,"predict")||str_contains(text,"forecast")) return VOICE_CMD_PREDICT_AQI;
    if(str_contains(text,"worst")||str_contains(text,"highest")) return VOICE_CMD_WORST_READING;
    if(str_contains(text,"trend")||str_contains(text,"improving")||str_contains(text,"worsening")) return VOICE_CMD_TREND;
    if(str_contains(text,"recommend")||str_contains(text,"should i")) return VOICE_CMD_RECOMMENDATION;
    if(str_contains(text,"drows")||str_contains(text,"headache")||str_contains(text,"health")||str_contains(text,"risk")) return VOICE_CMD_HEALTH_RISK;
    if(str_contains(text,"yesterday")||str_contains(text,"compare")||str_contains(text,"percent")) return VOICE_CMD_COMPARE_DAYS;
    if(str_contains(text,"export")||str_contains(text,"report")) return VOICE_CMD_EXPORT;
    
    return VOICE_CMD_UNKNOWN;
}

static bool parse_csv_row(char *line, float *pm25, float *aqi, char *date_out, char *time_out) {
    char *fields[13] = {0};
    int count = 0;
    char *save = NULL;
    
    for (char *f = strtok_r(line, ",", &save); f && count < 13; f = strtok_r(NULL, ",", &save)) {
        while (*f == ' ' || *f == '\t') f++;
        size_t len = strlen(f);
        while (len > 0 && (f[len-1] == '\r' || f[len-1] == '\n')) f[--len] = '\0';
        fields[count++] = f;
    }
    
    if (count < 8) return false;
    if (strncmp(fields[0], "Date", 4) == 0 || strncmp(fields[0], "OPRUSS", 6) == 0) return false;
    
    strncpy(date_out, fields[0], 15); date_out[15] = '\0';
    snprintf(time_out, 31, "%s", (count >= 13) ? fields[1] : "");
    
    int pm25_idx = (count >= 13) ? 2 : 1;
    *pm25 = (float)atof(fields[pm25_idx]);
    
    float p = *pm25;
    if (p <= 30) *aqi = (50.0f/30.0f)*p;
    else if (p <= 60) *aqi = 51+(49.0f/30.0f)*(p-30);
    else if (p <= 90) *aqi = 101+(99.0f/30.0f)*(p-60);
    else if (p <= 120) *aqi = 201+(99.0f/30.0f)*(p-90);
    else if (p <= 250) *aqi = 301+(99.0f/130.0f)*(p-120);
    else *aqi = 401+(99.0f/100.0f)*(p-250);
    
    return true;
}

static bool analyze_csv(const char *filepath, float *worst_aqi, float *worst_pm25, char *worst_time) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;
    
    char line[512];
    *worst_aqi = 0;
    
    while (fgets(line, sizeof(line), f)) {
        float pm25, aqi;
        char date[16], time_str[32];
        if (!parse_csv_row(line, &pm25, &aqi, date, time_str)) continue;
        if (aqi > *worst_aqi) {
            *worst_aqi = aqi;
            *worst_pm25 = pm25;
            snprintf(worst_time, 64, "%s %s", date, time_str);
        }
    }
    fclose(f);
    return *worst_aqi > 0;
}

static bool analyze_two_days(const char *dir_path, char *answer, size_t answer_size) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        snprintf(answer, answer_size, "Cannot access SD card.");
        return false;
    }
    
    char files[31][64];
    int file_count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) && file_count < 30) {
        if (strstr(entry->d_name, ".csv") && strlen(entry->d_name) == 12) {
            strncpy(files[file_count], entry->d_name, 63);
            file_count++;
        }
    }
    closedir(dir);
    
    if (file_count < 2) {
        snprintf(answer, answer_size, "Need at least 2 days of data. Please wait for more logs.");
        return false;
    }
    
    // Sort by date
    for (int i = 0; i < file_count-1; i++)
        for (int j = i+1; j < file_count; j++)
            if (strcmp(files[i], files[j]) > 0) {
                char tmp[64]; strcpy(tmp, files[i]);
                strcpy(files[i], files[j]); strcpy(files[j], tmp);
            }
    
    char today_path[128], yesterday_path[128];
    snprintf(today_path, sizeof(today_path), "%s/%s", dir_path, files[file_count-1]);
    snprintf(yesterday_path, sizeof(yesterday_path), "%s/%s", dir_path, files[file_count-2]);
    
    // Analyze yesterday
    float yesterday_sum = 0, yesterday_pm25_sum = 0;
    int yesterday_count = 0;
    char yesterday_date[16] = "";
    float worst_aqi = 0, worst_pm25 = 0; char worst_time[64] = "";
    
    FILE *yf = fopen(yesterday_path, "r");
    if (yf) {
        char line[512];
        while (fgets(line, sizeof(line), yf)) {
            float pm25, aqi; char date[16], time_str[32];
            if (parse_csv_row(line, &pm25, &aqi, date, time_str)) {
                yesterday_sum += aqi;
                yesterday_pm25_sum += pm25;
                yesterday_count++;
                strncpy(yesterday_date, date, 15);
            }
        }
        fclose(yf);
    }
    
    // Analyze today
    float today_sum = 0, today_pm25_sum = 0;
    int today_count = 0;
    char today_date[16] = "";
    
    FILE *tf = fopen(today_path, "r");
    if (tf) {
        char line[512];
        while (fgets(line, sizeof(line), tf)) {
            float pm25, aqi; char date[16], time_str[32];
            if (parse_csv_row(line, &pm25, &aqi, date, time_str)) {
                today_sum += aqi;
                today_pm25_sum += pm25;
                today_count++;
                strncpy(today_date, date, 15);
                if (aqi > worst_aqi) { worst_aqi = aqi; worst_pm25 = pm25; }
            }
        }
        fclose(tf);
    }
    
    float yesterday_avg = yesterday_count > 0 ? yesterday_sum/yesterday_count : 0;
    float today_avg = today_count > 0 ? today_sum/today_count : 0;
    float yesterday_pm25_avg = yesterday_count > 0 ? yesterday_pm25_sum/yesterday_count : 0;
    float today_pm25_avg = today_count > 0 ? today_pm25_sum/today_count : 0;
    
    if (yesterday_avg > 0) {
        float aqi_change = ((today_avg - yesterday_avg)/yesterday_avg)*100;
        float pm25_change = yesterday_pm25_avg > 0 ? ((today_pm25_avg - yesterday_pm25_avg)/yesterday_pm25_avg)*100 : 0;
        snprintf(answer, answer_size,
            "Yesterday: AQI %.1f, PM2.5 %.1f | Today: AQI %.1f, PM2.5 %.1f | "
            "AQI %s by %.1f%%, PM2.5 %s by %.1f%%.",
            yesterday_avg, (double)yesterday_pm25_avg,
            today_avg, (double)today_pm25_avg,
            aqi_change > 0 ? "increased" : "decreased", fabsf(aqi_change),
            pm25_change > 0 ? "increased" : "decreased", fabsf(pm25_change));
    } else {
        snprintf(answer, answer_size, "Today AQI: %.1f, PM2.5: %.1f. Yesterday data insufficient.", today_avg, (double)today_pm25_avg);
    }
    return true;
}

static void voice_processing_task(void *pv) {
    voice_task_params_t *params = (voice_task_params_t *)pv;
    char response[512] = {0};
    
    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        ch422g_set_sd_selected(true);
        vTaskDelay(pdMS_TO_TICKS(10));
        
        switch (params->cmd) {
            case VOICE_CMD_COMPARE_DAYS:
            case VOICE_CMD_TREND:
                analyze_two_days("/sdcard", response, sizeof(response));
                break;
            case VOICE_CMD_WORST_READING: {
                time_t now; struct tm tm;
                time(&now); localtime_r(&now, &tm);
                int d = (tm.tm_year+1900)*10000 + (tm.tm_mon+1)*100 + tm.tm_mday;
                char fp[64]; snprintf(fp, sizeof(fp), "/sdcard/%08d.csv", d);
                float wa=0, wp=0; char wt[64]="";
                if (analyze_csv(fp, &wa, &wp, wt) && wa>0)
                    snprintf(response, sizeof(response), "Worst today: AQI %.1f, PM2.5 %.1f at %s", (double)wa, (double)wp, wt);
                else snprintf(response, sizeof(response), "No data for today yet.");
                break;
            }
            default:
                snprintf(response, sizeof(response), "Analysis complete.");
                break;
        }
        
        ch422g_set_sd_selected(false);
        xSemaphoreGive(sd_mutex);
    } else {
        snprintf(response, sizeof(response), "SD card busy. Try again.");
    }
    
    lv_async_call(voice_response_update_cb, strdup(response));
    free(params);
    vTaskDelete(NULL);
}

static void voice_response_update_cb(void *data) {
    char *text = (char *)data;
    if (voice_response_lbl) lv_label_set_text(voice_response_lbl, text);
    if (voice_status_lbl) lv_label_set_text(voice_status_lbl, "Ready");
    free(text);
}

void aq_voice_engine_init(void) { ESP_LOGI(TAG, "Voice engine ready"); }

bool aq_voice_process(const char *voice_text, char *response, size_t response_size, bool from_mqtt) {
    if (!voice_text || !response || !response_size) return false;
    response[0] = '\0';
    
    voice_command_t cmd = aq_voice_classify(voice_text);
    ESP_LOGI(TAG, "Cmd: '%s' -> %d", voice_text, cmd);
    
    // Heavy SD commands - safe message for MQTT
    if (from_mqtt && (cmd == VOICE_CMD_TREND || cmd == VOICE_CMD_COMPARE_DAYS || cmd == VOICE_CMD_WORST_READING)) {
        snprintf(response, response_size, "For detailed analysis, use the device touch screen.");
        return true;
    }
    
    // Light commands
    switch (cmd) {
        case VOICE_CMD_CURRENT_AQI:
            if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                sensor_data_t s = real_data;
                xSemaphoreGive(sensor_mutex);
                snprintf(response, response_size,
                    "Current AQI: %d | PM2.5: %.1f | PM10: %.1f | Temp: %.1fC | Humidity: %.1f%% | TVOC: %.1f ppb",
                    s.aqi, (double)s.pm25, (double)s.pm10, (double)s.temp, (double)s.humidity, (double)s.tvoc);
                return true;
            }
            snprintf(response, response_size, "Cannot read sensor data.");
            return false;

        case VOICE_CMD_PREDICT_AQI:
            if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                sensor_data_t s = real_data;
                xSemaphoreGive(sensor_mutex);
                int predicted = s.pm25 > 60 ? s.aqi * 1.1 : s.aqi * 0.9;
                snprintf(response, response_size,
                    "Predicted AQI next hour: %d (Current: %d). PM2.5: %.1f. %s",
                    predicted, s.aqi, (double)s.pm25,
                    s.pm25 > 60 ? "May worsen." : "Should remain stable.");
                return true;
            }
            snprintf(response, response_size, "Cannot read sensor data.");
            return false;

        case VOICE_CMD_COMPARE_DAYS:    // ADD
        case VOICE_CMD_TREND:           // ADD
        case VOICE_CMD_WORST_READING:   // ADD
            if (!from_mqtt) {
                // Spawn background task for SD analysis
                voice_task_params_t *tp = malloc(sizeof(voice_task_params_t));
                if (tp) {
                    tp->cmd = cmd;
                    xTaskCreate(voice_processing_task, "voice_bg", 8192, tp, 5, NULL);
                    snprintf(response, response_size, "Analyzing data...");
                }
            } else {
                snprintf(response, response_size, "For detailed analysis, use the device touch screen.");
            }
            return true;

        case VOICE_CMD_RECOMMENDATION:
            if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                sensor_data_t s = real_data;
                xSemaphoreGive(sensor_mutex);
                snprintf(response, response_size, "AQI is %d. %s", s.aqi,
                    s.aqi <= 100 ? "Air quality acceptable." : "Take precautions, limit outdoor activity.");
                return true;
            }

            return false;
        case VOICE_CMD_HEALTH_RISK:
            if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                sensor_data_t s = real_data;
                xSemaphoreGive(sensor_mutex);
                snprintf(response, response_size, "AQI %d, PM2.5 %.1f. %s",
                    s.aqi, (double)s.pm25,
                    s.aqi <= 50 ? "No health risks." : s.aqi <= 100 ? "Mild risk for sensitive people." : "Risk of respiratory issues.");
                return true;
            }
            return false;
        case VOICE_CMD_FAN_STATUS:
            snprintf(response, response_size, "Fan: %s, Speed: %d", fan_manual_mode ? "Manual" : "Auto", fan_speed_level);
            return true;
        case VOICE_CMD_FAN_ON: fan_speed_level=1; fan_manual_mode=true; snprintf(response, response_size, "Fan ON at speed 1."); return true;
        case VOICE_CMD_FAN_OFF: fan_speed_level=0; fan_manual_mode=true; snprintf(response, response_size, "Fan OFF."); return true;
        case VOICE_CMD_EXPORT: snprintf(response, response_size, "Export from Screen 3 on device."); return true;
        default:
            snprintf(response, response_size, "Ask: AQI, health, trends, fan control.");
            return false;
    }
}