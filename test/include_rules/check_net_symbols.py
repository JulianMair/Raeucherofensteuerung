#!/usr/bin/env python3
"""Prüft, dass firmware/net weder core noch Treiber aufruft (ARC-04, CI-02, E-042).

Die Include-Regel verbietet Treiber-Header. Ob net trotzdem eine Funktion
aus core oder einem Treiber aufruft, zeigen erst die Symbole der gebauten
Bibliotheken: Kein Symbol, das net braucht (nm -u), darf eine verbotene
Bibliothek bereitstellen (nm --defined-only).

Aufruf in CI, nach dem Firmware-Build:
  check_net_symbols.py --nm NM --net LIB --forbidden LIB [LIB ...]
Gegenprobe mit fertigen nm-Ausgaben:
  check_net_symbols.py --net-symbols DATEI --forbidden-symbols DATEI
Exit-Code 0: keine verbotenen Aufrufe. Exit-Code 1: mindestens einer.
"""
import argparse
import subprocess
import sys
from pathlib import Path


def parse(text, undefined):
    """Symbole aus einer nm-Ausgabe: gebrauchte (U) oder bereitgestellte globale."""
    symbols = set()
    for line in text.splitlines():
        parts = line.split()
        if len(parts) < 2 or len(parts[-2]) != 1:
            continue  # Kopfzeile einer Objektdatei oder leer
        kind, name = parts[-2], parts[-1]
        if undefined and kind == "U":
            symbols.add(name)
        elif not undefined and kind.isupper() and kind != "U":
            symbols.add(name)
    return symbols


def run_nm(nm, args, library):
    return subprocess.run([nm, *args, library], check=True, capture_output=True, text=True).stdout


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--nm")
    parser.add_argument("--net")
    parser.add_argument("--forbidden", nargs="+", default=[])
    parser.add_argument("--net-symbols")
    parser.add_argument("--forbidden-symbols")
    args = parser.parse_args(argv[1:])

    if args.net_symbols:
        needed = parse(Path(args.net_symbols).read_text(), undefined=True)
        provided = {name: Path(args.forbidden_symbols).name
                    for name in parse(Path(args.forbidden_symbols).read_text(), undefined=False)}
    else:
        needed = parse(run_nm(args.nm, ["-u"], args.net), undefined=True)
        provided = {}
        for library in args.forbidden:
            for name in parse(run_nm(args.nm, ["--defined-only"], library), undefined=False):
                provided[name] = Path(library).name

    violations = sorted(needed & provided.keys())
    for name in violations:
        print(f"firmware/net ruft {name} aus {provided[name]} auf (ARC-04)")
    if violations:
        print(f"Verbotene Aufrufe aus firmware/net: {len(violations)}")
        return 1
    print(f"firmware/net ruft weder core noch Treiber auf ({len(needed)} Symbole geprüft)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
