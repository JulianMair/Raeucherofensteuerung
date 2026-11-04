/*
 * Log für core auf dem ESP32 (ARC-02): leitet jede Meldung an das Log von
 * ESP-IDF weiter, mit dem Tag "core". Sichtbar im seriellen Monitor.
 */
#ifndef ESP32_LOG_H
#define ESP32_LOG_H

#include "rof_log.h"

rof_log_t esp32_log_interface(void);

#endif /* ESP32_LOG_H */
