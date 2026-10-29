# Glossar

Anforderungen und Doku sind deutsch, Bezeichner im Code englisch (E-006). Diese Tabelle ist die verbindliche Zuordnung. Neue Begriffe kommen hier hinzu, bevor sie im Code auftauchen.

Alle öffentlichen Bezeichner aus core tragen das Präfix `rof_` (Typen, Funktionen) bzw. `ROF_` (Konstanten, Enum-Werte). `rof` steht für Räucherofen.

## Sensorrollen (SEN-02)

| Deutsch (CLAUDE.md) | Code | Bemerkung |
|---|---|---|
| `GARRAUM` | `ROF_ROLE_CHAMBER` | Garraum, sicherheitsrelevant |
| `KERN_1` | `ROF_ROLE_PROBE_1` | Kerntemperaturfühler 1. Bewusst nicht `CORE`, sonst Verwechslung mit dem Ordner `core/` |
| `KERN_2` | `ROF_ROLE_PROBE_2` | Kerntemperaturfühler 2 |
| `GENERATOR` | `ROF_ROLE_SMOKE_GEN` | Raucherzeuger |
| `NICHT_ZUGEORDNET` | `ROF_ROLE_UNASSIGNED` | |

## Rollenstatus (SEN-05)

Zustand einer Rolle, den die Sensorverwaltung meldet. Alles außer „in Ordnung“ ist für die Rolle ein Fehler.

| Deutsch | Code | Bedeutung |
|---|---|---|
| in Ordnung | `ROF_ROLE_STATUS_OK` | Sensor gefunden und gelesen |
| nicht zugeordnet | `ROF_ROLE_STATUS_UNASSIGNED` | kein Sensor hat die Rolle |
| fehlt | `ROF_ROLE_STATUS_MISSING` | zugeordneter Sensor nicht gefunden |
| Lesefehler | `ROF_ROLE_STATUS_READ_ERROR` | Sensor da, Messung fehlgeschlagen |
| doppelt | `ROF_ROLE_STATUS_DUPLICATE` | ID mehrfach gemeldet, Wert nicht eindeutig |

## Ursachen des sicheren Zustands (SAF-03 bis SAF-08)

In Prüfreihenfolge. Die erste Ursache, die auslöst, wird verriegelt.

| Deutsch | Code | Anforderung |
|---|---|---|
| Garraum keinem Sensor zugeordnet | `ROF_FAULT_CHAMBER_UNASSIGNED` | SAF-07 |
| Garraumsensor fehlt | `ROF_FAULT_CHAMBER_MISSING` | SAF-07 |
| Lesefehler des Garraumsensors | `ROF_FAULT_CHAMBER_READ_ERROR` | SAF-03 |
| ID des Garraumsensors doppelt | `ROF_FAULT_CHAMBER_DUPLICATE` | SAF-03 |
| Wert unplausibel | `ROF_FAULT_IMPLAUSIBLE` | SAF-04 |
| Übertemperatur | `ROF_FAULT_OVER_TEMPERATURE` | SAF-06 |
| Wert eingefroren | `ROF_FAULT_FROZEN` | SAF-05 |
| Regelzyklus überschreitet sein Zeitfenster | `ROF_FAULT_CYCLE_OVERRUN` | SAF-08 |

## Zustände (CTL-04)

| Deutsch (CLAUDE.md) | Code |
|---|---|
| `BEREIT` | `ROF_STATE_IDLE` |
| `AUFHEIZEN` | `ROF_STATE_HEATING` |
| `HALTEN` | `ROF_STATE_HOLDING` |
| `ABKUEHLEN` | `ROF_STATE_COOLING` |
| `FEHLER` | `ROF_STATE_FAULT` |
| `SICHERER_ZUSTAND` | `ROF_STATE_SAFE_STATE` |

## Interfaces (ARC-02)

| Deutsch (CLAUDE.md) | Code |
|---|---|
| Temperatursensor (lesen, aufzählen) | `rof_temp_sensor_t` mit `read`, `enumerate` |
| Ausgang (Tastgrad setzen) | `rof_output_t` mit `set_duty` |
| Uhr (Millisekunden) | `rof_clock_t` mit `now_ms` |
| Speicher (laden, speichern) | `rof_storage_t` mit `load`, `save` |
| Log | `rof_log_t` mit `write` |

## Weitere Begriffe

| Deutsch | Code |
|---|---|
| Sollwert | `setpoint` |
| Tastgrad | `duty` (0.0 bis 1.0) |
| Kalibrieroffset | `calibration_offset` |
| Kanal | `channel` |
| Regelzyklus | `control_cycle` |
| Sicherheitslogik | `safety` |
| Sensorverwaltung | `sensor_registry` |
| Rohwert | `raw_c` (Messwert ohne Offset) |
| Überlauf | `overflow` (mehr Sensoren als Kanäle) |
| Verriegelung, verriegeln | `fault` ist gesetzt, `latch` im Kommentar (der sichere Zustand bleibt bis zum Reset) |
| harte Grenze | `ROF_SAFETY_HARD_MAX_C` |
| plausibler Bereich | `plausible_min_c`, `plausible_max_c` |
| Zeitfenster des Regelzyklus | `cycle_window_ms` |
| Bezugswert (eingefrorener Wert) | `reference_c`, `reference_ms` |
| Sollwertprüfung | `rof_safety_check_setpoint`, Ergebnis `rof_setpoint_check_t` |
