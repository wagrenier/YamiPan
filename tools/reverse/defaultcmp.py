#!/usr/bin/env python3
"""The script runner's inline defaults: does each host virtual have the ROM's shape, in the ROM's slot?

python defaultcmp.py            every `FS2 0x...` virtual of game/scriptrunner.h that sits in a runner table
python defaultcmp.py --plant    the same with one body swapped, to prove the check sees a wrong one

A default is a few words, and nothing runs most of them, so no smoke run says anything: this reads both
sides. ROM (the ELF): the body of the slot, as one of
    empty       jr ra
    zero        jr ra / move v0,zero
    tail        j defaultCommand (a void or an int)
    bool        jal defaultCommand; sltu v0,zero,v0
    then-zero   jal defaultCommand; move v0,zero
    logs-zero   jal unsupportedCommand; move v0,zero
host (the header): the virtual's body, as the same words. A ROM function in no runner table is skipped,
and so is a slot whose ROM body has none of these shapes. A host bool on a ROM tail jump is a note, not a
difference: both answer 0. The other way round is the bug this was written for (a host void on a ROM
bool: the natives stored 0 where the ROM stores the answer, 2026-11-07).
Also checked: a `ROM vptr +0x..` or `+0x..` comment right above says the slot the ROM's table has.
"""
import json
import re
import struct
import sys

import yamipan

HDR = yamipan.REPO / 'src' / 'game' / 'scriptrunner.h'
BASE = 0x3147ac                 # StageObject's own runner table
DEFAULT, UNSUPPORTED = 0x1cf1e0, 0x1cf1b0


def image():
    data = open(yamipan.ELF, 'rb').read()
    ph, n = struct.unpack_from('<I', data, 28)[0], struct.unpack_from('<H', data, 44)[0]
    for i in range(n):
        t, off, va, _, fs, *_ = struct.unpack_from('<8I', data, ph + i * 32)
        if t == 1:
            return lambda a: struct.unpack_from('<I', data, off + a - va)[0]
    raise SystemExit('no load segment')


def rom_shape(word, a):
    w = [word(a + 4 * i) for i in range(8)]
    JR_RA, NOP, MOVE_V0_ZERO, SLTU, MOVE2 = 0x03e00008, 0, 0x0000102d, 0x0002102b, 0x0000102d
    if w[0] == JR_RA:
        return 'zero' if w[1] == MOVE_V0_ZERO else 'empty' if w[1] == NOP else None
    if w[0] >> 26 == 2:             # j
        return 'tail' if (w[0] & 0x3ffffff) << 2 == DEFAULT else None
    if w[0] >> 16 == 0x27bd and w[2] >> 26 == 3:   # addiu sp; sd ra; jal
        target = (w[2] & 0x3ffffff) << 2
        if w[5] == SLTU and target == DEFAULT:
            return 'bool'
        if w[5] == MOVE2:
            return {DEFAULT: 'then-zero', UNSUPPORTED: 'logs-zero'}.get(target)
    return None


def host_shape(body):
    body = re.sub(r'\(void\)\w+;', '', body)
    body = re.sub(r'\s+', ' ', body).strip()
    return {
        '': 'empty',
        'return 0;': 'zero', 'return nullptr;': 'zero', 'return false;': 'zero', 'return 0.0f;': 'zero',
        'return defaultCommand();': 'tail', 'defaultCommand();': 'tail',
        'return defaultCommand() != 0;': 'bool',
        'defaultCommand(); return nullptr;': 'then-zero',
        'unsupportedCommand(); return nullptr;': 'logs-zero',
    }.get(body, body)


def main():
    plant = '--plant' in sys.argv
    word = image()
    classes = json.load(open(yamipan.WORK / 'classes.json', encoding='utf8'))
    base = next(v for k in classes for v in k['vtables'] if v['addr'] == BASE)
    slot_of = {s['addr']: s['index'] for s in base['slots']}
    lines = HDR.read_text(encoding='utf-8').split('\n')
    text = '\n'.join(lines)
    seen = bad = same = 0
    covered = set()
    for i, line in enumerate(lines):
        m = re.search(r'^\s*// FS2 0x([0-9a-f]{8})\s*$', line)
        if not m or int(m[1], 16) not in slot_of:
            continue
        addr = int(m[1], 16)
        j = i + 1
        while lines[j].strip().startswith('//'):
            j += 1
        chunk = '\n'.join(lines[j:j + 12])
        v = re.match(r'\s*virtual\s[^{;]*\{', chunk)
        if v is None:
            print(f'0x{addr:06x} no inline virtual after the annotation: line {j + 1}')
            bad += 1
            continue
        k = v.end()
        depth, e = 1, k
        while depth:
            depth += {'{': 1, '}': -1}.get(chunk[e], 0)
            e += 1
        name = re.search(r'(\w+)\(', chunk).group(1)
        rom, host = rom_shape(word, addr), host_shape(chunk[k:e - 1])
        if plant and name == 'scriptUseSmoke':
            host = 'zero'
        if rom is None:
            continue                # slot 0: a real function, the base's own
        seen += 1
        covered.add(addr)
        if rom == 'tail' and host == 'bool':
            same += 1               # 0 either way, and an override's answer passes through
            print(f'0x{addr:06x} slot {slot_of[addr]} {name}: note, a tail jump in the ROM, bool here')
        elif rom != host:
            bad += 1
            print(f'0x{addr:06x} slot {slot_of[addr]} {name}: ROM {rom}, host {host!r}')
        slot = 8 + 4 * slot_of[addr]
        above = ' '.join(lines[max(0, i - 2):i])
        said = re.findall(r'ROM vptr ((?:\+0x[0-9a-f]+(?:, )?)+)', above)
        if len(said) == 1 and name in chunk and said[0].count('+') == 1 and int(said[0].strip(', ')[1:], 16) != slot:
            bad += 1
            print(f'0x{addr:06x} {name}: comment says {said[0]}, the table says +0x{slot:x}')
    missing = [a for a in slot_of if 0x171ad0 <= a < 0x1723c0 and a not in covered]
    print(f'{seen} defaults compared, {bad} differences, {same} notes; slots of the range with no inline virtual here: '
          + (', '.join(f'0x{a:06x}' for a in sorted(missing)) or 'none'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
