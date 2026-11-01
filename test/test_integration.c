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

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_heats_and_holds_for_six_hours);
    RUN_TEST(test_stop_cools_down_to_idle);

    return UNITY_END();
}
