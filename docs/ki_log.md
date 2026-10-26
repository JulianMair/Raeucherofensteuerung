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
