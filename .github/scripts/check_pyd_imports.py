"""Print DLL imports of the installed mantisbase extension that the loaded
DLL does not export. Used when the Windows wheel smoke test fails."""

import glob
import os
import site
import sys

import pefile


def find_pyd():
    roots = []
    try:
        roots.extend(site.getsitepackages())
    except Exception:
        pass
    roots.append(site.getusersitepackages())
    hits = []
    for root in roots:
        hits.extend(glob.glob(os.path.join(root, "**", "mantisbase*.pyd"), recursive=True))
    return hits[0] if hits else None


def exports(path):
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
    names = set()
    if hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        for sym in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            if sym.name:
                names.add(sym.name.decode(errors="replace"))
    return names


def locate(dll, pyd):
    found = []
    for dirpath, _, files in os.walk(os.path.dirname(pyd)):
        for name in files:
            if name.lower() == dll.lower():
                found.append(os.path.join(dirpath, name))
    base = os.path.dirname(sys.executable)
    for folder in (base, os.path.join(base, "DLLs"), os.path.join(base, "libs")):
        cand = os.path.join(folder, dll)
        if os.path.isfile(cand):
            found.append(cand)
    for folder in os.environ.get("PATH", "").split(os.pathsep):
        cand = os.path.join(folder, dll)
        if os.path.isfile(cand):
            found.append(cand)
    return found


def main():
    pyd = find_pyd()
    print("pyd:", pyd)
    if not pyd:
        return
    pe = pefile.PE(pyd, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
    missing = []
    for entry in pe.DIRECTORY_ENTRY_IMPORT:
        dll = entry.dll.decode(errors="replace")
        paths = locate(dll, pyd)
        print(f"import {dll} -> {paths[0] if paths else 'NOT FOUND'}")
        if not paths:
            missing.append(f"{dll}: DLL not found")
            continue
        have = exports(paths[0])
        for imp in entry.imports:
            if not imp.name:
                continue
            name = imp.name.decode(errors="replace")
            if name not in have:
                missing.append(f"{dll}: {name}")
    if missing:
        print("missing procedures:")
        for line in missing:
            print(" ", line)


if __name__ == "__main__":
    main()
