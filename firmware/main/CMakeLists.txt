idf_component_register(SRCS app_main.c control_task.c status_log.c
                       REQUIRES rof_core rof_platform_esp32 rof_exchange rof_proto esp_timer)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
