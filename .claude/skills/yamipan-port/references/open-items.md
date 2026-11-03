# Open items -- priorities, decisions, cross-TU questions, done log

The user's priorities first ("Next up"), then decisions with their date and who made them, then open
questions. Resolve items here when they are settled (move them to "Done" with the commit).

## Next up (in order)

0. **The user (2026-11-02): "start reversing actual code and try to match names as you go, my #1
   priority is getting source code back."** Library name seeding stops here (batch 021 was the last
   seeding batch; the "Next batches" list under item 2 is dropped -- library and game names now come
   at the TU that needs them). The runs go to source reconstruction as fast as possible, in this
   order, one per run:
   ~~(a) **bootstrap 5, the TU map**~~ **done 2026-11-02**: 16 library ranges in
       `data/layout.json`, 27 pinned TUs in `data/tus.json` (rx, rxsndstr, SDPR2, the boot's game
       code), `export.py` / `status.py` regenerated (architecture.md "Address map", "Modules").
       `tus.json` "queue" is left empty for (c); its settings carry the data root, slice 10240 and
       an empty smoke pad.
   ~~(b) **carry `tus.py` over from RosePan now**~~ **done 2026-11-02**: `tools/reverse/tus.py`
       with a **source-only mode while there is no `CMakeLists.txt`** (item 5): `check` runs
       callcheck on the TU's file and layoutcheck's lint only (no build, stubgen or smoke run),
       `finish` skips the build, refuses `--status done`, records `"build": "not built"` in the
       session and says so in the commit. `list` / `next` / `check` / `brief --no-stage` tried on
       the pinned TUs. The routine now runs one TU per run. **Next: (c).**
   **(c) started 2026-11-02: TU 1 `game/gamesystem.cpp` written (all 17 functions, `verify`,
   batch 022); TU 2 `game/pad.cpp` written (all 14 + the slot thunk, `verify`, batch 023; `pad.h`
   complete, `padSys*` now return int). Bootstrap 6 must give the host libpad a DualShock 2 in
   pressure mode (terminal mode 8 is the only one `Pad::update` reads: architecture.md "The pad
   layer") and the SCE_PAD* button bits in `sdk/libpad.h` (RosePan's lacks them). TU 3
   `game/perfmeter.cpp` written (all 10, `verify`, batch 024; RcntTimer moved to `game/rcnttimer.h`).
   Bootstrap 6 must give `sdk/fileio.h` the SDK 3.0 `sceOpen(const char *, int, ...)` (FS2 passes a
   mode, 0666; RosePan's takes two arguments) and the CRT `sprintf` / `strlen`. TU 4, the frontier's
   `unassigned@00116fb8`, was **library, not game code** (batch 025, 51 names): newlib float libm
   0x116fb8..0x119be8 and libgcc (libgcc2 + fp-bit soft-float) ..0x11c6b0, now two `lib_ranges`
   (architecture.md "Address map"); the host libm / compiler replace them, nothing to write. The
   other unread runs the frontier may pick next (0x121b80,
   0x12c9b8..0x12cd20, the VU0 math 0x135a90..0x1366c0, 0x142dd0..0x144080) get the same first
   look: library -> name + `lib_ranges`, game code -> `tus.py define` a real file.
   TU 5: the frontier picked the 5,138-function run `unassigned@00171300` (all of 0x171300..0x2e26b4);
   its frontier callees from `gamesystem.cpp` are scattered, so the run cut one real file out of it,
   **`game/sysdata.cpp` 0x23af30..0x23c020** (all 37, `verify`, batch 026: SaveData / StageSelect /
   GameOptions / FlagTable in one `SystemData` block, architecture.md "Modules"). The same cut is
   left for the others the boot calls in that run: `gameSceneThread` 0x231bb0, the memcard group
   0x2a9c70..0x2acf00 (`memcardInit` / `memcardLibInit` / `memcardLibEnd` / `memcardGetState`),
   `stageDefaultRecord` 0x296d00, `StageResult::getPerfectBits` 0x2b00d0 / `getStageResult`
   0x2b0580. TU 6 (the frontier's `unassigned@0023c020`, the same run's tail): **`game/datetime.cpp`
   0x23de20..0x23e740** cut out (`getLocalDateTime` + `DateTime::setFormat`, `verify`, batch 027;
   `DateTime` moved from `sysdata.h` to `datetime.h` as status / format bitfields: the old comment
   "every field BCD" was wrong, both callers ask for binary) and **libscf 0x2640e8..0x2647d8** made a
   `lib_ranges` entry (19 functions; bootstrap 6 must give `sdk/libscf.h` and `sdk/libcdvd.h` from
   RosePan, which already have `sceScfGetLocalTimefromRTC` / `sceCdCLOCK`; the host
   `sceScfGetLocalTimefromRTC` can use the PC's local clock). 0x23c020..0x23de20 (the slice's other
   13, Vt_00316510 ctors at 0x23db00 / 0x23db50) is still unread. TU 7 (the frontier's
   `unassigned@002647e0`, 1,680 functions 0x2647e0..0x2e26b4): **`game/memcard.cpp`
   0x2a9c70..0x2acff0** cut out (55 functions, batches 028-030); **all 55 written, `verify`** (the
   second memcard run, 2026-11-02, wrote stepSave / stepLoad / stepCheckAll / stepScan /
   buildIconSys and explained the four implicit `operator=` 0x2ac170..0x2ac3f0; stepScan looks for
   **the first game's saves**, the bonus behind `SaveData::unlock468`: architecture.md "Modules").
   `sysdata.h` corrected: `FlagTable` is 256 ints (0x400), `SystemData` 0x920 (hazards.md "Types and
   layouts"). `gamehooks.h` lost the memcard prototypes (now `memcard.h`; main calls `memcardEnd`
   0x2a9c70, the thunk). Bootstrap 6 must give `sdk/libmc.h` from RosePan (has every sceMc call used,
   sceMcTblGetDir alignas(64) and sceMcIconSys) and a host libmc whose missing directory answers -4,
   whose `sceMcGetDir` takes a null table with maxent -1 and answers the entry count (stepScan),
   and `sdk/fileio.h` with SCE_RDONLY / SCE_WRONLY / SCE_CREAT (memcard opens with them).
   Still in the unassigned run for later picks: `stageDefaultRecord` 0x296d00, `getStageResult`
   0x2b0580 / `StageResult::getPerfectBits` 0x2b00d0, `gameSceneThread` 0x231bb0.
   TU 8 (the frontier's `unassigned@001037c0`): **game code, `game/heap.cpp` 0x1037c0..0x104b40**
   (all 17, `verify`, batch 031): the heap allocator, `initGlobalHeap` / `shutdownGlobalHeap` and
   the RNG; 0x104b40 / 0x104be0 are MSL `__destroy_new_array` / `__destroy_arr` (the MSL lib_range
   now starts there). `randUnit` moved from `gamehooks.h` to `heap.h`. Bootstrap 6 must give
   `sdk/eekernel.h` from RosePan (SemaParam, CreateSema / DeleteSema / WaitSema / SignalSema).
   Decision to overrule: the host heap keeps the EE size (~26 MB) with 0x20-byte headers; if host
   allocations (8-byte pointers inside structs sized with sizeof) run it out, raise `sHeapSize`.
   TU 9 (the frontier's `unassigned@00121b80`): **`sdk/mcerror.cpp` 0x121b80..0x121ef0**, the one
   function `mcErrorCode` (`verify`, no new names: kept the existing `mcErrorCode`, no Ghidra
   change); `McCmd` / `McErrno` in `sdk/mcerror.h`, memcard.h's `McFunc` / `McError` now built from
   them. Doubts for the user: read as libmc's (link position, it maps commands FS2 never issues), so
   the tree gets `src/sdk/mcerror.*` before bootstrap 6 brings RosePan's `src/sdk/`; its real SDK
   name is unknown; the `McCmd` sceMcFuncNo* values are from the SDK's numbering as remembered
   (0x20 unidentified). Bootstrap 6: move `McCmd` into `sdk/libmc.h`; the host libmc keeps calling
   this encoder unchanged (pure code).
   TU 11 (the frontier's `unassigned@002647e0` again, for `stageDefaultRecord` / `getSaveExtra`):
   **`game/progress.cpp` 0x2955e0..0x296c10 + 0x296d00..0x296d60** cut out (24 functions, all
   written, `verify`, batch 033): `NaviTable` (the navigator file 2dEU/navi%s.d2b: nodes, unlock
   conditions, triggers, pending links), `SaveData::hasArchive` (SaveData +4 is now
   `archive[101]`) and `stageDefaultRecord` (moved from `gamehooks.h` to `progress.h`;
   architecture.md "Modules"). Doubts for the user: the file name and putting
   `stageDefaultRecord` in it are guesses (no strings; it sits after the out-of-line vector
   inlines 0x296c10..0x296d00, which were left out of every TU); the names NaviTable / goals /
   pending links are read from the code, not from the game. `getSaveExtra` was **not** written:
   it belongs to the navi menu 0x29a700..~0x2a0000 (the user of 0x5995e0, with its init 0x29ef70
   and the gp-0x79f8 getter 0x29efe0), a large class file (vtables 0x3185b0.., 0x318820,
   0x318840) for a later run; it stays on the frontier. Still in the unassigned run:
   `getStageResult` 0x2b0580 / `StageResult::getPerfectBits` 0x2b00d0, `gameSceneThread` 0x231bb0.
   TU 12 (the frontier's `unassigned@002647e0` once more, its head): **`game/boxgrid.cpp`
   0x2647e0..0x2676e0** cut out (29 functions, batch 034, `verify`; architecture.md "Modules"):
   `BoxGrid` / `BoxSet` / `Box`, the 2D segment / quad tests and the MSL vector instances (mapped to
   the host `<vector>` in a comment list; no code). Checked: callcheck clean, layout lint OK,
   g++ -fsyntax-only clean with stub headers, and a scratch test (one rotated box, size 8, 20,000
   random points) agrees with the brute-force box test. Doubts for the user: the names (BoxGrid,
   BoxSet, the BoxList / BoxColumn / BoxCells instance names, insertIdentity0/1 and getAllocator
   for three identity helpers) are read from the code, not the game; two vertical edges count as
   crossing whenever their x match, whatever their y (kept as the ROM does it). Bootstrap 6 must
   give `sdk/libvu0.h` from RosePan (`sceVu0FVECTOR` / `sceVu0FMATRIX`, 16-aligned); game code
   includes it from here on. Next in the run: 0x2676e0.. (scene / movie threads: getStageSelect,
   2dEU/BGMOVIEEU.d2b, the stage-unlock checks at 0x268ae0 / 0x269160).
   TU 13 (the frontier's `unassigned@002acff0`, 688 functions 0x2acff0..0x2e26b4, for
   `getStageResult` / `StageResult::getPerfectBits` / `runStartupSequence`): **`game/stageresult.cpp`
   0x2afc20..0x2b0d50** cut out (32 functions, batch 036, all written or mapped to the host
   `<map>` / `<vector>`, `verify`; architecture.md "Modules"). `runStartupSequence` renamed
   **`loadGamePack`** (it parses packgameEU.pak's tables, no screens); `StageResult` moved from
   `gamehooks.h` to `stageresult.h` (fields now `clock.value` / `clock2.value` / `mode`;
   `sysdata.cpp` follows) and `SaveData::_000` is `playTime`. New stand-ins in `gamehooks.h`:
   `PackFile`, `PackTables` (+ `ListMap` / `NameMap` / `EntryList`), `UiLayout`, `StageSound`,
   `StageScene` / `gStageScene` (gp-0x7a7c: data the build must define), `getInitPackItem` /
   `getLangPackItem` with the `InitPackItem` / `LangPackItem` ids. Doubts for the user: the file
   name and joining 0x2afc20 / 0x2afcd0 to it are guesses (by position); counters [1..3], the
   attempt / stage blocks, clock state bits 0x14 and the `PackTables` element types are read from
   their users' shapes only; the `UiLayout`, `StageScene` and `PackTables` names are the port's.
   Left in the unassigned run's head 0x2acff0..0x2afc20: a drawing cluster around 0x2adc70
   (getStageSelect, packet lists, 0x2e1c50 / 0x2e1b70) and `getSaveResource` 0x2c3d70 /
   `getSaveResourceAux` 0x2c3d30 / `loadMenuPack` 0x2c3e10 further on (memcard / scene callers).
   TU 14 (the frontier's `unassigned@002676e0`, 613 functions 0x2676e0..0x2955e0, for the
   overlay / pack-item callees of scene.cpp and stageresult.cpp): **`game/screen.cpp`
   0x2676e0..0x2681e0** cut out (18 functions, batch 037, all written, `verify`; architecture.md
   "Modules"): the overlay thread (BG movie screen + loading indicator), the modal `runLayout` /
   `runLayoutFile`, the stage e / h screens, archive and option screens, game-over and notice
   dialogs. The overlay prototypes moved from `gamehooks.h` to `screen.h`; `gamehooks.h` gained
   `FileLoader` (PackFile's base), `getInitPackData`, the four layout-path getters, `UiLayout::
   setBaseDir` / `m_result` / `m_done` and language items 0..3; `gfxtarget.h` gained `TexArea` /
   `gfxGetTexArea`; `gamepacket.h` packet lists 10..12; `thread.h` slots 4 / 6 are now
   `ThreadSlot_Dialog` / `ThreadSlot_Overlay`. Checked: callcheck clean, layout lint OK, g++
   -fsyntax-only clean on screen.cpp / scene.cpp with stub SDK headers. Doubts for the user: the
   file name, the screen names (what the stage e / h screens show is unread: the letters are the
   file names'), "notice" for language item 1, TexArea for the 0x814-byte GS page allocator. Left
   in the same run for later picks: the LCG 0x2681e0..0x268310 (its own small file), 0x268310..
   (vtables 0x315ab0 / 0x316c30, the stage-unlock checks 0x268ae0 / 0x269160), and the pack-item
   cluster 0x2902d0..0x2908c8 (`getInitPackItem` / `getLangPackItem` / `loadInitPack` /
   `getInitPackData` / the stage screen paths: the frontier's top entries now).
   TU 15 (the frontier's `unassigned@0023e740`, 473 functions 0x23e740..0x2640e8, for
   `UiLayout::~UiLayout` / `setData`): **`game/uilayout.cpp` 0x243390..0x246230** cut out (22
   functions, batch 038 with 50 names, all written, `verify`; architecture.md "Modules"): the S2D
   layout player, its script interpreter (30 opcodes, 17 condition kinds as `S2dOp` /
   `S2dCondKind` enums) and its nested layout base `UiSubLayout`. `UiLayout` moved from
   `gamehooks.h` to the new `uilayout.h` (gamehooks.h includes it; `draw` now answers the sequence
   count); `gamehooks.h` gained `FileJob` (0x132a40..0x132cf0). Stand-ins in `uilayout.h` for the
   rest of the run: `S2dData`, `UiSequence` / `UiActor` / `UiAnim` / `UiTrack`, `UiWidget::create`,
   `UiDrawer`, the game hooks (`playSound` .. `testGameState`, `inStage`, `getStagePackData`).
   Checked: callcheck clean, layout lint OK, g++ -fsyntax-only clean (stub SDK headers; screen /
   stageresult / scene unchanged). Doubts for the user: all names (UiSubLayout, S2dData, the
   opcode / condition names) are read from the code; op 7 / 8 read as "yield" / "restart";
   condition kind 16 with a < 0 reads an uninitialised register in the ROM (the port tests
   nothing); `UiSubLayout::m_childDir` (+0x14) is never set in this file (open passes it to the
   child's setBaseDir); **host: S2dData's pointer fields are 32-bit offsets in the file**: the S2D
   TU must relocate into host-sized side tables (or keep offsets and resolve on access), the
   in-place relocate cannot widen them. Next in the run: the sequence classes 0x246230..0x249550
   (`UiSequence::draw` / `advance`, the frontier's next callees), then the S2D file and the drawer.
   TU 16 (the frontier's `unassigned@00246230`, 393 functions 0x246230..0x2640e8): **`game/
   uisequence.cpp` 0x246230..0x249550** cut out (29 functions, batches 039-040 with 33 names, all
   written, `verify`; architecture.md "Modules"): `UiSequence`'s actors / widgets and their merged
   priority walk (draw, advance), `UiActor` / `UiAnim` (play, frame, apply) and the `UiWidget` /
   `UiTrack` bases. The stand-ins left `uilayout.h` for three headers: **`game/s2ddata.h`** (the S2D
   file: objects, actor defs, anim set entries now {actor, priority, anim, play flags},
   `S2dWidgetDef` with host name / type pointers), **`game/uidrawer.h`** (`UiVec4`, `UiMatrix`,
   `UiDrawer` with its fields) and **`game/uisequence.h`**; `uilayout.h` includes them.
   `S2dAnimSetEntry_NoWait` is now `UiPlay_Stop` (the same bit stops tracks). Checked: callcheck
   clean, layout lint OK, g++ -fsyntax-only -Wall clean on uisequence / uilayout / scene /
   stageresult (stub SDK headers; screen / sysdata only fail on the stub libmc.h, as before).
   Doubts for the user: all names (the keyed kind groups A / B / C, BaseColor / LayoutColor, pixel
   mode, add pass, refresh / getStatus, getText) are read from the code; `UiDrawer::m_b4` and the
   `UiDrawState` byte that sets it are unread; with kind 0x1c the actor's own colour is multiplied
   in twice (kept as the ROM does it); the ROM hands a null def name to strcmp in
   findWidgetByName (the port skips it). Next in the run: **the track classes 0x249550..0x24d200**
   (`UiAnim::init`, the factory over 40 kinds, then each class's draw / apply / getText; the
   frontier's `UiTrack::isDone` / `setPlay` / `step` are at 0x24d200..0x24d400 right after), then
   the S2D file 0x24d640.. and the drawer 0x24e580.. (`UiDrawer::begin` / `end` / ctor are on the
   frontier).
   TU 17 (the frontier's `unassigned@00249550`): **`game/uitrack.cpp` 0x249550..0x24d530** cut out
   (109 functions, batches 041-043 with 146 names, **all written, `verify`**; architecture.md
   "Modules"): the track factory `UiAnim::init`, the 41 track classes (`game/uitrack.h`, records
   per kind), `UiAnim::findTrack`, `UiTrack::step` / `isDone` / `setPlay`, the inline virtuals.
   **UiTrack slot 3 is `getAnchor`, not `getText`** (the first vertex xy; renamed in Ghidra and in
   uisequence.h / .cpp). `UiTrackKind` in uisequence.h now names every kind; `uidrawer.h` gained
   the drawer state / vertex calls (`UiPrim`, `UiBlend`, `kGsRegTex1_1`), `UiMatrix::interpolate`
   and the strip meshes; `gfxtarget.h` the `GsTexture` stand-in; `s2ddata.h` `S2dImage`,
   `imageCount` / `images` (+0x48 / +0x4c) and `getImage` / `getImageEntry`. Checked: callcheck
   clean, layout lint OK, g++ -fsyntax-only -Wall clean on uitrack / uisequence (stub SDK headers).
   **Finished 2026-11-03 (second run, batch 043, 20 names; `verify`)**: the rotate / pin applies
   of kinds 0x15..0x1a (read from the asm), the sheet-sprite draws, the text draw and its five
   setters (`getText`, `setText`, `setTextAt`, `setMessage`, `setMessageByName`); the UiMatrix
   helpers (`multiply` / `angle` / `rotate` / `setScale` / `setScaleRotTrans`, `uiTransformPoint`),
   `UiDrawer::setImage` / `drawSheetFlat` / `drawSheetGouraud` / `drawText` in uidrawer.h;
   `TimSheet` / `TimSheetSprite`, `FMsgFile` (+ the inline `fMsgCheck` / `fMsgEntry`), `wstrlen` /
   `wstrcpy` as stand-ins in uitrack.h. Choices: the sheet records got full host structs, so
   `getAnchor` reads `verts[0].xy` (the old fixed +0x40 is gone; on the EE both are +0x40); the text
   track's +0x1c / +0x20 are floats (`m_x` / `m_y`, the drawText position); `kUiTextLiteral` 10000 is
   the message a string text gets. Doubts: what `S2dTextRec` +0x1c.. holds (drawText reads +0x10 and
   +0x28 of it: more record than the struct declares); `UiPinMode` is 0 vs anything else; the
   rotate kinds force `UiCoord_Pixel`. Doubts for the
   user: every class / kind name is read from the code; `setDrawPreset` / `setTargetRegs` /
   `setNoAlphaTest` say what they write, not what for (TU 19: now `copyTarget` / `setTexRepeat`,
   the latter a misread register); a record kind without a class (0x20, past
   0x2b) leaves its slot unwritten in the ROM (the port stores null, which UiAnim::isKind would
   then dereference, as the ROM would garbage); the text track keeps an image id **or the
   init-pack name pointer** in the same field (kept as `intptr_t`). Bootstrap 6: the host draw
   module must take the `UiPrim` / `UiBlend` / TEX1 / scissor state and the four vertex layouts of
   `uitrack.h`, and `GsTexture` decodes its image at the source (no GS upload).
   TU 18 (the frontier's `unassigned@0024d530`, 255 functions 0x24d530..0x2640e8): **`game/
   s2ddata.cpp` 0x24d530..0x24dfa0** cut out (all 9, batch 044: `findSequence` 0x24d5a0 /
   `findVarIndex` 0x24d6e0 named, `verify`; architecture.md "Modules"): the S2D file's lookups and
   `relocate`. **Host choice, for the user to overrule: every pointer relocate writes is an
   `S2dRef<T>`** (a 32-bit offset from the field itself), so the S2D file keeps its EE layout and
   relocates in place; `s2ddata.h` and the `uitrack.h` records lost their `layout: host` and are
   asserted. Fixed readings: S2dImage +4 is the image's **name** (was `id`): the textured tracks'
   `m_imageId` became `m_imageName`, and the text track's keys are names (`const char *`, no more
   `intptr_t`); the header's `_20` / `_28` / `_30` / `_38` are the actor def / anim set / widget def
   / sequence counts; actor defs and anim sets have a name at +4, sequence defs at +8; the textured
   records' +8 is a name (`label`, unread). Checked: callcheck clean, layout lint OK, g++
   -fsyntax-only -Wall clean on s2ddata / uitrack / uisequence / uilayout / scene / stageresult
   (RosePan's SDK headers + stubs for libvifpk / eestruct), and a host harness relocated **1,764
   S2D files** found in ROM.000..ROMD.010 with every pointer inside its file and every name
   readable. Doubt: the ROM never relocates kind 0x22's sprite name (+0x14; kind 6's it does): a
   0x22 record would hand findSprite an index; no file on the disc has one, so the port keeps the
   ROM's behaviour. Next in the run: **`UiMatrix` / `UiDrawer` 0x24dfa0..0x252840** (the
   frontier's top: `setPrim`, `setTexture`, `setPrimColor`, the matrix helpers; 58 functions,
   ~18 KB: two runs), then 0x252840.. (the widget classes, vtables 0x316a78..0x316c10).
   TU 19 (the frontier's `unassigned@0024dfa0`): **`game/uidrawer.cpp` 0x24dfa0..0x252840** pinned
   (50 functions, batches 045-046; architecture.md "Modules"); **42 written, `partial`**: the
   matrix helpers, the drawer's ctor / dtor / begin / end, matrix, add pass, every state call and
   the four vertex-list draws. **Next run (the same TU): `interpolate` 0x24dfa0, `drawText`
   0x24eba0 (453 insns), 0x2503c0, `setScissor` 0x250520, `drawSheetFlat` / `drawSheetGouraud`
   0x250750 / 0x2509b0, `uiDrawTexStripFlat` / `Gouraud` 0x251350 / 0x2519a0** (each a draw* shape
   with UV from the sprite rect or the uvRect: the helpers putXyz / putUv / vertexColor are there).
   Fixed readings: `setNoAlphaTest` is **`setTexRepeat`** (CLAMP_1, hazards.md), kind 0x27
   `UiTexTrianglesRepeatTrack`; `setTargetRegs` is **`copyTarget`** (a 512x512 GS local copy);
   `UiDrawer::m_b4` is **`m_fixedColor`**; the vertex structs moved from uitrack.h to uidrawer.h and
   the draw* take them typed. **Host choices, for the user to overrule:** (1) the drawer calls a new
   host module **`src/yamipan/rendering/yamipan_drawui.h`** (declared only; bootstrap 6 implements
   it) with the GS register values the ROM would pack, keyed by the packet list (= layer), textures
   by their GsTexture owner (no TexArea, upload or TEX0: the user's no-GS-memory rule); RosePan's
   draw2d module only draws sprites, so this is new host code. (2) **Any aspect ratio**: the game
   itself has 4:3 / 16:9 UI modes (setMode, matrices scale(k, 1) / scale(1/k, 1), k = 4/3 / 16/9;
   architecture.md); the plan is that the renderer hands k from the display aspect (the host frame
   stays the game's frame stretched across the window, as the PS2's anamorphic 16:9 is), replacing
   `kUiScreenMatrix` -- not done yet, decide at bootstrap 6. Doubts: whether block 0x800 or 0x1800
   is the frame in `copyTarget` (the parameter name `toBackup` is a guess); the preset tables
   0x3071a0 / 0x307200..0x3072c0 are unread (names give only the FRAME masks); `setImage` deletes
   its temporary GsTexture right after the bind, so the host must decode during the call. Checked:
   callcheck clean, layout lint OK, g++ -fsyntax-only -Wall -Wextra clean on uidrawer / uitrack /
   uisequence / uilayout / s2ddata / scene / stageresult (RosePan's SDK headers + a libvifpk stub),
   and a host harness over the drawer reproduced the ROM's vertex words, UV, RGBAQ bytes, add-pass
   PRIM (0x15e -> 0x14e) and the copy's BITBLTBUF / TRXREG values. Bootstrap 6: `sdk/libgraph.h`
   (SCE_GS_SET_ALPHA / CLAMP / TRXPOS / TRXREG / BITBLTBUF / TRXDIR, sceGsDrawEnv1 with scissor1 at
   +0x30) is RosePan's; implement yamipan_drawui.
   **TU 19 finished 2026-11-03 (second run, batch 047; all 50 written, `verify`)**: `interpolate`,
   `drawText`, `setScissor` / new **`setScissorPixels`** 0x2503c0, the sheet draws and the strip
   meshes, which are **UiDrawer members** with a **float width** first (`drawTexStripFlat` /
   `Gouraud`; were free functions read as (mode, drawer)): a ribbon of point pairs, not uv'd
   vertices, so `UiStripVertex` / `UiColorStripVertex` are gone (the records hold `UiVertex` /
   `UiColorVertex`, +0x14 `width`). New `game/fontdata.h` (`FontData` glyph table: `nextGlyph` /
   `countLines` / `lineWidth` 0x1fe4d0 / 0x1fe150 / 0x1fe2b0, game-inferred; its TU 0x1fe020.. is
   not cut yet: stubgen stubs them). `UiFont::m_scale` -> **`m_spacing`** (pixels off each advance
   / line), `S2dTextRec` +0x1c is a **`UiTextBox`** {rgba, topLeft, _18[4], bottomRight}; the text
   track's `m_x` / `m_y` are the **alignment** 0..1 in that box, not a position. Doubts: the box's
   +0x18..+0x28 (two more corners?) is unread; a mirrored and rotated keyframe does not round-trip
   through `interpolate` at t = 1 (the ROM negates only x before atan2: kept); the glyph text is not
   scaled by the UI matrix (only the box is), so on a 4:3 mode the text keeps its texel size: the
   any-aspect renderer must keep that. Checked: callcheck clean, layout lint OK, g++ -fsyntax-only
   -Wall -Wextra clean on uidrawer.cpp, and a host harness: interpolate t = 0 / 1 return from / to,
   drawText's line / alignment / spacing maths, colour bytes, the ribbon offsets and the scissor
   word by hand. Next: the widget classes 0x252840.. (vtables 0x316a78..0x316c10), or what the
   queue pins.
   TU 20 (the frontier's `unassigned@00171300`, 2,851 functions 0x171300..0x231bb0, for
   `GsTexture` / the presets / FontData / boxgrid's 0x193780 callees): **`game/gsutil.cpp`
   0x230980..0x231410** cut out (12 functions, batch 048, all written, `verify`; architecture.md
   "Modules"): the seven `gfxUiPreset*` draw presets and `GsTexture` with its render-target tags
   `Zbuf` / `Draw` / `View` (new `readTarget` 0x231290). New `game/gsutil.h` holds them and rena's
   base **`Tim2Texture`** (stand-in, `layout: host`; 0x2e1c50 renamed `GsTexture::upload` ->
   `Tim2Texture::upload`); `gfxtarget.h` lost its GsTexture stand-in, `gamehooks.h` the preset
   prototypes; `uitrack.h` / `uidrawer.cpp` include gsutil.h. Checked: callcheck clean, layout lint
   OK, g++ -fsyntax-only -Wall -Wextra clean on gsutil / uidrawer / uitrack / uisequence / uilayout
   (RosePan's SDK headers + a libvifpk stub), and a host harness: every preset's register words
   equal the ROM tables 0x3071a0..0x3072f0 with FRAME / ZBUF masked as the ROM does, the three
   target TEX0 (0x664021800 / 0x664020800 / 0x664220000) and putTex0's six words. Doubts for the
   user: the file name and joining presets and GsTexture (no string, by position); "View" being
   PSMCT16 at block 0 (the display buffer?) is read from the TEX0, not checked against
   gfxInitRenderTargets; RgbOnly3 / 4 test the frame's alpha bit (DATE) -- what the layouts use
   them for is unread. **Bootstrap 6**: the host draw module must handle a GsTexture with
   `m_isTarget` by sampling its own copy of that render target (TBP0 0x1800 / 0x800 / 0), not by
   decoding an image; the `Tim2Texture` TU (rena, 0x2e1b70..0x2e2970) is where the TIM2 decode at
   the source belongs. Still in the run for later picks: `FontData` 0x1fe020..0x1fe5b0 (frontier),
   boxgrid's callees 0x193780..0x1938f8, `StageSound::playNamed` 0x1d6a80, the class 0x230820..
   0x230980 (vtables 0x3164b0 / 0x3164d0) and the pak parser 0x231410..0x231bb0.
   TU 21 (the frontier's `unassigned@00252840`, 196 functions 0x252840..0x2640e8, for
   `UiLayout::stopSound` / `fMsgRelocate` / `timSheetRelocate` / `findSprite`): **`game/uigame.cpp`
   0x25fa50..0x2640e8** cut out (40 functions, batch 049 with 10 names; architecture.md
   "Modules"); **14 written, `partial`**: UiFont, fMsg find / relocate, TimM find / relocate,
   playSound / stopSound, the three game-value hooks, both bindTarget, inStage. New
   `game/uigame.h`: UiFont / FMsgFile / TimSheet moved out of uitrack.h, now **EE layouts with
   S2dRef** (asserted; TimSheetSprite is 0x10 with a name index, TimSheetImage {name, TIM2}),
   `getGameValue` / `setGameValue`, `GameValue_StageFlags` 13, inStage / getStagePackData (moved
   from uilayout.h, which includes uigame.h). `gamehooks.h` gained `StagePlayer` (gStageScene
   +0x78), `StageSound::playNamedAlways` / `stopNamed`, `normalizeAngle`, `SeTable` (inline name
   search) / `SePlayer` / `getSePlayer`. Checked: callcheck clean, layout lint OK, g++
   -fsyntax-only -Wall -Wextra clean on uigame and its users, and a host harness relocated all 241
   TimM and 2,620 fMsg files on the disc (ROM.000..ROMD.010) with every pointer inside its file and
   every name found again. Doubts for the user: the file name and joining UiFont / fMsg / TimM to
   the hooks (by position); "player" for gStageScene +0x78 (the target's spelling
   "rCharactorPostion" says character; earlier comments said camera); the default yaw 0x3fc90e56 is
   a little under pi / 2 (kept); readGameValue with argument 0 not a variable stores through null
   in the ROM (the port drops it). **Next run (the same TU)**: `findSprite` 0x261d00 with the
   inventory scans 0x2628a0 / 0x262a90 (item tables 0x2faed0 / 0x2fafc0, item defs 0x574c10, the
   player's items +0x22c / +0x238 / +0x23c: name the item structs first), then the text variables
   0x260bc0 / 0x2612c0, `testGameState` 0x2604a0, `getGameValue` / `setGameValue`,
   `getStagePackData` and 0x2625c0..0x264070. Left in `unassigned@00252840` (156): the widget
   classes 0x25b8e0..0x25fa50 (`UiWidget::create`) and the MSL vector / tree user 0x252840..0x25b8e0.
   **Second run (2026-11-03, batch 050, 13 names): 21 of 40 written, still `partial`**:
   `testGameState` (41 named states: architecture.md "Modules"), `findSprite`, `scanItems` /
   `scanWeapons`, `getDoorState`, `getStagePackData`, `getButtonMapDraft`. GameOptions +0x20 / +0x44
   / +0x48 named wideScreen / mapMode (MapMode) / hints. Stand-ins: `StageObject` (getKind is a
   virtual, slot 123, declared plain), `StageObjectList::find` (since TU 26 `find1`), `StageScene::getFloor`,
   `StageFiles` / `getStageFiles`, `ItemDef` / `gItemDefs` (gamehooks.h), `getNaviTable`
   (progress.h), `UiScrollWidget` (uisequence.h). Doubts: rF* read as the player's floor; the
   scroll widget's class (only its position after Vt_00316ab0's methods); `gItemDefs` (bss
   0x574c10) has no definition yet -- whoever loads it (stage data) defines it, else stubgen must;
   PC guards where the ROM reads near null (no item def, no navi table, a sheet without weapons).
   **Next run (the same TU)**: the text variables 0x260bc0 / 0x2612c0 (with 0x261cf0, table 0x2fb120
   rItem00_C / rItem00_I), `getGameValue` 0x2633f0 / `setGameValue` 0x262e70, 0x262840 (steps the
   weapon preview), 0x263b90 (empty, called by 0x287f20) and 0x2625c0..0x264070.
   **Third run (2026-11-03, batch 051, 7 names): 30 of 40 written, still `partial`**: `setGameValue`
   / `getGameValue` (all 40 kinds from the jump tables: `GameValueKind`, `GameValueClock`,
   `StageClockState`; architecture.md "Modules"), `getPlayerMapPos` / `mapShowsPlayer` /
   `getPlayerHeading` (the out-of-line copies: the static inlines are gone, the binds and
   testGameState call them), `stepWeaponPreview`, `isHighestWinsStage`, `applyButtonMapDraft` /
   `loadButtonMapDraft`. Stand-ins in gamehooks.h: `PlayerMotion` (player +0x224, `refresh`
   0x1e66c0), `NaviMenu` / `getNaviMenu` 0x29efe0 (array extents guessed). Doubts: the clock
   state names say what the game values do, not what the scripts mean by them; kind 25's
   3 / 0x11 / 0xff are unread; the unnamed option bytes keep their offsets as names
   (GameValue_Option08 ..); SystemData may derive from FlagTable (the null-checked adjust), not
   restructured. PC: an amount index past its table arms 0 (the ROM reads the next table). Checked:
   callcheck clean, layout lint OK, g++ -fsyntax-only -Wall -Wextra clean on uigame and its users;
   no harness for the game values. **Next run (the same TU)**: the text variables 0x260bc0 /
   0x2612c0 (0x2612c0 is 0xa30 bytes: scanItems / scanWeapons, "%d/%d", "999/999", the clock
   "%02d:%02d:%02d", table 0x2fb120), then decide the tail 0x263b90..0x264070 (the class with a
   0x3c-byte member from 0x151600, called only from stage code: likely its own TU -- re-pin
   game/uigame.cpp to end at 0x263b90 or 0x263ba0 and give the rest a file).
   **Fourth run (2026-11-03, batch 052, 4 names): finished, all 32 written, `verify`**: re-pinned
   to **0x25fa50..0x263b90** (every caller of 0x263b90..0x2640e8 passes gStageScene +0xf4: that is
   one class, now the TU **`game/stagealert.cpp`**, 8 functions, unwritten); written
   `UiWidget::expandTextVariable` 0x260bc0 (table 0x2fb120 as `kUiItemTexts`) and
   `formatTextValue` 0x2612c0 (28 names, `UiTextValue`; architecture.md "Modules"). Stand-ins:
   `getPurposeText` 0x195d50 and `gMenuMemcard` (sbss 0x31ae94, data the build must define) in
   gamehooks.h, `NaviMenu::stage()` (values[113], +0x46c: the values extent is a guess),
   `wstrFromAscii` 0x2d4440 in uitrack.h (since TU 22 in `game/wstring.h`). Doubts: L / S in rWeaponL00 / rWeaponS00 read as the large
   / small layouts (both slot 8); "_C" read as caption; the table's +0x10 column is unread. Kept from
   the ROM: the navi sample "12:34:56" is not widened (16-bit garbage on screen when there is no navi
   menu). PC: no weapon message in the preview file answers nothing (the ROM divides by zero).
   Checked: callcheck clean, layout lint OK, g++ -fsyntax-only -Wall -Wextra -Wformat=2 clean on
   uigame / uitrack / uisequence / uilayout / memcard (RosePan's SDK headers + a libvifpk stub); no
   harness. Next in this region: `game/stagealert.cpp`, the widget classes 0x25b8e0..0x25fa50 (the
   text widget 0x25cab0 calls expandTextVariable) and the MSL vector / tree user 0x252840..0x25b8e0.
   TU 22 (the frontier's `unassigned@002b0d50`, 628 functions 0x2b0d50..0x2e1b70, for
   `wstrFromAscii` x17 / `wstrcpy` / `wstrlen`): **`game/wstring.cpp` 0x2d4150..0x2d46a0** cut out
   (5 functions, all written, `verify`, batch 053: `wstrcmp` 0x2d4150 / `wstrstr` 0x2d42a0 named;
   architecture.md "Modules"). New `game/wstring.h`; the three prototypes left `uitrack.h`
   (uitrack.cpp / uigame.cpp include wstring.h). The old comment "null for a null or odd dst" was
   half the story: the copies answer the end of the copy (no caller uses it yet). Checked: callcheck
   clean, layout lint OK, g++ -fsyntax-only -Wall -Wextra clean on wstring / uitrack / uigame
   (RosePan's SDK headers + a libvifpk stub), and a host harness (prefix compare, in-place widen,
   aliasing copy, null / odd pointers) passed. Doubt: the file name (no strings). **Next picks in the
   run** (the brief's other frontier callees): `getSaveResource` 0x2c3d70 / `getSaveResourceAux`
   0x2c3d30 / `loadMenuPack` 0x2c3e10 / `getSePlayer` 0x2c4140 (memcard / scene / uigame call them;
   0x2c3d30..0x2c4148 looks like one resource-loader group), `PackTables::PackTables` 0x2c0dc0
   (stageresult), and the SoundStream user 0x2b0d50..0x2b3890 (classes on vtables 0x311860..0x319260,
   called from unassigned@00171300).
   TU 23 (the frontier's `unassigned@002681e0`, 595 functions 0x2681e0..0x2955e0, for
   `getLangPackItem` / `getInitPackItem` / `getInitPackData` / the stage screen paths /
   `loadInitPack`): **`game/gamepaths.cpp` 0x28ff60..0x2908d0** cut out (14 functions, all written,
   `verify`, batches 054-055 with 25 names: the 8 unnamed path / pack functions, the 13 event movie
   path functions 0x2ec160..0x2ec520 and the PackFile slot thunks 0x131d00..0x131d30;
   architecture.md "Modules"). New `game/gamepaths.h` takes the
   pack / path prototypes from `gamehooks.h` (which includes it); `gamehooks.h`'s `PackFile` gained
   its slots 5..8 (`getData` / `getItem` / `getSize` / `getItemSize`, thunks 0x131d30..0x131d00) and
   the `eventMoviePath*` prototypes; `sysdata.h` gained `StageKind`. Checked: callcheck clean, layout
   lint OK, g++ -fsyntax-only -Wall -Wextra clean on gamepaths / screen / stageresult / uitrack /
   scene / uigame (RosePan's SDK headers + a libvifpk stub); no harness. Doubts for the user: the
   file name and its boundaries (sound-table code on both sides may make it one larger file);
   `StageKind` Easy / Normal / Hard is read from the e / n / h letters; "branch" for the event
   table's third field and for `StageResult::attempt[15]`. **Next picks in the run**: the LCG 0x2681e0..0x268310
   (its own small file; callers 0x195d40 / 0x199f00 / 0x19c180), `SePlayer::play` / `stop`
   0x292ac0 / 0x292db0 with the SoundStream / SeBank wrappers 0x2908d0..0x2931b0, `getNaviTable`
   0x2955d0, and 0x268310.. (vtables 0x315ab0 / 0x316c30, the stage-unlock checks 0x268ae0 /
   0x269160).
   TU 24 (the frontier's `unassigned@00171300`, for `FontData` / boxgrid's callees / StageSound):
   **`game/fontdata.cpp` 0x1fcb40..0x1fe6b0** cut out (31 functions, all written, `verify`, batch
   056 with 31 names incl. newlib `vsprintf` 0x12cab8 / `strncmp` 0x1286a8 and `gfxGetVramBlock`
   0x12f170; architecture.md "Modules"): `FontData` (the "knj" glyph table, wide and byte lookups),
   the printer base `TextPrinter` and `FontPrinter` (printf text as one GS sprite per glyph), the
   never-constructed vtable 0x315fc0 (`SpritePrinter`). `fontdata.h` grew from the stand-in
   (`m_magic`, `m_cellWidth`); `gfxtarget.h` gained `GfxVramArea` / `gfxGetVramBlock`. Host choices,
   for the user to overrule: the sprites go to `yamipan_drawui.h` (Prim + Begin(UV) + two vertices
   per glyph, keyed by the printer's packet; the texture bound by its Tim2Texture owner in
   setDrawState, uploadTexture only flushes); print / printScaled share a private `drawText`
   (print = scale 1, exact); SpritePrinter's glyph fetch answers null (its ROM call lands on its own
   printScaled). Kept from the ROM: SpritePrinter's second corner x + (x + w); measureLines zeroes
   before its null test (the port guards it); TEX1_1 shifts the whole bilinear byte. Checked:
   callcheck clean, layout lint OK, g++ -fsyntax-only -Wall -Wextra clean on fontdata / uidrawer /
   gsutil (RosePan's SDK headers + a libvifpk stub); no harness (no "knj" file located yet). Doubts:
   the file name; the printer / slot names (query2..6 say nothing); FontData +4 unread. **Next picks
   in the run**: the wide printer 0x2d3700..0x2d4150 (in unassigned@002b0d50, FontPrinter's
   subclass, made by 0x2b1010), boxgrid's callees 0x193780..0x1938f8, `StageSound::playNamed`
   0x1d6a80, the class 0x230820..0x230980 and the pak parser 0x231410..0x231bb0.
   TU 25 (the frontier's `game/gfxtarget.cpp`, pinned since TU 1): **re-pinned to 0x12dfa0..0x12f190**
   (the display records and gp-0x7ad8..-0x7ac8 are used only there; `RcntTimer` starts at 0x12f190)
   and **all 15 written, `verify`** (batch 057, 13 names; architecture.md "Modules"): the two display
   records (`GfxDisplay` 0x540: 0 = 512x512 interlaced with the DISPFB1 / DISPLAY1 flicker filter, 1 =
   256x256 progressive), the flip, the draw env clear, the display offset, the copy packet builder
   `gfxBuildCopyPacket` and the GS allocators' set-up. The old header comment "256x256 and 512x512
   offscreen draw envs" was wrong: they are display modes. `gfxtarget.h` now defines `GfxDisplay`,
   `GfxDrawEnv` (+ the clear), `GfxCopyPacket`, `GfxDisplayIndex`, and stand-ins `GsBlockMap` (TexArea
   derives from it), `GsRegionAlloc`; `gamehooks.h` gained `drawSysInit` 0x2d7cf0. Checked: callcheck
   clean, layout lint OK, g++ -fsyntax-only -Wall -Wextra clean on gfxtarget and its users gsutil /
   uidrawer / scene / fontdata / uigame / screen / sysdata (RosePan's SDK headers + stubs); no harness.
   Doubts for the user: what 0x2c4320 (the 256x256 user) is; why the 256 record's copy reads 32 lines
   fewer; `drawSysInit` is named only from its arguments; GsBlockMap slots 1 / 2 unread; on the host
   GsBlockMap's dtor is virtual (EE: not in the vtable) so delete is defined. **Bootstrap 6** must give
   `sdk/libgraph.h`: `sceGsTexEnv` (0x40: texflush, tex1, tex0, clamp with addrs), `sceGsSetDefDispEnv`
   / `sceGsSetDefDrawEnv` / `sceGsSetDefClear` (11 args) / **`sceGsSetDefTexEnv` (12 args: ..., tfx,
   cbp, cpsm, cld, filter: the ROM passes 1 last)**, `SCE_GS_ZNOUSE` / `ZALWAYS` / `ZGEQUAL` /
   `ZGREATER` (0..3); `sdk/libdma.h` `sceDmaSendN`; `sdk/eekernel.h` `WRITEBACK_DCACHE`; and **fix
   RosePan's swapped `SCE_GS_INTERLACE` 0 / `SCE_GS_NOINTERLACE` 1** (the SDK has NOINTERLACE 0,
   INTERLACE 1; FS2 passes 1 for the 512x512 field-mode record). Host: GS register writes go to
   eeregs' variables and the copy packet to the GIF channel, which drops it; the host renderer
   presents (any aspect: it ignores DISPLAY DX / DY; offsetX / offsetY are there for a nudge).
   **Next picks**: the GS allocators' TU 0x142dd0..0x144080 (`TexArea`, `GsBlockMap`, TexAreaList:
   many UI callers), the region allocator 0x2e1440..0x2e1b70, `drawSysInit` 0x2d7cf0.
   TU 26 (the frontier's `unassigned@00171300`, for boxgrid's callees 0x193780..0x1938f8 and
   `StageObjectList::find`): boxgrid's callees are **MSL vector instances** (vector<BoxColumn> /
   vector<BoxList> dtor / clear, out of line beside 0x1928f0..0x193770: the host `<vector>` covers
   them; left unassigned, they belong to whichever TU 0x1918d0.. is). Cut out instead: **`game/
   stageobjlist.cpp` 0x193900..0x1953f0** (45 functions, batch 058 with 45 names, all written,
   `verify`; architecture.md "Modules"): the stage's object registry, nine groups of a name map
   (the game's own strcmp tree, `StageNameMap` / `StageNameNode`, rebalanced once after loading)
   and a list (`StageObjArray`). `StageObjectList` moved from `gamehooks.h` to the new
   `stageobjlist.h` (gamehooks.h includes it); **`find` renamed `find1`** (group 1; uigame.cpp
   follows); `StageObject` gained `m_id` (+0x208) and a plain `destroy()` for the deleting dtor at
   vtable +0x10. Checked: callcheck clean, layout lint OK, g++ -fsyntax-only -Wall -Wextra clean
   on stageobjlist (uigame.cpp only fails on gfxtarget.h's `sceGsTexEnv`, missing from RosePan's
   libgraph.h: not this TU's), and a host harness (600 objects over 9 groups vs brute force:
   indexed lookups, ids, removals keeping order, the tree ordered and its count right after
   rebalance, teardown freeing every heap block) passed. Doubts for the user: the file name and
   every container / group name are read from the code (no strings); groups 0, 1, 3, 5..8 keep
   numbers until their objects' classes are read; the ROM's balance rotates on a height
   difference of one as well (kept; still yields a valid tree). **Next picks in the run**:
   `StageSound::playNamed` 0x1d6a80 / `stopNamed` 0x1d6a90, `StageScene::getFloor` 0x19aea0, the
   stage scene itself (ctor 0x19c180 / teardown 0x19bbe0 / load 0x17fb90: who owns +0x10), the
   class 0x230820..0x230980 and the pak parser 0x231410..0x231bb0.
   TU 27 (the frontier's `unassigned@001319f0`, the whole run between thread.cpp and perfmeter.cpp):
   **`game/fileloader.cpp` 0x1319f0..0x132f20** pinned (53 functions, batch 059 with 45 names, **all
   written, `verify`**; architecture.md "Modules"): `FileJob`, `FileLoader` (18 slots, all named),
   `PackFile` = FileLoader + a **`PackJob`** base at +0x9c (the old stand-in's `load(int, int)` is
   `load(int arg, bool showLoading)`), `TextTokenizer`. The three classes left `gamehooks.h` for the new
   `game/fileloader.h` (gamehooks.h includes it); `FileJob::m_arg` is **`m_index`** (the loader's entry
   index); `FileJobState` gained 0x82 / 0x83. New stand-ins: `game/pakarchive.h` (`PakArchive`,
   `PakHeader`: the parser TU 0x231410..0x231bb0 completes it), `rxFileCallSizeHook` /
   `rxFileCallLoadHook` (0x152740 / 0x152730, RosePan's signatures) and `sldiDecompress` 0x2d0a10 in
   gamehooks.h. Checked: callcheck clean, layout lint OK, g++ -fsyntax-only -Wall -Wextra clean on
   fileloader / gamepaths / stageresult / uilayout (screen / scene / uigame only fail on gfxtarget.h's
   `sceGsTexEnv`, as before), and a host harness (loader list, prefix, duplicate add, empty / missing
   files, unload, a dir-prefixed job, end-of-buffer placement and its no-memory case, a pack read into
   its buffer's end and unpacked, the tokenizer's word cut) passed. Doubts for the user: the file name
   (no strings) and keeping TextTokenizer in it (by position); `isPack` / `compact` / the
   PackJob fields +0x34 / +0x44 / +0x48 (the constructor's argument, unread) are names from the code;
   a pack item of size -1 (missing) answers "done" from getState (kept); TextTokenizer reads MSL's
   ctype table with a signed char in the ROM (the port tests the byte as unsigned). Bootstrap 6: the
   host rx file hooks answer 0 for an empty file and < 0 for a missing one, and jobAdd must stay
   asynchronous (hazards.md "Stubs and bookkeeping"). **Next picks**: the pak parser 0x231410..0x231bb0
   (`PakArchive`: now on the frontier from three TUs), `sldiDecompress` with the wide printer in
   unassigned@002b0d50, the loader's users 0x2b5dc0 / 0x2b60b0 (the stage data loads with setBuffer).
   TU 28 (the frontier's `unassigned@00231410`, between gsutil.cpp and scene.cpp):
   **`game/pakarchive.cpp` 0x231410..0x231bb0** pinned (17 functions, batch 060 with 7 names, **all
   written, `verify`**; architecture.md "Modules"): `PakArchive` (the stand-in completed) and the
   file's own `PakHeader` / `PakEntry` / `PakId`; the in-place pointers are `S2dRef`s (reused from
   game/s2ddata.h; a shared file-ref header would be cleaner: the user's call). Checked: callcheck
   clean, layout lint OK, g++ -Wall -Wextra clean on pakarchive / fileloader, and a host harness
   (plain pak, variant pack with mask 0 / dropped / packed items and the kept size, bad magic, a
   sub-pak found by name) passed. Kept as the ROM, for the user to overrule: `open(data, -1)`
   answers false for a good pak (relocate answers the variant); ids with a non-zero high half
   always answer null and a sub-pak hit from findIndex is its own index at level 0, so an item of
   a sub-pak is reachable by name only; the variant pack copies over mask-0 items placed after
   the pack point (memmove on the host). Host guards (null pak data in getItemSize / getSize /
   getItem / getData / getName, a dropped sub-pak in relocate) stand for the ROM's reads of low EE
   memory. The file name is the port's (no strings). **Next picks**: `sldiDecompress` with the wide
   printer in unassigned@002b0d50, the loader's users 0x2b5dc0 / 0x2b60b0, and the other PakArchive
   users 0x1a0ae0 / 0x1a0e70 / 0x1a1df0 (unassigned@001953f0).
   TU 30 (the frontier's inferred `rena_pkt_vif1pktmanage.cpp`, after TU 29 `game/gamepacket.cpp`):
   **`rena/rena_pkt_vif1pktmanage.cpp` 0x2eaff0..0x2ec110** pinned under a new `src/rena/` (the
   game's own file name from its strings; the stale inferred key dropped from tus.json), 17
   functions, batch 061 with 4 names (the file's MSL vector instances), **all 13 written + the 4
   instances mapped to the host `<vector>`, `verify`**; architecture.md "Modules" (rena
   `Vif1PktManage`). `Vif1PktManage` moved out of gamepacket.h (which includes the new header);
   `AddBuffer` / `AddList` now return the ROM's int. New: `src/rx/rxsys.h` (rxSysPrintf only) and
   `YamiPan_DrawUiKick` declared in yamipan_drawui.h (see "Decisions" 2026-11-03). Checked: callcheck
   clean, layout lint OK, g++ -Wall -Wextra clean on the TU and gamepacket.cpp, both message strings
   byte-equal to the ROM, and a host harness (libvifpk Reset / Next / CloseDmaTag mirrored from the
   asm; 3 frames over 2 buffers, a list opened twice in a frame) walked the chain: start tag, list 0,
   list 2's two segments in order, list 3, end tag; Kick handed over the 3 drawn lists. Kept as the
   ROM: `Open` of an id never added writes through a null list. **Next picks**: the rena
   `Tim2Texture` TU 0x2e1b70..0x2e2970 (the texture decode the draw module needs), bootstrap 6's
   host libvifpk (`sceVif1PkReset` / `Next` / `CloseDmaTag` as the harness has them).
   TU 31 (the frontier's `unassigned@00142dd0`): **`game/gsblockmap.cpp` 0x142dd0..0x144080**
   pinned (the whole run between the SIF library and sound.cpp; 14 functions, batch 062 with 8
   names, **all written, `verify`**; architecture.md "Modules"): `GsBlockMap` (alloc / free /
   findFree / setRange), `TexArea` (its three slots, the inline list clip) and `TexAreaList`
   (place / insert). The stand-ins left `gfxtarget.h` for the new `game/gsblockmap.h`
   (gfxtarget.h includes it); `getList` answers `TexAreaList *` and `UiDrawer::begin` /
   `m_texList` / `UiLayout::draw` / screen.cpp take that type (were `void *`); `gsutil.h`'s
   Tim2Texture stand-in gained `getVramBlocks` 0x2e1c30 and `m_image` (+0x40). Checked: callcheck
   clean, layout lint OK, g++ -fsyntax-only -Wall -Wextra clean on gsblockmap / uidrawer /
   uilayout / screen (gfxtarget.cpp only fails on the SDK gaps listed for bootstrap 6), and a host
   harness (gfxInitRenderTargets' marks, the TexArea's free range exactly 0x3700..0x3fbe, place
   new / resident / wrap / eviction / no fit, top allocations lowering and freeing raising the
   lists' end, in-word and page findFree both ways) passed. Kept as the ROM, for the user to
   overrule: setRange's head / tail bit counts (block & 0x7ff) >> 6 (right for every range the
   game marks; wrong in general); the in-word search may answer a run cut short at bit 31;
   `TexArea::alloc` ignores its fromTop / pageAligned; place answers -0x20 for no room. Doubts:
   the file / class names (no strings); `Tim2Texture` +0x30 (read as the CLUT's blocks) is unread.
   **Next picks**: the region allocator 0x2e1440..0x2e1b70 (`GsRegionAlloc`), `drawSysInit`
   0x2d7cf0, the rena `Tim2Texture` TU 0x2e1b70..0x2e2970.
   TU 34 (the frontier's `unassigned@001953f0`, 1,354 functions 0x1953f0..0x1fcb40, whose slice was
   mostly the movie player): **`game/movieplayer.cpp` 0x1faa30..0x1fcb40** cut out of its tail (41
   functions, batches 067-068 with 56 names: the player's helpers and libmpeg callbacks, `MovieRing` /
   `MovieAudioRing`, the base `StreamRing` / `FileStreamRing` 0x151f90..0x153110 (their TU
   `unassigned@00148db0` not cut), libvifpk `sceVif1PkRefLoadImage` 0x10b380, libmpeg
   `sceMpegDemuxPssRing` 0x116190; 4 class names; all 41 written, `verify`; architecture.md
   "Modules"). New `game/movieplayer.h` (MoviePlayer at its EE offsets, `MovieState` with all nine
   bits); `gamehooks.h` lost MoviePlayer and gained `StreamRing` / `FileStreamRing` and the sound
   layer the player uses (`SoundSink` / `SoundPcmSink` / `SoundAdpcmSink`, `SoundFeeder` +
   `SoundFeederState`); uiwidget's `kMovieBuffers` is `kMovieAudioChannels` (the third ctor argument
   is the audio stream count). Checked: callcheck clean, layout lint OK, g++ -fsyntax-only -Wall
   -Wextra clean (RosePan's SDK headers + scratch stubs). PC (marked): the reader starts null (the ROM
   leaves it uninitialised until open); feedIpu limits each IPU block to what the host decoder has
   parsed (`YamiPanMpegVideoPending`, RosePan's rxMpegNodataCallback pacing); the Stop / RestartDMA
   callbacks do nothing; `uploadFrame` hands the decoded macroblocks to the new host call
   `YamiPan_DrawUiMovie` (yamipan_drawui.h: the player's own texture drawn where the transfer would
   land, decoded at the source, no GS memory) instead of the per-macroblock image transfers.
   **Bootstrap 6 must give**: RosePan's `sdk/libmpeg.h` + `sceMpegDemuxPssRing(mp, buff, size,
   ringStart, ringSize)` (RosePan's host libmpeg has only `sceMpegDemuxPss`; the ring variant wraps
   at ringStart + ringSize), `sdk/libmpeg_video.h` with `RosePanMpegVideoPending` renamed
   `YamiPanMpegVideoPending`, RosePan's `sdk/eeregs.h` (REG_DMAC_4_IPU_TO_*, write-only), and the
   draw module's `YamiPan_DrawUiMovie`. Kept as the ROM, for the user to overrule: open never deletes
   an earlier file ring (a reopen leaks it), `close` does not clear the pointer. Doubts: what the
   0x4000 cut off an audio ring's end means (`kMovieAudioTail`, read as the SPU stream's tail); who
   sets the data ring (+0x64 / +0x68) and `MovieState_Abort` (0x8000), none here; `SoundSink::open`'s
   four middle arguments (0, 4, 0, 4) are passed to `rxSndPcmOpen` / `rxSndAdpcmOpen` unread.
   **Next in the run**: the rest of `unassigned@001953f0` (0x1953f0..0x1faa30: StageSound, StageScene,
   PlayerMotion, the 0x1fa230 texture class) and `unassigned@00148db0` for the ring classes.
   TU 33 (the frontier's `unassigned@00252840`, 156 functions): **`game/uiwidget.cpp`
   0x25b8e0..0x25fa50** cut out of its tail (56 functions, batch 066 with 74 names incl. FS2's
   `MoviePlayer` API, `fsmRelocate` / `fsmFindText`, `Clock::getSeconds` / `resetTime`, newlib
   `memmove` 0x1252d8; all written, `verify`; architecture.md "Modules"): `UiWidget::create` and the
   13 prg* widget classes, `UiTween`, `UiFont::countLines`. New `game/uiwidget.h`;
   `UiScrollWidget` (uisequence.h) is now `PrgTransMsg`; `S2dWidgetDef::_10` became `param[8]`;
   `UiTextTrack::m_30` became `m_msgKey` (the message image's name); `UiDrawer::closeDirect` made
   public (the movie widget calls it); stand-ins in `gamehooks.h`: `SoundStream`, `MoviePlayer`,
   `getUiWorkArea`, fsm. Checked: callcheck clean, layout lint OK, g++ -fsyntax-only clean (only
   the known gfxtarget.h / stageresult.cpp misses). Kept as the ROM, for the user to overrule:
   PrgRot::draw concatenates the scale twice (the map zoom is squared); an angle set at once or
   from a move is wrapped by pi, not 2 pi; prgMsg / prgMsgReference reserve a slot they leave
   empty; prgMsg's token replacement works in place (values are shorter than the tokens).
   PC: null checks where the ROM calls `findTrack` / `getAnchor` on whatever it found; the movie /
   stream widgets' text position starts at 0. Doubts: `GameOptions::_04` picks the movie's audio
   channel (0 mutes channel 1, else channel 0: a voice language?); the arguments the stream widget
   passes as constants (setBuffer, play, stop); `getUiWorkArea` (sbss 0x31ae40) is set by whom;
   `UiShowFlags`' meaning. **The movie draw path must go to a host movie module** (decode the
   movie file to a GPU texture, no GS upload) when bootstrap 6 / the movie TU comes:
   `MoviePlayer::drawFrame` / `prime` are the game's GS uploads. **Next in the run**:
   0x252840..0x25b8e0 (still unassigned: its frontier is `mslTreeIncrement` / 0x25b590, MSL
   instances already mapped to the host STL by vcmtable.cpp, and 0x252840 / 0x252ac0 called from
   unassigned@0023e740).
   TU 32 (the frontier's `unassigned@00296d60`, 267 functions 0x296d60..0x2a9c70, for
   `getNaviMenu` / `getSaveExtra` / the string dtor 0x297fb0): **`game/vcmtable.cpp`
   0x296d60..0x299d90** cut out of the run's head (52 functions, batches 063-065 with 59 names:
   all 9 game functions written, the 43 MSL string / set / map instances mapped to the host STL in
   a comment list, `verify`; architecture.md "Modules"): `VcmTable`, the VCM__ part fades of a
   rena model. New `game/vcmtable.h`; stand-ins in `gamehooks.h`: `RenaModel` (+ `ModelHeader` /
   `ModelPart` / `ModelGeometry` / `ModelMaterial` / `ModelPartRec`, `findGroup` 0x2e4100) and
   `VcmController` (`getWeight` 0x185af0; name +0x20, active +0x34). **The binary's std::string
   instances are in this file** (0x297fb0 etc., now `std::string::*`, lib-context). Checked:
   callcheck clean, layout lint OK, g++ -fsyntax-only -Wall -Wextra clean, and a host harness (5
   parts: own VCM__ name, a group's VCM__ name, a material tag, an untagged part, the bare VCM__
   fallback; attach by name incl. a non-token; weights 0.5 / 0 / 1) gave the expected tokens,
   states, links and record bytes. Kept as the ROM, for the user to overrule: each token takes
   one byte more than its run (a middle token keeps a trailing '_'; harmless for the one-token
   tags on the disc); the bare `VCM__` tag makes an empty token; the fade / off bits are compact
   and stored in bytes (a tag with more than 8 tokens loses the high ones); a state without
   tokens stores the previous state's alpha (register reuse; PC: the first one 0). PC: the token
   names are delete[]d (the ROM uses operator delete) and the states are zeroed at construction.
   Doubts: the file / class names (only the VCM__ string), geometry / material for the part's +8
   chain, what the renderer does with the record bytes +0xa..+0xc. Seen while checking, not this
   TU's: `game/stageresult.cpp:144` passes `void *drawCtx` to `UiLayout::draw`, which takes
   `TexAreaList *` since TU 31 (g++ error; fix the parameter type in stageresult.h / .cpp).
   **Next picks in the run**: the navi menu 0x29acb0..0x29eff0 (`getNaviMenu` 0x29efe0 /
   `getSaveExtra` 0x29efd0 at its end with its init 0x29ef70; widgets on vtables 0x318820 /
   0x318840; ~17 KB: two or three runs), the VU0 vector helpers 0x299d90..0x29a700 (15+ callers),
   the stage object class 0x29a700..0x29acb0 (vtables 0x311860.. / 0x3185b0..).
   TU 35 (the frontier's pinned `sound/sound.cpp` 0x144080..0x148db0): **all 98 written, `verify`**,
   no Ghidra change (batch 013's names held; architecture.md "Modules"). Kept as one file (the pin
   said "probably several": the three semaphores, the inlined SPU / volume / closeBank helpers and
   CodeWarrior's placement of inline slots tie it together; split later if a string says otherwise).
   New `src/sound/sound.h` takes SoundStream / SoundSink / SoundPcmSink / SoundAdpcmSink /
   SoundFeeder / SoundConfig / gSoundConfig and soundInit / Update / Shutdown out of `gamehooks.h`
   (which includes it) and adds SoundSeBank, SoundEffect, the SPU / IOP hooks and the state enums;
   SoundStream's stand-in signatures changed (`stop(bool wait)`, `openBankFile` answers the bank,
   `play(request, start)`), SoundFeeder's `update` answers int. New declaration headers for FS2-only
   libraries: `src/rx/rxsndstr.h` (rxSndStr* / rxSndStrBank*), `src/rx/rxsndut.h`,
   `src/sdpr2/sdpr2.h` (argument names read from these callers only). Defined here: `gSoundConfig`
   (no longer "data the build must define"). Checked: callcheck clean, layout lint OK. **Not checked:
   no compiler** -- `yamipan.MINGW_BIN` (CLion 2's MinGW) no longer exists on this machine and no
   other g++ was found, so no -fsyntax-only run this time (for the user: install / point MINGW_BIN
   at a MinGW; layoutcheck's GCC pass needs it too). Kept as the ROM, for the user to overrule:
   `update` asks rx about stream -1 right after a pending stop resolves; `setBuffer` does not free an
   earlier owned buffer; a stereo sink with interleave 0 would loop forever in the feeder. PC: null
   guards where all 32 SPU nodes are taken (the ROM writes through null); `m_fadeEnd` zeroed by the
   ctor; IOP addresses pass through u_int (the host sif heap must answer 32-bit addresses). Doubts:
   `rxSndStrPlayAt`'s 0, 4 arguments, `rxSndSetSysParam` kinds 0..2 and the SDPR2 config words are
   unread. **Bootstrap 6 must give**: RosePan's `rx/rxsnd.h` / `rx/rxthread.h` (they have every
   rxSnd* / rxTh* call used), `sdk/eekernel.h` **`DelayThread`** and `sdk/sif.h`
   **`sceSifFreeSysMemory`** (RosePan has neither), and host implementations (or stubs answering
   "ended") for rxsndstr / rxsndut / SDPR2. **Next picks**: the sound users 0x2908d0..0x2931b0
   (`SePlayer`, the SoundStream / SeBank wrappers; unassigned@002681e0), the ring classes TU
   `unassigned@00148db0` (StreamRing / FileStreamRing 0x151f90..0x153110, right after this file).
   TU 36 (the queue's `rx/rxsnd.c` 0x159bc0..0x162178, 2026-11-03): **all 229 written, `verify`**
   (225 + four FS2-only `rxSnd*GetActiveList` found hidden in the tails of resized functions,
   created in Ghidra and named, batch 069; architecture.md "rx" has the differences and the data
   addresses). New `src/rx/rxsnd.h` (C and C++: typedefs, `extern "C"`) and `src/rx/rxthread.h`
   (the seven rxTh* calls rxsnd makes). The file is RosePan's `rxsnd.cpp` carried to C: 215
   functions compared word for word in the two ELFs, the 14 others read in FS2. Checked: callcheck
   clean, layout lint OK, **gcc -std=c11 -fsyntax-only -Wall -Wextra clean** and the header from
   g++ (sound.cpp's only errors are the known DelayThread / sceSifFreeSysMemory) with RosePan's
   `src/sdk` on the include path. **For the user: a MinGW exists at `C:\msys64\ucrt64\bin`** (gcc /
   g++; TU 35 found none): point `YAMIPAN_MINGW_BIN` / `yamipan.MINGW_BIN` there. Doubts / choices:
   kept RosePan's literal command numbers and flag bits (the file is a carry-over; naming them as
   enums is a later pass over rxsnd.h, for the user to order); `s_queue` back to 0x1000 (FS2's
   margin makes RosePan's 0x1040 unneeded); `RosePan_RxSndNoteEeAddress` renamed
   `YamiPan_RxSndNoteEeAddress`, which **bootstrap 6 must give** in `iop/rxsnddrv/rxsnddrv.h` with
   RosePan's IOP driver (`src/iop/rxsnddrv/`), plus `sdk/sifman.h` / `sdk/sifrpc.h` / `sdk/eekernel.h`
   (`SemaParam`, `DeleteSema`, `SignalSema`, `sceSifGetOtherData`, `sceSifReceiveData`). The meaning
   of commands 0x52 / 0x53 / 0x62 is unread (the IOP module would tell). Next pick: `tus.py next`.
   TU 37 (the frontier's pinned `rx/rxsys.c` 0x100328..0x1037c0 + 0x153108..0x153518,
   2026-11-03): **all 126 written, `verify`** (batch 070: newlib `atoi` / `strtol` / `_strtol_r`;
   architecture.md "rx"). Every function is RoR's word for word (the new **`tools/reverse/rxcmp.py`**
   compares a shared library by name in both ELFs; `--data` pairs the data addresses), the data
   words and the console font too, so the file is RosePan's `rxsys.cpp` carried to C with FS2
   addresses and no FS2 difference. `src/rx/rxsys.h` is the full header now (C and C++, `extern
   "C"`); new `src/rx/rxfile.h` (the driver / marker / tag / segment / slot types, the slot pools and
   the 26 rxFile* calls rxsys makes, FS2 addresses); `rxthread.h` gained rxThYield / rxThRequest
   (+ `RxThRequestFunc`) / rxThInitOnce / rxThExitOnce. Host names renamed as TU 36 did:
   `RosePanDebugHandler` -> **`YamiPanDebugHandler`**, `RosePan_GetHostPointer` ->
   **`YamiPan_GetHostPointer`** (`yamipan/yamipan_memory.h`). Checked: callcheck clean, layout lint
   OK, g++ -x c++ -fsyntax-only -Wall -Wextra clean (RosePan's `src` + `sdk/pc_prefix.h` forced, a
   two-line shim for the renamed names); gcc -std=c11 fails **only inside RosePan's `sdk/sce_gs.h` /
   `libgraph.h`** (C++ `alignof` without `<stdalign.h>`, `sizeof(SCEGIFTAG_EOP)` on a macro).
   **Bootstrap 6 must give**: C-clean SDK headers (`sce_gs.h`, `libgraph.h`: the rx TUs are C),
   `sdk/pc_prefix.h` (scePrintf) forced on C as well, `yamipan/yamipan_memory.h`
   (`YamiPan_GetHostPointer`), `sdk/eekernel.h` with `YamiPanDebugHandler` / `SetDebugHandler`,
   `iDisableIntc`, `GetMemorySize`, `PollSema`; `sdk/libcdvd.h` (sceCdGetDiskType / sceCdMmode /
   SCECdPS2DVD / SCECdDVDV / SCECdINIT), `sdk/sifdev.h` / `fileio.h` (sceRead / sceLseek / sceIoctl).
   Host behaviour (RosePan's, unchanged): the boot path leaves s_bootDev empty (no tty), no BIOS
   ROM date, the exception screen is log-only. Doubt: FS2's disc has no system.ini, so the
   save-data name rxSysInit builds stays "BISCPS-00000" (nothing in rxsys reads it again). Next
   pick: `tus.py next` (rx/rxfile.c, rx/rxthread.c are the neighbours; RosePan has both).
   TU 38 (the frontier's pinned `rx/rxfile.c` 0x153518..0x156060 + 0x158400..0x159928,
   2026-11-03): **all 108 written, `verify`** (no new names, no Ghidra change; architecture.md
   "rx"). `rxcmp.py '^rxFile' --lo/--hi` over both ranges: **106 identical to RoR**, the other two
   (`rxFileOnCreate` / `rxFileOnGetSize`) print FS2's longer message (file, line 1018 / 1041, device,
   flags, ring, ring size, result; marked `FS2:`), so `src/rx/rxfile.c` is RosePan's `rxfile.cpp`
   without the three drivers, in C with FS2 addresses. `src/rx/rxfile.h` is the full header now
   (every core type + prototype, typedefs for C); new **`rx/filehost.h` / `filecd.h` / `filehdd.h`**
   hold the driver prototypes (and filecd's cache node / find hook) for their own TUs;
   `rxthread.h` gained rxThGetIndex / Delete / SetPriority / GetParam / SetState / Create.
   `gamehooks.h` lost its `rxFileCallSizeHook` / `rxFileCallLoadHook` prototypes (it includes
   `rx/rxfile.h`): **the game's 0x152730 / 0x152740 are jump thunks to the rx functions
   0x155690 / 0x1556d0 with the same Ghidra names** (callcheck matches by name, so fileloader.cpp
   keeps calling the rx ones). Checked: callcheck clean, layout lint OK, g++ -x c++ -fsyntax-only
   -Wall -Wextra clean on rxfile.c, rxsys.c, fileloader.cpp, gamepaths.cpp (RosePan's src +
   pc_prefix.h, two stub headers for yamipan_memory.h / libvifpk.h). Doubts for the user: (1) the
   second range 0x158400.. is very likely **its own object file** (text, data 0x2f5b18.. and bss
   0x4e7e80.. all come after the three drivers', and it has no rxfile.c string): kept in rxfile.c
   as pinned; (2) the zero word at data 0x2f5aa4 between s_verbose and filehost's data is unread
   (RoR has none there); (3) whether to give the thunks their own Ghidra names (a game rx-glue
   file with rxInit / rxTerm). Bootstrap 6: the host drivers come with filehost.c / filecd.c /
   filehdd.c (RosePan's, with FS2's longer host messages: rxcmp shows rxFileHost* and
   rxFileCdFindDir 0x10..0x18 longer). Next pick: `tus.py next`.
   TU 39 (the frontier's pinned `rx/rxthread.c` 0x159928..0x159bc0 + 0x166b50..0x1681c0,
   2026-11-03): **all 53 written, `verify`** (no new names, no Ghidra change; architecture.md
   "rx"). `rxcmp.py '^rxTh'`: all 53 RoR's word for word, so `src/rx/rxthread.c` is RosePan's
   `rxthread.cpp` in C with FS2 addresses; `src/rx/rxthread.h` is the full header now (slots, call
   queue, every prototype, C and C++). Named as written: `RX_TH_SYNC_COUNT / NEXT / NOWAIT`
   (rxThSync's modes), the INTC vblank cause and RCNT0 mode bits (file-local). Host names renamed
   as before: `RosePan_Sleep` / `RosePan_SleepNs` -> **`YamiPan_Sleep` / `YamiPan_SleepNs`**
   (`yamipan/os/yamipan_time.h`). Checked: callcheck clean, layout lint OK, **g++ -x c++
   -fsyntax-only -Wall -Wextra clean** (RosePan's src, pc_prefix.h, a shim for yamipan_time.h; one
   C++20 warning on `++` of the volatile vblank count, harmless in C). **Bootstrap 6 must give**,
   beside the C-clean `sce_gs.h` / `libgraph.h` TU 37 asked for: **a C spelling of
   `REG_RCNT0_COUNT`** (RosePan's `sdk/eeregs.h` defines it only under `__cplusplus`, a C++ proxy
   over `RosePan_Rcnt0Read` / `Write`; rxThVBlankInit / rxThReadHSync read and assign it), and
   `yamipan/os/yamipan_time.h` from `rosepan_time.h`. Doubt: the thunks 0x151f70 / 0x151f80 (game
   code calling rxThEnableDispatch / rxThDisableDispatch) sit with StreamRing's run
   0x151f90..0x153110; their TU should call the rx functions, as fileloader does. Next pick:
   `tus.py next`.
   TU 40 (the frontier's pinned `sdpr2/sdpr2.c` 0x1681c0..0x16c870, 2026-11-03): **all 36
   written, `verify`** (no new names, no Ghidra change; architecture.md "SDPR2"). `src/sdpr2/
   sdpr2.h` is the full header now: `Sdpr2Type` (the 19-slot function table, `layout: host`),
   `Sdpr2Slot` (the handle, `layout: host`), every API prototype, the limits as `SDPR2_*` names;
   the inline checks with this file's message copy are **`src/sdpr2/sdpr2check.h`** (static
   inline: each SDPR2 file gets its own strings, as the ROM's four copies) with **`SDPR2_TRY`**,
   which evaluates its expression twice as the ROM's macro does (a failed check prints twice, a
   failed init runs twice: kept). New minimal headers for the other three files: `sdpr2drv.h`
   (`sdpr2DrvInit(config)`), `sdpr2pair.h` (`sdpr2PairInit(void)`), `sdpr2single.h`
   (`sdpr2SingleInit(void)`, and `sdpr2CalcPanDeg` moved there from sdpr2.h; sound.cpp includes
   it). The API now returns int everywhere (Stop / StopAll / SetAngle / SetVolume / SetSpread were
   void in the old stand-in). Checked: callcheck clean (only the inline helpers as extra), layout
   lint OK, gcc -std=c11 and g++ -x c++ -fsyntax-only -Wall -Wextra clean, and a host harness
   (fake pair type): limits, the round of -360 to 360, the slot getting its type, the getters'
   setter check, double prints. Doubts for the user: the slot's +0x10 / +0x18 (angles?) and
   +0x0c flip / +0x28 state are set only by the types (unread here); the `ext40..48` slots and the
   `getLevels` out type are unread (null / not called on the disc); `sdpr2SetSurround` /
   `sdpr2SetPairMode` store the unlimited value (the ROM drops the limit's result; kept). Slots
   stay `static` in sdpr2.c: if sdpr2pair.c / sdpr2single.c read 0x537140 directly instead of
   through `sdpr2GetSlot`, make it extern there. Next pick: `tus.py next` (the other SDPR2 files
   are the neighbours).
   TU 41 (the frontier's `unassigned@00148db0`, 146 functions 0x148db0..0x152750, for the movie
   player's rings): **`game/streamring.cpp` 0x151f70..0x152750** cut out of its tail (21 functions:
   StreamRing / FileStreamRing's 17, all written, `verify`, plus the four rx jump thunks mapped to
   the rx calls; no new names, no Ghidra change; architecture.md "Modules"). The classes left
   `gamehooks.h` for the new `game/streamring.h` (gamehooks.h includes it; sound.h's forward
   declaration points there). This settles TU 39's doubt about the thunks 0x151f70 / 0x151f80.
   Checked: callcheck clean (only the inline helpers as extra), layout lint OK, g++ -fsyntax-only
   -Wall -Wextra clean on streamring / fileloader (movieplayer / uiwidget / scene / sound only fail
   on the known bootstrap-6 SDK gaps), and a host harness (owned ring and 7/8 threshold, caller
   memory zeroed, every slot -1 / null before open, missing file, contiguous cut at the ring end,
   consume total, primed, delete through the base pointer closing the stream once and freeing an
   owned ring only) passed. Doubts for the user: the file name (no strings); the destructor frees
   an owned ring before closing the rx stream that reads into it (kept as the ROM; on the host the
   rx file thread must not write into the freed block between the two: bootstrap 6's host rx
   should close synchronously). StreamRing's base slots 0x1530d0..0x153100 stay in
   game/spkrom.cpp (unwritten). Left in `unassigned@00148db0` (125 functions 0x148db0..0x151f70):
   classes on vtables 0x311200..0x311280 (0x150f50 / 0x151020 with "mdl", the Clock users
   0x151330..0x151840, a texture class 0x1518a0 with Tim2Texture / sceVif1Pk calls) and
   `debugSysInit` / `Update` / `Shutdown` 0x151270..0x151290 (empty; frontier from gamesystem.cpp).
   Next pick: `tus.py next`.
   TU 42 (the frontier's `unassigned@00133800`, 65 functions 0x133800..0x1366bc, 2026-11-03): cut
   into **`game/mathutil.cpp` 0x133800..0x1350d0** (18) and **`game/clock.cpp` 0x1350d0..0x135990**
   (14), **all 32 written, `verify`** (batches 071-072, 23 names: the math helpers, `__errno`
   0x123528, `Clock::create` / `setPaused` / `getDeltaSeconds`, and `setVecLength` 0x135fe0 /
   `scaleVec4` 0x136490 in the VU0 run; architecture.md "Modules"). New `game/mathutil.h` (took
   `normalizeAngle` from `gamehooks.h`; uigame.cpp includes it) and stand-in `game/vu0math.h`;
   **`clock.h` corrected: Clock +0x18 is a `PtrList *`** (made on the first child), not an inline
   {u16, u16}; the fields moved to a base **`ClockNode`** (its destructor 0x135550 is the real one,
   `~Clock` / `~MasterClock` only call it: the Ghidra name kept). Checked: callcheck clean, layout
   lint OK, g++ -fsyntax-only -Wall -Wextra clean on mathutil / clock (uigame / uiwidget /
   gamesystem: the same pre-existing stub-header errors as before), and a host harness: sinCos /
   asinAcos / atan2Cw / atan2Octant within 3.2e-7 of libm over 200,000 arguments (up to 1e5 for
   sin / cos), both Euler extractors round-trip angles (0.3, -1.1, 2), the clock tree's advance /
   pause / scale / detach. Doubts for the user: the file names (no strings) and the names
   ClockNode / matrixToRotYXZ / ZYX (the order read from the matrix entries, sce layout m[i] =
   image of axis i); **ROM bugs kept**: `ClockNode::~ClockNode` detaches only its FIRST child
   (removing it clears its slot, so `next(child)` ends the walk) -- the others keep a dangling
   parent, harmless while children die before parents (on the host a later child delete would
   touch freed memory: watch for it at bootstrap 7); when that child was the last, the ROM walks a
   null list (EE address 4), the port stops; `getBounds` starts its maximum at FLT_MIN, not
   -FLT_MAX; `matrixToRotYXZ`'s pole case reads m[2][2] (0 there). **Next: the VU0 run
   `unassigned@00135990` 0x135990..0x1366c0** (20 functions, all game code, asm needed: the
   decompiler loses the COP2 broadcast fields; decode the `.word 0x4b...` with dest bits 24..21,
   ft 20..16, fs 15..11, fd 10..6, funct 5..0 -- special2 opcode = (bits 10..6 << 2) | bits 1..0):
   rotation matrices 0x1359f0 / 0x135b90 from a sin / cos table 0x135c30, angle wrap 0x135e20 /
   0x135ec0, distances 0x135f20 / 0x135f70, normalize 0x136020, project 0x136090, identity / copy
   matrix 0x136400 / 0x136430; vu0math.h takes the rest of its prototypes then.
   TU 43 (the frontier's `unassigned@00171300`, 829 functions 0x171300..0x1938f8, for
   `getStageFiles`): **`game/stageloop.cpp` 0x17f150..0x17fce0** cut out (9 functions, batch 073
   with 45 names, **all written, `verify`**; architecture.md "Modules"): the stage thread, its
   game-over / clear / pause / restart helpers, `stageLoad`, `setStageLoader`. New
   `game/stageloop.h` (`gStageScene` is defined here now, its extern moved from gamehooks.h, which
   includes stageloop.h; `getStageFiles` moved too). gamehooks.h: `StageSound` gained ctor / dtor /
   pause / resume / setMuted / stopAll / isStopped; `StageScene` gained ctor / dtor and 13 methods
   and the fields +0 / +4 / +8 / +0x30 / +0x60 / +0x88 / +0xec / +0x10c / +0x110; `StageFiles`
   became a class (ctor, dtor, load / loadCommon / loadList / loadSubSceneList); new stand-ins
   `StageParty`, `KeepList`, `StageEvent`, `PauseMenuChoice` + pause / map menu start / result.
   `StageSelect::_0c` is **`exitKind`** (`StageExit`, sysdata.h); `GameSystem::beginStage` 0x12cce0
   declared in gamesystem.h (its TU unassigned@0012c9b8); `StageResult::update` takes a
   `TexAreaList *` (was `void *`: fixes the g++ error in stageresult.cpp seen since TU 32). Checked:
   callcheck clean, layout lint OK, g++ -fsyntax-only -Wall -Wextra clean on stageloop / stageresult
   / uigame / sysdata / scene / screen (RosePan's SDK headers + scratch libvifpk / sceGsTexEnv
   stubs); no harness (all control flow). Host: the frame brightness writes the ALPHA FIX byte into
   the copy packet as the ROM does; the any-aspect renderer must apply it as a brightness (the copy
   packet itself is dropped on the host). Doubts for the user: the file name; every name is read
   from the code (StageEvent and its slot 3, KeepList, StageParty, getActiveCharacter, the pause
   menu's 3 = quit / 4 = navi from its thread 0x20ad70, select = the map menu from 0x20b010 reading
   getStageFiles); stageAnswerZero's owner is unknown (kept in this file by position). PC: a sub-scene
   is built with `new` (the ROM writes through null when operator new fails). **Next picks**: the
   scene class itself (`StageScene` 0x195640..0x19c6a0 in unassigned@001953f0: ctor / dtor /
   update / draw are every stage's first stubs), `StageFiles` 0x1a37e0..0x1a4370, the menus
   0x20acb0..0x20b100, `StageSound` 0x1d3f80..0x1d79c0.
   TU 44 (the frontier's `unassigned@001953f0`, for StageSound's frontier callees): **`game/
   stagesound.cpp` 0x1d3dc0..0x1d7c50** cut out (45 functions, batches 074-075 with 30 names incl.
   the SE player's `fadeMaster` / `pause` / `resume` / `isPlaying` / `setVolume` / `setAttr` /
   `playUnscaled` and the `SoundTrack` stream class 0x290ca0..0x2916f0; architecture.md "Modules");
   **22 written, `partial`**: ctor / dtor / reset, the slot release and channel reset, stopAll,
   isStopped, isSettled, pause / resume / setMuted, setEffectsPaused, the effect play (playNamed /
   playNamedAlways), stopNamed / stopSource, the three stream-file setters. New `game/stagesound.h`
   (StageSound moved out of `gamehooks.h`, which includes it; `SeHandle` moved there too);
   `gamehooks.h`'s `SeEntry` got its fields (name, bank, program / note, **resumable +0x1d**, pitch
   spread, id, volume), `SePlayer` its methods and `m_masterVolume` (+0x84), new stand-in
   `SoundTrack : SoundStream`; `SePlayer::stop` answers int. Checked: callcheck clean, layout lint OK,
   g++ -fsyntax-only -Wall -Wextra clean on stagesound / stageresult (RosePan's SDK headers + a scratch
   libvifpk stub; stageloop / uigame / scene only show the known gfxtarget / rena SDK gaps); no harness.
   Doubts for the user: the file name; the channel state names 1 / 3 (Starting / Pausing) are read
   only from where the update sets them; `isStopped` also accepts a SoundStream state 5 no code sets
   (kept); `setEffectsPaused(true)` gives a paused handle `setAttr`, which the player ignores for
   paused handles (kept); a source's +0x100 / +0x108 / +0x115 are all that is declared of its class.
   2026-11-03 run 2 (batch 076, 19 names): **27 written**, still `partial`: the update 0x1d5580 with
   `updateSlots` 0x1d5e40 and `updateChannels` 0x1d56a0, `stageSoundLocateFacing` 0x1d7a30 /
   `stageSoundLocate` 0x1d79c0; SoundTrack / SePlayer got their placed play / placement / update /
   pause / attr methods, StageScene `raycast` / `getSoundAttr` / `fade` (+0xfc), new stand-ins
   `BoneNode`, `StageModel`, `StageNode`, `StageFade`, `StageCollisionFace`, `StageObjRecord`
   (`gStageObjRecords`, bss 0x539b10). Doubts for the user: **`StageObject::m_name` shrunk from
   0x1e0 to 0x10** (a character's model pointer is at +0x30, the raycast reads +0x34 / +0x37 of
   scene objects; item names are ItemDef's 0x10) -- if some object's name is longer, the stand-in
   has to split by family; `StageFade::isFullInMode2` (0x1512a0) and the class are guesses (mode 2,
   level 1.0); the update's a2 / a3 are called playerFlag (StagePlayer 0x214f60: a motion state
   0x11 / 0x2a, or 0x1fe800 of the party for the scene's player) and sceneFlag (StageScene
   0x19b900) -- what they mean in the game (sightjack?) is unread; listener kinds 9 / 0xb and record
   kind 0xf are unidentified; `getSoundAttr` (0x284190's area grid) passed as the sound attr is
   probably reverb. PC guards: updateChannels returns on a null listener (the ROM dereferences it),
   stageSoundLocateFacing skips an out-of-range record index (the ROM reads address 0x10).
   2026-11-03 run 3 (batch 077, 31 names): **all 45 written, `verify`**: the cue queries
   (getCueSubtitle, stopCue / stopCuesOf, hasCue, isCuePlaying, setCue / playCue, pickCueChannel,
   findCue), the music (playMusic / stopMusic / setMusicVolume), the positional sources (addSource /
   addSourceNamed, findSource, setSourcePitch, the source dtor), setSeTable (pairs X_J / X_D
   entries with X through altOffset). **Decision (taken unattended, the user may move it): new
   header `game/stagenode.h`** for the stand-ins of the uncut scene-node run unassigned@0017fce0
   (`StageNodeBase` 0x1d8540 / 0x1865c0, `StageNodeList` 0x1842c0 / 0x184370, `NodeTransform`
   0x1d7f20 / 0x1d7ee0, `StageNode` moved out of gamehooks.h): StageSoundSource now derives from
   StageNode, and gamehooks.h includes stagesound.h, so the base could not stay in gamehooks.h.
   The source's +0x4 is its parent node (was `_04`). `SeEntry` got `subtitleKey` +0x8 and
   `priority` +0x2c (a cue's: an equal or lower value takes a busy channel), `cycle` is `mutable`;
   `SoundTrack` got `m_table` +0x60 (`StreamTable`: count +0x10, entries from +0x40), hasSubtitles /
   getSubtitle / setVolume; `SePlayer` setTable / setPitch. Boundaries decided: 0x1d7c50.. is the
   node family's (its transform 0x1d7ee0 / 0x1d7f20 and node base ctor 0x1d8540 are there), not
   StageSound's; 0x1d3d20 (a lookup with eight callers elsewhere) stays outside. Doubts for the
   user: the subtitle text type (`const u_short *`, from 0x28fa90 testing a short); setSeTable
   resets each entry's altOffset on its own turn, so a base after its alternate loses the link
   (kept as the ROM); getCueSubtitle's priority test against 0x7fffffff is always true (kept).
   PC guards: setSeTable skips names of 32 characters or more (the ROM's stack buffer). Next for
   the sound: the callers 0x2a6380..0x2a6830 (the stage objects' sound commands) and the node
   family 0x17fce0.. / 0x1d7c50.. when the frontier reaches them.
   TU 45 (the frontier's `unassigned@002908d0`, 67 functions 0x2908d0..0x2955d8, for StageSound's
   callees, 2026-11-03): cut into **`game/seplayer.cpp` 0x2908d0..0x2937e0** (35 functions, all
   written, `verify`: SoundTrack 17, SePlayer 18) and `getNaviTable` 0x2955d0 moved into
   `game/progress.cpp` (now 0x2955d0..0x296c10; written there). 0x2937e0..0x2955d0 stays
   `unassigned@002937e0`: an **ELEVATOR2 stage object** (bone lookups "bn_bx_ib1" / "bn_b1_dl",
   vtables 0x318310 / 0x318334 / 0x31834c / 0x31855c, ctor 0x295010?, dtor 0x2952b0) and a menu
   overlay at 0x295470 / 0x2954f0. Batch 078 (7 names): `fsmFindTrack` 0x28fc80, `fsmTrackHasText`
   0x28fa90, `seDistanceVolume` 0x283df0, `stdtIsRelocated` 0x1a0570, `stdtEntryCount` 0x1a0560,
   `SePlayer::openStreamBank` 0x292090, `SePlayer::loadBank` 0x292140. **Choices (unattended, the
   user may move them): new header `game/seplayer.h`** takes SeEntry / SeName / SeTable /
   StreamTable / SePlayer / SoundTrack and `SeHandle` out of gamehooks.h / stagesound.h
   (stagesound.h includes it); **SEDT and StDT are now EE-layout file images with `S2dRef`
   pointers** (asserted, relocated in place like S2D; their `layout: host` is gone) and the
   handle's bits have helpers (`seHandleBank` ..., `seMakeHandle`). Fixed readings: the voice
   records are 3 x 0x40 `SeVoice` {scale -1 free / 0 unscaled / 1 under the master volume,
   level, pan, ratio, bank, placed} (voice banks by voice, pan voices by handle, stream banks by
   bank); `resume` does pass left / right on to `play` (the old comment said not read);
   `SePlayer::playPlaced`'s last argument is the SoundSeBank pan pair flag; SoundTrack's
   `flag` is **secondHalf** (play the request's second half: StageSound's player flag picks the
   alternate take). Doubts, kept as the ROM: SePlayer::pause / resume of a **stream bank** refuse
   while the stream plays (the test looks inverted); `SePlayer::setVolume` on a voice bank only
   rewrites the record (the level lands at the next master fade, when `update` re-levels);
   `setPlacement`'s stream-bank pan is not under the master volume. Unread: who fills an StDT
   bank's header (+0xc, not relocated by setTable; the stage loader 0x19d9e0 is the guess).
   PC guards: record writes only for voices 0..0x3f, `pause(-1)` answers -2 (the ROM reads bank
   -1), SoundTrack's freed body path is cleared (the ROM frees it twice when a bank has no file),
   no StDT table plays nothing. Checked: callcheck clean, layout lint OK, g++ -fsyntax-only -Wall
   clean on seplayer / stagesound / progress / stageresult (msys g++ with PATH set, RosePan's SDK
   headers + a libvifpk stub; uigame / scene / stageloop / uilayout still fail in gfxtarget.h /
   rena headers against RosePan's libgraph, not this TU). Next: the ELEVATOR2 object run when the
   frontier reaches it; the seplayer frontier is fsm (0x28fa90 / 0x28fc80 / 0x28fb80 / 0x28fd30)
   and seDistanceVolume in unassigned@002681e0, the StDT getters in unassigned@001953f0.
   TU 46 (the frontier's `unassigned@001953f0`, 972 functions 0x1953f0..0x1d3db8, 2026-11-03):
   cut into **`game/stagescene.cpp` 0x1953f0..0x1a68b0** (258 functions: StageScene 0x195640..
   0x19c180, the stage loaders, StageFiles 0x1a37e0..0x1a4270, string-map templates and
   this-adjust thunks last); 0x1a68b0..0x1d3db8 stays `unassigned@001a68b0` (the character /
   actor classes, vtables 0x315690 / 0x315850 / 0x315b10 / 0x315cf0). One file for scene and
   files is a guess (no source-path string): split StageFiles out if a later read says so.
   Written (11, `partial`): getPurposeText (now a member: uigame passes gStageScene),
   isSubSceneDone, getMission, getParent, raycast, drawPaused, stdtEntryCount, stdtIsRelocated,
   StageFiles::dataDir / load / loadSubSceneList. Batch 079 (17 names). **Choices (unattended):
   new header `game/stagescene.h`** takes StageScene / StageFiles / stdt* out of gamehooks.h
   (gamehooks.h includes it); **`FileLoader::m_arg` is `m_dir`** (+0x14, the directory
   `m_prefix` copies) with the inline `setDir` (stageLoad's two stores now call it; the ctor
   takes `const char *`); StageObject gained unions at +0x200 / +0x208 / +0x20c for the Root
   objects' collision fields (m_collides, m_collisionCount, m_collisions) and m_active (+0x34) /
   m_hidden (+0x37, a guess); `StagePlayer::character()` casts the player to its StageObject
   stand-in (the ROM's player is a character); `raycast`'s skip is a StageObject*. Guessed names
   to revisit: StageMission (+0xe8), StageViewStack (+0x34), StageViewTarget (+0x100),
   DemoStream (+0x28), StageObject::getAltSoundFlag, StagePlayer::isMode1,
   StageParty::getCompanion. PC choice: raycast's best-hit local starts as hit's contents (the
   ROM's is uninitialised and copied back even with no hit). **Next for this TU**: the
   constructor 0x19c180 / destructor 0x19bbe0 (the member map is the file comment of
   stagescene.h: about 25 member classes in other runs need a name and a minimal declaration
   each), then `parseScript` 0x1a20f0 (0x1408 bytes) with the line splitter 0x1a3500, the line
   list 0x1a5a60 (vtable 0x311210), the container reset 0x1a3dc0, the keyword handlers 0x1a0580..
   0x1a1b50 and StageFiles' 22 containers (the templates 0x1a4e80..0x1a6790). Checked: callcheck
   clean, layout lint OK, g++ -fsyntax-only -Wall clean on stagescene / seplayer / stagesound /
   fileloader (stageloop / uigame: only the known gfxtarget.h / rena errors).
   **Second run (2026-11-03, batch 080, 54 names; 30 of 258 written, still `partial`)**: the
   ctor 0x19c180 / dtor 0x19bbe0 and the small members (isCleared, isGameOver, random,
   getSubSceneBlock, clearSubSceneRequest, beginPlay, getOpenMenu, currentMission /
   parentScene (0x196960 / 0x196a40: second copies of getMission / getParent), getSoundAttr,
   addSoundArea, addArea2, getActiveCharacter). Every member class got a name and a stand-in in
   `gamehooks.h` (ctor / dtor and what this file calls; a `destroy()` stub where the ROM deletes
   through a vtable, as for StageObject); `StageScene` now has a full host field map;
   `WideFontPrinter` (0x2d4110) in fontdata.h; `StageMission` ctor / dtor / `markStart`,
   `StageEvent::destroy`, `StageFade` ctor / destroy, `StageParty` / `KeepList` ctor / dtor.
   Choices (unattended): **most member class names are guesses** from position and allocation
   (ScenePairTable, SceneRecordBuckets, StageNodeTree, SceneRender, ScreenFill, MenuProgHolder /
   MenuProgSet, ScenePool, PauseMenuState, UiItemList, StageMenu, SceneBoxList, SceneUiState,
   GameOverSequence / ClearSequence, SceneDirector, SceneModel, DrawLayerList, SceneBufferSet,
   SceneOwnedObject, SubSceneObject, resetStageGroupTable): rename them when their runs are read
   (Rand48, AreaGrid, StageAlert, WideFontPrinter, drawSysPush / drawSysPop are solid). PC
   choices: +0x120 starts null (the ROM ctor never sets it, the dtor deletes it when set); the
   ROM dtor asks the already-deleted sub-scene DemoStream isPlaying / stop (use after free): the
   port clears the pointer at the delete and skips the late stop; getOpenMenu answers null with
   no menu (a sub-scene; the ROM reads through null); the +0x124 second array freed with
   delete[] (the ROM frees p - 0x10, CodeWarrior's header, without element dtors).
   stagescene.cpp now includes gamesystem.h (gGameSystem's clock), which pulls in the rena
   header that fails the stub syntax check (rena_pkt_vif1pktmanage.h:87, known, not this TU).
   **Next for this TU**: `parseScript` 0x1a20f0 (0x1408 bytes) with the line splitter 0x1a3500,
   the line list 0x1a5a60 (vtable 0x311210), the container reset 0x1a3dc0, the keyword
   handlers 0x1a0580..0x1a1b50 and the templates 0x1a4e80..0x1a6790 (0x1a5a00, 0x1a5db0,
   0x1a5e60, 0x1a5f80, 0x1a60a0, 0x1a65a0, 0x1a6630, 0x1a6750 are in the brief's slice); then
   update 0x199400, draw 0x198970, setupCameras 0x199f00, enterSubScene 0x195e10, the pad
   tests 0x195a50 / 0x1958b0 and getFloor 0x19aea0 (stageloop calls them).
   **Third run (2026-11-03, batch 081, 45 names; 13 more written, 39 in stagescene.cpp, and 29 template instances annotated in stagelist.h, of 258, still `partial`)**: the stage
   script reader. `parseScript` 0x1a20f0 (one table-driven `parseLine` for the ROM's two compare
   chains; a sub-scene skips motion, mdl, rute, sndxv, sndseh, font, sndsth, message), `splitLines`
   0x1a3500, `reset` 0x1a3dc0, `parseTarget` 0x1a1b50, `parseEnd` 0x1a1820, `applyScriptTarget`
   0x19fcd0, `applyCharacterTarget` 0x19fac0, the `StageFiles` ctor 0x1a4270 / dtor 0x1a3f90,
   `StageScene::setScripts` 0x19b7a0 / `setRoute` 0x196280 / `isEventActive` 0x1954c0 / `addArea1`
   0x19ae90. New headers: **`game/stagelist.h`** (`StageList<T>`, `StringMap<T>`: one template for
   the ROM's instances, their addresses listed there) and **`game/stringlist.h`** (`String`,
   `StringList`: prototypes only, their TU is the run `unassigned@00148db0`). `strtok` 0x129a60 and
   `strcat` 0x127f78 are named. `StageFiles` has its full field map (every list by keyword; the
   three name maps textures / characters / groups). Choices (unattended): the host `StageList` keeps
   the value in the node (the ROM boxes it in a new'd 4 bytes); `StageScript` / `CollisionFile` in
   gamehooks.h are file-header stand-ins whose **relocate must not write 8-byte pointers into the
   32-bit fields** (their TU decides how); `font` drops the language path the ROM builds and never
   reads; `applyScriptTarget` answers false with no texture (the ROM reads through null);
   `splitLines` keeps the ROM's unchecked 0x410-byte line buffer. Guessed names: RoutePlayer (the
   rute file's meaning is unread), SigLamp, StageEvent_Active (state 2), `StageFileGroup`, every
   list name taken from its script keyword. `0x19cf80` (empty slot 13 of vtable 0x314d30) left for
   that class's run. Checked: callcheck clean, layout lint OK; g++ -fsyntax-only clean on stagescene
   / seplayer / stagesound / fileloader / stageresult (stageloop / uigame: only the known
   gfxtarget.h errors) with RosePan's SDK headers plus stub libvifpk.h / eestruct.h; a scratch test
   of `splitLines` (LF / CR pairs, trim, a Shift-JIS trail byte equal to a backslash), `StringMap`
   (order, duplicates, at) and `StageList` passes. **Next for this TU**: the keyword handlers
   parseTexture 0x1a1430, parseModel 0x1a0e70, parseMotion 0x1a0ae0 (with 0x1a1df0, the .mpk reader),
   parseMap 0x1a0950, parseSndseh 0x1a0580, parseSndsth 0x1a0040, parseMessage 0x19fdf0 (the first
   three need the MSL std::string uses 0x1a6310 / 0x1a4480 / 0x1a5a70 / 0x1a5b70 / 0x1072c0 and the
   path_language inline: the same code in the font keyword), then the apply targets 0x19f680,
   0x19f230, 0x19f140, 0x19f020, 0x19ddf0, 0x19d9e0 (one of them fills `groups`), then update /
   draw / setupCameras / enterSubScene / the pad tests / getFloor.
   **Fourth run (2026-11-03, batch 082, 35 names; 17 more written, 56 in stagescene.cpp, and the
   template instances listed at the end of that file, still `partial`)**: all the keyword handlers
   (`parseTexture`, `parseModel`, `parseMotion` with `readMotionList` 0x1a1df0, `parseMap`,
   `parseSndseh` with `fileWithSuffix` 0x1a4370, `parseSndsth`, `parseMessage`), `pathDir` 0x1a4480,
   `applyParticleTarget`, `applyBgTarget`, and the scene's `addArea0`, `setFlag84`, `setBgName`,
   `setAreaClip`, `setMessages`. `StreamTable::relocate` (seplayer.h / .cpp) now holds the body the
   ROM inlines in both `SoundTrack::setTable` and `parseSndsth`. `DrawLayerList` is **`AreaClip`**
   (0x2dccf0 takes the areaclip file; ctor added), `StageScene` +0x38 `bgScript`, +0x5c `lpt`, +0x68
   `bgName`, +0x84 `flag84`. New stand-ins in gamehooks.h: `ModelFile`, `ModelFileNode`,
   `ModelInstance`, `StageMapFile`, `LptFile`, `DoorRecord` / `gDoorRecords` (bss 0x537d80, filled at
   run time: the build must define it and its filler is unread), `createMapObject`,
   `isGateModelName`, `SubSceneMapObject`, `SubSceneGateObject`, `SubSceneDoorObject`;
   `StageObject::m_doorRecord` +0x298; `SeBankInfo` got its two file names. `ModelFile` and `LptFile`
   use S2dRef: **the ModelFile TU's relocate must write S2dRef, not 8-byte pointers** (the same goes
   for `StageMapFile::relocate`, six pointers at +0..+0x14). Choices (unattended): `pathDir` answers
   empty with no slash (the ROM builds a reversed range); `applyParticleTarget` stops at the end of
   the map when fewer than 16 textures follow common01 (the ROM reads through null); the handlers'
   stack path buffers stay unchecked at 0x100 as in the ROM; `enterDirOf` leaves `FileLoader::m_dir`
   dangling as the ROM does, harmless because only `setDir` touches it (grep before changing that).
   `getGameOptions()->_04` is the argument of `getFileSuffix`: rename `_04` when that option is read.
   Guessed names: ModelInstance, ModelFile (the file's magic at gp-0x7b48 is unread), the gate class
   (oGK99_01, oDC99_01), setFlag84 (caller 0x236b90 unread), setParticleFileA / B (which is which is
   unread), DoorRecord +0x34's meaning. Checked: callcheck clean, layout lint OK, g++ -fsyntax-only
   -Wall clean on stagescene (only the known rena_pkt_vif1pktmanage.h:87 errors) and seplayer; a
   scratch test of languageFile, fileWithSuffix and pathDir passes; nothing built or run. **Next for
   this TU**: `applyMapTarget` 0x19ddf0 (0x1228 bytes) with its door / item helpers 0x19d0c0
   (makes an item object: table 0x574c10, 0xa0 entries of 0x20) and 0x19d4e0, then `applyBgMotionTarget`
   0x19f230, `applyItemTarget` 0x19f020, `applySoundTarget` 0x19d9e0 (one of them fills `groups`),
   then update 0x199400 / draw 0x198970 / setupCameras 0x199f00 / enterSubScene 0x195e10 / the pad
   tests / getFloor.
   Open from TU 5: `gSystemData` is data the build will
   need defined (stubgen makes functions only); the semantics of SaveData +0x468, +0x3ac and the
   record kinds; whether 0x23bb30 or 0x23bb40 is the save data's own getter. ** Its callees' TUs are the frontier: `game/pad.cpp`, `game/gamepacket.cpp`,
   `game/perfmeter.cpp`, `game/thread.cpp`, `game/gfxtarget.cpp`, the clocks, the heap, sound,
   `rxInit` / SPK.ROM, system data. Choices made there, for the user to overrule:
   - **Headers for TUs not written yet are minimal** (`src/game/pad.h`, `clock.h`, `heap.h`,
     `perfmeter.h` (+ `RcntTimer`, `rcnt1Start`), `gamepacket.h` (+ rena's `Vif1PktManage`),
     `gfxtarget.h`, `sysdata.h` (options, ButtonMap, DateTime), `thread.h`); rx / sound / memcard /
     debug hooks / `gameSceneThread` share `src/game/gamehooks.h` until their TUs exist (move each
     prototype to its TU's header then).
   - **The SDK headers the game code includes are RosePan's names** (`sdk/eekernel.h`, `eeregs.h`,
     `libgraph.h`, `libdma.h`, `libpad.h`, `scetypes.h`) plus Sony's `sdk/eestruct.h` (`tD_STAT`) and
     `sdk/libvifpk.h` (`sceVif1Packet` with **`pCurrent` first**, as FS2's packet lists use it:
     RosePan's own `libvif.h` puts `pBase` first) and `ExitHandler()`: bootstrap 6 must provide them.
   - **`gsClearVram` is kept as the game wrote it** (packet + sceDmaSend on GIF) with a `PC:` note:
     with no GS emulator the host GIF channel drops it; host targets start cleared. The GS FINISH
     `GameSystem::flipFrame` writes into packet list 13 stays too (draws nothing).
   - **`main` keeps its name**; the platform layer's entry must call it (rename at bootstrap 6 if
     the host entry needs `main`).
   - Open: why `GamePad::read` 0x12d4f0 and the slot `GamePad::update` 0x12dc60 both exist; the
     names of heap marks 0 / 1 and the meter groups (guesses in `heap.h` / `perfmeter.h`);
     `Clock`'s byte +0x1c (MasterClock's last ctor argument); the empty `debugSys*` hooks' real role.
   (c) the queue (`tus.py next` already picks `game/gamesystem.cpp`, `main`'s TU, with the queue
       empty: "frontier empty: start from main"; a written TU's callees then raise the frontier): game code first (not library code), starting from `main` / `gameMainThread` /
       `GameSystem` and down what boot reaches (architecture.md "Program flow"), each TU written into
       `src/` with its header and names made at that TU.
   The platform layer (bootstrap 6) and first build (7) follow once the user has a toolchain; until
   then reconstructed TUs are `verify` (written, not compiled or seen on screen).

1. **The game**: Forbidden Siren 2, Europe, `SCES_538.51`, annotation tag `FS2` (decided, below;
   `yamipan.py` has `ANNOT`, the stub regex and the game-folder defaults since bootstrap 3).
2. Bootstrap (bootstrap.md, in order): identify the binary (done 2026-10-31: architecture.md
   "Binary and toolchain"; **CodeWarrior, stripped, SDK 3.0, rx + rena middleware, no sister**) ->
   its own Ghidra project (done:
   `C:\Users\willi\Documents\Ghidra Projects\CLaude Decomp.gpr`, `data/ghidra.json`) -> tools from
   RosePan on `ghidra_mcp.py` plus a 5.14.2 client (done 2026-10-31; `tus.py` / `autopilot.py` held
   back for step 7: Decisions) -> **name seeding (bootstrap 4, in progress)** -> TU map -> platform layer ->
   first build and run -> the queue. The scheduled runs do one step each.
   Bootstrap 4 so far (2026-10-31): snapshot `work/snapshots/20261031_084541.json` (6,393 names);
   `rtti.py` rewritten for CodeWarrior (490 vtables, architecture.md "Classes"); the second code island
   is the static-initialiser table (architecture.md "Static initialisers"); **910 function starts
   Ghidra lacks** in `data/missing_functions.json`. **Left for bootstrap 4, in this order:**
   ~~(a) create those functions~~ **done 2026-11-02 (batch 009)**: all 1,016 starts of
   `data/missing_functions.json` + 7 code pointers (`gameMainThread` and the six below) created and
   saved (Ghidra 6,395 -> 7,418 functions; 21 needed a second `create_function` call: ghidra.md),
   the 106 merged rx starts named with their batch 005-008 names, `rtti.py --missing` now finds 0
   (the JSON is `{}`) and `rxalign.py` pairs all six rx ranges one to one (0 entries);
   (b) seed names by reading with `name_ctx.py` batches (`tools/reverse/work/batches/NNN.json`, not
   committed; the ledger `data/context_names.json` is). **Batch 001 done (2026-10-31, 27 names)**:
   the entry chain, `main`, `exit` / `Exit` / `_InitSys`, the CodeWarrior + MSL runtime, the
   exception classes' destructors, operator new / delete and the global heap (architecture.md "Key
   functions and globals"). **Batch 002 done (2026-10-31, 26 names)**: `main`'s thread system,
   `gameInit`, heap init / teardown, libgraph sync and libkernl interrupt helpers (architecture.md
   "Program flow", "Thread system"). `gameMainThread` 0x12db00 failed: no Ghidra function there.
   **Code pointers passed as arguments are not in `data/missing_functions.json`** (it only has
   vtable slots and initialisers): 0x12db00, 0x12db70, 0x12dab0, 0x131840, 0x12dc20 were created
   and named in batch 009 (architecture.md "Program flow"). ~~A pass over lui/addiu pairs feeding
   a0..t1~~ **done 2026-11-02 (batch 010)**: `codeptrs.py` found 39 more starts (callbacks,
   rx memory hooks, SIF cmd handlers, array-element constructors passed to CodeWarrior's array
   new, function tables), all created; rerun gives 0. Left unnamed among them (read at their TU):
   the zero-and-return-this constructors 0x1716c0, 0x18bf20, 0x194910, 0x235390, 0x25f990,
   0x2cd970 (passed in a1 to the array-construct helper: name them with their class),
   0x1398c8 / 0x1398e8 (stored by 0x139848, before the SIF library), 0x140c88 / 0x1412f0 (word
   copy) and 0x140cc0 (word search) near `_InitSys`, 0x127c80 (returns 0), 0x113750 / 0x113898 (`_ch3dmaCSC` / `_ch4dma`, batch 012),
   0x11c928, ~~0x11feb8 / 0x120348 / 0x120750 (libmc callbacks)~~ (batch 020), 0x1395e0, 0x140840, 0x180018 (a lone
   nop in a table at 0x2f0e5c: check), 0x1b56f0, 0x1f22a0, 0x1fbe40..0x1fbf30 (six callbacks of
   0x1fbe20's family), 0x20ad20, 0x20b010, 0x2b5740, 0x2cd8a0. Its 29 `interior` hits are mostly
   constants that look like code addresses, plus **the first island's entry points** 0x2f3d60..
   0x2f4860, passed by `_InitSys`'s callees 0x140330..0x141390 (item (c)).
   **Batch 010 also named the SIF cmd / rpc library and the fileio head from RoR** by size
   alignment (architecture.md "Key functions and globals"; 0x13ac68 / 0x13ad00 named in batch 020).
   **Batch 003 done (2026-10-31, 34 names)**: `gameMainThread`'s and `gameInit`'s callees (pad,
   sound, rx init, SPK.ROM, GS / DMA / SIF / libpad library calls, options; architecture.md
   "Program flow").
   **Batch 004 done (2026-10-31, 52 names + 6 class names in `data/class_names.json`)**: the global
   object is `GameSystem` (ctor 0x12d0d0, `flipFrame` 0x12cdf0) with three pads (`Pad` / `GamePad` /
   `MaskedGamePad`), `GamePacketManager` on rena's `Vif1PktManage`, `PerfMeter` / `RcntTimer`,
   `MasterClock` / `Clock` / `PtrList`, libvifpk, `rxPrintf` (architecture.md "The global object").
   The pad update slots 0x12dc60 `GamePad::update` / 0x12dcb0 `MaskedGamePad::update` were created
   and named in batch 009 (0x130060 stays a thunk). Left unnamed: 0x133610 (empty, returns
   0, also called by 0x198970 / 0x199400: a PerfMeter bar? check those callers), 0x133600 /
   0x132f20 (empty), ~~0x12f6b0 / 0x12f600 (Pad::update's stick / post steps)~~ (batch 023: `Pad::readSticks` / `Pad::updateRepeat`), 0x119b88 (atan2f?),
   0x12d540 / 0x12d5a0 (GamePad ctor / dtor candidates, rtti.py).
   **Batch 005 done (2026-11-02, 253 names)**: main's teardown (memcard / sound / gfx / pad / rx;
   architecture.md "Program flow") and, on the user's tip (2026-11-02: "Rule of Rose had similar
   rx* names"), **the rx library from RosePan**: FS2 links RoR's rx (same functions, order and sizes,
   even `rxThFindCallback`'s bug), so the new `rxalign.py` aligned size sequences against
   `../RosePan/ror_functions.txt` (runs of >= 3) for rxsys 0x100350..0x1037b0, rxthread
   0x166b50..0x168150 and rxfile 0x153108..0x158e80; batch 003's rx guesses were renamed to RoR's
   (`rxThSetPrioIndex`, `rxThSetInterval`, `rxSysInit`, `rxSysSetBootPath`, `rxSysPrintf`,
   `rxThFindCallback`). Snapshot before it: `work/snapshots/20261102_132500.json`.
   **Batch 006 done (2026-11-02, 61 names)**: rx finished where it matches RoR -- rxsys
   0x100350..0x1037b0 + 0x153108..0x153518, rxfile to 0x159928, rxthread to 0x1681c0 (`rxalign.py`
   now fills gaps by span partition and breaks LCS ties by fewest breaks; reruns give 0 entries).
   Fixed batch 005: 0x1533f0..0x153450 were one slot off (now `rxSysGetFileSearchDir`,
   `rxSysPushSearchDir`, `rxSysPopSearchDir`, `rxSysGetLastName`) and 0x157c40 is RoR's
   `rxFileCdSetFindFunc`, not a FS2-only locate hook. **45 more rx starts Ghidra lacks** went into
   `data/missing_functions.json` ("rx from RosePan"; with 4(a); rerun the four `rxalign.py`
   ranges of architecture.md "rx" with `--batch` afterwards to name them).
   **Batch 007 done (2026-11-02, 204 names)**: rxsnd 0x159bc0..0x162178 from RoR (190 aligned,
   5 by reading, 3 FS2-only RPC wrappers) and rxthread's VBlank tail, which FS2 links at
   0x159928..0x159bc0 (architecture.md "rx"); 28 more merged rx starts in
   `data/missing_functions.json`. Snapshot before it: `work/snapshots/20261102_134502.json`.
   **Batch 008 done (2026-11-02, 114 names)**: rxsndut + rxsndstr 0x162178..0x166b50 by reading
   (all 113 Ghidra functions: XWS bank, stream core, PCM / ADPCM formats, stream source, memory
   stream; architecture.md "rx") + `strcmp` 0x128238; 33 merged starts with their names in
   `data/missing_functions.json` ("by reading, batch 008"; with 4(a)). Doubtful names, settle at
   the TU: `rxSndStrBankGetRequestRate` (FORM +10, rate or pitch?), `rxSndStrBankGetMarkParam`
   (MARK +4), `rxSndStrSetFloatParam` (0x163b40, +0x28, dirty bit 0x10 nobody reads),
   `rxSndStrPlayAt` 0x163738 (a bare tail call to `rxSndStrPlayEx`: why does it exist?).
   Snapshot before it: `work/snapshots/20261102_135750.json`.
   **Batch 009 done (2026-11-02, 113 names)**: bootstrap 4(a) above.
   **Batch 010 done (2026-11-02, 37 names)**: the code-pointer pass above.
   **Batch 011 done (2026-11-02, 108 names)**: **SDPR2** 0x1681c0..0x171300 by reading
   (architecture.md "rx", SDPR2; 10 merged starts created first with `ghidra.py mkfunc`). To settle
   at its TUs: `sdpr2PairPause` 0x16d3a8 calls the driver's *stop* slot (the one-voice type calls
   pause): bug or intent; `sdpr2SetSpread` / `sdpr2SetPairMode` / `sdpr2SetSurround` are names from
   what the values do, not from a string; `sdpr2CalcLevels` 0x16fbd0 unread; the A/B and C/D file cuts.
   **Batch 012 done (2026-11-02, 46 names, 15 merged starts created)**: rxMpeg was looked for
   and **is not linked** (RoR's `RxMpeg` is RoR game code; FS2's movie player is its own class at
   ~0x1fa880..~0x1fcbf0, named at its TU: architecture.md "Key functions and globals"); on the way
   **SDK 3.0 libmpeg 0x10c218..0x116978 and libipu 0x116978..0x116fb8 from RoR** (decoder members
   linked before mpeg.c, RoR after). Left unnamed there: 0x10c218, 0x111228, the accessors
   0x1123a0..0x112400 (DispWidth / Height / CenterOff?), RoR's own `FUN_` pairs (read at the TU).
   `rxalign.py` now skips RoR placeholders in `--batch` and takes `--lib`.
   **Batch 013 done (2026-11-02, 93 names + 3 class names)**: the game's sound layer
   0x144080..0x148d60 by reading (`SoundStream`, `SoundSink` / `SoundPcmSink` / `SoundAdpcmSink`,
   `SoundFeeder`, `SoundSeBank`, `SoundConfig`, the rx IOP / SPU memory hooks, `soundThreadLock`;
   architecture.md "The game's sound layer") + SDK iopheap 0x13ed90..0x13ef08 from RoR. To settle at
   the TU: the parameter order of `SoundSeBank::setPanPitchF` / `setPitch` (a float in f12 beside the
   object), output mode 0 = mono?, `SoundStream::freeBuffer`'s owned test (decompiles inverted), the
   class at 0x142e70..0x144040 (vptr +0x808, vtables 0x311100 / 0x311120) before it.
   **Batches 014-015 done (2026-11-02, 111 names, 14 merged starts created)**: `_InitSys`'s nine
   callees and the whole libkernl tail 0x13ef80..0x142dd0 from RoR (`rxalign.py 13ef80 142dd0
   2658c8 269710`, one to one), RoR's 14 placeholders there from FF3's list (FF2 symbols), the
   per-file `Copy` / `kCopy` pairs and TLB syscall stubs by reading (architecture.md "Key functions
   and globals", SDK). Doubts: the TLB stub names 0x140538..0x1405e8 follow ps2sdk's syscall
   numbers 0x55..0x59 (`ExpandScratchPad` 0x59 least sure); 0x141198 (`j InitTLB`, called by
   every exit.c entry) keeps Ghidra's thunk name; 0x142dd0 / 0x142df0 after the reboot-notify run
   are unread.
   **Batch 016 done (2026-11-02, 139 names, 75 starts created)**: libkernl's front 0x1366c0..
   0x139bc0 (klib.s by slot from RoR, glue / cache / intr / thread / deci2 / tty / kprintf /
   sifcmd head from RoR + FF3; architecture.md "Key functions and globals", SDK). Doubts: the
   seven klib.s slots RoR merged (0x55..0x59, 0x5b, 0x74) carry ps2sdk names; 0x1371c8 /
   0x1371d0 (glue, return -1) unnamed; `lseek` is FF3's low-confidence name; the kprintf.c
   helpers (`_kvprintf`, `bufferPutchar`, `deci2Output`, `serialOutput`, `kprintfSub`,
   `deci2Vprintf`, `kprintf`) and `getpid` / `sceSifGetDataTable` are names by reading, Sony's
   own unknown (RoR and FF3 have them unnamed); 0x123528 (glue's errno setter?) and the VU0
   math run 0x135a90..0x1366bc before klib.s are unread.
   **Batch 017 done (2026-11-02, 38 names)**: libkernl's fileio 0x13b7e8..0x13ed90 from RoR, one
   to one, every `SceStdio*Sema` string agreeing (+ `_fs_version`, `sceLseek64`, `get_iob` from
   FF3's FF2 symbols; architecture.md "Key functions and globals", SDK). Doubts: `_sceFsCallRpc` /
   `_sceFsCountOpenIob` are names by reading (Sony's own unknown); 0x13bc38 (rpc 0xfe) / 0x13bd78
   (rpc 0xfd) left unnamed (no callers, unnamed in RoR and FF3).
   **Batch 018 done (2026-11-02, 42 names, 7 merged starts created)**: libgraph / libdma / libdev
   0x1076e8..0x10ac60 from RoR (38 aligned, 4 by reading: `sceGszbufaddr`, `sceGsPutDrawEnv`,
   `sceGsSetDefTexEnv`, `sceDevVif1GetCnd`; architecture.md "Key functions and globals", SDK).
   Doubts: 0x109570 (RoR's unnamed 0x224) and RoR's unnamed libdev slots stay `FUN_`.
   **Batch 019 done (2026-11-02, 29 names, 1 merged start created)**: libcdvd 0x11c6b0..0x11e440
   from RoR (25 aligned, `_sceCdSC` 0x11d430 split off; `_sceCd_ncmd_prechk` / `sceCdNcmdDiskReady`
   from FF3's FF2-symbol list, `sceCdChgSys` from its string, `_sceCdGetDiskType` by reading;
   architecture.md "Key functions and globals", SDK). Doubts: `_sceCdGetDiskType` is a made-up
   internal name; 0x11dff0 (S-command RPC 0x25, two words, no callers) left unnamed; 0x11b800..
   0x11c6b0 before libcdvd is an unread cluster (not libcdvd). Snapshot before it:
   `work/snapshots/20261102_164802.json`.
   **Batch 020 done (2026-11-02, 35 names, 1 merged start created)**: libmc 0x11fb70..0x121b80
   (`rxalign.py 11fb70 121470 24d220 24eb28`: 19 aligned once the range ran to `sceMcGetEntSpace`
   0x121360, `sceMcChdir` 0x120b78 split off `mceStorePwd`; `sceMcSeek` / `mceReadFixAlign` /
   `mceSignalSema` and FS2's read-fast part 0x121470..0x121b80 by reading) + `sceSifSetRpcQueue`
   0x13ac68 / `sceSifRegisterRpc` 0x13ad00 (batch 010's open pair) + `memcpy` 0x125228
   (architecture.md "Key functions and globals", SDK). Doubts: the `mce*` internals are names by
   reading (Sony's own unknown); `sceMcReadFast` comes from the thread string, `sceMcWriteFast`
   (rpc 0x1b) by analogy only; 0x121b80 (error-code mapper, called only by the game's memory-card
   layer 0x2ac710..) left unnamed: libmc or game code?
   **Batch 021 done (2026-11-02, 23 names)**: libpad 0x11e440..0x11fb70 one to one with FF3's
   list (`../ReiPan/ff3_functions.txt` 0x283a28..0x285158: same sizes and order, FF3 merged
   PortOpen + PortClose; 14 names FF2 symbols, 6 FF3 names checked by reading) + three internals
   by reading (architecture.md "Key functions and globals", SDK). Doubts: `_scePadCmdHandler`,
   `_scePadSendActData`, `_scePadSetCallback` are made-up names; `rxalign.py` only reads RoR's
   list, so this alignment was done by hand (a `--ref` option for FF3's list would make the next
   one a command).
   **Next batches, in this order**: libc near strlen / strcpy (0x1252d8 memmove?; 0x106c00 / 0x106b80 = MSL's refcounted
   string rep); SDK functions from their `PsIIlib` / assert strings; rx (`rxfile.c`); rena
   (`rena_*.cpp`). Thunks 0x100218 (`_exit`) and 0x100270 keep Ghidra's thunk names (renaming a
   thunk may rename its target);
   (c) the first island ~0x2f3a40..0x2f4d20 (9 Ghidra functions, nothing references 0x2f3a40):
   **kernel patches the SDK installs** (batch 014: `InitAlarm` copies 0x2f4860 + 0x740 to kernel
   0x80076000; `InitExecPS2` / `InitTLBFunctions` / `InitSystemCallTableAddress` likely copy the
   rest): library code never run on the EE side, name it after those installers;
   (d) `libmatch.py` once the reference ELFs are here (items 3-4).
3. **The user (2026-11-02, in progress): Rule of Rose in Ghidra.** The user is loading RoR into
   Ghidra so its names can be re-applied there and compared with FS2 (BinDiff crashes on PS2
   programs, so not that). **2026-11-02: `SLUS_214.48` is in YamiPan's Ghidra project** (closed;
   `/mcp/instance_info` lists it): with 7.0.0 one server serves both once it is open, and
   `get_function_hash` gives opcode hashes for both programs (a cheap first compare). Once RoR answers or its
   ELF `SLUS_214.48` sits next to `../RosePan/ror_functions.txt`: **verify batch 005's rx names word
   for word** (libmatch.py's masked compare, RoR as the reference; a mismatch -> back to reading) and
   extend the match to the rest of rx and to the SDK 3.0 libraries both games link.
4. **The user: reference ELFs for libmatch.py.** The sibling checkouts on this machine have the
   function lists (`../ReiPan/ff3_functions.txt`, `../RosePan/ror_functions.txt`) but not the ELFs
   (`../ReiPan/SLES_538.25`, `../MioPan/SLES_523.84` with `symbols.txt` / `functions.txt`,
   `../RosePan/SLUS_214.48`); `libmatch.py` skips a reference whose ELF is missing (0 matches,
   2026-10-31). Copy them next to their lists (never committed) to match FS2's SDK 3.0 libraries.
5. **The user: a host toolchain (2026-11-02: MSYS2 installed, g++ only).** `C:\msys64\ucrt64\bin` has
   g++ / gcc 16.2.0 (Rev4) but no cmake, ninja or gdb, and it is not on PATH; set `YAMIPAN_MINGW_BIN`
   to it once `pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
   mingw-w64-ucrt-x86_64-gdb` has run. Earlier (2026-10-31): no MinGW g++, ninja or cmake on this machine (RosePan's paths,
   CLion's MinGW and Python's ninja, do not exist here; 2026-10-31). `build.py` / `layoutcheck.py` take
   them from PATH or `YAMIPAN_MINGW_BIN` / `YAMIPAN_NINJA`; bootstrap 6-7 need them.
6. ~~Install the GhidraMCP 7.0.0 built for Ghidra 12.2~~ **Done 2026-11-02** (the user installed
   the build from `../ghidra-mcp`, build 20261102): decompile, function at an address, tags and save
   all answer. Ghidra's function tags hold only 0x162250's provenance tag so far; the other 760 in
   `data/tags.json` are mirrored when a batch next tags them (or by a one-off loop over the file).
7. Fill architecture.md as each step learns something.
8. **The Pan layer (bootstrap 6-7), started 2026-11-03 by the user; first milestone reached the same
   day**: `CMakeLists.txt` exists, so `tus.py check` and `finish` are in full mode from here on. `build.py
   --run` is green: the window opens on D3D12, `gameInit` runs through, the frame loop and the job worker
   turn, stubs are reported. Written: `core/` (Application, Logger, Paths, Config, IniFile, Window,
   EventQueue), `os/Thread`, `ee/` (Kernel, InterruptController), `gpu/Device`, `rendering/`
   (DisplayGeometry, UiCanvas, Renderer, PresentPass, Screenshotter), `io/` (InputSystem, HostFiles),
   `ui/` (Overlay, four panels, the Siren theme), `src/sdk/` (eekernel, libgraph, libdma, libvifpk, libpad,
   libcdvd, fileio, sif). **Left, in this order** (each ends in a picture or a sound):
   ~~(a) the game-layer boot~~ **done 2026-11-03**: `game/spkrom.cpp` and `rx/filecd.c` written (`verify`)
       and `io::Disc` serves the data folder as the disc (architecture.md "PC platform layer"). A smoke
       run reads SPK.ROM through rx's own ISO code and walks `loadGamePack` -> `PackTables` ->
       `loadMenuPack` -> `menuSceneThread`: **the frontier is now the reached stubs, the TU loop's job**
       (`rx/filehost.c` and `filehdd.c` stay stubs: FS2 never opens those devices);
   (b) `rena/rena_2d_texture_tim2.cpp`, then `TextureStore` behind `YamiPan_DrawUiTexture` (TIM2 decoded
       at the source): nothing textured draws until then;
   (c) any aspect: the renderer hands k to the game's UI matrices (`kUiScreenMatrix`, see TU 19), the one
       place a game file needs a `PC:` touch;
   (d) placeholders to replace: `sdk/libmc.cpp` (the empty slot: a real card under `memcard/`),
       `sdk/libmpeg.cpp` (a movie reads as ended: PSS demux + FFmpeg), `sdk/sif.cpp` (no IOP: the audio
       milestone brings RosePan's IOP host and `iop/rxsnddrv`);
   (e) HDR in `PresentPass` (settings are there, the swapchain stays SDR), MSAA, anisotropy;
   (f) re-enable the scheduled task: the user's decision.

## Decisions

- 2026-11-03 (the user): **"Instead of using C style CPP, I want everything now to be C++, which
  means object oriented and more modern pragmatism. The basis should be the same like the window and
  ui and so on, but adapt it to FS2 and make it C++"**, then, the same day, **"I want the equivalent of
  the pan layer to be in C++ not necessarily the game layer, that should stay authentic."** Bootstrap
  6 starts now, on the user's word, after the scheduled task (`yamipan-next-tu`, disabled by the user)
  finished its last run (01b665f). `references/cpp-port.md` has what this means; it supersedes rule
  (4) of 2026-11-02 ("renamed, not rewritten") for the Pan layer. **The game layer is not touched**:
  `src/game`, `rena`, `sound`, `rx`, `sdpr2` keep their code, shapes and the game's file names
  (`rx/rxfile.c`, built as C++ like every TU in RosePan). Session choices inside that, for the user to
  overrule: SCE's public names stay (`src/sdk/` is the game's API; only what sits behind it is
  classes); the SDK carried over is what FS2 calls (about 100 `sce*` functions and 35 kernel services,
  against RosePan's 14 K lines); the game-facing seams game code already includes
  (`yamipan_drawui.h`, `yamipan_time.h`, `yamipan_memory.h`) keep their `YamiPan_*` functions as thin
  forwarders, so no game file changes; **EE threads run one at a time by priority** (settles the
  2026-11-02 question (b) on game/thread.cpp); **a render thread owns the GPU** (RosePan renders on the
  game thread): FS2 flips from its frame-sync thread, and this keeps the overlay alive through loads.

- 2026-11-03 (scheduled run, rena/rena_pkt_vif1pktmanage.cpp; for the user to overrule): **the
  frame's DMA chain stays built, with buffer offsets.** The packet buffers keep the ROM's tags (so
  used size, overflow and the layout of what other code writes stay the ROM's), but a tag's ADDR
  holds the target's byte offset in the current frame buffer instead of an EE address (host pointers
  do not fit 32 bits). `Kick` does not touch DMAC registers: it hands the lists holding draws, in
  chain order, to a new host call `YamiPan_DrawUiKick(lists, count)` (declared in
  `yamipan/rendering/yamipan_drawui.h`, implemented at bootstrap 6), whose `list` pointers are the
  same `sceVif1Packet *` the draw calls name. `rena/` is a new top-level directory under `src/` for
  rena's own files (keys keep the game's file names).

- 2026-11-02 (the user): **four rules for all code** (SKILL.md "The user's rules"): (1) no GS
  emulator, not even for textures (MioPan's `miopan_gs.cpp` reimplements the GS for textures: do not
  repeat it; decode assets at their source so the GS path is not needed); (2) name repeated values and
  state / flag bits as enums, `#define`s or `constexpr` while decompiling; (3) **arbitrary aspect ratio
  is the port's #1 feature**; (4) reuse MioPan / ReiPan / RosePan abstractions instead of
  reimplementing the basics.

- 2026-11-02 (the user): **source code first.** Stop seeding library names; reconstruct game code
  and name as each TU needs it ("Next up" item 0). `tus.py` comes over before the build exists, in a
  source-only mode; supersedes the 2026-10-31 decision to hold it for bootstrap 7.

- 2026-11-02 (the user): **bethington's GhidraMCP 7.0.0 replaces LaurieWired's 2.0.0** (the user
  installed it; both are `Extensions\GhidraMCP`, so it took 2.0.0's place) and **the tools were ported
  to it** ("Port tools to 7.0.0"): `ghidra_mcp.py` finds the one server per process by the open program
  and project on 8089..8104 and passes `program=`; `yamipan.py` / `ghidra.py` / `fetch.py` use its
  endpoints (ghidra.md); saving and function creation now work from the tools; the `ghidra` MCP server
  is the plugin's own bridge from `../ghidra-mcp` (`ghidra_bridge.py`). Supersedes the 2026-10-31
  decisions on 2.0.0 and on 5.14.2 beside it.
- 2026-10-31 (session decision, bootstrap 3; **superseded 2026-11-02 for `tus.py`**, which came over
  in source-only mode; `autopilot.py` still waits): **`tus.py` (and `autopilot.py`,
  which needs it) are carried over at bootstrap 7, not 3**: the routine switches to the TU loop as soon
  as `tools/reverse/tus.py` exists, and with no TU map, platform layer or build that loop would claim
  nothing useful. `status.py` / `tuid.py` import it lazily.
- 2026-10-31 (session decision, bootstrap 3): **provenance tags live in `data/tags.json`** (address ->
  tag, committed), mirrored to Ghidra's function tags only while 5.14.2 answers: 2.0.0 has no tags, and
  the repo record survives a Ghidra crash. `fetch.py` reads it; `name_ctx.py` / `libmatch.py` write it
  through `yamipan.set_tag`.

The user's standing rules, carried from the earlier ports (SKILL.md "The user's rules" says how they
apply here):

- 2026-10-31 (the user): **scheduled runs every 20 minutes**: the desktop app's task `yamipan-next-tu`
  (cron `*/20 * * * *`, prompt = `tools/reverse/routine.md`) advances the port by one step per run --
  a bootstrap step until `tus.py` exists, then one TU. `tools/reverse/preflight.py` gates each run
  (stop switch `work/STOP`, run lock `work/run.lock`, clean tree, work branch, Ghidra up), so a run
  still going makes the next one stop at once.
- 2026-10-31 (the user): work happens on the work branch **`fs2-port`** (on origin; `main` stays at
  the Ghidra switch). **Pushing each run's commit to origin/fs2-port is approved**; never force.
- 2026-10-31 (the user): the annotation tag is **`FS2`** (`// FS2 0x...`).
- 2026-10-31 (the user): GhidraMCP 2.0.0's gaps are covered by **bethington's GhidraMCP 5.14.2
  installed beside it** (the user installs it). The carried-over tools keep RosePan's 5.14.2 calls for
  what 2.0.0 lacks -- `save_program`, `create_function`, `disassemble_bytes`, the function-tag
  endpoints, `get_bulk_function_hashes` -- through a 5.14.2 client on its own ports (8089..8104,
  `program=` in the query string, as before 2026-10-31: `git show 8536a1f:tools/reverse/ghidra_mcp.py`);
  everything else and Claude's MCP tools go through 2.0.0.
- 2026-10-31 (the user): Ghidra 12.2 with **LaurieWired's GhidraMCP 2.0.0** replaces Ghidra 12.1.2
  with bethington's 5.14.2, on a new machine. YamiPan's project is
  `C:\Users\willi\Documents\Ghidra Projects\CLaude Decomp.gpr` (program `SCES_538.51`); the game's
  files moved to `C:\Users\willi\Documents\Decomp\Forbidden Siren 2`. The tools, the bridge and
  `.mcp.json` were switched the same day (ghidra.md).
- 2026-10-30 (the user): YamiPan ports **Forbidden Siren 2 (Europe, `SCES_538.51`)**. The game's
  data and ELF are in `C:\Users\willi\Documents\Decomp\Forbidden Siren 2` (since 2026-10-31):
  - `SCES_538.51`;
  - `ROM.000`..`ROM.007`, `ROMD.008`..`ROMD.010` and `SPK.ROM`;
  - `MODULES\`, `IOPCONF.CNF` and `SYSTEM.CNF`.

  The tools read the ELF and the data from there; nothing is copied into the repo.
- 2026-10-30 (the user): YamiPan is the next PS2 port; its skill carries ReiPan's and RosePan's
  learning. First step: Ghidra working with several instances (done the same day, ghidra.md).
- 2026-10-30 (session decision; updated 2026-10-31): one Ghidra project and process per game. With
  GhidraMCP 2.0.0 a backend is found by the program active in its window (`/health` on 8080..8095,
  `ghidra_mcp.py`), not by port, and every request checks it first.
- 2026-10-28 (the user, RosePan): the PC port displays correctly at any aspect ratio, read from one
  renderer-owned source; never a fixed 4:3 / 640x448 assumption in new host draw code.
- 2026-10-22 (the user, RosePan): drawing never goes through the emulated GS; the game's draw decisions
  are reconstructed and the packet-writing step calls a native host draw module per subsystem.
- 2026-10-15 (the user, ReiPan): methods, not free functions -- a function taking the object in a0 is a
  member of its class.
- ReiPan / RosePan (the user): the picture is the test; a drawing TU is `done` only when the screenshot
  matches the game's own assets; complete code nothing can show yet is `verify`.
- ReiPan / RosePan (the user): automated porting runs at effort xhigh or above; sessions never start
  other sessions, agents or scheduled tasks, and never edit `autopilot.py` or the routine.

## Open questions
- 2026-11-03 (scheduled run, game/gamepacket.cpp): `GamePacketManager` +0x34 (skip the next
  `Kick`) is only cleared in the ELF (ctor, `kickAndSwap`); no `lbu`/`sb` at GameSystem +0x4a0. Who
  sets it (a pointer to `gGameSystem->m_packets`, e.g. a loading or movie path)? Until found, the
  port never skips a frame's chain. Also: `closeList` leaves `IsOverflow`'s bool in v0; the 26
  callers were not all checked for reading it (screen.cpp and gamesystem.cpp ignore it).

- 2026-11-03 (scheduled run, game/scene.cpp): `tus.py next` picked unassigned@00171300 (0xc48d8
  bytes, 2913 functions: the big actor-class run) for its frontier, but four of its six frontier
  entries (0x193780 / 0x1937f0 / 0x193880 / 0x1938f0, called from game/boxgrid.cpp) are MSL
  `vector<vector<BoxList>>` destructor instances that boxgrid.cpp already gets from the host
  `<vector>`: they need no code (callcheck should ignore them, or they should be marked as
  host-provided). The session cut out game/scene.cpp (gameSceneThread) instead; boxContainsPoint
  0x2362a0 (a VU0 helper) is still in that run. Open: how `tus.py` should mark linkonce MSL
  instances so they leave the frontier.

- **(2026-11-02, game/thread.cpp) the thread system on the host.** (a) `RotateThreadReadyQueue` is not in
  RosePan's `sdk/eekernel.h`: bootstrap 6 must add it (g++ -fsyntax-only fails only on it). (b) The
  slots are cooperative only because the EE runs one thread at a time by priority (slots 0x30 above
  the scheduler 0x40; frame sync 0x10; job worker 0x50 spinning on an empty list): host threads run in
  parallel, so the platform layer needs a priority-run model (one runnable game thread at a time) or
  fibers, and the worker's spin must not burn a core. Decide at bootstrap 6. (c) Nothing in the ELF
  sets the quit byte 0x31adbc, so `threadRunMain` never returns on the console (main's teardown is dead
  code there); the port can set it for a clean exit. (d) Nothing pushes the scheduler detour stack
  0x4c8170 or sets the pre-switch hook 0x31adcc (gp offsets and lui pairs scanned): debug leftovers?
  (e) `bitMask` 0x130a20 is a generic helper, not thread code: it may end `game/pad.cpp` instead.
- **(2026-11-02, TU 7) memcard semantics guessed.** `memcardRequiredKB` (returns 80, settled: stepScan
  compares it with sceMcGetInfo's free clusters, 1 KB each; now `kSaveRequiredKB`);
  the byte 0x31ae84 (`memcardGetFlag` and kin, set by memcardInit: meaning unknown); `SaveStatus_Denied`
  (GetDir mapped result 5, from libmc -6); `Memcard::m_iconAux` (getSaveResourceAux(0), never written
  by stepSave: a size?); `MemcardRequest_Save7` (no starter in the file: who sets request 7?);
  `mcErrorCode` 0x121b80 may be library code (it sits between libmc and libc). The SaveFile byte at
  GameOptions +0x4d decides whether a load keeps its stamp: name it when the options menu is read.
  Second run: `stepCheckAll`'s open loop returns the starting file's read step even when it opened
  a later save (that save is read into the earlier SaveFile): kept as the ROM does it; if the port
  ever shows a wrong save list, this is why. What `SaveData::unlock468` unlocks (the first-game
  bonus) is for the TU that reads it (0x2b9710 and the title menu, in `unassigned@002acff0`).

- **(2026-11-02, bootstrap 5) TU keys and guessed cuts.** rx / SDPR2 keys end in `.c` because FS2's
  own strings say `rxfile.c` / `filehost.c`; RosePan reconstructed the same rx as `rx/*.cpp`. If the
  reconstruction reuses RosePan's C++ as is, rename the key with `tus.py define` (the extent stays).
  Guessed cuts to settle at their TUs: `rx/filecd.c` / `rx/filehdd.c` as own files, the six
  `rx/rxsndstr*.c` layers as six files, SDPR2's A/B and C/D cuts, every `game/*.cpp` name and both
  ends of `game/thread.cpp`, `game/perfmeter.cpp`, `game/gfxtarget.cpp`; `sound/sound.cpp`
  0x144080..0x148db0 is likely several files. Unread runs left as game code, not library:
  architecture.md "Address map" (`Heap` 0x1037c0..0x104b40: game code, settled 2026-11-02, TU 8).

- ~~Which game; engine kinship with FF2 / FF3 / RoR?~~ Settled 2026-10-31 (bootstrap 1): none (but the
  rx library is RoR's: 2026-11-02, batch 005); SCE's
  own rx runtime and rena renderer, built with Metrowerks CodeWarrior 2.4.1.01.
- **CodeWarrior, not GCC** (bootstrap 1, 2026-10-31). Consequences for bootstrap 3-4, to settle there:
  `sections.py` / `gcc2.py` have nothing to read (stripped, no linkonce sections); `rtti.py` needs a
  CodeWarrior variant (vtable `{type_info*, offset, fn...}` 4-byte entries, game classes with null
  type_info, vptr after the data in at least one class: architecture.md); `libmatch.py` can match
  Sony's prebuilt SDK 3.0 libraries (FF3 / RoR references) but not MSL C++ or CodeWarrior's runtime
  (no reference built with it); reconstruction.md needs a CodeWarrior ABI section (calling convention
  is the EABI / o32-like one GCC uses on the EE? 64-bit `long`? struct return and member-pointer
  layout?) before the first class TU. The host build stays MinGW / MSVC either way.
- A symbolised sister build? Forbidden Siren 1, the Japanese / US FS2 releases or a demo disc might
  keep `.mdebug` or a map; none is on this machine. Ask the user before bootstrap 4 if one exists.
- Two EE code islands outside .text: ~0x2f3a40..~0x2f4d20 (after the VU microcode) and
  ~0x310a00..~0x310f80. The second is settled (2026-10-31): static initialisers, table 0x310fb0 run by
  0x100280 (architecture.md). The first is still open (nothing references 0x2f3a40; kernel-patch or
  IOP-reboot code?).
- Multiple vptrs in one object, every secondary table with this_offset 0 (architecture.md "Classes"):
  embedded members or bases? Settle at the first TU of the +0xf4 family (ctor 0x171a30).
- ~~Can 5.14.2 sit beside 2.0.0?~~ Settled 2026-11-02: no (same extension name); bethington's 7.0.0
  replaced 2.0.0 and the tools moved to it (Decisions).
- **`tools/reverse/routine.md` step 4** still says "GhidraMCP 2.0.0 never saves ... save through the
  GhidraMCP 5.14.2 plugin's save_program": the tools now save through 7.0.0 themselves. The routine
  is the user's to edit (sessions never touch it); a run that saw `name_ctx.py` save need not report
  "Ctrl+S".
- RosePan's and ReiPan's own Ghidra tools (`rosepan.py`, `reipan.py`, pinned to 8089 and 5.14.2) do
  not run on this machine: their checkouts are read-only references here (cloned 2026-10-31).
- Parallel sessions on one game (several worktrees, one Ghidra): reads are fine, naming batches race
  (ghidra.md "Working with several"). Decide whether only one session per game names at a time, or the
  ledger learns to merge.

## Done

- 2026-11-02: TU 1 `game/gamesystem.cpp` (scheduled run): all 17 functions reconstructed into
  `src/game/gamesystem.cpp` / `.h` with minimal headers for the callee TUs, naming batch 022 (18
  names, saved by the tools), source-only check green (callcheck clean, layout lint OK); status
  `verify` (not built). Next: the next TU `tus.py next` picks.

- 2026-11-02: "Next up" 0(b) (scheduled run): `tus.py` carried over from RosePan with a source-only
  mode (no build: `check` = callcheck + layout lint, `finish` refuses `done`, records "not built");
  README, SKILL.md, bootstrap.md updated, status.md regenerated. Next: 0(c), the first TU
  (`game/gamesystem.cpp`).

- 2026-11-02: bootstrap 4, part 16 (scheduled run): naming batch 016 (139 names: libkernl's front,
  klib.s by slot from RoR, glue / cache / intr / thread / deci2 / tty / kprintf / sifcmd head from
  RoR + FF3), 75 function starts created, saved by the tools. Next: the other SDK 3.0 libraries
  (libgraph, libdma, libpad, libmc, libcdvd) by `rxalign.py`.

- 2026-11-02: bootstrap 4, part 14 (scheduled run): naming batch 013 (93 names: the game's sound
  layer by reading, SDK iopheap from RoR; 3 sink classes in `data/class_names.json`), saved by the
  tools. Next: `_InitSys`'s nine callees, then the SDK libraries by `rxalign.py`.

- 2026-11-02: bootstrap 4, part 11 (scheduled run): the code-pointer pass, new `codeptrs.py`
  (39 starts created, saved by the tools) and naming batch 010 (37 names: SIF cmd / rpc + fileio
  head from RoR by size alignment, `_root`, `rxSysNoMemFunc`, the rx memory hooks). Next: SDPR2,
  then rxMpeg.

- 2026-11-02: bootstrap 4, part 10 (scheduled run): 4(a), 1,023 missing function starts created in
  Ghidra (saved by the tools: no Ctrl+S needed) and naming batch 009 (113 names: the merged rx
  starts, the main loop's callbacks, the pad update slots); `yamipan.create_function` retries a
  call that answered without creating. Next: the code-pointer pass, then SDPR2, rxMpeg.

- 2026-11-02: bootstrap 4, part 9 (scheduled run): naming batch 008 (114 names: rxsndut +
  rxsndstr by reading, `strcmp`; 33 merged starts listed). Ghidra had unsaved changes (Ctrl+S).
  Next: SDPR2, then rxMpeg.

- 2026-11-02: bootstrap 4, part 8 (scheduled run): naming batch 007 (204 rx names: rxsnd from
  RosePan and rxthread's VBlank tail, which FS2 links before rxsnd; 28 merged starts listed).
  Ghidra had unsaved changes (Ctrl+S). Next: rxsndut + rxsndstr, SDPR2 (by reading).

- 2026-11-02: bootstrap 4, part 7 (scheduled run): naming batch 006 (61 rx names: the gaps and
  rxfile's tail from RosePan, 7 by reading, 5 fixes of batch 005), `rxalign.py` span partition and
  tie-breaking, 45 merged rx starts listed. Ghidra had unsaved changes (Ctrl+S). Next: rxsnd.

- 2026-11-02: bootstrap 4, part 6 (scheduled run): naming batch 005 (253 names: main's teardown,
  libmc / libpad / MSL bits, the SPK.ROM hooks, and 231 rx names from RosePan via the new
  `rxalign.py`). Ghidra had unsaved changes (Ctrl+S). Next: the rest of rx ("Next up").

- 2026-10-31: bootstrap 4, part 5 (scheduled run): naming batch 004 (52 names: `GameSystem` and
  its members, rena `Vif1PktManage`, libvifpk, clocks, debug meter, `rxPrintf`) and
  `data/class_names.json` (6 vtables). Ghidra had unsaved changes (Ctrl+S). Next: bootstrap 4(b)
  batch 005, main's teardown ("Next up").

- 2026-10-31: bootstrap 4, part 4 (scheduled run): naming batch 003 (34 names: libpad / libgraph /
  libdma / SIF, pad and sound systems, rx init and `spkRomLoad`, `gsClearVram`,
  `gfxInitRenderTargets`, options and system data). Ghidra had unsaved changes (Ctrl+S). Next:
  bootstrap 4(b) batch 004, the global object's class ("Next up").

- 2026-10-31: bootstrap 4, part 3 (scheduled run): naming batch 002 (26 names: the 8-slot thread
  scheduler and frame pacing, `gameInit`, heap init / teardown, libgraph / libkernl helpers,
  `_malloc_r` / `_free_r`). 0x12db00 needs a function first. Ghidra had unsaved changes (Ctrl+S).
  Next: bootstrap 4(b) batch 003 ("Next up").

- 2026-10-31: bootstrap 4, part 2 (scheduled run): naming batch 001 by reading (27 names: crt0 /
  libc / libkernl, CodeWarrior + MSL runtime, `main`, the heap), `data/context_names.json` and
  `data/tags.json` started. 4(a) still waits for 5.14.2. Ghidra had unsaved changes (Ctrl+S).
  Next: bootstrap 4(b) batch 002 ("Next up").

- 2026-10-31: bootstrap 4, part 1 (scheduled run): name snapshot, CodeWarrior `rtti.py` (490
  vtables, `--missing`), static-initialiser table found, 910 missing function starts listed. Next:
  bootstrap 4 (a)-(d) in "Next up".

- 2026-10-31: bootstrap 3, the tools (scheduled run): RosePan's tools carried over and renamed
  (`yamipan.py` on `ghidra_mcp.py`, a `Ghidra5` client for 5.14.2, `data/tags.json`, `data/layout.json`
  image sections for the stripped ELF, `ghidra.py find` over the ELF, `mipsdis.py` fallback for
  disassembly); `refresh.py` green against Ghidra (6,393 functions, `yamipan_functions.txt`,
  `status.md`). `tus.py` / `autopilot.py` wait for bootstrap 7. Next: bootstrap 4 (name seeding).

- 2026-10-31: bootstrap 1, the binary identified (scheduled run): stripped CodeWarrior 2.4.1.01 ELF,
  SDK 3.0 libraries, rx / rena middleware, gp 0x322870, `main` 0x12d910, address map with the VU
  microcode at 0x2ec900 (architecture.md). Next: bootstrap 3 (tools from RosePan).
- 2026-10-31: scheduled runs -- the work branch `fs2-port` (pushed), `tools/reverse/routine.md`,
  `tools/reverse/preflight.py`, the desktop app's task `yamipan-next-tu` every 20 minutes.
- 2026-10-31: the Ghidra layer on GhidraMCP 2.0.0 -- `ghidra_mcp.py` (finds the window by its active
  program via `/health` on 8080..8095, checks it before every request, parsed `data` and
  `GhidraRequestError`), `ghidra_bridge.py` (the wheel's server around that check, plus
  `list_instances` / `connect_instance`), `.mcp.json` (the venv beside Ghidra 12.2),
  `data/ghidra.json`. Tested live: every endpoint, a stdio MCP session (32 tools), the check refusing
  a write when the program was not active.
- 2026-10-30: multi-instance Ghidra -- `tools/reverse/ghidra_mcp.py` (discovery + client + CLI),
  `tools/reverse/ghidra_bridge.py` (the MCP bridge, instance-aware), `.mcp.json`; tested with two live
  Ghidras (the second bound 8090 by itself).
