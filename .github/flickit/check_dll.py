"""Compare freshly built thugsdl.dll files with the one shipped in Game/.

usage: check_dll.py <shipped.dll> <built.dll> [<built.dll> ...]

A built DLL replaces the shipped one in a player's game folder, so it has to load wherever the shipped
one loads. This fails the build if a built DLL is not a 32-bit DLL, lacks the Direct3DCreate9 export or
depends on a DLL the shipped one does not need. It also lists every function a built DLL imports that
the shipped one does not, because a C++ runtime function that only a newer Visual C++ redistributable
has would stop the DLL from loading on a PC with an older one.
"""
import os
import sys

import pefile

RUNTIME_DLLS = ("msvcp140", "vcruntime140")


def describe(path):
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(directories=[
        pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"],
        pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"],
    ])
    imports = {}
    for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        names = imports.setdefault(entry.dll.decode("latin-1").lower(), set())
        for imp in entry.imports:
            names.add(imp.name.decode("latin-1") if imp.name else "#%d" % imp.ordinal)
    export_dir = getattr(pe, "DIRECTORY_ENTRY_EXPORT", None)
    exports = sorted(s.name.decode("latin-1") for s in export_dir.symbols if s.name) if export_dir else []
    return {
        "path": path,
        "size": os.path.getsize(path),
        "machine": pe.FILE_HEADER.Machine,
        "is_dll": pe.is_dll(),
        "linker": "%d.%02d" % (pe.OPTIONAL_HEADER.MajorLinkerVersion, pe.OPTIONAL_HEADER.MinorLinkerVersion),
        "sections": [(s.Name.rstrip(b"\0").decode("latin-1"), s.Misc_VirtualSize) for s in pe.sections],
        "imports": imports,
        "exports": exports,
    }


def show(info):
    sections = "  ".join("%s %d" % section for section in info["sections"])
    print("%s\n  %d bytes, linker %s, sections: %s" % (info["path"], info["size"], info["linker"], sections))
    print("  exports: %s" % ", ".join(info["exports"]))


def check(shipped, built):
    problems = []
    if built["machine"] != 0x14C or not built["is_dll"]:
        problems.append("not a 32-bit x86 DLL")
    for name in shipped["exports"]:
        if name not in built["exports"]:
            problems.append("export %s is missing" % name)
    for dll in sorted(set(built["imports"]) - set(shipped["imports"])):
        problems.append("needs %s, which the shipped DLL does not" % dll)
    for dll in sorted(set(built["imports"]) & set(shipped["imports"])):
        added = sorted(built["imports"][dll] - shipped["imports"][dll])
        if not added:
            continue
        print("  imports from %s that the shipped DLL does not have: %s" % (dll, ", ".join(added)))
        if dll.startswith(RUNTIME_DLLS):
            print("::warning::%s imports %s from %s, which the shipped DLL does not. Check that it is not "
                  "something only a newer Visual C++ redistributable has." % (built["path"], ", ".join(added), dll))
    for dll in sorted(set(shipped["imports"]) - set(built["imports"])):
        print("  no longer needs %s" % dll)
    return problems


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    shipped = describe(argv[1])
    show(shipped)
    failed = False
    for path in argv[2:]:
        built = describe(path)
        show(built)
        problems = check(shipped, built)
        for problem in problems:
            print("::error::%s: %s" % (path, problem))
        print("  %s" % ("FAILED" if problems else "ok: loads wherever the shipped DLL loads, as far as its imports and exports show"))
        failed = failed or bool(problems)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
