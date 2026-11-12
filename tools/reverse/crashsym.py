"""Turn the [crash] lines of a run log into functions and source lines.

  python crashsym.py [log]     # default work/run_relwithdebinfo.log

yamipan::crash (src/yamipan/core/crash_report.cpp) prints a plain run's fault on stdout: the unwound
frames, then the words on the stack that point into the exe. binutils addr2line does the rest.
"""
import re
import subprocess
import sys
from pathlib import Path

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

IMAGE_BASE = 0x140000000    # MinGW's default for a 64-bit exe; the log's offsets are from the real base


def main():
    log = Path(sys.argv[1]) if len(sys.argv) > 1 else yamipan.WORK / 'run_relwithdebinfo.log'
    exe = yamipan.REPO / 'cmake-build-relwithdebinfo' / 'YamiPan' / 'YamiPan.exe'
    tool = Path(yamipan.MINGW_BIN) / 'addr2line.exe'
    lines = [l.strip() for l in log.read_text(encoding='utf-8', errors='replace').splitlines() if '[crash]' in l]
    if not lines:
        raise SystemExit(f'no [crash] lines in {log}')
    for line in lines:
        m = re.search(r'exe\+0x([0-9a-f]+)', line)
        if not m:
            print(line)
            continue
        address = IMAGE_BASE + int(m.group(1), 16)
        r = subprocess.run([str(tool), '-e', str(exe), '-f', '-C', hex(address)], capture_output=True, text=True)
        name, _, where = r.stdout.partition('\n')
        print(f'{line}  {name}  {where.strip()}')


main()
