---
name: depth200-callee-contract-deep-paths
description: "depth-200 truncates loops over contract-replaced callees; only shallow mutants killable, snapshot-free postconditions carry the kills"
metadata: 
  node_type: memory
  type: project
  originSessionId: 3baaabd9-ccad-4e51-ae31-e9d59b91ae0c
---

In a function whose loop body calls several contract-replaced callees (each
assuming a quantified/multi-clause ensures), the fixed `--depth 200` is consumed
on any path that actually exercises those calls. Verified on `slideArray` in
/app/2048-clone/2048.c (4 findTarget calls in a SIZE=4 loop).

**Why:** deep "real work" paths get truncated → strong postconditions hold
vacuously there; only mutants that make the body *shallow* are killable —
out-of-bounds loop bounds (caught by CBMC safety) and do-nothing loop bounds
(empty body → postcondition reached & fails). Deep-logic mutants (wrong merge,
tile deletion, double-merge via stop=t-1) survive regardless of spec strength.

**How to apply:**
- Lead with a CHEAP, snapshot-free postcondition that fails on do-nothing
  mutants. For a 2048-style slide, "left-packed" works: `(a[0]!=0||a[1]==0) &&
  (a[1]!=0||a[2]==0) && (a[2]!=0||a[3]==0)` — no `__CPROVER_old`, no shifts. This
  alone gave 4/10.
- `__CPROVER_old` snapshots are recorded at function ENTRY and cost depth there.
  Too many push even the cheap shallow-path kills out: left-packed+sum = 4/10,
  but adding two `*score>=old` / `<=old+sum` clauses dropped it to 0/10.
- Mass-conservation `sum(2^cell)` is the strongest invariant but vacuous at
  depth 200 (kept anyway, documented). Use a file-scope power-of-two table
  (uint64_t) instead of in-contract `1<<v` to cut cost — same trick as
  [[cbmc-depth200-starves-later-ensures]].
- Diagnose vacuity by appending `__CPROVER_ensures(0==1)`: if it "verifies",
  earlier clauses already exhausted the budget. See [[cbmc-depth-200-vacuous-loops]].
