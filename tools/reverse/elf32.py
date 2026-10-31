"""Minimal little-endian ELF32 reader: sections, VA->bytes, and the MIPS _gp from .reginfo.

An ELF whose loaded image is one unnamed section (FS2's SCES_538.51) gets the regions of data/layout.json
"image_sections" (yamipan.image_sections(), from architecture.md's address map) in its place, so
section_of() still tells code from data.
"""
import os
import struct


def _same(a, b):
    try:
        return os.path.samefile(a, b)
    except OSError:
        return False


class Elf:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.data = f.read()
        d = self.data
        assert d[:4] == b'\x7fELF' and d[4] == 1 and d[5] == 1, 'expected little-endian ELF32'
        (self.e_shoff,) = struct.unpack_from('<I', d, 0x20)
        self.e_shentsize, self.e_shnum, self.e_shstrndx = struct.unpack_from('<HHH', d, 0x2e)
        (self.e_phoff,) = struct.unpack_from('<I', d, 0x1c)
        self.e_phentsize, self.e_phnum = struct.unpack_from('<HH', d, 0x2a)
        raw = []
        for i in range(self.e_shnum):
            raw.append(struct.unpack_from('<IIIIIIIIII', d, self.e_shoff + i * self.e_shentsize))
        strtab_off = raw[self.e_shstrndx][4]
        self.sections = []
        for name_off, typ, flags, addr, off, size, *_ in raw:
            end = d.index(b'\0', strtab_off + name_off)
            name = d[strtab_off + name_off:end].decode('ascii', 'replace')
            self.sections.append({'name': name, 'type': typ, 'flags': flags,
                                  'addr': addr, 'offset': off, 'size': size})
        # Loadable segments give the authoritative VA->file mapping.
        self.segments = []
        for i in range(self.e_phnum):
            p_type, p_off, p_vaddr, _, p_filesz, p_memsz, *_ = struct.unpack_from(
                '<IIIIIIII', d, self.e_phoff + i * self.e_phentsize)
            if p_type == 1:
                self.segments.append((p_vaddr, p_off, p_filesz, p_memsz))
        if not any(x['name'] for x in self.sections if x['addr']):
            import yamipan          # only the game's own ELF: its address map says nothing of others
            regions = yamipan.image_sections() if _same(path, yamipan.ELF) else []
            if regions:
                self.sections = [x for x in self.sections if not x['addr']]
                for r in regions:
                    r['offset'] = self.offset(r['addr'])
                    self.sections.append(r)
        ri = self.section('.reginfo')
        self.gp = struct.unpack_from('<I', d, ri['offset'] + 20)[0] if ri else None

    def section(self, name):
        return next((s for s in self.sections if s['name'] == name), None)

    def section_of(self, va):
        for s in self.sections:
            if s['addr'] and s['addr'] <= va < s['addr'] + s['size'] and s['flags'] & 2:
                return s['name']
        return None

    def offset(self, va):
        for vaddr, off, filesz, _ in self.segments:
            if vaddr <= va < vaddr + filesz:
                return off + (va - vaddr)
        return None

    def read(self, va, n):
        o = self.offset(va)
        return None if o is None else self.data[o:o + n]

    def u32(self, va):
        b = self.read(va, 4)
        return None if b is None or len(b) < 4 else struct.unpack('<I', b)[0]

    def cstring(self, va, max_len=512):
        o = self.offset(va)
        if o is None:
            return None
        end = self.data.find(b'\0', o, o + max_len)
        if end < 0:
            return None
        return self.data[o:end]
