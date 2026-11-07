# Testbericht

Stand des Codes: `040e283`, gemessen am 2026-11-07 auf macOS (Apple Silicon). Dieselben Prüfungen laufen bei jedem Push in GitHub Actions (`.github/workflows/ci.yml`); der letzte Lauf vor M11 war [#18](https://github.com/JulianMair/Raeucherofensteuerung/actions/runs/42373296353) für `187ba70`, grün. Der Lauf zu M11 steht im KI-Log.

Die Tests auf der echten Platine stehen noch aus (Abschnitt „Hardware“). Ihre Ergebnisse werden hier und in `docs/hardwaretest.md` nachgetragen.

## Ergebnis auf einen Blick

| Prüfung | Ergebnis |
|---|---|
| Unity-Tests auf dem Host | 398 Tests in 19 Programmen, 0 Fehlschläge |
| ctest insgesamt | 39 von 39 bestanden: die 19 Unity-Programme und 20 Prüfungen mit Skripten |
| Zweigabdeckung `core/safety` (gcovr) | Zeilen 100 % (100 von 100), Zweige 100 % (80 von 80) |
| cppcheck | keine Funde (lokal 2.22, in CI 2.13) |
| Compiler | C11 mit `-Wall -Wextra -Wpedantic -Werror`, keine Warnung |
| OpenAPI (`docs/api.yaml`, Redocly 2.64.0) | gültig, 3 begründete Ausnahmen (E-029) |
| Weboberfläche | Lint und Typprüfung ohne Fehler, 105 Vitest-Tests in 10 Dateien bestanden |
| Größenbudget der Oberfläche | 34 662 B gzip von höchstens 81 920 B (42 %) |
| Firmware (`idf.py build`, ESP-IDF v5.5.6) | baut, App 916 112 B (0xdfa90), 71 % der App-Partition frei |
| Symbolprüfung `firmware/net` | 80 Symbole geprüft, kein Aufruf von core oder Treibern |
| Abgleich der Anforderungen | 56 Anforderungen: 45 erfüllt, 10 erfüllt (Hardware ausstehend), 1 teilweise |
| Rauchtest im Emulator QEMU | bestanden, zwei Läufe (`docs/hardwaretest.md`) |
| Hardwaretests H7 bis H10 | ausstehend |

## Teststufen

### Unit-Tests mit Unity

Jedes Testprogramm unter `test/` ist ein eigener ctest-Eintrag. Die Zeit ist in allen Tests injiziert (Fake-Uhr), kein Test wartet wirklich.

| Programm | Prüft | Tests |
|---|---|---|
| `test_sensor_registry` | Sensorverwaltung: an- und abstecken, Rollen, gespeicherte Zuordnung, doppelte IDs, mehr als 8 Sensoren | 48 |
| `test_safety` | jede Sicherheitsregel mit Auslösen und Nichtauslösen | 41 |
| `test_state_machine` | jeder Übergang aus `docs/zustandsautomat.md` | 38 |
| `test_controller` | Regelzyklus: Sicherheit vor dem Regler, Befehle, Snapshot, Prognose | 43 |
| `test_pi` | PI-Regler mit Anti-Windup | 10 |
| `test_slow_pwm` | langsame PWM | 6 |
| `test_forecast` | Fertigzeit-Prognose | 20 |
| `test_json_writer` | JSON schreiben | 10 |
| `test_json_reader` | JSON lesen, auch kaputt und unvollständig | 17 |
| `test_proto` | Befehle dekodieren, Zustand kodieren, Grenzen, Vertragstest gegen `docs/api-examples/state.json` | 35 |
| `test_platform_host` | Mocks und Fake-Uhr | 34 |
| `test_sim_oven` | Simulator gegen die analytische Lösung | 15 |
| `test_ds18b20_protocol` | CRC, ROM-Adresse, Scratchpad | 13 |
| `test_ds18b20_sensors` | Messablauf ohne Warten, Busfehler | 17 |
| `test_led_pwm` | LED-Ausgang mit Haltezeit | 8 |
| `test_request_path` | Pfade des Webservers | 11 |
| `test_dns_message` | DNS-Antworten des Captive Portals | 17 |
| `test_wifi_credentials` | Prüfung des WLAN-Passworts | 5 |
| `test_integration` | siehe nächster Abschnitt | 10 |
| **Summe** | | **398** |

Die Tests für `platform/esp32` und `firmware/net` prüfen die reinen Teile dieser Schichten auf dem Host. Der Code, der ESP-IDF aufruft, wird nur gebaut und auf der Hardware geprüft.

### Integration mit dem Simulator

`test_integration` lässt die echte Regelung gegen den Simulator laufen (Modell erster Ordnung mit Totzeit für Garraum und Kern). Die längsten Szenarien laufen über sechs simulierte Stunden; alle zehn zusammen brauchen unter 0,1 s Rechenzeit.

| Szenario | Belegt |
|---|---|
| `test_heats_and_holds_for_six_hours` | Aufheizen und Halten (E-024: HALTEN nach 8,8 min, Überschwingen 0,19 °C) |
| `test_stop_cools_down_to_idle` | Stopp und Abkühlen bis BEREIT |
| `test_chamber_sensor_lost_after_three_hours` | Sensorausfall nach Stunden führt in den sicheren Zustand |
| `test_chamber_value_frozen_after_three_hours` | eingefrorener Wert nach Stunden |
| `test_stuck_heater_trips_over_temperature_without_start` | Übertemperatur auch ohne Start |
| `test_restart_while_holding_does_not_resume_heating` | Neustart startet in BEREIT mit Ausgang 0 |
| `test_flooded_command_queue_does_not_disturb_control` | Entkopplung: geflutete Befehlsqueue |
| `test_silent_command_queue_does_not_disturb_control` | Entkopplung: kein Befehl mehr |
| `test_forecast_matches_simulated_cooking` | Prognose gegen den Simulator (E-047) |
| `test_forecast_without_second_probe` | Prognose mit nur einem Kernfühler |

Gemessene Abweichung der Prognose bei Kernziel 70 °C und Garraum 80 °C: +4,1 % nach 10 min Lernzeit, +0,3 % nach 20 min, 0 % nach 40 und 60 min. Gefordert sind höchstens 10 % (E-047).

### Weboberfläche

Vitest mit `@testing-library/preact` in jsdom. REST kommt aus MSW, die Livewerte aus einem Fake-WebSocket, die Kurve aus einem Fake-uPlot.

| Datei | Tests |
|---|---|
| `web/test/status.test.tsx` | 30 |
| `web/test/controls.test.tsx` | 20 |
| `web/test/validation.test.ts` | 15 |
| `web/test/client.test.ts` | 9 |
| `web/test/live.test.ts` | 8 |
| `web/test/labels.test.ts` | 7 |
| `web/test/sensors.test.tsx` | 7 |
| `web/test/history.test.ts` | 4 |
| `web/test/chart.test.tsx` | 3 |
| `web/test/App.test.tsx` | 2 |
| **Summe** | **105** |

Abgedeckt sind die Fälle aus CLAUDE.md: Anzeige aller Zustände, ungültige Eingaben, abgelehnter Sollwert, Verbindungsabbruch und Wiederverbindung, sicherer Zustand.

### Architektur- und Build-Prüfungen

Diese Prüfungen sind Python-Skripte und laufen als ctest-Einträge. Jede hat mindestens eine Gegenprobe, die zeigt, dass sie einen Verstoß erkennt.

| Prüfung | ctest-Einträge | Was sie sicherstellt |
|---|---|---|
| `test/include_rules/check_includes.py` | `include_rules`, `include_rules_accepts_clean_tree`, 10 × `include_rules_detects_…` | core und proto binden keine ESP- oder FreeRTOS-Header ein, core auch kein `stdlib.h` (kein malloc) und kein `time.h` (Zeit nur injiziert); `firmware/net` bindet keine Treiber ein, `platform/esp32` weder proto noch Netzwerk |
| `test/include_rules/check_net_symbols.py` | `net_symbols_accepts_clean`, `net_symbols_detects_core_call` | `firmware/net` ruft weder core noch Treiber auf; in CI zusätzlich am echten Firmware-Build |
| `test/kconfig_placeholder.cmake` | `kconfig_placeholder` | Der Standard des WLAN-Passworts in Kconfig ist genau der Platzhalter, den die Firmware ablehnt (E-041) |
| `firmware/tools/gzip_web.py` | `gzip_web_within_budget`, `gzip_web_over_budget`, `gzip_web_missing_source` | Größenbudget der Oberfläche |
| `test/requirements/check_evidence.py` | `requirements_evidence`, `requirements_evidence_detects_errors` | Jeder Nachweis in `docs/anforderungen.md` hat einen Status und zeigt auf vorhandene Tests und Dateien (E-049) |

### Statische Analyse und Abdeckung

- cppcheck mit `warning,style,performance,portability` über `compile_commands.json`, ohne `test/vendor`. Ein Fund lässt den Build scheitern.
- gcovr misst `core/safety` in einem eigenen Build-Ordner mit `--coverage`. Unter 100 % bei Zeilen oder Zweigen scheitert das Target `coverage` und damit CI.
- Redocly prüft `docs/api.yaml`. Die TypeScript-Typen der Oberfläche werden daraus erzeugt; passt die Oberfläche nicht zur API, scheitert ihre Typprüfung.

### Emulator QEMU

Claude hat die Firmware zweimal in QEMU gestartet (Stand `3b339bd` und `7db05c4`). Ergebnis: Der Regeltask läuft ohne Neustart, ohne Sensoren steht der Zustand auf `fault` mit Tastgrad 0, ein gestörter Bus kostet nur beim ersten Zugriff 1 s, danach dauert ein Zyklus 4 bis 6 ms, und mit dem Platzhalter-Passwort startet kein Access Point, während die Regelung weiterläuft. Einzelheiten stehen in `docs/hardwaretest.md`.

## Abgleich der Anforderungen

`docs/anforderungen.md` ordnet jeder Anforderung aus CLAUDE.md ihren Nachweis zu. Der ctest `requirements_evidence` prüft die Tabelle bei jedem Lauf.

| Bereich | erfüllt | erfüllt, Hardware ausstehend | teilweise |
|---|---|---|---|
| ARC Architektur | 8 | 1 | 1 |
| SAF Sicherheitslogik | 12 | 1 | 0 |
| SEN Sensoren | 5 | 2 | 0 |
| CTL Regelung | 6 | 0 | 0 |
| API | 2 | 2 | 0 |
| NET Netzwerk | 1 | 3 | 0 |
| WEB Oberfläche | 6 | 1 | 0 |
| CI | 4 | 0 | 0 |
| FC Prognose | 1 | 0 | 0 |
| **Summe** | **45** | **10** | **1** |

Teilweise erfüllt ist ARC-09 (Regelung blockiert nie). Was fehlt, steht unter „Bekannte Grenzen“.

## Gegenproben

Ein Test, der nie scheitern kann, belegt nichts. Deshalb wurde jede neue Prüfung mindestens einmal absichtlich zum Scheitern gebracht.

- **Dauerhaft als ctest:** die 10 Fälle `include_rules_detects_…`, `net_symbols_detects_core_call`, `gzip_web_over_budget`, `gzip_web_missing_source` und `requirements_evidence_detects_errors`. Sie erwarten, dass das Skript einen eingebauten Verstoß meldet.
- **Einmalig beim Entwickeln:** Claude hat den Code kurz absichtlich verändert und geprüft, dass der zugehörige Test rot wird. Diese Änderungen wurden nicht committet. In core und proto entstand jeder Test zuerst mit rotem Lauf (Test zuerst). Weitere Gegenproben, je Meilenstein im KI-Log:
  - M5: der Entkopplungstest ohne Begrenzung der Befehle je Zyklus
  - M6: Parser und Vertragstest
  - M7: DS18B20-Protokoll, Messablauf und LED-Ausgang
  - M8: Pfade, DNS, Platzhalter und die Symbolprüfung am echten Build
  - M9: das Erkennen von Stille auf dem WebSocket
  - M10: die Prognose

## Von Tests und Prüfungen gefundene Fehler

| Meilenstein | Gefunden durch | Fehler | Behoben |
|---|---|---|---|
| M2 | CI, cppcheck 2.13 | doppelte NULL-Prüfung in einem Test, lokal mit 2.22 nicht gemeldet (CI-Lauf #3 rot) | Prüfung in Hilfsfunktion `existing_channel()` (`b491cf4`) |
| M3 | gcov | `isnan()` erzeugte auf macOS Zweige, die ein `float` nie nimmt | NaN wird über Vergleiche erkannt (E-017) |
| M3 | Abdeckungsmessung | veraltete Zählerdatei verfälschte die Messung | Target `coverage` löscht die Zähler vor jeder Messung |
| M4 | Test | Test-Hilfsfunktion startete das Heizen ohne zugeordneten Garraum | Test korrigiert, Code war richtig (E-020) |
| M6 | Redocly | drei Warnungen, die zur API nicht passen | gezielte Ausnahmen mit Begründung (E-029) |
| M6 | Gegenprobe | erste Gegenprobe zum Vertragstest griff nicht (`sed` unter macOS) | mit Python wiederholt |
| M6 | CI, cppcheck 2.13 | `nullPointerRedundantCheck` in zwei Tests (CI-Lauf #8 rot) | Hilfsfunktionen `field()` und `open_example()` |
| M7 | Test | Ein gestörter 1-Wire-Bus hätte jeden Sensor einzeln bis zu 1 s warten lassen | Messablauf bricht beim ersten Busfehler ab (`3b339bd`), in QEMU bestätigt |
| M7 | Compiler | `-Wpedantic` und `#include_next` in `stdio.h` von ESP-IDF | `-Wpedantic` für proto nur auf dem Host (E-031) |
| M7 | cppcheck | Fehlalarm zu einem nicht belegten Feld, verdeckter Name im Test | vor dem Commit behoben |
| M8 | Gegenprobe | DNS-Test für Kompressionszeiger bestand nur wegen eines zu kurzen Pakets | Test verschärft |
| M8 | Compiler | Aufzählungswert hieß wie das Makro des Platzhalters | umbenannt in `NET_WIFI_PASSWORD_IS_PLACEHOLDER` |
| M8 | Durchsicht vor dem Commit | `strcmp` las einen nicht belegten Pfad | vor dem Commit behoben |
| M9 | Lint, Typprüfung | `eslint-disable` für eine nicht installierte Regel; Typ ließ `unassigned` als Rolle mit Messwert zu | behoben |
| M9 | Sichtprüfung | Zeitformat, doppelte Sensor-ID, Ausrichtung, Umbruch zwischen Zahl und Einheit | vor dem Commit behoben |
| M10 | Vertragstest | `api.yaml` ohne passendes proto committet, Commit rot | vor dem Push zurückgenommen und zusammengelegt |
| M10 | Durchsicht beim Erweitern | `esp_http_server` erlaubt 8 Pfade, die Firmware meldete 9 an (Fehler aus M8) | `max_uri_handlers` 16 mit `_Static_assert` (E-048) |
| M10 | Typprüfung, je Commit | Texte für neue Ergebnisse fehlten in drei Zwischencommits | Commits vor dem Push neu geordnet |
| M10 | Redocly | veraltetes Beispiel für `/api/config` | Beispiel angepasst |
| M10 | Test | falsche Erwartung im Lern-Test von `test_forecast` | Test korrigiert |
| M11 | `requirements_evidence` | veraltete Pfade in `docs/anforderungen.md` | Pfade korrigiert (`040e283`) |
| M11 | Prüfung jedes Commits in frischem Checkout vor dem Push | `requirements_evidence` verlangte `web/dist` und `firmware/sdkconfig`, die nur lokal existieren; in CI wäre der Host-Job rot gewesen | erzeugte Pfade ausgenommen, mit Gegenprobe (E-049) |

## Hardware

Die Tests auf der Platine führt der Nutzer aus. Die Schritte mit Erwartung stehen in `docs/hardwaretest.md`.

| Block | Schritte | Prüft unter anderem | Anforderungen | Ergebnis |
|---|---|---|---|---|
| H7 Firmware-Basis | H7-01 bis H7-10 | Start, Sensoren erkennen, an- und abstecken, gestörter Bus, Neustart, Laufzeit | SAF-10, SEN-03 | ausstehend |
| H8 Netzwerk | H8-01 bis H8-14 | Platzhalter-Passwort, Access Point, Captive Portal, REST, Rolle und Neustart mit Zuordnung, Heizen mit LED, Sensor beim Heizen abziehen, Reset, WebSocket, Flut, WLAN-Abbruch | ARC-05, ARC-09, SEN-04, CTL-03, API-02, API-03, NET-01 bis NET-04 | ausstehend |
| H9 Weboberfläche | H9-01 bis H9-11 | Bedienung am Handy, ungültige Eingaben, sicherer Zustand, Verbindungsverlust, Drehen | CTL-03, NET-03, WEB-04 | ausstehend |
| H10 Prognose | H10-01 bis H10-04 | Grenzen des Kernziels, Anzeige ohne Kernfühler, Prognose mit warmem Wasser | (FC-01 ist im Simulator belegt) | ausstehend |

## Bekannte Grenzen

- **Hardware:** Zehn Anforderungen sind ohne die Tests auf der Platine nicht vollständig belegt. Der Code dafür wird in CI gebaut, die reinen Teile sind auf dem Host getestet.
- **ARC-09 teilweise:** Der Entkopplungstest auf dem Host läuft in einem einzigen Thread mit Fake-Uhr. Er zeigt, dass die Regelung nicht von Befehlen abhängt, aber nicht das Verhalten des FreeRTOS-Schedulers. Dass der Regeltask weder auf die Queue noch auf den Mutex wartet, ist im Code belegt (E-036), nicht durch einen Test. Ein abgestürzter Webserver wird nirgends nachgestellt.
- **Regelgüte:** Gemessen ist sie nur im Simulator (E-024). Mit einer LED statt Heizelement gibt es auf der Hardware keine Strecke, an der sich die Regelgüte prüfen ließe.
- **Prognose:** Simulator und Prognose nutzen dasselbe Modell erster Ordnung. Die kleinen Abweichungen belegen deshalb die Rechnung, nicht die Güte an echtem Grillgut (E-047).
- **QEMU:** bildet weder RMT (1-Wire) noch WLAN noch HTTP nach.
- **Weboberfläche:** Die Tests laufen in jsdom, nicht in einem echten Browser. Das Layout bei 375 px Breite wurde im Browser von Hand geprüft; echte Handys stehen in H9 aus.
- **Abdeckung:** Gemessen und gefordert ist sie nur für `core/safety`.
- **cppcheck:** Lokal läuft 2.22, in CI 2.13. Beide melden unterschiedliche Dinge; maßgeblich ist CI.
- **Queue über HTTP:** Der HTTP-Server bearbeitet Anfragen nacheinander. Über HTTP liegt deshalb nie mehr als ein Befehl in der Queue; volle Queues prüft nur der Host-Test (E-039).

## Zahlen nachprüfen

| Zahl | Befehl |
|---|---|
| Unity und ctest | `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure` |
| Abdeckung | `cmake -S . -B build-cov -G Ninja -DROF_COVERAGE=ON && cmake --build build-cov --target coverage` |
| cppcheck | `cmake --build build --target cppcheck` |
| OpenAPI | `npx --yes @redocly/cli@2.64.0 lint docs/api.yaml` |
| Weboberfläche | `npm --prefix web ci && npm --prefix web run lint && npm --prefix web run typecheck && npm --prefix web test && npm --prefix web run build` |
| Größenbudget | `python3 firmware/tools/gzip_web.py web/dist /tmp/www --max-bytes 81920` |
| Firmware | `. ~/esp/esp-idf/export.sh && idf.py -C firmware build` |
