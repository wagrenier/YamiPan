"""Check that game types keep their FS2 (EE) layout on both host compilers, and lint for host-dependent types.

python layoutcheck.py [--no-msvc] [--keep]

Layouts are checked by the compilers themselves: every game struct carries
`static_assert(sizeof(T) == 0x..)` / `static_assert(offsetof(T, f) == 0x..)` (reconstruction.md
"Types and layouts"), and this script makes both host compilers evaluate them:

- MinGW GCC (-fsyntax-only) on one generated TU per game header: catches headers no .c includes yet
  and headers that are not self-contained (the build already covers the .c files).
- MSVC (cl /Zs, through vcvars64.bat found with vswhere, or YAMIPAN_VCVARS) on the same header TUs and
  every game .c file: the build only ever runs GCC, and MSVC lays out bitfields and aligns differently.
  Skipped with a note when MSVC is not installed or --no-msvc is given.

Lint (game files only, comments ignored):
- a struct/union/class definition with no size or offset static_assert, unless the comment lines just
  above it say `layout: host` (host-only object, PS2 size not kept on purpose; say why);
- plain `long` / `unsigned long` (32-bit on Windows, 64-bit on the EE: use u_long / int64_t),
  `wchar_t`, `__attribute__((packed))` (use #pragma pack on both compilers).

Flags come from the build's compile_commands.json (run build.py first). Exit 1 on any failure.
"""
import json
import re
import subprocess
import sys

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

OUT = yamipan.WORK / 'layoutcheck'
BDIR = yamipan.REPO / 'cmake-build-relwithdebinfo'
GAME_EXT = ('.c', '.cpp', '.cc', '.h', '.hpp')


def game_files():
    for path in sorted(yamipan.SRC.rglob('*')):
        if path.suffix in GAME_EXT and path.relative_to(yamipan.SRC).parts[0] not in yamipan.PLATFORM_DIRS:
            yield path


def strip_comments(text):
    """Blank out comments and string literals, keeping offsets and newlines."""
    def blank(m):
        return re.sub(r'[^\n]', ' ', m.group(0))
    return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', blank, text, flags=re.S)


def match_brace(code, i):
    depth = 0
    for j in range(i, len(code)):
        if code[j] == '{':
            depth += 1
        elif code[j] == '}':
            depth -= 1
            if depth == 0:
                return j
    return len(code) - 1


def lint(path):
    text = path.read_text(encoding='utf-8', errors='replace')
    code = strip_comments(text)
    lines = text.splitlines()
    rel = path.relative_to(yamipan.REPO).as_posix()
    out = []

    def line_of(pos):
        return code.count('\n', 0, pos) + 1

    for m in re.finditer(r'\b(typedef\s+)?(struct|union|class)\s+(?:alignas\s*\(\s*\d+\s*\)\s+)?(\w+)?\s*'
                         r'(?::[^;{()]*)?\{', code):
        names = [m.group(3)] if m.group(3) else []
        end = match_brace(code, m.end() - 1)
        if m.group(1):
            t = re.match(r'\s*(\w+)\s*;', code[end + 1:])
            if t:
                names.append(t.group(1))
        if not names:
            continue                                    # anonymous member struct/union
        ln = line_of(m.start())
        above = '\n'.join(lines[max(0, ln - 6):ln])
        if 'layout: host' in above:
            continue
        if any(re.search(r'static_assert\s*\(\s*(?:sizeof|offsetof)\s*\(\s*' + re.escape(n) + r'\b', code)
               for n in names):
            continue
        out.append(f'{rel}:{ln}: {"/".join(names)} has no sizeof/offsetof static_assert '
                   f'(or a "layout: host" note above it)')

    for m in re.finditer(r'\blong\b', code):
        before, after = code[max(0, m.start() - 6):m.start()], code[m.end():m.end() + 6]
        if re.search(r'long\s*$', before) or re.match(r'\s*long\b', after):
            continue                                    # long long: 64-bit everywhere
        out.append(f'{rel}:{line_of(m.start())}: plain `long` (EE 64-bit, Windows 32-bit): '
                   f'use u_long / int64_t')
    for m in re.finditer(r'\bwchar_t\b|__attribute__\s*\(\(\s*packed', code):
        out.append(f'{rel}:{line_of(m.start())}: `{m.group(0)}` differs between the EE and the hosts')
    return out


def build_flags():
    db = BDIR / 'compile_commands.json'
    if not db.exists():
        raise SystemExit(f'{db} missing: run build.py first')
    for e in json.loads(db.read_text(encoding='utf-8')):
        f = e['file'].replace('\\', '/')
        if '/src/' in f and f.split('/src/', 1)[1].split('/')[0] not in yamipan.PLATFORM_DIRS:
            cmd = e.get('command') or ' '.join(e['arguments'])
            break
    else:
        raise SystemExit('no game file in compile_commands.json')
    defines = re.findall(r'(?:^|\s)-D(\S+)', cmd)
    incs = re.findall(r'(?:^|\s)-I(\S+)', cmd) + re.findall(r'-isystem\s+(\S+)', cmd)
    prefix = re.search(r'-include\s+(\S+)', cmd)      # YamiPan force-includes nothing (cpp-port.md)
    return defines, incs, prefix.group(1) if prefix else None


def header_tus(headers):
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob('h_*.cpp'):
        old.unlink()
    tus = []
    for i, h in enumerate(headers):
        tu = OUT / f'h_{i:03d}_{h.stem}.cpp'
        tu.write_text(f'// layoutcheck: {h.relative_to(yamipan.SRC).as_posix()} on its own\n'
                      f'#include "{h.relative_to(yamipan.SRC).as_posix()}"\n', encoding='utf-8')
        tus.append(tu)
    return tus


def errors_of(output):
    keep = []
    for line in output.splitlines():
        if re.search(r'\berror\b|static assertion|static_assert', line, re.I):
            keep.append('    ' + line.strip())
    return list(dict.fromkeys(keep))


def run_gcc(tus, defines, incs, prefix):
    gxx = str(yamipan.MINGW_BIN + '\\g++.exe')
    args = [gxx, '-fsyntax-only', '-x', 'c++', '-std=gnu++23', '-fpermissive', '-w']
    if prefix:
        args += ['-include', prefix]
    args += ['-D' + d for d in defines] + ['-I' + i for i in incs] + [str(t) for t in tus]
    env = dict(__import__('os').environ)
    env['PATH'] = yamipan.MINGW_BIN + ';' + env.get('PATH', '')
    r = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace', env=env)
    return r.returncode, r.stdout + r.stderr


def find_vcvars():
    if yamipan.VCVARS:
        return yamipan.VCVARS
    vswhere = r'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    try:
        r = subprocess.run([vswhere, '-latest', '-products', '*', '-requires',
                            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'],
                           capture_output=True, text=True)
    except OSError:
        return ''
    root = r.stdout.strip().splitlines()[0] if r.stdout.strip() else ''
    bat = f'{root}\\VC\\Auxiliary\\Build\\vcvars64.bat' if root else ''
    return bat if bat and __import__('os').path.exists(bat) else ''


def run_msvc(files, defines, incs, prefix):
    bat = find_vcvars()
    if not bat:
        return None, 'MSVC not found (set YAMIPAN_VCVARS to vcvars64.bat)'
    rsp = OUT / 'msvc.rsp'
    opts = ['/nologo', '/Zs', '/TP', '/std:c++latest', '/permissive', '/Zc:preprocessor', '/W1', '/wd4996',
            '/D_CRT_SECURE_NO_WARNINGS', '/EHsc'] + ([f'/FI"{prefix}"'] if prefix else [])
    opts += [f'/D{d}' for d in defines] + [f'/I"{i}"' for i in incs] + [f'"{f}"' for f in files]
    rsp.write_text('\n'.join(opts) + '\n', encoding='utf-8')
    bat_cmd = OUT / 'msvc.bat'
    bat_cmd.write_text(f'@call "{bat}" >nul\r\n@cl @"{rsp}"\r\n', encoding='utf-8')
    r = subprocess.run(['cmd', '/c', str(bat_cmd)], capture_output=True, text=True, encoding='utf-8',
                       errors='replace')
    return r.returncode, r.stdout + r.stderr


def main():
    files = list(game_files())
    headers = [f for f in files if f.suffix in ('.h', '.hpp')]
    sources = [f for f in files if f.suffix not in ('.h', '.hpp')]
    failed = False

    warnings = [w for f in files for w in lint(f)]
    print(f'lint: {len(files)} game files, {len(warnings)} problems')
    for w in warnings:
        print('  ' + w)
    failed |= bool(warnings)

    defines, incs, prefix = build_flags()
    tus = header_tus(headers)
    rc, out = run_gcc(tus, defines, incs, prefix)
    print(f'gcc : {len(tus)} headers on their own: {"OK" if rc == 0 else "FAILED"}')
    if rc:
        print('\n'.join(errors_of(out)[:40]) or out[-3000:])
        failed = True

    if '--no-msvc' in sys.argv:
        print('msvc: skipped (--no-msvc)')
    else:
        rc, out = run_msvc(tus + sources, defines, incs, prefix)
        if rc is None:
            print('msvc: skipped -', out)
        else:
            print(f'msvc: {len(tus)} headers + {len(sources)} sources: {"OK" if rc == 0 else "FAILED"}')
            if rc:
                print('\n'.join(errors_of(out)[:40]) or out[-3000:])
                failed = True

    if '--keep' not in sys.argv:
        for t in tus:
            t.unlink()
    print('layoutcheck:', 'FAILED' if failed else 'OK')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
