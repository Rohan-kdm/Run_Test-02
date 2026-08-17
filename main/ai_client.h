#ifndef AI_CLIENT_H
#define AI_CLIENT_H

#include <stdbool.h>
#include <stddef.h>

// Callback when AI response is ready
typedef void (*ai_client_callback_t)(const char *response, bool success, void *user_data);

// Initialize AI client
void ai_client_init(void);

// Send question to cloud AI (non-blocking)
// Returns true if request was queued
bool ai_client_ask(const char *question, ai_client_callback_t callback, void *user_data);

// Check if a request is in progress
bool ai_client_is_busy(void);

// Get last error message
const char *ai_client_get_error(void);

#endif