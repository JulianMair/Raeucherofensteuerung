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

## E-020 FEHLER als Startsperre, Verriegeln nur beim Heizen (M4)

- **Kontext:** CLAUDE.md nennt die Zustände `FEHLER` und `SICHERER_ZUSTAND`, legt aber nicht fest, wie sie sich unterscheiden. Bisher (E-018) verriegelte die Sicherheitslogik bei jedem Fehler, auch im Leerlauf. Ein fehlender Garraumsensor beim Einschalten führte damit sofort in den sicheren Zustand, obwohl der Ausgang ohnehin aus war, und verlangte jedes Mal einen Reset.
- **Entscheidung:** Vom Nutzer gewählt:
  - Die Sicherheitslogik prüft weiter in jedem Zyklus alle Regeln (SAF-01). Sie verriegelt aber nur, wenn der Ausgang aktiv sein darf (AUFHEIZEN, HALTEN; im Eingabe-Struct `heating`) oder bei Übertemperatur. Übertemperatur verriegelt immer, weil sie auch bei Ausgang 0 entstehen kann, etwa durch ein klemmendes Relais.
  - Ohne Heizen bleibt jede andere Ursache nur aktiv (`rof_safety_active()`). Der Zustandsautomat geht dann in `FEHLER`: Ausgang 0, Start gesperrt. Ist die Ursache weg, geht er von selbst zurück nach `BEREIT`, ohne Reset.
  - `rof_safety_limit_duty()` liefert 0 bei Verriegelung und auch bei einer nur aktiven Ursache. „Bei einem Fehler ist der Ausgang 0“ gilt damit in jedem Zustand.
- **Abweichung vom Wortlaut:** CLAUDE.md verlangt, dass bei einem Fehler `SICHERER_ZUSTAND` gesetzt bleibt. Ohne Heizen ist es jetzt `FEHLER`. Der Ausgang ist in beiden Fällen 0; der Unterschied betrifft nur, ob ein Reset nötig ist.
- **Verworfen:** In jedem Zustand verriegeln und `FEHLER` für nicht sicherheitsrelevante Fehler verwenden, etwa einen ausgefallenen Kernfühler. Das entspricht dem Wortlaut, verlangt aber nach jedem Einschalten ohne Garraumsensor einen Reset und gibt `FEHLER` eine Bedeutung, die CLAUDE.md nicht fordert.

## E-021 Schwellen für die Zustandswechsel (M4)

- **Kontext:** AUFHEIZEN, HALTEN und ABKUEHLEN brauchen Bedingungen für den Wechsel. CLAUDE.md legt keine fest.
- **Entscheidung:** Vom Nutzer bestätigt, konfigurierbar in `rof_fsm_config_t`:
  - AUFHEIZEN → HALTEN, sobald der Garraum Sollwert − 2 °C erreicht.
  - HALTEN → AUFHEIZEN, wenn er unter Sollwert − 10 °C fällt, etwa bei offener Tür oder einem deutlich erhöhten Sollwert.
  - ABKUEHLEN → BEREIT, sobald er unter 50 °C liegt.
  - `init` verlangt, dass das Rückkehrband größer ist als das Halteband. Der Abstand zwischen beiden (Hysterese) verhindert, dass der Zustand bei kleinen Schwankungen um den Sollwert ständig wechselt.
- **Verworfen:**
  - Eine einzige Schwelle für Hin- und Rückweg: Schon kleine Schwankungen würden den Zustand hin- und herschalten.
  - HALTEN bis zum Stopp, ohne Rückkehr nach AUFHEIZEN: Nach offener Tür oder erhöhtem Sollwert zeigte die Oberfläche fälschlich „Halten“.

## E-022 Regeln für Befehle im Zustandsautomaten (M4)

- **Kontext:** Befehle kommen über die Queue aus dem Netz (ARC-05) und müssen im Regeltask geprüft werden (ARC-06). Ein ungültiger Befehl darf den Zustand nicht ändern.
- **Entscheidung:**
  - Jeder Befehl liefert ein `rof_cmd_result_t`. Bei Ablehnung bleibt der Zustand, und der Grund wird gemeldet.
  - Start ohne gesetzten Sollwert wird abgelehnt (`NO_SETPOINT`, vom Nutzer gewählt). Nach einem Neustart ist kein Sollwert gesetzt, geheizt wird also nur auf einen bewusst gesetzten Wert.
  - Start ist aus BEREIT und aus ABKUEHLEN erlaubt; aus ABKUEHLEN heizt der Ofen einfach wieder.
  - Stopp ohne Heizen tut nichts und gilt als erfolgreich. Ein Stopp-Knopf soll nie mit einer Fehlermeldung antworten, wenn der Ausgang ohnehin aus ist.
  - Rollen zuweisen ist während AUFHEIZEN und HALTEN gesperrt (vom Nutzer gewählt). Wer mitten im Heizen den Garraum auf einen anderen Fühler legt, regelte sonst plötzlich nach einem falschen Wert. Die Sperre fragt der Regelzyklus über `rof_fsm_roles_editable()` ab.
  - Den Reset führt der Regelzyklus über `rof_safety_reset()` aus. Der Automat sieht danach „nicht mehr verriegelt“ und geht nach BEREIT (Ü10), nie direkt zurück ins Heizen.
- **Verworfen:**
  - Ein Standardsollwert: Nach einem Neustart könnte versehentlich auf einen Wert geheizt werden, den niemand gewählt hat.
  - Stopp ohne Heizen ablehnen: Für Bedienende verwirrend, ohne Sicherheitsgewinn.

## E-023 Langsame PWM als reine Funktion in core (M5)

- **Kontext:** Der Ausgang soll als langsame PWM mit 4 s Periode schalten (CTL-03). Das Interface des Ausgangs bleibt „Tastgrad setzen“ (ARC-02).
- **Entscheidung:** Vom Nutzer gewählt: `rof_slow_pwm_on(period_ms, duty, now_ms)` in core sagt nur aus Tastgrad und Uhrzeit, ob der Ausgang gerade an ist. In jedem Fenster ist er zuerst `duty * period_ms` lang an, dann aus. Die Funktion hat keinen Zustand und ist auf dem Host voll getestet. Der Simulator schaltet seine Heizung damit, der LED-Treiber nutzt sie in M7. Ungültige Werte schalten aus, nie an.
- **Verworfen:** Die PWM nur im Treiber. Sie wäre dann nur auf der Hardware prüfbar, und der Simulator hätte mit dem Mittelwert statt mit echtem Ein und Aus gerechnet.

## E-024 PI-Regler: Einstellung nach SIMC, Anti-Windup durch angehaltene Integration (M5)

- **Kontext:** Der Regler braucht begründete Werte für Kp und Ti (CTL-02) und darf nach langem Aufheizen nicht überschwingen.
- **Entscheidung:**
  - Kp und Ti folgen der SIMC-Regel nach Skogestad für eine Strecke erster Ordnung mit Totzeit: Verstärkung 130 °C, Zeitkonstante 600 s, Totzeit 30 s plus etwa 2 s durch die PWM. Mit der Zielzeitkonstante gleich der Totzeit ergibt das Kp ≈ 600 / (130 · 64) ≈ 0,072, gerundet 0,075 pro °C, und Ti = min(600, 4 · 64) ≈ 250 s.
  - Anti-Windup: Steht der Ausgang am Anschlag und drückt der Fehler weiter in dieselbe Richtung, wird der I-Anteil nicht weiter aufaddiert. Das ist einfach zu erklären und zu testen.
  - Beim Eintritt ins Heizen beginnt der Regler mit leerem I-Anteil. Eine Regelabweichung, die kein Messwert sein kann (NaN, unendlich, über 1000 °C), ergibt Tastgrad 0 und lässt den I-Anteil unberührt.
  - Gemessen im Simulator bei 80 °C: HALTEN nach 8,8 min, Überschwingen 0,19 °C, danach 79,81 bis 80,19 °C. Nachstellen war nicht nötig.
- **Verworfen:**
  - Werte ausprobieren, bis es passt: nicht begründbar.
  - Anti-Windup durch Rückrechnung (back-calculation): wirksam, aber eine Verstärkung mehr, die man begründen und einstellen müsste.

## E-025 Simulator und Abnahmekriterien (M5)

- **Kontext:** Die Regelung muss auf dem Host über Stunden getestet werden (CTL-01, CTL-06).
- **Entscheidung:** Vom Nutzer bestätigt:
  - Umgebung 20 °C, Heizung bei Dauer-An +130 °C, Garraum Zeitkonstante 10 min und Totzeit 30 s, Kern 60 min und 2 min, PWM-Periode 4 s.
  - Der Simulator rechnet in Schritten von 100 ms nach dem expliziten Euler-Verfahren mit `double`, weil er nur auf dem Host läuft. Die Totzeiten stecken in Ringpuffern. Er ist zugleich die Uhr der Integrationstests: Simulierte Zeit läuft nur mit `sim_oven_advance()`.
  - Er liegt in einer eigenen Bibliothek `host_sim`, weil er die PWM aus core nutzt; die Mocks kennen weiterhin nur die Interfaces.
  - Abnahme bei Sollwert 80 °C: HALTEN nach höchstens 30 min, Überschwingen höchstens 3 °C, danach 6 Stunden innerhalb ±1 °C.
- **Verworfen:** Ein Modell zweiter Ordnung oder mit Wärmeverlust durch Rauchabzug: genauer, aber mit Parametern, die niemand für diesen Ofen kennt. Für den Nachweis der Regelung reicht erste Ordnung mit Totzeit.

## E-026 Befehle, Befehlsquelle und Snapshot (M5)

- **Kontext:** Befehle kommen über eine Queue an den Regeltask, der jeden selbst prüft (ARC-05, ARC-06). Der Zustand geht nur als Kopie nach außen (ARC-07). Eine volle Queue darf die Regelung nie blockieren (ARC-09). core darf FreeRTOS nicht kennen.
- **Entscheidung:**
  - Ein Befehl ist ein flaches Struct ohne Zeiger, damit er unverändert durch eine FreeRTOS-Queue passt. Der Befehlstyp beginnt bei 1, ein mit 0 gefüllter Befehl ist also nie gültig.
  - core kennt nur das Interface `rof_command_source_t`, dessen `receive()` nie wartet. In M7 steckt die FreeRTOS-Queue dahinter, auf dem Host ein Ringpuffer mit 16 Plätzen.
  - Je Zyklus bearbeitet der Regelzyklus höchstens 4 Befehle, der Rest wartet. Ein überlasteter Webserver kann die Regelung so nicht verzögern.
  - Neben Sollwert, Start, Stopp, Rolle zuweisen und Reset gibt es „Offset setzen“. Ohne ihn wäre der Kalibrieroffset (SEN-01) nie einstellbar; der Nutzer hat dem Plan mit diesem Befehl zugestimmt. Rolle und Offset sind beim Heizen gesperrt, weil beides den Messwert verschiebt, nach dem geregelt wird (E-022).
  - Vom Nutzer gewählt: Ergebnisse gehen über den Snapshot zurück. Jeder Befehl hat eine laufende Nummer. Der Regelzyklus hält die letzten 8 Ergebnisse, also zwei volle Zyklen, damit ein Absender sein Ergebnis auch dann findet, wenn mehrere Befehle in einen Zyklus fallen. `rof_snapshot_command_result()` sucht nach der Nummer.
  - Der Snapshot ist eine vollständige Kopie ohne Zeiger in den Regelzyklus; nur der Sensortyp zeigt auf den konstanten Text des Treibers.
- **Verworfen:**
  - Eine eigene Antwort-Queue: ein zweiter Weg aus dem Regeltask, und bei voller Antwort-Queue müsste er Ergebnisse verwerfen.
  - Nur das letzte Ergebnis im Snapshot: Bei mehreren Befehlen in einem Zyklus fände ein Absender sein Ergebnis nicht.

## E-027 Feste Reihenfolge im Regelzyklus (M5)

- **Kontext:** Die Sicherheitslogik muss vor dem Regler laufen (SAF-01) und das letzte Wort über den Ausgang haben (SAF-02).
- **Entscheidung:** `rof_controller_cycle()` legt die Reihenfolge an genau einer Stelle fest: Befehle, Sensoren, Sicherheitslogik (mit `heating` aus dem Zustandsautomaten), Zustandsautomat, PI-Regler nur in AUFHEIZEN und HALTEN, `rof_safety_limit_duty()`, Ausgang. `rof_controller_init()` setzt den Ausgang ausdrücklich auf 0 (SAF-10). Der Zeitschritt des Reglers kommt aus der Uhr, nicht aus einer festen Annahme.
- **Verworfen:** Die Reihenfolge erst in der Firmware festlegen: Sie wäre dann nicht auf dem Host testbar.

## E-028 Form der API (M6)

- **Kontext:** `docs/api.yaml` ist die einzige Quelle der Schnittstelle (API-01). Webserver (M8) und Oberfläche (M9) richten sich danach.
- **Entscheidung:**
  - REST für Lesen und Befehle, ein WebSocket `/ws`, der jede Sekunde denselben Zustand wie `GET /api/state` schickt.
  - Sensor-IDs sind Texte aus 16 Hex-Ziffern. JavaScript stellt ganze Zahlen nur bis 2^53 genau dar, eine 64-Bit-ROM-Adresse ginge als Zahl kaputt.
  - Enum-Werte sind englische Texte in Kleinbuchstaben, wie die Bezeichner im Code (E-006). Temperaturen ohne gültigen Wert sind `null`, weil JSON kein NaN kennt.
  - HTTP-Status nach Ursache: 400 kaputte oder unvollständige Anfrage, 409 Befehl passt nicht zum Zustand, 413 zu groß, 422 Wert außerhalb der Grenzen, 500 Speicherfehler, 503 Queue voll, 504 kein Ergebnis nach 2 s.
  - Die Grenzen stehen im Snapshot und sind über `GET /api/config` lesbar. So prüfen proto und die Oberfläche vorab, ohne core aufzurufen (ARC-04); der Regeltask prüft trotzdem selbst (ARC-06).
  - Das Beispiel für den Zustand liegt als eigene Datei `docs/api-examples/state.json`. proto muss genau dieses JSON erzeugen (Test), und die Oberfläche kann es in M9 als Testdaten nutzen.
- **Verworfen:**
  - IDs als JSON-Zahlen: in JavaScript nicht genau darstellbar.
  - Deutsche Enum-Texte: Code und API wären unterschiedlich benannt.
  - Nur `GET /api/state`, Grenzen fest in der Oberfläche: Bei einer anderen Konfiguration würden Oberfläche und Gerät auseinanderlaufen.

## E-029 Prüfung der OpenAPI-Datei mit Redocly (M6)

- **Kontext:** Eine fehlerhafte `api.yaml` soll sofort auffallen, nicht erst in M9 (vom Nutzer gewählt).
- **Entscheidung:** CI prüft `docs/api.yaml` mit `@redocly/cli` in fester Version 2.64.0 über `npx`. `redocly.yaml` nutzt `recommended-strict`, damit jede Warnung die CI rot macht. Drei Stellen sind einzeln in `.redocly.lint-ignore.yaml` ausgenommen, jede mit Begründung: `GET /api/state` und `GET /api/config` haben keinen 4xx-Fall, und ein WebSocket antwortet mit 101 statt 2xx.
- **Verworfen:**
  - Die beiden Regeln ganz abschalten: Dann fielen auch echte Lücken bei anderen Pfaden nicht mehr auf.
  - Erfundene 4xx- oder 2xx-Antworten eintragen: Die Datei würde etwas beschreiben, das es nicht gibt.
  - Die neueste Version: Sie war erst zwei Tage alt. Eine Version mit gut zwei Wochen Abstand ist für eine feste Prüfung stabiler.

## E-030 Eigener strenger JSON-Code in proto (M6)

- **Kontext:** proto muss Anfragen lesen und Zustand schreiben, ohne malloc und so, dass jede Zeile erklärbar ist (API-04). Alle Anfragen der API sind flache Objekte wie `{"setpoint_c": 80}`.
- **Entscheidung:**
  - Vom Nutzer gewählt: ein eigener Leser nur für flache Objekte (`rof_json_parse_flat`). Er folgt der JSON-Grammatik, prüft Zahlen erst gegen die Grammatik und rechnet sie danach mit `strtof` um, das allein auch `+1`, `0x10` oder `NaN` annähme. Abgelehnt werden Verschachtelung, Arrays, Zeichen nach dem Objekt, doppelte Schlüssel, rohe Steuerzeichen, unbekannte Maskierungen und `\u` außerhalb von ASCII; ein Fühlertyp oder eine Rolle braucht nichts anderes.
  - Vom Nutzer gewählt: Unbekannte Felder werden mit ihrem Namen abgelehnt. Nur die eigene Oberfläche spricht mit der API, ein Tippfehler im Feldnamen fällt so sofort auf.
  - Feste Größen: höchstens 4 Felder, Schlüssel bis 24, Texte bis 32 Zeichen, Anfragen bis 256 Byte. Das Ergebnis steht in einem Struct des Aufrufers.
  - Ein eigener Schreiber (`rof_json_writer_t`) schreibt in einen Puffer fester Größe, setzt Kommas selbst, maskiert Texte und schreibt `null` für NaN und unendlich. Nur eine Funktion schreibt in den Puffer, und sie prüft vorher den Platz.
  - Temperaturen mit zwei Nachkommastellen, das ist feiner als ein Messschritt des DS18B20.
  - proto linkt nur die Header von core. Die Include-Regel für proto erlaubt nur noch die Standard-Header, die es braucht (wie in E-010 angekündigt), darunter `stdio.h` für `snprintf` und `stdlib.h` für `strtof`. Dass proto kein malloc nutzt, prüft keine Maschine, sondern die Durchsicht des Codes.
- **Verworfen:**
  - cJSON: bequem, aber mit malloc bei jeder Anfrage.
  - jsmn: bewährt und ohne malloc, aber fremder Code, und die Auswertung der Teile hätte trotzdem eigenen Code gebraucht.
  - Unbekannte Felder still ignorieren: Tippfehler blieben unbemerkt.

## E-031 ESP-IDF v5.5.6, gleiche Version lokal und in CI (M7)

- **Kontext:** CLAUDE.md verlangt ESP-IDF 5.x und in CI den Container `espressif/idf` mit fester Version (CI-04). Seit März 2026 gibt es auch 6.x.
- **Entscheidung:**
  - ESP-IDF v5.5.6, die neueste 5.x-Version (16.10.2026), lokal unter `~/esp/esp-idf` und in CI als `espressif/idf:v5.5.6`. Beide bauen dieselbe Firmware mit demselben Compiler.
  - Die einzige Komponente aus der Registry, `espressif/onewire_bus`, steht mit `==1.1.2` in `idf_component.yml`. `firmware/dependencies.lock` kommt ins Repository; ihr Hash stimmt mit dem der Registry überein.
  - core, proto und platform/esp32 bleiben, wo sie sind. Hüllen in `firmware/components/` lesen ihre Quellen aus `sources.cmake`, dieselbe Datei nutzt der Host-Build. So gibt es jede Quellliste nur einmal.
  - Für core gelten in der Firmware dieselben Warnungen wie auf dem Host einschließlich `-Wpedantic`. Für proto nicht, weil `stdio.h` von ESP-IDF `#include_next` nutzt, eine GCC-Erweiterung. Streng nach C11 prüft proto der Host-Build.
- **Verworfen:**
  - ESP-IDF 6.x: nicht 5.x, wie CLAUDE.md verlangt.
  - Den Container auch lokal nutzen: Flashen aus einem Container geht auf macOS nicht, und der Nutzer wollte keinen Docker-Nachbau.
  - Die Quelllisten in den Hüllen wiederholen: Eine neue Datei müsste an zwei Stellen eingetragen werden.

## E-032 DS18B20: eigener Ablauf auf dem Bus von Espressif (M7)

- **Kontext:** Eine Messung des DS18B20 dauert bis zu 750 ms. Der Regeltask darf nicht so lange stehen (ARC-09). Die fertige Komponente `espressif/ds18b20` wartet nach dem Anstoßen 800 ms mit `vTaskDelay`.
- **Entscheidung:**
  - Vom Nutzer gewählt: Das Timing des Busses kommt aus `espressif/onewire_bus` (RMT-Hardware), alles darüber ist eigener Code. `ds18b20_protocol` (CRC, ROM-Adresse, Scratchpad) und `ds18b20_sensors` (Ablauf) sind reines C mit Host-Tests, `esp32_onewire_bus` schickt nur die Befehle aus dem Datenblatt.
  - Ablauf je Abfrage: Werte der vorigen Messung lesen, neue Messung bei allen bekannten Sensoren anstoßen, dann den Bus absuchen, während die Messung läuft. Beim Takt von 1 s liegt fast eine Sekunde zwischen Anstoßen und Lesen. Die Zeit zählt ab dem Ende des Befehls.
  - Einen Wert liefert nur ein Sensor, der beim Anstoßen schon bekannt war. Ein später eingesteckter Sensor hätte noch den Einschaltwert 85 °C im Scratchpad, und 85 °C ist im Räucherofen ein gültiger Wert.
  - Ein Wert gilt nur bis zur nächsten Messung. Ein Lesefehler zeigt nie den alten Wert.
  - Lauter Nullen (Datenleitung auf Masse) haben eine gültige CRC. Erkannt werden sie am Konfigurationsregister, dessen Bits 0 bis 4 immer 1 sind.
  - Versagt der Bus selbst, wartet der RMT-Treiber je Zugriff bis zu 1 s. Die Abfrage liest dann nicht weiter und stößt nichts an. So kostet ein gestörter Bus höchstens zwei Zugriffe je Zyklus statt einen je Sensor. Aufgefallen beim Lesen des RMT-Treibers, mit Test behoben, im QEMU-Rauchtest bestätigt.
  - Die Sensor-ID hat den Familiencode 28 im höchsten Byte, so wie man DS18B20-Adressen üblicherweise liest.
  - Plausibel ist ein Wert nur, wenn core das sagt (SAF-04). Der Treiber prüft nur, ob die Daten korrekt übertragen wurden.
- **Verworfen:**
  - `espressif/ds18b20` mit eigenem Sensortask und Zwischenspeicher: ein Task mehr, und die Werte bräuchten eine Altersprüfung.
  - Eigenes 1-Wire per Bit-Banging: Das Timing im Mikrosekundenbereich ist mit Interrupts störanfällig.
  - Den Zustand nach dem Einschalten am Wert 85 °C erkennen: Das schlösse echte 85 °C aus.

## E-033 Verhalten bei Fehlern beim Start (M7)

- **Kontext:** Beim Start kann jeder Teil der Hardware versagen. Die Regelung soll möglichst laufen und Fehler zeigen, aber nie mit einem Ausgang, den sie nicht steuern kann.
- **Entscheidung:**
  - Der LED-Ausgang kommt zuerst. Lässt er sich nicht einrichten, startet die Regelung nicht; der Pin ist dann nicht als Ausgang geschaltet.
  - NVS und 1-Wire-Bus dürfen fehlen. Ihre Interfaces liefern dann `ROF_ERR_IO`, und core zeigt das: keine gespeicherte Zuordnung, fehlender Garraumsensor, damit FEHLER und kein Start.
  - Vor dem ersten Regelzyklus sucht `app_main` die Sensoren, stößt eine Messung an und wartet sie ab. Der erste Zyklus hat so schon Werte, der Start zeigt nicht kurz FEHLER.
- **Verworfen:**
  - Bei jedem Fehler neu starten: Ein dauerhaft defekter Sensor ergäbe eine Neustartschleife ohne Anzeige.
  - Ohne Ausgang weiterlaufen: Die Anzeige würde einen Tastgrad zeigen, den niemand schaltet.

## E-034 LED-Ausgang mit Haltezeit (M7)

- **Kontext:** Die langsame PWM schaltet ein Timer unabhängig vom Regeltask (E-023). Hängt der Regeltask, liefe der letzte Tastgrad weiter, bis der Watchdog nach 5 s neu startet.
- **Entscheidung:**
  - Vom Nutzer gewählt: Ein Tastgrad gilt nur so lange wie das Zeitfenster des Regelzyklus (3 s, SAF-08). Kommt kein neuer, ist die LED aus. Das steckt in `led_pwm`, reines C mit Host-Tests. Es geht über den Wortlaut von CLAUDE.md hinaus und stützt SAF-08.
  - Ein `esp_timer` prüft alle 50 ms und schaltet den Pin, das sind 1,25 Prozent Auflösung bei 4 s Periode. Den Zustand teilen Regeltask und Timer-Task, unter Umständen auf verschiedenen Kernen. Ein Spinlock schützt ihn.
  - Beim Start wird der Pegel 0 vorgegeben, bevor der Pin ein Ausgang wird (SAF-10).
  - Die LED hängt zwischen Pin und Masse, Pegel 1 heißt an. Die eingebaute RGB-LED der DevKitC-1 ist adressierbar und eignet sich nicht.
- **Verworfen:**
  - PWM-Hardware (LEDC): Dann schaltete nicht `rof_slow_pwm_on`, und die Logik wäre nicht auf dem Host prüfbar (E-023).
  - Nur der Watchdog: bis zu 5 s weiter heizen mit einem Regeltask, der nicht mehr prüft.

## E-035 NVS als Speicher (M7)

- **Kontext:** Die Sensorzuordnung muss einen Neustart überleben (SEN-04). Das Interface verspricht, dass der Puffer bei einem Fehler unverändert bleibt.
- **Entscheidung:**
  - Namensraum `rof`, je Schlüssel ein Block (`nvs_set_blob`), nach jedem Schreiben `nvs_commit`.
  - Lesen erst die Größe, dann über eine Kopie von höchstens 256 Byte. Scheitert das Lesen mittendrin, bleibt der Puffer des Aufrufers unverändert. Die Zuordnung braucht höchstens 106 Byte (E-012).
  - Ist die NVS-Partition voll oder von einer neueren ESP-IDF-Version, wird sie gelöscht und neu eingerichtet, wie es ESP-IDF vorsieht. Die Zuordnung ist dann weg; das Log meldet es als Warnung.
  - Geschrieben wird nur beim Zuweisen von Rolle oder Offset, und das ist beim Heizen gesperrt (E-022). Das kurze Anhalten beider Kerne beim Schreiben in den Flash trifft also nie das Heizen.
- **Verworfen:**
  - Direkt in den Puffer des Aufrufers lesen: Bei einem Lesefehler mittendrin wäre er teilweise überschrieben.
  - Beim Lesefehler der Partition nicht löschen, sondern ohne Speicher laufen: Nach einem Update von ESP-IDF bliebe der Speicher dauerhaft unbenutzbar.

## E-036 Regeltask, Snapshot-Ablage und Watchdog (M7)

- **Kontext:** Der Regeltask hat die höchste Priorität der Anwendung und darf nie blockieren (ARC-08, ARC-09). Der Zustand geht nur als Kopie unter Mutex nach außen (ARC-07).
- **Entscheidung:**
  - Regeltask mit Priorität 10, über dem Statuslog (1) und dem HTTP-Server von ESP-IDF (5, ab M8). Fest auf CPU 1, WLAN und lwIP laufen auf CPU 0. Takt mit `vTaskDelayUntil` jede Sekunde.
  - Befehlsqueue mit 16 Plätzen und Snapshot-Ablage liegen in der Komponente `rof_exchange`, die keinen Treiber kennt. Der Webserver in M8 nutzt nur sie.
  - Der Regeltask holt Befehle ohne Warten und nimmt den Mutex der Snapshot-Ablage ohne Warten. Ist er belegt, veröffentlicht er im nächsten Zyklus und zählt das mit. Leser kopieren unter dem Mutex, das dauert Mikrosekunden.
  - Task-Watchdog mit 5 s und Neustart (`CONFIG_ESP_TASK_WDT_PANIC`). Nach dem Neustart ist der Zustand BEREIT und der Ausgang 0 (SAF-10).
  - Queue, Mutex und beide Tasks liegen in statischem Speicher. Nach dem Start legt die Firmware nichts mehr an, bis auf den kurzen Iterator von `onewire_bus` bei jeder Suche.
  - Vom Nutzer gewählt: In M7 ist das serielle Statuslog der einzige Weg zu beobachten. Es zeigt alle 5 s Zustand, Sensoren, Zykluszeit, freien Stack und Heap, mit den Namen aus proto wie in der API. Befehle gibt es erst mit der API in M8.
- **Verworfen:**
  - Den Mutex mit Wartezeit nehmen: Mit Prioritätsvererbung ginge das meist schnell, aber der Regeltask könnte warten.
  - Doppelpuffer ohne Mutex: CLAUDE.md verlangt einen Mutex.
  - Eine serielle Testkonsole für Befehle schon in M7: eine Funktion außerhalb von CLAUDE.md, vom Nutzer abgelehnt.
