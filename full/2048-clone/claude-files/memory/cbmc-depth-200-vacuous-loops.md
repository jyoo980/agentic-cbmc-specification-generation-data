---
name: cbmc-depth-200-vacuous-loops
description: "run-cbmc's fixed --depth 200 can make specs vacuously pass (0 kill score) for functions with long scan loops"
metadata: 
  node_type: memory
  type: project
  originSessionId: e017aad1-097e-407d-b760-5e3e72aefb46
---

`run-cbmc` runs `cbmc --depth 200` (hardcoded `_CBMC_DEPTH` in `tools/run_cbmc_and_mutation_testing.py`, with `--partial-loops --unwind 5`). `--depth` bounds steps along the executed path. A function whose body lies past step 200 has its postconditions reported SUCCESS *vacuously* — the violating paths are unreachable within the bound, so mutation testing yields a 0.0 kill score even for a correct, strong spec.

**Seen in:** `2048-clone/2048.c::addRandom`. Its 4x4 double scan loop (16 iterations) plus the is_fresh/old harness costs ~340 path-steps to reach the `board[x][y]=n` write; under depth 200 the write is never reached. Confirmed correct + has teeth by running the same goto-binary with NO `--depth` limit: all 17 postconditions pass and a deliberately-false probe (`board[0][0]==__CPROVER_old(board[0][0])`) FAILS. Precondition style (is_fresh vs w_ok) barely changes the threshold; the loop traversal is the cost and is irreducible without editing the (off-limits) C code.

**How to diagnose:** if kill score is 0 despite a strong spec, replicate the pipeline manually (see `docs/running-cbmc-directly.md`) and re-run `cbmc` with no `--depth`. If a false postcondition then fails, the depth bound — not the spec — is the ceiling. Do NOT hardcode depth/unwind into specs (project rule). See [[stubs-dir-for-external-callees]].

**Faster in-harness diagnostic (no manual pipeline):** temporarily replace ALL ensures with one always-false clause, e.g. `__CPROVER_ensures(board[0][0] == (uint8_t)(board[0][0] + 1))`, and run `run-cbmc`. If it still reports "Verified", the postconditions are unreachable within depth 200 → strengthening ensures cannot raise the kill score. Restore afterward.

**Seen again:** `2048-clone/2048.c::rotateBoard` (90°-CCW in-place rotation, nested loop). `goto-instrument --unwind 5` unrolls the nested loop structurally to 5×5=25 inner bodies; their symex exceeds 200 steps, so even the FIRST ensures is never reached (always-false probe still "verified"). Full 16-cell permutation spec `board[i][j]==__CPROVER_old(board[j][SIZE-1-i])` is correct but vacuously checked. Kill score capped at 6/29 (0.2069), coming ENTIRELY from the `__CPROVER_assigns(__CPROVER_object_whole(board))` frame catching the earliest out-of-object writes (is_fresh gives a tight 16-byte object, so out-of-bounds-row write mutants in the first iterations get killed); later-iteration and within-object index mutants survive. Reordering/strengthening ensures is futile here. Loop invariants don't help: the pipeline never passes `--apply-loop-contracts`, so they'd be ignored.
