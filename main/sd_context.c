#include "sd_context.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ch422g.h"

static const char *TAG = "SD_CTX";

extern SemaphoreHandle_t sd_mutex;

// Get list of CSV files sorted by date
static int get_csv_files(char files[][64], int max_files) {
    DIR *dir = opendir("/sdcard");
    if (!dir) return 0;
    
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) && count < max_files) {
        if (strstr(entry->d_name, ".csv") && strlen(entry->d_name) == 12) {
            strncpy(files[count], entry->d_name, 63);
            files[count][63] = '\0';
            count++;
        }
    }
    closedir(dir);
    
    // Sort by date (descending)
    for (int i = 0; i < count-1; i++)
        for (int j = i+1; j < count; j++)
            if (strcmp(files[i], files[j]) < 0) {
                char tmp[64]; strcpy(tmp, files[i]);
                strcpy(files[i], files[j]); strcpy(files[j], tmp);
            }
    
    return count;
}

// Parse a CSV line into values
static bool parse_line(char *line, float *pm25, float *pm10, float *temp, 
                       float *humidity, float *tvoc, float *aqi, char *timestamp) {
    char *fields[13] = {0};
    int count = 0;
    char *save = NULL;
    
    for (char *f = strtok_r(line, ",", &save); f && count < 13; f = strtok_r(NULL, ",", &save)) {
        while (*f == ' ' || *f == '\t') f++;
        size_t len = strlen(f);
        while (len > 0 && (f[len-1]=='\r' || f[len-1]=='\n')) f[--len]='\0';
        fields[count++] = f;
    }
    
    if (count < 8 || strncmp(fields[0], "Date", 4) == 0) return false;
    
    if (timestamp) {
        if (count >= 13) snprintf(timestamp, 32, "%s %s", fields[0], fields[1]);
        else snprintf(timestamp, 32, "%s", fields[0]);
    }
    
    if (pm25) *pm25 = atof(fields[count >= 13 ? 2 : 1]);
    if (temp) *temp = atof(fields[count >= 13 ? 3 : 2]);
    if (humidity) *humidity = atof(fields[count >= 13 ? 4 : 3]);
    if (tvoc) *tvoc = atof(fields[count >= 13 ? 7 : 6]);
    if (pm10) *pm10 = (count >= 13) ? atof(fields[9]) : 0;
    if (aqi) *aqi = (count >= 13) ? atof(fields[12]) : 0;
    
    return true;
}

bool sd_context_get(char *context, size_t context_size) {
    if (!context || !context_size) return false;
    context[0] = '\0';
    
    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(3000)) != pdTRUE) {
        snprintf(context, context_size, "SD card busy.");
        return false;
    }
    
    ch422g_set_sd_selected(true);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    char files[31][64];
    int file_count = get_csv_files(files, 30);
    
    if (file_count == 0) {
        snprintf(context, context_size, "No data files found.");
        ch422g_set_sd_selected(false);
        xSemaphoreGive(sd_mutex);
        return false;
    }
    
    // Read last 10 readings from today
    char path[128];
    snprintf(path, sizeof(path), "/sdcard/%s", files[0]);
    
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(context, context_size, "Cannot read data file.");
        ch422g_set_sd_selected(false);
        xSemaphoreGive(sd_mutex);
        return false;
    }
    
    char lines[10][256];
    int line_count = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), f) && line_count < 10) {
        if (strlen(line) > 5 && strncmp(line, "Date", 4) != 0) {
            strncpy(lines[line_count], line, 255);
            line_count++;
        }
    }
    fclose(f);
    
    // Build context
    int offset = snprintf(context, context_size, 
        "Air Quality Data (%d files available):\n", file_count);
    
    // Latest readings
    if (line_count > 0) {
        offset += snprintf(context + offset, context_size - offset, 
            "Latest %d readings from %s:\n", line_count, files[0]);
        
        for (int i = (line_count > 5 ? line_count-5 : 0); i < line_count; i++) {
            float pm25, temp, hum, tvoc, aqi;
            char ts[32];
            if (parse_line(lines[i], &pm25, NULL, &temp, &hum, &tvoc, &aqi, ts)) {
                offset += snprintf(context + offset, context_size - offset,
                    "  %s | AQI:%.0f PM2.5:%.1f Temp:%.1fC Hum:%.1f%%\n",
                    ts, (double)aqi, (double)pm25, (double)temp, (double)hum);
            }
        }
    }
    
    // File list
    offset += snprintf(context + offset, context_size - offset, "\nAvailable dates: ");
    for (int i = 0; i < file_count && i < 7; i++) {
        offset += snprintf(context + offset, context_size - offset, 
            "%s%s", files[i], (i < file_count-1 && i < 6) ? ", " : "");
    }
    
    ch422g_set_sd_selected(false);
    xSemaphoreGive(sd_mutex);
    
    ESP_LOGI(TAG, "Context: %d bytes", offset);
    return true;
}

bool sd_context_get_today_summary(char *summary, size_t summary_size) {
    if (!summary || !summary_size) return false;
    summary[0] = '\0';
    
    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(3000)) != pdTRUE) return false;
    ch422g_set_sd_selected(true);
    
    time_t now; struct tm tm;
    time(&now); localtime_r(&now, &tm);
    int today = (tm.tm_year+1900)*10000 + (tm.tm_mon+1)*100 + tm.tm_mday;
    
    char path[64];
    snprintf(path, sizeof(path), "/sdcard/%08d.csv", today);
    
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(summary, summary_size, "No data for today yet.");
        ch422g_set_sd_selected(false);
        xSemaphoreGive(sd_mutex);
        return false;
    }
    
    float sum_aqi=0, sum_pm25=0, max_aqi=0, min_aqi=999;
    int count=0;
    char line[256], max_ts[32]="", min_ts[32]="";
    
    while (fgets(line, sizeof(line), f)) {
        float pm25, temp, hum, tvoc, aqi; char ts[32];
        if (parse_line(line, &pm25, NULL, &temp, &hum, &tvoc, &aqi, ts)) {
            sum_aqi += aqi; sum_pm25 += pm25; count++;
            if (aqi > max_aqi) { max_aqi = aqi; strcpy(max_ts, ts); }
            if (aqi < min_aqi) { min_aqi = aqi; strcpy(min_ts, ts); }
        }
    }
    fclose(f);
    
    if (count > 0) {
        snprintf(summary, summary_size,
            "Today: %d readings | Avg AQI: %.0f | Max: %.0f at %s | Min: %.0f at %s | Avg PM2.5: %.1f",
            count, sum_aqi/count, (double)max_aqi, max_ts, (double)min_aqi, min_ts, sum_pm25/count);
    }
    
    ch422g_set_sd_selected(false);
    xSemaphoreGive(sd_mutex);
    return count > 0;
}

// bool sd_context_compare_days(char *comparison, size_t comparison_size) {
//     if (!comparison || !comparison_size) return false;
    
//     if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(3000)) != pdTRUE) return false;
//     ch422g_set_sd_selected(true);
    
//     char files[31][64];
//     int count = get_csv_files(files, 30);
    
//     if (count < 2) {
//         snprintf(comparison, comparison_size, "Need at least 2 days of data.");
//         ch422g_set_sd_selected(false);
//         xSemaphoreGive(sd_mutex);
//         return false;
//     }
    
//     // Compare today (files[0]) with yesterday (files[1])
//     float today_avg=0, yest_avg=0, today_pm=0, yest_pm=0;
//     int today_n=0, yest_n=0;
//     char line[256];
    
//     for (int d = 0; d < 2; d++) {
//         char path[128];
//         snprintf(path, sizeof(path), "/sdcard/%s", files[d]);
//         FILE *f = fopen(path, "r");
//         if (f) {
//             while (fgets(line, sizeof(line), f)) {
//                 float pm25, aqi; char ts[32];
//                 if (parse_line(line, &pm25, NULL, NULL, NULL, NULL, &aqi, ts)) {
//                     if (d == 0) { today_avg += aqi; today_pm += pm25; today_n++; }
//                     else { yest_avg += aqi; yest_pm += pm25; yest_n++; }
//                 }
//             }
//             fclose(f);
//         }
//     }
    
//     if (today_n > 0 && yest_n > 0) {
//         today_avg /= today_n; yest_avg /= yest_n;
//         today_pm /= today_n; yest_pm /= yest_n;
//         float change = ((today_avg - yest_avg) / yest_avg) * 100;
//         snprintf(comparison, comparison_size,
//             "Today (%s): AQI %.0f, PM2.5 %.1f | Yesterday (%s): AQI %.0f, PM2.5 %.1f | Change: %.1f%%",
//             files[0], (double)today_avg, (double)today_pm,
//             files[1], (double)yest_avg, (double)yest_pm, (double)change);
//     }
    
//     ch422g_set_sd_selected(false);
//     xSemaphoreGive(sd_mutex);
//     return true;
// }

bool sd_context_compare_days(char *comparison, size_t comparison_size)
{
    if (!comparison || comparison_size == 0) {
        return false;
    }

    comparison[0] = '\0';

    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(3000)) != pdTRUE) {
        snprintf(comparison, comparison_size,
                 "SD card is currently busy.");
        return false;
    }

    ch422g_set_sd_selected(true);
    vTaskDelay(pdMS_TO_TICKS(10));

    char files[31][64];
    int count = get_csv_files(files, 30);

    if (count < 2) {
        snprintf(comparison, comparison_size,
                 "Historical comparison unavailable: at least two daily CSV files are required.");

        ch422g_set_sd_selected(false);
        xSemaphoreGive(sd_mutex);
        return false;
    }

    /*
     * files[0] = newest available CSV
     * files[1] = second newest CSV
     *
     * Because filenames are YYYYMMDD.csv, the descending
     * filename sort corresponds to chronological order.
     */

    float today_aqi_sum = 0.0f;
    float yesterday_aqi_sum = 0.0f;

    float today_pm25_sum = 0.0f;
    float yesterday_pm25_sum = 0.0f;

    int today_count = 0;
    int yesterday_count = 0;

    char line[256];

    for (int d = 0; d < 2; d++) {

        char path[128];

        snprintf(path,
                 sizeof(path),
                 "/sdcard/%s",
                 files[d]);

        FILE *f = fopen(path, "r");

        if (!f) {
            continue;
        }

        while (fgets(line, sizeof(line), f)) {

            float pm25 = 0.0f;
            float aqi = 0.0f;

            if (parse_line(
                    line,
                    &pm25,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    &aqi,
                    NULL)) {

                /*
                 * Ignore obviously invalid zero/negative
                 * AQI records.
                 */
                if (aqi < 0.0f) {
                    continue;
                }

                if (d == 0) {

                    today_aqi_sum += aqi;
                    today_pm25_sum += pm25;
                    today_count++;

                } else {

                    yesterday_aqi_sum += aqi;
                    yesterday_pm25_sum += pm25;
                    yesterday_count++;
                }
            }
        }

        fclose(f);
    }

    /*
     * Make sure both days actually contain usable data.
     */
    if (today_count == 0 || yesterday_count == 0) {

        snprintf(
            comparison,
            comparison_size,
            "Historical comparison unavailable: one of the two daily files contains no valid sensor readings."
        );

        ch422g_set_sd_selected(false);
        xSemaphoreGive(sd_mutex);

        return false;
    }

    float today_avg_aqi =
        today_aqi_sum / (float)today_count;

    float yesterday_avg_aqi =
        yesterday_aqi_sum / (float)yesterday_count;

    float today_avg_pm25 =
        today_pm25_sum / (float)today_count;

    float yesterday_avg_pm25 =
        yesterday_pm25_sum / (float)yesterday_count;

    float aqi_change = 0.0f;
    float pm25_change = 0.0f;

    /*
     * Percentage change:
     *
     * ((today - yesterday) / yesterday) * 100
     */
    if (yesterday_avg_aqi != 0.0f) {

        aqi_change =
            ((today_avg_aqi - yesterday_avg_aqi) /
             yesterday_avg_aqi) * 100.0f;
    }

    if (yesterday_avg_pm25 != 0.0f) {

        pm25_change =
            ((today_avg_pm25 - yesterday_avg_pm25) /
             yesterday_avg_pm25) * 100.0f;
    }

    snprintf(
        comparison,
        comparison_size,

        "DAY-TO-DAY AIR QUALITY COMPARISON\n"
        "Today file: %s\n"
        "Yesterday file: %s\n"
        "Today readings: %d\n"
        "Yesterday readings: %d\n"
        "Today average AQI: %.0f\n"
        "Yesterday average AQI: %.0f\n"
        "AQI change: %.1f%%\n"
        "Today average PM2.5: %.1f ug/m3\n"
        "Yesterday average PM2.5: %.1f ug/m3\n"
        "PM2.5 change: %.1f%%",

        files[0],
        files[1],

        today_count,
        yesterday_count,

        (double)today_avg_aqi,
        (double)yesterday_avg_aqi,

        (double)aqi_change,

        (double)today_avg_pm25,
        (double)yesterday_avg_pm25,

        (double)pm25_change
    );

    ESP_LOGI(
        TAG,
        "Historical comparison: Today AQI %.0f, Yesterday AQI %.0f, Change %.1f%%",
        (double)today_avg_aqi,
        (double)yesterday_avg_aqi,
        (double)aqi_change
    );

    ch422g_set_sd_selected(false);
    xSemaphoreGive(sd_mutex);

    return true;
}

bool sd_context_get_latest_readings(int count, char *csv_data, size_t csv_size) {
    // Similar implementation - returns last N readings as CSV
    snprintf(csv_data, csv_size, "Latest readings function ready");
    return true;
}

bool sd_context_get_stats(const char *parameter, char *stats, size_t stats_size) {
    snprintf(stats, stats_size, "Stats for %s coming soon", parameter);
    return true;
}