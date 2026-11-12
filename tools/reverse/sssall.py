#!/usr/bin/env python3
"""Every stage script on the disc, and which natives they call (offline).

python sssall.py                 scan ROM.000..ROMD.010: raw scripts and the SLDI packs' (a few minutes),
                                 write work/scripts/<first entry>_<n>.bin and work/sssall.json, print the table
python sssall.py --table         print the table from work/sssall.json, no scan
python sssall.py --unwritten     the same, only natives src/game/scriptrunner.cpp does not bind

A script is found by its SSS block (magic, first table 0x10, version 0.4) and walked back to the file's
header, which is the place where the entries' sizes (sssdis.entries) end exactly at that block. The table
says for each native how many scripts and how many calls use it; a native no script calls need not be
written. sssdis.py reads the files in work/scripts/. Calls behind a computed jump are counted too: the
walk is linear, like sssdis.py's.
"""
import json
import mmap
import re
import struct
import sys
from pathlib import Path

import sssdis
import yamipan

BLOCK = b'SSS\x1a\x10\0\0\0\0\0\x04\0'
OUT = Path(__file__).parent / 'work' / 'scripts'
JSON = Path(__file__).parent / 'work' / 'sssall.json'
SRC = Path(__file__).parent.parent.parent / 'src' / 'game' / 'scriptrunner.cpp'
FIRST, LAST = 0x19, 0x235
MAX_SCRIPT = 0x200000


def sldi(d, pos, end):
    """game/sldi.cpp over d[pos:]: the unpacked bytes, None when it stops being a pack."""
    size = struct.unpack_from('<i', d, pos + 12)[0]
    if size <= 0 or size > 0x2000000:
        return None
    out = bytearray(size)
    s = pos + 16
    o = 0
    while o < size:
        if s >= end:
            return None
        tag = d[s]
        kind = tag & 0xc0
        if kind == 0x40:
            n = tag & 0x3f
            s += 1
        elif kind == 0:
            if tag & 0x20:
                n = d[s + 1] | d[s + 2] << 8
                s += 3
            else:
                n = (tag & 0x1f) << 8 | d[s + 1]
                s += 2
        elif kind == 0x80:
            if tag & 0x20:
                dist = d[s + 1] | d[s + 2] << 8
                n = d[s + 3]
                s += 4
            else:
                dist = (tag & 0x1f) << 8 | d[s + 1]
                n = d[s + 2]
                s += 3
            if dist == 0 or dist > o or o + n > size:
                return None
            if dist >= n:
                out[o:o + n] = out[o - dist:o - dist + n]
            else:
                for i in range(n):
                    out[o + i] = out[o - dist + i]
            o += n
            continue
        else:
            return None
        if o + n > size:
            return None
        out[o:o + n] = d[s:s + n]
        s += n
        o += n
    return out


def start_of(d, block, low):
    """The script file's first byte, the block at offset block; None when no header fits."""
    p = block - 0x10
    p -= p % 4
    while p >= low:
        count, table2 = struct.unpack_from('<hh', d, p + 8)
        if 0 < count < 2000 and 0 <= table2 < 2000:
            pos = 0xc + count * 0x1c + table2 * 0x10
            good = p + pos <= block
            for i in range(count):
                if not good:
                    break
                size, slots = struct.unpack_from('<Hh', d, p + 0xc + i * 0x1c + 0x18)
                size = (size & 0xfffc) | ((size & 3) << 16)
                if slots < 0:
                    good = False
                pos += size + slots * 0x10
                good = good and p + pos <= block
            if good and p + pos == block:
                return p
        p -= 4
    return None


def block_end(d, block, end):
    """Past the block's tables: magic, size (low two bits free), next at size."""
    p = block + 0x10
    tables = struct.unpack_from('<H', d, block + 0xc)[0]
    for _ in range(tables):
        if p + 8 > end:
            return p
        p += struct.unpack_from('<I', d, p + 4)[0] & ~3
    return min(p, end)


def scripts_in(d, end, base=0):
    """(start, bytes) of each script in d[base:end]."""
    found = []
    low = base
    at = d.find(BLOCK, base, end)
    while at >= 0:
        start = start_of(d, at, max(low, at - MAX_SCRIPT))
        if start is not None:
            stop = block_end(d, at, end)
            found.append(bytes(d[start:stop]))
            low = stop
        at = d.find(BLOCK, at + 4, end)
    return found


def natives_of(d):
    """{native: calls}."""
    calls = {}
    for name, pos, size in sssdis.entries(d):
        for n, _ in sssdis.walk(d, name, pos, size, FIRST, LAST):
            calls[n] = calls.get(n, 0) + 1
    return calls


def scan():
    OUT.mkdir(parents=True, exist_ok=True)
    seen = {}
    files = sorted(p for p in Path(yamipan.GAME_DIR).iterdir() if re.match(r'ROMD?\.\d+$', p.name))
    packs = 0
    for path in files:
        with open(path, 'rb') as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as d:
            size = len(d)
            for script in scripts_in(d, size):
                seen.setdefault(script, path.name)
            at = d.find(b'SLDI\x01\0\0\0')
            while at >= 0:
                data = sldi(d, at, size)
                if data is not None:
                    packs += 1
                    for script in scripts_in(data, len(data)):
                        seen.setdefault(script, f'{path.name}@{at:#x}')
                at = d.find(b'SLDI\x01\0\0\0', at + 4)
        print(f'{path.name}: {packs} packs, {len(seen)} scripts so far', flush=True)

    rows = []
    skipped = 0
    for script, where in seen.items():
        name = script[0xc:0x1c].split(b'\0')[0].decode('latin1')
        if not re.fullmatch(r'[\w@.-]+', name):     # sizes that happened to fit
            skipped += 1
            continue
        n = sum(1 for r in rows if r['first'] == name)
        file = f'{name or "noname"}_{n}.bin'
        (OUT / file).write_bytes(script)
        rows.append({'file': file, 'first': name, 'where': where, 'size': len(script),
                     'calls': {f'{k:#x}': v for k, v in sorted(natives_of(script).items())}})
    JSON.write_text(json.dumps(rows, indent=0))
    print(f'{len(rows)} scripts in {packs} packs ({skipped} false hits left out) -> {OUT}, {JSON}')


def table(unwritten):
    rows = json.loads(JSON.read_text())
    used = {}
    for r in rows:
        for k, v in r['calls'].items():
            e = used.setdefault(int(k, 16), [0, 0])
            e[0] += 1
            e[1] += v
    have = set(int(x, 16) for x in re.findall(r'\{\s*(0x[0-9a-f]+),\s*native\w+\s*\}', SRC.read_text(encoding='utf8')))
    never = [n for n in range(FIRST, LAST + 1) if n not in used and n not in have]
    print(f'{len(rows)} scripts; {len(used)} natives called, {len(have)} written; '
          f'{len(never)} unwritten natives no script calls')
    order = sorted(used.items(), key=lambda kv: (-kv[1][0], -kv[1][1]))
    for n, (scripts, calls) in order:
        if unwritten and n in have:
            continue
        print(f'{n:#x}: {scripts} scripts, {calls} calls{"" if n not in have else "  written"}')
    if unwritten:
        print('never called:', ' '.join(hex(n) for n in never))


def main():
    if '--table' in sys.argv or '--unwritten' in sys.argv:
        table('--unwritten' in sys.argv)
    else:
        scan()
        table(True)


if __name__ == '__main__':
    main()
