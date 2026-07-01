---
name: cbss-depth200-vacuity
description: "CalculateBlockSymbolSize 0/6 kills under hardcoded cbmc --depth 200, inherent vacuity not weak spec"
metadata: 
  node_type: memory
  type: project
  originSessionId: d71f2b4d-16a7-4701-85a6-94f87864f370
---

`CalculateBlockSymbolSize` (Syzygy_Zopfli/c_code/zopfli.c) is a thin dispatcher: `if (lstart + ZOPFLI_NUM_LL*3 > lend)` → CalculateBlockSymbolSizeSmall, else ZopfliLZ77GetHistogram + CalculateBlockSymbolSizeGivenCounts. Verifies at --depth 200 with a union-of-callees contract (5 mandatory `is_fresh`: ll_lengths, d_lengths, lz77, lz77->litlens, lz77->dists, + token-validity forall) and ensures `>= ll_lengths[256]`.

All 6 avocado mutants are on the branch guard. Sibling of [[cbssgc-depth200-vacuity]] / [[cbsss-depth200-vacuity]].

**Why 0/6 under --depth 200 (don't chase):**
- The 5 `is_fresh` are all required to discharge the replaced callees' own `is_fresh` requires — can't drop or swap to `r_ok` (r_ok makes the CBSSS is_fresh obligation FAIL verification; tested). is_fresh prologue pushes the in-branch guard-image asserts to a depth-330 frontier (SUCCESS@320, FAILED@350).
- Callees CBSSS+CBSSGC both require `lz77->size <= 3` → caller must require it → `lend <= 3` → else (histogram) branch unreachable AND the boundary `lstart+3*NUM_LL == lend` unreachable. So `>=`, `!=`, and the `lstart - ...` mutant are **equivalent** (differ from `>` only at that boundary) — unkillable at ANY depth.
- The `<`, `<=`, `==` mutants divert all states into the else arm; the else-arm `__CPROVER_assert(lstart+3*NUM_LL <= lend)` kills them, but only at depth ≥350 (vacuous @200).

Theoretical max 3/6 (only `<`,`<=`,`==`) and only above depth ~320; under the mandated --depth 200 it is inherently 0/6. Kill script: /app/kill_cbss.sh. See [[avocado-depth200-vacuity]].

**2026-06-27 re-confirm + new failed attempt:** still verifies SUCCESSFUL, still 0/6. Tried tightening `lz77->size <= 3` → `<= 1` to shrink the is_fresh(litlens/dists) + token-forall prologue and pull the else-arm assert under depth 200 — still 0/3 on the divert mutants (`<`,`<=`,`==`). The depth bottleneck is the FIXED-SIZE is_fresh on ll_lengths (288 entries) + d_lengths (32 entries), mandated by the callee contracts and not shrinkable. Window size is irrelevant. Don't retry size-tightening either.
