"""Decompile functions over GhidraMCP: python dec.py <addr|name> [...]"""
import sys

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

for target in sys.argv[1:]:
    print(f'===== {yamipan.PROGRAM} {target}')
    print(yamipan.decompile(target))
    print()
