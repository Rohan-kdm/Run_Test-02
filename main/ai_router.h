#ifndef AI_ROUTER_H
#define AI_ROUTER_H

#include <stdbool.h>
#include "aq_voice_engine.h"

// AI source types
typedef enum {
    AI_SOURCE_LOCAL,    // Use offline engine (fast, no internet)
    AI_SOURCE_CLOUD,    // Use Vercel/Groq (broad knowledge, needs WiFi)
    AI_SOURCE_HYBRID    // Try cloud first, fallback to local
} ai_source_t;

typedef void (*ai_router_callback_t)(
    const char *response,
    bool success,
    void *user_data
);

bool ai_router_ask_async(
    const char *question,
    ai_router_callback_t callback,
    void *user_data
);

// Initialize AI router
void ai_router_init(void);

// Process question - auto-routes to best source
// Returns true if processed successfully
bool ai_router_ask(const char *question, char *answer, size_t answer_size);

// Set preferred AI source
void ai_router_set_source(ai_source_t source);

// Get current source
ai_source_t ai_router_get_source(void);

// Check if cloud AI is available
bool ai_router_is_cloud_available(void);

// Get router status string
const char *ai_router_status(void);

#endif