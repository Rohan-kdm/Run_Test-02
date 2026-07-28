#include "spiffs.h"

#include "esp_log.h"
#include "esp_spiffs.h"
// #include "esp_vfs_spiffs.h"

static const char *TAG = "SPIFFS";

void spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    size_t total = 0;
    size_t used = 0;

    ret = esp_spiffs_info(NULL, &total, &used);
    FILE *f = fopen("/spiffs/hello.txt", "r");

    if (f == NULL) {
        ESP_LOGE(TAG, "Cannot open hello.txt");
    }
    else {

        char buf[64];

        fgets(buf, sizeof(buf), f);

        ESP_LOGI(TAG, "Read: %s", buf);

        fclose(f);
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG,
                 "SPIFFS mounted successfully");
        ESP_LOGI(TAG,
                 "Total: %u KB",
                 (unsigned)(total / 1024));
        ESP_LOGI(TAG,
                 "Used : %u KB",
                 (unsigned)(used / 1024));
    }
}