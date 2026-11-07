idf_component_register(SRCS app_main.c control_task.c status_log.c
                       REQUIRES rof_core rof_platform_esp32 rof_exchange rof_proto rof_net esp_timer)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)

# Weboberfläche (NET-03, E-038, E-043): Beim Bauen wird jede Datei aus
# WEB_SOURCE_DIR mit gzip komprimiert und als LittleFS-Abbild für die
# Partition "www" erzeugt; idf.py flash schreibt es mit. Fehlt der Build der
# Oberfläche, bricht der Firmware-Build ab: npm --prefix web run build.
set(WEB_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../../web/dist)
set(WEB_GZIP_DIR ${CMAKE_BINARY_DIR}/www_gzip)
idf_build_get_property(python PYTHON)
add_custom_target(web_gzip
    COMMAND ${python} ${CMAKE_CURRENT_LIST_DIR}/../tools/gzip_web.py ${WEB_SOURCE_DIR} ${WEB_GZIP_DIR}
    VERBATIM)
littlefs_create_partition_image(www ${WEB_GZIP_DIR} FLASH_IN_PROJECT DEPENDS web_gzip)
