"""TIM2 textures -> PNG: the reference pictures a drawn screen is checked against.

python tm2png.py PATH.tm2 [...] [--out DIR]          convert TIM2 files
python tm2png.py --scan FILE [...] [--out DIR]       carve every embedded TIM2 out of an archive / any blob
                                                     (FS2 packs its .tm2 files; offsets are printed)

Output goes to work/ref/<name>/<texture>[_<picture>].png by default. Open them with the Read tool next to
work/shots/last.png. FS2 uses TIM2 (rena_2d_texture_tim2.cpp; where its .tm2 files live in ROM.00x is open).

TIM2 (Sony): 'TIM2', version, alignment, picture count, then per picture a header {total, clut size,
image size, header size, clut colours, format, mipmaps, clut type, image type, width, height, GS regs},
image data, then the CLUT. Image types 1/2/3 = 16/24/32-bit direct, 4/5 = 4/8-bit indexed; CLUT type
low bits 1/2/3 = 16/24/32-bit entries, bit 0x80 = linear order (else 8-bit CLUTs are in CSM1 order,
blocks of 8 entries 8..15 and 16..23 swapped). PS2 alpha 0..0x80 is scaled to 0..0xff.
"""
import struct
import sys
import zlib
from pathlib import Path

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REF = yamipan.WORK / 'ref'


def png(path, w, h, rgba):
    """Write RGBA bytes (w*h*4) as a PNG."""
    raw = b''.join(b'\0' + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    data = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(data)


def colour(buf, off, kind):
    """(r, g, b, a) of one 16/24/32-bit TIM2 colour."""
    if kind == 1:
        v, = struct.unpack_from('<H', buf, off)
        return ((v & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, ((v >> 10) & 31) * 255 // 31,
                255 if v & 0x8000 else 0)
    if kind == 2:
        return buf[off], buf[off + 1], buf[off + 2], 255
    return buf[off], buf[off + 1], buf[off + 2], min(255, buf[off + 3] * 2)


def pictures(buf, base=0):
    """([(index, w, h, rgba bytes)], total byte size) of the TIM2 at buf[base:]."""
    if buf[base:base + 4] != b'TIM2':
        raise ValueError('not a TIM2')
    align = 128 if buf[base + 5] == 1 else 16
    count, = struct.unpack_from('<H', buf, base + 6)
    off = base + (16 if align == 16 else 128)
    out = []
    for i in range(count):
        total, clut_size, image_size, header_size, clut_colours = struct.unpack_from('<IIIHH', buf, off)
        _fmt, _mips, clut_type, image_type, w, h = struct.unpack_from('<BBBBHH', buf, off + 16)
        if total == 0 or w > 4096 or h > 4096 or image_type not in (0, 1, 2, 3, 4, 5):
            raise ValueError(f'implausible picture header at {off:#x}')
        image = off + header_size
        clut = image + image_size
        if image_size == 0:                 # a palette alone
            off += total
            continue
        pal = []
        if image_type in (4, 5):
            ck = clut_type & 0x3f
            size = {1: 2, 2: 3, 3: 4}[ck]
            pal = [colour(buf, clut + k * size, ck) for k in range(clut_colours)]
            if image_type == 5 and not clut_type & 0x80 and clut_colours >= 32:
                for b in range(0, clut_colours, 32):          # CSM1: swap entries 8..15 / 16..23
                    pal[b + 8:b + 16], pal[b + 16:b + 24] = pal[b + 16:b + 24], pal[b + 8:b + 16]
        rgba = bytearray(w * h * 4)
        for p in range(w * h):
            if image_type == 4:
                byte = buf[image + p // 2]
                c = pal[(byte >> 4) if p & 1 else (byte & 15)]
            elif image_type == 5:
                c = pal[buf[image + p]]
            else:
                c = colour(buf, image + p * {1: 2, 2: 3, 3: 4}[image_type], image_type)
            rgba[p * 4:p * 4 + 4] = bytes(c)
        out.append((i, w, h, bytes(rgba)))
        off += total
    return out, off - base


def convert(buf, stem, out_dir, base=0):
    written = []
    pics, size = pictures(buf, base)
    for i, w, h, rgba in pics:
        name = stem + (f'_{i}' if i else '') + '.png'
        png(Path(out_dir) / name, w, h, rgba)
        written.append(f'{name} {w}x{h}')
    return written, size


def main(argv):
    out = None
    if '--out' in argv:
        i = argv.index('--out')
        out = Path(argv[i + 1])
        del argv[i:i + 2]
    if argv and argv[0] == '--scan':
        for p in argv[1:]:
            buf = Path(p).read_bytes()
            dest = out or REF / Path(p).stem
            pos, n = 0, 0
            while (pos := buf.find(b'TIM2', pos)) >= 0:
                try:
                    lines, size = convert(buf, f'{Path(p).stem}_{pos:08x}', dest, pos)
                    for l in lines:
                        print(f'  @{pos:#010x} {l}')
                    n += 1
                    pos += max(size, 4)
                except (ValueError, struct.error, IndexError, KeyError):
                    pos += 4
            print(f'{p}: {n} TIM2 files -> {dest}')
        return 0
    for p in argv:
        lines, _ = convert(Path(p).read_bytes(), Path(p).stem, out or REF / Path(p).stem)
        for l in lines:
            print(l)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]) if len(sys.argv) > 1 else __doc__)
