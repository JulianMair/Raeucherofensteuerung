# Host-Build: nur der reine C-Teil von platform/esp32, damit die Unity-Tests
# ihn prüfen. Die Treiber mit ESP-IDF baut nur die Firmware.
include(sources.cmake)

add_library(platform_esp32_portable STATIC ${ROF_ESP32_PORTABLE_SOURCES})
target_include_directories(platform_esp32_portable PUBLIC ${ROF_ESP32_INCLUDE_DIR})
target_link_libraries(platform_esp32_portable
    PUBLIC rof_core
    PRIVATE project_warnings)
