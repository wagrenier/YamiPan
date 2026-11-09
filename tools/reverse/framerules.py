"""framerules.py [src] [out.json]: candidate frame-counted rules in the game layer, function by function.

A function that reads the clock (getDeltaSeconds / getSeconds / a dt parameter) is time-based; in the others, lines
that change state by a fixed amount are candidates: counters (++ / -- / += 1 on a member, a field or a global),
fixed steps (+= / -= / *= a constant or a member that is not a time), eases (approach*, lerp*), random rolls, and
constants whose names say frames or steps. A grep finds candidates, not rules: the triage is by reading, and the
read list is .claude/skills/yamipan-port/references/framerate-inventory.md. Lines written in other shapes (a counter
kept in a local and stored back, a step inside a helper) are missed. Writes work/framerules.json (per function: file,
address, name, whether it reads the clock, the candidate lines with their kind) and prints a count per file.
"""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parent.parent / "src"
OUT = Path(sys.argv[2]) if len(sys.argv) > 2 else HERE / "work" / "framerules.json"
MARK = re.compile(r'^// FS2 0x([0-9a-f]{8})')
TIME = re.compile(r'getDeltaSeconds|getSeconds|\bdt\b|deltaSeconds|\bseconds\b|stageFps|scriptFrames(From|To)30|framesPerSecond')
STATE = r'(?:(?:\w+(?:\[[^\]]*\])?(?:\.|->))*(?:m_\w+|g[A-Z]\w*|s[A-Z]\w*|\w+->\w+|\w+\.\w+)(?:\[[^\]]*\])?)'
KINDS = [
    ('counter', re.compile(r'(?:' + STATE + r'\s*(?:\+\+|--))|(?:(?:\+\+|--)\s*' + STATE + r')|(?:' + STATE + r'\s*[+-]=\s*1\b(?!\.))')),
    ('step', re.compile(STATE + r'\s*[+-]=\s*(?!1\b)[^;]+;')),
    ('scale', re.compile(STATE + r'\s*\*=\s*[^;]+;')),
    ('self', re.compile(r'(' + STATE + r')\s*=\s*\(*\s*\1\s*[-+*]')),
    ('ease', re.compile(r'\b(approach\w*|lerp\w*|ease\w*|chase\w*|damp\w*)\s*\(', re.I)),
    ('random', re.compile(r'\b(randNext|random|randFloat|randRange|rand48\w*|rand)\s*\(')),
    ('frames-const', re.compile(r'\bk\w*(Frames?|PerFrame|Step|Speed|Wait|Delay|Blink|Life)\w*\b')),
]
SKIP_LINE = re.compile(r'^\s*(//|\*|/\*)|callcheck|\bfor\s*\(')

out = []
for path in sorted(list(ROOT.glob('game/*.cpp')) + list(ROOT.glob('rena/*.cpp')) + list(ROOT.glob('sound/*.cpp'))):
    lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
    marks = [i for i, l in enumerate(lines) if MARK.match(l)]
    for n, start in enumerate(marks):
        end = marks[n + 1] if n + 1 < len(marks) else len(lines)
        body = lines[start:end]
        name = ''
        for l in body[1:12]:
            if '(' in l and not l.lstrip().startswith('//'):
                name = l.strip()
                break
        timed = any(TIME.search(l) for l in body if not SKIP_LINE.match(l))
        hits = []
        for k, l in enumerate(body):
            if SKIP_LINE.match(l):
                continue
            code = l.split('//')[0]
            for kind, rx in KINDS:
                if rx.search(code):
                    hits.append({'line': start + k + 1, 'kind': kind, 'text': code.strip()[:140]})
                    break
        if hits:
            out.append({'file': str(path.relative_to(ROOT)).replace('\\', '/'), 'addr': MARK.match(lines[start]).group(1),
                        'name': name[:120], 'timed': timed, 'hits': hits})

OUT.parent.mkdir(parents=True, exist_ok=True)
json.dump(out, open(OUT, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
files = {}
for f in out:
    s = files.setdefault(f['file'], [0, 0, 0])
    s[0] += 1
    s[1] += len(f['hits'])
    s[2] += f['timed']
print(f"{len(out)} functions with candidates in {len(files)} files, {sum(len(f['hits']) for f in out)} lines")
for name, (fn, hits, timed) in sorted(files.items(), key=lambda x: -x[1][1]):
    print(f"{hits:5} lines {fn:4} functions ({timed} timed)  {name}")
