# KI-Log

Kennzeichnung der KI-Nutzung je Meilenstein. Werkzeug: Claude Code (Anthropic). Die Vorgaben stehen in `CLAUDE.md`.

## 2026-10-26 – M0 Gerüst

**Modell:** Claude Opus 5.5

**Von der KI erzeugt:**
- Plan für M0 mit Rückfragen
- Ordnerstruktur mit `.gitkeep`, `.gitignore`
- Haupt-`CMakeLists.txt` mit C11, Warnungen als Fehler und cppcheck-Target
- `test/CMakeLists.txt`, `test/test_dummy.c`
- Übernahme von Unity v2.6.1 nach `test/vendor/unity/` (unverändert, Quelle und Prüfsumme in der README dort)
- `.github/workflows/ci.yml`
- `README.md`, `docs/anforderungen.md` mit IDs, `docs/entscheidungen.md` (E-001 bis E-005), Gerüste für `docs/zustandsautomat.md` und `docs/api.yaml`
- Anpassung von Gitea auf GitHub in `CLAUDE.md`

**Vom Nutzer entschieden (Rückfragen im Plan):**
- Unity als Kopie im Repository
- GitHub statt Gitea
- Werkzeuge installiert der Nutzer selbst
- Anforderungen bekommen schon in M0 IDs

**Vom Nutzer geändert oder abgelehnt:**
- Den vorgeschlagenen CI-Nachbau in einem Docker-Container (`ubuntu:24.04`) abgelehnt. Geprüft wird lokal mit clang und über GitHub Actions mit GCC.
- Das GitHub-Repo selbst angelegt und `main` selbst gepusht, ohne die vorgeschlagene gh-CLI. Das Repo ist öffentlich statt wie empfohlen privat.
- Am erzeugten Code und an der Doku nichts geändert.

**Ergebnis:** Der erste CI-Lauf auf GitHub (Job `host`) war grün.

## 2026-10-26 – M1 Interfaces

**Modell:** Claude Opus 5.5

**Von der KI erzeugt:**
- Plan für M1 mit Rückfrage zur Sprache der Bezeichner
- `docs/glossar.md` mit der Zuordnung deutscher Begriffe zu englischen Bezeichnern
- Interfaces in `core/include`: `rof_status.h`, `rof_clock.h`, `rof_temp_sensor.h`, `rof_output.h`, `rof_storage.h`, `rof_log.h`
- Fake-Uhr und Mocks in `platform/host` samt 29 Unity-Tests in `test/test_platform_host.c`
- Prüfung der Include-Regeln `test/include_rules/check_includes.py` mit sieben Gegenproben als ctest-Tests
- CMake-Targets `rof_core` und `platform_host`, cppcheck-Ausnahme `constParameterCallback`
- Entscheidungen E-006 bis E-010, Nachweise in `docs/anforderungen.md`, README-Stand

**Vom Nutzer entschieden:**
- Englische Bezeichner im Code statt der empfohlenen deutschen
- TDD wie empfohlen: rote Phase nur lokal, Test und Implementierung in einem Commit
- Plan für M1 ohne Änderungen freigegeben

**Vom Nutzer geändert oder abgelehnt:**
- Die Zeile `Co-Authored-By: Claude` in den Commit-Nachrichten. Auf Wunsch des Nutzers ist sie aus allen bisherigen Commits entfernt und wird nicht mehr gesetzt. Die KI-Nutzung ist stattdessen hier und in der README gekennzeichnet.
- Weitere Änderungen trage ich nach dem Review von M1 nach.

## 2026-10-26 – M2 Sensorverwaltung

**Modell:** Claude Opus 5.5

**Von der KI erzeugt:**
- Plan für M2 mit Rückfragen zu Rollenkonflikt, Offset-Grenzen, Pushen und Aufräumen
- Sensorverwaltung `core/include/rof_sensor_registry.h` und `core/sensors/rof_sensor_registry.c`, testgetrieben in sieben Schritten: jeweils erst die Tests mit rotem Lauf, dann die Implementierung, danach ein reines Refactoring
- 48 Unity-Tests in `test/test_sensor_registry.c`
- CMake-Target `rof_core_interfaces`, damit Mocks nur die Interfaces kennen; Mock-Sensorbus auf 24 Plätze erweitert
- Entscheidungen E-011 bis E-015, Rollenstatus im Glossar, Nachweise für SEN-01 bis SEN-07

**Vom Nutzer entschieden:**
- Eine vergebene Rolle wird verschoben statt abgelehnt
- Kalibrieroffset ±5 °C
- Claude pusht nach jedem Meilenstein selbst, nie mit force, und prüft die CI
- Die lokale Sicherung der alten Historie wird gelöscht
- Plan für M2 ohne Änderungen freigegeben

**Vom Nutzer geändert oder abgelehnt:** nichts.

**Nach dem Push:** Der CI-Lauf #3 war rot. cppcheck 2.13 auf dem Runner meldete eine doppelte NULL-Prüfung in einem Test (`nullPointerRedundantCheck`), die cppcheck 2.22 lokal nicht meldete. Das CI-Log hat Claude auf Wunsch des Nutzers über Claude in Chrome gelesen, weil GitHub es nur angemeldet herausgibt. Ein Versuch, cppcheck 2.13 lokal aus dem Quellcode zu bauen, war vom Berechtigungssystem blockiert worden. Nach dem Fix (`test(sensors): drop redundant null checks before existing_channel`) war Lauf #4 grün.

## 2026-10-29 – M3 Sicherheitslogik

**Modell:** Claude Opus 5.5

**Von der KI erzeugt:**
- Plan für M3 mit Rückfragen zu Plausibilität, Grenzwerten, Watchdog und CI
- Sicherheitslogik `core/include/rof_safety.h` und `core/safety/rof_safety.c`, testgetrieben Regel für Regel: jeweils erst die Tests mit rotem Lauf, dann die Implementierung
- 38 Unity-Tests in `test/test_safety.c`, jede Regel mit Tests für Auslösen und Nichtauslösen
- Abdeckungsmessung: CMake-Option `ROF_COVERAGE`, Target `coverage` mit gcovr, Skript `test/coverage/reset_counters.cmake`, CI-Schritt mit 100 Prozent Zeilen und Zweigen für `core/safety`
- cppcheck-Funde als GitHub-Annotationen, relative Pfade, Zeitlimit von 15 Minuten für den CI-Job
- Entscheidungen E-016 bis E-019, Ursachen im Glossar, Nachweise für SAF-01 bis SAF-12

**Vom Nutzer entschieden:**
- Plausibilität nur für den Garraum
- Grenzwerte: harte Grenze 120 °C, plausibel −20 bis 125 °C, Sollwert 20 bis 110 °C, eingefroren bei weniger als 0,05 °C Änderung in 10 Minuten, Zeitfenster 3 Sekunden
- Der Watchdog misst den Abstand der Zyklen
- cppcheck-Annotationen und Zeitlimit in CI
- Plan für M3 ohne Änderungen freigegeben

**Auffälligkeiten beim Umsetzen:**
- `isnan()` erzeugte auf macOS Zweige, die ein `float` nie nimmt. Statt sie von der Messung auszunehmen, erkennt der Code NaN jetzt über Vergleiche (E-017).
- Eine veraltete gcov-Zählerdatei verfälschte einmal die lokale Messung. Das Target `coverage` löscht die Zähler seitdem vor jeder Messung.

**Vom Nutzer geändert oder abgelehnt:** nichts. CI-Lauf #5 nach dem Push war grün, auch die Abdeckungsprüfung mit GCC.

## 2026-11-01 – M4 Zustandsautomat

**Modell:** Claude Opus 5.5

**Von der KI erzeugt:**
- Plan für M4 mit Rückfragen zu FEHLER und SICHERER_ZUSTAND, Rollen beim Heizen, Schwellen und Start ohne Sollwert
- Änderung der Sicherheitslogik: Verriegeln nur beim Heizen oder bei Übertemperatur, `rof_safety_active()`, `limit_duty` auch bei aktiver Ursache 0; drei neue Tests, Abdeckung weiter 100 Prozent
- Zustandsautomat `core/include/rof_state_machine.h` und `core/control/rof_state_machine.c`, testgetrieben in sechs Schritten
- 38 Unity-Tests in `test/test_state_machine.c`, jeder Übergang Ü1 bis Ü10 mit mindestens einem Test
- `docs/zustandsautomat.md` mit Zustands-, Übergangs- und Befehlstabelle und Diagramm
- Entscheidungen E-020 bis E-022, Glossar, Nachweise für CTL-04, CTL-05 und SAF-10

**Vom Nutzer entschieden:**
- FEHLER als Startsperre ohne Heizen; verriegelt wird nur beim Heizen oder bei Übertemperatur. Claude hat darauf hingewiesen, dass das vom Wortlaut in CLAUDE.md abweicht (E-020).
- Rollen zuweisen nur ohne Heizen
- Schwellen 2 °C, 10 °C und 50 °C
- Start ohne Sollwert wird abgelehnt
- Plan für M4 ohne Änderungen freigegeben

**Vom Nutzer geändert oder abgelehnt:** nichts. CI-Lauf #6 nach dem Push war grün.

## 2026-11-01 – M5 Simulator und Regler

**Modell:** Claude Opus 5.5

**Von der KI erzeugt:**
- Plan für M5 mit Rückfragen zu Ort der PWM, Rückmeldung von Befehlen und Simulator-/Abnahmewerten
- Langsame PWM `core/control/rof_slow_pwm.c`, PI-Regler `core/control/rof_pi.c` (Werte nach SIMC), Regelzyklus `core/control/rof_controller.c` mit Befehlsprüfung, Begrenzung auf 4 Befehle je Zyklus und Snapshot, Befehle `core/include/rof_command.h`
- Simulator `platform/host/sim_oven.{h,c}`, Befehlsqueue `platform/host/mock_command_queue.{h,c}`, Fehlereinspeisung im Mock-Ausgang
- Tests: `test_slow_pwm`, `test_pi`, `test_sim_oven` (gegen die analytische Lösung), `test_controller`, `test_integration` mit 8 Szenarien über zusammen rund 35 simulierte Stunden, darunter der Entkopplungstest; Gegenprobe ohne Befehlsbegrenzung
- Entscheidungen E-023 bis E-027, Glossar, Nachweise für CTL-01 bis CTL-03, CTL-06, SAF-01, SAF-02, SAF-13 und ARC-02 bis ARC-09 (Host-Teil)

**Vom Nutzer entschieden:**
- PWM als reine Funktion in core
- Rückmeldung von Befehlen über den Snapshot; Claude hat ergänzt, dass die letzten 8 Ergebnisse gehalten werden
- Simulator- und Abnahmewerte wie vorgeschlagen
- Plan mit dem zusätzlichen Befehl „Offset setzen“ freigegeben, den Claude als über CLAUDE.md hinausgehend markiert hatte

**Auffälligkeiten beim Umsetzen:**
- Eine Test-Hilfsfunktion startete das Heizen direkt nach dem Zuordnen des Garraums. Der Start wurde abgelehnt, weil der Automat nach E-020 ohne Zuordnung in FEHLER stand. Korrigiert wurde der Test, nicht der Code.
- Die Abnahmewerte wurden ohne Nachstellen erreicht; die gemessenen Werte stehen in E-024.

**Vom Nutzer geändert oder abgelehnt:** nichts. CI-Lauf #7 nach dem Push war grün.

## 2026-11-01 – M6 API

**Modell:** Claude Opus 5.5

**Von der KI erzeugt:**
- Plan für M6 mit Rückfragen zu JSON-Parser, unbekannten Feldern und OpenAPI-Prüfung
- `docs/api.yaml` (OpenAPI 3.1) mit allen Endpunkten, dem WebSocket und Beispielen; `docs/api-examples/state.json`
- CI-Schritt „OpenAPI prüfen“ mit Redocly 2.64.0, `redocly.yaml` (`recommended-strict`) und `.redocly.lint-ignore.yaml` mit drei begründeten Ausnahmen
- proto: JSON-Schreiber, strenger Leser für flache Objekte, Dekodieren der Anfragen, Kodieren von Zustand, Grenzen und Antworten, Namen der API, HTTP-Status
- Grenzen im Snapshot (core)
- Tests: `test_json_writer`, `test_json_reader`, `test_proto` mit Vertragstest gegen `docs/api-examples/state.json`; Gegenproben für Parser und Vertragstest
- strengere Include-Regel für proto mit neuer Gegenprobe
- Entscheidungen E-028 bis E-030, Glossar, Nachweise für API-01 bis API-04, ARC-10, SAF-09

**Vom Nutzer entschieden:**
- eigener kleiner JSON-Parser
- unbekannte Felder ablehnen
- OpenAPI-Prüfung in CI schon ab M6
- Plan für M6 ohne Änderungen freigegeben

**Auffälligkeiten beim Umsetzen:**
- Redocly meldete drei Warnungen, die zur API nicht passen (kein 4xx bei reinem Lesen, 101 beim WebSocket). Statt die Regeln abzuschalten, sind genau diese drei Stellen mit Begründung ausgenommen (E-029). Claude hatte im Plan angekündigt, Warnungen zu beheben statt abzuschalten; hier wäre „beheben“ ein erfundener Antwortstatus gewesen.
- Eine erste Gegenprobe zum Vertragstest griff nicht, weil `sed` unter macOS die Adresse `0,/…/` nicht kennt; mit Python wiederholt, danach griff sie.
- CI-Lauf #8 nach dem Push war rot: cppcheck 2.13 meldete in `test_json_reader.c` und `test_proto.c` `nullPointerRedundantCheck`, das lokale cppcheck 2.22 nicht. Claude hat die Prüfungen in die Hilfsfunktionen `field()` und `open_example()` verschoben, wie `existing_channel()` aus M2 (`b491cf4`). CI-Lauf #9 war grün.

**Vom Nutzer geändert oder abgelehnt:** nichts.

## 2026-11-04 – M7 Firmware-Basis

**Modell:** Claude Opus 5.5

**Von der KI erzeugt:**
- Plan für M7 mit Rückfragen zu Testeingabe, DS18B20-Treiber, Haltezeit des Ausgangs und Installation von ESP-IDF
- Installation von ESP-IDF v5.5.6 unter `~/esp/esp-idf` und QEMU für Xtensa (vom Nutzer freigegeben)
- ESP-IDF-Projekt `firmware/` mit `sdkconfig.defaults` (16 MB Flash, Task-Watchdog mit Neustart), `Kconfig.projbuild` (GPIOs), Hüllen für core, proto und platform/esp32; gemeinsame Quelllisten `core/sources.cmake`, `proto/sources.cmake`, `platform/esp32/sources.cmake`
- CI-Job `firmware` im Container `espressif/idf:v5.5.6`
- platform/esp32, reines C mit Host-Tests: `ds18b20_protocol` (CRC, ROM-Adresse, Scratchpad), `ds18b20_sensors` (Messablauf ohne Warten), `led_pwm` (langsame PWM mit Haltezeit)
- platform/esp32 mit ESP-IDF: `esp32_onewire_bus` (auf `espressif/onewire_bus` 1.1.2), `esp32_led_output`, `esp32_clock`, `esp32_log`, `esp32_nvs_storage`
- Firmware: `rof_exchange` (Befehlsqueue, Snapshot-Ablage), Regeltask, Statuslog, `app_main`
- Tests: `test_ds18b20_protocol`, `test_ds18b20_sensors`, `test_led_pwm` mit Gegenproben; Include-Regel für platform/esp32 mit zwei Gegenproben
- Rauchtest der Firmware in QEMU
- Entscheidungen E-031 bis E-036, Glossar, Nachweise, `docs/hardwaretest.md`, README-Abschnitt Firmware

**Vom Nutzer entschieden:**
- in M7 nur das Statuslog, keine serielle Testkonsole
- Bus von Espressif, DS18B20-Schicht selbst
- LED fällt ohne neuen Tastgrad nach 3 s ab; Claude hatte darauf hingewiesen, dass das über den Wortlaut von CLAUDE.md hinausgeht (E-034)
- Claude installiert ESP-IDF
- Plan für M7 ohne Änderungen freigegeben

**Auffälligkeiten beim Umsetzen:**
- Die fertige Komponente `espressif/ds18b20` wartet 800 ms je Messung; deshalb nur der Bus von Espressif (E-032).
- Beim Lesen des RMT-Treibers fiel auf, dass jeder Zugriff auf einen gestörten Bus bis zu 1 s wartet. Der erste Messablauf hätte jeden Sensor einzeln versucht. Claude hat einen Test geschrieben, der das zeigt, und den Ablauf geändert (`3b339bd`); der QEMU-Rauchtest bestätigt es.
- `stdio.h` von ESP-IDF nutzt `#include_next`. Mit `-Wpedantic` baute proto in der Firmware nicht; für proto gilt `-Wpedantic` deshalb nur auf dem Host (E-031).
- cppcheck meldete einen Fehlalarm zu einem nicht belegten Feld und einen verdeckten Namen im Test; beides vor dem Commit behoben.
- Claude hat den CI-Job für die Firmware schon nach Commit 3 gepusht, um ihn früh zu prüfen, statt erst nach dem Meilenstein. CI-Lauf #10 war grün.

**Vom Nutzer geändert oder abgelehnt:** nichts. CI-Lauf #11 nach dem Push war grün. Der Nutzer hat M8 vor dem Hardwaretest begonnen; die Ergebnisse aus `docs/hardwaretest.md` stehen noch aus. Zum frühen Push nach Commit 3 hat Claude im Plan für M8 nachgefragt.

## 2026-11-07 – M8 Netzwerk

**Modell:** Claude Opus 5.5

**Von der KI erzeugt:**
- Plan für M8 mit Rückfragen zu Platzhalter-Passwort, Inhalt von LittleFS, Symbolprüfung und frühem Push
- Ergebnissuche von core nach proto verschoben (`rof_proto_command_result`), mit Tests
- Partitionstabelle mit LittleFS, gzip-Skript `firmware/tools/gzip_web.py`, Abbild mit `joltwallet/littlefs` 1.23.1, Platzhalterseite `firmware/www/index.html`
- firmware/net, reines C mit Host-Tests: `request_path` (Sensorpfade, sichere Dateipfade, Content-Types), `dns_message` (Antworten für das Captive Portal), `wifi_credentials` (SSID, Passwort, Platzhalter)
- firmware/net mit ESP-IDF: `wifi_ap` (WPA2, DHCP-Option 114), `dns_server`, `http_server` (LittleFS, Umleitung), `api_handlers` (REST über Queue und Snapshot), `ws_live` (WebSocket jede Sekunde); Hülle `firmware/components/rof_net`
- Tests: `test_request_path`, `test_dns_message`, `test_wifi_credentials`, ctest `kconfig_placeholder`, Symbolprüfung `check_net_symbols.py` mit zwei ctests und CI-Schritt; Gegenproben für Pfade, DNS, Platzhalter und Symbole, letztere auch am echten Build
- QEMU-Rauchtest: Mit Platzhalter startet kein Access Point, die Regelung läuft weiter
- Entscheidungen E-037 bis E-042, Glossar, Nachweise, Checkliste H8 in `docs/hardwaretest.md`, README

**Vom Nutzer entschieden:**
- mit Platzhalter-Passwort startet der Access Point nicht
- Platzhalterseite in LittleFS bis M9
- Symbolprüfung für firmware/net in CI
- früher Push erlaubt, wenn ein neuer oder geänderter CI-Schritt sonst erst am Ende auffiele
- Plan für M8 ohne Änderungen freigegeben

**Auffälligkeiten beim Umsetzen:**
- Die Gegenprobe zum DNS-Code zeigte, dass der Test für Kompressionszeiger nur wegen eines zu kurzen Pakets bestand. Claude hat den Test verschärft, danach griff die Gegenprobe.
- Im ersten Entwurf des Datei-Handlers las `strcmp` einen nicht belegten Pfad, wenn die Zuordnung scheiterte. Beim Durchlesen vor dem Commit bemerkt und behoben.
- Ein Aufzählungswert hieß wie das Makro des Platzhalters; der Compiler meldete es, umbenannt in `NET_WIFI_PASSWORD_IS_PLACEHOLDER`.
- cppcheck meldete erneut einen verdeckten Namen in einem Test (`id`), umbenannt.
- Bei der Checkliste fiel auf, dass der HTTP-Server Befehle nacheinander bearbeitet. Über HTTP liegt deshalb nie mehr als ein Befehl in der Queue; die Erwartung für den Flut-Test wurde angepasst (E-039).
- Zwei frühe Pushes nach der neuen Regel: nach dem LittleFS-Abbild (CI-Lauf #12) und nach der Symbolprüfung (CI-Lauf #13), beide grün.

**Vom Nutzer geändert oder abgelehnt:** wird nach dem Review von M8 ergänzt.
