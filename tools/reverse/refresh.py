"""Regenerate every derived artifact in one go.

python refresh.py [--no-ghidra]

1. fetch.py     Ghidra -> work/ (skipped when Ghidra is not reachable or --no-ghidra)
2. features.py  only when fetch ran (the function list may have changed)
3. sections.py  work/sections.json (the ELF's own symbols; offline)
4. rtti.py      work/classes.json (class map with current slot names)
5. export.py    yamipan_functions.txt (file inference uses classes.json)
6. status.py    .claude/skills/yamipan-port/references/status.md

Run it after any naming batch, Ghidra fix or reconstruction step, before committing.
"""
import subprocess
import sys
from pathlib import Path

import yamipan

HERE = Path(__file__).resolve().parent


def run(script, *args):
    print(f'== {script}', flush=True)
    r = subprocess.run([sys.executable, str(HERE / script), *args], cwd=HERE)
    if r.returncode:
        raise SystemExit(f'{script} failed ({r.returncode})')


if '--no-ghidra' not in sys.argv and yamipan.ghidra_up():
    run('fetch.py')
    run('features.py')
else:
    print('== Ghidra not reachable (or --no-ghidra): using cached work/ data')
run('sections.py', '--quiet')
run('rtti.py', '--quiet')
run('export.py')
run('status.py')
