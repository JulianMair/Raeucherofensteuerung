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
/* So viele Befehlsergebnisse bleiben abrufbar: zwei volle Zyklen (E-026) */
#define ROF_CONTROLLER_RECENT_RESULTS 8u

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
    /* Ergebnisse der zuletzt bearbeiteten Befehle als Ringpuffer (E-026) */
    rof_command_record_t recent[ROF_CONTROLLER_RECENT_RESULTS];
    size_t recent_count;
    size_t recent_next;
} rof_controller_t;

/* ---- Snapshot (ARC-07, E-026) -------------------------------------------
 * Eine vollständige Kopie des Zustands für die Außenwelt. Sie enthält keine
 * Zeiger in den Regelzyklus hinein; nur type zeigt auf den konstanten
 * Text des Treibers. In der Firmware kopiert der Regeltask sie unter einem
 * Mutex, und der Webserver liest nur diese Kopie (M7). */

typedef struct {
    rof_role_status_t status;
    float temp_c; /* nur bei ROF_ROLE_STATUS_OK gültig */
} rof_role_snapshot_t;

typedef struct {
    rof_sensor_id_t id;
    const char *type;
    rof_role_t role;
    float offset_c;
    bool present;
    bool duplicate;
    rof_status_t read_status;
    float temp_c; /* letzter gültiger Wert inklusive Offset */
} rof_channel_snapshot_t;

typedef struct {
    uint64_t time_ms; /* Zeitpunkt des letzten Zyklus */
    rof_state_t state;
    bool has_setpoint;
    float setpoint_c;
    float duty; /* zuletzt an den Ausgang gegebener Tastgrad */
    rof_fault_t latched_fault;
    rof_fault_t active_fault;
    rof_role_snapshot_t roles[ROF_ROLE_COUNT]; /* Index ist die Rolle, [0] ungenutzt */
    size_t channel_count;
    rof_channel_snapshot_t channels[ROF_SENSOR_CHANNELS_MAX];
    bool sensor_overflow;
    size_t recent_count;
    rof_command_record_t recent[ROF_CONTROLLER_RECENT_RESULTS]; /* neuestes zuerst */
} rof_snapshot_t;

/* Die vereinbarten Standardwerte aller Module (E-016, E-021, E-024). */
rof_controller_config_t rof_controller_default_config(void);

/* Setzt alle Module auf und schaltet den Ausgang ausdrücklich auf 0
 * (SAF-10). ROF_ERR_INVALID_ARG: ein Interface fehlt oder eine
 * Konfiguration ist nicht stimmig. */
rof_status_t rof_controller_init(rof_controller_t *ctrl, const rof_controller_config_t *config,
                                 const rof_controller_io_t *io);

/* Ein Regelzyklus in der oben beschriebenen Reihenfolge. */
void rof_controller_cycle(rof_controller_t *ctrl);

/* Schreibt den aktuellen Zustand nach *snapshot. */
void rof_controller_snapshot(const rof_controller_t *ctrl, rof_snapshot_t *snapshot);

/* Sucht das Ergebnis des Befehls mit dieser Nummer. false, wenn es (noch)
 * nicht oder nicht mehr im Snapshot steht. */
bool rof_snapshot_command_result(const rof_snapshot_t *snapshot, uint32_t seq,
                                 rof_cmd_result_t *result);

#endif /* ROF_CONTROLLER_H */
