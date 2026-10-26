# Mocks und Fake-Uhr für die Host-Tests
add_library(platform_host STATIC
    fake_clock.c
    mock_output.c
    mock_storage.c
    mock_temp_sensor.c
)
target_include_directories(platform_host PUBLIC .)
target_link_libraries(platform_host
    PUBLIC rof_core
    PRIVATE project_warnings)
