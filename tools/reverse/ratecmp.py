"""ratecmp.py NAME=run.log NAME=run.log: compare two YAMIPAN_RATE_LOG runs (25 Hz and 50 Hz) of the same pad script, by stage time.

Per run: the game frame's length in game seconds, the player's and the camera's speed, how evenly each moves from one
game frame to the next (a 25 Hz-stepped motion at 50 Hz shows as steps alternating with zeros), and the eye's
distance behind the player. Then the two runs side by side from the moment the player starts walking: positions at
the same stage time.
"""
import bisect
import math
import re
import statistics
import sys

LINE = re.compile(r'\[rate\] frame (\d+) t ([\d.]+) stage ([\d.]+) player (\S+) (\S+) (\S+) eye (\S+) (\S+) (\S+)')


def load(path):
    rows = []
    for line in open(path, encoding='utf-8', errors='replace'):
        m = LINE.search(line)
        if m:
            v = [float(x) for x in m.groups()]
            rows.append({'frame': int(v[0]), 't': v[1], 'stage': v[2], 'player': tuple(v[3:6]), 'eye': tuple(v[6:9])})
    return rows


def walk_start(rows):
    for a, b in zip(rows, rows[1:]):
        dt = b['stage'] - a['stage']
        if dt > 0 and math.dist(a['player'], b['player']) / dt > 50:
            return a['stage']
    return None


def summary(name, rows):
    steps = [b['stage'] - a['stage'] for a, b in zip(rows, rows[1:])]
    print(f"{name}: {len(rows)} stage frames, stage time {rows[0]['stage']:.2f} .. {rows[-1]['stage']:.2f} s, "
          f"frame length {statistics.median(steps) * 1000:.1f} ms (median)")
    for what in ('player', 'eye'):
        moves = [math.dist(a[what], b[what]) for a, b in zip(rows, rows[1:]) if b['stage'] > a['stage']]
        moving = [m for m in moves if m > 0.5]
        if not moving:
            continue
        zeros = sum(1 for a, b, c in zip(moves, moves[1:], moves[2:]) if b < 0.05 and a > 1 and c > 1)
        speeds = [math.dist(a[what], b[what]) / (b['stage'] - a['stage'])
                  for a, b in zip(rows, rows[1:]) if b['stage'] > a['stage'] and math.dist(a[what], b[what]) > 0.5]
        ratio = [max(x, y) / min(x, y) for x, y in zip(moving, moving[1:]) if min(x, y) > 0.5]
        print(f"  {what}: moving in {len(moving)} frames, median {statistics.median(speeds):.0f} units/s, "
              f"step {statistics.median(moving):.2f} units a frame, next/this step ratio median "
              f"{statistics.median(ratio):.2f} (p90 {sorted(ratio)[int(len(ratio) * 0.9)]:.2f}), stalls between moves {zeros}")
    lag = [math.dist(r['player'], r['eye']) for r in rows]
    print(f"  eye to player: median {statistics.median(lag):.0f}, min {min(lag):.0f}, max {max(lag):.0f}")


def at(rows, key, when):
    times = [r['stage'] for r in rows]
    i = bisect.bisect_left(times, when)
    if i <= 0 or i >= len(rows):
        return None
    a, b = rows[i - 1], rows[i]
    f = (when - a['stage']) / (b['stage'] - a['stage']) if b['stage'] > a['stage'] else 0
    return tuple(p + (q - p) * f for p, q in zip(a[key], b[key]))


runs = [(p.split('=')[0], load(p.split('=')[1])) for p in sys.argv[1:]]
for name, rows in runs:
    summary(name, rows)
if len(runs) == 2:
    (na, a), (nb, b) = runs
    sa, sb = walk_start(a), walk_start(b)
    print(f"walking starts at stage {sa:.2f} s ({na}) and {sb:.2f} s ({nb})")
    print(" since   player apart   eye apart   eye-player " + na + " / " + nb)
    for since in (0.5, 1, 2, 3, 4, 6, 8, 10, 12, 14, 16, 18, 20):
        pa, pb = at(a, 'player', sa + since), at(b, 'player', sb + since)
        ea, eb = at(a, 'eye', sa + since), at(b, 'eye', sb + since)
        if None in (pa, pb, ea, eb):
            break
        print(f"{since:6.1f} {math.dist(pa, pb):12.1f} {math.dist(ea, eb):11.1f}   "
              f"{math.dist(pa, ea):6.0f} / {math.dist(pb, eb):.0f}")
