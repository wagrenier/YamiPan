"""Dump what the offline tools need from Ghidra into work/.

python fetch.py

Writes work/hash_fs2.json (every function's start, name, instruction count and normalized opcode hash,
from GhidraMCP's paged get_function_hash) and work/fs2_tags.json (the provenance tag of every tagged
function). Read-only. Tags come from data/tags.json, the record the naming tools keep
(yamipan.set_tag), so they need no Ghidra.
"""
import yamipan

funcs = [{'address': f['address'], 'name': f['name'], 'hash': f.get('hash', ''),
          'instruction_count': f.get('instruction_count', 0)}
         for f in yamipan.paged('get_function_hash', 'functions')]
yamipan.WORK.mkdir(parents=True, exist_ok=True)
yamipan.save_json(yamipan.WORK / 'hash_fs2.json', {'functions': funcs}, indent=None)
print(f'{yamipan.PROGRAM}: {len(funcs)} functions')

tags = {a: t for a, t in yamipan.tags().items() if t in yamipan.ALL_TAGS}
yamipan.save_json(yamipan.WORK / 'fs2_tags.json', tags)
print(f'{yamipan.PROGRAM} tags: {len(tags)}')
