#ifndef AIR_QUALITY_MODEL_H
#define AIR_QUALITY_MODEL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * NORMALIZATION CONFIGURATION
 *
 * These are model parameters.
 *
 * IMPORTANT:
 * PM2.5 / PM10 / TVOC limits are currently configurable
 * engineering values. We can finalize them later according
 * to the standard selected for OPRUSS.
 * ============================================================ */

/* PM2.5 */
#define AQ_PM25_ACCEPTABLE       15.0f
#define AQ_PM25_CRITICAL         35.0f

/* PM10 */
#define AQ_PM10_ACCEPTABLE       45.0f
#define AQ_PM10_CRITICAL         100.0f

/* TVOC */
#define AQ_TVOC_ACCEPTABLE       250.0f
#define AQ_TVOC_CRITICAL         1000.0f


/* ============================================================
 * COMFORT PARAMETERS
 * ============================================================ */

#define AQ_TEMP_OPT              28.0f
#define AQ_TEMP_DEVIATION        10.0f

#define AQ_HUM_OPT               50.0f
#define AQ_HUM_DEVIATION         30.0f


/* ============================================================
 * CAQI WEIGHTS
 * ============================================================ */

#define AQ_W_PM25                0.40f
#define AQ_W_PM10                0.35f
#define AQ_W_TVOC                0.15f
#define AQ_W_TEMP                0.05f
#define AQ_W_HUM                 0.05f


/* ============================================================
 * NORMALIZED AIR DATA
 *
 * All valid normalized parameters are in [0,1].
 *
 * PM10_N = -1 means PM10 is unavailable.
 * ============================================================ */

typedef struct
{
    float pm25_n;
    float pm10_n;
    float tvoc_n;

    float temp_n;
    float humidity_n;

    float aqi_n;

    bool valid;

} normalized_air_data_t;


/* ============================================================
 * NORMALIZE RAW SENSOR DATA
 * ============================================================ */

bool air_quality_normalize(
    float pm25,
    float pm10,
    float tvoc,
    float temperature,
    float humidity,
    int aqi,
    normalized_air_data_t *out
);


/* ============================================================
 * CAQI
 *
 * Only use this when all required sensor data is valid.
 * Fan controller will be implemented separately later.
 * ============================================================ */

bool air_quality_calculate_caqi(
    const normalized_air_data_t *data,
    float *caqi
);


/* ============================================================
 * UTILITY
 * ============================================================ */

float air_quality_clamp01(float value);

#ifdef __cplusplus
}
#endif

#endif