# Projekt: Räucherofensteuerung auf ESP32 S3

Du setzt mit mir eine Steuerungssoftware für einen Räucherofen um. Das Projekt ist meine Prüfungsleistung im IU Modul Projekt: Software Development. Bewertet wird die Software, nicht der Ofen. Ich komme aus der Webentwicklung und lerne Embedded gerade. Ich muss am Ende jede Zeile erklären können.

## Arbeitsweise (verbindlich)

1. Wir arbeiten Meilenstein für Meilenstein (Liste unten). Beginne einen Meilenstein immer mit einem kurzen Plan: welche Dateien, welche Tests, welche Commits. Warte auf mein OK, bevor du Code schreibst.
2. Kleine Commits. Jeder Commit baut und alle Tests sind grün. Commit Nachrichten nach Conventional Commits auf Englisch, zum Beispiel `feat(safety): latch safe state on sensor fault` oder `test(registry): cover unplugged sensor`.
3. Bei Logik im Ordner core gilt Test zuerst: erst der fehlschlagende Test, dann die Implementierung, dann Refactoring. Gern als getrennte Commits.
4. Nach jedem Meilenstein STOPPST du und gibst mir:
   - eine Zusammenfassung, was entstanden ist
   - die 3 bis 5 Stellen im Code, die ich verstehen muss, mit Erklärung
   - offene Fragen oder Risiken
   Dann wartest du auf "weiter".
5. Jede nicht offensichtliche Entscheidung kommt als kurzer Eintrag nach `docs/entscheidungen.md` (Kontext, Entscheidung, verworfene Alternativen).
6. Nach jedem Meilenstein ein Eintrag in `docs/ki_log.md`: Datum, Meilenstein, was du erzeugt hast, was ich geändert oder abgelehnt habe. Das brauche ich für die Kennzeichnung der KI Nutzung.
7. Wenn etwas unklar ist, frag nach statt zu raten. Wenn eine meiner Vorgaben schlecht ist, sag es mir ehrlich und begründe es.
8. Keine Geheimnisse ins Repository. Das WLAN Passwort kommt aus Kconfig mit einem Platzhalter als Standard.

## Umgebung

- Board: ESP32 S3 DevKitC 1 (N16R8), ESP IDF 5.x, FreeRTOS
- Sensoren zum Testen: mehrere DS18B20 an einem 1 Wire Bus, GPIO per Kconfig (Standard GPIO 4)
- Ausgang zum Testen: LED statt Heizelement, GPIO per Kconfig. Keine Netzspannung im Projekt.
- Entwicklungsrechner: macOS auf Apple Silicon
- Versionskontrolle: Git mit GitHub, CI mit GitHub Actions (siehe `docs/entscheidungen.md`)

## Architekturregeln (nicht verhandelbar)

### 1. Schichten und Entkopplung

```
core/        reines C11, keine ESP Header, kein FreeRTOS, kein malloc nach der Initialisierung
  include/   Interfaces
  safety/    Sicherheitslogik
  sensors/   Sensorverwaltung
  control/   Zustandsautomat und Regler
  forecast/  Fertigzeit Prognose (optional)
proto/       reines C: JSON Kodierung von Befehlen und Zustand, Validierung, Host testbar
platform/
  host/      Mocks, Fake Uhr, Simulator
  esp32/     Treiber DS18B20, LED Ausgang, NVS Speicher, Uhr
firmware/    ESP IDF Projekt: Tasks, Queue, Access Point, HTTP und WebSocket Server
web/         Preact, TypeScript, Vite
test/        Unity Tests für core und proto
docs/        Anforderungen, API, Entscheidungen, KI Log
```

- core spricht nur mit Interfaces: Temperatursensor (lesen, aufzählen), Ausgang (Tastgrad setzen), Uhr (Millisekunden), Speicher (laden, speichern), Log. Die Zeit ist immer injiziert, nie direkt gelesen.
- Die Hardwareschicht kennt die Webseite nicht. Die Webseite kennt nur die API. Der Webserver bindet keine Treiber ein und ruft core nie direkt auf.
- Kommunikation zwischen Netzwerk und Regelung ausschließlich so:
  - Befehle (Sollwert, Start, Stopp, Sensorrolle zuweisen, Reset) gehen über eine FreeRTOS Queue an den Regeltask. Der Regeltask validiert jeden Befehl selbst und verwirft ungültige.
  - Zustand geht als Kopie eines Snapshot Structs nach außen, geschützt durch Mutex. Der Webserver liest nur diese Kopie.
- Der Regeltask hat die höchste Priorität der Anwendung. Eine volle Queue, ein abgestürzter Webserver oder ein WLAN Abbruch dürfen die Regelung nie blockieren.
- CI prüft automatisch, dass core und proto keinen ESP oder FreeRTOS Header einbinden, und dass firmware/net keine Treiber einbindet. Bei Verstoß schlägt der Build fehl.

### 2. Sicherheitslogik

Eigenes Modul `core/safety`, wird in jedem Regelzyklus vor dem Regler ausgewertet. Liefert es einen Fehler, ist der Ausgang 0 und der Zustand `SICHERER_ZUSTAND` bleibt gesetzt, bis ich ihn manuell zurücksetze und die Ursache weg ist.

Auslöser mindestens:
- Lesefehler eines Sensors mit Sicherheitsrolle (Garraum)
- Wert außerhalb des plausiblen Bereichs (Grenzen konfigurierbar)
- eingefrorener Wert: keine Änderung über eine konfigurierbare Zeit bei aktivem Ausgang
- Übertemperatur über einer harten Grenze, die nicht über die API änderbar ist
- Garraumsensor fehlt oder ist keinem Kanal zugeordnet
- Regelzyklus überschreitet sein Zeitfenster (Watchdog)

Weitere Regeln:
- Sollwerte außerhalb der Grenzen werden abgelehnt, nicht begrenzt, und der Grund wird gemeldet.
- Nach einem Neustart startet das System immer in `BEREIT` mit Ausgang 0. Die Heizung läuft erst nach erneutem Start durch mich.

Tests:
- Jede Regel hat mindestens einen Test für Auslösen und einen für Nichtauslösen.
- Zweigabdeckung von `core/safety` 100 Prozent, gemessen mit gcov in CI.
- Ein Integrationstest auf dem Host belegt die Entkopplung: Befehlsqueue wird geflutet bzw. kein Befehl kommt mehr, die Regelung hält die Temperatur im Simulator trotzdem.

### 3. Variable Sensoren

Modul `core/sensors` mit einer Sensorverwaltung:
- bis zu 8 Kanäle, jeder mit eindeutiger ID (beim DS18B20 die ROM Adresse), Typ, Rolle und Kalibrieroffset
- Rollen: `GARRAUM`, `KERN_1`, `KERN_2`, `GENERATOR`, `NICHT_ZUGEORDNET`
- Treiber melden über `aufzaehlen()`, welche Sensoren gerade da sind. Neue Sensoren erscheinen als nicht zugeordnet.
- Die Zuordnung ID zu Rolle wird über das Speicher Interface gespeichert und nach Neustart wiederhergestellt.
- Fehlt ein zugeordneter Sensor, wird seine Rolle als fehlerhaft gemeldet. Beim Garraum löst das die Sicherheitslogik aus.
- Ein weiterer Sensortyp (später PT1000 mit MAX31865) muss nur als neuer Treiber hinzukommen, ohne Änderung in core.

Tests auf dem Host: Sensoren an und abstecken, Rolle zuweisen, Neustart mit gespeicherter Zuordnung, doppelte IDs, mehr als 8 Sensoren.

### 4. Regelung und Simulator

- Simulator auf dem Host: thermisches Modell erster Ordnung mit Totzeit für Garraum und Kern, Parameter konfigurierbar.
- Regler: PI mit Anti Windup, Ausgang als langsame Pulsweitenmodulation (Zykluszeit konfigurierbar, Standard 4 Sekunden).
- Zustandsautomat: `BEREIT`, `AUFHEIZEN`, `HALTEN`, `ABKUEHLEN`, `FEHLER`, `SICHERER_ZUSTAND`. Übergänge als Tabelle in `docs/zustandsautomat.md`, jeder Übergang hat einen Test.
- Integrationstests lassen mehrere Stunden simulierte Zeit in Sekunden laufen, inklusive Sensorausfall nach Stunden.

### 5. API

- Eine OpenAPI Datei `docs/api.yaml` ist die einzige Quelle für die Schnittstelle. Die TypeScript Typen werden daraus generiert (`openapi-typescript`).
- REST für Befehle und Konfiguration, ein WebSocket `/ws` für Livewerte im Takt von 1 Sekunde.
- Kodierung und Validierung liegen in `proto/` und werden auf dem Host mit Unity getestet, inklusive kaputtem JSON, fehlenden Feldern und Werten außerhalb der Grenzen.

### 6. Netzwerk

- Der ESP32 spannt einen eigenen Access Point mit WPA2 auf. Kein Router, kein Internet nötig.
- Kleiner DNS Server als Captive Portal, damit sich die Seite beim Verbinden öffnet. Zusätzlich erreichbar unter 192.168.4.1.
- Weboberfläche als gzip Dateien aus LittleFS.

### 7. Weboberfläche

- Preact, TypeScript (strict), Vite, uPlot für die Kurven
- Ansichten: Status mit allen Sensoren und Rollen, Sollwert setzen, Start, Stopp, Reset des sicheren Zustands, Sensorrollen zuweisen, Livekurve, Fertigzeit Prognose (wenn vorhanden)
- Klare Anzeige bei Verbindungsverlust und bei aktivem Fehler oder sicherem Zustand
- Auf dem Handy bedienbar
- Tests:
  - Vitest und `@testing-library/preact` für Komponenten
  - MSW als Mock für REST, ein Fake WebSocket für Livewerte
  - getestet werden mindestens: Anzeige aller Zustände, ungültige Eingaben, abgelehnter Sollwert, Verbindungsabbruch und Wiederverbindung, sicherer Zustand
- Größenbudget: gezippt unter 80 KB, in CI geprüft

### 8. CI (GitHub Actions)

- Host: CMake Build, ctest, gcov für core/safety, cppcheck, Prüfung der Include Regeln
- Web: `npm ci`, Lint, Typprüfung, Vitest, Build, Größenbudget
- Firmware: `idf.py build` im Container `espressif/idf` mit fester Version

## Meilensteine

Jeder Meilenstein endet mit dem Stopp aus der Arbeitsweise.

- **M0 Gerüst:** Ordnerstruktur, CMake mit Host Build, Unity, ein Dummy Test, CI für Host, README, .gitignore, leere Dateien in docs
- **M1 Interfaces:** Interfaces in core/include, Mocks und Fake Uhr in platform/host, Include Prüfung in CI
- **M2 Sensorverwaltung:** core/sensors mit allen Tests
- **M3 Sicherheitslogik:** core/safety mit allen Tests und 100 Prozent Zweigabdeckung. Kommt bewusst vor dem Regler.
- **M4 Zustandsautomat:** mit Tabelle in docs und einem Test je Übergang
- **M5 Simulator und Regler:** inklusive Integrationstests über mehrere simulierte Stunden und dem Entkopplungstest
- **M6 API:** OpenAPI Datei, proto/ mit Tests
- **M7 Firmware Basis:** ESP IDF Projekt, Regeltask, Queue, Snapshot, Treiber für DS18B20 und LED, NVS Speicher, Firmware Build in CI. Ich teste auf der Hardware und melde dir die Ergebnisse.
- **M8 Netzwerk:** Access Point, Captive Portal, HTTP Server, WebSocket, LittleFS
- **M9 Weboberfläche:** App mit allen Tests und Größenbudget in CI
- **M10 Fertigzeit Prognose (optional):** Modell, das die Annäherung der Kerntemperatur an den Garraum aus den Messwerten schätzt, Ausgabe als Zeitspanne, Tests mit simulierten Verläufen
- **M11 Abschluss:** README mit Bauen, Flashen, Testen; Testbericht; Abgleich jeder Anforderung mit ihrem Test

## Was du nicht tust

- Keine Meilensteine überspringen oder zusammenlegen
- Keine Funktionen außerhalb dieser Liste einbauen, auch keine kleinen Extras
- Keine Netzspannung, kein Display, kein MQTT
- Nichts committen, das nicht baut oder dessen Tests rot sind
