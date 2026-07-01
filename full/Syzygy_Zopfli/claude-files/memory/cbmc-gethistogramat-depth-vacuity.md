---
name: cbmc-gethistogramat-depth-vacuity
description: ZopfliLZ77GetHistogramAt verifies soundly but kill 0 — depth-200 wall + unwind-5 vs 288-iter copy loops
metadata: 
  node_type: memory
  type: project
  originSessionId: 0c5f172d-299c-4100-a755-7274982ad17d
---

`ZopfliLZ77GetHistogramAt` in /app/Syzygy_Zopfli/c_code/zopfli.c: wrote a sound contract (is_fresh on the store + its 7 internal arrays at symbolic sizes — ll_counts/d_counts sized by llpos/dpos = ZOPFLI_NUM_LL*(lpos/ZOPFLI_NUM_LL) etc., ll_symbol/d_symbol/dists sized by lz77->size; is_fresh on the two output histograms; two symbolic-range forall preconditions bounding ll_symbol<288 / d_symbol<32; assigns object_whole of both outputs; conditional exact-equality ensures guarded by `lpos+1==lz77->size`).

Verifies but kill = 0 (all 41 mutants survive). Confirmed vacuous: `__CPROVER_ensures(1 == 0)` also "verifies". Same depth-200 wall as the sibling [[cbmc-calculateblocksymbolsizesmall-depth-vacuity]] / [[cbmc-tryoptimizehuffmanforrle-depth-vacuity]] / Add* cases — the 8 is_fresh allocations + 2 symbolic foralls exhaust depth during setup before reaching the loops.

**Why:** even past the depth wall the two copy loops run 288/32 iterations while the harness uses `--partial-loops --unwind 5` without `--apply-loop-contracts` (see [[cbmc-harness-ignores-loop-contracts]]), so the exact-equality ensures could never discharge anyway. Kill cannot be raised under this pipeline.

**How to apply:** don't burn retries trying to raise kill on these pointer-heavy lz77 histogram/blocksize functions — write a sound contract, confirm via 1==0 that it's depth-vacuous, document inline, move on.

Build notes: needs `-I /app/Syzygy_Zopfli/stubs` (for the x86_64-linux-gnu FILE.h stub include in zopfli.c). Each forall across the whole contract must use a UNIQUE bound-variable name (reusing `i` → "redeclaration ... with no linkage"). Avoid `*/` inside block comments (e.g. "Size*/Add*") — it closes the comment early → PARSING ERROR.
