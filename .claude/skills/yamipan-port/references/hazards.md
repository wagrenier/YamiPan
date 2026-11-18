# Hazards -- things that already cost time

Carried from ReiPan (FF3, `../ReiPan/.claude/skills/reipan-port/references/hazards.md`, 1,080 lines)
and RosePan (RoR, `../RosePan/.claude/skills/rosepan-port/references/hazards.md`, 1,040 lines): the
ones any PS2 port meets, in short form. Their files keep the full stories with addresses; read the
matching section there when a symptom here fits. Add this game's own as **symptom / cause / fix**,
dated, in the section they belong to. GhidraMCP traps live in ghidra.md.

- **`py -` with an empty heredoc never ends (2026-11-05).** Symptom: a shell line of greps hangs past its timeout and the output fills with `_pyrepl` tracebacks. Cause: Python's interactive reader retries forever when it has no console. Fix: run a script file, never `py -` without a body. A long Bash heredoc holding apostrophes also failed to parse: write such files with the Write tool.

- **A black shape on a clear white background gets a pale edge when scaled up (2026-11-06, FS2).** Symptom: the sight's black cross shows as a light streak over the black mask. Cause: the art's clear texels are RGB 255, alpha 0, and bilinear blends straight alpha, so the edge is half white. On the GS the shape was drawn 1:1 with UVs on texel centres and never interpolated. Fix: nearest (`TEX1_1` 0) for what the ROM draws 1:1, as `game/menuprogset.cpp` does; check it at a window larger than the frame.
- **A shot copied right after `build.py --run` can be empty (2026-11-06).** `last.png` is rewritten as the game exits: take the newest non-empty `shot_*.png`.

## Decompiler (the most common time sink)

- **A register the decompiler dropped is the argument** (2026-11-04, FS2). Symptom: `invertRigidMatrix(auStack_40)` / `matrixToRotZYX(auStack_20)` with one argument, and a field read just before for a null test (pose +0x150), so an earlier run named the field a count. Cause: the test's `lw a1,0x150(a0)` is left in a1 and the call takes it: the field is a matrix pointer. Fix: when a known two-argument helper shows one argument, read the asm before naming what was tested.
- **A body swallows the function after it.** Code reached only through data (vtable slots, callback,
  dispatch and step tables, hook lists, code after a tail call) was never made a function, so the body
  before runs on over it: the decompile stops at the first `jr ra` while the callee list shows calls it
  never made, and callcheck reports them MISSING. Tells: a size far above the decompile, a callback
  address inside another function, a Body that starts before its Entry. Fix: `hidden.py <lo> <hi>` over
  every pinned range before naming (it flags code after `jr ra`, after an assert's spin, a frame setup
  after a `b`), `ghidra.py refs` the address, `ghidra.py mkfunc`. Before reading a class,
  `ghidra.py func` every slot `rtti.py` lists; before a dispatcher, every word of its table. Every `LAB_`
  stored into a table is a function.
- **FS2: 904 vtable slots are not Ghidra functions** (2026-10-31, bootstrap 4). Symptom: a slot list
  stops after one entry; a ctor shows up as a slot (0x3112f0 slot 2 = FUN_00171a30, which also stores
  the vtable). Cause: auto-analysis never followed the untyped vtables, so tiny virtuals and dtors were
  merged into the function before them (0x131d70 is a 2-word thunk, then nops, then the dtor 0x131d80
  inside the same body). Fix: `rtti.py` accepts any .text word as a slot and `--missing` lists them in
  `data/missing_functions.json`; create them all (`ghidra.py mkfunc`, or F in Ghidra) before
  trusting function sizes, callers or the "ctor/dtor" users of a vtable.
- **Dispatch tables of tiny functions** (RoR's script natives: 512 entries, 471 of them `jr ra; move
  v0,zero`): read the table from the ELF, classify each target by its first words, mkfunc the real ones,
  let one host function stand for the identical empty ones, and generate the C++ initializer.
- **Tail calls are not calls to Ghidra** (`j` at a body's end; "0 callers"): features.py records them
  and callcheck counts them. A run of tiny tail-call wrappers can decompile one call off: read the asm.
- **Arguments**: Ghidra drops them for callees it typed `void f(void)`, orders mixed float / int ones
  wrongly (floats go in f12.. whatever their position), misses a register passed through untouched
  (a callee reading a2 its caller never set takes the caller's caller's argument), and chains a void
  callee's v0 into the next call. Read the asm before every `jal` whose arguments matter.
- **VU0 macro code** decompiles into `extraout_vfN` noise and helpers that pass values in vf1 / vf2
  show as `void f(float)`: reconstruct from the asm. `vaddz.x` adds only z; `vsqrt` is sqrt(|x|); vf0 is
  (0,0,0,1); constant registers are set once at start-up (MioPan's skill "VU0 macro-mode idioms").
- **Switch tables**: on bytes they decompile into goto soup (`mipsemu.py --grid` the ROM over every
  input, diff the C rewrite); "Too many branches" leaves only the dispatch (read the table from the ELF
  and the asm case by case).
- **Variadic unpackers**: 8-byte slots; floats promoted to double (`va_arg(ap, double)`).
- **ROM quirks to keep, guarded**: loops that reuse their counter, a read of the byte after a copy,
  a table reader that stops at a null field (end host tables where the row id stops matching).
- **Twin functions differing only in `ldl` / `sdl`** are overloads on a matrix / vector type.
- **An int-typed `this` turns float fields into int conversions** (2026-11-03, TU 12): Ghidra showed
  `BoxGrid`'s +0x10 / +0x14 as `piVar[4] = (int)(n / w)` and `(float)param_1[4]`, but the asm is
  `swc1` / `lwc1` (no `cvt.w.s` before the store, no `cvt.s.w` after the load): float fields. Read
  the store / load opcode whenever a decompiled int field is converted to or from float.

- **Recursive calls lose their arguments, and their call-site null tests look like part of the
  callee** (2026-11-03, TU 26): Ghidra prints `FUN_001951e0()` with no argument inside
  `if (node->right == 0) h = 0; else h = FUN_001951e0();`. The callee (a tree height) never tests
  null; every caller does. Porting the call as `height(node->right)` crashed a host harness on the
  first leaf. Keep each guard (a null-safe wrapper at the call sites), and say whose test it is.
  A host harness over the containers found it in seconds: write one for any tree / list code.
- **A float argument passed straight through is invisible** (2026-11-03, TU 42): `lerpVec4` 0x133be0
  shows as `(out, from, to)` calling `FUN_00136490(tmp, tmp)`, because its own f12 (t) is never
  touched before the `jal` and the callee scales by f12. Read the f12 / f13 uses in the asm of any
  function whose callee takes a float. Also: Ghidra prints VU0 macro ops with the broadcast field and
  dest mask lost (`_vmulbc`, `_vmaddabc`, `.word 0x4b...` in the listing): decode the word (dest bits
  24..21 = xyzw, ft 20..16, fs 15..11, fd 10..6, funct 5..0; funct 0x3c..0x3f is special2, opcode =
  (bits 10..6 << 2) | bits 1..0) before writing lanes (`vadd.xyz` keeps the destination's w).

## Names from a sister game (FF2 -> FF3; any engine shared with a symbolised build)

- **Names are hints.** Wrong ones sat in every subsystem (sound, pad, file load, 2D, C++ runtime,
  maths); check the body before trusting a name in a new module, fix with `name_ctx.py` (records an
  override the matcher never restores).
- **Dropped modules still attract matches** (FF2's scene-test menu, FF2's sister mechanic in FF3): in a
  module the game cannot have, every match is wrong; name from the body, never keep the file name.
- **Shifted runs**: families of same-shaped functions (copy / draw, soft-float, effect requests,
  libpad) match one or more slots off even at high scores; compare constants and call shapes.
- **Debug-build calls** (`printf`, assert helpers, range checks) are absent from retail: drop them when
  porting; find counterparts by module and class, not by shape. Identical id wrappers cannot be told
  apart by shape: name them by what they call.
- The link-order matcher is greedy: small anchor changes move hundreds of names. Always look at the plan.
- **Size alignment against a sister library shifts runs of equal sizes** (2026-11-02, FS2 rx from
  RoR): a plain LCS of function sizes paired four 0x20-byte `rxSys*` wrappers one slot off
  (batch 005: `rxSysGetFileSearchDir` on what calls `rxFilePushSearchDir`); both pairings were
  equally long. Fix: `rxalign.py` now prefers the alignment with the fewest breaks and warns ('!')
  about one-size runs; still check such runs by what they tail-call.
- **Ghidra merges small functions it never saw called** (setters, thread entries passed by pointer)
  into the function before them, so sizes stop matching the sister's: compare spans (next start -
  start) between anchors (`rxalign.py` '~' / '+'), and list the merged starts for creation.

- **A virtual slot named from its first caller's guess** (2026-11-03, TU 17): UiTrack slot 3 was
  named `getText` in TU 16 (base answers null, UiAnim::getText walks the tracks); the overrides at
  0x24d420..0x24d520 return the record's first vertex and `UiWidget::create` reads two floats from
  it. Before naming a slot, read one override and one consumer of its value, not only the base.
- **Register-passed arguments past a3 vanish from the decompile** (2026-11-03, TU 17): the track
  `apply(this, xform, parent, colour, state)` slots show `in_a3_lo` / `in_t0_lo` and lose their
  parameters when Ghidra's prototype is shorter; read them from the asm (`ghidra.py asm`), or fix the
  prototype first.
- **A shared-library function longer than its sister's hides a function in its tail** (2026-11-03,
  TU `rx/rxsnd.c`): batch 007 read `rxSndBgmSetParam6` / `SeSetParamC` / `PcmSetVolume` /
  `AdpcmGetPosition` as "same code, other size"; each had a whole FS2-only function (0x44 bytes,
  no callers) merged after its `jr ra`. For a carried-over library: compare sizes with the sister's
  list, disassemble every extra tail (`ghidra.py asm`), `ghidra.py mkfunc` what starts there; then
  compare the same-size ones word for word with jump targets, `lui` and gp offsets masked (a short
  script over both ELFs) instead of reading them.

## File inference (which TU a function belongs to)

- Inferred extents are guesses where no string anchors them; inferred file *names* from a sister
  game's slot get taken literally and are often wrong. Decide from strings, classes, names, callers
  (`tuid.py`), and record the deciding signals in `define --why`.
- `define` drops the whole inferred extent (a pinned TU is exactly its ranges): list every range,
  including a neighbour you only extend. Defining a file inside an `unassigned@` run leaves the rest
  under new keys: fix the pinned queue in the same session.
- Renaming moves inferred boundaries (export anchors on names): re-brief after a name batch.
- Satellites (inline getters, templates) go to the TU that calls them most, not always their owner.
- A stub inside a TU marked done is never picked: grep `stub of <TAG>` and the run log's reached stubs.
- **The frontier picks library runs** (2026-11-02): `tus.py next` claimed `unassigned@00116fb8`
  because game code calls `atan2f` / `__extendsfdf2` in it -- newlib libm + libgcc soft-float, not
  game code. An `unassigned@` run between library ranges: read its shapes first (fdlibm constants,
  fp-bit unpack / pack, `__clz_tab`); if library, name it, add it to `layout.json` `lib_ranges`,
  `export.py` + `status.py`, and finish the emptied key `verify` with a summary saying so.

## Types and layouts

- **A clear loop's bound is not a byte count** (2026-11-02, TU 7): `FlagTable::clear` writes 8 ints per
  pass with a counter `+= 8` up to 0x100 -- 256 ints, 0x400 bytes, not the 64 ints TU 5 read into it
  (`sysdata.h` had `sizeof(SystemData)` 0x620 instead of 0x920). Check the counter's unit, and look
  for a copy of the same type elsewhere (here the save's implicit `operator=`, 0x80 doubleword moves).

- **MinGW is not "GCC layout"**: `-mms-bitfields` by default. **`long` is 32-bit on both hosts.** A
  plain `float[4]` is not a VU0 vector. Tail padding differs between the hosts. (reconstruction.md)
- **A mid-struct pointer moves every later host offset**: asserts after it become `>=` bounds; data that
  keeps EE bytes (file images, DMA) never holds host pointers. Files the ROM relocates in place keep
  their file offsets on the host (RoR's i3m / i3c: resolved through a `Ptr(root, field)` helper).
  When written code already indexes the relocated pointers (`data->animSets[i]`, `rec->name`), a
  **self-relative 32-bit ref** keeps both the EE layout and the call sites (2026-11-03, TU 18:
  `S2dRef<T>` in `game/s2ddata.h`: `rel` = target minus the field's own address, 0 null, implicit
  `T *` conversion, copy deleted so a moved copy cannot dangle). Relocate stays in place, as the ROM.
- **Tool input can lose a backslash** (2026-11-03, TU 18): a `\0` typed into a heredoc or `py -c`
  reached the file as a NUL byte (grep then says "Binary file matches"). Check edited docs for NULs;
  write such bytes as `bytes([92, 48])`.
- **EE sizes under-allocate host objects**: `operator new(0x28)`, a list sized with the EE record size,
  an out-buffer sized from what is read back.
- **Ghidra's `ptr + n` on an `int *` is n words.** `param_1 != -0x240` is a null test of `&obj->member`.
- **A bss global with "no writer"** may be a member of a global struct, or written gp-relative.
- **A forwarding wrapper's arity comes from its callers**, not its body.
- **Giving a POD base a constructor moves its derived classes' fields (MinGW)**: keep layout bases POD.
- **EE float semantics**: literals not round-to-nearest; division by zero gives +-MAX; 0x7fffffff /
  0xffffffff stored into floats are the EE's extremes and NaNs on the host.
- **ROM data the code writes back must not be `const`** on the host.
- **A GS register number read from memory, not checked against `SCE_GS_*`** (2026-11-03, TU 19):
  TU 17 named `UiDrawer::setNoAlphaTest` from its values 0 / 5, but the A+D register is 8, CLAMP_1
  (TEST_1 is 0x47): it is the texture wrap (`setTexRepeat`), and kind 0x27 was "no alpha test" for
  two TUs. Fix: decode every A+D register number and GIF tag (NREG / FLG / REGS nibbles) with
  RosePan's `sdk/libgraph.h` before naming what a function writes.

- **A test that looks like a cross product and is not** (2026-11-05). `isInsideQuad` 0x2400c0 / `isInsideTriangle` 0x2403f0 read as a sign test of cross products, but the asm at 0x2404a8 is `mula.s` x, x then `msub.s` y, y: ax * bx - ay * by. Ghidra printed the indices faithfully; only the reader corrected them. Copy what the code does and say so in a comment (`insideTerm`). It only moves the leg cursor, which nothing reads, so the bug never showed in the ROM.
- **A host return type narrower in meaning than the ROM** (2026-11-05). `getFloorAttr` was `u_short` with 0xffff for none; the ROM loads with `lh` and answers -1, and callers test `< 0`. An int field filled from it was 65535, so the test never fired. Before writing a "none" constant, read the load at the end of the function.

## Stubs and bookkeeping

- **Stubs return `{}`**: null buffers, zero counts, "not ready". A getter stub's null crashes the first
  real consumer; a readiness stub returning 0 hangs a loop; a polled loader helper turns a step-per-call
  loader into a hang; a stub's untouched out-buffer becomes garbage once its consumer is real; a string
  copy stub makes every texture the last one. After a TU, a new hang or crash: the stubs on that path.
- **A real function behind a polled stub can stall the boot**: the stub answered "ready" at once.
- **A file-scope object whose constructor is a stub** runs that stub before main (before the logger).
- **A stub that tolerated null becomes a member that does not**: guard or fix the caller in the session.
- **Callers' local prototypes disagree with the real signature**: in C++ they are overloads and keep
  linking to an old stub; include the real header.
- **A name you pick may already exist at another address**: stubgen then resolves to the wrong one.
- **Stale names after renaming**: stubgen, callcheck, stage, tus read cached `work/` data; `refresh.py`
  (never piped into `head`: the closed pipe kills it). Batch numbers collide: take the highest + 1.
- **A stubbed modal screen inside a retry loop spins for ever** (2026-11-03, game/menuscene.cpp):
  `runTitleScreen` stubbed answers 0, `menuFlow` then asks again with no `threadYield` in between (the
  real screen yields), so the cooperative game thread never gives way. A log that stops at a modal's
  stub with no crash is this.
- **When a run loses ground after a TU**, diff its log against a run of the previous commit.
- **A host harness's `jobAdd` stub must queue, not run** (2026-11-03, TU 27): `startLoad` /
  `FileJob::start` set "queued" *after* jobAdd answers, so a job run inside jobAdd ends "done" and is
  then marked "queued" again: `waitLoaded` spins forever. Queue the jobs and run them in the
  `threadYield` stub, as the job thread would. The host thread layer must keep jobAdd asynchronous too.

- **"No callers or references found" can be a vtable slot** (2026-11-04). Symptom: stubgen puts a class's stub in an unrelated file. Cause: `game/sysdata.cpp` ended with `sysDataReturnZero` 0x23c010, called by nobody: a `return 0` right before the next class, in fact the slot of vtable 0x316510 (`ghidra.py refs` lists the DATA references; the function's own decompile says nothing). Fix: before writing a tiny function at a TU's edge, look at its refs; stubgen places a stub by the TU whose range holds the address, so a stub in the wrong file means the range is wrong.

- **A stub that returns nothing can make a loader succeed on nothing** (2026-11-04, a day of "no model ever reaches the code"). Symptom: `StageFiles::load` answers true, no crash, the stage plays empty. Cause: `String` / `StringList::pushBack` were stubs, so the script had zero lines and the loop never ran. Fix: when a run is empty but silent, list the reached stubs in order (`run_relwithdebinfo.log`) and look for a data structure among them before tracing the caller.

- **A free-function stub left behind after its code became a member keeps the TU "unfinished"** (2026-11-07, game/stageelevator.cpp). Symptom: `tus.py list` shows a TU whose functions are all written as 14/13/27, its brief lists "stubs living in other files". Cause: the session that turned `elevatorCall` .. `elevatorSwitchPlace` into `StageElevator::` members deleted the callers' uses but not the old `YAMIPAN_STUB` bodies in src/lib/unassigned_stubs.cpp; they link (nobody calls them) and `yamipan.stubs()` counts their addresses. Fix: when a function changes name or becomes a member, grep its FS2 address in src/ and delete the old stub in the same session. Check: a stub whose address also has a `// FS2 0x...` annotation in a game source is one of these (after the cleanup only ScriptRunner::getProperty / setProperty match, on purpose: their default case logs an unknown property).

## callcheck / stubgen

- **stubgen and new declarations** (2026-11-03): a member declared right after `public:` with an
  address-less comment (`// FS2 vtable +0x18`) comes out with `public:` in its signature and a `return
  {};` in a void; a derived class whose base has no default ctor gets a stub that does not compile (it
  needs `: Base(...)`). Fix both by hand in src/lib/unassigned_stubs.cpp; stubgen leaves them alone
  afterwards.

- **Implicit copy assignments are calls in the ROM** (2026-11-02, TU 7): CodeWarrior emits
  `SaveData::operator=` and kin as out-of-line functions, so a struct assignment shows as a missing
  `operator=`; explain it with `// callcheck: operator= (...)` (callcheck.py accepts `=` in the names
  since TU 7). Likewise a base constructor in an initialiser list and a deleting destructor's
  `operator_delete`.

- **Template arguments in a Ghidra name defeat callcheck** (2026-11-03, TU 12): `std::max<float>`
  never matches a `std::max(...)` call nor a `// callcheck: max<float>` note (the note is read as
  `max`). Name template instances in the overload style, `std::max(const_float&,const_float&)`, whose
  last component is `max`.
- `// callcheck: A, B (why)`: names first, comma-separated, one parenthesis last, all on that line;
  a member destructor is `~Class`, and a member call is its last component (`free`, not
  `Heap::free`: the qualified form stays MISSING, 2026-11-03, TU 42); ALL-CAPS callees read as macros and are skipped. The name list ends
  at the first `(`: a name after it is not read (2026-11-02). `operator_new[]` / `operator_delete[]`
  are accepted since 2026-11-02 (callcheck's pattern took no brackets before). Calls are counted:
  a callee the ROM calls twice from code an inline helper hides (`delete[]` in two branches) is
  listed twice in the note, or it stays MISSING (2026-11-03, TU 17).
- A goto label in column 0 and a trailing `<TAG> 0x` on a one-line definition break the parser; a
  constructor with an initializer list parses as the base's name.
- Your own `static inline` wrappers hide the ROM's calls; constructor / destructor / new / delete calls
  are invisible: name them in a note. A call the optimiser proves dead gets no stub. Platform
  substitutions (`srand` -> the host's) show as missing / extra.
- stubgen: one declaration per line; it copies the first declaration of an overloaded name; places stubs
  by inference (move them); skips data and missing vtables (stub a new class's virtuals by hand).

- **stubgen on a member declared under `public:`** (2026-11-03, `GsRegion::release`): a prototype on
  the line after `public:` came out as `public:\n    void GsRegion::release()` with `return {};`, and
  the build broke on the stub. Cause: it takes the access label as part of the declaration. Fix by hand
  in `unassigned_stubs.cpp`: drop the label and the return.

- 2026-11-03 (game/stagescene.cpp): **stubgen reads two shapes wrong.** Symptom: `expected unqualified-id before 'public'` in unassigned_stubs.cpp, and an undefined reference stubgen never fills. Cause: its declaration regex lets the return type run over a newline, so a method right after `public:` gets `public:` in its stub; and an overload of a declared name is taken for the existing one. Fix: put a comment line between `public:` and the method, delete the bad stub, and give the overload its own name (`setPositionXyz`).

- **`name_ctx.py` skips a name another address already has, silently** (2026-11-04). Symptom: one row of a batch stays `FUN_...` while the run reports nothing, and callcheck goes on printing the old name. Cause: the plugin refuses a duplicate name (`copyVec4` was 0x133800). Fix: grep yamipan_functions.txt for each new name first, and check the batch landed.
- **A template's second instance cannot take the first's Ghidra name, and header inlines never leave the slice** (2026-11-07, game/playermotion.cpp). Symptom: `tus.py brief` keeps offering functions already written (StageList's carried-list instance 0x217700..0x217940, StageCharacter's one-liners, CharacterBlock7c8), and name_ctx.py reports `name already used at 0017e3b0`. Cause: `reconstructed()` reads tags from .c/.cpp only, and the plugin keeps names unique. Fix: list header-written functions in the TU's .cpp block "Written in headers" (one `FS2 0x...` each); leave a duplicate instance `FUN_` and explain it with a `// callcheck: FUN_... (StageList::pushBack, second instance)` line at each caller.
- **Renaming or moving a method leaves its stub in unassigned_stubs.cpp** (2026-11-04). Symptom: `no declaration matches` in lib/unassigned_stubs.cpp, or a duplicate definition. Fix: delete this TU's stub blocks by hand before the first `tus.py check` (protocol step 5).

- **A batch given as a bare number names nothing** (2026-11-07, game/kunitoris.cpp). Symptom: `py name_ctx.py 229` prints `named 0/0` and exits 0, no refresh. Cause: it takes a path, not a batch number. Fix: `py name_ctx.py work/batches/229.json`.
- **Callcheck notes keep FUN_ names after their callees are named** (2026-11-07, game/kunitoris.cpp). Symptom: a function that was clean goes MISSING (`add, hasOffset`) and its note's `FUN_...` show as extra once a later batch names them. Cause: callcheck matches a callee by its current name; the note still says FUN_002cb010. Fix: after a batch, grep the TU for `FUN_00` in callcheck lines and put the new last components in.
- **`operator++` cut a callcheck note short** (2026-11-09, game/packtables.cpp). Symptom: PackTables::parse went MISSING on names its note listed (`begin, end, operator[]`) and showed `operator` as extra. Cause: the note's pattern stopped at the first `+`, so every name after `operator++` was dropped. Fix: callcheck.py accepts `+` in note names since 2026-11-09; a `// callcheck: operator++` now explains an iterator increment.

## Sessions

- **A session never launches other sessions, agents or scheduled tasks** (the auto-mode classifier
  blocks it); the user starts each TU's session.
- **`tus.py finish` commits with `git add -A`**: look at `git status` first. Two sessions in one
  checkout race (a scheduled run swept another's half-done files): side work goes in a sibling worktree.
- **Autopilot sessions run on a fixed allow-list**: `.claude/` writes may be denied (reference text
  goes in `--notes` under "For the references:"), no `rm`, no shell loops. A denied call is final.
- **No backticks in `--notes` / `--summary`**: the shell runs them.
- **`last.png` can come out 0 bytes**: check its size after `check`.

## Shell and Python on Windows

- **A heredoc whose body has an odd number of apostrophes fails in the Bash tool**, even with a quoted
  delimiter: "unexpected EOF while looking for matching". A batch JSON with one apostrophe did it, and
  nothing ran (2026-11-03). Write such files with the Write tool.

 Nothing in the command runs, not even what stands before the heredoc, so a check run beside it tests the old tree (2026-11-03).
- **A long heredoc in one Bash call** (2026-11-03): one holding C++ with apostrophes in comments failed
  with "unexpected EOF while looking for matching" even with a quoted delimiter, and only the part
  before it had run. Write big files with the Write tool, small edits with a script in the scratchpad.

- **`python` is the Microsoft Store stub** (2026-10-31): `C:\Users\willi\AppData\Local\Microsoft\WindowsApps`
  comes before Python 3.14 on PATH, so `python` prints "Python was not found" (exit 9009) until the
  user turns off its App execution alias. Use `py <tool>.py` (the routine does); tools that start
  Python use `sys.executable`, never `python`.
- **Command-line git had no GitHub credential** (2026-10-31; the user pushes with GitHub Desktop): a
  push or fetch started from Claude's shell waited forever on Git Credential Manager, whose sign-in
  never showed. Fixed the same day: `git push` typed into the user's Terminal panel opened GCM's
  browser sign-in, and the credential is stored now. Unattended pushes still use
  `git -c credential.interactive=never push`, which fails at once if it ever goes missing.
- **Heredocs mangle backslashes and apostrophes**, a python heredoc's `\n` became a real newline in a
  C string, a long heredoc fails with `ENAMETOOLONG`: write files with the Write tool.
- **Git Bash rewrites arguments that look like paths** (`\\` collapsed in a JSON argument): pass paths
  through environment variables or files.
- **A NUL byte makes git treat a file as binary** and flips its line endings; MSVC rejects raw NULs.
- **`sed -i` over `src/game/*` rewrites every file's CRLF as LF** (2026-11-03, TU 19): with
  core.autocrlf the untouched files show as modified with an empty diff. Restore those with
  `git checkout -- <file>` (no content lost), or limit the sed to files that contain the match
  (`grep -l`).
- **Python text mode vs CRLF**: `open(p, encoding='utf-8', newline='')`, keep the file's own endings.
- **A module-level function named like a stdlib package shadows it** (`def http` after `import
  http.client`): import the submodule under another name.
- **A gdb dump with no game frames meant the game never started** (fixed 2026-11-03): `gdbrun.py` did not pass `YAMIPAN_DATA_DIR`, so the app sat in its Game Data window. It cost one run the location of the black after the card. If a dump shows no `YamiPanEe*` threads, look for that first.
- **Exit 0xC0000374 (-1073740940) is heap corruption**, usually a use-after-free the EE heap forgave. Under gdb the debug heap reports it at the free; `gdbdrive.py` now logs the stack to work/gdb/log.txt. The plain build can run past it, so a run that gets further than gdb is not clean. First case 2026-11-03: `~UiSequence` freed the actors, then `PrgMovie::close` wrote to a text track inside one.
- **`gdbrun.py --help` launches the game**; read its docstring instead.
- `python -I` drops the script's directory from `sys.path`: run tools as `python tool.py` from their folder.
- **MSYS2's g++ fails silently outside its shell** (2026-11-03, TU 14): `/c/msys64/ucrt64/bin/g++.exe`
  from Git Bash exits 1 with no message, even on a broken file, because cc1plus cannot find its DLLs.
  Put `/c/msys64/ucrt64/bin` first on PATH (`export PATH=/c/msys64/ucrt64/bin:$PATH`) before a
  `-fsyntax-only` check; test it on a bad file first. Until bootstrap 6 the SDK headers are stubs in
  the scratchpad, so only errors in the TU's own files count.
  Quicker (2026-11-03, TU 21 second run): `-Isrc -I../RosePan/src -I<scratch>` takes RosePan's SDK
  headers; only `sdk/libvifpk.h` (sceVif1Packet) needs a scratch stub. Known misses that are not
  the TU's: gamesystem.cpp (eestruct.h), perfmeter.cpp (3-argument sceOpen), thread.cpp.
- **No GCC case ranges** (`case 1 ... 10:`, 2026-11-03): MSVC (layoutcheck) rejects them; write the
  numbered groups as range tests before the switch.

- **A big heredoc fails with `ENAMETOOLONG: name too long, uv_spawn`** (2026-11-03): the Bash tool
  passes the command on one command line, so a multi-screen edit script never starts. Write the
  script and its long texts with Write into the scratchpad and run the file. **`src/` is CRLF**: a
  Python edit script must normalise to LF on read and restore on write, or every multi-line
  `str.replace` misses.
- **The compiler is `C:\msys64\ucrt64\bin\g++.exe`** on this machine (2026-11-03): `yamipan.py`'s
  default CLion path no longer exists. Until the build, a syntax check needs `-I src -I <copy of
  RosePan's src/sdk/*.h>` plus stub `sdk/libvifpk.h` / `sdk/eestruct.h` in a scratch folder.

- **A long shell command with several heredocs can fail before it runs** (2026-11-03): "unexpected
  EOF while looking for matching" from bash, on quoted heredocs holding an odd number of apostrophes,
  and backslashes inside a heredoc can reach the file halved. Bash parses the whole command first, so
  nothing in it ran. Fix: write source files with the Write tool, and patch with a script file, not an
  inline heredoc with escapes.

- **A long Bash heredoc with mixed quotes can fail to parse as a whole** (2026-11-04): `unexpected EOF while looking for matching` and nothing runs, so no step of the command happened. Use the Write tool for source files and for Python scripts over a few lines.

## Build and run

- **A new worktree's first build takes RosePan's ImGui, ImPlot and SDL** (2026-11-09). Symptom: in a fresh
  `git worktree` (here `../YamiPan-rate`), `build.py` configures, compiles most files, then
  `src/yamipan/ui/panels.cpp` fails with `no matching function for call to 'PlotLine(...)'`. Cause: the first
  configure reuses a sibling's fetched sources (build.py's docstring), RosePan's, whose ImPlot is older than
  YamiPan's v0.17. Fix: point the worktree's build at the main checkout's own copies, then build again:
  `cmake -S . -B cmake-build-relwithdebinfo -DFETCHCONTENT_SOURCE_DIR_IMGUI=<main>/cmake-build-relwithdebinfo/_deps/imgui-src`
  (and `_IMPLOT` / `_SDL3` the same). The build also rewrites `resources/shaders/msl/*.msl` there: not changes,
  never commit them. Copy `tools/reverse/work/feat_fs2.json` across for callcheck.
- **Smart App Control blocks the freshly built exe** (2026-11-09). Symptom: `build.py --run` (and
  `tus.py check --run`) builds green, then fails with `OSError: [WinError 4551] An Application Control
  policy has blocked this file`; no shots. Cause: Windows Code Integrity, policy
  {0283ac0f-fff1-49ae-ada1-8a933130cad6} (Smart App Control), refused the unsigned `YamiPan.exe` built
  at 08:01 (event 3077 in Microsoft-Windows-CodeIntegrity/Operational); the run of 07:55 still started.
  Fix: the user's (a security setting: a session never changes it); until then a TU ends `verify` with
  "smoke run blocked" in the notes.
- **`gdbrun.py` never times out** (2026-11-04). It returns when the game exits, and a game that no longer
  crashes does not exit, so a background run waited 3 hours. SKILL.md "Time limits" says how to bound it.
- **A prototype moved between headers leaves its stub behind** (2026-11-04): stubgen does not remove a
  stale stub, so the build fails with a redefinition or a multiple definition. Delete it from
  `src/lib/unassigned_stubs.cpp` by hand.

- **`work/shots/last.png` can be cut short** (2026-11-03). Symptom: a PNG reader fails with "incomplete or
  truncated stream" on last.png. Likely cause, seen once and not proven: the smoke run is killed when its
  time is up, during the write. Fix: use the newest complete `shot_*.png` for pixel work; the numbered
  shots of a screen at rest are the same picture.

- **MinGW must be on PATH** or `cc1plus` fails silently: use `build.py`. Shader builds can hit a file
  lock: rerun.
- **HLSL keeps names a C++ reader would not expect** (2026-11-14). Symptom: shadercross fails with
  `modifiers must appear before type` on a parameter, and `expected expression` where it is used. Cause:
  `point` (like `line`, `triangle`, `linear`, `centroid`, `sample`) is an HLSL keyword, a geometry shader's
  primitive; `lit` is an intrinsic, so a function of that name is an overload of it. Fix: other names
  (`pointSource`, `lightedColor`). Check a shader edit with the shadercross CLI before a full build:
  `shadercross <file>.hlsl -o <out>.spv -I resources/shaders/hlsl` (and `.dxil`, `.msl`), the CLI from
  `extern/shadercross/windows` of a sibling (MioPan).
- **layoutcheck compiles every header alone**: a header relying on its .c's includes fails there.
- **A flag in CMakeLists.txt can break Visual Studio while every check stays green** (2026-11-05).
  Symptom: `D8021 invalid numeric argument '/Wextra'` on every Pan-layer file. Cause: layoutcheck
  compiles every source with cl, but with options it writes itself, so it never sees the build's. Fix:
  each compiler's flags sit in `if(MSVC)` in CMakeLists.txt, so a new flag goes in both branches. To
  check them, configure a separate tree from a vcvars64 shell with `-G Ninja` and cl.
- **`u_long128` is a struct under cl** (2026-11-05): `C2440` on an integer initialiser or a cast, fine
  under GCC, whose `__int128` it is. Keep masks and offsets in `uintptr_t`; layoutcheck catches this now.
- **Exit 0x20474343** is an uncaught C++ exception; **3221225477** an access violation: `gdbrun.py`
  with the same pad for the backtrace (the game runs on its own named thread).
- **A run without the scripted pad** sits in the first menu: a trace in later code prints nothing.
- **A smoke run can write the user's ini**: runs pass their own settings.
- **A fresh build directory (a new worktree) fails in `panels.cpp`** (2026-11-07): `no matching
  function for call to 'PlotLine(...)'`. Cause: `build.py` configures with a sibling's fetched sources,
  and RosePan's `implot-src` is ImPlot 1.0 where YamiPan pins v0.17 (`cmake/imgui.cmake`). Fix:
  configure by hand with `-DFETCHCONTENT_SOURCE_DIR_{SDL3,IMGUI,IMPLOT}` set to the main checkout's
  `cmake-build-relwithdebinfo/_deps/*-src`, then `ninja`.

- **`py -` with a heredoc hangs** (2026-11-05). Symptom: a Bash command never returns, and its output fills with `_pyrepl` tracebacks (`WinError 123`). Cause: `py -` starts the interactive REPL instead of reading the heredoc. Fix: write the script with Write and run the file.

- **A long Python edit in a Bash heredoc breaks (2026-11-05).** Symptom: `unexpected EOF while looking for matching '`, or a `\n` meant for C code arrives as a real newline and the file no longer compiles. Cause: the text passes through the shell tool first. Fix: write the edit script to the scratchpad with Write and run that file; in a temporary probe use `putchar(10)`.

## Platform layer (MioPan's, through ReiPan and RosePan)

- **`sceGsSyncV` hung at boot** (2026-11-03): `gameInit` waits two vblanks before anything starts the
  clock. Fix: the field clock runs from `Kernel::startMainThread`, as the console's does from power-on.
- **MinGW's timed waits ignore the 1 ms timer period** (2026-11-07). Symptom: the 50 Hz field clock
  delivered fields 8 ms late on average (up to 16 ms), so a frame that needs two fields sometimes got
  25 ms instead of 40 and took three; `YamiPan_Sleep(1)` (rxthread.c's idle loops) took 15.4 ms. Cause:
  `std::condition_variable::wait_until` and `std::this_thread::sleep_for` under MinGW's libstdc++ land
  on the 15.6 ms system tick even with SDL's `timeBeginPeriod(1)` in force (measured in one process:
  Win32 `Sleep(1)` 1.6 ms, `sleep_for(1ms)` 15.5 ms; `timeBeginPeriod` and the power-throttling opt-out
  change nothing for the std calls). Fix: `os::TimedWaiter`, an SDL semaphore (a wake() ends the wait at
  once) until 2 ms before the deadline, then `SDL_DelayNS` slices, a high-resolution waitable timer on
  Windows; `YamiPan_Sleep` through `SDL_DelayNS` too. No Win32 of its own (the project includes no
  windows.h).
- **A null call in the job worker** (2026-11-03, `FileJob::run`): `rxInit` is a stub, so the rx file hooks
  are null. Not a kernel bug: the boot reached the file layer (open-items.md "Next up" 8a).

- **SDK shims only answer what the last game asked**: a placeholder answer looks like missing hardware
  (FF3's pad check stopped every run on `scePadInfoPressMode`). Read the shim against Sony's semantics.
- **An EE wait on an IOP status never ends when no service answers the RPC**; when adding an IOP
  command, its payload size must match what the EE sends.
- **RosePan's `sdk/libgraph.h` swaps `SCE_GS_INTERLACE` / `SCE_GS_NOINTERLACE`** (2026-11-03, TU 25):
  it defines INTERLACE 0 / NOINTERLACE 1; the SDK has NOINTERLACE 0 / INTERLACE 1 (sceGsResetGraph's
  `inter`). Symptom: a host that honours the mode runs FS2's 512x512 field-mode screen progressive
  and the 256x256 one interlaced. Fix: correct the values when bootstrap 6 copies the header (and
  tell RosePan).
- **A missing memory-card directory must answer -4** (`sceMcResNoEntry`), or a fresh card reads damaged.
- **GS colour**: textured vertex colours above 0x80 brighten (MODULATE is `(Ct * Cf) >> 7`); a fragment
  with alpha 0 is still written on the GS; two GS origins in one game (top-left pixel constants vs
  centre-relative 2048): derive a host quad from its own packet's constants; decode PRIM from the GIF
  tag (strip / fan / sprite) before building the quad.
- **Threads**: SleepThread on the EE main thread needs its own wakeup counter; SuspendThread used as a
  mutex must be honoured by host threads reading guarded state; the OS main thread pumps events while
  the game runs on its own thread (Windows' modal move loop froze single-threaded runs).
- **Movies**: a decode loop that drops its frame on a starved demuxer freezes for good: decode into a
  spare frame, swap on success.
- **A movie's sound underruns though the decoder keeps time** (2026-11-13, game/movieplayer.cpp). Symptom:
  `rxsnddrv: ADPCM .. underrun` near the same stream position each run, the audio ring at 0 while the video
  ring is full. Cause: the demux stops at the first video packet the ring refuses, and FS2's PSS put their
  audio up to ~1.5 s behind their video in the file, more than the ROM's 0x30000 ring holds. Fix: the host
  decoder may hold `kMovieHostVideoLead` beyond the ring; a walk of the file (stream id + PTS per packet,
  the demux held at ring + lead) sizes it before any run. Do not grow the ring: the work area can be a
  borrowed game buffer.
- **`feedIpu` spins forever once its ring is spent** (2026-11-13). Symptom: a movie's sound keys on, underruns
  after a few KB, nothing follows; the game is stuck in `MoviePlayer::feedIpu`. Cause: its loop demuxes while
  the video ring holds under 4 KB; behind a full audio ring the demux takes nothing, and the spinning game
  thread starves the sound update that would drain it. Fix: on the host the loop also needs the decoder's
  own backlog short (`YamiPanMpegVideoPending`), and `isDrained` waits for it, or the film ends early.
- **A host convention on top of a ROM-faithful port** (a vector negated twice; a host shader replacing
  microcode drops its culling): when a picture is mirrored or misplaced, list every sign / transform
  between the ROM's value and the shader.

- 2026-11-03 (game/scene.cpp): **a table's unread first field moves Ghidra's base.** Symptom: the
  decompiler showed gameSceneThread's scene table at 0x2fabd4 (`PTR_FUN_002fabd4[i * 6]`), and the
  words there did not fit threadSlotCreate's arguments. Cause: field +0 (a name string) is never
  read, so Ghidra folds the +4 into the base address. Fix: read the `lui` / `addiu` pair in the asm
  (`ghidra.py asm`): the base was 0x2fabd0, the entry `{name, func, cleanup, arg, stackSize, stack}`.

- **A pad script from an older note lands on the wrong screen (2026-11-03).** Symptom: RIGHT then CROSS on the title opens Load Game, or the title never answers. Cause: pad timing is in reads and the read rate changes between machines and runs (now ~0.76 a frame), so an old script's boot presses were still playing on the title, and the unanswered title loops ~1000 frames up, ~1000 black. Fix: boot presses only up to the logo, one RIGHT, one CROSS, and **read the highlight in the shots** rather than trust the script (open-items.md has the current one). A press in the script is not a press the game saw.

- **stubgen copies `public:` into a stub** (2026-11-04). Symptom: `expected unqualified-id before 'public'` in unassigned_stubs.cpp, and a stray `return {};` in a void stub. Cause: the first member right after `public:` is read with that line as part of its declaration. Fix: put a comment line between `public:` and the first annotated member.
- **`name_ctx.py` takes a path** (2026-11-04). `name_ctx.py 095` says "named 0/0" and still refreshes. Pass `work/batches/095.json`.
- **Address lists in comments count only the first address of a line** (2026-11-04). Symptom: a finished file still shows thunks and inline getters as todo in the brief. Cause: the tools read one FS2 address per comment line. Fix: one `//   FS2 0x...  what` per line, also for what lives inline in the header.
- **Stand-ins of one rena class can hide under several names** (2026-11-04). `StageModelData`, `RenaModel` and `ModelInstance` were the same object with three host layouts, and `ModelHeader` / `ModelPart` / `ModelGeometry` were the file's node / mesh / parts records. Before adding a stand-in for a rena type, grep which offsets the others read and check the constructor.
- **A class name may already be taken by another subsystem** (2026-11-04): `StageEffect` is StageSound's one-shot, so grep the name across `src/game` before adding a stand-in, or every file fails with a redefinition.
- **A pad file path does nothing, silently** (2026-11-04). Symptom: a New Game run sits black with only 22 stubs, no error. Cause: `build.py --pad` and `gdbrun.py --pad` take the script text; a path is parsed as a script with no entries. Fix: `--pad "$(cat work/newgame_pad.txt)"`.
- **A later `build.py --run` overwrites work/run_relwithdebinfo.log** (2026-11-04), even `--run 1`: copy the log first. `work/gdb/log.txt` keeps gdb's run.
- **Stub constructors leave a real base's subclass fields as heap garbage** (2026-11-04). Symptom: 0xbaadf00d in `StageSound::update` from a camera's `m_linked`. Cause: `StageCamera::StageCamera` is a stub, so nothing set the field. Fix until the ctor is written: `// PC:` zeroing in `StageObject::StageObject`.

## Harness and shell (2026-11-04)

- **A long Bash heredoc dies on an apostrophe; a heredoc's backslash can come out as a real newline** (2026-11-04, game/playermotion.cpp). Symptom: "unexpected EOF while looking for matching" before anything ran, or a C string in the source cut in two. Fix: write the edit script with the Write tool into the scratchpad and run it with `py`. Also, `cd` in one Bash call carries to the next: use absolute paths.
- **A stub remover that stops at the first closing brace eats half of a return-brace stub** (2026-11-04). `return {};` holds a brace: search for the closing one after it. tools/reverse has no script for this yet.
- **"RM2" inside other data.** Symptom: a disc harness crashes in `relocateNode` with a null node on
  ROM.006. Cause: 4 16-aligned hits of the magic in a pack are not models (base 0x324d46: odd). Fix:
  only a file whose base +0x30 is 0 on the disc is a model. The game itself never scans, so this is
  a harness trap only.
- **Models are not on the disc raw.** Symptom: a scan of ROM.00x finds no aligned RM2 (S2D, TimM and
  fMsg files do turn up raw). Cause: models live in the SLDI packs. Fix: scan every 0x800 for "SLDI"
  version 1 and `sldiDecompress` into a 0x1800000 buffer (game/sldi.cpp links alone).
- **Backslashes in a heredoc.** Symptom: `\\n` written in a Bash heredoc reaches Python as a real
  newline; a long heredoc can die with "unexpected EOF". Fix: put edit scripts in a file with the Write
  tool and run that.
- **Source files may be CRLF.** A replace script with LF in its search text fails its assert. Fix:
  normalise to LF, replace, and write the file back as it was.

**A here-document can break the Bash tool (2026-11-04).** Symptom: `unexpected EOF while looking for matching` on a command whose quoted heredoc is correct in a terminal. Cause: not found; both times the body held triple quotes and apostrophes. Fix: write the script with the Write tool into the scratchpad and run it; do not retry the heredoc. Also: a stray `cat > file` with no input hangs until the 60-second timeout.

**A Bash heredoc with an apostrophe in it fails (2026-11-05).** Symptom: exit 2, "unexpected EOF while looking for matching" a quote, on a command whose heredoc delimiter is quoted, and none of the command runs, also what comes before it. Cause: the tool parses the text first. Fix: write the file with the Write tool and run it.

## VU1 addresses are not EE offsets (2026-11-04)

Symptom: the VU1 program tests "address 15" and nothing writes bss +15 quadwords; `xrefs` on the block it seems to name finds a view matrix. Cause: what VU1 reads at a low address comes from a UNPACK in the packet, not from the block the upload function fills; 0x2e5bb0's `pauVar21[0xc]` is an offset in its own buffer. Fix: search the code for the UNPACK word itself (`lui 0x6c01` + `ori ..., 0xf`, a short scan of the ELF), and print `dest` on `ilw`: address 15 is three fields, x the pass mask, y the model flags, z the mesh's.

## Stubs that answer null (2026-11-04)

Symptom: a loader "ran", instances were counted, yet nothing appears and the stub never shows in the reached list of the next function. Cause: `createMapObject` was a stub answering null and `applyBgTarget` skips a null; 3D path TU 4 counted 22 instances and read that as success, so TU 5 built a renderer for an empty record. Fix: when something does not reach the draw, walk its callers for a `continue` on a stub's answer, and read the reached stubs in work/run_relwithdebinfo.log, not build.py's output, which only shows its tail.

- **A stub answering no can leave an object half built** (2026-11-05). Symptom: a null pose in `ModelPose::play` under a script native, on a model that loaded. Cause: `isMotionTarget` said no to every object, so nothing called `setMotionList`, and that is what makes a pose. Fix: a guard only moves the crash to the next native (it did): trace the object and name, then look for who should have filled it.
- **A shot's number is not a game frame** (2026-11-05). Symptom: a forced event at game frame 500 is nowhere near shot 500 or 5000. Cause: the host presents far faster than the game steps (about 9 to 1 on a stage, 148,000 presents in 260 s), and the ratio moves with the window size. Fix: `py shotscan.py LOW HIGH STEP` prints three rows' brightness per shot, in numeric order (`ls` order hides the run's order); find the change, then Read those shots. Retry after a game over plays Mission 1 and the hints again, so the stage is not the last bright stretch of the run.
- **To force what a stage does not reach yet** (2026-11-05). A temporary block before the letterbox test in `StageScene::update`, with `--env` frame numbers, set `currentMission()->letterbox` and called `startGameOver(player, nullptr)` on node 4 with the Start-on-a-stage line below, `--pad "$(cat work/newgame_pad.txt)"`, 260 s. Mark every line, remove by the mark.
- **A class the ROM builds and the host never did fails without a sound** (2026-11-06). Symptom: L2 did nothing, every test on the way to `SightJack::start` looked right, and the code that should call it was simply absent: no stub, no frontier entry. Cause: `ghidra.py refs` on `StageParty::start` shows a caller in a vtable class (0x27c300) that no written function reaches by call, so stubgen and the frontier never name it; `findLink` answers null for an unknown key and the motion plays with no code. Fix: when something is never reached, run `ghidra.py refs` on it first and read every caller, the vtable ones too; open-items.md lists the other missing link keys.
- **Node 4 by pad reads, measured** (2026-11-06). About 23 reads a second, not 60: control comes near read 5000 (210 s), and a 300 s run ends near read 7000. Anything past that never fires, whatever the script says: the last two sessions' L2 at 12000.. and 10000.. were never pressed. `shot_<frame>` numbers run far faster (1,500 a second), so `--shot-every 1500` is about one a second.
- **A second pass that looks like the surface's own texture is a binding, not a formula** (2026-11-06). Symptom: the truck showed a fan of scales under the flashlight; two sessions read VU1 and compared UVs and found no difference. Cause: `drawOver` never bound its `picture` (commit 0580779 folded the spot's and the shadow's passes into it and dropped the line), so both passes sampled the surface's texture. `Renderer::uploadImages` also never uploaded `ModelPass::shadowImage`, so the shadow had no picture to bind. Fix: bind it, and upload both. Look at what is sampled before reading the microcode: the user saw it from the picture alone.
- **To test a stage, start on it** (2026-11-05). Replaying stage 1 costs minutes and a save prompt. A temporary line after `saveExtraInit` in `menuPlayLoop` that sets `savedSystem()->select.stage` from an `--env` variable reaches a node in under 3 minutes. The road from node 1 is 38, a film, then 4. A film node ends in the save dialog: with no card its default Yes only loops, and Left chooses No. The doorwalk pad holds the stick from read 8000, which drags the navigator's cursor; use a pad of Cross only there.
- **A clean scripted run proves nothing unless the marker fired** (2026-11-05). Symptom: a fix for a crash on turning left passed a 520 s run with the stick pushed left, and the marker on the suspect path stayed silent. Cause: the pad never brought the mesh into view, and the marker sat on the wrong branch: the theory was the list path, the truth was the plain one. Fix: put the marker after both branches, and when the user can reproduce in seconds, hand over the build with a temporary start on the stage. Then keep only what the marker proved: the list fallback and a colour guard written on that theory came out again.
- **A runner slot's default answers no without a word** (2026-11-05). Symptom: a stage's trigger never fires, and no stub or warning shows in the log. Cause: `ScriptRunner`'s virtuals are written as the ROM's defaults, so a class with no override, here `BoxVolume` on native 0x44, answers false silently; only stubs print. Fix: when a script waits forever, list what its natives call on its target and read that class's own table at the slot, not the base's.
- **A SceneRender set with no keys is all zero** (2026-11-04). Symptom: writing `update` as the ROM has it would turn every light off and give every camera range 0, so nothing draws. Cause: only script natives key the tracks; the ROM has no default. Fix: the entries of the stage's script key them (`game/scriptrunner.cpp`); the `isKeyed` guards that stood in are gone, so a stage whose script does not key a set really is dark. A one-key track never writes `value` either: `VectorTrack::step` needs count 2 (the scripts always make keys 0 and 1).

## Bash heredocs in the harness (2026-11-05)
A Bash call carrying a whole source file fails with ENAMETOOLONG (uv_spawn), and nothing has run: write big files with the Write tool (2026-11-05).

- **Symptom:** a long `cat <<EOF` or `py - <<E` in the Bash tool stops with "unexpected EOF while looking for matching" although the delimiter is quoted. **Cause:** not found; both failures held long bodies with apostrophes. **Fix:** write the file with the Write tool, and a patch script to the scratchpad, then run it.
- **Symptom:** a patch through `py - <<'EOF'` that wrote the text `\\0` (meant as backslash-zero in a comment) left a NUL byte: git called the header binary, `file` said "data" (2026-11-07, rena/tim2.h, architecture.md). **Cause:** the backslashes are unescaped once more on the way in. **Fix:** patch text holding backslashes with the Edit tool or a script file; check `file <path>` says text before finishing.
- **Symptom:** a first write to src/game/gamehooks.h from Python failed with PermissionError, a second a moment later worked. **Cause:** not found, the file was free by then. **Fix:** retry once before looking further.
- **Symptom:** `stubgen.py` printed nothing and left stubs whose declaration changed, so the build failed with no declaration matches. **Fix:** delete a written TU's stubs from src/lib/unassigned_stubs.cpp by hand first, as the protocol says; the build then adds the new prototypes' stubs.

## Debugging a run (2026-11-04)

- **`build.py --run` loses stderr when the run hits its limit.** Symptom: a trace with `fprintf(stderr, ...)` never appears in `work/run_relwithdebinfo.log` though the code ran. Cause: on timeout only stdout is kept, and a game run always ends by timeout. Fix: trace with `printf`, like `YAMIPAN_STUB`.
- **Shell heredocs with an apostrophe or a backslash-n go wrong here.** Symptom: "unexpected EOF while looking for matching" although the heredoc is quoted, or a `\n` in a C string arrives as a line break with a CR. Fix: write files with the Write tool, and scripts that hold source text too.

## A guard that sees another session (2026-11-04)

**Symptom:** `gdbrun.py` answers "YamiPan.exe is already running" with no run of this session alive, and every game run takes twice as long. **Cause:** another Claude session on the machine was running two copies of the game from its own scratchpad; `gdbrun.py` looks for the process name only. **Fix:** do not touch them; check `Get-CimInstance Win32_Process` for the path and parent before believing the guard. Run gdb from a scratch script for that one run, or print faults from a temporary vectored handler in main.cpp and read them with addr2line. Under that load a stage takes over 170 s to reach under gdb.

## GCC deletes `if (this == nullptr)` (2026-11-04)

**Symptom:** an access violation reading address 8 inside `StageScript::findEntry`, the moment a sub-scene builds its lights; the source plainly tests `this`. **Cause:** the ROM calls members on null on purpose (a sub-scene has no `scripts[0]`), and GCC at -O2 treats `this` as non-null and removes the test. **Fix:** `-fno-delete-null-pointer-checks` for that file (CMakeLists.txt, `set_source_files_properties`). A new `this == nullptr` in game code needs the same line. Also: **a piped `printf` is buffered**, so the last `[stub]` line of a crashed run is not the last stub it reached.

## A picture that looks like a place can still be wrong (2026-11-04)

**Symptom:** with no `YAMIPAN_CAMERA` a stage showed an alley between two walls, textured and in perfect perspective, and the notes took it for the map. The user could not make sense of it. **Cause:** two things together, neither of them the math. The unlinked camera stands at the world origin, about 9,500 units from stage 1's map, in empty space; and every map object but `Root` had been moved to that same origin. The ROM's map object ctor 0x1765e0 ends with `setDirtyTree(false)` and the port's did not, so the first `calcMatrices` rebuilt the world matrix `setModel` had copied from the node, from position and rotation, both zero. Bones went with it, to inverse(node) * bone. The alley was the inside of `oMK01_04`, the grey panel a door with its corner on the screen's centre. **Fix:** the ctor's call (mapobject.cpp). **Also:** an object that takes its matrix from outside must clear `dirty`: look for that call in every object ctor. And before believing a view, check the camera against the stage's own numbers (`F_PC01` and the `CAM_` records of its `.map`, architecture.md "Where stage 1 is"). With the fix that default view is black: nothing is wrong, nothing is there.

## A callcheck line lists what you leave out (2026-11-04)

**Symptom:** callcheck goes on reporting MISSING under a function with a `// callcheck:` line. **Cause:** the line lists the ROM's calls that the code does not make, not the calls it does make: a helper's name there explains nothing. **Fix:** copy the names from the `missing:` line and give the reason in parentheses. The same check reads the annotation nearest the function, so a second `FS2 0x...` on that comment line takes the function over (a jump stub: name it elsewhere).

## Scratch tests of pose math (2026-11-04)

**Symptom:** a solver seems to do nothing or to miss in a scratch test. **Cause:** `buildMatrix` only reads a rotation slot under `PoseTrack_Quat` or `_Euler`, so a hand-made bone needs its `tracks` bits; and a parent left turned by an earlier case moves the result. **Also:** the decompiler drops float and vector arguments of the VU helpers (`scaleVec4(a, b)`, `vecCross(a, a)`, `vecDot(a)`): read the asm around each such call. The EE's `sqrt.s` takes the magnitude, so a root of a small negative is a number there and NaN on the host: `sqrtf(fabsf(x))`.


## A run's stderr is lost when build.py stops it (2026-11-04)

**Symptom:** `fprintf(stderr, ...)` traces print nothing after `build.py --run N`, though the code ran. **Cause:** on the timeout path `build.py` keeps only stdout, and it lists only the first 60 `[stub]` lines. **Fix:** trace on stdout with an explicit flush, then read work/run_relwithdebinfo.log, not the printed summary. Budget it: a New Game run that reaches the event takes 6 minutes.

## Long shell commands and background runs (2026-11-04)

**Symptom:** the Bash tool answers `ENAMETOOLONG` for a big heredoc, or a background run dies at once with paths not found. **Cause:** the command line has a length limit, and a background command does not start in the directory the last foreground `cd` left. **Fix:** write source files and edit scripts with the Write tool; start every background command with an absolute `cd`.

- **A shot number is not a time (2026-11-04).** Symptom: "black from shot 4050" of one run was the intro movie in the next. Cause: movies and loading present frames at their own pace, so the same shot number lands elsewhere in each run. Fix: name a moment by the scene's `frameCount` or a native, printed on stdout; shots only by their file sizes within one run (2708 bytes is a pure black frame).
- **`work/run_relwithdebinfo.log` is written when the run ends (2026-11-04).** Symptom: greps during a run show the previous run's traces, and it looks as if a new trace never fired. Fix: wait for the end of the run before reading it; `work/shots/` does fill during the run, but holds old files past the new run's last frame.
- **An event that never ends (2026-11-04).** Symptom: black for ever after an event's last fade, the script stopped (`isRunning` 0). Cause: `going` also waits for the voice, and a stream only ends through its format's update setting RXSNDSTR_STATUS_END. Any format left as a stub holds a wait on the stream for ever. Fix: the format has to report the end, even when silent (rx/rxsndstradpcm.c, since 2026-11-06 the driver's silent side there, feeding rxSndAdpcmUpdate); the PCM format is still a stub, and any request of that format would hang the same way. Removing that block before a driver answers brings this back. Since 2026-11-09 RXSNDDRV runs on every run and the block steps aside while it serves: the driver's reports end the stream, so an event that hangs points at the driver's ADPCM / PCM path or at iop::Spu2's voice clock.

## YAMIPAN_CAMERA hides every view change (2026-11-04)

- Symptom: a native that switches the view changes nothing on screen; 15 shots are byte-identical across it, while the log says the top view changed. Cause: `StageCamera::calcViewMatrix` applies the debug camera to every StageCamera, not only the main one, so any map camera draws from the debug place. Fix: test view switches with `YAMIPAN_CAMERA` unset. A black shot from the unlinked main camera is then expected: it sits at the world origin, which is empty space.

## Temporary logging from the game (2026-11-04)

- Symptom: a `fprintf(stderr, ...)` added for a run shows nothing in work/run_<config>.log, which looks like the code was never reached. Cause: only stdout reaches that log (the stub lines are `printf`). Fix: `printf` and `fflush(stdout)`.
- Symptom: code generated through a Bash heredoc comes out with a real line break inside a C string. Cause: the shell tool turned a doubled backslash into a single one before Python saw it. Fix: write the script with the Write tool, and run it.

- **A New Game run reaches the stage at a different time each run** (2026-11-04, game/stagecollision.cpp): with the same work/newgame_pad.txt, 170 s ended before the stage loaded and 210 s got there. A probe that fires at stage load needs `--run 230`; before trusting "no output", check how far the log got.
- **A bash heredoc can eat a backslash and break on a quote** (2026-11-04): a C printf with a newline escape came out as a real newline, and a long heredoc holding apostrophes failed the whole command, so nothing ran. Write anything with quotes or escapes through the Write tool as a scratch script.
- **CRLF in the work tree**: `git` normalises to LF but files on disk may be CRLF or mixed, so a Python replace of a multi-line string misses. Open in text mode (universal newlines) and write with `newline=chr(10)`.

- **`mv` of a backup does not rebuild** (2026-11-04, game/stagecharacter.cpp): a file restored with `mv` keeps its old mtime, so ninja links the temporary code, and a 5-minute run tests the wrong binary (the log still printed TEMP). `touch` it, or restore with `cp`. Before a long run, build alone and grep the log's first run for the temporary line. A second run was lost the same way: a helper that failed printed its traceback above a run started by `;`, not `&&`.
- **A character's pose is not placed by setup** (2026-11-04): after `setBaseMotion` + `bindBones` + `placeBones` the bones stand at the world's origin (bn_pelvis 0, 0, 53) until something calls `StageModel::advance` or `applyMotion` with the object's position. Symptom: the object is at its map point, shown, drawn, and nothing is there. An object's own position says nothing about its pose.

## Testing a run (2026-11-04)

- **Prints from a temporary test vanish.** Symptom: a test's `printf` never reached `work/run_relwithdebinfo.log`, so the branch looked dead. Cause: `build.py` kills a run at its limit and stdout is block-buffered. Fix: `fflush(stdout)` after each line, and put a guard on `n` so it cannot flood.
- **`build.py` does not pass every variable on? Not proven:** a test behind `getenv("YAMIPAN_WALKTEST")` printed nothing, but that run also had the buffering above. Fix: no switch in a temporary test, it is removed before the commit anyway.
- **Shot numbers are not stage time.** The title loop presents about 1,500 frames a second, so a run reaches shot 400,000 and only about 1,400 of those frames are the stage. `newgame_pad.txt` counts pad reads, and a New Game needs 200+ s to reach the stage: an 80 s run ends on the title with no error at all. This answers "shot numbers ran ten times faster": it is the menu, not the game. Look at last.png before reading a run as a failed test.
- **A name made from a callee is a guess about the caller** (2026-11-05). The roadmap and the queue called 0x20d8c0 "the move" because it calls `getMoveScale`; it works out how visible the character is, and `moveScale` is concealment. Follow what writes the result (+0x7b8 here, read by the sight test) before believing either name.
- **`stubgen.py` does not remove a stub whose class or signature changed** (2026-11-05). Symptom: `no declaration matches` in src/lib/unassigned_stubs.cpp after moving a method to a subclass or changing a parameter. Fix: delete those stubs by hand, then build; the file is not regenerated from scratch.
- **Temporary C logging through a Bash heredoc** (2026-11-05). A backslash-n written in a Python string in a Bash heredoc reached the C source as a real newline, which cost two rebuilds. Put such a line in with the Edit tool. The same long command with a heredoc full of quotes also failed once with "unexpected EOF"; a script written with Write has neither trouble.
- **A handoff's address is a claim.** The note said Walk was 0x2775d0; that ctor's vtable holds DamageDead's timer. Check a link's name against the strings the ctor passes (`FUN_001eef90(map, name, ...)`) before reading its class.
- **A scripted stick past read 7000 never fires in a 285 s New Game run** (2026-11-04). Symptom: the stage runs, the character stands, sticks read 0, nothing blanks them. Cause: `YAMIPAN_AUTOPAD` counts pad reads, and work/newgame_pad.txt's own presses run to 11500 though the run ends near read 7030 (the stage is live from about 5600). Fix: put the pushes at reads 5720..6970, and cut the base script's CROSS presses from 6000 on. Also: a Bash command over about 32 KB fails with ENAMETOOLONG; large files go through Write.
- **A stub in a math helper looks like a logic bug** (2026-11-05). Symptom: `CharacterGround` found the floor and still sank 50 a frame, then fell for ever. Cause: `vecDistance` 0x135f20 was a stub answering 0, so `distance - 50` was -50; it had also silently broken "nearest of several meshes" in `StageScene::raycast`. Fix: written, inline in `game/vu0math.h`. Lesson: check the run log's `[stub]` list for the helpers a wrong number passes through before reading the logic; `grep "\[stub\]" work/run_relwithdebinfo.log`. `isPlayer` / `isPartner` were stubs answering false the same way.
- **A test's printf can vanish** (2026-11-05). Symptom: no lines from a temporary test in work/run_relwithdebinfo.log, and it reads as if the code never ran. Cause: stdout to a file is block-buffered, and a crash or `build.py`'s kill loses the tail. Fix: `fflush(stdout)` after each test line. **Also:** on the EE a null pointer plus an offset reads low memory and answers rubbish, on the PC it faults, so a native taking an object from a label cell needs a look at the null case.
- **A long teleport in a temporary test is a walk** (2026-11-05). Symptom: F_PC01, moved by a test next to a ledge, stood 85 units away and 55 higher a few frames later with no motion playing. Cause: `CharacterGround::sweepWalls` sweeps from last frame's position, so a far jump stops at the first wall on the way. Fix: test from where the sweep will not meet a wall, or go in two short steps; also clear the ground's `lastPosition` in the test. Also: `build.py` kills a run at its limit, so the newest shot and last.png can be empty files: read the one before.
- **`fprintf(stderr)` does not reach `work/run_relwithdebinfo.log`** (2026-11-05): only stdout does. Temporary traces: `printf` + `fflush(stdout)`.
- **A stubbed ctor leaves a host object of garbage** (2026-11-05). Symptom: an access violation in 3 of 4 runs, never under gdb, nowhere near the new code's logic. Cause: a stub body runs no member init, and plain `new` is the host heap, so every field holds what the heap held; `StageMenu`'s `pending` and `pendingOn` were read as a string. Fix: when written code starts reading a class, write its ctor (or give its members default initialisers) first. Still stubbed and worth a look when their fields get read: `PauseMenuState`, `SceneUiState`, `KeepList`, `StageParty`, `AreaClip`, `SceneBoxList`, `Rand48` (`grep -n "::.*(.*)$" src/lib/unassigned_stubs.cpp`).
- **A crash that hides from gdb: read `[crash]` first** (2026-11-05). Every run now prints the fault and its stack on stdout (`yamipan::crash`): signal handlers (per thread under Windows' CRT, so `os::Thread` installs them in each thread) and std::terminate, with the frames symbolised by `<stacktrace>` (MinGW links `stdc++exp`). No registers or fault address since the Win32 filter went (2026-11-18, no windows.h); `crashsym.py` read the old `exe+0x` lines. Several plain runs side by side reproduce a rare fault in one go; two of three caught this one.
- **A hook folded into another function changes who runs first** (2026-11-05). Symptom: a script's request reached no one: native 0x87 answered 1 and `wants.useDoor` was 0 at the condition. Cause: the character's start hook lived in `stageUpdateCharacterMotions`, after `runStageScripts`, where the ROM has it in `actionStart`, before. Fix: the ROM's order (`sideHook0`, `sideHook1`). When a PC comment says "this is their order", check what runs between.
- **Line endings and rewriting source from a script** (2026-11-05). Symptom: a five-line edit showed as 10,000 changed lines, then string literals broke across lines. Cause: Python text-mode `open` turns CRLF sources mixed, and `\\n` in an inline heredoc is eaten by the shell. Fix: write the edit script with the Write tool, into the scratchpad, and read and write bytes (`'rb'`, `'wb'`), or use Edit; files can be LF or CRLF, so detect it.

- **A Bash heredoc with an apostrophe in it fails here** (2026-11-05). Symptom: `unexpected EOF while looking for matching` on a line inside a quoted heredoc, and nothing runs, so the edits did not happen. Fix: write the text with the Write tool into the scratchpad and run or `cat` it. `sed -i` also turns a working copy from CRLF to LF, which is harmless (git normalises) but breaks a later match on `\r\n`.
- **`gStageScene->files` belongs to a sub-scene** (2026-11-05). The stage's own is `getStageFiles()`. A temporary test that used the field crashed with an empty log, and lived once it used the function; the stack top was lost, so null there is the likely cause, not a proven one.
- **The first stage has no items** (2026-11-05): group 3 stays empty and no `gItemDefs` name has a file group, so a pickup cannot be tested there, not even by calling `placeItem`.

- **A temporary log with a relative path writes nothing (2026-11-05).** Symptom: `fopen("work/x.log")` in game code gives no file and no error, and a run looks as if the code was never reached. Cause: the game does not run from tools/reverse. Fix: an absolute path. In the same runs: shot numbers count presented frames (900,000 in a run), pad reads do not, and the stage goes live near read 5600, about 300 s into a New Game: a 200 s run ends before the player has control.

- **A temporary `new StageWeaponItem` crashes the next frame (2026-11-05).** Symptom: `read of 0x18` in `StageModel::advance`, from `ScenePairTable::actionMovement`. Cause: `StageItem`'s ctor joins `ScenePairList_Items` at once, and an item with no model then gets its movement hook. Fix: `gStageScene->pairs->remove(item)` right after `new` in a test that only needs the object's fields.
- **A monitor on work/run_relwithdebinfo.log sees the last run (2026-11-05).** Symptom: "CRASH" the moment a new run starts. Cause: `build.py` writes that log only when it returns, so during a run it is the last run's, whole, crash lines and temporary prints included. Fix: watch for `YamiPan.exe` leaving `tasklist`; shots in work/shots show a run is alive; read the log after that.
- **A stray `cat > file` in a one-line Bash command waits on stdin for ever (2026-11-05).** It hung a command for two minutes with nothing to show. Give every tool call in a long line `</dev/null` and a `timeout`.
- **A base-class stub can eat a game rule without a sign (2026-11-05).** Symptom: F_PC01 from behind for days, and notes calling the picture right. Cause: stage 1's Root sets the flag that forces the view, and the store ended in `ScriptRunner::setProperty`, a stub that logs its first hit once among hundreds of `[stub]` lines. Fix: when the game does less than the user describes, ask what the script stores: scanning the script file for stores with `ScriptVar_Property` found it in minutes.
- **A field name from one reader can be backwards (2026-11-05).** Symptom: `prepareDrawFlags` read as "the part a camera shows", and the queue asked what hides the body. Cause: `drawMode` 2 was named from that one function; `drawTree` and `PartSwitch` say 2 is hidden. Fix: read the consumer of a flag before naming it, here with a scan of the ELF for the byte's offset.
- **`build.py --run` builds first, so restoring the source while it starts tests the restored game (2026-11-06).** Symptom: a background run with temporary code wrote no log and showed stage 1. Cause: the source was restored seconds after the command started, and the build step picked that up. Fix: restore only after the run ends, or run the exe directly.
- **A pad press on a round period can lock to a cycle the press itself causes (2026-11-06, game/stagecar.cpp).** Symptom: two runs at the truck's door pressed X every 80 reads, the prompt showed Get in, and nothing happened, which read as a bug in the door code. Cause, from a log of focus and presses per frame: the cycle was 320 frames, four times the press period, and no press fell inside the 45 frames the item list held the focus. X also closes the stage's hint, which is probably what keeps the two in step; that part was not proven. No code was wrong. Fix: use a prime period (X every 37 reads got in on the first try), or log focus and presses with a frame number before reading any code. Before calling a button dead, count presses inside the window.
- **`--pad` timings are pad reads, not frames, and a run's frame rate is not stable (2026-11-05).** Symptom: the same pad script reached stage 1 at frame ~7000 in one run and ~285000 in another (with the Cross presses after read 5300 dropped), at 30 fps and at about 700. Cause: unmeasured. Fix: find a shot by its size or by a log line, not by the frame the last run had.
- **A long Bash call with several heredocs did nothing (2026-11-05).** Symptom: `unexpected EOF while looking for matching` and not one of its commands ran, the earlier ones included. Fix: Write the files, then run a script; put the checks in a second call.

## Reading vtables and register arguments (2026-11-05, game/characterbrain.cpp)

- **Symptom:** a vtable read as 8-byte `{delta, index, pfn}` gives nonsense deltas such as -31184. **Cause:** FS2 is CodeWarrior: vtables are plain 4-byte pointers; the GCC 2 shape in reconstruction.md is not this game's. **Fix:** read words; `0x8630` / `0x1d` was simply 0x1d8630.
- **Symptom:** Ghidra shows `FUN(0, route, obj, force)` with a leading 0, or a call with too few arguments. **Cause:** the first shown is f12 (a float), then a0..; an argument in t0 is dropped when the caller set it far before the call. **Fix:** read the callee's prologue (`mov.s f20,f12`, which a-registers it saves) and `mipsdis.py lo hi` at the call.
- **Symptom:** `ghidra.py asm` in a loop hits the tool's time limit. **Cause:** each call starts a Ghidra lookup. **Fix:** `mipsdis.py` is offline and takes under a second.
- **Symptom:** a `bash` tool call hangs on `python -` or `cat > file` with no input, or dies with "unexpected EOF" on a long heredoc holding quotes. **Fix:** write long edit scripts with the Write tool into the scratchpad and run them with `py`.

## Reading VU1 (2026-11-05, yamipan/rendering/model_renderer.cpp)

- **Symptom:** `bal it=vi31 is=vi0 imm=0` in the middle of a vertex loop, and `muli` by a constant nobody seems to load. **Cause:** vudis.py printed a LOI's float as if it were a lower instruction; 0x437f0000 reads as that `bal`. **Fix:** done, it prints `loi 255`. Any note written from the older listing may have read a constant as a branch.
- **Symptom:** a VU flag test seems to read the wrong instruction (`fmand` after a `maddz`, where the code only makes sense for the `subx` after it). **Cause:** MAC flags appear 4 cycles after the FMAC issues, counted in cycles and not in lines: a `div` waiting for the register that the `maddz` writes stalls one cycle and moves the test one instruction on. **Fix:** count the stalls before deciding which result a flag test sees (0x334..0x337).
- **Symptom:** a GIF packet in data seems short of a register. **Cause:** REGS is read nibble by nibble from the low end, so bytes `ee ee 6e` are e, e, e, e, e, 6, not e, e, e, e, 6. **Fix:** count NREG nibbles.
- **Symptom:** "a second GS pass" in the notes turned out to be another feature: the alpha fringe of 0x2fdb60 was the shadow's state. **Cause:** the note was written from the packet's TEST and ALPHA without asking what its TEX0 is. **Fix:** read TEX0 and the kernel that kicks a state before naming a pass.

**A probe's output disappears from a timed run (2026-11-05).** Symptom: `fprintf(stderr, ...)` in a run under `build.py --run` shows in no log, and a `grep` says 0. Cause: `build.py` keeps only stdout of a run it stops at the limit. Fix: a temporary probe writes its own file (the working directory is `cmake-build-relwithdebinfo/YamiPan/`). Sample on the condition itself, not every Nth pass: a frame has several passes, so `n % 200` can always land on the same one, and it was a pass with no shadows.

**A coloured test can hide in a modulated pass (2026-11-05).** Symptom: a shadow forced red shows nothing. Cause: the pass multiplies by the texture, and a dark picture makes the vertex colour black. Fix: bind the white texture for the test as well, and first check that the camera can see the place at all.

**Plates on a wall clip in and out, the depth is fine by every test (2026-11-06).** Symptom: flat parts just over a surface come and go as the camera moves; the panels in the elevator car, stage 9, show it non-stop. Cause: `buildWorldToClip` turned the GS's Z into the usual 0 at the near plane, `(zmax w - z) / range`, about `1 - 1 / w`. The shader gets there from a clip z nearly equal to its clip w, each its own float sum with its own rounding, at world coordinates around 10000 and a near plane of 1. A simulation in float32 lost a plate 2 units over a wall on 6% of its points at distance 1000 and 23% at 4000; the GS's order lost none. Fix: keep the GS's order, zmax near, GEQUAL over a clear of 0, as RosePan and ReiPan do. A depth test that matches the ROM's registers says nothing about its precision. The screenshots did not show this: shots at the frame's 512 lines, one every 50 frames, looked whole under both mappings, and only the live window told them apart (the same run on each mapping, the user watching). Why the shots missed it is not known. Judge a flicker in the live window, not from shots.

## Reading tables by hand (2026-11-05)

- **`scriptnatives.inc` has a comment line with the same shape as an entry.** Symptom: a quick script pairs each native id with the previous id's address, and the slots in a handoff note look wrong (0x4a seemed to call +0x108; it calls +0x10c). Cause: `re.findall(r'\{[^}]*\}')` also takes `{pops, code words, ROM address}` from the header comment, so every index is off by one. Fix: match the `/* 0x019 */` key with the entry, never count entries. The tell that saved the session: a native already bound disagreed with its new id.
- **Primary vtable offsets are the runner's + 0x3c.** CodeWarrior's runner table sits inside the primary one (0x312240 + 0x3c = 0x31227c), the primary's own slots follow it, so rtti.py's 190-slot runner table holds them too: primary +0x2f8 is the word at 0x312240 + 0x2f8, listed at offset 0x2bc of 0x31227c. Do not look it up by the same number in the runner's list.
- **A decompile can show a tail callee's body.** 0x210930 is 0x10 bytes, a tail jump to 0x2179c0 on the table at +0x468, and Ghidra prints the callee as its own body. callcheck says so (missing FUN_002179c0): check with `ghidra.py func` before you trust the size.
- **A two-word `return 0` can hide after another function's epilogue** (2026-11-07). Symptom: the brief lists a tail callee as `FUN_<addr>` that `yamipan_functions.txt` lacks, and an inline host body says `return 0` where the ROM jumps somewhere. Cause: 0x1dd2e0 (`jr ra; move v0,zero`, CharacterBrain::answerNo) is reached only by a `j` from 0x21b960, so neither analysis nor codeptrs.py started it and CharacterBrain::dropSlots's body swallowed it. Fix: `ghidra.py asm` the callee; when it sits right after a `jr ra` pair, `ghidra.py mkfunc <addr>` (Ghidra trims the host function), name it, and write the jump as the call it is.
- **A `// callcheck:` note stops at its first parenthesis** (2026-11-05). Symptom: names listed after `name (why),` still show as missing. Cause: callcheck reads `[\w, ~\[\]=]+`, so the note must be names first and one parenthesis last. Fix: `// callcheck: a, b, c (why a; why c)`.
- **A Python edit script in a Bash heredoc can fail in this harness** (2026-11-05). Symptom: `unexpected EOF while looking for matching` on a quoted `'PY'` heredoc holding triple quotes. Fix: write the script to the scratchpad with Write and run it.  Parallel Bash calls also race on the shell's directory: a `cd` in one can land in the other, so send them one after another.
- **Another session can share this checkout during a scheduled run** (2026-11-05). Symptom: `PermissionError` writing a header that is neither read-only nor denied by its ACL, and `git status` shows files the run never touched. Cause: preflight only sees the tree at the start; another Claude Code session was editing `layoutcheck.py` and running `cl` over the sources, and a compiler holding a header open makes the write a sharing violation. Fix: look at the `git status` and at the `cl` parents before retrying, retry once the compile ends, leave its files alone, and read `git status` again before `tus.py finish`, whose `git add -A` takes everything. It also sees half-applied edits, so keep a multi-file edit short.
- **`import build` starts a build** (2026-11-05). build.py has no main guard. Symptom: a one-line import hangs past its timeout. Fix: read the file; run it only to build.
- **A swap of two Ghidra names fails** (2026-11-05). Symptom: name_ctx.py says `name already used` for both, `named 0/2`. Cause: each waits for the other, the batch's retry cannot end it. Fix: a batch to two temporary names, then the real one. With `--no-refresh` nothing saves: call `yamipan.save_program()`.
- **Flat 2D draws came out twice as bright** (2026-11-05). Symptom: a panel of (0xc0, 0x90, 0x40) on screen as (255, 255, 127). Cause: 0x80 = 1.0 belongs to MODULATE, which only textured primitives have; a flat primitive's colour goes out as it is, and the shader scaled both (white stand-in texture). Alpha stays 0x80 = opaque either way. Fix: the scale is a uniform, chosen by whether the draw has an image. Any colour judged by eye on a flat fill before this date was judged on the wrong brightness.
- **A 2D element too dark or too brief to judge** (2026-11-05). Do not wait for a room or a script: in a temporary build force the inputs (here the line widths) and draw a flat panel under it, then compose the expected picture from the asset and compare per pixel, mapping through the 512 wide frame. A dump for it: `--env YAMIPAN_TEX_DUMP=<dir>`, and pick the file by size; a picture with flat alpha is not the strip however well its size fits. PNG shots are filtered: a scratch reader must undo the filters, and there is no PIL here.
- **One-pixel ticks at the seams of tiled sprites, at some window sizes only** (2026-11-05). Symptom: thin vertical lines on ScenePool's strip at 960 x 720, none at 683 x 512. Cause: a pixel centre exactly on a sprite's edge, and the UV rounds into the next cell of the sheet. Fix: UiCanvas insets sprite UVs by 1/64 texel. Test: a picture test at one render size says nothing about another, and shots are scaled to 512 high, so the window size of a run must be stated with its numbers.

**A long hand-translated search can be proved, not just compiled (2026-11-05).** Symptom: 1,600-instruction functions (0x253420, 0x255330) that no smoke run reaches. Cause: the walk needs a foe on a stage the pad does not get to. Fix: `tools/reverse/eerun.py` runs the ROM's own function on the data from the disc beside the host's in a small exe, `rutecmp.py` is the pattern. Traps met: a float register carried a pointer (0x256b24), so it keeps bits, not a float; `tus.py check` rewrites work/feat_fs2.json, so a comparison started beside it dies on half a file; and the same ROM's magic test passes for both cases, "ord" on disc and "ORD" after the fix-up.

## Proving code no script reaches (2026-11-06)
- **Symptom:** a whole class of AI code is written, and no run reaches it: the route's tasks stayed unseen for two runs because nothing gives a brain its scripts. **Fix:** a temporary driver, removed before the commit. At the top of `CharacterRoute::update`, every 7 s, give the first character with `getLifeState() == LifeState_Up` the next command, and log the command, its answer, the position and the motion to a file; `typeid(*task).name()` in `setAction` and `setMove` logs what started (rtti is on). Start on the stage with the temporary line of "To test a stage, start on it". It took 280 s and proved more than the two runs before it. **Traps:** `build.py --run` keeps only stdout of a run it stops, so log to a file, not stderr; pick the character by life state, the first `update` belongs to an NPC that never comes up.

- **A long inline Python heredoc fails in the Bash tool (2026-11-06).** Symptom: "unexpected EOF while looking for matching" or a backslash-n turning into a real newline in the output. Cause: odd apostrophes and escapes in a long command. Fix: Write the script to the scratchpad and run it. **A forced jack needs byte 9 really set (2026-11-06).** Symptom: the jack starts, the stick does nothing, mode stays 2. Cause: lifting the test in `stepAlt` is not enough, `CharacterPad::update` blanks the stick while the party runs unless byte 9 is set. Fix: set the flag. **Stage 4 is not reached by a pad (2026-11-06).** Symptom: a pad written for node 4 plays New Game's room and nothing fires. Cause: those runs had a temporary stage override, removed afterwards. Fix: temporary build, the stage line in game/menuscene.cpp.

## An offset named from one use (2026-11-06)

- **A field's note can be wrong in what it stores.** Symptom: open-items.md called +0x10 of the film "colour (0x44444444)" and `_b0` an unknown word, and 0x305b80 "blue". Cause: read from the constructor's constants, not from who reads them; +0x5c is the seed of `demoFleckRandNext`, +0x10 the colour (0), and a CLUT word's bytes are 0xAABBGGRR, so 0x331c00 is brown. Fix: name a field from its reader, and print a table's colours as RGB before describing them.
- **`git checkout` hands a file back with CRLF** under autocrlf, whatever it was in the working copy (`.claude/` and some of src are CRLF already). An edit script matching text with bare LF then finds nothing. Fix: load, turn CRLF to LF, edit, turn back.

## A parameter called small (2026-11-06)

- **`small` is a Windows macro.** Symptom: "expected ')' before 'const'" or "invalid use of incomplete type" in a header that looks right. Cause: rpcndr.h defines `small` as `char`, so a parameter of that name breaks in any unit that sees windows.h. Fix: `motorSmall`. `near`, `far`, `min`, `max`, `interface` go the same way.
- **eerun.py runs the libc only through hooks.** The ROM's memset and memcpy use MMI ops eerun does not have ("unsupported op .word"): hook them by name as rumblecmp.py does, in Python, with `ee.hook`.

## A heredoc with apostrophes and game code that moves registers (2026-11-06)

- **One long Bash call that writes several files can fail whole.** Symptom: "unexpected EOF while looking for matching" and nothing ran, not even the first line. Fix: write each file with the Write tool, and the edits as a Python script in the scratchpad.
- **CodeWarrior moves a 128-bit register with `paddub rd, rs, zero` (.word 0x70xx..28) and uses the second pipeline's `div1` / `mflo1` / `mult1`.** eerun.py has those and `pcpyld` since ripplecmp.py; the ROM's scratchpad (0x70000000) lands on low memory in eerun's mask, so a comparison reads it at that address.

## A stand-in that works is not the ROM's (2026-11-06)

Symptom: game/vu0math.h carried 30 stand-ins for weeks, written from the decompiler, and the game ran. Read
against the asm, five were wrong: `rotateByAngles` multiplied the other way round, `vecDirection` of equal points
was NaN and its w wrong, `vecNormalize` and `addScaledVec3` had the wrong w, `invertRigidMatrix` broke in place. Cause: the decompiler drops VU0 field masks
and broadcasts (hazards "Decompiler"), and identity or single-axis inputs hide a wrong order. Fix: VU0 code is
read in `ghidra.py asm`, never from the decompilation, and proved with a compare tool (vu0cmp.py); test
composition with two turns that do not commute. In eerun, a 64-bit move through qmtc2 leaves sign bits in the
upper lanes, NaN in Python and harmless on a VU: clear the vf's between calls.

- **Finished template code still counted as unwritten (2026-11-06).** Symptom: `status.py` stayed at the same reconstructed count after a TU written in a header. Cause: `yamipan.game_sources()` scans .c / .cpp only, and `ANNOT_RE` needs `FS2` before each address, so the StringMap tags in game/stagelist.h and the second address of any `FS2 0x1, 0x2` list are invisible too. Fix: list the addresses at the end of the TU's .cpp, each with its own `FS2 0x`, as game/playermotion.cpp does for `LiteralMap`. The older StringMap and StageList tags have not been moved.

- **A temporary log in a timed run must go to stdout (2026-11-06).** Symptom: `fprintf(stderr, ...)` lines never reach work/run_relwithdebinfo.log, and a run looks as if the code was not reached. Cause: `build.py --run N` keeps only stdout when the timeout stops the game. Fix: `printf` plus `fflush(stdout)`. Also check that the place is called at all: `StageMapObject::sideHook0` never runs for a plain map object in New Game's first room; `StageScene::updateVcms` is a safe per-frame spot.

- **The library's `sceVu0RotMatrix` 0x10bce0 is not the game's `rotationMatrixXyz` 0x135b90** (2026-11-06, game/vehiclepose.cpp). **Symptom:** a comparison with the ROM differs by thousands of ulps in a position turned by a small angle. **Cause:** the library takes the sine from the cosine by `sqrt(1 - c * c)`, so a small angle's sine is off: sin 0.047 is 0.0469817, truly 0.0469827. The game's own series is not. **Fix:** `sceVu0SinCos` and `sceVu0RotMatrixZ` in sdk/libvu0.h are the library's, to the bit; `sceVu0RotMatrixX` there still uses `cosf`, and a code path that calls the ROM's 0x10bce0 is not `rotationMatrixXyz`.


- **A UV counts in the GS's frame, not in the file's picture (2026-11-06, yamipan/rendering/texture_store.cpp).** **Symptom:** a character's head and half his back were "missing": dark, as if looking into the model, with the back of it gone. Walls and doors looked plausible but were stretched, so nothing said they were wrong. **Cause:** TEX0's TW and TH are the picture's size rounded up to a power of two, and S = 1 is the far side of that frame. A 192 x 128 file has TW 8, so S = 1 is texel 256 and the file's pixels end at 0.75. The host's Image was the file's size, so S = 1 was texel 192 and every UV on such a picture landed too far in: 153 of the 280 textures of stage 3 have the mismatch, and a body's atlas puts its UV islands on the wrong one. **It looked like culling and was not:** the model had neither flag, and blended depth was a real bug that changed nothing here. Dump what a model's flags say before reasoning from the look of a hole. **Fix:** `Tim2Decoder::decode` makes the Image the frame, the picture in its corner, its last row and column run on over the rest.
- **A run stopped by its limit keeps stdout only (2026-11-06).** Symptom: `fprintf(stderr, ...)` logs are missing from a `build.py --run` that ended "still running". Cause: on the timeout build.py reads only `t.stdout`. Fix: temporary logs go to stdout with `fflush`; `work/run_relwithdebinfo.log` has them, not the `--log` file.

- **A worktree of this repo neither builds nor checks as it stands (2026-11-06).** **Symptom:** `ImPlot::PlotLine` does not compile, and callcheck stops on a missing work/feat_fs2.json. **Cause:** build.py's first configure there picked up RosePan's older libraries, and work/ is ignored, so its cached files stay behind. **Fix:** set FETCHCONTENT_SOURCE_DIR_IMGUI, _IMPLOT and _SDL3 in the worktree's CMakeCache to YamiPan's own cmake-build-relwithdebinfo/_deps, and copy work/feat_fs2.json over.

- **A blend the host only approximates waits until something bright uses it (2026-11-06, yamipan/rendering/ui_canvas.cpp, model.frag.hlsl).** **Symptom:** the flashlight drew a hard-edged white square that turned with the lamp, with the road still visible inside it, blown out. With the spot, the shadow and the point light all switched off it stayed. **Cause:** the effect `dento1`, ALPHA_1 0x49, Cd * As + Cd, went to `BlendMode::DestinationBoost`, Cs * Cd + Cd, and a comment said so. Its sprite is white with the shape in alpha, so the whole quad doubled the frame. **Fix:** model.frag hands the blend As as the colour, after fog. **Look for it:** a blend whose source is the destination, or whose factor is not the one the GS has, is fine until the sprite's colour and alpha differ.

- **To tell which pass draws something, switch passes inside one run (2026-11-06).** **Symptom:** four runs of stage 7 with one pass off in each could not be compared: the runs are not in step, so the pose differed and the patch moved. **Fix:** a temporary global that the screenshot code advances, each shot in the next mode, `--shot-every` small: the same pose, one mode a shot, and a mode with everything off says at once when the answer is none of them. A "glow" with straight edges is a polygon: crop and enlarge before calling it fine (the earlier note did not).

- **Two sound tables answer the same kind of question (2026-11-06, game/playermotion.cpp).** **Symptom:** a voice that never plays, no error, and the code path looks right. **Cause:** `StageSound::findCue` searches the stream table, and `playVoiceHeard`'s inline search in the ROM is of the SE table: the host took the one for the other from a comment, and a null cue returns quietly. **Fix:** read which pointer the search walks (sound +8, +0 is the SE table). A temporary log of the name and whether it resolved finds this in one run. A stage's SE table holds only that stage's cast, so a null there is not proof of a bug either: test a name that fired.

- **preflight's clean tree only holds at its moment (2026-11-06).** **Symptom:** mid-run, `git diff --stat` showed 300 lines of teleport work in scriptrunner.cpp and teleport.cpp that were not this run's; an x64-Debug YamiPan.exe had started; two minutes later the same files were clean again, with no new commit and an empty stash list. **Cause:** a live session on the same tree. **Fix:** `tus.py finish` does `git add -A`, so look at `git status` right before it. If anything is not yours, use `--no-commit` and commit your own paths by name; never stash or revert what is not yours. `sed -i` can also fail with "Device or resource busy" while something holds a file: use the Edit tool.

- **A TU folded into a file leaves its claim behind** (2026-11-06). Symptom: `tus.py next --claim` offers an `unassigned@` key "resuming an unfinished session" with 0 functions and an empty brief. Cause: `finish` on the file's key does not release the claim on the old key, and `define` empties the run. Fix: `tus.py claim <file>` after `define`, before `finish`; `pick()` now skips an active record with no functions and no task.

## sqrt.s is not sqrtf (2026-11-06)

**Symptom:** a character's position goes NaN and the run crashes later somewhere innocent (`StageCollision::heightAtNode`, because a NaN passes the box test), with the log pointing at an enemy's attack. **Cause:** the EE's `sqrt.s` takes the absolute value; `sqrtf` of a value a rounding below zero is NaN. `CharacterGround::meetWall` has sqrt(1 - dot x dot) with a dot of two unit normals, and head-on faces give -1.0000001. A NaN then passes every test as "not less, not greater": `sine < 0.2` false, the clamp false. **Fix:** `sqrtf(fabsf(...))` wherever the operand is a difference, not a sum of squares. vu0math.cpp's helpers already do this; bare `sqrtf` in game code does not. **Found by:** a temporary one-shot print on the two places that write the position (`applyMovement`, `moveBy`): the first bad step and its caller. Other roots of a difference may be waiting: read the operand of each `sqrtf` in src/game.

## A stub can jam what is already written (2026-11-06)

**Symptom:** a character's voices play once or twice, then fall silent for the stage, with no error and every stub hit only once in the log. **Cause:** `playVoice` and `playVoiceHeard` were written and take a slot (`m_soundState`, `m_soundPriority`); the code that gives it back is the frame's `updateSounds`, a stub, so the priority test refused later voices for ever. **Fix:** write the frame's half. **The lesson:** when a written function takes a resource, a state or a counter, find who gives it back before calling it proved: a run that shows it once proves only the first time.
**The EE cannot make a NaN, so every NaN is a port bug, and the CPU can find them all (2026-11-06).** Its FPU has no infinity and no NaN: 0 / 0 and overflow give the largest float. So in a test build clear the invalid and zero-divide masks of MXCSR on the stage thread (`_mm_setcsr(_mm_getcsr() & ~0x3f & ~0x280)`, armed again each frame) with a vectored exception handler that logs RIP once per address, sets the masks in `ContextRecord->MxCsr` and `FltSave.MxCsr`, and continues. SSE faults arrive as 0xC00002B5. **It only reports the first fault of a frame**, so fix and run again: clearing the fog fault uncovered the bone chain behind it. Run with teleports to reach real play. Over all 144 file events, 19 s each, this found: `DrawSysFog` dividing by far - near with no fog set (`eeDivide`), `keyTime / keySpan` on a key with no length, libm `acosf` past 1 in `isPointInView` and `seesPoint` (`eeAcosf`), and `ModelPoseBone::rawMatrix`, which the ctor never sets, in the ROM as well, and `buildMatrix` skips on event and value bones, so a child multiplied by heap garbage. It also found by way of the crash handler `playNamed` on a motion the object does not have. Reading the code had found none of these but `stageparticle.cpp`'s sqrt.s.

**A forced motion can run with no motion in it (2026-11-06).** Symptom: a forced request reaches its link, the row's next follows and nothing crashes, yet the body never moves; motions last one frame or none. Cause: the row is the sheet's, for every character; the motion file comes with the character, and `requestedFile` answers null when this one has none. Fix: log `requestedFile` first, before reading anything into a forced run. Also, `requestNow` clears `m_walkPending`, so a link's walk from `onRequest` only runs through `request`.
**The disc has NaN normals, and they do nothing (2026-11-06).** The model files hold 0xffc00000, x86's "indefinite", in the normals of faces of no area: the exporter normalised nothing. Measured over every file event: 309 meshes on 14 stages, 2,145 vertices, exactly 715 triangles with all three corners NaN, no strips, largest area 0. So no pixel comes of them, here or on the PS2, and the VU1's treating the pattern as a huge number never shows. `usableNormal` in model_renderer.cpp swaps them out only to keep the trap clean: it reports the first fault of a frame, and an ordered compare on a NaN faults. After it: 0 faults over 309 events. **Trap:** a probe that remembers what it has seen needs no cap: one stopped at 4096 meshes, reported nothing, and sent this looking for code that writes vertex data, which does not exist.
- **A vtable offset past the primary table's end is a slot of a later table (2026-11-06).** Symptom: `lw t9,0x1f4(t9)` on an object's vptr at +0 reads like slot 123 of the runner's table (`scriptAddMenuLine`), which makes no sense. Cause: CodeWarrior lays a class's tables end to end, and the object's slots repeat after the first table's seven, so an offset from the +0 vptr is counted from there: 0x318310 + 0x1f4 is 0x31834c + 0x1b8, `getKind`. Fix: add the offsets to the table's address before reading the slot list of `rtti.py`.

## Shell and tools, slips of 2026-11-06

- **A run hung 5 minutes on nothing.** Symptom: a command ran past its timeout with empty output. Cause: a bare `cat > file` with no heredoc reads stdin forever; `py -` with an empty heredoc opens the REPL. Fix: put a throwaway script in the scratchpad with the Write tool.
- **`callcheck:` lists stop at the first parenthesis.** Symptom: MISSING calls that the comment plainly names. Cause: what follows a `(` is taken as a remark. Fix: remarks go at the end of the list. The same for `!`, `*` and `-` until 2026-11-09 (`operator!=`, `operator*`, `operator--` cut the list short); callcheck.py's list pattern takes them now.
- **`--log` is the build's log.** The game's output is in work/run_<config>.log; a temporary print is there, not in the file `--log` names.
- **A run killed by its limit leaves a cut-off shot.** last.png can be empty and the newest shot_*.png unreadable: read the third newest.

## A default's shape says the virtual's type (2026-11-07)

**Symptom:** natives 0x104, 0x105, 0x106, 0x15a stored 0 on 159 scripts, with a comment that explained it ("void in the ROM: their acc is what v0 held"). **Cause:** the base's bodies were never read for the type: the writer saw void-looking calls and a stored v0, and took the store for a leak. A native that keeps `andi v0,0xff` has a bool in hand, and a default ending `sltu v0,zero,v0` declares one. **Fix:** a comment that excuses a store is a claim, check it against the default's body; `py defaultcmp.py` does for scriptrunner.h.

### A queued TU at `verify` is skipped without a word (2026-11-07)

Symptom: the run before had queued game/playermotion.cpp for two stubs, and `tus.py next` gave a frontier TU instead. Cause: `pick` skips queue entries at done, verify or blocked, and the two stubs were not in that file at all: they sat in unassigned@00218500, which only `define` joins to it. Fix: `tus.py queue` now warns. To send the next run to stubs in an unassigned run, queue the run itself; when it happens anyway, read the handoff, then `define` and `claim` by hand and say so in the report.

### A smoke test that "fails" may be the place, not the code (2026-11-07)

Symptom: picking Set Smoke Candle did nothing and the wait gave up after one frame. Cause: the player stood at a ledge with the Down prompt, so `PutItemCondition` (word for word the ROM's) never allowed it. Fix: read the condition against the ROM before suspecting the new code, and walk somewhere open: stage 17, `780:LSDOWN*100` before the menu.

### Code past isSubScene does not run in a stage's opening (2026-11-07)

Symptom: a probe in `StageScene::update` or `updateSmokeFog` prints twice and then nothing for 30 s, while shots change and the stage thread sits in its normal `threadYield`. Cause: a stage's opening runs as a sub-scene, and the `if (!isSubScene)` block of the main scene does not run under it; it looks like a hang and is not. Fix: give a run 60 to 100 s before judging, and count frames from inside the block. With five instances at once the scene ran about 5 frames a second.

**A stub hides a null `this` (2026-11-07).** `Rand48`'s stubs never touched the object, so `rng->next` on a sub-scene (rng null, gStageScene during an event) looked fine; the real body would crash. When a stub becomes code, look at who can hold a null: the ctor's sub-scene branch in stagescene.cpp leaves about ten members null.

## stubgen's file for an unplaced function lacks the directory (2026-11-07)

Symptom: `tus.py check` stays red on one undefined reference although stubgen says "1 stubs (new
file)". Cause: for a function in an unassigned run whose inferred file in yamipan_functions.txt has no
directory (here `rena_2d_texture_tim2.cpp` for 0x2e1b10), stubgen writes `src/<file>` at the root of
src/, which no target builds, and the loop repeats. Fix: delete that file and put the stub in
src/lib/unassigned_stubs.cpp by hand (or pin the address to its real TU first). Also: `callcheck.py`
takes a path relative to tools/reverse (`../../src/...`), not the TU key. The same bare key also
reaches the queue: `tus.py next` claimed `rena_2d_texture_tim2.cpp` as its own one-function TU
(2026-11-07). Fix: `tus.py define <the real key> <ranges incl. the address>`, `tus.py claim` the real
key, and delete the bare key's entry (and any queue line) from data/tus.json.

## An inlined list walk can run the wrong way (2026-11-07)

Symptom: none on screen; found when the run after playermotion.cpp (0x217700..0x217a80, a second
StageList<StageObject*> instance) was written. Cause: findCarried 0x216310 and giveItem's stack search
were written with the list walk inlined from the head (next), but the ROM calls getTail 0x217700 and
walks +8 prev: of two carried objects with the same name the ROM finds the one carried last, the host
found the first. Fix: when a callee of a written function is a template helper (FUN_ in a callcheck
line), read the loop's step field (+4 next or +8 prev) before inlining it; stagelist.h now has getTail
and find.

## Two host fields for one EE byte: the pause menu never opened (2026-11-07)

Symptom: Start in a stage did nothing, with the pause menu code written. Cause: `wantsPauseMenu` read
`StageObject::m_canPause` (+0x3a1, named from the reader alone) while the ROM's writer,
`AlwaysCondition::onUpdate` 0x276468, stores the same EE offset of the character, which the host knows
as `CharacterAllows::standing` inside `StageCharacter::m_allows`. On the host they are two bytes; only
one was written. Fix: read `player->m_allows.standing`; `m_canPause` is padding again. When a field is
named from its reader only, scan the ROM for stores to that offset (a byte loop over the text segment
for `sb/sh/sw ..., 0x<off>(...)` takes a second) before trusting nobody writes it.

## tus.py define with no ranges empties the TU (2026-11-07)

Symptom: `tus.py define game/playermotion.cpp $ARGS --why ...` printed "0 functions": the file's 100+
pinned ranges were gone. Cause: `$ARGS` came from a file a failed redirect never wrote (a relative path
into the scratchpad), so define got no ranges, and define *replaces* the list (and the why) with what
it is given, without a check. Fix: `git checkout -- tools/reverse/data/tus.json` (it was clean), then
define again with the old ranges plus the new ones. To add a range to a big file, build the argument
list from tus.json in the same command (`&&`), check its word count, and only then define.
