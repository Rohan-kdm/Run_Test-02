#include "ui.h"
#include "web_download.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h> // For rand()
#include <string.h>
#include <stdbool.h>
#include "opruss_boot_logo.h"
#include "OPRUSS_logo_black_bg.h"
#include <time.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2c.h"
#include <errno.h>
#include <string.h>
// #include "lv_qrcode.h"
#include "esp_netif.h"
// #include "backlight.h"


typedef struct
{
    uint32_t samples;

    float sum_aqi;
    float sum_pm25;
    float sum_pm10;
    float sum_temp;
    float sum_humidity;
    float sum_tvoc;

    float max_aqi;
    float min_aqi;

} report_stats_t;

#define I2C_MASTER_NUM I2C_NUM_0  // Or I2C_NUM_1 depending on your port selection

// --- CSS Color Variables Translated ---
#define COLOR_BG        lv_color_hex(0x0B0E14)
#define COLOR_CARD      lv_color_hex(0x161A23)
#define COLOR_LINE      lv_color_hex(0x262D3A)
#define COLOR_TEXT      lv_color_hex(0xF5F7FA)

// --- OPRUSS Brand Colors Applied ---
#define COLOR_DIM       lv_color_hex(0x666767) // Changed to OPRUSS Secondary Grey
#define COLOR_ACCENT    lv_color_hex(0xF27330) // Changed to OPRUSS Primary Orange

#define COLOR_EXCELLENT lv_color_hex(0x2ECC71) // Good / Connected
#define COLOR_POOR      lv_color_hex(0xE74C3C) // Poor / Disconnected
#define COLOR_GOOD      lv_color_hex(0xF1C40F) // Moderate
#define COLOR_TEXT_SECONDARY lv_color_hex(0xB0BEC5)

// --- OPRUSS Brand Colors ---
#define COLOR_OPRUSS_ORG lv_color_hex(0xF27330) // Primary Orange
#define COLOR_OPRUSS_GRY lv_color_hex(0x666767) // Secondary Grey

#define CHART_POINTS 24

#define MOUNT_POINT "/sdcard"

static int16_t pm25_history[CHART_POINTS];
static int16_t pm10_history[CHART_POINTS];
static int16_t tvoc_history[CHART_POINTS];
static int16_t aqi_history[CHART_POINTS];
static int16_t temp_history[CHART_POINTS];
static int16_t hum_history[CHART_POINTS];

static int16_t weekly_aqi_history[7];

static lv_obj_t *calendar_obj = NULL;

static lv_obj_t *month_dd = NULL;
static lv_obj_t *year_dd = NULL;

static lv_obj_t *next_btn = NULL;

static lv_calendar_date_t start_date;
static lv_calendar_date_t end_date;

static bool selecting_start = true;

static char year_list[3000];

static void build_year_list(void)
{
    year_list[0]=0;

    char tmp[8];

    for(int y=1901;y<=2026;y++)
    {
        sprintf(tmp,"%d",y);
        strcat(year_list,tmp);

        if(y!=2026)
            strcat(year_list,"\n");
    }
}

// ==========================================
// REAL DATA INTEGRATION HOOKS
// ==========================================
// Toggle this to false to use real ESP-NOW and Wi-Fi data
bool use_mock_data = false;

// Your external RTOS tasks (ESP-NOW receiver, Wi-Fi event loop) 
// should update these variables directly.
bool wifi_is_connected = false;

// typedef struct {
//     int aqi;
//     int pm25;
//     int pm10;
//     int tvoc;
//     float temp;
//     int humidity;
//     int fan_speed;
//     int filter_life;
// } sensor_data_t;

sensor_data_t real_data = {0};
extern SemaphoreHandle_t sensor_mutex;

extern SemaphoreHandle_t sd_mutex;

// Temporary stub implementation to satisfy the linker
// void set_backlight_level(uint8_t level) {
//     // TODO: Hardware LEDC / PWM duty cycle logic goes here
//     (void)level; // Prevents unused parameter warning
// }

// ==========================================
// DYNAMIC UI ELEMENT POINTERS
// ==========================================
static lv_obj_t * top_time_lbl = NULL;
static lv_obj_t * top_wifi_lbl = NULL;
static lv_obj_t * aqi_arc = NULL;
static lv_obj_t * aqi_val_lbl = NULL;
static lv_obj_t * aqi_cat_lbl = NULL;

static lv_obj_t * pm25_val_lbl = NULL;
static lv_obj_t * pm10_val_lbl = NULL;
static lv_obj_t * tvoc_val_lbl = NULL;
static lv_obj_t * temp_val_lbl = NULL;
static lv_obj_t * hum_val_lbl = NULL;

static lv_obj_t * purifier_fan_lbl = NULL;
static lv_obj_t * purifier_filter_lbl = NULL;
static lv_obj_t * purifier_status_lbl = NULL; 
static uint32_t filter_start_time = 0;

static lv_obj_t *email_modal;
static lv_obj_t *email_ta;
static lv_obj_t *email_kb;

static lv_obj_t *voice_orb;
static lv_obj_t *voice_status_lbl;
static lv_obj_t *voice_response_lbl;

static lv_anim_t orb_anim;

static const char *TAG = "UI";

static lv_chart_series_t *ser_pm25 = NULL;
static lv_chart_series_t *ser_c3_pm25 = NULL;

static lv_chart_series_t *ser_pm10 = NULL;

static lv_obj_t *chart1 = NULL;
static lv_obj_t *chart3 = NULL;

static lv_obj_t *chart2 = NULL;
static lv_obj_t *chart4 = NULL;
static lv_obj_t *chart5 = NULL;
static lv_obj_t *chart6 = NULL;
static lv_obj_t *chart7 = NULL;
static lv_obj_t *chart8 = NULL;

static lv_chart_series_t *ser_aqi = NULL;

static lv_chart_series_t *ser_c3_pm10 = NULL;
static lv_chart_series_t *ser_c3_tvoc = NULL;

static lv_chart_series_t *ser_c4_comp = NULL;
static lv_chart_series_t *ser_c5_dist = NULL;
static lv_chart_series_t *ser_c6_eff = NULL;
static lv_chart_series_t *ser_c7_comf = NULL;
static lv_chart_series_t *ser_c8_week = NULL;

static lv_obj_t *calendar_modal = NULL;
static lv_obj_t *calendar_title = NULL;
static lv_obj_t *cancel_btn = NULL;
static lv_obj_t *close_btn = NULL;

static lv_calendar_date_t custom_start_date;
static lv_calendar_date_t custom_end_date;
static lv_calendar_date_t selected_dates[2];
static bool selecting_start_date = true; // Toggle between picking 'From' and 'To'
static lv_obj_t * calendar_popup = NULL;

static const char *month_list =
"January\n"
"February\n"
"March\n"
"April\n"
"May\n"
"June\n"
"July\n"
"August\n"
"September\n"
"October\n"
"November\n"
"December";

static void export_btn_event_cb(lv_event_t * e);
static void export_popup_create(void);


// Global pointers for Screen 3 values
lv_obj_t * val_aqi_daily = NULL;
lv_obj_t * val_aqi_weekly = NULL;
lv_obj_t * val_aqi_max = NULL;
lv_obj_t * val_aqi_min = NULL;
lv_obj_t * val_pm25 = NULL;
lv_obj_t * val_pm10 = NULL;
lv_obj_t * val_temp = NULL;
lv_obj_t * val_hum = NULL;
lv_obj_t * val_comp = NULL;
lv_obj_t * val_peak = NULL;
lv_obj_t * val_tvoc = NULL;
lv_obj_t * val_status = NULL;



// Forward declarations of screen builders
static lv_obj_t* build_splash_screen(void);
static lv_obj_t* build_screen_1(void);
static lv_obj_t* build_screen_2(void);
static lv_obj_t* build_screen_3(void);
static lv_obj_t* build_screen_4(void);
static lv_obj_t* build_screen_5(void);
static lv_obj_t* build_screen_6(void);
static void ui_init_main(void);

static int current_screen_idx = 0;

//Forward declaration for time and wifi
static char last_time[32] = "--:--";

static bool merge_csv_files(int start_day, int end_day)
{    // 1. Turn off / heavy dim backlight
    // set_backlight_level(0);
    
    // CRITICAL: Force LVGL to flush pending draw calls before blocking the CPU
    // lv_refr_now(NULL);
    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(5000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to obtain SD mutex");
        return false;
    }
    FILE *dst = NULL;
    FILE *src = NULL;

    char src_name[128];
    char line[512];

    // bool header_written = false;
    bool found_any_file = false;

    report_stats_t stats = {0};

    stats.min_aqi = 100000.0f;
    stats.max_aqi = -100000.0f;

    ESP_LOGI(TAG, "Creating export report...");

    /* Create output report */
    // dst = fopen(MOUNT_POINT "/export_temp.csv", "w");

    // if (dst == NULL)
    // {
    //     ESP_LOGE(TAG, "Failed to create export file");
    //     return false;
    // }

    // fprintf(dst, "OPRUSS AQ7 AIR QUALITY REPORT\n");
    // fprintf(dst, "Generated Report\n\n");
    // fprintf(dst, "Date,Time,PM2.5,Temp,Humidity,Pressure,Gas,TVOC,eCO2,PM10,Fan Speed,Filter Life,AQI\n");

    /* Loop through requested dates */
    struct tm date_tm = {0};

    /* Convert YYYYMMDD -> struct tm */
    date_tm.tm_year = (start_day / 10000) - 1900;
    date_tm.tm_mon  = ((start_day / 100) % 100) - 1;
    date_tm.tm_mday = start_day % 100;

    while (1)
    {
        /* Normalize date (handles month/year/leap year rollover) */
        mktime(&date_tm);

        int file_date =
            (date_tm.tm_year + 1900) * 10000 +
            (date_tm.tm_mon + 1) * 100 +
            date_tm.tm_mday;

        if (file_date > end_day)
            break;

        snprintf(src_name,
                sizeof(src_name),
                MOUNT_POINT "/%08d.csv",
                file_date);


        ESP_LOGI(TAG, "Opening (PASS1): %s", src_name);
        src = fopen(src_name, "r");

        if (src == NULL)
        {
            ESP_LOGE(TAG, "PASS1 fopen FAILED: %s", src_name);
        }
        else
        {
            ESP_LOGI(TAG, "PASS1 fopen OK: %s", src_name);
        }

        if (src == NULL)
        {
            ESP_LOGW(TAG, "Missing: %s", src_name);
        }
        else
        {
            found_any_file = true;

            ESP_LOGI(TAG, "Merging: %s", src_name);

           char line[256];

            while (fgets(line, sizeof(line), src))
            {
                // 1. Strip trailing \r and \n characters (fixes Windows line ending issues)
                size_t len = strlen(line);
                while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
                    line[--len] = '\0';
                }

                // 2. Skip empty lines
                if (len == 0) {
                    continue;
                }

                // 3. Skip CSV Header row if present
                if (strncmp(line, "Date", 4) == 0 || strncmp(line, "Time", 4) == 0) {
                    continue;
                }

                sensor_data_t sample = {0};
                char date[16];
                char time[16];

                // 4. Parse the 13 CSV fields
                // 1. Try parsing the expected 13-column format first
                int parsed_count = sscanf(line, 
                    "%15[^,],%15[^,],%f,%f,%f,%f,%f,%f,%f,%f,%d,%d,%d",
                    date, time, &sample.pm25, &sample.temp, &sample.humidity, 
                    &sample.pressure, &sample.gas, &sample.tvoc, &sample.eco2, 
                    &sample.pm10, &sample.fan_speed, &sample.filter_life, &sample.aqi);

                // 2. If it fails, fallback to the 8-column test data format
                if (parsed_count != 13) {
                    parsed_count = sscanf(line, 
                        "%15[^ ] %15[^,],%f,%f,%f,%f,%f,%f,%f",
                        date,       // Grabs "7/14/2026" (stops at the space)
                        time,       // Grabs "17:04:28" (stops at the comma)
                        &sample.pm25,      // Grabs 16
                        &sample.temp,      // Grabs 429496736
                        &sample.humidity,  // Grabs 429496768
                        &sample.pressure,  // Grabs 1015
                        &sample.gas,       // Grabs 4304
                        &sample.tvoc,      // Grabs 62
                        &sample.eco2       // Grabs 426
                    );
                    
                    // Override parsed_count to trick the rest of your logic into 
                    // thinking it got a full 13-column row, so the merge continues.
                    if (parsed_count == 9) {
                        parsed_count = 13; 
                        
                        // Manually zero out the remaining missing columns so you 
                        // don't merge random garbage memory into your final files.
                        sample.pm10 = 0;
                        sample.fan_speed = 0;
                        sample.filter_life = 0;
                        sample.aqi = 0;
                    }
                }

                // 5. Check if all 13 fields were successfully parsed
                // if (parsed_count != 13)
                // {
                //     // Log what went wrong and how many items were actually read
                //     ESP_LOGW(TAG, "Invalid CSV row (parsed %d/13): '%s'", parsed_count, line);
                //     continue;
                // }

                /* Update report stats for valid rows */
                if (stats.samples == 0) {
                    // Initialize min/max on the first valid sample
                    stats.max_aqi = sample.aqi;
                    stats.min_aqi = sample.aqi;
                } else {
                    if (sample.aqi > stats.max_aqi) stats.max_aqi = sample.aqi;
                    if (sample.aqi < stats.min_aqi) stats.min_aqi = sample.aqi;
                }

                stats.samples++;
                stats.sum_aqi       += sample.aqi;
                stats.sum_pm25      += sample.pm25;
                stats.sum_pm10      += sample.pm10;
                stats.sum_temp      += sample.temp;
                stats.sum_humidity  += sample.humidity;
                stats.sum_tvoc      += sample.tvoc;
            }

            ESP_LOGI(TAG, "PASS1 Closing %s", src_name);
            fclose(src);
            ESP_LOGI(TAG, "PASS1 Closed");
        }
        
        date_tm.tm_mday++; // <--- FIX 1: Added missing day increment so PASS 1 doesn't loop infinitely
    } // <--- FIX 2: Added missing brace to close PASS 1's "while(1)" loop

    /* Compute Averages Safely */
    float avg_aqi      = stats.sum_aqi / stats.samples;
    float avg_pm25     = stats.sum_pm25 / stats.samples;
    float avg_pm10     = stats.sum_pm10 / stats.samples;
    float avg_temp     = stats.sum_temp / stats.samples;
    float avg_humidity = stats.sum_humidity / stats.samples;
    float avg_tvoc     = stats.sum_tvoc / stats.samples;

    /* Create Output Report */
    dst = fopen(MOUNT_POINT "/report.csv", "w");
    // dst = fopen(MOUNT_POINT "/test.txt", "w");
    if (dst == NULL)
    {
        ESP_LOGE(TAG,
         "Failed to create export file: errno=%d (%s)",
         errno,
         strerror(errno));
        xSemaphoreGive(sd_mutex);
        return false;
    }

    /* Generate Current Time String */
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char gen_time[64];
    strftime(gen_time, sizeof(gen_time), "%Y-%m-%d %H:%M:%S", &timeinfo);

    /* Write Report Headers & Summary */
    fprintf(dst, "OPRUSS AQ7 AIR QUALITY REPORT\n\n");
    fprintf(dst, "Generated:\n%s\n\n", gen_time);
    fprintf(dst, "Period:\n%08d to %08d\n\n", start_day, end_day);

    fprintf(dst, "SUMMARY\n");
    fprintf(dst, "Metric,Value\n");
    fprintf(dst, "Samples,%lu\n", (unsigned long)stats.samples);
    fprintf(dst, "Average AQI,%.1f\n", avg_aqi);
    fprintf(dst, "Maximum AQI,%.1f\n", stats.max_aqi);
    fprintf(dst, "Minimum AQI,%.1f\n", stats.min_aqi);
    fprintf(dst, "Average PM2.5,%.1f\n", avg_pm25);
    fprintf(dst, "Average PM10,%.1f\n", avg_pm10);
    fprintf(dst, "Average Temperature,%.1f\n", avg_temp);
    fprintf(dst, "Average Humidity,%.1f\n", avg_humidity);
    fprintf(dst, "Average TVOC,%.1f\n\n", avg_tvoc);

    /* Prepare for Pass 2 */
    fprintf(dst, "RAW DATA\n");
    fprintf(dst, "Date,Time,PM2.5,Temp,Humidity,Pressure,Gas,TVOC,eCO2,PM10,Fan Speed,Filter Life,AQI\n");

    /* --- PASS 2: COPY RAW DATA --- */
    
    /* Reset Date Struct */
    date_tm.tm_year = (start_day / 10000) - 1900;
    date_tm.tm_mon  = ((start_day / 100) % 100) - 1;
    date_tm.tm_mday = start_day % 100;

    while (1)
    {
        mktime(&date_tm);
        int file_date = (date_tm.tm_year + 1900) * 10000 + (date_tm.tm_mon + 1) * 100 + date_tm.tm_mday;
        
        if (file_date > end_day)
            break;

        snprintf(src_name, sizeof(src_name), MOUNT_POINT "/%08d.csv", file_date);

        ESP_LOGI(TAG, "Opening (PASS2): %s", src_name);
        src = fopen(src_name, "r");


        if (src == NULL)
        {
            ESP_LOGE(TAG, "PASS2 fopen FAILED: %s", src_name);
        }
        else
        {
            ESP_LOGI(TAG, "PASS2 fopen OK: %s", src_name);
        }

        if (src != NULL)
        {
            ESP_LOGI(TAG, "Appending raw data: %s", src_name);

            char line[256];

            while (fgets(line, sizeof(line), src))
            {
                // 1. Strip trailing \r and \n characters
                size_t len = strlen(line);
                while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
                    line[--len] = '\0';
                }

                // 2. Skip empty lines
                if (len == 0) {
                    continue;
                }

                // 3. Skip daily CSV header rows so they aren't written into the middle of the merged file
                if (strncmp(line, "Date", 4) == 0 || strncmp(line, "Time", 4) == 0) {
                    continue;
                }

                // 4. Validate structure matching PASS 1
                sensor_data_t sample = {0};
                char date[16];
                char time[16];

                // 1. Try parsing the expected 13-column format first
                int parsed_count = sscanf(line, 
                    "%15[^,],%15[^,],%f,%f,%f,%f,%f,%f,%f,%f,%d,%d,%d", 
                    date, time, &sample.pm25, &sample.temp, &sample.humidity, 
                    &sample.pressure, &sample.gas, &sample.tvoc, &sample.eco2, 
                    &sample.pm10, &sample.fan_speed, &sample.filter_life, &sample.aqi);

                // 2. If it fails, fallback to the 8-column test data format
                if (parsed_count != 13) {
                    parsed_count = sscanf(line, 
                        "%15[^ ] %15[^,],%f,%f,%f,%f,%f,%f,%f",
                        date,       // Grabs "7/14/2026" (stops at the space)
                        time,       // Grabs "17:04:28" (stops at the comma)
                        &sample.pm25,      // Grabs 16
                        &sample.temp,      // Grabs 429496736
                        &sample.humidity,  // Grabs 429496768
                        &sample.pressure,  // Grabs 1015
                        &sample.gas,       // Grabs 4304
                        &sample.tvoc,      // Grabs 62
                        &sample.eco2       // Grabs 426
                    );
                    
                    // Override parsed_count to trick the rest of your logic into 
                    // thinking it got a full 13-column row, so the merge continues.
                    if (parsed_count == 9) {
                        parsed_count = 13; 
                        
                        // Manually zero out the remaining missing columns so you 
                        // don't merge random garbage memory into your final files.
                        sample.pm10 = 0;
                        sample.fan_speed = 0;
                        sample.filter_life = 0;
                        sample.aqi = 0;
                    }
                }
                // 5. Skip any corrupt/invalid rows
                if (parsed_count != 13)
                {
                    ESP_LOGW(TAG, "PASS2 Skipping invalid row (parsed %d/13): '%s'", parsed_count, line);
                    continue;
                }

                // 6. Write valid row to destination file
                fprintf(dst, "%s\n", line);
            }

            ESP_LOGI(TAG, "PASS2 Closing %s", src_name);
            fclose(src);
            ESP_LOGI(TAG, "PASS2 Closed");
        } // <--- FIX 3: Added missing brace to close `if (src != NULL)`. It got removed when you commented out the block below it!
        
        //    while (fgets(line, sizeof(line), src))
        //    {
        //        ESP_LOGI(TAG, "PASS2 Read line");
        //        /* Skip source CSV header */
        //        if (strncmp(line, "Date,", 5) == 0) {
        //            continue;
        //        }
        //        /* Fast direct copy to output */
        //        fputs(line, dst);
        //    }
        //    ESP_LOGI(TAG, "PASS2 Closing %s", src_name);
        //    fclose(src);
        //    ESP_LOGI(TAG, "PASS2 Closed");
        // }
        
        date_tm.tm_mday++;
    }

    // <--- FIX 4: Removed rogue "}" right here.

    fclose(dst);
    ESP_LOGI(TAG, "Export report successfully created.");

    xSemaphoreGive(sd_mutex);

    // 2. Turn backlight back on
    // set_backlight_level(100);


    return true;

}

static void calendar_event_cb(lv_event_t * e)
{
    // lv_event_code_t code = lv_event_get_code(e);
    // lv_obj_t * calendar = lv_event_get_target(e);

    // if(code == LV_EVENT_VALUE_CHANGED) {
    //     lv_calendar_date_t date;
    //     if(lv_calendar_get_pressed_date(calendar, &date)) {
    //         if(selecting_start_date) {
    //             custom_start_date = date;
    //             selecting_start_date = false;
    //             // Optionally, update a label here to tell the user to "Select End Date"
    //         } else {
    //             custom_end_date = date;
    //             selecting_start_date = true;
                
    //             // Both dates selected, close the calendar modal
    //             lv_obj_del_async(calendar_modal);
    //             calendar_modal = NULL;
                
    //             ESP_LOGI(TAG, "Custom Range Selected: %d/%d/%d to %d/%d/%d", 
    //                      custom_start_date.day, custom_start_date.month, custom_start_date.year,
    //                      custom_end_date.day, custom_end_date.month, custom_end_date.year);
    //         }
    //     }
    // }
    ESP_LOGI(TAG, "Target=%p", lv_event_get_target(e));
    ESP_LOGI(TAG, "Current=%p", lv_event_get_current_target(e));
    ESP_LOGI(TAG, "Stored=%p", calendar_obj);

    lv_calendar_date_t date;

    bool ok = lv_calendar_get_pressed_date(calendar_obj, &date);

    ESP_LOGI(TAG, "Pressed=%d", ok);

    if(!ok) return;

    if(selecting_start_date)
    {
    custom_start_date = date;

    selecting_start_date = false;

    lv_label_set_text(calendar_title,
                      "Select End Date");
    
    selected_dates[0] = custom_start_date;

    lv_calendar_set_highlighted_dates(calendar_obj,
                                    selected_dates,
                                    1);

    return;
    }

custom_end_date = date;

selected_dates[1] = custom_end_date;

lv_calendar_set_highlighted_dates(calendar_obj,
                                  selected_dates,
                                  2);

ESP_LOGI(TAG,
         "Range %02d/%02d/%04d -> %02d/%02d/%04d",
         custom_start_date.day,
         custom_start_date.month,
         custom_start_date.year,
         custom_end_date.day,
         custom_end_date.month,
         custom_end_date.year);

lv_obj_del_async(calendar_modal);

calendar_modal = NULL;

selecting_start_date = true;
}

static void dropdown_cb(lv_event_t *e)
{
    int month =
        lv_dropdown_get_selected(month_dd)+1;

    int year =
        lv_dropdown_get_selected(year_dd)+1901;

    lv_calendar_set_showed_date(calendar_obj,
                                year,
                                month);
}

static void close_calendar_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    if(calendar_modal)
    {
        lv_obj_del_async(calendar_modal);
        calendar_modal = NULL;
    }
}

static void create_calendar_modal(void)
{
    if(calendar_modal != NULL) return;

    calendar_modal = lv_obj_create(lv_scr_act());
    lv_obj_clear_flag(calendar_modal, LV_OBJ_FLAG_SCROLLABLE);
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    int current_year = timeinfo.tm_year + 1900;
    int current_month = timeinfo.tm_mon + 1;
    int current_day = timeinfo.tm_mday;
    lv_obj_set_size(calendar_modal, 330, 420);
    lv_obj_center(calendar_modal);
    lv_obj_set_style_bg_color(calendar_modal, COLOR_CARD, 0); // Using your defined colors
    lv_obj_set_style_border_color(calendar_modal, COLOR_LINE, 0);
    
    // Title
    // lv_obj_t * title = lv_label_create(calendar_modal);
    // lv_label_set_text(title, "Select Start Date");
    calendar_title = lv_label_create(calendar_modal);

    lv_label_set_text(calendar_title,
                    "Select Start Date");

    lv_obj_set_style_text_color(calendar_title,
                            COLOR_TEXT,
                            0);

    lv_obj_align(calendar_title,
                LV_ALIGN_TOP_MID,
                0,
                15);


    // LVGL Calendar Widget
    // lv_obj_t  * calendar = lv_calendar_create(calendar_modal);
    month_dd = lv_dropdown_create(calendar_modal);

    lv_dropdown_set_options(month_dd,
                            month_list);

    lv_dropdown_set_selected(month_dd,
                            current_month - 1);

    lv_obj_set_width(month_dd,
                    150);

    lv_obj_align(month_dd,
             LV_ALIGN_TOP_LEFT,
             120,
             50);

    lv_obj_set_width(month_dd,120);

    calendar_obj = lv_calendar_create(calendar_modal);
    lv_obj_clear_flag(calendar_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(calendar_obj, 260, 260);
    lv_obj_align(calendar_obj, LV_ALIGN_CENTER, 0, 75);
    memset(selected_dates, 0, sizeof(selected_dates));

    // build_year_list(current_year);
    build_year_list();

    year_dd =
    lv_dropdown_create(calendar_modal);

    lv_dropdown_set_options(year_dd,
                            year_list);

    lv_dropdown_set_selected(year_dd,
                            current_year - 1901);

    lv_obj_set_width(year_dd,
                    110);

    lv_obj_align(year_dd,
             LV_ALIGN_TOP_LEFT,
             20,
             50);

    lv_obj_set_width(year_dd,90);

    lv_obj_add_event_cb(month_dd,
                    dropdown_cb,
                    LV_EVENT_VALUE_CHANGED,
                    NULL);

    lv_obj_add_event_cb(year_dd,
                        dropdown_cb,
                        LV_EVENT_VALUE_CHANGED,
                        NULL);

    close_btn = lv_btn_create(calendar_modal);

    lv_obj_set_size(close_btn,35,35);

    lv_obj_align(close_btn,
                LV_ALIGN_TOP_RIGHT,
                -10,
                10);

    lv_obj_t *close_lbl =
    lv_label_create(close_btn);

    lv_label_set_text(close_lbl,
                    LV_SYMBOL_CLOSE);

    lv_obj_center(close_lbl);

    lv_obj_add_event_cb(close_btn,
                        close_calendar_cb,
                        LV_EVENT_CLICKED,
                        NULL);
    
    // Note: timeinfo.tm_year is years since 1900, tm_mon is 0-indexed
    // lv_calendar_set_today_date(calendar_obj, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    lv_calendar_set_today_date(calendar_obj,
                           current_year,
                           current_month,
                           current_day);
    lv_calendar_set_showed_date(calendar_obj,
                            current_year,
                            current_month);
    // lv_calendar_set_showed_date(calendar_obj, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1);

    // Highlight today
    static lv_calendar_date_t highlighted_days[1];
    highlighted_days[0].year = current_year;
    highlighted_days[0].month = current_month;
    highlighted_days[0].day = current_day;
    lv_calendar_set_highlighted_dates(calendar_obj, highlighted_days, 1);

    // Optional: Add a header to switch months (Requires lv_calendar header addons enabled in lv_conf.h)
    // lv_calendar_header_arrow_create(calendar);
    // lv_calendar_header_dropdown_create(calendar_obj);


    lv_obj_add_event_cb(calendar_obj, calendar_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    selecting_start_date = true; // Reset state machine
}

static void update_top_time(void)
{
    if (!top_time_lbl) return;

    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year >= (2020 - 1900))
    {
        strftime(last_time,
                 sizeof(last_time),
                 "%H:%M %a %d %b",
                 &timeinfo);
    }

    lv_label_set_text(top_time_lbl, last_time);
}

static void update_wifi_status(void)
{
    if (!top_wifi_lbl) return;

    if (wifi_is_connected)
    {
        lv_label_set_text(top_wifi_lbl,
                          LV_SYMBOL_WIFI " Connected");
        lv_obj_set_style_text_color(top_wifi_lbl,
                                    COLOR_EXCELLENT,
                                    0);
    }
    else
    {
        lv_label_set_text(top_wifi_lbl,
                          LV_SYMBOL_WIFI " Disconnected");
        lv_obj_set_style_text_color(top_wifi_lbl,
                                    COLOR_POOR,
                                    0);
    }
}

static void shift_history(int16_t *buf, int16_t value)
{
    memmove(&buf[0], &buf[1], (CHART_POINTS - 1) * sizeof(int16_t));
    buf[CHART_POINTS - 1] = value;
}

static void init_history_buffers(void)
{
    memset(pm25_history, 0, sizeof(pm25_history));
    memset(pm10_history, 0, sizeof(pm10_history));
    memset(tvoc_history, 0, sizeof(tvoc_history));
    memset(aqi_history, 0, sizeof(aqi_history));
    memset(temp_history, 0, sizeof(temp_history));
    memset(hum_history, 0, sizeof(hum_history));
    memset(weekly_aqi_history, 0, sizeof(weekly_aqi_history));
}

// Updated card creation function to extract the value label
static lv_obj_t * create_metric_card_linked(lv_obj_t * parent, const char * title, const char * val, const char * unit, lv_color_t color, lv_obj_t ** out_label) {
    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_style_bg_color(card, COLOR_CARD, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 12, 0);

    lv_obj_t * title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_12, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t * val_label = lv_label_create(card);
    lv_label_set_text(val_label, val);
    lv_obj_set_style_text_color(val_label, color, 0);
    lv_obj_set_style_text_font(val_label, &lv_font_montserrat_24, 0);
    // lv_obj_align(val_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_align(val_label, LV_ALIGN_TOP_LEFT, 0, 22);
    
    // Assign the created label to our global pointer
    if (out_label != NULL) {
        *out_label = val_label;
    }

    lv_obj_t * unit_label = lv_label_create(card);
    lv_label_set_text(unit_label, unit);
    lv_obj_set_style_text_color(unit_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(unit_label, &lv_font_montserrat_12, 0);
    // lv_obj_align_to(unit_label, val_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, -3);
    lv_obj_align_to(unit_label, val_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
    return card;
}


static void export_btn_event_cb(lv_event_t * e)
{
    export_popup_create();
}

static lv_obj_t *export_popup = NULL;

static lv_obj_t *export_qr = NULL;

static lv_obj_t *export_ip_label = NULL;

static lv_obj_t *export_range_label = NULL;

static lv_obj_t *download_title = NULL;

static lv_obj_t *email_modal = NULL;
static lv_obj_t *email_ta = NULL;
static lv_obj_t *email_kb = NULL;

// static httpd_handle_t server = NULL;
static lv_obj_t * cb_include_stats = NULL;
static lv_obj_t * cb_include_raw = NULL;

static bool merge_csv_files(int start_day,
                            int end_day);

static void open_email_input_modal(void);

static void email_kb_event_cb(lv_event_t *e);

// static esp_err_t download_get_handler(httpd_req_t *req);

// static httpd_handle_t start_webserver(void);

static void close_popup_cb(lv_event_t * e)
{
    LV_UNUSED(e);

    if(export_popup != NULL)
    {
        lv_obj_del(export_popup);
        export_popup = NULL;
    }
}

static void show_export_qr(const char *url,
                           int start_day,
                           int end_day)
{
    if (export_popup) {
        lv_obj_del(export_popup);
        export_popup = NULL;
    }

    export_popup = lv_obj_create(lv_scr_act());
    lv_obj_center(export_popup);
    lv_obj_set_size(export_popup, 430, 360);

    lv_obj_set_style_bg_color(export_popup, COLOR_CARD, 0);
    lv_obj_set_style_border_color(export_popup, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(export_popup, 14, 0);

    /* Title */

    download_title = lv_label_create(export_popup);
    lv_label_set_text(download_title, "Export Ready");
    lv_obj_set_style_text_font(download_title,
                               &lv_font_montserrat_18,
                               0);

    lv_obj_align(download_title,
                 LV_ALIGN_TOP_MID,
                 0,
                 12);

    /* Date Range */

    export_range_label = lv_label_create(export_popup);

    char range[64];

    snprintf(range,
             sizeof(range),
             "%08d  →  %08d",
             start_day,
             end_day);

    lv_label_set_text(export_range_label,
                      range);

    lv_obj_align(export_range_label,
                 LV_ALIGN_TOP_MID,
                 0,
                 45);

    /* QR Code */

    // export_qr = lv_qrcode_create(export_popup,
    //                              180,
    //                              lv_color_black(),
    //                              lv_color_white());

    // lv_qrcode_update(export_qr,
    //                  url,
    //                  strlen(url));

    // lv_obj_align(export_qr,
    //              LV_ALIGN_CENTER,
    //              0,
    //              10);

    /* URL */

    /* Instruction */

    lv_obj_t *scan_lbl = lv_label_create(export_popup);

    lv_label_set_text(scan_lbl,
                    "Scan with your phone\nto download the report");

    lv_obj_set_style_text_align(scan_lbl,
                                LV_TEXT_ALIGN_CENTER,
                                0);

    lv_obj_align(scan_lbl,
                LV_ALIGN_BOTTOM_MID,
                0,
                -60);

    /* IP Title */

    lv_obj_t *ip_title = lv_label_create(export_popup);

    lv_label_set_text(ip_title,
                    "Device IP");

    lv_obj_align(ip_title,
                LV_ALIGN_BOTTOM_LEFT,
                20,
                -35);

    /* IP Address */

    export_ip_label = lv_label_create(export_popup);

    const char *ip = strstr(url, "://");

    if(ip)
    {
        ip += 3;

        char ip_buf[32];

        int i = 0;

        while(*ip && *ip != '/' && i < sizeof(ip_buf)-1)
            ip_buf[i++] = *ip++;

        ip_buf[i] = '\0';

        lv_label_set_text(export_ip_label,
                        ip_buf);
    }

    lv_obj_align(export_ip_label,
                LV_ALIGN_BOTTOM_RIGHT,
                -20,
                -35);

    lv_obj_t *close_btn = lv_btn_create(export_popup);

    lv_obj_set_size(close_btn,120,42);

    lv_obj_align(close_btn,
                LV_ALIGN_BOTTOM_MID,
                0,
                -8);

    lv_obj_add_event_cb(close_btn,
                        close_popup_cb,
                        LV_EVENT_CLICKED,
                        NULL);

    lv_obj_t *lbl = lv_label_create(close_btn);

    lv_label_set_text(lbl,"Close");

    lv_obj_center(lbl);
}

static void export_email_cb(lv_event_t * e)
{
    lv_obj_t *dd = lv_event_get_user_data(e);

    char period[32];

    lv_dropdown_get_selected_str(dd,
                                 period,
                                 sizeof(period));

    ESP_LOGI(TAG,
             "Email selected : %s",
             period);

    /*----------------------------------------------------------
     * Get Current Date
     *---------------------------------------------------------*/

    /*----------------------------------------------------------
     * Get Current Date
     *---------------------------------------------------------*/
    int start_day = 0;
    int end_day = 0;

    if(strcmp(period, "Custom Range") == 0)
    {
        // Use the global custom dates directly
        start_day = (custom_start_date.year * 10000) + (custom_start_date.month * 100) + custom_start_date.day;
        end_day = (custom_end_date.year * 10000) + (custom_end_date.month * 100) + custom_end_date.day;
    }
    else
    {
        // Handle dynamic relative dates
        time_t now;
        time(&now);

        struct tm start_tm = *localtime(&now);
        struct tm end_tm   = *localtime(&now);

        if(strcmp(period, "Last 7 Days") == 0)
        {
            start_tm.tm_mday -= 6;
        }
        else if(strcmp(period, "Last Month") == 0)
        {
            start_tm.tm_mday -= 29;
        }

        /* Normalize dates (handles negative days/month rollovers automatically) */
        mktime(&start_tm);
        mktime(&end_tm);

        start_day = (start_tm.tm_year + 1900) * 10000 + (start_tm.tm_mon + 1) * 100 + start_tm.tm_mday;
        end_day = (end_tm.tm_year + 1900) * 10000 + (end_tm.tm_mon + 1) * 100 + end_tm.tm_mday;
    }

    ESP_LOGI(TAG, "Merge Range : %d -> %d", start_day, end_day);

    // time_t now;
    // time(&now);

    // struct tm start_tm = *localtime(&now);
    // struct tm end_tm   = *localtime(&now);

    // if(strcmp(period, "Last 7 Days") == 0)
    // {
    //     start_tm.tm_mday -= 6;
    // }
    // else if(strcmp(period, "Last Month") == 0)
    // {
    //     start_tm.tm_mday -= 29;
    // }
    // else if(strcmp(period, "Custom Range") == 0)
    // {
    //     start_day = (custom_start_date.year * 10000) + (custom_start_date.month * 100) + custom_start_date.day;
    //     end_day = (custom_end_date.year * 10000) + (custom_end_date.month * 100) + custom_end_date.day;
    // }

    // /* Normalize dates */
    // mktime(&start_tm);
    // mktime(&end_tm);

    // int start_day =
    //     (start_tm.tm_year + 1900) * 10000 +
    //     (start_tm.tm_mon + 1) * 100 +
    //     start_tm.tm_mday;

    // int end_day =
    //     (end_tm.tm_year + 1900) * 10000 +
    //     (end_tm.tm_mon + 1) * 100 +
    //     end_tm.tm_mday;

    // ESP_LOGI(TAG,
    //          "Merge Range : %d -> %d",
    //          start_day,
    //          end_day);

    /*----------------------------------------------------------
     * Merge CSV Files
     *---------------------------------------------------------*/

    if(merge_csv_files(start_day, end_day))
    {
        ESP_LOGI(TAG,
                 "CSV Merge Successful");

        // open_email_input_modal();
    }
    else
    {
        ESP_LOGE(TAG,
                 "CSV Merge Failed");
    }

    if(export_popup != NULL)
    {
        lv_obj_del(export_popup);
        export_popup = NULL;
    }
}

static void export_wifi_cb(lv_event_t * e)
{
    lv_obj_t *dd = lv_event_get_user_data(e);

    char period[32];

    lv_dropdown_get_selected_str(dd,
                                 period,
                                 sizeof(period));

    ESP_LOGI(TAG,
             "Wi-Fi selected : %s",
             period);

    /*----------------------------------------------------------
     * Get Current Date
     *---------------------------------------------------------*/

    /*----------------------------------------------------------
     * Get Current Date
     *---------------------------------------------------------*/
    int start_day = 0;
    int end_day = 0;

    if(strcmp(period, "Custom Range") == 0)
    {
        // Use the global custom dates directly
        start_day = (custom_start_date.year * 10000) + (custom_start_date.month * 100) + custom_start_date.day;
        end_day = (custom_end_date.year * 10000) + (custom_end_date.month * 100) + custom_end_date.day;
    }
    else
    {
        // Handle dynamic relative dates
        time_t now;
        time(&now);

        struct tm start_tm = *localtime(&now);
        struct tm end_tm   = *localtime(&now);

        if(strcmp(period, "Last 7 Days") == 0)
        {
            start_tm.tm_mday -= 6;
        }
        else if(strcmp(period, "Last Month") == 0)
        {
            start_tm.tm_mday -= 29;
        }

        /* Normalize dates (handles negative days/month rollovers automatically) */
        mktime(&start_tm);
        mktime(&end_tm);

        start_day = (start_tm.tm_year + 1900) * 10000 + (start_tm.tm_mon + 1) * 100 + start_tm.tm_mday;
        end_day = (end_tm.tm_year + 1900) * 10000 + (end_tm.tm_mon + 1) * 100 + end_tm.tm_mday;
    }

    ESP_LOGI(TAG, "Merge Range : %d -> %d", start_day, end_day);

    // time_t now;
    // time(&now);

    // struct tm start_tm = *localtime(&now);
    // struct tm end_tm   = *localtime(&now);

    // if(strcmp(period, "Last 7 Days") == 0)
    // {
    //     start_tm.tm_mday -= 6;
    // }
    // else if(strcmp(period, "Last Month") == 0)
    // {
    //     start_tm.tm_mday -= 29;
    // }
    // else if(strcmp(period, "Custom Range") == 0)
    // {
    //     start_day = (custom_start_date.year * 10000) + (custom_start_date.month * 100) + custom_start_date.day;
    //     end_day = (custom_end_date.year * 10000) + (custom_end_date.month * 100) + custom_end_date.day;
    // }

    // /* Normalize dates */
    // mktime(&start_tm);
    // mktime(&end_tm);

    // int start_day =
    //     (start_tm.tm_year + 1900) * 10000 +
    //     (start_tm.tm_mon + 1) * 100 +
    //     start_tm.tm_mday;

    // int end_day =
    //     (end_tm.tm_year + 1900) * 10000 +
    //     (end_tm.tm_mon + 1) * 100 +
    //     end_tm.tm_mday;

    // ESP_LOGI(TAG,
    //          "Merge Range : %d -> %d",
    //          start_day,
    //          end_day);
    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(5000)) == pdTRUE)
    {
        // DIR *dir = opendir(MOUNT_POINT);
        DIR *dir = opendir(MOUNT_POINT);

        if (dir)
        {
            struct dirent *entry;

            while ((entry = readdir(dir)) != NULL)
            {
                ESP_LOGI(TAG, "%s", entry->d_name);
            }

            closedir(dir);
        }
        xSemaphoreGive(sd_mutex);
    }

    uint8_t write_buf = 0x01;

    esp_err_t err = i2c_master_write_to_device(
        I2C_MASTER_NUM,
        0x24,
        &write_buf,
        1,
        pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "CH422G 0x24: %s", esp_err_to_name(err));

    write_buf = 0x0A;

    err = i2c_master_write_to_device(
        I2C_MASTER_NUM,
        0x38,
        &write_buf,
        1,
        pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "CH422G 0x38: %s", esp_err_to_name(err));
    // DIR *dir = opendir("/sdcard");
    DIR *dir = opendir(MOUNT_POINT);

    if (dir)
    {
        ESP_LOGI(TAG, "SD still accessible before merge");

        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL)
        {
            ESP_LOGI(TAG, "Found: %s", ent->d_name);
        }

        closedir(dir);
    }
    else
    {
        ESP_LOGE(TAG, "SD inaccessible before merge");
    }
    /*----------------------------------------------------------
     * Merge CSV Files
     *---------------------------------------------------------*/

    if(merge_csv_files(start_day, end_day))
    {
        ESP_LOGI(TAG,
                 "CSV Merge Successful");

        // server = start_webserver();

        // if(start_webserver() != NULL)
        // {
        //     // const char *url = get_download_url();

        // //     show_export_qr(url,
        // //                 start_day,
        // //                 end_day);
        // }
        // else
        // {
        //     ESP_LOGE(TAG, "Failed to start HTTP server");
        // }
        ESP_LOGI(TAG, "HTTP server already running");
    }
    else
    {
        ESP_LOGE(TAG,
                 "CSV Merge Failed");
    }

    // if(export_popup != NULL)
    // {
    //     lv_obj_del(export_popup);
    //     export_popup = NULL;
    // }
}

static void export_dropdown_cb(lv_event_t * e)
    {
        lv_obj_t * dropdown = lv_event_get_target(e);
        char buf[32];
        lv_dropdown_get_selected_str(dropdown, buf, sizeof(buf));

        if(strcmp(buf, "Custom Range") == 0)
        {
            create_calendar_modal();
        }
    }


static void export_popup_create(void)
{
    if(export_popup) return;

    // Outer Popup Container
    export_popup = lv_obj_create(lv_scr_act());
    lv_obj_add_flag(export_popup, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(export_popup, 360, 320);
    lv_obj_center(export_popup);

    // Styling
    lv_obj_set_style_bg_color(export_popup, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(export_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(export_popup, COLOR_LINE, 0);
    lv_obj_set_style_border_width(export_popup, 1, 0);
    lv_obj_set_style_radius(export_popup, 16, 0);
    lv_obj_set_style_shadow_width(export_popup, 20, 0);
    lv_obj_set_style_shadow_opa(export_popup, LV_OPA_30, 0);

    // Close Button (Top Right 'X')
    lv_obj_t * close_btn = lv_btn_create(export_popup);
    lv_obj_set_size(close_btn, 28, 28);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_bg_opa(close_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, close_popup_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * x_lbl = lv_label_create(close_btn);
    lv_label_set_text(x_lbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(x_lbl, COLOR_DIM, 0);
    lv_obj_center(x_lbl);

    // Modal Title
    lv_obj_t * title = lv_label_create(export_popup);
    lv_label_set_text(title, "Export Report");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 15, 10);

    // Period Header
    lv_obj_t * period_lbl = lv_label_create(export_popup);
    lv_label_set_text(period_lbl, "Export Period");
    lv_obj_set_style_text_color(period_lbl, COLOR_DIM, 0);
    lv_obj_set_style_text_font(period_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(period_lbl, LV_ALIGN_TOP_LEFT, 15, 42);

    // Dropdown Range Selection
    lv_obj_t * dd = lv_dropdown_create(export_popup);
    lv_dropdown_set_options(dd, "Today\nLast 7 Days\nLast Month\nCustom Range");
    lv_obj_set_width(dd, 300);
    lv_obj_align(dd, LV_ALIGN_TOP_LEFT, 15, 62);
    lv_obj_set_style_bg_color(dd, COLOR_BG, 0);
    lv_obj_set_style_border_color(dd, COLOR_LINE, 0);
    lv_obj_set_style_text_color(dd, COLOR_TEXT, 0);

    lv_obj_add_event_cb(dd, export_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Checkbox 1: Include Statistics
    cb_include_stats = lv_checkbox_create(export_popup);
    lv_checkbox_set_text(cb_include_stats, "Include Statistics");
    lv_obj_add_state(cb_include_stats, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(cb_include_stats, COLOR_TEXT, 0);
    lv_obj_align(cb_include_stats, LV_ALIGN_TOP_LEFT, 15, 125);

    // Checkbox 2: Include Raw Sensor Data
    cb_include_raw = lv_checkbox_create(export_popup);
    lv_checkbox_set_text(cb_include_raw, "Include Raw Sensor Data");
    lv_obj_add_state(cb_include_raw, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(cb_include_raw, COLOR_TEXT, 0);
    lv_obj_align(cb_include_raw, LV_ALIGN_TOP_LEFT, 15, 160);

    // Action Button 1: Email
    lv_obj_t * email_btn = lv_btn_create(export_popup);
    lv_obj_set_size(email_btn, 140, 40);
    lv_obj_align(email_btn, LV_ALIGN_BOTTOM_LEFT, 15, -15);
    lv_obj_set_style_bg_color(email_btn, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(email_btn, 10, 0);
    lv_obj_add_event_cb(email_btn, export_email_cb, LV_EVENT_CLICKED, dd);

    lv_obj_t * l_email = lv_label_create(email_btn);
    lv_label_set_text(l_email, " Email");
    lv_obj_center(l_email);

    // Action Button 2: Wi-Fi
    lv_obj_t * wifi_btn = lv_btn_create(export_popup);
    lv_obj_set_size(wifi_btn, 140, 40);
    lv_obj_align(wifi_btn, LV_ALIGN_BOTTOM_RIGHT, -15, -15);
    lv_obj_set_style_bg_color(wifi_btn, COLOR_CARD, 0);
    lv_obj_set_style_border_color(wifi_btn, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(wifi_btn, 1, 0);
    lv_obj_set_style_radius(wifi_btn, 10, 0);
    lv_obj_add_event_cb(wifi_btn, export_wifi_cb, LV_EVENT_CLICKED, dd);

    lv_obj_t * l_wifi = lv_label_create(wifi_btn);
    lv_label_set_text(l_wifi, LV_SYMBOL_WIFI " Wi-Fi");
    lv_obj_set_style_text_color(l_wifi, COLOR_TEXT, 0);
    lv_obj_center(l_wifi);
    // if(export_popup != NULL)
    //     return;

    // export_popup = lv_obj_create(lv_scr_act());

    // lv_obj_set_size(export_popup, 360, 270);

    // lv_obj_center(export_popup);

    // lv_obj_set_style_radius(export_popup, 12, 0);

    // lv_obj_set_style_pad_all(export_popup, 15, 0);

    // /*--------------------*/
    // /* Title */
    // /*--------------------*/

    // lv_obj_t *title = lv_label_create(export_popup);

    // lv_label_set_text(title,
    //                   "Export Report");

    // lv_obj_set_style_text_font(title,
    //                            &lv_font_montserrat_20,
    //                            0);

    // lv_obj_align(title,
    //              LV_ALIGN_TOP_MID,
    //              0,
    //              5);

    // /*--------------------*/
    // /* Report Period Label */
    // /*--------------------*/

    // lv_obj_t *period_lbl = lv_label_create(export_popup);

    // lv_label_set_text(period_lbl,
    //                   "Report Period");

    // lv_obj_align(period_lbl,
    //              LV_ALIGN_TOP_LEFT,
    //              20,
    //              55);

    // /*--------------------*/
    // /* Dropdown */
    // /*--------------------*/

    // lv_obj_t *dd = lv_dropdown_create(export_popup);

    // lv_dropdown_set_options(
    //         dd,
    //         "Today\n"
    //         "Yesterday\n"
    //         "Last 7 Days\n"
    //         "Last 30 Days");

    // lv_obj_set_width(dd, 220);

    // lv_obj_align(dd,
    //              LV_ALIGN_TOP_LEFT,
    //              20,
    //              80);

    // /*--------------------*/
    // /* Email Button */
    // /*--------------------*/

    // lv_obj_t *email_btn = lv_btn_create(export_popup);

    // lv_obj_set_size(email_btn,
    //                 130,
    //                 45);

    // lv_obj_align(email_btn,
    //              LV_ALIGN_BOTTOM_LEFT,
    //              20,
    //              -20);

    // lv_obj_add_event_cb(email_btn,
    //                     export_email_cb,
    //                     LV_EVENT_CLICKED,
    //                     dd);

    // lv_obj_t *email_lbl = lv_label_create(email_btn);

    // lv_label_set_text(email_lbl,
    //                   LV_SYMBOL_UPLOAD
    //                   " Email");

    // lv_obj_center(email_lbl);

    // /*--------------------*/
    // /* WiFi Button */
    // /*--------------------*/

    // lv_obj_t *wifi_btn = lv_btn_create(export_popup);

    // lv_obj_set_size(wifi_btn,
    //                 130,
    //                 45);

    // lv_obj_align(wifi_btn,
    //              LV_ALIGN_BOTTOM_RIGHT,
    //              -20,
    //              -20);

    // lv_obj_add_event_cb(wifi_btn,
    //                     export_wifi_cb,
    //                     LV_EVENT_CLICKED,
    //                     dd);

    // lv_obj_t *wifi_lbl = lv_label_create(wifi_btn);

    // lv_label_set_text(wifi_lbl,
    //                   LV_SYMBOL_WIFI
    //                   " Wi-Fi");

    // lv_obj_center(wifi_lbl);

    // /*--------------------*/
    // /* Close Button */
    // /*--------------------*/

    // lv_obj_t *close_btn = lv_btn_create(export_popup);

    // lv_obj_set_size(close_btn,
    //                 32,
    //                 32);

    // lv_obj_align(close_btn,
    //              LV_ALIGN_TOP_RIGHT,
    //              -5,
    //              5);

    // lv_obj_add_event_cb(close_btn,
    //                     close_popup_cb,
    //                     LV_EVENT_CLICKED,
    //                     NULL);

    // lv_obj_t *close_lbl = lv_label_create(close_btn);

    // lv_label_set_text(close_lbl,
    //                   LV_SYMBOL_CLOSE);

    // lv_obj_center(close_lbl);

}

static void show_export_qr(const char *url,
                           int start_day,
                           int end_day);

// --- Mock Data & Dynamic Update Timer ---
static void main_data_timer_cb(lv_timer_t * timer) {
    /*COMMENTED OUT DUE TO DOUBLE OCCURENCE*/
    // if (top_time_lbl != NULL) {
    //     time_t now;
    //     struct tm timeinfo;
    //     time(&now);
    //     localtime_r(&now, &timeinfo);

    //     // Check if the time has been set via NTP yet (year will be > 1970)
    //     if (timeinfo.tm_year < (2020 - 1900)) {
    //         lv_label_set_text(top_time_lbl, "Syncing Time...");
    //     } else {
    //         char time_str[64];
    //         // Formats to: "17:53 · Mon 20 Jul"
    //         strftime(time_str, sizeof(time_str), "%H:%M %a %d %b", &timeinfo);
    //         lv_label_set_text(top_time_lbl, time_str);
    //     }
    // }

    // // --- Wi-Fi Status Update ---
    // if (top_wifi_lbl != NULL) {
    //     // ALWAYS use the real Wi-Fi connection status from main.c
    //     if (wifi_is_connected) {
    //         lv_label_set_text(top_wifi_lbl, LV_SYMBOL_WIFI " Connected");
    //         lv_obj_set_style_text_color(top_wifi_lbl, COLOR_EXCELLENT, 0);
    //     } else {
    //         lv_label_set_text(top_wifi_lbl, LV_SYMBOL_WIFI " Disconnected");
    //         lv_obj_set_style_text_color(top_wifi_lbl, COLOR_POOR, 0);
    //     }
    // }
    update_top_time();
    update_wifi_status();

    sensor_data_t ui_data;

    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(5)))
    {
        ui_data = real_data;

        xSemaphoreGive(sensor_mutex);
    }
    else
    {
        return;
    }
    
    shift_history(pm25_history, (int16_t)ui_data.pm25);
    shift_history(pm10_history, (int16_t)ui_data.pm10);
    shift_history(aqi_history, (int16_t)ui_data.aqi);
    shift_history(temp_history, (int16_t)(ui_data.temp * 10));
    shift_history(hum_history, (int16_t)(ui_data.humidity * 10));
    shift_history(tvoc_history, (int16_t)ui_data.tvoc);

    if(chart1 && ser_pm25 && ser_pm10)
    {
        for(int i = 0; i < CHART_POINTS; i++)
        {
            ser_pm25->y_points[i] = pm25_history[i];
        }

        for(int i = 0; i < CHART_POINTS; i++)
        {
            ser_pm10->y_points[i] = pm10_history[i];
        }

        lv_chart_refresh(chart1);
    }

    if(chart2 && ser_aqi)
    {
        for(int i = 0; i < CHART_POINTS; i++)
        {
            ser_aqi->y_points[i] = aqi_history[i];
        }

        lv_chart_refresh(chart2);
    }

    if(chart3 && ser_c3_pm25 && ser_c3_pm10 && ser_c3_tvoc)
    {
        ser_c3_pm25->y_points[0] = 0;
        ser_c3_pm25->y_points[1] = (lv_coord_t)ui_data.pm25;

        ser_c3_pm10->y_points[0] = 0;
        ser_c3_pm10->y_points[1] = (lv_coord_t)ui_data.pm10;

        ser_c3_tvoc->y_points[0] = 0;
        ser_c3_tvoc->y_points[1] = (lv_coord_t)ui_data.tvoc;

        lv_chart_refresh(chart3);
    }

    if(chart6 && ser_c6_eff)
    {
        for(int i = 0; i < CHART_POINTS; i++)
        {
            ser_c6_eff->y_points[i] = temp_history[i] / 10;
        }

        lv_chart_refresh(chart6);
    }

    if(chart7 && ser_c7_comf)
    {
        for(int i = 0; i < CHART_POINTS; i++)
        {
            ser_c7_comf->y_points[i] = hum_history[i] / 10;
        }

        lv_chart_refresh(chart7);
    }

    if(chart8 && ser_c8_week)
    {
        for(int i = 0; i < 7; i++)
        {
            ser_c8_week->y_points[i] = weekly_aqi_history[i];
        }

        lv_chart_refresh(chart8);
    }

    if(chart4 && ser_c4_comp)
    {
        int compliant = 0;
        int non_compliant = 0;

        for(int i = 0; i < CHART_POINTS; i++)
        {
            if(pm25_history[i] <= 60)
                compliant++;
            else
                non_compliant++;
        }

        ser_c4_comp->y_points[0] = compliant;
        ser_c4_comp->y_points[1] = non_compliant;

        lv_chart_refresh(chart4);
    }

    if(chart5 && ser_c5_dist)
    {
        int good = 0;
        int satisfactory = 0;
        int moderate = 0;
        int poor = 0;
        int very_poor = 0;
        int severe = 0;

        for(int i = 0; i < CHART_POINTS; i++)
        {
            int aqi = aqi_history[i];

            if(aqi <= 50)
                good++;
            else if(aqi <= 100)
                satisfactory++;
            else if(aqi <= 200)
                moderate++;
            else if(aqi <= 300)
                poor++;
            else if(aqi <= 400)
                very_poor++;
            else
                severe++;
        }

        ser_c5_dist->y_points[0] = good;
        ser_c5_dist->y_points[1] = satisfactory;
        ser_c5_dist->y_points[2] = moderate;
        ser_c5_dist->y_points[3] = poor;
        ser_c5_dist->y_points[4] = very_poor;
        ser_c5_dist->y_points[5] = severe;

        lv_chart_refresh(chart5);
    }


    // --- Purifier Logic ---
    if (purifier_filter_lbl != NULL && purifier_status_lbl != NULL && purifier_fan_lbl != NULL) {
        char buf[32];
        
        int p_fan = use_mock_data ? (65 + (int)(lv_rand(0, 10) - 5)) : ui_data.fan_speed; 
        bool p_state = true; // Switch state
        
        uint32_t elapsed_ms = lv_tick_get() - filter_start_time;
        int p_filter = use_mock_data ? (100 - (int)((elapsed_ms * 100) / 10000)) : ui_data.filter_life; 
        if (p_filter < 0) p_filter = 0;

        snprintf(buf, sizeof(buf), "Fan %d%%", p_fan);
        lv_label_set_text(purifier_fan_lbl, buf);

        if (p_filter == 0) {
            lv_label_set_text(purifier_status_lbl, "REPLACE");
            lv_obj_set_style_text_color(purifier_status_lbl, COLOR_POOR, 0);
            lv_label_set_text(purifier_filter_lbl, "Filter 0%");
            lv_obj_set_style_text_color(purifier_filter_lbl, COLOR_POOR, 0);
        } else if (p_filter <= 20) {
            lv_label_set_text(purifier_status_lbl, "CHANGE");
            lv_obj_set_style_text_color(purifier_status_lbl, COLOR_OPRUSS_ORG, 0); 
            snprintf(buf, sizeof(buf), "Filter %d%%", p_filter);
            lv_label_set_text(purifier_filter_lbl, buf);
            lv_obj_set_style_text_color(purifier_filter_lbl, COLOR_OPRUSS_ORG, 0);
        } else {
            lv_label_set_text(purifier_status_lbl, p_state ? "ON" : "OFF");
            lv_obj_set_style_text_color(purifier_status_lbl, p_state ? COLOR_EXCELLENT : COLOR_TEXT, 0);
            snprintf(buf, sizeof(buf), "Filter %d%%", p_filter);
            lv_label_set_text(purifier_filter_lbl, buf);
            lv_obj_set_style_text_color(purifier_filter_lbl, COLOR_TEXT, 0);
        }
    }

    // // --- AQI AND METRICS DATA INJECTION ---
    // int current_aqi = use_mock_data ? (10 + (int)lv_rand(0, 140)) : real_data.aqi;
    // int current_pm25 = use_mock_data ? (int)(8 + lv_rand(0, 15)) : real_data.pm25;
    // int current_pm10 = use_mock_data ? (int)(15 + lv_rand(0, 20)) : real_data.pm10;
    // int current_tvoc = use_mock_data ? (int)(100 + lv_rand(0, 45)) : real_data.tvoc;
    // float current_temp = use_mock_data ? (24.0f + ((float)lv_rand(0, 29) / 10.0f)) : real_data.temp;
    // int current_hum = use_mock_data ? (int)(45 + lv_rand(0, 8)) : real_data.humidity;
    /*above code commented out because now we are dealing with real data from esp-now*/

    int current_aqi  = ui_data.aqi;
    int current_pm25 = (int)ui_data.pm25;
    int current_tvoc = (int)ui_data.tvoc;
    float current_temp = ui_data.temp;
    int current_hum = (int)ui_data.humidity;

    ESP_LOGI(TAG,
         "AQI=%d PM25=%.1f PM10=%.2f TVOC=%.1f TEMP=%.1f HUM=%.1f",
         ui_data.aqi,
         (double)ui_data.pm25,
         (double)ui_data.pm10,
         (double)ui_data.tvoc,
         (double)ui_data.temp,
         (double)ui_data.humidity);

    if (aqi_val_lbl != NULL && aqi_arc != NULL && aqi_cat_lbl != NULL) {
        lv_arc_set_value(aqi_arc, current_aqi);
        lv_label_set_text_fmt(aqi_val_lbl, "%d", current_aqi);

        lv_color_t target_color;
        const char * cat_text;
        if (current_aqi <= 50) {
            target_color = COLOR_EXCELLENT; cat_text = "Good";
        } else if (current_aqi <= 100) {
            target_color = COLOR_GOOD; cat_text = "Moderate";
        } else {
            target_color = COLOR_POOR; cat_text = "Poor";
        }

        lv_obj_set_style_arc_color(aqi_arc, target_color, LV_PART_INDICATOR);
        lv_obj_set_style_text_color(aqi_val_lbl, target_color, 0);
        lv_label_set_text(aqi_cat_lbl, cat_text);
        lv_obj_set_style_text_color(aqi_cat_lbl, target_color, 0);
    }

    if (pm25_val_lbl != NULL) lv_label_set_text_fmt(pm25_val_lbl, "%d", current_pm25);
    // if (pm10_val_lbl != NULL) lv_label_set_text_fmt(pm10_val_lbl, "%d", current_pm10);
    if (pm10_val_lbl)
    {
        char buf[16];

        if(ui_data.pm10 < 0.0f)
        {
            strcpy(buf, "0");
        }
        else
        {
            snprintf(buf,
                    sizeof(buf),
                    "%.0f",
                    (double)ui_data.pm10);
        }

        lv_label_set_text(pm10_val_lbl, buf);
    }
    if (tvoc_val_lbl != NULL) lv_label_set_text_fmt(tvoc_val_lbl, "%d", current_tvoc);
    
    // SAFE FLOAT FORMATTING (Splits float into two integers)
    if (temp_val_lbl != NULL) {
        int t_int = (int)current_temp;
        int t_dec = (int)((current_temp - t_int) * 10);
        if (t_dec < 0) t_dec = -t_dec; // Handle negative temperatures safely
        lv_label_set_text_fmt(temp_val_lbl, "%d.%d", t_int, t_dec);
    }
    
    if (hum_val_lbl != NULL) lv_label_set_text_fmt(hum_val_lbl, "%d", current_hum);
}

// --- Navigation Callback ---
static void nav_event_cb(lv_event_t * e) {
    int target_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (target_idx == current_screen_idx) return; 

    // Reset pointers to prevent timer crashing when screen changes
    aqi_arc = NULL; aqi_val_lbl = NULL; aqi_cat_lbl = NULL;
    pm25_val_lbl = NULL; pm10_val_lbl = NULL; tvoc_val_lbl = NULL;
    temp_val_lbl = NULL; hum_val_lbl = NULL;
    purifier_fan_lbl = NULL; purifier_filter_lbl = NULL; purifier_status_lbl = NULL;
    top_time_lbl = NULL; top_wifi_lbl = NULL;

    chart1 = NULL;
    chart2 = NULL;
    chart3 = NULL;
    chart4 = NULL;
    chart5 = NULL;
    chart6 = NULL;
    chart7 = NULL;
    chart8 = NULL;

    ser_pm25 = NULL;
    ser_pm10 = NULL;
    ser_aqi = NULL;

    ser_c3_pm25 = NULL;
    ser_c3_pm10 = NULL;
    ser_c3_tvoc = NULL;

    ser_c4_comp = NULL;
    ser_c5_dist = NULL;
    ser_c6_eff = NULL;
    ser_c7_comf = NULL;
    ser_c8_week = NULL;

    lv_obj_t * new_scr = NULL;
    switch (target_idx) {
        case 1: new_scr = build_screen_1(); break;
        case 2: new_scr = build_screen_2(); break;
        case 3: new_scr = build_screen_3(); break;
        case 4: new_scr = build_screen_4(); break;
        case 5: new_scr = build_screen_5(); break;
        case 6: new_scr = build_screen_6(); break;
    }

    if (new_scr != NULL) {
        lv_scr_load_anim(new_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
        current_screen_idx = target_idx;
    }
}

// --- Master Layout Generator ---
static lv_obj_t* create_base_layout(const char* top_title, int active_nav_idx, lv_obj_t** out_content) {
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * main_cont = lv_obj_create(scr);
    lv_obj_set_size(main_cont, 800, 480);
    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(main_cont, 0, 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_bg_opa(main_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_layout(main_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t * top_bar = lv_obj_create(main_cont);
    lv_obj_set_size(top_bar, 800, 42);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_border_side(top_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(top_bar, 1, 0);
    lv_obj_set_style_border_color(top_bar, COLOR_LINE, 0);
    lv_obj_set_style_pad_hor(top_bar, 22, 0);

    /* Left : Time */
    // top_time_lbl = lv_label_create(top_bar);
    // update_top_time();
    // lv_label_set_text(top_time_lbl, top_title);
    top_time_lbl = lv_label_create(top_bar);

    if (top_title)
        lv_label_set_text(top_time_lbl, top_title);
    else
        update_top_time();
    lv_obj_set_style_text_color(top_time_lbl, COLOR_DIM, 0);
    lv_obj_align(top_time_lbl, LV_ALIGN_LEFT_MID, 18, 0);

    /* Center : OPRUSS */
    lv_obj_t * opruss_logo_img = lv_img_create(top_bar);
    lv_img_set_src(opruss_logo_img, &OPRUSS_logo_black_bg); // Updated variable
    lv_obj_align(opruss_logo_img, LV_ALIGN_CENTER, 0, 0);

    /* Right : WiFi */
    
    top_wifi_lbl = lv_label_create(top_bar);
    update_wifi_status();
    // lv_label_set_text(top_wifi_lbl, LV_SYMBOL_WIFI " Syncing...");
    lv_obj_set_style_text_color(top_wifi_lbl, COLOR_DIM, 0);
    lv_obj_align(top_wifi_lbl, LV_ALIGN_RIGHT_MID, -18, 0);

    lv_obj_t * body_cont = lv_obj_create(main_cont);
    lv_obj_set_size(body_cont, 800, 438);
    lv_obj_clear_flag(body_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(body_cont, 0, 0);
    lv_obj_set_style_border_width(body_cont, 0, 0);
    lv_obj_set_style_bg_opa(body_cont, LV_OPA_TRANSP, 0);
    // (Using absolute layout coordinates instead of flex for precise floating spacing)

    // --- Floating Rounded Sidebar Rail ---
    lv_obj_t * rail = lv_obj_create(body_cont);
    lv_obj_set_size(rail, 64, 414);                 // Height leaves room for top/bottom gaps
    lv_obj_set_pos(rail, 10, 12);                   // X: 10px left offset, Y: 12px top offset
    lv_obj_clear_flag(rail, LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_set_style_bg_color(rail, lv_color_hex(0x0D1118), 0);
    lv_obj_set_style_bg_color(rail, COLOR_CARD, 0);
    lv_obj_set_style_border_width(rail, 1, 0);
    lv_obj_set_style_border_color(rail, COLOR_LINE, 0);
    lv_obj_set_style_radius(rail, 18, 0);           // Smooth curvy top and bottom edges!
    lv_obj_set_style_pad_top(rail, 20, 0); 
    lv_obj_set_style_pad_row(rail, 24, 0); 
    lv_obj_set_layout(rail, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(rail, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rail, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char* icons[] = {LV_SYMBOL_HOME, LV_SYMBOL_EYE_OPEN, LV_SYMBOL_LIST, LV_SYMBOL_MUTE, LV_SYMBOL_FILE, LV_SYMBOL_SETTINGS}; /*added some extra icons i.e. charts and mic -refer lv_symbol_def.h */
    /*CAN BE REPLACE LV_SYMBOL_IMAGE AND LV_SYMBOL_AUDIO with these LV_SYMBOL_CHART and LV_SYMBOL_MIC*/

    for(int i = 0; i < 6; i++) {
        lv_obj_t * btn = lv_btn_create(rail);
        lv_obj_set_size(btn, 38, 38);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_set_style_bg_color(btn, (i + 1 == active_nav_idx) ? COLOR_ACCENT : COLOR_BG, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        
        lv_obj_t * icon_lbl = lv_label_create(btn);
        lv_label_set_text(icon_lbl, icons[i]);
        lv_obj_center(icon_lbl);

        lv_obj_add_event_cb(btn, nav_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(i + 1));
    }

    lv_obj_t * content = lv_obj_create(body_cont);
    lv_obj_set_size(content, 800 - 84, 438);        // Adjusted width to clear the floating rail
    lv_obj_set_pos(content, 84, 0);                 // Positioned right after the rail container
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_set_style_pad_all(content, 18, 0);
    lv_obj_set_style_pad_left(content, 45, 0);
    lv_obj_set_style_pad_right(content, 18, 0);
    lv_obj_set_style_pad_top(content, 18, 0);
    lv_obj_set_style_pad_bottom(content, 18, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    
    *out_content = content; 
    return scr;
}

// --- Helper: Metric Card ---
// static lv_obj_t* create_metric_card(lv_obj_t* parent, const char* title_text, const char* val_text, const char* unit_text, lv_color_t val_color, lv_obj_t** out_val) {
//     lv_obj_t* card = lv_obj_create(parent);
//     lv_obj_set_size(card, lv_pct(100), lv_pct(100)); 
//     lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE); 
//     lv_obj_set_style_bg_color(card, COLOR_CARD, 0);
//     lv_obj_set_style_radius(card, 18, 0);
//     lv_obj_set_style_border_width(card, 0, 0);
//     lv_obj_set_style_pad_all(card, 14, 0);
//     lv_obj_set_layout(card, LV_LAYOUT_FLEX);
//     lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

//     lv_obj_t* title = lv_label_create(card);
//     lv_label_set_text(title, title_text);
//     lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
//     lv_obj_set_style_text_color(title, COLOR_DIM, 0);

//     lv_obj_t* val = lv_label_create(card);
//     lv_label_set_text(val, val_text);
//     lv_obj_set_style_text_color(val, val_color, 0);
//     lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);

//     if (out_val != NULL) {
//         *out_val = val; 
//     }

//     lv_obj_t* unit = lv_label_create(card);
//     lv_label_set_text(unit, unit_text);
//     lv_obj_set_style_text_color(unit, COLOR_DIM, 0);

//     return card;
// }
static lv_obj_t* create_metric_card(lv_obj_t* parent,
                                    const char* title_text,
                                    const char* val_text,
                                    const char* unit_text,
                                    lv_color_t val_color,
                                    lv_obj_t** out_val)
{
    lv_obj_t* card = lv_obj_create(parent);

    lv_obj_set_size(card, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(card, COLOR_CARD, 0);
    lv_obj_set_style_radius(card, 18, 0);
    lv_obj_set_style_border_width(card, 0, 0);

    /* Smaller padding for tighter layout */
    lv_obj_set_style_pad_left(card, 14, 0);
    lv_obj_set_style_pad_right(card, 14, 0);
    lv_obj_set_style_pad_top(card, 12, 0);
    lv_obj_set_style_pad_bottom(card, 12, 0);

    /* Don't let flex spread the widgets */
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    /* Only 2px between title and value */
    lv_obj_set_style_pad_row(card, 2, 0);

    /* Title */
    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, COLOR_DIM, 0);

    /* Value */
    lv_obj_t* val = lv_label_create(card);
    lv_label_set_text(val, val_text);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(val, val_color, 0);

    if(out_val)
        *out_val = val;

    /* Unit */
    lv_obj_t* unit = lv_label_create(card);
    lv_label_set_text(unit, unit_text);
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(unit, COLOR_DIM, 0);

    return card;
}
// ==========================================
// SCREEN 0: SPLASH SCREEN (BRAND GUIDELINES)
// ==========================================
static void splash_end_cb(lv_timer_t * timer) {
    init_history_buffers();
    ui_init_main();
    lv_timer_del(timer);
}

static lv_obj_t* build_splash_screen(void) {
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

    lv_obj_t * logo = lv_img_create(scr);
    lv_img_set_src(logo, &opruss_boot_logo);
    lv_obj_center(logo);

    lv_timer_create(splash_end_cb, 5000, NULL);
    return scr;
}

// ==========================================
// SCREEN 1: HOME DASHBOARD
// ==========================================
static lv_obj_t* build_screen_1(void) {
    lv_obj_t * content;
    lv_obj_t * scr = create_base_layout(NULL, 1, &content);


    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    
    // Centers the gauge and metrics grid vertically and spaces them nicely horizontally
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Gauge Area
    lv_obj_t * gauge_cont = lv_obj_create(content);
    lv_obj_set_size(gauge_cont, 300, 300);
    lv_obj_clear_flag(gauge_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(gauge_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gauge_cont, 0, 0);
    
    aqi_arc = lv_arc_create(gauge_cont);
    lv_obj_set_size(aqi_arc, 280, 280);
    lv_arc_set_rotation(aqi_arc, 135);
    lv_arc_set_bg_angles(aqi_arc, 0, 270);
    lv_arc_set_range(aqi_arc, 0, 300); 
    // lv_arc_set_value(aqi_arc, 42); 
    lv_arc_set_value(aqi_arc, 0);
    lv_obj_set_style_arc_color(aqi_arc, COLOR_LINE, LV_PART_MAIN);
    lv_obj_set_style_arc_width(aqi_arc, 18, LV_PART_MAIN);
    lv_obj_set_style_arc_color(aqi_arc, COLOR_EXCELLENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(aqi_arc, 18, LV_PART_INDICATOR);
    lv_obj_remove_style(aqi_arc, NULL, LV_PART_KNOB); 
    lv_obj_center(aqi_arc);

    lv_obj_t * aqi_lbl = lv_label_create(gauge_cont);
    lv_label_set_text(aqi_lbl, "AQI");
    lv_obj_set_style_text_color(aqi_lbl, COLOR_DIM, 0);
    lv_obj_align(aqi_lbl, LV_ALIGN_CENTER, 0, -40);

    aqi_val_lbl = lv_label_create(gauge_cont);
    // lv_label_set_text(aqi_val_lbl, "42");
    lv_label_set_text_fmt(aqi_val_lbl, "%d", real_data.aqi);
    lv_obj_set_style_text_font(aqi_val_lbl, &lv_font_montserrat_24, 0); 
    lv_obj_set_style_text_color(aqi_val_lbl, COLOR_EXCELLENT, 0);
    lv_obj_align(aqi_val_lbl, LV_ALIGN_CENTER, 0, 0);

    aqi_cat_lbl = lv_label_create(gauge_cont);
    lv_label_set_text(aqi_cat_lbl, "--");
    lv_obj_set_style_text_color(aqi_cat_lbl, COLOR_EXCELLENT, 0);
    lv_obj_align(aqi_cat_lbl, LV_ALIGN_CENTER, 0, 40);

    // Metrics Grid
    lv_obj_t * metrics_cont = lv_obj_create(content);
    lv_obj_set_size(metrics_cont, 400, 300); 
    lv_obj_clear_flag(metrics_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(metrics_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(metrics_cont, 0, 0);
    
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(metrics_cont, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(metrics_cont, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(metrics_cont, row_dsc, 0);
    lv_obj_set_style_pad_row(metrics_cont, 14, 0); 
    lv_obj_set_style_pad_column(metrics_cont, 14, 0); 

    lv_obj_t * c1 = create_metric_card(metrics_cont, "PM2.5", "--", "ug/m3", COLOR_TEXT, &pm25_val_lbl);
    lv_obj_set_grid_cell(c1, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_t * c2 = create_metric_card(metrics_cont, "PM10", "--", "ug/m3", COLOR_TEXT, &pm10_val_lbl);
    lv_obj_set_grid_cell(c2, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_t * c3 = create_metric_card(metrics_cont, "TVOC", "--", "ppb", COLOR_TEXT, &tvoc_val_lbl);
    lv_obj_set_grid_cell(c3, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_t * c4 = create_metric_card(metrics_cont, "TEMP", "--", "C", COLOR_TEXT, &temp_val_lbl);
    lv_obj_set_grid_cell(c4, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_t * c5 = create_metric_card(metrics_cont, "HUMIDITY", "--", "%RH", COLOR_TEXT, &hum_val_lbl);
    lv_obj_set_grid_cell(c5, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    
    // Custom Purifier Card
    lv_obj_t* c6 = lv_obj_create(metrics_cont);
    lv_obj_set_style_bg_color(c6, COLOR_CARD, 0);
    lv_obj_set_style_radius(c6, 18, 0);
    lv_obj_set_style_border_width(c6, 0, 0);
    lv_obj_set_style_pad_all(c6, 10, 0); 
    lv_obj_clear_flag(c6, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_grid_cell(c6, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

    lv_obj_t* p_title = lv_label_create(c6);
    lv_label_set_text(p_title, "PURIFIER");
    lv_obj_set_style_text_color(p_title, COLOR_DIM, 0);
    lv_obj_set_style_text_font(p_title, &lv_font_montserrat_14, 0);
    lv_obj_align(p_title, LV_ALIGN_TOP_LEFT, 0, 0);

    purifier_status_lbl = lv_label_create(c6);
    lv_label_set_text(purifier_status_lbl, "ON");
    lv_obj_set_style_text_color(purifier_status_lbl, COLOR_EXCELLENT, 0);
    lv_obj_set_style_text_font(purifier_status_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(purifier_status_lbl, LV_ALIGN_CENTER, 0, -12); 

    purifier_filter_lbl = lv_label_create(c6);
    lv_label_set_text(purifier_filter_lbl, "Filter 100%");
    lv_obj_set_style_text_color(purifier_filter_lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(purifier_filter_lbl, &lv_font_montserrat_14, 0); 
    lv_obj_align(purifier_filter_lbl, LV_ALIGN_BOTTOM_MID, 0, -16); 

    purifier_fan_lbl = lv_label_create(c6);
    lv_label_set_text(purifier_fan_lbl, "Fan 65%");
    lv_obj_set_style_text_color(purifier_fan_lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(purifier_fan_lbl, &lv_font_montserrat_14, 0); 
    lv_obj_align(purifier_fan_lbl, LV_ALIGN_BOTTOM_MID, 0, 0); 

    filter_start_time = lv_tick_get();

    update_top_time();
    update_wifi_status();
    // main_data_timer_cb(NULL);

    return scr;
}

// ==========================================
// SCREEN 2: Analytics
// ==========================================
static lv_obj_t* build_screen_2(void) {
    lv_obj_t * content;
    lv_obj_t * scr = create_base_layout(NULL, 2, &content);

    // Make content flex and scrollable vertically
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_bottom(content, 40, 0); // Padding for scrolling
    lv_obj_set_style_pad_row(content, 40, 0); // Increased spacing to accommodate X-axis labels

    // --- Helper macro for quick title creation ---
    #define CREATE_CHART_TITLE(parent, text) \
        do { \
            lv_obj_t * lbl = lv_label_create(parent); \
            lv_label_set_text(lbl, text); \
            lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0); \
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0); \
        } while(0)

    // -----------------------------------------------------
    // CHART 1: CPCB Statutory Compliance (Line Chart)
    // -----------------------------------------------------
    CREATE_CHART_TITLE(content, "1. CPCB Statutory Compliance");
    
    // lv_obj_t * chart1 = lv_chart_create(content);
    chart1 = lv_chart_create(content);
    lv_obj_set_size(chart1, 610, 200);
    lv_obj_set_style_bg_color(chart1, COLOR_CARD, 0);
    lv_obj_set_style_border_width(chart1, 0, 0);
    lv_obj_set_style_pad_left(chart1, 20, 0);
    lv_obj_set_style_pad_bottom(chart1, 15, 0);
    lv_chart_set_type(chart1, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart1, 24); // 24 hours
    lv_chart_set_range(chart1, LV_CHART_AXIS_PRIMARY_Y, 0, 200);
    
    // Add Physical Axes (Y: 5 major ticks 0-200 | X: 5 major ticks 0-24)
    lv_chart_set_axis_tick(chart1, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 5, 2, true, 65);
    lv_chart_set_axis_tick(chart1, LV_CHART_AXIS_PRIMARY_X, 10, 5, 5, 2, true, 30);

    ser_pm10 = lv_chart_add_series(chart1, COLOR_OPRUSS_ORG, LV_CHART_AXIS_PRIMARY_Y);
    // lv_chart_series_t * ser_pm25 = lv_chart_add_series(chart1, COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    // lv_chart_series_t * ser_pm25 = lv_chart_add_series(chart1, COLOR_DIM, LV_CHART_AXIS_PRIMARY_Y);
    ser_pm25 = lv_chart_add_series(chart1, COLOR_DIM, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_series_t * ser_limit = lv_chart_add_series(chart1, COLOR_POOR, LV_CHART_AXIS_PRIMARY_Y);

    for(int i = 0; i < 24; i++) {
        lv_chart_set_next_value(chart1, ser_pm10, 0);
        lv_chart_set_next_value(chart1, ser_pm25, 0);
        lv_chart_set_next_value(chart1, ser_limit, 100);
    }

    lv_obj_t * desc1 = lv_label_create(content);
    // lv_label_set_text(desc1, "[ X-Axis: Time (24h) | Y-Axis: Concentration (ug/m3) ]\nTracking particle fields (Orange: PM10, Blue: PM2.5). Crossing Red Line indicates legal failure.");
    lv_label_set_text(desc1, "[ X-Axis: Time (24h) | Y-Axis: Concentration (ug/m3) ]\nTracking particle fields (Orange: PM10, Light Grey: PM2.5). Crossing Red Line indicates legal failure.");
    lv_obj_t * ylbl = lv_label_create(content);
    lv_label_set_text(ylbl, "ug/m3");

    lv_obj_set_style_text_font(ylbl,
                            &lv_font_montserrat_12,
                            0);

    lv_obj_set_style_transform_angle(ylbl,
                                    900,
                                    0);      // 90°

    lv_obj_align_to(ylbl,
                    chart1,
                    LV_ALIGN_OUT_LEFT_MID,
                    -18,
                    0);
    lv_obj_set_style_text_color(desc1, COLOR_DIM, 0);
    
    

    // -----------------------------------------------------
    // CHART 2: Indian AQI Trend (Line Chart)
    // -----------------------------------------------------
    CREATE_CHART_TITLE(content, "2. Indian AQI Trend");
    
    chart2 = lv_chart_create(content);
    lv_obj_set_size(chart2, 640, 200);
    lv_obj_set_style_bg_color(chart2, COLOR_CARD, 0);
    lv_obj_set_style_border_width(chart2, 0, 0);
    lv_chart_set_type(chart2, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart2, CHART_POINTS); 
    lv_chart_set_range(chart2, LV_CHART_AXIS_PRIMARY_Y, 0, 500); 
    
    // Add Physical Axes (Y: 6 ticks 0-500 | X: 5 ticks 0-24)
    lv_chart_set_axis_tick(chart2, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 6, 2, true, 65);
    lv_chart_set_axis_tick(chart2, LV_CHART_AXIS_PRIMARY_X, 10, 5, 5, 2, true, 30);

    ser_aqi = lv_chart_add_series(chart2, COLOR_GOOD, LV_CHART_AXIS_PRIMARY_Y);

    for(int i = 0; i < CHART_POINTS; i++) {
        lv_chart_set_next_value(chart2, ser_aqi, 0);
    }

    lv_obj_t * desc2 = lv_label_create(content);
    lv_label_set_text(desc2, "[ X-Axis: Time (24h) | Y-Axis: Indian AQI Value ]\nHistorical 24-hour Air Quality Index performance.");
    // lv_obj_t * ylbl = lv_label_create(content);
    lv_label_set_text(ylbl, "AQI");

    lv_obj_set_style_text_font(ylbl,
                            &lv_font_montserrat_12,
                            0);

    lv_obj_set_style_transform_angle(ylbl,
                                    900,
                                    0);      // 90°

    lv_obj_align_to(ylbl,
                    chart1,
                    LV_ALIGN_OUT_LEFT_MID,
                    -18,
                    0);
    lv_obj_set_style_text_color(desc2, COLOR_DIM, 0);

    // -----------------------------------------------------
    // CHART 3: Pollutant Contribution (Bar Chart)
    // -----------------------------------------------------
    CREATE_CHART_TITLE(content, "3. Pollutant Contribution");
    
    // lv_obj_t * chart3 = lv_chart_create(content);
    chart3 = lv_chart_create(content);
    lv_obj_set_size(chart3, 640, 200);
    lv_obj_set_style_bg_color(chart3, COLOR_CARD, 0);
    lv_obj_set_style_border_width(chart3, 0, 0);
    lv_chart_set_type(chart3, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(chart3, 2); 
    lv_chart_set_range(chart3, LV_CHART_AXIS_PRIMARY_Y, 0, 150);
    
    // Add Physical Axes (Y: 4 ticks 0-150)
    lv_chart_set_axis_tick(chart3, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 4, 2, true, 65);

    // lv_chart_series_t * ser_c3_pm25 = lv_chart_add_series(chart3, COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    // Update the PM2.5 bar to use Light Grey so it doesn't match the Orange PM10 bar
    // lv_chart_series_t * ser_c3_pm25 = lv_chart_add_series(chart3, COLOR_DIM, LV_CHART_AXIS_PRIMARY_Y);
    ser_c3_pm25 = lv_chart_add_series(chart3, COLOR_DIM, LV_CHART_AXIS_PRIMARY_Y);
    ser_c3_pm10 = lv_chart_add_series(chart3, COLOR_OPRUSS_ORG, LV_CHART_AXIS_PRIMARY_Y);
    ser_c3_tvoc = lv_chart_add_series(chart3, COLOR_DIM, LV_CHART_AXIS_PRIMARY_Y);

    lv_chart_set_next_value(chart3, ser_c3_pm25, 0); 
    lv_chart_set_next_value(chart3, ser_c3_pm10, 0);
    lv_chart_set_next_value(chart3, ser_c3_tvoc, 0);

    lv_chart_set_next_value(chart3, ser_c3_pm25, 0);
    lv_chart_set_next_value(chart3, ser_c3_pm10, 0);
    lv_chart_set_next_value(chart3, ser_c3_tvoc, 0);

    lv_obj_t * desc3 = lv_label_create(content);
    lv_label_set_text(desc3, "[ X-Axis: Categorical Pollutants | Y-Axis: AQI Contribution Score ]\nHighlights the dominant pollutant driving the current AQI.");
    // lv_obj_t * ylbl = lv_label_create(content);
    lv_label_set_text(ylbl, "Score");

    lv_obj_set_style_text_font(ylbl,
                            &lv_font_montserrat_12,
                            0);

    lv_obj_set_style_transform_angle(ylbl,
                                    900,
                                    0);      // 90°

    lv_obj_align_to(ylbl,
                    chart1,
                    LV_ALIGN_OUT_LEFT_MID,
                    -18,
                    0);
    lv_obj_set_style_text_color(desc3, COLOR_DIM, 0);

    // -----------------------------------------------------
    // CHART 4: Compliance Hours (Bar Chart)
    // -----------------------------------------------------
    CREATE_CHART_TITLE(content, "4. Compliance Hours");
    
    chart4 = lv_chart_create(content);
    lv_obj_set_size(chart4, 640, 150);
    lv_obj_set_style_bg_color(chart4, COLOR_CARD, 0);
    lv_obj_set_style_border_width(chart4, 0, 0);
    lv_chart_set_type(chart4, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(chart4, 2); 
    lv_chart_set_range(chart4, LV_CHART_AXIS_PRIMARY_Y, 0, 24);
    
    // Add Physical Axes (Y: 5 ticks 0-24)
    lv_chart_set_axis_tick(chart4, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 5, 2, true, 65);

    ser_c4_comp = lv_chart_add_series(chart4, COLOR_EXCELLENT, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_next_value(chart4, ser_c4_comp, 0); // 21 Hours Compliant
    lv_chart_set_next_value(chart4, ser_c4_comp, 0);  // 3 Hours Exceeded

    lv_obj_t * desc4 = lv_label_create(content);
    lv_label_set_text(desc4, "[ X-Axis: Compliant vs. Exceeded | Y-Axis: Hours (0-24) ]\nSummary of daily facility compliance times.");
    // lv_obj_t * ylbl = lv_label_create(content);
    lv_label_set_text(ylbl, "Hours");

    lv_obj_set_style_text_font(ylbl,
                            &lv_font_montserrat_12,
                            0);

    lv_obj_set_style_transform_angle(ylbl,
                                    900,
                                    0);      // 90°

    lv_obj_align_to(ylbl,
                    chart1,
                    LV_ALIGN_OUT_LEFT_MID,
                    -18,
                    0);
    lv_obj_set_style_text_color(desc4, COLOR_DIM, 0);

    // -----------------------------------------------------
    // CHART 5: AQI Category Distribution (Bar Chart)
    // -----------------------------------------------------
    CREATE_CHART_TITLE(content, "5. AQI Category Distribution");
    
    chart5 = lv_chart_create(content);
    lv_obj_set_size(chart5, 640, 150);
    lv_obj_set_style_bg_color(chart5, COLOR_CARD, 0);
    lv_obj_set_style_border_width(chart5, 0, 0);
    lv_chart_set_type(chart5, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(chart5, 6); // 6 Categories
    lv_chart_set_range(chart5, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    
    // Add Physical Axes (Y: 5 ticks 0-100%)
    lv_chart_set_axis_tick(chart5, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 5, 2, true, 65);

    ser_c5_dist = lv_chart_add_series(chart5, COLOR_GOOD, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_next_value(chart5, ser_c5_dist, 0); // Good
    lv_chart_set_next_value(chart5, ser_c5_dist, 0); // Satisfactory
    lv_chart_set_next_value(chart5, ser_c5_dist, 0); // Moderate
    lv_chart_set_next_value(chart5, ser_c5_dist, 0); // Poor
    lv_chart_set_next_value(chart5, ser_c5_dist, 0);  // Very Poor
    lv_chart_set_next_value(chart5, ser_c5_dist, 0);  // Severe

    lv_obj_t * desc5 = lv_label_create(content);
    lv_label_set_text(desc5, "[ X-Axis: CPCB Categories (Good to Severe) | Y-Axis: Frequency (%) ]\nShows how air quality was distributed over the day.");
    // lv_obj_t * ylbl = lv_label_create(content);
    lv_label_set_text(ylbl, "%");

    lv_obj_set_style_text_font(ylbl,
                            &lv_font_montserrat_12,
                            0);

    lv_obj_set_style_transform_angle(ylbl,
                                    900,
                                    0);      // 90°

    lv_obj_align_to(ylbl,
                    chart1,
                    LV_ALIGN_OUT_LEFT_MID,
                    -18,
                    0);
    lv_obj_set_style_text_color(desc5, COLOR_DIM, 0);

    // -----------------------------------------------------
    // CHART 6: Purifier Performance (Line Chart)
    // -----------------------------------------------------
    CREATE_CHART_TITLE(content, "6. Purifier Performance");
    
    chart6 = lv_chart_create(content);
    lv_obj_set_size(chart6, 640, 150);
    lv_obj_set_style_bg_color(chart6, COLOR_CARD, 0);
    lv_obj_set_style_border_width(chart6, 0, 0);
    lv_chart_set_type(chart6, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart6, CHART_POINTS); 
    lv_chart_set_range(chart6, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    
    // Add Physical Axes (Y: 5 ticks 0-100 | X: 5 ticks 0-10)
    lv_chart_set_axis_tick(chart6, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 5, 2, true, 65);
    lv_chart_set_axis_tick(chart6, LV_CHART_AXIS_PRIMARY_X, 10, 5, 5, 2, true, 30);

    ser_c6_eff = lv_chart_add_series(chart6, COLOR_EXCELLENT, LV_CHART_AXIS_PRIMARY_Y);
    for(int i = 0; i < CHART_POINTS; i++) {
        lv_chart_set_next_value(chart6, ser_c6_eff, 0); 
    }

    lv_obj_t * desc6 = lv_label_create(content);
    lv_label_set_text(desc6, "[ X-Axis: Operational Time | Y-Axis: Filter Efficiency (%) ]\nTracks hardware degradation over time.");
    // lv_obj_t * ylbl = lv_label_create(content);
    lv_label_set_text(ylbl, "Efficiency %");

    lv_obj_set_style_text_font(ylbl,
                            &lv_font_montserrat_12,
                            0);

    lv_obj_set_style_transform_angle(ylbl,
                                    900,
                                    0);      // 90°

    lv_obj_align_to(ylbl,
                    chart1,
                    LV_ALIGN_OUT_LEFT_MID,
                    -18,
                    0);
    lv_obj_set_style_text_color(desc6, COLOR_DIM, 0);

    // -----------------------------------------------------
    // CHART 7: Indoor Comfort Index (Line Chart)
    // -----------------------------------------------------
    CREATE_CHART_TITLE(content, "7. Indoor Comfort Index");
    
    chart7 = lv_chart_create(content);
    lv_obj_set_size(chart7, 640, 150);
    lv_obj_set_style_bg_color(chart7, COLOR_CARD, 0);
    lv_obj_set_style_border_width(chart7, 0, 0);
    lv_chart_set_type(chart7, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart7, CHART_POINTS); 
    lv_chart_set_range(chart7, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    
    // Add Physical Axes
    lv_chart_set_axis_tick(chart7, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 5, 2, true, 65);
    lv_chart_set_axis_tick(chart7, LV_CHART_AXIS_PRIMARY_X, 10, 5, 5, 2, true, 30);

    ser_c7_comf = lv_chart_add_series(chart7, COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    for(int i = 0; i < CHART_POINTS; i++) {
        lv_chart_set_next_value(chart7, ser_c7_comf, 0);
    }

    lv_obj_t * desc7 = lv_label_create(content);
    lv_label_set_text(desc7, "[ X-Axis: Time (24h) | Y-Axis: Comfort Score (0-100) ]\nCalculated comfort score merging Temperature and Humidity data.");
    // lv_obj_t * ylbl = lv_label_create(content);
    lv_label_set_text(ylbl, "Comfort");

    lv_obj_set_style_text_font(ylbl,
                            &lv_font_montserrat_12,
                            0);

    lv_obj_set_style_transform_angle(ylbl,
                                    900,
                                    0);      // 90°

    lv_obj_align_to(ylbl,
                    chart1,
                    LV_ALIGN_OUT_LEFT_MID,
                    -18,
                    0);
    lv_obj_set_style_text_color(desc7, COLOR_DIM, 0);

    // -----------------------------------------------------
    // CHART 8: Weekly AQI Report (Bar Chart)
    // -----------------------------------------------------
    CREATE_CHART_TITLE(content, "8. Weekly AQI Report");
    
    chart8 = lv_chart_create(content);
    lv_obj_set_size(chart8, 640, 150);
    lv_obj_set_style_bg_color(chart8, COLOR_CARD, 0);
    lv_obj_set_style_border_width(chart8, 0, 0);
    lv_chart_set_type(chart8, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(chart8, 7); 
    lv_chart_set_range(chart8, LV_CHART_AXIS_PRIMARY_Y, 0, 300);
    
    // Add Physical Axes (Y: 4 ticks 0-300)
    lv_chart_set_axis_tick(chart8, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 4, 2, true, 65);

    ser_c8_week = lv_chart_add_series(chart8, COLOR_OPRUSS_ORG, LV_CHART_AXIS_PRIMARY_Y);
    for(int i = 0; i < 7; i++) {
        lv_chart_set_next_value(chart8, ser_c8_week, 0); 
    }

    lv_obj_t * desc8 = lv_label_create(content);
    lv_label_set_text(desc8, "[ X-Axis: Days of the Week | Y-Axis: Average AQI ]\nLong-term weekly air quality trend.");
    // lv_obj_t * ylbl = lv_label_create(content);
    lv_label_set_text(ylbl, "AQI");

    lv_obj_set_style_text_font(ylbl,
                            &lv_font_montserrat_12,
                            0);

    lv_obj_set_style_transform_angle(ylbl,
                                    900,
                                    0);      // 90°

    lv_obj_align_to(ylbl,
                    chart1,
                    LV_ALIGN_OUT_LEFT_MID,
                    -18,
                    0);
    lv_obj_set_style_text_color(desc8, COLOR_DIM, 0);

    return scr;
}

// ==========================================
// SCREEN 3: AIR QUALITY ANALYSIS
// ==========================================
// static lv_obj_t* build_screen_3(void) {
//     lv_obj_t * content;
//     lv_obj_t * scr = create_base_layout(NULL, 3, &content);

//     static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
//     static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    
//     lv_obj_set_layout(content, LV_LAYOUT_GRID);
//     lv_obj_set_style_pad_top(content, 55, 0);
//     lv_obj_set_style_grid_column_dsc_array(content, col_dsc, 0);
//     lv_obj_set_style_grid_row_dsc_array(content, row_dsc, 0);
//     lv_obj_set_style_pad_row(content, 14, 0); 
//     lv_obj_set_style_pad_column(content, 14, 0); 

//     /* Export Button */
//     lv_obj_t * export_btn = lv_btn_create(content);

//     lv_obj_set_size(export_btn, 130, 42);

//     lv_obj_align(export_btn,
//                 LV_ALIGN_TOP_RIGHT,
//                 -5,
//                 -45);

//     lv_obj_add_event_cb(export_btn,
//                         export_btn_event_cb,
//                         LV_EVENT_CLICKED,
//                         NULL);

//     lv_obj_t * lbl = lv_label_create(export_btn);

//     lv_label_set_text(lbl,
//                     LV_SYMBOL_DOWNLOAD " Export");

//     lv_obj_center(lbl);

//     lv_obj_t * c1 = create_metric_card(content, "Daily Avg", "38", "AQI", COLOR_TEXT, NULL);
//     lv_obj_set_grid_cell(c1, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
//     lv_obj_t * c2 = create_metric_card(content, "Weekly Avg", "45", "AQI", COLOR_TEXT, NULL);
//     lv_obj_set_grid_cell(c2, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
//     lv_obj_t * c3 = create_metric_card(content, "Max", "168", "AQI", COLOR_POOR, NULL);
//     lv_obj_set_grid_cell(c3, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
//     lv_obj_t * c4 = create_metric_card(content, "Min", "12", "AQI", COLOR_EXCELLENT, NULL);
//     lv_obj_set_grid_cell(c4, LV_GRID_ALIGN_STRETCH, 3, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    
//     return scr;
// }

static lv_obj_t* build_screen_3(void) {
    lv_obj_t * content;
    lv_obj_t * scr = create_base_layout(NULL, 3, &content);

    // Top Right Export Button
    lv_obj_t * export_btn = lv_btn_create(content);
    lv_obj_set_size(export_btn, 130, 38);
    lv_obj_set_style_bg_color(export_btn, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(export_btn, 10, 0);
    lv_obj_align(export_btn, LV_ALIGN_TOP_RIGHT, -10, 0);
    lv_obj_add_event_cb(export_btn, export_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * exp_lbl = lv_label_create(export_btn);
    lv_label_set_text(exp_lbl, LV_SYMBOL_DOWNLOAD " Export");
    lv_obj_center(exp_lbl);

    // Cards Grid shifted down below the Export button
    lv_obj_t * cards_grid = lv_obj_create(content);
    lv_obj_clear_flag(cards_grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(cards_grid, lv_pct(100), 360);
    lv_obj_align(cards_grid, LV_ALIGN_TOP_LEFT, 0, 48); // Shifted down Y=48
    lv_obj_set_style_bg_opa(cards_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cards_grid, 0, 0);
    lv_obj_set_style_pad_all(cards_grid, 0, 0);

    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    
    lv_obj_set_layout(cards_grid, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(cards_grid, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(cards_grid, row_dsc, 0);
    lv_obj_set_style_pad_row(cards_grid, 12, 0); 
    lv_obj_set_style_pad_column(cards_grid, 12, 0); 

    // Row 1: High Level Metrics
    lv_obj_t * c1 = create_metric_card_linked(cards_grid, "Daily Avg", "38", "AQI", COLOR_TEXT, &val_aqi_daily);
    lv_obj_set_grid_cell(c1, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

    lv_obj_t * c2 = create_metric_card_linked(cards_grid, "Weekly Avg", "45", "AQI", COLOR_TEXT, &val_aqi_weekly);
    lv_obj_set_grid_cell(c2, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

    lv_obj_t * c3 = create_metric_card_linked(cards_grid, "Maximum", "168", "AQI", COLOR_POOR, &val_aqi_max);
    lv_obj_set_grid_cell(c3, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

    lv_obj_t * c4 = create_metric_card_linked(cards_grid, "Minimum", "12", "AQI", COLOR_EXCELLENT, &val_aqi_min);
    lv_obj_set_grid_cell(c4, LV_GRID_ALIGN_STRETCH, 3, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

    // Row 2: Secondary Averages
    lv_obj_t * c5 = create_metric_card_linked(cards_grid, "Avg PM2.5", "24.0", "ug/m3", COLOR_TEXT, &val_pm25);
    lv_obj_set_grid_cell(c5, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

    lv_obj_t * c6 = create_metric_card_linked(cards_grid, "Avg PM10", "42.0", "ug/m3", COLOR_TEXT, &val_pm10);
    lv_obj_set_grid_cell(c6, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

    lv_obj_t * c7 = create_metric_card_linked(cards_grid, "Avg Temp", "26.5", "C", COLOR_TEXT, &val_temp);
    lv_obj_set_grid_cell(c7, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

    lv_obj_t * c8 = create_metric_card_linked(cards_grid, "Avg Humidity", "58.0", "%RH", COLOR_TEXT, &val_hum);
    lv_obj_set_grid_cell(c8, LV_GRID_ALIGN_STRETCH, 3, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

    // Row 3: Analytics & Compliance Summary
    lv_obj_t * c9 = create_metric_card_linked(cards_grid, "Compliance", "87.5%", "Score", COLOR_EXCELLENT, &val_comp);
    lv_obj_set_grid_cell(c9, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 2, 1);

    lv_obj_t * c10 = create_metric_card_linked(cards_grid, "Peak Hour", "18:00", "PMR", COLOR_ACCENT, &val_peak);
    lv_obj_set_grid_cell(c10, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 2, 1);

    lv_obj_t * c11 = create_metric_card_linked(cards_grid, "TVOC Avg", "120", "ppb", COLOR_TEXT, &val_tvoc);
    lv_obj_set_grid_cell(c11, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 2, 1);

    lv_obj_t * c12 = create_metric_card_linked(cards_grid, "Status", "NORMAL", "Level", COLOR_EXCELLENT, &val_status);
    lv_obj_set_grid_cell(c12, LV_GRID_ALIGN_STRETCH, 3, 1, LV_GRID_ALIGN_STRETCH, 2, 1);

    return scr;
}

static void orb_anim_cb(void *obj, int32_t v)
{
    lv_obj_set_size((lv_obj_t *)obj, v, v);

    /* Keep it circular */
    lv_obj_set_style_radius((lv_obj_t *)obj,
                            LV_RADIUS_CIRCLE,
                            0);
}

void ui_update_screen3_live_data(int aqi_day, int aqi_week, int aqi_max, int aqi_min, 
                                 float pm25, float pm10, float temp, float hum, 
                                 float compliance, const char* peak_time, int tvoc, const char* status) {
    if(val_aqi_daily)  lv_label_set_text_fmt(val_aqi_daily, "%d", aqi_day);
    if(val_aqi_weekly) lv_label_set_text_fmt(val_aqi_weekly, "%d", aqi_week);
    if(val_aqi_max)    lv_label_set_text_fmt(val_aqi_max, "%d", aqi_max);
    if(val_aqi_min)    lv_label_set_text_fmt(val_aqi_min, "%d", aqi_min);
    
    if(val_pm25)       lv_label_set_text_fmt(val_pm25, "%.1f", pm25);
    if(val_pm10)       lv_label_set_text_fmt(val_pm10, "%.1f", pm10);
    if(val_temp)       lv_label_set_text_fmt(val_temp, "%.1f", temp);
    if(val_hum)        lv_label_set_text_fmt(val_hum, "%.1f", hum);
    
    if(val_comp)       lv_label_set_text_fmt(val_comp, "%.1f%%", compliance);
    if(val_peak)       lv_label_set_text(val_peak, peak_time);
    if(val_tvoc)       lv_label_set_text_fmt(val_tvoc, "%d", tvoc);
    if(val_status)     lv_label_set_text(val_status, status);
}

void voice_start_listening(void)
{
    lv_anim_init(&orb_anim);

    lv_anim_set_var(&orb_anim, voice_orb);

    lv_anim_set_values(&orb_anim, 140, 165);

    lv_anim_set_time(&orb_anim, 700);

    lv_anim_set_playback_time(&orb_anim, 700);

    lv_anim_set_repeat_count(&orb_anim,
                             LV_ANIM_REPEAT_INFINITE);

    lv_anim_set_exec_cb(&orb_anim,
                        orb_anim_cb);

    lv_anim_start(&orb_anim);

    lv_label_set_text(voice_status_lbl,
                      "Listening...");
}

void voice_start_thinking(void)
{
    lv_anim_del(voice_orb, orb_anim_cb);

    lv_label_set_text(voice_status_lbl,
                      "Thinking...");
}

void voice_start_speaking(void)
{
    lv_anim_init(&orb_anim);

    lv_anim_set_var(&orb_anim, voice_orb);

    lv_anim_set_values(&orb_anim, 145, 175);

    lv_anim_set_time(&orb_anim, 350);

    lv_anim_set_playback_time(&orb_anim, 350);

    lv_anim_set_repeat_count(&orb_anim,
                             LV_ANIM_REPEAT_INFINITE);

    lv_anim_set_exec_cb(&orb_anim,
                        orb_anim_cb);

    lv_anim_start(&orb_anim);

    lv_label_set_text(voice_status_lbl,
                      "Speaking...");
}

void voice_stop_animation(void)
{
    lv_anim_del(voice_orb,
                orb_anim_cb);

    lv_obj_set_size(voice_orb,
                    150,
                    150);

    lv_label_set_text(voice_status_lbl,
                      "Ready");
}

// ==========================================
// SCREEN 4: VOICE ASSISTANT
// ==========================================
static lv_obj_t *build_screen_4(void)
{
    lv_obj_t *content;
    lv_obj_t *scr = create_base_layout("Disconnected", 4, &content);

    /* ==============================
       AI Orb
       ============================== */

    voice_orb = lv_obj_create(content);

    lv_obj_set_size(voice_orb, 150, 150);

    lv_obj_set_style_radius(voice_orb,
                            LV_RADIUS_CIRCLE,
                            0);

    lv_obj_set_style_border_width(voice_orb,
                                  0,
                                  0);

    lv_obj_set_style_bg_color(voice_orb,
                              COLOR_ACCENT,
                              0);

    /* Shift upward */
    lv_obj_align(voice_orb,
                 LV_ALIGN_CENTER,
                 -10,
                 -90);

    /* ==============================
       Status
       ============================== */

    voice_status_lbl = lv_label_create(content);

    lv_label_set_text(voice_status_lbl,
                      "Ready");

    lv_obj_set_style_text_font(voice_status_lbl,
                               &lv_font_montserrat_18,
                               0);

    lv_obj_set_style_text_color(voice_status_lbl,
                                COLOR_TEXT,
                                0);

    lv_obj_align_to(voice_status_lbl,
                    voice_orb,
                    LV_ALIGN_OUT_BOTTOM_MID,
                    0,
                    15);

    /* ==============================
       AI Response Card
       ============================== */

    lv_obj_t *card = lv_obj_create(content);

    lv_obj_set_size(card,
                480,
                150);

    lv_obj_align(card,
                LV_ALIGN_BOTTOM_MID,
                -20,
                -20);

    lv_obj_set_style_radius(card,
                            16,
                            0);

    lv_obj_set_style_bg_color(card,
                              COLOR_CARD,
                              0);

    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);

    lv_obj_set_style_shadow_width(card, 18, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);

    lv_obj_set_style_border_width(card,
                                  0,
                                  0);

    voice_response_lbl = lv_label_create(card);

    lv_obj_set_width(voice_response_lbl,
                     380);

    lv_label_set_long_mode(voice_response_lbl,
                           LV_LABEL_LONG_WRAP);

    lv_obj_set_style_text_color(
        voice_response_lbl,
        lv_color_white(),
        0);

    lv_obj_set_style_text_font(
            voice_response_lbl,
            &lv_font_montserrat_18,
            0);

    lv_obj_set_style_text_line_space(
            voice_response_lbl,
            6,
            0);

    lv_label_set_text(
        voice_response_lbl,
        "                Welcome to OPRUSS AI\n\n"
        
        "     Connect an I2S microphone and\n" 
        "   speaker to enable voice interaction.");

    // lv_obj_center(voice_response_lbl);
    lv_obj_align(voice_response_lbl,
             LV_ALIGN_BOTTOM_MID,
             18,
             0);

   /* ==============================
   Quick Commands Card
   ============================== */

    // lv_obj_t *cmd_card = lv_obj_create(content);

    // lv_obj_set_size(cmd_card, 280, 150);

    // lv_obj_align(cmd_card,
    //             LV_ALIGN_BOTTOM_LEFT,
    //             20,
    //             -20);

    // lv_obj_set_style_radius(cmd_card,
    //                         14,
    //                         0);

    // lv_obj_set_style_bg_color(cmd_card,
    //                         COLOR_CARD,
    //                         0);

    // lv_obj_set_style_border_width(cmd_card,
    //                             0,
    //                             0);

    // lv_obj_t *cmd = lv_label_create(cmd_card);

    // lv_label_set_text(
    //     cmd,
    //     LV_SYMBOL_AUDIO " Current AQI\n"
    //     LV_SYMBOL_AUDIO " Explain Pollution\n"
    //     LV_SYMBOL_AUDIO " 24 Hour Trend\n"
    //     LV_SYMBOL_AUDIO " CPCB Violations\n"
    //     LV_SYMBOL_AUDIO " Daily Report");

    // lv_obj_set_style_text_font(cmd,
    //                         &lv_font_montserrat_16,
    //                         0);

    // lv_obj_set_style_text_color(cmd,
    //                             COLOR_TEXT,
    //                             0);

    // lv_obj_align(cmd,
    //             LV_ALIGN_CENTER,
    //             0,
    //             0);
    return scr;
}


// ==========================================
// SCREEN 5: VOICE NOTES
// ==========================================
static lv_obj_t* build_screen_5(void) {
    lv_obj_t * content;
    lv_obj_t * scr = create_base_layout(NULL, 5, &content);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);

    for(int i = 0; i < 3; i++) {
        lv_obj_t * row = lv_obj_create(content);
        lv_obj_set_size(row, lv_pct(100), 60);
        lv_obj_set_style_bg_color(row, COLOR_CARD, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        
        lv_obj_t * lbl = lv_label_create(row);
        lv_label_set_text_fmt(lbl, "Voice Note %d - 18 Jun", i+1);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 10, 0);
        
        lv_obj_t * btn = lv_btn_create(row);
        lv_obj_set_size(btn, 40, 40);
        lv_obj_set_style_bg_color(btn, COLOR_ACCENT, 0);
        lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_t * play_lbl = lv_label_create(btn);
        lv_label_set_text(play_lbl, LV_SYMBOL_PLAY);
        lv_obj_center(play_lbl);
    }
    return scr;
}


// ==========================================
// SCREEN 6: SETTINGS (INTERACTIVE)
// ==========================================
static lv_obj_t* build_screen_6(void) {
    lv_obj_t * content;
    lv_obj_t * scr = create_base_layout(NULL, 6, &content);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);

    lv_obj_t * row1 = lv_obj_create(content);
    lv_obj_set_size(row1, lv_pct(100), 60);
    lv_obj_set_style_bg_color(row1, COLOR_CARD, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    
    lv_obj_t * lbl1 = lv_label_create(row1);
    lv_label_set_text(lbl1, "Auto-reconnect watchdog");
    lv_obj_align(lbl1, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t * sw = lv_switch_create(row1);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_state(sw, LV_STATE_CHECKED); 

    lv_obj_t * row2 = lv_obj_create(content);
    lv_obj_set_size(row2, lv_pct(100), 60);
    lv_obj_set_style_bg_color(row2, COLOR_CARD, 0);
    lv_obj_set_style_border_width(row2, 0, 0);

    lv_obj_t * lbl2 = lv_label_create(row2);
    lv_label_set_text(lbl2, "Display Brightness");
    lv_obj_align(lbl2, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t * slider = lv_slider_create(row2);
    lv_obj_set_width(slider, 200);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_slider_set_value(slider, 80, LV_ANIM_OFF);

    return scr;
}

// --- Main Init After Splash ---
static void ui_init_main(void) {
    current_screen_idx = 1;
    lv_obj_t * start_scr = build_screen_1();
    // lv_scr_load(start_scr);
    // Replace instant lv_scr_load with a smooth 500ms fade animation
    lv_scr_load_anim(start_scr, LV_SCR_LOAD_ANIM_FADE_OUT, 250, 0, true);
    
    lv_timer_create(main_data_timer_cb, 1000, NULL);
}

// --- Public Init Function ---
void ui_init(void) {
    // ui_data.pm10 = -1.0f;
    real_data.pm10 = -1.0f;
    // This prevents the white flash before the splash screen is fully rendered.
    lv_obj_set_style_bg_color(lv_scr_act(), COLOR_BG, 0);
    current_screen_idx = 0; 
    lv_obj_t * splash_scr = build_splash_screen();
    lv_scr_load(splash_scr);
}

void ui_push_sensor_data(sensor_data_t *incoming)
{
    if(incoming == NULL)
        return;

    real_data = *incoming;
}

// /*Above code is for pseudo data generation and testing only*/
// /*Below code is for real data reception and broadcast over rs485 port*/

