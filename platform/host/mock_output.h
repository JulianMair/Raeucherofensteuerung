/*
 * Mock des Heizausgangs für Host-Tests. Er merkt sich den zuletzt
 * gültig gesetzten Tastgrad und zählt alle Aufrufe.
 */
#ifndef MOCK_OUTPUT_H
#define MOCK_OUTPUT_H

#include "rof_output.h"

typedef struct {
    float duty;         /* zuletzt gültig gesetzter Tastgrad, nach init 0.0 */
    unsigned set_calls; /* alle Aufrufe, auch abgelehnte */
} mock_output_t;

void mock_output_init(mock_output_t *mock);
rof_output_t mock_output_interface(mock_output_t *mock);

#endif /* MOCK_OUTPUT_H */
