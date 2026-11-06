"""Find the shots of a run: mean brightness of three rows of each screenshot.

py shotscan.py [LOW HIGH STEP]   (numbers of work/shots/shot_<n>.png, in numeric order)

Shot numbers count presented frames, not game frames, and the host presents far faster than the game
steps (about 9 to 1 in a menu-free stage, more on a loading screen), so a game frame cannot be turned
into a shot number: scan for the change instead. The rows are near the top, the middle and near the
bottom: bars, fades and screen changes show at once. Pure Python (no PIL on this machine).
"""
import re
import struct
import sys
import zlib
from pathlib import Path

SHOTS = Path(__file__).parent / 'work' / 'shots'


def read_rows(path):
    """{row: mean brightness} of rows near the top, middle and bottom."""
    data = path.read_bytes()
    pos = 8
    chunks = []
    while pos < len(data):
        size, kind = struct.unpack('>I4s', data[pos:pos + 8])
        if kind == b'IHDR':
            width, height, _, colour = struct.unpack('>IIBB', data[pos + 8:pos + 18])
        elif kind == b'IDAT':
            chunks.append(data[pos + 8:pos + 8 + size])
        pos += 12 + size
    raw = zlib.decompress(b''.join(chunks))
    bpp = {2: 3, 6: 4}[colour]
    stride = width * bpp
    wanted = (12, height // 2, height - 12)
    previous = bytearray(stride)
    means = {}
    for y in range(height):
        start = y * (stride + 1)
        kind = raw[start]
        row = bytearray(raw[start + 1:start + 1 + stride])
        for i in range(stride):
            left = row[i - bpp] if i >= bpp else 0
            up = previous[i]
            corner = previous[i - bpp] if i >= bpp else 0
            if kind == 1:
                row[i] = (row[i] + left) & 255
            elif kind == 2:
                row[i] = (row[i] + up) & 255
            elif kind == 3:
                row[i] = (row[i] + ((left + up) >> 1)) & 255
            elif kind == 4:
                guess = left + up - corner
                pa, pb, pc = abs(guess - left), abs(guess - up), abs(guess - corner)
                best = left if pa <= pb and pa <= pc else up if pb <= pc else corner
                row[i] = (row[i] + best) & 255
        if y in wanted:
            means[y] = sum(row[0:stride:bpp * 7]) / (width / 7)
        previous = row
    return width, height, [round(means[y]) for y in wanted]


numbers = sorted(int(re.search(r'(\d+)', p.name).group(1)) for p in SHOTS.glob('shot_*.png'))
if not numbers:
    sys.exit('no shots in ' + str(SHOTS))
low, high, step = [int(a) for a in sys.argv[1:4]] if len(sys.argv) > 3 else (numbers[0], numbers[-1], 0)
if step == 0:
    step = max(30, (high - low) // 60 // 30 * 30)
for n in numbers:
    if low <= n <= high and (n - low) % step == 0:
        path = SHOTS / f'shot_{n:05}.png'
        if path.stat().st_size:
            width, height, rows = read_rows(path)
            print(n, f'{width}x{height}', rows)
