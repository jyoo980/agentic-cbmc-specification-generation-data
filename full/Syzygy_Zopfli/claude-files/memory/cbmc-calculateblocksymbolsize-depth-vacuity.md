---
name: cbmc-calculateblocksymbolsize-depth-vacuity
description: "CalculateBlockSymbolSize dispatcher verifies soundly but kill 0/6; depth-200 wall, all mutants on the branch condition"
metadata: 
  node_type: memory
  type: project
  originSessionId: aa56d477-2380-40d2-ac1a-81add3d02ef8
---

`CalculateBlockSymbolSize` in /app/Syzygy_Zopfli/c_code/zopfli.c is a thin
dispatcher (small branch -> CalculateBlockSymbolSizeSmall; else
ZopfliLZ77GetHistogram + CalculateBlockSymbolSizeGivenCounts). Gave it the same
precondition shape as sibling [[cbmc-getdynamiclengths-no-mutable-operators]] /
GetDynamicLengths: `lstart==0 && lend>=ZOPFLI_NUM_LL*3` to drive the histogram
branch, full is_fresh on lz77 + 5 internal arrays + ll_lengths/d_lengths, the two
symbol foralls, ll/d length bounds (>=1, <CBSGC_LEN_BOUND), `__CPROVER_assigns()`,
and postcondition `return_value in [CBSGC_RESULT_MIN, CBSGC_RESULT_BOUND]`.

Verifies, but **kill 0/6** — all 6 mutants are on the branch line
`lstart + ZOPFLI_NUM_LL*3 > lend` (5 relational + 1 arithmetic). Same depth-200
vacuity wall as [[cbmc-zopflilz77gethistogram-depth-vacuity]] and
[[cbmc-calculateblocksymbolsizegivencounts]] siblings: the is_fresh setup exhausts
depth before control reaches the branch/return, so the postcondition is never
checked and every mutant passes vacuously too. Contract is sound, kept as-is.

Build note: needs `-I /app/Syzygy_Zopfli/stubs` for the FILE.h stub.
