"""Compare freshly built thugsdl.dll files with the one shipped in Game/.

usage: check_dll.py [--sdl2 Game/SDL2.dll] <shipped thugsdl.dll> <built thugsdl.dll> [<built thugsdl.dll> ...]

A built DLL replaces the shipped one in a player's game folder, so it has to load wherever the shipped
one loads. This fails the build if a built DLL
  - is not a 32-bit DLL or lacks the Direct3DCreate9 export,
  - depends on a DLL the shipped one does not need,
  - calls an SDL function that the SDL2.dll shipped next to it does not have (--sdl2).
It also lists every function a built DLL imports that the shipped one does not, and warns when such a
function comes from the Visual C++ runtime, because one that only a newer Visual C++ redistributable
has would stop the DLL from loading on a PC with an older one.
"""
import argparse
import os
import sys

import pefile

RUNTIME_DLLS = ("msvcp140", "vcruntime140")

# Functions that vcruntime140.dll has exported since its first version (Visual Studio 2015). The plain C
# ones: whether the compiler calls them or expands them inline depends on the optimization level. The
# current exception and its context: the C++ exception handling the compiler makes for a catch block
# asks them (in the export list of the first vcruntime140.dll, which Wine copied in October 2015, and
# msvcp140.dll's own exception_ptr uses them).
RUNTIME_BASELINE = frozenset((
    "memchr", "memcmp", "memcpy", "memmove", "memset",
    "strchr", "strrchr", "strstr", "wcschr", "wcsrchr", "wcsstr",
    "__current_exception", "__current_exception_context",
))


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


def check(shipped, built, sdl2):
    problems = []
    if built["machine"] != 0x14C or not built["is_dll"]:
        problems.append("not a 32-bit x86 DLL")
    for name in shipped["exports"]:
        if name not in built["exports"]:
            problems.append("export %s is missing" % name)
    for dll in sorted(set(built["imports"]) - set(shipped["imports"])):
        problems.append("needs %s, which the shipped DLL does not" % dll)
    if sdl2 is not None:
        for name in sorted(built["imports"].get("sdl2.dll", set()) - set(sdl2["exports"])):
            problems.append("calls %s, which %s does not have" % (name, sdl2["path"]))

    for dll in sorted(set(built["imports"]) & set(shipped["imports"])):
        added = sorted(built["imports"][dll] - shipped["imports"][dll])
        if not added:
            continue
        print("  imports from %s that the shipped DLL does not have: %s" % (dll, ", ".join(added)))
        unknown = [name for name in added if name not in RUNTIME_BASELINE]
        if dll.startswith(RUNTIME_DLLS) and unknown:
            print("::warning::%s imports %s from %s, which the shipped DLL does not. Check that it is not "
                  "something only a newer Visual C++ redistributable has." % (built["path"], ", ".join(unknown), dll))
    for dll in sorted(set(shipped["imports"]) - set(built["imports"])):
        print("  no longer needs %s" % dll)
    return problems


def main():
    parser = argparse.ArgumentParser(description="Compare built thugsdl.dll files with the shipped one.")
    parser.add_argument("--sdl2", help="the SDL2.dll players have; every SDL function a built DLL calls must be in it")
    parser.add_argument("shipped")
    parser.add_argument("built", nargs="+")
    args = parser.parse_args()

    shipped = describe(args.shipped)
    show(shipped)
    sdl2 = None
    if args.sdl2:
        sdl2 = describe(args.sdl2)
        print("%s\n  %d bytes, %d functions" % (sdl2["path"], sdl2["size"], len(sdl2["exports"])))

    failed = False
    for path in args.built:
        built = describe(path)
        show(built)
        problems = check(shipped, built, sdl2)
        for problem in problems:
            print("::error::%s: %s" % (path, problem))
        print("  %s" % ("FAILED" if problems else "ok"))
        failed = failed or bool(problems)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
