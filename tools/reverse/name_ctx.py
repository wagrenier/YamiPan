"""Apply names worked out by reading the code, and keep the ledger.

python name_ctx.py <batch.json> [--no-refresh]
batch: [{"addr": "0010eebc", "name": "DogActor::DogActor", "tag": "game-inferred" | "lib-context",
         "why": "evidence in one line"}, ...]

game-inferred  a game function named from its code and context (strings, asserts, vtable slot, callers).
              Methods are 'Class::method' (Ghidra keeps the '::' literally); an overload that would
              collide gets its parameters without spaces: 'DogModel::setHide(bool)'.
lib-context   a library / runtime function named by reading it (Sony / libc / libgcc names).

The ELF's own symbols (tag game-symbol) are never renamed. Replacing a lib-match name records the address
in data/overrides.json (libmatch.py --apply never restores it). Every name lands in
data/context_names.json with its evidence. Batch order does not matter: an entry whose name is still
held by another function the same batch renames is retried after the rest. Afterwards the program is
saved (save_program; with Ghidra down the
report must say "Ghidra has unsaved changes: Ctrl+S") and refresh.py runs (fetch, features, export, status), so stubgen / callcheck / stage see the new
names; --no-refresh skips both. Never pipe this into `head`: a closed pipe kills the refresh.
"""
import subprocess
import sys
from pathlib import Path

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
args = [x for x in sys.argv[1:] if not x.startswith('--')]
batch = yamipan.load_json(args[0], [])
ledger = yamipan.load_json(yamipan.DATA / 'context_names.json', {})
overrides = yamipan.load_json(yamipan.DATA / 'overrides.json', {})
yamipan.ensure_tags()

names = yamipan.function_names()
taken = {}
for a, n in names.items():
    taken.setdefault(n, []).append(a)

renamed_in_batch = {e['addr'].lower().replace('0x', '').zfill(8) for e in batch}
done, failed, deferred = 0, [], []
queue = list(batch)
while queue:
    e = queue.pop(0)
    a = e['addr'].lower().replace('0x', '').zfill(8)
    ai = int(a, 16)
    if e['tag'] not in yamipan.CONTEXT_TAGS:
        failed.append((a, e['name'], 'unknown tag ' + e['tag']))
        continue
    if ' ' in e['name']:
        failed.append((a, e['name'], 'Ghidra rejects whitespace in names'))
        continue
    if a not in names:
        failed.append((a, e['name'], 'not a function start (python ghidra.py mkfunc first)'))
        continue
    others = [x for x in taken.get(e['name'], []) if x != a]
    if others and e not in deferred and all(x in renamed_in_batch for x in others):
        deferred.append(e)              # held by a function this batch renames: retry last
        queue.append(e)
        continue
    if others:
        failed.append((a, e['name'], 'name already used at ' + ','.join(others)))
        continue
    old = [t for t in [yamipan.get_tag(ai)] if t in yamipan.ALL_TAGS]
    if 'game-symbol' in old:
        failed.append((a, e['name'], f'{names[a]} is the ELF\'s own symbol; not renamed'))
        continue
    if any(t in yamipan.MATCH_TAGS for t in old):
        overrides[a] = {'was': names[a], 'now': e['name']}
    if names[a] != e['name']:
        err = yamipan.rename(ai, e['name'])
        if err:
            failed.append((a, e['name'], err))
            continue
        if a in taken.get(names[a], []):          # the old name is free now (shifted runs)
            taken[names[a]].remove(a)
    yamipan.set_tag(ai, e['tag'])
    ledger[a] = {'name': e['name'], 'tag': e['tag'], 'why': e.get('why', ''),
                 'was': ledger.get(a, {}).get('was', names[a])}
    taken.setdefault(e['name'], []).append(a)
    done += 1

yamipan.save_json(yamipan.DATA / 'context_names.json', ledger)
yamipan.save_json(yamipan.DATA / 'overrides.json', overrides)
print(f'named {done}/{len(batch)}; ledger {len(ledger)}; overrides {len(overrides)}')
for x in failed:
    print('  FAILED', *x)
sys.stdout.flush()

if '--no-refresh' not in sys.argv and done:
    print(yamipan.save_program()[:120], flush=True)
    subprocess.run([sys.executable, str(Path(__file__).resolve().parent / 'refresh.py')],
                   cwd=Path(__file__).resolve().parent)
