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
