# NET-04, E-041: Der Standard des WLAN-Passworts in Kconfig muss genau der
# Platzhalter sein, den wifi_credentials ablehnt. Sonst startete der
# Access Point mit einem öffentlich bekannten Passwort.
# Aufruf: cmake -DKCONFIG=... -DHEADER=... -P kconfig_placeholder.cmake
file(READ ${KCONFIG} kconfig)
file(READ ${HEADER} header)
string(REGEX MATCH "config ROF_WIFI_PASSWORD[^\n]*\n[^\n]*\n[ \t]*default \"([^\"]*)\"" _ "${kconfig}")
set(kconfig_default "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define NET_WIFI_PASSWORD_PLACEHOLDER \"([^\"]*)\"" _ "${header}")
set(header_placeholder "${CMAKE_MATCH_1}")
if(kconfig_default STREQUAL "" OR NOT kconfig_default STREQUAL header_placeholder)
    message(FATAL_ERROR "Kconfig-Standard \"${kconfig_default}\" ist nicht der Platzhalter \"${header_placeholder}\"")
endif()
message(STATUS "Platzhalter stimmt überein: ${kconfig_default}")
