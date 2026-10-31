"""Dump a table from the ELF's .data / .rodata as host C++ initializers (offline).

python datadump.py <addr> <count> <fields> [--type NAME] [--var NAME] [--stride N]
python datadump.py 2e3a40 12 "i:id,f:speed,P:func,h:flags,h:pad"     # 12 rows {int, float, ptr, short, short}

<fields> is a comma list of code:name, codes (little endian, EE sizes):
  b/B s8/u8   h/H s16/u16   i/I s32/u32   q/Q s64/u64   f float   P pointer (printed as the symbol it
  points at: a function name, a vtable / data address, or nullptr)   S pointer to a C string (printed as
  the literal)   xN N raw bytes (an array, printed as {..})
--stride overrides the row size (padding between rows). The output is a C++ initializer list, one row per
line with its EE address, for a host table in src/ (the pattern ReiPan used for FF3's nested enemy,
character and ghost tables: rebuild pointer arrays with host pointers, copy the rest as values). Pointers
into .data that are themselves tables need their own dump; the comment shows where they point.
"""
import json
import struct
import sys

import yamipan
from elf32 import Elf

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

SIZES = {'b': 1, 'B': 1, 'h': 2, 'H': 2, 'i': 4, 'I': 4, 'q': 8, 'Q': 8, 'f': 4, 'P': 4, 'S': 4}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2

    def opt(name, default=None):
        if name in argv:
            i = argv.index(name)
            v = argv[i + 1]
            del argv[i:i + 2]
            return v
        return default
    tname, vname, stride = opt('--type', 'ROW'), opt('--var', 'table'), opt('--stride')
    base, count = int(argv[0], 16), int(argv[1], 0)
    fields = []
    for f in argv[2].split(','):
        code, _, name = f.partition(':')
        size = int(code[1:]) if code[0] == 'x' else SIZES[code]
        fields.append((code, name or f'f{len(fields)}', size))
    row = int(stride, 0) if stride else sum(s for _, _, s in fields)
    elf = Elf(str(yamipan.ELF))
    names = {int(a, 16): n for a, n in yamipan.cached_names().items()}
    vts = {s['addr']: s['name'][len('.gnu.linkonce.d.'):] for s in elf.sections
           if s['name'].startswith('.gnu.linkonce.d.')}
    print(f'// {count} x {row:#x} bytes at 0x{base:08x} (datadump.py)')
    print(f'static {tname} {vname}[{count}] = {{')
    for r in range(count):
        a = base + r * row
        raw = elf.read(a, row)
        if raw is None or len(raw) < row:
            raise SystemExit(f'0x{a:08x} is outside the loaded image')
        vals, notes, off = [], [], 0
        for code, name, size in fields:
            chunk = raw[off:off + size]
            off += size
            if code[0] == 'x':
                vals.append('{' + ', '.join(f'0x{b:02x}' for b in chunk) + '}')
            elif code == 'f':
                vals.append(f'{struct.unpack("<f", chunk)[0]!r}f')
            elif code in ('P', 'S'):
                p = struct.unpack('<I', chunk)[0]
                if p == 0:
                    vals.append('nullptr')
                elif code == 'S':
                    s = elf.cstring(p, 512)
                    vals.append(json.dumps(s.decode('latin-1')) if s is not None else f'/* 0x{p:08x} */ nullptr')
                elif p in names:
                    vals.append(names[p].split('(')[0])
                elif p in vts:
                    vals.append(f'/* {vts[p]} */ nullptr')
                else:
                    vals.append('nullptr')
                    notes.append(f'{name} -> 0x{p:08x} ({elf.section_of(p) or "?"})')
            else:
                v = struct.unpack('<' + code, chunk)[0]
                vals.append(f'{v:#x}' if code.isupper() and v > 9 else str(v))
        print(f'    {{ {", ".join(vals)} }},  // 0x{a:08x}' + (f'  {"; ".join(notes)}' if notes else ''))
    print('};')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
