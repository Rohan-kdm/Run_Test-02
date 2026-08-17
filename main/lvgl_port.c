#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lvgl_port.h"

static const char *TAG = "lv_port";
static SemaphoreHandle_t lvgl_mux;
static TaskHandle_t lvgl_task_handle = NULL;

// Import our new LovyanGFX C-Wrapper functions
extern void lgfx_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
extern void lgfx_touch_read_cb(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

static void tick_increment(void *arg) {
    lv_tick_inc(LVGL_PORT_TICK_PERIOD_MS);
}

static esp_err_t tick_init(void) {
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &tick_increment,
        .name = "LVGL tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    return esp_timer_start_periodic(lvgl_tick_timer, LVGL_PORT_TICK_PERIOD_MS * 1000);
}

static void lvgl_port_task(void *arg) {
    uint32_t task_delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
    while (1) {
        if (lvgl_port_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            lvgl_port_unlock();
        }
        if (task_delay_ms > LVGL_PORT_TASK_MAX_DELAY_MS) task_delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
        else if (task_delay_ms < LVGL_PORT_TASK_MIN_DELAY_MS) task_delay_ms = LVGL_PORT_TASK_MIN_DELAY_MS;
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

esp_err_t lvgl_port_init(void) {
    lv_init();
    ESP_ERROR_CHECK(tick_init());

    // 1. Setup LVGL Draw Buffers (Using 1/10th screen size in PSRAM)
    static lv_disp_draw_buf_t disp_buf;
    int buffer_size = LVGL_PORT_H_RES * LVGL_PORT_V_RES / 10; 
    void *buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    void *buf2 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buffer_size);

    // 2. Setup LovyanGFX Display Driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LVGL_PORT_H_RES;
    disp_drv.ver_res = LVGL_PORT_V_RES;
    disp_drv.flush_cb = lgfx_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    // 3. Setup LovyanGFX Touch Driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lgfx_touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    
    BaseType_t core_id = (LVGL_PORT_TASK_CORE < 0) ? tskNO_AFFINITY : LVGL_PORT_TASK_CORE;
    xTaskCreatePinnedToCore(lvgl_port_task, "lvgl", LVGL_PORT_TASK_STACK_SIZE, NULL, LVGL_PORT_TASK_PRIORITY, &lvgl_task_handle, core_id);

    return ESP_OK;
}

bool lvgl_port_lock(int timeout_ms) {
    const TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_port_unlock(void) {
    xSemaphoreGiveRecursive(lvgl_mux);
}