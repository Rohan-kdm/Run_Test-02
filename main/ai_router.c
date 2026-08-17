// #include "ai_router.h"
// #include "aq_voice_engine.h"
// #include <stdio.h>
// #include <string.h>
// #include <ctype.h>
// #include "esp_log.h"
// #include "esp_wifi.h"
// #include "ai_client.h"

// static const char *TAG = "AI_ROUTER";

// static ai_source_t g_source = AI_SOURCE_HYBRID;
// static bool g_cloud_available = false;


// static bool str_contains(const char *text, const char *needle) {
//     if (!text || !needle) return false;
//     char lt[256], ln[128]; size_t i;
//     for(i=0; text[i]&&i<255; i++) lt[i]=tolower((unsigned char)text[i]);
//     lt[i]=0;
//     for(i=0; needle[i]&&i<127; i++) ln[i]=tolower((unsigned char)needle[i]);
//     ln[i]=0;
//     return strstr(lt, ln) != NULL;
// }

// typedef enum {
//     CATEGORY_SIMPLE,
//     CATEGORY_ANALYSIS,
//     CATEGORY_BROAD,
//     CATEGORY_CONTROL
// } question_category_t;

// // Forward declaration - ai_client.h has dependency issues
// void ai_client_init(void);

// static question_category_t classify_question(const char *question) {
//     if (!question) return CATEGORY_SIMPLE;
//     if (str_contains(question, "fan") || str_contains(question, "turn on") || 
//         str_contains(question, "turn off") || str_contains(question, "speed"))
//         return CATEGORY_CONTROL;
//     if (str_contains(question, "current aqi") || str_contains(question, "air quality now") ||
//         str_contains(question, "what is the aqi") || str_contains(question, "status"))
//         return CATEGORY_SIMPLE;
//     if (str_contains(question, "yesterday") || str_contains(question, "compare") ||
//         str_contains(question, "trend") || str_contains(question, "worst") ||
//         str_contains(question, "highest") || str_contains(question, "average") ||
//         str_contains(question, "summarize") || str_contains(question, "analyze"))
//         return CATEGORY_ANALYSIS;
//     return CATEGORY_BROAD;
// }

// // static void ai_router_cloud_callback(const char *response, bool success, void *user_data) {
// //     ESP_LOGI(TAG, "Cloud AI: %s (ok=%d)", response ? response : "NULL", success);
// // }

// void ai_router_init(void) {
//     ESP_LOGI(TAG, "AI Router ready");
//     ai_client_init(); 
//     aq_voice_engine_init();
// }

// bool ai_router_ask(const char *question, char *answer, size_t answer_size) {
//     if (!question || !answer || !answer_size) return false;
    
//     extern bool wifi_is_connected;
//     g_cloud_available = wifi_is_connected;
    
//     question_category_t category = classify_question(question);
//     bool success = false;
    
//     ESP_LOGI(TAG, "Q: '%s' | Cat: %d | Cloud: %d", question, category, g_cloud_available);
    
//     // Always use local engine for response
//     success = aq_voice_process(question, answer, answer_size, false);

//     // Fire cloud request (response comes via MQTT callback)
//     // if (g_cloud_available && g_source != AI_SOURCE_LOCAL) {
//     //         if (ai_client_ask(question, ai_router_cloud_callback, NULL)) {
//     //         ESP_LOGI(TAG, "Cloud request sent");
//     //     }
//     // }
    
//     return success;
// }

// void ai_router_set_source(ai_source_t source) { g_source = source; }
// ai_source_t ai_router_get_source(void) { return g_source; }
// bool ai_router_is_cloud_available(void) { return g_cloud_available; }

// const char *ai_router_status(void) {
//     if (g_cloud_available)
//         return (g_source == AI_SOURCE_CLOUD) ? "Cloud AI" : "Hybrid AI (Online)";
//     return "Local AI (Offline)";
// }

#include "ai_router.h"
#include "aq_voice_engine.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "ai_client.h"

static const char *TAG = "AI_ROUTER";

static ai_source_t g_source = AI_SOURCE_HYBRID;
static bool g_cloud_available = false;

static bool str_contains(const char *text, const char *needle) {
    if (!text || !needle) return false;
    char lt[256], ln[128]; size_t i;
    for(i=0; text[i]&&i<255; i++) lt[i]=tolower((unsigned char)text[i]);
    lt[i]=0;
    for(i=0; needle[i]&&i<127; i++) ln[i]=tolower((unsigned char)needle[i]);
    ln[i]=0;
    return strstr(lt, ln) != NULL;
}

typedef enum {
    CATEGORY_SIMPLE,
    CATEGORY_ANALYSIS,
    CATEGORY_BROAD,
    CATEGORY_CONTROL
} question_category_t;

void ai_client_init(void);

static question_category_t classify_question(const char *question) {
    if (!question) return CATEGORY_SIMPLE;
    if (str_contains(question, "fan") || str_contains(question, "turn on") || 
        str_contains(question, "turn off") || str_contains(question, "speed"))
    {
        return CATEGORY_CONTROL;
    }
    if (str_contains(question, "current aqi") || str_contains(question, "air quality now") ||
        str_contains(question, "what is the aqi") || str_contains(question, "status"))
    {
        return CATEGORY_SIMPLE;
    }
    if (str_contains(question, "yesterday") || str_contains(question, "compare") ||
        str_contains(question, "trend") || str_contains(question, "worst") ||
        str_contains(question, "highest") || str_contains(question, "average") ||
        str_contains(question, "summarize") || str_contains(question, "analyze") ||
        str_contains(question, "predict") ||
        str_contains(question, "forecast") ||
        str_contains(question, "next hour"))
    {
        return CATEGORY_ANALYSIS;
    }
    return CATEGORY_BROAD;
}

// Callback specifically deployed when Groq Cloud answers
static void ai_router_cloud_callback(const char *response, bool success, void *user_data) {
    if (success && response) {
        ESP_LOGI(TAG, "Groq AI Generated Response: %s", response);
        // Integrate with your UI or MQTT publisher here
        // Example: mqtt_publish_response(response);
    } else {
        ESP_LOGW(TAG, "Groq Cloud Request Failed or Timeout");
    }
}

void ai_router_init(void) {
    ESP_LOGI(TAG, "AI Router ready");
    ai_client_init(); 
    aq_voice_engine_init();
}

bool ai_router_ask(const char *question, char *answer, size_t answer_size) {
    if (!question || !answer || !answer_size) return false;
    
    extern bool wifi_is_connected;
    g_cloud_available = wifi_is_connected;
    
    question_category_t category = classify_question(question);
    bool success = false;
    
    ESP_LOGI(TAG, "Q: '%s' | Cat: %d | Cloud: %d", question, category, g_cloud_available);
    
    // Always attempt local engine processing first
    success = aq_voice_process(question, answer, answer_size, false);

    // If it's a broad chat or analysis question AND Wi-Fi is connected, fall back to Groq
    if (g_cloud_available && g_source != AI_SOURCE_LOCAL && (category == CATEGORY_ANALYSIS || category == CATEGORY_BROAD)) {
        if (ai_client_ask(question, ai_router_cloud_callback, NULL)) {
            ESP_LOGI(TAG, "Context forwarded to Groq Cloud API...");
            snprintf(answer, answer_size, "OPRUSS AI is thinking..."); // Immediate UI placeholder
            success = true;
        }
    }
    
    return success;
}

bool ai_router_ask_async(
    const char *question,
    ai_router_callback_t callback,
    void *user_data
)
{
    if (!question || !callback)
        return false;

    extern bool wifi_is_connected;

    g_cloud_available = wifi_is_connected;

    /*
     * ONLINE:
     * Send directly to the Groq worker.
     *
     * ai_client.c will build the complete context:
     * - realtime sensor data
     * - fan status
     * - today's SD data
     * - yesterday's SD data
     * - day comparison
     */
    if (g_cloud_available &&
        g_source != AI_SOURCE_LOCAL)
    {
        if (ai_client_ask(
                question,
                callback,
                user_data))
        {
            ESP_LOGI(
                TAG,
                "Screen 4 request forwarded to Groq"
            );

            return true;
        }

        callback(
            "Unable to send request to Cloud AI.",
            false,
            user_data
        );

        return false;
    }

    /*
     * OFFLINE:
     * Use the on-board/local AI.
     *
     * This path remains synchronous because
     * aq_voice_process() is synchronous.
     */
    char local_response[512] = {0};

    bool success = aq_voice_process(
        question,
        local_response,
        sizeof(local_response),
        false
    );

    callback(
        local_response,
        success,
        user_data
    );

    return success;
}

void ai_router_set_source(ai_source_t source) { g_source = source; }
ai_source_t ai_router_get_source(void) { return g_source; }
bool ai_router_is_cloud_available(void) { return g_cloud_available; }

const char *ai_router_status(void) {
    if (g_cloud_available)
        return (g_source == AI_SOURCE_CLOUD) ? "Cloud AI" : "Hybrid AI (Online)";
    return "Local AI (Offline)";
}