# Hardwaretest

Tests auf der echten Platine. Der Nutzer führt sie aus und meldet die Ergebnisse, Claude trägt sie hier ein. In M11 fließen sie in den Testbericht ein.

## Aufbau

- ESP32-S3-DevKitC-1 N16R8, am Mac über den USB-Anschluss mit der Aufschrift „UART“
- DS18B20: VDD an 3,3 V, GND an GND, Daten an GPIO 4. Ein Pull-up von 4,7 kΩ zwischen Daten und 3,3 V. Parasitäre Speisung wird nicht unterstützt (E-032)
- LED mit Vorwiderstand (z. B. 330 Ω) zwischen GPIO 5 und GND. Die eingebaute RGB-LED eignet sich nicht (E-034)
- Andere Pins: `idf.py -C firmware menuconfig`, Menü „Raeucherofensteuerung“

## Bauen, Flashen, Beobachten

In jedem neuen Terminal zuerst die Umgebung von ESP-IDF laden:

```bash
. ~/esp/esp-idf/export.sh
```

Bauen, flashen und den seriellen Monitor öffnen. Den Port findet `idf.py` meist selbst, sonst mit `-p /dev/cu.…` angeben. Beenden mit Ctrl+].

```bash
idf.py -C firmware build flash monitor
```

## M7: Firmware-Basis

Ohne Netzwerk lassen sich noch keine Befehle senden. Geprüft wird, was ohne Befehle sichtbar ist.

| ID | Schritt | Erwartet | Ergebnis |
|---|---|---|---|
| H7-01 | Flashen, Monitor öffnen | „Regelung läuft: 1-Wire an GPIO 4, LED an GPIO 5“, danach kein Neustart | |
| H7-02 | Statuslog abwarten (alle 5 s) | „Zustand fault, Sollwert -, Tastgrad 0.00, Ursache aktiv chamber_unassigned“. Ohne zugeordneten Garraum ist der Start gesperrt (E-020) | |
| H7-03 | LED beobachten | Die LED bleibt die ganze Zeit aus | |
| H7-04 | Sensoren im Statuslog | Je DS18B20 eine Zeile „Sensor 28… DS18B20, Rolle unassigned, …“ mit plausibler Raumtemperatur. Jede ID beginnt mit 28 | |
| H7-05 | Einen Sensor in der Hand erwärmen | Der Wert steigt innerhalb weniger Sekunden | |
| H7-06 | Einen Sensor abziehen | Spätestens im übernächsten Statuslog fehlt seine Zeile | |
| H7-07 | Den Sensor wieder einstecken | Er erscheint wieder mit plausiblem Wert, nie mit 85.00 °C | |
| H7-08 | Datenleitung einige Sekunden mit einer Drahtbrücke auf GND legen, dann lösen | Fehlermeldungen zum Bus, kein Neustart, „zuletzt“ im Statuslog unter 3000 ms. Danach erscheinen die Sensoren wieder | |
| H7-09 | Reset-Taste (EN) drücken | Neustart, danach wie H7-01 bis H7-03, die LED bleibt aus | |
| H7-10 | Einige Minuten laufen lassen, Laufzeitzeile lesen | „max“ deutlich unter 1000 ms, „Stack frei“ über 1000 B, „Snapshot belegt“ 0 oder selten, „Heap frei“ bleibt gleich | |

Mit dem Netzwerk in M8 kommen dazu: Rolle zuweisen und Neustart mit gespeicherter Zuordnung (SEN-04), Start und LED-Takt (CTL-03), Garraumsensor beim Heizen abziehen (SAF-03, SAF-02), Reset.

## M8: Netzwerk

Vorbereitung: In `idf.py -C firmware menuconfig` unter „Raeucherofensteuerung“ ein eigenes WLAN-Passwort setzen (8 bis 63 Zeichen). Es steht danach nur in `firmware/sdkconfig`, nicht im Repository. Für H8-01 zuerst einmal mit dem Platzhalter flashen.

Der Mac oder das Handy verbindet sich mit dem WLAN „Raeucherofen“. Die Befehle laufen auf dem Mac, `ID` ist eine Sensor-ID aus dem Statuslog.

```bash
curl -s http://192.168.4.1/api/state
```

```bash
curl -s http://192.168.4.1/api/config
```

```bash
curl -s -X PUT -d '{"role":"chamber"}' http://192.168.4.1/api/sensors/ID/role
```

```bash
curl -s -X POST -d '{"setpoint_c":30}' http://192.168.4.1/api/setpoint
```

```bash
curl -s -X POST http://192.168.4.1/api/start
```

```bash
curl -s -X POST http://192.168.4.1/api/stop
```

```bash
curl -s -X POST http://192.168.4.1/api/reset
```

Ungültige Anfragen, `-i` zeigt den HTTP-Status:

```bash
curl -s -i -X POST -d '{"setpoint_c":500}' http://192.168.4.1/api/setpoint
```

```bash
curl -s -i -X POST -d '{"setpoint_c":' http://192.168.4.1/api/setpoint
```

Flut aus 40 gleichzeitigen Befehlen. Reset ist in BEREIT harmlos, er geht aber durch die Queue:

```bash
for i in $(seq 1 40); do curl -s -o /dev/null -w "%{http_code} " -X POST http://192.168.4.1/api/reset & done; wait; echo
```

Livewerte über den WebSocket, zum Beispiel mit `websocat` (`brew install websocat`):

```bash
websocat ws://192.168.4.1/ws
```

| ID | Schritt | Erwartet | Ergebnis |
|---|---|---|---|
| H8-01 | Mit dem Platzhalter-Passwort flashen | Log: „Access Point startet nicht: WLAN-Passwort ist noch der Platzhalter …“, kein WLAN sichtbar, das Statuslog läuft weiter (NET-04) | |
| H8-02 | Eigenes Passwort setzen, flashen | Log: „Access Point "Raeucherofen" läuft“, das WLAN ist mit WPA2 sichtbar (NET-01) | |
| H8-03 | Handy verbinden | Das Handy meldet eine Anmeldeseite, sie zeigt die Oberfläche (seit M9; davor die Platzhalterseite) (NET-02, NET-03) | |
| H8-04 | Im Browser `http://192.168.4.1/` und `http://beispiel.de/` öffnen | Beide zeigen die Oberfläche (seit M9; davor die Platzhalterseite) | |
| H8-05 | `GET /api/state` | JSON wie `docs/api-examples/state.json`, mit den echten Sensoren | |
| H8-06 | `GET /api/config` | Grenzen: Sollwert 20 bis 110, Offset 5, harte Grenze 120 | |
| H8-07 | Rolle `chamber` zuweisen | `{"seq":…,"result":"ok"}`, danach im Statuslog Zustand `idle` statt `fault` | |
| H8-08 | Reset-Taste (EN) drücken | Nach dem Neustart hat der Sensor weiter die Rolle `chamber` (SEN-04), Zustand `idle`, LED aus (SAF-10) | |
| H8-09 | Sollwert 30, dann Start | Zustand `heating`. Kalt leuchtet die LED dauernd, in der Hand erwärmt geht sie aus, dazwischen blinkt sie in Fenstern von 4 s (CTL-03) | |
| H8-10 | Sollwert 500 und kaputtes JSON | 422 mit `{"result":"setpoint_too_high"}`, 400 mit `{"error":"bad_json"}`; der Zustand ändert sich nicht (SAF-09) | |
| H8-11 | Beim Heizen den Garraumsensor abziehen, Reset, wieder einstecken, Reset | Sofort `safe_state`, LED aus, `chamber_missing` verriegelt. Reset mit fehlendem Sensor: 409 `cause_active`. Nach dem Einstecken: Reset 200, Zustand `idle` (SAF-02, SAF-07) | |
| H8-12 | WebSocket öffnen | Sofort eine Nachricht, danach eine je Sekunde, gleicher Inhalt wie `/api/state` (API-03) | |
| H8-13 | Flut aus 40 Befehlen | Antworten 409. Der Server bearbeitet Befehle nacheinander, deshalb dauert die Flut einige Sekunden; ab 8 offenen Verbindungen schließt er die ältesten, dann erscheint `000`. Im Statuslog bleibt die Zykluszeit gleich, kein `safe_state`, kein Neustart (ARC-09) | |
| H8-14 | Beim Heizen das WLAN am Mac trennen | Das Statuslog läuft weiter, die LED schaltet weiter nach Tastgrad (ARC-09) | |

Hinweis zu H8-09: Hat der Ausgang 10 Minuten lang ohne Änderung über 0,05 °C geheizt, meldet die Sicherheitslogik `frozen`. Das ist gewollt (SAF-05). Bei Raumtemperatur ohne Heizelement passiert das leicht.

## M9: Weboberfläche

Vorbereitung: Oberfläche bauen, dann die Firmware wie gewohnt bauen und flashen. Ohne `web/dist` bricht der Firmware-Build ab.

```bash
npm --prefix web ci
```

```bash
npm --prefix web run build
```

Die Schritte am Handy, das mit dem WLAN „Raeucherofen“ verbunden ist:

| ID | Schritt | Erwartet | Ergebnis |
|---|---|---|---|
| H9-01 | Verbinden, Anmeldeseite öffnen | Die Oberfläche erscheint statt der früheren Platzhalterseite, mit Zustand, Bedienung, Kurve und Sensoren (WEB-02, NET-03) | |
| H9-02 | Ohne zugeordneten Garraum | Oranges Band „Fehler: Start gesperrt“ mit Ursache „Garraum keinem Sensor zugeordnet“ | |
| H9-03 | Unter „Sensoren“ einem Sensor die Rolle Garraum geben | „Ausgeführt“, Zustand „Bereit“, das Band verschwindet | |
| H9-04 | Sollwert „abc“, „15“ und „120“ eingeben | Meldung unter dem Feld, nichts wird gesendet (WEB-06) | |
| H9-05 | Sollwert „30,5“ setzen, Start | „Ausgeführt“, Zustand „Aufheizen“, die Kurve zeigt Garraum und Sollwert (WEB-02) | |
| H9-06 | Beim Heizen Rolle oder Offset ändern wollen | Felder gesperrt mit Hinweis | |
| H9-07 | Garraumsensor abziehen | Rotes Band „Sicherer Zustand“ mit Ursache, Start gesperrt, Reset frei. Reset vor dem Einstecken: „Die Ursache besteht noch“. Nach dem Einstecken: Reset, Zustand „Bereit“ (WEB-06) | |
| H9-08 | WLAN am Handy aus, nach 10 s wieder an | Band „Verbindung getrennt, letzte Daten vor … s“, Werte grau, Knöpfe gesperrt; nach dem Einschalten verbindet sich die Seite von selbst, das Band verschwindet (WEB-03) | |
| H9-09 | Mit dem Handy aus der Reichweite gehen, zurückkommen | Das Band erscheint nach wenigen Sekunden, nicht erst nach Minuten (E-045) | |
| H9-10 | Handy drehen | Die Kurve passt sich der Breite an, nichts ragt über den Rand (WEB-04) | |
| H9-11 | Offset 0,5 bei einem Sensor setzen | „Ausgeführt“, der Wert des Sensors steigt um 0,5 °C (SEN-01) | |

## M10: Fertigzeit-Prognose

Ohne Heizelement lässt sich die Prognose nur eingeschränkt prüfen. Der Garraum muss halten, und der Kernfühler muss sich ihm von unten nähern.

| ID | Schritt | Erwartet | Ergebnis |
|---|---|---|---|
| H10-01 | Kernziel „25“ und „120“ eingeben | Meldung „Mindestens 30 °C“ bzw. „Höchstens 100 °C“, nichts gesendet | |
| H10-02 | Kernziel 68 setzen, ohne Kern 1 | „Fertig Kern 1: kein Fühler“ | |
| H10-03 | Kern 1 zuordnen, Zustand BEREIT | „wartet, bis der Garraum hält“; das Statuslog zeigt `waiting` | |
| H10-04 | Optional: Garraumfühler in Wasser von etwa 45 °C, Sollwert 45, Start, bis HALTEN; Kernfühler aus dem Kühlschrank in dasselbe Wasser | Erst „lernt noch“, nach 5 Minuten „noch ca. … min“ bis zu einem Kernziel unter 45 °C; die Restzeit sinkt mit der Zeit | |

## Rauchtest im Emulator

Claude hat die Firmware vor der Übergabe im Emulator QEMU gestartet (ESP-IDF v5.5.6, QEMU 9.2.2 von Espressif, Stand `3b339bd`). QEMU bildet die RMT-Hardware nicht nach, der 1-Wire-Bus verhält sich dort wie ein gestörter Bus.

| Prüfung | Ergebnis |
|---|---|
| Start, Regeltask und Statuslog laufen | ja, kein Neustart in 40 s |
| Zustand ohne Sensoren | `fault`, Ursache `chamber_unassigned`, Tastgrad 0.00 |
| Gestörter Bus | einmal 1 s Wartezeit beim ersten Zugriff, danach scheitert jeder Zugriff sofort; Zyklus 4 bis 6 ms, höchstens 21 ms |
| Speicher | Stack des Regeltasks 3280 von 6144 B frei, Heap rund 358 KB frei |

Nach M8 noch einmal (Stand `7db05c4`, 30 s):

| Prüfung | Ergebnis |
|---|---|
| Access Point mit Platzhalter-Passwort | startet nicht, Log: „Access Point startet nicht: WLAN-Passwort ist noch der Platzhalter, bitte in idf.py menuconfig setzen“ |
| Regelung ohne Netzwerk | läuft weiter, Zustand `fault` ohne Sensoren, Zyklus höchstens 20 ms |
| Speicher | Heap rund 293 KB frei; WLAN und HTTP kann QEMU nicht nachbilden |
