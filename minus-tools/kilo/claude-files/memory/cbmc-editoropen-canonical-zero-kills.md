---
name: cbmc-editoropen-canonical-zero-kills
description: "editorOpen in kilo.c verifies under the canonical CBMC harness but kills 0/9 mutants — the body past malloc is depth-truncated (vacuous postconditions), getline's line buffer is untracked, and the error path self-prunes"
metadata: 
  node_type: memory
  type: project
  originSessionId: f5053f09-22af-47a1-ac11-0cf6a9af8d6e
---

`editorOpen` (/app/kilo/kilo.c) verifies under the canonical harness (tools/run_cbmc.py:
goto-cc + /app/stubs/string.c, `--partial-loops --unwind 5`,
`--replace-call-with-contract editorInsertRow --enforce-contract editorOpen`,
`cbmc --depth 200`) but its mutation kill score is **0/9** and cannot be raised there.

**Verifying contract** (requires/assigns are the live part):
- `is_fresh(filename, 8)` + `filename[7]=='\0'` (bounds-tracked NUL-terminated name),
- `is_fresh(E.filename, 1)` (old name is a real dynamic object → free is exercised),
- `is_fresh(E.row, sizeof(erow))` (live object for editorInsertRow's replaced contract),
- `assigns(E.dirty, E.filename, E.numrows, E.row, object_whole(E.row), object_whole(E.filename))`,
  `frees(E.filename, E.row)`. Freeing E.filename needs BOTH `object_whole(E.filename)` in
  assigns AND it in frees (same pattern editorInsertRow uses for E.row) — with only one of
  them you get `assigns.* POINTER_OBJECT(E.filename) is assignable: FAILURE`.
- ensures `return_value==0||==1` and `E.dirty==0` (both TRUE but vacuous, see below).

**Why 0/9 (all three reasons confirmed empirically with assert(0) probes):**
1. **Body past the early allocations is unreachable within `--depth 200`.** `assert(0)`
   right after `E.filename = malloc(fnlen)` SUCCEEDS (unreachable); after `free` it FAILS
   (reachable). So strlen→malloc→memcpy→the getline loop are all depth-truncated. Hence
   every postcondition is vacuous: falsifying ensures to `return_value==999` (or `!=1`, hit
   on the short return-1 path) still SUCCEEDS. Same `--depth 200` vacuity as
   [[cbmc-depth-vacuous-and-recursive-callees]] / [[cbmc-isfresh-mutation-kills]]. Raising
   depth isn't an option (harness-fixed) and `cbmc --depth 1000` crashes with an internal
   "invariant violation" on the editorInsertRow-replaced binary.
2. **The getline `line` buffer is untracked.** `getline` has no body, so `line` is a nondet
   object of unbounded extent; `line[linelen-1]` and `line[linelen+1]` both pass all
   pointer/bounds checks. The 6 loop-body mutants (`==`↔`!=`, `&&`↔`||`, `[len-1]`↔`[len+1]`)
   and the loop-condition flip (`!=-1`→`==-1`) change only dead/untracked computation.
3. **The `if(!fp)` error path self-prunes.** `exit(1)` is modeled no-return (assume false),
   so the `errno != ENOENT` → `==` mutant (M4) changes only which pruned path is taken; the
   return value (1 either way) isn't observed (vacuous postcondition). The strlen `+1`→`-1`
   mutant (M7) would overflow `fnlen` to SIZE_MAX only when strlen==0, but malloc(SIZE_MAX)
   self-prunes, so memcpy's source-readable check never fires.

Net: the spec is as strong as it can be (tight is_fresh preconditions, exact assigns/frees,
true postconditions), but the canonical harness structurally cannot score it — analogous to
editorUpdateRow. Helper script: /app/kilo/run_mutants_editorOpen.sh (replays the canonical
pipeline per mutant; reports 0/9).
