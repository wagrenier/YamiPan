"""Dump what the offline tools need from Ghidra into work/.

python fetch.py

Writes work/hash_fs2.json (function starts and names; opcode hashes when GhidraMCP 5.14.2 answers) and
work/fs2_tags.json (the provenance tag of every tagged function). Read-only.

GhidraMCP 2.0.0 has no bulk hashes: the function list comes from its paged `functions`, and "hash" stays
empty unless 5.14.2's get_bulk_function_hashes answers (nothing downstream needs it yet). Tags come from
data/tags.json, the record the naming tools keep (yamipan.set_tag), so they need no Ghidra.
"""
import json

import yamipan

funcs = None
if yamipan.ghidra5_up():
    try:
        funcs = json.loads(yamipan.http5('GET', 'get_bulk_function_hashes', filter='all', limit=50000))['functions']
    except (ValueError, KeyError, yamipan.GhidraError):
        funcs = None
if funcs is None:
    funcs = [{'address': f['address'], 'name': f['name'], 'hash': '', 'instruction_count': 0}
             for f in yamipan.paged('functions')]
yamipan.WORK.mkdir(parents=True, exist_ok=True)
yamipan.save_json(yamipan.WORK / 'hash_fs2.json', {'functions': funcs}, indent=None)
print(f'{yamipan.PROGRAM}: {len(funcs)} functions')

tags = {a: t for a, t in yamipan.tags().items() if t in yamipan.ALL_TAGS}
yamipan.save_json(yamipan.WORK / 'fs2_tags.json', tags)
print(f'{yamipan.PROGRAM} tags: {len(tags)}')
