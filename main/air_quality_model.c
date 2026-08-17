#include "air_quality_model.h"

#include <math.h>


/* ============================================================
 * CLAMP TO [0,1]
 * ============================================================ */

float air_quality_clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;

    if (value > 1.0f)
        return 1.0f;

    return value;
}


/* ============================================================
 * POLLUTANT NORMALIZATION
 *
 * 0 = acceptable or better
 * 1 = critical or worse
 * ============================================================ */

static float normalize_pollutant(
    float value,
    float acceptable,
    float critical)
{
    if (!isfinite(value))
        return -1.0f;

    if (value <= acceptable)
        return 0.0f;

    if (value >= critical)
        return 1.0f;

    if (critical <= acceptable)
        return -1.0f;

    return air_quality_clamp01(
        (value - acceptable) /
        (critical - acceptable)
    );
}


/* ============================================================
 * TEMPERATURE COMFORT DEVIATION
 *
 * T_N = |T - Topt| / DeltaT
 *
 * Topt = 28 C
 * DeltaT = 10 C
 * ============================================================ */

static float normalize_temperature(
    float temperature)
{
    if (!isfinite(temperature))
        return -1.0f;

    return air_quality_clamp01(
        fabsf(
            temperature -
            AQ_TEMP_OPT
        ) /
        AQ_TEMP_DEVIATION
    );
}


/* ============================================================
 * HUMIDITY COMFORT DEVIATION
 *
 * RH_N = |RH - RHopt| / DeltaRH
 *
 * RHopt = 50 %
 * DeltaRH = 30 %
 * ============================================================ */

static float normalize_humidity(
    float humidity)
{
    if (!isfinite(humidity))
        return -1.0f;

    return air_quality_clamp01(
        fabsf(
            humidity -
            AQ_HUM_OPT
        ) /
        AQ_HUM_DEVIATION
    );
}


/* ============================================================
 * AQI NORMALIZATION
 *
 * AQI is normalized against 500.
 *
 * 0 AQI   -> 0.0
 * 250 AQI -> 0.5
 * 500 AQI -> 1.0
 * ============================================================ */

static float normalize_aqi(int aqi)
{
    if (aqi < 0)
        return -1.0f;

    return air_quality_clamp01(
        ((float)aqi) /
        500.0f
    );
}


/* ============================================================
 * MAIN NORMALIZATION FUNCTION
 * ============================================================ */

bool air_quality_normalize(
    float pm25,
    float pm10,
    float tvoc,
    float temperature,
    float humidity,
    int aqi,
    normalized_air_data_t *out)
{
    if (out == NULL)
        return false;

    /*
     * Clear output first.
     */
    *out = (normalized_air_data_t){0};

    /*
     * Check finite values.
     */
    if (!isfinite(pm25) ||
        !isfinite(pm10) ||
        !isfinite(tvoc) ||
        !isfinite(temperature) ||
        !isfinite(humidity))
    {
        out->valid = false;
        return false;
    }


    /* --------------------------------------------------------
     * PM2.5
     * -------------------------------------------------------- */

    out->pm25_n =
        normalize_pollutant(
            pm25,
            AQ_PM25_ACCEPTABLE,
            AQ_PM25_CRITICAL
        );


    /* --------------------------------------------------------
     * PM10
     *
     * IMPORTANT:
     *
     * Your firmware uses PM10 = -1 when unavailable.
     *
     * -1 remains -1.
     *
     * We NEVER convert unavailable PM10 into 0.
     * -------------------------------------------------------- */

    if (pm10 < 0.0f)
    {
        out->pm10_n = -1.0f;
    }
    else
    {
        out->pm10_n =
            normalize_pollutant(
                pm10,
                AQ_PM10_ACCEPTABLE,
                AQ_PM10_CRITICAL
            );
    }


    /* --------------------------------------------------------
     * TVOC
     * -------------------------------------------------------- */

    out->tvoc_n =
        normalize_pollutant(
            tvoc,
            AQ_TVOC_ACCEPTABLE,
            AQ_TVOC_CRITICAL
        );


    /* --------------------------------------------------------
     * TEMPERATURE
     * -------------------------------------------------------- */

    out->temp_n =
        normalize_temperature(
            temperature
        );


    /* --------------------------------------------------------
     * HUMIDITY
     * -------------------------------------------------------- */

    out->humidity_n =
        normalize_humidity(
            humidity
        );


    /* --------------------------------------------------------
     * AQI
     * -------------------------------------------------------- */

    out->aqi_n =
        normalize_aqi(
            aqi
        );


    /*
     * At this stage the normalization itself succeeded.
     *
     * PM10 may still be unavailable (-1), which is handled
     * separately by CAQI / future prediction code.
     */

    out->valid = true;

    return true;
}


/* ============================================================
 * CAQI CALCULATION
 *
 * CAQI =
 *
 * 0.40 PM2.5_N
 * 0.35 PM10_N
 * 0.15 TVOC_N
 * 0.05 T_N
 * 0.05 RH_N
 *
 * For now this is ONLY calculated when PM10 is available.
 *
 * No fan control is performed here.
 * ============================================================ */

bool air_quality_calculate_caqi(
    const normalized_air_data_t *data,
    float *caqi)
{
    if (data == NULL ||
        caqi == NULL)
    {
        return false;
    }

    if (!data->valid)
        return false;

    /*
     * PM10 unavailable.
     *
     * Don't silently pretend it is zero.
     */
    if (data->pm10_n < 0.0f)
        return false;

    /*
     * Other normalized values must be valid.
     */
    if (data->pm25_n < 0.0f ||
        data->tvoc_n < 0.0f ||
        data->temp_n < 0.0f ||
        data->humidity_n < 0.0f)
    {
        return false;
    }


    *caqi =
          AQ_W_PM25 *
          data->pm25_n

        + AQ_W_PM10 *
          data->pm10_n

        + AQ_W_TVOC *
          data->tvoc_n

        + AQ_W_TEMP *
          data->temp_n

        + AQ_W_HUM *
          data->humidity_n;


    *caqi =
        air_quality_clamp01(
            *caqi
        );

    return true;
}