"""Print where the DLL's own functions are, from the MSVC linker's map (thugsdl.map), for the CI log.

The guard (src/guard_game.h) writes an address in the DLL as thugsdl.dll+offset. This lists the offset each of
the DLL's own functions starts at (those of its source files; the C runtime's and SDL's are left out), so that
the function an offset is in is the last one listed at or below it.

usage: map_functions.py thugsdl.map
"""
import re
import sys

LINE = re.compile(r"^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8})\s+((?:[fi]\s+)*)(\S+)\s*$")
BASE = re.compile(r"Preferred load address is ([0-9a-fA-F]+)")


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    base = None
    functions = {}
    with open(sys.argv[1], encoding="latin-1") as f:
        for line in f:
            m = BASE.search(line)
            if m:
                base = int(m.group(1), 16)
                continue
            m = LINE.match(line)
            if not m or base is None:
                continue
            name, address, flags, obj = m.group(1), int(m.group(2), 16), m.group(3), m.group(4)
            if "f" not in flags.split() or ":" in obj or address < base:
                continue
            functions.setdefault(address - base, (name, obj))
    if base is None or not functions:
        print("map_functions: no load address or no functions of the DLL's own in", sys.argv[1])
        return 1
    print("The DLL's own functions: thugsdl.dll+offset, name, source (preferred load address %08X)" % base)
    for offset in sorted(functions):
        name, obj = functions[offset]
        print("+%06X  %s  %s" % (offset, name, obj))
    print("%d functions" % len(functions))
    return 0


if __name__ == "__main__":
    sys.exit(main())
