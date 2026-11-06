#!/usr/bin/env python3
"""Every event of the game and what starts it, read offline from the disc.

python eventscan.py                  print the stages, films and events
python eventscan.py --write          the same, and write src/game/teleporttable.inc (game/teleport.cpp's table)
python eventscan.py --event d0705    who starts one event

SPK.ROM names every file: dataEU/pks<stage>c<part>EU.enc are the stages and their parts,
demoEU/s<stage>/*.edp the file events. kStageMovies in game/gamepaths.cpp lists the event movies and the
navigator's table (packinitEU.pak, item 1) the film nodes. What SPK.ROM cannot say is which part starts an
event, and the scripted events (`sd_*`, an entry of the stage's script) have no file at all: this unpacks
each stage pack and reads its scripts for the natives that start an event, 0x15f, 0x160, 0x1f2 (file) and
0x155, 0x161, 0x162 (scripted), with the event's name from the script's label table. A file event no
script starts is listed for every part of its stage, with an empty `started by`.
"""
import re
import struct
import sys
from pathlib import Path

import sssall
import sssdis
import yamipan

ROOT = Path(__file__).parent.parent.parent
TABLE = ROOT / 'src' / 'game' / 'teleporttable.inc'
MOVIES = ROOT / 'src' / 'game' / 'gamepaths.cpp'

FILE_NATIVES = {0x15f, 0x160, 0x1f2}
SIMPLE_NATIVES = {0x155, 0x161, 0x162}
LABEL_MAGIC = b'LBL\x1a'
FIRST_LABEL = 3         # ScriptLabel_First: the player, the partner and self are not names
NAVI_ITEM = 1           # kInitItemNaviTable
NODE_SIZE = 0x70
NODE_SINGLE_PART = 1    # kNaviKindSinglePart
STARTERS_SHOWN = 3


def spk():
    """{upper-case path: (archive, offset, size)}."""
    d = (Path(yamipan.GAME_DIR) / 'SPK.ROM').read_bytes()
    count, strings = struct.unpack_from('<II', d, 8)
    files = {}
    for i in range(count):
        name, archive, offset, size = struct.unpack_from('<4I', d, 0x20 + i * 0x10)
        text = d[strings + name:d.index(b'\0', strings + name)].decode('latin1')
        where = d[strings + archive:d.index(b'\0', strings + archive)].decode('latin1')
        files[text] = (where, offset, size & 0x7fffffff)
    return files


def read(entry):
    archive, offset, size = entry
    with open(Path(yamipan.GAME_DIR) / archive.split('/')[-1], 'rb') as f:
        f.seek(offset)
        return f.read(size)


def film_nodes(files):
    """The navigator's single-part nodes, from its table in the init pack."""
    d = read(files['DATAEU/PACKINITEU.PAK'])
    data_base = struct.unpack_from('<i', d, 0xc)[0]
    data, size = struct.unpack_from('<ii', d, 0x20 + NAVI_ITEM * 0x10 + 8)
    table = data_base + data
    count = struct.unpack_from('<i', d, table + 8)[0]
    assert 0x10 + count * NODE_SIZE == size, 'the navigator table has moved'
    return [i for i in range(count)
            if struct.unpack_from('<h', d, table + 0x10 + i * NODE_SIZE)[0] == NODE_SINGLE_PART]


def event_movies():
    """(id, stage, part, branch) of kStageMovies."""
    text = MOVIES.read_text(encoding='utf8')
    return [(m[4], int(m[1]), int(m[2]), int(m[3]))
            for m in re.finditer(r'\{\s*(\d+), (\d), (\d), eventMoviePath(\d{4}) \}', text)]


def labels_of(script):
    """The script's label names by index; {} without a table."""
    blocks = [m.start() for m in re.finditer(LABEL_MAGIC, script)]
    if not blocks:
        return {}
    block = blocks[-1]
    count = struct.unpack_from('<H', script, block + 0xc)[0]
    names = {}
    for i in range(count):
        at = block + struct.unpack_from('<i', script, block + 0x10 + i * 4)[0] + 4
        names[i] = script[at:script.index(b'\0', at)].decode('latin1')
    return names


def starts(script):
    """(entry, native, event name) for each event a script starts."""
    labels = labels_of(script)
    found = []
    for name, pos, size in sssdis.entries(script):
        end = pos + size - 0xc
        for p in range(pos, end, 4):
            word = struct.unpack_from('<I', script, p)[0]
            native = word & 0xffffff
            if word >> 24 != 1 or native not in FILE_NATIVES | SIMPLE_NATIVES:
                continue
            label = struct.unpack_from('<I', script, p + 4)[0]
            if label >= FIRST_LABEL and label in labels:
                found.append((name, native, labels[label]))
    return found


def scan():
    """Stages {stage: part bits}, films [node], events {(stage, name): row}."""
    files = spk()
    stages = {}
    events = {}
    for path, entry in sorted(files.items()):
        m = re.fullmatch(r'DATAEU/PKS(\d\d)C(\d)EU\.ENC', path)
        if not m:
            continue
        stage, part = int(m[1]), int(m[2]) - 1
        stages[stage] = stages.get(stage, 0) | 1 << part
        d = read(entry)
        if d[:4] == b'SLDI':
            d = sssall.sldi(d, 0, len(d))
        for script in sssall.scripts_in(d, len(d)):
            for caller, native, name in starts(script):
                # The name as the script has it: a script entry's is case-sensitive (sd_MT04_08).
                row = events.setdefault((stage, name.lower()), {
                    'name': name,
                    'kind': 'Teleport_FileEvent' if native in FILE_NATIVES else 'Teleport_SimpleEvent',
                    'parts': 0, 'starters': set(), 'branch': 0})
                row['parts'] |= 1 << part
                if re.fullmatch(r'[\w@.-]+', caller):
                    row['starters'].add(caller)

    # A script's file event counts only where its file is; a file no script starts still is an event.
    rows = {}
    for key, row in events.items():
        stage, name = key
        path = f'DEMOEU/S{stage:02d}/{name.upper()}.EDP'
        if row['kind'] == 'Teleport_SimpleEvent' or path in files:
            rows[key] = row
    for path in files:
        m = re.fullmatch(r'DEMOEU/S(\d\d)/(.+)\.EDP', path)
        if m and (int(m[1]), m[2].lower()) not in rows:
            rows[int(m[1]), m[2].lower()] = {'name': m[2].lower(), 'kind': 'Teleport_FileEvent',
                                             'parts': stages[int(m[1])], 'starters': set(), 'branch': 0}
    for number, stage, part, branch in event_movies():
        rows[stage, 'ev' + number] = {'name': 'ev' + number, 'kind': 'Teleport_Movie', 'parts': 1 << part,
                                      'starters': {'the stage clear'}, 'branch': branch}
    return stages, film_nodes(files), rows


def order(key):
    """By stage; the numbered events and movies in their number's order, the scripted ones after them."""
    stage, name = key
    number = re.fullmatch(r'(?:d|ev)(\d{4})(\w?)', name)
    return (stage, 0, number[1], number[2]) if number else (stage, 1, name, '')


def table(stages, films, rows):
    out = ['// Generated by tools/reverse/eventscan.py --write from the disc: do not edit. game/teleport.cpp includes it.',
           '',
           'static const TeleportStage kTeleportStages[] = {']
    out += [f'    {{ {stage}, {parts} }},' for stage, parts in sorted(stages.items())]
    out += ['};', '', 'static const short kTeleportFilms[] = {']
    out += ['    ' + ', '.join(str(n) for n in films[i:i + 10]) + ',' for i in range(0, len(films), 10)]
    out += ['};', '', 'static const TeleportEvent kTeleportEvents[] = {']
    for key in sorted(rows, key=order):
        stage = key[0]
        row = rows[key]
        name = row['name']
        starters = sorted(row['starters'])
        text = ', '.join(starters[:STARTERS_SHOWN]) + (' ...' if len(starters) > STARTERS_SHOWN else '')
        out.append(f'    {{ "{name}", {stage}, {row["parts"]}, {row["kind"]}, {row["branch"]}, "{text}" }},')
    out += ['};', '']
    return '\n'.join(out)


def main():
    args = sys.argv[1:]
    stages, films, rows = scan()
    if '--event' in args:
        wanted = args[args.index('--event') + 1].lower()
        for (stage, name), row in sorted(rows.items()):
            if name == wanted:
                print(f'stage {stage}: {row}')
        return
    print(f'{len(stages)} stages, {sum(bin(p).count("1") for p in stages.values())} parts, {len(films)} films')
    for kind in ('Teleport_FileEvent', 'Teleport_SimpleEvent', 'Teleport_Movie'):
        print(f'{kind}: {sum(1 for r in rows.values() if r["kind"] == kind)}')
    print('no script starts:', ', '.join(n for (_, n), r in sorted(rows.items()) if not r['starters']))
    if '--write' in args:
        TABLE.write_text(table(stages, films, rows), encoding='utf8', newline='\n')
        print(f'wrote {TABLE}')


if __name__ == '__main__':
    main()
