# Host-Build: nur der reine C-Teil von firmware/net, damit die Unity-Tests
# ihn prüfen. Den Webserver mit ESP-IDF baut nur die Firmware.
include(sources.cmake)

add_library(net_portable STATIC ${ROF_NET_PORTABLE_SOURCES})
target_include_directories(net_portable PUBLIC ${ROF_NET_INCLUDE_DIR})
target_link_libraries(net_portable
    PUBLIC rof_proto
    PRIVATE project_warnings)
