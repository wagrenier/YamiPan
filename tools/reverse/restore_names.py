"""Snapshot and restore function names.

python restore_names.py --snapshot [FILE]   save every current name (default work/snapshots/<time>.json)
python restore_names.py <snapshot.json>     rename every function whose name differs back (tags untouched)

Take a snapshot before any bulk write (sections.py / libmatch.py --apply, a big name_ctx batch).
"""
import sys
import time

import yamipan

if '--snapshot' in sys.argv:
    rest = [a for a in sys.argv[1:] if a != '--snapshot']
    path = rest[0] if rest else yamipan.WORK / 'snapshots' / f'{time.strftime("%Y%m%d_%H%M%S")}.json'
    names = yamipan.function_names()
    yamipan.save_json(path, {'program': yamipan.PROGRAM, 'names': names})
    print(f'{len(names)} names -> {path}')
    sys.exit(0)
snap = yamipan.load_json(sys.argv[1], {})
before = snap.get('names', snap)
now = yamipan.function_names()
changed = [(a, n) for a, n in before.items() if now.get(a) != n and a in now]
failed = []
for a, n in changed:
    err = yamipan.rename(int(a, 16), n)
    if err:
        failed.append((a, n, err))
print(f'restored {len(changed) - len(failed)}/{len(changed)}')
for f in failed:
    print('  FAILED', *f)
if changed:
    print(yamipan.save_program()[:120])
