#include "status_log.h"

#include "rof_proto.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

static const char *const TAG = "status";

static StaticTask_t task_buffer;
static StackType_t task_stack[STATUS_LOG_STACK_BYTES];
static snapshot_store_t *log_store;
static const control_task_t *log_control;

/* Die Namen kommen aus proto und sind dieselben wie in der API */
static void log_snapshot(const rof_snapshot_t *s)
{
    char setpoint[16] = "-";
    if (s->has_setpoint) {
        snprintf(setpoint, sizeof(setpoint), "%.1f °C", s->setpoint_c);
    }
    ESP_LOGI(TAG, "Zustand %s, Sollwert %s, Tastgrad %.2f, Ursache aktiv %s, verriegelt %s",
             rof_proto_state_name(s->state), setpoint, s->duty, rof_proto_fault_name(s->active_fault),
             rof_proto_fault_name(s->latched_fault));

    for (size_t i = 0; i < s->channel_count && i < ROF_SENSOR_CHANNELS_MAX; i++) {
        const rof_channel_snapshot_t *ch = &s->channels[i];
        char id[ROF_PROTO_SENSOR_ID_LEN + 1];
        rof_proto_format_sensor_id(ch->id, id);
        const char *type = ch->type != NULL ? ch->type : "";
        if (ch->present && !ch->duplicate && ch->read_status == ROF_OK) {
            ESP_LOGI(TAG, "Sensor %s %s, Rolle %s, %.2f °C", id, type, rof_proto_role_name(ch->role),
                     ch->temp_c);
        } else {
            ESP_LOGI(TAG, "Sensor %s %s, Rolle %s, kein Wert (%s%s)", id, type,
                     rof_proto_role_name(ch->role), rof_proto_read_status_name(ch->read_status),
                     ch->duplicate ? ", doppelt" : "");
        }
    }
    if (s->sensor_overflow) {
        ESP_LOGW(TAG, "Mehr Sensoren am Bus, als es Kanäle gibt");
    }
    /* Fertigzeit-Prognose (FC-01): Restzeit nur bei ok */
    char target[16] = "-";
    if (s->has_core_target) {
        snprintf(target, sizeof(target), "%.1f °C", s->core_target_c);
    }
    ESP_LOGI(TAG, "Kernziel %s, Prognose Kern 1 %s %lu s, Kern 2 %s %lu s", target,
             rof_proto_forecast_status_name(s->forecast[0].status), (unsigned long)s->forecast[0].remaining_s,
             rof_proto_forecast_status_name(s->forecast[1].status), (unsigned long)s->forecast[1].remaining_s);
}

static void log_runtime(void)
{
    ESP_LOGI(TAG, "Regeltask: %lu Zyklen, zuletzt %lu ms, max %lu ms, Stack frei %lu B, "
                  "Snapshot %lu-mal belegt; Heap frei %lu B, min %lu B",
             (unsigned long)log_control->cycles, (unsigned long)(log_control->last_cycle_us / 1000u),
             (unsigned long)(log_control->max_cycle_us / 1000u), (unsigned long)log_control->stack_free_bytes,
             (unsigned long)log_store->skipped, (unsigned long)esp_get_free_heap_size(),
             (unsigned long)esp_get_minimum_free_heap_size());
}

static void status_log_main(void *arg)
{
    (void)arg;
    /* Eigene Kopie, statisch statt auf dem Stack */
    static rof_snapshot_t snapshot;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(STATUS_LOG_PERIOD_MS));
        if (snapshot_store_read(log_store, &snapshot, pdMS_TO_TICKS(100))) {
            log_snapshot(&snapshot);
        } else {
            ESP_LOGW(TAG, "Kein Snapshot verfügbar");
        }
        log_runtime();
    }
}

void status_log_start(snapshot_store_t *store, const control_task_t *control)
{
    log_store = store;
    log_control = control;
    xTaskCreateStatic(status_log_main, "status", STATUS_LOG_STACK_BYTES, NULL, STATUS_LOG_PRIORITY,
                      task_stack, &task_buffer);
}
