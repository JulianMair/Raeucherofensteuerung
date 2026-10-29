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

## E-008 Einheiten an den Interfaces (M1)

- **Kontext:** Alle Module tauschen Temperaturen, Zeiten und Tastgrade aus. Einheitliche Typen verhindern Umrechnungsfehler.
- **Entscheidung:**
  - Temperatur als `float` in °C. Der ESP32-S3 hat eine Gleitkommaeinheit für `float`, und Werte wie 85,5 °C bleiben lesbar. Tests vergleichen Gleitkommawerte nur mit Toleranz (`TEST_ASSERT_FLOAT_WITHIN`).
  - Zeit als `uint64_t` in Millisekunden seit dem Start, monoton. Ein `uint32_t` liefe nach 49 Tagen über und bräuchte Sonderbehandlung.
  - Tastgrad als `float` von 0.0 bis 1.0.
- **Verworfen:** Ganzzahlen in Hundertstelgrad. Sie sind auf Chips ohne Gleitkommaeinheit üblich, hier aber unnötig und schwerer zu lesen.

## E-009 Sensor-ID als 64-Bit-Zahl, Sensortyp als Text (M1)

- **Kontext:** SEN-01 verlangt je Kanal eine eindeutige ID und einen Typ. Ein neuer Sensortyp soll nur als Treiber hinzukommen, ohne Änderung in core (SEN-06).
- **Entscheidung:** Die ID ist ein `uint64_t`, in den die ROM-Adresse des DS18B20 genau passt. Den Typ liefert der Treiber als konstanten Text, z. B. `"DS18B20"`. core speichert und meldet ihn, wertet ihn aber nie aus.
- **Verworfen:** Ein Enum mit allen Sensortypen in core. Jeder neue Typ hätte dann eine Änderung in core bedeutet.

## E-010 Include-Regeln als Positivliste, geprüft per ctest (M1)

- **Kontext:** CI soll prüfen, dass core und proto keine ESP- oder FreeRTOS-Header einbinden und dass firmware/net keine Treiber einbindet (CI-02). Bei einem Verstoß soll der Lauf fehlschlagen.
- **Entscheidung:**
  - Ein Python-Skript ohne Fremdpakete (`test/include_rules/check_includes.py`) läuft als ctest-Test. Damit prüft es lokal und in CI dasselbe, `ci.yml` bleibt unverändert.
  - core hat eine Positivliste: nur eigene Header und sieben Standard-Header. Weil `stdlib.h` fehlt, kann core kein `malloc` aufrufen. Ein Aufruf ohne Deklaration ist in C11 mit `-Werror` ein Fehler. Damit gilt für core „gar kein malloc“, strenger als „kein malloc nach der Initialisierung“ (ARC-01). Weil `time.h` fehlt, kann core die Zeit nur über `rof_clock_t` lesen (ARC-03). Weil `stdio.h` fehlt, gibt core Meldungen nur über `rof_log_t` aus.
  - proto darf die ganze C-Standardbibliothek, eigene Header und `core/include` einbinden. Das wird in M6 nachgeschärft.
  - firmware/net darf weder ESP-IDF-Treiber (`driver/…`) noch Header aus `platform/esp32` einbinden. Der Pfad wird in M8 an die echte Ordnerstruktur angepasst.
  - Gegenproben: Kleine Testordner mit je einem absichtlichen Verstoß. ctest prüft dort die genaue Meldung, nicht nur den Exit-Code.
- **Verworfen:**
  - Eine Negativliste (`esp_*`, `freertos/*`): Sie übersieht jeden Header, an den niemand gedacht hat.
  - `grep` direkt in `ci.yml`: läuft lokal nicht mit und ist schwer zu testen.
  - Sich darauf verlassen, dass ESP-Header auf dem Host fehlen: Das deckt Dateien außerhalb des Host-Builds und firmware/net nicht ab.

## E-011 Lebensdauer der Kanäle und Vorrang zugeordneter Sensoren (M2)

- **Kontext:** Es gibt höchstens 8 Kanäle (SEN-01). Sensoren werden an- und abgesteckt, und es können mehr als 8 stecken (SEN-07). Ein fehlender zugeordneter Sensor muss als Fehler sichtbar bleiben (SEN-05).
- **Entscheidung:**
  - Ein Kanal bleibt, solange sein Sensor steckt oder er eine Rolle oder einen Offset trägt. Abgezogene Sensoren ohne Einstellungen geben ihren Kanal am Ende von `update()` frei.
  - Bekannte Kanäle behalten ihren Platz, auch zugeordnete ohne Sensor. Neue Sensoren bekommen nur freie Kanäle, sonst wird `overflow` gesetzt. Ein frei gewordener Kanal steht deshalb erst ab dem nächsten `update()` zur Verfügung.
  - `update()` fragt je Treiber bis zu 16 Sensoren ab (`ROF_SENSOR_SCAN_MAX`), doppelt so viele wie Kanäle. So wird ein zugeordneter Sensor auch hinter acht neuen noch gefunden. Steckt er hinter mehr als 16, gilt er als fehlend. Das ist die sichere Seite.
  - Fehler (fehlender Sensor, doppelte ID, Überlauf) werden einmal je Auftreten geloggt, nicht bei jedem `update()`.
- **Verworfen:**
  - Kanäle in der Reihenfolge des Busses neu vergeben: Ein neuer Sensor könnte einen zugeordneten verdrängen.
  - Nur 8 Sensoren je Treiber abfragen: Ein zugeordneter Sensor an neunter Stelle würde fälschlich fehlen.

## E-012 Speicherformat der Sensorzuordnung (M2)

- **Kontext:** Rollen und Offsets müssen einen Neustart überleben (SEN-04). Eine spätere Firmware kann das Format ändern, und gespeicherte Daten können beschädigt sein.
- **Entscheidung:** Ein Block unter dem Schlüssel `sensor_roles`: Versions-Byte, Anzahl, dann je Kanal ID (8 Byte), Rolle (1 Byte) und Offset (4 Byte). Mehrbytewerte stehen mit dem niederwertigsten Byte zuerst. Gespeichert werden nur Kanäle mit Rolle oder Offset. Beim Laden wird der ganze Block verworfen, sobald irgendetwas nicht stimmt: Version, Länge, Rolle, Offset, doppelte ID oder doppelte Rolle. Die Verwaltung startet dann ohne Zuordnung, der Garraum ist nicht zugeordnet und das Heizen gesperrt.
- **Verworfen:**
  - Das Struct direkt mit `memcpy` speichern: Füllbytes und Feldreihenfolge hängen vom Compiler ab, und ein neues Feld macht alte Daten unlesbar, ohne dass es jemand merkt.
  - Nur fehlerhafte Einträge überspringen und den Rest übernehmen: Bei teilweise beschädigten Daten wäre unklar, welcher Sensor wirklich der Garraum ist.

## E-013 Doppelte Sensor-IDs werden nicht gelesen (M2)

- **Kontext:** Meldet ein Bus oder melden zwei Treiber dieselbe ID, ist unklar, welcher Sensor beim Lesen antwortet (SEN-07).
- **Entscheidung:** Der Kanal wird als `duplicate` markiert und nicht gelesen. Seine Rolle meldet `ROF_ROLE_STATUS_DUPLICATE`, beim Garraum ist das in M3 ein Auslöser. Die Markierung verschwindet, sobald die ID wieder eindeutig ist.
- **Verworfen:**
  - Den ersten Treffer lesen: Er kann vom falschen Sensor stammen, ohne dass es auffällt.
  - Die doppelte Meldung ignorieren: Der Fehler bliebe unsichtbar.

## E-014 Rollen verschieben, Offset ±5 °C, Speichern als Transaktion (M2)

- **Kontext:** Beim Zuweisen kann die Rolle schon vergeben sein. Für den Offset braucht es Grenzen. Und das Speichern kann scheitern.
- **Entscheidung:**
  - Vom Nutzer entschieden: Eine vergebene Rolle wird verschoben, der bisherige Sensor ist danach nicht zugeordnet. Es gibt keinen Moment ohne Garraumsensor.
  - Vom Nutzer entschieden: Der Offset darf zwischen −5 °C und +5 °C liegen. Werte außerhalb und NaN werden abgelehnt, nicht begrenzt. Der DS18B20 ist auf ±0,5 °C genau; eine größere Abweichung spricht eher für einen defekten Sensor.
  - Zuweisen und Offset setzen speichern sofort. Scheitert das Speichern, wird die Änderung im RAM zurückgenommen und der Fehler gemeldet. RAM und Flash stimmen so immer überein.
- **Verworfen:**
  - Eine vergebene Rolle ablehnen: Man müsste erst die alte Zuordnung lösen. Dazwischen gäbe es keinen Garraumsensor.
  - Die Änderung trotz Speicherfehler behalten: Nach dem nächsten Neustart wäre sie stillschweigend weg.

## E-015 Rollenstatus mit Grund statt Ja oder Nein (M2)

- **Kontext:** Die Sicherheitslogik in M3 und die Oberfläche in M9 müssen wissen, ob eine Rolle einen gültigen Wert hat, und wenn nicht, warum.
- **Entscheidung:** `rof_sensor_registry_role()` liefert einen Status (`OK`, `UNASSIGNED`, `MISSING`, `READ_ERROR`, `DUPLICATE`) und schreibt die Temperatur nur bei `OK`. Ein Aufrufer kann einen ungültigen Wert so nicht versehentlich verwenden.
- **Verworfen:** Ein `bool` mit Temperatur. Der Grund für den Fehler ginge verloren, und die Oberfläche könnte ihn nicht anzeigen.

## E-016 Grenzwerte der Sicherheitslogik (M3)

- **Kontext:** Die Sicherheitslogik braucht Grenzen für plausible Werte, Sollwerte, eingefrorene Werte und den Regelzyklus sowie eine harte Obergrenze (SAF-04 bis SAF-09).
- **Entscheidung:** Vom Nutzer bestätigt:
  - Harte Grenze 120 °C. Sie ist eine Konstante beim Bauen (`ROF_SAFETY_HARD_MAX_C`), nicht Teil der Konfiguration und damit nicht über die API änderbar (SAF-06). Sie liegt unter dem Messbereichsende des DS18B20 von 125 °C; eine Grenze darüber würde nie auslösen.
  - Plausibel −20 bis 125 °C, nur für den Garraum, denn nur er hat eine Sicherheitsrolle.
  - Sollwerte 20 bis 110 °C. `init` verlangt, dass der Sollwertbereich im plausiblen Bereich und echt unter der harten Grenze liegt, sonst würde ein erlaubter Sollwert die Sicherheitslogik auslösen.
  - Eingefroren: weniger als 0,05 °C Änderung in 10 Minuten bei aktivem Ausgang. 0,05 °C liegt knapp unter einem Messschritt des DS18B20 (0,0625 °C), jeder Messschritt zählt also als Änderung.
  - Zeitfenster des Regelzyklus 3 Sekunden.
- **Verworfen:**
  - Eine harte Grenze in der Konfiguration: Sie wäre über die API änderbar und widerspräche SAF-06.
  - Plausibilität für alle Sensoren: Ein defekter Kernfühler würde die Heizung abschalten, obwohl die Heizung nur auf den Garraum wirkt.

## E-017 Zweigabdeckung mit gcov und gcovr (M3)

- **Kontext:** `core/safety` muss 100 Prozent Zweigabdeckung haben, gemessen mit gcov in CI (SAF-12).
- **Entscheidung:**
  - Die CMake-Option `ROF_COVERAGE` baut core mit `--coverage -O0` in einem eigenen Ordner `build-cov`. Das Target `coverage` führt alle Tests aus und lässt gcovr nur über `core/safety` laufen. Es scheitert unter 100 Prozent Zeilen oder Zweige. Lokal und in CI läuft derselbe Befehl, wie bei cppcheck (E-004).
  - Zweige, die man aus C nicht erreicht, werden durch Umbau des Codes vermieden, nicht von der Messung ausgenommen. Beispiel: `isnan()` ist auf macOS ein Makro mit eigenen Zweigen für `double` und `long double`, die ein `float` nie nimmt. Die Sicherheitslogik erkennt NaN deshalb daran, dass jeder Vergleich mit NaN falsch ist.
  - Maßgeblich ist die Messung in CI mit GCC. Lokal misst Apples gcov aus LLVM, das Zweige leicht anders zählen kann.
- **Verworfen:**
  - Ausnahmen per Kommentar (`GCOVR_EXCL_BR_LINE`): Sie würden 100 Prozent vortäuschen, ohne dass jeder Zweig getestet ist.
  - Abdeckung über alle Module: Gefordert ist sie für `core/safety`. Eine Grenze für alle würde Fehler in der Sicherheitslogik hinter gut abgedeckten anderen Modulen verstecken.

## E-018 Aufbau der Sicherheitslogik (M3)

- **Kontext:** Die Sicherheitslogik muss in jedem Regelzyklus vor dem Regler laufen, bei einem Fehler den Ausgang auf 0 halten und erst nach einem manuellen Reset bei behobener Ursache wieder freigeben (SAF-01, SAF-02). Sie braucht 100 Prozent Zweigabdeckung (SAF-12).
- **Entscheidung:**
  - Die Eingabe je Zyklus ist ein einfaches Struct `rof_safety_input_t` mit Rollenstatus und Wert des Garraums und dem angelegten Tastgrad. Die Sicherheitslogik ruft die Sensorverwaltung nicht selbst auf. Das hält sie klein, voll testbar und unabhängig vom Ablauf im Regelzyklus.
  - Die erste auslösende Ursache wird verriegelt und einmal geloggt. Spätere Ursachen ändern sie nicht, damit der Grund der Abschaltung erhalten bleibt.
  - `rof_safety_limit_duty()` liefert 0, solange verriegelt ist. Der Regelzyklus gibt jeden Tastgrad durch diese Funktion, bevor er ihn setzt (wird in M5 belegt).
  - Ein Reset prüft alle Regeln mit der aktuellen Eingabe. Er hebt die Verriegelung nur auf, wenn keine Regel auslöst, und startet die Zeit für eingefrorene Werte neu.
  - Der Watchdog misst den Abstand zwischen zwei Zyklen, nicht die Dauer eines Zyklus (vom Nutzer gewählt). So fällt auch ein Regeltask auf, der gar nicht mehr drankommt.
  - Ein unbekannter Rollenstatus gilt als Lesefehler, nie als gültig.
  - Logtexte stehen in einer Tabelle statt in einem `switch`, damit Logging keine Zweige erzeugt, die nur für die Abdeckung getestet werden müssten.
- **Verworfen:**
  - Die Sicherheitslogik liest die Sensorverwaltung selbst: Sie hinge dann an deren Ablauf, und Tests bräuchten eine vollständige Sensorverwaltung.
  - Die letzte statt der ersten Ursache verriegeln: Folgefehler würden den eigentlichen Grund überschreiben.
  - Den Ausgang direkt über `rof_output_t` abschalten: Dann gäbe es zwei Stellen, die den Ausgang setzen. Der Regelzyklus bleibt die einzige.

## E-019 cppcheck-Funde als GitHub-Annotationen, Zeitlimit für den CI-Job (M3)

- **Kontext:** In M2 meldete cppcheck 2.13 auf dem Runner einen Fund, den cppcheck 2.22 lokal nicht meldete. Das CI-Log gibt GitHub nur angemeldet heraus, Claude konnte den Fund deshalb nicht selbst lesen. Außerdem hing ein Lauf 7 Minuten an einem langsamen Paketspiegel; ohne Grenze darf ein Job 6 Stunden laufen.
- **Entscheidung:** Vom Nutzer gewählt:
  - Die Cache-Variable `ROF_CPPCHECK_TEMPLATE` setzt das Ausgabeformat von cppcheck. CI setzt sie auf das Format eines GitHub-Workflow-Befehls (`::error file=…,line=…::…`). GitHub zeigt jeden Fund dann als Annotation am Commit, und die Annotationen sind über die API ohne Anmeldung lesbar. Lokal bleibt das gewohnte Format.
  - cppcheck gibt Pfade relativ zum Projekt aus (`--relative-paths`). Das braucht die Annotation, und lokal ist es lesbarer.
  - Der CI-Job bricht nach 15 Minuten ab (`timeout-minutes`).
- **Verworfen:**
  - In CI und lokal dieselbe cppcheck-Version erzwingen: Ubuntu liefert 2.13, Homebrew die aktuelle Version. Eine eigene Version in CI zu bauen kostet bei jedem Lauf Minuten.
  - Die Funde als Datei-Artefakt hochladen: Artefakte gibt GitHub ebenfalls nur angemeldet heraus.
