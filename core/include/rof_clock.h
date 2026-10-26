/*
 * Uhr-Interface (ARC-02, ARC-03). core liest die Zeit ausschließlich hierüber.
 *
 * Aufbau wie bei allen Interfaces: ctx zeigt auf den Zustand der
 * Implementierung und wird an jede Funktion durchgereicht, ähnlich wie
 * `this` bei einer Methode. core kennt nur dieses Struct, nie die
 * Implementierung dahinter.
 */
#ifndef ROF_CLOCK_H
#define ROF_CLOCK_H

#include <stdint.h>

typedef struct {
    void *ctx;
    /* Millisekunden seit dem Start, monoton steigend. 64 Bit laufen praktisch nie über. */
    uint64_t (*now_ms)(void *ctx);
} rof_clock_t;

static inline uint64_t rof_clock_now_ms(const rof_clock_t *clk)
{
    return clk->now_ms(clk->ctx);
}

#endif /* ROF_CLOCK_H */
