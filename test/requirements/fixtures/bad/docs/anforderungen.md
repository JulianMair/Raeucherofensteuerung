# Gegenprobe für check_evidence.py: absichtlich falsche Nachweise

| ID | Anforderung | Meilenstein | Nachweis |
|---|---|---|---|
| ABC-01 | stimmt | M1 | `test_exists` in `test/test_ok.c`, gebaut nach `web/dist` |
| ABC-02 | Test fehlt | M1 | `test_missing` |
| ABC-03 | Datei fehlt | M1 | `core/fehlt.c` |
| ABC-04 | kein Nachweis | M1 |  |
