"""IOP modules: unpack the IOPRP images, identify every .irx, write ELFs Ghidra can load.

python iop.py [report]                     every image and module under <game data>/MODULES: what each is
python iop.py list IMG [...]               a ROMDIR image's entries (offset, size, EXTINFO date / version)
python iop.py extract [IMG ...] [--out D]  unpack images (default: every *.IMG in MODULES) to work/iop/<img>/
python iop.py irx FILE [...] [--stubs]     one module: .iopmod, sections, imports (named), exports
python iop.py elf FILE [...] [--base A] [--out D]
                                           a linked MIPS ELF per module (work/iop/elf/<file>.elf by
                                           default): code at A (0), import stubs and export entries as
                                           symbols, _gp set, so Ghidra's ELF loader takes it as is
python iop.py cmp [A B] [--out F]          two builds of a module word for word (default: RoR's RXSNDDRV
                                           against FS2's), every function of RosePan's reconstruction
                                           placed in B; the table goes to F (work/iop/rxsnddrv_cmp.txt)

FS2 reboots the IOP with modules/ioprp300.img, then loads every .irx in IOPCONF.CNF (rxSysLoadIopModules,
src/rx/rxsys.c). The game's own IOP code is rxsnddrv.irx, the rx sound driver the EE's rxSnd* functions
call by SIF RPC; the rest are Sony's. Carried over from RosePan's tools/reverse/iop.py (2026-11-06).

Formats. A ROMDIR image (the IOP boot ROM's layout, also IOPRP*.IMG): 16-byte entries {name[10],
extinfo size u16, file size u32} until an empty name; the files follow one another from offset 0, each
16-aligned (RESET is size 0, so ROMDIR describes itself at offset 0). EXTINFO holds each entry's records
in entry order: {value u16, length u8, type u8, data[length]}; type 1 date (BCD 0xYYYYMMDD), 2 version
(in value), 3 comment, 0x7f fixed. An IRX is a MIPS ELF of type 0xff80, linked at 0 with its REL
sections kept for loadcore; .iopmod {module info, entry, gp, text / data / bss size, version u16,
name}. Imports: 0x41e00000, 0, version u16, mode u16, library name[8], then stubs {jr ra; addiu zero,
zero, index} up to a zero pair. Exports: 0x41c00000, 0, version, mode, name[8], function words to 0.
Import names come from PCSX2's pcsx2/IopModuleNames.cpp (yamipan.PCSX2, read-only) when that checkout
exists, else from RXSNDDRV_IMPORTS below; unknown indexes print as <lib>_<index>.
"""
import difflib
import re
import struct
import sys
from pathlib import Path

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
OUT = yamipan.WORK / 'iop'
IMPORT_MAGIC, EXPORT_MAGIC = 0x41e00000, 0x41c00000
JR_RA = 0x03e00008
# E_MIPS_ABI_O32 | EF_MIPS_NOREORDER, MIPS I. Ghidra's MIPS.opinion wants the ABI bits for a 32-bit
# language: with 0 its ELF loader takes MIPS:LE:64:64-32R6addr and the R3000 code decodes as bad data.
ELF_FLAGS = 0x1001
ROR_IRX = Path(r'C:\Users\willi\Documents\Decomp\Rule of Rose\MODULES\RXSNDDRV.IRX')

# What RXSNDDRV imports, for a machine without PCSX2's checkout. RosePan's architecture.md "The IOP side"
# lists these names per library (made with PCSX2's table); both games' modules import the same 40
# indexes, and the names follow them in order.
RXSNDDRV_IMPORTS = {
    'libsd': {4: 'sceSdInit', 5: 'sceSdSetParam', 6: 'sceSdGetParam', 7: 'sceSdSetSwitch', 8: 'sceSdGetSwitch',
              9: 'sceSdSetAddr', 10: 'sceSdGetAddr', 11: 'sceSdSetCoreAttr', 13: 'sceSdNote2Pitch',
              15: 'sceSdProcBatch', 17: 'sceSdVoiceTrans', 18: 'sceSdBlockTrans', 20: 'sceSdBlockTransStatus',
              23: 'sceSdSetEffectAttr', 26: 'sceSdSetTransIntrHandler'},
    'sysmem': {4: 'AllocSysMemory', 5: 'FreeSysMemory', 6: 'QueryMemSize', 7: 'QueryMaxFreeMemSize',
               8: 'QueryTotalFreeMemSize'},
    'intrman': {17: 'CpuSuspendIntr', 18: 'CpuResumeIntr'},
    'loadcore': {5: 'FlushDcache'},
    'sifcmd': {14: 'sceSifInitRpc', 17: 'sceSifRegisterRpc', 19: 'sceSifSetRpcQueue', 22: 'sceSifRpcLoop',
               23: 'sceSifGetOtherData'},
    'sifman': {7: 'sceSifSetDma', 8: 'sceSifDmaStat'},
    'stdio': {4: 'printf'},
    'sysclib': {14: 'memset'},
    'thbase': {4: 'CreateThread', 6: 'StartThread', 20: 'GetThreadId'},
    'thevent': {4: 'CreateEventFlag', 6: 'SetEventFlag', 7: 'iSetEventFlag', 10: 'WaitEventFlag'},
    'vblank': {5: 'WaitVblankEnd'},
}


def modules_dir():
    if yamipan.GAME_DATA is None:
        sys.exit('no game data: set $YAMIPAN_DATA')
    d = next((p for p in yamipan.GAME_DATA.iterdir() if p.is_dir() and p.name.lower() == 'modules'), None)
    if d is None:
        sys.exit(f'no MODULES folder in {yamipan.GAME_DATA}')
    return d


# ---- names ---------------------------------------------------------------------------------------
_names = None


def import_names():
    """{library: {index: name}} from PCSX2's table (empty if the checkout is missing)."""
    global _names
    if _names is None:
        _names = {}
        src = yamipan.PCSX2 / 'pcsx2' / 'IopModuleNames.cpp'
        if src.exists():
            lib = None
            for line in src.read_text(encoding='utf-8', errors='replace').splitlines():
                if line.startswith('#if 0'):
                    break           # the "undocumented functions from old list" block is disabled there too
                m = re.match(r'\s*MODULE\((\w+)\)', line)
                if m:
                    lib = _names.setdefault(m.group(1), {})
                    continue
                m = re.match(r'\s*EXPORT\(\s*(\d+),\s*(\w+)\)', line)
                if m and lib is not None:
                    lib.setdefault(int(m.group(1)), m.group(2))
    return _names


def import_name(lib, index):
    return (import_names().get(lib, {}).get(index) or RXSNDDRV_IMPORTS.get(lib, {}).get(index)
            or f'{lib}_{index}')


# ---- ROMDIR images --------------------------------------------------------------------------------
def romdir(buf):
    """[{name, off, size, ext}] of a ROMDIR image; ext = decoded EXTINFO records."""
    entries, i = [], 0
    while i * 16 + 16 <= len(buf):
        raw, ext_size, size = struct.unpack_from('<10sHI', buf, i * 16)
        if not raw.rstrip(b'\0'):
            break
        entries.append({'name': raw.split(b'\0')[0].decode('ascii', 'replace'), 'ext_size': ext_size,
                        'size': size})
        i += 1
    off = 0
    for e in entries:
        e['off'] = off
        off += (e['size'] + 15) & ~15
    extinfo = next((e for e in entries if e['name'] == 'EXTINFO'), None)
    pos = extinfo['off'] if extinfo else None
    for e in entries:
        e['ext'] = {}
        if pos is None or not e['ext_size']:
            continue
        end = pos + e['ext_size']
        while pos + 4 <= end:
            value, length, kind = struct.unpack_from('<HBB', buf, pos)
            data = buf[pos + 4:pos + 4 + length]
            if kind == 1 and length >= 4:
                d = struct.unpack_from('<I', data)[0]
                e['ext']['date'] = f'{d >> 16:04x}-{(d >> 8) & 0xff:02x}-{d & 0xff:02x}'
            elif kind == 2:
                e['ext']['version'] = f'{value >> 8}.{value & 0xff:02x}'
            elif kind == 3:
                e['ext']['comment'] = data.split(b'\0')[0].decode('ascii', 'replace')
            pos += 4 + length
        pos = end
    return entries


def is_elf(b):
    return b[:4] == b'\x7fELF'


def entry_filename(e, data):
    if is_elf(data):
        return e['name'] + '.irx'
    return e['name'] + ('.txt' if e['name'] == 'IOPBTCONF' else '.bin')


def cmd_list(paths):
    for p in paths:
        buf = Path(p).read_bytes()
        print(f'== {p} ({len(buf)} bytes)')
        print(f'   {"entry":10} {"offset":>8} {"size":>7}  {"date":10} {"ver":>5}  module')
        for e in romdir(buf):
            data = buf[e['off']:e['off'] + e['size']]
            mod = ''
            if is_elf(data):
                info = irx(data)
                mod = f'{info["name"]} {info["version"]}' if info else 'ELF (not an IRX)'
            x = e['ext']
            print(f'   {e["name"]:10} {e["off"]:#8x} {e["size"]:#7x}  {x.get("date", ""):10} '
                  f'{x.get("version", ""):>5}  {mod or x.get("comment", "")}')


def cmd_extract(paths, out):
    for p in paths:
        p = Path(p)
        buf = p.read_bytes()
        dest = (out or OUT) / p.stem.lower()
        dest.mkdir(parents=True, exist_ok=True)
        n = 0
        for e in romdir(buf):
            if e['name'] in ('RESET', 'ROMDIR', 'EXTINFO') or not e['size']:
                continue
            data = buf[e['off']:e['off'] + e['size']]
            (dest / entry_filename(e, data)).write_bytes(data)
            n += 1
        print(f'{p.name}: {n} files -> {dest.relative_to(yamipan.REPO) if dest.is_relative_to(yamipan.REPO) else dest}')


# ---- IRX ------------------------------------------------------------------------------------------
def sections(b):
    shoff, = struct.unpack_from('<I', b, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', b, 0x2e)
    raw = [struct.unpack_from('<10I', b, shoff + i * shentsize) for i in range(shnum)]
    stroff = raw[shstrndx][4] if shnum else 0
    out = []
    for s in raw:
        end = b.find(b'\0', stroff + s[0])
        out.append({'name': b[stroff + s[0]:end].decode('ascii', 'replace'), 'type': s[1], 'flags': s[2],
                    'addr': s[3], 'off': s[4], 'size': s[5], 'link': s[6], 'info': s[7]})
    return out


def irx(b):
    """The module's facts, or None when b is not an IRX."""
    if not is_elf(b) or struct.unpack_from('<H', b, 0x10)[0] != 0xff80:
        return None
    secs = sections(b)
    mod = next((s for s in secs if s['type'] == 0x70000080), None)
    info = {'sections': secs, 'name': '?', 'version': '?', 'entry': 0, 'gp': 0}
    if mod:
        mi, entry, gp, tsize, dsize, bsize, ver = struct.unpack_from('<6IH', b, mod['off'])
        name = b[mod['off'] + 26:mod['off'] + mod['size']].split(b'\0')[0].decode('ascii', 'replace')
        info.update(name=name, version=f'{ver >> 8}.{ver & 0xff:02x}', entry=entry, gp=gp, text=tsize,
                    data=dsize, bss=bsize, moduleinfo=mi)
    img = image(b, secs)
    info['image'] = img
    info['imports'], info['exports'] = tables(img)
    syms = next((s for s in secs if s['type'] == 2), None)
    info['symbols'] = syms['size'] // 16 - 1 if syms else 0
    return info


def image(b, secs):
    """The module as loaded at 0: PROGBITS at their addresses, NOBITS as zeros."""
    alloc = [s for s in secs if s['type'] in (1, 8) and s['name'] != '.iopmod' and s['size']]
    size = max((s['addr'] + s['size'] for s in alloc), default=0)
    img = bytearray(size)
    for s in alloc:
        if s['type'] == 1:
            img[s['addr']:s['addr'] + s['size']] = b[s['off']:s['off'] + s['size']]
    return bytes(img)


def tables(img):
    """Import and export tables found in the loaded image."""
    imports, exports = [], []
    for at in range(0, len(img) - 20, 4):
        magic, = struct.unpack_from('<I', img, at)
        if magic not in (IMPORT_MAGIC, EXPORT_MAGIC):
            continue
        nxt, ver, mode = struct.unpack_from('<IHH', img, at + 4)
        raw = img[at + 12:at + 20]
        lib = raw.split(b'\0')[0].decode('ascii', 'replace')
        if nxt or not re.fullmatch(r'[A-Za-z0-9_]+', lib or '-'):
            continue
        pos = at + 20
        if magic == IMPORT_MAGIC:
            stubs = []
            while pos + 8 <= len(img):
                w0, w1 = struct.unpack_from('<II', img, pos)
                if w0 == 0 and w1 == 0:
                    break
                if w0 != JR_RA or (w1 & 0xffff0000) != 0x24000000:
                    stubs = None
                    break
                stubs.append((pos, w1 & 0xffff))
                pos += 8
            if stubs is not None:
                imports.append({'at': at, 'lib': lib, 'version': f'{ver >> 8}.{ver & 0xff:02x}', 'stubs': stubs})
        else:
            funcs = []
            while pos + 4 <= len(img):
                w, = struct.unpack_from('<I', img, pos)
                if w == 0:
                    break
                funcs.append(w)
                pos += 4
            exports.append({'at': at, 'lib': lib, 'version': f'{ver >> 8}.{ver & 0xff:02x}', 'funcs': funcs})
    return imports, exports


def cmd_irx(paths, show_stubs):
    for p in paths:
        b = Path(p).read_bytes()
        info = irx(b)
        if info is None:
            print(f'== {p}: not an IRX')
            continue
        print(f'== {p}: {info["name"]} {info["version"]}  entry {info["entry"]:#x}  gp {info["gp"]:#x}  '
              f'text {info.get("text", 0):#x} data {info.get("data", 0):#x} bss {info.get("bss", 0):#x}  '
              f'symbols {info["symbols"]}')
        for s in info['sections']:
            if s['name'] and s['type'] in (1, 8):
                print(f'   {s["name"]:10} {s["addr"]:#08x}..{s["addr"] + s["size"]:#08x}')
        for t in info['exports']:
            print(f'   export {t["lib"]} {t["version"]}: ' + ', '.join(f'{i}:{f:#x}' for i, f in enumerate(t['funcs'])))
        for t in info['imports']:
            if show_stubs:
                print(f'   import {t["lib"]} {t["version"]} (table {t["at"]:#x})')
                for at, i in t['stubs']:
                    print(f'      {at:#07x}  {i:3}  {import_name(t["lib"], i)}')
            else:
                print(f'   import {t["lib"]} {t["version"]}: ' + ', '.join(import_name(t['lib'], i) for _, i in t['stubs']))


# ---- ELF for Ghidra -------------------------------------------------------------------------------
def symbols_for(info, base):
    syms = [('_start', info['entry'] + base, 'func')]
    if info['gp']:
        syms.append(('_gp', info['gp'] + base, 'object'))
    seen = set()
    for t in info['imports']:
        syms.append((f'_imp_{t["lib"]}', t['at'] + base, 'object'))
        for at, i in t['stubs']:
            n = import_name(t['lib'], i)
            n = n if n not in seen else f'{t["lib"]}_{n}'
            seen.add(n)
            syms.append((n, at + base, 'func'))
    for t in info['exports']:
        syms.append((f'_exp_{t["lib"]}', t['at'] + base, 'object'))
        for i, f in enumerate(t['funcs']):
            if f != info['entry']:
                syms.append((f'{t["lib"]}_export_{i}', f + base, 'func'))
    return syms


def write_elf(info, base, dest):
    """A little-endian MIPS ET_EXEC: one PT_LOAD over the image (bss as memsz), sections mirroring the
    IRX's, and a symbol table. Relocations need nothing at base 0; at another base the HI16 / LO16 /
    32 / 26 fixups are applied here."""
    img = bytearray(info['image'])
    secs = [s for s in info['sections'] if s['type'] in (1, 8) and s['name'] != '.iopmod' and s['size']]
    if base:
        relocate(img, info, base)
    filesz = max((s['addr'] + s['size'] for s in secs if s['type'] == 1), default=0)
    memsz = max((s['addr'] + s['size'] for s in secs), default=0)
    syms = symbols_for(info, base)

    strtab = bytearray(b'\0')
    symtab = bytearray(16)
    for name, value, kind in syms:
        sec = next((k + 1 for k, s in enumerate(secs) if s['addr'] <= value - base < s['addr'] + s['size']), 0xfff1)
        symtab += struct.pack('<IIIBBH', len(strtab), value, 0, (1 << 4) | (2 if kind == 'func' else 1), 0, sec)
        strtab += name.encode() + b'\0'
    shstr = bytearray(b'\0')
    names = {}
    for n in [s['name'] for s in secs] + ['.symtab', '.strtab', '.shstrtab']:
        names[n] = len(shstr)
        shstr += n.encode() + b'\0'

    ehsize, phsize = 52, 32
    data_off = 0x1000
    body = bytearray(img[:filesz])
    sym_off = data_off + len(body)
    str_off = sym_off + len(symtab)
    shs_off = str_off + len(strtab)
    sh_off = (shs_off + len(shstr) + 3) & ~3
    out = bytearray(data_off)
    out[:ehsize] = struct.pack('<16sHHIIIIIHHHHHH', b'\x7fELF\x01\x01\x01' + bytes(9), 2, 8, 1,
                               info['entry'] + base, ehsize, sh_off, ELF_FLAGS, ehsize, phsize, 1, 40,
                               len(secs) + 4, len(secs) + 3)
    out[ehsize:ehsize + phsize] = struct.pack('<8I', 1, data_off, base, base, filesz, memsz, 7, 0x10)
    out += body + symtab + strtab + shstr
    out += bytes(sh_off - len(out))
    sh = bytearray(40)
    for s in secs:
        flags = 0x2 | (0x4 if s['name'] == '.text' else 0x1 if s['name'] in ('.data', '.bss') else 0)
        sh += struct.pack('<10I', names[s['name']], s['type'], flags, s['addr'] + base,
                          data_off + s['addr'] if s['type'] == 1 else data_off + filesz, s['size'], 0, 0, 16, 0)
    n_local = 1
    sh += struct.pack('<10I', names['.symtab'], 2, 0, 0, sym_off, len(symtab), len(secs) + 2, n_local, 4, 16)
    sh += struct.pack('<10I', names['.strtab'], 3, 0, 0, str_off, len(strtab), 0, 0, 1, 0)
    sh += struct.pack('<10I', names['.shstrtab'], 3, 0, 0, shs_off, len(shstr), 0, 0, 1, 0)
    out += sh
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(out)


def relocate(img, info, base):
    """Apply the IRX's REL entries for a load at base (MIPS R_32 2, R_26 4, R_HI16 5, R_LO16 6)."""
    raw = info.get('raw')
    if raw is None:
        sys.exit('relocation needs the module bytes')
    secs = info['sections']
    for s in secs:
        if s['type'] != 9:
            continue
        target = secs[s['info']]
        pending = []
        for k in range(s['size'] // 8):
            off, rinfo = struct.unpack_from('<II', raw, s['off'] + k * 8)
            at = target['addr'] + off
            kind = rinfo & 0xff
            w, = struct.unpack_from('<I', img, at)
            if kind == 2:
                w = (w + base) & 0xffffffff
            elif kind == 4:
                w = (w & 0xfc000000) | ((((w & 0x3ffffff) << 2) + base) >> 2 & 0x3ffffff)
            elif kind == 5:
                pending.append(at)
                continue
            elif kind == 6:
                lo = w & 0xffff
                for hi_at in pending:
                    hw, = struct.unpack_from('<I', img, hi_at)
                    full = ((hw & 0xffff) << 16) + (lo - 0x10000 if lo & 0x8000 else lo) + base
                    hi = ((full + 0x8000) >> 16) & 0xffff
                    struct.pack_into('<I', img, hi_at, (hw & 0xffff0000) | hi)
                pending = []
                full = lo + base if not (lo & 0x8000) else lo - 0x10000 + base
                w = (w & 0xffff0000) | (full & 0xffff)
            struct.pack_into('<I', img, at, w)


def cmd_elf(paths, base, out):
    for p in paths:
        b = Path(p).read_bytes()
        info = irx(b)
        if info is None:
            print(f'{p}: not an IRX')
            continue
        info['raw'] = b
        dest = (out or OUT / 'elf') / f'{Path(p).stem.lower()}.elf'
        write_elf(info, base, dest)
        n = len(symbols_for(info, base))
        print(f'{Path(p).name}: {info["name"]} {info["version"]} -> {dest} (base {base:#x}, {n} symbols)')


# ---- compare two builds ---------------------------------------------------------------------------
def masked_text(path):
    """(.text words with what differs between links masked, the module's facts): every field a REL entry
    relocates (R_32 2, R_26 4, R_HI16 5, R_LO16 6) and the immediates of gp-relative instructions."""
    b = Path(path).read_bytes()
    info = irx(b)
    if info is None:
        sys.exit(f'{path}: not an IRX')
    secs = info['sections']
    text = next(s for s in secs if s['name'] == '.text')
    words = list(struct.unpack_from(f'<{text["size"] // 4}I', b, text['off']))
    keep = {2: 0, 4: 0xfc000000, 5: 0xffff0000, 6: 0xffff0000}
    for s in secs:
        if s['type'] == 9 and secs[s['info']] is text:
            for k in range(s['size'] // 8):
                off, rinfo = struct.unpack_from('<II', b, s['off'] + k * 8)
                words[off // 4] &= keep[rinfo & 0xff]
    for i, w in enumerate(words):
        if w >> 26 >= 8 and (w >> 21) & 31 == 28:
            words[i] = w & 0xffff0000
    return words, info


def rose_functions():
    """{address: name} of RosePan's reconstruction (a line '// IOP 0x...' above each function)."""
    found = {}
    for p in sorted((yamipan.ROSEPAN / 'src' / 'iop' / 'rxsnddrv').glob('*.cpp')):
        lines = p.read_text(encoding='utf-8', errors='replace').splitlines()
        for i, line in enumerate(lines):
            m = re.match(r'// IOP 0x([0-9a-fA-F]+)\s*$', line)
            if not m:
                continue
            call = next((re.search(r'([A-Za-z_]\w*)\s*\(', x) for x in lines[i + 1:i + 8] if '(' in x), None)
            found.setdefault(int(m.group(1), 16), f'{p.stem}:{call.group(1) if call else "?"}')
    return found


def cmd_cmp(paths, out):
    a_path, b_path = paths if len(paths) == 2 else (ROR_IRX, modules_dir() / 'RXSNDDRV.IRX')
    a, ia = masked_text(a_path)
    b, ib = masked_text(b_path)
    blocks = difflib.SequenceMatcher(None, a, b, autojunk=False).get_matching_blocks()
    to_b = {x + k: y + k for x, y, n in blocks for k in range(n)}
    same = len(to_b)
    print(f'A {a_path}: {ia["name"]} {ia["version"]}, .text {len(a) * 4:#x}')
    print(f'B {b_path}: {ib["name"]} {ib["version"]}, .text {len(b) * 4:#x}')
    print(f'{same} of {len(a)} words of A and of {len(b)} of B align with relocated fields masked '
          f'({same * 100 / len(a):.1f}% / {same * 100 / len(b):.1f}%)')

    names = rose_functions()
    starts = sorted(x // 4 for x in names if x < len(a) * 4)
    rows = []
    for k, lo in enumerate(starts):
        hi = starts[k + 1] if k + 1 < len(starts) else len(a)
        hits = [to_b[i] - i for i in range(lo, hi) if i in to_b]
        whole = len(hits) == hi - lo and len(set(hits)) == 1
        at = (lo + hits[0]) * 4 if hits else None       # the first word: code inserted later moves the rest
        pct = len(hits) * 100 // (hi - lo)
        if at is None:      # the alignment keeps order, so a moved function aligns nowhere: look for it
            body = a[lo:hi]
            at, best = max(((i * 4, sum(p == q for p, q in zip(body, b[i:]))) for i in range(len(b) - len(body))),
                           key=lambda x: x[1])
            base = max(at // 4 - 32, 0)     # the best place can sit past code inserted at its head
            near = difflib.SequenceMatcher(None, body, b[base:at // 4 + len(body) + 32], autojunk=False)
            first = near.get_matching_blocks()[0]
            at = (base + first.b - first.a) * 4
            pct = -(sum(m.size for m in near.get_matching_blocks()) * 100 // (hi - lo))
        rows.append((lo * 4, at, (hi - lo) * 4, whole, pct, names[lo * 4]))
    whole = [r for r in rows if r[3]]
    print(f"{len(whole)} of {len(rows)} functions of RosePan's reconstruction are in B word for word "
          f'({sum(r[2] for r in whole) * 100 // sum(r[2] for r in rows)}% of the code by size)')
    print('match: = word for word, N% aligned in place, ~N% moved (best place elsewhere in B)')
    text = [f'{"A":>6} {"B":>6} {"size":>5}  match  RosePan (file:function)']
    for ra, rb, size, ok, pct, name in rows:
        match = '=' if ok else f'{pct}%' if pct >= 0 else f'~{-pct}%'
        text.append(f'{ra:6x} {rb:6x} {size:5x}  {match:>5}  {name}')
    gaps = [(x + n, y + n, nx - x - n, ny - y - n) for (x, y, n), (nx, ny, _) in zip(blocks, blocks[1:])
            if nx - x - n or ny - y - n]
    text.append('')
    text.append(f'{"A at":>6} {"B at":>6} {"-A":>5} {"+B":>5}  differing runs, in bytes')
    text += [f'{x * 4:6x} {y * 4:6x} {da * 4:5x} {db * 4:5x}' for x, y, da, db in gaps]
    dest = out or OUT / 'rxsnddrv_cmp.txt'
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text('\n'.join(text) + '\n', encoding='utf-8')
    print(f'{len(gaps)} differing runs; A only {sum(g[2] for g in gaps) * 4:#x} bytes, B only '
          f'{sum(g[3] for g in gaps) * 4:#x} bytes -> {dest}')


# ---- report ---------------------------------------------------------------------------------------
def cmd_report():
    md = modules_dir()
    conf = next((p for p in yamipan.GAME_DATA.iterdir() if p.name.lower() == 'iopconf.cnf'), None)
    loaded = []
    if conf:
        loaded = [l.split()[0].lower() for l in conf.read_text(errors='replace').splitlines()
                  if l.strip() and not l.startswith('-')]
    print(f'game data: {yamipan.GAME_DATA}')
    print(f'iopconf.cnf loads: {", ".join(loaded) or "(none)"}')
    imgs = sorted(p for p in md.rglob('*') if p.suffix.lower() == '.img')
    irxs = sorted(p for p in md.rglob('*') if p.suffix.lower() == '.irx')
    in_img = {}
    for p in imgs:
        buf = p.read_bytes()
        for e in romdir(buf):
            data = buf[e['off']:e['off'] + e['size']]
            info = irx(data)
            if info:
                in_img.setdefault(info['name'], []).append(p.name)
    cmd_list(imgs)
    print()
    print(f'{"file":22} {"module":24} {"ver":>5} {"code KB":>7}  {"loaded":6}  imports')
    for p in irxs:
        info = irx(p.read_bytes())
        if not info:
            continue
        rel = p.relative_to(md).as_posix().lower()
        libs = ', '.join(f'{t["lib"]}({len(t["stubs"])})' for t in info['imports'])
        exp = ''.join(f'  exports {t["lib"]}' for t in info['exports'])
        print(f'{rel:22} {info["name"][:24]:24} {info["version"]:>5} {info.get("text", 0) / 1024:7.1f}  '
              f'{"yes" if "modules/" + rel in loaded else "-":6}  {libs}{exp}')


def main(argv):
    out = Path(argv[argv.index('--out') + 1]) if '--out' in argv else None
    base = int(argv[argv.index('--base') + 1], 0) if '--base' in argv else 0
    args = [a for i, a in enumerate(argv) if not a.startswith('--') and (i == 0 or argv[i - 1] not in ('--out', '--base'))]
    cmd = args[0] if args else 'report'
    files = args[1:]
    if cmd == 'report':
        cmd_report()
    elif cmd == 'list':
        cmd_list(files or sorted(p for p in modules_dir().rglob('*') if p.suffix.lower() == '.img'))
    elif cmd == 'extract':
        cmd_extract(files or sorted(p for p in modules_dir().rglob('*') if p.suffix.lower() == '.img'), out)
    elif cmd == 'irx':
        cmd_irx(files, '--stubs' in argv)
    elif cmd == 'elf':
        cmd_elf(files, base, out)
    elif cmd == 'cmp':
        cmd_cmp(files, out)
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
