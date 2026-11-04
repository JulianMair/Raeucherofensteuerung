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

## Rauchtest im Emulator

Claude hat die Firmware vor der Übergabe im Emulator QEMU gestartet (ESP-IDF v5.5.6, QEMU 9.2.2 von Espressif, Stand `3b339bd`). QEMU bildet die RMT-Hardware nicht nach, der 1-Wire-Bus verhält sich dort wie ein gestörter Bus.

| Prüfung | Ergebnis |
|---|---|
| Start, Regeltask und Statuslog laufen | ja, kein Neustart in 40 s |
| Zustand ohne Sensoren | `fault`, Ursache `chamber_unassigned`, Tastgrad 0.00 |
| Gestörter Bus | einmal 1 s Wartezeit beim ersten Zugriff, danach scheitert jeder Zugriff sofort; Zyklus 4 bis 6 ms, höchstens 21 ms |
| Speicher | Stack des Regeltasks 3280 von 6144 B frei, Heap rund 358 KB frei |
