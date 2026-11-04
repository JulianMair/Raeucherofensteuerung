# core als ESP-IDF-Komponente. Die Quellen stehen in core/sources.cmake,
# dieselbe Liste nutzt der Host-Build.
include(${CMAKE_CURRENT_LIST_DIR}/../../../core/sources.cmake)

idf_component_register(SRCS ${ROF_CORE_SOURCES}
                       INCLUDE_DIRS ${ROF_CORE_INCLUDE_DIR})

# Dieselben strengen Warnungen wie auf dem Host (E-003)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wpedantic -Werror)
