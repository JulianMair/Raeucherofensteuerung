include(sources.cmake)

# Nur die Header aus include/. Mocks und Treiber brauchen die Interfaces,
# aber nicht die Logik von core.
add_library(rof_core_interfaces INTERFACE)
target_include_directories(rof_core_interfaces INTERFACE ${ROF_CORE_INCLUDE_DIR})

# Die Logik von core
add_library(rof_core STATIC ${ROF_CORE_SOURCES})
# m: logf in der Prognose; unter Linux steht die Mathe-Bibliothek nicht von selbst bereit
target_link_libraries(rof_core
    PUBLIC rof_core_interfaces m
    PRIVATE project_warnings)

if(ROF_COVERAGE)
    # -O0, damit jeder Zweig im Quelltext auch im Programm ein Zweig bleibt.
    # Wer rof_core linkt, braucht die gcov-Laufzeit, daher PUBLIC.
    target_compile_options(rof_core PRIVATE --coverage -O0)
    target_link_options(rof_core PUBLIC --coverage)
endif()
