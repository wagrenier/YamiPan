"""Names for SCE's rx middleware from RosePan, by aligning function sizes in address order.

python rxalign.py <lo> <hi> <ror_lo> <ror_hi> [--min-run N] [--batch out.json]

FS2's rx (rxsys / rxfile / rxthread) is the library Rule of Rose links: the same functions in the
same order with the same sizes, down to rxThFindCallback's delay-slot bug (architecture.md "rx").
This aligns FS2's functions in [lo, hi) with RosePan's in [ror_lo, ror_hi) (../RosePan/
ror_functions.txt) by the longest common subsequence of their sizes and prints the pairs. A pair
counts only inside a run of at least --min-run (default 3) consecutive matches on both sides, so a
lone 0x14-byte coincidence is not taken. --batch writes those pairs as a name_ctx.py batch
(tag lib-context) for every FS2 function not yet named that way; review it, then apply it with
name_ctx.py. Offline: reads yamipan_functions.txt.
"""
import argparse
import json
import re

import yamipan

ROR_FUNCTIONS = yamipan.REPO.parent / 'RosePan' / 'ror_functions.txt'
LINE = re.compile(r'/\* ([0-9a-f]{8}) ([0-9a-f]{8}) \*/ (\S+)\(\)')


def load(path, lo, hi):
    out = []
    for line in open(path, encoding='utf-8'):
        m = LINE.match(line)
        if m and lo <= int(m[1], 16) < hi:
            out.append((int(m[1], 16), int(m[2], 16), m[3]))
    return out


def align(a, b):
    """-> [(i, j)] pairs of an LCS of the two size sequences."""
    n, m = len(a), len(b)
    d = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(n - 1, -1, -1):
        for j in range(m - 1, -1, -1):
            d[i][j] = d[i + 1][j + 1] + 1 if a[i][1] == b[j][1] else max(d[i + 1][j], d[i][j + 1])
    pairs, i, j = [], 0, 0
    while i < n and j < m:
        if a[i][1] == b[j][1] and d[i][j] == d[i + 1][j + 1] + 1:
            pairs.append((i, j))
            i, j = i + 1, j + 1
        elif d[i + 1][j] >= d[i][j + 1]:
            i += 1
        else:
            j += 1
    return pairs


def runs(pairs, min_run):
    """Keep the pairs inside runs of >= min_run consecutive matches on both sides."""
    keep, run = set(), []
    for p in pairs + [None]:
        if run and (p is None or p[0] != run[-1][0] + 1 or p[1] != run[-1][1] + 1):
            if len(run) >= min_run:
                keep.update(run)
            run = []
        if p is not None:
            run.append(p)
    return keep


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('range', nargs=4)
    ap.add_argument('--min-run', type=int, default=3)
    ap.add_argument('--batch')
    opt = ap.parse_args()
    lo, hi, rlo, rhi = (int(x, 16) for x in opt.range)
    min_run, out = opt.min_run, opt.batch
    a = load(yamipan.FUNCTIONS_TXT, lo, hi)
    b = load(ROR_FUNCTIONS, rlo, rhi)
    pairs = align(a, b)
    keep = runs(pairs, min_run)
    matched = dict(pairs)
    batch = []
    for i, (addr, size, name) in enumerate(a):
        j = matched.get(i)
        if j is None:
            print(f'{addr:08x} {size:5x} {name:32s}   -')
            continue
        ror = b[j]
        mark = '=' if (i, j) in keep else '?'
        print(f'{addr:08x} {size:5x} {name:32s} {mark} {ror[2]}  (RoR {ror[0]:08x})')
        if mark == '=' and name != ror[2]:
            batch.append({'addr': f'0x{addr:x}', 'name': ror[2], 'tag': 'lib-context',
                          'why': f'rx library: RosePan names RoR 0x{ror[0]:x} {ror[2]}; same size 0x{size:x} '
                                 f'and place in a run of matching functions (rxalign.py)'})
    print(f'{len(keep)} kept of {len(pairs)} size matches; {len(a)} FS2 / {len(b)} RoR functions')
    if out:
        with open(out, 'w', encoding='utf-8') as f:
            json.dump(batch, f, indent=1, ensure_ascii=False)
        print(f'{len(batch)} entries -> {out}')


if __name__ == '__main__':
    main()
