"""Shared paths, the address map and the Ghidra helpers for the reversing tools. Not a CLI.

Carried over from RosePan's rosepan.py (bootstrap 3, 2026-10-31). Every path can be overridden with an
environment variable. The game ELF and the disc's files stay in the game folder (never committed).
ReiPan, MioPan and RosePan are sibling checkouts used only as *library* references (libmatch.py):
Forbidden Siren 2 shares no game code with Fatal Frame or Rule of Rose, but it links Sony's SDK 3.0
libraries like FF3 and RoR. It was built with Metrowerks CodeWarrior (architecture.md).

Ghidra goes through ghidra_mcp.py: bethington's GhidraMCP 7.0.0 (the Ghidra process with SCES_538.51
open) for reads, renames, saving, creating functions, disassembling bytes, function tags and hashes.
While Ghidra is down those helpers degrade: tags live in data/tags.json only (the repo copy is the
record anyway), save_program() says "save by hand", create_function() returns False.
"""
import json
import os
import re
import shutil
from pathlib import Path

import ghidra_mcp

TOOLS = Path(__file__).resolve().parent
REPO = TOOLS.parents[1]
WORK = TOOLS / 'work'   # regenerated: Ghidra dumps, features, stages, shots, logs (gitignored)
DATA = TOOLS / 'data'   # results worth keeping: name ledger, provenance tags, TU state, layout

# The game folder: the ELF and the disc's files (ROM.000.., SPK.ROM, MODULES\), the data root.
GAME_DIR = Path(os.environ.get('YAMIPAN_GAME_DIR', r'C:\Users\willi\Documents\Decomp\Forbidden Siren 2'))
ELF = Path(os.environ.get('YAMIPAN_ELF', GAME_DIR / 'SCES_538.51'))
GAME_DATA = Path(os.environ.get('YAMIPAN_DATA', GAME_DIR))
FUNCTIONS_TXT = REPO / 'yamipan_functions.txt'
# Program name as Ghidra reports it (data/ghidra.json "program").
PROGRAM = os.environ.get('YAMIPAN_PROGRAM', ghidra_mcp.PROGRAM or 'SCES_538.51')

# Library references (read-only, offline): an ELF plus a functions.txt-style list with sizes. The ELFs
# are not in the sibling checkouts on this machine (open-items.md): libmatch.py finds no references
# until they are copied next to their lists.
REIPAN = Path(os.environ.get('YAMIPAN_REIPAN', REPO.parent / 'ReiPan'))
MIOPAN = Path(os.environ.get('YAMIPAN_MIOPAN') or next(
    (p for p in (REPO.parent / 'MioPan', REPO.parent / 'MioPan-Mikompilation') if p.exists()), REPO.parent / 'MioPan'))
ROSEPAN = Path(os.environ.get('YAMIPAN_ROSEPAN', REPO.parent / 'RosePan'))
# IOP import names (read-only, optional): PCSX2's table of Sony IOP library exports by index (iop.py).
PCSX2 = Path(os.environ.get('YAMIPAN_PCSX2', REPO.parent / 'pcsx2'))
REFERENCES = (
    # (label, ELF, function list, note)
    ('ff3', REIPAN / 'SLES_538.25', REIPAN / 'ff3_functions.txt', 'Sony SDK 3.0 libraries (same versions as FS2)'),
    ('ff2', MIOPAN / 'SLES_523.84', MIOPAN / 'functions.txt', 'ee-gcc 2.96 runtime, newlib, SDK 2.8 (debug symbols)'),
)

SRC = REPO / 'src'
PLATFORM_DIRS = ('yamipan', 'sdk')        # the PC platform layer: not FS2 game code
SKILL_DIR = REPO / '.claude' / 'skills' / 'yamipan-port'


def _which_dir(exe):
    found = shutil.which(exe)
    return str(Path(found).parent) if found else ''


# Host toolchains: MinGW GCC builds (build.py); MSVC syntax-checks every source here (layoutcheck.py), and Visual
# Studio builds the whole tree from CMakeLists.txt.
# MSYS2's ucrt64 holds g++, ninja, cmake and gdb here and is not on PATH, so a scheduled run finds them
# only through this default.
MINGW_BIN = os.environ.get('YAMIPAN_MINGW_BIN') or _which_dir('g++') or r'C:\msys64\ucrt64\bin'
NINJA = os.environ.get('YAMIPAN_NINJA') or shutil.which('ninja') or str(Path(MINGW_BIN) / 'ninja.exe')
CMAKE = shutil.which('cmake') or str(Path(MINGW_BIN) / 'cmake.exe')
VCVARS = os.environ.get('YAMIPAN_VCVARS', '')    # vcvars64.bat; empty = find it with vswhere
EXE_NAME = 'YamiPan'

# Provenance tags on every name the tools write (data/tags.json, and Ghidra's function tags).
SYMBOL_TAGS = ('game-symbol',)                     # the ELF's own symbols (FS2 is stripped: none so far)
MATCH_TAGS = ('lib-match', 'lib-match-low')        # library code matched to a symbolised reference ELF
CONTEXT_TAGS = ('lib-context', 'game-inferred')    # named by reading: a library / runtime function, game code
ALL_TAGS = SYMBOL_TAGS + MATCH_TAGS + CONTEXT_TAGS
GENERIC_NAME = re.compile(r'^(FUN_|thunk_FUN_)')

# Source annotation above every reconstructed function, and the stub macros (src/common/yamipan_stub.h).
ANNOT = 'FS2'
ANNOT_RE = re.compile(r'FS2 0x([0-9a-fA-F]{8})')
STUB_RE = re.compile(r'YAMIPAN_(?:STUB|BRIDGE)\("([^"]+)",\s*0x([0-9a-fA-F]+)\)')

LAYOUT_JSON = DATA / 'layout.json'
TUS_JSON = DATA / 'tus.json'
TAGS_JSON = DATA / 'tags.json'


def load_json(path, default):
    try:
        with open(path, encoding='utf-8') as f:
            return json.load(f)
    except FileNotFoundError:
        return default


def save_json(path, obj, indent=1):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(obj, f, indent=indent, sort_keys=isinstance(obj, dict))
        f.write('\n')


# ---- the address map ------------------------------------------------------------------------------
# data/layout.json: {"lib_ranges": [[lo, hi, what], ...], "satellite_ranges": [[lo, hi, what], ...],
# "image_sections": [[lo, hi, name, flags], ...]} (hex strings). Library ranges are replaced on PC
# (src/sdk, host libc); never a TU, never frontier. libmatch.py --ranges proposes them. Satellites have
# no TU of their own and ride with their class's TU (RoR's linkonce sections; FS2 has none so far).
# image_sections: FS2's ELF has one unnamed section for the whole image, so the tools that ask "which
# section is this address in" (features.py, libmatch.py, datadump.py) get the address map's regions
# instead (architecture.md "Address map"; elf32.Elf reads them).

def _ranges(key):
    return [(int(lo, 16), int(hi, 16)) for lo, hi, *_ in load_json(LAYOUT_JSON, {}).get(key, [])]


LIB_RANGES = _ranges('lib_ranges')
SATELLITE_RANGES = _ranges('satellite_ranges')


def image_sections():
    """[{'name', 'type', 'flags', 'addr', 'offset', 'size'}] for the regions of an ELF without section
    names (data/layout.json image_sections; offset is filled in by elf32.Elf)."""
    out = []
    for lo, hi, name, flags in load_json(LAYOUT_JSON, {}).get('image_sections', []):
        lo, hi = int(lo, 16), int(hi, 16)
        out.append({'name': name, 'type': 8 if name in ('.bss', '.sbss') else 1, 'flags': int(flags),
                    'addr': lo, 'offset': None, 'size': hi - lo})
    return out


def in_lib(addr):
    return any(lo <= addr < hi for lo, hi in LIB_RANGES)


def is_satellite(addr):
    return any(lo <= addr < hi for lo, hi in SATELLITE_RANGES)


def tu_key(inferred_file):
    """TU key (path under src/, 'rx/rxfile.c') for an inferred file, or None for library objects and
    unknown files."""
    if not inferred_file or inferred_file.startswith('('):
        return None
    if inferred_file.startswith('lib/'):
        return None
    return inferred_file


def tu_ranges():
    """[(lo, hi, tu_key)] from data/tus.json: address ranges sessions have pinned to a TU.
    They override export.py's inference."""
    out = []
    for key, tu in load_json(TUS_JSON, {}).get('tus', {}).items():
        for lo, hi in tu.get('ranges', []):
            out.append((int(lo, 16), int(hi, 16), key))
    return sorted(out)


def src_path_for(inferred_file):
    """Where YamiPan keeps an inferred source file under src/ (None for unknown runs)."""
    if not inferred_file or inferred_file.startswith('('):
        return None
    return SRC / inferred_file


# ---- Ghidra ---------------------------------------------------------------------------------------
# http() is bethington's GhidraMCP 7.0.0 (ghidra_mcp.py): the parsed JSON answer; `program` is added to
# the query string, a POST's parameters go in `body`. Every endpoint and its parameters: GET /mcp/schema.

GhidraError = ghidra_mcp.GhidraError
GhidraRequestError = ghidra_mcp.GhidraRequestError
_g = ghidra_mcp.Ghidra(PROGRAM)
UNSAVED = 'Ghidra has unsaved changes: Ctrl+S (no GhidraMCP server answered to save them)'


def http(method, endpoint, body=None, timeout=300, **query):
    """GhidraMCP: method endpoint -> the parsed answer (GhidraRequestError on {"error": ...})."""
    return _g.http(method, endpoint, body, timeout=timeout, **query)


def ghidra_up():
    """True when a GhidraMCP server has FS2's program open."""
    return _g.up()


def paged(endpoint, key, limit=5000, **query):
    """Every item of a paged list endpoint (`offset` / `limit`; the list under `key`, the count in
    `total` or `total_matching`)."""
    out, offset = [], 0
    while True:
        d = http('GET', endpoint, offset=offset, limit=limit, **query)
        items = d.get(key) or []
        out += items
        offset += len(items)
        total = d.get('total', d.get('total_matching'))
        if not items or total is None or offset >= total:
            return out


def function_names():
    """{'0010baf8': 'Name', ...} for every function in the program."""
    return {f['address'].lower().zfill(8): f['name']
            for f in paged('find_functions', 'functions', sort_by='address') if ':' not in f['address']}


def function_at(addr):
    """The function containing addr as {'address', 'name', 'body_start', 'body_end', 'signature'}, or None."""
    try:
        d = http('GET', 'get_functions', function=f'0x{addr:x}', fields='entry_point,body_start,body_end,signature',
                 timeout=60)
    except GhidraRequestError as e:
        if e.code == 'NOT_FOUND':
            return None
        raise
    return {'address': d.get('entry_point') or d.get('address'), **d}


def has_function(addr):
    """Whether a function *starts* at addr (get_functions answers for any address inside a body)."""
    f = function_at(addr)
    return bool(f and f.get('address')) and int(str(f['address']).replace('0x', ''), 16) == addr


def rename(addr, name):
    """Rename the function at addr. Returns '' on success, else the server's answer. strict_mode off:
    the plugin's naming gate wants PascalCase verbs, YamiPan follows the game's own style."""
    try:
        http('POST', 'rename_function', {'old_name': f'0x{addr:x}', 'new_name': name, 'strict_mode': 'off'})
    except GhidraRequestError as e:
        return str(e)[:200]
    return ''


def decompile(addr):
    d = http('GET', 'get_functions', function=f'0x{addr:08x}' if isinstance(addr, int) else addr,
             fields='decompiled_code', timeout=120)
    return (d.get('decompiled_code') or '').replace('\r\n', '\n').strip()


def disassemble_function(addr):
    """'addr: instruction' lines of the function containing addr."""
    d = http('GET', 'disassemble_function', address=f'0x{addr:08x}' if isinstance(addr, int) else addr, timeout=120)
    return '\n'.join(f'{i["address"]}: {i["instruction"]}' + (f'  ; {i["comment"]}' if i.get('comment') else '')
                     for i in d.get('instructions', []))


def save_program(tries=6):
    """Save the program, retrying while a previous write still holds the transaction lock. Ghidra down:
    UNSAVED (say it in the report; the user saves with Ctrl+S)."""
    import time
    if not ghidra_up():
        return UNSAVED
    out = ''
    for _ in range(tries):
        try:
            return json.dumps(http('GET', 'save_program'))
        except GhidraRequestError as e:
            out = str(e)
            if e.code != 'LOCKED':
                return out
        time.sleep(3)
    return out


# Ghidra writes. Every writer saves with save_program() when its batch is done.

def create_function(addr, name='', tries=4):
    """Create a function at addr (disassembling first); retries while a save holds the lock or the call
    answered without creating it. True if a
    function starts there afterwards; False (and nothing written) while Ghidra is down."""
    import time
    if not ghidra_up():
        return False
    for _ in range(tries):
        try:
            http('POST', 'create_function', {'address': f'0x{addr:x}', 'disassemble_first': True,
                                             **({'name': name} if name else {})})
            if has_function(addr):      # it can answer success and create nothing: a second call does
                return True
        except GhidraRequestError as e:
            if e.code != 'LOCKED':
                break
            time.sleep(3)
    return has_function(addr)


def tags():
    """{'0010baf8': tag} from data/tags.json, the record of every provenance tag."""
    return load_json(TAGS_JSON, {})


def get_tag(addr):
    return tags().get(f'{addr:08x}')


def set_tag(addr, tag):
    """Give the function at addr provenance `tag`: data/tags.json, and Ghidra's function tags when Ghidra
    answers (removing any other provenance tag there). Returns the previous provenance tags."""
    key = f'{addr:08x}'
    t = tags()
    prev = [t[key]] if key in t else []
    t[key] = tag
    save_json(TAGS_JSON, t)
    if ghidra_up():
        old = http('GET', 'get_functions', function=f'0x{addr:x}', fields='tags').get('tags') or []
        old = [x if isinstance(x, str) else x.get('name') for x in old]
        drop = [x for x in old if x in ALL_TAGS and x != tag]
        if drop:
            http('POST', 'remove_function_tag', {'function': f'0x{addr:x}', 'tags': ','.join(drop)})
        if tag not in old:
            http('POST', 'add_function_tag', {'function': f'0x{addr:x}', 'tags': tag,
                                              'tag_comments': {tag: TAG_COMMENTS.get(tag, '')}})
    return prev


TAG_COMMENTS = {
    'game-symbol': "FS2's own symbol (the ELF is stripped: only if a symbolised build turns up).",
    'lib-match': 'Library/runtime code matched word for word (relocations masked) to a symbolised '
                 'reference ELF (libmatch.py).',
    'lib-match-low': 'Library match on a short body or a disputed name: a hint, verify before relying on it.',
    'lib-context': 'Library/runtime function named by reading it against the SDK / a reference.',
    'game-inferred': 'Name inferred from FS2 code and context (strings, asserts, vtable, callers).',
}


def ensure_tags():
    """Nothing to do: add_function_tag creates a provenance tag, with its comment, on first use."""


# ---- cached state (works offline) ----------------------------------------------------------------

def cached_names():
    """{'0010baf8': 'Name'} from the last fetch/features run (work/feat_fs2.json), for offline use."""
    try:
        feats = json.load(open(WORK / 'feat_fs2.json', encoding='utf-8'))['functions']
    except FileNotFoundError:
        return {}
    return {f'{f["addr"]:08x}': f['name'] for f in feats}


def functions():
    """{addr int: {'name', 'size', 'file', 'prov'}} from yamipan_functions.txt (export.py)."""
    out, cur = {}, None
    if not FUNCTIONS_TXT.exists():
        return out
    for line in FUNCTIONS_TXT.read_text(encoding='utf-8').splitlines():
        if line.startswith('// FILE -- '):
            cur = line[len('// FILE -- '):].strip()
            cur = None if cur.startswith('(') else cur
        elif (m := re.match(r'/\* ([0-9a-f]{8}) ([0-9a-f]{8}) \*/ (.+?)\(\)\{\}(?:\s+// (\S+))?\s*$', line)):
            out[int(m.group(1), 16)] = {'name': m.group(3), 'size': int(m.group(2), 16),
                                        'file': cur, 'prov': m.group(4) or ''}
    return out


def game_sources():
    for path in SRC.rglob('*'):
        if path.suffix in ('.c', '.cpp', '.cc') and path.relative_to(SRC).parts[0] not in PLATFORM_DIRS:
            yield path


def reconstructed():
    """{addr int: (path, line)} for every game function in src/ annotated `FS2 0x<addr>`."""
    out = {}
    if not SRC.exists():
        return out
    for path in game_sources():
        for i, line in enumerate(path.read_text(encoding='utf-8', errors='replace').splitlines(), 1):
            for m in ANNOT_RE.finditer(line):
                out.setdefault(int(m.group(1), 16), (path, i))
    return out


def stubs():
    """{addr int: (path, name)} for every YAMIPAN_STUB (and YAMIPAN_BRIDGE: temporary stand-ins that work
    but are not FS2's code, still to reconstruct) in game code."""
    out = {}
    if not SRC.exists():
        return out
    for path in game_sources():
        for m in STUB_RE.finditer(path.read_text(encoding='utf-8', errors='replace')):
            out[int(m.group(2), 16)] = (path, m.group(1))
    return out


def display(name):
    """A name as people read it: GCC 2-style mangled names demangled (CodeWarrior's mangling is close to
    it; FS2 is stripped, so this matters only for reference names), everything else unchanged."""
    import gcc2
    d = gcc2.demangle(name)
    return d.display if d else name
