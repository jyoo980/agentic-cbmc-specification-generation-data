---
name: cbmc-editorrowstostring
description: editorRowsToString spec is 9/11-strong (max) but canonically UNVERIFIABLE — malloc-may-fail NULL-deref + should_malloc_fail crash + unnameable return buffer
metadata: 
  node_type: memory
  type: project
  originSessionId: 86e6d4db-9010-4aea-b94c-b5a3bbde711c
---

editorRowsToString (kilo.c) builds one heap string from all rows: two loops over
E.numrows (compute totlen += size+1; then memcpy each row + '\n'), malloc, returns buf.

**Spec (strong, sound):** requires is_fresh(buflen); E.numrows in [0,1];
is_fresh(E.row, sizeof(*E.row)) (exactly ONE row ⇒ reading E.row[numrows] is OOB,
kills off-by-one); E.row[0].size in [1,8]; is_fresh(E.row[0].chars, size).
assigns(*buflen) only. ensures(*buflen>=0) and the killer postcond
`__CPROVER_return_value[*buflen] == '\0'` (too-few-rows leaves byte at *buflen
nondet; too-many overflows the allocation).

**Strength = 9/11 (the MAX).** The 2 survivors are the `!=` loop-guard mutants on
both loops: `j != numrows` ≡ `j < numrows` for ascending j with numrows>=0 →
genuinely EQUIVALENT, unkillable. Measured via the sound pipeline below.

**CANONICALLY UNVERIFIABLE** (run_cbmc fails, like [[cbmc-editorfind]]):
- Only externals are malloc + memcpy — both shared, so NO unique external exists to
  scope an auto-selected /app/stubs file (selection keys on a `/* FUNCTION: name */`
  marker the target calls; marking malloc/memcpy collides with editoropen.c's malloc
  for editorOpen).
- run_cbmc attempt-1 (no --add-library): enforce OK (malloc is no-body ⇒ recognized
  by NAME ⇒ the returned buffer's *p writes are in-frame), but `cbmc --depth 200`
  with DEFAULT malloc-may-fail makes the un-guarded `p=malloc(...)` a real NULL-deref
  (5 FAILUREs at *p) — a genuine latent bug in the C, not a spec flaw.
- run_cbmc attempt-2 (--add-library): goto-instrument ABORTS in
  instrument_spec_assigns create_car_expr on the builtin malloc model's
  `bool should_malloc_fail` ("no definite size") — same should_malloc_fail crash that
  hits any direct-malloc function under --add-library (see [[cbmc-editorinsertchar]]).

**Why no stub rescues it:** the malloc'd buffer is the RETURN value (local `buf`),
not stored in a named global/param — unlike editorOpen (object_whole(E.filename)) or
editorUpdateRow (object_whole(row->render)). So *p can't be an assigns target; it's
assignable ONLY via builtin-malloc by-name recognition (no body at enforce), which
then needs `--no-malloc-may-fail` to dodge NULL — a flag canonical never passes. A
non-failing allocate-stub (`__CPROVER_allocate`) dodges NULL but loses frame
recognition → lone `*p assignable: FAILURE`.

**Sound proof pipeline** (verify_editorInsertRow.sh-style, baseline + 9/11):
goto-cc kilo.c --function F (no stub, no add-library) → goto-instrument
--partial-loops --unwind 5 → --enforce-contract F → `cbmc --no-malloc-may-fail`
(NO --depth). Adding `--depth 200` alone collapses kills to 2/11 (OOB paths
truncated — the [[cbmc-depth-vacuous-and-recursive-callees]] effect). Harnesses kept
at /app/kilo/_score_erts.py (depth-capped), _score_erts_nodepth.py (true 9/11),
_run_erts.py.
