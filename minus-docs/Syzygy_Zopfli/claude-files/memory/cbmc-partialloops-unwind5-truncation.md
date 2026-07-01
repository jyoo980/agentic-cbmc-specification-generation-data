---
name: cbmc-partialloops-unwind5-truncation
description: run-cbmc unwinds loops only 5x with --partial-loops; postconditions referencing array indices beyond the unwind bound give spurious counterexamples
metadata: 
  node_type: memory
  type: project
  originSessionId: 2a3b9842-0698-4ac3-a2e2-216bb71cde2e
---

run-cbmc instruments with `goto-instrument --partial-loops --unwind 5` (then `cbmc --depth 200`). With `--partial-loops` a loop body runs at most 5 iterations and then **falls through with no unwinding assertion** — so a `for (i=0;i<30;i++)` scan effectively only examines indices 0..4, yet the post-loop code still executes.

Consequence: a postcondition that reasons about array indices NOT reached within the unwind bound (e.g. a preservation/"already had ≥2 nonzero in [0..29]" clause over zopfli.c `PatchDistanceCodesForBuggyDecoders`) fails with a *real* CBMC counterexample even though it is true for the full 30-iteration C semantics. This is NOT depth vacuity (which yields spurious SUCCESS) — it yields a genuine FAILURE model.

Fixes: (a) restrict the spec to indices always visited (here, indices 0 and 1 are read on the first two iterations regardless of unwind — "never overwrite a nonzero d_lengths[0]/[1]" verifies and raised kill score 0.42→0.67), or (b) use loop contracts so the loop is summarized over all iterations instead of unrolled. Never hardcode the unwind bound in a spec. Related: [[cbmc-depth200-isfresh-vacuity]].
