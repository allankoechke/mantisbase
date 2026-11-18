"""Package version for the mantisbase Python distribution.

The base version is ``project(MantisBase VERSION x.y.z)`` in the repository
CMakeLists.txt. On GitHub Actions, a release tag selects the PEP 440 form:

- ``v0.4.9`` stays ``0.4.9``
- ``v0.4.9-alpha.1`` becomes ``0.4.9a1``
- ``v0.4.9-beta.1`` becomes ``0.4.9b1``
- ``v0.4.9-rc.1`` becomes ``0.4.9rc1``

``MB_PYTHON_VERSION`` overrides the tag. The numeric part must still match
the CMake version. A wheel built from the sdist reads ``_mb_version.txt``,
which the sdist build records after the tag has been applied.
"""

from __future__ import annotations

import os
import re
import sys
from pathlib import Path

_CMAKE_VERSION = re.compile(
    r"project\(MantisBase VERSION (?P<value>[0-9]+\.[0-9]+\.[0-9]+)\)"
)
_TAG = re.compile(
    r"v(?P<base>[0-9]+\.[0-9]+\.[0-9]+)"
    r"(?:-(?P<channel>alpha|beta|rc)\.(?P<n>[0-9]+))?"
)
_PEP440 = re.compile(
    r"^(?P<base>[0-9]+\.[0-9]+\.[0-9]+)(?:(?P<kind>a|b|rc)(?P<n>[0-9]+))?$"
)
_CHANNEL = {"alpha": "a", "beta": "b", "rc": "rc"}


class VersionError(RuntimeError):
    pass


def _cmake_version(project_dir: Path) -> str | None:
    cmake = project_dir.parent.parent / "CMakeLists.txt"
    if not cmake.is_file():
        return None
    match = _CMAKE_VERSION.search(cmake.read_text(encoding="utf-8"))
    if match is None:
        raise VersionError(f"MantisBase version not found in {cmake}")
    return match.group("value")


def _recorded_version(project_dir: Path) -> str | None:
    path = project_dir / "_mb_version.txt"
    if not path.is_file():
        return None
    text = path.read_text(encoding="utf-8").strip()
    if _PEP440.fullmatch(text) is None:
        raise VersionError(f"Invalid recorded version {text!r} in {path}")
    return text


def _pep440(base: str, channel: str | None, number: str | None) -> str:
    if channel is None:
        return base
    return f"{base}{_CHANNEL[channel]}{number}"


def _check_override(explicit: str, base: str) -> str:
    match = _PEP440.fullmatch(explicit)
    if match is None or match.group("base") != base:
        raise VersionError(
            f"MB_PYTHON_VERSION {explicit!r} must be {base} "
            f"or a PEP 440 pre-release of it, for example {base}b1"
        )
    return explicit


def resolve_version(project_dir: Path | None = None) -> str:
    if project_dir is None:
        project_dir = Path(__file__).resolve().parent

    base = _cmake_version(project_dir)
    if base is None:
        recorded = _recorded_version(project_dir)
        if recorded is None:
            raise VersionError(
                "MantisBase version not found. Expected "
                "../../CMakeLists.txt or _mb_version.txt next to the Python project."
            )
        return recorded

    explicit = os.environ.get("MB_PYTHON_VERSION", "").strip()
    if explicit:
        return _check_override(explicit, base)

    tag = os.environ.get("GITHUB_REF_NAME", "").strip()
    match = _TAG.fullmatch(tag)
    if match is None:
        return base
    if match.group("base") != base:
        raise VersionError(
            f"git tag {tag} is {match.group('base')} but MantisBase is {base}"
        )
    return _pep440(base, match.group("channel"), match.group("n"))


def dynamic_metadata(settings, project):
    field = settings.get("field", "version")
    if field != "version":
        raise VersionError(f"version_provider only supports version, not {field}")
    return {"version": resolve_version()}


if __name__ == "__main__":
    try:
        version = resolve_version()
    except VersionError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
    print(version)
    github_env = os.environ.get("GITHUB_ENV", "").strip()
    if github_env:
        with open(github_env, "a", encoding="utf-8") as handle:
            handle.write(f"MB_PYTHON_VERSION={version}\n")
