#ifndef AQ_VOICE_ENGINE_H
#define AQ_VOICE_ENGINE_H

#include <stdbool.h>
#include <stddef.h>

// Voice command types
typedef enum {
    VOICE_CMD_WORST_READING,
    VOICE_CMD_TREND,
    VOICE_CMD_RECOMMENDATION,
    VOICE_CMD_HEALTH_RISK,
    VOICE_CMD_COMPARE_DAYS,
    VOICE_CMD_CURRENT_AQI,
    VOICE_CMD_PREDICT_AQI,  
    VOICE_CMD_FAN_STATUS,
    VOICE_CMD_FAN_ON,
    VOICE_CMD_FAN_OFF,
    VOICE_CMD_EXPORT,
    VOICE_CMD_UNKNOWN
} voice_command_t;

// Initialize voice engine
void aq_voice_engine_init(void);

// Process voice input text and generate response
bool aq_voice_process(const char *voice_text, char *response, size_t response_size, bool from_mqtt);

#endif