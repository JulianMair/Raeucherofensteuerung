# ESP-IDF-Projekt der Räucherofensteuerung (M7).
# Bauen, nachdem export.sh von ESP-IDF geladen ist:
#   idf.py -C firmware build
# Komponenten liegen in main/ und components/. core und platform/esp32
# binden Hüllen in components/ ein, die Quellen bleiben an ihrem Ort.
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(raeucherofen)
