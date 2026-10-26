# Entscheidungen

Jede nicht offensichtliche Entscheidung mit Kontext, Entscheidung und verworfenen Alternativen. Neue Einträge kommen ans Ende.

## E-001 GitHub statt Gitea (M0)

- **Kontext:** CLAUDE.md sah Gitea mit Gitea Actions vor. Vorhanden ist nur ein GitHub-Konto.
- **Entscheidung:** Hosting auf GitHub, CI mit GitHub Actions in `.github/workflows/`. CLAUDE.md ist entsprechend angepasst.
- **Verworfen:** Einen eigenen Gitea-Server mit Runner aufsetzen. Das kostet Zeit und bringt der Software nichts. Gitea Actions liest `.github/workflows/` ebenfalls, ein späterer Wechsel bleibt also möglich.

## E-002 Unity als Kopie im Repository (M0)

- **Kontext:** Die Host-Tests brauchen das Testframework Unity. Builds sollen offline und mit fester Version funktionieren.
- **Entscheidung:** Unity v2.6.1 liegt unverändert unter `test/vendor/unity/`. Quelle und Prüfsumme stehen in der README dort. Unity bekommt keine strengen Warnungen und wird von cppcheck ausgenommen, weil es nicht unser Code ist.
- **Verworfen:**
  - CMake `FetchContent`: braucht bei jedem frischen Konfigurieren Netz.
  - Git-Submodule: Klonen und CI brauchen `--recursive`, das ist eine Fehlerquelle mehr.
  - Das Unity aus ESP-IDF: Es ist für den Firmware-Build gedacht, nicht für den Host.

## E-003 Strenges C11 und Warnungen als Fehler (M0)

- **Kontext:** core läuft später auf dem Host (clang, gcc) und auf dem ESP32 (xtensa-gcc). Code, der nur auf einem Compiler funktioniert, soll früh auffallen.
- **Entscheidung:** `CMAKE_C_EXTENSIONS OFF`, also `-std=c11` statt `-std=gnu11`. Eigene Targets linken `project_warnings` mit `-Wall -Wextra -Wpedantic -Werror`. Generator ist Ninja, den nutzt auch ESP-IDF.
- **Verworfen:**
  - `gnu11`: GNU-Erweiterungen würden unbemerkt nicht portablen Code zulassen.
  - Warnungen nur anzeigen: Sie bleiben erfahrungsgemäß liegen.

## E-004 cppcheck als CMake-Target (M0)

- **Kontext:** cppcheck soll lokal und in CI mit genau denselben Optionen laufen.
- **Entscheidung:** Das Target `cppcheck` im Haupt-`CMakeLists.txt` liest `build/compile_commands.json` und prüft damit genau die Dateien, die auch gebaut werden. CI ruft nur `cmake --build build --target cppcheck` auf.
- **Verworfen:**
  - Den Aufruf direkt in `ci.yml` schreiben: Dann gäbe es zwei Stellen, die auseinanderlaufen.
  - cppcheck mit einer Verzeichnisliste aufrufen: Die Liste müsste bei jedem neuen Ordner angepasst werden.

## E-005 Anforderungen mit IDs (M0)

- **Kontext:** In M11 muss jede Anforderung ihrem Test zugeordnet werden.
- **Entscheidung:** `docs/anforderungen.md` vergibt IDs nach Bereich (ARC, SAF, SEN, CTL, API, NET, WEB, CI, FC). Tests nennen ihre ID im Kommentar, die Spalte „Nachweis“ wird je Meilenstein gepflegt.
- **Verworfen:** Die Zuordnung erst in M11 nachträglich rekonstruieren. Das ist aufwendig und fehleranfällig.

## E-006 Englische Bezeichner im Code, deutsches Glossar (M1)

- **Kontext:** CLAUDE.md und die Anforderungen benennen Rollen, Zustände und Interfaces auf Deutsch. C-Standardbibliothek und ESP-IDF sind englisch.
- **Entscheidung:** Bezeichner im Code sind englisch. `docs/glossar.md` ordnet jeden deutschen Begriff verbindlich zu. „Kern“ wird zu `PROBE`, nicht zu `CORE`, damit es nicht mit dem Ordner `core/` verwechselt wird.
- **Verworfen:** Deutsche Bezeichner. Sie entsprechen den Anforderungen 1:1, mischen sich aber in jeder Zeile mit englischem C und ESP-IDF.
