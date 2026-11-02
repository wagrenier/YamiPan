"""Names for SCE's rx middleware from RosePan, by aligning function sizes in address order.

python rxalign.py <lo> <hi> <ror_lo> <ror_hi> [--min-run N] [--batch out.json] [--missing]

FS2's rx (rxsys / rxfile / rxthread) is the library Rule of Rose links: the same functions in the
same order with the same sizes, down to rxThFindCallback's delay-slot bug (architecture.md "rx").
This aligns FS2's functions in [lo, hi) with RosePan's in [ror_lo, ror_hi) (../RosePan/
ror_functions.txt) by the longest common subsequence of their sizes and prints the pairs (among
the longest, the one with the fewest breaks: a plain LCS paired a run of 0x20-byte rxSys wrappers
one slot off in batch 005). A pair counts only inside a run of at least --min-run (default 3)
consecutive matches on both sides, so a lone 0x14-byte coincidence is not taken ('='; '!' warns
about a kept run of one size, still worth checking by its calls).

Between two kept pairs, Ghidra often merged small functions (a setter only reached through a
pointer, or never called, falls into the function before it). Such a gap is filled ('~') when its
FS2 functions' spans (next start - start) partition its RoR functions' spans exactly: each FS2
function is the first RoR function of its group, and the others ('+') are starts Ghidra lacks, at
the same offset. A gap whose totals differ (changed code: FS2's rx prints file / line in its
error messages, so some functions grew) is filled from both ends as far as the spans allow; a
lone FS2 / RoR pair left in the middle prints '^' (by place only: read it, not in --batch), the
rest '-'.

--batch writes the '=' and '~' pairs as a name_ctx.py batch (tag lib-context) for every FS2
function not yet named that way; review it, then apply it with name_ctx.py. --missing adds the '+'
starts to data/missing_functions.json (bootstrap 4(a) creates them; a rerun then names them).
Offline: reads yamipan_functions.txt.
"""
import argparse
import json
import re

import yamipan

ROR_FUNCTIONS = yamipan.REPO.parent / 'RosePan' / 'ror_functions.txt'
MISSING = yamipan.DATA / 'missing_functions.json'
LINE = re.compile(r'/\* ([0-9a-f]{8}) ([0-9a-f]{8}) \*/ (\S+)\(\)')


def load(path, lo, hi):
    """-> [(addr, size, name, span)] in [lo, hi); span = next function's start - addr."""
    fns = []
    for line in open(path, encoding='utf-8'):
        m = LINE.match(line)
        if m:
            fns.append((int(m[1], 16), int(m[2], 16), m[3]))
    fns.sort()
    out = []
    for k, (addr, size, name) in enumerate(fns):
        if lo <= addr < hi:
            span = fns[k + 1][0] - addr if k + 1 < len(fns) else size
            out.append((addr, size, name, span))
    return out


def align(a, b):
    """-> [(i, j)] pairs of an LCS of the two size sequences; among the longest, the one with the
    most diagonal continuations (fewest breaks), so a run of equal sizes is not paired one off."""
    n, m = len(a), len(b)
    none = (-1, 0)
    d = [[(0, 0)] * (m + 1) for _ in range(n + 1)]     # best (matches, continuations) from (i, j)
    t = [[none] * (m + 1) for _ in range(n + 1)]       # the same, with (i, j) itself matched
    for i in range(n - 1, -1, -1):
        for j in range(m - 1, -1, -1):
            if a[i][1] == b[j][1]:
                nxt = t[i + 1][j + 1]
                go = max(d[i + 1][j + 1], (nxt[0], nxt[1] + 1) if nxt != none else none)
                t[i][j] = (go[0] + 1, go[1])
            d[i][j] = max(t[i][j], d[i + 1][j], d[i][j + 1])
    pairs, i, j, matched = [], 0, 0, False
    while i < n and j < m:
        if matched:     # right after a pair: continue the diagonal when that is as good
            nxt = t[i][j]
            if nxt != none and (nxt[0], nxt[1] + 1) >= d[i][j]:
                pairs.append((i, j))
                i, j = i + 1, j + 1
                continue
            matched = False
        if t[i][j] != none and t[i][j] == d[i][j]:
            pairs.append((i, j))
            i, j, matched = i + 1, j + 1, True
        elif d[i + 1][j] >= d[i][j + 1]:
            i += 1
        else:
            j += 1
    return pairs


def runs(pairs, min_run, a=None):
    """Keep the pairs inside runs of >= min_run consecutive matches on both sides. With a, warn
    about kept runs of one size with a gap next to them: the LCS may have shifted them by one
    (batch 005 named rxSysGetFileSearchDir..rxSysGetLastName one slot off that way)."""
    keep, run = set(), []
    for p in pairs + [None]:
        if run and (p is None or p[0] != run[-1][0] + 1 or p[1] != run[-1][1] + 1):
            if len(run) >= min_run:
                keep.update(run)
                if a and len({a[i][1] for i, _ in run}) == 1:
                    print(f'! run of {len(run)} functions of size {a[run[0][0]][1]:#x} at '
                          f'{a[run[0][0]][0]:08x}..{a[run[-1][0]][0]:08x}: may be shifted, check its ends '
                          f'by their calls')
            run = []
        if p is not None:
            run.append(p)
    return keep


def partition(a, b, fa, fb):
    """Walk fa / fb from their start while each a span is the sum of the next b spans.
    -> ({i: [j, ...]}, a consumed, b consumed)."""
    part, k = {}, 0
    for n, i in enumerate(fa):
        got, grp = 0, []
        while k < len(fb) and got < a[i][3]:
            got += b[fb[k]][3]
            grp.append(fb[k])
            k += 1
        if got != a[i][3]:
            return part, n, k - len(grp)
        part[i] = grp
    return part, len(fa), k


def fill_gaps(a, b, keep):
    """Span partitions of the gaps between kept pairs -> ({i: [j, ...]}, {i}) where the first j
    names i; the set holds the lone FS2 / RoR pairs left in the middle, paired by position only."""
    groups, by_place = {}, set()
    anchors = [(-1, -1)] + sorted(keep) + [(len(a), len(b))]
    for (i0, j0), (i1, j1) in zip(anchors, anchors[1:]):
        fa, fb = list(range(i0 + 1, i1)), list(range(j0 + 1, j1))
        if not fa or not fb:
            continue
        # From the left anchor, then (reversed, each group put back in order) from the right one.
        head, na, nb = partition(a, b, fa, fb)
        if na == len(fa) and nb == len(fb):
            groups.update(head)
            continue
        if i0 < 0 or j0 < 0:    # no left anchor: the range start is not a boundary
            head, na, nb = {}, 0, 0
        ra, rb = fa[na:][::-1], fb[nb:][::-1]
        tail, ta, tb = partition(a, b, ra, rb) if i1 < len(a) and j1 < len(b) else ({}, 0, 0)
        tail = {i: grp[::-1] for i, grp in tail.items()}
        groups.update(head)
        groups.update(tail)
        mid_a, mid_b = fa[na:len(fa) - ta], fb[nb:len(fb) - tb]
        if len(mid_a) == 1 and len(mid_b) == 1:
            groups[mid_a[0]] = mid_b
            by_place.add(mid_a[0])
    return groups, by_place


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('range', nargs=4)
    ap.add_argument('--min-run', type=int, default=3)
    ap.add_argument('--batch')
    ap.add_argument('--missing', action='store_true')
    opt = ap.parse_args()
    lo, hi, rlo, rhi = (int(x, 16) for x in opt.range)
    a = load(yamipan.FUNCTIONS_TXT, lo, hi)
    b = load(ROR_FUNCTIONS, rlo, rhi)
    pairs = align(a, b)
    keep = runs(pairs, opt.min_run, a)
    groups, by_place = fill_gaps(a, b, keep)
    matched = dict(pairs)
    batch, merged = [], {}
    for i, (addr, size, name, span) in enumerate(a):
        if i in by_place:
            ror = b[groups[i][0]]
            print(f'{addr:08x} {size:5x} {name:32s} ^ {ror[2]}  (RoR {ror[0]:08x}, span {ror[3]:x}: '
                  f'by place only, read it)')
            continue
        if i in groups:
            grp = groups[i]
            ror = b[grp[0]]
            print(f'{addr:08x} {size:5x} {name:32s} ~ {ror[2]}  (RoR {ror[0]:08x})')
            for j in grp[1:]:
                x = addr + b[j][0] - ror[0]
                print(f'{x:08x}       {"  (inside " + name + ")":32s} + {b[j][2]}  (RoR {b[j][0]:08x})')
                merged[f'{x:08x}'] = f'{b[j][2]} (rx from RosePan, RoR {b[j][0]:#x}; inside {name})'
            if name != ror[2]:
                names = ', '.join(b[j][2] for j in grp)
                batch.append({'addr': f'0x{addr:x}', 'name': ror[2], 'tag': 'lib-context',
                              'why': f'rx library: RosePan names RoR 0x{ror[0]:x} {ror[2]}; between matched '
                                     f'neighbours its span 0x{span:x} is RoR\'s {names} (rxalign.py span '
                                     f'partition)'})
            continue
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
    print(f'{len(keep)} kept of {len(pairs)} size matches, {len(groups)} by span partition '
          f'({len(merged)} starts Ghidra lacks); {len(a)} FS2 / {len(b)} RoR functions')
    if opt.batch:
        with open(opt.batch, 'w', encoding='utf-8') as f:
            json.dump(batch, f, indent=1, ensure_ascii=False)
        print(f'{len(batch)} entries -> {opt.batch}')
    if opt.missing and merged:
        miss = yamipan.load_json(MISSING, {})
        new = {x: w for x, w in merged.items() if x not in miss}
        yamipan.save_json(MISSING, dict(sorted({**miss, **new}.items())))
        print(f'{len(new)} new starts -> {MISSING.relative_to(yamipan.REPO)}')


if __name__ == '__main__':
    main()
