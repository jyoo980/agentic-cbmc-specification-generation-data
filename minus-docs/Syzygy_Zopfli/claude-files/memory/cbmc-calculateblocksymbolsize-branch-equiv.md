---
name: cbmc-calculateblocksymbolsize-branch-equiv
description: CalculateBlockSymbolSize in zopfli.c verifies but scores 0/6 — all survivors are equivalent branch-selector mutants
metadata: 
  node_type: memory
  type: project
  originSessionId: 78847409-1902-4d0d-bcf6-9e9cd746c3d3
---

`CalculateBlockSymbolSize` (zopfli.c ~line 1521) verifies with the union-of-callees
precondition (ZopfliLZ77GetHistogram + CalculateBlockSymbolSizeGivenCounts/Small)
and postcondition `return_value >= ll_lengths[256]`, but mutation kill score is 0/6.

All 6 survivors are on the branch condition `if (lstart + ZOPFLI_NUM_LL * 3 > lend)`
(RELATIONAL > -> <,<=,>=,==,!= and ARITHMETIC + -> -). These are **equivalent
mutants**: the small-block path and histogram path compute the SAME value, and
`CalculateBlockSymbolSizeGivenCounts` re-checks the identical condition (line ~326)
and delegates back to `...Small` for small ranges, so flipping the selector cannot
change the observable result. Empty range returns `ll_lengths[256]` on both paths.

Cannot pin an exact-value postcondition to kill them: the CBMC harness leaves
`lz77->ll_symbol`/`d_symbol` unconstrained vs `litlens`/`dists`, so the two paths
are not provably equal and an exact-value spec would fail on the original. Same
class as [[cbmc-calculatetreesize-minselect-unkillable]]. Do not re-attempt to
raise this kill score.
