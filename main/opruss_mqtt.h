#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdbool.h>

// Initialize MQTT client and connect to broker
void mqtt_client_init(void);

// Publish sensor data to MQTT topic
void mqtt_publish_sensor_data(
    int aqi,
    float pm25,
    float pm10,
    float temp,
    float humidity,
    float tvoc
);

// Update current fan status
void mqtt_set_fan_status(
    const char *mode,
    const char *speed,
    int rpm
);

// Publish response to MQTT
void mqtt_publish_response(const char *response);

// Check if MQTT is connected
bool mqtt_is_connected(void);

// Get device ID
const char *mqtt_get_device_id(void);

#endif