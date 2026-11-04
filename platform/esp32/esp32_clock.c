#include "esp32_clock.h"

#include "esp_timer.h"

static uint64_t clock_now_ms(void *ctx)
{
    (void)ctx;
    /* esp_timer_get_time() zählt Mikrosekunden seit dem Start */
    return (uint64_t)esp_timer_get_time() / 1000u;
}

rof_clock_t esp32_clock_interface(void)
{
    return (rof_clock_t){ .ctx = NULL, .now_ms = clock_now_ms };
}
