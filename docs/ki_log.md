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

**Vom Nutzer geändert oder abgelehnt:** wird nach dem Review von M3 ergänzt.
