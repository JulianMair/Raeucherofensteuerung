# Mocks und Fake-Uhr für die Host-Tests
add_library(platform_host STATIC
    fake_clock.c
    mock_log.c
    mock_output.c
    mock_storage.c
    mock_temp_sensor.c
)
target_include_directories(platform_host PUBLIC .)
target_link_libraries(platform_host
    PUBLIC rof_core_interfaces
    PRIVATE project_warnings)

# Simulator des Ofens. Eigene Bibliothek, weil er die langsame PWM aus
# core nutzt; die Mocks oben kennen dagegen nur die Interfaces.
add_library(host_sim STATIC
    sim_oven.c
)
target_include_directories(host_sim PUBLIC .)
target_link_libraries(host_sim
    PUBLIC rof_core
    PRIVATE project_warnings)
