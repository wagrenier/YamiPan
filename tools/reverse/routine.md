---
name: yamipan-next-tu
description: Advance the Forbidden Siren 2 port in YamiPan by one step (a bootstrap step or one TU), commit and push.
---

YamiPan scheduled run: advance the Forbidden Siren 2 (PS2, Europe, SCES_538.51) -> PC port by exactly ONE step -- the next bootstrap step while the TU loop does not exist yet, one translation unit (TU) once it does -- commit, push, report.

Repository: C:\Users\willi\Documents\git\YamiPan, on the work branch fs2-port. Work only inside it; ../RosePan, ../ReiPan, ../MioPan and ../GhidraMCP are read-only references. Nobody is watching this run: never wait for answers or ask questions -- decide, write doubts into .claude/skills/yamipan-port/references/open-items.md, and carry on.

Python: run every Python command with the launcher `py` (`py preflight.py`, `py tus.py next`), also where the skill writes `python <tool>.py`: on this machine `python` can be the Microsoft Store stub ("Python was not found").

1. Preflight. From C:\Users\willi\Documents\git\YamiPan\tools\reverse run `py preflight.py`. It checks the user's stop switch (work/STOP), a running autopilot, the run lock, a clean tree, the work branch and Ghidra serving SCES_538.51, then takes the run lock (work/run.lock).
   - Exit 3: the previous run is still going. Reply "previous run still going" and stop.
   - Any other non-zero exit: stop, change nothing, and reply with its line. A dirty tree or a stale lock usually means a run crashed: say so, so the user can look.

2. Advance one step. Invoke the Skill tool with skill "yamipan-port" and args "next" (if that skill is not listed, read C:\Users\willi\Documents\git\YamiPan\.claude\skills\yamipan-port\SKILL.md and follow it), then:
   - While tools/reverse/tus.py does not exist, the skill's "Bootstrap" applies: do the next unfinished step of references/bootstrap.md (open-items.md "Next up" says where it stands), one step per run. Record its facts in architecture.md and tick it in open-items.md. Commit with git yourself (tus.py finish does not exist yet): look at `git status`, `git add -A`, `git commit` with a message naming the bootstrap step and ending with the Co-Authored-By line from your session instructions. A step too big for one run: commit what is coherent (and builds, once there is a build), and write in open-items.md "Next up" exactly what is left.
   - Once tus.py exists: the skill's "One TU per session" protocol end to end: `tus.py next --claim`, brief, boundaries, names, header, code, `tus.py check <TU> --run 15`, record knowledge in the skill references, and `tus.py finish <TU> --status done|verify|partial|blocked --summary ... --notes ... --coauthor "<the Co-Authored-By line from your session instructions>"`. `tus.py next` honours the pinned queue in tools/reverse/data/tus.json (host tasks first), then the user's priorities in open-items.md "Next up". Work on that one TU only; do not start a second one.

3. Look at the screen once the port builds and runs -- the visual test decides, not the logs. Smoke runs save screenshots to tools/reverse/work/shots/ (shot_<frame>.png every 60 frames, last.png); open them with the Read tool. Reference pictures come from the game's own assets (tm2png.py into work/ref/). A TU on the drawing path is done only when the screenshot shows what the reference shows; a wrong picture, or code still to write, means --status partial with the difference in the notes; complete code nothing on screen can show yet is --status verify.

4. Ghidra. GhidraMCP 2.0.0 never saves: after any naming or other change in Ghidra, save it through the GhidraMCP 5.14.2 plugin's save_program once that plugin is installed (open-items.md); until then write "Ghidra has unsaved changes: Ctrl+S" in the report.

5. Push. After the commit, run `git -c credential.interactive=never push origin HEAD` (pushing each run's commit to origin/fs2-port is approved: open-items.md "Decisions", 2026-10-31). Never force-push, rewrite history, or delete branches or tags. If the push fails, say so in the report and leave the commit local.

6. Release the lock: `py preflight.py --release` from tools/reverse -- always, also when the step failed or was abandoned.

7. Report in 3-5 lines: the step or TU, its status, the commit hash and whether it was pushed, what the screenshot shows compared with the reference (once there is one), and the next blocker.

Rules: never start other sessions, agents or scheduled tasks; do not edit tools/reverse/autopilot.py, tools/reverse/preflight.py or this routine (tools/reverse/routine.md); never switch branches; leave the tree clean (look at `git status` before committing for files that are not yours). Do not put backticks in --notes or --summary (the shell runs them). If the step cannot be finished, keep what is coherent and builds, revert what does not, and leave notes the next run can act on.
