#include "rof_forecast.h"

#include <math.h>
#include <string.h>

void rof_forecast_init(rof_forecast_t *fc)
{
    memset(fc, 0, sizeof(*fc));
}

void rof_forecast_clear(rof_forecast_t *fc)
{
    fc->count = 0;
    fc->next = 0;
}

/* Index des i-ten Punkts, gezählt ab dem ältesten */
static size_t sample_index(const rof_forecast_t *fc, size_t i)
{
    return (fc->next + ROF_FORECAST_WINDOW - fc->count + i) % ROF_FORECAST_WINDOW;
}

bool rof_forecast_add(rof_forecast_t *fc, uint64_t now_ms, float chamber_c, float probe_c)
{
    if (fc->count > 0) {
        const uint64_t last_ms = fc->time_ms[sample_index(fc, fc->count - 1)];
        if (now_ms - last_ms < ROF_FORECAST_SAMPLE_MS) {
            return false;
        }
    }
    fc->time_ms[fc->next] = now_ms;
    fc->chamber_c[fc->next] = chamber_c;
    fc->probe_c[fc->next] = probe_c;
    fc->next = (fc->next + 1) % ROF_FORECAST_WINDOW;
    if (fc->count < ROF_FORECAST_WINDOW) {
        fc->count++;
    }
    return true;
}

static float mean_chamber(const rof_forecast_t *fc)
{
    float sum = 0.0f;
    for (size_t i = 0; i < fc->count; i++) {
        sum += fc->chamber_c[sample_index(fc, i)];
    }
    return sum / (float)fc->count;
}

bool rof_forecast_time_constant(const rof_forecast_t *fc, float *tau_s)
{
    if (fc->count < 2) {
        return false;
    }
    const float chamber_c = mean_chamber(fc);
    const uint64_t first_ms = fc->time_ms[sample_index(fc, 0)];

    /* Punkte (t, y) mit t in Sekunden ab dem ältesten und y = ln(T_g - T_k) */
    float t[ROF_FORECAST_WINDOW];
    float y[ROF_FORECAST_WINDOW];
    float mean_t = 0.0f;
    float mean_y = 0.0f;
    for (size_t i = 0; i < fc->count; i++) {
        const size_t index = sample_index(fc, i);
        const float gap_c = chamber_c - fc->probe_c[index];
        if (!(gap_c > 0.0f)) {
            return false; /* Kern nicht unter dem Garraum, auch NaN */
        }
        t[i] = (float)(fc->time_ms[index] - first_ms) / 1000.0f;
        y[i] = logf(gap_c);
        mean_t += t[i];
        mean_y += y[i];
    }
    mean_t /= (float)fc->count;
    mean_y /= (float)fc->count;

    /* Steigung der Regressionsgeraden: Kovarianz durch Varianz von t */
    float covariance = 0.0f;
    float variance = 0.0f;
    for (size_t i = 0; i < fc->count; i++) {
        covariance += (t[i] - mean_t) * (y[i] - mean_y);
        variance += (t[i] - mean_t) * (t[i] - mean_t);
    }
    if (!(variance > 0.0f)) {
        return false;
    }
    const float slope = covariance / variance;
    if (!(slope < 0.0f)) {
        return false; /* der Abstand schrumpft nicht: keine Annäherung */
    }
    *tau_s = -1.0f / slope;
    return true;
}

static rof_forecast_result_t result_of(rof_forecast_status_t status)
{
    return (rof_forecast_result_t){ .status = status, .remaining_s = 0 };
}

/* Restzeit nach dem Modell: tau * ln((T_g - T_k) / (T_g - T_ziel)) */
static rof_forecast_result_t remaining_time(const rof_forecast_t *fc, const rof_forecast_input_t *in)
{
    const float chamber_c = mean_chamber(fc);
    const float gap_target_c = chamber_c - in->target_c;
    float tau_s = 0.0f;
    if (!(gap_target_c >= ROF_FORECAST_MIN_GAP_C) || !rof_forecast_time_constant(fc, &tau_s)) {
        return result_of(ROF_FORECAST_UNREACHABLE);
    }
    /* Kern unter dem Ziel und Ziel unter dem Garraum: Der Quotient ist größer 1 */
    const float remaining_s = tau_s * logf((chamber_c - in->probe_c) / gap_target_c);
    if (!(remaining_s <= (float)ROF_FORECAST_MAX_REMAINING_S)) {
        return result_of(ROF_FORECAST_UNREACHABLE);
    }
    return (rof_forecast_result_t){ .status = ROF_FORECAST_OK, .remaining_s = (uint32_t)(remaining_s + 0.5f) };
}

rof_forecast_result_t rof_forecast_update(rof_forecast_t *fc, const rof_forecast_input_t *in, uint64_t now_ms)
{
    if (!in->probe_ok || !in->chamber_ok) {
        rof_forecast_clear(fc);
        return result_of(ROF_FORECAST_UNAVAILABLE);
    }
    /* Gelernt wird nur in HALTEN, auch ohne Ziel */
    if (in->holding) {
        (void)rof_forecast_add(fc, now_ms, in->chamber_c, in->probe_c);
    } else {
        rof_forecast_clear(fc);
    }
    if (!in->has_target) {
        return result_of(ROF_FORECAST_NO_TARGET);
    }
    if (in->probe_c >= in->target_c) {
        return result_of(ROF_FORECAST_REACHED);
    }
    if (!in->holding) {
        return result_of(ROF_FORECAST_WAITING);
    }
    if (fc->count < ROF_FORECAST_MIN_SAMPLES) {
        return result_of(ROF_FORECAST_LEARNING);
    }
    return remaining_time(fc, in);
}
