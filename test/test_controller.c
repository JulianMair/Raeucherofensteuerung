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

    return UNITY_END();
}
