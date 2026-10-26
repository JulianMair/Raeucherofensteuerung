/*
 * Fake-Uhr für Host-Tests. Sie steht still, bis der Test sie vorstellt.
 * So laufen Stunden simulierter Zeit in Millisekunden echter Zeit.
 */
#ifndef FAKE_CLOCK_H
#define FAKE_CLOCK_H

#include "rof_clock.h"

#include <stdint.h>

typedef struct {
    uint64_t now_ms;
} fake_clock_t;

void fake_clock_init(fake_clock_t *fake, uint64_t start_ms);
void fake_clock_advance(fake_clock_t *fake, uint64_t delta_ms);

/* Liefert das Interface, das core übergeben wird. */
rof_clock_t fake_clock_interface(fake_clock_t *fake);

#endif /* FAKE_CLOCK_H */
