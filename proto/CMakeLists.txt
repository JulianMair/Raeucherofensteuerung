# proto: JSON-Kodierung von Befehlen und Zustand, reines C (API-04).
# Linkt nur die Header von core, nicht dessen Logik: Ein Aufruf einer
# core-Funktion aus proto scheitert schon beim Linken (ARC-04, E-030).
add_library(rof_proto STATIC
    rof_json_reader.c
    rof_json_writer.c
    rof_proto_decode.c
    rof_proto_names.c
)
target_include_directories(rof_proto PUBLIC include)
target_link_libraries(rof_proto
    PUBLIC rof_core_interfaces
    PRIVATE project_warnings)
