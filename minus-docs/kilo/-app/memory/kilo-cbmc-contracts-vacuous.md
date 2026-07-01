---
name: kilo-cbmc-contracts-vacuous
description: "In /app/kilo/kilo.c, all CBMC contracts verify vacuously (is_fresh requires are UNSAT), so mutation kill scores are 0 for every function regardless of spec strength"
metadata: 
  node_type: memory
  type: project
  originSessionId: bbcca269-87db-44ba-a897-1cf9da19dad5
---

In `/app/kilo/kilo.c`, MANY functions' CBMC contracts verify **vacuously** under
`run-cbmc`. Adding `__CPROVER_ensures(0 == 1)` to a function (e.g.
`editorUpdateRow`, `editorRowAppendString`) still "verifies successfully" — proof
that the `requires` preconditions are UNSAT.

**UPDATE 2026-06-25: vacuity is NOT universal.** `editorDelChar` verifies
NON-vacuously and genuinely kills 11/25 mutants. So kill score CAN be a real
signal here for some functions — check empirically, don't assume 0.

`abAppend` (the abuf realloc/memcpy helper): vacuous, kill 0/3. **UPDATE
2026-06-25: its `requires` were switched from `is_fresh` to
`__CPROVER_w_ok`/`__CPROVER_r_ok`** (see [[kilo-editorRefreshScreen-spec]]):
is_fresh in a *replaced* requires aborts goto-instrument, which blocked every
caller. abFree was changed the same way. Both still verify (still 0/3 — no
regression). abAppend's `ensures` is now is_fresh-on-success (the disjunction
with OBJECT_SIZE caused well-definedness FAILURES when the contract was
replaced). NOTE: is_fresh in *ensures* is fine (collapses vacuously); only
is_fresh in *replaced requires* is fatal.

**Threshold/poison effect (editorDelChar):** with `requires` using
`is_fresh(E.row, ...)` + targeted per-row `.size` constraints, and `ensures`
only on `E.dirty`/`E.numrows`, the proof is non-vacuous (11 kills). But adding
ANY postcondition over a cursor field (`E.cx`/`E.cy`/`E.rowoff`/`E.coloff`) —
even a logically-true one the correct body satisfies — flips the WHOLE proof to
vacuous (killed drops to 0/25, base still "Verified"). Strengthening made the
score strictly worse. So: cap cursor `ensures`; the surviving 14 mutants
(cursor-update logic) are not killable without triggering this collapse.

Cause: the `requires` clauses use `__CPROVER_is_fresh(...)`, and the contract
instrumentation emits the warning `function 'malloc' is not declared` in
`__CPROVER_replace_ensures_is_fresh`. The `is_fresh` assumptions collapse to false,
making preconditions unsatisfiable.

Consequences when verifying functions here:
- **Mutation kill score is 0/N for ALL functions**, including the already-committed
  strong specs. It is NOT a usable signal for spec strength in this environment.
  Do not chase kill score — "obvious it cannot be increased."
- `__CPROVER_old(...)` appears not to capture entry values, but that's just an
  artifact of the vacuity (post-state is unreachable).
- An `at + 1` (or similar) overflow check **in a `requires` clause** still fires as
  `overflow.*: FAILURE` even though the contract is vacuous — it's checked during
  assumption construction, before is_fresh makes it UNSAT. This false "non-vacuous"
  signal misled me. Keep overflowing arithmetic OUT of `requires`; it's fine in
  `ensures` (post-state unreachable under vacuity, so it never fires).
- CBMC quirk: `at < row->size` / `at <= row->size - 1` phrasings behave oddly vs
  `at + 1 <= row->size`; irrelevant given global vacuity.

Goal under these conditions: write the strongest *intended* spec that still exits 0
(no spurious requires-expression overflow), covering all branches with guarded
`ensures`. See [[kilo-editorRowDelChar-spec]].
