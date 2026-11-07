#!/usr/bin/env python3
"""Komprimiert die Dateien der Weboberfläche für LittleFS (NET-03, WEB-07, E-038, E-043).

Aufruf:  python3 gzip_web.py QUELLE ZIEL [--max-bytes N]
Jede Datei QUELLE/pfad wird zu ZIEL/pfad.gz. Versteckte Dateien wie
.DS_Store bleiben draußen. Die Zeit im gzip-Kopf ist 0, damit bei gleichem
Inhalt das gleiche Abbild entsteht. ZIEL wird vorher geleert.
Mit --max-bytes scheitert der Aufruf, wenn alle .gz zusammen größer sind
(Größenbudget). Fehlt QUELLE, scheitert er ebenfalls.
"""
import argparse
import gzip
import shutil
import sys
from pathlib import Path


def visible_files(source):
    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source)
        if path.is_file() and not any(part.startswith(".") for part in relative.parts):
            yield path, relative


def main(argv):
    parser = argparse.ArgumentParser(description="Weboberfläche mit gzip für LittleFS komprimieren")
    parser.add_argument("source", type=Path)
    parser.add_argument("target", type=Path)
    parser.add_argument("--max-bytes", type=int, help="Größenbudget für alle .gz zusammen")
    args = parser.parse_args(argv[1:])

    if not args.source.is_dir():
        print(f"Ordner der Weboberfläche fehlt: {args.source}. Erst bauen: npm --prefix web ci && npm --prefix web run build")
        return 1
    if args.target.exists():
        shutil.rmtree(args.target)
    total = 0
    for path, relative in visible_files(args.source):
        out = args.target / relative.parent / (relative.name + ".gz")
        out.parent.mkdir(parents=True, exist_ok=True)
        data = path.read_bytes()
        with open(out, "wb") as raw, gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as gz:
            gz.write(data)
        size = out.stat().st_size
        total += size
        print(f"{relative}: {len(data)} B -> {size} B")
    print(f"Weboberfläche gzip gesamt: {total} B")
    if args.max_bytes is not None:
        if total > args.max_bytes:
            print(f"Größenbudget überschritten: {total} B > {args.max_bytes} B")
            return 1
        print(f"Größenbudget eingehalten: {total} B <= {args.max_bytes} B")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
