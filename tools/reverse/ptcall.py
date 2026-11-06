"""Which particle definitions the disc holds (offline).

python ptcall.py [flag bit, hex]

Unpacks the SLDI packs as sssall.py does and looks for particle files in them: +4 version 0xf, +8 a
count, then 0x158-byte records with printable names (ptcdump.py lists one file). Says how many files and
definitions it found, how many definitions use each flag and each ALPHA_1, and with a flag bit which
ones have it. A heuristic scan: a file that is not in a pack is not seen.
"""
import collections
import mmap
import re
import struct
import sys
from pathlib import Path

import ptcdump
import sssall
import yamipan

VERSION = 0xf
HEADER = 0x10
MAX_COUNT = 400


def name_ok(data, off):
    raw = data[off:off + 0x10]
    text = raw.split(b'\0')[0]
    return 1 < len(text) < 0x10 and all(0x20 < c < 0x7f for c in text)


def files_in(data):
    """(offset, count) of what reads as a particle file."""
    at = data.find(struct.pack('<I', VERSION), 4)
    while at >= 0:
        base = at - 4
        if base % 0x10 == 0 and base + HEADER + ptcdump.RECORD <= len(data):
            count = struct.unpack_from('<I', data, base + 8)[0]
            end = base + HEADER + count * ptcdump.RECORD
            if 0 < count <= MAX_COUNT and end <= len(data) and \
                    all(name_ok(data, base + HEADER + i * ptcdump.RECORD) for i in range(count)):
                yield base, count
                at = data.find(struct.pack('<I', VERSION), end + 4)
                continue
        at = data.find(struct.pack('<I', VERSION), at + 4)


def main():
    wanted = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0
    seen = set()
    flags = collections.Counter()
    blends = collections.Counter()
    hits = []
    total = 0
    paths = sorted(p for p in Path(yamipan.GAME_DIR).iterdir() if re.match(r'ROMD?\.\d+$', p.name))
    for path in paths:
        with open(path, 'rb') as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as d:
            at = d.find(b'SLDI\x01\0\0\0')
            while at >= 0:
                data = sssall.sldi(d, at, len(d))
                if data is not None:
                    data = bytes(data)
                    for base, count in files_in(data):
                        body = data[base:base + HEADER + count * ptcdump.RECORD]
                        if body in seen:
                            continue
                        seen.add(body)
                        for i in range(count):
                            off = base + HEADER + i * ptcdump.RECORD
                            bits = struct.unpack_from('<I', data, off + 0x150)[0]
                            alpha = struct.unpack_from('<Q', data, off + 0x70)[0]
                            total += 1
                            blends[alpha] += 1
                            for bit in ptcdump.FLAGS:
                                if bits & bit:
                                    flags[bit] += 1
                            if bits & wanted:
                                hits.append(f'{path.name}@{at:#x} {ptcdump.text(data, off)}')
                at = d.find(b'SLDI\x01\0\0\0', at + 4)
        print(f'{path.name}: {len(seen)} files, {total} definitions so far', flush=True)
    for bit, name in ptcdump.FLAGS.items():
        print(f'{bit:#8x} {name}: {flags[bit]}')
    print('ALPHA_1:', ' '.join(f'{k:#x} x{v}' for k, v in blends.most_common()))
    for hit in hits:
        print(hit)


if __name__ == '__main__':
    main()
