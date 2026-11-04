# Writing the game's code into YamiPan

How a function goes from Ghidra into `src/`. Accuracy to the ROM beats elegance: this is a
reconstruction, not a rewrite. The rules come from ReiPan (FF3, GCC 3, ported from FF2's code) and
RosePan (RoR, GCC 2.9x, no source to port from); they hold for any PS2 C/C++ game. `<TAG>` below is the
game's annotation tag (open-items.md: chosen at bootstrap, like `FF3` and `ROR`). When something here
proves wrong or incomplete, fix this file.

## Where code goes

- `yamipan_functions.txt` (export.py) gives each function's **inferred file**; the TU key is that path
  under `src/`. Keep the game's own directory layout and file names (its `__FILE__` strings) when it has
  them, case included.
- **Anchored is a fact, filled is a guess.** A function that references its file's path string is in
  that file; functions between two anchors, a class's methods placed by majority, a file named after a
  sister game's link-order slot are not proven (FF3's `graphics/motion/anicode.c` held menu-window
  code). Weigh the brief's identity section (`tuid.py`: strings, classes, names, callers) and pin the
  extent: `tus.py define <file> <lo>-<hi> --why "..."` (hi exclusive; a pinned TU is exactly its
  ranges). Naming the file must not stall the decompilation.
- Files the game does not name (no string): a descriptive file in the directory the code serves,
  recorded with `define --why` and in architecture.md.
- Inline methods and template instances (linkonce sections, singleton getters) have no file of their
  own: write them in their class's header (inline, as the original was) or its .cpp, annotated either way.
- Library code (`data/layout.json` lib_ranges: Sony SDK, kernel stubs, libgcc, newlib, the C++ runtime)
  is never reconstructed: `src/sdk/` replaces the SDK, the host libc the rest. Its names matter only so
  callers read right.
- Never put game code under `src/yamipan/` or `src/sdk/` (the platform layer). Platform changes are
  allowed when needed; keep them minimal and say so in the commit summary.

## Every function carries its address

```cpp
// ──────────────────────────────────────────────────────────────────────
// One line on what it does and why, in the game's terms.
// <TAG> 0x0010eebc; the actor's constructor (stores its vtable at +0x94).

Actor::Actor(...)
{
```

- `<TAG> 0x<8 hex digits>` is load-bearing: status / tus count reconstructed functions by it,
  callcheck compares callees at that address, stubgen and the brief find the code by it. Exactly one per
  function, in the comment block directly above it (blank lines allowed), on its own line -- never
  trailing a one-line definition (it then belongs to the next function), never in a prototype comment
  or file header (the next definition inherits it), never two on one line. Other addresses: plain `0x...`.
- Name functions exactly as Ghidra has them; renaming in code means renaming in Ghidra (`name_ctx.py`).
- Globals and statics carry their address (`/* bss 0x6e9800 */`, `/* sdata gp-0x7ff0 */`).
- Assert line numbers (when the game passes `__LINE__`): keep the ROM's line as a trailing `/* NNN */` on
  the asserting statement -- it orders statements and anchors later matching.

## The compiler's ABI

**GCC 2.9x** (FF2, RoR): mangling `setId__8DogActori` (`gcc2.py`); one constructor per signature; one
destructor `Class::~Class(this, int flags)` (`_$_<class>`) that deletes when `flags & 1` (write a normal
destructor; explain the invisible calls with `// callcheck: ~Base, <delete wrapper> (...)`); vtables
`_vt$<class>` of 8-byte `{short delta, short index, void *pfn}` entries, entry 0 the `__tf<class>`
type-info function, entry i the i-th virtual in declaration order; **the vptr sits after the data of
the first polymorphic class in the hierarchy** (RoR's DogEntity at +0x94), not at 0. A virtual call
reads `(**(code **)(*(int *)(this + VPTR) + 8*i + 4))(this + delta)`. Never reconstruct `__tf`
functions; a runtime cast (`__dynamic_cast`, `__rtti_si`) is `dynamic_cast` in source.

**GCC 3.x** (FF3): Itanium ABI. `_Z` mangling, `_ZTV` vtables with the vptr at offset 0, C1 / C2
(complete / base) constructor twins and D0 / D1 / D2 destructors -- the complete and deleting dtors take
two vtable slots where the destructor is declared (usually first; FF3's GameModel has them at +8/+0xc:
check `rtti.py --class`). Twins count only with the `<TAG>` annotation on each.

**Both**: `rtti.py --class X` / `--vtable ADDR` lists bases, slots with their targets, ctors / dtors
and the vptr offset; a slot's class comes from the vtable, and an inherited slot is the base's method.
The EE passes the first float arguments in f12.. whatever their position, ints and pointers in a0..t3
(a0 = `this`); Ghidra's order for mixed float / int signatures is unreliable: read the registers before
the `jal`. `long` / `u_long` are 64-bit (`ld` / `sd`); `(int)param` casts are often 64-bit register
noise; `lq` / `sq` move 16-byte vectors. Static constructors: `.ctors` entries mark files with
file-scope objects; on the host the same `static T obj;` runs before main, so that constructor must be
real, not a stub.

**Soft-float double and 64-bit integer helpers** (libgcc, 0x119be8..0x11c6b0 in FS2; never TUs):
the EE has a single-precision FPU only, so every `double` operation and `long long` divide in game
code is a call. Write the C expression, not the call: `__extendsfdf2(f)` is `(double)f`,
`__truncdfsf2(d)` `(float)d`, `__adddf3` / `__subdf3` / `__muldf3` / `__divdf3` the operators,
`__cmpdf2(a, b) < 0` `a < b`, `__floatsidf` / `__fixdfsi` / `__fixunsdfsi` the casts, `__divdi3` /
`__moddi3` / `__udivdi3` / `__umoddi3` `/` and `%` on 64-bit, `__muldi3` `*`. A `double` in a
`printf`-family call (`PerfMeter::writeCsv`, `_kvprintf`) is the varargs promotion of a float.
Callcheck: name such calls in `// callcheck: ...` comments where the operator hides them.

## Semantics

- Follow the game's decompilation. A sister game's code (MioPan for FF3) is a starting point only
  where the engine is shared: start from that file, convert it to this game's classes and offsets, drop
  the sister's debug-only calls, verify every function against this game's ROM, and say
  `from <project> <file>` plus what changed. Otherwise ReiPan / RosePan / MioPan are references for the
  PS2 side only (SDK behaviour, GS / VIF / VU packets, the platform layer), never game logic.
- **Classes are real; methods, not free functions** (the user's rule). A function whose callers pass
  the object in a0 and that uses a0 only as that type is a member: `void Actor::update()` called
  `actor->update()`. Declare every member in its class with `// <TAG> 0x...` (stubgen stubs it from that
  line). A function that takes the object later or not at all stays free (or `static`). Real class names
  come from the binary (vtable names, RTTI); a class not identified yet gets a minimal stand-in holding
  only the slots called so far, renamed in every caller once known. A function the ROM calls through a
  plain function-pointer table stays a file function even when it takes the object first. Pitfalls: a
  member hides a free function or base member of the same name (`::Name(...)`, `using Base::Load;`); a
  parameter shadowing a field needs `this->`.
- **Asserts**: an assert compiles to `if (!(expr)) handler(__FILE__, __LINE__, "expr")`. Write it back
  as the project's assert macro with the ROM's expression text (it names the members); the host macro
  reports and continues. Explain the handler call in a `callcheck:` note. A retail build with a sister
  debug build: the debug build's asserts / printfs are *not* in retail -- never reconstruct them.
- Ghidra pitfalls to read the asm for: variadic calls lose arguments; comma-and-`&&` chains are ladders
  of `if`s; callees typed `void f(void)` lose their arguments; tail calls (`j`) are calls; switch tables
  on bytes decompile into goto soup (check the rewrite with `mipsemu.py --grid` over every input); VU0
  macro code is `extraout_vfN` noise (hazards.md "Decompiler").
- **Types and constants (the user, 2026-11-02): name the patterns while decompiling.** A value that is
  compared against, switched on or stored as a state / mode / kind / id gets an `enum` (in the owning
  class's header); flag words get named bits (`enum ActorFlags { ActorFlag_Visible = 0x10, ... }` or
  `constexpr` masks) and every `& 0x10` / `|= 0x10` uses the name; a number repeated across functions
  (sizes, limits, frame counts, file ids) gets a `#define` / `constexpr` once. A name whose meaning is
  still a guess says so (`State_Unk3`, comment with the evidence) rather than staying a bare number.
  Enum names the binary carries are real names; file numbers get their table's names. Bare hex in
  finished code is for one-off addresses and hardware register values only.
  Step machines whose steps are base + phase (memcard's `SaveStep_Data + kSavePhaseWrite`): make one
  of the two `constexpr int`, not both enums (enum + enum arithmetic is deprecated in C++20, an error
  later), and write the dispatch as a `switch` with fall-through where the ROM falls through.
- Singletons keep the ROM's shape: a getter returning a function-local static whose constructor is the
  ROM's init.
- EE addresses in game code (load targets, packet buffers, scratchpad 0x70000000) go through the
  platform layer's host-pointer translation at the point of use. Hardware goes through the SDK
  replacements (`sceGs*`, `sceDma*`, `sceSif*`, libvu0) -- never SDL from game code. **Drawing is the
  exception** (the user's rule): the function that would write GIF packets calls the host draw module
  for its subsystem instead. A PS2 behaviour emulated in game code is marked `// PC:` with the reason.
- `printf` goes to the log; keep the game's messages (often the best names).

## Types and layouts

Types are rediscovered piece by piece; write each down as code the moment reconstructed code touches
it, and make the compilers prove its layout.

- **Recreate the type, never an offset.** `*(int *)(obj + 0x1cc)` is not finished code: define (or
  extend) the struct and use the field; pad what is unknown (`u_char _1d0[0x38];`).
- **Where:** the header of the TU that owns the object (its class, constructor, global, getter), even
  when that TU is not written yet (create the header minimal). File formats: the loader's header.
- **Field names**: assert expressions, the game's member style, then what the code does. A sister
  game's layout is a hint, not this game's (FF2's GRA3DCAMERA was 0x1e0, FF3's 0x220): mark unverified
  fields `// unverified`.
- **Size evidence**: constructor clears and copies, `memcpy` / copy-loop lengths, array strides,
  allocation sizes, the distance to the next global. Write the EE size in the comment (`/* EE 0x220 */`).
- **Every struct carries asserts** right after it: `static_assert(sizeof(T) == 0x..)` and `offsetof`
  for each field code touches (all of them for file formats and DMA data). Structs holding pointers
  cannot keep the EE size: assert the offsets before the first pointer and say `layout: host -- <why>`,
  or keep EE addresses as `u_int` when the bytes come from a file or go to the hardware.
  `layoutcheck.py` compiles every header alone under MinGW GCC and MSVC and fails on structs without
  asserts.
- **Polymorphic classes**: the host vptr is 8 bytes at offset 0. Write the class with its virtuals in EE
  slot order (`// EE slot 3 (+0x18)`), members in EE order with EE offsets in comments, `layout: host`,
  `>=` lower-bound asserts. Data that must keep EE bytes never lives inside such a class. Alternative
  when a class family must keep EE offsets (RoR's Dog* layer): non-polymorphic, the EE vptr word kept
  as a class tag the constructors set, and a `call<Slot>()` member that switches on it for virtuals
  called through a base pointer.
- **Host types** (EE: GCC MIPS; hosts: MinGW GCC and MSVC on x64, both LLP64):
  - EE `long` is 64-bit, host `long` 32-bit: `int64_t` / `u_long` (uint64_t), never plain `long`.
  - Pointers: 4 bytes on the EE, 8 on the host. EE sizes in `operator new(N)` under-allocate host
    objects: `new T` on the host.
  - 16-byte alignment: `sceVu0FVECTOR` / `FMATRIX` / `IVECTOR`, `u_long128`, anything moved with `lq` /
    `sq`. Use the platform's aligned typedefs or `alignas(16)`, never `float v[4]` (4-aligned, shifts
    every later field).
  - Bitfields: MSVC *and* MinGW (MS layout by default) open a new unit whenever the declared type
    changes; EE GCC packs by alignment. One type per run, the one whose unit reproduces the bytes;
    assert the size. GS / VIF / DMA registers: the SDK's shift / mask macros.
  - Enums: 4 bytes everywhere; when the ROM stores one in a byte / short, use that integer type and name
    the enum in a comment. No `wchar_t`, `long double`, `__attribute__((packed))` (use `#pragma pack`)
    in game types; `s_char` / `u_char` where the sign matters.
  - Tail padding: MinGW reuses a base's tail padding, MSVC does not: `>=` asserts on derived classes.
  - Float literals: read the bits from the ELF; EE constants are not always round-to-nearest
    (`0x3e4cccd0` is not `0.2f`): hex floats for odd ones. EE float division by zero gives +-MAX,
    `vsqrt` takes `|x|`, 0x7fffffff / 0xffffffff stored as floats are the EE's extreme values but NaNs
    on the host: guard such spots with a `// PC:` helper.

## Stubs and the frontier

- Called but not yet written -> `stubgen.py --from-build` (run by `tus.py check`): `YAMIPAN_STUB("Name",
  0xADDR)` bodies (log once, return `{}`) in the callee's inferred file; a run then shows which missing
  pieces were reached.
- **Bridges** (`YAMIPAN_BRIDGE`): temporary stand-ins that *work* but are not the game's code, so later
  TUs run on real data; counted as stubs; deleted when their TU is written; never a `<TAG> 0x` in a
  bridge file.
- **Stub hazards** (each cost sessions): a stub returns `{}` -- null pointers, zero counts, "not
  ready". An untouched out-parameter is stack garbage; a getter returning null crashes its first real
  consumer; a stub on a readiness path (`IsReady`, `IsLoadEnd`, `Wait*`) returning 0 hangs the caller's
  loop; a polled helper behind a real state machine stalls the boot. After a TU, a new hang or crash
  starts with the stubs on that path.
- Declarations first: stubgen needs a prototype in a header (members in their class with the
  annotation). Constructors, destructors, out-of-line virtuals of a new polymorphic class and data are
  stubbed by hand.
- Replace the whole stub when reconstructing; the `// stub of <TAG> 0x...` comment goes with it. When a
  TU defines a function, grep `src/` for local declarations of its name (C++ overloads can keep linking
  to an old stub) and include the real header instead.
- The **frontier** (callees of written code not yet written) plus the stubs the last run reached rank
  the next TU after the pinned queue.

## Verify and commit

`tus.py check <TU> --run 15` does 1-4, `tus.py finish ...` 5-6:

1. Build green (undefined references become stubs).
2. `callcheck.py` on the TU's file: no *missing* calls, or `// callcheck: A, B (why)` (names first, one
   parenthesis last, all on the `callcheck:` line). *Extra* calls are informational. Write the
   names unqualified (`set`, `~UiDrawer`, not `UiVec4::set`): the names after a `Class::name` token
   no longer count (2026-11-03, game/uilayout.cpp).
3. `layoutcheck.py`: headers alone under MinGW GCC and MSVC with their asserts, sources under MSVC,
   every struct asserted or `layout: host`, no host types.
4. Smoke run with the scripted pad: reached stubs listed and ranked. **Look at the screen**
   (`work/shots/last.png`, `shot_*.png`, Read tool) and compare with the game's own assets
   (`tm2png.py` -> `work/ref/`). A wrong picture is a bug to fix or record, never a pass.
5. `refresh.py`: function list and status regenerated.
6. One commit per session, made by `finish` (refuses a red build, a failed layoutcheck, `main`); the
   message lists the written and stubbed functions and the screenshot (`progress/`).

- **A layout-driven widget's values can be read off the game itself** (2026-11-03, PrgMcCard): the S2D file holds the variables' real names, so dump `S2dVar::name` and `getString` from the ctor once, and to name a value that selects a text image, set the variable in a temporary loop and take a shot per value (`--shot-every`): two guesses fell that way. Neither stays in the commit.
