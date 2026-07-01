---
name: cbmc-tryoptimizehuffmanforrle-depth-vacuity
description: TryOptimizeHuffmanForRle verifies soundly but kill 0; depth-200 exhausted by 4 is_fresh arrays + 6 replaced callee contracts
metadata: 
  node_type: memory
  type: project
  originSessionId: d2f2e1c7-b67c-4e57-9443-2eafaff7db98
---

`TryOptimizeHuffmanForRle` in `Syzygy_Zopfli/c_code/zopfli.c` verifies soundly
but mutation kill = 0 (all 9 mutants on the `treesize2+datasize2 < treesize+datasize`
comparison and the returned sums survive).

**Why:** the depth-200 wall (see [[cbmc-depth-200-object-limit]]). Its contract
requires four large is_fresh arrays — ll_counts(288)+d_counts(32) size_t and
ll_lengths(288)+d_lengths(32) unsigned = 640 elements — plus it calls six
contract-bearing callees that get replaced (CalculateTreeSize, CalculateBlock-
SymbolSizeGivenCounts, OptimizeHuffmanForRle, ZopfliCalculateBitLengths,
PatchDistanceCodesForBuggyDecoders). The is_fresh + replaced-contract setup
exhausts depth 200 before control reaches the return/ensures. Confirmed with the
`__CPROVER_ensures(1 == 0)` probe — still "verifies".

**How to apply:** kept a sound contract (requires lstart==0 && lend>=ZOPFLI_NUM_LL*3,
the four is_fresh arrays, CBSGC value-bound foralls reused from
CalculateBlockSymbolSizeGivenCounts, assigns object_whole on both length arrays,
ensures return_value >= (14+4*3)+CBSGC_RESULT_MIN). Don't chase the kill score —
it's the same depth wall as the sibling CalculateBlockSymbolSize* / Add* funcs.

Note: although the body calls `ZopfliCalculateBitLengths(...,ZOPFLI_NUM_LL=288,...)`
while that callee's contract requires n<=ZLLCL_MAX_N=8, this does NOT cause a
failure here — the depth wall hits during setup before that call's precondition
is checked. Run needs `-I /app/Syzygy_Zopfli/stubs` (FILE.h stub).
