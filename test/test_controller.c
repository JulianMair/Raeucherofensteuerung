/*
 * Tests des Regelzyklus core/control/rof_controller (SAF-01, SAF-02, SAF-10,
 * ARC-02, ARC-05, ARC-06). Hier mit Mocks; über Stunden mit dem Simulator
 * prüft test_integration.c.
 */
#include "unity.h"

#include "fake_clock.h"
#include "mock_command_queue.h"
#include "mock_log.h"
#include "mock_output.h"
#include "mock_storage.h"
#include "mock_temp_sensor.h"
#include "rof_controller.h"

#include <string.h>

#define CHAMBER_ID 0x28CC000000000001ULL
#define PROBE_ID 0x28CC000000000002ULL

static mock_temp_sensor_t bus_mock;
static rof_temp_sensor_t bus;
static const rof_temp_sensor_t *drivers[1];
static mock_storage_t flash_mock;
static rof_storage_t flash;
static mock_output_t heater_mock;
static rof_output_t heater;
static fake_clock_t clock_fake;
static rof_clock_t clk_if;
static mock_log_t log_mock;
static rof_log_t logger_if;
static mock_command_queue_t queue_mock;
static rof_command_source_t commands;

static rof_controller_config_t config;
static rof_controller_io_t io;
static rof_controller_t ctrl;

void setUp(void)
{
    mock_temp_sensor_init(&bus_mock);
    bus = mock_temp_sensor_interface(&bus_mock);
    drivers[0] = &bus;
    mock_storage_init(&flash_mock);
    flash = mock_storage_interface(&flash_mock);
    mock_output_init(&heater_mock);
    heater = mock_output_interface(&heater_mock);
    fake_clock_init(&clock_fake, 0);
    clk_if = fake_clock_interface(&clock_fake);
    mock_log_init(&log_mock);
    logger_if = mock_log_interface(&log_mock);
    mock_command_queue_init(&queue_mock);
    commands = mock_command_queue_source(&queue_mock);

    mock_temp_sensor_plug(&bus_mock, CHAMBER_ID, "DS18B20", 20.0f);
    mock_temp_sensor_plug(&bus_mock, PROBE_ID, "DS18B20", 20.0f);

    config = rof_controller_default_config();
    io = (rof_controller_io_t){
        .drivers = drivers,
        .driver_count = 1,
        .storage = &flash,
        .output = &heater,
        .clk = &clk_if,
        .logger = &logger_if,
        .commands = &commands,
    };
    TEST_ASSERT_EQUAL(ROF_OK, rof_controller_init(&ctrl, &config, &io));
}

void tearDown(void) {}

/* Ein Regelzyklus eine Sekunde nach dem letzten */
static void cycle(void)
{
    fake_clock_advance(&clock_fake, 1000);
    rof_controller_cycle(&ctrl);
}

static void set_chamber(float temp_c)
{
    mock_temp_sensor_set_temp(&bus_mock, CHAMBER_ID, temp_c);
}

/* Sensoren erkennen und den Garraum zuordnen, wie es später ein Befehl tut.
 * Im ersten Zyklus fehlt die Zuordnung noch, der Automat steht deshalb in
 * FEHLER (E-020). Ein Zyklus nach dem Zuordnen ist er wieder BEREIT. */
static void assign_chamber(void)
{
    cycle();
    TEST_ASSERT_EQUAL(ROF_STATE_FAULT, rof_fsm_state(&ctrl.fsm));
    TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_assign(&ctrl.registry, CHAMBER_ID, ROF_ROLE_CHAMBER));
    cycle();
    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, rof_fsm_state(&ctrl.fsm));
}

static void start_heating(float setpoint_c)
{
    TEST_ASSERT_EQUAL(ROF_CMD_OK, rof_fsm_set_setpoint(&ctrl.fsm, &ctrl.safety, setpoint_c));
    TEST_ASSERT_EQUAL(ROF_CMD_OK, rof_fsm_start(&ctrl.fsm));
}

/* ---- Start -------------------------------------------------------------- */

/* SAF-10: nach init BEREIT, und der Ausgang wurde ausdrücklich auf 0 gesetzt */
static void test_init_turns_output_off_and_starts_idle(void)
{
    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, rof_fsm_state(&ctrl.fsm));
    TEST_ASSERT_EQUAL_UINT(1, heater_mock.set_calls);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, heater_mock.duty);
}

/* E-007: Interfaces und Konfiguration werden geprüft */
static void test_init_rejects_missing_interfaces_or_config(void)
{
    rof_controller_t other;
    rof_controller_io_t broken;
    rof_controller_config_t c;

    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_controller_init(NULL, &config, &io));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_controller_init(&other, NULL, &io));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_controller_init(&other, &config, NULL));

    broken = io;
    broken.output = NULL;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_controller_init(&other, &config, &broken));

    broken = io;
    broken.commands = NULL;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_controller_init(&other, &config, &broken));

    broken = io;
    broken.clk = NULL;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_controller_init(&other, &config, &broken));

    broken = io;
    broken.storage = NULL;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_controller_init(&other, &config, &broken));

    c = config;
    c.max_commands_per_cycle = 0;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_controller_init(&other, &c, &io));

    c = config;
    c.safety.plausible_min_c = c.safety.plausible_max_c;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_controller_init(&other, &c, &io));

    c = config;
    c.fsm.hold_band_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_controller_init(&other, &c, &io));

    c = config;
    c.pi.kp = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_controller_init(&other, &c, &io));
}

/* ---- Ablauf eines Zyklus ------------------------------------------------ */

/* Ohne Start bleibt der Ausgang aus */
static void test_idle_cycle_keeps_output_off(void)
{
    assign_chamber();

    cycle();

    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, rof_fsm_state(&ctrl.fsm));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, heater_mock.duty);
}

/* CTL-02: beim Heizen setzt der Zyklus den Tastgrad des PI-Reglers.
 * 0,075 * 5 °C + I-Anteil nach 1 s (0,075 * 5 / 250) = 0,3765 */
static void test_heating_cycle_sets_pi_duty(void)
{
    assign_chamber();
    set_chamber(75.0f);
    start_heating(80.0f);

    cycle();

    TEST_ASSERT_EQUAL(ROF_STATE_HEATING, rof_fsm_state(&ctrl.fsm));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3765f, heater_mock.duty);
}

/* Der I-Anteil rechnet mit der Zeit aus der Uhr: zwei Sekunden, zwei Schritte */
static void test_integral_uses_time_from_clock(void)
{
    assign_chamber();
    set_chamber(75.0f);
    start_heating(80.0f);
    cycle();

    fake_clock_advance(&clock_fake, 1000);
    cycle(); /* insgesamt 2 s seit dem letzten Zyklus */

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.375f + 3.0f * 0.0015f, heater_mock.duty);
}

static void test_holding_is_reached(void)
{
    assign_chamber();
    set_chamber(79.0f);
    start_heating(80.0f);

    cycle();

    TEST_ASSERT_EQUAL(ROF_STATE_HOLDING, rof_fsm_state(&ctrl.fsm));
    TEST_ASSERT_TRUE(heater_mock.duty > 0.0f);
}

/* SAF-01: die Sicherheitslogik läuft vor dem Regler. Fällt der Garraum
 * beim Heizen aus, ist der Ausgang noch im selben Zyklus 0. */
static void test_safety_acts_before_controller_in_same_cycle(void)
{
    assign_chamber();
    set_chamber(75.0f);
    start_heating(80.0f);
    cycle();
    TEST_ASSERT_TRUE(heater_mock.duty > 0.0f);

    mock_temp_sensor_unplug(&bus_mock, CHAMBER_ID);
    cycle();

    TEST_ASSERT_EQUAL(ROF_STATE_SAFE_STATE, rof_fsm_state(&ctrl.fsm));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, heater_mock.duty);
}

/* E-020: ohne Heizen führt derselbe Ausfall nur zu FEHLER. Das belegt, dass
 * der Zyklus der Sicherheitslogik richtig sagt, ob geheizt wird. */
static void test_unplugged_chamber_while_idle_is_a_fault(void)
{
    assign_chamber();

    mock_temp_sensor_unplug(&bus_mock, CHAMBER_ID);
    cycle();

    TEST_ASSERT_EQUAL(ROF_STATE_FAULT, rof_fsm_state(&ctrl.fsm));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, heater_mock.duty);
}

/* SAF-02: die Sicherheitslogik hat das letzte Wort über den Ausgang */
static void test_over_temperature_cuts_output(void)
{
    assign_chamber();
    set_chamber(75.0f);
    start_heating(80.0f);
    cycle();

    set_chamber(121.0f);
    cycle();

    TEST_ASSERT_EQUAL(ROF_STATE_SAFE_STATE, rof_fsm_state(&ctrl.fsm));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, heater_mock.duty);
}

static void test_stop_turns_output_off(void)
{
    assign_chamber();
    set_chamber(75.0f);
    start_heating(80.0f);
    cycle();

    rof_fsm_stop(&ctrl.fsm);
    cycle();

    TEST_ASSERT_EQUAL(ROF_STATE_COOLING, rof_fsm_state(&ctrl.fsm));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, heater_mock.duty);
}

/* CTL-02: bei jedem neuen Heizen beginnt der Regler mit leerem I-Anteil */
static void test_pi_restarts_when_heating_starts_again(void)
{
    assign_chamber();
    set_chamber(75.0f);
    start_heating(80.0f);
    for (int i = 0; i < 300; i++) {
        cycle();
    }
    TEST_ASSERT_TRUE(heater_mock.duty > 0.6f); /* I-Anteil aufgebaut */
    rof_fsm_stop(&ctrl.fsm);
    cycle();

    TEST_ASSERT_EQUAL(ROF_CMD_OK, rof_fsm_start(&ctrl.fsm));
    cycle();

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3765f, heater_mock.duty);
}

/* Lässt sich der Ausgang nicht setzen, wird das als Fehler geloggt */
static void test_output_failure_is_logged(void)
{
    assign_chamber();
    const unsigned errors_before = log_mock.count[ROF_LOG_ERROR];
    mock_output_set_status(&heater_mock, ROF_ERR_IO);

    cycle();

    TEST_ASSERT_EQUAL_UINT(errors_before + 1, log_mock.count[ROF_LOG_ERROR]);
}

/* SAF-10: Neustart mitten im Halten: wieder BEREIT, Ausgang 0, Rollen bleiben */
static void test_restart_while_holding_starts_idle_with_output_off(void)
{
    assign_chamber();
    set_chamber(79.0f);
    start_heating(80.0f);
    cycle();
    TEST_ASSERT_EQUAL(ROF_STATE_HOLDING, rof_fsm_state(&ctrl.fsm));

    TEST_ASSERT_EQUAL(ROF_OK, rof_controller_init(&ctrl, &config, &io));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, heater_mock.duty);
    cycle();

    float chamber_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, rof_fsm_state(&ctrl.fsm));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, heater_mock.duty);
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, rof_sensor_registry_role(&ctrl.registry, ROF_ROLE_CHAMBER, &chamber_c));
}

/* ---- Befehle (ARC-05, ARC-06, ARC-09) ----------------------------------- */

static void send(rof_command_t command)
{
    TEST_ASSERT_TRUE(mock_command_queue_push(&queue_mock, command));
}

static void send_simple(uint32_t seq, rof_command_type_t type)
{
    send((rof_command_t){ .seq = seq, .type = type });
}

static void send_setpoint(uint32_t seq, float setpoint_c)
{
    send((rof_command_t){ .seq = seq, .type = ROF_COMMAND_SET_SETPOINT, .value_c = setpoint_c });
}

static void send_assign(uint32_t seq, rof_sensor_id_t id, rof_role_t role)
{
    send((rof_command_t){ .seq = seq, .type = ROF_COMMAND_ASSIGN_ROLE, .sensor_id = id, .role = role });
}

static void send_offset(uint32_t seq, rof_sensor_id_t id, float offset_c)
{
    send((rof_command_t){ .seq = seq, .type = ROF_COMMAND_SET_OFFSET, .sensor_id = id, .value_c = offset_c });
}

/* Ergebnis des Befehls mit dieser Nummer. Scheitert, wenn es nicht vorliegt. */
static rof_cmd_result_t result_of(uint32_t seq)
{
    for (size_t i = 0; i < ctrl.recent_count; i++) {
        if (ctrl.recent[i].seq == seq) {
            return ctrl.recent[i].result;
        }
    }
    TEST_FAIL_MESSAGE("kein Ergebnis zu dieser Nummer");
    return ROF_CMD_OK;
}

static bool has_result(uint32_t seq)
{
    for (size_t i = 0; i < ctrl.recent_count; i++) {
        if (ctrl.recent[i].seq == seq) {
            return true;
        }
    }
    return false;
}

/* ARC-05: Befehle kommen über die Befehlsquelle und wirken im selben Zyklus */
static void test_commands_take_effect_in_the_cycle(void)
{
    assign_chamber();
    set_chamber(75.0f);
    send_setpoint(1, 80.0f);
    send_simple(2, ROF_COMMAND_START);

    cycle();

    TEST_ASSERT_EQUAL(ROF_CMD_OK, result_of(1));
    TEST_ASSERT_EQUAL(ROF_CMD_OK, result_of(2));
    TEST_ASSERT_EQUAL(ROF_STATE_HEATING, rof_fsm_state(&ctrl.fsm));
    TEST_ASSERT_TRUE(heater_mock.duty > 0.0f);
}

/* ARC-09: höchstens 4 Befehle je Zyklus, der Rest wartet */
static void test_at_most_four_commands_per_cycle(void)
{
    for (uint32_t seq = 1; seq <= 10; seq++) {
        send_setpoint(seq, 80.0f);
    }

    cycle();
    TEST_ASSERT_EQUAL_UINT(4, queue_mock.received);
    cycle();
    TEST_ASSERT_EQUAL_UINT(8, queue_mock.received);
    cycle();
    TEST_ASSERT_EQUAL_UINT(10, queue_mock.received);
}

/* SAF-09, ARC-06: ungültiger Sollwert abgelehnt mit Grund und gewarnt */
static void test_invalid_setpoint_is_rejected_with_reason(void)
{
    const unsigned warnings_before = log_mock.count[ROF_LOG_WARN];
    send_setpoint(1, 200.0f);

    cycle();

    TEST_ASSERT_EQUAL(ROF_CMD_SETPOINT_TOO_HIGH, result_of(1));
    TEST_ASSERT_EQUAL_UINT(warnings_before + 1, log_mock.count[ROF_LOG_WARN]);
}

/* E-022 über einen Befehl: Start ohne Sollwert */
static void test_start_without_setpoint_is_rejected(void)
{
    assign_chamber();
    send_simple(1, ROF_COMMAND_START);

    cycle();

    TEST_ASSERT_EQUAL(ROF_CMD_NO_SETPOINT, result_of(1));
    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, rof_fsm_state(&ctrl.fsm));
}

/* ARC-06: unbekannte Befehle werden verworfen, auch ein mit 0 gefüllter */
static void test_unknown_command_type_is_rejected(void)
{
    send_simple(1, (rof_command_type_t)0);
    send_simple(2, (rof_command_type_t)99);

    cycle();

    TEST_ASSERT_EQUAL(ROF_CMD_UNKNOWN_COMMAND, result_of(1));
    TEST_ASSERT_EQUAL(ROF_CMD_UNKNOWN_COMMAND, result_of(2));
}

/* SEN-02 über einen Befehl */
static void test_assign_role_via_command(void)
{
    cycle();
    send_assign(1, PROBE_ID, ROF_ROLE_PROBE_1);

    cycle();

    float probe_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_CMD_OK, result_of(1));
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, rof_sensor_registry_role(&ctrl.registry, ROF_ROLE_PROBE_1, &probe_c));
}

static void test_assign_with_unknown_sensor_or_role_is_rejected(void)
{
    cycle();
    send_assign(1, 0x1234, ROF_ROLE_PROBE_1);
    send_assign(2, PROBE_ID, (rof_role_t)99);

    cycle();

    TEST_ASSERT_EQUAL(ROF_CMD_UNKNOWN_SENSOR, result_of(1));
    TEST_ASSERT_EQUAL(ROF_CMD_INVALID_VALUE, result_of(2));
}

/* SEN-01 über einen Befehl: Offset setzen, außerhalb ±5 °C abgelehnt */
static void test_set_offset_via_command(void)
{
    cycle();
    send_offset(1, PROBE_ID, 1.5f);
    send_offset(2, PROBE_ID, 6.0f);

    cycle();

    TEST_ASSERT_EQUAL(ROF_CMD_OK, result_of(1));
    TEST_ASSERT_EQUAL(ROF_CMD_INVALID_VALUE, result_of(2));
}

/* E-022: Rollen und Offsets sind beim Heizen gesperrt */
static void test_sensor_settings_are_locked_while_heating(void)
{
    assign_chamber();
    set_chamber(75.0f);
    start_heating(80.0f);
    cycle();
    send_assign(1, PROBE_ID, ROF_ROLE_PROBE_1);
    send_offset(2, CHAMBER_ID, 1.0f);

    cycle();

    TEST_ASSERT_EQUAL(ROF_CMD_SENSORS_LOCKED, result_of(1));
    TEST_ASSERT_EQUAL(ROF_CMD_SENSORS_LOCKED, result_of(2));
}

/* SEN-04: lässt sich nicht speichern, wird das gemeldet und nichts geändert */
static void test_storage_error_is_reported(void)
{
    cycle();
    mock_storage_set_save_status(&flash_mock, ROF_ERR_IO);
    send_assign(1, PROBE_ID, ROF_ROLE_PROBE_1);

    cycle();

    TEST_ASSERT_EQUAL(ROF_CMD_STORAGE_ERROR, result_of(1));
}

/* SAF-02: Reset nur im sicheren Zustand */
static void test_reset_outside_safe_state_is_rejected(void)
{
    assign_chamber();
    send_simple(1, ROF_COMMAND_RESET);

    cycle();

    TEST_ASSERT_EQUAL(ROF_CMD_NOT_IN_SAFE_STATE, result_of(1));
}

/* SAF-02: Reset abgelehnt, solange die Ursache besteht */
static void test_reset_with_cause_active_is_rejected(void)
{
    assign_chamber();
    set_chamber(75.0f);
    start_heating(80.0f);
    cycle();
    mock_temp_sensor_unplug(&bus_mock, CHAMBER_ID);
    cycle();
    send_simple(1, ROF_COMMAND_RESET);

    cycle();

    TEST_ASSERT_EQUAL(ROF_CMD_CAUSE_ACTIVE, result_of(1));
    TEST_ASSERT_EQUAL(ROF_STATE_SAFE_STATE, rof_fsm_state(&ctrl.fsm));
}

/* SAF-02, Ü10: Reset bei behobener Ursache führt nach BEREIT, nicht ins Heizen */
static void test_reset_after_cause_is_gone_returns_to_idle(void)
{
    assign_chamber();
    set_chamber(75.0f);
    start_heating(80.0f);
    cycle();
    mock_temp_sensor_unplug(&bus_mock, CHAMBER_ID);
    cycle();
    mock_temp_sensor_plug(&bus_mock, CHAMBER_ID, "DS18B20", 75.0f);
    cycle(); /* Sensor wieder da, aber noch verriegelt */
    TEST_ASSERT_EQUAL(ROF_STATE_SAFE_STATE, rof_fsm_state(&ctrl.fsm));
    send_simple(1, ROF_COMMAND_RESET);

    cycle();

    TEST_ASSERT_EQUAL(ROF_CMD_OK, result_of(1));
    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, rof_fsm_state(&ctrl.fsm));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, heater_mock.duty);
}

/* E-026: die letzten 8 Ergebnisse bleiben abrufbar, ältere fallen heraus */
static void test_last_eight_results_are_kept(void)
{
    for (uint32_t seq = 1; seq <= 12; seq++) {
        send_setpoint(seq, 80.0f);
    }

    cycle();
    cycle();
    cycle();

    TEST_ASSERT_FALSE(has_result(4));
    TEST_ASSERT_TRUE(has_result(5));
    TEST_ASSERT_TRUE(has_result(12));
}

/* ---- Snapshot (ARC-07) -------------------------------------------------- */

static rof_snapshot_t snap;

/* Vor dem ersten Zyklus: BEREIT, kein Sollwert, Ausgang 0, keine Ergebnisse */
static void test_snapshot_before_first_cycle(void)
{
    memset(&snap, 0xAB, sizeof(snap)); /* Müll, damit jedes Feld wirklich geschrieben werden muss */

    rof_controller_snapshot(&ctrl, &snap);

    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, snap.state);
    TEST_ASSERT_FALSE(snap.has_setpoint);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, snap.duty);
    TEST_ASSERT_EQUAL_size_t(0, snap.recent_count);
}

/* Zustand, Sollwert, Tastgrad, Zeit und Rollen */
static void test_snapshot_shows_state_setpoint_duty_and_roles(void)
{
    assign_chamber();
    set_chamber(75.0f);
    start_heating(80.0f);
    cycle();

    rof_controller_snapshot(&ctrl, &snap);

    TEST_ASSERT_EQUAL(ROF_STATE_HEATING, snap.state);
    TEST_ASSERT_TRUE(snap.has_setpoint);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 80.0f, snap.setpoint_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3765f, snap.duty);
    TEST_ASSERT_EQUAL_UINT64(rof_clock_now_ms(&clk_if), snap.time_ms);
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, snap.roles[ROF_ROLE_CHAMBER].status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 75.0f, snap.roles[ROF_ROLE_CHAMBER].temp_c);
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_UNASSIGNED, snap.roles[ROF_ROLE_PROBE_1].status);
}

/* SEN-01: alle Kanäle mit ID, Typ, Rolle und Wert */
static void test_snapshot_lists_channels(void)
{
    assign_chamber();
    set_chamber(75.0f);
    cycle();

    rof_controller_snapshot(&ctrl, &snap);

    TEST_ASSERT_EQUAL_size_t(2, snap.channel_count);
    TEST_ASSERT_EQUAL_UINT64(CHAMBER_ID, snap.channels[0].id);
    TEST_ASSERT_EQUAL_STRING("DS18B20", snap.channels[0].type);
    TEST_ASSERT_EQUAL(ROF_ROLE_CHAMBER, snap.channels[0].role);
    TEST_ASSERT_TRUE(snap.channels[0].present);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 75.0f, snap.channels[0].temp_c);
    TEST_ASSERT_EQUAL_UINT64(PROBE_ID, snap.channels[1].id);
    TEST_ASSERT_EQUAL(ROF_ROLE_UNASSIGNED, snap.channels[1].role);
    TEST_ASSERT_FALSE(snap.sensor_overflow);
}

/* WEB-03 Vorbereitung: verriegelte und aktive Ursache sind sichtbar */
static void test_snapshot_shows_faults(void)
{
    assign_chamber();
    set_chamber(75.0f);
    start_heating(80.0f);
    cycle();
    mock_temp_sensor_unplug(&bus_mock, CHAMBER_ID);
    cycle();

    rof_controller_snapshot(&ctrl, &snap);

    TEST_ASSERT_EQUAL(ROF_STATE_SAFE_STATE, snap.state);
    TEST_ASSERT_EQUAL(ROF_FAULT_CHAMBER_MISSING, snap.latched_fault);
    TEST_ASSERT_EQUAL(ROF_FAULT_CHAMBER_MISSING, snap.active_fault);
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_MISSING, snap.roles[ROF_ROLE_CHAMBER].status);
}

/* E-028: die Grenzen stehen im Snapshot, damit Webserver und Oberfläche
 * prüfen können, ohne core aufzurufen */
static void test_snapshot_shows_limits(void)
{
    rof_controller_snapshot(&ctrl, &snap);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, snap.limits.setpoint_min_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 110.0f, snap.limits.setpoint_max_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ROF_SENSOR_OFFSET_MAX_C, snap.limits.offset_max_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ROF_SAFETY_HARD_MAX_C, snap.limits.hard_max_c);
}

static void test_snapshot_limits_follow_config(void)
{
    config.safety.setpoint_max_c = 95.0f;
    TEST_ASSERT_EQUAL(ROF_OK, rof_controller_init(&ctrl, &config, &io));

    rof_controller_snapshot(&ctrl, &snap);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 95.0f, snap.limits.setpoint_max_c);
}

/* E-026: Ergebnisse, neuestes zuerst. Die Suche nach der Nummer liegt in
 * proto (E-037). */
static void test_snapshot_command_results(void)
{
    send_setpoint(1, 80.0f);
    send_setpoint(2, 200.0f);
    send_simple(3, ROF_COMMAND_STOP);
    cycle();

    rof_controller_snapshot(&ctrl, &snap);

    TEST_ASSERT_EQUAL_size_t(3, snap.recent_count);
    TEST_ASSERT_EQUAL_UINT32(3, snap.recent[0].seq);
    TEST_ASSERT_EQUAL(ROF_COMMAND_STOP, snap.recent[0].type);
    TEST_ASSERT_EQUAL_UINT32(1, snap.recent[2].seq);
    TEST_ASSERT_EQUAL_UINT32(2, snap.recent[1].seq);
    TEST_ASSERT_EQUAL(ROF_CMD_SETPOINT_TOO_HIGH, snap.recent[1].result);
}

/* E-026: auch über das Ende des Ringpuffers hinweg neuestes zuerst */
static void test_snapshot_results_wrap_around(void)
{
    for (uint32_t seq = 1; seq <= 10; seq++) {
        send_setpoint(seq, 80.0f);
    }
    cycle();
    cycle();
    cycle();

    rof_controller_snapshot(&ctrl, &snap);

    TEST_ASSERT_EQUAL_size_t(ROF_CONTROLLER_RECENT_RESULTS, snap.recent_count);
    TEST_ASSERT_EQUAL_UINT32(10, snap.recent[0].seq);
    TEST_ASSERT_EQUAL_UINT32(3, snap.recent[ROF_CONTROLLER_RECENT_RESULTS - 1].seq);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_turns_output_off_and_starts_idle);
    RUN_TEST(test_init_rejects_missing_interfaces_or_config);

    RUN_TEST(test_idle_cycle_keeps_output_off);
    RUN_TEST(test_heating_cycle_sets_pi_duty);
    RUN_TEST(test_integral_uses_time_from_clock);
    RUN_TEST(test_holding_is_reached);
    RUN_TEST(test_safety_acts_before_controller_in_same_cycle);
    RUN_TEST(test_unplugged_chamber_while_idle_is_a_fault);
    RUN_TEST(test_over_temperature_cuts_output);
    RUN_TEST(test_stop_turns_output_off);
    RUN_TEST(test_pi_restarts_when_heating_starts_again);
    RUN_TEST(test_output_failure_is_logged);
    RUN_TEST(test_restart_while_holding_starts_idle_with_output_off);

    RUN_TEST(test_commands_take_effect_in_the_cycle);
    RUN_TEST(test_at_most_four_commands_per_cycle);
    RUN_TEST(test_invalid_setpoint_is_rejected_with_reason);
    RUN_TEST(test_start_without_setpoint_is_rejected);
    RUN_TEST(test_unknown_command_type_is_rejected);
    RUN_TEST(test_assign_role_via_command);
    RUN_TEST(test_assign_with_unknown_sensor_or_role_is_rejected);
    RUN_TEST(test_set_offset_via_command);
    RUN_TEST(test_sensor_settings_are_locked_while_heating);
    RUN_TEST(test_storage_error_is_reported);
    RUN_TEST(test_reset_outside_safe_state_is_rejected);
    RUN_TEST(test_reset_with_cause_active_is_rejected);
    RUN_TEST(test_reset_after_cause_is_gone_returns_to_idle);
    RUN_TEST(test_last_eight_results_are_kept);

    RUN_TEST(test_snapshot_before_first_cycle);
    RUN_TEST(test_snapshot_shows_state_setpoint_duty_and_roles);
    RUN_TEST(test_snapshot_lists_channels);
    RUN_TEST(test_snapshot_shows_faults);
    RUN_TEST(test_snapshot_shows_limits);
    RUN_TEST(test_snapshot_limits_follow_config);
    RUN_TEST(test_snapshot_command_results);
    RUN_TEST(test_snapshot_results_wrap_around);

    return UNITY_END();
}
