---
name: cbmc-deflatepart-vacuity
description: "ZopfliDeflatePart verifies but 0/44; top-level driver depth-200 vacuity, no goto-instrument crash despite in-body malloc/free"
metadata: 
  node_type: memory
  type: project
  originSessionId: 7a71f8c4-d4e1-4062-89a1-9fcafce712e5
---

`ZopfliDeflatePart` in zopfli.c VERIFIES but kills 0/44 mutants — the depth-200 vacuity family ([[cbmc-depth200-isfresh-vacuity]], [[cbmc-blocksplit-vacuity]], [[cbmc-lz77optimal-vacuity]], [[cbmc-lz77greedy-vacuity]]). The interface (options/in/bp/outsize/out/*out + output-buffer invariant) plus immediate forwarding into the heaviest contract-replaced callees (ZopfliBlockSplit, ZopfliLZ77Optimal, ZopfliCalculateBlockSizeAutoType, ZopfliAppendLZ77Store, ZopfliBlockSplitLZ77, AddLZ77BlockAutoType) exhausts --depth 200 before the split/cost/emit loops run. Also unsatisfiable callee preconds under contract replacement (InitLZ77Store size==0 vs Optimal/Append size==3; NULL *splitpoints vs fresh).

All 44 surviving mutants live in the unreachable loop bodies (`splitpoints[i-1]`→`i+1`, `&&`→`||` block-split guards, final emit loop).

NOTABLE: despite top-level in-body `malloc` (splitpoints array) and several `free()` calls, goto-instrument did NOT crash under --enforce-contract — unlike the malloc/free-body-crash family ([[cbmc-addsorted-malloc-body-crash]], [[cbmc-copylz77store-malloc-body-crash]], [[cbmc-lz77optimalrun-free-body-crash]]). So in-body malloc/free is not always fatal; the crash needs an unsized freed pointer or a top-level should_malloc_fail site that goto-instrument can't size.

Strong sound spec left in place. Use `-I /tmp/cbmc-inc` ([[cbmc-aarch64-fileh-stub]]).
