# Unity als statische Bibliothek. Fremdcode, daher ohne project_warnings.
add_library(unity STATIC vendor/unity/unity.c)
target_include_directories(unity PUBLIC vendor/unity)
# 64-Bit-Vergleiche wie TEST_ASSERT_EQUAL_UINT64 einschalten
target_compile_definitions(unity PUBLIC UNITY_SUPPORT_64)

add_executable(test_platform_host test_platform_host.c)
target_link_libraries(test_platform_host PRIVATE unity platform_host project_warnings)
add_test(NAME platform_host COMMAND test_platform_host)

add_executable(test_sensor_registry test_sensor_registry.c)
target_link_libraries(test_sensor_registry PRIVATE unity rof_core platform_host project_warnings)
add_test(NAME sensor_registry COMMAND test_sensor_registry)

add_executable(test_safety test_safety.c)
target_link_libraries(test_safety PRIVATE unity rof_core platform_host project_warnings)
add_test(NAME safety COMMAND test_safety)

add_executable(test_state_machine test_state_machine.c)
target_link_libraries(test_state_machine PRIVATE unity rof_core platform_host project_warnings)
add_test(NAME state_machine COMMAND test_state_machine)

add_executable(test_slow_pwm test_slow_pwm.c)
target_link_libraries(test_slow_pwm PRIVATE unity rof_core project_warnings)
add_test(NAME slow_pwm COMMAND test_slow_pwm)

add_executable(test_pi test_pi.c)
target_link_libraries(test_pi PRIVATE unity rof_core project_warnings)
add_test(NAME pi COMMAND test_pi)

add_executable(test_sim_oven test_sim_oven.c)
target_link_libraries(test_sim_oven PRIVATE unity host_sim project_warnings m)
add_test(NAME sim_oven COMMAND test_sim_oven)

add_executable(test_controller test_controller.c)
target_link_libraries(test_controller PRIVATE unity rof_core platform_host project_warnings)
add_test(NAME controller COMMAND test_controller)

add_executable(test_integration test_integration.c)
target_link_libraries(test_integration PRIVATE unity host_sim platform_host project_warnings)
add_test(NAME integration COMMAND test_integration)

add_executable(test_ds18b20_protocol test_ds18b20_protocol.c)
target_link_libraries(test_ds18b20_protocol PRIVATE unity platform_esp32_portable project_warnings)
add_test(NAME ds18b20_protocol COMMAND test_ds18b20_protocol)

add_executable(test_ds18b20_sensors test_ds18b20_sensors.c)
target_link_libraries(test_ds18b20_sensors PRIVATE unity platform_esp32_portable platform_host project_warnings)
add_test(NAME ds18b20_sensors COMMAND test_ds18b20_sensors)

add_executable(test_led_pwm test_led_pwm.c)
target_link_libraries(test_led_pwm PRIVATE unity platform_esp32_portable project_warnings)
add_test(NAME led_pwm COMMAND test_led_pwm)

add_executable(test_request_path test_request_path.c)
target_link_libraries(test_request_path PRIVATE unity net_portable project_warnings)
add_test(NAME request_path COMMAND test_request_path)

# Include-Regeln (CI-02): Prüfung des echten Projekts plus Gegenproben.
# Jede Gegenprobe ist ein kleiner Ordner mit einem absichtlichen Verstoß.
# Sie besteht nur, wenn genau die erwartete Meldung erscheint.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CHECK_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include_rules/check_includes.py)
set(INCLUDE_FIXTURES ${CMAKE_CURRENT_SOURCE_DIR}/include_rules/fixtures)

add_test(NAME include_rules COMMAND ${Python3_EXECUTABLE} ${CHECK_INCLUDES} ${PROJECT_SOURCE_DIR})
add_test(NAME include_rules_accepts_clean_tree
         COMMAND ${Python3_EXECUTABLE} ${CHECK_INCLUDES} ${INCLUDE_FIXTURES}/clean)

function(add_include_violation_test fixture expected_output)
    set(name include_rules_detects_${fixture})
    add_test(NAME ${name}
             COMMAND ${Python3_EXECUTABLE} ${CHECK_INCLUDES} ${INCLUDE_FIXTURES}/${fixture})
    set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected_output}")
endfunction()

add_include_violation_test(core_esp      "core/src/bad.c:2: verbotenes Include esp_log.h")
add_include_violation_test(core_freertos "core/include/bad.h:2: verbotenes Include freertos/FreeRTOS.h")
add_include_violation_test(core_stdlib   "core/bad.c:2: verbotenes Include stdlib.h \\(kein malloc")
add_include_violation_test(core_time     "core/bad.c:2: verbotenes Include time.h \\(Zeit nur")
add_include_violation_test(proto_esp     "proto/bad.c:2: verbotenes Include esp_err.h")
add_include_violation_test(proto_time    "proto/bad.c:2: verbotenes Include time.h")
add_include_violation_test(net_driver    "firmware/net/bad.c:2: verbotenes Include driver/gpio.h")
add_include_violation_test(net_platform  "firmware/net/bad.c:2: verbotenes Include ds18b20.h")
add_include_violation_test(platform_proto "platform/esp32/bad.c:2: verbotenes Include rof_proto.h")
add_include_violation_test(platform_net  "platform/esp32/bad.c:2: verbotenes Include esp_http_server.h")

add_executable(test_json_writer test_json_writer.c)
target_link_libraries(test_json_writer PRIVATE unity rof_proto project_warnings)
add_test(NAME json_writer COMMAND test_json_writer)

add_executable(test_json_reader test_json_reader.c)
target_link_libraries(test_json_reader PRIVATE unity rof_proto project_warnings)
add_test(NAME json_reader COMMAND test_json_reader)

add_executable(test_proto test_proto.c)
target_link_libraries(test_proto PRIVATE unity rof_proto project_warnings)
add_test(NAME proto COMMAND test_proto)
# Vertragstest: proto muss genau das Beispiel aus docs/api-examples erzeugen
target_compile_definitions(test_proto PRIVATE
    ROF_API_EXAMPLE_STATE="${PROJECT_SOURCE_DIR}/docs/api-examples/state.json")
