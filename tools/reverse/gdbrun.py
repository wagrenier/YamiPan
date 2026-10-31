"""Run YamiPan under gdb for a crash hunt, and inspect the held game from outside.

  python gdbrun.py [--config relwithdebinfo] [--pad SCRIPT]   # play; returns when the game exits
  python gdbrun.py --cmd "bt 20" "frame 6" "p *ene->cmn"       # gdb commands on the held game
  python gdbrun.py --break                                     # stop a hung game and hold it
  python gdbrun.py --cmd continue | quit                       # resume / kill the held game

gdb (CLion's, `$YAMIPAN_GDB` overrides) runs gdbdrive.py: first-chance faults inside DLLs pass once,
a fault in YamiPan.exe (or a repeat in a DLL) writes registers, disassembly and every thread's
backtrace to work/gdb/crash.txt and holds the stopped game; --cmd then talks to it through
work/gdb/cmd.txt -> out.txt. Game and gdb output: work/gdb/log.txt. Host input unless --pad
(build.py's scripted pad syntax). Build first (build.py); -O2 -g, so some locals are optimized out.
"""
import ctypes
import os
import subprocess
import sys
import time
from pathlib import Path

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

GDB = os.environ.get('YAMIPAN_GDB', str(Path(yamipan.MINGW_BIN).parents[1] / 'gdb' / 'win' / 'x64' / 'bin' / 'gdb.exe'))
DIR = yamipan.WORK / 'gdb'


def opt(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


def game_pid():
    r = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq YamiPan.exe', '/FO', 'CSV', '/NH'],
                       capture_output=True, text=True)
    for line in r.stdout.splitlines():
        cols = line.strip('"').split('","')
        if len(cols) > 1 and cols[0].lower() == 'yamipan.exe':
            return int(cols[1])
    return None


def command(cmds):
    out = DIR / 'out.txt'
    out.unlink(missing_ok=True)
    (DIR / 'cmd.tmp').write_text('\n'.join(cmds) + '\n', encoding='utf-8')
    os.replace(DIR / 'cmd.tmp', DIR / 'cmd.txt')
    for _ in range(600):
        if out.exists():
            print(out.read_text(encoding='utf-8', errors='replace'))
            out.unlink()
            return
        time.sleep(0.1)
    raise SystemExit('no answer in 60 s: is a game held? (look at work/gdb/log.txt)')


def brk():
    pid = game_pid()
    if pid is None:
        raise SystemExit('YamiPan.exe is not running')
    (DIR / 'break.flag').write_text('1')
    k = ctypes.windll.kernel32
    h = k.OpenProcess(0x1F0FFF, False, pid)
    if not h or not k.DebugBreakProcess(h):
        raise SystemExit(f'DebugBreakProcess({pid}) failed')
    k.CloseHandle(h)
    for _ in range(100):
        if (DIR / 'crash.txt').exists():
            print(f'held; state in {DIR / "crash.txt"}')
            return
        time.sleep(0.1)
    print('no stop reported yet; look at', DIR / 'log.txt')


def play():
    config = opt('--config', 'relwithdebinfo')
    exe = yamipan.REPO / f'cmake-build-{config}' / 'YamiPan' / 'YamiPan.exe'
    if game_pid() is not None:
        raise SystemExit('YamiPan.exe is already running')
    for f in ('crash.txt', 'cmd.txt', 'out.txt', 'break.flag'):
        (DIR / f).unlink(missing_ok=True)
    env = dict(os.environ, YAMIPAN_GDB_DIR=str(DIR),
               PATH=yamipan.MINGW_BIN + os.pathsep + os.environ.get('PATH', ''))
    env.pop('YAMIPAN_AUTOPAD', None)
    if opt('--pad'):
        env['YAMIPAN_AUTOPAD'] = opt('--pad')
    log = DIR / 'log.txt'
    print(f'{exe.name} under gdb; output in {log}, a fault leaves {DIR / "crash.txt"}')
    with open(log, 'w', encoding='utf-8', errors='replace') as f:
        r = subprocess.run([GDB, '-q', '-nx', '-batch', '-x', str(yamipan.TOOLS / 'gdbdrive.py'),
                            '--args', str(exe)], cwd=exe.parent, env=env, stdout=f, stderr=subprocess.STDOUT)
    for line in log.read_text(encoding='utf-8', errors='replace').splitlines():
        if line.startswith('[gdbdrive]'):
            print(line)
    sys.exit(r.returncode)


if __name__ == '__main__':
    DIR.mkdir(parents=True, exist_ok=True)
    if '--cmd' in sys.argv:
        command(sys.argv[sys.argv.index('--cmd') + 1:])
    elif '--break' in sys.argv:
        brk()
    else:
        play()
