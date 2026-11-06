"""Which of a class's ROM overrides of the script runner's slots has the host not bound?

py slotcheck.py <rtti class> <host class> [<base rtti class>]
    py slotcheck.py Vt_0031227c StageCharacter

The class's runner table (the vptr at +0xf4 of a stage object) is compared slot by slot with a base
table (default Vt_003147ac, StageObject's own). A slot that differs is an
override in the ROM. Its default's address names the host virtual, through the `FS2 0x...` annotation
in game/scriptrunner.h, and the host class's body says whether it overrides that virtual.

Printed: only the slots with something to look at. `NOT BOUND` is a virtual the host class leaves on
the default, so a script gets the default where the ROM runs the class's own code (four of these on
StageCharacter on 2026-11-06, natives 0x26, 0x47 and 0x68 among them). `no host virtual` is a slot
scriptrunner.h has no name for: read who calls it (a host method may stand in, or no script reaches
it: sssall.py). Inherited overrides are not followed (StageDoor reads as not bound on what
StageMapObject gives it), and a default annotated under another table's address reads as no host
virtual (scriptPlayMotion does): check before writing one.
"""
import re
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[2] / 'src' / 'game'


def table(cls):
    out = subprocess.run([sys.executable, 'rtti.py', '--class', cls], capture_output=True, text=True).stdout
    return {int(m[1]): (m[2], m[3]) for m in re.finditer(r'slot\s+(\d+) \+0x[0-9a-f]+ ([0-9a-f]{8}) (\S+)', out)}


def virtuals():
    lines = (SRC / 'scriptrunner.h').read_text(encoding='utf-8').split('\n')
    found = {}
    for i, line in enumerate(lines):
        for addr in re.findall(r'FS2 0x([0-9a-f]{8})', line):
            for near in lines[i:i + 4]:
                m = re.search(r'virtual\s+[^(]*?(\w+)\(', near)
                if m:
                    found.setdefault(addr, m[1])
                    break
    return found


def host_body(name):
    for path in SRC.glob('*.h'):
        text = path.read_text(encoding='utf-8')
        m = re.search(r'^class ' + name + r'\b[^;\n]*\n\{.*?^};', text, re.M | re.S)
        if m:
            return m[0]
    raise SystemExit(f'no class {name} in src/game/*.h')


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    rom, base = table(sys.argv[1]), table(sys.argv[3] if len(sys.argv) > 3 else 'Vt_003147ac')
    body = host_body(sys.argv[2])
    names = virtuals()
    missing = unnamed = 0
    for slot in sorted(rom):
        addr, name = rom[slot]
        default = base.get(slot)
        if default is None or addr == default[0] or slot >= 108:
            continue
        virtual = names.get(default[0])
        if virtual is None:
            unnamed += 1
            print(f'slot {slot:3} +0x{8 + 4 * slot:03x} {addr} {name}: no host virtual (default {default[0]})')
        elif not re.search(r'\b' + virtual + r'\([^\n]*override', body):
            missing += 1
            print(f'slot {slot:3} +0x{8 + 4 * slot:03x} {addr} {name}: NOT BOUND, {sys.argv[2]} leaves {virtual} on the default')
    print(f'{missing} not bound, {unnamed} with no host virtual')


if __name__ == '__main__':
    main()
