# Unity als statische Bibliothek. Fremdcode, daher ohne project_warnings.
add_library(unity STATIC vendor/unity/unity.c)
target_include_directories(unity PUBLIC vendor/unity)

add_executable(test_dummy test_dummy.c)
target_link_libraries(test_dummy PRIVATE unity project_warnings)
add_test(NAME dummy COMMAND test_dummy)
