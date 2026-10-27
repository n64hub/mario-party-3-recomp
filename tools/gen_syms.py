"""Writes syms/marioparty3.us.syms.toml and syms/marioparty3.us.datasyms.toml from the decomp's US ELF with N64Recomp's
own context dump (--dump-context), the way the MarioKart64RecompSyms and Starfox64RecompSyms repos were made.

The ELF is tools/build_decomp.sh's marioparty3.elf, as the decomp links it. KMC gcc puts every function, static ones
too, in the symbol table, and splat sizes every function of the assembly, so the ELF alone gives N64Recomp each
function with its size. Most are named by their address (splat's func_8001234), the rest as the decomp named them.

N64Recomp reads each section's VRAM from the ELF's section headers and its ROM offset from the load address
marioparty3.ld gives it, which is the ROM's: the decomp builds the ROM uncompressed, byte for byte. Each code section's
bytes are checked against the ROM's there.

Usage: py -3 -I tools/gen_syms.py <N64Recomp> <elf> <rom>
"""
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# The decomp's entrypoint (asm/entrypoint.s), the ROM header's: the boot code copies 1 MB from ROM 0x1000 there.
ENTRYPOINT = 0x80000400
# Sizes for symbols the decomp gives none, so that each is a function us.toml can stub: libleo's scrambled boot of a
# 64DD disk game (src/72750.c), which func_80071B50_main decodes and calls. As a symbol of no size it is left out of the
# dump, found as the target of that call and analyzed as a static, and its bytes aren't code until they are decoded.
FUNCTION_SIZES = 'function_sizes = [ { name = "func_80071C8C_main", size = 0x344 } ]'


def elf_sections(path):
    """Each section's bytes in the ELF, by name."""
    data = Path(path).read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 2:
        sys.exit(f"{path}: not a 32-bit big-endian ELF")
    e_shoff, = struct.unpack_from(">I", data, 0x20)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from(">HHH", data, 0x2E)
    headers = [struct.unpack_from(">IIIIIIIIII", data, e_shoff + i * e_shentsize) for i in range(e_shnum)]
    names = headers[e_shstrndx][4]
    return {data[names + h[0]:data.index(b"\0", names + h[0])].decode(): data[h[4]:h[4] + h[5]] for h in headers}


def check_rom_offsets(dump, sections, rom):
    """Each [[section]]'s bytes are the ROM's at its rom."""
    for name, offset in re.findall(r'name = "([^"]+)"\nrom = (0x[0-9A-F]+)', dump):
        code = sections[name]
        if rom[int(offset, 16):int(offset, 16) + len(code)] != code:
            sys.exit(f"{name}: the ROM's bytes at {offset} aren't the ELF's")


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__.strip().splitlines()[-1])
    n64recomp, elf, rom = (Path(a).resolve() for a in sys.argv[1:])
    with tempfile.TemporaryDirectory() as tmp:
        config = Path(tmp) / "context.toml"
        config.write_text(
            "[input]\n"
            f"entrypoint = 0x{ENTRYPOINT:08X}\n"
            f'elf_path = "{elf.as_posix()}"\n'
            'output_func_path = "out"\n'
            # The decomp's bss sections are named <segment>_bss (splat's marioparty3.ld).
            'bss_section_suffix = "_bss"\n'
            f"{FUNCTION_SIZES}\n")
        run = subprocess.run([str(n64recomp), config.name, "--dump-context"], cwd=tmp, capture_output=True, text=True)
        if run.returncode != 0:
            sys.exit((run.stdout + run.stderr)[-3000:])
        dump = (Path(tmp) / "dump.toml").read_text()
        check_rom_offsets(dump, elf_sections(elf), rom.read_bytes())
        out = ROOT / "syms"
        out.mkdir(exist_ok=True)
        (out / "marioparty3.us.syms.toml").write_text(dump, newline="\n")
        (out / "marioparty3.us.datasyms.toml").write_text((Path(tmp) / "data_dump.toml").read_text(), newline="\n")
    print(f"{dump.count('[[section]]')} sections, {dump.count('{ name = ')} functions; "
          "wrote syms/marioparty3.us.syms.toml and syms/marioparty3.us.datasyms.toml")


if __name__ == "__main__":
    main()
