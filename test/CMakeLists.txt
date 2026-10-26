# Unity als statische Bibliothek. Fremdcode, daher ohne project_warnings.
add_library(unity STATIC vendor/unity/unity.c)
target_include_directories(unity PUBLIC vendor/unity)
# 64-Bit-Vergleiche wie TEST_ASSERT_EQUAL_UINT64 einschalten
target_compile_definitions(unity PUBLIC UNITY_SUPPORT_64)

add_executable(test_platform_host test_platform_host.c)
target_link_libraries(test_platform_host PRIVATE unity platform_host project_warnings)
add_test(NAME platform_host COMMAND test_platform_host)
