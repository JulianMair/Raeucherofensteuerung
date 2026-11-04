# proto als ESP-IDF-Komponente. Die Quellen stehen in proto/sources.cmake.
# Wie auf dem Host nur mit den Headern von core, nicht mit dessen Logik
# (ARC-04): rof_core steht hier nur für die Include-Pfade.
include(${CMAKE_CURRENT_LIST_DIR}/../../../proto/sources.cmake)

idf_component_register(SRCS ${ROF_PROTO_SOURCES}
                       INCLUDE_DIRS ${ROF_PROTO_INCLUDE_DIR}
                       REQUIRES rof_core)

# Ohne -Wpedantic: stdio.h von ESP-IDF nutzt #include_next, eine
# GCC-Erweiterung. Streng nach C11 prüft der Host-Build (E-003).
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
