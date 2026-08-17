#ifndef SD_CONTEXT_H
#define SD_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>

// Get context from SD card for AI queries
// Extracts relevant recent data and summary statistics
bool sd_context_get(char *context, size_t context_size);

// Get summary of today's data
bool sd_context_get_today_summary(char *summary, size_t summary_size);

// Get comparison between two days
bool sd_context_compare_days(char *comparison, size_t comparison_size);

// Get the latest N readings as CSV
bool sd_context_get_latest_readings(int count, char *csv_data, size_t csv_size);

// Get min/max/average for a specific parameter
bool sd_context_get_stats(const char *parameter, char *stats, size_t stats_size);

#endif