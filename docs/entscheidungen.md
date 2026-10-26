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

## E-007 Interfaces als Struct mit ctx und Funktionszeigern (M1)

- **Kontext:** core darf nur über Interfaces mit Hardware, Uhr und Speicher sprechen (ARC-02). Auf dem Host stehen Mocks dahinter, auf dem ESP32 Treiber. DS18B20 und später PT1000 sollen gleichzeitig laufen können (SEN-06).
- **Entscheidung:** Jedes Interface ist ein Struct aus `void *ctx` und Funktionszeigern. `ctx` zeigt auf den Zustand der Implementierung und wird an jede Funktion durchgereicht, ähnlich wie `this`. Zu jedem Interface gibt es kleine `static inline`-Hilfsfunktionen wie `rof_clock_now_ms(&clk)`, damit niemand den falschen `ctx` übergibt. `ctx` ist nicht `const`, weil Implementierungen beim Aufruf ihren Zustand ändern dürfen. Ein Mock zählt zum Beispiel Aufrufe, ein Treiber startet eine Messung. Die cppcheck-Meldung `constParameterCallback` ist deshalb abgeschaltet.
- **Interfaces sind Pflicht:** Die Hilfsfunktionen prüfen nicht auf `NULL`. Jede Prüfung wäre ein zusätzlicher Zweig in jedem Modul, das sie benutzt, und erschwert die 100 % Zweigabdeckung in `core/safety`. Module prüfen ihre Interfaces einmal bei der Initialisierung.
- **Verworfen:**
  - Austausch beim Linken (eine `.c` je Plattform hinter demselben Header): Damit gäbe es nur eine Implementierung pro Programm, also nicht zwei Sensortreiber gleichzeitig. Auch Tests könnten nicht pro Testfall eine andere Implementierung einsetzen.
  - `const void *ctx`: Das passt nur für reine Lesefunktionen und würde je Interface unterschiedliche Regeln erzeugen.
