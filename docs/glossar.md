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
| Zustandsautomat | `rof_fsm_…` (finite state machine), Zustand `rof_state_t` |
| Übergang | Ü1 bis Ü10 in `docs/zustandsautomat.md`, im Code als Kommentar |
| Befehl, Befehlsergebnis | `rof_cmd_result_t` (`ROF_CMD_…`) |
| aktive Ursache (ohne Verriegelung) | `rof_safety_active()` |
| Halteband, Rückkehrband | `hold_band_c`, `reheat_band_c` |
| abgekühlt | `cooled_c` |
| Regelzyklus (alles zusammen) | `rof_controller`, `rof_controller_cycle()` |
| Befehlsquelle | `rof_command_source_t` (in der Firmware die FreeRTOS-Queue) |
| laufende Nummer eines Befehls | `seq` |
| Snapshot | `rof_snapshot_t` |
| langsame PWM, Periode | `rof_slow_pwm_on()`, `period_ms` |
| Verstärkung, Nachstellzeit (PI) | `kp`, `ti_s` |
| I-Anteil | `integral` |
| Simulator | `sim_oven` (nur `platform/host`) |
| Zeitkonstante, Totzeit | `tau_s`, `dead_s` |
| Kern (Fühler im Fleisch) | `probe` |
| Anfrage, Inhalt der Anfrage | request, `body` |
| Ablehnung durch proto | `rof_proto_result_t` mit `ROF_PROTO_…` |
| Vertragstest (proto gegen API-Beispiel) | `test_state_matches_api_example` |
| Schreiber, Leser (JSON) | `rof_json_writer_t`, `rof_json_parse_flat()` |
| Regeltask | `control_task` (Firmware) |
| Befehlsqueue | `command_queue_t` in `rof_exchange` |
| Snapshot-Ablage | `snapshot_store_t` in `rof_exchange` |
| Statuslog | `status_log` (Firmware, serielles Log) |
| Abfrage der Sensoren | `ds18b20_sensors_poll()` |
| Messung anstoßen | `convert_all` (Befehl CONVERT T) |
| Scratchpad (Speicher des DS18B20 mit Messwert und CRC) | `scratchpad`, `sp` |
| ROM-Adresse | `rom`, als Sensor-ID `rof_sensor_id_t` |
| Haltezeit des Tastgrads | `hold_ms` in `led_pwm_t` |
| reiner C-Teil von platform/esp32 | `ROF_ESP32_PORTABLE_SOURCES` |
| reiner C-Teil von firmware/net | `ROF_NET_PORTABLE_SOURCES`, Host-Bibliothek `net_portable` |
| Access Point | `wifi_ap` |
| Platzhalter-Passwort | `NET_WIFI_PASSWORD_PLACEHOLDER` |
| Captive Portal (Seite öffnet sich beim Verbinden) | `dns_message`, `dns_server`, Umleitung in `http_server` |
| statische Datei der Weboberfläche | `net_static_file_t` |
| Livewerte über den WebSocket | `ws_live` |
| Ergebnis eines Befehls suchen | `rof_proto_command_result()` |
| Symbolprüfung | `check_net_symbols.py` |
| Weboberfläche | `web/`, Einstieg `App` |
| Liveverbindung | `connectLive()`, `useLive()` |
| Ergebnis eines Befehls in der Oberfläche | `CommandOutcome` |
| Verlauf der Livekurve | `History`, `appendState()` |
| Prüfung der Eingaben | `checkSetpoint()`, `checkOffset()` |
| Größenbudget | `gzip_web.py --max-bytes` |

In der API (`docs/api.yaml`) stehen dieselben englischen Namen in Kleinbuchstaben, zum Beispiel `ROF_STATE_SAFE_STATE` als `"safe_state"` oder `ROF_ROLE_CHAMBER` als `"chamber"`.
