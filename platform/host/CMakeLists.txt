# Mocks und Fake-Uhr für die Host-Tests
add_library(platform_host STATIC
    fake_clock.c
)
target_include_directories(platform_host PUBLIC .)
target_link_libraries(platform_host
    PUBLIC rof_core
    PRIVATE project_warnings)
