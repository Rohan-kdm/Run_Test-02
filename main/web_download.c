#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "web_download.h"

static const char *TAG = "WEB_DOWNLOAD";

extern SemaphoreHandle_t sd_mutex;
extern char latest_report_filename[128];
#define MOUNT_POINT "/sdcard"

// --- HTTP GET Download Handler ---
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[256];
    char query_str[128];
    char start_date[32] = {0};
    char end_date[32] = {0};

    // 1. Check if query string exists (e.g., /download?start=20260701&end=20260729)
    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
        if (httpd_query_key_value(query_str, "start", start_date, sizeof(start_date)) == ESP_OK &&
            httpd_query_key_value(query_str, "end", end_date, sizeof(end_date)) == ESP_OK) {
            snprintf(filepath, sizeof(filepath), "%s/report_%s_to_%s.csv", MOUNT_POINT, start_date, end_date);
        } else {
            snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, latest_report_filename);
        }
    } else {
        snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, latest_report_filename);
    }

    ESP_LOGI(TAG, "Attempting to serve file: %s", filepath);

    // 2. Protect SD card access with your FreeRTOS mutex
    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD Card busy");
        return ESP_FAIL;
    }

    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, latest_report_filename);
        f = fopen(filepath, "r");
    }

    if (f == NULL) {
        xSemaphoreGive(sd_mutex);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Report file not found. Generate export first.");
        return ESP_FAIL;
    }

    // 3. Set HTTP headers for file download attachment
    httpd_resp_set_type(req, "text/csv");
    char header_val[256];
    snprintf(header_val, sizeof(header_val), "attachment; filename=\"%s\"", latest_report_filename);
    httpd_resp_set_hdr(req, "Content-Disposition", header_val);

    // 4. Allocate chunk buffer on the HEAP to prevent stack overflow
    char *chunk = malloc(1024);
    if (chunk == NULL) {
        fclose(f);
        xSemaphoreGive(sd_mutex);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    size_t chunk_size;
    esp_err_t err = ESP_OK;

    while ((chunk_size = fread(chunk, 1, 1024, f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, chunk_size) != ESP_OK) {
            err = ESP_FAIL;
            break;
        }
        // Feed the Task Watchdog Timer during large file transfers
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    free(chunk);
    fclose(f);
    xSemaphoreGive(sd_mutex);

    // 5. Send empty chunk to signal completion of response
    httpd_resp_send_chunk(req, NULL, 0);

    return err;
}

// URI registration structure
static const httpd_uri_t download_uri = {
    .uri       = "/download",
    .method    = HTTP_GET,
    .handler   = download_get_handler,
    .user_ctx  = NULL
};

// --- Start Web Server ---
httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // Increased stack size from default 4096 to 8192 to prevent potential overflows
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &download_uri);
        ESP_LOGI(TAG, "Registered /download URI handler successfully");
        return server;
    }

    ESP_LOGE(TAG, "Failed to start HTTP server!");
    return NULL;
}