# Anforderungen

Alle Anforderungen stammen aus `CLAUDE.md`. Jede hat eine feste ID aus Bereich und Nummer. Tests nennen die ID, die sie belegen, im Kommentar über der Testfunktion, zum Beispiel `/* SAF-03 */`. Die Spalte „Nachweis“ füllt sich mit jedem Meilenstein. In M11 dient sie als Abgleich zwischen Anforderung und Test.

## ARC – Architektur (Abschnitt 1)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| ARC-01 | core ist reines C11: keine ESP-Header, kein FreeRTOS, kein malloc nach der Initialisierung | M1 | erfüllt für M1: ctest `include_rules` erlaubt in core nur eigene Header und sieben Standard-Header, also auch kein `stdlib.h` und damit kein malloc. Gegenproben `include_rules_detects_core_esp`, `_core_freertos`, `_core_stdlib`. C11 ohne Erweiterungen über `CMAKE_C_EXTENSIONS OFF` (E-003) |
| ARC-02 | core spricht nur über Interfaces: Temperatursensor (lesen, aufzählen), Ausgang (Tastgrad setzen), Uhr (Millisekunden), Speicher (laden, speichern), Log | M1, M5 | core-Module sprechen nur über die Interfaces aus `core/include`: Sensorverwaltung über Temperatursensor, Speicher und Log; Sicherheitslogik über Uhr und Log; der Regelzyklus `rof_controller` zusätzlich über Ausgang und Befehlsquelle. Belegt durch `test_controller` und `test_integration`, die nur Mocks und den Simulator dahinter haben |
| ARC-03 | Die Zeit wird immer injiziert, nie direkt gelesen | M1, M5 | `rof_clock_t` ist die einzige Zeitquelle in core, `time.h` ist verboten (`include_rules_detects_core_time`). Der Regelzyklus rechnet den Zeitschritt des Reglers aus der Uhr (`test_integral_uses_time_from_clock`), der Simulator ist in den Integrationstests die Uhr |
| ARC-04 | Die Hardwareschicht kennt die Webseite nicht, die Webseite kennt nur die API, der Webserver bindet keine Treiber ein und ruft core nie direkt auf | M8 | offen |
| ARC-05 | Befehle (Sollwert, Start, Stopp, Sensorrolle zuweisen, Reset) gehen ausschließlich über eine FreeRTOS-Queue an den Regeltask | M7 | teilweise: Befehle kommen nur über die Befehlsquelle `rof_command_source_t` (`test_commands_take_effect_in_the_cycle`). Die FreeRTOS-Queue dahinter folgt in M7 |
| ARC-06 | Der Regeltask validiert jeden Befehl selbst und verwirft ungültige | M7 | teilweise: `rof_controller` prüft jeden Befehl selbst und verwirft ungültige mit Grund (`test_invalid_setpoint_is_rejected_with_reason`, `test_unknown_command_type_is_rejected`, `test_assign_with_unknown_sensor_or_role_is_rejected`, `test_sensor_settings_are_locked_while_heating`, `test_reset_outside_safe_state_is_rejected`). Im Regeltask auf dem ESP32 folgt M7 |
| ARC-07 | Der Zustand geht nur als Kopie eines Snapshot-Structs nach außen, geschützt durch Mutex. Der Webserver liest nur diese Kopie | M7 | teilweise: `rof_snapshot_t` ist eine vollständige Kopie ohne Zeiger in den Regelzyklus (`test_snapshot_*` in `test_controller`). Mutex und Webserver folgen in M7 und M8 |
| ARC-08 | Der Regeltask hat die höchste Priorität der Anwendung | M7 | offen |
| ARC-09 | Eine volle Queue, ein abgestürzter Webserver oder ein WLAN-Abbruch blockieren die Regelung nie | M5, M7 | teilweise: höchstens 4 Befehle je Zyklus (`test_at_most_four_commands_per_cycle`), Entkopplungstest auf dem Host (siehe SAF-13). Priorität des Regeltasks und WLAN-Abbruch folgen in M7 und M8 |
| ARC-10 | proto ist reines C und auf dem Host testbar | M6 | offen |

## SAF – Sicherheitslogik (Abschnitt 2)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| SAF-01 | Eigenes Modul `core/safety`, in jedem Regelzyklus vor dem Regler ausgewertet | M3, M5 | `test_safety_acts_before_controller_in_same_cycle`: Fällt der Garraum beim Heizen aus, ist der Ausgang im selben Zyklus 0. Die Reihenfolge steht an einer Stelle in `rof_controller_cycle()` |
| SAF-02 | Bei Fehler ist der Ausgang 0, und `SICHERER_ZUSTAND` bleibt gesetzt, bis manuell zurückgesetzt wird und die Ursache weg ist | M3, M5 | `test_trip_stays_latched_after_cause_is_gone`, `test_first_cause_is_kept`, `test_reset_while_cause_persists_is_rejected`, `test_reset_after_cause_is_gone_releases_output`, `test_reset_restarts_the_freeze_timer`. Im Regelzyklus geht jeder Tastgrad über `rof_safety_limit_duty()` (`test_over_temperature_cuts_output`, `test_reset_with_cause_active_is_rejected`, `test_reset_after_cause_is_gone_returns_to_idle`). Seit M4 (E-020): verriegelt wird beim Heizen und bei Übertemperatur; ohne Heizen bleibt eine Ursache aktiv, der Ausgang ist trotzdem 0 (`test_fault_without_heating_is_active_but_not_latched`) und der Automat zeigt FEHLER |
| SAF-03 | Auslöser: Lesefehler eines Sensors mit Sicherheitsrolle (Garraum) | M3 | auslösen: `test_chamber_read_error_trips`, `test_chamber_duplicate_trips`, `test_unknown_chamber_status_counts_as_read_error`; nicht auslösen: `test_valid_chamber_does_not_trip` |
| SAF-04 | Auslöser: Wert außerhalb des plausiblen Bereichs, Grenzen konfigurierbar | M3 | auslösen: `test_implausible_values_trip`, `test_plausible_range_is_configurable`; nicht auslösen: `test_plausible_values_do_not_trip` |
| SAF-05 | Auslöser: eingefrorener Wert, also keine Änderung über eine konfigurierbare Zeit bei aktivem Ausgang | M3 | auslösen: `test_unchanged_value_with_output_on_trips_after_timeout`, `test_changes_below_threshold_count_as_frozen`, `test_freeze_limits_are_configurable`; nicht auslösen: `test_real_change_restarts_the_timer`, `test_unchanged_value_with_output_off_does_not_trip`, `test_output_off_restarts_the_timer` |
| SAF-06 | Auslöser: Übertemperatur über einer harten Grenze, die nicht über die API änderbar ist | M3 | auslösen: `test_temperature_above_hard_limit_trips`, `test_hard_limit_ignores_wider_config`, `test_over_temperature_trips_with_output_off`; nicht auslösen: `test_temperature_at_hard_limit_does_not_trip`. Nicht über die API änderbar: Konstante `ROF_SAFETY_HARD_MAX_C`, kein Feld der Konfiguration (E-016) |
| SAF-07 | Auslöser: Garraumsensor fehlt oder ist keinem Kanal zugeordnet | M3 | auslösen: `test_unassigned_chamber_trips`, `test_missing_chamber_trips`; nicht auslösen: `test_valid_chamber_does_not_trip`. Den Rollenstatus liefert die Sensorverwaltung (SEN-05). Ohne Heizen führt die Ursache zu FEHLER statt zur Verriegelung (E-020, `test_u7_fault_in_idle_blocks_start`) |
| SAF-08 | Auslöser: Regelzyklus überschreitet sein Zeitfenster (Watchdog) | M3 | auslösen: `test_cycle_gap_over_window_trips`, `test_cycle_window_is_configurable`; nicht auslösen: `test_cycles_within_window_do_not_trip`, `test_first_cycle_has_no_window` |
| SAF-09 | Sollwerte außerhalb der Grenzen werden abgelehnt, nicht begrenzt, und der Grund wird gemeldet | M3, M6 | teilweise: `test_setpoint_within_limits_is_ok`, `test_setpoint_outside_limits_is_rejected_with_reason`. Die Meldung des Grunds über die API folgt in M6 |
| SAF-10 | Nach einem Neustart startet das System immer in `BEREIT` mit Ausgang 0. Die Heizung läuft erst nach erneutem Start | M4, M7 | teilweise: `test_starts_idle_with_output_off_and_no_setpoint`, `test_u10_successful_reset_returns_to_idle`, `test_start_without_setpoint_is_rejected`; `rof_controller_init` setzt den Ausgang ausdrücklich auf 0 (`test_init_turns_output_off_and_starts_idle`), Neustart mitten im Halten heizt nicht weiter (`test_restart_while_holding_does_not_resume_heating`). Neustart der Firmware folgt in M7 |
| SAF-11 | Jede Regel hat mindestens einen Test für Auslösen und einen für Nichtauslösen | M3 | jede Regel SAF-03 bis SAF-08 hat Tests für Auslösen und Nichtauslösen, siehe dort |
| SAF-12 | Zweigabdeckung von `core/safety` 100 Prozent, gemessen mit gcov in CI | M3 | CMake-Target `coverage` (gcovr, nur `core/safety`, Zeilen und Zweige 100 Prozent), CI-Schritt „Abdeckung core/safety“ (E-017) |
| SAF-13 | Integrationstest auf dem Host belegt die Entkopplung: Befehlsqueue geflutet bzw. kein Befehl mehr, die Regelung hält die Temperatur im Simulator trotzdem | M5 | `test_flooded_command_queue_does_not_disturb_control` (6 h lang je Sekunde 100 Befehle) und `test_silent_command_queue_does_not_disturb_control` (6 h ohne Befehl): HALTEN innerhalb ±1 °C, keine Verriegelung, höchstens 4 Befehle je Zyklus. Ohne die Begrenzung scheitert der erste Test (Gegenprobe). Echtes FreeRTOS-Scheduling folgt in M7 |

## SEN – Sensorverwaltung (Abschnitt 3)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| SEN-01 | Bis zu 8 Kanäle, jeder mit eindeutiger ID (DS18B20: ROM-Adresse), Typ, Rolle und Kalibrieroffset | M2 | `test_sensor_registry`: `test_plugged_sensors_appear_as_unassigned_channels` (ID, Typ, Rolle), `test_offset_is_added_to_reading`, `test_offset_outside_limits_is_rejected`, `test_nine_sensors_fill_eight_channels_and_report_overflow` (höchstens 8) |
| SEN-02 | Rollen: `GARRAUM`, `KERN_1`, `KERN_2`, `GENERATOR`, `NICHT_ZUGEORDNET` | M2 | `test_all_roles_can_be_assigned_to_different_sensors`, `test_assign_moves_role_from_previous_sensor`, `test_assign_unassigned_releases_role`, `test_assign_rejects_invalid_role` |
| SEN-03 | Treiber melden über `aufzaehlen()`, welche Sensoren da sind. Neue Sensoren erscheinen als nicht zugeordnet | M2 | `test_plugged_sensors_appear_as_unassigned_channels`, `test_unplugged_unassigned_sensor_frees_its_channel`, `test_replugged_sensor_gets_a_channel_again` |
| SEN-04 | Die Zuordnung ID zu Rolle wird über das Speicher-Interface gespeichert und nach Neustart wiederhergestellt | M2 | `test_assign_sets_role_and_saves`, `test_saved_block_has_version_count_and_entries`, `test_restart_restores_roles_and_offsets`, `test_assign_is_undone_when_saving_fails`, `test_corrupt_stored_blocks_are_discarded` |
| SEN-05 | Fehlt ein zugeordneter Sensor, wird seine Rolle als fehlerhaft gemeldet (beim Garraum siehe SAF-07) | M2 | `test_missing_assigned_sensor_reports_missing`, `test_read_error_reports_read_error`, `test_restored_role_is_missing_until_sensor_found`, `test_bus_error_makes_assigned_role_missing`. Auslösen beim Garraum folgt mit SAF-07 in M3 |
| SEN-06 | Ein weiterer Sensortyp (z. B. PT1000 mit MAX31865) kommt nur als neuer Treiber hinzu, ohne Änderung in core | M2 | `test_second_driver_type_works_without_core_changes`. Der Commit dazu ändert nur Tests, nichts in core |
| SEN-07 | Host-Tests: an- und abstecken, Rolle zuweisen, Neustart mit gespeicherter Zuordnung, doppelte IDs, mehr als 8 Sensoren | M2 | an- und abstecken: `test_unplugged_unassigned_sensor_frees_its_channel`; Rolle zuweisen: `test_assign_sets_role_and_saves`; Neustart: `test_restart_restores_roles_and_offsets`; doppelte IDs: `test_duplicate_id_gives_one_channel_marked_duplicate`, `test_duplicate_assigned_sensor_reports_duplicate`; mehr als 8: `test_nine_sensors_fill_eight_channels_and_report_overflow`, `test_assigned_sensor_has_priority_over_new_sensors` |

## CTL – Regelung, Simulator, Zustandsautomat (Abschnitt 4)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| CTL-01 | Simulator auf dem Host: thermisches Modell erster Ordnung mit Totzeit für Garraum und Kern, Parameter konfigurierbar | M5 | `platform/host/sim_oven.{h,c}`, Garraum und Kern erster Ordnung mit Totzeit, Parameter in `sim_oven_config_t`. `test_sim_oven` vergleicht die Sprungantwort mit der analytischen Lösung (`test_step_response_matches_first_order_model`, `test_chamber_does_not_react_within_dead_time`, `test_probe_follows_chamber_slowly`) |
| CTL-02 | Regler: PI mit Anti-Windup | M5 | `core/control/rof_pi.c`, `test_pi` mit P- und I-Anteil, Begrenzung, Anti-Windup nach oben und unten (`test_integral_does_not_wind_up_while_saturated`, `test_integral_does_not_wind_down_while_saturated`), Neustart des Reglers beim Heizen (`test_pi_restarts_when_heating_starts_again`) |
| CTL-03 | Ausgang als langsame Pulsweitenmodulation, Zykluszeit konfigurierbar, Standard 4 Sekunden | M5 | `core/control/rof_slow_pwm.c`, Standard 4 s (`test_default_period_is_four_seconds`), konfigurierbar (`test_period_is_configurable`), Einschaltdauer je Fenster gleich Tastgrad (`test_on_time_per_window_matches_duty`). Der Simulator schaltet seine Heizung damit; der LED-Treiber folgt in M7 |
| CTL-04 | Zustandsautomat mit `BEREIT`, `AUFHEIZEN`, `HALTEN`, `ABKUEHLEN`, `FEHLER`, `SICHERER_ZUSTAND` | M4 | `test_state_machine` mit allen sechs Zuständen, Zustandstabelle in `docs/zustandsautomat.md` |
| CTL-05 | Übergänge als Tabelle in `docs/zustandsautomat.md`, jeder Übergang hat einen Test | M4 | Übergänge Ü1 bis Ü10 als Tabelle in `docs/zustandsautomat.md`, je Übergang mindestens ein Test, dazu Gegenproben ohne Übergang |
| CTL-06 | Integrationstests über mehrere Stunden simulierte Zeit, inklusive Sensorausfall nach Stunden | M5 | `test_integration`: 6 h Halten bei 80 °C (HALTEN nach 8,8 min, Überschwingen 0,19 °C, danach ±0,2 °C), Abkühlen bis BEREIT, Garraumsensor fällt nach 3 h aus, Wert friert nach 3 h ein, Heizung klemmt, Neustart nach 2 h Halten |

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
| CI-01 | Host: CMake-Build, ctest, gcov für `core/safety`, cppcheck, Prüfung der Include-Regeln | M0, M1, M3 | `.github/workflows/ci.yml`, Job `host`: Build, ctest (inklusive Include-Prüfung), cppcheck, gcov-Abdeckung von `core/safety` |
| CI-02 | CI prüft, dass core und proto keinen ESP- oder FreeRTOS-Header einbinden und dass `firmware/net` keine Treiber einbindet. Bei Verstoß schlägt der Build fehl | M1, M8 | ctest `include_rules` mit sieben Gegenproben `include_rules_detects_*` und `include_rules_accepts_clean_tree`. Der Pfad `firmware/net` wird in M8 gegen die echte Struktur geprüft |
| CI-03 | Web: `npm ci`, Lint, Typprüfung, Vitest, Build, Größenbudget | M9 | offen |
| CI-04 | Firmware: `idf.py build` im Container `espressif/idf` mit fester Version | M7 | offen |

## FC – Fertigzeit-Prognose (optional, M10)

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| FC-01 | Modell schätzt aus den Messwerten die Annäherung der Kerntemperatur an den Garraum, Ausgabe als Zeitspanne, Tests mit simulierten Verläufen | M10 | offen |

## Nicht im Umfang

Netzspannung, Display, MQTT sowie Funktionen außerhalb dieser Liste.
