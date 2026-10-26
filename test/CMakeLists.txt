# Unity als statische Bibliothek. Fremdcode, daher ohne project_warnings.
add_library(unity STATIC vendor/unity/unity.c)
target_include_directories(unity PUBLIC vendor/unity)
# 64-Bit-Vergleiche wie TEST_ASSERT_EQUAL_UINT64 einschalten
target_compile_definitions(unity PUBLIC UNITY_SUPPORT_64)

add_executable(test_platform_host test_platform_host.c)
target_link_libraries(test_platform_host PRIVATE unity platform_host project_warnings)
add_test(NAME platform_host COMMAND test_platform_host)

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
add_include_violation_test(net_driver    "firmware/net/bad.c:2: verbotenes Include driver/gpio.h")
add_include_violation_test(net_platform  "firmware/net/bad.c:2: verbotenes Include ds18b20.h")
