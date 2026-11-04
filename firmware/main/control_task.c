#include "control_task.h"

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *const TAG = "control";

static StaticTask_t task_buffer;
static StackType_t task_stack[CONTROL_TASK_STACK_BYTES];

static void record_cycle(control_task_t *task, int64_t start_us)
{
    const uint32_t cycle_us = (uint32_t)(esp_timer_get_time() - start_us);
    task->cycles++;
    task->last_cycle_us = cycle_us;
    if (cycle_us > task->max_cycle_us) {
        task->max_cycle_us = cycle_us;
    }
    task->stack_free_bytes = uxTaskGetStackHighWaterMark(NULL);
}

static void control_task_main(void *arg)
{
    control_task_t *task = arg;
    if (esp_task_wdt_add(NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Anmeldung beim Task-Watchdog fehlgeschlagen");
    }
    /* Statisch statt auf dem Stack: Der Snapshot ist gut ein halbes Kilobyte */
    static rof_snapshot_t snapshot;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        const int64_t start_us = esp_timer_get_time();
        ds18b20_sensors_poll(task->sensors);
        rof_controller_cycle(task->controller);
        rof_controller_snapshot(task->controller, &snapshot);
        snapshot_store_publish(task->store, &snapshot);
        esp_task_wdt_reset();
        record_cycle(task, start_us);
        /* Fester Takt: wartet bis eine Sekunde nach dem letzten Aufwachen */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS));
    }
}

void control_task_start(control_task_t *task)
{
    xTaskCreateStaticPinnedToCore(control_task_main, "control", CONTROL_TASK_STACK_BYTES, task,
                                  CONTROL_TASK_PRIORITY, task_stack, &task_buffer, CONTROL_TASK_CORE);
}
