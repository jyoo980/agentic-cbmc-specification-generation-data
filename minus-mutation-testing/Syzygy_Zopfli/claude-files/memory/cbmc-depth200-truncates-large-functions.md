---
name: cbmc-depth200-truncates-large-functions
description: run-cbmc runs cbmc with --depth 200; large functions get trivial SUCCESS because assertions are never reached
metadata: 
  node_type: memory
  type: reference
  originSessionId: 90a95bfc-422a-4e8e-88a4-594b6d15cecf
---

`run-cbmc` (tools/run_cbmc.py) runs the final cbmc step with
`--depth 200 --unwind 5` (constants `_CBMC_DEPTH=200`, `_CBMC_UNWIND=5`). The
`--depth` bound counts symex steps along a path and **silently drops paths that
exceed it** — CBMC then reports VERIFICATION SUCCESSFUL because it never reached
the violated assertion.

For a large function (e.g. `ZopfliDeflatePart` in Syzygy_Zopfli), the
contract-enforcement harness's own setup (allocating all `is_fresh` requires
objects, nested `**out`, `object_whole` havocs) consumes the 200-step budget
**before the body / first callee call site is reached**. Consequence: the tool
returns exit 0 for *any* contract — even contradictory `requires`, a wrong
`*bp==42` postcondition, or a deliberately violated callee precondition. The
"pass" is real per the tool config but **vacuous / depth-truncated**, not a proof.

Detect: temporarily add an impossible postcondition (e.g. `__CPROVER_ensures(*bp == 42)`)
or violate the first callee's precondition; if the tool still says "verified
successfully", the run is depth-truncated.

Confirm/diagnose at full depth by re-running the *already-instrumented* binary
the tool leaves behind, WITHOUT `--depth`:
```
cbmc checking-<FUNC>-contracts.goto --function <FUNC>
```
This reaches the assertions (e.g. `[AddNonCompressedBlock.precondition.18] ...: FAILURE`)
and gives the real verdict. You can't change `--depth` from a spec (it's a
CLI-arg value — forbidden by CLAUDE.md), so the official tool will still report
success regardless; use the full-depth run only to author a genuinely sound spec.

Related: the orchestrator `ZopfliDeflatePart` builds its `ZopfliLZ77Store`
dynamically (NULL buffers after `ZopfliInitLZ77Store`, grown by realloc), but
every consumer callee contract (`ZopfliLZ77Optimal`, `AddLZ77BlockAutoType`,
`ZopfliAppendLZ77Store`, ...) requires pre-allocated `is_fresh` buffers with
forall invariants. The init/append/blocksplit postconditions are too weak to
re-establish those, so the btype 1/2 paths are NOT dischargeable under modular
replacement at full depth — a contract-modeling gap, not fixable without editing
the C code. See [[cbmc-isfresh-malloc-vacuous-pass]].
