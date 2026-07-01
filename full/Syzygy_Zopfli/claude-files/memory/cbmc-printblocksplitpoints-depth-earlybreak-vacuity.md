---
name: cbmc-printblocksplitpoints-depth-earlybreak-vacuity
description: PrintBlockSplitPoints verifies soundly but kill 0/24; ZOPFLI_APPEND_DATA depth wall + early-break loop-bound equivalence
metadata: 
  node_type: memory
  type: project
  originSessionId: f85d63ba-3bde-478a-a21b-ba2768278453
---

`PrintBlockSplitPoints` in /app/Syzygy_Zopfli/c_code/zopfli.c verifies soundly
but kill score is 0/24.

Contract: is_fresh(lz77) + pin lz77->size==2 + is_fresh(litlens/dists at size) +
pin nlz77points==1 + is_fresh(lz77splitpoints) + lz77splitpoints[0] < size +
empty `__CPROVER_assigns()` (only locals + stderr written). Needs `-I
/app/Syzygy_Zopfli/stubs` for `<x86_64-linux-gnu/bits/types/FILE.h>`.

Two independent reasons no mutant dies:
1. Depth wall — like [[cbmc-addsorted-append-depth-vacuity]] and
   [[cbmc-tracebackwards-depth-vacuity]], the ZOPFLI_APPEND_DATA macro's
   malloc/realloc/memset goto-machinery + 4 is_fresh objects exhaust fixed
   --depth 200 before the post-append print loops / exit are reached.
2. Early-break equivalence — the scan loop breaks as soon as
   `npoints == nlz77points`, so it never runs to its `i == lz77->size` boundary;
   loop-bound mutants (5406 `i < lz77->size`, 5411 print loop) stay
   behaviourally equivalent regardless of depth.

Cannot be raised without --depth (harness fixes it). Survivors are all
RELATIONAL mutants on loop/branch conditions.
