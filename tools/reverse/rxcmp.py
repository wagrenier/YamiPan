"""Compare a library FS2 shares with Rule of Rose word for word, function by function, by name.

python rxcmp.py <name-regex> [--lo ADDR --hi ADDR] [--show NAME]

Pairs the functions of yamipan_functions.txt and ../RosePan/ror_functions.txt whose names match the
regex (FS2's limited to lo..hi when given) and compares their words with relocated fields masked:
j / jal targets, lui immediates and the immediates of instructions whose base register came from a lui
or is gp. Prints one line per function: '=' identical, 'size' different sizes, '!' same size but
different words (the first differing offsets), and the names only one side has. --show NAME prints the
two disassemblies side by side (mipsdis.py) for a function that differs. Offline (both ELFs on disk).
For the carried-over rx TUs (hazards.md "A shared-library function longer than its sister's").
"""
import argparse
import re
import struct
import sys
from pathlib import Path

import yamipan
from elf32 import Elf

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROR_ELF = Path(r'C:\Users\willi\Documents\Decomp\Rule of Rose\SLUS_214.48')
ROR_LIST = yamipan.REPO.parent / 'RosePan' / 'ror_functions.txt'
FS2_LIST = yamipan.FUNCTIONS_TXT
LINE = re.compile(r'^/\* ([0-9a-f]{8}) ([0-9a-f]{8}) \*/ (\S+?)\(\)\{\}')
IMM_OPS = {8, 9, 10, 11, 12, 13, 14, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 46, 49, 53, 55,
           57, 61, 63, 30, 31}


def read_list(path):
    out = {}
    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        m = LINE.match(line)
        if m:
            out.setdefault(m.group(3), (int(m.group(1), 16), int(m.group(2), 16)))
    return out


def words(elf, addr, size):
    off = elf.offset(addr)
    return list(struct.unpack_from('<%dI' % (size // 4), elf.data, off))


def masked(ws):
    out, lui = [], set()
    for w in ws:
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        if op in (2, 3):
            w &= ~0x3ffffff
        elif op == 15:
            lui.add(rt)
            w &= ~0xffff
        elif op in IMM_OPS and (rs == 28 or rs in lui):
            w &= ~0xffff
        out.append(w)
    return out


def data_refs(ws, gp):
    """[(word index, address)] of the lui / gp-relative addresses the code forms (straight-line guess)."""
    out, base = [], {}
    for i, w in enumerate(ws):
        op, rs, rt, rd = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
        imm = w & 0xffff
        lo = imm - 0x10000 if imm & 0x8000 else imm
        if op == 15:
            base[rt] = imm << 16
        elif op in IMM_OPS and (rs == 28 or rs in base):
            addr = ((gp if rs == 28 else base[rs]) + lo) & 0xffffffff
            out.append((i, addr))
            if op in (9, 13):
                base[rt] = addr if op == 9 else (base.get(rs, 0) | imm)
            elif op < 40:
                base.pop(rt, None)
        elif op == 0:
            base.pop(rd, None)
        elif op in (8, 10, 11, 12, 14) or 32 <= op < 40:
            base.pop(rt, None)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('regex')
    ap.add_argument('--lo', type=lambda s: int(s, 16), default=0)
    ap.add_argument('--hi', type=lambda s: int(s, 16), default=1 << 32)
    ap.add_argument('--show')
    ap.add_argument('--data', action='store_true',
                    help='pair the data addresses identical functions form: RoR -> FS2, with the users')
    a = ap.parse_args()
    rx = re.compile(a.regex)
    fs2 = {n: v for n, v in read_list(FS2_LIST).items() if rx.search(n) and a.lo <= v[0] < a.hi}
    ror = {n: v for n, v in read_list(ROR_LIST).items() if rx.search(n)}
    ef, er = Elf(str(yamipan.ELF)), Elf(str(ROR_ELF))
    if a.show:
        import mipsdis
        (fa, fs), (ra, rs) = fs2[a.show], ror[a.show]
        wf, wr = words(ef, fa, fs), words(er, ra, rs)
        for i in range(max(len(wf), len(wr))):
            l = ' '.join(mipsdis.decode(fa + 4 * i, wf[i], {}, ef.gp)) if i < len(wf) else ''
            r = ' '.join(mipsdis.decode(ra + 4 * i, wr[i], {}, er.gp)) if i < len(wr) else ''
            mark = ' ' if i < len(wf) and i < len(wr) and masked([wf[i]]) == masked([wr[i]]) else '*'
            print(f'{mark} {4 * i:04x}  {l:<44} {r}')
        return
    if a.data:
        pairs = {}
        for n, (fa, fs) in fs2.items():
            if n in ror and ror[n][1] == fs:
                wf, wr = words(ef, fa, fs), words(er, ror[n][0], fs)
                if masked(wf) != masked(wr):
                    continue
                for (i, x), (j, y) in zip(data_refs(wr, er.gp), data_refs(wf, ef.gp)):
                    pairs.setdefault((x, y), set()).add(n)
        for (x, y), users in sorted(pairs.items()):
            print(f'  ror {x:08x} -> fs2 {y:08x}  ({(y - x) & 0xffffffff:08x})  {", ".join(sorted(users)[:3])}')
        return
    same = 0
    for n, (fa, fs) in sorted(fs2.items(), key=lambda kv: kv[1][0]):
        if n not in ror:
            print(f'  only FS2  {fa:08x} {fs:5x} {n}')
            continue
        ra, rs = ror[n]
        if fs != rs:
            print(f'  size      {fa:08x} {fs:5x} / ror {ra:08x} {rs:5x} {n}')
            continue
        mf, mr = masked(words(ef, fa, fs)), masked(words(er, ra, rs))
        diff = [4 * i for i, (x, y) in enumerate(zip(mf, mr)) if x != y]
        if diff:
            print(f'  !         {fa:08x} {fs:5x} / ror {ra:08x} {n}: +' + ' +'.join(f'{d:x}' for d in diff[:8]))
        else:
            same += 1
    for n in sorted(set(ror) - set(fs2)):
        print(f'  only RoR  {ror[n][0]:08x} {ror[n][1]:5x} {n}')
    print(f'{same} of {len(fs2)} identical (masked)')


if __name__ == '__main__':
    main()
