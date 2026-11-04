# Austausch zwischen Regeltask und Außenwelt: Befehlsqueue und Snapshot.
# Bindet keine Treiber ein, damit firmware/net (M8) sie nutzen darf (ARC-04).
idf_component_register(SRCS command_queue.c snapshot_store.c
                       INCLUDE_DIRS .
                       REQUIRES rof_core freertos)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
