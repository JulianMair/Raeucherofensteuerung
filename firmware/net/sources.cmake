# Quellen von firmware/net, gelesen vom Host-Build (firmware/net/CMakeLists.txt)
# und von der Firmware (firmware/components/rof_net).
#   ROF_NET_PORTABLE_SOURCES: reines C ohne ESP-IDF, läuft auch in den Host-Tests
#   ROF_NET_IDF_SOURCES:      Netzwerk mit ESP-IDF, nur in der Firmware
set(ROF_NET_PORTABLE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/request_path.c
)
set(ROF_NET_IDF_SOURCES
)
set(ROF_NET_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR})
