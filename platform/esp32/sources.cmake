# Quellen von platform/esp32, gelesen vom Host-Build (platform/esp32/CMakeLists.txt)
# und von der Firmware (firmware/components/rof_platform_esp32).
#   ROF_ESP32_PORTABLE_SOURCES: reines C ohne ESP-IDF, läuft auch in den Host-Tests
#   ROF_ESP32_IDF_SOURCES:      Treiber mit ESP-IDF, nur in der Firmware
set(ROF_ESP32_PORTABLE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/ds18b20_protocol.c
    ${CMAKE_CURRENT_LIST_DIR}/ds18b20_sensors.c
    ${CMAKE_CURRENT_LIST_DIR}/led_pwm.c
)
set(ROF_ESP32_IDF_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/esp32_led_output.c
    ${CMAKE_CURRENT_LIST_DIR}/esp32_onewire_bus.c
)
set(ROF_ESP32_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR})
