"""Export FS2's current symbol state as <repo>/yamipan_functions.txt (MioPan functions.txt style).

python export.py

FS2 ships without symbols for .text, so source files are inferred, in this order of strength:
  1. pinned ranges (data/tus.json, `tus.py define`): exactly what a session decided;
  2. string anchors: a function that references one of FS2's own source paths ("rxfile.c", the
     __FILE__ of an assert or error print) is in that file;
  3. class anchors: a class's methods (Ghidra / section names 'Class::...'), vtable slots and ctors /
     dtors (rtti.py) are in the file most of its string-anchored functions are in;
  4. link order: a function between two anchors of the same file inherits it.
Library code (data/layout.json lib_ranges) is listed under its library object (libmatch.py), named
linkonce sections (satellites) under their class's file. Everything else lands in "(unassigned)".
Each line: /* addr size */ name(){}  // provenance. Names: Ghidra's, else the ELF's own section symbol.
Works offline (cached names) when Ghidra is not running.
"""
import json
import re
import sys
from bisect import bisect_right
from collections import Counter, defaultdict

import yamipan
import sections

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
# a source path alone ("rxfile.c", an assert's __FILE__) or leading a message ("rena_2d_texture_tim2.cpp: ...")
SRC_STRING = re.compile(r'^([\w./-]*\w\.(?:cpp|cc|c))(?:$|[:,(\s])')
SHORT = {'game-symbol': 'sym', 'lib-match': 'lib', 'lib-match-low': 'lib?', 'lib-context': 'lib-ctx',
         'game-inferred': 'game'}


def build():
    feat = {f['addr']: f for f in json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']}
    names = {int(a, 16): n for a, n in (yamipan.function_names() if yamipan.ghidra_up()
                                        else yamipan.cached_names()).items()}
    tags = {int(a, 16): t for a, t in yamipan.load_json(yamipan.WORK / 'fs2_tags.json', {}).items()}
    S = sections.load()
    size = {a: f['size'] * 4 for a, f in feat.items()}
    for f in S['functions']:                     # the ELF's own symbols, also where Ghidra has no function yet
        size.setdefault(f['addr'], f['size'])
        if yamipan.GENERIC_NAME.match(names.get(f['addr'], 'FUN_')):
            names[f['addr']] = f['name']
        tags.setdefault(f['addr'], 'game-symbol')
    for a in size:
        names.setdefault(a, f'FUN_{a:08x}')
    lib_file = {int(a, 16): m.get('file') for a, m in yamipan.load_json(yamipan.DATA / 'lib_matches.json', {}).items()}
    # without a libmatch.py file, a library function is listed under its layout.json range's library
    lib_label = [(int(lo, 16), int(hi, 16), what.split(':')[0].strip())
                 for lo, hi, what in yamipan.load_json(yamipan.LAYOUT_JSON, {}).get('lib_ranges', [])]
    addrs = sorted(size)

    # 2. string anchors
    anchor = {}
    for a in addrs:
        if yamipan.in_lib(a):
            continue
        for s in feat.get(a, {}).get('strings', []):
            m = SRC_STRING.match(s)
            if m:
                anchor[a] = m.group(1).removeprefix('./')
                break
    # 3. class anchors
    classes = yamipan.load_json(yamipan.WORK / 'classes.json', [])
    members = defaultdict(set)
    for a in addrs:
        if '::' in names[a]:
            members[names[a].split('::')[0]].add(a)
    # A class owns the slots it overrides (an inherited slot points at the base's method, in the base's
    # file), and the ctors / dtors that store its vtable.
    prim = {c['name']: [s['addr'] for s in c['vtables'][0]['slots']] for c in classes}
    for c in classes:
        base = prim.get(c['bases'][0], []) if c['bases'] else []
        for v in c['vtables']:
            members[c['name']].update(s['addr'] for k, s in enumerate(v['slots'])
                                      if s['addr'] and (v is not c['vtables'][0] or k >= len(base)
                                                        or base[k] != s['addr']))
            members[c['name']].update(int(u.rsplit('@', 1)[1], 16) for u in v['users'])
    class_file = {}
    for cls, ms in members.items():
        votes = Counter(anchor[a] for a in ms if a in anchor)
        if votes:
            class_file[cls] = votes.most_common(1)[0][0]
    for cls, fl in class_file.items():
        for a in members[cls]:
            if a in size and not yamipan.in_lib(a) and not yamipan.is_satellite(a):
                anchor.setdefault(a, fl)
    yamipan.save_json(yamipan.WORK / 'class_files.json', class_file)

    # 4. link-order fill between anchors of the same file (game code only)
    game = [a for a in addrs if not yamipan.in_lib(a) and not yamipan.is_satellite(a)]
    anchored = [a for a in game if a in anchor]
    file_of = {}
    for a in game:
        if a in anchor:
            file_of[a] = anchor[a]
            continue
        i = bisect_right(anchored, a)
        if 0 < i < len(anchored) and anchor[anchored[i - 1]] == anchor[anchored[i]]:
            file_of[a] = anchor[anchored[i - 1]]
        else:
            file_of[a] = None
    # 1. pinned ranges win, and a pinned TU is exactly its ranges
    pinned = yamipan.tu_ranges()
    pinned_keys = {key for _, _, key in pinned}
    for a in game:
        if file_of[a] in pinned_keys:
            file_of[a] = None
    for lo, hi, key in pinned:
        for a in game[bisect_right(game, lo - 1):bisect_right(game, hi - 1)]:
            file_of[a] = key
    for a in addrs:
        if yamipan.in_lib(a):
            file_of[a] = '(library) ' + (lib_file.get(a) or next((w for lo, hi, w in lib_label if lo <= a < hi), 'sdk'))
        elif yamipan.is_satellite(a):
            cls = names[a].split('::')[0] if '::' in names[a] else None
            file_of[a] = class_file.get(cls) or f'(satellite) {cls or "?"}'
    return addrs, size, names, tags, file_of, len(anchor)


def main():
    addrs, size, names, tags, file_of, n_anchor = build()
    out = ['// Forbidden Siren 2 (SCES_538.51) functions - generated by tools/reverse/export.py; do not edit by hand.',
           "// Source files: FS2's own __FILE__ strings, class membership and link order (export.py); pinned",
           '// ranges from tools/reverse/data/tus.json win. provenance: sym = the ELF\'s own section symbol,',
           '// lib / lib? = library match (high / low), lib-ctx = library named by reading, game = inferred FS2',
           '// name, (none) = unnamed. Library and satellite sections are marked in their FILE line.', '']
    prev = object()
    order = sorted(addrs, key=lambda a: (a,))
    for a in order:
        fl = file_of.get(a)
        if fl != prev:
            out += ['', '// ' + '*' * 77, f'// FILE -- {fl or "(unassigned)"}', '// ' + '*' * 77, '']
            prev = fl
        n = names[a]
        t = SHORT.get(tags.get(a), '' if yamipan.GENERIC_NAME.match(n) else 'native')
        out.append(f'/* {a:08x} {size[a]:08x} */ {n}(){{}}' + (f'  // {t}' if t else ''))
    yamipan.FUNCTIONS_TXT.write_text('\n'.join(out) + '\n', encoding='utf-8')
    game_files = {f for f in file_of.values() if f and not f.startswith('(')}
    named = sum(1 for a in addrs if not yamipan.GENERIC_NAME.match(names[a]))
    assigned = sum(1 for a in addrs if file_of.get(a) and not file_of[a].startswith('('))
    print(f'{yamipan.FUNCTIONS_TXT.name}: {len(addrs)} functions, {named} named, {assigned} game functions in '
          f'{len(game_files)} source files ({n_anchor} anchors)')


if __name__ == '__main__':
    main()
