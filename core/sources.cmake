# Quellen von core. Der Host-Build (core/CMakeLists.txt) und die Firmware
# (firmware/components/rof_core) lesen beide diese Datei, damit es die
# Liste nur einmal gibt.
set(ROF_CORE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/control/rof_controller.c
    ${CMAKE_CURRENT_LIST_DIR}/control/rof_pi.c
    ${CMAKE_CURRENT_LIST_DIR}/control/rof_slow_pwm.c
    ${CMAKE_CURRENT_LIST_DIR}/control/rof_state_machine.c
    ${CMAKE_CURRENT_LIST_DIR}/safety/rof_safety.c
    ${CMAKE_CURRENT_LIST_DIR}/sensors/rof_sensor_registry.c
)
set(ROF_CORE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
