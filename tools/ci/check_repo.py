"""Repo checks CI runs without the ROM.

1. No ROM or ROM-derived data is tracked: ROM extensions, the generated RecompiledFuncs/ and rsp/
   output, roms/ and extracted/, any file starting with an N64 ROM header in any byte order, and
   any file big enough to be a RAM or ROM dump.
2. Every tracked .toml parses.
3. Every tracked Python file compiles (most tools read the ROM at import, so they can't be run).

    py -3 -I tools/ci/check_repo.py
"""
import subprocess
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

ROM_EXTENSIONS = {".z64", ".n64", ".v64", ".rom"}
# Generated from the ROM by N64Recomp / RSPRecomp, or the ROM itself (see .gitignore).
ROM_DERIVED_DIRS = ("roms/", "extracted/", "RecompiledFuncs/", "rsp/")
# The first word of an N64 ROM in .z64, .v64 and .n64 byte order.
ROM_MAGICS = (b"\x80\x37\x12\x40", b"\x37\x80\x40\x12", b"\x40\x12\x37\x80")
# RDRAM dumps are 4 or 8 MB and ROMs larger. The launcher's fonts (assets/, RecompFrontend's Chiaro at up to 6 MB) are
# the only tracked files near that size, so they are left out of the size check by their extension.
MAX_FILE_SIZE = 4 * 1024 * 1024
FONT_EXTENSIONS = {".otf", ".ttf"}


def tracked_files() -> list[str]:
    out = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, check=True, capture_output=True).stdout
    return [p for p in out.decode().split("\0") if p]


def main() -> int:
    errors = []
    for rel in tracked_files():
        path = ROOT / rel
        if not path.is_file():  # submodules
            continue
        if path.suffix.lower() in ROM_EXTENSIONS:
            errors.append(f"{rel}: ROM file extension")
        if rel.startswith(ROM_DERIVED_DIRS):
            errors.append(f"{rel}: generated from the ROM")
        with path.open("rb") as f:
            if f.read(4) in ROM_MAGICS:
                errors.append(f"{rel}: starts with an N64 ROM header")
        if path.stat().st_size > MAX_FILE_SIZE and path.suffix.lower() not in FONT_EXTENSIONS:
            errors.append(f"{rel}: {path.stat().st_size} bytes, larger than any source file should be (a dump?)")
        if path.suffix == ".toml":
            try:
                tomllib.loads(path.read_text(encoding="utf-8"))
            except (tomllib.TOMLDecodeError, UnicodeDecodeError) as e:
                errors.append(f"{rel}: invalid TOML: {e}")
        if path.suffix == ".py":
            try:
                compile(path.read_bytes(), rel, "exec")
            except SyntaxError as e:
                errors.append(f"{rel}:{e.lineno}: {e.msg}")
    for e in errors:
        print(e, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
