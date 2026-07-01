---
name: cbmc-editorrowdelchar
description: editorRowDelChar verifies 6/9 via conditional preconditions+assigns to fit the early-return postcondition inside --depth 200; 3 survivors are equivalent
metadata: 
  node_type: memory
  type: project
  originSessionId: 1a230ace-0e5a-4bc0-bbb4-aa51737fd57b
---

`editorRowDelChar` (kilo.c) verifies under the canonical pipeline (PASS) with kill
score **6/9** — the maximum: the 3 survivors are provably **equivalent mutants**.

Key facts (canonical pipeline = `--partial-loops --unwind 5`, `--replace-call-with-contract editorUpdateRow`, `--enforce-contract`, `cbmc --depth 200`):

- editorUpdateRow is called BEFORE `row->size--`, and its contract requires
  `row->size == 1`. Since the canonical pipeline replaces it with that contract,
  the incoming `row->size` is FORCED to 1. With the guard `row->size <= at` and
  `at >= 0`, the only proceeding offset is `at == 0`.
- At `at == 0` the three memmove-arithmetic mutants are TEXTUALLY EQUAL to the
  original: `size+at`==`size-at`, `chars-at+1`==`chars+at+1`, `chars-at`==`chars+at`.
  These (06,08,09) are genuine equivalent mutants — unkillable at ANY depth
  (confirmed: at depth 500 they fail identically to the original, which itself
  fails there due to --partial-loops unsoundness on the deep proceed path).
- Killed: all 5 guard relationals (via OOB memmove on the wrong proceed/return
  decision, or postcondition) + the `chars+at-1` source mutant (reads chars[-1]).

THE TECHNIQUE that raised the score from 0/9 to 6/9 (see [[cbmc-depth-vacuous-and-recursive-callees]] and [[cbmc-editorrowappendstring]] for the depth-200-truncation wall):
- The is_fresh setup at function ENTRY (esp. `is_fresh(row->hl,8)` + the
  editorUpdateRow contract-replacement machinery) consumes the whole depth-200
  budget, so even the SHALLOW early-return path's `ensures` was vacuously
  truncated → 0 kills.
- Fix: make every precondition/assigns target that is only needed on the
  PROCEED path CONDITIONAL on `(at < row->size)`:
  `__CPROVER_requires((at < row->size) ==> __CPROVER_is_fresh(row->hl, 8))` etc.,
  and a conditional assigns group `__CPROVER_assigns(row->size, E.dirty; (at < row->size): object_whole(row->chars), row->render, row->rsize, row->hl, object_whole(row->hl))`.
  On the early-return path the is_fresh allocations / w_ok checks are skipped, so
  that path's entry stays under depth 200 and its postcondition is actually checked.
- Conditional assigns syntax: groups separated by `;`, each `condition: target,target`.
  Needed because object_whole(row->hl) validity is asserted at entry on ALL paths
  unless the target is guarded — otherwise the early-return path fails
  `assigns ... is valid` since hl is only fresh on the proceed path.
- Do NOT add a content `ensures` with an `old(row->chars[at+1])` snapshot: the
  snapshot is evaluated unconditionally at entry, costs ~40 depth, pushes the
  shallow check back past 200 (drops kills 6→0), and is vacuous anyway (proceed
  path needs depth ~400).

Harness: `/app/kilo/run_mutants_editorRowDelChar.sh` (mirrors canonical pipeline,
resolves mutation-site line numbers via grep so it survives spec edits).
