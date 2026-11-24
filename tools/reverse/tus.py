"""Translation units (TUs): the unit of work of one session.

A TU is one FS2 object file, reconstructed as one source file under src/; its key is that path
('game/gamesystem.cpp': FS2 is stripped, so most file names are descriptive guesses until a source
string settles them). Membership: address ranges pinned in
data/tus.json (`define`), else export.py's inference (yamipan_functions.txt). Game code inference could not
place forms 'unassigned@<addr>' runs until a session defines them. Named linkonce sections (inline
methods, data/layout.json satellite_ranges) ride with their class's TU, else with their main caller's.
data/tus.json also keeps each TU's state, the handoff notes for the next session, the session history
and the pinned queue.

python tus.py list [--all] [-n N]           the queue: state, progress, frontier score
python tus.py next [--claim] [--json]       pick the next TU: active, queue, partial, frontier, main
python tus.py claim <tu>                    work on another TU instead (releases the current claim)
python tus.py brief <tu> [--no-stage]       work/tu/<slug>/brief.md + stage.py bundle of the session's slice
python tus.py define <tu> <lo-hi>... [--why TEXT]   pin FS2 address ranges (hi exclusive) to a TU
python tus.py queue <tu> [--top | --remove] [--task TEXT]   edit the pinned queue; --task makes <tu> a host task:
                                            a src/ file with no FS2 functions (platform layer work, a fix),
                                            picked like a TU, its brief shows TEXT
python tus.py check <tu> [--run N] [--pad S] build, stub undefined references, rebuild; callcheck the TU;
                                            layoutcheck; smoke-run N s with settings.smoke_pad (or S, 'none')
python tus.py finish <tu> --status done|verify|partial|blocked --summary TEXT [--notes TEXT]
                         [--coauthor LINE] [--no-commit]    record the session, refresh, commit
python tus.py mark <tu> done|verify|partial|blocked|todo --why TEXT   set another TU's state from what a
                                            run showed (in its history; committed by the session's finish)

Source-only mode (no CMakeLists.txt yet: no host toolchain or platform layer): check runs callcheck on
the TU's file and layoutcheck's lint only; finish skips the build, refuses --status done (nothing was
seen on screen) and records the session as 'not built'.

<tu> is a key ('game/gamesystem.cpp'), a unique file name ('gamesystem.cpp'), an 'unassigned@...' run, or a
function name/address (the TU that contains it).
"""
import json
import os
import re
import shutil
import subprocess
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
HERE = Path(__file__).resolve().parent
# verify: code complete, but nothing on screen can show it yet (the visual test decides 'done'); not
# picked again until a later session sees its output and marks it done.
STATES = ('todo', 'active', 'partial', 'verify', 'done', 'blocked')
DEFAULT_SLICE = 0x2800          # bytes of FS2 code staged per session (~2500 instructions)
HIT_WEIGHT = 5                  # a stub the last smoke run reached outweighs five call sites
KEY_SUFFIXES = ('.c', '.cpp', '.cc')


# ---- model -------------------------------------------------------------------

class Model:
    """Functions, TU membership, reconstruction state and frontier, computed from the current
    yamipan_functions.txt, work/feat_fs2.json, data/tus.json and src/."""

    def __init__(self):
        self.funcs = yamipan.functions()
        self.feat = {f['addr']: f for f in
                     json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']}
        self.db = yamipan.load_json(yamipan.TUS_JSON, {})
        self.db.setdefault('settings', {}).setdefault('slice_bytes', DEFAULT_SLICE)
        self.db.setdefault('queue', [])
        self.db.setdefault('tus', {})
        self.rec = yamipan.reconstructed()
        self.stub = yamipan.stubs()
        self.done = {a for a in self.rec if a not in self.stub}
        self.hits = self._stub_hits()
        self.callers = defaultdict(list)
        for a, f in self.feat.items():
            for c in dict.fromkeys(f['calls'] + f.get('tails', []) + f.get('fptrs', [])):
                self.callers[c].append(a)
        self._assign()
        self.frontier = Counter()
        # settings.frontier_ignore: callees the port replaces wholesale (the assert handler, written
        # back as FS2_ASSERT), so they never pull a session towards their file.
        ignore = {int(x, 16) for x in self.db['settings'].get('frontier_ignore', [])}
        for a in self.done:
            f = self.feat.get(a, {})
            for c in f.get('calls', []) + f.get('tails', []) + f.get('fptrs', []):
                if c not in self.done and c in self.feat and not yamipan.in_lib(c) and c not in ignore:
                    self.frontier[c] += 1

    def name(self, a):
        return self.funcs.get(a, {}).get('name') or self.feat.get(a, {}).get('name') or f'FUN_{a:08x}'

    def size(self, a):
        return self.feat.get(a, {}).get('size', 0) * 4 or self.funcs.get(a, {}).get('size', 0)

    def _stub_hits(self):
        hits = set()
        for log in yamipan.WORK.glob('run_*.log'):
            for m in re.finditer(r'\[stub\] .+? \(FS2 0x([0-9a-fA-F]{8})\) reached',
                                 log.read_text(encoding='utf-8', errors='replace')):
                hits.add(int(m.group(1), 16))
        return hits

    def _assign(self):
        """self.tu_of {addr: key} for game code; satellites without a class file go to their main caller's TU."""
        self.tu_of = {}
        run = None
        for a in sorted(self.funcs):
            if yamipan.in_lib(a):
                run = None
                continue
            key = yamipan.tu_key(self.funcs[a]['file'])
            if yamipan.is_satellite(a):
                if key:
                    self.tu_of[a] = key
                run = None
                continue
            if key is None:                      # unplaced game code: group contiguous runs
                run = run or f'unassigned@{a:08x}'
                key = run
            else:
                run = None
            self.tu_of[a] = key
        for a in sorted(self.funcs):
            if yamipan.is_satellite(a) and a not in self.tu_of:
                owners = Counter(self.tu_of[c] for c in self.callers.get(a, []) if c in self.tu_of)
                if owners:
                    self.tu_of[a] = max(owners, key=lambda k: (owners[k], -len(k)))
        self.members = defaultdict(list)
        for a, k in sorted(self.tu_of.items()):
            self.members[k].append(a)

    def resolve(self, text):
        """TU key from a key, unique file name, unassigned run, function name or address."""
        if text in self.members or text in self.db['tus']:
            return text
        text = text.replace('\\', '/').removeprefix('src/')
        if text in self.members or text in self.db['tus']:
            return text
        cands = [k for k in self.members if k.endswith('/' + text)]
        if len(cands) == 1:
            return cands[0]
        if len(cands) > 1:
            raise SystemExit(f'{text!r} is ambiguous: {", ".join(cands)}')
        addrs = []
        if re.fullmatch(r'(0x)?[0-9a-fA-F]{6,8}', text):
            addrs = [int(text, 16)]
        else:
            addrs = [a for a, f in self.funcs.items() if f['name'] == text]
        for a in addrs:
            if a in self.tu_of:
                return self.tu_of[a]
        if '/' in text and text.endswith(KEY_SUFFIXES):
            return text                          # a new TU, e.g. about to be defined
        raise SystemExit(f'unknown TU {text!r}')

    def entry(self, key):
        return self.db['tus'].get(key, {})

    def stats(self, key):
        m = self.members.get(key, [])
        done = [a for a in m if a in self.done]
        stubs = [a for a in m if a in self.stub]
        score = sum(self.frontier[a] + HIT_WEIGHT * (a in self.hits) for a in m if a not in self.done)
        e = self.entry(key)
        state = e.get('status') or ('done' if m and len(done) == len(m) else 'partial' if done else 'todo')
        return {'key': key, 'status': state, 'explicit': 'status' in e, 'total': len(m), 'done': len(done),
                'stubs': len(stubs), 'bytes': sum(self.size(a) for a in m),
                'bytes_done': sum(self.size(a) for a in done), 'score': score,
                'hits': sum(a in self.hits for a in m if a not in self.done),
                'fuzzy': key.startswith('unassigned@') or not e.get('ranges')}

    def save(self):
        yamipan.save_json(yamipan.TUS_JSON, self.db)

    def claim(self, key):
        """Mark `key` active; any other active TU goes back to what it was before its claim."""
        for k, e in self.db['tus'].items():
            if k != key and e.get('status') == 'active':
                prev = e.pop('prev_status', None)
                e.pop('claimed', None)
                if prev:
                    e['status'] = prev
                else:
                    e.pop('status')
        e = self.db['tus'].setdefault(key, {})
        if e.get('status') != 'active':
            if e.get('status'):
                e['prev_status'] = e['status']
            e['status'] = 'active'
            e['claimed'] = time.strftime('%Y-%m-%d %H:%M')
        self.save()

    def pick(self):
        """(key, reason) of the next TU to work on, or (None, reason)."""
        explicit = {k: self.stats(k) for k in self.db['tus']}
        # A claim whose range has since joined a file holds no functions: not a session to resume.
        active = [s for s in explicit.values() if s['status'] == 'active' and s['explicit']
                  and (self.members.get(s['key']) or self.entry(s['key']).get('task'))]
        if active:
            return active[0]['key'], 'resuming an unfinished session'
        # The pinned queue is the user's priority: it goes before partial TUs (a queued TU left
        # partial stays in the queue, so it is still continued first).
        for k in self.db['queue']:
            if k not in self.members and not self.entry(k).get('ranges') and not self.entry(k).get('task'):
                continue                         # stale: an unassigned run since defined under a file name
            if self.stats(k)['status'] not in ('done', 'verify', 'blocked'):
                return k, 'pinned in the queue'
        partial = sorted((s for s in explicit.values() if s['status'] == 'partial' and s['explicit']),
                         key=lambda s: -s['score'])
        if partial:
            return partial[0]['key'], 'continuing a partial TU'
        ranked = sorted((self.stats(k) for k in self.members), key=lambda s: (-s['score'], s['key']))
        ranked = [s for s in ranked if s['score'] > 0 and s['status'] not in ('done', 'verify', 'blocked')]
        if ranked:
            return ranked[0]['key'], f'frontier score {ranked[0]["score"]}'
        main = next((a for a, f in self.funcs.items() if f['name'] == 'main'), None)
        if main in self.tu_of and self.stats(self.tu_of[main])['status'] not in ('done', 'verify', 'blocked'):
            return self.tu_of[main], 'frontier empty: start from main'
        return None, 'nothing on the frontier and no queued TU'

    def slice(self, key):
        """The functions one session should do, in order: reached stubs, frontier, then their
        same-TU callees, then the rest in address order; capped at slice_bytes."""
        todo = [a for a in self.members.get(key, []) if a not in self.done]
        budget = self.db['settings']['slice_bytes']
        prio = sorted(todo, key=lambda a: (-(a in self.hits), -self.frontier[a], a))
        out, used, seen = [], 0, set()

        def add(a):
            nonlocal used
            if a in seen or a in self.done or self.tu_of.get(a) != key:
                return
            if out and used + self.size(a) > budget:
                return
            seen.add(a)
            out.append(a)
            used += self.size(a)
            f = self.feat.get(a, {})
            for c in f.get('calls', []) + f.get('tails', []):
                add(c)
        for a in prio:
            add(a)
        return out, [a for a in todo if a not in seen]


# ---- helpers -----------------------------------------------------------------

def slug(key):
    return re.sub(r'[^\w.-]+', '_', key).strip('_')


def run(cmd, **kw):
    print('$', ' '.join(str(c) for c in cmd), flush=True)
    return subprocess.run(cmd, cwd=HERE, **kw)


def git(*args, check=True):
    r = subprocess.run(['git', '-C', str(yamipan.REPO), *args], capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if check and r.returncode:
        raise SystemExit(f'git {" ".join(args)} failed: {r.stderr.strip()}')
    return r.stdout


def opt(args, name, default=None):
    if name in args:
        i = args.index(name)
        val = args[i + 1]
        del args[i:i + 2]
        return val
    return default


def flag(args, name):
    if name in args:
        args.remove(name)
        return True
    return False


def class_context(M, key, members):
    """Brief lines: the classes whose methods / vtables live in this TU (rtti.py)."""
    classes = yamipan.load_json(yamipan.WORK / 'classes.json', [])
    mine = set(members)
    L = []
    for c in classes:
        v = c['vtables'][0]
        slots_here = [s for s in v['slots'] if s['addr'] in mine]
        users_here = [u for u in v['users'] if int(u.rsplit('@', 1)[1], 16) in mine]
        named_here = [a for a in members if M.name(a).startswith(c['name'] + '::')]
        if not (slots_here or users_here or named_here):
            continue
        L.append(f'- `{c["name"]}`' + (f' : {", ".join(c["bases"])}' if c['bases'] else '')
                 + f'; vtable 0x{v["addr"]:08x}, {len(v["slots"])} slots'
                 + (f', vptr at +0x{c["vptr_offset"]:x}' if c['vptr_offset'] is not None else '')
                 + f'; here: {len(slots_here)} virtuals, ctors/dtors {", ".join(u.split("@")[0] for u in users_here) or "-"}'
                 + f' (`python rtti.py --class {c["name"]}`)')
    return L


# ---- commands ----------------------------------------------------------------

def cmd_list(args):
    show_all = flag(args, '--all')
    n = int(opt(args, '-n', 30))
    M = Model()
    rows = [M.stats(k) for k in set(M.members) | set(M.db['tus'])]
    if not show_all:
        rows = [s for s in rows if s['score'] or s['explicit'] or s['done'] or s['stubs']] or rows
    order = {'active': 0, 'partial': 1, 'todo': 2, 'verify': 3, 'blocked': 4, 'done': 5}
    rows.sort(key=lambda s: (order[s['status']], -s['score'], s['key']))
    key, why = M.pick()
    print(f'next: {key} ({why})')
    print(f'{"TU":44} {"state":8} {"done/stub/total":>15} {"KB":>6} {"score":>5} {"hits":>4}')
    for s in rows[:n]:
        print(f'{s["key"][:44]:44} {s["status"]:8} {s["done"]:>5}/{s["stubs"]:>3}/{s["total"]:<5} '
              f'{s["bytes"] / 1024:6.1f} {s["score"]:>5} {s["hits"]:>4}' + ('  fuzzy' if s['fuzzy'] else ''))
    if len(rows) > n:
        print(f'... {len(rows) - n} more (-n N, --all)')


def cmd_next(args):
    claim = flag(args, '--claim')
    as_json = flag(args, '--json')
    M = Model()
    key, why = M.pick()
    if key is None:
        print(json.dumps({'tu': None, 'reason': why}) if as_json else f'no TU to do: {why}')
        return 1
    if claim:
        M.claim(key)
    if as_json:
        print(json.dumps({'tu': key, 'reason': why, **M.stats(key)}))
    else:
        print(f'{key}  ({why})' + ('  [claimed]' if claim else ''))
        print(f'brief: python tus.py brief {key}')
    return 0


def cmd_brief(args):
    no_stage = flag(args, '--no-stage')
    M = Model()
    key = M.resolve(args[0])
    s, e = M.stats(key), M.entry(key)
    members = M.members.get(key, [])
    sl, later = M.slice(key)
    out_dir = yamipan.WORK / 'tu' / slug(key)
    out_dir.mkdir(parents=True, exist_ok=True)
    L = []
    w = L.append
    w(f'# TU {key}')
    w('')
    w(f'- state: **{s["status"]}**; {s["done"]} reconstructed, {s["stubs"]} stubs, {s["total"]} functions, '
      f'{s["bytes"]:#x} bytes; frontier score {s["score"]}, {s["hits"]} reached stubs')
    dest = yamipan.SRC / key if not key.startswith('unassigned@') else None
    w(f'- destination: `src/{key}`' if dest else
      '- destination: **undecided** -- a run of game code export.py could not place. Decide the real '
      'file (a neighbouring TU, a class\'s file, FS2\'s own source-path strings), then '
      '`python tus.py define <file> <lo>-<hi>`.')
    if e.get('ranges'):
        w(f'- ranges (pinned): ' + ', '.join(f'{lo}-{hi}' for lo, hi in e['ranges'])
          + (f' -- {e["why"]}' if e.get('why') else ''))
    elif members:
        w(f'- membership is **inferred** (source strings, classes, link order) and may be wrong. Check the '
          f'boundaries against the neighbours below, then pin them: `python tus.py define {key} <lo>-<hi> '
          f'--why "..."`.')
    w('')
    if e.get('task'):
        w('## Host task (no FS2 functions: platform-layer work, queued by the user)')
        w('')
        w(e['task'])
        w('')
    if not key.startswith('unassigned@') and not e.get('task'):
        import tuid                     # is the file really what its name says?
        L.extend(tuid.evidence(M, key))
    if e.get('notes'):
        w('## Handoff notes from the previous session')
        w('')
        w(e['notes'])
        w('')
    if e.get('sessions'):
        w('## Session history')
        w('')
        for x in e['sessions'][-6:]:
            w(f'- {x.get("date", "?")} {x.get("status", "")}: {x.get("summary", "")}'
              + (f' (session `{x["session"]}`)' if x.get('session') else ''))
        w('')
    cc = class_context(M, key, members)
    if cc:
        w('## Classes in this TU (CodeWarrior: 4-byte vtable slots after {type_info, 0}; the vptr can follow '
          'the data; architecture.md "Binary and toolchain")')
        w('')
        L.extend(cc)
        w('')
    w(f'## This session\'s slice ({len(sl)} functions, {sum(M.size(a) for a in sl):#x} bytes)')
    w('')
    w('Staged in `work/stage/' + f'tu_{slug(key)}/` (decompilation, callers, callees, strings, class).'
      if not no_stage else 'Not staged (--no-stage).')
    w('')
    w('| FS2 addr | name | size | prov | state | why first | called from other TUs |')
    w('|---|---|---|---|---|---|---|')
    for a in sl:
        f = M.funcs.get(a, {})
        ext = sorted({M.tu_of.get(c, '?') for c in M.callers.get(a, []) if M.tu_of.get(c) != key})
        why = 'reached stub' if a in M.hits else f'frontier x{M.frontier[a]}' if M.frontier[a] else ''
        w(f'| `{a:08x}` | {M.name(a)} | {M.size(a):#x} | {f.get("prov") or "-"} | '
          f'{"stub" if a in M.stub else "todo"} | {why} | {", ".join(ext)[:80] or "-"} |')
    w('')
    if later:
        w(f'Left for later sessions: {len(later)} functions, {sum(M.size(a) for a in later):#x} bytes '
          f'(continue with `--status partial` when the slice is done).')
        w('')
    out_calls, lib_calls = defaultdict(set), set()
    for a in sl:
        f = M.feat.get(a, {})
        for c in f.get('calls', []) + f.get('tails', []):
            if yamipan.in_lib(c):
                lib_calls.add(M.name(c))
            elif M.tu_of.get(c, 'lib') != key and c not in M.done:
                out_calls[M.tu_of.get(c, '?')].add(M.name(c))
    if out_calls:
        w('## Calls out of this TU (declare in their TU headers; the build turns them into stubs)')
        w('')
        for k, names in sorted(out_calls.items()):
            w(f'- `{k}`: {", ".join(sorted(names))}')
        w('')
    if lib_calls:
        w('## Library calls (src/sdk or the host libc provide them; name any FUN_ here first)')
        w('')
        w('- ' + ', '.join(sorted(lib_calls)))
        w('')
    elsewhere = [(a, M.stub[a][0]) for a in members if a in M.stub
                 and dest and M.stub[a][0].resolve() != dest.resolve()]
    if elsewhere:
        w('## Stubs of this TU living in other files (delete them when you write the function)')
        w('')
        for a, p in elsewhere:
            w(f'- {M.name(a)} (FS2 0x{a:08x}): `{p.relative_to(yamipan.REPO).as_posix()}`')
        w('')
    own = [a for a in members if not yamipan.is_satellite(a)]
    if own:
        lo, hi = own[0], own[-1]
        order = sorted(a for a in M.funcs if not yamipan.in_lib(a) and not yamipan.is_satellite(a))
        i, j = order.index(lo), order.index(hi)
        w('## Neighbours (for boundary decisions)')
        w('')
        for a in order[max(0, i - 6):i] + ['...'] + order[j + 1:j + 7]:
            if a == '...':
                w(f'- ... {key}: `{lo:08x}`..`{hi + M.size(hi):08x}` ...')
            else:
                w(f'- `{a:08x}` {M.name(a)} -- {M.tu_of.get(a, "?")}')
        w('')
    w('## Protocol')
    w('')
    w('Follow the session protocol in the yamipan-port skill (SKILL.md, "One TU per session"): boundaries, '
      'names, header, code, `python tus.py check ' + key + ' --run 15`, record knowledge, '
      '`python tus.py finish ' + key + ' --status ... --summary ... --notes ...`.')
    path = out_dir / 'brief.md'
    path.write_text('\n'.join(L) + '\n', encoding='utf-8')
    if sl and not no_stage:
        r = run([sys.executable, 'stage.py', *[f'{a:08x}' for a in sl], '--label', f'tu_{slug(key)}'])
        if r.returncode:
            print('stage.py failed (is Ghidra running?)')
    print(f'brief: {path.relative_to(yamipan.REPO).as_posix()}')
    return 0


def cmd_define(args):
    why = opt(args, '--why', '')
    M = Model()
    key = args[0]
    if key.startswith('unassigned@') or not key.endswith(KEY_SUFFIXES):
        raise SystemExit('define needs a file key under src/, e.g. game/gamesystem.cpp')
    key = key.replace('\\', '/').removeprefix('src/')
    ranges = []
    for r in args[1:]:
        lo, hi = (int(x, 16) for x in r.split('-'))
        if hi <= lo:
            raise SystemExit(f'empty range {r}')
        ranges.append((lo, hi))
    for lo, hi, other in yamipan.tu_ranges():
        if other != key and any(lo < h and l < hi for l, h in ranges):
            raise SystemExit(f'{lo:08x}-{hi:08x} already belongs to {other}; redefine that TU first')
    e = M.db['tus'].setdefault(key, {})
    e['ranges'] = [[f'{lo:08x}', f'{hi:08x}'] for lo, hi in sorted(ranges)]
    if why:
        e['why'] = why
    M.save()
    r = run([sys.executable, 'export.py'])
    if r.returncode:
        return r.returncode
    M = Model()
    print(f'{key}: {len(M.members.get(key, []))} functions: '
          + ', '.join(M.name(a) for a in M.members.get(key, []))[:1500])
    return 0


def cmd_claim(args):
    M = Model()
    key = M.resolve(args[0])
    if key.startswith('unassigned@'):
        raise SystemExit('define the run as a real file first (tus.py define <file> <lo>-<hi>)')
    M.claim(key)
    print(f'{key}: claimed (other active claims released)')
    return 0


def cmd_queue(args):
    top, remove = flag(args, '--top'), flag(args, '--remove')
    task = opt(args, '--task')
    M = Model()
    key = M.resolve(args[0])
    if task:
        M.db['tus'].setdefault(key, {})['task'] = task
    q = [k for k in M.db['queue'] if k != key]
    if not remove:
        q = [key] + q if top else q + [key]
    M.db['queue'] = q
    M.save()
    print('queue:', ', '.join(q) or '(empty)')
    if not remove and M.stats(key)['status'] in ('done', 'verify', 'blocked'):
        print(f'warning: {key} is {M.stats(key)["status"]}: next skips it. Functions in an unassigned run stay '
              'out of it until `define` joins them, so claim by hand or queue that run.')
    return 0


def progress_shot(key, claimed):
    """Copy the session's last smoke-run screenshot to progress/<time>_<tu>.png, so the screen's history
    can be browsed per TU. Only a shot taken since the claim counts. progress/ is git-ignored: the shots
    show the game's own content, so they stay on this machine and are never committed."""
    last = yamipan.WORK / 'shots' / 'last.png'
    if not last.exists():
        return None
    try:
        since = time.mktime(time.strptime(claimed, '%Y-%m-%d %H:%M')) if claimed else time.time() - 3 * 3600
    except ValueError:
        since = time.time() - 3 * 3600
    if last.stat().st_mtime < since:
        return None
    dest = yamipan.REPO / 'progress' / f'{time.strftime("%Y%m%d-%H%M")}_{slug(key)}.png'
    dest.parent.mkdir(exist_ok=True)
    shutil.copyfile(last, dest)
    return dest.relative_to(yamipan.REPO).as_posix()


def has_build():
    return (yamipan.REPO / 'CMakeLists.txt').exists()


def source_only_check(key):
    """Source-only mode (no CMakeLists.txt: no host toolchain or platform layer yet): what works without a
    compiler -- callcheck on the TU's file and layoutcheck's lint. Returns (callcheck rc, lint problems)."""
    import layoutcheck
    path = yamipan.SRC / key
    cc = run([sys.executable, 'callcheck.py', str(path)]).returncode if path.exists() else 0
    problems = [w for f in layoutcheck.game_files() for w in layoutcheck.lint(f)]
    for w in problems:
        print('  ' + w)
    return cc, problems


def build():
    r = run([sys.executable, 'build.py'], capture_output=True, text=True, encoding='utf-8', errors='replace')
    print(r.stdout[-6000:], r.stderr[-2000:])
    return r.returncode, r.stdout


def cmd_check(args):
    secs = opt(args, '--run')
    M = Model()
    key = M.resolve(args[0])
    if not has_build():
        cc, problems = source_only_check(key)
        print(f'check {key}: SOURCE-ONLY (no CMakeLists.txt yet: not built, not run), callcheck '
              f'{"clean" if cc == 0 else "has MISSING calls (fix or explain)"}, layout lint '
              f'{"OK" if not problems else f"{len(problems)} problems (finish refuses)"}; '
              f'finish with --status verify|partial|blocked')
        return 1 if problems else 0
    log = yamipan.WORK / 'build_relwithdebinfo.log'
    rc, out = build()
    for _ in range(3):
        if rc == 0 or 'undefined reference' not in out or re.search(r'\berror: (?!ld returned)', out):
            break
        r = run([sys.executable, 'stubgen.py', '--from-build', str(log)])
        if r.returncode:
            break
        rc, out = build()
    if rc:
        print(f'CHECK FAILED: build is red for {key}')
        return rc
    files = [yamipan.SRC / key] if (yamipan.SRC / key).exists() else []
    cc = run([sys.executable, 'callcheck.py', *map(str, files)]).returncode if files else 0
    lc = run([sys.executable, 'layoutcheck.py']).returncode
    if secs:
        # The smoke run's scripted pad (settings.smoke_pad: through the boot to the screen under work);
        # $YAMIPAN_AUTOPAD or --pad override it.
        pad = opt(args, '--pad') or os.environ.get('YAMIPAN_AUTOPAD') or M.db['settings'].get('smoke_pad')
        run([sys.executable, 'build.py', '--run', secs] + (['--pad', pad] if pad else []))
    print(f'check {key}: build OK, callcheck {"clean" if cc == 0 else "has MISSING calls (fix or explain)"}, '
          f'layoutcheck {"OK" if lc == 0 else "FAILED (asserts, missing asserts, host types or code cl rejects; finish refuses)"}')
    return 0


def cmd_finish(args):
    status = opt(args, '--status')
    summary = opt(args, '--summary', '')
    notes = opt(args, '--notes')
    coauthor = opt(args, '--coauthor', os.environ.get('YAMIPAN_COAUTHOR',
                                                      'Co-Authored-By: Claude <noreply@anthropic.com>'))
    no_commit = flag(args, '--no-commit')
    if status not in ('done', 'verify', 'partial', 'blocked') or not summary:
        raise SystemExit('finish needs --status done|verify|partial|blocked and --summary')
    branch = git('rev-parse', '--abbrev-ref', 'HEAD').strip()
    if not no_commit and branch in ('main', 'master'):
        raise SystemExit(f'refusing to commit on {branch}; work on a branch')
    M = Model()
    key = M.resolve(args[0])
    if has_build():
        rc, _ = build()
        if rc:
            raise SystemExit('build is red. Fix it, or revert the unfinished part (git checkout -- <files>) and '
                             'finish with --status blocked and notes saying what is missing.')
        if run([sys.executable, 'layoutcheck.py']).returncode:
            raise SystemExit('layoutcheck failed: a type does not keep its layout on GCC or MSVC, lacks its '
                             'static_asserts, uses a host-dependent type, or has code cl rejects, in the game, src/sdk or the '
                             'Pan layer (see its output).')
    else:
        # Source-only mode: nothing is compiled or seen, so 'done' (the picture is the test) is impossible.
        if status == 'done':
            raise SystemExit('source-only mode (no CMakeLists.txt): nothing was built or seen on screen; '
                             'finish with --status verify (or partial / blocked)')
        _, problems = source_only_check(key)
        if problems:
            raise SystemExit('layout lint failed (see above): add the static_asserts or a "layout: host" note')
    e = M.db['tus'].setdefault(key, {})
    shot = progress_shot(key, e.get('claimed'))
    e['status'] = status
    e.pop('claimed', None)
    e.pop('prev_status', None)
    if notes is not None:
        e['notes'] = notes
    e.setdefault('sessions', []).append({
        'date': time.strftime('%Y-%m-%d %H:%M'), 'status': status, 'summary': summary,
        **({} if has_build() else {'build': 'not built'}),
        **({'session': os.environ['YAMIPAN_SESSION_ID']} if os.environ.get('YAMIPAN_SESSION_ID') else {})})
    e['sessions'] = e['sessions'][-12:]
    if status in ('done', 'verify'):
        M.db['queue'] = [k for k in M.db['queue'] if k != key]
    M.save()
    r = run([sys.executable, 'refresh.py'])
    if r.returncode:
        raise SystemExit('refresh.py failed')
    if no_commit:
        print(f'{key}: {status} (not committed)')
        return 0
    git('add', '-A')
    diff = git('diff', '--cached', '-U0')
    rec, stubbed, path = [], [], ''
    for line in diff.splitlines():
        if line.startswith('+++'):
            path = line[4:]
        elif line.startswith('+') and path.endswith(KEY_SUFFIXES):
            for m in yamipan.ANNOT_RE.finditer(line):
                (stubbed if 'stub of' in line else rec).append(M.name(int(m.group(1), 16)))
    rec, stubbed = list(dict.fromkeys(rec)), list(dict.fromkeys(stubbed))
    title = f'{key}: ' + (f'reconstruct {len(rec)} function{"s" * (len(rec) != 1)}' if rec else 'session work') \
        + (f', stub {len(stubbed)}' if stubbed else '') + f' ({status})'
    body = [title, '', summary.strip(), '']
    if not has_build():
        body += ['Not built: source-only mode (no host toolchain or platform layer yet).', '']
    if rec:
        body += ['Reconstructed: ' + ', '.join(rec), '']
    if stubbed:
        body += ['Stubbed: ' + ', '.join(stubbed), '']
    if shot:
        body += [f'Screen: {shot}', '']
    if os.environ.get('YAMIPAN_SESSION_ID'):
        body += [f'Session: {os.environ["YAMIPAN_SESSION_ID"]}', '']
    body.append(coauthor)
    r = subprocess.run(['git', '-C', str(yamipan.REPO), 'commit', '-q', '-F', '-'], input='\n'.join(body) + '\n',
                       text=True, encoding='utf-8')
    if r.returncode:
        raise SystemExit('git commit failed')
    print(f'{key}: {status}; committed {git("rev-parse", "--short", "HEAD").strip()}: {title}')
    return 0


def cmd_mark(args):
    """Set another TU's state from what a run showed (a verify TU seen working -> done; a wrong picture ->
    partial), recorded in its session history; the session's finish commits it."""
    why = opt(args, '--why')
    if len(args) < 2 or args[1] not in ('done', 'verify', 'partial', 'blocked', 'todo') or not why:
        raise SystemExit('mark needs <tu> done|verify|partial|blocked|todo --why TEXT')
    M = Model()
    key = M.resolve(args[0])
    e = M.db['tus'].setdefault(key, {})
    old = e.get('status', 'todo')
    e['status'] = args[1]
    e.setdefault('sessions', []).append({'date': time.strftime('%Y-%m-%d %H:%M'), 'status': args[1],
                                         'summary': f'marked {old} -> {args[1]}: {why}'})
    e['sessions'] = e['sessions'][-12:]
    if args[1] in ('done', 'verify'):
        M.db['queue'] = [k for k in M.db['queue'] if k != key]
    M.save()
    print(f'{key}: {old} -> {args[1]}')
    return 0


COMMANDS = {'list': cmd_list, 'next': cmd_next, 'claim': cmd_claim, 'brief': cmd_brief, 'define': cmd_define,
            'queue': cmd_queue, 'check': cmd_check, 'finish': cmd_finish, 'mark': cmd_mark}

if __name__ == '__main__':
    if len(sys.argv) < 2 or sys.argv[1] not in COMMANDS:
        print(__doc__)
        sys.exit(2)
    sys.exit(COMMANDS[sys.argv[1]](sys.argv[2:]) or 0)
