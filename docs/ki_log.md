# KI-Log

Kennzeichnung der KI-Nutzung je Meilenstein. Werkzeug: Claude Code (Anthropic). Die Vorgaben stehen in `CLAUDE.md`.

## 2026-10-26 – M0 Gerüst

**Modell:** Claude Opus 5.5

**Von der KI erzeugt:**
- Plan für M0 mit Rückfragen
- Ordnerstruktur mit `.gitkeep`, `.gitignore`
- Haupt-`CMakeLists.txt` mit C11, Warnungen als Fehler und cppcheck-Target
- `test/CMakeLists.txt`, `test/test_dummy.c`
- Übernahme von Unity v2.6.1 nach `test/vendor/unity/` (unverändert, Quelle und Prüfsumme in der README dort)
- `.github/workflows/ci.yml`
- `README.md`, `docs/anforderungen.md` mit IDs, `docs/entscheidungen.md` (E-001 bis E-005), Gerüste für `docs/zustandsautomat.md` und `docs/api.yaml`
- Anpassung von Gitea auf GitHub in `CLAUDE.md`

**Vom Nutzer entschieden (Rückfragen im Plan):**
- Unity als Kopie im Repository
- GitHub statt Gitea
- Werkzeuge installiert der Nutzer selbst
- Anforderungen bekommen schon in M0 IDs

**Vom Nutzer geändert oder abgelehnt:**
- Den vorgeschlagenen CI-Nachbau in einem Docker-Container (`ubuntu:24.04`) abgelehnt. Geprüft wird lokal mit clang und über GitHub Actions mit GCC.
- Das GitHub-Repo selbst angelegt und `main` selbst gepusht, ohne die vorgeschlagene gh-CLI. Das Repo ist öffentlich statt wie empfohlen privat.
- Am erzeugten Code und an der Doku nichts geändert.

**Ergebnis:** Der erste CI-Lauf auf GitHub (Job `host`) war grün.
