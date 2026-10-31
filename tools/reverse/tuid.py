"""Is this TU really the file its key says? Evidence from several independent signals.

python tuid.py <TU>        (tus.py brief includes the same section)

Most FS2 TU names come from the game's own __FILE__ strings, which are facts; but a run is only
anchored where a function references its file's string, and link-order fill or class membership can
carry a neighbour's name across a file boundary. Before settling a TU's extent and name, weigh:

  1. strings     source paths the functions reference (decisive: the file's own asserts / error prints),
                 and the other strings (what the code prints, loads, compares)
  2. classes     the classes whose names, vtable slots or ctors / dtors are in it (rtti.py), and the file
                 those classes were placed in
  3. names       class / prefix of the functions already named in it
  4. neighbours  which TUs call into it and which it calls (a module talks mostly to its own kind)

and pick a descriptive name (in the directory of FS2's own files it talks to) when they disagree.
"""
import re
import sys
from collections import Counter

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
SRC_STRING = re.compile(r'^[\w./-]*\w\.(?:cpp|cc|c|h|hpp)$')


def _prefix(name):
    if name.startswith(('FUN_', 'thunk_')) or not name:
        return None
    if '::' in name:
        return name.split('::')[0] + '::'
    m = re.match(r'([A-Z0-9][a-z0-9]*[A-Z]?[a-z0-9]*|[a-z][a-z0-9]*[A-Z]?[a-z0-9]*)', name)
    return m.group(1) if m else None


def evidence(M, key):
    """Markdown lines weighing whether TU `key` is the file its key says."""
    members = [a for a in M.members.get(key, []) if not yamipan.is_satellite(a)]
    if not members:
        return []
    L = []
    w = L.append
    w(f'## Is it really `{key}`? (identity evidence -- anchors are facts, the fill between them is a guess)')
    w('')
    strs = Counter()
    for a in members:
        for s in M.feat.get(a, {}).get('strings', []):
            strs[s] += 1
    src = sorted(s for s in strs if SRC_STRING.match(s))
    anchored = sum(1 for a in members if any(SRC_STRING.match(s) and not s.endswith(('.h', '.hpp'))
                                             for s in M.feat.get(a, {}).get('strings', [])))
    if src:
        w(f'- source-path strings ({anchored} of {len(members)} functions anchored): '
          + ', '.join(f'"{s}" x{strs[s]}' for s in src[:8]))
    else:
        w(f'- source-path strings: none in {len(members)} functions (the name came from classes or link order)')
    other = [s for s, _ in strs.most_common(40) if s not in src and len(s) > 1][:12]
    if other:
        w('- strings: ' + ', '.join(f'"{s[:40]}"' for s in other))
    classes = yamipan.load_json(yamipan.WORK / 'classes.json', [])
    class_file = yamipan.load_json(yamipan.WORK / 'class_files.json', {})
    mine = set(members)
    here = Counter()
    for c in classes:
        for v in c['vtables']:
            here[c['name']] += sum(1 for s in v['slots'] if s['addr'] in mine)
            here[c['name']] += sum(1 for u in v['users'] if int(u.rsplit('@', 1)[1], 16) in mine)
    for a in members:
        if '::' in M.name(a):
            here[M.name(a).split('::')[0]] += 1
    if here:
        w('- classes: ' + ', '.join(f'{c} x{n}' + (f' (placed in {class_file[c]})' if c in class_file else '')
                                    for c, n in here.most_common(8) if n))
    pre = Counter(p for p in (_prefix(M.name(a)) for a in members) if p)
    if pre:
        w('- names: ' + ', '.join(f'{p} x{n}' for p, n in pre.most_common(8))
          + f' ({sum(1 for a in members if M.name(a).startswith("FUN_"))} unnamed)')
    callers = Counter(M.tu_of.get(c, '?') for a in members for c in M.callers.get(a, []) if M.tu_of.get(c) != key)
    callees = Counter(M.tu_of.get(c, 'lib') for a in members for c in M.feat.get(a, {}).get('calls', [])
                      if M.tu_of.get(c, 'lib') != key and not yamipan.in_lib(c))
    if callers:
        w('- called from: ' + ', '.join(f'{k} x{n}' for k, n in callers.most_common(5)))
    if callees:
        w('- calls into: ' + ', '.join(f'{k} x{n}' for k, n in callees.most_common(5)))
    w('')
    w('If these point elsewhere than the key -- another file\'s source string, classes placed in another '
      'file, callers from one other module only -- the boundary or the name is wrong: pin the real extent '
      'with `tus.py define <file> <lo>-<hi> --why "<the evidence>"` and say which signals decided.')
    w('')
    return L


def main(argv):
    import tus
    M = tus.Model()
    key = M.resolve(argv[0])
    print('\n'.join(evidence(M, key)) or f'{key}: no functions')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
