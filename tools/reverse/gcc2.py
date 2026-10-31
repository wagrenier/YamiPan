"""GCC 2.x ("gnu v2") name mangling: demangle and mangle, for the names Rule of Rose's ELF keeps.

python gcc2.py <mangled> [...]            demangle each
python gcc2.py --mangle "Class::method(int, float *) const" [...]
python gcc2.py --selftest                 demangle every named section of the ELF, report failures

RoR was built with an ee-gcc 2.9x (the same ABI family as Fatal Frame 2's 2.96-ee), whose C++ names look like
  reset__9DogEntity              DogEntity::reset(void)
  getId__C8DogActor              DogActor::getId(void) const
  setId__8DogActori              DogActor::setId(int)
  __9DogEntityi                  DogEntity::DogEntity(int)          (constructor)
  _$_9DogEntity                  DogEntity::~DogEntity(void)        (destructor; '$' is the CPLUS_MARKER)
  __tf9DogEntity                 DogEntity type_info function       (builds the RTTI node on first call)
  _vt$9DogEntity                 DogEntity virtual table            (_vt$Derived$Base: a secondary one)
  __eq__C9DogEntityRC9DogEntity  DogEntity::operator==(DogEntity const &) const
  func__FPCci                    func(char const *, int)            (a free function)
  _9DogEntity$count              DogEntity::count                   (a static data member)
  Q23Foo3Bar                     Foo::Bar                           (qualified name, Q_12_ for 10+ parts)
  t6vector1Zi                    vector<int>                        (template; Z = type argument)

demangle(m) returns a Demangled(kind, qual, name, params, const, display) or None when the name is not
GCC 2 mangled (plain C names, Ghidra's FUN_ names). ghidra_name() is the project's Ghidra spelling:
the qualified name without parameters ('DogEntity::reset', 'DogEntity::~DogEntity'); overloads that
would collide get their parameter list without spaces ('DogEntity::reset(int,float*)').
"""
import re
import sys
from collections import namedtuple

Demangled = namedtuple('Demangled', 'kind qual name params const display')

BUILTIN = {'v': 'void', 'b': 'bool', 'c': 'char', 'w': 'wchar_t', 's': 'short', 'i': 'int', 'l': 'long',
           'x': 'long long', 'f': 'float', 'd': 'double', 'r': 'long double', 'e': '...'}
OPERATORS = {
    'nw': 'new', 'dl': 'delete', 'vn': 'new[]', 'vd': 'delete[]', 'as': '=', 'eq': '==', 'ne': '!=',
    'lt': '<', 'gt': '>', 'le': '<=', 'ge': '>=', 'pl': '+', 'mi': '-', 'ml': '*', 'dv': '/', 'md': '%',
    'apl': '+=', 'ami': '-=', 'aml': '*=', 'adv': '/=', 'amd': '%=', 'pp': '++', 'mm': '--', 'nt': '!',
    'co': '~', 'ad': '&', 'or': '|', 'er': '^', 'aad': '&=', 'aor': '|=', 'aer': '^=', 'ls': '<<',
    'rs': '>>', 'als': '<<=', 'ars': '>>=', 'aa': '&&', 'oo': '||', 'cm': ',', 'vc': '[]', 'cl': '()',
    'rf': '->', 'rm': '->*', 'cn': '?:', 'mx': '>?', 'mn': '<?'}
OP_CODES = {v: k for k, v in OPERATORS.items()}


class _P:
    """A cursor over a mangled string; every parse_* returns text and advances."""

    def __init__(self, s, i=0):
        self.s, self.i = s, i
        self.types = []                 # argument types seen so far, for T<n> / N<count><n>

    def peek(self, n=1):
        return self.s[self.i:self.i + n]

    def eof(self):
        return self.i >= len(self.s)

    def number(self):
        m = re.match(r'\d+', self.s[self.i:])
        if not m:
            raise ValueError(f'number expected at {self.i} in {self.s!r}')
        self.i += len(m.group(0))
        return int(m.group(0))

    def ident(self):
        n = self.number()
        if self.i + n > len(self.s):
            raise ValueError('identifier runs past the end')
        out = self.s[self.i:self.i + n]
        self.i += n
        return out

    def class_name(self):
        """<len><name> | Q<parts> | t<template> -> 'Foo::Bar<int>'."""
        c = self.peek()
        if c == 'Q':
            self.i += 1
            if self.peek() == '_':
                self.i += 1
                count = self.number()
                if self.peek() == '_':
                    self.i += 1
            else:
                count = int(self.peek())
                self.i += 1
            return '::'.join(self.class_name() for _ in range(count))
        if c == 't':
            self.i += 1
            name = self.ident()
            nargs = self.number()
            args = [self.template_arg() for _ in range(nargs)]
            return f'{name}<{", ".join(args)}>'
        if c == 'K':                    # remembered class (rare in 2.9x)
            raise ValueError('K back-reference not supported')
        if c.isdigit():
            return self.ident()
        raise ValueError(f'class name expected at {self.i} in {self.s!r}')

    def template_arg(self):
        if self.peek() == 'Z':
            self.i += 1
            return self.type()
        t = self.type()                 # a non-type argument: its type, then the value
        if t in ('bool',):
            v = self.s[self.i]
            self.i += 1
            return 'true' if v == '1' else 'false'
        if t.endswith('*') or t.endswith('&'):
            return '&' + self.ident() if self.peek().isdigit() else '?'
        neg = self.peek() == 'm'
        if neg:
            self.i += 1
        v = self.number()
        if self.peek() == '_' and t not in ('char',):
            self.i += 1
        return ('-' if neg else '') + str(v)

    def type(self):
        """One type -> its C++ spelling."""
        quals = []
        while self.peek() in ('C', 'V', 'U', 'S') and not self.eof():
            c = self.peek()
            nxt = self.s[self.i + 1:self.i + 2]
            if c == 'S' and nxt not in BUILTIN:     # not 'signed X': stop
                break
            self.i += 1
            quals.append({'C': 'const', 'V': 'volatile', 'U': 'unsigned', 'S': 'signed'}[c])
        c = self.peek()
        if c == '':
            raise ValueError('type expected at end')
        if c in BUILTIN:
            self.i += 1
            base = BUILTIN[c]
            sign = [q for q in quals if q in ('unsigned', 'signed')]
            cv = [q for q in quals if q not in ('unsigned', 'signed')]
            return ' '.join(sign + [base] + cv)
        cv = ' '.join(q for q in quals if q in ('const', 'volatile'))
        if c in ('P', 'R'):
            self.i += 1
            inner = self.type()
            star = '*' if c == 'P' else '&'
            if inner.endswith(')') and '(*' not in inner and '(' in inner:      # pointer to function
                ret, _, rest = inner.partition('(')
                out = f'{ret}({star})({rest}'
            elif inner.endswith(']'):                                          # pointer to array
                base, _, dims = inner.partition('[')
                out = f'{base} ({star})[{dims}'
            else:
                out = inner + f' {star}'
            return out + (' ' + cv if cv else '')
        if c == 'A':
            self.i += 1
            n = self.number()
            if self.peek() == '_':
                self.i += 1
            return f'{self.type()}[{n}]'
        if c == 'F':
            self.i += 1
            args = self.arg_list(stop='_')
            self.i += 1                                     # the '_' before the return type
            ret = self.type()
            return f'{ret}({", ".join(args)})'
        if c == 'M':
            self.i += 1
            cls = self.class_name()
            const = ''
            if self.peek() == 'C':
                self.i += 1
                const = ' const'
            if self.peek() == 'F':
                self.i += 1
                args = self.arg_list(stop='_')
                self.i += 1
                ret = self.type()
                return f'{ret} ({cls}::*)({", ".join(args)}){const}'
            return f'{self.type()} {cls}::*'
        if c in ('T', 'N'):
            raise ValueError('repeat code inside a type')
        if c == 'G':                                        # old marker before a class name
            self.i += 1
            return self.type()
        name = self.class_name()
        return name + (' ' + cv if cv else '')

    def arg_list(self, stop=None):
        args = []
        while not self.eof() and (stop is None or self.peek() != stop):
            c = self.peek()
            if c == 'T':
                self.i += 1
                n = self.single_or_number()
                t = self.types[n]
                args.append(t)
                self.types.append(t)
            elif c == 'N':
                self.i += 1
                count = self.single_or_number()
                n = self.single_or_number()
                for _ in range(count):
                    args.append(self.types[n])
                    self.types.append(self.types[n])
            else:
                t = self.type()
                args.append(t)
                self.types.append(t)
        return args

    def single_or_number(self):
        """T/N use one digit, or several digits closed by '_'."""
        m = re.match(r'(\d+)_', self.s[self.i:])
        if m and len(m.group(1)) > 1:
            self.i += len(m.group(0))
            return int(m.group(1))
        v = int(self.s[self.i])
        self.i += 1
        return v


def _params(p):
    args = p.arg_list()
    if args == ['void']:
        args = []
    return args


def demangle(m):
    """Demangled(...) for a GCC 2 mangled name, or None."""
    if not m or m.startswith(('FUN_', 'thunk_FUN_', 'LAB_', 'DAT_')):
        return None
    try:
        return _demangle(m)
    except (ValueError, IndexError, KeyError):
        return None


def _member(p, kind, name):
    """After '__': [C]<class><args> (member), F<args> (free), S<class>... (static member)."""
    const = False
    static = False
    if p.peek() == 'F':
        p.i += 1
        params = _params(p)
        return Demangled(kind, '', name, params, False, f'{name}({", ".join(params)})')
    if p.peek() == 'S' and (p.s[p.i + 1:p.i + 2].isdigit() or p.s[p.i + 1:p.i + 2] in 'Qt'):
        p.i += 1
        static = True
    if p.peek() == 'C' and (p.s[p.i + 1:p.i + 2].isdigit() or p.s[p.i + 1:p.i + 2] in 'Qt'):
        p.i += 1
        const = True
    cls = p.class_name()
    p.types.append(cls)                 # T0 can name the class of a member function
    params = _params(p)
    if kind == 'ctor':
        name = cls.split('::')[-1].split('<')[0]
    disp = f'{cls}::{name}({", ".join(params)})' + (' const' if const else '') + (' [static]' if static else '')
    return Demangled(kind, cls, name, params, const, disp)


def _demangle(m):
    marker = re.match(r'_[$.]_(.+)$', m)                    # destructor
    if marker:
        p = _P(marker.group(1))
        cls = p.class_name()
        if not p.eof():
            raise ValueError('trailing text after a destructor')
        name = '~' + cls.split('::')[-1].split('<')[0]
        return Demangled('dtor', cls, name, [], False, f'{cls}::{name}()')
    for prefix, kind in (('_vt$', 'vtable'), ('_vt.', 'vtable')):
        if m.startswith(prefix):
            parts = []
            p = _P(m[len(prefix):])
            while not p.eof():
                parts.append(p.class_name())
                if p.peek() in ('$', '.'):
                    p.i += 1
            qual = parts[0]
            disp = f'{qual} virtual table' + (f' for {"::".join(parts[1:])}' if len(parts) > 1 else '')
            return Demangled(kind, qual, '_vt', parts[1:], False, disp)
    for prefix, kind in (('__tf', 'typeinfo_fn'), ('__ti', 'typeinfo')):
        if m.startswith(prefix) and len(m) > 4:
            p = _P(m[4:])
            t = p.type()
            if not p.eof():
                raise ValueError('trailing text after a type_info name')
            return Demangled(kind, t, prefix[2:], [], False, f'{t} type_info {"function" if kind == "typeinfo_fn" else "node"}')
    g = re.match(r'_GLOBAL_[$.]([ID])[$.](.*)$', m)
    if g:
        return Demangled('global_' + ('ctor' if g.group(1) == 'I' else 'dtor'), '', m, [], False,
                         f'global {"constructors" if g.group(1) == "I" else "destructors"} keyed to {g.group(2)}')
    thunk = re.match(r'__thunk_(\d+)_(.+)$', m)
    if thunk:
        inner = demangle(thunk.group(2))
        if inner:
            return inner._replace(kind='thunk', display=f'thunk (this - {thunk.group(1)}) to {inner.display}')
    sd = re.match(r'_((?:\d|Q|t).*?)[$.](\w+)$', m)          # static data member _9Class$name
    if sd and not m.startswith('__'):
        try:
            p = _P(sd.group(1))
            cls = p.class_name()
            if p.eof():
                return Demangled('data', cls, sd.group(2), [], False, f'{cls}::{sd.group(2)}')
        except ValueError:
            pass
    if m.startswith('__') and len(m) > 2:
        rest = m[2:]
        if rest[0].isdigit() or rest[0] in 'Qt':           # constructor __<class><args>
            return _member(_P(rest), 'ctor', '')
        op = re.match(r'(op)(.+?)__(.*)$', rest)           # conversion operator __op<type>__<class>
        if op:
            t = _P(op.group(2)).type()
            d = _member(_P(op.group(3)), 'method', f'operator {t}')
            return d
        om = re.match(r'([a-z]{2,3})__(.*)$', rest)
        if om and om.group(1) in OPERATORS:
            op = OPERATORS[om.group(1)]
            return _member(_P(om.group(2)), 'method', 'operator' + (' ' if op[0].isalpha() else '') + op)
    # name__<signature>: the LAST '__' that starts a valid signature (overlapping: a method whose name
    # ends in '_' gives 's2dBgDispOn___5S2dBgi')
    for k in [k for k in range(len(m) - 1) if m[k:k + 2] == '__'][::-1]:
        if k == 0:
            continue
        name, sig = m[:k], m[k + 2:]
        if not sig:
            continue
        try:
            d = _member(_P(sig), 'method', name)
            return d
        except (ValueError, IndexError):
            continue
    return None


def ghidra_name(d, overloaded=False):
    """The project's Ghidra spelling of a demangled name (see the module doc)."""
    if d is None:
        return None
    if d.kind in ('typeinfo_fn', 'typeinfo', 'vtable', 'data', 'global_ctor', 'global_dtor'):
        return {'typeinfo_fn': f'{d.qual}::__tf', 'typeinfo': f'{d.qual}::__ti', 'vtable': f'{d.qual}::_vt',
                'data': f'{d.qual}::{d.name}'}.get(d.kind, d.name)
    base = f'{d.qual}::{d.name}' if d.qual else d.name
    if overloaded:
        base += '(' + ','.join(p.replace(' ', '') for p in d.params) + ')' + ('const' if d.const else '')
    return re.sub(r'\s+', '_', base)        # Ghidra rejects whitespace in names ('operator_int')


# ---- mangling (for names a session writes into Ghidra or a header) ------------------------------

_TYPE_CODES = {'void': 'v', 'bool': 'b', 'char': 'c', 'wchar_t': 'w', 'short': 's', 'int': 'i', 'long': 'l',
               'long long': 'x', 'float': 'f', 'double': 'd', 'long double': 'r', '...': 'e'}


def _mangle_class(name):
    parts = [x for x in name.split('::') if x]
    enc = ''.join(f'{len(x)}{x}' for x in parts)
    if len(parts) == 1:
        return enc
    return (f'Q{len(parts)}' if len(parts) < 10 else f'Q_{len(parts)}_') + enc


def mangle_type(t, top=True):
    """One parameter type -> its code. A struct / class passed by value gets GCC 2's 'G' marker, an enum
    does not; the name cannot tell them apart, so write 'struct X' for a by-value struct."""
    t = re.sub(r'\s+', ' ', t.strip())
    by_value_struct = top and re.match(r'(struct|class) ', t) is not None
    t = re.sub(r'^(struct|class|enum) ', '', t)
    t = re.sub(r'\*\s*const$', '*', t)        # T *const: a top-level const is not part of a signature
    arr = re.match(r'(.*?)\s*\(([*&])\)\s*((?:\[\d+\])+)$', t)    # float (*)[3][3], float (&)[3]
    if arr:
        dims = ''.join(f'A{n}_' for n in re.findall(r'\[(\d+)\]', arr.group(3)))
        return ('P' if arr.group(2) == '*' else 'R') + dims + mangle_type(arr.group(1), False)
    if t.endswith('&'):
        return 'R' + mangle_type(t[:-1], False)
    if t.endswith('*'):
        return 'P' + mangle_type(t[:-1].strip(), False)
    if t.startswith('const ') and ' ' in t[6:] and not t[6:].startswith(('unsigned', 'signed', 'long')):
        t = t[6:] + ' const'                  # 'const Foo' == 'Foo const'
    words = t.split(' ')
    prefix = ''
    if 'const' in words:
        prefix += 'C'
        words.remove('const')
    if 'volatile' in words:
        prefix += 'V'
        words.remove('volatile')
    if 'unsigned' in words:
        prefix += 'U'
        words.remove('unsigned')
        if not words:
            words = ['int']
    if 'signed' in words:
        words.remove('signed')
        if words == ['char']:
            prefix += 'S'
        if not words:
            words = ['int']
    base = ' '.join(words)
    base = {'u_char': 'unsigned char', 'u_short': 'unsigned short', 'u_int': 'unsigned int'}.get(base, base)
    if base.startswith('unsigned '):
        return prefix + 'U' + _TYPE_CODES[base[9:]]
    if base in _TYPE_CODES:
        return prefix + _TYPE_CODES[base]
    return prefix + ('G' if by_value_struct and not prefix else '') + _mangle_class(base)


def _compress(codes, first_index):
    """GCC 2's repeats: an argument equal to the one before it extends a run (T<i> for one repeat,
    N<count><i> for more); one equal to an earlier non-adjacent argument is T<i>. int / float-like
    builtins (one letter, optionally U/S-prefixed, except bool) are never compressed."""
    def simple(c):
        return re.fullmatch(r'[US]?[cswilxfdr]', c) is not None

    def idx(i):
        return str(i) if i < 10 else f'{i}_'
    out, k = [], 0
    while k < len(codes):
        c = codes[k]
        if not simple(c) and c in codes[:k]:
            j = codes.index(c)
            run = 1
            if j == k - 1 or (k > 0 and codes[k - 1] == c):
                while k + run < len(codes) and codes[k + run] == c:
                    run += 1
            out.append(f'T{idx(first_index + codes.index(c))}' if run == 1
                       else f'N{idx(run)}{idx(first_index + codes.index(c))}')
            k += run
            continue
        out.append(c)
        k += 1
    return ''.join(out)


_KEYWORDS = {'const', 'volatile', 'unsigned', 'signed', 'void', 'bool', 'char', 'wchar_t', 'short', 'int',
             'long', 'float', 'double'}


def _strip_param_name(p):
    """'int count' -> 'int', 'Foo *obj' -> 'Foo *', 'u_int' -> 'u_int' (a lone type stays)."""
    m = re.match(r'(.*?[\w*&\s])\s*\b([A-Za-z_]\w*)$', p)
    if m and m.group(2) not in _KEYWORDS and re.search(r'[A-Za-z_*&]', m.group(1)) \
            and m.group(1).strip() not in ('struct', 'class', 'enum'):
        return m.group(1).strip()
    return p


def mangle(sig):
    """'Class::method(int, float *) const' -> 'method__C5Classi Pf'-style GCC 2 symbol (no T/N compression:
    GCC 2 compresses repeated class arguments, so a name with repeated class-typed parameters may differ
    from the ROM's; verify against an existing symbol when it matters)."""
    m = re.match(r'\s*(.*?)\s*\((.*)\)\s*(const)?\s*$', sig)
    if not m:
        raise ValueError(f'not a signature: {sig!r}')
    full, params, const = m.group(1), m.group(2).strip(), bool(m.group(3))
    plist = [] if params in ('', 'void') else [p.strip() for p in params.split(',')]
    codes = [mangle_type(_strip_param_name(p)) for p in plist]
    penc = _compress(codes, 1 if '::' in full else 0)
    if '::' in full:
        cls, name = full.rsplit('::', 1)
        short = cls.split('::')[-1]
        if name == short:
            return f'__{_mangle_class(cls)}{penc}'
        if name == '~' + short:
            return f'_$_{_mangle_class(cls)}'
        if name.startswith('operator'):
            name = '__' + OP_CODES[name[8:].strip()]
        return f'{name}__{"C" if const else ""}{_mangle_class(cls)}{penc}'
    return f'{full}__F{penc or "v"}'


def _selftest():
    import yamipan
    from elf32 import Elf
    e = Elf(str(yamipan.ELF))
    bad, n = [], 0
    for s in e.sections:
        for pre in ('.gnu.linkonce.t.', '.gnu.linkonce.d.'):
            if s['name'].startswith(pre):
                n += 1
                d = demangle(s['name'][len(pre):])
                if d is None:
                    bad.append(s['name'][len(pre):])
    print(f'{n - len(bad)}/{n} section names demangle')
    for b in bad[:60]:
        print('  FAILED', b)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    argv = sys.argv[1:]
    if not argv:
        sys.exit(__doc__)
    if argv[0] == '--selftest':
        sys.exit(_selftest())
    if argv[0] == '--mangle':
        for a in argv[1:]:
            print(f'{a}  ->  {mangle(a)}')
        sys.exit(0)
    for a in argv:
        d = demangle(a)
        print(f'{a}  ->  {d.display if d else "(not GCC 2 mangled)"}')
