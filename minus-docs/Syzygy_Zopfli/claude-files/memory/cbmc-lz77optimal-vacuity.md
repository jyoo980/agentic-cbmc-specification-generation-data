---
name: cbmc-lz77optimal-vacuity
description: ZopfliLZ77Optimal in zopfli.c verifies but 0/28; depth-200 + unsatisfiable InitLZ77Store-vs-Greedy/OptimalRun callee preconds make body unreachable
metadata: 
  node_type: memory
  type: project
  originSessionId: 6e31e2de-041e-4751-95b4-a1f02825b5fe
---

`ZopfliLZ77Optimal` (zopfli.c) VERIFIES but kills 0/28 mutants — vacuous at the
harness's fixed `--depth 200`. Top-level twin of [[cbmc-lz77optimalfixed-vacuity]]
plus the refinement loop. Two independent causes, both confirmed:

1. Depth: ~18 is_fresh objects (s, s->lmc, 3 lmc arrays, in, store + its 7 backing
   arrays) + 2 cache foralls exhaust depth before the loop body/postconditions.
2. Unsatisfiable callee preconds under contract replacement (same as
   [[cbmc-blocksplit-vacuity]] family): body does `ZopfliInitLZ77Store(&currentstore)`
   (ensures size==0, NULL arrays) immediately before `ZopfliLZ77Greedy` and
   `LZ77OptimalRun` on `&currentstore`, each of which REQUIRES size==3 + fresh size-4
   arrays + cleared histograms → path infeasible.

Confirmed vacuity: false ensures `instart == 424242` still "Verifies" at depth 200.

Strong sound spec left in place: s/lmc/in block mirrors ZopfliLZ77Greedy/LZ77OptimalRun
verbatim (blockstart<=instart, lmc arrays span inend-blockstart, length==1/dist==0);
`store` only consumed by `ZopfliCopyLZ77Store(&currentstore, store)` which tears down
dest, so store's 7 arrays must be freeable (required fresh) + a `__CPROVER_frees` on
them; numiterations>=1; inend malloc bound. See [[cbmc-depth200-isfresh-vacuity]].
