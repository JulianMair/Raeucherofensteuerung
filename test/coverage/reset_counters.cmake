# Löscht alle gcov-Zählerdateien (*.gcda) im Build-Ordner, damit jede
# Messung frisch beginnt. Alte Zähler zu inzwischen geändertem Code
# verfälschen sonst das Ergebnis (E-017).
# Aufruf: cmake -DBUILD_DIR=<Build-Ordner> -P reset_counters.cmake
file(GLOB_RECURSE counters "${BUILD_DIR}/*.gcda")
if(counters)
    file(REMOVE ${counters})
endif()
