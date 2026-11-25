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


def check_pe(path, missing, seen, depth):
    key = os.path.normcase(os.path.abspath(path))
    if key in seen or depth > 2:
        return
    seen.add(key)
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
    if not hasattr(pe, "DIRECTORY_ENTRY_IMPORT"):
        return
    for entry in pe.DIRECTORY_ENTRY_IMPORT:
        dll = entry.dll.decode(errors="replace")
        paths = locate(dll, path)
        chosen = paths[0] if paths else None
        print(f"import {dll} -> {chosen or 'NOT FOUND'} (from {os.path.basename(path)})")
        if not chosen:
            missing.append(f"{dll}: DLL not found")
            continue
        have = exports(chosen)
        for imp in entry.imports:
            if not imp.name:
                continue
            name = imp.name.decode(errors="replace")
            if name not in have:
                missing.append(f"{dll}: {name}")
        # The loader fails on a missing procedure in libmantisbase.dll even
        # when every direct import of the pyd itself resolves.
        bundled = os.path.dirname(os.path.abspath(chosen)) == os.path.dirname(os.path.abspath(path))
        if bundled and not dll.lower().startswith("api-ms-win-"):
            check_pe(chosen, missing, seen, depth + 1)


def main():
    pyd = find_pyd()
    print("pyd:", pyd)
    if not pyd:
        return
    missing = []
    check_pe(pyd, missing, set(), 0)
    if missing:
        print("missing procedures:")
        for line in missing:
            print(" ", line)


if __name__ == "__main__":
    main()
