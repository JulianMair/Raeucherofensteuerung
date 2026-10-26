#!/usr/bin/env python3
"""Prüft die Include-Regeln der Architektur (CI-02, ARC-01, ARC-03).

Aufruf:  python3 check_includes.py [WURZEL]
WURZEL ist das Projektverzeichnis, Standard ist das Repository dieses Skripts.
Exit-Code 0: alle Regeln eingehalten. Exit-Code 1: mindestens ein Verstoß.

Regeln:
  core/          nur eigene Header aus core/ und die Standard-Header in CORE_STD
  proto/         nur eigene Header, core/include und die C11-Standardbibliothek
  firmware/net/  keine Treiber: weder ESP-IDF "driver/..." noch Header aus platform/esp32/
"""
import re
import sys
from pathlib import Path

# Standard-Header, die core einbinden darf. Was bewusst fehlt, begründet CORE_HINTS.
CORE_STD = {"stdint.h", "stdbool.h", "stddef.h", "string.h", "math.h", "float.h", "limits.h"}
CORE_HINTS = {
    "stdlib.h": "kein malloc in core (ARC-01)",
    "time.h": "Zeit nur über rof_clock_t (ARC-03)",
    "stdio.h": "Ausgaben nur über rof_log_t",
}

C11_STD = {
    "assert.h", "complex.h", "ctype.h", "errno.h", "fenv.h", "float.h", "inttypes.h",
    "iso646.h", "limits.h", "locale.h", "math.h", "setjmp.h", "signal.h", "stdalign.h",
    "stdarg.h", "stdatomic.h", "stdbool.h", "stddef.h", "stdint.h", "stdio.h", "stdlib.h",
    "stdnoreturn.h", "string.h", "tgmath.h", "threads.h", "time.h", "uchar.h", "wchar.h",
    "wctype.h",
}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')


def includes(path):
    """Liefert (Zeilennummer, Klammerart, Name) für jedes #include der Datei."""
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    for number, line in enumerate(lines, start=1):
        match = INCLUDE_RE.match(line)
        if match:
            yield number, match.group(1), match.group(2)


def sources(directory):
    if not directory.is_dir():
        return []
    return sorted(p for p in directory.rglob("*") if p.suffix in (".c", ".h"))


def exists_in(name, directories):
    return any((d / name).is_file() for d in directories)


def check_core(root, violations):
    core = root / "core"
    for path in sources(core):
        own = [path.parent, core / "include"]
        for line, bracket, name in includes(path):
            if bracket == '"' and exists_in(name, own):
                continue
            if bracket == "<" and name in CORE_STD:
                continue
            hint = CORE_HINTS.get(name, "core darf nur eigene Header und " +
                                  ", ".join(sorted(CORE_STD)) + " einbinden")
            violations.append((path, line, name, hint))


def check_proto(root, violations):
    proto = root / "proto"
    for path in sources(proto):
        own = [path.parent, proto, proto / "include", root / "core" / "include"]
        for line, bracket, name in includes(path):
            if bracket == '"' and exists_in(name, own):
                continue
            if bracket == "<" and name in C11_STD:
                continue
            violations.append((path, line, name,
                               "proto darf nur eigene Header, core/include und die C-Standardbibliothek einbinden"))


def check_net(root, violations):
    net = root / "firmware" / "net"
    esp32 = root / "platform" / "esp32"
    driver_headers = {p.name for p in esp32.rglob("*.h")} if esp32.is_dir() else set()
    for path in sources(net):
        for line, _bracket, name in includes(path):
            if name.startswith("driver/"):
                violations.append((path, line, name, "firmware/net bindet keine ESP-IDF-Treiber ein"))
            elif Path(name).name in driver_headers:
                violations.append((path, line, name,
                                   "firmware/net bindet keine Treiber aus platform/esp32 ein"))


def main(argv):
    root = Path(argv[1]).resolve() if len(argv) > 1 else Path(__file__).resolve().parents[2]
    violations = []
    check_core(root, violations)
    check_proto(root, violations)
    check_net(root, violations)

    for path, line, name, hint in violations:
        print(f"{path.relative_to(root)}:{line}: verbotenes Include {name} ({hint})")
    if violations:
        print(f"Include-Regeln verletzt: {len(violations)}")
        return 1
    print("Include-Regeln eingehalten")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
