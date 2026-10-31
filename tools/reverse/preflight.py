"""Preflight and run lock for unattended runs (the scheduled routine, tools/reverse/routine.md):

    python preflight.py             check, then take the run lock; one line; exit 0 when the run may go
    python preflight.py --release   drop the run lock (the run's last step, also after a failure)

The checks, in order; the first that fails stops the run and nothing is changed:

  - work/STOP does not exist (the user's stop switch);
  - work/autopilot.lock does not exist (a terminal autopilot.py is running in this checkout);
  - work/run.lock is not held. Younger than LOCK_HOURS, the previous run is still going: exit 3, and
    the routine stops quietly. Older, a run probably died holding it: exit 1 (look, then delete it);
  - the tree is clean (`git status --porcelain`): a run mid-step, someone working, or a crashed run;
  - HEAD is a work branch (not main / master / a detached HEAD): commits never land on main;
  - Ghidra serves YamiPan's program (ghidra_mcp.up(): a CodeBrowser window has it as the active tab).

Then it creates work/run.lock with O_EXCL, so two runs starting together cannot both take it. The lock
is in work/ (gitignored): it never makes the tree dirty. Exit codes: 0 go, 1 a check failed, 3 the
previous run is still going.
"""
import argparse
import json
import os
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

import ghidra_mcp

TOOLS = Path(__file__).resolve().parent
REPO = TOOLS.parent.parent
WORK = TOOLS / 'work'
STOP = WORK / 'STOP'
AUTOPILOT_LOCK = WORK / 'autopilot.lock'
RUN_LOCK = WORK / 'run.lock'
# A TU at effort xhigh can take a few hours, more when a usage limit pauses it: past this, the lock is
# taken for a crashed run's.
LOCK_HOURS = 6
GO, FAILED, BUSY = 0, 1, 3


def git(*args):
    r = subprocess.run(['git', *args], cwd=REPO, capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else None


def check():
    """(exit code, one line)."""
    if STOP.exists():
        return FAILED, f'{STOP} exists: the user stopped the runs (delete it to resume)'
    if AUTOPILOT_LOCK.exists():
        return FAILED, f'{AUTOPILOT_LOCK} exists: autopilot.py is running in this checkout'
    if RUN_LOCK.exists():
        hours = (time.time() - RUN_LOCK.stat().st_mtime) / 3600
        if hours < LOCK_HOURS:
            return BUSY, f'previous run still going (run.lock taken {hours * 60:.0f} min ago)'
        return FAILED, (f'{RUN_LOCK} is {hours:.1f} h old: a run probably died holding it. Look at git status '
                        f'and the last run, then delete the lock')
    dirty = git('status', '--porcelain')
    if dirty is None:
        return FAILED, f'git status failed in {REPO}'
    if dirty:
        first = dirty.splitlines()[0]
        return FAILED, (f'the tree is dirty ({len(dirty.splitlines())} paths, first: {first.strip()}): a run '
                        f'mid-step, someone working, or a run that crashed')
    branch = git('rev-parse', '--abbrev-ref', 'HEAD')
    if branch in (None, '', 'HEAD', 'main', 'master'):
        return FAILED, f'on {branch or "no branch"}: check out the work branch (fs2-port) first'
    if not ghidra_mcp.up():
        try:
            ghidra_mcp.find(cache=False)
            why = 'it stopped answering'
        except ghidra_mcp.GhidraError as e:
            why = str(e).splitlines()[0]
        return FAILED, f'Ghidra does not serve {ghidra_mcp.PROGRAM}: {why}'
    return GO, f'go: {branch}, clean, Ghidra serves {ghidra_mcp.PROGRAM}'


def acquire(line):
    WORK.mkdir(parents=True, exist_ok=True)
    try:
        fd = os.open(RUN_LOCK, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    except FileExistsError:
        return BUSY, 'previous run still going (run.lock appeared while checking)'
    with os.fdopen(fd, 'w', encoding='utf-8') as f:
        json.dump({'started': datetime.now().isoformat(timespec='seconds'), 'pid': os.getpid(),
                   'head': git('rev-parse', '--short', 'HEAD')}, f)
    return GO, line + '; run lock taken'


def release():
    try:
        RUN_LOCK.unlink()
        return GO, 'run lock released'
    except FileNotFoundError:
        return GO, 'no run lock to release'


def main():
    ap = argparse.ArgumentParser(description='Preflight and run lock for unattended YamiPan runs.')
    ap.add_argument('--release', action='store_true', help='drop the run lock')
    args = ap.parse_args()
    if args.release:
        code, line = release()
    else:
        code, line = check()
        if code == GO:
            code, line = acquire(line)
    print(line)
    sys.exit(code)


if __name__ == '__main__':
    main()
