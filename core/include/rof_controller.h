/*
 * Regelzyklus (SAF-01, SAF-02, ARC-02, ARC-05, ARC-06, E-026).
 *
 * Verbindet alle Module von core. Ein Aufruf von rof_controller_cycle()
 * ist ein Regelzyklus, immer in dieser Reihenfolge:
 *   1. Befehle aus der Befehlsquelle holen, prüfen, ausführen
 *   2. Sensorverwaltung aktualisieren
 *   3. Sicherheitslogik auswerten (vor dem Regler, SAF-01)
 *   4. Zustandsautomat weiterschalten
 *   5. PI-Regler, nur in AUFHEIZEN und HALTEN
 *   6. rof_safety_limit_duty(): Die Sicherheitslogik hat das letzte Wort (SAF-02)
 *   7. Tastgrad an den Ausgang
 * Wie oft der Zyklus läuft, legt der Aufrufer fest (Firmware: jede Sekunde).
 */
#ifndef ROF_CONTROLLER_H
#define ROF_CONTROLLER_H

#include "rof_clock.h"
#include "rof_command.h"
#include "rof_log.h"
#include "rof_output.h"
#include "rof_pi.h"
#include "rof_safety.h"
#include "rof_sensor_registry.h"
#include "rof_state_machine.h"
#include "rof_status.h"
#include "rof_storage.h"
#include "rof_temp_sensor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ROF_CONTROLLER_DEFAULT_MAX_COMMANDS 4u

typedef struct {
    unsigned max_commands_per_cycle; /* ARC-09: mehr wartet auf den nächsten Zyklus */
    rof_safety_config_t safety;
    rof_fsm_config_t fsm;
    rof_pi_config_t pi;
} rof_controller_config_t;

/* Alle Interfaces, mit denen der Regelzyklus spricht (ARC-02). Die Zeiger
 * müssen gültig bleiben, solange der Regelzyklus läuft. */
typedef struct {
    const rof_temp_sensor_t *const *drivers;
    size_t driver_count;
    const rof_storage_t *storage;
    const rof_output_t *output;
    const rof_clock_t *clk;
    const rof_log_t *logger;
    const rof_command_source_t *commands;
} rof_controller_io_t;

typedef struct {
    rof_controller_config_t config;
    rof_controller_io_t io;
    rof_sensor_registry_t registry;
    rof_safety_t safety;
    rof_fsm_t fsm;
    rof_pi_t pi;
    float applied_duty;  /* zuletzt an den Ausgang gegebener Tastgrad */
    bool was_heating;    /* Ausgang war im letzten Zyklus freigegeben */
    bool has_last_cycle;
    uint64_t last_cycle_ms;
} rof_controller_t;

/* Die vereinbarten Standardwerte aller Module (E-016, E-021, E-024). */
rof_controller_config_t rof_controller_default_config(void);

/* Setzt alle Module auf und schaltet den Ausgang ausdrücklich auf 0
 * (SAF-10). ROF_ERR_INVALID_ARG: ein Interface fehlt oder eine
 * Konfiguration ist nicht stimmig. */
rof_status_t rof_controller_init(rof_controller_t *ctrl, const rof_controller_config_t *config,
                                 const rof_controller_io_t *io);

/* Ein Regelzyklus in der oben beschriebenen Reihenfolge. */
void rof_controller_cycle(rof_controller_t *ctrl);

#endif /* ROF_CONTROLLER_H */
