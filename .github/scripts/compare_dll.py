# Temporary: how close is a freshly built thugsdl.dll to the one shipped in Game/?
import os
import sys

import pefile


def text_of(path):
    pe = pefile.PE(path)
    for s in pe.sections:
        if s.Name.rstrip(b"\0") == b".text":
            return pe, s.get_data()[: s.Misc_VirtualSize]
    raise SystemExit("no .text in " + path)


def windows(data, n):
    return set(data[i : i + n] for i in range(0, len(data) - n + 1))


built_path, flags = sys.argv[1], sys.argv[2]
shipped_path = "Game/thugsdl.dll"
pe_b, text_b = text_of(built_path)
pe_s, text_s = text_of(shipped_path)

print("FLAGS", flags)
for label, path, pe in (("BUILT", built_path, pe_b), ("SHIPPED", shipped_path, pe_s)):
    print(
        label,
        os.path.getsize(path),
        "linker %d.%d" % (pe.OPTIONAL_HEADER.MajorLinkerVersion, pe.OPTIONAL_HEADER.MinorLinkerVersion),
        " ".join("%s=%d" % (s.Name.rstrip(b"\0").decode(), s.Misc_VirtualSize) for s in pe.sections),
    )
for n in (6, 10, 16):
    wb, ws = windows(text_b, n), windows(text_s, n)
    print("SIMILARITY n=%d: %.1f%% of the shipped code's %d-byte windows also occur in this build" % (n, 100.0 * len(ws & wb) / len(ws), n))
