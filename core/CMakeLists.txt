# Nur die Header aus include/. Mocks und Treiber brauchen die Interfaces,
# aber nicht die Logik von core.
add_library(rof_core_interfaces INTERFACE)
target_include_directories(rof_core_interfaces INTERFACE include)

# Die Logik von core
add_library(rof_core STATIC
    sensors/rof_sensor_registry.c
)
target_link_libraries(rof_core
    PUBLIC rof_core_interfaces
    PRIVATE project_warnings)
