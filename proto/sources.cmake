# Quellen von proto. Der Host-Build (proto/CMakeLists.txt) und die Firmware
# (firmware/components/rof_proto) lesen beide diese Datei.
set(ROF_PROTO_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/rof_json_reader.c
    ${CMAKE_CURRENT_LIST_DIR}/rof_json_writer.c
    ${CMAKE_CURRENT_LIST_DIR}/rof_proto_decode.c
    ${CMAKE_CURRENT_LIST_DIR}/rof_proto_encode.c
    ${CMAKE_CURRENT_LIST_DIR}/rof_proto_names.c
)
set(ROF_PROTO_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
