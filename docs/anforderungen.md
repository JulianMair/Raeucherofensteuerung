# Anforderungen

Alle Anforderungen stammen aus `CLAUDE.md`. Jede hat eine feste ID aus Bereich und Nummer. Tests nennen die ID, die sie belegen, im Kommentar über der Testfunktion, zum Beispiel `/* SAF-03 */`. Die Spalte „Nachweis“ füllt sich mit jedem Meilenstein. In M11 dient sie als Abgleich zwischen Anforderung und Test.

## ARC – Architektur (Abschnitt 1)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| ARC-01 | core ist reines C11: keine ESP-Header, kein FreeRTOS, kein malloc nach der Initialisierung | M1 | erfüllt für M1: ctest `include_rules` erlaubt in core nur eigene Header und sieben Standard-Header, also auch kein `stdlib.h` und damit kein malloc. Gegenproben `include_rules_detects_core_esp`, `_core_freertos`, `_core_stdlib`. C11 ohne Erweiterungen über `CMAKE_C_EXTENSIONS OFF` (E-003) |
| ARC-02 | core spricht nur über Interfaces: Temperatursensor (lesen, aufzählen), Ausgang (Tastgrad setzen), Uhr (Millisekunden), Speicher (laden, speichern), Log | M1 | teilweise: Interfaces in `core/include`, Host-Implementierungen getestet in `test_platform_host`. Belegt ist die Anforderung erst, wenn core-Module nur diese Interfaces nutzen (ab M2) |
| ARC-03 | Die Zeit wird immer injiziert, nie direkt gelesen | M1 | teilweise: `rof_clock_t`, `time.h` in core verboten (`include_rules_detects_core_time`). Nutzung durch core-Module ab M2 |
| ARC-04 | Die Hardwareschicht kennt die Webseite nicht, die Webseite kennt nur die API, der Webserver bindet keine Treiber ein und ruft core nie direkt auf | M8 | offen |
| ARC-05 | Befehle (Sollwert, Start, Stopp, Sensorrolle zuweisen, Reset) gehen ausschließlich über eine FreeRTOS-Queue an den Regeltask | M7 | offen |
| ARC-06 | Der Regeltask validiert jeden Befehl selbst und verwirft ungültige | M7 | offen |
| ARC-07 | Der Zustand geht nur als Kopie eines Snapshot-Structs nach außen, geschützt durch Mutex. Der Webserver liest nur diese Kopie | M7 | offen |
| ARC-08 | Der Regeltask hat die höchste Priorität der Anwendung | M7 | offen |
| ARC-09 | Eine volle Queue, ein abgestürzter Webserver oder ein WLAN-Abbruch blockieren die Regelung nie | M5, M7 | offen |
| ARC-10 | proto ist reines C und auf dem Host testbar | M6 | offen |

## SAF – Sicherheitslogik (Abschnitt 2)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| SAF-01 | Eigenes Modul `core/safety`, in jedem Regelzyklus vor dem Regler ausgewertet | M3 | offen |
| SAF-02 | Bei Fehler ist der Ausgang 0, und `SICHERER_ZUSTAND` bleibt gesetzt, bis manuell zurückgesetzt wird und die Ursache weg ist | M3 | offen |
| SAF-03 | Auslöser: Lesefehler eines Sensors mit Sicherheitsrolle (Garraum) | M3 | offen |
| SAF-04 | Auslöser: Wert außerhalb des plausiblen Bereichs, Grenzen konfigurierbar | M3 | offen |
| SAF-05 | Auslöser: eingefrorener Wert, also keine Änderung über eine konfigurierbare Zeit bei aktivem Ausgang | M3 | offen |
| SAF-06 | Auslöser: Übertemperatur über einer harten Grenze, die nicht über die API änderbar ist | M3 | offen |
| SAF-07 | Auslöser: Garraumsensor fehlt oder ist keinem Kanal zugeordnet | M3 | offen |
| SAF-08 | Auslöser: Regelzyklus überschreitet sein Zeitfenster (Watchdog) | M3 | offen |
| SAF-09 | Sollwerte außerhalb der Grenzen werden abgelehnt, nicht begrenzt, und der Grund wird gemeldet | M3, M6 | offen |
| SAF-10 | Nach einem Neustart startet das System immer in `BEREIT` mit Ausgang 0. Die Heizung läuft erst nach erneutem Start | M4, M7 | offen |
| SAF-11 | Jede Regel hat mindestens einen Test für Auslösen und einen für Nichtauslösen | M3 | offen |
| SAF-12 | Zweigabdeckung von `core/safety` 100 Prozent, gemessen mit gcov in CI | M3 | offen |
| SAF-13 | Integrationstest auf dem Host belegt die Entkopplung: Befehlsqueue geflutet bzw. kein Befehl mehr, die Regelung hält die Temperatur im Simulator trotzdem | M5 | offen |

## SEN – Sensorverwaltung (Abschnitt 3)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| SEN-01 | Bis zu 8 Kanäle, jeder mit eindeutiger ID (DS18B20: ROM-Adresse), Typ, Rolle und Kalibrieroffset | M2 | offen |
| SEN-02 | Rollen: `GARRAUM`, `KERN_1`, `KERN_2`, `GENERATOR`, `NICHT_ZUGEORDNET` | M2 | offen |
| SEN-03 | Treiber melden über `aufzaehlen()`, welche Sensoren da sind. Neue Sensoren erscheinen als nicht zugeordnet | M2 | offen |
| SEN-04 | Die Zuordnung ID zu Rolle wird über das Speicher-Interface gespeichert und nach Neustart wiederhergestellt | M2 | offen |
| SEN-05 | Fehlt ein zugeordneter Sensor, wird seine Rolle als fehlerhaft gemeldet (beim Garraum siehe SAF-07) | M2 | offen |
| SEN-06 | Ein weiterer Sensortyp (z. B. PT1000 mit MAX31865) kommt nur als neuer Treiber hinzu, ohne Änderung in core | M2 | offen |
| SEN-07 | Host-Tests: an- und abstecken, Rolle zuweisen, Neustart mit gespeicherter Zuordnung, doppelte IDs, mehr als 8 Sensoren | M2 | offen |

## CTL – Regelung, Simulator, Zustandsautomat (Abschnitt 4)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| CTL-01 | Simulator auf dem Host: thermisches Modell erster Ordnung mit Totzeit für Garraum und Kern, Parameter konfigurierbar | M5 | offen |
| CTL-02 | Regler: PI mit Anti-Windup | M5 | offen |
| CTL-03 | Ausgang als langsame Pulsweitenmodulation, Zykluszeit konfigurierbar, Standard 4 Sekunden | M5 | offen |
| CTL-04 | Zustandsautomat mit `BEREIT`, `AUFHEIZEN`, `HALTEN`, `ABKUEHLEN`, `FEHLER`, `SICHERER_ZUSTAND` | M4 | offen |
| CTL-05 | Übergänge als Tabelle in `docs/zustandsautomat.md`, jeder Übergang hat einen Test | M4 | offen |
| CTL-06 | Integrationstests über mehrere Stunden simulierte Zeit, inklusive Sensorausfall nach Stunden | M5 | offen |

## API – Schnittstelle (Abschnitt 5)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| API-01 | `docs/api.yaml` (OpenAPI) ist die einzige Quelle für die Schnittstelle, die TypeScript-Typen werden daraus generiert (`openapi-typescript`) | M6, M9 | offen |
| API-02 | REST für Befehle und Konfiguration | M6, M8 | offen |
| API-03 | WebSocket `/ws` für Livewerte im Takt von 1 Sekunde | M6, M8 | offen |
| API-04 | Kodierung und Validierung in `proto/`, Host-Tests mit Unity inklusive kaputtem JSON, fehlenden Feldern und Werten außerhalb der Grenzen | M6 | offen |

## NET – Netzwerk (Abschnitt 6, Arbeitsweise 8)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| NET-01 | Eigener Access Point mit WPA2, kein Router und kein Internet nötig | M8 | offen |
| NET-02 | Kleiner DNS-Server als Captive Portal, zusätzlich erreichbar unter 192.168.4.1 | M8 | offen |
| NET-03 | Weboberfläche als gzip-Dateien aus LittleFS | M8 | offen |
| NET-04 | Keine Geheimnisse im Repository. Das WLAN-Passwort kommt aus Kconfig mit einem Platzhalter als Standard | M0, M8 | teilweise: `.gitignore` schließt `firmware/sdkconfig` aus |

## WEB – Weboberfläche (Abschnitt 7)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| WEB-01 | Preact, TypeScript (strict), Vite, uPlot für die Kurven | M9 | offen |
| WEB-02 | Ansichten: Status mit allen Sensoren und Rollen, Sollwert setzen, Start, Stopp, Reset des sicheren Zustands, Sensorrollen zuweisen, Livekurve, Fertigzeit-Prognose (wenn vorhanden) | M9 | offen |
| WEB-03 | Klare Anzeige bei Verbindungsverlust und bei aktivem Fehler oder sicherem Zustand | M9 | offen |
| WEB-04 | Auf dem Handy bedienbar | M9 | offen |
| WEB-05 | Tests mit Vitest und `@testing-library/preact`, MSW als Mock für REST, Fake-WebSocket für Livewerte | M9 | offen |
| WEB-06 | Getestet mindestens: Anzeige aller Zustände, ungültige Eingaben, abgelehnter Sollwert, Verbindungsabbruch und Wiederverbindung, sicherer Zustand | M9 | offen |
| WEB-07 | Größenbudget gezippt unter 80 KB, in CI geprüft | M9 | offen |

## CI – Continuous Integration (Abschnitte 1 und 8)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| CI-01 | Host: CMake-Build, ctest, gcov für `core/safety`, cppcheck, Prüfung der Include-Regeln | M0, M1, M3 | teilweise: `.github/workflows/ci.yml`, Job `host` mit Build, ctest (inklusive Include-Prüfung) und cppcheck. gcov folgt in M3 |
| CI-02 | CI prüft, dass core und proto keinen ESP- oder FreeRTOS-Header einbinden und dass `firmware/net` keine Treiber einbindet. Bei Verstoß schlägt der Build fehl | M1, M8 | ctest `include_rules` mit sieben Gegenproben `include_rules_detects_*` und `include_rules_accepts_clean_tree`. Der Pfad `firmware/net` wird in M8 gegen die echte Struktur geprüft |
| CI-03 | Web: `npm ci`, Lint, Typprüfung, Vitest, Build, Größenbudget | M9 | offen |
| CI-04 | Firmware: `idf.py build` im Container `espressif/idf` mit fester Version | M7 | offen |

## FC – Fertigzeit-Prognose (optional, M10)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| FC-01 | Modell schätzt aus den Messwerten die Annäherung der Kerntemperatur an den Garraum, Ausgabe als Zeitspanne, Tests mit simulierten Verläufen | M10 | offen |

## Nicht im Umfang

Netzspannung, Display, MQTT sowie Funktionen außerhalb dieser Liste.
