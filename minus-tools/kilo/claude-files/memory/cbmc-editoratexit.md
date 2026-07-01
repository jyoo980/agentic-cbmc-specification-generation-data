---
name: cbmc-editoratexit
description: editorAtExit verifies with assigns(E.rawmode)+ensures(E.rawmode==0); 0 mutants (maxed); non-vacuous at depth 200 via disableRawMode contract
metadata: 
  node_type: memory
  type: project
  originSessionId: 75fa1929-5866-4a50-ae74-235835d8e382
---

`editorAtExit(void)` is a thin wrapper that just calls `disableRawMode(STDIN_FILENO)`.

Contract: `__CPROVER_assigns(E.rawmode)` + `__CPROVER_ensures(E.rawmode == 0)`, mirroring [[cbmc-disablerawmode]]'s own ensures (disableRawMode clears E.rawmode unconditionally).

- **0 mutants** — `get-mutants` prints "No mutant(s) generated (no mutable operators)". Kill score is structurally 0/0, already maxed.
- **Verifies canonically** (depth 200): in-file callee disableRawMode replaced via `--replace-call-with-contract`; needs `stubs/rawmode.c` (provides tcsetattr that disableRawMode calls).
- **Non-vacuous**: unlike many depth-200 cases, the disableRawMode contract-replacement is cheap enough that the ensures IS reached — flipping editorAtExit's ensures to `E.rawmode==1` (while keeping disableRawMode's at ==0) gives VERIFICATION FAILED. Confirmed real postcondition check.
- Gotcha: `sed 's/E.rawmode == 0)/.../'` hits BOTH disableRawMode's and editorAtExit's ensures (separate lines) — use a perl scoped to the `void editorAtExit` block for a clean non-vacuity test.
- No `run-cbmc` binary exists; replicate the pipeline manually (goto-cc with rawmode.c → `--partial-loops --unwind 5` → `--replace-call-with-contract disableRawMode --enforce-contract editorAtExit` → `cbmc --depth 200`).
