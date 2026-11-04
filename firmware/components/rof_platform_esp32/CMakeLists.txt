# platform/esp32 als ESP-IDF-Komponente: der reine C-Teil und die Treiber.
# Die Quellen stehen in platform/esp32/sources.cmake.
include(${CMAKE_CURRENT_LIST_DIR}/../../../platform/esp32/sources.cmake)

idf_component_register(SRCS ${ROF_ESP32_PORTABLE_SOURCES} ${ROF_ESP32_IDF_SOURCES}
                       INCLUDE_DIRS ${ROF_ESP32_INCLUDE_DIR}
                       REQUIRES rof_core onewire_bus)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
