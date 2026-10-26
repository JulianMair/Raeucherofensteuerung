/*
 * Log-Mock für Host-Tests. Er zählt die Meldungen je Stufe und kopiert
 * die letzte Meldung, damit Tests prüfen können, was core gemeldet hat.
 */
#ifndef MOCK_LOG_H
#define MOCK_LOG_H

#include "rof_log.h"

#define MOCK_LOG_MSG_MAX 128

typedef struct {
    unsigned count[ROF_LOG_LEVEL_COUNT];
    rof_log_level_t last_level;
    char last_msg[MOCK_LOG_MSG_MAX]; /* längere Meldungen werden gekürzt */
} mock_log_t;

void mock_log_init(mock_log_t *mock);
rof_log_t mock_log_interface(mock_log_t *mock);

#endif /* MOCK_LOG_H */
