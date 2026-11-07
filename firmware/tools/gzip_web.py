#!/usr/bin/env python3
"""Komprimiert die Dateien der Weboberfläche für LittleFS (NET-03, E-038).

Aufruf:  python3 gzip_web.py QUELLE ZIEL
Jede Datei QUELLE/pfad wird zu ZIEL/pfad.gz. Versteckte Dateien wie
.DS_Store bleiben draußen. Die Zeit im gzip-Kopf ist 0, damit bei gleichem
Inhalt das gleiche Abbild entsteht. ZIEL wird vorher geleert.
"""
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
    if len(argv) != 3:
        print(__doc__)
        return 2
    source, target = Path(argv[1]), Path(argv[2])
    if target.exists():
        shutil.rmtree(target)
    total = 0
    for path, relative in visible_files(source):
        out = target / relative.parent / (relative.name + ".gz")
        out.parent.mkdir(parents=True, exist_ok=True)
        data = path.read_bytes()
        with open(out, "wb") as raw, gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as gz:
            gz.write(data)
        size = out.stat().st_size
        total += size
        print(f"{relative}: {len(data)} B -> {size} B")
    print(f"Weboberfläche gzip gesamt: {total} B")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
