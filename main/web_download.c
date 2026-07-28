/*=============================================================
 *
 *  web_download.c
 *
 *=============================================================*/

#include "web_download.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "esp_log.h"
#include "esp_http_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t sd_mutex;

#define MOUNT_POINT "/sdcard"

static const char *TAG = "DOWNLOAD";

/*-------------------------------------------------------------
 * Parse YYYYMMDD
 *------------------------------------------------------------*/
static bool parse_date(const char *str, struct tm *tm_out)
{
    if (strlen(str) != 8)
        return false;

    char buf[5];

    memset(tm_out, 0, sizeof(struct tm));

    memcpy(buf, str, 4);
    buf[4] = 0;
    tm_out->tm_year = atoi(buf) - 1900;

    memcpy(buf, str + 4, 2);
    buf[2] = 0;
    tm_out->tm_mon = atoi(buf) - 1;

    memcpy(buf, str + 6, 2);
    buf[2] = 0;
    tm_out->tm_mday = atoi(buf);

    tm_out->tm_hour = 0;
    tm_out->tm_min = 0;
    tm_out->tm_sec = 0;

    if (mktime(tm_out) == -1)
        return false;

    return true;
}

/*-------------------------------------------------------------
 * Send one file
 *------------------------------------------------------------*/
static esp_err_t send_csv_file(
        httpd_req_t *req,
        const char *filename,
        bool *header_sent)
{
    FILE *fp = fopen(filename, "r");

    if (!fp)
    {
        ESP_LOGW(TAG, "Missing %s", filename);
        return ESP_OK;
    }

    char line[512];
    bool first_line = true;

    while (fgets(line, sizeof(line), fp))
    {
        if (first_line)
        {
            first_line = false;

            if (*header_sent)
                continue;

            *header_sent = true;
        }

        esp_err_t err =
            httpd_resp_send_chunk(
                req,
                line,
                strlen(line));

        if (err != ESP_OK)
        {
            fclose(fp);
            return err;
        }
    }

    fclose(fp);

    return ESP_OK;
}

/*-------------------------------------------------------------
 * Download handler
 *------------------------------------------------------------*/
static esp_err_t download_handler(httpd_req_t *req)
{
    char query[128];

    char start_str[16];
    char end_str[16];

    memset(query, 0, sizeof(query));
    memset(start_str, 0, sizeof(start_str));
    memset(end_str, 0, sizeof(end_str));

    if (httpd_req_get_url_query_str(
            req,
            query,
            sizeof(query)) != ESP_OK)
    {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Missing query");

        return ESP_FAIL;
    }

    if (httpd_query_key_value(
            query,
            "start",
            start_str,
            sizeof(start_str)) != ESP_OK)
    {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Missing start");

        return ESP_FAIL;
    }

    if (httpd_query_key_value(
            query,
            "end",
            end_str,
            sizeof(end_str)) != ESP_OK)
    {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Missing end");

        return ESP_FAIL;
    }

    struct tm start_tm;
    struct tm end_tm;

    if (!parse_date(start_str, &start_tm))
    {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Bad start date");

        return ESP_FAIL;
    }

    if (!parse_date(end_str, &end_tm))
    {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Bad end date");

        return ESP_FAIL;
    }

    if (difftime(
            mktime(&start_tm),
            mktime(&end_tm)) > 0)
    {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Start > End");

        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/csv");

    httpd_resp_set_hdr(
        req,
        "Content-Disposition",
        "attachment; filename=\"AQ7_Report.csv\"");

    if (xSemaphoreTake(
            sd_mutex,
            pdMS_TO_TICKS(10000)) != pdTRUE)
    {
        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "SD busy");

        return ESP_FAIL;
    }

    bool header_sent = false;

    struct tm current = start_tm;

        while (1)
        {
            char filename[64];

            snprintf(filename,
                    sizeof(filename),
                    MOUNT_POINT "/%04d%02d%02d.csv",
                    current.tm_year + 1900,
                    current.tm_mon + 1,
                    current.tm_mday);

            ESP_LOGI(TAG, "Checking %s", filename);

            esp_err_t err = send_csv_file(
                                req,
                                filename,
                                &header_sent);

            if (err != ESP_OK)
            {
                xSemaphoreGive(sd_mutex);
                return err;
            }

            /* Reached last date? */
            if ((current.tm_year == end_tm.tm_year) &&
                (current.tm_mon  == end_tm.tm_mon)  &&
                (current.tm_mday == end_tm.tm_mday))
            {
                break;
            }

            /* Next day */
            current.tm_mday++;
            mktime(&current);
        }

        xSemaphoreGive(sd_mutex);

        httpd_resp_send_chunk(req, NULL, 0);

        ESP_LOGI(TAG, "CSV download complete");

        return ESP_OK;
    }

    /*-------------------------------------------------------------
    * Start HTTP Server
    *------------------------------------------------------------*/
    httpd_handle_t start_webserver(void)
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();

        config.server_port = 80;
        config.max_uri_handlers = 8;

        httpd_handle_t server = NULL;

        esp_err_t err = httpd_start(&server, &config);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start HTTP server");
            return NULL;
        }

        httpd_uri_t download_uri =
        {
            .uri       = "/download",
            .method    = HTTP_GET,
            .handler   = download_handler,
            .user_ctx  = NULL
        };

        err = httpd_register_uri_handler(
                    server,
                    &download_uri);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG,
                    "Failed to register /download");

            httpd_stop(server);

            return NULL;
        }

        ESP_LOGI(TAG,
                "===================================");

        ESP_LOGI(TAG,
                "HTTP Download Server Started");

        ESP_LOGI(TAG,
                "URI : /download");

        ESP_LOGI(TAG,
                "Example:");

        ESP_LOGI(TAG,
                "http://<ESP-IP>/download?start=20260701&end=20260710");

        ESP_LOGI(TAG,
                "===================================");

        return server;
    }