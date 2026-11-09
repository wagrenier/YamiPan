# Open items -- priorities, decisions, cross-TU questions, done log

The user's priorities first ("Next up"), then decisions with their date and who made them, then open
questions. Resolve items here when they are settled (move them to "Done" with the commit).

**The debug menu can teleport anywhere: F2 in the game, or `YAMIPAN_TELEPORT` at boot (the user, 2026-11-06).** Temporary "New Game forced to stage N" builds are no longer needed: `--env YAMIPAN_TELEPORT=7` (stage 7), `7.2` (its part 2), `d0705`, `sd_MT04_08` or `ev0102` (an event; any case), in 5 to 15 s with no pad. architecture.md "Teleport". **Seen, 2026-11-06:** d0705, d0103, d0205, d0401, d0801, ev0102 and a run of sd_MT04_08 reached and played; left from a stage, a running event, a film and the navigator; a movie ending into the navigator with its node off the saved route; Skip off plays the stage's opening movie. **Events now run from their trigger (the user, 2026-11-06):** the player goes on what starts the event and that script runs from the block holding it, so what it does after the event runs too (architecture.md "Teleport"). **Swept with a temporary harness, removed: all 322 events reached.** 289 started by their own script, 20 directly (first-frame 0x1f2 / 0x155, brain scripts, no script), 13 movies; 9 s each over 5 game instances, 55 of them again for 50 s. Nothing fired twice, no player fell (lowest z -898, a script's own marker); on 20 triggers the probe found no floor and the player stayed. Seen: after d0103 the boy stands by the stone wall in the rain, where the story leaves him, and plays on. **The sweep found two bugs.** Natives `hideLoose` 0xa8 and `forEachNamed` read through a null `findAll` list, fixed. **The NaN is fixed (a scheduled run, 2026-11-06):** it never came from the enemy. `CharacterGround::meetWall` took `sqrtf` of 1 - dot x dot where the ROM has `sqrt.s`, which takes the absolute value; two faces dead against each other give a dot just past -1, and the NaN went into the position (hazards.md "sqrt.s is not sqrtf"). **Not tried:** real keys and mouse (a background window gets no keyboard focus, so a temporary hook fed ImGui itself), and whether events play right, beyond the handful looked at: the sweep proves they start, not that they look right. **Doubt for the user:** what a stage's earlier steps would have set stays unset, so a late event can run in a stage that is not quite ready for it; the panel's "Run events from their trigger" turns all this off.

**The NaN is gone from the game's logic (the user, 2026-11-06, "Yes, fix it").** The player's NaN under an enemy attack had already gone by 7e34f39 (`meetWall`, hazards.md "sqrt.s is not sqrtf"). Checked by trapping every invalid float on the stage thread over all 144 file events: no fault left in game logic, no crash; the six sites it found are fixed and listed in hazards.md. **The stage thread is clean:** the same trap over all 309 file and scripted events, 9 s each: 0 faults, 0 crashes. The last three, `litColor`, `addPointLight` and `skinVertex`, were the exporter's NaN normals (hazards.md). **Not covered:** nobody moved in these runs, so walking, driving and the fight code a pad reaches are untested for this, and only the stage thread was trapped.

## Next up (in order)

**game/routeplayer.cpp counts 100 of 100: its 52 MSL instances 0x254e10..0x255080 and 0x258bc0..0x25b850 are named (a scheduled run, 2026-11-09, batch 247; architecture.md, MSL map), `verify`.** The queue and the frontier were empty, so the run took the partial TU with the highest score (124). All were std::vector / std::set / std::sort / std::lower_bound members and iterator ops the host has from the standard library: listed in a ledger after the file's includes, the callcheck comments now name them. Also: `mslTreeBalanceInsert` 0x25b590 and the two rotations, shared by every set and map. callcheck.py's comment pattern now takes `!`, `*` and `-` (hazards.md). Doubt, not blocking: AreaNumberList (RouteFile +0x0c) is a second 2-byte vector instance beside vector<short>, so its element type in the ROM is something else (u_short, or a short typedef); the host keeps std::vector<short>, which sorts the same. Nothing runs differently; the 15 s smoke shows the boot language screen as before. Next partial TUs by score: game/stagecharacter.cpp (81), game/stagecollision.cpp (52), game/stagelight.cpp (37).

**game/stageitem.cpp counts 139 of 139 (a scheduled run, 2026-11-09, batches 245-246; architecture.md "The bullet and the items"), `verify`.** The queue and the frontier were empty, so the run took the partial TU with the highest score. Its 31 uncounted functions were header inlines: the kinds' getKind, empty side hooks, getCollisionObject / collisionHook0 / onCollide defaults, ItemPhysics::getCollisionParent, and the 4-byte StageList instance emitted at the file's end (now named `StageNodeList::dtor / clear / pushFront / getTail / Node::Node / Node::unlink`), listed in stageitem.cpp's "Written in headers" block after checking each ROM body. 0x183780 was a second `StageItem::canPick`: it is only `j 0x1838d0`, the items' virtual slot, now `StageItem::canPick_thunk`. New code: `CollisionBodyArray::clear` 0x1845f0, which the ROM's dtor 0x184430 calls before freeing (count 0, first slot null; the ROM tests the slot's address for null, kept). Nothing runs differently; the 15 s smoke run shows the language screen as before. **Next with the queue empty:** the next partial TUs by score, game/routeplayer.cpp (47 of 100), game/stagecharacter.cpp (87 of 96), game/stagecollision.cpp (57 of 75).

**game/packtables.cpp counts 52 of 52: its 43 MSL instances 0x2c0d00..0x2c3d30 are named (a scheduled run, 2026-11-09, batch 244; architecture.md, MSL map), `verify`.** The queue, the frontier and the unassigned runs were empty, so the run took the partial TU with the highest score. Every function was a std::map / std::set / std::vector / std::string member the host already has from the standard library (ListMap, NameMap, EntryList, a set<int> `IntSet` in readSheet, a vector<string> `StringVector` in splitFields); the file's ledger maps each to its host form. Nothing runs differently; the 15 s smoke run shows the language screen as before. callcheck.py now reads `+` in note names (hazards.md). Which of the two iterator copies (0x2c0d00 / 0x2c0d50) is the const one is a guess. **Next with the queue empty:** the next partial TUs by score, game/stageitem.cpp (108 of 139), game/routeplayer.cpp (47 of 100), game/stagecharacter.cpp (87 of 96); unassigned@0017fce0 has no functions left (a stale entry).

**unassigned@0026d020 was `GunMoveCondition::~GunMoveCondition`, the deleting dtor of vtable 0x3173b0, now pinned to game/playermotion.cpp, 588 functions (a scheduled run, 2026-11-09, batch 243), `verify`.** CodeWarrior put it right before the condition's own onApply 0x26d090, not with its siblings' dtors at 0x2764b0..0x2765e0 (SnipeCondition 0x276570, GunAttackCondition 0x2765e0); the same 0x70 bytes: vtable 0x3173b0 at +0x10, then MotionCondition's 0x317750, the +8 block freed, the object under the flag. The host's class got the same empty override its siblings have. Nothing runs differently; the 15 s smoke run ends on the language select as before. **Left in the map:** one 1-function run, unassigned@0029a830; the queue and the frontier are otherwise empty.

**The queue, the frontier and the unassigned runs were all empty, so a scheduled run (2026-11-09, batch 242) took game/scriptrunner.cpp's frontier: 28 of CodeWarrior's out-of-line inline copies named (scene / mission / object getters, StageObjArray's walk, the running script's yield pair, two `j` forwarders `stageHideObjects_thunk` and `StageScene::isInputBlocked_thunk`, `scriptLabelCell` 0x1c97f0), listed with the 6 ScriptRunner accessors in a ledger after the label cells; the TU counts 488 of 669, frontier score 78 -> 5, `verify`.** Nothing runs differently (smoke run: the language screen). Left in the TU: `StageNodeBase::setParent` 0x1b54a0, `attachEffectToObject` 0x1b37b0, FUN_001afd20, FUN_001b6910 (nativeIsWalking's isMovingMotion test, 0x3ac), FUN_001b5340 / FUN_001cee90 and their helpers 0x1b56d0, 0x1cf0a0, and the natives no script calls (sssall.py). Doubt for the user: `effectSpawnFailed` 0x1c9770 is an empty function the port does not call; the 12 callcheck MISSING lines of scriptrunner.cpp predate this run.

**unassigned@00269da0 was `SubSceneModelObject::~SubSceneModelObject_thunk`, the dtor's -0x40 adjustor (vtable slot 0x316cb4), now pinned to game/subsceneobject.cpp, 51 functions (a scheduled run, 2026-11-09), `verify`.** The dtor's comment already named it and the name in Ghidra was already right; the run only added it to the file's ledger of thunks and extended the range 0x268a20..0x269db0. Nothing runs differently; the 15 s smoke run ends on the language select as before. **Left in the map:** two 1-function runs, unassigned@0026d020 and unassigned@0029a830; the queue and the frontier are otherwise empty, so the next runs take those, then re-assess from the roadmap (roadmap-3d-control.md).

**unassigned@001ec640 was `MotionLink::onRequest`, the base's empty slot +0x14, now pinned to game/playermotion.cpp, 587 functions, 582 written (a scheduled run, 2026-11-09, batch 241), `verify`.** The host already had it (`virtual void onRequest() {}` in playermotion.h); all seven empty MotionLink defaults are now named and placed (architecture.md, BittenLink). Nothing runs differently; the smoke run reaches the language select as before. **Left in the map:** three more 1-function runs, unassigned@00269da0, unassigned@0026d020 and unassigned@0029a830 (`tus.py list --all`); the queue and the frontier are otherwise empty, so the next runs take those, then re-assess from the roadmap (roadmap-3d-control.md).

**unassigned@001e5d50 was `MotionLink::onRun`, the base's empty slot +0x1c, now pinned to game/playermotion.cpp, 586 functions (a scheduled run, 2026-11-09, batch 240), `verify`.** The host already had it (`virtual void onRun() {}` in playermotion.h); batch 240 also named the other empty defaults inside playermotion.cpp, onStart 0x1eaba0, onLeave 0x1eabb0, onCancel 0x1ec3f0 (architecture.md, BittenLink). **Smoke run blocked:** Smart App Control refused the built YamiPan.exe (WinError 4551, hazards.md "Build and run"): the build is green but there is no picture until the user allows the exe or turns the policy off. **Next with the queue empty:** unassigned@001ec640 (`onRequest`, the same work: name it, pin 0x1ec640-0x1ec650 into playermotion.cpp), then 00269da0, 0026d020, 0029a830.

**unassigned@0021c290 was two door-state checks, now pinned to game/playermotion.cpp, 585 functions (a scheduled run, 2026-11-09, batch 239; architecture.md, afterScripts), `verify`.** `StageCharacter::doorJustOpened` 0x21c290 and `doorJustUnlocked` 0x21c2d0 read what afterScripts keeps (+0x618 the door it used, +0x614 that door's mode then; the ROM's this is the status block, character + 0x590); their only callers, natives 0x8c 0x1c6ba0 and 0x8a 0x1c6d50, are bound now in scriptrunner.cpp's door table (`nativeDoorJustOpened`, `nativeDoorJustUnlocked`, named in Ghidra). Nothing on screen shows them: the 15 s smoke run ends on the language screen as before. **Doubt:** the 2026-11-05 runs bound every native the disc calls, so these two are probably unused by the shipped scripts. **Left beside them:** natives 0x8b 0x1c6c70 (the character's byte +0x610, which afterScripts clears; its writer is not found) and 0x8d 0x1c6b00 (`doorActions`) are still unbound and unnamed. **Next with the queue empty:** the 1-function runs unassigned@001e5d50, unassigned@001ec640 (MotionLink's default request slot +0x14), 00269da0, 0026d020, 0029a830.

**unassigned@00283cb0 was the car links' three deleting dtors, now pinned to game/playermotion.cpp, now 583 functions (a scheduled run, 2026-11-09, batch 238; architecture.md "The car links' hierarchy"), `verify`.** 0x283cb0 DriveLink, 0x283d10 ApproachBiteLink, 0x283d80 ApproachOutLink were already header inlines; their vtable stores showed the ROM's tree: ApproachBite and ApproachOut both derive from ApproachCar (0x317fe0). The port's invented base `CarDoorLink` is gone: its onStart 0x283910 / onLeave 0x283780 are ApproachCarLink's (renamed in Ghidra), and ApproachBiteLink now derives from ApproachCarLink too (it overrides every slot, so nothing runs differently). The 15 s smoke run ends on the language screen. callcheck's 7 MISSING lines in the file are the older ones (takeHits, ApproachBiteLink::onRun, MotionGadgetSpecial::onUpdate, leaveLoud / leaveNoticed / leaveAtOnce, the MotionCondition ctor). **Next with the queue empty:** unassigned@0021c290 (2), then the 1-function runs unassigned@001e5d50, unassigned@001ec640 (0x1ec640 is MotionLink's default request slot +0x14, in Drive's and ApproachCar's tables), 00269da0, 0026d020, 0029a830.

**unassigned@00186a00 was StageNode's four inline slots, now pinned to game/nodetransform.cpp, which counts 30 of 30 (a scheduled run, 2026-11-09), `verify`.** 0x186a00 setDirty, 0x186a10 rebuildMatrix, 0x186a20 getMatrix, 0x186a30 getWorldMatrix sit in the lights' run but belong to StageNode (vtable 0x315a80, shared by 0x315a20, 0x314180, 0x315160, 0x315190); they were already header inlines in stagenode.h and match the ROM. The file's other 10 uncounted functions (the bone and matrix nodes' empty slots, StageNode::setDirtyTree, the list's begin / linkValue) were checked the same way and listed in its "Written in headers" block. Nothing runs differently: the 15 s smoke run ends on the language screen. **Next with the queue empty:** unassigned@00283cb0 (3), unassigned@0021c290 (2), then the 1-function runs (unassigned@001e5d50, unassigned@001ec640).

**yamipan/rendering/model_renderer.cpp, the GPU path for model batches, rigid then skinned (2026-11-07, branch claude/cool-swanson-07c8ec), `verify`.** Asked after "why only 20 fps": the frame rate is the ROM's own cap (PAL, 2 fields a frame, 25 fps), and the vertex path was most of the game thread's work under it. **What changed:** every batch goes to the GPU once, the first frame that draws it, cached by its pointers into the model file (positions, normals, colours, UVs, bone ids, weights, count, strip) with the first vertex as a fingerprint against another file at the same addresses; flushed by `forgetTextures` (a stage's pack) or past 32 chunks of 4 MiB. A frame records only `ModelBatchDraw`s, one `ModelInstance` per run (matrix, lights, scroll, flags, where its palette starts) and, skinned, the matrices the batch's bones reach (`Frame::modelPalettes`, a storage buffer, shared by the batches of a prim). model_static.vert / model_skinned.vert (colour, reflection) and model_static_projected.vert / model_skinned_projected.vert (spot, shadow), one body each in model_static_color.hlsli / model_static_projected.hlsli and the VU1 maths in model_static.hlsli, do skinVertex, litColor, the point light, the reflection, spotVertex and shadowVertex as the CPU path does, and cut colours to a byte the same way. Back-face culling is the rasterizer's: the index buffer turns a strip's odd faces round, the side comes from the sign of the projection's determinant and the batch's mirroring. **All three later passes are on the GPU.** The spot cone and shadow box tests use the batch's mesh box (rigid) or that box placed by each of its bones (skinned), and only a box that does not clear them walks the vertices on the CPU (skinned: placed through the palette as before). SDL caps a pipeline at 16 vertex attributes, so the instance's scroll, flags, reflection scale and palette start ride in its light rows' w lanes, and a skinned vertex's bone count in its colour's alpha, which VU1 never loads. The CPU path stays only for a batch larger than a chunk (none seen). **Measured, `drawBatch` per frame, game-thread time (temporary probe, removed), 60 s runs:** stage 2, 4,052 batches / 98,919 vertices (1,659 skinned): 5.4..5.9 ms before, 1.65..2.56 ms after, the probe's game-thread work 7.0..7.7 ms before, 3.9..5.65 after. Stage 7, 3,083 batches / 75,930 vertices: 4.0..4.4 ms before, 1.2..1.9 after; 2,728 batches: 3.3..3.9 before, 1.4..1.6 after. With rigid batches alone on the GPU (an intermediate step), stage 2 had not moved: the skinned batches were 3.6..4.8 ms of it. What is left is per-batch bookkeeping, about 0.5 us a batch (the layer and cache lookups, the instance). The machine is shared and viewed over Remote Desktop, so presents and shot timing say nothing here; only game-thread time does. **Seen:** `--run 60` on stages 2 and 7, before and after, looked at: the same pictures (the girl in the rain woods, the plants, stage 7's player with the flashlight's spot on the road). Shot-to-shot differences are the moving mist and rain: the baseline differs from itself by as much (mean 5.4..6.4 of 255, 20 % of pixels over 8). **Doubts for the user:** (a) the skinned box for the spot and shadow tests takes a vertex to be a weighted mean of its bones' places, so weights that do not sum to 1 could skip a test that should have walked; (b) **the pass's `eye` is the ROM's, settled the same day:** on stage 2 it is exactly 1/400 of the projection's centre in the far passes, and the camera itself in the main pass. 0x2e5da0 works VU address 4 out as -(view rows . view translation), which is the camera only for a view without scale, and `setFarMatrices` scales the far passes' view by 1/20, so it comes to the camera / 400. VU1 takes that eye into each mesh's space through the inverse `uploadMeshMatrix` 0x2e7610 sends to VU 216..219 (VIF unpack of 14 quadwords at 212: the world matrix, its inverse, two of scales, the transpose) and culls there (0x12f..0x143). So in the far passes the ROM culls, and maps reflections, from near the world's origin: `fillEye` is right as it is. The rasterizer culls from the projection's centre, so **a batch with `ModelFlag_TagC` in a pass whose eye is not that centre (`eyeAtCentre`) goes to the placed path**, which keeps VU1's answer; the reflection takes the pass's eye on both paths. Counted with a temporary probe (removed): about half the passes are off-centre on stages 1, 2, 7 and 9, and **not one batch with TagC is drawn on any of them**, in either kind of pass, so no culling happens there at all. Not seen: a reflection (none on stages 2 and 7), a shadow on screen (none was before either).

**The volumes' tail is named and placed: unassigned@0017f040 and unassigned@0017ef90 are gone (a scheduled run, 2026-11-07, batch 237; architecture.md "Volumes"), `verify`.** 0x17ef90 `StageVolume::getKind` and 0x17f040..0x17f150 (`SphereVolume::getKind`, its empty `sideHook0..3` 0x17f080..0x17f050, and 12 adjustor thunks: the side hooks of both volumes -0x18, the three dtors and `BoxVolume::scriptIsFacing` = containsObject -0x40) joined game/stageplayer.cpp, which now has 0x17e860..0x17ed30 and 0x17ef10..0x17f150. The sphere's side hooks are new header overrides (empty, like the base's); everything else was already written as header inlines and is now listed in stageplayer.cpp's "Written in headers" and thunk blocks. Nothing runs differently: the 15 s smoke run ends on the language screen. **Left with the queue empty:** unassigned@00186a00 (4), 00283cb0 (3), 0021c290 (2), and the 1-function runs 001e5d50, 001ec640, 00269da0, 0026d020, 0029a830 (tus.py list --all -n 400). Side note: 0x215300 `StageCharacter::isPassenger` lies in stageplayer.cpp's range but is written in game/playermotion.cpp.

**The static initialisers are named and placed: unassigned@00310a00 is gone (a scheduled run, 2026-11-07, batch 236; architecture.md "Static initialisers"), `verify`.** The 9 functions of 0x310a00..0x310f80 are CodeWarrior's `__sinit_<file>_cpp`, each joined to the port file owning what it sets and written as an `FS2` comment at that global (the host's compiler constructs it): packtables (gDoorRecords), stageloop and stagescene (dead copies of a header's zoom pair), scriptrunner (a ScriptRunner at 0x577a60 nothing reads: not ported), playermotion x3 (the zoom pairs of setScope, SnipeCondition::onApply, ViewCamera::setZoom), uigame (57 name pointers), stageresult (gStageResult). `ViewCamera::kZoomLow / kZoomSpan` are now the formula from `kZoomPercent` 30; the EE rounds toward zero, so the ROM's values are one ulp from the host's (noted, harmless). The 15 s smoke run ends on the language screen, as before. **Left with the queue empty:** unassigned@0017f040 (17, tiny), 00186a00 (4), 00283cb0 (3), 0021c290 (2), and the 1-function runs 0017ef90, 001e5d50, 001ec640, 00269da0, 0026d020, 0029a830 (tus.py list --all -n 400); the many with 0 functions are stale map entries.

**Sony's TIM2 sample is whole: rena/tim2.cpp 20 of 20, 0x2e97a8..0x2eaeb0 (a scheduled run, 2026-11-07, batch 235; architecture.md, Tim2Texture), `verify`.** The queue and the frontier were empty; unassigned@002e9d90 (11 functions) was the rest of tim2.c and is pinned there: Tim2SetClutColor, Tim2GetTexel, Tim2SetTexel, Tim2GetTextureColor, the GS loaders Tim2LoadPicture / LoadImage / LoadClut (an older cut with no tbp / cbp: the addresses come from the picture's GsTex0 / GsMiptbp), Tim2TakeSnapshot and the statics Tim2LoadTexture, Tim2CalcBufWidth, Tim2GetLog2. None has a caller in the ROM but GetTextureColor's own; on the host the loaders keep their decisions and send nothing (no GS), the snapshot writes black rows. Nothing runs differently: the 15 s smoke run ends on the language screen. **Left with the queue empty:** the todo unassigned@ runs with functions (tus.py list --all -n 400): 00310a00 (9), 0017f040 (17, tiny), 00186a00 (4), 00283cb0 (3), 0021c290 (2), and six of 1 function each; the many with 0 functions are stale map entries.

**rena's light records are complete in source, and three leftover runs are gone (a scheduled run, 2026-11-07, batch 234; architecture.md "The lights of a stage"): game/scenerender.cpp, `verify`.** unassigned@002e00c0, unassigned@002e0580 and unassigned@001fa980 were the light records' virtual slots and dtors: now pinned to game/scenerender.cpp (0x1f6ac0..0x1faa30, 0x2e00c0..0x2e0840). `RenaLight` is the ROM's abstract class now (virtual dtor, setActive, isActive, getKind, getMatrix, getAngle, getRadius, getColor, getShade; `RenaLightKind`), over two empty bases the port names `RenaObject` (0x315f00) and `RenaLightBase` (0x315f20). The upload slot stays rena_draw.cpp's fillPointLight / fillSpotLight. Nothing runs differently: the 15 s smoke run ends on the language screen, a 35 s YAMIPAN_TELEPORT=7 run shows Akiko on the lit road as before. **During this run another session edited the same working tree** (a temporary frame-timing probe in src/game/gamesystem.cpp, src/yamipan/seams.cpp and src/yamipan/probe_tmp.h, "not to be committed"), which broke the build for a few minutes; it was gone again before the commit and none of it is in this one. A scheduled run commits with git add -A, so a live session and the routine in one tree can mix their work: pause the routine (work/STOP) while working by hand. **Left with the queue empty:** the todo unassigned@ runs (tus.py list --all), most with 0 functions counted: see what each is.

**unassigned@002081b0 was JackFill, now pinned to game/stageparty.cpp (a scheduled run, 2026-11-07, batch 233), `verify`.** 0x2081b0..0x208330 is the sight-jack fill's three functions: draw 0x2081b0 and the ctor 0x2082e0 were already written in stageparty.cpp; the deleting dtor 0x208280 is now `JackFill::~JackFill` (puts vtable 0x316030 back, runs ~ScreenFill). Nothing runs differently; the 15 s smoke run ends on the language screen as before. callcheck's one MISSING in that file, `SightJack::drawLive` (gfxGetDrawEnv, gsFillRect), is old and explained by its PC note (RippleSheet::draw's host pass). Left for later runs with the queue empty: the partials unassigned@002e00c0 (1 of 4), unassigned@002e0580 (1 of 3); whether 0x2e9d90.. is more of the TIM2 sample.

**Sony's TIM2 library is complete in source: rena/tim2.cpp 9 of 9 (a scheduled run, 2026-11-07, batch 232; architecture.md, Tim2Texture), `verify`.** The queue and the frontier were empty, so the run took the partial TU. New: `Tim2CheckFileHeaer`, `Tim2GetUserSpace`, `Tim2GetUserData`, `Tim2GetComment`, `Tim2GetClutColor`; nothing in the host calls them yet (`Tim2GetClutColor`'s one caller, 0x2ea380, is in unassigned@002e9d90, not read). Left for later runs with the queue empty: the partials unassigned@002081b0 (2 of 3), unassigned@002e00c0 (1 of 4), unassigned@002e0580 (1 of 3); and whether 0x2e9d90.. is more of the TIM2 sample.

**game/stageelevator.cpp's "13 stubs" were dead leftovers, now deleted: the TU counts 27 of 27 (a scheduled run, 2026-11-07), `verify`.** The 13 old free functions `elevatorCall` .. `elevatorSwitchPlace` (and `elevatorCalledFloors`, no address) stayed in src/lib/unassigned_stubs.cpp after batch 184 made them `StageElevator::` members; nothing called them, but tus.py counted their addresses as stubs (hazards.md "A free-function stub left behind"). The file's orphan "// stub of" comments went too; it holds 4 stubs now, all StageObject virtuals with no known address (onViewTop, cameraStep, onLinkChanged, followsPlayerLight). Nothing runs differently; smoke run 15 s: the language screen, as before. **Left (the queue and the frontier are empty):** the partial unassigned@002081b0, unassigned@002e00c0, unassigned@002e0580, rena/tim2.cpp; unassigned@0017fce0 (todo, 0 functions counted: look what it is); the callcheck MISSING lines in playermotion.cpp listed in the next entry; the four StageObject stubs above (find their ROM slots: `rtti.py --class` on StageObject's vtable).

**game/playermotion.cpp is complete in source, 577 of 577 (a scheduled run, 2026-11-07), `verify`.** The last 14 of its slice were already written, as inlines in headers: the carried list's second StageList<StageObject *> instance 0x217700..0x2179ac (getTail, find, pushBack, erase, pushFront, Node::Node, Node::unlink: stagelist.h), StageCharacter::doNothing / wasPlacedThisFrame / setSpawnPoint / canTakeUpTrap / heardWithin and CharacterBlock7c8::clear / find (gamehooks.h). Each was checked against the ROM and listed in playermotion.cpp's "Written in headers" block, so tus.py counts them; the instance stays FUN_ in Ghidra (names are unique there) and giveItem / dropItem / findCarried explain it in callcheck lines. Nothing runs differently; smoke run 15 s: the boot, as before. **Left for the next runs (the queue and the frontier are empty):** the partial unassigned@002081b0, unassigned@002e00c0, unassigned@002e0580, rena/tim2.cpp; game/stageelevator.cpp's 13 stubs; callcheck MISSING lines older than this run in playermotion.cpp (takeHits 0x1e18c0, ApproachBiteLink::onRun 0x283060, MotionGadgetSpecial::onUpdate 0x280230, leaveLoud 0x219510, the MotionCondition ctor 0x276510).

**The car motions' run is pinned to game/playermotion.cpp, and the special motions' dtors are written (a scheduled run, 2026-11-07, batch 231), `verify`.** The queue and the frontier were empty, so the run took the partial unassigned@00280620 (10 of 11 written): 0x280620..0x2811c0, 0x281370..0x2813e0 and 0x281548..0x281a40 (also unassigned@00281550) joined playermotion.cpp, where MotionDragOutCar, MotionGetOffCar, MotionGetOnCar and findCarDoor already were. Named: 0x281370 `MotionSpecialAction::~MotionSpecialAction`, 0x281550 `MotionSpecial::~MotionSpecial`, 0x2816b0 `MotionPartnerCall::~MotionPartnerCall` (each the deleting dtor in slot +8 of its vtable, 0x317d30 / 0x317d50 / 0x317d10, the vptr at +0x10). All six dtors of the family are out of line now with empty bodies (nothing of their own to free). Smoke run 15 s: the language screen, as before. **Left in playermotion.cpp's slice (older, not this run's):** FUN_00217700, FUN_00217770, FUN_00217840, FUN_00217890, FUN_00217920, FUN_00217940 (the carried-item list helpers that dropItem / giveItem / findCarried call, the callcheck MISSING lines) and CharacterBlock7c8::find 0x21a8d0: a good next run. Other partial runs left after this: unassigned@002081b0, unassigned@002e00c0, unassigned@002e0580, rena/tim2.cpp (5 of Sony's TIM2 functions nobody calls).

**rena's box test is written, nothing calls it yet: rena/rena_camera.cpp 0x2e2970..0x2e2cf0 (a scheduled run, 2026-11-07, batch 230; architecture.md "The part box test"), `verify`.** `boxClipTest` 0x2e2a60 answers 0 inside, 1 crossing the guard band, 2 outside, on the six frame planes and six guard-band planes of the draw state's +0xc0 block (`RenaClipView`). 0x2e2970 is now `RenaCamera::RenaCamera` (DrawSysCamera's rena base, still written in game/drawsys.cpp). **Left, a question for the user:** rena_draw.cpp's `boxOutside` stands in (the host projection, width margin 3 for every aspect). Using the ROM's test means building the planes (0x2e5bb0, 0x2e5fc0: the clip matrices at a margin of 0 and 300 pixels from 0x59b188 / 0x59b18c and the screen scale) from the renderer's display size, so wide screens are not culled at 4:3; and storing a skinned part's centre only on an answer of 1, as the ROM does (rena_draw.cpp skinnedPartOutside's comment). Not done in this run: it changes what is drawn.

**unassigned@002b0d50 was one jump, now `DemoStream::getDataWritten` in game/demoevent.cpp (a scheduled run, 2026-11-07, batch 217), `verify`.** 0x2b0d50 is `j SoundStream::getDataWritten` with a0 unchanged, the out-of-line copy of an inline forward; its one caller is `StageScene::enterSubScene`'s voice wait. The host already reached the base's through inheritance, so nothing runs differently; the 15 s smoke run ends on the language screen as before. demoevent.cpp's range now starts at 0x2b0d50. **Next:** unassigned@002b22f0, the seven-key pad (its view 0x2b2c80, 0x2b2620 and the stub `SceneOwnedObject::isSevenKeyOpen` 0x2b2c10).

**The pause menu and the map menu open now, and draw (a scheduled run, 2026-11-07, batch 212; architecture.md "The pause menu and the map menu"): game/pausemenu.cpp, `verify`, all 7 functions of 0x20acb0..0x20b120 (was unassigned@0020acb0); `StageCharacter::latchAltAction` 0x20b1d0 joined game/playermotion.cpp with 0x20b120..0x20b240.** **A bug came out of it:** `wantsPauseMenu` read `StageObject::m_canPause`, which nothing wrote, so Start never paused; the ROM reads the character's `m_allows.standing` (+0x3a1, AlwaysCondition) and so does the port now (hazards.md). **Seen (YAMIPAN_TELEPORT=7, scripted pad):** Akiko's page with its six lines and the mission text, Down moves the cursor, Start resumes, Select shows the map's grid, cross and compass (progress/20261107-pause-menu-stage7.png, -map-menu-stage7.png). **Open:** no map drawing under the grid in that run (does stage 7 part 1 have one? the map layout's items are unread); Triangle did nothing in the pause menu; the screens behind answers 4..9, Quit and Restart were not driven. Smoke pads: pad reads run about 10 a second here with a teleport, so press Start at read 300, not 1500.

**The lock-gadget helpers are written (a scheduled run, 2026-11-07, batch 208; architecture.md "The number lock"): `missionRestoreCheck` 0x29f2a0 and `findNumLockTarget` 0x29f440 joined game/scriptrunner.cpp (was unassigned@0029f2a0), `verify`.** `StageMission` +0xb8 is a `LockGadgetKind` (2 number lock, 3 seven keys), not a pointer: `restoreArg` became `restoreKind`, `setRestore` / `saveObjectFlags` take an int, and `missionRestoreCheck` answers `LockRestoreBits` (open 1, tried 2) as an int. `SceneOwnedSlot` moved to stagescene.h. The number-lock natives (0x201, 0x202, 0x22c) now find their lock; their PC null guard stays for stages without one. **New stub:** `SceneOwnedObject::isSevenKeyOpen` 0x2b2c10 (unassigned@002b22f0, with the seven-key view 0x2b2c80 and 0x2b2620). **Seen:** nothing: the 15 s smoke run ends on the language screen as before; the lock code needs a stage with oMK21_06 / oSJ08_07 and a script calling useLockGadget. **Doubt:** the ROM's switch names kind 1 and answers 0 for it like the default; no caller found that saves kind 1 (only native 0xaa saves, with 2 or 3). **Next:** unassigned@002b22f0, the seven-key pad (its view and isSevenKeyOpen).

**The frame filter is written, and the opening event is soft now (a scheduled run, 2026-11-07, batch 207; architecture.md "The frame filter"): game/framefilter.cpp, `verify`, all 5 functions of 0x2849c0..0x286790 (was unassigned@002849c0).** `gsFrameFilter` and its helpers keep the ROM's decisions (halves, buffers, when to blur, each tap's shift and alpha, the order) and hand each sprite to a new host pass, `FrameFilterPass` (`YamiPan_DrawUiFrameFilterBegin` / `Step`; scratch textures keyed by the game's VRAM block, no GS memory). **Seen (New Game, work/newgame_pad.txt, 240 s, no crash; a second run with the filter forced off, removed):** event d0101's ceiling lamp soft with the filter (progress/20261107-d0101-frame-filter-on.png), sharp planks and ring without (-off.png), mean brightness the same. **Not seen:** the view filters of camview / camjack (player on stage 6, partner on stage 12, the two SNPCs): no run reaches them. No reference picture of the game exists for either, so `verify`. **Doubts for the user:** (a) the host samples every tap bilinear at the render size, the GS nearest at its own size: the same at 512 wide, smoother past it; (b) the scratch buffers are the render size, not the ROM's 256 and less, so the blur is as wide in the picture but sharper in detail at a high resolution, as the palette views' doubt (h); (c) the down-sample's UV starts at texel 1 in the ROM, the host samples pixel centres: the same 2 x 2 mean, a sub-pixel drift apart.

**The sky mist is written (a scheduled run, 2026-11-07, batches 201-202; architecture.md "The sky mist"): game/skymist.cpp, `verify`, 23 functions of 0x2ad030..0x2afc20; game/helpscreen.cpp (`runHelpScreen` 0x2acff0, alone), `verify`.** The frontier's unassigned@002acff0 was these two. The seeded `MenuProgHolder` is a holder of the sky mist: renamed `SkyMistHolder`, StageScene's `menuProg` is `skyMist`, `createMenuProg` / `InitItem_MenuProg` are `createSkyMist` / `InitItem_SkyMist`. **Seen (YAMIPAN_TELEPORT=7, 40 s, a temporary harness, removed):** the mist runs (116..123 puffs, 3..4 wisps alive, no crash), but only about 0.4 puffs a frame pass the ROM's tests from the stage's opening view and none shows in the shots, also with their colour forced bright: nothing on screen yet tells whether it looks right. **Not proved:** no ROM-vs-host comparison was written (a sightcmp.py-style run of 0x2adc70 would prove the puffs' corners, colours and tests). **Doubts for the user:** (r) on list 3 the ROM draws without a depth test; the host's effect pass tests depth against the far models, so a far model in front of a puff now hides it; (s) the cut at w 500 is done on the host as one polygon; the ROM's three cases give the same polygon, read from the code; (t) `mistWrap` returns v + 2 radius for any offset past the radius (the ROM's own slip, kept), so in most of a map the drifting mist is not where the camera is: is the effect that rare on a real PS2? (u) the names (sky mist, puffs, wisps, swirl) are readings of the code. **Next:** a view looking up on a stage, or stage 0x16 / 0x17 (near, with depth), to see it.

**The PCM stream format is written (a scheduled run, 2026-11-07; architecture.md "PCM format ... ADPCM format"): rx/rxsndstrpcm.c, `verify`, all 10 functions of 0x164398..0x164c38, no name changed.** The ADPCM format's twin, cut down: 1 or 2 channels only (mode 0 / 2), no tracks, pitch or attribute in update, writes of any size, start clamped but not aligned, the read ring the slot's whole bufSize, `rxSndPcmOpen(mode, iopAddr, iopKB)`. The same PC changes as ADPCM: user data is the stream id, PCM_SILENT_BUFFER stands for an iopAddr of 0, and a block "the driver's silent side" answers `rxSndPcmUpdate` (status by wall clock, the ring reported drained, the position as (block, offset) as `rxSndPcmAddCallback` counts it). This should end the hang of hazards.md "An event that never ends" for a PCM request. **Not seen:** the smoke run opens no PCM stream (language screen); which game sound uses format 1 was not looked up. **Doubt:** the silent side takes PCM as 16-bit (rate * 2 bytes per channel) until RXSNDDRV's PCM path is read; if it is 8-bit or the rate means something else, a PCM request ends at the wrong time. **Next on this road:** the audio milestone.

**The gadget views are written: SigLamp and the number lock now have their camera, lights and look (a scheduled run, 2026-11-07, batch 205; architecture.md "The gadget views"): game/gadgetview.cpp, `verify`, all 53 functions of 0x2a7b10..0x2a9c70 (was unassigned@002a7b10).** `SceneOwnedObject` is the 8-byte handle with 15 forwarding slots, `GadgetView` the body with the pairs and the look's states, `SigLampView` / `NumLockView` its two subclasses here; `createMapObject` now builds `NumLockGadget` into StageScene +0x90 with the first lock. **Renamed from guesses:** `start` is `hasObject`, `hasTarget` `isInView`, `getTarget` `getUser`, `isSlot2c` `cancel`, `isSlot44` `isLooking`, `setNames` `addPair` (callers in stagescene.cpp, scriptrunner.cpp, playermotion.cpp updated). `RoutePlayer` no longer derives from `SceneOwnedObject` (it only borrowed its destroy; vtable 0x316a78 is not a gadget's). **Port differences:** the handle's dtor deletes the body directly (the ROM through the body's slot 0); `NumLock::setInView` takes a bool (the ROM passes the camera; the body is empty). **Seen:** nothing on screen: the 15 s smoke run stays at the language screen, and New Game with work/doorwalk_pad.txt (280 s, no crash) shows the dark first-person wall as before; whether that stage has a gadget was not looked up. **Not proven:** no comparator; the look never ran. **Doubts for the user:** (a) the beam's length is the point light's y at `enterView`, which `prepare` set to 0 (it is placed at (5, 0, 0), the asm checked), so unless a stage script moves the light by its name SigLamp_pl (it is in the scene's object list, so one can), the ray has no length and the point light stays at the lamp until something else moves it: read again with a run on stage 9 (it builds a SigLamp, open-items "the 11 links") before trusting it; (b) the mask's four quarters cover the 512 x 512 window and the 2D module stretches that over a wide display: the ring takes the frame's shape, as the film's (i); (c) pad left turns a dial up and right down, up / down pick the dial: the ROM's bits (0x8000, 0x2000, 0x1000, 0x4000), not checked against how the lock faces the camera. **Next:** findNumLockTarget 0x29f440 (unassigned@0029f2a0, also the caller of `wasTried` / `isLockOpen`) and the seven-key pad's view 0x2b2c80 (unassigned@002b22f0), which now has its base class here.

**The number lock is a class now, and its dials turn on the model (a scheduled run, 2026-11-07, batch 197; architecture.md "The number lock"): game/numlock.cpp, `verify`, all 29 functions of 0x2a6ab0..0x2a7b10, split off unassigned@002a6980.** `NumLock` (the map object of oMK21_06 / oSJ08_07, now built by `createMapObject`) and `NmlPose` (setBaseMotion's kind 3, until now a stub ctor). The seven `numLock*` free functions are members; SpNumLockLink and the natives 0x201 / 0x202 / 0x22c call them. **Port differences:** the natives skip a null lock (findNumLockTarget is still a stub; the ROM calls through it), NmlPose's bone indices start at 0 where the ROM leaves them, and a pose with no bones skips the dial quaternions. **Checked:** the float order of the dial math against the asm (no comparator: the class needs StageMapObject linked; a numlockcmp.py would have to stand in for it). **Not seen:** the smoke run stays at the language screen; the lock is on the stages whose map has oMK21_06 / oSJ08_07 (not looked up). **Next:** unassigned@002a6980 is now 0x2a7b10..0x2a9c68 plus 0x2a6980: the number-lock view (StageScene +0x90, ctor 0x2a89b0, `createMapObject` should build it, see the PC note there), SigLamp, and 0x2a9be0 (stagescene.cpp calls it 3 times); getSoundKind 0x2a6980 is game/charactersound.cpp's and should be pinned there.

**The last unnamed code beside the sight-jack was std::vector, and nothing in the game changes (a scheduled run, 2026-11-07, batch 196; architecture.md "The sight-jack"): `verify`, the 8 functions of 0x206890..0x207108, pinned to game/stageparty.cpp; 0x2067d0, `CharacterPad::pressedBind`, to game/characterinput.cpp.** unassigned@002067d0 was no file. `BearingVector` is the gathers' list, `RippleLists` and `RippleWaves` the ripple sheet's, their dtors here because ~SightJack inlines ~RippleSheet. The host keeps std::vector. **Proved:** new `tools/reverse/bearingvectorcmp.py`, one script on the ROM's code and std::vector: 6,000 ops, 133,165 elements, 2,291 inserts of a value inside the vector, 0 differ, the dtors free what they own; `--plant` 827 of 1,500. **Port difference:** capacity grows differently, and `stepAlertedFoe`'s reserve is dropped; no caller can see either. Nothing new to look at on screen.

**Stage scripts get real random numbers now (a scheduled run, 2026-11-07; architecture.md, StageScene ctor / dtor): game/rand48.cpp, `done`, the 3 stubs of 0x2681e0..0x268310.** `Prop_Random100` always answered 0 until now, so **every script's random branch took one side**; it now answers 0..99. **Proved:** new `tools/reverse/rand48cmp.py`, one op script on the ROM's code and the host's, 20,000 ops, 1,890 draws thrown away, 0 differences; `--plant` (Java's n - 1) 8,288. **Seen, temporary probe, removed:** stage 7 made 734 draws in 35 s, d0103 made 9, stage 1 none; no crash, none went through the null guard; the pictures look as before, no script-driven difference was picked out. **Port difference:** `StageScene::random` answers 0 on a sub-scene, where the ROM runs through a null pointer. No event was seen to ask. **For the user's eye:** behaviour that used to be fixed may now vary between plays: what the scripts of stage 7 draw for was not read. Also: eerun.py's mult now writes LO and HI in the three-operand form, and it has multu, mult1 and the EE's untrapped div by 0.

**The smoke candle's fog and the glow's fog are written and agree with the ROM; neither has been seen on screen (a scheduled run, 2026-11-07, batches 193-194; architecture.md "The fog around a character"): game/stagefog.cpp, `verify`, all 12 functions of 0x2ce0c0..0x2cedb0.** They were the two stubs a run reached (`SceneBufferSet`, `SceneBlock98`), split off unassigned@002c42a0; that run keeps the Extra mode, 0x2c42a0..0x2ce0c0, `runExtraMode` and its 93 functions, still a stub. New names: `SmokeFog`, `GlowFog`, `CharacterFog`; `StageScene::smokeFog`, `glowFog`, `updateSmokeFog`, `updateGlowFog`, `createSmokeFog`, `createGlowFog`, `addSmokeSource`, `measureGlow`; `StageMission::smokeRange`. **Consequence:** `StageCharacter::m_glow` now runs for kinds 5 and 0xc, it never did before. **Proved:** new `tools/reverse/fogcmp.py`, random frames of both fogs on the ROM's code with its real list code: 32,703 ops, smoke shown 169 times, glow fog 93, 0 differences; `--plant` 698. **Seen in a log only, temporary probes, removed:** stage 17 with F_PC17 warped into smoke box 5 and "Set Smoke Candle" from the game's menu: the branch ran ("smoke shown") at about scene frame 440, for under 30 frames, then the warp's place set off a story event (TMPSTR branch, the camera goes to another character) and no shot caught the fog. Boxes 2, 3 and 4 dropped the player through the floor (a warp to the top of the box, z fell without end), box 0 stood but the set never happened in the run. No glow run got a glowing character within 500 of the player: logs on 18, 21, 26, 28 show the fog built, never running. No crash in the sweep of stages 1..30, 22 s each, or the 100 s runs. **Not done, for the next run:** the picture. Stage 17 box 0 at ground level, or stage 20; warp, Set, stay in the box with shots every 10 frames; look for hatsu_fog against the light set change. For the glow, stages 18, 21, 26, 28 or 29: warp next to a character of kind 5 or 0xc in state 2..5. The warp only counts after the opening's sub-scene ends: hazards.md "Code past isSubScene does not run in a stage's opening".

**The smoke candle works from the game's own menu: "Throw Smoke Candle" and "Set Smoke Candle" run, and the flare burns (a scheduled run, 2026-11-07, batch 192; architecture.md "The item waits"): game/playermotion.cpp, `verify`, the two stubs of the last entry written.** `StageCharacter::useItem` 0x218b50 is `throwCarried`, `item218850` is `putSmoke`; the range 0x218500..0x219b30 joined the file. **Proved:** new `tools/reverse/itemwaitcmp.py`, both waits on the ROM's own code and the host's while the history moves under them: 61,205 ops, 0 differences, every state reached; `--plant` 2,572. **Seen, temporary probes, removed (YAMIPAN_TELEPORT=17, pad to the menu, no forced state):** F_PC17 throws: Ask, Asked at frame 0x4e0, 20 calls of Playing, a burning flare out ahead with smoke over it, progress/20261107-flare-in-flight-stage17.png (its trail in the air), progress/20261107-flare-thrown-stage17.png. Set, after walking from the ledge: Ask, Asked at 0 (the ROM's slip), 19 of Playing, the column of smoke at the player's feet, progress/20261107-flare-set-stage17.png. The default part of stages 1..30 ran 24 s each with the new code, no crash, and nothing called the waits unasked. **Not seen:** a brain's path (in those 24 s only F_PC17 and F_PC20 were given a flare; the other parts of the stages were not run), native 0x15a (7 scripts), a thrown I_NUT on stage 24, the wait's true answer as such (the script went on, its branch was not read). **For the user's eye:** nothing to compare with from a real PS2: is the column as tall and as white? Dark ragged patches float in the fog on stage 17, at about (530, 170) and (40, 310): a run with the same walk and no candle shows them too, so they are not from this work; what they are was not looked at. **Next:** the flare has no light: `SceneBufferSet::createFor` 0x2ce620, a stub, answers null where the ROM makes "a new set when owner is I_HATUEN"; unassigned@002c42a0, which the frontier gives anyway. **Deviation of this run:** `tus.py next` gave unassigned@002c42a0, since it skips a queued `verify`; the run read the handoff and claimed by hand (hazards.md), and the queue is empty again. Its slice still holds 13 stubs, none touched.

**The item natives answer the ROM's bool now, and what they call is still a stub (a scheduled run, 2026-11-07, batch 191; architecture.md "The runner table's defaults"): `verify`, the 106 inline defaults of 0x171ad0..0x1723c0, pinned to game/stagedoor.cpp.** The run was never code of its own: 84 slots of the script runner's table, the StageObject accessors, the door record's five setters, nearly all written as header inlines already, the rest (27 slots) added as virtuals, the character's 13 overrides bound, all 87 unnamed functions named. **A bug came out of it:** slots +0x16c, +0x170, +0x174, +0x17c (`scriptUseSmoke`, `scriptUseNamedItem`, `scriptUseItemAt`, `scriptPutItem`) were `void` with the comment "void in the ROM: their acc is what v0 held", and natives 0x104, 0x105, 0x106, 0x15a stored 0. The ROM's defaults end `sltu v0,zero,v0`, its natives keep `andi v0,0xff` as acc, and `useItem` 0x218b50 and 0x218850 answer through tail jumps: they are bool. Fixed down the whole chain (brain, character, natives). **Nothing changed on screen then: `useItem` and `item218850` were stubs that answered false, and 0x105 and 0x106 are on 159 scripts.** Written since (the entry above). **Proved:** new `tools/reverse/defaultcmp.py`, every inline default of scriptrunner.h against the ROM's body and slot: 84 compared, 0 differences, `--plant` 1; 5 notes (below). **Not run:** nothing reaches the 27 new slots: natives 0x2e, 0x36, 0x37, 0x4b, 0x63, 0x69, 0x6c are on no script, so they stay unbound. **Left:** `ScriptProxy`'s 17 forwards of those slots (0x1a6b50, 0x1a7060, 0x1a7170, 0x1a71f0, 0x1a7230, 0x1a7350, 0x1a73e0, 0x1a7420, 0x1a7460, 0x1a7520, 0x1a7560, 0x1a75e0, 0x1a7620, 0x1a7660, 0x1a7700, 0x1a77c0, 0x1a7800) are unwritten; `py slotcheck.py Vt_00315690 ScriptProxy` lists them; unreachable until a native is. **Doubts:** slots 49..53 (`scriptAttackBite`, `scriptSwingAtBite`, `scriptSwingAt`, `scriptFireAtBite`, `scriptFireAt`) are plain tail jumps in the ROM, so an int there, and bool here: the same 0 by default, the overrides not re-read for a value above 1. The new names of slots no class overrides (`command5`..`command79`) are numbers, their argument counts read from the proxy's forwards.

**The camera's place hides the map's far parts now: game/areaclip.cpp is written and agrees with the ROM on all 20 area clip files of the disc (a scheduled run, 2026-11-07, batches 189-190; architecture.md "The area clip"): `verify`, the 59 functions of 0x2dac70..0x2e00c0.** It was stubs, reached every frame. `ModelInstance` and `ModelPartRec` now derive from the ROM's `ClipTarget`, so they carry a host vptr; `distant` and `flagE` are `clipped`. Names of the MSL instances are the port's. **Proved:** new `tools/reverse/areaclipcmp.py`, the whole ctor on the smallest file (64 cells), 1,140 sampled (box, cell) pairs on the other 19, 4,000 random frames, 134,343 setClipped calls, 3,375 frames with the camera in a box, 931 hidden characters: 0 differences; `--plant` 409. eerun.py gained `vclipw` and the clip flags. **Seen, temporary probes, removed:** stage 2 walking: 11 hidden, 16 more, 3 shown back, the world in view unchanged against the same frame unclipped; no crash on 2, 7, 9, 12, 20. **Not seen:** a character hidden (only 3 files have those lists), an effect stopped. **Doubts:** (a) stage 7 finds no target for 57 of 481 names: not checked against the ROM, `findTarget` was not compared, it needs real models; (b) `drawSysBuildShadows` sets a far character's `clipped` and only the area clip clears it, so on a stage with no area clip file it would stay hidden: every one of the 20 files has the list, but which stages have no file at all was not checked, and nothing else in the host clears it. **Left beside it:** unassigned@002e00c0, four rena light virtuals of 0x31a720, nothing to do with this class.

**RcntTimer is written, and the timer modes do not reach the host (a scheduled run, 2026-11-07): `done`, all 4 functions of game/rcnttimer.cpp, 0x12f190..0x12f290.** Harmless today, one gap for later: `REG_RCNT0_MODE` and `REG_RCNT1_MODE` are plain variables in sdk/eeregs.h, and nothing calls `InterruptController::setTimerMode`, so both host timers count the bus clock instead of H-blanks. RCNT1's count is never read in the ROM, so that one cannot matter. RCNT0's can (`rxThReadHSync`, set to H-blank at src/rx/rxthread.c:863): if anything ever depends on it, route the mode writes through `setTimerMode` the way the count already goes through `EeTimerCount`. Not changed here: it is a platform change with no symptom behind it.

**The gun sights draw: game/menuprogset.cpp is written and agrees with the ROM vertex for vertex (a scheduled run, 2026-11-06, batch 188; architecture.md "The gun sight"): `verify`, all 11 functions of 0x2b9ef0..0x2bb600.** It was stubs: looking down a scope showed nothing but the zoom. The critters' walk now calls the real `quatBetween` instead of a copy. **Proved:** new `tools/reverse/sightcmp.py`, 400 random frames of both sights on the ROM's code and the host's, the ROM's GS packet walked: 30,600 vertices, 0 differences (texture, blend, test, prim, colour, UV, XYZ); 3,000 `quatBetween`, 0 bits; `--plant`: 3,158. eerun.py gained `plzcw`. **Seen, temporary harness, removed (YAMIPAN_TELEPORT=2, the state forced):** both sights at 4:3, 16:9 and 21:9: round hole, cross, spot, no line at the frame's edge; progress/20261106-sight-r64c-16x9.png, -4x3.png, 20261106-sight-rifle-16x9.png. **Not seen:** a real `setScope` with a real gun (the state was forced), the zoom moving the ticks, nothing to compare with from a real PS2. **Doubts:** the ROM sets neither CLAMP_1 nor TEX1_1 here, so repeat and linear are what rena's passes leave, not something read; the mask's art needs both. Beyond the ROM on purpose: the mask's outer column out to the display's edges, its far UV ended at the outer texel, nearest ticks and lines. **For the user's eye:** at wide aspects the rifle's mask goes on at the edge's own 78% black rather than opaque; does that look right? **Next:** a stage with a rifle, the real up-of-the-d-pad into the scope.


**A thrown item or a dropped weapon falls now: game/itemphysics.cpp is written and agrees with the ROM bit for bit (a scheduled run, 2026-11-06, batch 187; architecture.md "The items' physics"): `verify`, all 15 functions of 0x23c010..0x23de20.** The class moved out of game/stageitem.h; `ItemPhysics::draw` drew nothing and is `endFrame`. Until now these were stubs: no body existed and nothing could fall. **Proved:** new `tools/reverse/itemcmp.py`, random lives on the ROM's code and the host's: 6,046 ops, 8,946 rays, 115 landings, 12 rests, 0 differences; `--plant`: 1,158. eerun.py gained the two-register `mult`. **Seen, temporary harness, removed (YAMIPAN_TELEPORT=2):** a kitchen knife forced into the air falls to the path, stops, lies flat with its handle and blade, progress/20261106-item-knife-landed.png; dropped beside the player, the game's own "Pick up" comes up. **Not seen:** a real throw or drop (only the same calls forced), a thrown item (stage 2 loads none), its touch sphere, the landing sound. **For the user's eye:** does a thrown can bounce and settle as on a real PS2? The numbers agree; nobody has watched the original beside it. **Next:** a stage with a thrown item and the throw from `ThrowLink`, to see the real path.

**The effects' draw order is the ROM's now: MSL's own sort replaces `std::stable_sort` (a scheduled run, 2026-11-06, batch 186; architecture.md "The effects' sort"): game/stageparticle.cpp, `verify`.** unassigned@001920d0 was no file of its own: the MSL vector and sort instances the object of stageparticle.cpp emitted, now pinned there, 0x1920d0..0x193900, all 17 functions named. New `game/mslsort.h` holds the sort (0x192150, 0x193480, 0x1935b0); the vector instances stay `std::vector`, listed in the .cpp. Why it matters: a key of 0 (ParticleFlag_NoFarLimit) makes ties common, the sort is not stable, a counter (0x31a8f0, starts -4) moves its samples from call to call, and the ROM sorts the far list first; the host sorted near first and stably. **Proved:** new `sortcmp.py`, the ROM's 0x192150 on eerun.py and the template in a small exe: 400 lists (198 through the quicksort), 18,951 elements, 0 differ, the counter running across all; `--plant` (a counter that steps once) makes 133 differ. **Seen:** YAMIPAN_TELEPORT=2, 60 s, no crash: the path with the rain streaks and motes over it, as progress/20261106-particles-rain-on.png. No picture can show the tie order by eye, so `verify`. **Left, same question, other instances (`mslsort.h` is ready for them):** the host still uses std::sort or std::stable_sort where the ROM has its own instance: 0x1de2f0 characterbrain.cpp, 0x2da2e0 drawsys.cpp (shadow keys), 0x258f80 routeplayer.cpp (it has its own counter, 0x31aae4). Not read: whether each is this template, and whether ties reach them. Done since: 0x21e120 playermotion.cpp (pickSearchPoint); 0x2a0040 and 0x29fdb0 scriptrunner.cpp (2026-11-07, scriptPickNamed and missionPickSpawnPoint, both proved with sortcmp.py --instance).

**The stage's alarm is written and agrees with the ROM: a script's `alertOn` runs the ALERT rumble, a foe or a cry flashes the screen red (a scheduled run, 2026-11-06, batch 185; architecture.md "The stage's alarm"): game/stagealert.cpp, `verify`, 8 of 8 functions.** The class is in game/stagealert.h now; `GameOptions::_10` is `alertMode` (`GameAlertMode`), `GameValue_Option10` is `GameValue_AlertMode`, the two constants of characterbrain.cpp are gone. **Proved:** new `tools/reverse/alertcmp.py`, random scripts on the ROM's own code and the host's: 61,078 ops, 0 differences, and it catches both planted bugs. eerun.py's `div.s` by 0 now gives the largest float, as the EE, where it gave 0. **Seen, temporary builds, removed:** on stage 2 the forced flash is a dark red veil over the whole frame, and a forced `show` starts ALERT, counts, stops it, goes idle. d2303 and d0401 call `show` from their own scripts; stage 9 does not. **Not seen:** a real `flash` (it needs the option at 1, default 2) or `cry`, and the forced flash on a wide window, though the fill is the same `gsFillRect` as the scene's fade. **For the user's eye:** no in-game reference says how red the flash really looks; the ROM's slip that ends Ending after one update is kept and marked in the code.

**The memory streams are written and match the ROM offline (a scheduled run, 2026-11-06; architecture.md "Memory stream"): rx/rxsndstrmem.c, `verify`.** All 16 functions of 0x1660e0..0x166b50, no name changed. **Proved:** new `strmemcmp.py`, the ROM's own functions on eerun.py against the host in a small exe, random scripts over EE and IOP streams, marker passes, callbacks and tags: 22,805 ops, 498 callbacks, 0 differences, and it caught the two bugs planted to test it. eerun.py gained ldl / ldr / sdl / sdr. **Not seen in the game:** no memory stream opened in 60 to 70 s each of d0101, stage 5 and d0401, and no stream bank either; only `SePlayer::openStreamBank` leads here. Which stage or menu has an isStream bank is still unknown, and that is the way to see it run. **Next:** rx/rxsndstrpcm.c, then the audio milestone.

**The stream sources are written, and a file stream's real bytes now reach rxSndAdpcmWrite (a scheduled run, 2026-11-06; architecture.md "Stream sources"): rx/rxsndstrstream.c, `verify`.** All 18 functions of 0x1656e8..0x1660e0, no name changed. **Seen with `YAMIPAN_TELEPORT=d0101`, temporary logs, removed:** the event's voice opens, is read to its end, closes when its timer ends, the looping ambience opens, the room is on screen, no crash in 110 s. **Not proved:** no branch or mark callback fired in 100 s, and that run does not show whether d0101 has any, nor whether st01amb1 reached its loop point. Nothing is audible: the block "the driver's silent side" in rx/rxsndstradpcm.c still decides when a voice ends. **Next:** rx/rxsndstrmem.c (its 16 functions are stubs, so memory bodies read nothing), rx/rxsndstrpcm.c, then the audio milestone.

**The ADPCM stream format is written, and the stream's silent end now comes through it (a scheduled run, 2026-11-06; architecture.md "PCM format ... ADPCM format"): rx/rxsndstradpcm.c, `verify`.** All 10 functions of 0x164c38..0x1656e8, no name changed. The bridge of 2026-11-04 is gone: `rxSndStrAdpcmOpen` opens a real `rxSndAdpcm` stream, and the block "the driver's silent side" in the same file answers for RXSNDDRV through `rxSndAdpcmUpdate`, from the request's length, so `rxsnd.c` is the ROM's again in `rxSndAdpcmGetPosition`. **Seen (YAMIPAN_TELEPORT=d0101, 75 s, temporary log, removed):** the voice opens paused, runs from 48000 Hz stereo, position 2797714 bytes at the end, ended at 51.03 s as the 51.02 s the bridge gave, "Teleport: d0101 over", the room is on screen, and a looping stream carries on. **No sound and no data yet:** `rxsndstrstream.c` is still stubs, so the feed reads nothing, marks and branches never fire, and `dataWritten` stays 0. **Next on this road:** rx/rxsndstrstream.c and rx/rxsndstrmem.c (the sources), rx/rxsndstrpcm.c (still hangs a wait on a PCM request, hazards.md "An event that never ends"). When the sources are real the feed writes to the fake IOP address and the report says the buffer is always empty: harmless, but check a source's ring drains without spinning. `callcheck.py` now skips keywords as definitions: a `fail:` label at the left margin read as a function called `if`.

**The elevator is written and rides: a call brings it, the doors work, a rider goes up with it, a door closing on someone opens again (a scheduled run, 2026-11-06, batch 184; architecture.md "The elevator"): game/stageelevator.cpp, `verify`.** `StageElevator` for the model oZS99_01, 27 functions 0x2939b0..0x295470; the free `elevator*` functions are its members now and `getZs9901` answers the class. The two motion file readers 0x2937e0..0x2939b0 joined game/playermotion.cpp. **Seen on stage 9 (YAMIPAN_TELEPORT=9, temporary probes, removed, three runs of 45 to 60 s, no crash):** with the pad alone, Cross at the B3 panel, facing it: the car came down, called 4, state 5, 2, 1, lamp out on arrival. `canCall` true facing the panel, false with the back to it. A player put in the car at B3 and `press(InsideB1)`: `floorObject` is the elevator, z follows the car from -1080 to -480, the rider is noted and cleared. A player put in the doors at B1 while they closed: the next frame state went from closing to opening, and they closed again. progress/20261106-elevator-b3-panel.png; the car's inside, its panel and lamps also showed in a shot since overwritten. **Not seen:** the ride started by Cross inside the car, a character other than the player as rider, 0x1cd release, the sounds and the rumble as such (the calls ran, nobody listened), a wide display, a real pad. There is no picture of the real game, so `verify`. **Doubts for the user:** (r) the names are the port's: StageElevator, ElevatorButton, `isOpen`.. from the steps; (s) ENV_SL61 / 62 are read as the landings' lights from their names and from `setTreeScriptFlag` only; (t) the probe's player walked 100 units across the car after the doors reopened, with no pad: not looked into, and it may be the script.

**The stage's menu is written and on screen: Triangle opens it, "Shout" is its first line (a scheduled run, 2026-11-06, batch 183; architecture.md "game/stagemenu.cpp"): game/stagemenu.cpp, `verify`.** The list window 0x208d40..0x20acb0, its fades 0x22fea0..0x2303c0, `StageMenu::update`, `draw`, `destroy`, `setData`, `addLine`: 29 functions. The frontier's unassigned@0022fc50 was two things: six MSL vector helpers, now listed in game/characterroute.cpp, and these fades. **Proved:** `tools/reverse/menucmp.py` plays random menu frames on the ROM's own window and the host's: 16 scripts, 355,157 lines, 10,689 plates, 9,967 texts, 62 picks, 0 differ. It found one slip: `clear` must leave `enabled`. It does not cover `StageMenu::update` (the pad, the focus), read from the code only. **Seen (YAMIPAN_TELEPORT=2, scripted Triangle and Cross, 135 s, no crash, a temporary log, removed):** the script asks for one line, order 7, enabled; the orange plate fading to the right with black "Shout" on it; a second Triangle closes it, a third opens it again, Cross picks and the text pointer comes back. **The final build, stage 7, 125 s, no crash:** the same plate, progress/20261106-stage-menu-shout.png, and Down with one line leaves it as it is. **Not seen:** what the script does with the pick (no shout was recognised in the shots, and whether the pick reached the script's `isMenuPick` was not logged), two or more lines in a run, grey or half transparent plates in a run (only in menucmp), a wide display, a real pad. There is no picture of the real game to compare with, so `verify`, not `done`. **Doubts for the user:** (p) the names of the looks and of `StageMenuLine_Leaving` are readings of the code; (q) show flag 3 is also `kShowFlagNotFalling` in game/playermotion.cpp: one of the two names is wrong, or the byte does both.

**All 43 names of the ROM's links now exist: the last eleven are written, none has played its real motion (a scheduled run, 2026-11-06, batch 182; architecture.md "The stage's own links"): game/playermotion.cpp, `verify`.** GadgetLink (SigLamp, NumLock, SevenKey), SpFear, SpRing, SpRobo, SpDrop, SpPadlock, SpNumLock, BossAttack, BossIdle, 36 functions. **Seen in logs, temporary harness, removed (YAMIPAN_TELEPORT, the player's request forced at frame 200, on stages 9, 7, 21, 2, 14 runs of 30 to 70 s, no crash):** on stage 9, stood by oZS09_04, the walk ends on it (2963.7 2013.1), 0xc8, 0xc9, a forced 0xca, back to standing; on stage 7 oDC07_05 comes to the player and shows; on stage 21 SpDrop runs and oMK21_09 is back at 11214.5 8558.16 2198.22, inactive and hidden; on stage 2 the player walks to oMT02_30, 0xd7, standing. **What that does not prove:** in those runs `requestedFile` answered null for every row asked (0xc7, 0xc8..0xca, 0xcb, 0xcc, 0xce, 0xd1..0xd3, 0xd7): the stage's default character holds no such motion, so the player's half never played and the bodies did not move, and no object motion was confirmed either. The motions are likely loaded for the character of the part that uses them, parts in the architecture note; 21.2 forced gave the same start and no file, so `.2` did not reach it or the file belongs elsewhere: not resolved. **Pictures:** nothing judged: the shots are the follow camera against the character's head (stage 9 shows a cord by the shoulder, stage 7 the top of the head); no reference. **Never ran:** SpFear, SpNumLock, the gadget links (SigLamp's class is a stub, and it builds on stage 9, so its GadgetLink exists), BossAttack, BossIdle, the sound EVE_S09_04 (P_AC01 was not found by find1 on stage 9; the code asks find6, not logged), the 0xc9 hold's end. **New stubs:** `SceneOwnedObject::takeMotionText`, `takeMotionUser`, `setRecordFlag31`, `numLockSpinDial`, `numLockSetDial`, `numLockDialsRight`, `numLockDialsTurning`. **Doubts for the user:** (a) nothing in the ROM read here sets `finished` in 0xc9, which is what ends the hold: a script is assumed; (b) `playNamedHeard2` in the ROM gets 800.0f in f12 and the host's has no such argument: this and the earlier callers drop it, not traced into `addSourceNamed`; (c) `CMakeSettings.json` appeared in the repo root at 18:55 during this run, from nothing this run started (Visual Studio?): left in place and listed in .git/info/exclude so `tus.py finish` does not commit it; decide whether it stays. **Next:** the file has no unwritten function left, so a run should drive the real scripts: play 9.1 or 10.1 to where sp_ron or sp_fear fires; StageAlert and ItemPhysics stay the first TUs a plain fight reaches.

**A real edge works, and a character's voices no longer jam: the sound family is written (a scheduled run, 2026-11-06, batch 181; architecture.md "What a character sounds like each frame"): game/playermotion.cpp, `partial`; game/charactersound.cpp keeps `verify`, now 39 functions.** Written: `updateSounds` 0x2a4a40 (0x1874 bytes), `getBreathName` 0x2a2280, `stopBodySounds`, `stopSounds`, `initSounds`, `playFallDownSound`, `playGetUpVoice`, `playBumpSound`, `playJobSound`, `playIdleVoice`, `playPanicVoice`, `playMotionSound`, `hasAlteredVoice`, `isViewedThrough` 0x212300, `getSurfaceMoveSound`, `getSurfaceWallSound`. **The bug it closes:** with `updateSounds` a stub, nothing ever cleared `m_soundState`, so after a character's first voice with a priority `m_soundPriority` stayed and every voice of a higher number dropped out at the priority test, for the rest of the stage. **And:** `initSounds` a stub left `m_voiceOn` (+0x44c, was `m_44c`) 0, which `sayFound` and the frame's own words test, unless a script set it. **Seen in a log, temporary build, removed (YAMIPAN_TELEPORT=d2303, 70 s, no pad):** 17 sources and 2 cues let go, SE31_ATK and SE31_WND played by the foes themselves. **A real edge, temporary harness, removed (it walks the Root meshes for faces of flags 0x142, stands the player where `faceAhead` and `findEdge` answer, 30 frames later `requestNow` dm_r or dm_f: the blow is forced, the edge and the search are the game's):** stage 2, a face of flags 0x40 at z 1040: dm_r 0xa6, `findDive` drop -180, dv_f 0x94, 0x96 at z 860, endurance 60 to 33, FALLDW3 on surface 5, dm_rev 0xb2, SPC_GETUP played, idle; from behind 0xa4, 0x95, 0x97, 60 to 32, dm_rev_b 0xb1, idle (no Revive link, the data's); d2303 likewise, 100 to 73. **Nothing below the edge (d2303, a face at z 847, `blocked` 1):** 0x94 with `m_bfc`, `cam_dive` takes the view, a black view with one lit beam, then the game's own "Mission failed", Retry / Give Up. **So the earlier doubt on landing damage is answered: a 180 fall costs 27 or 28.** No crash in 12 runs of 30 to 70 s. **Not heard:** nobody listened. **Never ran:** BOSS1_*, SRT_MOVE, YRT_MOVE, the surface move sound, the _J breaths, `playBumpSound`, `playJobSound`, `playMotionSound`, a blow from a foe at an edge (the harness's is forced), the turn round bn_pelvis (those edges have head room), the partner's camera, the ladder's fall. **Reached stubs on this path:** `ItemPhysics::setMoving` 0x23cfc0 (needs 0x23d0c0), `StageAlert::show`, `::update`, `::draw`, `StageObject::cameraStep`. **Next:** the 11 links left of this TU (SigLamp, NumLock, SevenKey, SpFear, SpRing, SpRobo, SpDrop, SpPadlock, SpNumLock, BossAttack, BossIdle) as stages reach them; `StageAlert` and ItemPhysics are their own TUs and both now run on a plain fight. **Doubts for the user:** (a) `hasMaleModel` tests name[4] of the pose for m, and only `playJobSound` asks: the sex is a reading; (b) 8 of 10 SE.._WND calls got false from `playVoice` in that run, the reason (no free channel, or no cue on the stage) was not read; (c) `m_winded` is read as 0 or 1 so far, so NPC_BRH_H / NPC_BRH_H_J (index 2) would never play: nothing found that writes 2; (d) on sheet kind 0xd, surface 0 means silence, the state doubling as "none": the ROM's, kept; (e) the bite's source stops only for sheet sounds of the enemy kind with `resumable` set, as the ROM reads; no bite ran; (f) the frame's words start with `m_voiceDelay` up to 290 frames, so a stage's first seconds have none.

**The dive and the trap are written: a fall over an edge ends in the game's own Mission failed, and a trap holds and lets go (a scheduled run, 2026-11-06, batch 180; architecture.md "Falling over an edge and being caught"): game/playermotion.cpp, `partial`.** `DiveFallLink`, `DiveLandLink`, `TrapHitLink` with their nine ROM functions and the three dtors; 32 of the 43 link names now exist. The file's slice is closed too: the dtors and empty slots that sat in the header are in the .cpp, where the tools count them. **Forced, temporary harness, removed (YAMIPAN_TELEPORT=2, a request on the player at takeHits' 450th call, the player lifted first, so no real edge):** deadly, 450 up: 0x94, 0x96, dm_rev, idle, endurance 60 kept (the fall ended before `deathFrames`). Deadly, 4000 up: 0x94, endurance 0 after the flight, 0x96, dm_ded, then the game's own "Mission failed", Retry / Give Up; `cam_dive` took the view, progress/20261106-dive-camdive-forced.png, a view low over the path. Back dive, not deadly: 0x95, 0x97, dm_rev_b, idle. Trap with no trap object: 0x9b held for 293 calls, 0x9c, idle. No crash in 5 runs of 45 to 55 s. **Never ran:** a real edge (`findDive` from a blow), a real trap (`takeTrapHit`, `startLinger`), the shaken stick, the turn round bn_pelvis (only with no head room), the partner's camera, the ladder's fall. **Reached stubs this opened:** `playGetUpVoice`, `playFallDownSound`, written since, and a real edge has run since (the paragraph above). **Doubts for the user:** (a) the survivable forced dive took no landing damage, because the lift does not move `floorHeight`, so `takeLanding`'s drop was about 0: landing damage stays unproven; (b) in the 4000 run the top view was still not the main camera at dm_ded with `m_bfc` 0, while in the 450 run it was back: not read whether the game over took over or `onCancel` did not remove it; (c) show flag byte 3 has only the script's property and 0x230400 as readers, its name `kShowFlagNotFalling` says when it changes, not what it does; (d) a thin bright horizontal line near the top of the shots during that forced camera is not explained, and the forced height makes the view itself no reference; (e) `TrapHitLink::onLeave` tests the trap for null where the ROM does not, for a script's own trp_s.

**A blow that lands now fells what it hits, and the first real fight runs to the game's own game over (a scheduled run, 2026-11-06, batch 179; architecture.md "What a hit does"): game/playermotion.cpp, `partial`.** `PlayerMotion::takeHits` 0x1e18c0 is whole (0x25c0 bytes, split in the host into `takeLightHit`, `takeBlowHit`, `landBlow`, `reactToBlow`, `takeTrapHit`, `takeLanding`, `takeShoves`), with `takeBlow` 0x2103d0, `takeFallDamage` 0x20e770, `rumbleHit` 0x20e6e0, `playerEvent` 0x20e6a0, `MotionProbe::spawnBlood` 0x26a010, `playDieVoice` 0x2a2e30. **Seen, `YAMIPAN_TELEPORT=d2303`, no pad, a temporary log, removed:** F_E23_YO1 hits F_PC23 seven times (blood BLOOD, count 3, radius 15, dm_r 0xa6 each time, endurance 100 to 0), then dm_c 0xb0 at endurance 0, `playDieVoice`, and the game's own "Mission failed", Retry / Give Up (progress/20261106-first-real-blow-d2303.png is a frame of the fight). **Runs:** before the fix 2 of 2 crashed on the NaN; after it 4 of 4 d2303 runs of 60 to 70 s and 1 of stage 1 (120 s, work/doorwalk_pad.txt) had no crash. **The stage 1 picture was not looked at** (its last shot was caught half-written and the next run overwrote the rest): whether a door now hurts the walker (`atDoor`) is not known. **Never ran:** shots at a character (head shots, `flag3ce`), the car's blows, a light hurting (`takeLightHit`, Motion_Burn), a trap catching (the link is written since batch 180, a real trap has not run), landing damage, the climber's hit. **Reached stubs left on this path:** `ItemPhysics::setMoving` 0x23cfc0 (a dropped weapon; it needs 0x23d0c0), `hasAlteredVoice`. **Next was** the links DiveFall, DiveLand, TrapHit: done, batch 180. **Doubts for the user:** (a) `takeBlow` adds the *victim's* 打撃攻撃力修正値 (+0x6c0 of s4, the victim) to a weapon's damage, where `landBlow` adds the shooter's 射撃攻撃力修正値: read from the asm and kept, it looks like a slip in the ROM; (b) a shot on a bone outside the table takes whatever s3 held in the ROM; the host says body; (c) part 4 is the left thigh's and goes to `m_rightLeg` (+0x5a4, 右足): the ROM's, or the columns are named the other way; (d) `Motion_CarDamage` 0xbf and `Motion_CarCollapse` 0xc0 are named from the code, their sheet labels unread; (e) the blue-green cloud in that frame is taken to be an effect of the fight, not proven to be BLOOD.

**A character can be hit, fall, lie down and get up: five of the missing links are written, and nothing in the game starts the fall yet (a scheduled run, 2026-11-06, batch 178; architecture.md "Being hit, lying down and getting up"): game/playermotion.cpp, `partial`.** Damage, DamageLand, DamageFatal, DamageDead, Revive, with their 0x277xxx code, `MotionCondition`'s 29 dtors, `setWallCheck` 0x1e5d60; `lowerLabel` is 0x1eb3b0. `m_reviveFrames` moved into `DamageDeadLink`, its real place. `flag3b6` is `divesOverEdges`, `m_3d8` is `m_dropsWeapon`. **Seen in a log, temporary build, removed (YAMIPAN_TELEPORT=2, 150 s, no crash, forced on three foes of kind 8):** dm_c with endurance 0 went to dm_ded, counted 1250 down to 0 at 25 frames a second, `takeCorpse` set -1, and that foe was in dm_rev with endurance 1 when the run ended; dm_car_b and dm_lnd with endurance left went dm_rev, then idle. `scriptHurt` reactions 1 and 2 ran Damage on four characters, dm_f_h, and walked on after it. **Not on screen:** three runs and no shot frames a foe lying clearly: progress/20261106-damage-foe-falls-left.png has a hooded shape low at the left edge that is standing in the shots before it, not proven to be the fallen one. The poses are not judged. **Never ran:** the edge search and dive (no log line of it, and DiveFall has no link yet, so a dive would run with none), the arm-lost weapon throw, the car-floor rays, `shiryodead`, `yamidead`, Yrt's push body. **A real result on the way:** the first run's forced blows also fell on a kind 1 character, and it ended on the game's own "Mission failed", Retry / Give Up screen; so that road works; which death failed it was not read. **Doubts for the user:** (a) `m_loop` is called the pose's update flag, and DamageDead sets it 1 after 2 s to mean something nearer "stop it": the name may be backwards; (b) the host guards `pushBody` and `wallBody` against null where the ROM writes straight through (PC); (c) the shot of the girl still has the hard white shards at the lamp, the earlier doubt; (d) `Motion_ReviveBack` has no link in the sheet, so a body rising from dm_ded_b runs no Revive code: the data's, kept. **Left of the links:** DiveFall, DiveLand, TrapHit, SigLamp, NumLock, SevenKey, SpFear, SpRing, SpRobo, SpDrop, SpPadlock, SpNumLock, BossAttack, BossIdle.

**Getting past stage 1 comes first (the user, 2026-11-05, live: "add a top priority to this").** The stage ends, the navigator works and the next mission starts (Done). The road is node 1 -> film node 38 -> node 4; `node(1).link` is 38, 31 and 38's link 0 is 4. The user played node 4 past gaining control and turning left (Done). **Next: play on from node 4.** `SceneDirector` and `GameOverSequence` are written (Done). What will show up next is its stubs: no brain walks yet (`CharacterRoute`), and the party's view (`StageParty`, below).

**What gives a brain its scripts was already there (a scheduled run, 2026-11-06): the earlier "next" was wrong.** Native 0x2f and `scriptSetBrainEntry` 0x21b970 are written, and the four `setSlotEntry` calls of the old note are that function. In node 4, 230 s with temporary logs: F_NPC04, F_E04_SB1, F_E04_SRT1, F_E04_SRT2 all get their entries. `F_E04_SRT1`'s entry 52 in `scripts[1]` is only "wait 30, restart", so the Srt stands by design, and no `runCommand` fired because nothing in those first rooms asks for one. **Doubts for the user:** F_NPC04 and F_E04_SB1 go to brain state 7 in the first frames because endurance is written 0: `Root` 0x8cc does it (property 0x3c below 2) and `F_E04_SB1` 0x10d4 too, so it reads as the script's, but no run shows when they get up; F_E04_SRT2 goes from brain state 1 to 0 at once, which fits native 0x17e in its entry; the condition before it was not worked through. The first foe that walks needs a stage or a later flag that wakes them, not more brain code. The temporary driver of the route run remains the only proof of walking.

**game/stageparty.cpp, the sight-jack, shows the view through a character (a scheduled run, 2026-11-06), still `partial`.** **Seen (temporary build, removed: New Game forced to stage 4, `UiShowFlags` byte 9 set to 1, tuned fill forced to 0.08, pad work/jack2_pad.txt, a stick sweep, 290 s, no crash):** progress/20261106-jack-view-sb1.png is the street through F_E04_SB1's eyes, brightened by `gsFrameScale`, grain over it. F_NPC04 tunes too (a dark near-blank view, not looked into). At the real level (tuned off the stick, 0.57 and 0.70) the grain hides almost everything; no in-game reference says whether that is right. **Written this run:** `gsFrameScale` 0x2cfb20, `gsFrameContrast` 0x2cf880, `gsFrameAdd` 0x2058e0 (host passes `YamiPan_DrawUiFrameScale`, `YamiPan_DrawUiFrameContrast`, blend modes DestinationScale, DestinationBoost), `jackMarker` 0x205f50, `drawBindFlash` 0x1ffcd0 (it is not an arrow: a side of the frame lights), `drawFoeMarkers` 0x201520, `drawFadingMarker` 0x201380, `drawWarning` 0x201840, the game over's lists, `StageCharacter::canBeJacked`, `isJackBlank`, `canSeeVisions`. **Not seen:** no cross came on screen in the shots looked at (the viewer is out of those views), so `jackMarker`, the bind flash, the tints, the warning and the fading cross have run at most, not been judged. **Why node 4 does nothing on L2:** byte 9 is 0 there, by its script (the paragraph above); it does two things: `stepAlt` returns, and `CharacterPad::update` blanks the stick while the party runs. **Written since (a scheduled run, 2026-11-06, batches 159, 160):** the palette views `SightJack::drawPaletteView` 0x1ff010 and the film `JackFilm::draw` 0x204550 (architecture.md has both), the sound KAKOSI that `setVisionTarget` had lost, `JackFilm::clone` for `SceneModelHolder`. The film's range joined this TU: it lies between its functions. **Seen, temporary build, removed (New Game, work/doorwalk_pad.txt, the passes forced over the first room on a timer, 245 s and 280 s, no crash):** sepia and grey ramps over the wall (softened as the half-size pass should, grey matching the plain frame in layout); the negative ramp, progress/20261106-jack-negative-ramp.png, with the jump grey to pale blue at index 0x10 that the ROM's table has; the film at levels 0, 1/3, 2/3 and 1 in a temporary white, progress/20261106-jack-film-test-white.png: a soft ring closing in from the frame's edges, mirrored about the centre, more of it at each level. So the film is the vignette that narrows the view with distance, in black in the game. **Not seen:** any of it through a real jack (byte 9, above), so the film's black on a live view, the three stages' ramps and a vision are not judged. The film frames were not extracted, so there is no reference picture of them; the seen ring is the only evidence. **Doubts for the user:** (h) the pass halves the render size, not 512: at a high resolution the view is sharper than the PS2's 256 wide; (i) the ramp and the film stretch over any aspect, as the grain does: the film is a ring that takes the frame's shape; (j) the host looks up the film's alpha after bilinear, the GS before. **Next, in order:** (1) was the flecks, done: game/demofleck.cpp, `verify` (a scheduled run, 2026-11-06, batch 161; architecture.md). **Seen, temporary build, removed (New Game, work/doorwalk_pad.txt, the flecks forced over the first room in 180-frame phases off / real / alpha times 6, 290 s, no crash):** progress/20261106-flecks-forced-boost6.png shows dark blotches and small clots on the wall, gone in the off phase; progress/20261106-flecks-forced-real.png, the real level, is hardly visible on that dark wall. **Not seen:** a real jack or event overlay, and any wide display (the shots were 4:3, `YamiPan_DrawUiSqueezeFactor` was 1). **Doubts for the user:** (k) the sheet is taken to be texture 83c6a44efe777343 (work/ref/texdump, 512 by 64, 4-bit, speckles): its size fits the ROM's cells, but the temporary log that would have said so was filtered out by build.py, so it is unproven; (l) the ROM never sets TEX1_1 or CLAMP_1 here, so the flecks take what the list left, and the host's list may differ from the GS's; (m) each fleck keeps its shape at a wide display while its place spreads over the frame, which is the port's choice, not the ROM's. (2) was the palette views, done. (3) was the film, done. (4) `SceneModelHolder::setPlace`, done (`StageDaylight`). (5) `KeepList::addNamed` 0x2bc3c0 (PETITJACK), done (`RumbleList`). (6) Stage 0x1b's boss film 0x2d10f0. **Also new from before:** `ScreenFill` draws the scene's own fill too, alpha 4 over every stage frame, 2 x 81 tiles a frame on the jack and 81 on the scene. **Other motion links the ROM has and the host does not:** DiveFall, DiveLand, TrapHit, Damage, DamageLand, DamageFatal, DamageDead, Revive, ApproachCar, ApproachOut, Drive, SpWait, NPCReaction, SigLamp, NumLock, SevenKey, SpFear, SpRing, SpRobo, SpDrop, SpPadlock, BossAttack, BossIdle (keys of the ctor's map, 0x1eeXXX; `findLink` answers null for them): worth a look before blaming anything that should start in a motion. **Doubts for the user:** (a) `m_film`'s fields are copied from the ctor and unread; (b) stage 0x1b's object at +0xb4 (0x2d31d0) is not built; (c) `SightJack::reset` ends a vision before clearing the viewer, where the ROM reads through a null viewer; (d) the +0xb8 modes 3 and 5 are named from the stick tests alone; (e) `isJackBlank` off stage 0x12 tests a register the ROM never set: the host answers no; (f) the ROM clamps once after `gsFrameContrast`, the host twice, so a bright pixel can end a little darker; (g) `jackMarker` keeps the ROM's rays at 4/3 of the width, a shape for the PS2's pixels, under the squeeze.

**The real sight-jack is on screen, and byte 9 is answered (a scheduled run, 2026-11-06).** Node 4 cannot jack by design: its `Root` locks `Control_Alt` for good, as E01 and E05 do; E02 and others never touch it (architecture.md "Which stages let the player jack"). **Seen, temporary build, removed (New Game forced to stage 2, work/jack2_pad.txt, 290 s, no crash, no flag forced):** the hint page, progress/20261106-stage2-hints.png, whose first line tells the player to sightjack; L2 gives noise with the film's black ring over it, progress/20261106-stage2-real-jack-noise-film.png; the stick tunes a real view through a character, with grain and the ring, progress/20261106-stage2-real-jack-view.png. So the film in black on a live view, which earlier runs could only force in white, is now seen. **Not judged:** whose eyes those are, whether the level is right (no reference from the game), the crosses, the bind flash, a vision, the jack's three stages' ramps. **Forcing the stage is the way to test what stage 1 and 4 cannot show:** temporarily change `getStageSelect()->stage = 1` in `menuscene.cpp`; stage 3 boots too and plays as Shigeru Fujita, progress/20261106-stage3-dredge-inside.png, and the story's order is not needed for it.

**`SceneModelHolder` is `StageDaylight`, written (a scheduled run, 2026-11-06, batch 162; architecture.md).** It is in game/stageparty.cpp, range joined, and `StageScene::model` is now `daylight`. **Seen, temporary build, removed (New Game forced to stage 3, work/doorwalk_pad.txt, 285 s, no crash, the answer forced on a timer, film with 4 frames):** the pale added haze at its peak, progress/20261106-daylight-forced-peak.png, and settled, top-weighted, progress/20261106-daylight-forced-settled.png, then going. The film's ring on going in was not caught clearly: a shot about 1 s in shows no plain ring. Without forcing, stage 3's first room is inside all the way, so the real game shows nothing there. **Not seen:** a real walk through a boundary, stages 0x11..0x1b, a wide display. **Doubts for the user:** (n) the name is a reading of the colours; (o) the ROM's strips are 512 tall from the window's top and the host's frame is 512 tall too, so the bottom colour lands on the frame's bottom only as far as that frame is the GS's; (p) the peak is a strong haze, about a quarter of white at the top: nothing but the ROM's table says that is right.

**`KeepList` was the pad's rumble, written (a scheduled run, 2026-11-06, batch 163; architecture.md "The rumble"): game/rumblelist.cpp, `verify`.** Class now `RumbleList`, `StageScene::rumble`, `removeUnkept` now `removeUnlooped`. `tools/reverse/rumblecmp.py` played random scripts of add, removeNamed, isPlaying, removeUnlooped and update on the ROM's own code and the host's, on the disc's file: 40 scripts, 24,000 ops, 2,547 updates with events, 0 differences. **Seen in a log, temporary build, removed (New Game, work/doorwalk_pad.txt, 250 s, no crash, ALERT forced every 300 frames and removed 90 later):** the motors 70 and 1 and the event byte reach `updateRumble`, with the vibration option on. **Not felt:** nobody had a pad in hand, so the motors on a real pad are unproven, and no track has started by the game's own call in a run. The host now really rumbles on a sight-jack warning and a vision (PETITJACK, KAKOSI). **Doubts for the user:** (q) `updateRumble` advanced ALERT about 1.2 frames per call in that run, a 40-frame pulse every 33 calls: the `frames` argument's rate was not looked into; (r) ELEVATOR1 and ELEVATOR2 have empty step lists, ELEVATOR2 with the loop flag, so both end on their first frame: the data's, kept; (s) the elevator passes its +0x2f8 as the source, taken to be an object from the ROM's pointer compare, not read further; (t) a non-matching track's event bytes stay in the buffer until the next track overwrites them: the ROM's, kept, it is what the comparison shows. Next: stage 0x1b's boss film 0x2d10f0 (game/stageparty.cpp's `drawLive`, still `partial`), and `StageAlert` (all stubs) is now the first caller that would start ALERT by itself.

**The boss film was a sheet of water, written (a scheduled run, 2026-11-06, batch 164; architecture.md "The ripple sheet"): game/ripplesheet.cpp, `verify`.** It is 0x2d10f0..0x2d3700, its own object past `ModelMotion`. `tools/reverse/ripplecmp.py` ran the ROM's own code beside the host's on one generator: 200 frames, 45,000 points, one differs by 1/16 of a pixel (the EE's accumulator rounding), and every corner of the ROM's packet fits the rule the host lays the sheet by, the ROM's slip of one corner included. `drawLive` has no gap left: game/stageparty.cpp is now `verify`. **Seen, temporary build, removed (New Game, work/doorwalk_pad.txt, 285 s, no crash, the pass forced over the first room):** progress/20261106-ripple-forced-a.png and -b.png: the door and wall bent, blue glints moving over them, the picture pushed from the warm tint towards blue. **Reached on the real path, temporary build, removed (New Game forced to stage 0x1b, the jack2 pad with presses and stick sweeps added to read 16000, 560 s, no crash):** the stage boots, its card says Defeat Mother, the player stands in a red cave (progress/20261106-stage1b-boss-player.png), L2 gives the noise, and a temporary log wrote once target F_E27_BOSS, boss 1, camera z 895.3, so `drawLive` took the ripple's branch; it logged one line in 120 draws, so that view lasted under 120 frames and **no shot caught the real ripple: on a real boss view it is not judged.** A longer stick hold on the boss is the test. **Doubts for the user:** (u) the name is a reading of the picture; (v) the bend is in frame units, so on a wide display it stretches sideways with the frame, as the film and the grain do; (w) the host samples the frame with a bilinear filter, the ROM takes whatever TEX1_1 the list left; (x) the ROM leaves the height in the frame's alpha and nothing later in `drawLive` reads it, the host drops it. Next: `StageAlert` (all stubs), the first caller that would start ALERT by itself, then the car.

**The VU0 math is written and agrees with the ROM, and the stand-ins were wrong (a scheduled run, 2026-11-06, batch 165; architecture.md "The VU0 math"): game/vu0math.cpp, `verify`.** `tools/reverse/vu0cmp.py` runs all 33 ROM functions on eerun.py beside the host: 49 cases, 19,600 calls, 0 differ, 0 ulp, angles past pi, zero vectors and out on top of an input included. It compares with eerun's float model, not a real EE: denormals and overflow past the largest float are not carried over. **What changes in the game:** `rotateByAngles` multiplied the other way round (the ROM turns `in` by R, the host turned R by `in`), so `getLampMatrix`, `getEyeMatrix`, `getLampTarget` and `SightJack::aimAt` composed the look and the body in the wrong order whenever both turn. **This may be the odd upward view in the fight and aim stances left open under "The first-person view": a run with a pad in those stances should say so.** Also: `vecDirection` of equal points gave NaN and kept a w, the ROM gives zero and w 0; `vecNormalize` kept in's w, the ROM's is 0; `addScaledVec3` kept a's w, the ROM's is b's (no effect in `VectorTrack::sample`, which sets w after); `invertRigidMatrix` broke with out on top of m; `sinCosVec`, `sinCosPair` and the three `rotateMatrix*` used sinf / cosf and now run the ROM's two series. **Seen (New Game, work/doorwalk_pad.txt, 290 s, no crash):** the language screen, the title and the dark first-person wall, which reads like the earlier runs' wall (not compared pixel by pixel, no earlier shot of the same frame). No shot was taken in a fight or aim stance, so the fix itself is not judged. **Doubts for the user:** (y) past -pi..pi the ROM's rotations drift and the host now drifts with them, where sinf hid it: a host caller that passes an unwrapped angle the ROM wraps would show here; (z) `vecCross`'s w is a stale register in the ROM, 0 here; nothing was found reading it. `sdk/libvu0.h` keeps its own sinf / cosf `sceVu0RotMatrixX`, unused by this file. Next: `StageAlert`, as above.

**The motion maps are the ROM's own, and 25 links are missing (a scheduled run, 2026-11-06, batch 166; architecture.md "Motions").** The frontier's unassigned@001eedb0 was `BittenLink`'s dtor and three instances of one map: `LiteralMap<T>` in game/stagelist.h replaces `std::map` in `PlayerMotion`, and the range joined game/playermotion.cpp (0x1efef0, `getCollisionObject`, joined game/characterground.cpp). `tools/reverse/literalmapcmp.py`: 10,890 ops on the ROM's code beside the host's, 0 differ. Conditions and links are now built in the ROM's order. **Seen (New Game, work/doorwalk_pad.txt, 290 s, no crash):** the dark first-person wall as in earlier runs, not compared pixel by pixel. That run says nothing about the one behaviour change, the specials' walk in insertion order: it runs `clearAllowed` and `clearEnabled`, and whether their order matters was not looked into. **For the user to reorder: the ctor has 18 of the ROM's 43 link names.** Not written: DiveFall, DiveLand, TrapHit, Damage, DamageLand, DamageFatal, DamageDead, Revive, ApproachCar, ApproachOut, Drive, SpWait, NPCReaction, ApproachBite, SigLamp, NumLock, SevenKey, SpFear, SpRing, SpRobo, SpDrop, SpPadlock, SpNumLock, BossAttack, BossIdle. `findLink` answers null for those, so a motion naming one runs with no link code. **Damage, DamageLand, DamageFatal, DamageDead and Revive look like the ones a walking foe needs first; ApproachCar and Drive go with the car below.** Whether a null link is safe on those paths was not checked.

**game/stageparticle.cpp, the stage's particle effects and the critters, `verify` (scheduled runs, 2026-11-06, batches 168, 169; architecture.md "Particles", "Critters").** `spawn` answered null until batch 168, so no effect in the game existed. **Seen, temporary build, removed (New Game forced to stage 2, work/doorwalk_pad.txt, 285 s, no crash, drawing toggled every 60 ticks):** rain, thin streaks falling over the woods and the wall, progress/20261106-particles-rain-on.png, and the same woods without it, progress/20261106-particles-stage2-off.png. So the whole path works and the earlier blank screens were only stage 1's first room: a dark wall with no lamp and no rain in view. About 350 quads a frame reach the host there. There is no picture of the real game to compare with, so `verify`, not `done`. **Motes were critters:** `spawnMotes` is now `spawnCritters`, class `CritterNest` 0x2b3ef0 with `CritterRat` and `CritterCrawler`, all written, native 0x20f live. Only 37 scripts call it: AREA01 and ALERT01, 02, 04 (none of it runs in a smoke run). **Seen, temporary build, removed (stage 2, nests forced ahead of the camera, 285 s, no crash):** progress/20261106-critters-forced.png, small brown shapes lying flat on the path, no box round them; another run's log shows them wander, turn, climb the slope and stay near the nest. **Not seen:** a critter flee and hide on screen (the log's states never reached Hidden in the view), a real script's nest, the sheet itself (texture index 12, not looked at, so the kinds' names Small, Rat, Side, Long are readings). **Closed:** Refract (flag 0x40) needs no code: tools/reverse/ptcall.py finds it on 0 of the disc's 766 definitions in 30 files, as it does Collides. `playSoundFrom` now has the ROM's branch for a label with no object. **Doubts for the user:** (a) ALPHA_1 0x49, Cd * As + Cd, 67 definitions, is drawn as DestinationBoost, Cs * Cd + Cd: exact only where a texel's colour equals its alpha, a shader flag would make it exact; 0x41 (`BLOOD`, 2 definitions) falls to Alpha; (b) the ROM's bytes +0, +1, +2 of the inner object (no spawn, no update, no draw) are not kept: nothing in the run sets them, the rest of the ROM was not searched; (c) `stopChain` was renamed `restartChain`: it restarts, so `StageCharacter::stopBodyEffects` may be misnamed too; (d) `setRunning`'s holders are trusted as in the ROM; `forgetEffect` clears the chain pointers of a deleted effect (PC); (e) the on-frame test's y window stays 4:3 on a display narrower than 4:3; (f) a critter's ALPHA_1 is whatever the ROM's list left: the host takes 0x44, and it takes the ROM's white at 0xff as MODULATE, so a sprite may be twice as bright as it should if the sheet's TEX0 says otherwise; (g) `CritterNest`'s ctor runs before the nest has its place, so the first critters start at the origin and fall until the range sends them home, and one that lands on something at the origin stays there: the ROM's, kept; a nest placed higher than its range above the ground drops them for ever in a loop, which is what the forced test did with some; (h) the critters and `spawnCritters` are pinned to this TU though the ROM has them far apart, for the user to split into their own file if wanted. `setVisible` 0x18c170 waits for SceneBlock98 and SceneBufferSet. **Forcing stage 2 is the way to see effects: its woods open with rain in view about 190 s into the same pad.**

**The map object's file is closed, and script slot +0x9c was not the car's (a scheduled run, 2026-11-06, batch 167; architecture.md "The map object"): game/mapobject.cpp, `verify`, 70 of 70 functions, no stub.** `StageObject::breakPart` 0x175900 was still a stub and is written, with `advanceMotion` 0x1755f0 and `StageMapObject::getProperty` 0x174b00. Slot +0x9c answers the object itself on every class on `StageMapObject`, so `getScriptCar` is now `getScriptMapObject` and natives 0x198 and 0x19a run on any map object, which the host's null answer had turned off. **Seen, temporary build, removed (New Game, work/doorwalk_pad.txt, 285 s, no crash):** a forced break over all objects at stage frames 300 and 600 found 43 objects and none with parts, so `breakPart` has never run: the first stage with glass is its test (break a window: the piece goes, glass%d_1 and BREAK_GLASS play, and on a door doaglass%d_1 from its bone). Natives 0x198 and 0x19a have not run either: only six scripts (ALERT01 at ROM.002 and ROM.006) call them. **Doubts for the user:** (p) `isGateModelName` is now `isCarModelName`, because createMapObject gives those two names (oGK99_01, oDC99_01) the class 0x28e5a0, kind 3, CAR_* sounds. `SubSceneGateObject`, `GateParts` and `StageObject_SubSceneGate` are therefore probably the sub-scene's car, but they keep the old name. (q) `sevenKeysPoseValue` 0x2b23b0 is a stub of a class not written; the name says where it is, not what the value means. (r) `StageCar` still stands on `StageObject` here, not on `StageMapObject` as in the ROM, which is why the slot is typed `StageObject *`: the car's run should change both.

**game/vectortrack.cpp is proven against the ROM (a scheduled run, 2026-11-06, batch 170; architecture.md "The tracks and the fades"), `verify`.** The frontier took it only because seven functions were inline in game/gamehooks.h, where the tools do not count them: they are in the .cpp now, and the classes in game/vectortrack.h. `tools/reverse/vectorcmp.py`: 60 scripts, 20,155 ops, 968,131 words, 0 differ. It found the host's Bezier weights and control points up to 48 ulp off (the order of the float work, no visible change expected). **Seen (New Game, work/doorwalk_pad.txt, 285 s, no crash):** the dark first-person wall as in earlier runs, not compared pixel by pixel. **Doubt for the user:** `VectorFade::step`'s guard for frames 0 stays: the ROM's 0 / 0 is the FPU's largest float, the host's answer is the end value, and vectorcmp.py does not test it. The source comment says nothing asks for it; this run did not check that. Next: `StageAlert`, as above.

**A character now answers four script slots it ignored (a scheduled run, 2026-11-06, batch 171; architecture.md "What a character answers on the runner's slots").** The frontier's unassigned@0017e140 was the list instance and 63 thunks; it joined game/stagecharacter.cpp, 0x17e860..0x17e950 joined game/stageplayer.cpp. Reading the thunks against the host showed `setPosition`, `onScriptExit`, `scriptPlayMotion` and `cmdIsLit` left on the default, so a script's warp of a character skipped motion, ground and brain, and playMotion on one did nothing. All written, status `verify`. **Seen, temporary build, removed (New Game, work/doorwalk_pad.txt, 285 s, no crash, the three forced on the player every 400 ticks):** the warp lands exactly, the motion goes to 0, the lamp reads 0. The second forced warp, 120 units through a wall, left the player falling (z -357745 by the third): my offset, not a finding, though it shows nothing catches a warp out of the map. Without forcing, stage 1's first room reaches none of them, and the screen is the same dark first-person wall, not compared pixel by pixel. **Not tested:** a brain's side of these, an uncontrolled character (`stopAndRetry`, `playActionAndRetry`): stage 4's F_NPC04 is the first candidate. **For a later run:** `py slotcheck.py Vt_00313bfc StageItem` says +0xfc 0x184770 is not bound on `StageItem` (a box's slot; native 0x44 on an item), not read yet. StageDoor's one report is a false positive. The other classes with a runner table have not been run through it.

**The car's and the elevator's poses are written and agree with the ROM (a scheduled run, 2026-11-06, batches 172, 173; architecture.md "The car and elevator poses"): game/vehiclepose.cpp, `verify`, 21 of 21 functions.** The frontier's unassigned@0027cab0 split: 0x27dc40..0x280010 is this file, the rest joined game/playermotion.cpp, where the links' base, the 17 dtors of the links the host has, `MotionGadgetSpecial`'s dtor and the ROM's own `MotionConditionList` (it replaces `std::vector`) are written. `tools/reverse/vehiclecmp.py`: 40 scripts, 6,040 ops, 515,686 words. **The elevator: 0 differ,** every button from every state, with the log of which motion plays. **The car: 670 words differ, worst 208 ulp,** all in `rootOffset[1]` and the body's aim target. Those are one-ulp differences of a ground height near 500 taken down to a small number, and the VU's accumulator and normalize rounding; not looked into further. **Not covered:** `CarPose::update` and `ElevatorPose::update`, which need real bones, and `powf`, Python's on the ROM's side. **Seen (New Game, work/doorwalk_pad.txt, 285 s, no crash):** the dark first-person wall as in earlier runs, not compared pixel by pixel; it is what shows the links still work on the new list. **Nothing on screen shows the poses yet:** the poses are made only for the car and elevator objects, which `createMapObject` does not build, so that object is what comes next for them, 0x28a890..0x28e5a0 and 0x294280... **Found on the way:** the library's `sceVu0RotMatrix` is not the game's `rotationMatrixXyz` (hazards.md); the seven links not written have their dtors named and waiting in game/playermotion.cpp's range: DamageDeadLink 0x27ccd0, DamageFatalLink 0x27cd50, DamageLandLink 0x27cdd0, DamageLink 0x27ce50, TrapHitLink 0x27cfd0, DiveLandLink 0x27d150, DiveFallLink 0x27d1d0. **Doubts for the user:** (p) the file name and the names of the buttons (`callInsideB3`, `callFloorB3`) are readings of the motion names: `bb` as inside the car and `fb` as the landing is a guess that has not been seen; (q) `elevatorIsAtB1` 0x294c70 and `elevatorIsAtB3` 0x294c30 still stand behind Lock's "always stops" note in `BtAttackLink::onStart`: they read this pose through a floor object, and go with the elevator object.

**The car stands on stage 7, with a wrong picture (a scheduled run, 2026-11-06, batch 174; architecture.md "The car"): game/stagecar.cpp, `partial`, 58 of 59 functions, 0x28e850 left.** `StageViewTarget` is gone (`StageScene::car`), `GateParts` and `PlayerGear` are `CarParts`, `createMapObject` builds the real class. **Seen, temporary build, removed (New Game forced to stage 7, work/doorwalk_pad.txt, 300 s, no crash):** the stage plays, the truck stands at (7387, 2739, 140.4) with the game's own prompt "Get in the car by pressing X", progress/20261106-car-stage7-away.png. **The picture is wrong:** in a close view the body shows a smeared, fanned texture with the glass over it. My guess is `PartSwitch::init` 0x187060 and `PartSwitch::show` 0x186fe0, both reached stubs, so every window and part shows at once; that is not proven. **Forced drive, same build (throttle 160, steer 0.25, no driver, so no collisions):** it pulls away, turns about 600 units wide at 30 units a frame, which is 32 km/h, and brakes to 0. After it left the road the contacts sank without end while the position stayed at z 79.8 (c0 z -2383): the ground ray starts 50 above a contact, so once a contact is more than 50 under the floor it is lost. With no walls that may be what the ROM does too; a drive with a driver is the test. **Next, in this order, for the user to reorder:** (1) the two `PartSwitch` stubs, 0x186ee0's run, which every stage reaches: then compare the truck with its textures; (2) `StageCharacter::rideCar` 0x17af50, 0x45c bytes, which is what writes `m_driver`: the stage 7 prompt is waiting on it, and `PlayerMotion::setCarSpecialFlag` 0x1dee90, reached, goes with it. **Not proven:** nothing here is compared with the ROM by a tool; a `carcmp.py` on the pattern of vehiclecmp.py could run `syncBody`, the pedals and `getHitLevel`. **Doubts for the user:** (p) the names `CarMode_*` come from the bones each mode switches, and slot names from the parts they show; (q) kind 3 in the sheet is left out of every character hit, `kStageObjRecordCarProof`, whose meaning is a reading; (r) the stage 7 node for a real New Game path was not looked at, the run forced `stage = 7`.

**Get in and drive work from the stage's real start, with no game code changed (a scheduled run, 2026-11-06; architecture.md "The car"): game/stagecar.cpp, `verify`.** The X that "did nothing" at the door was the pad script: X every 80 reads never fell inside the 45 frames the item list held the focus (hazards.md). **Seen, temporary build, removed (New Game forced to stage 7, 300 s, no crash, nothing forced but the stage):** X with the prompt up: `UiItemList::pick`, native 0x168, motion 0xbc, then 0xb9 in the seat, the driver's view. R1 held: throttle 255, speed 23 units a frame, the truck went from (7387, 2739, 140) to (6464, 3180, 78) with the player in it, which is also the first drive with a driver. There the game took over by itself: a film of the veranda with red glows, progress/20261106-car-drive-veranda-film.png; the mission text "Point the lights at the enemies near the veranda", progress/20261106-car-drive-headlight-hint.png; dark figures by the veranda through the windscreen, progress/20261106-car-drive-veranda-seat.png. **Pad for this:** car3's start, its X presses from read 5000 replaced by CROSS*2 every 37 reads up to 5700, then 5800:R1*700, 6100:LSRIGHT*120, 6500:CIRCLE*150, CROSS*2 every 37 from 6700. In these runs read minus stage frame was about 5200. **Not seen:** the headlights on the enemies: the Square run is void, because `build.py --run` rebuilt the exe from the source I had just restored, so it played stage 1 (hazards.md). Also not seen: getting out, a collision with a driver, steering (the LSRIGHT fell after the scripted stop or was not judged), the old sinking contacts on a real drive. **Next there:** forced stage 7, the pad above with Square at about read 6800 and 7300, and the headlights' effect on the veranda's foes. **Doubts for the user:** (p) a black box at the top left of the driver's view, about 120 by 40, in the forced shot too; its size changes with the view, so it may be a mirror, not looked into; (q) the car read speed 0 at once when the film began and stood there: whether a script stopped it or something in the host did was not looked at; (r) the walk back to (7410, 2839) stays unexplained: in the log it is a 320-frame cycle of the player between there and (7324, 2878) under a held stick; (s) the hint's focus came and went every 37 frames in the seat until frame 633, so X kept closing a window that came back; which window was not read off a shot.

**The truck's fan was a host bug in the lamp's pass, fixed (the user, 2026-11-06, live: "it seems to repeat the texture on which the light ends up on ... flips it and repeats it hundreds of times").** `drawOver` in yamipan/rendering/model_renderer.cpp took `picture` and only null-checked it: `binding.texture` stayed the surface's, so the spot's pass and the shadow's pass drew the surface's own texture at their own UVs. It now binds `picture`. **Seen, temporary build, removed (New Game forced to stage 7, work/doorwalk_pad.txt, 300 s, no crash):** progress/20261106-car-lamp-fixed.png: the cab is clean paint and rust, with the prompt, and a soft pool of light on the wall and the road. **This withdraws the guesses in the two paragraphs below:** the reflection layer, VU1 0x66b, the Gouraud alpha and the spot texture's clamp: the ROM and the host never differed there. **Not judged:** whether the lamp's strength, the white spot at the hand and the bright road match the game: no reference shows the lamp. The cabin's pillar was not looked at again against Hints 3. **The shadow's pass changed with this too:** it now uses the shadow's picture for the first time, and no shot was looked at for it, so "The shadows are written and have not been seen" below still stands.

**Sitting in the car works, and the wrong picture is not the part switch (a scheduled run, 2026-11-06, batch 175; architecture.md "The car"): game/stagecar.cpp, `partial`, 59 of 59 functions, no stub.** Written: `CarRunOver::at` 0x28e850, `PartSwitch::partName`, `show`, `init` (range joined game/stagemodel.cpp), `StageCharacter::rideCar` 0x17af50 (joined game/playermotion.cpp), `isDriver` 0x215310. **Seen, temporary build, removed (New Game forced to stage 7, work/doorwalk_pad.txt, 300 s, no crash, `rideCar` forced on the player, throttle 160 forced for a while):** the driver's view with wheel and dash, and the game's own text "Use R1 to go forward", progress/20261106-car-seat-forced.png; `m_driver` and `m_gear` set; the car moved off with the player in the seat; out again by the seat. **A reference at last:** the stage's hint page, progress/20261106-car-hints.png, Hints 3 is the game's picture of the same cabin view. The shape agrees: wheel low in the middle, pillar on the right. **Still wrong:** the windows now show one state at a time, but the rounded panels of the cab keep a scaled, glossy pattern, progress/20261106-car-close-after-partswitch.png, and the pillar inside has the same bright smear where Hints 3 has a dark pillar. So the handoff's guess was wrong. The new guess, not proven: a reflection layer (a mesh's second texture) in yamipan/rendering/model_renderer.cpp. **Not explained (for the user):** with the forced ride the car stopped dead at (7026, 3002) under full throttle, probably the fence ahead of it, not looked into; later, long after the forced exit, the car stood back at its map place with the player in the seat, so something other than this test seated them, and a run with nothing forced did not do that (`m_driver` stayed null for 2,400 ticks). That same plain run shows the player back at (7410.8, 2840) about every 480 ticks, whatever the pad did; that looks like a script placing them, and it was not looked into. **What is next for the car:** the three conditions `GettingOnCar` 0x280f50, `GettingOffCar` 0x280e20, `DragOutCar` 0x280a60 with their links 0x283060.., left out of `PlayerMotion::initList`: that is the prompt's real answer. Then the caller of the table, 0x2103d0.

**The truck's wrong picture is the lamp's pass, and the car's door code is written but no button has used it yet (a scheduled run, 2026-11-06, batch 176; architecture.md "The car"): game/stagecar.cpp stays `partial`.** **Seen, temporary build, removed (New Game forced to stage 7, 285 s, no crash, the spot pass switched off in `drawBatch`):** the cab is clean white rust and glass, progress/20261106-car-no-spot-pass.png, against the fan with the pass on, progress/20261106-car-close-after-partswitch.png. So the handoff's second guess, a reflection layer, was wrong too: the truck's materials have no second texture. The pattern also changes from shot to shot at the same place, which fits a lamp that sways. **Not found:** why the ROM does not show it. VU1 0x32d..0x347 was read: for a vertex behind the lamp the ROM does what `spotVertex` does (q 1, s = 0.5 + x * scale, no alpha), so that is not the difference. **Next for the picture, untested guesses, in this order:** the part's box against the cone, VU1 0x66b, which the host skips; alpha, whose test is As >= 0x50 on the GS with Gouraud alpha linear on the screen while the host's is perspective-correct, on panels whose triangles cross the lamp's plane; the spot texture's clamp. The spot pass is back on, so the truck is still wrong in a normal run. **Written:** `MotionGetOnCar`, `MotionGetOffCar`, `MotionDragOutCar`, the links ApproachCar, ApproachOut, ApproachBite, Drive, NPCReaction, SpWait, `findCarDoor`, `isPassenger`, `getCarSpecialFlag`, `setCarSpecialFlag` (both were stubs). **Seen:** at the door `allowed` turns 1, and the item list shows "Get in" over the Use button, which needs `canGetOnCar`, so that part is new. **Not seen:** the special ever enabled: in the two logged runs that reached the door, X pressed every 80 to 400 reads, `MotionGetOnCar::onApply` never saw `enabled`, so 0xbc, ApproachCar, ApproachOut, Drive and ApproachBite have not run, and nothing here has run with a driver. Next step there: log `UiItemList::pick`, `pressed` at stagescene.cpp 3074 and native 0x168 at the door; the pad is also blanked for the Use button while `getInputFocus` is someone else (`CharacterPad::update`). A run walks back and forth anyway: (7411, 2840) comes back about every 480 ticks, still unexplained. work/car3_pad.txt reaches the door from the stage's real start (8250, 2658). **Also written, on a guess that turned out wrong for the truck, kept because it is the ROM's: the reflection pass,** a second additive pass by the bounced eye ray for a material with `texture2` (VU1 0x77f, `reflectionVertex` in model_renderer.cpp). No material logged on stage 7 has one (of 390 logged, 386 have no ext texture; the other 4 were not looked at), so **it has never drawn anything**: look for it on the first material with `texture2`. Still unwritten there: `ext` textures (material flag 4, VU1 0x4df) and the level's specular path (VU1 0x5db).

**A character's voices are written, and `playVoiceHeard` was silent until now (a scheduled run, 2026-11-06, batch 177; architecture.md "Character voices"): game/charactersound.cpp, `verify`, 23 functions.** The frontier's unassigned@002a31b0 plus `getAttackVoice` 0x2a25b0, which `playAttackVoice` builds its name with; `playVanishSound` moved in. New names: `playHitSound` 0x2a3220, `playTrapHitVoice` 0x2a36e0, `playQuestionVoice` 0x2a4340, `enum SoundKind`. **The bug:** `playVoiceHeard` 0x2a65c0 searched the stream table (`findCue`, the S-prefixed names), where the ROM does a binary search of the stage's SE table, so every call dropped out at the null, the script native's at scriptrunner.cpp too. It now uses `SeTable::find`. **Seen in a log, temporary build, removed (YAMIPAN_TELEPORT=2, a pad of stick, Square, Circle and R1, 200 s, no crash):** E18_DMG_L 39 times and E18_FIND once, null cue before the fix, found after it, the same counts in a second run. A foe in stage 2 reaches `sayFound` and `playAttackVoice` by itself. Of 69 names this file can ask for, stage 2's SE table has 28, among them PC_DMG_L, PC_DW1, the PC and NPC breaths, E18_Q and E18_CALL; PC_AT1..5, PC_TRP_*, YRT_* and SRT_AT are not in it. **The table is per stage, so that is taken to be its cast, not an error; only stage 2's was listed.** **Not heard:** nobody listened, so no voice is judged, and the priority path of `playVoiceHeard` (stopCue, stopSource) ran for the first time with nothing checked but no crash. No PC or NPC voice fired in those runs, so `sayPartyVoice` itself has not run. **No comparison tool:** the names were read off the asm and tables, not compared by eerun. **Waiting on others:** `PlayerMotion::takeHits` past its first line is what calls `playHitSound` (7 times) and `playTrapHitVoice`; `StageAlert::cry` (a stub) calls `playQuestionVoice`; `hasAlteredVoice` 0x215af0 is a reached stub that picks NPC_AT_NW1 or NW2. **Left of the family:** the stubs 0x2a2b60..0x2a2e30 (`playJobSound`, `playIdleVoice`, `playPanicVoice`, `playGetUpVoice`, `playDieVoice`) and `playMotionSound`, `playFallDownSound` can join this file. **Doubts for the user:** (a) the file name is mine, the ROM has no source string here; (b) a shibito's attack voice uses the format `%s_DMG_%c`, so it says its damage name: the ROM's, kept, and E18_DMG_L is what fired; (c) `m_44c` gates `sayFound` and is still unnamed; (d) the shot of that run shows hard white shards round the lamp at the girl's hand, not looked into.

**The car, elevator and lock follow** in the order below, after the sight-jack: for the user to reorder.

**The queue was empty, so a scheduled run refilled it (2026-11-05, for the user to reorder):** (1) ~~`game/gameoversequence.cpp`~~ written (Done). (2) ~~`game/characterroute.cpp`~~ written (Done). The car, elevator and lock classes after that, in the order the next paragraph gives.

**The rute file is written and checked against the ROM (a scheduled run, 2026-11-05).** game/routeplayer.cpp, `verify` (Done): `findPath` and `pathLength` now answer. `tools/reverse/rutecmp.py` ran the ROM's own code on `eerun.py` beside the host's over 12 stages' files from the disc: 3,840 questions (areaPath, wayToCrossing, crossingPath, areaCrossings) and the file's own tables, 0 differences; 409 of the 960 areaPath questions end with no steps, the rest walk. New Game, work/doorwalk_pad.txt, 300 s: the stage builds its `RoutePlayer` from the real file, no crash, picture unchanged (the dark first-person wall). What neither covers: `pathSetIndex` and the route on a walking character, which no smoke run reaches yet. The first stage with a foe that walks is that test. **Doubts for the user:** (1) `kRouteLinkReversed`: `link()` indexes with the short as it is, so a leg with the top bit would land before the table; none of the 12 files has a negative index in a stored path, so the case stays the ROM's and is not guarded. (2) Three lookups in the ROM read past the table when a row is missing; the host gives no way or an empty run (the comments in `chainOf`, `wayAlongChains`). They never fired in the comparison. (3) What `areaPath` writes counts a stored run twice and the first chain's "same chain backwards" test compares a chain with itself: both the ROM's, kept. (4) A link's third node (+8) and the file's +0x18, +0x38 and the +0x34 lists are read by nothing written; `RoutePlayer::areaList` now serves `findWayOut`. **Written since (Done, 2026-11-06), under other names:** `playAction` 0x22cac0 (task 0x225fc0, update 0x2257c0), `attack` 0x22d350 (0x2299b0, 0x227820, 0x227ed0), `approach` 0x22d6d0 (0x229c20), `actOnCharacter` 0x22ccf0 (0x226d40), `actLeftRight` 0x22cde0 (0x226270, 0x226da0), `actLeft` 0x22cf90 (0x227510), `actOnFoe` 0x22d220 (0x227470), `actOnFoeRight` 0x22d0f0 (0x227050), `action22c0f0` (0x2251f0), `action22c840` (0x2257b0), `action22c960` (no task: FUN_001ded60), `action22cbe0` (0x2260f0). `CharacterBrain::gunAction` passes no rounds; the ROM leaves a1 as it was, here 0, and a gun outside G_NN38, R_MURATA, G_26NEN takes its magazine anyway. **Not tested:** New Game, doorwalk pad, 300 s: no crash, the route is built and `allow` runs, picture unchanged (the dark first-person wall), but no brain asks to move in those rooms. The first stage with a foe that walks is the test.

**The natives now have an order, from the disc (scheduled runs, 2026-11-05).** tools/reverse/sssall.py unpacks the 114 SLDI packs: 434 stage scripts, 364 natives called, **the unwritten ones no script calls need no code, as far as the scan sees: it finds version 0.4 blocks only** (`py sssall.py --unwritten` prints both lists, most used first; `work/sssall.json` per script; its "written" counts every bound native, called or not). The sixth run bound the last 89 (Done): **every native the scan finds a script calling is bound, 364 of 364.** **The natives are no longer the frontier; the classes behind their stubs are, in this order of what they wait on: (1) the car, 0x28a890..0x28e5a0 with its pose class: `createMapObject` makes a plain object of CAR, so `getScriptCar` answers null, `StageScene::viewTarget` is never set and 0x18d `rideCar` 0x17af50 (24 scripts, 105 calls) and the eight other car natives do nothing; rideCar itself is a stub. (2) the elevator zs9901, 0x2937e0..0x2955d0 (ELEVATOR2) with its pose class 0x27f7d0..0x27fea8: state +0x408, motion files +0x3d8..+0x404; every native of it answers through stubs; `elevatorCalledFloors` is the object's +0x2fc. (3) the number lock, 0x2a6b70.. with a pose class holding the dials at +0x3f0, and `SceneOwnedObject` slots +0x10, +0x2c, +0x44 of the three gadgets. (4) `StageParty` itself (0x1fe6b0.., its record's ctor 0x203d70, all stubs): 0x1fe, 0x1ff, 0x21e, 0x20d fill its past-sight record.** `createMapObject`'s classes are the way in (roadmap-3d-control.md: 0x174940, 0x28e5a0 the car, 0x295360 the elevator, 0x2a7340 the lock). **game/scriptrunner.cpp closed at `verify`, so the queue is empty: re-assess, do not follow the frontier (SKILL.md). For the user to reorder; left to itself the next run takes the route first, the paragraph After game/characterbrain.cpp below, since nothing moves a character without it, then the classes above.** Native 0x2f is written, so a character's script can fill its brain's slots; stage 1's first rooms do not call it. **Behind stubs:** `StageCharacter::findNearestFoe` 0x214a80 (natives 0x8e, 0x97 NEAR and the brain's; it is all that is missing of the picks: its FUN_002179c0 is canSeeThisFrame's real address, FUN_001b9a60 and 0x2107e0 are small), `StageAlert::cry` 0x263f10 and `show` 0x263f80 (StageAlert has no fields on the host yet: +0xc shown, +0x10, the fade +0x14), `StageCharacter::item218850`, `CharacterRoute` itself: `askRouteAction12c` / `130` have nothing to ask. **`takeHits` past its first line can go on now:** takeDamage 0x20eb40 is written; what it still needs is `0x26a010`, `0x2103d0`, `0x20e6e0`.

**63 natives are written and have not run (a scheduled run, 2026-11-05).** New Game, work/doorwalk_pad.txt, 285 s, no crash, no unwritten native reached, picture unchanged (the dark first-person wall): 67 natives ran, the same 67 as before, and of this batch's work only 0x3a, now on the real sight pick. A temporary first-call log said so and was removed. A run under about 200 s proves nothing here: at 150 s the same pad is still in the opening movie. The first stage whose script has a foe is the test for the weights, the picks and the trap and reload slots: it needs a smoke pad past the first rooms.

**The flashlight's white squares were the effect `dento1`, fixed (the user, 2026-11-06, live: "giants white squares that rotate").** Its blend, ALPHA_1 0x49, was drawn as Cs * Cd + Cd, so a white sprite doubled the frame over its whole quad; model.frag now hands the blend As (hazards.md, architecture.md "What a lamp shows"). **This corrects the note below:** the "soft pool" there is the point light, not the spot, and the "glow at the hand" was this bug. **Open, for the user:** the spot's pictures are black on the disc, so the ROM's spot pass adds nothing as it was read. If the real game shows a textured pool from the torch, something in that read is wrong; no reference shows the lamp yet.

**The lamp is real now, and three renderer bugs are gone (the user, 2026-11-06, live; progress/20261106-lamp-stage4-after.png, progress/20261106-texture-frame-stage3-before.png and -after.png).** Stage 4's F_PC04 has T_TORCH1: spot `T_TORCH1_sl` (cone 16, radius 1300, its own picture `T_TORCH1_sl.tm2`, reach trimmed to about 401 by the map), point `T_TORCH1_pl2`. **Seen, New Game forced to stage 4, the lamp switched on by a temporary hook (removed), 290 s, no crash:** a soft pool of light on the ground ahead of him, a glow at the hand. Fixed: the spot and shadow passes sampled the material (`drawOver` bound only the sampler), `ModelPass::shadowImage` was never uploaded, a blended material did not write depth (the ROM's does, only the ghost masks it), the cone test 0x66b (the host had left it out as a saving, so it did differ there, whatever the truck's note above says; it follows the ROM now, and the binding is what cured the truck), and the Image was the file's picture where a UV counts in TEX0's frame (hazards.md, architecture.md "Textures"). **The last is what the user saw as holes:** Fujita's head and back on stage 3, with 153 of that stage's 280 textures wrong the same way, walls and doors too. **Not judged:** no picture of the real game to compare the lamp with, so `verify`; the map's hard-edged bands on a far wall before the frame fix were not looked at again from the same place; the shadow pass now runs and no shot was taken of a shadow on purpose. A ModelMaterial_Kind1's frames are still not drawn (`ext_03_00` mat 31, Fujita's).

**The shadows are written and have not been seen (a scheduled run, 2026-11-05).** yamipan/rendering/model_renderer.cpp lays them over map models (Done). New Game on stage 1 cannot show one: the player casts nothing in first person, and in d0101 the boy's shadow is built but the shots looked at are off it. The test is the first stage or event with a visible character on a lit floor: a soft dark patch at the feet, falling off with height, moving with the character. Compare the patch with shadow_00, which was not looked at. Stage 0x1b still has none (its target, yamipan_drawshadow.h, is empty).

**The first-person view (the user, 2026-11-05, live: "add this to your priority queue since it is needed for gameplay"): done 2026-11-05, game/viewcamera.cpp (Done).** New Game's first segment now plays from the eyes with no body on screen, and the scope is not black. Left of it: whether the odd upward view in the fight and aim stances had the same cause was not looked at; the sight drawn over the scope is the stubbed menu side, not this camera.

**game/stageplayer.cpp (2026-11-05): closed at `verify`, the control task's list is empty (Done).** The queue goes to yamipan/rendering/model_renderer.cpp and game/scriptrunner.cpp, both open-ended. For the user to reorder: the route (next paragraph) is what the new sight and hearing wait for, and `StageCharacter::updateSounds` 0x2a4a40 (1,615 i, reached every frame) is what would fill the noise list with footsteps.

**After game/characterbrain.cpp (a scheduled run, 2026-11-05, for the user to reorder): the brain has nothing to drive it.** Two pieces: (1) ~~the route~~ written (Done), it was 0x22a310..0x22f270 and its task classes (0x224ba0.. and 0x22f320..), `game/characterroute.h` lists what the brain calls; 668 i in `update` 0x22e740 alone, and it needs `RouteFilter` 0x23f290.., `RoutePlayer` 0x252b10.. (2) **what gives a brain its scripts:** the four `setSlotEntry` calls at 0x21b9b0..0x21b9f0, with the character's other callers of the ask* family (0x218124..0x21b428). The queue's next item, game/stageplayer.cpp, does not need them.

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
   the VU0 math 0x135a90..0x1366c0, 0x142dd0..0x144080) get the same first
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
   scratch test of languageFile, fileWithSuffix and pathDir passes; nothing built or run.
   **Fifth run (2026-11-03, batch 085, 86 names; the 20-function slice written, still `partial`)**:
   `applyMapTarget` (one member per map section), `placeItem` 0x19d0c0, `placeCharacter` 0x19d4e0,
   `applyItemTarget`, `loadCommon` with `createMenuProg`, `wantsMapMenu` / `wantsPauseMenu` (shared
   `playerAction`, inline in the ROM), `isInputBlocked`, `isBusy`, `partnerResponds`,
   `restoreObjectFlags`, `getFloor`, `enterSubScene`, `removeView`, `StageViewStack::remove`,
   `stageFps`, `setFlag84To`. **The map file's layout is proven by `relocate` 0x186ee0** and is now EE
   structs in gamehooks.h (`StageMapFile` + six record types, S2dRef sections: `relocate`, still a
   stub, must use `S2dRef::set`). Corrected: `AreaGrid::addId` takes a **box object**, not an id
   (`addBox`), so `addArea0/1/2` / `addSoundArea` are `addFloorBox` / `addAbBox` / `addSmokeBox` /
   `addReverbBox`, `StageArea_*` likewise; `getSoundAttr` is REVERB_ON, `getFloor` the ENV_MB box's
   number. New stand-ins: `StageObject()` (0x18b700) with `m_xform` +0x140, `StageMarker`,
   `StageCamera`, `StageLight` / `SpotLight` / `PointLight`, `StageVolume` / `SphereVolume` /
   `BoxVolume`, `AreaBox` / `CbAreaBox` / `FloorAreaBox`, `StageCharacter`, `StageItem` and four
   classes, the sub-scene classes, `ScriptRunner` / `gRunningScript`, `SubSceneData`,
   `StageObjectTree`, `gWeaponDefs` (lib/unassigned_data.cpp holds the two new globals). ROM oddities
   kept: the YAM_PL test reads the **spot** section at the point light's index, so a YAM_PL record
   becomes a SphereVolume; F_YRT / F_SRT without a name_Snn record search the plain name and drop the
   answer, placing nothing. Guessed names: partner, `partnerResponds`, controlled, `isSkippable`,
   "volume", `setVector` (the ENV_ lights all pass 1, 0, 0), the PlayerAction values, `StageItemE`.
   `StageScript::entry` treats `table` as an offset: the relocate TU must keep it one. Checked: build
   green, callcheck clean, layoutcheck OK, smoke run unchanged (language screen; nothing here runs
   before a stage loads).
   **Sixth run (2026-11-03, batch 086, 90 names; 18 more written, 151 of 258, still `partial`)**: the frame itself:
   `update`, `draw`, `setupCameras`, with `calcMatrices`, `propagateNodes`, `updateVcms`, `updateRumble`
   / `getRumbleSource`, `getInputFocus`, `setPlayer`, `prepareDrawFlags`, `drawViewFilter`,
   `collectViewTargets`, `SceneArrayPair::restoreFlags`, `StageViewStack::push` / `removePriority`,
   `pushView`, `removeViewPriority`. `setFlag84` / `setFlag84To` are **`releaseControl` /
   `requestControl`**: +0x84 holds 0, 1 or a StageObject, so on the host it is `uintptr_t
   controlRequest`. `m_hidden` +0x37 is **`m_dead`** (update deletes such objects). Camera kinds read
   from the vtables: 7 StageCamera, 9 `StageMainCamera`, 10 `ViewCamera` (camview), 11 camjack (the
   party's), 13 `DemoCamera`. ScenePairTable is the four action steps; the perf labels gave the frame's
   order (architecture.md "Modules"). `sdk/eekernel` gained `GetTimerSystemTime`. Stubs still in this
   TU's update: `runStageScripts` 0x19aba0, `updateBlock98` 0x19b1f0, `updateBuffers` 0x19afb0.
   Choices (unattended): the two GS register packets of the letterbox go through `YamiPan_DrawUiReg`;
   `gsFillRect` 0x2eaeb0 is a stub, so **a sub-scene's letterbox and every fill draw nothing until that
   TU** (14 call sites; it should end in the UI canvas). ROM oddity kept: `removePriority` reads the
   entry after the shift, so below the top it clears and returns the object that moved in. Stand-ins
   to go with the camera TU: `StageObject::cameraScreen` / `cameraView` hand out &m_itemDef and
   &m_parts as the +0x200 / +0x240 matrices (the family has no room for them); `getName` slot +0x21c
   is read as `m_name` (every vtable read answers +0x20). Guessed names: oSJ24_02 / 03 as sight
   objects, `collectViewTargets`, `getRumbleSource`, `CharacterBrain`, `ModelPose`, `altLatch`,
   `SceneBlock98`, the view filter's table names. `GameOptions::_08` reads as the vibration option
   (`updateRumble`): rename when the option screen is read. Checked: build green, callcheck clean,
   layoutcheck OK, smoke run unchanged (language screen; no stage loads yet, so **none of this has
   run**). **Next for this TU**: `runStageScripts`, `updateBlock98`, `updateBuffers`, `applySoundTarget`
   0x19d9e0, `applyBgMotionTarget` 0x19f230, `loadList` 0x1a3a60.
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
   **Seventh run (2026-11-03, batch 088, 71 names; the last 6 stubs and 28 small members written, still `partial`)**:
   runStageScripts, updateBuffers, updateBlock98, applySoundTarget, applyBgMotionTarget, loadList, and
   startClear / startGameOver, saveObjectFlags + SceneArrayPair::saveFlags, requestSubScene, the two
   startEvent, the mission's value accessors, getMenuMode, door faces, the sphere / segment / floor
   queries (architecture.md "The stage's script step"). `StageScene::subSceneRequest` is now
   `const char *` (the ROM keeps the name in the int); `StageMission::busy` was the first byte of the
   pending event's name, `purposeText` ends before +0x84. Nothing here has run: no stage loads, the
   smoke run stays on the language screen. Doubts: SceneBufferSet / SceneBlock98 look like the
   characters' and the player's light (spotlight, render +0xe24, bones), left unnamed as such; the two
   event classes StageFileEvent / StageSimpleEvent, the mission's value / scriptVar names and
   setTreeScriptFlag are guesses; `StageObject::scriptRunner()` points into the padding at +0x40 until
   the character TU gives the object a real member (it must before a stage runs). PC difference:
   applySoundTarget skips a bank with an empty file instead of calling getData(null). Not written in
   the slice: 0x195db0 (SubSceneObject's deleting dtor), 0x19cf40 (answers "DemoExecuter"), 0x19cf80
   (empty slot): no host virtuals to hang them on yet.
   **Eighth run (2026-11-03, batch 089, 70 names; the last 67 written, 258 of 258, `verify`)**: the
   four collision finders left, the buffer / block98 / oZS99_01 accessors, `isInView`,
   `playerOrTargetInView`, `viewLinkedObject` (now public), `getViewBottom`, `stageHideObjects`,
   `StageFiles::hasItem` / `hasCharacter`, and every vtable function (architecture.md "The script
   runner's vtable"). **Nothing here has run**: the smoke run still stays on the language screen, the
   picture is unchanged. The stand-in `SubSceneObject` is now **`DemoExecuter`**, the ROM's own name
   (slot 7); `ScriptRunner` has a host vptr and a virtual dtor, so `->destroy()` became `delete`.
   Choices for the user to overrule:
   (1) **`ScriptRunner` is now polymorphic while `StageObject::scriptRunner()` still casts padding to
   it**: non-virtual calls work as before, but a virtual call there would crash, and only 20 of its 108
   slots are declared (host order is not EE order). The character / runner TU must give the object a
   real member, as the seventh run already asked. Nothing calls a slot yet.
   (2) `command10` .. `command101`, `sideHook0..3` and their `void *` arguments are placeholders: the
   commands' meaning is unread. Only the Shift-JIS string and the forwarding in 0x315690 are facts.
   (3) `getKind` of `SubSceneDoorObject`, `StageMarker`, `SubSceneProp` and StageMarker's `sideHook*` are
   plain members: `StageObject::getKind` is still a non-virtual stub, so **nothing reaches them until
   the objects' TU makes it virtual**, and `SubSceneMapObject` has no StageObject base on the host.
   (4) Fifteen this-adjust thunks and nine deleting dtors are comment-only (end of stagescene.cpp): the
   host compiler makes them. 0x19c060 / 0x19c0c0 (vtables 0x314ce0 / 0x314c90) have no host class yet;
   their ctors are 0x2b2c80 / 0x2a89b0.
   (5) The five Ghidra thunks 0x19d040.. were not renamed: renaming a thunk can rename its target, so
   they keep showing `ScriptRunner::defaultCommand`; the source names them command80..101.
   PC difference: `raycastObject` / `raycastObjectExcept` start the best hit as hit's own contents where
   the ROM copies an uninitialised local, as `raycast` already does.
   **With this the queue is empty**: per SKILL.md "Starting sessions" the next run re-assesses from the
   menu's blockers above (layout paths, `runTitleScreen`, `loadMenuPack`) rather than following the
   frontier blindly.

   **`game/areagrid.cpp` 0x283df0..0x2849c0 cut out of `unassigned@002681e0` (2026-11-03, batch 090, all
   11 written, `verify`; architecture.md "Modules").** `AreaBox` and `AreaGrid` moved from `gamehooks.h`
   to `areagrid.h`, with `seDistanceVolume`. Corrected: `AreaGrid::contains` returns the box, and
   `seDistanceVolume` is unsigned. Nothing in the game has run it (the smoke run stays on the language
   screen); a scratch test agrees with a brute-force scan of the boxes over 200,000 random points with
   rotated boxes, using test versions of the math. **Before a stage can use the grids:**
   `applyMatrix` 0x136690, `invertRigidMatrix` 0x136500 (run `unassigned@00135990`, declared in
   `vu0math.h`) and `StageNode::getMatrix` / `getWorldMatrix` 0x186a20 / 0x186a30 (run
   `unassigned@0017fce0`) are still stubs; until they are written `AreaBox::getWorldBounds` writes
   stack garbage into the bounds. Doubts for the user: the file name; `seDistanceVolume` joined by
   position only; `areaCell` is a `PC:` guard for cells of size 0 (the EE saturates, x64 would index
   before the array). Left in the run, in the order the slice had them: `gsFrameFilter`
   0x2849c0..0x286790 (needs a render-target decision: see the texture store's "Render targets"),
   `StageMission` 0x2867a0.., `fsm*` 0x28fb80..0x28ff54, `Rand48` 0x2681e0..0x268310, and the
   `FUN_00268a20` / `SubSceneData::getScript` cluster.

   **`game/layoutpaths.cpp` 0x2ec160..0x2ec8e0 cut out of `unassigned@002ec110` (2026-11-03, all 24 written,
   `verify`, no new names, no Ghidra change; architecture.md "Modules").** With the queue empty `tus.py next`
   offered `unassigned@0017fce0` (the frontier); this run released that claim and took the first of the
   menu's blockers instead, as the handoff above and SKILL.md "Starting sessions" ask. **The menu now
   goes on past the language screen:** language, the no-card dialog, the SCE Europe logo, then the loop on
   the stub `runTitleScreen`. The dialog and the logo equal their assets (512x216 and 512x512 TIM2s from
   `YAMIPAN_TEX_DUMP`, through tm2png.py; the logo keeps its x 48 -> 64, y 240 under the 4:3 mapping). To
   get there (the default smoke run still rests on the language screen): `build.py --run 40 --shot-every
   120 --pad <LEFT and CROSS by turns, every 200 reads>`. CROSS alone answers the dialog's default No,
   which shows it again; LEFT moves to Yes. Presented frames run far ahead of pad reads (dialog at frame
   ~45000, logo ~84000), so read `<read>` as game time, not shots.
   **`unassigned@00231c90` is queued next, for `runTitleScreen` 0x231c90** (five lines over
   `titleLayoutPath` and `runLayoutFile`). Queued by that run, not by the user; written the same day (below). After
   it, by the list under item 8: `loadMenuPack` / `unloadMenuPack`, `runHelpScreen`, the navi block.
   Doubts for the user: (1) the file name is the port's; (2) `FUN_002ec110`, right before the range, is a
   singleton `{100, 100, 100, _, 1.0f, ...}` at 0x5a52d0 (guard sbss 0x31af30; +0x14 the draw env,
   +0x18 / +0x1c read each frame), used only by drawSys and 0x2e8080..0x2e86b0: left for that TU, so the
   key `unassigned@002ec110` still holds that one function (resolved 2026-11-07: `rena/rena_drawtarget.cpp`, the port's file name, placed by link order; it could as well be the tail of rena_pkt_vif1pktmanage.cpp); (3) 22 of the 24 paths have not been seen
   opening a file yet (the logo's has; the dialog's image comes from a layout, which one is not proven).
   New Pan hook: `YAMIPAN_TEX_DUMP=<dir>` (README), so a screen's references need no patched build.

   **`game/titlescreen.cpp` 0x231c90..0x231d40 cut out of `unassigned@00231c90` (2026-11-03, all 3 written,
   `verify`, batch 091; architecture.md "Modules"). The title screen is on screen:** after the logo the title
   layout plays the "Sony Computer Entertainment Europe presents" card, then Load Game / New Game, "Select
   Menu Item" and the copyright line slide in and stay, the first menu with a moving layout.
   Compared by eye with the textures `YAMIPAN_TEX_DUMP` gave (tm2png.py into `work/ref/`, not committed): the
   labels and the copyright line show those TIM2s; Load Game is orange as the selected item. Not compared
   numerically. To get there: `build.py --run 75 --shot-every 1500 --pad <LEFT and CROSS by turns, every 200
   reads, 159 events>`; the menu rests from frame ~261000 (a shot's number, far ahead of game time).
   **Missing from the picture:** the background. The layout opens a movie widget (`getUiWorkArea` is reached
   for the first time here, so `PrgMovie::open` ran), and `sdk/libmpeg.cpp` reads every movie as ended
   (item 8(d)); black behind the menu is most likely that placeholder, not this TU (inferred: no reference of the real background was seen). Which movie it names was
   not read. **Not tested:** the answers. Any press on the title crashes first: `UiLayout::playSound` calls
   the stub `getSePlayer` 0x2c4140, whose null is read (access violation, exit 0xC0000005, seen with CROSS
   every 300 reads after read 32000). So TitleAnswer 1 = New Game, 2 = Load Game still rests on menuFlow's
   reading alone, and the two `TitleLayoutArg` names on the existing "without its intro unless withIntro"
   comment. Doubts for the user: (1) the file name is the port's; (2) `loadingBegin` / `loadingTick` are
   read from their call sites: both are `j overlayShowLoading`, and fileloader.cpp now calls them where the
   ROM does (PackJob::load's wait is the shared `FileJob::wait` here, as before).
   **`game/menupack.cpp` 0x2c3d30..0x2c42c0 is defined and queued next, for `getSePlayer` and `loadMenuPack`:
   queued by this run, not by the user, drop it to overrule.** The range is a first guess from the function
   list, unread: check it against the brief. It should end the crash and bring the menu's sound table
   (seMenu_SE.seh); the sound itself waits for the audio milestone, so check `SePlayer::loadBank` and
   `openStreamBank` survive `sdk/sif.cpp`'s no-IOP placeholder. After it, by item 8: `runHelpScreen`, the
   navi block, then `setUiWorkArea` 0x243370 with `getUiWorkArea` (the movie widget has a null work area
   until then). `unassigned@00231d40` is what is left of the run: the collision code to 0x236b90 (a
   `game/stagecollision.cpp` to cut: `StageCollision` is on stagescene.cpp's frontier).
   **`game/menupack.cpp` 0x2c3d30..0x2c42a0 written (2026-11-03, all 9, `verify`, batch 092; architecture.md
   "Modules"). The title now answers and no longer crashes:** with the title's pad script (159 LEFT / CROSS
   events every 200 reads, then CROSS every 300 reads from read 32100) CROSS on Load Game leaves the title for
   a new screen, a dark red picture of a window frame with red specks, no text. **Not compared with a
   reference**, so whether it is complete is open; `PrgMcCard::PrgMcCard` 0x2b9d60 is the stub reached there and
   very likely its missing text (inferred). Two 140 s runs ended "still running", no crash. New Game was not
   tried. So TitleAnswer 2 = Load Game is seen on screen, 1 = New Game still rests on the reading.
   **Sound:** `SePlayer::loadBank` / `openStreamBank` survive `sdk/sif.cpp`'s no-IOP placeholder (the run gets
   through createMenuSePlayer and plays the press), but nothing is audible and whether the banks hold data was
   not looked at. Corrected: `getSaveResource` / `getSaveResourceAux` are now `getMenuPackItem` /
   `getMenuPackItemSize`, `Memcard::m_iconAux` is `m_iconSize`. `0x2c42a0` left the range: it is slot 1 of
   Vt_00319f00, an inline virtual of the class at 0x2ca870.., to read with it (the key `unassigned@002c42a0`
   holds it). New `SeTable::bankCount` (parseSndseh uses it too). Doubts for the user: (1) the file name, and
   `MessageTable` / `MessageFile` are the port's; (2) MessageTable sits here by position only, it may be the
   end of another file; no message file was seen (a raw scan of the ROMs for the magic found none, so they
   are presumably packed), so the file format is read from the code alone and `setFile` / `find` have not run;
   (3) PC difference: a missing seMenu_SE.seh would reach `SePlayer::setTable` with null as the ROM does; the
   ROM reads address 0, the port would crash (the pack has it, the run shows). `StageMission::setMessages`
   0x286c30 and the forwarder 0x286c20 are still unwritten. **Next, for the user to pin:** `PrgMcCard` 0x2b9d60
   (the screen above), then by item 8 `runHelpScreen`, the navi block, `setUiWorkArea` 0x243370 with
   `getUiWorkArea`.
   **`game/prgmccard.cpp` 0x2b85a0..0x2b9ef0 cut out of `unassigned@002b6b90` (2026-11-03, all 7 written, `verify`, batch 093; architecture.md "Modules").** With the queue empty `tus.py next` offered the frontier's `unassigned@0017fce0` (399 stage objects); this run released it and took the stub the menu stood on, as the last handoff and SKILL.md "Starting sessions" ask. **The load screen is complete with no card:** the red picture now carries the "Load Game" header, the empty bar and "No memory card (8MB)(for PlayStation®2) inserted in MEMORY CARD slot 1.", the text the code should show for the host's empty slot; decide closes it, the layout answers 0 and the title comes back (many rounds in one run, no crash, so the dtor is tested). Not compared with a capture of the real game (none exists here), only with the layout's own text images. All 0x22 rfMSG_FLAG values were read on screen through a temporary loop (not committed), which replaced two wrong guesses (0xc is Loading, 0x1f Checking).
   **Not tested:** `stepSave` and the list, yes / no and every path with a card: `sdk/libmc.cpp` is still the empty slot, so stepLoad stops at McMessage_NoCard. Item 8(d)'s real card under `memcard/` is what tests them. Whether the 0x15..0x1d text images are chosen by something other than rfMSG_FLAG (the list's selected slot, presumably) was not read. `McVar_Select` / `McVar_YesNo` (rfSELECT, rfYESNO) are only cleared by the code; the layout's use of them was not read. `GameOptions::_4d` became `autoSave`, `GameValue_Option4d` `GameValue_AutoSave`. Doubts for the user: (1) the file name; (2) the `McSave_*` state names are read from the code; the numbers are the ROM's.
   **Run note:** this machine now presents ~30 frames a second (3,000 to 10,000 frames in the runs here, against the ~261000 of the notes above) and pad reads run at ~0.76 a frame (measured by the uiworkarea run; this note first said far ahead, which was wrong), so `--shot-every 20000` gave no shots and the 159 LEFT / CROSS events were still being read when the title was up: use --shot-every 300, and read pad scripts' timing from the shots. A 3-read press can fall between frames; hold it (`CROSS*40`). Why the rate changed was not looked at (a hidden window or vsync, presumably).
   **Next, for the user to pin:** by item 8, `runHelpScreen` 0x2acff0, the navi block, `setUiWorkArea` 0x243370 with `getUiWorkArea` (reached by the title's movie widget), or New Game (TitleAnswer 1, untried), which is the road into a stage.
   **`game/uiworkarea.cpp` 0x243350..0x243390 cut out (2026-11-03, all 4 written, `verify`, no new names, no Ghidra change; architecture.md "Modules"). New Game is open and the road now reaches the stage's opening card.** `tus.py next` offered the frontier's `unassigned@0017fce0` again; this run first tried New Game (untried until now) and took what stopped it. Seen on screen, in order: title with New Game orange, the Screen setup page (brightness ramp, corner marks, "Adjust brightness level until centre is black"), the dark red window picture with "Select Difficulty Level. Press X to start." and Easy / Normal / Options, then **the first in-game picture, the card "Shu Mikami / Aonoku Settlement/Mikami's House, 2nd Floor / -3:45:15"** (frames ~4020..4140, then black). CROSS took Normal. None of it compared with a reference.
   **Stops at:** black from frame ~4200 on (still black at 5700, 180 s), no crash, no new stub after `fsmRelocate` / `fsmFindText` 0x28fd30 / 0x28fb80, both reached here for the first time. **Why it stays black is not known:** the wait was not located (a gdb break at 240 s showed no game frames in crash.txt, and that run may not have got that far). Inferred from reading only: the card's layout waits on PrgMovie or PrgSndStream, and `sdk/libmpeg.cpp` / `sdk/sif.cpp` are still placeholders (item 8(d)); the stubbed subtitle pair returns null and is not itself a wait. To check next: which widget the layout holds on, then whether the two fsm functions or the placeholders end it.
   **The crash:** one run with the two work-area functions as stubs died with an access violation right after setUiWorkArea; the first run with them written did not. One run each, the fault address was never seen (gdb ran out of time), so it is **not proven** these four fixed it: the null path reaches `MoviePlayer::setWorkArea`'s own heap allocation, which is legal. Watch for it coming back. Also unchecked: that the host MoviePlayer's workSize fits the stage pack's buffer it now borrows.
   **Pad on the title (the user, mid-run, 2026-11-03): RIGHT moves to New Game, and only on the rested menu; CROSS there opens Load if RIGHT missed.** The older scripts (159 LEFT / CROSS events, then presses from read 32100) are stale on this machine: reads run at ~0.76 a frame, so those 159 events are still playing when the title is up, one of their CROSS presses opens Load, and read 32100 is never reached in a run; this cost four runs here. The title idles ~1000 frames, goes black ~1000 and returns, which reads like a hang but is the unanswered-title loop. Script that works, with shots every 60:
   `200:LEFT,400:CROSS,600:LEFT,800:CROSS,1000:LEFT,1200:CROSS,2180:RIGHT*10,2400:CROSS*10` then `<r>:CROSS*10` every 300 reads from 2800, 30 of them (`build.py --run 180`, or `tus.py check --pad`). It depends on timing, so check that the highlight moved (shot ~2880 on), not that the run ended.
   Doubts for the user: (1) the file name is the port's, and UiTrack's two base slots sit there by position only; (2) `unassigned@0017fce0` was released untouched a third time: **it is what a stage needs next** (StageObject, lights, items), so **it should be pinned once the card's wait is understood**. **Next, for the user to pin:** the black after the card, then `saveExtraInit` 0x29ef70 and `currentNaviAnswer` 0x29ad40 (both reached, still stubs, on the New Game path), then the stage objects.
   **Host task `sdk/libmpeg.cpp` done 2026-11-03 (`verify`; architecture.md "PC platform layer", Movies). The black after the card was the opening movie: the road now reaches `stageThread`.** `tus.py next` offered `unassigned@0017fce0` a fourth time; this run first located the wait, since the last handoff asked for that. gdb on the held game: `runLayout` in `menuPlayMovie`, one sequence, a `PrgMovie` with MoviePlayer state 15 (open, primed, ready, playing, never file end), because the placeholder `sceMpegDemuxPssRing` consumed nothing. So the movie "played" for ever: the old comment "reads as ended" was wrong. The earlier run's empty gdb dump was `gdbrun.py` starting the game with no data folder (fixed; hazards.md).
   **Seen on screen (`build.py --run 420 --pad` with work/newgame_pad.txt):** moving film pictures for the first time: a rust-stained wall at frames ~138000..270000 (title side), then setup, difficulty, the Shu Mikami card, then the opening film, a man's face between black bars, ~39 s at frames ~372000..447000, and it ends by itself. **Not compared with a reference:** the only reference is the same stream, so wrong colour levels or a wrong position would not show; the film looks dim, which may be the film. The bars are in frame units through `UiCanvas`, so the any-aspect rule for movies waits for item 8(c).
   **Silent:** `kMovieHostHasSound` is false in game/movieplayer.cpp (no feeders, no IOP: a sink would never drain and the demux would stall). **Remove it with the audio milestone**, and then check `Over` waits for the sound again, and that `MoviePlayer::getFrame` subtitles line up. FS2's sub-stream ids were not read from a disc file: the channel is taken as the low nibble of the first payload byte, untested until feeders exist. Whether the disc's PSS end in the 0x1b9 terminator was not looked at; the movie ended either way (FileEnd + drained).
   **A game-file change for the user to look at:** `~UiSequence` now removes widgets before actors (`PC:`, game/uisequence.h). In the reconstructed order `PrgMovie::close` wrote to a text track inside a freed actor; harmless on the EE heap, 0xC0000374 here, in every movie that ends. The ROM's order was taken from the reconstruction, not re-read in Ghidra. `PrgSndStream::close` had the same shape and is covered by the same change.
   **Stops at:** access violation in `listenerFacing` (game/stagesound.cpp:609) from `StageSound::update` from `StageScene::update`, first frame of stageThread: `StageObject::getMatrix` is a stub answering null. 95 stubs reached, the stage's whole setup among them. No stage picture yet (black). **Queued next, in this order:** `unassigned@002c42a0` (for `sldiDecompress` 0x2d0a10, reached by the stage pack: the stub leaves the pack unpacked, so what the stage has read is not known to be real, check this first), then `unassigned@0017fce0` (StageObject, lights, items). Several stubs carry address 0 (`StageObject::getKind`, `getMatrix`, `StageObject::StageObject`, `StageCamera::StageCamera`, `PointLight::PointLight`, `MenuProgHolder`): stubgen found no address, the code's own addresses are in gamehooks.h.
   **Run note:** frames were ~2600 a second in these runs (448,800 in 170 s; the note above said ~30), yet newgame_pad.txt still reached the card and the film, so the rate is not what the script's reads follow. `--shot-every 3000` suits a run this fast. `gdbdrive.py` now logs the stack of a stop in ntdll.
   Doubts for the user: (1) one `Image` per decoded picture is one GPU texture per picture (RosePan's persistent texture is the cheaper way; none of it showed here); (2) `MpegDecoder` drops RosePan's first-call-gives-nothing rule (RoR's sequencing needed it, FS2's does not); (3) a stage pack too big for its movie work area is still unchecked, nothing went wrong in these runs.
   **`game/sldi.cpp` 0x2d0a10..0x2d0d10 cut out of `unassigned@002c42a0` (2026-11-04, the one function `sldiDecompress`, `verify`, no new names, no Ghidra change; architecture.md, the PackFile paragraph). The stage pack is real now.** Tested two ways, neither on screen: a scratch test unpacked all 114 SLDI files on the disc (result = the header's size, nothing written past it, each starts with "pak", and the same bytes in place at the end of a 0x1800000 block as the game does it); a temporary trace in the New Game run showed `sldi magic=SLDI size=11396480`, then `pak variant=1 count=159`. So what the stage reads from that pack is the file's. **What each item's reader does with it is not checked**: their readers are still stubs (`ScenePool::setImage`, `StageParty::setImage`, `MenuProgSet::load` and others).
   **Stops at:** the same place as before: exit 0xC0000005 with `StageObject::getMatrix` the last stub in the log. Read from the log tail and the exit code only; gdb was not run, so `listenerFacing` as the fault is carried from the libmpeg run, not seen again. 98 stub lines against that run's 95; the difference was not compared by name. Last picture: the opening film (dim, as noted there), no stage picture.
   **Queue:** `unassigned@002c42a0` is off it, since sldi was the reason it was queued; `unassigned@0017fce0` (StageObject, lights, items) is next. The rest of that run stays on the frontier: `resetStageGroupTable` 0x2cf510 and `WideFontPrinter::WideFontPrinter` 0x2d4110 are reached and still stubs, `SceneBufferSet` / `SceneBlock98` / `SceneModel` / `SceneModelHolder` 0x2ce0c0..0x2d07a0 is one more cut to make, and 0x2c42c0..0x2ce0c0 (`runExtraMode`, Vt_00319f00..) is unread.
   **Run note:** frame rate depends on the shots: a run with `--shot-every 60` reached frame ~170,000, one with 3000 reached ~576,000, both before the same crash. Use 3000 when only the end matters.
   Doubts for the user: (1) the file name is the port's and the single-function file is by position only; (2) the ROM has no bound on dst or on a repeat's distance, kept: a bad file would write out of bounds, the disc's do not.
   **`game/stageobject.cpp` 0x189ce0..0x18c050 cut out of `unassigned@0017fce0` (2026-11-04, batch 094 with 67 names, `partial`; architecture.md "The object family"). The object family is real C++ now and the first stage frame's update runs to the end.** `StageObject` is `StageNodeBase` + `StageSideBase` + `ScriptRunner` as in the ROM, with real virtuals: slots 0..6 on `StageNodeBase` (`getMatrix` is `m_xform.world`), `getKind` pure as in the ROM with an override per host class (17 one-liners from the vtables; `StageObject_Kind21` was `StageObject_WeaponItem`), the object's slots +0x200..+0x248, ctor, dtor, the model / clock / sound helpers and the whole `StageObjectTree`. `StageNodeList` is `StageList<StageNodeBase *>` (new `erase`), `SeEntry +0xc` is `loudness`, `ScriptRunner` moved above `StageObject` in gamehooks.h.
   **Seen:** `listenerFacing` no longer crashes; the New Game run goes through `StageScene::update`, 111 stubs reached against 98 (gdb's log). **Stops at:** access violation in `StageScene::draw` (game/stagescene.cpp:3270), `drawSysCurrent()` is a stub answering null, so `->camera` faults. Read from gdb's backtrace. **No stage picture yet**, the last picture is black; nothing here draws, so there is nothing to compare with a reference.
   **Closed 2026-11-05, `verify`:** the three script-facing slots are written (`getProperty` 0x18a5b0, `setProperty` 0x189f40, `scriptAddMenuLine` 0x189ce0, which the old note misread as sound: uiState +0xd0 is the stage menu, 0x539b10 the object records) and compared with the ROM case by case.
   **Next, in order:** `drawSysCurrent` 0x2d4fd0 with `drawSysInit` / `drawSysTick` / `drawSysClearTargetView` (the draw system, the new first blocker), then the ctors that are still stubs: `StageNodeBase` 0x1d8540, `NodeTransform` 0x1d7f20 and `update` 0x1d7d50, `updateMatrix` 0x1d8420 (run `unassigned@001d7c50`: until then every object's matrix is uninitialised), `ScriptRunner`, `StageCamera` 0x177d60 / `StageMainCamera` 0x179700 / `calcViewMatrix` 0x177810.
   Doubts for the user: (1) **`// PC:` in `StageObject::StageObject`** stays (decided 2026-11-05): the host keeps the subclasses' fields in one flat block each ctor fills only as far as its TU has read, so the zeroing is the host's rule, not a stopgap. (2) `StagePlayer::character()` is still a `reinterpret_cast` of a separate layout. Now that `StageObject` has real virtuals, a virtual call through it on anything that is not a real object crashes; in these runs the player is null, so this was not exercised. (3) `SubSceneMapObject` and the gate object still have no host base (they are StageObjects in the ROM). (4) `StageObject_Volume` and `StageObject_Marker` are both 0x1a, read from the ROM; `sideHook0..3` are defaults on the host where the ROM's base table has empty slots. (5) The file name, `noiseHook`, "noise" / "loudness" for the record at uiState (the code only shows a float compared with 0 going into a list) and `playNamedHeard2` are the port's; the ranges 0x185fc0 and 0x1865c0 are joined by what they do, not by position. (6) **`build.py --pad` takes the script text, not a file**: pass `"$(cat work/newgame_pad.txt)"` (hazards.md).
   **`game/stagelight.cpp` 0x184860..0x186ee0 cut out of `unassigned@0017fce0` (2026-11-04, batch 095 with 78 names, all written, `verify`; architecture.md "The lights").** `StageLight` / `SpotLight` / `PointLight` left gamehooks.h for the new `game/stagelight.h`, with their second base `CollisionOwner`, the commands and the frame. **`VcmController` is gone: VcmTable's controllers are `StageLight`.** New stand-ins in gamehooks.h: `CollisionBody`, `CollisionOwner`, `CollisionContact`, `VectorKey` / `VectorTrack`, `PartSwitch`, `StageParticle`, `LptRecord`, `StageObjectTree::setParent`; in vu0math.h `vecDot`, `vecDistanceSq`, `vecNormalize`, `BoundingSphere` (inline: they answer real numbers, a stub would not).
   **Seen:** the New Game run constructs lights and adds them to the scene with no fault; 114 stubs reached against 111; the stop is unchanged, `drawSysCurrent` null in `StageScene::draw` (stagescene.cpp:3271), read from gdb's crash dump. **No stage picture**, so nothing here is on screen yet. Host-pass only: compiled, callcheck clean, nothing compared with the ROM by running it and no harness for the light maths. The ctor's colours were checked: n / 255.0f gives the ROM's float bits for all five values.
   **`game/stageitem.cpp` 0x17fce0..0x184860 cut out of `unassigned@0017fce0` (2026-11-04, batch 096 with 98 names, all 139 written or mapped, `verify`; architecture.md "The bullet and the items"). The run is used up, so the queue is empty.** The bullet (`StageBulletBase`, `StageBullet`), `StageItem` and its four kinds, `ItemLamp`, and `ItemPhysics` declared for the unread run 0x23c020..0x23de20; `stageitem.h` holds them all, `gamehooks.h` lost them. `CollisionOwner` changed for every class: `onCollide` takes (contact, sweep, face, t, distance), `collisionHook3` takes the body, `CollisionContact` has both bodies (asm of 0x17fea0, 0x180030; `StageLight` follows). `0x180018` is padding: its Ghidra function can go.
   **Seen:** the default smoke run still rests on the language screen, unchanged. The New Game run under gdb (`gdbrun.py --pad "$(cat work/newgame_pad.txt)"`) ends where the last two did: SIGSEGV in `StageScene::draw` (game/stagescene.cpp:3272), `drawSysCurrent()` a stub answering null, with a black last shot, so **there is no stage picture and nothing of this file is on screen**. 113 stubs reached against 114 (the bullet's ctor is real now). Of the new code only `StageBullet::StageBullet` and the `StageBulletBase` part ran, from `setupCameras`, with no fault; no item is placed yet (`PackTables::parse` and `StageModel::init` are stubs, so `setupItem` is not reached) and `ScenePairTable::add` is a stub, so no frame or collision hook has run.
   **Next (pinned: `unassigned@002d46a0`; the frontier is the stage's first picture, in this order):** `drawSysCurrent` 0x2d4fd0 (run `unassigned@002d46a0`), `NodeTransform` 0x1d7f20 / `update` 0x1d7d50 and `updateMatrix` 0x1d8420, then what placing items needs: `PackTables::parse` 0x2bcf10 (it fills `gItemDefs` / `gWeaponDefs`; it calls `ItemDef::setLandSound` / `setPickSound`, written here), `StageModel::init` 0x188b10 and `ModelFile::createInstance`.
   Doubts for the user: (1) the file name, `StageBulletBase`, `ItemPhysics`, `ItemLamp`, `m_aimLock`, `m_aimPoint`, `m_bulletFlying`, `m_handNode`, `ModelFlag_Item`, `ModelFlag_Frozen` and "lamp" for `StageItemE` are the port's guesses, from the code only (T_TORCH2 is the one string). (2) Kind 0x18's class is only ever the bullet's base here. (3) Four `PC:` guards: a bullet with a null owner skips the aim and the +0x688 write (the ROM reads low memory; its one such bullet never runs), and `itemDef()` / `weaponDefAt()` answer the first record for a bad index where the ROM reads from address 0. (4) `StageObject::StageObject`'s `PC:` zeroing stays: its reason was stub subclass ctors, and the cameras' and characters' are still stubs. (5) `ItemDef::startCount` / `isBlade` and `WeaponDef::range` / `magazine` are named from their users only; the tables are not loaded yet, so no value is checked.
   `sysDataReturnZero` 0x23c010 (TU 5) was a mislabel, gone: it is slot 1 of vtable 0x316510, `ItemPhysics::onCollide` (inline in stageitem.h), and `game/sysdata.cpp` now ends at 0x23c010.
   **`game/stageitem.cpp` is host-pass only:** compiled by GCC and MSVC, callcheck clean, nothing compared with the ROM by running it, no harness.
   Doubts for the user: (1) the file name and names (`CollisionOwner`, `VectorTrack`, `PartSwitch`, `StageParticle`, `m_usable`, `m_castsShadow`, the mode names) are read from the code. `shadowAt` is named from `updateLights` asking it for characters only and is a guess. (2) Past the ctor nothing in the file writes +0x2b4 (`m_mode`), +0x2b8, +0x2c8 or +0x2a8: the writers are the classes built at 0x181f40 / 0x182f40; +0x240, +0x244, +0x248, +0x250, +0x260, +0x2b0, +0x2d4, +0x2d8 are unread by the light and keep offset names. (3) `cmd*` are plain host virtuals, not overrides of `ScriptRunner`, because that class has no such commands until its TU: the script cannot reach them yet. (4) `StageList<AreaBox *>`'s boxes are deleted in the dtor through `AreaBox`'s host dtor. (5) PC changes: `m_reachSq` zeroed in the spot's ctor (the ROM leaves it to the first frame), the two 0x10-byte name buffers of `bindParts` widened to 0x20.
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
   **Pinned 2026-11-03 by the user: `game/menuscene.cpp` 0x2b5740..0x2b6b90** (14 functions, first in the
   queue, before the partial stagescene): the reached stub on the way to a first picture. **Then
   `rena/rena_2d_texture_tim2.cpp` 0x2e1b70..0x2e2970** (7 functions, pinned the same day: nothing
   textured draws without it), **then the host task `yamipan/rendering/texture_store.cpp`** (the Pan
   side, its brief has the whole job: `tus.py brief` on that key). Queue order is the user's: once all
   three are through, the loop goes back to stagescene unless more is pinned.
   **`game/menuscene.cpp` written 2026-11-03 (all 14, `verify`, batch 083; architecture.md "Modules").**
   The default smoke run (no pad) now stays on 2dEU/languageEU.d2b: black for the first ~3000 presented
   frames, then one white band across the screen (y 224..288 of the 683x512 shot) and nothing else, an
   untextured layout: `Tim2Texture::Tim2Texture` and `UiTrack::draw` are its reached stubs, so this is
   the picture the two next pinned TUs answer to. No reference is extracted yet (`work/ref/` is empty):
   carve the language screen's TIM2s with tm2png.py when those TUs need it. Keep `settings.smoke_pad`
   empty until then: **CROSS** leaves the language screen (CIRCLE does nothing there), e.g. `build.py
   --run 8 --pad "30:CROSS,60:CROSS,90:CROSS"`, and the flow then runs `menuCheckCard` and the logo on
   null paths and hangs in `menuFlow`'s title retry on the stub `runTitleScreen` (hazards.md "Stubs"). **So
   the menu's next blockers, for the user to pin after the textures:** the layout paths 0x2ec110..0x2ec8e0
   (tiny, one sprintf each; `naviTuLayoutPath` .. `titleLayoutPath`, `helpLayoutPath`, `naviLayoutPath`,
   the two old ones and the event movies; all stubs answering null, so the scene runs no layout but the
   language one), `runTitleScreen` 0x231c90 (one short function right after game/scene.cpp),
   `loadMenuPack` / `unloadMenuPack` 0x2c3db0.. (the resident menu pack), `runHelpScreen` 0x2acff0, the
   navi block `runNaviFile` 0x295470, `applyNaviAnswer` / `currentNaviAnswer` 0x29acb0 / 0x29ad40,
   `saveExtraInit` 0x29ef70, then `setUiWorkArea` 0x243370 with `getUiWorkArea`.
   Doubt for the user: the path getters were left as stubs to keep the run to one TU, though the menu
   cannot pass the language screen properly without them.
   **`rena/rena_2d_texture_tim2.cpp` written 2026-11-03 (all 7, `verify`, batch 084; architecture.md
   "Modules").** The class moved from `gsutil.h` to `rena/rena_2d_texture_tim2.h`, the setTex0 packet
   constants with it. The picture is unchanged (the one white band): nothing binds a texture until the
   texture-store host task, which is next. Tested with a scratch program on real TIM2s carved from
   ROM.000 / 001: sizes, PSM, CPSM, the pixel and CLUT offsets agree with the files' headers (the files'
   own TEX0 words differ only in TBW, TCC and CLD, which the game works out). `upload` is empty on
   purpose (no GS memory; its comment says what the ROM sends) and `putTex0` still writes its packet.
   Doubts for the user: (1) four callees of `setupMip` / `load` lie outside the TU, in Sony's sample
   library, and stubs would have left the class useless, so **`rena/tim2.cpp` 0x2e97a8..0x2e9d90 was
   defined and four of its nine functions written in the same run** (one TU a run bent for that); its
   file name is Sony's sample's and the end of the range a guess. (2) The ROM indexes its tables with
   ClutType's whole byte, so a linear-order CLUT (bit 7) would read past them; the host masks the type
   bits. No disc file has one. (3) `GsRegion` (+8, `release` 0x2e0840, still a stub, unreachable until
   something gives a region) is a stand-in in `gfxtarget.h` for the unread TU at 0x2d46a0... (4) The
   second CLUT +0x3c has no writer yet: when one turns up, TextureStore must decode with it.
   For TextureStore: `m_image` is the file itself, not a copy; `m_clutKind` / `m_clutCount` say a 4-bit
   image can hold several 16-entry rows, the first being TEX0's unless a caller moves CBP.
   ~~(b) textures~~ **done 2026-11-03**: `rendering::TextureStore` (architecture.md "PC platform layer").
   The language screen is the first textured picture: at rest it equals the assets, offset (0, 0) in a
   search of +-2 frame units, peak 192 as in the file, mean difference 2.66 over the row (the 4:3 stretch).
   The decoder equals tm2png.py byte for byte on 20 disc TIM2s (4-bit CLUT32 and CLUT16, 8-bit CSM1, 32-bit)
   and 7 synthetic ones for what the disc lacks (16 and 24-bit direct, linear and 16-bit 8-bit CLUTs),
   through the real `Tim2Texture`. Proven on screen so far: UV scale, the 512x512 mapping, alpha blend,
   bilinear, 4-bit only. **Not yet seen on screen**: the font's path (`FontPrinter::setDrawState`, TEXA,
   its alpha test), additive and the other blend modes, scissor, repeat. Expect the next text screen to
   test them. Left for later:
   - **Render targets**: a `GsTexture` with `m_isTarget` (tags Zbuf / Draw / View, TBP0 0x1800 / 0x800 / 0)
     is logged once and binds nothing, and `UiCanvas::emit` drops a textured draw with no image rather
     than draw a white sheet. They need `copyTarget` and a host copy of the scene to sample. Choice for
     the user to overrule: dropping (the effect is missing) over the plain shape.
   - **Repeat on a non-power-of-two image**: the GS wraps at 2^TW, the host sampler at the image's own
     width. Nothing seen uses it.
   - **TEXA is not applied**: 16-bit and 24-bit alpha is fixed at decode as tm2png.py does it, while the
     game sets TEXA per draw (the font: TA0 0, AEM 1, TA1 0x80). Only two CLUT16 files exist on the disc,
     so this is unlikely to matter, but check them if one draws wrongly.
   - **`m_altClut`** (the second CLUT) has no writer yet; the store ignores it.
   - Getting a screen's references: nothing maps a d2b to its TIM2s offline yet, so run with
     `--env YAMIPAN_TEX_DUMP=<dir>` and pass the files to tm2png.py.
   (c) any aspect: the renderer hands k to the game's UI matrices (`kUiScreenMatrix`, see TU 19), the one
       place a game file needs a `PC:` touch; **not done, and that one place is not enough** (a scheduled
       run, 2026-11-05): no game file reads the aspect yet, and what draws in raw window coordinates,
       with no matrix (ScenePool's strip, WideFontPrinter, gsFillRect), comes out 4/3 wider at 16:9:
       a strip cell is 26.7 pixels of a 683-wide window and 35.5 of a 910-wide one at the same height.
       **The user decided, 2026-11-05: squeeze about the centre.** `YamiPan_DrawUiSqueeze(list, on)`
       (UiCanvas::setSqueezed), per element, factor (4/3) / aspect from PictureLayout, 1 at 4:3 and
       narrower. In use: ScenePool, DemoStream::drawSubtitles, UiPrompt. Everything else in raw window
       coordinates still stretches: left- or right-anchored elements need an anchor of their own, and
       full-screen fades and fills must not be squeezed;
   (d) placeholders to replace: `sdk/libmc.cpp` (the empty slot: a real card under `memcard/`),
       ~~`sdk/libmpeg.cpp`~~ (done 2026-11-03: above), `sdk/sif.cpp` (no IOP: the audio
       milestone brings RosePan's IOP host and `iop/rxsnddrv`);
   (e) HDR in `PresentPass` (settings are there, the swapchain stays SDR), MSAA, anisotropy;
   ~~(f) re-enable the scheduled task~~ **done 2026-11-03, on the user's word**; `yamipan.py` now
       finds MSYS2's toolchain without PATH, which a scheduled run does not have.

**game/drawsys.cpp, verify (2026-11-04, second run, batch 098): all 164 written; where the New Game run stands.** New here: `drawSysAddModel` 0x2d5140, `drawSysFindKind1Surface` 0x2d46a0, `parseNameTags`, the plant search 0x2da220, and the boss shadow texture 0x2d5e10 with its blits (architecture.md "The draw system"). `StageModelData` and `RenaModel` were folded into `ModelInstance`; the file's records are in gamehooks.h in EE layout.
**Seen (`build.py --run 240 --pad "$(cat work/newgame_pad.txt)"`, 118 screenshots to frame 7080):** Easy / Normal / Options on the red background at frame 3600, black at the last frame, no crash; every rena pass and the scene draws up to `SceneDirector::draw` are reached stubs. Black is all the code can show: nothing draws 3D. No reference picture to compare yet.
Next, in the order the picture needs them (each its own TU): NodeTransform 0x1d7f20 / update 0x1d7d50 / updateMatrix 0x1d8420, `PackTables::parse` 0x2bcf10, `StageModel::init` 0x188b10 (calls `drawSysAddModel` and `drawSysFindKind1Surface`; needs a +0xa8 field), `ModelFile::createInstance` 0x2e5210 with 0x2e4800 and 0x2e3e10 (must allocate `ModelSurface` and the +0x40 / +0x44 tables), then the rena draw run `unassigned@002e2970` and a 3D host module, which must also fill in `yamipan_drawshadow.h` (empty bodies in seams.cpp).
Left out on purpose (file header): the frame's GsRegionAlloc, FlushCache, the TEST_1 packets, the uncached packet switch, the sceVif1PkCall flags branch; the VU1 upload of SceneRender +0x80 and +0xf4 * 1.2 waits for the SceneRender TU. No 3D host module exists yet, so the depth clear is stored as a rect only. The boss target's GS env, pages and copies are gone too.
Doubts: the file and class names are the port's; `sRegionAlloc` / `sReservedBlock` are stored and unused; the `flags` parameter of `drawSysRender` is really a packet pointer no caller passes. **New, for the user to overrule:** (1) a new host seam `src/yamipan/rendering/yamipan_drawshadow.h` for the boss target (blits as data, no GS pages); (2) `ModelFlag_Ghost` / `ModelFlag_TagC` / `ModelMaterial_Kind1` / `ModelInstance::colorMatrix` / `drawMode` / `ghostParam0..1` say where they are set, not what they do: their readers are in the rena run, and "drawn black" for the zeroed colorMatrix is a guess; (3) the 0x2d7030 + T8H > T8 path is read as "the result becomes an 8-bit image", and which channel the host should use as brightness is not proven: check against the boss stage on a real picture; (4) the guard in `drawSysFindKind1Surface` is for unused table entries, the ROM reads through them.

**game/packtables.cpp, 3D path 1 of 5 (2026-11-04).** Why no model reached the code: **`String` and `StringList` were stubs**, so `splitLines` gave every stage script zero lines and `StageFiles::load` read nothing (it returned true). Now `game/stringlist.cpp` (0x148db0..0x149510, its own TU), and the chain below it, in the order the run stopped: `ModelFile::isValid` / `relocate` / `getNodeCount` (new `rena/rena_model.cpp`, the magic is "RM2"; TU 4 of the 3D path goes on in that file with createInstance, 0x2e4800 and 0x2e3e10), `stageMotionTargetName` 0x175c00, `StageMapFile::relocate` 0x186ee0 (both in stagescene.cpp until their file is cut), and `StageMission`'s stub ctor now zeroes itself (it walks its lists once a stage has scripts). `PackTables::parse` is written, cut into one method per sheet (architecture.md "The parameter tables").
**Seen:** ModelFile::createInstance reached in the New Game run; applyBgTarget skips the null instance (a PC: guard in stagescene.cpp to remove with the real createInstance; placeItem and placeCharacter have no guard yet). Not reached yet / left as stubs: `StageScript::relocate` 0x1d3a40 (answers null, so the script bodies are not relocated: on a 64-bit host its two table fields need the design in gamehooks.h's comment) and `CollisionFile::relocate` 0x23ae40.
Doubts for the user: the pack's column names are the game's, but the English field names are read from the Japanese; `ItemDef::maxCount` is really the sheet's No, and 所持上限 (carry limit) is `stackKind`: the names stay until the UI's users are re-read (UiItemSlot::max and showsCount); `MotionBone::name2`, `MotionModel` a..d and `MotionDef::offsetA/B` say where they are stored, not what they do; the rows with a number outside the table are skipped (the ROM writes through null).

**game/nodetransform.cpp, 3D path 2 of 5 (2026-11-04), verify: 0x1d7c50..0x1d85b0, all written (batches 100, 101).** The rotation-matrix helpers, `translateMatrix` and `multiplyMatrix` went into vu0math.h with it; architecture.md "The node transform". The stubs in unassigned_stubs.cpp went with it (the stub file does not clean itself: a rename left `StageObject::propagate` there and turned the build red).
**Seen:** `build.py --run 240` with work/newgame_pad.txt: 31,560 frames, no crash, assert or fault; the opening movie at frame 3600, black at the last frame, the rena passes and scene draws reached as stubs. Nothing draws 3D, so no picture tests this yet: the first map model is its test. The rotation orders were checked numerically; a wrong sign would show as a turned or mirrored map.
For TU 3 `game/stagemodel.cpp`: `StageBoneNode` / `StageMatrixNode` are in stagenode.h, ready for `bindBones` and `setBaseMotion`; `BoneNode::value` must become `StageNodeBase *` (stagesound.cpp's two `StageNode *head` follow).
Doubts for the user: the names `positionOnly` / `rotationXyz` are read from `update` only, the writers 0x18e280 / 0x190220 are unread; `StageBoneNode` ("a bone with no motion of its own") is read from `bindBones` skipping bones the pose owns, not proven; the host's sinf / cosf replace the ROM's series, which differs only outside -pi..pi.

**game/stagemodel.cpp, 3D path 3 of 5 (2026-11-04), verify: 0x187230..0x188d60, all 19 written (batches 102, 103).** architecture.md "The model holder". `BoneNode` and `ModelLink` are gone (one `StringMap<StageNodeBase *>`, `StageModel::boneMap`); `StringMap::balance` is new; new header `game/modelpose.h` holds the pose as a stand-in with its short accessors written (clearPlacement, setPosition, getPosition, rotate, resetSwingBones, getBoneMatrix, popEvent). `ModelInstance::getTextureName` and `setTexture` (0x2e47d0, 0x2e4550) went into rena_model.cpp with `StageModel::init`.
**Seen:** `build.py --run 240` with work/newgame_pad.txt: reached the stage, no crash or fault, still running when stopped, black at the last frame (frame 2520, about 10 fps in this run). The model paths did not run: `ModelFile::createInstance` still answers null and no pose was built, so nothing on screen tests this file yet. Checked apart: `StringMap::balance` on 3,000 random and sorted trees (scratch test): no key lost, chain intact, no node leaning by more than one.
For TU 4 `rena/rena_model.cpp`, what createInstance must now fill or `StageModel` breaks: `ModelInstance::rootBone` (+0x10, init writes through it), `bones` with `ModelBone::node` at the file's `ModelNodeBone` records (a changed type), `inverseBinds` +0x20, `bindMatrices` +0x24, `rootMatrix` +0x28, `textures` +0x50. Allocate it with plain `new`: `~StageModel` does `delete data`, and `~ModelInstance` 0x2e46d0 is a stub (slot 0 of 0x31a7f0). Also still stubs there: `ModelSurface::setFrames` 0x2e3d90, `modelTexturePremultiplyPalette` 0x2e1700.
Left as stubs for the pose TU (unassigned@00149510, 0x14e0a0..0x151020): ctor, dtor, load, update, solve, the four play calls, getSurfaceFrames, `ModelMotion` ctor and dtor, the four subclass ctors. While load is a stub a pose has no bones and models stand in their bind pose, and `ModelPose::rotate` has a PC guard for the missing root.
Doubts for the user: the names are the port's. CarPose, ElevatorPose, NmlPose, SkyPose come only from the ROM's tags car / elv / nml / sky; "neck" for +0x184 and the +0x350 limits is read from one bone name, not proven; `MotionPlay_Base` / `MotionPlay_Arg0` and `m_loop` say where they are passed, not what they do (the pose's update is unread); `poseOwnsBone`, the `motionEventFlag` tables and `StageModelSlot` are the port's shapes; `FUN_00135e20` in `rotate` is replaced by `addAngles`, which wraps the same range but through fmodf; `m_animateSurface` +0xa4 has no writer in code read so far; advance's `arg` is never read.

**rena/rena_model.cpp, 3D path 4 of 5 (2026-11-04), verify: 0x2e1700..0x2e1b10, 0x2e2cf0..0x2e41a0, 0x2e44e0..0x2e5450 and 0x2e9790, every function written (batches 104, 105).** architecture.md "A model instance". `CharacterPart` is gone, it was `ModelPartRec` (now in gamehooks.h near the top, with `next`, `drawMode`, `flagE`, `hasMesh`). `ModelInstance` is complete to +0xc0 (`colorMatrix` became `lightColors`, new `boneBinds`, `lightDirs`, the block fields), `ModelSurface` has every field (`waveIndex` / `_28` / `isWave` became `blendFix` / `frame0` / `frame1`). Renamed in Ghidra: `RenaModel::findGroup` to `ModelInstance::findGroup`. New: `invertAffineMatrix` (vu0math.h), `ModelTexAnim`, `ModelFile::findNode` (two, createInstance's inlines). The PC guard in applyBgTarget is gone; `createInstance` answers null only when the ROM would crash (not an RM2, no such node).
**Seen:** host harness over every RM2 inside the 114 SLDI packs: 1,768 models, 4,702 nodes, 286,866 bones, 89,814 surfaces, 399,314 meshes, 0 bad: all pointers inside the pack, root and bone inverses right to 5e-7, every part record reached, `findGroup` and `modelFindPart` right, the block filled to the byte, nothing leaked, `userCount` back to 0. `build.py --run 240` with work/newgame_pad.txt: no crash, still running when stopped, and a temporary counter showed 22 real instances (`far_`, the oMK objects, doors, `Root` with 1,565 bones, `ext_shc_01`), `renaDrawModels` reached as a stub. **Black at the last frame, which is all the code can show**: no 3D host module exists, so nothing on screen tests this file yet.
For TU 5 `yamipan/rendering/model_renderer.cpp`: `renaDrawModels` 0x2e5450, `renaSetLights` 0x2e8080, `renaEndPass` 0x2e85e0, `SceneModelHolder::draw` 0x2d0550 are what the run reaches; 0x2e41a0 and 0x2e43e0 sit between this TU's ranges and are the draw run's. The geometry is below `ModelMeshPart`: `ModelPrim::batches`, DMA tags into the file (decode at the source). A surface's textures: `ModelInstance::textures[surface.textureSlot]`. A `_s` texture has its premultiplied palette in `Tim2Texture::m_altClut` (the texture store must use it). `ModelPose` is still a stub, so characters stand in their bind pose: the map model is the first test.
Doubts for the user: the names are the port's; `level`, `levelScale` and material +0x10 / +0x14 say where they go, what they mean is unread (the disc only has levels 0, 3, 4); `ModelTexAnim::wave.period` "in 300ths" is read from the step only; `ModelPartRec::flagE` and `ModelInstance` +0x30 / +0x4c / +0x60 have no reader yet; `rootBone->node` is really the file node, cast, and nothing reads it; a texture whose CLUT is not 32-bit would be misread by the premultiply, as in the ROM.

**yamipan/rendering/model_renderer.cpp, 3D path 5 of 5 (2026-11-04), partial: the first 3D picture.** New host module `rendering::ModelRenderer` (depth buffer D32F, shaders model.vert / model.frag, one render pass per model pass among the 2D stretches, `Frame::modelPasses`), seam `yamipan_drawmodel.h`, game side `rena/rena_draw.cpp` (the eight rena calls of `rena_draw.h`, the walk 0x2e62f0 / 0x2e41a0 / 0x2e6590 / 0x2e6b70 / 0x2e83b0, the blended table). Also needed and written in this run, which bent "one TU": **`game/stagecamera.cpp`** (ctor 0x177d60, `calcViewMatrix` 0x177810; the camera's matrices are now `StageCamera` members, no longer `&m_itemDef` / `&m_parts`) and **`game/mapobject.cpp`** (`createMapObject` 0x1766c0, `setupMapObject` 0x1760c0).
**Why no model reached the record: `createMapObject` was a stub answering null**, so `applyBgTarget` made every instance (the 22 counted last run) and skipped it at `if (object == nullptr) continue`.
**Seen:** `YAMIPAN_CAMERA=auto`, `build.py --run 290 --pad` with work/newgame_pad.txt, last.png at shot 3000: textured ground of the stage's `Root` model, in perspective with correct depth, dark. **No in-game reference exists, so the picture is not compared with the game**: it shows the path works (geometry, UV, texture, depth, projection), not that colours or lighting are right. Any-aspect: the projection widens with `PictureLayout::aspect`, not yet looked at on a wide window.
**Next, in the order the picture needs them:**
- ~~The camera's step~~ **written 2026-11-04, second run** (see below); it does nothing until a character links it.
- ~~The light formula is a guess~~ **read from VU1 the same run**: colour = vertex * (ambient + sum of colour * max(0, direction . normal)), alpha 0x80, `ModelMesh::vuFlags` & 4 copies the vertex colour instead (architecture.md "What VU1 does to a colour"). `SceneRender::updateLights` 0x1f8620 and `update` 0x1f73c0 are still stubs, so the record's lights are drawSysInit's defaults (ambient 0.1) and a lit mesh is dark until they are written.
- **createMapObject's special classes** (doors _dp / _ds 0x174940, 0x28e5a0, 0x295360, 0x2a7340, 0x2b2a60) come out as plain objects; **setupMapObject's collision half** is left out, so nothing on a map collides.
- ~~**Skinned meshes**~~ **written 2026-11-04 (rena/rena_draw.cpp, end of this section)**, was: 0x2e43e0 / 0x2e68a0 are skipped (parts->boneCount != 0: characters do not draw). The palette: bone world * inverseBinds[b] * M, M = boneBinds[mesh.bone] or unit; the 7-tag batches carry bone ids and weights (architecture.md "The batches of a model"). The pose TU comes with it.
- Left out of the draw: ~~a ModelMaterial_Kind1's two frames (base only)~~ (written 2026-11-09: the faces animate, architecture.md "A face is a material of kind 1"), the alpha fringe (the ROM's second GS pass 0x2fdb60 blends what TEST_1 dropped; the host discards it, so plants have hard edges), ghost pass 0x2e7960, fade 0x2e96a0 / 0x2e9220, mips (0x2e7240's level choice), shadows, spotlight, `m_altClut` for _s textures, `SceneModelHolder::draw`, per-object draw order (`StageNodeTree::draw` is still a stub).
- Performance: every vertex is transformed and lit on the game thread each frame and uploaded whole. Fine for a test; cache per model when characters come.
Doubts for the user: the file names and `StageMapObject` are the port's; `partOutside` culls wide of the frame instead of porting 0x2e2a60; `clearDepth` keeps the depth across far_1..3 (read from the list comments, not from the packets); 0x91 on map models is copied without names; a keepsake of the debug hook in game code is `// PC:` until the step is written.

**yamipan/rendering/model_renderer.cpp, second run of the 3D path (2026-11-04), partial: the main camera and the VU1 light formula (batch 106).** Also written beside the TU: `game/stagecamera.cpp` is now the whole base (addToScene 0x177d20, lookAt 0x177900, aimFrom 0x177860, setScrZ 0x177ac0, onViewTop 0x177d00) and `StageMainCamera` (ctor 0x179700, step 0x178160, onViewTop 0x179490, onLinkChanged 0x179680, its five CollisionOwner hooks). `StageObject::onViewTop` / `cameraStep` / `onLinkChanged` are virtual now, the other cameras' slots stubs. `MotionDef::offsetA` / `offsetB` / `offsetFlag` became `cameraFocus` / `cameraEye` / `cameraShift`: they are the camera's numbers. `tools/reverse/vudis.py` carried over from RosePan, with the load / store and EFU forms it lacked.
**Why the stage still needs YAMIPAN_CAMERA: no character exists.** `StageCharacter`'s ctor 0x17de30 and `setup` 0x17c700 are stubs, so `setupCameras` finds no player, nothing calls `setPlayer`, `m_linked` stays null and the step returns at once. **The step has never run**: it reads `m_motion`, `m_input`, `m_ground`, `m_bodySize`, `m_aimPoint`, none of which any object has yet. `StageCamera::addToScene` being a stub also meant `find0("camera")` answered null; it is real now, and the run below did not mind.
**Seen:** `YAMIPAN_CAMERA=auto`, `tus.py check --run 290` with work/newgame_pad.txt: no crash, assert or fault, build, callcheck and layoutcheck clean, 145 screenshots, stopped still running. Mean brightness of the same view, 0..255: the old added formula 28.86, the VU1 formula at ambient 0.1 3.14 (almost black), the VU1 formula with `YAMIPAN_AMBIENT=1` 10.92, max 54. So the override reaches the game and the multiply answers to the light (3.5 times, not 10, because the default directional lights add to it), **but the map is still dark at 1.0**: its vertex colours are low (a night stage), or the real lights go above 1, or dark is right here: see the user's lead on the opening event below. **The view is `aimDebugCamera`'s, not the game's**, so no shot so far says anything about where the game's camera stands. **The picture is darker than last run's and is not compared with the game**: the formula is proven by reading the microcode, not by the screen. progress/ holds the 1.0 shot.
**Next, in the order the picture needs them:**
- **The lights: pinned first by the user, closed 2026-11-04** ("Done", at its top).
- **The character.** ctor 0x17de30 (the input 0x239c60 is short; 0x1dd680 brain 0x9b0; `new` of the motion 0x1ed410, 843 lines of decompilation), setup 0x17c700 (setModel, the record, `createBlock98`, 0x1ecda0 which needs `setBaseMotion`, `bindBones`, `placeBones`; 0x17d970; the ground 0x1f3d20 EE 0x1c0), and below them the pose TU unassigned@00149510 (0x14e0a0..0x151020), because `onLinkChanged` and StageSound both find `bn_head` in a bone map only a loaded pose fills. That is several TUs; the pose first. `YAMIPAN_CAMERA`, `stageCameraDebugLookAt`, `aimDebugCamera` go when the step runs on a real character.
- **game/scriptrunner.cpp left to do (2026-11-04):** natives by what a run reaches (`[stub] scriptNative_<id>`); stage 1's object script (script0, 71 natives, most used 0x53, 0x89, 0x79, 0x12c, 0x15b, 0x136, 0x14f) runs only once `runStageScripts` has a mission and characters. `sssnatives.py --list lo-hi` shows each one's size and callees. Base `getProperty` 0x1cfc20 / `setProperty` 0x1cf200 and `StageObject`'s 0x177f30 / 0x177380 are stubs answering 0. Doubts: **a native that leaves an object in acc must leave its handle**, because a host pointer does not fit the 32-bit registers (even values are refused in `scriptObjectOfValue`); nothing sets `m_parentScript` yet, so the natives that do (and `ScriptFrame`'s callers) are unread; the ROM's case 7 passes a stale register as the property sub index, the host passes 0; opcode 0x22 (a raw EE pointer) keeps the runner as target: no script read uses it. Names `ScriptTable`, `ScriptBlock`, `ScriptFrame`, `cmdSet*` and the natives' are read from the code, not the game.
- **game/demoevent.cpp, first run (2026-11-04), partial (batch 109).** Written: `StageMission`, `StageEvent` with its file and script forms, `StageFade`, `gsFillRect`, `DemoCamera`, the pack lookup, `DemoStream` but its subtitles, natives 0x4e, 0x53, 0x138, 0x139, 0x155, 0x15f..0x162, 0x1f2. **Proven: the opening is d0101** (architecture.md "Scripted events"). **Not done: the picture.** The event runs and nothing of it shows; `references/roadmap-3d-control.md` has what stops it, in order, and the user's new analysis. Three `PC:` guards to delete: the null target in `ScriptRunner::execute`, the 60-frame bound in `enterSubScene`, the bone checks in `DemoCamera::cameraStep`. Doubts: "film" and "flecks" for the overlay's two effects are guesses from their shape; `kDemoCutSpeed` 1.2 as 60 / 50 is a guess; `reset` is called by the ctor only here; `MissionNotice` holds one record because the next field starts 0x14 later, though its setters take an index; the setters of the timed records and notices are left out until a native needs them (0x286cf0, 0x286de0, 0x286e00, 0x286f30); the whole mission now runs every frame, so stage scripts reach stubbed natives that pop and skip.
- **Queue decision of this run (2026-11-04, for the user to overrule):** `game/modelpose.cpp` went to the top, because both the opening's camera and the character stop on it. Nothing else in the queue moved.
- **game/scenepairs.cpp (2026-11-04), verify (batch 117): the frame's action steps run, and the opening moves.** All 18 of 0x171300..0x171a30 written (`ScenePairTable`, `ScenePairList`, `ScenePairNode`, `ScenePairListId`; moved out of `gamehooks.h`), with the plain map object's frame and setters in `game/mapobject.cpp`, because the table would otherwise have called empty hooks (architecture.md "The action lists"). **Seen:** New Game run of 240 s with work/newgame_pad.txt, 7,440 frames, no crash or fault; 75 distinct stubs reached, none of them a side hook. The queue's note said nothing could show this; d0101 shows it: in the same cut, shot 4260 has the father's hand reaching across the boy's face and shot 4320 has it on his hair, and the boy's head has moved (progress/20261104-d0101-motion-a.png, -b.png). Later cuts and the subtitles follow. **No shot of the real game exists, so it is compared with nothing but itself**; the motion is plausible, its speed unchecked. **Not seen:** the sea (no event camera looks at it; `YAMIPAN_CAMERA` or a temporary camera would), `StageLight::sideHook2` beyond the picture staying lit, any item or bullet (none placed; the bullet of setupCameras is off), the three map setters (nothing seen calling them). After the event the screen is black as before: no player. Doubts for the user: the file name; `ScenePairTable` is five lists, not pairs, and kept its name to spare every caller; the list is presumably a template with one instance found; `m_touchedBy` read from the collision hook only; the ROM's `sideHook2` multiplies the step by 1.0, left out.

- **game/demoevent.cpp, second run (2026-11-04), verify (batch 116): the subtitles.** All 94 functions of the TU are written or accounted for. Added: `DemoStream::setFont` / `drawSubtitles`, the fsm readers (0x28fa90..0x28ff60 added to this TU), `MissionNotice::format`, `showNotice`, `setNoticeEnabled`, `setNotice`, `addTimedRecord`, `setPurposeText`, `eventPackName`, `eventDemoDir`, and `WideFontPrinter` (0x2d3700..0x2d4150 added to `game/fontdata.cpp`). `rx/rxsndstradpcm.c`: the silent bridge now counts its position (`rxSndStrAdpcmBridgePosition`, reached from `rxSndAdpcmGetPosition` by a `PC:` line), and `rxSndStrAdpcmGetHandle` is a bridge. **Seen, New Game, 235 s, no crash, no `YAMIPAN_CAMERA`:** progress/20261104-d0101-subtitle-outside.png ("It's outside!" over the turning ceiling lamp) and progress/20261104-d0101-subtitle-daddy.png ("Daddy...? Kanae...?" in the room); also "Open the door!" over a black frame. The words and times are the file's, read back in the run. Spacing looks wide, not compared with the game. **The event's picture has no reference**, so it stays `verify`. Shot numbers run about 11 a second in the event, and shots up to about 1800 are the intro movie, not the event. **Left:** `gsFrameFilter` 0x2849c0 (read: passes of down-sample, blur, up-sample; the blur rate is `arg`), `demoFilmDraw` 0x204550, `demoFleckDraw` 0x2a11c0 and `demoFleckStep`, none in this TU: each needs a host pass, and the first is d0101's own look, which may explain why the picture is so dark. They have no key in the queue: **the user's call whether they go before the character** (they are not needed for control). The natives that call the new mission setters (0x1ae400, 0x1af120, 0x1af290, 0x1af830, 0x1af9e0, 0x1afd20) stay with `game/scriptrunner.cpp`. `PauseMenuState::addNotice` and `update` are stubs. Doubts: the index argument of the notice setters is honoured for 0 only (`PC:`), the ROM would run into `characters`; `isNotRunning` is a plain description, the callers are unread. The three `PC:` guards of the first run are still there: the null target in `ScriptRunner::execute`, the 60-frame bound in `enterSubScene`, the bone checks in `DemoCamera::cameraStep`.

- **The opening event (the user's lead, 2026-11-04, live: "the game begins with a scripted event where it is almost perfectly dark and you see something circle ... it might be wrong coordinates due to those events ... not being implemented").** The last run agrees: `StageScript::relocate` 0x1d3a40, `ScriptRunner::ScriptRunner` / `setEntry` 0x1d2f40 / `run` 0x1d27e0, `StageMission::StageMission` 0x287900 / `setup` 0x2870f0 / `tickPending` 0x286ce0 / `afterScripts` 0x286c40 and `DemoCamera::DemoCamera` 0x26ce20 are all stubs and all reached, so **no script has ever run and no event can start**. During an event the view is the demo camera (view priority 4 over the main camera's 0), so the main camera's step would not be what the opening shows even with a character. Expected in the game there: nearly black, a circling something. So a dark stage may be right, and the first test of the lights needs the event's camera. Unproven: that the opening is the event d0101, the only one with its own filter in `StageScene::draw` (`gsFrameFilter` 1 pass, arg 5, while the demo camera's cut is below 2); that filter is a candidate for the circle, nothing more. To find the path: mission +0xc8 (the pending event's name, `afterScripts` starts it), `requestSubScene` 0x196060, `DemoCamera` 0x26ce20 and its step, `DemoExecuter`, `drawOverlay` 0x26c920. **Queued as `game/demoevent.cpp`, right after the lighting (the user, 2026-11-04).**
- ~~**The lights (pinned).**~~ Written, closed ("Done"); architecture.md "The lights of a stage". The question it asked is answered: the keys come from the stage's script (entry s01_l on stage 1), and colours go up to 2.0. `getActiveCharacter` was the top camera (now `getTopView`), so no guard was needed for it.
- ~~Back-face culling~~ and ~~the point light 0x545~~ **written 2026-11-05 (Done)**. **The ghost's alpha** 0x011, **the moved vertices** of material flag 8 (0x186): architecture.md.
- The other items of the list above (special map objects, collision, skinned meshes, the left-out draw features) stand.
Doubts for the user: the names are the port's; `lookPitch` / `lookYaw` (input +0x10 / +0x18) are named from how the camera uses them, 0x239d50 that writes them is unread, so a swapped pair would show as a stick moving the view the wrong way; `padSway` is a stub for a float of the pad's subclass (+0x12c), unread; `GroundVolume` ("the camera stays above its top") is read from this one reader; `getNormal` is a stub: a face's +0x48 needs the collision TU's layout. The motions in `kSwayMotions` have no labels yet.

**game/scenerender.cpp, the pinned lights, first run (2026-11-04), partial (batch 107).** Written: all of `SceneRender` 0x1f6ac0..0x1fa900 and rena's light records, `game/vectortrack.cpp` (VectorTrack, VectorKey, VectorFade: was `StageModelSlot`), `StageCamera::setRange`, `rotateMatrixY`, `scaleVec3`, `blendVec4`, `boxContains`. `SceneRender` moved from gamehooks.h to `game/scenerender.h`. Renamed by what the code does: `StageModel::_b4..` to `m_lit`, `m_ownAmbient`, `m_lightProxy`, `m_lightSet`, `m_inSbBox`, `m_lightDirs`, `m_lightColors`, `m_ambient`, `m_lightFades`; `StageLight::m_240..` to `m_modelFade`, `m_modelFadeFrames`, `m_lightsModels`, `m_modelColor`, `m_modelAmbient`, `m_priority`; `SpotLight::m_2d4` / `m_2d8` to `m_texture` / `m_intensityPercent`; `StageScene::getActiveCharacter` to `getTopView`. `tools/reverse/sssdis.py` is new.

**Seen:** `YAMIPAN_CAMERA=auto`, `tus.py check --run 290` with work/newgame_pad.txt: 3000 frames, no crash, assert or fault, callcheck and layoutcheck clean, and the shot is the same near-black ground as before (work/shot_unkeyed.png): the guards keep the defaults. **A temporary test, not committed:** setup keyed every set with ambient 1.0 and three lights of (1, .9, .8), range 16000: the ground came up visibly, textured, no NaN, no fault, over 200 s of run (work/shot_keyed_test.png). So the path from tracks to fades to record to `renaSetLights` works. **The test's numbers were made up and it is not a comparison with the game: no picture here has the script's real values yet.** The test did not reach a model with `m_lit` that I could look at, so `lightObject` ran at most on items and is unproven by the screen.

Stubs now on this path, for the character's TU: `PlayerMotion::getLampMatrix`, `StagePlayer::setGearLightOrder`, `StageObject::getCharacterClass`, `isYriOrKne`, `followsPlayerLight` (a field +0x5cc of a character, no address). Doubts for the user: (1) the file names and `RenaLight` / `RenaPointLight` / `RenaSpotLight` are the port's; (2) which entries of the s01 file run when is read from `setupCameras`, not seen; (3) `addSetKeys` walks the tracks of a set through one lambda, not in the ROM's order, which only matters if key creation order did; (4) the two record lights, the point and the spot, are filled but no host draw reads them; (5) `m_e10` is written and unread.

**game/modelpose.cpp, partial (2026-11-04, batch 110): the pose plays; the solvers and the event's picture do not exist yet.** Roadmap step 1. Written: `ModelPose` ctor, dtor, load, update, solve, the four play calls, the walk, every target setter; `ModelPoseBone` (bind, seek, readKey, interpolate, buildMatrix, advance, stepRoot, keyAt, aimAxis); `ModelMotion`; the quaternions 0x149510..0x1497c0; `motionBoneMatrix`, `motionBoneSuffix`; script native **0x159** (`nativeDemoCut`, in scriptrunner.cpp, which starts a cut: without it the camera has bones and no motion); in vu0math.h the real `setVecLength`, `scaleVec4`, `vecCross`, `wrapAngleVec`, `wrapAngleLane`, and `vecNormalize` keeps zero as zero (the VU does, the host gave NaN).

**Seen:** a New Game run of 200 s, no crash or fault. `DemoCamera` has 3 bones. Sampled every 60 camera steps from step 1 to 421: `bn_top` time runs at 1.2 a step (1.2 .. 505.2), position stays around (7240, 1161, -6103) with a slow handheld drift, its forward row swings between about -0.08 and -0.21 in z, `lens` 50 degrees; rows are unit and orthogonal to four places in every sample. The bone checks in `cameraStep` are deleted.

**Not seen: any picture of the event.** Everything after the intro movie (from about shot 5400 to the end of the run) is pure black, so nothing on screen proves this code, and the camera's world matrix reaching the renderer is unchecked. Black is what the roadmap expects while the sub-scene's objects are stubs, but that is the expectation, not a measurement. No character uses a pose yet either, so `load`'s name lists, the blend into a new motion, `stepRoot` (flag 0x40), the growth tables and the walk have run on nothing, or on the 3-bone camera only.

**The rest of the TU, second run (2026-11-04), verify, batch 111: all 92 written, no stubs.** The five solvers, `solveLeg`, `settle` / `settleFixed`, the swing (`stepSwing`, `stepSpring`, `addWind`, `limitSwingAxis`, `swingAhead`, `swingBetweenCalves`) and the aim helpers; architecture.md has what they do. Renamed by what the code does: `solveNeckLimits` to `solveChestTool` (it hangs the left tool bone on spine1) with `hasChestTool`, `chestToolPosition`, `chestToolAxis`; `radius` to `length` (+0x1f0, +0x2c of the record: the leg solver reads it as a bone length); `_395` / `_3a0` to `windOn` / `wind`; `lookArg` to `lookBone`, a bone, not an int; `_374` to the three head limits. `swing` was the w of +0x270 and is `swingPos[3]` again.

**Seen, outside the game only** (scratch test linking modelpose.cpp, not committed, with libm stand-ins for sinCos and asinAcos): `solveLeg` puts the foot on the target to 4 places, `kneePlacement` closes on both lengths, `aimOwnTurn` and `aimThrough` end at cosine 1.0000, the S spring settles on its target with no NaN after the parent jumps. `aimLimited` ends at cosine 0.65 on a 100-degree turn: that is the ROM, which multiplies flat * pitch (asm 0x14b5a8), not a slip here. **In the game:** a 200 s New Game run, no fault in the log; no pose solver can be said to have run, because `m_solvers` has no writer, no character has a pose, and nothing logs a sw bone. The screen is what it was: title, then pure black after the intro movie.

`m_solvers` (+0x20) and +0x1a0 still have no writer found so far: the bits' names come from what each runs.

**Doubts for the user:** the names of this second run are the port's, read from the code: "chest tool" is a guess at what those five carry; the bone-name letters are read from the switch, no real sw bone name was looked at, and `solveTool`'s arm chain names (hand, wrist, forearm, upper arm, shoulder) assume the order of kLeftArmBones. `getRecordOffset` / `placeRecord` read their record as a `PoseBoneRecord` from the +0x20 fields only. `getFootPositions` assumes a toe's position through its parent (the ROM reads +0xc). `kPoseFlagHasPelvis` is written to +0x18 and `solve` tests +0x1c: two fields, as the ROM has them, the first looks unread. Native 0x159 passes overlay 0 because the ROM compares a null pointer with "SJN" / "SJE" there; on the PS2 that reads address 0, so an overlay is possible in principle. The PS2 byte at runner +0xb8 (`m_cutWaited`) is set by 0x1b3ae0, 0x1b7e70, 0x1b9370 and cleared by none of them.

**game/subsceneobject.cpp, verify (2026-11-04, batch 112, 56 names): the first pictures of the opening.** Written: `SubSceneModelObject` and its lamp, the map, gate, door, character, item and prop classes (the stubs are gone), the small `CharacterHidePart`, `applyCharacterModelFlags`, `applyMapModelFlags`, `isGateModelName`, the eight `StageMission` result accessors, and natives 0x144, 0x145, 0x148, 0x163, 0x164. The classes moved from `gamehooks.h` to `game/subsceneobject.h`. architecture.md "The event's objects" has the map.

**Seen (`build.py --run 330 --shot-every 150`, work/newgame_pad.txt, no `YAMIPAN_CAMERA`, no crash):** the screen is no longer black after the intro. Two pictures from the event's own camera, with the letterbox: a tilted square ceiling lamp, shot 3600 and earlier, and at shot 3900 a door between two walls, so `bn_top` moves the view and the cuts change. Copies in progress/. **Both are very dark, no character shows and nothing was compared with the game: there is no reference for this scene.**

**The black after the event is explained (second run, 2026-11-04).** It was two things, one after the other, and the first reading above in the history ("the script ends early") was wrong. (1) The script does not end early: it runs d010100Cam01 from frame 0, d010101Cam02 at 929, and fades to black itself at 1262 (native 0x164, 15 frames), then stops. The event then waits for its voice (`StageScene::update`: `going` is the script or the stream), and the stream stub never ended, so the event held its black last frame for ever. **Fixed by a bridge in `rx/rxsndstradpcm.c`**: a silent stream that lasts as long as its request. d0101's voice is 2,798,720 bytes, 48 kHz, 2 channels: 51.02 s, and the event now ends at frame 1276, one frame from the end of the script's own fade (1262 + 15), so the host's 25 frames a second and the script's timing agree. (2) **The user, watching live: "after the events ends, it goes to a black screen and then nothing, I think there is a fading and then it should give control to the player but something is missing." Right, and measured:** the stage comes back (`Root` runs, top view kind 9, frames to 4080 and on, no crash) and **the fade is not it**: `Root` calls native 0x4e fadeIn(15) on the third frame after the event and the level reaches 0. **What is missing is the player.** `Root` calls native 0x70 with `F_PC01` (0x1ae0), which finds the character and calls `setPlayer`, which links the main camera; `StageCharacter` is a stub and 0x70 too, so nothing links the camera, it stands at the world origin in empty space and the frame is black. Proof: the same run with `YAMIPAN_CAMERA=7236.6,6105.9,1207.1,7211.6,6144.9,1118.1` shows the room after the event (progress/20261104-d0101-after-fixedcam.png), fade clear. So the next visible step after the event is roadmap-3d-control.md steps 4..6, already queued (`game/stagecharacter.cpp` and on), and until then the screen stays black there without `YAMIPAN_CAMERA`. Nothing else stands between the event and the stage: natives 0x136, 0x49, 0xcf, 0x3e, 0x3f are reached afterwards and pop and skip.

**The task's question:** `DemoCamera::playCutNamed` is reached two ways: native 0x159 and **native 0x148**, runner slot +0x140, thunk 0x26d000. Both are written.

**Still stubs here, by choice:** `MapObjectParts` (ctor, dtor, `copyShown`) and `collisionFindRecord` wait for the collision file, so a map object has no parts and answers as with a null record; `GateParts` (no gate was seen to bind); `StageObject::setupBody` 0x17d970 goes with StageCharacter. Their prototypes are in `gamehooks.h`. Left out: 0x2689a0, a sound on a named bone, called by nothing written.

**`PC:` marks once meant to go with StageCharacter:** kept on purpose (2026-11-04). `SubSceneCharacter::setup` and `bindMotion` still test for a null stage self, and `ScriptRunner::execute` still guards `m_target`: F_PC01 exists now, but no run has shown that every event's character does. Delete them when a log shows no event reaches the null path.

**The objects animate since `game/scenepairs.cpp` (2026-11-04):** `sideHook2` is `ScenePairTable`'s third step. `DemoCamera` steps by itself. `game/scenepairs.cpp` is in the queue after `game/demoevent.cpp`.

**Doubt for the user (2026-11-04, the format written 2026-11-06, the rest stands):** the streams have no sound: the voice's length only, on a wall clock (rx/rxsndstradpcm.c, the driver's silent side). A request with bit 0 of flags loops and never ends. The silent streams now end for music too: if a run shows a stream restarting or a wait that was held before going on, look here.

**Doubts for the user:** (1) all names but the class names already there are the port's, read from the code: "lamp", "actor", "hide part", "gate parts", the property names; (2) `applyMapModelFlags` now also clears 0x80 and sets the own-light bytes on a kind 3 stage object, as the ROM's inline copy does: the stage's map objects were built without that; (3) property 0x6c always answers 0 in this family, as its kinds never match; (4) native 0x163's colour is the second argument alone, because the ROM overwrites its own shifts: if a fade comes out the wrong colour, look here first.

**rena/rena_draw.cpp, skinned meshes (2026-11-04, batch 114, 16 names), verify: the first character on screen.** Written: `drawSkinnedMesh` 0x2e43e0, `skinnedPartOutside` 0x2e68a0, `buildPalette` 0x2e9480, and the skin in the host module (`YamiPanModelBatch::palette`, `boneIds`, `weights`, `bonesPerVertex`, `firstBoneOnly`, `skinVertex` in model_renderer.cpp). Renamed in the headers: `ModelPrim::_0` to `paletteBones`, `isLong` to `paletteCount`, `skinned` to `listFlags`, `ModelMeshPart::_24` to `paletted`. architecture.md "Skinning". The TU is pinned 0x2e41a0..0x2e44e0 and 0x2e5450..0x2e9790.

**Seen, New Game runs with work/newgame_pad.txt, no crash:** (1) at shot 2400, from the event's own camera with the letterbox, **the boy of d0101 lies on the bedding, seen from above, whole**: pajamas with their planets and stars, hands, hair, the joints unbroken (progress/20261104-skinned-character-d0101.png; that run had `YAMIPAN_CAMERA` set, which the event's view outranks). His head bone stood at (7255.6, 6079.3, 1142.3), by his root at (7258.5, 6064.5, 1137.3), which is `F_PC01` of the stage file. (2) A temporary camera on `iMK99_wave00_101`, centre (4760.8, 5101.3, -158.6): **a sea that meets the quay**, one surface with no cracks between the copies, a tyre fender on the wall (progress/20261104-skinned-waves-oMK01_01.png; the window happened to be about 3.7:1 wide). **No shot of the real game exists, so neither is compared with the game.** What they prove is the task's test: a skinned model stands where its bones say. Status is `verify`, not `done`, for that reason, and because the pinned range still holds the unwritten shadow, spotlight, ghost and fade code. These two tests ran on a build with temporary logging and the wave camera; the final build, the same skin code with those lines removed, ran the 15 s check only.

**Left to do (the motions: closed by `game/scenepairs.cpp`, entry below):** Characters are dark on their own lights: `SceneRender` has no player to light. Fade batches (`listFlags` & 2 with a fade set: 0x2e9220, 0x2e96a0) draw as the plain list. Back-face culling stays out.

**Doubts for the user:** (1) for the first 900 or so draws, `head_00`'s bone stood at (-13.0, 31.4, -384.2) while its root was at (7258.6, 6064.2, 1137.0), then jumped to the root. It reads like a pose not composed with its root until the event's first motion, and nothing on screen showed it, but it is unread. (2) The ROM stores a skinned part's centre only when 0x2e2a60 answers 1, so a part wholly in frame sorts by the previous centre. The host always stores it (a blended skinned part may sort differently from the ROM, and more sensibly). (3) `Skin` copies for the blended table are the port's, and each prim's palette is rebuilt per material on the game thread: not measured, roadmap "Performance" stands. (4) The Ghidra names of this batch are the host's short ones (`drawTree`, `drawPart`, ...).

**game/stagecollision.cpp, step 2 of the roadmap (2026-11-04, batch 118, 38 names), verify.** Written: the collision file (`relocate`, `collisionFindRecord`), `StageCollision` with its tree and every query, `StageCollisionFace`, the `BoundingSphere` tests, `CollisionBody`, `CollisionBodyArray`, `SceneRecordBuckets` (`add`, `remove`, `collide`, `collidePair`), `surfaceOfName`; in `game/mapobject.cpp` `setupMapObject`'s collision half, `StageMapObject` as a `CollisionOwner` with its dtor, and `MapObjectParts` (ctor, dtor, `copyShown`). So `StageScene::raycast` and the other scene queries now answer, and the world's collide step runs each frame. The stand-ins moved from `gamehooks.h` to `game/stagecollision.h`. Changed signatures: `CollisionOwner::collisionHook0` takes contact, normal, face, depth; `sphereHits` returns a count and fills `StageCollisionFace **` (the host's pointer would have overrun the old int); `heightAt`'s last argument is the second face; `StageCollision::nodes` was the mesh's matrix, now `m_matrix`.

**Seen, New Game runs with work/newgame_pad.txt:** no crash, d0101 still draws (shot 1500: the boy in bed and the man beside him; shot 2100+: the ceiling lamp). A temporary test in the mesh ctor, since removed, on stage 1's real data, 3,000 random segments, vertical rays and spheres against each mesh of over 100 faces, compared with brute force over all faces: `Root` (8,165 faces) rayBad 0, segBad 0, sphereBad 0, heightBad 1 of 3,000; `oMK06_03` all 0. Against code it does not share: 0 hit points off their plane, 0 outside their triangle by barycentric test, 0 off the segment, and 1,000 down-rays agree with `heightAt`. The one height miss was not chased. **Nothing on screen shows collision yet: no character stands.** Moved meshes (`m_matrix` set), the world's pairs and `spherePush` have run in no test.

**Left to do:** `StageObject::breakPart` 0x175900 is a stub, so a bullet breaks no glass; `MapObjectParts` leaves the doors' effect null (kinds 2, 0x21: a bone's name by `FUN_00186f80` and the door's table) until those classes exist; `PartSwitch::init` / `show` are stubs, so the parts show nothing; `sSurfaceUsed` is cleared per stage by `resetSurfaceTables` (written 2026-11-07) and nothing reads it. `FUN_00295010`, the special map object with several meshes, is unwritten.

**Doubts for the user:** (1) the file name and every class name are the port's, and `kCollisionMelee` for category 0x40 is a guess from `collisionHook0` testing the object's +0x238; (2) `surfaceOfName`'s ids stay bare numbers: the surface table is not read, so an enum would be invented; (3) the ranges pinned hold two runs plus the surface helpers 0x2cedb0, 0x2cf590 by call, not by position; (4) a push's w is forced to 0 in `spherePush`, where the ROM leaves two cases to the VU register: it matters only on a moved mesh.

- **game/scriptrunner.cpp, 2026-11-05 run, partial (batches 139, 140): every native of stage 1's object script, then the most used on the disc.** Written: 0x47, 0x4c, 0x4d, 0x4f, 0x52 (stage clear), 0x5b, 0x5f, 0x60, 0x6f, 0x71, 0x7a, 0x83, 0xa8, 0xad, 0xaf, 0xb4, 0x126, 0x13c, 0x13d, 0x20f, then 0x2f, 0x31, 0x3c, 0xb9, 0xbb, 0xbc, 0xbd, 0xc3, 0xca, 0xde, 0xf4, 0xf5, 0x15d, 0x16a; `StageCharacter::tryShout`, `playVoice`, `isInReachOf`, `measurePath`, `stopMoving`, `holdsWeapon`, `scriptSetBrainEntry`, `scriptHeardKind`, `scriptIsCrouching`; `StageObject::scriptAddMenuLine`; `SceneBoxList` was misnamed and is now `SceneEffectList`, new TU game/sceneeffects.cpp, all 7 written. **Seen, New Game, 240 s, work/doorwalk_pad.txt, no crash, a temporary trace removed:** 67 natives ran, among them the new 0x13c, 0x13d, 0x20f, 0x5b; no `[stub] scriptNative_` line. The picture is the dark first-person wall as before: nothing here draws. **Not proven:** the other 30 new natives ran in no run (their scripts come later); 0x2f and 0x15d are the ones to watch. **Stubs behind them:** `StageMenu::addLine` (0x230800 -> 0x209d90, the list window 0x208f40..0x20ac30, so a locked door's question stays silent), `StageNodeTree::spawn`, `spawnMotes` 0x190990 (class 0x2b3ef0, 300 steps at birth), `isLive`, `StageParticle::setStopFrames`, `RouteFilter::pathLength`. `measurePath` answers -1 (no way) while the route has no filter: a `PC:` guard. StageParty's range and angle live on the host class until its record 0x203d70 exists, with the ctor's 10000 and 20 degrees. Doubts for the user: the names (`spawnMotes`, `hideLoose`, `addMenuLine` and its `keep` argument, `m_biteCandidate`, whose writers are these natives and which may mean more than biting) are read from the code; `kLineCar` is only a guess from kind 3.

## Decisions

- 2026-11-09 (the user): **RXSNDDRV runs by default.** `YAMIPAN_RXSNDDRV` and `YAMIPAN_RXSNDDRV_LOG` are
  gone: every run boots the driver, the EE's sound calls, SIF DMA and IOP heap go to the IOP host, and
  sdk/sif.cpp's stand-ins (fake IOP addresses, dropped transfers) and the stream formats'
  ADPCM_SILENT_BUFFER / PCM_SILENT_BUFFER with them. The driver logs the first four calls of each
  command and its first 16 stream events. Audible since the same day (the synthesiser, below).

- 2026-11-07 (the user): **RoR's RXSNDDRV reconstruction comes into YamiPan** as `src/iop/rxsnddrv/`, since
  FS2's driver is the same source, revised (architecture.md "The IOP side"). Done the same day for the
  driver itself (compiled, linked, re-addressed, not booted). Left, in order: (1) the IOP host as yamipan::
  classes (cpp-port.md: RosePan's sdk/iop_host, iop_sif, iop_libsd, iop_voice and iop_reverb, about 5,000
  lines: threads, SIF RPC, the SPU2 register model, the voice engine, reverb) and an audio device, which
  YamiPan has none of; (2) boot the driver and let sif.cpp's sceSifCallRpc reach its server, retiring the
  silent stand-ins (rx/rxsndstradpcm.c's "driver's silent side"); (3) FS2's 38 changed, 2 moved and 4 new
  functions and the grown records, read in Ghidra's rxsnddrv.elf before the driver's answers are trusted.
  **2026-11-09: (1) begun.** The IOP kernel and memory are in (`src/yamipan/iop/`, architecture.md "The IOP
  side"); the driver boots (on every run since 2026-11-09, the user's call), its threads run (tick at
  50 Hz), and its RPC server
  answers through `YamiPan_IopRpcDispatch` (command 0x12: version 0x0101); libsd runs on `iop::Spu2`
  (registers, sound RAM, transfers that complete, voices that play in time); (2) is in too: the EE's
  calls reach the driver, SIF DMA copies, the IOP heap is iop::SystemMemory, and the stream formats' silent
  sides step aside (architecture.md "The EE's side"; the boot, two banks and an event seen, its shots
  the same as without the driver). **Doubt (2026-11-09):** the event's ADPCM stream starves: the EE wrote
  about one 0x180..0x700-byte block per frame (1.5 MB in all), while 48 kHz stereo ADPCM plays about
  55 KB a second, so the driver's SPU ring ran dry (fill 0x80 at its end). About 1.1 KB a frame is that rate
  at PAL's 50 frames a second, and these runs drew about 28 (Remote Desktop), so the feed may simply be
  frame-bound; the other suspect is the driver's status reply (RoR's layout: the free space the EE reads
  in rxSndAdpcmUpdate), part of (3). Check at full frame rate before reading (3).
  The voice engine and the sound card are in too (2026-11-09, architecture.md "The synthesiser and the
  sound card"): d0101's event stream is mixed and played. Left of (1): reverb (RosePan's
  sdk/iop_reverb.cpp), the player's volume settings; then (3), FS2's differences.

- 2026-11-05 (Claude's call, a scheduled run, for the user to overrule): **doors are on, and `kDoorsEnabled` is gone.** The stage crash that switched them off is explained and fixed (Done, game/playermotion.cpp sixth run). `StageMenu::update`, `draw` and `destroy` stay stubs on purpose: with `update` written and no list window, the menu button would open a menu nobody sees that holds the input focus. `game/stagemenu.cpp` is a second file in a one-TU run, like `game/uiitemlist.cpp`: its stubbed ctor was the crash.

- 2026-11-04 (Claude's call, a scheduled run, for the user to overrule): **`CharacterInput` went from `gamehooks.h` to its own `game/characterinput.h`, with `CharacterPad` (vtable 0x3164f8), `StickHistory`, `PadAction`, `ControlFlag` and `CharacterFire`.** `CharacterInput::update` 0x236b90, one ROM function of 0x2f00 bytes, is cut into `step*` sections; `lookPitch` / `lookYaw` became accessors on one vector, since `lerpVec4` writes the four lanes. `UiShowFlags` is really the script's control switches (17 bytes: walk, run, sticks, lamp, car), so the name is wrong; it stays until the UI side is read. `GameOptions::_14` / `_15` are `invertY` / `invertX` (default 1, 0). `m_turnRate` comes from the sheet as a PC default, as walk and run do, until `setProperty` 0x212530.
- 2026-11-04 (Claude's call, a scheduled run, for the user to overrule): **`game/characterinput.cpp`, then `game/characterground.cpp`, moved ahead of the partial `game/playermotion.cpp`.** What is left of the motion (24 conditions, the other links, the list 0x1df0b0, `clearNoFlash`) is attacks, doors, ladders and the like, and no run can test it while nothing fills `CharacterWants`. A `partial` item holds its place, so the queue would have stayed on it for several runs with the character standing still. The input comes first because it alone gives a first playable test, through walls. Brain, player and the rest keep their order.
- 2026-11-04 (Claude's call, a scheduled run, for the user to overrule): **`game/stagecharacter.cpp` can be judged now and stays behind `game/playermotion.cpp`.** The stretched limbs were the missing motion (Done, game/playermotion.cpp). Its status in tus.json is still `partial`; the next runs stay on the motion, because control needs the Walk link first.
- 2026-11-04 (Claude's call, a scheduled run, for the user to overrule): **`game/playermotion.cpp` moved ahead of the partial `game/stagecharacter.cpp` in the queue.** The character's picture cannot be right until a motion plays, and a `partial` item holds its place, so the queue would have stood still on a TU whose own code is complete. `game/stagecharacter.cpp` stays right behind it and comes back when the picture can be judged.
- 2026-11-04 (the user, live: "do as you best see fit"): **the rest of the roadmap is queued, in its order, after `game/demoevent.cpp`:** `game/scenepairs.cpp`, `game/stagecollision.cpp`, `game/stagecharacter.cpp`, `game/playermotion.cpp`, `game/characterground.cpp`, `game/characterbrain.cpp`, `game/characterinput.cpp`, `game/stageplayer.cpp`. The opening stays ahead of the character, the user's order. All of them go ahead of `yamipan/rendering/model_renderer.cpp` and `game/scriptrunner.cpp`, which are open-ended: a `partial` item holds its place, so behind them the character would never come up. This was Claude's call, for the user to overrule. The file names are guesses a run may change with `tus.py define`.

- 2026-11-04 (the user, live): **the sub-scene's objects, skinned meshes and natives 0x70 / 0x64 go in the priority queue.** After `game/modelpose.cpp` come `game/subsceneobject.cpp`, `rena/rena_draw.cpp` and `game/stagecamera.cpp`, then `game/demoevent.cpp` and the rest. They sit ahead of `demoevent` because `tus.py next` takes the first queued item not `verify` or `done`, and `demoevent` cannot finish without them. The natives have their own key because `game/scriptrunner.cpp` is 84 of 652: moved up, it would hold the slot for many runs. Their code still goes in `scriptrunner.cpp`. Still not queued: the rest of the roadmap, `game/scenepairs.cpp`, collision and `StageCharacter`, so nothing can show the natives working yet.

- 2026-11-04 (the user, live): **the opening event goes right after the pinned lighting**, ahead of the character. Queue: `game/scriptrunner.cpp` (the VM, which the lighting's values come from, so it serves the pin), `game/scenerender.cpp`, `game/demoevent.cpp` (a new key, the file name is the session's choice), `yamipan/rendering/model_renderer.cpp`.

- 2026-11-04 (the user, live, during the scheduled run): **"add the stub but make sure to pin the lighting as a priority as to not forget about removing the stub".** The stub is `YAMIPAN_AMBIENT`. The lighting is `game/scenerender.cpp`, at the top of the queue, ahead of what is left of the model renderer and of the character; its end condition is the stub's removal (top of "Next up").

- 2026-11-04 (the user, during the rena/rena_model.cpp run): **"make sure the japanese text is there
  when shiftjs text is identified instead of just dropping the raw bytes"**, and to keep it in the notes
  for future sessions. reconstruction.md "Japanese text" has the rule. Done the same day: the printed
  messages in rena_model.cpp, rena_pkt_vif1pktmanage.cpp and sound.cpp are UTF-8 literals (a host
  print now shows Japanese, not Shift-JIS bytes); memcard.cpp's icon titles keep their bytes and show the
  text beside them; packtables.cpp already had it. The user then asked whether compile flags could break
  the comparisons: no, every string compared or written out is still in \x escapes, which no charset
  flag converts (checked: memcard.cpp and packtables.cpp compile to the same assembly with and without
  -fexec-charset=CP932); only the printed literals depend on the charset, and only for how they read.

- 2026-11-04 (the user, "Yes, pin it"): **the 3D path goes first.** After asking what is missing for the
  3D to show, the user pinned it at the top of the queue. Five tasks in `tus.json` "queue", in order:
  `game/packtables.cpp` (first find why no model reaches the code), `game/nodetransform.cpp`,
  `game/stagemodel.cpp`, `rena/rena_model.cpp`, `yamipan/rendering/model_renderer.cpp`. The first
  milestone is one static map model on screen. The file keys are the session's choices (no extents
  pinned yet: each run settles its own with `tus.py define`, and may rename the key); a task left
  `partial` stays pinned until `verify` or `done`.

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

- 2026-11-05 (the user, live, during the eleventh game/playermotion.cpp run): **the first-person view goes to the top of the queue, "since it is needed for gameplay".** `game/viewcamera.cpp`, top of "Next up".

## Open questions

- (2026-11-09, frame rate) **Past the ROM's 25 fps: the user chose A ("Yes, start A")**, blending the last two
  game frames on the render thread, game code untouched (roadmap-framerate.md; B, NTSC at 30 Hz, and C,
  simulating at the monitor's rate, stay unchosen). Done: field-time stamps, the camera blend, the models'
  instances, bones and lights, and the particles (`rendering/frame_blend.cpp`, Graphics > Smooth motion, on by
  default), seen on stages 2 and 7. The test scene is **stage 2** (the user: stage 7's opening takes control away
  for a tutorial prompt). Not blended: the placed-path batches, the sky mist's own drift, the game's own 2D.
- (2026-11-07, game/kunitoris.cpp, sixth session) **The TU is complete: 93 of 93 functions, `verify`** (batch 229: the drawable and mover bases, kunitorisRandom, copyPair, the two update thunks). Not seen: the picture test needs the smoke pad taken from the language select to the top menu's answer 10 (Extra mode), the title, the level select and a level; compare with the level files (pack items 0..15) and the pictures 0x10..0x14 (tm2png.py on dataEU/kuniEU.pak). Earlier note follows.
- (2026-11-07, game/kunitoris.cpp) `KunitorisGame`'s destructor is written (inline in the ROM; the board's pieces through `~KunitorisBoard`). Open: the board hierarchy's virtuals (vtables 0x319f60 / 0x319f70 / 0x319fd0 / 0x319f80 / 0x319fe0, architecture.md) are not modelled yet: `KunitorisBoard` and `KunitorisPiece` are plain host classes; FUN_002c81c0 (the board draw) needs the piece's slot 1 draw(drawer, pass), so the next session should make 0x319f60 a virtual base (dtor, draw) first. `m_selectToggle` is still a guess from kunitorisThread alone; the fields _84.._98 and _75c.._774 are named since the fifth session (2026-11-07: the spawn / drain / step periods and timers, m_heldDir, m_frame, the fever bar, rate, level and chain; architecture.md "Kunitoris's rules"); KunitorisGame::update and its helpers are written, the minigame is playable in code but not seen. The smoke run never reaches Extra mode (top menu answer 10): the picture test needs the smoke pad taken there.
- 2026-11-07 (game/stagecamera.cpp tail, batch 214): **StageCharacter's slot +0x25c (0x179a90) passes
  an a1 its callers never set.** ROM: m_brain->setEntry(a1), then setState(m_brain->m_state, 1). The
  only callers of +0x25c (0x17964c, 0x199d2c, 0x29aab4) are camera paths that leave a1 alone, so in
  the source the slot likely has no argument and the value is an unset local. The host passes null
  (setState's switchSlot binds the entry again). Not reached: only group 0 objects are stepped. Also
  added: StageMainCamera::sideHook3 -> cameraStep (the ROM thunk 0x1799f0); a 40 s teleport run to
  stage 1 showed the camera behind the girl as before, but whether the pair table steps the main
  camera (so it now steps twice a frame, as in the ROM) was not traced.
- 2026-11-07 (game/gsluminance.cpp): **file name and the PSMT4 shift are guesses.** No strings in
  0x2b6b90..0x2b85a0: the name is the port's. That gsLumShiftPages (0x2b7b00) leaves R in G and B in
  alpha is read from how the next two steps use scratch, not from the PSMT4 swizzle; the host shader
  does the sum directly, so nothing on PC depends on it. No screen shows it: the smoke run stops at
  the language screen and the palette views need the sight-jack.
- 2026-11-07 (rena/rena_2d_texture_vram.cpp): **two small leftovers beside it.** `Tim2Texture::getTex0`
  0x2e1b10 (unassigned@002e1b10, 3 lines: the TEX0 template with TBP0 = block, CBP = block +
  m_imageBlocks) is a hand stub in src/lib/unassigned_stubs.cpp; stubgen wanted it in
  `src/rena_2d_texture_tim2.cpp` (no `rena/`: the inferred file in yamipan_functions.txt lacks the
  directory), so it belongs in rena/rena_2d_texture_tim2.cpp. **Done 2026-11-07:** rena/rena_2d_texture_tim2.cpp
  now starts at 0x2e1b10, getTex0 written there, hand stub deleted.
  RenaPointLight's slots 3 (0x2e0820, returns 1) and 6 (0x2e0830, returns -1.0f) are left unplaced
  beside the other light virtuals (0x2e07d0, 0x2e07f0): game/scenerender.cpp's class, not written
  (RenaLight is host-layout without virtuals). The file name rena_2d_texture_vram.cpp is a guess.
- 2026-11-06 (user, `tools/reverse/iop.py`): **RXSNDDRV, FS2's IOP code, is extracted and decompiles**
  (architecture.md "The IOP side"): `python iop.py elf <MODULES>/RXSNDDRV.IRX` ->
  `work/iop/elf/rxsnddrv.elf`. **Imported 2026-11-06 as program `rxsnddrv.elf`** in `CLaude Decomp`, open
  beside SCES_538.51 (ghidra.md "Hazards" before pointing tools at it). **Settled 2026-11-07** (Decisions):
  RosePan's driver is `src/iop/rxsnddrv`, with FS2's changes as `python iop.py cmp`
  lists them: 152 of its 192 functions stand as they are, 38 changed and 2 moved need reading, and 4
  functions are FS2's own (0x2d9c, 0x3660, 0x4454, 0xd1dc). The dispatcher 0x360 also
  settles what the EE's commands do where "Next up" says the IOP module would tell.

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
  (GetDir mapped result 5, from libmc -6); ~~`Memcard::m_iconAux`~~ (settled 2026-11-03: `m_iconSize`, the item's size, game/menupack.cpp); `MemcardRequest_Save7` (no starter in the file: who sets request 7?);
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

- **game/stageparty.cpp, 2026-11-06, partial (batch 156, 27 names): the sight-jack without its drawing.** All of `StageParty` 0x1fe6b0..0x1fed30 and, in `SightJack`, ctor, dtor, reset, start, stop, update, aimAt, tune, pickBearing, keep, recall, canView, keepNearestFoe, stepAlertedFoe, pollButtons, stepMode, beginVision; `CharacterPad::pressedBind` 0x2067d0, `JackCamera`, `JackFill`. Stage 1 and node 4 no longer show the StageParty stubs. Never started in a run (Next up); the screen is unchanged, the draw writes no packets.
- **game/characterroute.cpp, 2026-11-06, verify (batch 155, 68 names): the 12 actions and their task classes, the TU is whole.** Written: `RouteNumbAction`, `RouteVoiceAction`, `RoutePlayAction`, `RouteQuickTurnAction`, `RouteFollowAction`, `RouteFleeAction` with `RouteFleeLightAction` and `RouteFleeFoeAction`, `RouteBackAction` with `RouteBackFromLightAction` and `RouteBackFromFoeAction`, `RouteSwoopAction`, `RouteStalkAction`, `RouteHuntAction`, `RouteWanderAction`, and `findWayOut` 0x227f30. Renamed for what they do: action22c0f0 `goNumb`, action22c840 `speak`, action22c960 `restIn`, action22cbe0 `quickTurn`, actOnCharacter `followCharacter`, actLeftRight `fleeLight`, actLeft `backFromLight`, actOnFoeRight `fleeFoe`, actOnFoe `backFromFoe`, approach `wanderNear`. Beside them: `PlayerMotion::setHoldIfFree` 0x1ded60, `specialMotion`, `StageCharacter::isVoicePlaying` 0x2a6430, `getWeaponRange` 0x20ce10, `CharacterBlock7c8::find` 0x21a8d0. **Proved:** new tools/reverse/wayoutcmp.py ran the ROM's 0x227f30 on `eerun.py` beside the host's over 12 stages' rute files from the disc, 1,800 questions, 0 differences, every one found a way: `RoutePlayer::areaList` and the +0x34 lists work. **Seen, node 4, 280 s, no crash, with a temporary driver (removed):** every 7 s it gave `F_E04_SRT1` the next command against `F_E04_SRT2`. wanderNear, followCharacter, attack (hunt and stalk), fleeFoe, backFromFoe, playAction "NULL" and restIn took; the Srt walked from (5918, 5060) to (6082, 4417) and back to (5955, 4718) in motions 0, 9, 11. That is the first time `RouteWalk`, `RouteFilter::findPath` and the rute file moved a character. fleeLight and backFromLight answered no, there was no light on it; quickTurn no, a Srt lacks the ability; goNumb no, it is for flyers; speak no, the cue was made up. So those tasks' update code, `RouteSwoopAction`, the bite and the shots in `RouteHuntAction` and `RoutePlayAction` with a special have not run. **On screen:** the foe stays out of view, the shots show the boy on the bridge as before, so no picture shows any of this. Doubts for the user: (1) every class name is the port's, read from the code; `cr_3` is called cowering from where it is used, and "Yri or Kne keep out of the light" from the code that tests them. (2) A stored path ending in a chain: the ROM asks `chainNode(index, count)`, one past the list, and reads whatever follows; the host takes the last node. It never showed in the comparison. (3) `flag3be` is what the hunt's tackle tests, `m_6c8` 1 bites and 2 fires bursts, `m_6d0` turns close fighting off, `m_6d4` lets it move: all named by their readers only, and left under their offsets. (4) `findWayOut` passes the list's entry as both the area and the crossing of `crossingPath`: the ROM does (read in the asm), and it finds ways.
- **game/routeplayer.cpp (2026-11-05), verify (batch 154, 26 names):** the whole rute file: all 45 functions that are not MSL instances (0x252840..0x25b8e0), `RoutePlayer` moved from gamehooks.h to game/routeplayer.h, 19 stubs gone. tools/reverse/eerun.py and rutecmp.py are new: 12 stages' files, 3,840 questions, the ROM and the host answer alike.
- **game/routefilter.cpp (2026-11-05), verify (batch 153, 45 names):** all 36 functions of 0x23f290..0x242e80 written or mapped to the host vector, plus `RoutePath::append` 0x258870 and `planarDistance` 0x258b80, which the rute side shares. `RouteFilter` is the handle, `RouteSearch` the 0x60 record; `RouteNode` moved from gamehooks.h to game/routefilter.h, and `RoutePlayer` got the 16 prototypes the search calls (stubs). `StageScene::getFloorAttr` now answers a signed short as the ROM does (`lh`, -1): the host answered 0xffff, so `floorAttr < 0` never fired. Seen: a scratch harness on the funnel (corridor, L both ways round, S, a gate behind the start, 20,000 random corridors): right corners and every gate crossed. New Game with work/doorwalk_pad.txt, 240 s: no crash, the dark first-person wall as before, the filter is built (pathNames and maxStepCount stubs reached). Nothing on screen can show it until game/routeplayer.cpp. Doubts: every name is the port's; the third field of a span is copied and never read; `stepDistance` on a chain step reads stale registers in the ROM, here 0.

- **game/characterroute.cpp (2026-11-05), partial (batch 152, 83 names; 85 functions reconstructed, the TU tool counts 14 stubs):** extent pinned 0x21efa0..0x22fc50, all one vtable run. Written: ctor, dtor, update, stop, the looks, the turns, the moves, the walk 0x223080 with RouteTurn, RouteTurnPoint, RouteTurnObject, RouteRoll, RouteElevator, RouteDoorStep, four detours, RouteLadderStep, RouteFollowNode; RouteMoveTurn, RouteMoveCar; useDoor, useDoorKey, useElevator, dragOutCar, trap, reload, pickup, throw, crouch both ways, shoot (hit is a jump to it), melee, hitInTime, hitRunning, swingAt, attackTask, action22bf60. Corrected: lookAtYaw's second argument is the pitch; useDoorKey's argument unlocks, it does not lock; StageCar +0x381 is the throttle and +0x383 the brake as game/characterinput already had them. New prototypes for RouteFilter, the elevator and the car wait in unassigned_stubs.cpp.

- **game/gameoversequence.cpp, 2026-11-05, verify (batch 151): all 17 functions, the game over runs to its end.** `GameOverSequence`, `DeathCamera` ("CAM_DEAD", the class at vtable 0x318cf0; the open item's 0x318f4c was inside it), `SceneDirector`, `drawLetterbox`. What the port called the blackout is the letterbox: `StageMission::letterbox` +0xda, native 0x1f6 (architecture.md "The game over"). **Seen, node 4, temporary hook removed, forced on a standing boy:** bars ramp in and out at the forced frames, 4:3 and 21:9 (1195 x 512): both edges reached, the same share of the height. CAM_DEAD: the high shot swinging round him from the front to the back, states 1, 2, 3, 4 at game frames 1000, 1001, 1052, 1153. Fade, then "Mission failed." with Retry / Give Up; the pad's Cross retried and Mission 1 played again. **Not seen:** a real death (a fall motion, a culprit, state 3 cut short by `Corpse`, gear in hand); state 3 shows nothing new, `StageParty::start` and `startGameOverView` 0x1fe7c0 are stubs, so the view drops back to the main camera; no reference picture of the shot exists. `StageParty::isRunning` still answers false. Doubts for the user: `canBeJacked`, `DeathCamera` and the file name are guesses; the camera stepping twice a frame comes from `ViewCamera`'s thunk, not measured on a PS2.

- **The squeeze about the centre, 2026-11-05, game/scenepool.cpp done (the user's decision).** New seam `YamiPan_DrawUiSqueeze`; ScenePool's strip and text, the subtitles of DemoStream and the Use prompt go through it. **Seen, New Game, 240 s, the same temporary panel and forced widths, removed after:** against the strip composed from the asset, 77,160 pixels, at 4:3 (window 960 x 720) mean 0.27, worst 5 of 255, at 16:9 (1280 x 720) mean 0.28, worst 7, none over 12 at either. progress/20261105-strip-squeezed-16x9.png. A short test text measured 50 x 17 pixels, centred, at both (the numbers are in the commit's report). Not seen with the squeeze: a real subtitle and the real prompt, which use the same call; their 4:3 is unchanged by construction. The user resized the window during a run; every shot of that run was still 910 x 512, so the numbers stand. **A second host fix found on the way:** at a render size where a pixel centre falls exactly on a sprite's edge (960 x 720: 156 x 1.875 = 292.5) the UV rounded into the next cell of the sheet, one-pixel ticks at cell seams, absent at 683 wide. UiCanvas pulls a sprite's UVs in by 1/64 texel.

- **game/scenepool.cpp, 2026-11-05, verify: the strip proven, and flat draws fixed.** The code was complete; this run read all of it against the ROM (ctor, draw, drawStrip, the row marks, the table 0x2fb4b0, colour 0x80101010: no difference) and looked at the picture. Test, removed afterwards: a flat panel under the strip and forced widths 150, 330, 0, 90, which use the concave corners and the empty line. The expected frame was composed from the asset and the table: **New Game, 300 s, shot against composed strip, 78,720 pixels: mean difference 0.18, worst 6 of 255 at 4:3 (683 x 512), mean 0.26, worst 5 at 16:9 (910 x 512), no pixel over 12.** progress/20261105-strip-16x9.png has the 16:9 one. No capture of the real game exists, so the reference is the asset and the ROM's table. `verify`, not `done`: at 16:9 the strip is right but stretched, like all the raw 2D ("Next up", item (c) of the Pan layer list). **Host fix found by this test:** ui.frag scaled every colour by 255/128, GS MODULATE, on flat draws too, where the GS has no MODULATE: a backdrop of (0xc0, 0x90, 0x40) showed as (255, 255, 127), and shows (192, 144, 64) now. The fragment uniform's y is the scale: 255/128 on a texture, 1 flat. Alpha stays on the 0x80 scale. So every flat fill is half as bright as before, as the ROM has it: gsFillRect, the dark fill of events, StageFade, ScreenFill. Not looked at after the fix: an event's dark fill or a fade on screen. The first notice did not come up in a 345 s run of work/walklong_pad.txt with this build: the pad's timing has drifted, so that script is no way to reach it now.

- **A batch with no UVs, 2026-11-05 (the user: "as soon as I gain control of the character and turned left it just crashed"; confirmed fixed by the user's play).** `ModelRenderer::drawBatch` read `batch.uvs` unchecked. The batch is on the plain path (`prim.listFlags` 0): one triangle, unskinned, its colours there, uvSet 1, listA 1, listB 2. It now draws with zero UVs plus the scroll. **Not looked at:** that triangle on screen, which model it belongs to, and whether its texture's corner looks right. A scripted turn did not reach it. `listEntry`, which only tries entry 0 as its fallback, was not the cause and stands as it was.

- **The next mission starts, 2026-11-05 (the user: "it crashed at the start of the next mission"): `StageObject::isMotionTarget` 0x175c40 written.** It was a stub answering no, so the shared animals' motion lists never reached their objects: node 4's crow `oCM99_20` had a model and no pose, and native 0xb1 `CROW_s` faulted in `ModelPose::play` with both null. A target's objects are `oCM99_<index><digit>` for CAT, CROW, tonbi (the format at 0x303078) and the exact name for the other five. Checked on node 4, 330 s, no crash, the first picture of the stage right: the boy on a lit pier from a third-person view, trailing `work/shots`; the run started on that node by a temporary hook, removed. **Not looked at:** the crow itself on screen, and CAT and tonbi.

- **The navigator, game/navimenu.cpp, 2026-11-05: seen in the user's own playthrough, no screenshot kept.** After stage 1 it appears, the cursor can be moved, and picking the next node starts that stage. Written: `runNaviFile`, `runNaviLayout`, `applyNaviAnswer`, `currentNaviAnswer`, `saveExtraInit`, `getSaveExtra`, `getNaviMenu`, `PrgScaleNavi` and `PrgScaleNaviScroll` (batch 149, 21 names). **`getNaviMenu`'s NaviMenu is PrgScaleNavi itself:** it is now its +0x2a8..+0x628 with named fields, in EE order, because layouts read it by index. **Not looked at:** the overview on L1, the link and back requests, any aspect ratio but the window's, and how the map compares with the real game's. The variable names in `NaviVar` are read from what the code does, not from the file; the file's own can be printed from `S2dVar::name`.

- **The stage's end, 2026-11-05, on the user's report ("the kid reaches downstairs" and nothing starts): seen in the user's own playthrough, no screenshot kept.** Two blockers. `BoxVolume` had no runner slot +0xfc: native 0x44 on a box asks whether the object is inside (`containsObject` 0x17e950 through the thunk 0x17f130, table 0x3125bc), and the port answered the base's false, so no `AREA_*` script of any stage fired. `SphereVolume`'s slot is the real default (table 0x312a7c), so the override is the box's alone. `ClearSequence` 0x2b1500..0x2b1670 was stubs: now the ROM's two-second fade to black, done at full black, in game/demoevent.cpp. Stage 1's `AREA01` at (6959, 6342, 810), 320 below the start floor, says SPC_DIE and calls native 0x52. **Not looked at:** the same fix wakes `AREA_AC10` (Use prompt, event `sd_MK01_10`) and `AREA11`, and every other stage's boxes. `GameOverSequence` 0x2b1670..0x2b1cb0 is still stubs, so a death by that road never ends the stage.

- **game/scriptrunner.cpp, sixth native run (2026-11-05), partial (batches 146..148): the last 89 natives, every one the disc calls.** Bound: 0x39, 0x3b, 0x43, 0x55, 0x56, 0x58, 0x62, 0x81, 0xae, 0xc2, 0xd2, 0xd3, 0xed, 0x12b, 0x130, 0x131, 0x135, 0x140, 0x142, 0x14d, 0x14e, 0x154, 0x15a, 0x15c, 0x170, 0x177, 0x17a, 0x17c, 0x17d, 0x18d..0x18f, 0x191, 0x193..0x195, 0x198, 0x19a, 0x19e, 0x1ac, 0x1c8, 0x1ca..0x1d0, 0x1d3, 0x1d5, 0x1d7..0x1d9, 0x1dc, 0x1e6, 0x1eb..0x1ee, 0x1f0, 0x1f3, 0x1f5, 0x1fc..0x1ff, 0x201, 0x202, 0x204, 0x205, 0x20d, 0x211, 0x218, 0x21b..0x220, 0x222..0x226, 0x228, 0x22a..0x22c, 0x230. Written behind them: runner slots +0x9c `getScriptCar`, +0xc8, +0xd0, +0xd8, +0x15c, +0x174, +0x1a0, +0x1b0, +0x1b4, the proxy's +0x9c, +0xc8 and +0x15c; `StageCharacter::isOnElevator`, `clearSlowChange`, `setSlowChange`, `startDrainSlow`, `playVoiceHeard` 0x2a65c0, `setRespawnPlace`; `PlayerMotion::isSpecialOnly`, `setHold`; `findEffectGroup` 0x1b3440, `setEffectsOn` 0x1b3240. **Stubs on purpose, the class does not exist on the host:** `StageCharacter::rideCar`, `StageCar::resetPlace`, the elevator's and the number lock's functions, `StageParty`'s four vision setters and `isCompanionMode`, `SceneOwnedObject::setNames`, `setSevenKeyNumbers`, `isSlot2c`, `isSlot44`, `getRecordFlag31`, `PlayerMotion::getCarSpecialFlag`, `setCarSpecialFlag` (GettingOnCar is unwritten, playermotion.cpp's PC note). Native 0x218, the most called of the batch (252 calls), is only the lights' broken flag. **Seen, New Game, work/doorwalk_pad.txt, two runs of 290 s (before the last 16 and on the final code), no crash:** no unwritten native reached and none of this batch's stubs: nothing shows this code ran, and stage 1's first rooms have no car, elevator, lock or clock. Picture unchanged, the dark first-person wall, with `Open` and the stick hint in the first run's last shot and without them in the second's. **All of it is unverified.** The elevator's names were swapped once and corrected from native 0x1ce (batch 147). A second Claude session worked in this checkout during the run (commits 8f70652, c23d7b3): hazards.md.
- **game/scriptrunner.cpp, fifth native run (2026-11-05), partial (batches 144, 145): 52 natives and the blow.** Bound: 0x34, 0x5c, 0x5e, 0x61, 0x68, 0x6d, 0x6e, 0xa6, 0xaa, 0xab, 0xb2, 0xb6, 0xbe, 0xd0, 0xf9, 0x127, 0x15e, 0x167, 0x168, 0x169, 0x16b, 0x16c, 0x16d, 0x16f, 0x172, 0x173, 0x175, 0x176, 0x178, 0x179, 0x185, 0x18b, 0x1a9, 0x1b5, 0x1c0, 0x1c4, 0x1d1, 0x1d2, 0x1e7, 0x1e8, 0x1e9, 0x1fb, 0x200, 0x208, 0x209, 0x20a, 0x20e, 0x212, 0x21a, 0x227, 0x234, 0x235. New behind them: StageCharacter::takeDamage 0x20eb40 (was a stub), getHandClass 0x210540 (was a stub), scriptHurt 0x217d60, setMotionByLabel 0x217c80, scriptIsCompanion, scriptUseSpecial, scriptDragOutCar, scriptCrouch; missionObjectInSight 0x29f200 (the TU's range grew to 0x29f2a0); applySpawnPointList 0x1d0b50; runner slots +0x118, +0x1a4, +0x1a8, +0x1ac. CharacterBrain::crouch1 / dragOutCar / useSpecial are the same slots under script names, and crouch passes its loudness on: the old comment on f12 had it right. playActionAndRetry answers now (native 0x61 keeps it). Native 0x6b takes bn_top for kinds 1..6. New Game 290 s with work/doorwalk_pad.txt on the final code: no crash, no unwritten native, picture unchanged (the dark first-person wall). None of this ran: no temporary log was added, and stage 1's first rooms hit nobody.

**yamipan/rendering/model_renderer.cpp, fourth run (2026-11-05), verify: the shadow pass, not yet seen on screen.** Read from VU1: architecture.md "A shadow in VU1". Written: `shadowVertex`, `outsideShadowBox`, the pipeline `shadowPipeline` (the spot's shaders, its own blend, clamped sampler; `createProjectedPipeline` shared with the spot), `ModelDraw` shadow vertices drawn between a draw and its spot; on the game side `renaSetShadowTexture`, `renaSetShadowMatrices`, `renaSetShadowCount`, `YamiPanModelPass::shadows`, `ModelFlag_TakesShadow`, `ModelMaterialFlag_NoShadow`. **Measured with temporary probes (removed), New Game, doorwalk and newgame pads, 200..240 s, no crash:** in stage 1's room 22 objects, 1 flagged, 21 in the main group, so no caster: the flagged one is the first-person player. In d0101 one caster: count 1, 78..510 shadow vertices a pass, alpha up to 118 of 0x80, up to 274 vertices inside the picture and its fade. So the numbers look right and the pass runs. **No screenshot shows it:** a test with a red shadow and a white picture left no red in any event or stage shot, and the event shots looked at aim at the ceiling. That does not prove the pass reaches the screen. Final code, `tus.py check --run 15`: build, callcheck and layoutcheck clean; its run is the boot and title, no shadow in it.
**Doubts for the user:** (1) fog: VU 180..184 have no writer found, the host draws the shadow black whatever the fog. (2) The frame's alpha: the host writes the shadow's alpha as the GS does, so a spot over a shadowed batch follows it and can vanish outside the shadow's picture; first thing to look at if a lamp and a shadow meet. (3) `renaBeginFrame` clears the texture on the host only, so the boss stage's stale shadow_00 is not used. (4) The box test uses the batch's vertices, the ROM its 8 bounds corners: a few more batches are skipped.

**yamipan/rendering/model_renderer.cpp, third run (2026-11-05), verify: the scene's point light, the spot's pass, back-face culling.** Read from VU1 with `vudis.py` fixed (it printed LOI floats as `bal`): architecture.md "The scene's lamp in VU1". Written: `addPointLight` (0x545 / 0x59a, ahead of the multiply), `spotVertex` and a second pipeline with shaders model_spot.vert / .frag (0x2eb, ALPHA_1 0x58, projective ST, alpha test, FOGCOL 0, alpha not written), `facesEye` (0x128 / 0x14f, `ModelFlag_TagC`), and on the game side `fillPointLight` 0x2e05f0, `fillSpotLight` 0x2e00f0, `fillEye`, `renaSetSpotRecord` (0x2d5710 in drawSysRender), flags `ModelFlag_TakesSpot` 1, `ModelFlag_TakesPoint` 0x80, `ModelMaterialFlag_PointFlat` 0x10. A batch's vertices are now worked out once each: strips did it three times.
**Seen, New Game, work/doorwalk_pad.txt, four runs of 285..290 s, no crash:** (1) no forcing: both lights off in every sample (5 of 5 in the main pass), picture as before. **So nothing real has lit this code.** (2) A temporary test (removed) put a point light of weight 4, radius 1200 at the camera: the dark room of stage 1 and the room of d0101 come up with facing and fall-off that look right. (3) The same with the spot only, the stage's own `spotlight` picture along the camera's axes: 29,706 spot vertices a frame, the picture hardly changes, since the pass adds the vertex's own lit colour and these are nearly black; a faint haze in d0101. **The disc itself has not been seen on screen.** No reference exists. Back-face culling ran on whatever has `_c` in its name, with nothing to compare.
**Withdrawn:** the "alpha fringe": 0x2e7c30 / 0x2fdb60 is the shadow pass's GS state (TEX0 gp-0x86b8, CLAMP 5), so TEST_1 drops those pixels in the ROM's model pass too.
**Doubts for the user:** (1) `RenaSpotLight::intensity` is misnamed, it widens x; kept for its callers. (2) The cone test (0x66b) is left out as a saving, but with CLAMP_1 repeat it may also hide repeats: first thing to check when a lamp shows. (3) VU 200.x is written as 2 and the kernel tests non-zero; the host takes a bool. (4) `kMapModelFlag10`, bit 0x10 of a map model: no reader found. (5) Left in the same VU1 flow, unread: material flag 4 (0x4df), 0x484 and 0x393 after the main kick, shadow state 0x98 / 0x9e.


- **game/stageplayer.cpp (2026-11-05), verify (batch 138): the character around the scripts.** Written: `beforeScripts` 0x21c7c0, `afterScripts` 0x21c310 (both moved from StageObject to StageCharacter), `stepStatus` 0x21c3d0, `ageSightTable` 0x217a80, `updateVisibility` 0x20d8c0 (the roadmap's "move"), `canSee` 0x210940, `isPointInView` 0x210b30, `isAlly`, `getAltSoundFlag`, `getActionState`, `isMode1`, `isMode2`, `isScenarioLead`, `isYriOrKne`, `getCharacterClass`, all of `SceneNoiseList` (renamed from SceneUiState, `gStageScene->noises`), `BoxVolume`, `SphereVolume`, `StageVolume::addToScene`, `StageMarker::addToScene`. architecture.md has what each does.

**Seen, New Game, 285 s, work/doorwalk_pad.txt, no crash, three runs, temporary logs removed:** F_PC01 stays in the scene with brain state 1 through the new `afterScripts`; visibility 0.525 walking and 0.500 standing at dark 0.5, the stage's default; the three box volumes enter group 7, and each `contains` answers true just above its own origin and false a million units away; a noise reaches the list (at most 1 at a time: `updateSounds` is a stub), no hearing bit was ever set (nobody to hear). The picture is the dark first-person view at the wall, as before; nothing here draws. **Not proven:** the hard cases of `updateVisibility` (no `VISIBILITY_` volume on stage 1), `canSee` and `isPointInView` on a real target, thrown items, the muffling rays: nothing calls them with anything to find until the route runs. Left of the control task: natives 0x13c, 0x13d (`StageParty`, the class 0x203d70 behind it), 0x5b, 0x20f (spawn an effect: `StageNodeTree`, stub). `isHoldingAction` stands in for the second half of `getAltSoundFlag`, the same test. `StageParty::isRunning` is still a stub answering false. Doubts for the user: the file name and its extent; `SceneNoise_Sound` / `_Any` / `_Ally` / `_Foe` are read from who hears them; +0x660 / +0x664 have no writer found but the frame's reset, and `m_foeCache` the same.

- **game/characterbrain.cpp (2026-11-05), verify (batch 137, 189 names): the whole brain, not only the part the queue item asked for.** Written: `ScriptProxy` 0x1a68b0 (the 36 forwarders whose slot has a host name; the other 28 are listed in the header), `BrainSubRunner`, `BrainFrame`, `BrainWait`, `CharacterBrain` 0x1d85b0..0x1dec30 with `runCommand` 0x1d8630 (59 commands, `BrainCommand`), `setState` for real, the ask* family, `sideHook1` now runs `updateRoute`. `CharacterRoute` is declared and stubbed (`game/characterroute.h`, its `filter` stays null, so `RouteFilter::allow` runs on null as before). MSL's sort 0x1de2f0..0x1deb80 is `std::sort` (equal distances may come out in another order). The player's `canTakeOver` and `getBrainPace` now read the real class through `getState`. Host changes outside the TU: `ScriptRunner::setScriptParent` and `friend class CharacterBrain`; `StageCharacter::m_68c`; Mission `_b0` is `lastFoeAlert`, a float whose ctor bits are 0x800000 (demoevent.cpp); `PlayerMotion::armGadgetSpecial` (the ROM inlines it in 0x1da430, the class is playermotion.cpp's).
  **Seen:** New Game with work/newgame_pad.txt, 270 s, stopped still running, no crash: F_PC01's first-person view of the room and its door, as in the previous runs. The stubs reached say the real ctor, `reset` and `setState` ran (`CharacterRoute::CharacterRoute`, `stop`, `stopLook`); `CharacterRoute::update` was not reached, so `updateRoute` stopped at the enabled flag or the scene's route, and nothing was seen of a brain script: none exists yet. No enemy, no walking NPC, no alarm appeared.
  **Doubts for the user:** every name here is read from the code; the state names, "overlay" and "retry" are the port's. Runner slots no native names yet are plain members (`walkToPoint700`, `fireAt`, `useSmoke`..), to become virtuals when their native comes. The `addNoise` calls of `crouch1` / `crouch3` leave f12 as the caller had it in the ROM; the port passes 0. Commands 0x2d and 0x3c test the opposite way from their neighbours: kept as the ROM has it, it may be a bug in the game.

- **game/stagecharacter.cpp, second run (2026-11-05), verify: re-judged, and its last bridge is gone.** The stretched limbs of the first run do not return: with a temporary third-person view (ForceView ignored, removed) F_PC01 stands whole from behind with the main camera linked, `isViewLinked` 1, no spike (`progress/20261105-f_pc01-third-person.png`); no reference exists for that view. Written: the terminate hook `StageCharacter::sideHook3` 0x179d30 whole, `PlayerMotion::endFrame` 0x1e06f0 whole, `releaseAction` 0x1dedb0 (moved from `StageCharacter` to the motion: its a0 is the motion), `isHoldingAction` 0x214f20; batch 136 (30 names); ranges added to both TUs; `m_b24` / `m_b28` / `m_b2c` are `m_stanceTime` / `m_hitTime` / `m_soundEvent` (the last was a float, it is an index); architecture.md "The terminate hook".

**Seen, New Game, work/doorwalk_pad.txt, 285 s runs, no crash:** a temporary probe (removed) read him walking up the room to z 1130.8 at (7187, 5880) on floor surface 13, motion 3, `m_wallCheckNow` 1, then pushing the door's wall by a few units; `updateSounds` is reached each frame. First person, final code: the view from his eyes, as before. **Not seen working:** `m_runTime` and the trip (stage 1 refuses Run, so it read 0 throughout); smokes, splash, ripples and the lamp (stage 1 has none, all null); the timed motion rate (its setter 0x20d260 is unwritten, so `m_slowStart` stays -1); the carried items' lights (he carries nothing there).

**Left, for later runs:** `takeHits` past its first line (needs `0x20eb40`, `0x26a010`, `0x2103d0`, `0x20e6e0` and something that can hit); `updateSounds` 0x2a4a40 (0x193c bytes) and the voice family 0x2a2070..0x2a4680, all stubs with their names in `gamehooks.h`, one TU of their own: nothing plays yet for a motion's S0..S3. They were left to keep this run to one TU, and they are not in the queue.

**Doubts for the user:** (1) `kRayWater` for mask 0x10000000 and `kSurfaceWater` for 0x1f come from what the code does with them (L_SPLASH, F_HAMON), not from a table. (2) `hasAlteredVoice` 0x215af0 is a name for a test whose meaning is unread. (3) The voice names are the sheet's event names read as voices; only SHOUT (`PC_SHOUT`, `NPC_SHOUT`) was read to its strings. (4) The ROM stores a byte into the low end of the float `m_slowTo` (+0x464); the host keeps `m_slowInDrain` beside it, which only differs if the setter writes the byte to pass a value, and it is unwritten. (5) `PlayerMotion::takeHits` keeps its PC: note; a character's `m_wallCheckNow` depends on that one line.

- **game/playermotion.cpp, fourteenth run (2026-11-05), verify: the TU is closed, and he walks through a door.** Written: the whole of `resetActionState` 0x21e050 (`ScriptPoint`, EE 0x20, with `set` 0x20b220 as a member; the seven points +0x6e0..+0x7a0; `SightEntry` and the table +0x468 of 0x18 lines; `m_sightOwner` +0x588, `m_7a0`, `m_7c0`, `m_7c4`). `m_vector730` is now `m_scriptPoint.point`. The stale callcheck notes in `DoorLink::onRun`, `canUnlockDoor` and `giveItem` now name what the ROM calls. **One call is still not written, and the note says so:** the brain's half of `sideHook1`, 0x1dd2f0 -> 0x22e740, game/characterbrain.cpp.

**Seen, New Game, a temporary test (removed), F_PC01 put at oMK99_ds_05's use place, then only the pad (LSUP held, CROSS tapped):** the first door crossed in play. Motion 73, door mode 0 -> 1 -> 2, motion 0, then motion 3 (walk), and x goes 7177 -> 7035 past the old room's edge at x 7134, stopping at the next room's wall at (7035, 5862). The picture during the opening shows the door slid aside with the next room behind it. **That proves the door's faces go off when it is open**, the sixth run's "not proven". No reference exists. After the crossing the view is nearly black (a dark surface right at the eye, or an unlit room): not told apart, and there is no capture to compare with.

**"He gets stuck" and "wanders by 10 units" (thirteenth run), measured frame by frame:** pushing a wall or a shut door he swings 10 to 12 units along his walking direction with a period of 27 frames, one step of the walk. Rotation stays constant. Forward he comes by the motion, back he goes by a wall push. Reading, not proven: the leading foot's bone sphere reaches the wall and pushes the whole body back (`CharacterGround::keepInside` / the wall body's bone spheres). In free walking the same code stays within 2 units of a line, so this is not the walk itself. In first person it shows as the view pumping back and forth at a wall. It may be the ROM's own behaviour; the code was read and no fault found, and nothing here compares it with the real game. If the user finds it wrong, look at whether the ROM stops the walk at a wall (`wallCount` / `wallAngles` readers).

**For tests:** the opening is much shorter with CROSS tapped through it: the stage's first frame came at about 235 s with CROSS every 230 to 400 reads from read 5200, and about 276 s with none. `build.py` prints only the log's tail; the full output is in work/run_relwithdebinfo.log. The pad used: the newgame prefix, then `LSUP*950` at every 1000 reads from 5000, `CROSS*8` every 230 reads from 5200.

- **game/playermotion.cpp, thirteenth run (2026-11-05), partial (batch 135): the natives stage 1 asks, and the notices.** Written: natives 0x3a (answers 0: the sight table +0x468 is the brain's), 0x3e, 0x3f, 0x46, 0x49, 0x6a, 0x6b, 0x7d, 0x94, 0xac, 0xb1, 0xb5, 0xcf, 0x116, 0x124, 0x125, 0x129, 0x133, 0x1aa, 0x1c7, 0x1fa; `StageCharacter::hasBiter` (was hasFollower, a wrong guess), `findItem`, `isRunningMotion`, `setScriptPoint`, `setLightResist`; all of `game/pausemenustate.cpp` and `game/scenepool.cpp`, new TUs (architecture.md "The stage's notices"). The slice's folded functions (0x210e50, 0x21b1c0, 0x21cb00.., 0x21cc50) carry their addresses now, so the TU counts 244 of 244 written. **Left in natives on stage 1: 0x13c, 0x13d (the sight-jack's range and angle on StageParty's record), 0x5b and 0x20f (particles: `SceneBoxList` over `StageNodeTree::spawn`, the class 0x2b3ef0).** 0x6b plays at the object's own place: `bn_top` and the map's points (0x190650) are left out, and the sound is silent anyway. Older callcheck gaps in this file (DoorLink::onRun, canUnlockDoor, sideHook1, giveItem, resetActionState) are not from this run.

**Seen, New Game, work/walklong_pad.txt, 340 s, no crash:** the script's first notice comes up at the foot of the screen in the game's own font, "Left analog stick: Move", 4 s, with the prompt "Open" at the top (shot 654000). No reference exists for this frame: the wording and place come from the ROM's text and origin, not from a capture. The dark strip behind the text could not be told apart on this dark room (proven later over a temporary backdrop: Done, game/scenepool.cpp). Any aspect: the text goes through the font printer and the UI module, seen at 4:3 only.

**"He gets stuck on something": not a bug that this run could find.** A trace of the player every 15 frames: he walks freely over x 7134..7397, y 5881..6070 at z 1131, a room of about 260 x 190 units, and stops only at its edges, last in a corner at (7137, 6064). There he keeps walking in place, and the position wanders by about 10 units in a cycle of 9.6 s, the same numbers each time (t 118.6 and 128.2 both 7134.4, 6069.3). The room is closed until a door is opened, and the pad pressed nothing there. Reading: the user was in the first room with its door shut. Unproven: that the ROM wanders the same way at a wall. If the user sees it elsewhere, trace `sweepWalls` there.

- **game/playermotion.cpp, twelfth run (2026-11-05), partial (batch 134): a character's properties, and the status block.** Written: `StageCharacter::setProperty` 0x212530 and `getProperty` 0x2132d0 (all 0x74 ids, the getter from the asm), `StageObject::getProperty` 0x18a5b0 (SubSceneModelObject's copy now calls it; StageDoor reaches it through the map object), **`resetActionState` 0x21e050 and `initStatusBlock` 0x21cc70, which had been a stub**, `setRightArm` 0x211300, `getWieldSkills`, `canFireLongWeapon`, `getWeaponClass`, the rounds and carried-ammo pairs, `takeNamedItems` 0x21bb90, `getScriptValue` / `setScriptValue`, `isLampOn` / `setLampOn`, `setSides`, `setSideBit`, `setShot`, `StageViewTarget::isNear`, `ScriptRunner::cmdIsLit`. `CharacterBrain::setState` 0x1dbca0 is written **only as far as storing the state**; the rest belongs to game/characterbrain.cpp, queued. The PC default of the rates in the motion's ctor is gone: they come from the sheet. `m_sides` moved from `initStatus` to the block, where the ROM has it.
**Two doubts of earlier runs are answered.** (1) The eleventh run's `getLifeState` answering 0: F_PC01 **has** a brain, and its state stayed 0 because `resetActionState` and `setState` were stubs. Now start 1 gives state 1 and life state 2 at both samples of a New Game run, so the clocks of `sideHook0` run (flinch, MSP, endurance). (2) The fourth run's doubt about the pace: +0x5e0 / +0x5e4 are the sheet's, through the block; no default was ever right, and F_PC01's turn rate is 1.50, not the 1.00 the old guard left for a sheet without one (architecture.md "The status block").
**Seen, New Game, 300 s, work/newgame_pad.txt, with a temporary probe since removed, no crash, 4,860 frames:** the block prints F_PC01's values (architecture.md), `life 2 active=1 motion=0`. F_PC01's script reads 0x29, 0x04, 0x4a..0x4c and stores 0x36 and 0x61; Root reads 0x3b, 0x3c. **The stage's script goes on: natives 0x3a, 0x49, 0xcf, 0x3e, 0x3f and `PauseMenuState::hasNotice` / `addNotice` are in that run's stub list**, ahead of what a run had reached before. last.png: the same room corner and sliding door from eye height as the viewcamera run, no body.
**Regression, the final code, New Game, work/walk_pad_long.txt, 300 s, no crash, 6,120 frames:** same picture, same stub list; that pad only pushes the stick at frame 12000, so it proved nothing about walking. **work/walkshort_pad.txt (stick from frame 5000), tus.py check --run 330, no crash, 7,320 frames:** the last shot is another corner of the room with a low box in front, and the picture does not change from frame 4920 on. **The user, watching live, 2026-11-05: "it is moving its just you get stuck on something"**: the character walks with the sheet's rates and stops against something in the room. What it stops on was not found (a temporary probe with the stick held: 207 units in 200 steps, motion 3, moveKind 7, turning nothing, then no more samples in that run).
**Not proven:** most of the 0x74 ids (only the seven above were asked); `takeNamedItems`, `setCarriedAmmo` and the revive ids (nothing reaches them on stage 1); `setRightArm`'s `m_armLost` has no reader yet.
**Changes a player can feel:** the sheet's 射撃 100 gives F_PC01 a scatter of 1.00 where the host had 0, so the tenth run's shots spread now and a bullet no longer steers after the aim, as the ROM does (architecture.md "Firearms"). 素手攻撃 0 means F_PC01's forcing takes nothing off a door; that is the sheet's number as read and it is worth a look when a door is forced.
**Doubts for the user:** `CharacterWeaponClass` numbers stay plain (1, 2, 4, 3, 5, 6, 7 of item kinds 0..6, default 1: what they mean is unread); `m_3d8`, `m_5b4`, `m_5b8`, `m_5ba`, `m_62c`, `m_670`, `m_6c8..m_6d4`, `m_44c` keep their offsets as names, only their writers and readers are known; `m_drainRate` is the sheet's 操り耐性 by position, odd for a drain, and kept. The kind 6 object's id 0x6b (0x2b23b0) is left out: no such class yet.
**Left in this TU:** as the eleventh run lists it (the car specials, the unwritten links, the brain in `sideHook1`, native 0x7a and the StageMenu window, the gadget and car natives), less the properties. **The next thing the script needs:** the natives above, then `CharacterBrain::setState`'s rest with game/characterbrain.cpp.

- **game/viewcamera.cpp (2026-11-05), done (batch 133): the first-person view.** Written: `ViewCamera` ctor 0x29aae0, `onViewTop` 0x29a9c0, `cameraStep` 0x29a8c0, the thunk 0x29ac90 (`sideHook3`), the dtor 0x29a730 and its thunk 0x29aca0 (`= default`); `PlayerMotion::getEyeMatrix` 0x1e5f70; `StageObject::setProperty` 0x189f40 in game/stageobject.cpp, outside this TU's ranges because the view needs it. `ViewCamera` moved from gamehooks.h to game/viewcamera.h. The three questions of the queue entry:
  1. **The slots.** The object's table runs on through the side and runner tables, so offsets from the vptr at +0 reach past 0x3185b0's seven slots: +0x24c is `onViewTop`, +0x25c `cameraStep`, +0x260 the empty 0x178020. architecture.md "The view camera".
  2. **What forces the view: stage 1's own script, and that was the second bug.** `Root` at +0xc54 stores 1 in property 0x56 sub 7, which is byte 10, `Control_ForceView`. The store went to `ScriptRunner::setProperty`, a stub, because 0x189f40 was unwritten, so nothing ever set the flag. The same stores turn off Alt, Ready, the two menu bytes, byte 3 and Run: **that is the ROM, so the pause menu, the map and running being refused on stage 1 is expected** until Root clears them. Root also reaches `setResultValue` (0x3c) and the lights' 0x4f now.
  3. **What hides the body: `prepareDrawFlags`, read the wrong way round.** `drawMode` 2 is hidden, with all under it (`drawTree` 0x2e41a0 skips the subtree; `PartSwitch::show` gives the shown part 0 and the rest 2). So a view camera hides the followed character's **head** (the whole body with the gear of `isMode1`), and `kPartOff` / `kPartShow` are now `kPartDrawn` / `kPartHidden`. The rest of the body is not hidden: it is below the eye. Looking straight down may show it; that was not tried.

**Seen, New Game with work/newgame_pad.txt, 285 s, no crash:** shots 7200 and 8400 are a dim room corner and a sliding door from eye height, no body, where every earlier run showed F_PC01 whole from behind. **A temporary probe, since removed, 400 s:** `top=camview` on every line, `head 2`, the eye about 98 units above the feet (F_PC01 is a child, body height 110), the eye staying within 5 units of the character as the character moved about 200 units. Shot 286500 is the curtained window side of the room, no body. `setScope(true)` called by hand at step 3000: `top=camview_snipe`, shot 286650 is the same wall nearer (zoomed), **not black**, and `camview` came back when it was switched off. No sight over it. **Not proven:** that the 200 units came from the scripted pad and not from a script (the pad script and the frames did not line up in that run: hazards.md); turning the view with the right stick; the head part going back on in the third-person view.

**Open, closed by the twelfth playermotion run:** the properties are written. The doubt: the name of the file and `sideHook3` as the step's name are the port's.

- **game/playermotion.cpp, eleventh run (2026-11-05), partial (batch 132): the special actions, the bite, the status setters.** Written: `MotionSpecial` and its classes `MotionSpecialAction`, `MotionPartnerCall`, `MotionGadgetSpecial`, `PlayerMotion::initList` 0x1df0b0, `initSlots14` / `initSlots18` for real, `isSpecialAllowed` / `setSpecial` on motion and character, `MotionLink::addSpecial`, Walk's list, Always's cr_2; `BiteCondition`, `startBite`, `BiteLink`, the plain link Bitten; `setFlinch`, `setEndurance` (the game over is now reachable), `setBrainState`, `getAttackReach`; natives 0x19f..0x1a2, 0x1a7. `startGameOver` and `GameOverSequence` take two objects, not ints (the ctor keeps them). The slot +0x1c question of the tenth run is answered: 0x1def90 and the brain's 0x1da430 (architecture.md "The special actions"). New stubs: `takeDamage` 0x20eb40, `spawnBlood` 0x26a010, `CharacterBrain::setState` / `revive`, `stopBodySounds`, `isRecordBusy`, `SceneOwnedObject::canUse` / `start`, `elevatorCanCall` / `elevatorCall`.
**Seen, New Game, temporary tests since removed, no crash:** F_PC01's sheet (kind 0) builds 19 specials, as the reading counts: 3 calls, 13 sp_, NumLockOpen, NumLockClose, sp_uryen. Endurance 100/100 after `initStatus`, which was 0 while the setter was a stub. `setFlinch(999)` gives 0: this sheet's limit is 0, and `m_flinched` sets each call, as the ROM does. `setEndurance(0)` at frame 1500: endurance 0, `gameOver` set, no crash. `setSpecial` each frame from frame 2400: sp_lock plays motion 197, sp_hole 196, each about 50 frames and back to 0, with Walk's motions between. sp_rake asks 207, which has no link in the host: **the character stays in 207 for good** (frames 3502..4000, activeLink null): any special whose motion's link is not written (SpWait?) hangs like that, so write those links before a script can ask for one.
**Regression, the final code with no test, New Game, 330 s, work/battle_pad.txt: no crash; last.png shows the room and door with the boy whole from behind, which is the known wrong third-person view (top of Next up).** That run reached 10,020 frames, so the same pad script covered less than in the longest test.
**Not proven:** the bite itself (no second character on stage 1 and nothing writes +0x624 or wants.bite; not one line of BiteCondition or BiteLink was seen to run), cr_1..3 (no partner there), the gadgets, natives 0x19f..0x1a7 (no script reached them).
**Answered by the twelfth run (brain state 0, because `resetActionState` was a stub): `getLifeState` answered 0 at every sample of this run, in Walk, at frames 400..4000,** not the 2 the tenth run's note says. Its first tests are m_active (+0x34, the ROM's) and the brain's state 0: the character may have a `CharacterBrain` that stays at state 0 because `update` is not written, or m_active may be clear. Not measured which. While it answers 0 the clocks of `sideHook0` do not run: flinch, MSP and endurance do not recover. Check this first if a clock seems dead.
**Left in this TU:** the car specials 0x280f50, 0x280e20, 0x280a60 (need the car); the links in architecture.md's list the host lacks; the brain in `sideHook1`; native 0x7a and the `StageMenu` window; the gadget natives 0x1aaed0, 0x1ac800, 0x1acb00, 0x1acd00, 0x1aeb40, 0x1aee20, 0x1c46d0 and the car natives 0x1b6420.. . `tus.py`'s reconstructed count reads .cpp files only: functions written inline in a header carry a comment line in the .cpp.

- **game/playermotion.cpp, tenth run (2026-11-05), partial (batch 129): firearms, traps, throws, the start hook.** Written: the conditions GunOn, GunMove, Snipe, GunAttack, Reload, Showt, Jack, Throw, TrapCancel, TrapSet (only Bite is left); the links Gun, GunAttack, Reload, TrapSet, TrapCancel, Throw; `PlayerMotion::setScope` 0x1dfd90 (was the stub `clearNoFlash`; `noFlash` is `scopeOn`), `ViewCamera::setZoom` 0x29a840, new `StageCamera::setProjection` 0x177be0, `ShotSpread` and `scatter` 0x269db0, `MotionProbe::findTrap`, `StageCharacter::findCarried`, the five reload and trap sounds, all of `sideHook0` 0x17a860, `getLifeState` 0x212460 (was a stub answering 0, so a character now answers 2), `hasKyoModel`. The slice's getters carry their addresses: 0x26f720, 0x26f730, 0x26f780, 0x26f8d0, the empty link slots 0x1eaba0, 0x1eabb0, 0x1ec3f0, the two ctors 0x27b640, 0x27c140. Renamed fields: `CharacterAbilities::jack`, `gunMove`, `traps`, `throwing`, `call`, `reload` (were flagNNN); `m_aimLock` is `m_shotScatter`; `m_3e4..m_3ec` are the three clocks. **A bug of the earlier reading, fixed:** `StageScene::draw` called `player->m_motion->overlay->draw()` on a pointer nobody set; the ROM is `menuProgs->draw(scopeZoom)`, a float at +0xbd8 (asm 0x198c30; architecture.md "Firearms"). `PlayerOverlay` is gone; `MenuProgSet::draw` stays a stub, so the sight is not drawn.
**Seen, New Game, 400 s, a temporary test since removed (a bare `StageWeaponItem` R_89 with no model in `m_weapons[0]`, 2 rounds, 30 B_556 carried, work/gun_pad.txt: R1, Cross, up and down of the d-pad), no crash, 87 motion changes logged:** rf_aon (122), rf_aim (129), two rf_fire (130) with the ammo going 2, 1, 0, scope on at 0.30 and off again from the d-pad, rf_aoff (136) when R1 goes, and with an empty gun Cross no longer fires. The idle reload asks rf_rl_a (134) after two seconds of motion 0, again and again. **The picture:** with the rifle up the camera looks up the room's corner over the top of his head (the hair at the foot of the frame, progress/20261105-rifle-aim.png); no reference exists for it. **Through the scope the frame is pure black** (shot_244680): `ViewCamera` has only its ctor, `getKind` and `setZoom`, so camview_snipe never takes the eye. That is the next blocker for the scope, in unassigned@00299d90 (slots from vtable 0x3185b0), and it stops `camview` and `camview_zoom` the same way. **`scatter` measured:** 20,000 draws at range 1000, amount 1: largest stray 99.50 unsettled, 49.94 settled, length 1000.00, as the reading says (architecture.md "Firearms"). **Not proven:** `ReloadLink::onRun`: rounds stayed 30 and ammo 0, because F_PC01 has no reload motion, so no Click comes and 134 ends at once; a shot's bullet, flash and hit (the fake weapon has no model and stands at his feet); the butt; TrapSet, TrapCancel, Throw, Showt and Jack, which nothing asks for: `wants.trapSet`, `trapCancel`, `throwItem`, `call`, `reload` have no writer read yet (0x218850, the brain); the P_URIEN half of `sideHook0` (no kyo model on stage 1). **Regression, the final code with no test weapon, New Game, 340 s, work/battle_pad.txt: no crash; the last good shot (shot_267240) shows the same upward view of the room's corner with the top of his head at the foot of the frame.** So that view is not the gun's: it comes in the plain stance too, and the question left by the ninth run (is the camera in the stance right?) stands. Motions were keyed to shots only in the gun run, where the aim's shot looks the same. No reference exists. One run's last.png was empty, cut by the stop in mid-write.
**Doubts for the user:** (1) `GunMoveCondition` asks slot 14 at a wall and goes on, as the ROM does: nothing returns there, so a later request of the same frame cannot replace it (`request` keeps the first). (2) `CharacterFire_Single` (3) is the only name here with no evidence beyond its handling: the input never sets it. (3) `ViewCamera::setZoom` sits in playermotion.cpp, and `kZoomLow` / `kZoomSpan` are one pair where the ROM has three copies; they are computed from the same 30. (4) `setScope` does not null-check `camview_snipe`, as the ROM does not; a sub-scene has no such view, and only a rifle's Snipe reaches it.
**Left in this TU:** Bite (conditions 0x26fb60 / 0x26fd30 with 0x26fbc0, 0x21abb0, 0x215290, 0x20ce80, the link 0x278db0, vtable 0x317860), the brain in `sideHook1`, `setFlinch` 0x211150 and `setEndurance` 0x211430 (stubs; `setEndurance` holds the game over), native 0x7a and the `StageMenu` window. **The list 0x1df0b0** (`PlayerMotion::initList`, called from the ctor at 0x1edb88): it builds the named special actions into map +0x2c, and Walk adds the names of data 0x2fb3a0 to its list. Classes: 0x2814a0 vtable 0x317d30 (ctor name, id or -1 for the row of table 0x577b20, 0x100 x 0x210, whose +0xa0 name equals it: 0x2813e0; onUpdate 0x281540, onApply 0x281460: enabled +0x15 and allowed +0x14 ask the id) is most of them; 0x2812e0 vtable 0x317d10 is the partner's (0x2811d0, 0x2812c0, 0x2811c0); 0x280a60 / 0x280f50 / 0x280e20 / 0x280410 are cars and Zs9901, and need classes the host lacks. Slots +0x14 0x1df0a0, +0x18 0x1df030, +0x1c 0x1da4e0 are the slice's last two functions. **Start the next run by finding who calls slot +0x1c** (script natives, presumably): nothing found yet.

- **game/playermotion.cpp, ninth run (2026-11-05), partial (batch 128): the fight.** Written: the conditions BtOn, Lock (with `faceTarget`), BtAttack, BtAtkCombo, BtMove; the links Battle and BtAttack; `findTargetAhead`, `findTargetBeside`, `isTargetValid`, `canSeeTarget`, `findBlowPart`, `getTargetPoint`, `getLockPoint`, `isEnemyOf`, `getSides`, `getTakenOverPlayer`, `isBlowActive`, `nearestPart`, `getBrainPace` (it was a stub answering 0; a character without a brain now answers 1). New fields: `CharacterAllows::armed`, `lockFind`, `lockSwitch`; `m_sides` +0x590, `m_foe` (was `m_450`), `m_294` is an object, `CharacterBrain::state` +0x17c. `PartSwitch::partName` 0x186f80 is a stub answering null, and the callers go round it (it needs the model's part-name table).
**Seen, New Game, 420 s, work/battle_pad.txt (level_pad.txt, then from read 5800 every 420 reads R1 for 220, Cross twice, the stick up), no crash:** a temporary log, since removed: F_PC01 has abilities +0x3c2 and +0x3c3 and sides 12; 12 times `BtOn` asked bt_on (motion 77), then Lock ran in the stance, 22 times Cross started a blow from bt_s (motion 79, fire 1), and each came back to the stance or left it (33 leaves of Battle). The stage has no target in reach, so the search was never seen to find one, and no blow has met anything. **Not proven by the picture:** the shots of those runs show F_PC01's head from close by, one of them (shot_922800) filling the frame with the camera looking down at the bedding, where the room's other shots show head and shoulders with the door. The main camera takes its offsets from the motion's row (`cameraFocusOffset`, `cameraEyeOffset`), so the sheet may ask for this, or a bone or lean may be wrong: there is no reference picture of the stance, and which motion each shot shows was not measured. **Next:** a run with shots keyed to the motion, then compare with the sheet's offsets. Doubts: bt_at0 with no weapon needs ability +0x3c4 (`initAbilities`) and F_PC01 passes; `BtAttackLink::onStart` always stops Lock, because the two tests 0x294c70 / 0x294c30 read a floor object of kind 4 and the host has no such class; `m_sides` is set in `initStatus`, the ROM sets it in 0x21cc70, which is unread.

- **game/playermotion.cpp, seventh run (2026-11-05), partial (batch 127): what a character carries, written and NOT SEEN working.** Written: `StageCharacter::giveItem` 0x216aa0 and `swapWeapons` 0x216400 (both were stubs), `dropItem` 0x216920, `dropWeapon` 0x2166d0, `throwItem` 0x216630, `canDropWeapon` 0x216760, `canWield` 0x20b240 (`WieldClass`), `playPickSound` 0x2a3160, `noticeItem` 0x2184b0, `MotionProbe::findItem` 0x26a4a0, the conditions `PickupItem` and `PutItem`, new `PickupLink` (vtable 0x317b90, +0xc08, map key Pickup). `CharacterAllows::useItem5` and `CharacterWants::useItem5` are `pickup`, `UiUse_Gate` was wrong and is `UiUse_Pickup` (kind 5). `m_lookObject` +0xbd0 is the item in reach, so `MotionLookTarget` is gone: the head looks at its pick point. `m_pickItem`, `m_putItem` stand beside the EE ints, as `m_door` does.
  **Stage 1 has no items, so nothing of this can be seen there.** Measured, New Game, 290 s, a temporary test (removed) at tick 20 on the stage: list group 3 empty, F_PC01's sheet has both item columns empty, no `gItemDefs` name has a file group in `StageFiles::groups` (so `placeItem` cannot be forced either), m_lampItem and both weapons null. **Seen:** the condition runs on the player every frame; three 290 s runs lived to their limit, no [crash] line, and a fourth died in the test (the top of the stack was cut off from the log; it used `gStageScene->files`, a sub-scene field, and lived once it used `getStageFiles()`, so that is the likely cause, not a proven one); the character still idles and fidgets (motions 20, 21). The 15 s smoke run ends on the language screen, as before.
  **Not proven, none of it ran:** `giveItem` in every branch, `dropItem`, the pickup motion 0x12 / 0x13, the click, the sound, the notice, the lamp on the chest (`T_TORCH2`, `chestToolPosition`). **Next run needs a stage with items** (a stage whose map points or sheet name one); that run judges this code, with the lamp first: a character with a lamp makes `m_lampItem` non-null for the first time, and `SceneRender` reads it, so expect new light code to run.
  **Left out:** `ItemPhysics` was still stubs here (written 2026-11-06, game/itemphysics.cpp). Nothing sets `wants.putItem` / `m_putItem`: the writer 0x218850 is unread, so PutItem cannot fire. The list helpers 0x217700..0x217960 are `StageList`'s. **Doubts:** `canWield`'s mask is `WieldClass_Any` for 0xffff, the bit names are from the switch; `dropItem` as a name: it leaves the item where the character stands, a use may show it means something else; `m_inUse` set by the Pickup link on a plain item is read as used, unproven; the ROM walks the item list from the tail for a stack, the port from the head (one item per name, so no difference is expected); `noticeItem` casts the item text to `u_short *` as the ROM passes it, unread how the text is encoded; `playPickSound` guards a null name (PC).

- **game/playermotion.cpp, sixth run (2026-11-05), partial: the door crash found, and a door opens.** **The crash:** `StageMenu::StageMenu` was a stub, so `state`, `pending` and `pendingOn` were whatever the host heap held, and native 0x45 (the door scripts' first read of them) passed garbage to `wstrcmp`. The two faults caught: `read of 0x5ee44150`, `read of 0xffffffffb1827ea0`, both in `wstrlen` from `nativeIsMenuPick`. It depended on the heap's contents, which is why gdb never saw it and 3 of 4 runs died. Fixed by the ctor in new **game/stagemenu.cpp** (0x2303c0..0x230980, the rest stubs) with `pending` now a `const u_short *` (it held a 32-bit EE pointer in an `int`). **The door that would not open:** the port ran the character's start hook after the scripts, in `stageUpdateCharacterMotions`, so `memset(&m_wants)` wiped what native 0x87 had just set. Now `StageCharacter::sideHook0` 0x17a860 (allows, `beginFrame`, wants) runs in `actionStart` before the scripts, `sideHook1` 0x17a810 (input, `controlStep`) in `actionControl`, as in the ROM. New host tool: **yamipan::crash** (`core/crash_report`, an unhandled-exception filter that prints `[crash]` lines on stdout) and `tools/reverse/crashsym.py`.
  **Seen, New Game, a temporary test (removed), F_PC01 put at oMK99_ds_05's side point and Use pressed at the game's own place in `StageScene::update`:** pick 108, native 0x87 answers 1, `DoorCondition::onApply` runs, motion 73, door mode 0 -> 1 -> 2, motion 0, hand 2. The scripted pad's own CROSS then closed it (motion 74, modes 2 -> 3 -> 0) and opened it again, so the real button path works both ways. **The picture:** progress/20261105-door-shut.png (the shut door, label Open), progress/20261105-door-open.png (door slid aside, the dark room behind it, label Close). **No reference exists** for the door or the prompt. **Runs:** before the fix 2 of 3 died and earlier 3 of 4; after it, 7 of 7 lived to their limit (3 with the ctor alone, 1 with the hooks, 3 on the final build), no [crash] line.
  **Not proven:** walking through the open door (nothing tested the door's faces being off); walking on the final build: the walking-pad run shows the same room, with a small shift between two shots, so the moved hooks are proven only by the door run and the press path; a key, a lock, a forced door, type R doors (only oMK99_ds_05, a type T, was opened); sounds (`DR_%02d_*`) by ear. **Left, so `partial`:** the rest of 0x17a860 (kinds 1 and 9's flags +0x3a8 / +0x3ac, flinch / health / endurance clocks, the player's look-ahead point), the brain in `sideHook1`, native 0x7a and the `StageMenu` window 0x20ac30 (0x208f40..0x20ac30), the shared property ids, `hasNotice`, `hasFollower`, the 18 other conditions, the other links, the list 0x1df0b0, `clearNoFlash`. **Doubts:** the name `StageMenu_Opening` / `Closing` and `request` are read from 0x230400 alone; the two hooks' names stay `sideHook0` / `1`.

- **game/playermotion.cpp, fifth run (2026-11-05), partial (batch 126): doors, written and SWITCHED OFF (`kDoorsEnabled = false`).** Written: new **game/stagedoor.cpp / .h** (`StageDoor`, vtable 0x3112f0, all 26 of 0x1723c0..0x174a10 + 0x171a30), new **game/mapobject.h** (the class moved out of mapobject.cpp), `StageMapObject::setMotionList` 0x175d40, `DoorCondition`, `DoorLink`, `MotionProbe::findDoor`, `StageCharacter::doorActions` and its door helpers, natives 0x44, 0x45, 0x79, 0x87..0x89, 0xf6, 0x11b, 0x11c, 0x11d, 0x12c, 0x136, 0x14f, 0x15b, 0x231. **How doors work (read from the script oMK99_dp_01):** no Use-kind of their own: each door has a script entry that reads the player's `doorActions`, posts its label with natives 0x79 / 0xf6, and on menu mode 5 calls natives 0x87..0x89 on the player, which set wants.useDoor / useDoorKey; `DoorCondition::onApply` then requests 0x43..0x4c.
  **Seen with kDoorsEnabled true, a temporary test (removed) under gdb, F_PC01 put 30 units before oMK99_ds_05:** all 10 of stage 1's doors built (dp_01 mode 5, dp_02 0, dp_04 2, dp_05 5, ds_02 0, ds_03 5, ds_04 0, ds_05 0, ds_06 0, ds_07 0, all key 0, hp 100), `findDoor` finds it, `allows.door` set, the script posts the label (front slot full), `pick` answers 108 (7 + 100 + 1). **Not seen: the door opening.** Motion stays 0 and the mode 0 after pick. That test picked from the start hook, not at the game's place after `itemList->update`, so it does not tell: the next test must press the real way (StageScene::update's `pressed`). No picture exists yet.
  **THE BLOCKER: with kDoorsEnabled true a plain New Game run dies with an access violation just after the stage starts (3 of 4 such runs; run 1 of the session lived 300 s), and runs under gdb did not fault (crash.txt never written, 2 runs of about 4 min), so the cause is unknown.** Null guards added on the PC paths (native 0x44 / 0x231 cells, `m_mapParts`, an empty list, no bn_top) did not cure it. Suspects, unproven: `MapObjectParts` now built for every door with effects (arg2 1; `seen[(id - 1) >> 6]` holds 512 ids), `setMotionList` running for the non-door targets that used to reach a stub, the natives on objects with no character. Way in: turn on one piece at a time; a plain run needs about 285 s to reach the stage, and the log is buffered, so fflush a print. Not checked with kDoorsEnabled false beyond the 15 s smoke (out of time): the switch unbinds all of it, so a New Game run should be as at 1c892c1.
  **Left:** native 0x7a (slot +0x154, `StageMenu` 0x230800, the locked door's question; `StageMenu::setData` is a stub), the object ids of `getProperty` / `setProperty` shared with the plain map object, `hasNotice`, `hasFollower`, the brain's three, the rest of the TU as before (the 18 other conditions, the other links, the list 0x1df0b0, `clearNoFlash`). `m_doorBlow` +0x5d8 has only the setter 0x212530, so forcing a door takes nothing off. Doubts: every name is the port's; `kDoorKeyBothSides` and the key-side reading come from `keyWorksFrom` alone; the arg of `setMode` in `DoorLink::onLeave` is assumed to be the door's own mode (the decompile drops it).

- **game/playermotion.cpp, fourth run (2026-11-05), partial (batch 125): ledges, and the Use button's prompt.** Written: the conditions Level, LevelLift, Ladder, Jump, LadderMove; the links Up, Fall, Jump, Ladder; `levelBlocked`, `ladderBlocked`, `walkFileTarget`, `startWalkTo`, `pointBehind`, `isOpeningLead`, `getLadderRank`, `playLandSound`; `StageNodeTree::contains` / `setBoxes` (the wind's boxes, the tree holds the grid until its inner object is written). New file **game/uiitemlist.cpp**, because all three conditions reach the player through it: `UiItemList` (ctor, `update`, `offer`, `pick`, `post`, `enableKind`, `setKind`), `UiPrompt` (its label, fades and text), `canTakePrompt`, `canUsePrompt`; natives 0xf7, 0xf8. `CharacterAllows` / `CharacterWants`: `useItem`, `useItem2`, `useItem6` were guesses, now `level` / `useLevel`, `ladder` / `useLadder`, `jump` / `useJump`; `UiPickSlot::timer` is `kind`. **Seen, New Game, three 290 s runs, a temporary test (removed) that put F_PC01 at stage 1's two ledge faces, no crash:** Use at the top: `allows.level` 2, fal_l, lnd_l, z 831 to 771; Use at the foot: `allows.level` 1, u_060 scaled to the 60 rise, z 771 to 831. **The picture:** progress/20261105-level-prompt-down.png: he stands by the sliding door with "Down" at the top centre, the game's font, the label from the stage's own script. **No reference exists** for the prompt's place or look. **Not proven:** Ladder, LadderMove, Jump: stage 1's map has **no faces of flags 0x2020 or 0x180** (mesh Root: 8,165 faces, 2 of flag 0x40, 2 of flag 2), so nothing there can reach them; LevelLift (nothing sets wants +0x341, the brain is the guess); `post` and native 0xf6 / 0x79 (not written); a real pad; `setBoxes` ran with the stage's bbx with no crash, but the wind is not looked at. **Left out, so `partial`:** the button's picture beside the label (a sprite per button of the options' map from the init pack's sheet; its width is kept, so the text sits as far right as in the ROM); the 19 other conditions (BtOn, GunOn, Door, PutItem, PickupItem, TrapSet, TrapCancel, Throw, Showt, Reload, Jack, Bite, Lock, BtAttack, BtAtkCombo, BtMove, GunAttack, Snipe, GunMove); the other links; the list 0x1df0b0 (with its slots 0x1df030, 0x1df0a0); `clearNoFlash`. Doors come next by what a player meets first: `Door` condition 0x2710a0 / 0x271630 and the link 0x27b640, once doors are real objects. **Doubts for the user:** (1) `game/uiitemlist.cpp` is a second file in a one-TU run: it could not be left out, as nothing climbs without it; its TU is defined in tus.json with status partial. (2) The names `isOpeningLead`, `getLadderRank`, `hasSettled`, `UiPrompt`, `UiUseKind` are read from the code; `StageCar::hasSettled` tests +0x3ac >= 0.3, whose meaning is a guess, and no car object exists, so a kind 3 floor answers the stub's false. (3) `StageCar::getMode` is a host stand-in for +0x39c.

- **game/characterground.cpp (2026-11-05), verify: he stands on the map.** Written: all 19 of 0x1eff00..0x1f4bb0 (`CharacterGround`, vtable 0x315ee0: ctor, dtor, `reset`, `faceNear`, `setContact`, `setHitBone`, `beginFrame` 0x1f3930, `endFrame` 0x1f38d0, `checkFloor`, `sweepWalls`, `isInsideCar`, `leaveCar`, `keepInside`, `wallCounts`, the four hooks; the bone tables in `game/groundbones.inc`, dumped from the ELF), plus `StageObject::isPlayer` / `isPartner` and **`vecDistance` 0x135f20, a stub answering 0 until now**: every `distance - N` of the ground moved him 50 down a frame, and `StageScene::raycast` and its kin never picked the nearest of several meshes, so the camera and sound rays are likely to change too. `GroundVolume`, `floats` and `wallCheck` were `CollisionBody`s all along (`volume`, `pushBody`, `wallBody`); `onGround` was `CollisionOwner::m_enabled`, `falling` is `hasFloor`. **Seen, New Game, 285 s, no crash:** F_PC01 at z 1139.2 (the ray's floor, flags 4, attr 59), landed, for all 1,320 frames logged; with the stub he fell for ever. **Not proven:** walls and sliding: the scripted stick did not fire in the run (frames ended at 1,320 on the stage, the run spent its time before it), so next run: a pad that moves him, then watch `wallCount`. The body bits (hits, blows, cars, doors) ran only as far as sync. **Bridge, PC:** `StageCharacter::sideHook3` stands for the terminate hook 0x179d30: `endFrame` and the copy of `m_wallCheck` to `m_wallCheckNow` (+0xbac, head of 0x1e18c0). **Stubs left outside the TU:** `StageCar`'s six (0x289dd0.., no kind 3 object exists on the host yet), `getHandClass`, `playBumpSound`, `winsStruggle`, `ItemPhysics::getMeanVelocity`. **The shot** (progress/20261105-ground-stands.png, frame 694200): F_PC01 upright, whole, from behind, in the room with the sliding door, the floor at the bottom edge; no reference exists for it. **A look to take:** one frame of an earlier run of this session, 699600, before `vecDistance` was written, showed a room with the view rolled; which camera that was is not known and it was not seen again, so it may be nothing. Doubts: every name is the port's; `viewCar()` is a `reinterpret_cast` of `StageViewTarget`, as `collectViewTargets` does; `m_doorMode` +0x248 has no writer yet; `kItemThrownKnife` / `kItemThrownBlade` are guesses; the `kHandClassDriving` meaning is read from 0x210540's first test only.

- **game/characterinput.cpp (2026-11-04), verify (batch 124): the pad moves the character.** Written: all of 0x236b90..0x23a870 (`CharacterInput` with `update`, `readFire`, `hasAutoFire`, `CharacterPad::update`), `forcedViewChange` 0x214ee0, `getTurnRate` 0x2162d0, and `padSway` (it is `GamePad::m_dir[2]`, the left stick's x). `lookPitch` / `lookYaw` checked: +0x10 is minus the right stick's y (or the left's in an aim), +0x18 minus its x, through the invert options, as the camera used them. `stageUpdateCharacterMotions` now calls `update` where the control hook does; the brain 0x1dd2f0 is still left out. **Seen, New Game, 285 s, no crash, scripted left stick at reads 5720..6970 (the stage is live from about read 5600 and this run ends near 7030, so a script past that does nothing):** F_PC01 runs (`w_run` 22 then `run` 9, 12 units a frame), `t_l` and `t_r` on the spot (rotation -0.48 to 0.79 and on), backs (`w_b`), strafes with L1 (id 8), from (7286, 6064) to (4712, 3798), and stops; he goes through walls and stays at z 1138, as expected without `game/characterground.cpp`. Idle0 (id 20) did play, which answers the "no fidget" doubt under game/playermotion.cpp. **Not proven:** a real pad or keyboard; the right stick, the buttons, the aim and scope, the car (nothing sets `m_gear`), the companion (`offer`, `StageParty::start`, `canTakeOver`, `getLifeState`, `isCompanionWalking` are stubs). No reference exists for the look of it (progress/20261104-f_pc01-pad-walk.png: him from behind, whole, on black outside the room). Doubts: names of `useItem*`, `squatAction`, `dpadSide`, `padUp` and so on are from their buttons, as no condition reads them yet; `useItem2` asks kind 2 for both its values, as the ROM does (asm 0x237bb8, 0x238820); noFlash is the scope, its name should change with `clearNoFlash` when its TU comes.

- **game/playermotion.cpp, third run (2026-11-04), partial (batches 122, 123): `controlStep` is whole.** Written: the rest of `controlStep` (`stepEvents`, `stepAim`, `stepFeet`, `stepWind`, `stepHealth`, `stepFidgets`, gravity), `getLampMatrix`, `getLampTarget` (renamed from `getLampAngles`), `startWalk`, `startWalkAt` (float rate), `findEdge`, `findFloor`, `pickGunMotion`, `pickArmedMotion`, `isStageDrained`, `isMotionSlowed`, `playLadderStep`, `playStepSound`, `findStepSound`, `getSurfaceSound` (a bullet's sound was a stub until now), `src/game/stepsounds.inc`. `m_3f4` is `m_winded`, `m_3f8` is the float `m_idleTime`, `WalkLink::edgeHit` was wrong and is `edgeObject` (architecture.md "Motions").
**Seen:** New Game, 285 s, twice with work/newgame_pad.txt, no crash, stopped still running: F_PC01 whole in the room, from behind, as before (last.png of the run). **Not seen, and not proven:** no `[fidget]` from a temporary log (removed) in the 285 s run, though the delay is 10..20 s: either the stage time was too short or `stepFidgets` does not get there; nobody measured which. `StageNodeTree::contains` was not reached (the model's `lowerNames` may be 0, unread). Steps, feet on faces, gravity, `findEdge` and health cannot act until the ground exists: `floorFace` is null, `wallCheck` is null. The pair of `+0x10` / `+0x20` on the ground is read from this one reader only.
**Doubts for the user:** (1) the ROM's `playNamed` calls pass 800.0 in f12 at `playLadderStep`; the host `playNamed` has no such argument and neither has `addSourceNamed`, so it is dropped unread. (2) `+0xbd8` of the motion is written as a float (0.3) by `clearNoFlash` 0x1dfd90, but the host calls it `overlay` from `StageScene::draw`: one of the two readings is wrong; `clearNoFlash` is really the view switch to camview_snipe (first person), noted, not written. (3) `sdata 0x31a9cc` as 0: the ELF has no bytes there, it is the health clamp's low end. (4) answered 2026-11-07: `resetSurfaceTables` 0x2cf510 sets every cache slot of all 44 surfaces to 1 (not looked up), none to 0, so the host's lookup on demand matches. (5) The file name: three ranges of this TU now sit far from the motion (0x2a2100, 0x2a49c0, 0x2cf0b0).

- **game/playermotion.cpp, second run (2026-11-04), partial (batch 121): the conditions and Walk: the character walks when something asks.** Written: `MotionCondition` with Always, QuickTurn, Squat, Turn, Move (update and apply); `MotionLink::apply` / `addCondition`; `WalkLink` (it replaces the host's `PlayerLookState`: architecture.md "Motions" has the correction); `MotionProbe::faceAhead` / `pointAhead`; `controlStep` 0x1e3e80, **its head only**; `applyMovement` 0x1e0e20; `leanBody` / `leanBodyAt`, the three pose targets, `startWalkFile`; `StageCharacter::moveStep`, `isControlled`, `isTakenOver`, `getMotionRate`; `StageCamera::seesPoint`. `CharacterWants` (+0x2c0) and `CharacterAllows` (+0x360) replace the raw arrays. The idle bridge is gone from `stageUpdateCharacterMotions`: Move asks for the idle. Six abilities are named (`quickTurn`, `moveCrouched`, `strafe`, `walkBack`, `runStart`, `stairs`).

**Seen, New Game with work/newgame_pad.txt, 285 s, no crash:** motion 0 through the Walk link, F_PC01 whole in the room, as before the bridge went. A temporary test, since removed, filled `moveKind` on a 900-frame cycle: id 3 `w` (x, y moved about 1.5 units a frame in the direction he faces, `moveYaw` 0.02 turned him 0.8 rad in 40 frames), id 22 `w_run` then id 9 `run` (about 12 units a frame), id 12 `t_l` turning on the spot, id 29 `sq_s`, back to 0. The shot (`progress/20261104-f_pc01-run-test.png`) shows him in a running pose, whole, from behind, with the main camera following. No reference exists for it, and the darkness around him is outside the room: **he walks through walls** and stays at z 1138, since `CharacterGround`'s step 0x1f3930 is not written. The Walk motion's look was judged from that one frame only.

**Left:** the rest of `controlStep` (from "if motionId has a row": motion events to `setContact` and `0x1f00a0`, step sounds, `setAimDirection`, feet by raycast, wind, the fidgets 0x14 / 0x2b, status timers); `findEdge` 0x26b960 (0x814 bytes, stub; `onUpdate` skips it while `wallCheck` is null, as it is until game/characterground.cpp); `getLampAngles` 0x1e6160, `getSwayMatrix` 0x20c940, `ViewCamera::setZoom` 0x29a840 are stubs; `m_runTime` / `m_runDistance` are never counted up (their writer is unread); the 24 other conditions and all the other links, so a motion whose link is missing has no conditions and only ends through 遷移モーション; the start and control hooks 0x17a860 / 0x17a810 are still bridged in `stageUpdateCharacterMotions`, so `StageCharacter`'s sideHooks stay empty: they belong to the next TU in the queue. The partner's cr_2 at the head of `AlwaysCondition::onApply` needs the list of 0x1df0b0.

**Doubts for the user:** (1) answered, twelfth run: the pace comes from the sheet through `initStatusBlock`, the PC default is gone. (2) The conditions and links (0x26d020..0x27d8d0) sit far from the motion core in the ROM, so they are probably their own object file; they stay in `playermotion.cpp` to keep this run to one TU, pinned by `tus.py define`, and can be cut out later. (3) The kind names of `CharacterMoveKind` come from the motions Move asks for; which stick direction gives which kind is 0x236b90's to say. (4) `m_walkKind` holds `startWalkFile`'s 0.35 as raw float bits, since `startWalk` is still a stub taking int: it is probably a float.

- **game/playermotion.cpp (2026-11-04), partial (batch 120): the motion core, and the spikes are gone.** Written: ctor, dtor, `loadMotions`, `reset`, `beginFrame`, `refresh`, `request`, `requestNow`, `stepLower`, `stepUpper`, `stepFace`, `arbitrate`, `resolveHold`, `noteLeaving`, `startRequested`, `pickStance`, `isEnding`, `requestedFile`, `stageUpdateCharacterMotions`, and `StageCharacter::noteMotion` 0x21a960 from outside the range (architecture.md "Motions"). `PlayerMotion` left `gamehooks.h` for `game/playermotion.h` and is a real host layout now. **Seen:** New Game runs of 300 and 260 s with work/newgame_pad.txt, no crash, stopped still running; a temporary log (removed) read F_PC01 at motion 0 (`s`) on pose shc_ch_02. **The cause guessed in the last run was right:** with a motion playing, the boy stands whole in front of the main camera, seen from behind, no spike (progress/20261104-f_pc01-stage-idle.png against progress/20261104-f_pc01-stage-stretched.png). **No reference exists for this shot**, so whether the stance and the camera distance are the game's is not checked; the shot is dark as all stage shots are. **Not done, so `partial`:** the links, the conditions and the list 0x1df0b0 (`findLink` answers null, as for a motion with no link; `initSlots14/18` are empty); `startWalk` 0x1ead00, `startWalkAt` 0x1eaea0, `clearNoFlash`, `swapWeapons`, `isScenarioLead`, `CharacterGround::setContact`, `StageParty::isRunning / stop` and `getLampMatrix` are stubs. **Three bridges in `stageUpdateCharacterMotions`, marked PC:** it calls `beginFrame` (the ROM: start hook 0x17a860) and `advance` (movement hook 0x17a490), and it asks for the idle each frame. **Next slice of this TU:** the Walk link 0x2775d0 (vtable 0x3177a0), with the conditions it reads, then who requests motions (0x20d8c0). Doubts for the user: the file name; the group names in `playermotion.h` (`isMovingMotion`, `isHoldableMotion`, `isStandableMotion`, `isArmedMotion`) and `isScenarioLead` are the port's; the shot numbers ran ten times faster than in the last run (256,200 in 300 s; the last run's newest shot was 25,440), and nobody measured why, so the host's frame pace needs a look.
- **game/stagecharacter.cpp (2026-11-04), partial (batch 119): the character is built, the main camera links, and the picture is wrong.** Written: ctor 0x17de30, dtor 0x17dd10, `setup` 0x17c700, `setupBody` 0x17d970, `reset` 0x17c620, `initAbilities`, `initStatus`, `rollIdleDelay`, `stopBodyEffects`, and `isViewLinked` 0x2176d0 from outside the range (architecture.md "The character, built"). **`StagePlayer` is gone as a stand-in**: `typedef StageCharacter StagePlayer`. It read EE offsets through a host object, so the first real player would have read garbage (`posX`, `weapon`, `items`, `motion`, `map`); `uigame.cpp`, `getFloor` and the draw now read the real fields. `YAMIPAN_CAMERA`, `stageCameraDebugLookAt` and `aimDebugCamera` are removed, as the brief asked. Parts are stubs (input, brain, ground), and `PlayerMotion`'s ctor, dtor, `loadMotions` and `stageUpdateCharacterMotions` are bridges in lib/unassigned_stubs.cpp. **Seen:** New Game runs of 270 to 330 s with work/newgame_pad.txt, no `YAMIPAN_CAMERA`, no crash, stopped still running. A temporary log (removed) read F_PC01 at (7286, 6064, 1138), yaw -0.482, shown, pose `shc_ch_02`, body size 3, main camera at (7296.4, 6137.6, 1228.0), linked. After the event the room shows from that camera instead of black: **the black after the event is fixed.** With the pose bridge the boy stands at his mark in front of the camera, hair and pajamas right, **and his limbs are stretched into long spikes off to the right** (progress/20261104-f_pc01-stage-stretched.png, brightened sheet -brightened.png). The shot is dark as all stage shots are; the sheet is 5 x. **The cause is a guess, not measured:** no motion plays, because the bridge has no `PlayerMotion::reset`, so the bones with no track may stay where they stood before the pose moved to the object. The event draws the same boy unbroken, with a motion playing, which fits. Test it first in `game/playermotion.cpp`: play the sheet's `startMotion` in `reset`; if the spikes stay, look at `StageModel::advance` with no motion and at `bindBones` for a pose placed after binding. **Not seen:** `giveItem` is a stub, so the character holds nothing and the two item loops find no taker; the effects, since `StageNodeTree::spawn` is a stub and answers null; `setupBody`'s spirit branch; the status setters; any character but F_PC01 on screen, or any comparison with the real game (no reference shot exists). The two event guards stayed (above). **Doubts for the user:** the file name; `CharacterAbilities`' flags are named by offset because nothing reads them yet; the sound fields' stubs `initSounds` / `stopSounds` 0x2a6910 / 0x2a6830 were left to their own TU.

**Natives 0x70 and 0x64, the view (key `game/stagecamera.cpp`, code in `game/scriptrunner.cpp`), 2026-11-04, `verify` (batch 115).** `nativeSetPlayer` 0x1c81a0 and `nativeScriptView` 0x1c8dc0 with `isNullName` 0x1c8f40, `findCameraObject` 0x1d1260, `findLightObject` 0x1b56b0; 0x1b54a0 is `StageNodeBase::setParent` out of line (architecture.md). `// PC:` in both: the player or partner label with nobody behind it leaves the name null (the ROM's stack is uninitialised there), and the native then does nothing.
**Seen, 0x70:** New Game run of 200 s with work/newgame_pad.txt, no crash: `scriptNative_70` has left the stub list; the last shot is a dark framed object, not black, and was not compared with an earlier run. `StageCharacter` does not exist, so `findCharacter("F_PC01")` answers null and `setPlayer(null)` survives; no log line shows the call itself, and **the link itself has not been seen**: that needs a character.
**Seen, 0x64:** nothing in a New Game run reaches `sd_MK01_10`, so a temporary build, since removed, held native 0x1f2 off and called `nativeScriptView` with `CAM_EV10` at stage frame 60 and `NULL` at frame 400, no `YAMIPAN_CAMERA`. The log's top view went `CAM_EV10` kind 7, then `camera` kind 9, and the shots show the tatami, the picture book and the bedding (progress/20261104-script-view-cam-ev10.png), then black, which is the unlinked main camera at the empty origin. The scene matches what the stage file says of that camera; no shot of the real game exists, so it is not compared with the game. **Not run:** the event branch (`scr_demo_pl` on the view). `demoSwitchOn` and `setParent` run in other paths, but this call of them has not. A first attempt with `YAMIPAN_CAMERA` set showed nothing: hazards.md.

**Map objects in their places, 2026-11-04 (the user's report: 3D renders but makes no sense).** `StageMapObject`'s ctor now makes the ROM's `setDirtyTree(false)` call (0x1765e0), and `StageCamera`'s ctor, `setRange` and `setScrZ` pass the ROM's far, 1000 + `m_range` + `m_range2`. The rotation order, `multiplyMatrix`, `invertRigidMatrix`, `rotateMatrixX`, `calcViewMatrix` and the screen matrix were compared with the ROM and match; the far was the only math difference, and it changed 3 of 349,696 pixels in a small room, so it is correct but was not the symptom. **Seen:** from the player's start, door `oMK99_ds_06` is missing without the fix (the closet behind it shows) and stands in its place with it, level with its neighbour and the track. From `CAM_EV10` the picture is byte-identical with and without: that room is `Root`, which was never hurt, so what makes a stage readable is the camera, not this fix. **For the runs:** since `3abd62f` every New Game run goes black when the stage starts, with or without this fix and with any `YAMIPAN_CAMERA`: `Root` asks for d0101 through native 0x1f2, a fade to black and then the event, and the event draws nothing yet (`SubSceneMapObject` and the others are stubs). That is not this fix. Measured on `3abd62f`: black without the patch, black with it, and with the patch and that native held off in a test build the door stands as on `8b9b3f5`, the identical PNG. So `YAMIPAN_CAMERA` shows the stage only while the event is held off. With it held off, stage 1 from its own camera is `YAMIPAN_CAMERA=7236.6,6105.9,1207.1,7211.6,6144.9,1118.1`. Not compared with the game: no reference exists.

**The pinned lights, closed 2026-11-04 (game/scenerender.cpp, `verify`).** The user's pin: "add the stub but make sure to pin the lighting as a priority as to not forget about removing the stub". Removed: `YAMIPAN_AMBIENT` (`applyDebugAmbient`, its call, the README paragraph) and `SceneRender::isKeyed` with its two guards. The picture now takes the script's lights, nothing was brightened. **Measured:** the old guess is wrong, fog is not why the stage is dark: `aimDebugCamera` stands 437 from its target, far inside fog 1100..1500. The trace in a New Game run: the record holds ambient 1.37255, 1.41176, 1.56863, every lit vertex gets it, directional 0, and the map's own vertex colours average only 20 of 0x80 (0.16), so a lit vertex averages 27, 28, 31. A dark stage is what the numbers say, on a map with nothing lit but its ambient (spot, skinned meshes and lamps do not draw yet), and agrees with the user's lead on the opening event. **A bug found by reading VU1 0x22f..0x259:** the fog value is clamp(A + B / W, far value, near value), VU address 24 = (A, near value, far value, B times the two scales of 0x59b190, 0x59b194); the host only saturated, so stage 1's near value 230 was lost. `YamiPanModelPass::fogNearValue` / `fogFarValue` and model.vert.hlsl now do it: everything inside the fog near is 230 / 255 of its colour, darker, as the ROM. **Seen, no game reference exists:** mean brightness 0..255 of the same `YAMIPAN_CAMERA=auto` ground: 3.2 unkeyed, 11.3 / 12.1 / 12.2 RGB with the script's lights (blue-tinted as they ask), 10.2 / 10.9 / 11.0 with the fog limit, a ratio of 0.90, which is 230 / 255. Without YAMIPAN_CAMERA the view is where the unlinked camera stands: an alley between two walls, a dark red shape at its far end, ground visible, mean 12, max 58 (work/shot_lights_default_camera.png): the first shot that looks like a place. **Corrected the same day: that was not the map.** The camera stands at the world origin there, about 9,500 units from stage 1, and the alley was the map objects piled up at that origin by a missing `setDirtyTree(false)` (hazards.md, "A picture that looks like a place can still be wrong"). With that fixed the view is black. The lights' numbers above came from the `auto` view of `Root`, which the bug never touched. Both are 280 s New Game runs with no crash, assert or fault. Neither is compared with the game, nor can be until the event camera or a character stands where the game does: that is why this ends `verify`, not `done`. Noted, unexplained: one run reached frame 8460 in 290 s, three others about 2940 (10 fps on the stage), same pad; the per-vertex work on the game thread is the known suspect. Doubt: the host's W is taken to be VU1's W over the two scales of 0x59b190, 0x59b194, read from the shape of the upload, not proven; at 437 the value clamps, so this run cannot show it. Next: `game/demoevent.cpp`.

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
- **game/scriptrunner.cpp, 2026-11-05 third run, partial (batch 142, 88 names): 56 natives by use on the disc, and the respawn behind them.** Written: 0x2d, 0x32, 0x33, 0x35, 0x3d, 0x50, 0x51, 0x54, 0x57, 0x59, 0x5a, 0x5d, 0x82, 0x85, 0x86, 0x90, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa5, 0xa7, 0xb0, 0xb3, 0xc4, 0xc5, 0xce, 0xdd, 0xe9, 0xff, 0x100, 0x11f, 0x123, 0x128, 0x12a, 0x12e, 0x132, 0x137, 0x13b, 0x13f, 0x143, 0x166, 0x171, 0x174, 0x17e, 0x17f, 0x18c, 0x192, 0x19b, 0x1c5, 0x1c6, 0x1ef, 0x1f6, 0x1f9, 0x214. Behind them, in the same TU: `missionPickSpawnPoint`, `missionSpawnAt`, `missionSpawnPointTaken` (stubs until now; the TU's range grew to 0x29f500..0x29fde0), `scriptPickNamed`; in game/playermotion.cpp `StageCharacter::canSpawn` and `spawn` (stubs on StageObject until now, called each frame for every character off stage), `isSpawnable`, the four leaves, `wakeBrain`, `getClimbPoint`, `findLooseWeapon`, the P_SRC search points, and the character's sides of the runner slots +0x94, +0xb4, +0xb8, +0xd4, +0xdc, +0xfc, +0x12c, +0x130. **Seen, New Game, work/doorwalk_pad.txt, 285 s, twice, no crash, no `[stub] scriptNative_` line:** the dark first-person view at the door wall, as before; nothing here draws. `parseSpawnId` 0x29fb30 was a stub answering 0, so until now every map name became spawn point 0 and no `StageMarker` was built; it is written, and the second run covers it. **Not proven:** none of the 56 is called by stage 1's first rooms (67 natives run there, all written before this run), so nothing here has run on real arguments; the first stage with F_YRT or F_SRT waiting and a P_SPW point is the test. Sorts: `std::stable_sort` by distance for three ROM instances of std::sort, whose comparison was read in one (0x2a10f0) and assumed in the other two; ties can fall differently. Doubts for the user: the names are read from the code (`leaveLoud`, `leaveQuiet`, `leaveNoticed`, `leaveAtOnce`; `strong` for the class 7 of a spawn, F_YRT / F_SRT and YRT_VANISH / SRT_VANISH say kinds 7 and 3 are not stronger and weaker for sure); 0x3d and 0x93 read the same bit, kept as two natives as the ROM has them; `kMissionSpawnPoints` was 0x60 and is 100, what fills the mission's size and what native 0x174 walks.
- **game/uiitemlist.cpp, closed 2026-11-05, done (batch 150): the Use prompt's button.** `UiPrompt::setSheet` keeps the sheet (+0x24) and a Tim2Texture of its first sprite's picture (+0x20), `UiPrompt::draw` puts the button's sprite left of the label through the host UI module, `~UiPrompt` 0x208c00 deletes it; all 20 functions written. **Seen, New Game, work/walklong_pad.txt, 340 s, shot_07800:** the amber cross beside "Open" at the top, centred with the label, drawn in the same coordinates as the text. How to find a short-lived 2D element: `--shot-every 30`, then count bright pixels in its band with System.Drawing from PowerShell (no PIL here). Natives 0x79 and 0xf6, named in the old handoff, were already written. Not looked at: the press blink (two sprites alternating, kPressKeys) and the dim blocked state; the other buttons of the map, reached through options.
