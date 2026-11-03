"""Check reconstructed functions call what FS2 calls.

python callcheck.py [src files ...]        # default: all FS2 game code under src/

For each function definition carrying a `FS2 0x<addr>` annotation (the last one in the comment block
right above it, or on its first line), compares the identifiers called in the body with the callees FS2
has at that address (jal targets and tail jumps, by current name):
  missing  FS2 calls it, the reconstruction does not (or calls it under another name)
  extra    the reconstruction calls it, FS2 has no such callee
A callee matches under its Ghidra name's last component ('DogActor::update' -> update, an overload
'DogModel::setHide(bool)' -> setHide) or under the name of the source definition annotated with its
address, so method calls (`obj->update()`) need no comment. Calls the parser cannot see (implicit base
constructors / destructors, a member's destructor, the operator delete of a delete-expression, calls
through a vtable slot) are explained with a `// callcheck: Name1, Name2 (why)` comment in the block or
body -- names first, one parenthesis last, all on that line. Inlined helpers, macros and host-side calls
(YamiPan_*, printf, std::...) show up as extra; judge them, don't chase a zero count. Exit 1 when
anything is missing.
"""
import json
import re
import sys
from collections import Counter
from pathlib import Path

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

feat = {f['addr']: f for f in json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']}
funcs = yamipan.functions()
nm = lambda a: funcs.get(a, {}).get('name') or feat.get(a, {}).get('name') or f'FUN_{a:08x}'
KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'case', 'do', 'else', 'defined',
            'static_cast', 'reinterpret_cast', 'const_cast', 'dynamic_cast', 'alignof', 'decltype'}

files = [Path(p).resolve() if Path(p).exists() else yamipan.REPO / p for p in sys.argv[1:]] or \
    list(yamipan.game_sources())

# What precedes the name (return type, class qualifier) is optional, so a destructor (`T::~T()`: no
# return type, and `~` is not a name character) matches too, under its full name.
DEF_RE = re.compile(r'^(?:[A-Za-z_][\w\s\*&:<>,]*?\b)?([A-Za-z_]\w*(?:::~?\w+)*)\s*\(([^;{]*?)\)\s*(?:const\s*)?\n?\{',
                    re.M)
BRACE_RE = re.compile(r'[{}]')


def body_of(text, m):
    depth = 0
    for b in BRACE_RE.finditer(text, m.end() - 1):
        depth += 1 if b.group() == '{' else -1
        if depth == 0:
            return text[m.end() - 1:b.end()]
    return None


def definitions(text, stubs=False):
    """(match, comment block, FS2 address, body) for each annotated definition; stubs (a body calling
    YAMIPAN_STUB) are skipped unless asked."""
    for m in DEF_RE.finditer(text):
        before = text[:m.start()]
        cut = before.rfind('\n}')
        block = (before[cut + 2:] if cut >= 0 else before) + m.group(0)
        am = yamipan.ANNOT_RE.findall(block)
        body = body_of(text, m) if am else None
        if body is not None and (stubs or 'YAMIPAN_STUB' not in body):
            yield m, block, int(am[-1], 16), body


def short(name):
    """'DogModel::setHide(bool)' -> 'setHide', 'Class::~Class' -> '~Class'."""
    return re.sub(r'\(.*$', '', name).split('::')[-1]


src_names = {}
for path in yamipan.game_sources():
    for m, _, a, _ in definitions(path.read_text(encoding='utf-8', errors='replace'), stubs=True):
        src_names.setdefault(a, set()).add(m.group(1).split('::')[-1])

problems = checked = 0
for path in files:
    text = path.read_text(encoding='utf-8', errors='replace')
    for m, block, addr, body in definitions(text):
        explained = [n for e in re.findall(r'callcheck:\s*([\w, ~\[\]=]+)', block + body)
                     for n in re.split(r'[,\s]+', e) if n]
        body = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"', '', body, flags=re.S)
        src_calls = Counter(c.split('::')[-1] for c in re.findall(r'((?:(?<=[>.:])~)?\b[A-Za-z_][\w:]*)\s*\(', body)
                            if c[0] == '~' or c not in KEYWORDS and (not c.isupper() or c.startswith('FUN_')))
        src_calls.update(explained)
        callees = {}
        f = feat.get(addr, {})
        for c in f.get('calls', []) + f.get('tails', []):
            g = short(nm(c))
            callees.setdefault(g, {g}).update(src_names.get(c, ()))
        missing = sorted(n for n, names in callees.items() if not names & set(src_calls))
        extra = sorted(set(src_calls) - set().union(*callees.values()))
        checked += 1
        line = text.count('\n', 0, m.start()) + 1
        rel = path.relative_to(yamipan.REPO).as_posix()
        if missing or extra:
            print(f'{rel}:{line} {m.group(1)} (FS2 0x{addr:08x}) {"MISSING" if missing else "extra-only"}')
            if missing:
                print('    missing:', ', '.join(missing))
            if extra:
                print('    extra:  ', ', '.join(extra))
        problems += bool(missing)
print(f'checked {checked} annotated functions, {problems} with missing FS2 calls')
sys.exit(1 if problems else 0)
