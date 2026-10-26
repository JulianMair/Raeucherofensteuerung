# Host-Build für core, proto und die Unity-Tests.
# Die Firmware in firmware/ ist ein eigenes ESP-IDF-Projekt und wird hier nicht eingebunden.
cmake_minimum_required(VERSION 3.20)
project(raeucherofen LANGUAGES C)

# Reines C11 ohne Compiler-Erweiterungen (-std=c11 statt -std=gnu11)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Schreibt build/compile_commands.json, das cppcheck und Editoren lesen
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Strenge Warnungen für unseren eigenen Code. Fremdcode wie Unity linkt dieses Target nicht.
add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_subdirectory(test)
