---
name: cbmc-blocksplitlz77-vacuity
description: "ZopfliBlockSplitLZ77 verifies but 0/53; ~12 is_fresh objects + 4 foralls exceed depth 200, plus an unsatisfiable size>=10 vs callee size<=8 conflict"
metadata: 
  node_type: memory
  type: project
  originSessionId: 84961117-881a-4cae-8e0f-fc3e24784d78
---

`ZopfliBlockSplitLZ77` (zopfli.c) verifies but kills 0/53 mutants — depth-200
is_fresh vacuity. ~12 is_fresh objects (options, lz77 + its 7 column arrays, npoints,
splitpoints, *splitpoints) plus 4 __CPROVER_forall preconditions (needed because the
body forwards lz77 to the contract-replaced EstimateCost, whose precondition demands
all columns fresh) exceed --depth 200; a false ensures (*npoints==999999) still
"Verifies" at depth 200, proving the body is unreachable.

Extra wrinkle vs other vacuity cases: even raising the depth couldn't give a
non-vacuous functional proof — the loop is reached only when lz77->size >= 10 (the
early-return guard), but the contract-replaced FindLargestSplittableBlock requires
lz77size <= 8 (see [[cbmc-findlargestsplittableblock-vacuity]]), so that precondition
would be asserted-false at the call site. The two callee contracts are mutually
exclusive at this call. Strong sound spec left in place.

Same family as [[cbmc-depth200-isfresh-vacuity]]. Note this function's own top-level
malloc(lz77->size)/free(done) did NOT crash goto-instrument (unlike the
[[cbmc-addsorted-malloc-body-crash]] class) — depth exhaustion hits first.
