---
name: pbsp-depth200-vacuity
description: "PrintBlockSplitPoints verifies but 8/24 kills @depth200; maximally strong (24/24 @260+), dfcc floor blocks rest, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: ac336d05-13de-43e2-b198-cd1b94b09c84
---

PrintBlockSplitPoints (/app/Syzygy_Zopfli/c_code/zopfli.c) verifies @depth200 with an
r_ok-based regime spec (lz77->size==1, dists[0]==0, nlz77points==1, splitpoints[0]==0).

Kill score is **8/24 @depth200** and this is the ceiling at the official scoring depth.
Proven maximally strong: **24/24 @depth260+** (no equivalent mutants — even the
main-loop bound mutants and the two print-loop batches all die once reachable).

The frontier is sharp: 8 kills @240, 24 kills @260. The 16 survivors are all spec
points sitting *after* the `ZOPFLI_APPEND_DATA` realloc/malloc step (the closing
`assert(npoints==nlz77points)`, the `break` test, and the two print loops). The
~250-step frontier ≈ the documented ~245 [[zvld-depth200-vacuity]] dfcc
enforce-contract floor for this codebase, so it's the contract machinery's intrinsic
floor, not a weak spec. No requires/regime lever pulls those asserts under 200.

The 8 kills @200 are exactly the cheap paths that *don't* reach the append:
guard mutants that skip the loop, `match !=` (no append → empty-loop assert fails),
and `length !=0` (reads unconstrained lz77->litlens → OOB memory-safety kill — keep
litlens WITHOUT r_ok, it's a free kill; adding r_ok would lose it).

**Don't chase.** Spec is optimal. Scripts: /app/_verify_pbsp.sh, /app/kill_pbsp.py
(takes depth arg). Same class as [[avocado-depth200-vacuity]].
