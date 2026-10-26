/*
 * Ausgang-Interface (ARC-02): setzt den Tastgrad des Heizausgangs.
 * Im Testaufbau steckt eine LED dahinter, kein Heizelement.
 */
#ifndef ROF_OUTPUT_H
#define ROF_OUTPUT_H

#include "rof_status.h"

typedef struct {
    void *ctx;
    /* duty von 0.0 (aus) bis 1.0 (dauernd an).
     * ROF_ERR_INVALID_ARG: duty außerhalb dieses Bereichs oder NaN,
     * der Ausgang bleibt dann unverändert. */
    rof_status_t (*set_duty)(void *ctx, float duty);
} rof_output_t;

static inline rof_status_t rof_output_set_duty(const rof_output_t *output, float duty)
{
    return output->set_duty(output->ctx, duty);
}

#endif /* ROF_OUTPUT_H */
