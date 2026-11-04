# Hazards -- things that already cost time

Carried from ReiPan (FF3, `../ReiPan/.claude/skills/reipan-port/references/hazards.md`, 1,080 lines)
and RosePan (RoR, `../RosePan/.claude/skills/rosepan-port/references/hazards.md`, 1,040 lines): the
ones any PS2 port meets, in short form. Their files keep the full stories with addresses; read the
matching section there when a symptom here fits. Add this game's own as **symptom / cause / fix**,
dated, in the section they belong to. GhidraMCP traps live in ghidra.md.

## Decompiler (the most common time sink)

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

## Build and run

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
- **layoutcheck compiles every header alone**: a header relying on its .c's includes fails there.
  MSVC runs only in layoutcheck (syntax): MSVC-only errors show up there.
- **Exit 0x20474343** is an uncaught C++ exception; **3221225477** an access violation: `gdbrun.py`
  with the same pad for the backtrace (the game runs on its own named thread).
- **A run without the scripted pad** sits in the first menu: a trace in later code prints nothing.
- **A smoke run can write the user's ini**: runs pass their own settings.

## Platform layer (MioPan's, through ReiPan and RosePan)

- **`sceGsSyncV` hung at boot** (2026-11-03): `gameInit` waits two vblanks before anything starts the
  clock. Fix: the field clock runs from `Kernel::startMainThread`, as the console's does from power-on.
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
