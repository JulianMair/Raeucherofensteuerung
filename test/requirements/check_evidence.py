#!/usr/bin/env python3
"""Prüft die Nachweise in docs/anforderungen.md (M11, E-049).

Aufruf:  python3 check_evidence.py [WURZEL]
WURZEL ist das Projektverzeichnis, Standard ist das Repository dieses Skripts.

Für jede Zeile einer Anforderung (| ID | Anforderung | Meilenstein | Nachweis |):
  - Der Nachweis ist nicht leer.
  - Jede genannte Testfunktion `test_...` gibt es in test/*.c, entweder als
    Funktion oder als Testprogramm test/<name>.c. `test_x_*` steht für alle,
    die so beginnen; mindestens eine muss es geben.
  - Jeder genannte Pfad (core/, proto/, platform/, firmware/, web/, test/,
    docs/, .github/) existiert. `a.{h,c}` steht für a.h und a.c.
    Ausgenommen sind die Pfade in GENERATED: Sie entstehen erst beim Bauen
    oder stehen absichtlich nicht im Repository und fehlen deshalb in einem
    frischen Checkout wie in CI.
Exit-Code 0: alle Nachweise stimmen. Exit-Code 1: mindestens einer nicht.
"""
import re
import sys
from pathlib import Path

ROW_RE = re.compile(r"^\| ([A-Z]+-\d+) \|(.*)\|$")
CODE_RE = re.compile(r"`([^`]+)`")
TEST_RE = re.compile(r"^test_[A-Za-z0-9_]+\*?$")
PATH_PREFIXES = ("core/", "proto/", "platform/", "firmware/", "web/", "test/", "docs/", ".github/")
# web/dist entsteht beim Bauen der Oberfläche, firmware/sdkconfig enthält das
# WLAN-Passwort und steht deshalb in .gitignore (NET-04).
GENERATED = ("web/dist", "firmware/sdkconfig")


def requirement_rows(text):
    """(ID, Nachweis) für jede Zeile einer Anforderung"""
    for line in text.splitlines():
        match = ROW_RE.match(line.strip())
        if match:
            cells = [cell.strip() for cell in match.group(2).split(" | ")]
            yield match.group(1), cells[-1] if cells else ""


def test_names(root):
    """Alle Testfunktionen und Testprogramme in test/*.c"""
    names = set()
    for path in (root / "test").glob("*.c"):
        names.add(path.stem)
        names.update(re.findall(r"static void (test_[A-Za-z0-9_]+)\(", path.read_text(errors="replace")))
    return names


def expand(path):
    """a.{h,c} wird zu a.h und a.c"""
    match = re.match(r"^(.*)\{([^}]*)\}(.*)$", path)
    if not match:
        return [path]
    return [match.group(1) + part + match.group(3) for part in match.group(2).split(",")]


def check(root):
    doc = root / "docs" / "anforderungen.md"
    names = test_names(root)
    problems = []
    for rid, evidence in requirement_rows(doc.read_text()):
        if evidence == "":
            problems.append(f"{rid}: kein Nachweis")
            continue
        for code in CODE_RE.findall(evidence):
            if TEST_RE.match(code):
                if code.endswith("*"):
                    found = any(name.startswith(code[:-1]) for name in names)
                else:
                    found = code in names
                if not found:
                    problems.append(f"{rid}: Test nicht gefunden: {code}")
            elif code.startswith(PATH_PREFIXES):
                for path in expand(code.split()[0]):
                    if path.rstrip("/") in GENERATED:
                        continue
                    if not (root / path.rstrip("/")).exists():
                        problems.append(f"{rid}: Pfad nicht gefunden: {path}")
    return problems


def main(argv):
    root = Path(argv[1]).resolve() if len(argv) > 1 else Path(__file__).resolve().parents[2]
    problems = check(root)
    for problem in problems:
        print(problem)
    if problems:
        print(f"Nachweise fehlerhaft: {len(problems)}")
        return 1
    print("Alle Nachweise zeigen auf vorhandene Tests und Dateien")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
