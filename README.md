# Räucherofensteuerung (ESP32-S3)

Steuerungssoftware für einen Räucherofen auf einem ESP32-S3. Prüfungsleistung im IU-Modul „Projekt: Software Development“. Getestet wird mit DS18B20-Sensoren und einer LED statt Heizelement, ohne Netzspannung.

Stand: Meilenstein M2 (Sensorverwaltung). Die vollständige Anleitung zu Bauen, Flashen und Testen folgt in M11.

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

Dieselben Schritte laufen in CI (`.github/workflows/ci.yml`) bei jedem Push. Lokal misst Apples `gcov` (LLVM), in CI misst GCC; maßgeblich ist CI.

## Dokumentation

- `docs/anforderungen.md`: alle Anforderungen mit ID und Nachweis
- `docs/entscheidungen.md`: Entscheidungen mit verworfenen Alternativen
- `docs/zustandsautomat.md`: Zustände und Übergänge
- `docs/api.yaml`: OpenAPI-Beschreibung der Schnittstelle
- `docs/glossar.md`: deutsche Fachbegriffe und ihre englischen Namen im Code
- `docs/ki_log.md`: Kennzeichnung der KI-Nutzung

## KI-Nutzung

Teile des Projekts sind mit Claude Code (Anthropic) entstanden. Die Vorgaben dafür stehen in `CLAUDE.md`. Was je Meilenstein erzeugt, geändert oder abgelehnt wurde, steht in `docs/ki_log.md`.
