"""Name FS2's library and runtime code from symbolised reference ELFs (offline plan, Ghidra apply).

python libmatch.py                 plan: work/libmatch.json and a summary (read-only)
python libmatch.py --list [FILTER] the plan, one line per match
python libmatch.py --ranges        print the library ranges the matches imply (for data/layout.json)
python libmatch.py --apply         create missing functions, rename, tag lib-match / lib-match-low, save,
                                   record data/lib_matches.json, then refresh.py

FS2 shares no game code with anything symbolised, but its Sony libraries are prebuilt objects: the
SDK 3.0 libraries are the versions FF3 (SLES_538.25, named in ReiPan) and RoR (SLUS_214.48, named in
RosePan) link. FS2 was built with Metrowerks CodeWarrior, so its runtime (MSL C / C++, CodeWarrior's
support code) has no symbolised reference here: the ee-gcc runtime in FF2's debug build (SLES_523.84)
is unlikely to match. Object code of the same library version is byte-identical apart from
relocations, so each reference function is compared word for word with the relocated fields masked:
jal/j targets, lui immediates, and the %lo / gp-relative immediates of instructions whose base register
came from a lui (or is gp). Candidate starts are Ghidra's function starts, the named linkonce sections,
and every address after a `jr ra` + delay slot (functions Ghidra never found).

Tiers: lib-match = a unique masked match of >= 6 instructions that ends on a function boundary;
lib-match-low = 3..5 instructions, or a name both references disagree on (a hint: check the body).
A pattern that occurs more than once in FS2 is skipped (identical small helpers).
"""
import json
import re
import struct
import sys
from collections import defaultdict

import gcc2
import yamipan
from elf32 import Elf
from features import decode

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

PLAN = yamipan.WORK / 'libmatch.json'
APPLIED = yamipan.DATA / 'lib_matches.json'
MIN_WORDS = 3
HIGH_WORDS = 6
IMM_OPS = {9, 25, 13, 32, 33, 34, 35, 36, 37, 39, 40, 41, 43, 49, 55, 57, 63, 30, 31}
FF3_LIB_RANGES = ((0x100000, 0x100388), (0x273a30, 0x2ab000))     # crt0; Sony libs, libgcc, newlib (ReiPan)
DEBUG_ONLY = re.compile(r'_fixed_array|SetAssertPreMessage|PrintAssert|_SetLineInfo|type_info_function')
# Class-qualified FF2 symbols in the SDK region are FF2's own templates / classes (ctl::, CEditRoot), except
# the ee-gcc 2.9x C++ runtime's.
RUNTIME_CLASSES = {'type_info', '__user_type_info', '__si_type_info', '__class_type_info', '__builtin_type_info',
                   '__attr_type_info', '__func_type_info', '__ptr_type_info', '__ptmf_type_info',
                   '__ptmd_type_info', '__array_type_info', '__pointer_type_info', 'exception', 'bad_exception',
                   'bad_alloc', 'bad_cast', 'bad_typeid'}


def masked(words):
    """The words with relocated fields cleared; word k depends only on words[:k+1]."""
    out, hi = [], set()
    for w in words:
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        if op in (2, 3):
            out.append(w & 0xfc000000)
            continue
        if op == 15:
            out.append(w & 0xffff0000)
            hi.add(rt)
            continue
        if op in IMM_OPS and (rs == 28 or rs in hi):
            out.append(w & 0xffff0000)
            if op in (9, 25, 13) and rt not in (0, 28):
                hi.add(rt)                # the full address now lives in rt
            continue
        out.append(w)
        _, _, dest = decode(w)
        if dest is not None:
            hi.discard(dest)
    return out


def words_at(elf, addr, n):
    raw = elf.read(addr, 4 * n) or b''
    return list(struct.unpack('<%dI' % (len(raw) // 4), raw[:len(raw) // 4 * 4]))


def ff3_reference():
    """[(addr, size, name, file)] of FF3's named library functions (ReiPan's ff3_functions.txt)."""
    path = yamipan.REFERENCES[0][2]
    out = []
    if not path.exists():
        return out
    for line in path.read_text(encoding='utf-8').splitlines():
        m = re.match(r'/\* ([0-9a-f]{8}) ([0-9a-f]{8}) \*/ (\S+?)\(\)\{\}', line)
        if not m:
            continue
        a, size, name = int(m.group(1), 16), int(m.group(2), 16), m.group(3)
        if yamipan.GENERIC_NAME.match(name) or not any(lo <= a < hi for lo, hi in FF3_LIB_RANGES):
            continue
        out.append((a, size, name, None))
    return out


def _ff2_symbols():
    """[(addr, size, name)] from the readelf symbol table at the end of MioPan's symbols.txt."""
    path = yamipan.MIOPAN / 'symbols.txt'
    out = []
    if path.exists():
        for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
            m = re.match(r'\s*\d+: ([0-9a-f]{8})\s+(\d+) FUNC\s+\w+\s+\w+\s+\d+ (\S+)$', line)
            if m:
                out.append((int(m.group(1), 16), int(m.group(2)), m.group(3)))
    return out


def ff2_reference():
    """[(addr, size, name, file)] of FF2's library / runtime functions: its symbol table, limited to
    crt0 and the SDK region (from the first non-game object after the game's, libexcep.c 0x27a998):
    FF2's game objects (/zero2np/ paths, and helpers without debug info among them) are not libraries."""
    functions = yamipan.MIOPAN / 'functions.txt'
    file_of, cur = {}, None
    if functions.exists():
        for line in functions.read_text(encoding='utf-8', errors='replace').splitlines():
            if line.startswith('// FILE -- '):
                cur = line[len('// FILE -- '):].strip()
            elif (m := re.match(r'/\* ([0-9a-f]{8}) ', line)):
                file_of[int(m.group(1), 16)] = cur

    def game(f):
        return bool(f) and ('/zero2np/' in f or '/zero_rom/' in f)
    cutoff = min((a for a, f in file_of.items() if a > 0x101000 and f and not game(f)), default=0)
    out, seen = [], set()
    for a, size, name in _ff2_symbols():
        f = file_of.get(a)
        lib = a >= cutoff or (f or '').endswith('crt0.s')
        if not lib or game(f) or a in seen or not size or DEBUG_ONLY.search(name):
            continue
        d = gcc2.demangle(name)
        if d and d.qual and d.qual.split('<')[0] not in RUNTIME_CLASSES and not d.qual.startswith('std'):
            continue
        seen.add(a)
        out.append((a, size, name, f.rsplit('/', 1)[-1] if f else None))
    return out


def ref_names(label):
    """{addr: Ghidra spelling} of every function a reference names (callee names for disambiguation)."""
    if label == 'ff2':
        return {a: ghidra_spelling(n) for a, _, n in _ff2_symbols()}
    path = yamipan.REFERENCES[0][2]
    out = {}
    if path.exists():
        for line in path.read_text(encoding='utf-8').splitlines():
            m = re.match(r'/\* ([0-9a-f]{8}) [0-9a-f]{8} \*/ (\S+?)\(\)\{\}', line)
            if m and not yamipan.GENERIC_NAME.match(m.group(2)):
                out[int(m.group(1), 16)] = m.group(2)
    return out


def callee_names(elf, a, n, names):
    """Names (or None) of the jal targets in [a, a + 4n), in order."""
    out = []
    for k, w in enumerate(words_at(elf, a, n)):
        if w >> 26 == 3:
            out.append(names.get(((a + 4 * k) & 0xf0000000) | ((w & 0x3ffffff) << 2)))
    return out


def ghidra_spelling(name):
    d = gcc2.demangle(name)
    return gcc2.ghidra_name(d) if d and d.kind != 'method' or (d and d.qual) else name


def candidates(elf, feat):
    """FS2 addresses where a function may start."""
    text = [s for s in elf.sections if s['flags'] & 4 and s['addr'] and s['name'] != '.vutext']
    starts = {f['addr'] for f in feat}
    for s in elf.sections:
        if s['name'].startswith('.gnu.linkonce.t.'):
            starts.add(s['addr'])
    for s in text:
        ws = words_at(elf, s['addr'], s['size'] // 4)
        k = 0
        while k < len(ws) - 1:
            if ws[k] == 0x03e00008:                                  # jr ra
                j = k + 2
                while j < len(ws) and ws[j] == 0:
                    j += 1
                if j < len(ws):
                    starts.add(s['addr'] + 4 * j)
                k = j
            else:
                k += 1
    return sorted(starts), text


def plan():
    elf = Elf(str(yamipan.ELF))
    feat = json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']
    names = {f['addr']: f['name'] for f in feat}
    starts, text = candidates(elf, feat)
    start_set = set(starts)

    def sec_end(a):
        for s in text:
            if s['addr'] <= a < s['addr'] + s['size']:
                return s['addr'] + s['size']
        return a

    refs, ref_elf = [], {}
    for label, elf_path, _, _ in yamipan.REFERENCES:
        if not elf_path.exists():
            print(f'reference {label}: {elf_path} missing, skipped')
            continue
        relf = ref_elf[label] = Elf(str(elf_path))
        rows = ff3_reference() if label == 'ff3' else ff2_reference()
        n = 0
        for a, size, name, fl in rows:
            ws = words_at(relf, a, size // 4)
            while len(ws) > MIN_WORDS and ws[-1] == 0:
                ws.pop()
            if len(ws) < MIN_WORDS:
                continue
            refs.append({'ref': label, 'ref_addr': a, 'name': name, 'file': fl, 'pat': tuple(masked(ws))})
            n += 1
        print(f'reference {label}: {n} library functions ({elf_path.name})')
    index = defaultdict(list)
    for r in refs:
        index[r['pat'][:MIN_WORDS]].append(r)
    longest = max((len(r['pat']) for r in refs), default=0)

    hits = defaultdict(list)            # FS2 addr -> [ref]
    occurrences = defaultdict(int)      # (ref, ref_addr) -> count
    for c in starts:
        avail = min(longest, (sec_end(c) - c) // 4)
        if avail < MIN_WORDS:
            continue
        mw = masked(words_at(elf, c, avail))
        for r in index.get(tuple(mw[:MIN_WORDS]), ()):
            n = len(r['pat'])
            if n <= len(mw) and tuple(mw[:n]) == r['pat']:
                hits[c].append(r)
                occurrences[(r['ref'], r['ref_addr'])] += 1
    # The longest unique body wins at each start; FF2's real symbols are preferred over the names ReiPan
    # gave FF3's libraries, which cover the SDK 3.0 code FF2 does not have.
    cands = {}
    for c, rs in hits.items():
        rs = [r for r in rs if occurrences[(r['ref'], r['ref_addr'])] == 1]
        if rs:
            best_len = max(len(r['pat']) for r in rs)
            rs = [r for r in rs if len(r['pat']) == best_len]
            rs.sort(key=lambda r: (r['ref'] != 'ff2', r['ref_addr']))
            cands[c] = rs
    resolved = {c: ghidra_spelling(rs[0]['name']) for c, rs in cands.items()
                if len({ghidra_spelling(r['name']) for r in rs}) == 1}
    rnames = {label: ref_names(label) for label in ref_elf}
    named = {s['addr'] for s in elf.sections if s['name'].startswith('.gnu.linkonce.t.')}
    out = []
    for c, rs in sorted(cands.items()):
        if c in named:
            continue                    # the ELF's own symbol wins (sections.py)
        best_len = len(rs[0]['pat'])
        spellings = {ghidra_spelling(r['name']) for r in rs}
        pick, disputed = rs[0], len(spellings) > 1
        if disputed:
            # identical bodies under several names (wrappers whose jal targets were masked): the name whose
            # reference callees agree with what FS2's callees were already matched to
            ror_calls = callee_names(elf, c, best_len, resolved)

            def score(r):
                ref_calls = callee_names(ref_elf[r['ref']], r['ref_addr'], best_len, rnames[r['ref']])
                agree = sum(1 for x, y in zip(ref_calls, ror_calls) if x and y and x == y)
                clash = sum(1 for x, y in zip(ref_calls, ror_calls) if x and y and x != y)
                return agree - 2 * clash
            scored = sorted(rs, key=lambda r: -score(r))
            if score(scored[0]) > 0 and (len(scored) == 1 or score(scored[0]) > score(scored[1])):
                pick, disputed = scored[0], False
        end = c + 4 * best_len
        boundary = end in start_set or (elf.u32(end) or 0) == 0 or end >= sec_end(c)
        tier = 'lib-match' if best_len >= HIGH_WORDS and boundary and not disputed else 'lib-match-low'
        out.append({'addr': f'{c:08x}', 'size': 4 * best_len, 'name': ghidra_spelling(pick['name']),
                    'symbol': pick['name'], 'ref': pick['ref'], 'ref_addr': f'{pick["ref_addr"]:08x}',
                    'file': next((r['file'] for r in rs if r['file']), None), 'tier': tier,
                    'also': sorted(spellings - {ghidra_spelling(pick['name'])}),
                    'current': names.get(c), 'boundary': boundary})
    yamipan.save_json(PLAN, out)
    have = sum(1 for m in out if m['current'] is not None)
    renames = sum(1 for m in out if m['current'] and yamipan.GENERIC_NAME.match(m['current']))
    print(f'{len(out)} matches ({sum(m["tier"] == "lib-match" for m in out)} high, '
          f'{sum(m["tier"] == "lib-match-low" for m in out)} low): {have} at existing Ghidra functions '
          f'({renames} still FUN_), {len(out) - have} need a function created -> {PLAN.relative_to(yamipan.REPO)}')
    return out


def ranges(out):
    """Contiguous runs of matched library code (gaps of unmatched functions up to 8 allowed)."""
    feat = json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']
    allstarts = sorted({f['addr'] for f in feat} | {int(m['addr'], 16) for m in out})
    lib = {int(m['addr'], 16) for m in out}
    runs, cur, gap = [], None, 0
    for a in allstarts:
        if a in lib:
            if cur is None:
                cur = [a, a]
            cur[1], gap = a, 0
        elif cur is not None:
            gap += 1
            if gap > 8:
                runs.append(cur)
                cur, gap = None, 0
    if cur:
        runs.append(cur)
    size = {int(m['addr'], 16): m['size'] for m in out}
    for lo, hi in runs:
        n = sum(1 for a in lib if lo <= a <= hi)
        tot = sum(1 for a in allstarts if lo <= a <= hi)
        files = sorted({m['file'] or '?' for m in out if lo <= int(m['addr'], 16) <= hi})
        print(f'  {lo:08x}..{hi + size.get(hi, 4):08x}  {n}/{tot} matched  {", ".join(files)[:150]}')


def apply(out):
    if not yamipan.ghidra_up():
        raise SystemExit(f'no GhidraMCP window has {yamipan.PROGRAM} active (python ghidra_mcp.py which)')
    yamipan.ensure_tags()
    current = yamipan.function_names()
    taken = defaultdict(list)
    for a, n in current.items():
        taken[n].append(a)
    applied = yamipan.load_json(APPLIED, {})
    done, kept, failed = 0, [], []
    for m in out:
        a, ai = m['addr'], int(m['addr'], 16)
        if a not in current:
            if not yamipan.create_function(ai):
                failed.append((a, m['name'], 'could not create a function'))
                continue
            current[a] = f'FUN_{a}'
        now = current[a]
        if now != m['name'] and not yamipan.GENERIC_NAME.match(now):
            kept.append((a, now, m['name']))            # Ghidra's own (syscall) or a hand name stays
            continue
        if now != m['name']:
            if [x for x in taken.get(m['name'], []) if x != a]:
                failed.append((a, m['name'], 'name used at ' + ','.join(taken[m['name']])))
                continue
            err = yamipan.rename(ai, m['name'])
            if err:
                failed.append((a, m['name'], err))
                continue
            taken[m['name']].append(a)
        yamipan.set_tag(ai, m['tier'])
        applied[a] = {k: m[k] for k in ('name', 'symbol', 'ref', 'ref_addr', 'file', 'tier', 'size')}
        done += 1
    yamipan.save_json(APPLIED, applied)
    print(f'applied {done}/{len(out)}; kept existing names {len(kept)}; failed {len(failed)}')
    for x in kept[:40]:
        print('  kept', *x)
    for x in failed:
        print('  FAILED', *x)
    print(yamipan.save_program()[:120])
    import subprocess
    subprocess.run([sys.executable, str(yamipan.TOOLS / 'refresh.py')], cwd=yamipan.TOOLS)


if __name__ == '__main__':
    args = sys.argv[1:]
    if '--list' in args and PLAN.exists():
        flt = args[args.index('--list') + 1] if len(args) > args.index('--list') + 1 else ''
        for m in yamipan.load_json(PLAN, []):
            line = (f'{m["addr"]} {m["size"]:#6x} {m["tier"]:13} {m["name"]:36} {m["ref"]}@{m["ref_addr"]} '
                    f'{m["file"] or "":18} now {m["current"]}' + (f' also {m["also"]}' if m['also'] else ''))
            if flt in line:
                print(line)
        sys.exit(0)
    result = plan()
    if '--ranges' in args:
        ranges(result)
    if '--apply' in args:
        apply(result)
