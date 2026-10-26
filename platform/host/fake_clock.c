#include "fake_clock.h"

static uint64_t fake_clock_now_ms(void *ctx)
{
    const fake_clock_t *fake = ctx;
    return fake->now_ms;
}

void fake_clock_init(fake_clock_t *fake, uint64_t start_ms)
{
    fake->now_ms = start_ms;
}

void fake_clock_advance(fake_clock_t *fake, uint64_t delta_ms)
{
    fake->now_ms += delta_ms;
}

rof_clock_t fake_clock_interface(fake_clock_t *fake)
{
    rof_clock_t clk = { .ctx = fake, .now_ms = fake_clock_now_ms };
    return clk;
}
