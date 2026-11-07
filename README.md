# Räucherofensteuerung (ESP32-S3)

Steuerungssoftware für einen Räucherofen auf einem ESP32-S3. Prüfungsleistung im IU-Modul „Projekt: Software Development“. Getestet wird mit DS18B20-Sensoren und einer LED statt Heizelement, ohne Netzspannung.

Stand: Meilenstein M8 (Netzwerk). Die vollständige Anleitung zu Bauen, Flashen und Testen folgt in M11.

## Ordner

| Ordner | Inhalt |
|---|---|
| `core/` | Reines C11 ohne ESP- und FreeRTOS-Header: Interfaces, Sicherheitslogik, Sensorverwaltung, Regelung |
| `proto/` | Reines C: JSON-Kodierung von Befehlen und Zustand, Validierung |
| `platform/host/` | Mocks, Fake-Uhr, Simulator für die Host-Tests |
| `platform/esp32/` | Treiber für DS18B20, LED-Ausgang, NVS-Speicher, Uhr |
| `firmware/` | ESP-IDF-Projekt: Tasks, Queue, Access Point, HTTP- und WebSocket-Server |
| `web/` | Weboberfläche mit Preact, TypeScript und Vite |
| `test/` | Unity-Tests für core und proto, Unity selbst unter `test/vendor/unity/` |
| `docs/` | Anforderungen, API, Entscheidungen, Zustandsautomat, KI-Log |

## Voraussetzungen (macOS)

```bash
brew install cmake ninja cppcheck gcovr
```

Außerdem `python3` für die Prüfung der Include-Regeln. Es ist auf macOS vorinstalliert.

## Host-Build und Tests

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target cppcheck
```

ctest führt die Unity-Tests und die Prüfung der Include-Regeln (`test/include_rules/`) aus.

Abdeckung von `core/safety` (muss bei Zeilen und Zweigen 100 Prozent sein), in einem eigenen Build-Ordner:

```bash
cmake -S . -B build-cov -G Ninja -DCMAKE_BUILD_TYPE=Debug -DROF_COVERAGE=ON
cmake --build build-cov
cmake --build build-cov --target coverage
```

Prüfung der API-Beschreibung (braucht Node.js):

```bash
npx --yes @redocly/cli@2.64.0 lint docs/api.yaml
```

Dieselben Schritte laufen in CI (`.github/workflows/ci.yml`) bei jedem Push. Lokal misst Apples `gcov` (LLVM), in CI misst GCC; maßgeblich ist CI.

## Firmware

Gebaut wird mit ESP-IDF v5.5.6, derselben Version wie in CI (E-031). Einmalig installieren, rund 2 GB:

```bash
git clone -b v5.5.6 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
```

```bash
~/esp/esp-idf/install.sh esp32s3
```

In jedem neuen Terminal die Umgebung laden, dann bauen:

```bash
. ~/esp/esp-idf/export.sh
```

```bash
idf.py -C firmware build
```

Flashen und den seriellen Monitor öffnen (beenden mit Ctrl+]):

```bash
idf.py -C firmware flash monitor
```

Pins, WLAN-Name und WLAN-Passwort stehen in `idf.py -C firmware menuconfig` unter „Raeucherofensteuerung“. Das Passwort hat einen Platzhalter als Standard; mit ihm startet der Access Point nicht, die Regelung läuft dann ohne WLAN (E-041). Mit eigenem Passwort spannt der ESP32 das WLAN „Raeucherofen“ auf, die Seite ist unter `http://192.168.4.1/` erreichbar und öffnet sich beim Verbinden meist von selbst. Aufbau und Prüfschritte stehen in `docs/hardwaretest.md`.

Beim Bauen wird `firmware/www/` mit gzip komprimiert und als LittleFS-Abbild in die Partition `www` geschrieben (E-038). Bis M9 liegt dort nur eine Platzhalterseite.

`firmware/sdkconfig` entsteht beim ersten Bauen aus `firmware/sdkconfig.defaults` und steht nicht im Repository. Nach einer Änderung an `sdkconfig.defaults` die Datei `firmware/sdkconfig` löschen, sonst gelten die alten Werte weiter.

## Dokumentation

- `docs/anforderungen.md`: alle Anforderungen mit ID und Nachweis
- `docs/entscheidungen.md`: Entscheidungen mit verworfenen Alternativen
- `docs/zustandsautomat.md`: Zustände und Übergänge
- `docs/api.yaml`: OpenAPI-Beschreibung der Schnittstelle, Beispiel für den Zustand in `docs/api-examples/state.json`
- `docs/glossar.md`: deutsche Fachbegriffe und ihre englischen Namen im Code
- `docs/hardwaretest.md`: Aufbau und Prüfschritte auf der Platine
- `docs/ki_log.md`: Kennzeichnung der KI-Nutzung

## KI-Nutzung

Teile des Projekts sind mit Claude Code (Anthropic) entstanden. Die Vorgaben dafür stehen in `CLAUDE.md`. Was je Meilenstein erzeugt, geändert oder abgelehnt wurde, steht in `docs/ki_log.md`.
