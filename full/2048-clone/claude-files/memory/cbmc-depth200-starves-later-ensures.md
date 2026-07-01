---
name: cbmc-depth200-starves-later-ensures
description: fixed --depth 200 can starve the 2nd+ ensures clause even in loop-free funcs with init-heavy bodies
metadata: 
  node_type: memory
  type: project
  originSessionId: 6feb158b-6238-4b95-a5d9-b6086dec35a8
---

The mutation harness runs each mutant with a fixed `cbmc ... --depth 200` (no `--pointer-check` flag in the command, but pointer/bounds checks are ON by default in this CBMC). Beyond the loop case in [[cbmc-depth-200-vacuous-loops]], depth-200 also silently truncates **straight-line** functions whose body does a lot of work before the postconditions.

Observed in `2048-clone/2048.c` `getColors`: the body initializes four 32-byte local arrays (~128 stores) plus two pointer derefs. With two `__CPROVER_ensures` clauses, only the FIRST is reached within depth 200; the SECOND passes vacuously, so its in-bounds wrong-value mutant survives (verifies SUCCESSFUL, but fails at `--depth 205+`). OOB-pointer mutants are still killed early by the built-in dereference checks at the body's `*(ptr + ...)`.

Levers that bought back depth (each ~a few steps), raising kill score 0.33 -> 0.83:
- Replace `__CPROVER_is_fresh(p, ...)` with `__CPROVER_w_ok(p, ...)` for output pointers — is_fresh acts like malloc (loop), w_ok is a cheap writability check. Original still verifies; mutants get caught.
- Collapse multiple `__CPROVER_requires(...)` into ONE clause joined by `&&` (fewer assume steps).
- Use a file-scope `static const` lookup table in the contract instead of an in-contract compound literal (avoids per-call re-initialization in the ensures).
- Put the most important ensures FIRST (it's the one guaranteed to be reached).

Could not fit BOTH symmetric in-bounds value-postconditions (foreground AND background) within depth 200 — ~5 steps short, dominated by fixed body init + 6 dereference checks per output read. 0.833 was the ceiling; the lone survivor is a pure bounded-depth artifact, not a spec weakness. Diagnose with a standalone repro + `for d in 200 205 ...; do cbmc ... --depth $d; done`.
