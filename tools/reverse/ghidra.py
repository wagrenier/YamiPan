"""Small Ghidra operations, for sessions without the MCP tools (autopilot).

python ghidra.py asm <lo> <hi>          disassemble a byte range, read-only (works where no function covers it)
python ghidra.py func <addr>            the function containing addr: name, entry, body range
python ghidra.py xrefs <addr>           references Ghidra knows to addr
python ghidra.py refs <addr>            every way code could reach addr: Ghidra xrefs, `jal`/`j` encodings,
                                        the address as a data word (vtable slot, table entry), and
                                        `addiu ..., lo16` halves of a lui/addiu pair
python ghidra.py find <hex> [mask]      byte-pattern search over the ELF's image (little-endian bytes, e.g.
                                        e29b070c; mask: hex of the same length, 00 bits ignored)
python ghidra.py segments               memory blocks
python ghidra.py mkfunc <addr> [name]   create a function at addr (a Ghidra write) and save;
                                        use when a body swallowed the next function (hazards.md)

Addresses are hex (0x optional). Reads go to GhidraMCP 7.0.0 (yamipan.http). `asm` is the offline
disassembler (mipsdis.py: no `_` on delay slots, `li` as addiu): GhidraMCP 7.0.0's disassemble_bytes has
no dry run and would write instructions into the program. `find` reads the ELF, not Ghidra.
"""
import json
import struct
import sys

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')


def addr(text):
    return int(text, 16)


def get(endpoint, **query):
    return yamipan.http('GET', endpoint, timeout=120, **query)


def find(pattern, mask=''):
    """Image addresses where the little-endian byte pattern occurs (mask bytes select the bits that count)."""
    from elf32 import Elf
    elf = Elf(str(yamipan.ELF))
    pat = bytes.fromhex(pattern)
    msk = bytes.fromhex(mask) if mask else b''
    out = []
    for vaddr, off, filesz, _ in elf.segments:
        img = elf.data[off:off + filesz]
        if not msk:
            i = img.find(pat)
            while i >= 0:
                out.append(vaddr + i)
                i = img.find(pat, i + 1)
            continue
        for i in range(len(img) - len(pat) + 1):
            if all((img[i + k] ^ pat[k]) & msk[k] == 0 for k in range(len(pat))):
                out.append(vaddr + i)
    return out


def le(word):
    return struct.pack('<I', word).hex()


def disasm(lo, hi):
    """Instructions of [lo, hi) as {'address', 'mnemonic', 'operands'} dicts, from mipsdis.py over the ELF
    (read-only; Ghidra's disassemble_bytes writes)."""
    import mipsdis
    from elf32 import Elf
    elf = Elf(str(yamipan.ELF))
    names = yamipan.cached_names()
    ins = []
    for pc in range(lo & ~3, hi, 4):
        w = elf.u32(pc)
        text = mipsdis.decode(pc, w, names, elf.gp)[0] if w is not None else '.word ?'
        mnem, _, ops = text.partition(' ')
        ins.append({'address': f'{pc:08x}', 'mnemonic': mnem, 'operands': ops.strip()})
    return ins


def cmd_asm(lo, hi):
    for ins in disasm(addr(lo), addr(hi)):
        print(f'{ins["address"]}: {ins["mnemonic"]} {ins["operands"]}'.rstrip())


def cmd_func(a):
    print(json.dumps(yamipan.function_at(addr(a)), indent=1))


def xrefs(a):
    return yamipan.paged('get_xrefs_to', 'references', limit=500, address=f'0x{a:x}')


def show_xrefs(a, indent=''):
    for x in xrefs(a):
        fn = x.get('from_function')
        print(f'{indent}{x["from_address"]} {x.get("type", "")}' + (f'  in {fn}' if fn else ''))


def cmd_xrefs(a):
    show_xrefs(addr(a))


def cmd_refs(a):
    a = addr(a)
    print('ghidra xrefs:')
    show_xrefs(a, '  ')
    target = (a >> 2) & 0x3ffffff
    for what, pat in (('jal', le(0x0c000000 | target)), ('j', le(0x08000000 | target)), ('data word', le(a))):
        hits = [h for h in find(pat) if h % 4 == 0]
        print(f'{what} ({pat}): ' + (', '.join(f'{h:08x}' for h in hits) or 'none'))
    lo16 = a & 0xffff
    hits = [h for h in find(struct.pack('<H', lo16).hex()) if h % 4 == 0]
    print(f'addiu/lw lo16 ({lo16:04x}): {len(hits)} candidates (check the lui before each)'
          + (': ' + ', '.join(f'{h:08x}' for h in hits[:20]) if hits else ''))


def cmd_find(pattern, mask=''):
    hits = find(pattern, mask)
    print(', '.join(f'{h:08x}' for h in hits) or 'none')


def cmd_segments():
    for s in get('list_program_items', kind='segments', limit=0).get('items', []):
        perms = ''.join(c if s.get(k) else '-' for c, k in (('r', 'readable'), ('w', 'writable'), ('x', 'executable')))
        print(f'{s["start"]}..{s["end"]}  {perms}  {s["name"]}')


def cmd_mkfunc(a, name=''):
    ok = yamipan.create_function(addr(a), name)
    print(f'{addr(a):08x}: {"created" if ok else "FAILED"}')
    print(yamipan.save_program()[:120])


COMMANDS = {'asm': cmd_asm, 'func': cmd_func, 'xrefs': cmd_xrefs, 'refs': cmd_refs, 'find': cmd_find,
            'segments': cmd_segments, 'mkfunc': cmd_mkfunc}

if __name__ == '__main__':
    if len(sys.argv) < 2 or sys.argv[1] not in COMMANDS:
        raise SystemExit(__doc__)
    if not yamipan.ghidra_up():
        raise SystemExit(f'no GhidraMCP window has {yamipan.PROGRAM} active (python ghidra_mcp.py which)')
    COMMANDS[sys.argv[1]](*sys.argv[2:])
