# firmware/net als ESP-IDF-Komponente: Access Point, DNS, HTTP und WebSocket.
# Die Quellen stehen in firmware/net/sources.cmake. Kein Treiber, kein core:
# Mit der Regelung spricht net nur über rof_exchange und proto (ARC-04).
include(${CMAKE_CURRENT_LIST_DIR}/../../net/sources.cmake)

idf_component_register(SRCS ${ROF_NET_PORTABLE_SOURCES} ${ROF_NET_IDF_SOURCES}
                       INCLUDE_DIRS ${ROF_NET_INCLUDE_DIR}
                       REQUIRES rof_proto rof_exchange esp_wifi esp_netif esp_event)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
