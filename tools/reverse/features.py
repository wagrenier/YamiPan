"""Extract per-function features from the FS2 ELF.

python features.py                       # work/hash_fs2.json -> work/feat_fs2.json
python features.py <elf> <hashes.json> <out.json>

Function starts/names come from Ghidra (fetch.py); bodies are [start, next start) clipped to the
containing executable section. Everything else is decoded here: call order, function pointers,
string/float/data references, coarse instruction mix. Offline.
"""
import json
import struct
import sys

import yamipan
from elf32 import Elf

DATA_SECTIONS = {'.data', '.rodata', '.sdata', '.lit4', '.lit8', '.sbss', '.bss', '.ctors', '.dtors',
                 '.gcc_except_table', '.eh_frame'}
# Coarse classes are what survive best; fine tokens add detail.
LOADS = {32, 33, 34, 35, 36, 37, 38, 39, 26, 27, 30, 55, 49, 54}
STORES = {40, 41, 42, 43, 44, 45, 46, 31, 63, 57, 62}
BRANCHES = {4, 5, 6, 7, 20, 21, 22, 23}
ALU_I = {8, 9, 10, 11, 12, 13, 14, 24, 25}
COARSE_CH = {'ld': 'L', 'st': 'S', 'alu': 'a', 'shift': 'h', 'muldiv': 'm', 'br': 'b', 'j': 'j',
             'jr': 'r', 'call': 'c', 'callr': 'C', 'lui': 'u', 'fpu': 'f', 'fmov': 'F', 'vu': 'v',
             'mmi': 'x', 'sys': 'y', 'other': 'o'}


def decode(word):
    """(coarse, fine, dest_reg) for one instruction."""
    op = word >> 26
    rs, rt, rd = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
    funct = word & 63
    if op == 0:
        if funct in (8, 9):
            return ('jr' if funct == 8 else 'callr'), f'sp{funct}', (rd if funct == 9 else None)
        if funct in (12, 13, 15):
            return 'sys', f'sp{funct}', None
        if funct in (24, 25, 26, 27, 28, 29, 30, 31):
            return 'muldiv', f'sp{funct}', None
        if funct < 8 or funct in (20, 22, 23, 56, 58, 59, 60, 62, 63):
            return 'shift', f'sp{funct}', rd
        return 'alu', f'sp{funct}', rd
    if op == 1:
        return 'br', f'ri{rt}', None
    if op == 2:
        return 'j', 'j', None
    if op == 3:
        return 'call', 'jal', 31
    if op in BRANCHES:
        return 'br', f'op{op}', None
    if op == 15:
        return 'lui', 'lui', rt
    if op in ALU_I:
        return 'alu', f'op{op}', rt
    if op == 17:
        if rs == 8:
            return 'br', 'bc1', None
        if rs in (0, 2, 4, 6):
            return 'fmov', f'c1m{rs}', rt if rs in (0, 2) else None
        return 'fpu', f'c1{rs}_{funct}', None
    if op == 18:
        return 'vu', 'cop2', None
    if op == 16:
        return 'sys', 'cop0', None
    if op == 28:
        return 'mmi', f'mmi{funct}_{(word >> 6) & 31 if funct in (8, 9, 40, 41) else 0}', rd
    if op in LOADS:
        return 'ld', f'op{op}', (rt if op not in (49, 54) else None)
    if op in STORES:
        return 'st', f'op{op}', None
    return 'other', f'op{op}', None


def sext16(v):
    return v - 0x10000 if v & 0x8000 else v


def extract(elf_path, hashes_path, out_path):
    elf = Elf(str(elf_path))
    GP = elf.gp
    with open(hashes_path, encoding='utf-8') as f:
        ghidra_funcs = json.load(f)['functions']
    exec_sections = [s for s in elf.sections if s['flags'] & 4 and s['addr']]

    def exec_end(va):
        for s in exec_sections:
            if s['addr'] <= va < s['addr'] + s['size']:
                return s['addr'] + s['size']
        return None

    funcs = sorted(({'addr': int(g['address'], 16), 'name': g['name'], 'hash': g['hash'],
                     'ghidra_insns': g['instruction_count']} for g in ghidra_funcs
                    if ':' not in g['address']),                        # overlay (VU) functions skipped
                   key=lambda f: f['addr'])
    funcs = [f for f in funcs if exec_end(f['addr'])]
    # The ELF's named linkonce sections are functions even before Ghidra has them (sections.py --apply).
    known = {f['addr']: f for f in funcs}
    import sections
    for s in sections.load(elf)['functions']:
        if s['addr'] not in known:
            funcs.append({'addr': s['addr'], 'name': s['name'], 'hash': '', 'ghidra_insns': 0})
        elif yamipan.GENERIC_NAME.match(known[s['addr']]['name']):
            known[s['addr']]['name'] = s['name']        # the ELF's symbol over Ghidra's FUN_ until seeded
    funcs.sort(key=lambda f: f['addr'])
    starts = [f['addr'] for f in funcs]
    start_set = set(starts)

    def printable_string(va):
        raw = elf.cstring(va, 256)
        if raw is None or len(raw) < 3:
            return None
        for enc in ('ascii', 'shift_jis'):
            try:
                s = raw.decode(enc)
                break
            except UnicodeDecodeError:
                continue
        else:
            return None
        if sum(ch.isprintable() or ch in '\n\t' for ch in s) < len(s):
            return None
        return s

    out = []
    for i, fn in enumerate(funcs):
        start = fn['addr']
        end = min(starts[i + 1] if i + 1 < len(starts) else exec_end(start), exec_end(start))
        code = elf.read(start, end - start) or b''
        words = struct.unpack('<%dI' % (len(code) // 4), code[:len(code) // 4 * 4])
        n = len(words)
        while n > 1 and words[n - 1] == 0:       # trailing padding nops after the final jr ra / j
            n -= 1
        # Registers holding a known constant: lui, lui+addiu/ori, gp+addiu.
        val = {28: GP} if GP else {}
        calls, fptrs, strings, data, floats, ints, tails = [], [], [], [], [], [], []
        coarse, fine = [], []

        def ref(a, forming, op):
            if a in start_set:
                fptrs.append(a)
                return
            sec = elf.section_of(a)
            if sec not in DATA_SECTIONS and not (sec or '').startswith('.gnu.linkonce.d'):
                return
            data.append(a)
            if forming:
                s = printable_string(a)
                if s is not None:
                    strings.append(s)
            elif op == 49 and sec in ('.lit4', '.sdata', '.rodata', '.data'):   # lwc1 from a literal pool (FS2: '.data', no section names)
                v = elf.u32(a)
                if v is not None:
                    floats.append(v)

        for k in range(n):
            w = words[k]
            pc = start + 4 * k
            c, t, dest = decode(w)
            coarse.append(c)
            fine.append(t)
            op = w >> 26
            rs, rt = (w >> 21) & 31, (w >> 16) & 31
            imm = w & 0xffff
            if op == 3:
                calls.append((pc & 0xf0000000) | ((w & 0x3ffffff) << 2))
            if op == 2:                                   # j: a tail call when it leaves the body
                t2 = (pc & 0xf0000000) | ((w & 0x3ffffff) << 2)
                if not start <= t2 < end:
                    tails.append(t2)
            if op == 15:                                  # lui
                val[rt] = imm << 16
                continue
            if op in (9, 25) and rs in val:               # addiu/daddiu from a known value
                v = (val[rs] + sext16(imm)) & 0xffffffff
                if rs != 0:
                    ref(v, True, op)
                if rt not in (0, 28):
                    val[rt] = v
                continue
            if op == 13 and rs in val and rs != 0:        # ori after lui
                v = val[rs] | imm
                if elf.section_of(v) is None and v not in start_set:
                    ints.append(v)
                else:
                    ref(v, True, op)
                if rt not in (0, 28):
                    val[rt] = v
                continue
            if (op in LOADS or op in STORES) and rs in val and rs != 0:
                ref((val[rs] + sext16(imm)) & 0xffffffff, False, op)
            elif op == 17 and rs == 4 and rt in val and rt != 0:   # mtc1 of a known value
                floats.append(val[rt])
            if dest is not None and dest not in (0, 28):
                val.pop(dest, None)
        out.append({
            'addr': start, 'name': fn['name'], 'hash': fn['hash'], 'size': n,
            'calls': calls, 'tails': tails, 'fptrs': fptrs, 'strings': strings, 'data': data,
            'floats': floats, 'ints': ints, 'coarse': ''.join(COARSE_CH[x] for x in coarse), 'fine': fine,
        })

    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump({'gp': GP, 'functions': out}, f)
    print(f'{elf_path.name if hasattr(elf_path, "name") else elf_path}: {len(out)} functions, '
          f'{sum(len(o["calls"]) for o in out)} calls, {sum(len(o["tails"]) for o in out)} tail jumps, '
          f'{sum(len(o["strings"]) for o in out)} string refs, {sum(len(o["floats"]) for o in out)} float consts')


if __name__ == '__main__':
    if len(sys.argv) == 4:
        extract(*sys.argv[1:4])
    else:
        extract(yamipan.ELF, yamipan.WORK / 'hash_fs2.json', yamipan.WORK / 'feat_fs2.json')
