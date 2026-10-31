"""Turn undefined references into YAMIPAN_STUB definitions so the build links.

python stubgen.py --from-build work/build_relwithdebinfo.log [--dry-run]
python stubgen.py Name1 Class::Method ...        [--dry-run]

For each function:
  - the declaration is found in src/ (headers first), giving the exact signature;
  - the FS2 address comes from the declaration's trailing `// FS2 0x...` comment, else from the current
    Ghidra names (yamipan_functions.txt: 'Name', or 'Class::Method' / its overload spelling);
  - the stub goes into the source file the address's TU is inferred to be (export.py), created if
    missing, including the declaring header and common/yamipan_stub.h.
Class members are stubbed when their class declares them; constructors, destructors, templates and data
symbols are listed for hand-writing.
"""
import re
import sys
from collections import defaultdict

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

dry = '--dry-run' in sys.argv
args = [a for a in sys.argv[1:] if not a.startswith('--')]
if '--from-build' in sys.argv:
    log = args.pop(0)
    text = open(log, encoding='utf-8', errors='replace').read()
    wanted = sorted(set(re.findall(r"undefined reference to [`'](.*?)'", text)))
else:
    wanted = args

funcs = yamipan.functions()
by_name = defaultdict(list)
for a, f in funcs.items():
    by_name[f['name']].append(a)
    if '(' in f['name']:                                  # overload spelling: also by its bare name
        by_name[f['name'].split('(')[0]].append(a)

sources = [p for p in yamipan.SRC.rglob('*') if p.suffix in ('.h', '.hpp', '.c', '.cpp')] if yamipan.SRC.exists() else []
sources.sort(key=lambda p: (p.suffix not in ('.h', '.hpp'), str(p)))
texts = {p: p.read_text(encoding='utf-8', errors='replace') for p in sources}
ANNOT = re.compile(r'FS2 0x([0-9a-fA-F]{6,8})\b')


def find_decl(name):
    # Parameters may hold one level of parentheses (array pointers: `float (*mat)[4]`). A statement such
    # as `return f(x);` looks like a declaration with return type `return`: skipped.
    pat = re.compile(r'^[ \t]*(?:extern\s+(?:"C"\s+)?)?([A-Za-z_][\w\s\*&:<>,]*?[\s\*&])' + re.escape(name)
                     + r'\s*\(((?:[^;{}()]|\([^;{}()]*\))*)\)\s*;([^\n]*)', re.M)
    for p in sources:
        for m in pat.finditer(texts[p]):
            ret = m.group(1).strip()
            if ret.split()[0] in ('return', 'else', 'case', 'goto', 'delete', 'throw'):
                continue
            am = ANNOT.findall(m.group(3))
            return p, ret, m.group(2).strip(), int(am[0], 16) if am else 0
    return None


def find_member_decl(cls, meth):
    """A member's declaration inside `class|struct cls { ... }`: (file, return type, parameters without
    default values, ' const' or '', FS2 address from the line's trailing `// FS2 0x...` or 0)."""
    pat = re.compile(r'^[ \t]*((?:(?:virtual|static|inline|explicit)\s+)*)([A-Za-z_][\w\s\*&:<>,]*?[\s\*&])'
                     + re.escape(meth) + r'\s*\(((?:[^;{}()]|\([^;{}()]*\))*)\)\s*(const)?\s*(?:override)?\s*'
                     r'(=\s*0\s*)?;([^\n]*)', re.M)
    short = cls.split('::')[-1]
    for p in sources:
        t = texts[p]
        for cm in re.finditer(r'\b(?:class|struct)\s+' + re.escape(short) + r'\b[^;{()]*\{', t):
            i, depth = cm.end() - 1, 0
            for j in range(i, len(t)):
                depth += (t[j] == '{') - (t[j] == '}')
                if depth == 0:
                    break
            for m in pat.finditer(t[i:j + 1]):
                ret = m.group(2).strip()
                if m.group(5) or ret.split()[0] in ('return', 'else', 'case', 'goto', 'delete', 'throw'):
                    continue
                params = re.sub(r'\s*=\s*[^,]+', '', m.group(3).strip())     # default arguments
                am = ANNOT.findall(m.group(6))
                return p, ret, params, ' const' if m.group(4) else '', int(am[0], 16) if am else 0
    return None


def owner_file(addr):
    return yamipan.src_path_for(funcs.get(addr, {}).get('file')) if addr else None


plan = defaultdict(list)
skipped = []
for sym in wanted:
    if '(' not in sym:
        skipped.append((sym, 'data symbol (a vtable or a global): define it by hand'))
        continue
    name = sym.split('(', 1)[0]
    if '::' in name:
        cls, meth = name.rsplit('::', 1)
        if '<' in name or meth == cls.split('::')[-1] or meth.startswith('~'):
            skipped.append((sym, 'template, constructor or destructor: write by hand'))
            continue
        decl = find_member_decl(cls, meth)
        if not decl:
            skipped.append((sym, f'no declaration of {meth} in class {cls} in src/'))
            continue
        where, ret, params, const, addr = decl
        if not addr:
            addrs = sorted(set(by_name.get(f'{cls}::{meth}', []) + by_name.get(cls + meth, [])))
            addr = addrs[0] if len(addrs) == 1 else 0
        target = where if where.suffix not in ('.h', '.hpp') else owner_file(addr)
        target = target or yamipan.SRC / 'lib' / 'unassigned_stubs.cpp'
        plan[target].append((name, ret, params, addr, where, 1, const))
        continue
    decl = find_decl(name)
    if not decl:
        skipped.append((sym, 'no declaration found in src/'))
        continue
    hdr, ret, params, addr = decl
    n = 1
    if not addr:
        addrs = sorted(set(by_name.get(name, [])))
        addr, n = (addrs[0] if len(addrs) == 1 else 0), len(addrs)
    target = owner_file(addr) or yamipan.SRC / 'lib' / 'unassigned_stubs.cpp'
    plan[target].append((name, ret, params, addr, hdr, n, ''))

for target, items in sorted(plan.items()):
    exists = target.exists()
    body = target.read_text(encoding='utf-8') if exists else ''
    add = []
    includes = ['common/yamipan_stub.h'] + sorted({h.relative_to(yamipan.SRC).as_posix() for *_, h, _, _ in items
                                                   if h.suffix in ('.h', '.hpp')})
    head = [f'#include "{i}"' for i in includes if f'"{i}"' not in body]
    code = re.sub(r'//[^\n]*|/\*.*?\*/', '', body, flags=re.S)      # comments mention names too
    for name, ret, params, addr, hdr, n, suffix in items:
        if re.search(r'\b' + re.escape(name) + r'\s*\((?:[^;(){}]|\([^;(){}]*\))*\)\s*(?:const\s*)?\{', code):
            continue                                            # already defined here
        note = f'FS2 0x{addr:08x}' if addr else f'FS2 address unknown ({n} candidates)'
        ret_stmt = '' if ret.strip() == 'void' else '\n    return {};'
        add.append(f'\n// stub of {note}\n{ret} {name}({params}){suffix}\n{{\n'
                   f'    YAMIPAN_STUB("{name}", 0x{addr:08x});{ret_stmt}\n}}\n')
    if not add:
        continue
    rel = target.relative_to(yamipan.REPO).as_posix()
    print(f'{rel}: {len(add)} stubs' + ('' if exists else ' (new file)'))
    if dry:
        continue
    target.parent.mkdir(parents=True, exist_ok=True)
    if not exists:
        body = (f'// {rel[len("src/"):]} -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).\n'
                '// Functions below are stubs until reconstructed; see tools/reverse/stage.py.\n\n')
    if head:
        body = body.replace('\n\n', '\n' + '\n'.join(head) + '\n\n', 1) if exists and '#include' in body \
            else body + '\n'.join(head) + '\n'
    if '// ---- stubs (tools/reverse/stubgen.py)' not in body:
        body += '\n// ---- stubs (tools/reverse/stubgen.py) ----\n'
    target.write_text(body + ''.join(add), encoding='utf-8')

for sym, why in skipped:
    print(f'skipped {sym}: {why}')
