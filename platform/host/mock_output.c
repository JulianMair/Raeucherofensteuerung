#include "mock_output.h"

static rof_status_t mock_set_duty(void *ctx, float duty)
{
    mock_output_t *mock = ctx;
    mock->set_calls++;
    /* So formuliert, dass auch NaN abgelehnt wird: Jeder Vergleich mit NaN ist falsch. */
    if (!(duty >= 0.0f && duty <= 1.0f)) {
        return ROF_ERR_INVALID_ARG;
    }
    mock->duty = duty;
    return ROF_OK;
}

void mock_output_init(mock_output_t *mock)
{
    mock->duty = 0.0f;
    mock->set_calls = 0;
}

rof_output_t mock_output_interface(mock_output_t *mock)
{
    rof_output_t output = { .ctx = mock, .set_duty = mock_set_duty };
    return output;
}
