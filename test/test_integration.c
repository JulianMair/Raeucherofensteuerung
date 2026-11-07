/*
 * Integrationstests über mehrere Stunden simulierte Zeit (CTL-06, SAF-13).
 *
 * Der echte Regelzyklus aus core regelt den Simulator aus platform/host.
 * Befehle kommen wie später vom Webserver über die Befehlsqueue. Jede
 * simulierte Sekunde folgt ein Regelzyklus, wie im Regeltask der Firmware.
 */
#include "unity.h"

#include "mock_command_queue.h"
#include "mock_log.h"
#include "mock_storage.h"
#include "rof_controller.h"
#include "sim_oven.h"

#define SETPOINT_C 80.0f
#define MINUTE_S 60u
#define HOUR_S 3600u

static sim_oven_t sim;
static rof_temp_sensor_t sim_sensors;
static const rof_temp_sensor_t *drivers[1];
static rof_output_t sim_heater;
static rof_clock_t sim_clock;
static mock_storage_t flash_mock;
static rof_storage_t flash;
static mock_log_t log_mock;
static rof_log_t logger_if;
static mock_command_queue_t queue_mock;
static rof_command_source_t commands;

static rof_controller_config_t config;
static rof_controller_io_t io;
static rof_controller_t ctrl;
static rof_snapshot_t snap;
static uint32_t next_seq;

void setUp(void)
{
    sim_oven_config_t sim_config = sim_oven_default_config();
    TEST_ASSERT_EQUAL(ROF_OK, sim_oven_init(&sim, &sim_config));
    sim_sensors = sim_oven_sensors(&sim);
    drivers[0] = &sim_sensors;
    sim_heater = sim_oven_output(&sim);
    sim_clock = sim_oven_clock(&sim);
    mock_storage_init(&flash_mock);
    flash = mock_storage_interface(&flash_mock);
    mock_log_init(&log_mock);
    logger_if = mock_log_interface(&log_mock);
    mock_command_queue_init(&queue_mock);
    commands = mock_command_queue_source(&queue_mock);
    next_seq = 1;

    config = rof_controller_default_config();
    io = (rof_controller_io_t){
        .drivers = drivers,
        .driver_count = 1,
        .storage = &flash,
        .output = &sim_heater,
        .clk = &sim_clock,
        .logger = &logger_if,
        .commands = &commands,
    };
    TEST_ASSERT_EQUAL(ROF_OK, rof_controller_init(&ctrl, &config, &io));
}

void tearDown(void) {}

/* Eine simulierte Sekunde, danach ein Regelzyklus */
static void run_one_second(void)
{
    sim_oven_advance(&sim, 1000);
    rof_controller_cycle(&ctrl);
}

static void run_seconds(unsigned seconds)
{
    for (unsigned i = 0; i < seconds; i++) {
        run_one_second();
    }
}

static rof_state_t state(void)
{
    rof_controller_snapshot(&ctrl, &snap);
    return snap.state;
}

static uint32_t send(rof_command_t command)
{
    command.seq = next_seq++;
    TEST_ASSERT_TRUE(mock_command_queue_push(&queue_mock, command));
    return command.seq;
}

/* Wie beim ersten Einschalten: Sensoren erkennen, Rollen zuweisen */
static void prepare_oven(void)
{
    run_one_second();
    send((rof_command_t){ .type = ROF_COMMAND_ASSIGN_ROLE, .sensor_id = SIM_OVEN_CHAMBER_ID, .role = ROF_ROLE_CHAMBER });
    send((rof_command_t){ .type = ROF_COMMAND_ASSIGN_ROLE, .sensor_id = SIM_OVEN_PROBE_ID, .role = ROF_ROLE_PROBE_1 });
    run_one_second();
    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, state());
}

static void start_at(float setpoint_c)
{
    send((rof_command_t){ .type = ROF_COMMAND_SET_SETPOINT, .value_c = setpoint_c });
    send((rof_command_t){ .type = ROF_COMMAND_START });
    run_one_second();
    TEST_ASSERT_EQUAL(ROF_STATE_HEATING, state());
}

/* Läuft, bis der Zustand erreicht ist. Liefert die Sekunden, höchstens limit_s + 1. */
static unsigned run_until_state(rof_state_t target, unsigned limit_s)
{
    unsigned elapsed_s = 0;
    while (state() != target && elapsed_s <= limit_s) {
        run_one_second();
        elapsed_s++;
    }
    return elapsed_s;
}

/* ---- Aufheizen, Halten, Abkühlen (CTL-06) ------------------------------- */

/* CTL-06, Abnahme E-025: HALTEN nach höchstens 30 min, Überschwingen
 * höchstens 3 °C, danach 6 Stunden innerhalb ±1 °C. Der Kern folgt. */
static void test_heats_and_holds_for_six_hours(void)
{
    prepare_oven();
    start_at(SETPOINT_C);

    const unsigned to_holding_s = run_until_state(ROF_STATE_HOLDING, 30 * MINUTE_S);
    TEST_ASSERT_TRUE_MESSAGE(to_holding_s <= 30 * MINUTE_S, "HALTEN nicht nach 30 min erreicht");

    double max_c = sim.chamber_c;
    double min_after_settle_c = 1000.0;
    double max_after_settle_c = -1000.0;
    for (unsigned s = 0; s < 6 * HOUR_S; s++) {
        run_one_second();
        TEST_ASSERT_EQUAL(ROF_STATE_HOLDING, state());
        if (sim.chamber_c > max_c) {
            max_c = sim.chamber_c;
        }
        if (s >= 30 * MINUTE_S) {
            min_after_settle_c = sim.chamber_c < min_after_settle_c ? sim.chamber_c : min_after_settle_c;
            max_after_settle_c = sim.chamber_c > max_after_settle_c ? sim.chamber_c : max_after_settle_c;
        }
    }

    TEST_ASSERT_TRUE_MESSAGE(max_c <= SETPOINT_C + 3.0, "Überschwingen über 3 °C");
    TEST_ASSERT_TRUE_MESSAGE(min_after_settle_c >= SETPOINT_C - 1.0, "unter Sollwert - 1 °C");
    TEST_ASSERT_TRUE_MESSAGE(max_after_settle_c <= SETPOINT_C + 1.0, "über Sollwert + 1 °C");
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, snap.roles[ROF_ROLE_PROBE_1].status);
    TEST_ASSERT_TRUE(snap.roles[ROF_ROLE_PROBE_1].temp_c > 70.0f);
    TEST_ASSERT_TRUE(snap.roles[ROF_ROLE_PROBE_1].temp_c < SETPOINT_C + 1.0f);
}

/* CTL-06: Stopp nach einer Stunde Halten, Abkühlen bis unter 50 °C, dann BEREIT */
static void test_stop_cools_down_to_idle(void)
{
    prepare_oven();
    start_at(SETPOINT_C);
    run_until_state(ROF_STATE_HOLDING, 30 * MINUTE_S);
    run_seconds(HOUR_S);

    send((rof_command_t){ .type = ROF_COMMAND_STOP });
    run_one_second();
    TEST_ASSERT_EQUAL(ROF_STATE_COOLING, state());

    unsigned cooling_s = 0;
    while (state() == ROF_STATE_COOLING && cooling_s < 2 * HOUR_S) {
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, snap.duty);
        run_one_second();
        cooling_s++;
    }

    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, state());
    TEST_ASSERT_TRUE(snap.roles[ROF_ROLE_CHAMBER].temp_c < 50.0f);
}

/* ---- Fehler nach Stunden (CTL-06, SAF-02 bis SAF-07, SAF-10) ------------ */

static void heat_and_hold_for(unsigned seconds)
{
    prepare_oven();
    start_at(SETPOINT_C);
    run_until_state(ROF_STATE_HOLDING, 30 * MINUTE_S);
    run_seconds(seconds);
    TEST_ASSERT_EQUAL(ROF_STATE_HOLDING, state());
}

/* CTL-06, SAF-07: Garraumsensor fällt nach 3 h aus. Sofort sicherer Zustand,
 * Ausgang 0, der Ofen kühlt ab und bleibt aus. */
static void test_chamber_sensor_lost_after_three_hours(void)
{
    heat_and_hold_for(3 * HOUR_S);

    sim.chamber_unplugged = true;
    run_one_second();

    TEST_ASSERT_EQUAL(ROF_STATE_SAFE_STATE, state());
    TEST_ASSERT_EQUAL(ROF_FAULT_CHAMBER_MISSING, snap.latched_fault);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, snap.duty);

    run_seconds(HOUR_S);

    TEST_ASSERT_EQUAL(ROF_STATE_SAFE_STATE, state());
    TEST_ASSERT_TRUE(sim.chamber_c < 30.0);
}

/* CTL-06, SAF-05: Garraumwert friert nach 3 h ein. Nach 10 min sicherer Zustand. */
static void test_chamber_value_frozen_after_three_hours(void)
{
    heat_and_hold_for(3 * HOUR_S);

    sim_oven_freeze_chamber(&sim);
    run_seconds(9 * MINUTE_S);
    TEST_ASSERT_EQUAL(ROF_STATE_HOLDING, state());
    const unsigned to_safe_s = run_until_state(ROF_STATE_SAFE_STATE, 2 * MINUTE_S);

    TEST_ASSERT_TRUE(to_safe_s <= 2 * MINUTE_S);
    TEST_ASSERT_EQUAL(ROF_FAULT_FROZEN, snap.latched_fault);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, snap.duty);
}

/* SAF-06: die Heizung klemmt, ohne dass jemand gestartet hat. Bei über 120 °C
 * verriegelt die Sicherheitslogik, obwohl gar nicht geheizt wird (E-020). */
static void test_stuck_heater_trips_over_temperature_without_start(void)
{
    prepare_oven();
    sim.heater_stuck_on = true;

    const unsigned to_safe_s = run_until_state(ROF_STATE_SAFE_STATE, HOUR_S);

    TEST_ASSERT_TRUE(to_safe_s <= HOUR_S);
    TEST_ASSERT_EQUAL(ROF_FAULT_OVER_TEMPERATURE, snap.latched_fault);
    TEST_ASSERT_TRUE(sim.chamber_c > 120.0);
    TEST_ASSERT_TRUE(sim.chamber_c < 121.0);
}

/* SAF-10, SEN-04: Neustart nach 2 h Halten. BEREIT, Ausgang 0, Rollen wieder
 * da, und ohne neuen Start wird nicht geheizt. */
static void test_restart_while_holding_does_not_resume_heating(void)
{
    heat_and_hold_for(2 * HOUR_S);

    TEST_ASSERT_EQUAL(ROF_OK, rof_controller_init(&ctrl, &config, &io));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, sim.duty);
    run_seconds(HOUR_S);

    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, state());
    TEST_ASSERT_FALSE(snap.has_setpoint);
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, snap.roles[ROF_ROLE_CHAMBER].status);
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, snap.roles[ROF_ROLE_PROBE_1].status);
    TEST_ASSERT_TRUE(sim.chamber_c < 30.0);
}

/* ---- Entkopplung (SAF-13, ARC-09) --------------------------------------- */

/* Ein Webserver, der wild Befehle schickt: ungültige und harmlose gültige.
 * Er schreibt wie mit Wartezeit 0; ist die Queue voll, scheitert er nur selbst. */
static void flood_queue(unsigned attempts)
{
    const rof_command_t flood[] = {
        { .type = ROF_COMMAND_SET_SETPOINT, .value_c = 500.0f }, /* zu hoch */
        { .type = (rof_command_type_t)99 },                     /* unbekannt */
        { .type = ROF_COMMAND_START },                          /* läuft schon */
        { .type = ROF_COMMAND_SET_SETPOINT, .value_c = SETPOINT_C },
        { .type = ROF_COMMAND_RESET },                          /* nicht im sicheren Zustand */
    };
    for (unsigned i = 0; i < attempts; i++) {
        rof_command_t command = flood[i % (sizeof(flood) / sizeof(flood[0]))];
        command.seq = next_seq++;
        (void)mock_command_queue_push(&queue_mock, command);
    }
}

/* Hält 6 h und prüft je Zyklus Band, Zustand und Befehlsgrenze.
 * flood_per_second = 0 heißt: kein einziger Befehl mehr. */
static void hold_six_hours_with(unsigned flood_per_second)
{
    for (unsigned s = 0; s < 6 * HOUR_S; s++) {
        flood_queue(flood_per_second);
        const unsigned received_before = queue_mock.received;

        run_one_second();

        TEST_ASSERT_TRUE(queue_mock.received - received_before <= ROF_CONTROLLER_DEFAULT_MAX_COMMANDS);
        TEST_ASSERT_EQUAL(ROF_STATE_HOLDING, state());
        TEST_ASSERT_EQUAL(ROF_FAULT_NONE, snap.latched_fault);
        TEST_ASSERT_TRUE(sim.chamber_c >= SETPOINT_C - 1.0 && sim.chamber_c <= SETPOINT_C + 1.0);
    }
}

/* SAF-13: die Befehlsqueue wird 6 h lang geflutet, die Regelung hält trotzdem.
 * Je Zyklus höchstens 4 Befehle, der Rest wartet oder wird beim Absender abgewiesen. */
static void test_flooded_command_queue_does_not_disturb_control(void)
{
    heat_and_hold_for(30 * MINUTE_S);

    hold_six_hours_with(100);

    TEST_ASSERT_TRUE(queue_mock.rejected_pushes > 0); /* die Queue war wirklich voll */
}

/* SAF-13: nach dem Start kommt kein Befehl mehr, etwa weil der Webserver
 * abgestürzt ist. Die Regelung hält 6 h ohne ihn. */
static void test_silent_command_queue_does_not_disturb_control(void)
{
    heat_and_hold_for(30 * MINUTE_S);
    const unsigned received_before = queue_mock.received;

    hold_six_hours_with(0);

    TEST_ASSERT_EQUAL_UINT(received_before, queue_mock.received);
}

/* ---- Fertigzeit-Prognose (FC-01, E-047) --------------------------------- */

#define CORE_TARGET_C 70.0f
#define FORECAST_CHECKS 4u

/* FC-01, Abnahme E-047: Ab 10 Minuten Lernzeit liegt die vorhergesagte
 * Restzeit höchstens 10 Prozent neben der tatsächlichen, mindestens mit
 * 2 Minuten Toleranz. Der Simulator nähert den Kern erster Ordnung mit
 * Totzeit an (tau 1 h, Totzeit 2 min). Gemessen: +4,1 % nach 10 min,
 * +0,3 % nach 20 min, danach 0 % (E-047). */
static void test_forecast_matches_simulated_cooking(void)
{
    static const unsigned CHECK_AFTER_MIN[FORECAST_CHECKS] = { 10, 20, 40, 60 };
    uint32_t predicted_s[FORECAST_CHECKS];
    unsigned checked_at_s[FORECAST_CHECKS];
    size_t checks = 0;

    prepare_oven();
    send((rof_command_t){ .type = ROF_COMMAND_SET_CORE_TARGET, .value_c = CORE_TARGET_C });
    start_at(SETPOINT_C);
    TEST_ASSERT_TRUE(run_until_state(ROF_STATE_HOLDING, 30u * MINUTE_S) <= 30u * MINUTE_S);

    unsigned holding_s = 0;
    for (;;) {
        rof_controller_snapshot(&ctrl, &snap);
        TEST_ASSERT_EQUAL(ROF_STATE_HOLDING, snap.state);
        if (snap.roles[ROF_ROLE_PROBE_1].temp_c >= CORE_TARGET_C) {
            break;
        }
        if (checks < FORECAST_CHECKS && holding_s == CHECK_AFTER_MIN[checks] * MINUTE_S) {
            TEST_ASSERT_EQUAL(ROF_FORECAST_OK, snap.forecast[0].status);
            predicted_s[checks] = snap.forecast[0].remaining_s;
            checked_at_s[checks] = holding_s;
            checks++;
        }
        TEST_ASSERT_TRUE(holding_s < 6u * HOUR_S);
        run_one_second();
        holding_s++;
    }

    TEST_ASSERT_EQUAL_size_t(FORECAST_CHECKS, checks);
    TEST_ASSERT_EQUAL(ROF_FORECAST_REACHED, snap.forecast[0].status);
    for (size_t i = 0; i < FORECAST_CHECKS; i++) {
        const uint32_t actual_s = holding_s - checked_at_s[i];
        const uint32_t tolerance_s = actual_s / 10u > 120u ? actual_s / 10u : 120u;
        TEST_ASSERT_UINT32_WITHIN(tolerance_s, actual_s, predicted_s[i]);
    }
}

/* Kern 2 ist im Simulator nicht angeschlossen: keine Prognose, kein Einfluss auf Kern 1 */
static void test_forecast_without_second_probe(void)
{
    prepare_oven();
    send((rof_command_t){ .type = ROF_COMMAND_SET_CORE_TARGET, .value_c = CORE_TARGET_C });
    start_at(SETPOINT_C);
    run_until_state(ROF_STATE_HOLDING, 30u * MINUTE_S);
    run_seconds(10u * MINUTE_S);

    rof_controller_snapshot(&ctrl, &snap);
    TEST_ASSERT_EQUAL(ROF_FORECAST_OK, snap.forecast[0].status);
    TEST_ASSERT_EQUAL(ROF_FORECAST_UNAVAILABLE, snap.forecast[1].status);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_heats_and_holds_for_six_hours);
    RUN_TEST(test_stop_cools_down_to_idle);

    RUN_TEST(test_chamber_sensor_lost_after_three_hours);
    RUN_TEST(test_chamber_value_frozen_after_three_hours);
    RUN_TEST(test_stuck_heater_trips_over_temperature_without_start);
    RUN_TEST(test_restart_while_holding_does_not_resume_heating);

    RUN_TEST(test_flooded_command_queue_does_not_disturb_control);
    RUN_TEST(test_silent_command_queue_does_not_disturb_control);
    RUN_TEST(test_forecast_matches_simulated_cooking);
    RUN_TEST(test_forecast_without_second_probe);

    return UNITY_END();
}
