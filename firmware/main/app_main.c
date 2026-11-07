/*
 * Start der Firmware (SAF-10, E-033). ESP-IDF ruft app_main() auf, wenn
 * FreeRTOS läuft. Reihenfolge:
 *   1. LED-Ausgang: Pin auf 0, Timer der langsamen PWM läuft
 *   2. NVS-Speicher, Uhr, Log
 *   3. 1-Wire-Bus und DS18B20, erste Messung
 *   4. Befehlsqueue und Regelzyklus; der lädt die Sensorzuordnung und
 *      setzt den Ausgang auf 0, der Zustand ist BEREIT
 *   5. Snapshot-Ablage, Regeltask, Statuslog
 *   6. Netzwerk: Access Point, DNS, HTTP; scheitert das, regelt der Ofen ohne Bedienung weiter
 * Alles liegt statisch im Speicher, zur Laufzeit wird nichts angelegt.
 */
#include "command_queue.h"
#include "control_task.h"
#include "dns_server.h"
#include "ds18b20_sensors.h"
#include "esp32_clock.h"
#include "esp32_led_output.h"
#include "esp32_log.h"
#include "esp32_nvs_storage.h"
#include "esp32_onewire_bus.h"
#include "http_server.h"
#include "rof_controller.h"
#include "rof_slow_pwm.h"
#include "snapshot_store.h"
#include "status_log.h"
#include "wifi_ap.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *const TAG = "main";

static rof_clock_t clock_if;
static rof_log_t logger;
static esp32_led_output_t led;
static rof_output_t output;
static esp32_nvs_storage_t nvs;
static rof_storage_t storage;
static esp32_onewire_bus_t onewire;
static ds18b20_bus_t bus;
static ds18b20_sensors_t sensors;
static rof_temp_sensor_t sensor_driver;
static const rof_temp_sensor_t *drivers[1];
static command_queue_t queue;
static rof_command_source_t commands;
static rof_controller_t controller;
static snapshot_store_t store;
static control_task_t control;

void app_main(void)
{
    clock_if = esp32_clock_interface();
    logger = esp32_log_interface();
    const rof_controller_config_t config = rof_controller_default_config();

    /* 1. Ohne steuerbaren Ausgang startet die Regelung nicht. Ein Tastgrad
     * gilt so lange wie das Zeitfenster des Regelzyklus (E-034). */
    esp_err_t err = esp32_led_output_init(&led, CONFIG_ROF_LED_GPIO, &clock_if, ROF_SLOW_PWM_DEFAULT_PERIOD_MS,
                                          config.safety.cycle_window_ms);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LED an GPIO %d nicht einrichtbar (%s), Regelung startet nicht", CONFIG_ROF_LED_GPIO,
                 esp_err_to_name(err));
        return;
    }
    output = esp32_led_output_interface(&led);

    /* 2. Ohne Speicher läuft die Regelung, aber ohne gespeicherte Zuordnung */
    bool erased = false;
    err = esp32_nvs_storage_init(&nvs, &erased);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS nicht nutzbar (%s), Sensorzuordnung wird nicht gespeichert", esp_err_to_name(err));
    }
    if (erased) {
        ESP_LOGW(TAG, "NVS war voll oder veraltet und wurde gelöscht, die Sensorzuordnung ist weg");
    }
    storage = esp32_nvs_storage_interface(&nvs);

    /* 3. Ohne Bus läuft die Regelung und meldet den fehlenden Garraumsensor */
    err = esp32_onewire_bus_init(&onewire, CONFIG_ROF_ONEWIRE_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "1-Wire-Bus an GPIO %d nicht einrichtbar (%s)", CONFIG_ROF_ONEWIRE_GPIO,
                 esp_err_to_name(err));
    }
    bus = esp32_onewire_bus_interface(&onewire);
    ds18b20_sensors_init(&sensors, &bus, &clock_if);
    sensor_driver = ds18b20_sensors_interface(&sensors);
    drivers[0] = &sensor_driver;
    /* Suchen, Messung anstoßen, abwarten: Dann hat schon der erste
     * Regelzyklus Messwerte, und der Start zeigt nicht kurz FEHLER. */
    ds18b20_sensors_poll(&sensors);
    ds18b20_sensors_poll(&sensors);
    vTaskDelay(pdMS_TO_TICKS(DS18B20_CONVERSION_MS + 50u));

    /* 4. Regelzyklus */
    command_queue_init(&queue);
    commands = command_queue_source(&queue);
    const rof_controller_io_t io = {
        .drivers = drivers,
        .driver_count = 1,
        .storage = &storage,
        .output = &output,
        .clk = &clock_if,
        .logger = &logger,
        .commands = &commands,
    };
    if (rof_controller_init(&controller, &config, &io) != ROF_OK) {
        ESP_LOGE(TAG, "Regelzyklus nicht initialisierbar, Regelung startet nicht");
        return;
    }

    /* 5. Tasks */
    snapshot_store_init(&store);
    control.controller = &controller;
    control.sensors = &sensors;
    control.store = &store;
    control_task_start(&control);
    status_log_start(&store, &control);
    ESP_LOGI(TAG, "Regelung läuft: 1-Wire an GPIO %d, LED an GPIO %d", CONFIG_ROF_ONEWIRE_GPIO,
             CONFIG_ROF_LED_GPIO);

    /* 6. Erst nach dem Regeltask: Das Netzwerk hat keinen Einfluss auf die
     * Regelung (ARC-09). Den Grund eines Fehlers nennt das Log. */
    if (wifi_ap_start(CONFIG_ROF_WIFI_SSID, CONFIG_ROF_WIFI_PASSWORD) != ESP_OK) {
        ESP_LOGW(TAG, "Kein WLAN, die Regelung läuft ohne Bedienung weiter");
        return;
    }
    if (dns_server_start() != ESP_OK) {
        ESP_LOGW(TAG, "Kein DNS, die Seite öffnet sich nicht von selbst, bleibt aber unter %s erreichbar",
                 NET_AP_IP_TEXT);
    }
    if (http_server_start() != ESP_OK) {
        ESP_LOGW(TAG, "Kein HTTP-Server, die Regelung läuft ohne Bedienung weiter");
    }
}
