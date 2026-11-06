"""List a particle file's definitions (offline).

python ptcdump.py <file> [name]

A file is a stage's "particle" entry as it stands in memory: a 0x10-byte header (+4 version 0xf, +8 count),
then 0x158-byte records (game/stageparticle.h, ParticleDef). With a name, every field of that record;
without, one line each and a count of the flags and blends in use.
"""
import collections
import struct
import sys

RECORD = 0x158
FLAGS = {
    0x1: 'Billboard', 0x2: 'ViewLocal', 0x4: 'Stretch', 0x10: 'ViewAround', 0x20: 'OnFloor', 0x40: 'Refract',
    0x80: 'Collides', 0x100: 'RandomFrame', 0x200: 'NoFog', 0x400: 'Local', 0x800: 'ScaleByLens',
    0x1000: 'ChainAttached', 0x2000: 'LitByFacing', 0x4000: 'NoRoomInside', 0x8000: 'NoEmitInside',
    0x10000: 'NoFarLimit', 0x20000: 'AlphaByFacing', 0x40000: 'TakesTurn', 0x80000: 'FarWorld',
    0x100000: 'PositionOnly',
}


def text(data, off):
    return data[off:off + 0x10].split(b'\0')[0].decode('latin-1')


def flag_names(flags):
    return '|'.join(n for b, n in FLAGS.items() if flags & b) or '0'


def main(argv):
    data = open(argv[0], 'rb').read()
    version, count = struct.unpack_from('<2I', data, 4)
    print(f'version {version:#x}, {count} definitions')
    seen = collections.Counter()
    blends = collections.Counter()
    for index in range(count):
        base = 0x10 + index * RECORD
        rec = data[base:base + RECORD]
        name = text(rec, 0)
        flags = struct.unpack_from('<I', rec, 0x150)[0]
        alpha = struct.unpack_from('<Q', rec, 0x70)[0]
        texture = struct.unpack_from('<i', rec, 0x130)[0]
        life, burst = struct.unpack_from('<i', rec, 0x40)[0], struct.unpack_from('<i', rec, 0xe4)[0]
        size = struct.unpack_from('<f', rec, 0x48)[0]
        blends[alpha & 0xff] += 1
        for bit in FLAGS:
            if flags & bit:
                seen[FLAGS[bit]] += 1
        if len(argv) > 1:
            if name == argv[1]:
                for off in range(0x40, RECORD, 4):
                    word = struct.unpack_from('<I', rec, off)[0]
                    print(f'+{off:03x} {word:#010x} {struct.unpack_from("<f", rec, off)[0]:g}')
            continue
        print(f'{name:16} tex {texture:2} life {life:5} burst {burst:3} size {size:7.1f} alpha {alpha & 0xff:#04x} '
              f'chain {text(rec, 0x10):12} {flag_names(flags)}')
    if len(argv) == 1:
        print('flags:', dict(seen.most_common()))
        print('ALPHA_1 low byte:', {f'{k:#04x}': v for k, v in blends.items()})
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
