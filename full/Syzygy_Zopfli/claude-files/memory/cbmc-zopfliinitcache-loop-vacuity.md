---
name: cbmc-zopfliinitcache-loop-vacuity
description: "ZopfliInitCache verifies soundly but kill 0/16; sublen loop (24*blocksize iters) exceeds unwind 5, making function exit unreachable"
metadata: 
  node_type: memory
  type: project
  originSessionId: d8a48677-7470-49d9-b827-5763a69ab79c
---

`ZopfliInitCache` in Syzygy_Zopfli/c_code/zopfli.c verifies but kill 0/16; all 16 mutants are loop-bound/NULL-check operators that survive.

**Why:** The body's third init loop runs `ZOPFLI_CACHE_LENGTH * 3 * blocksize` = `24*blocksize` iterations (≥24 even for blocksize==1), far above the harness unwind cap of 5. CBMC's loop unwinding makes the function's normal exit unreachable — proven directly: `__CPROVER_ensures(0)` VERIFIES. So every postcondition (even `length[ka]==99` or `false`) passes vacuously and no mutant is distinguishable. Pinning `blocksize==4` does NOT help (the sublen loop still overshoots). Same loop-bound/vacuity ceiling family as [[cbmc-harness-ignores-loop-contracts]], [[cbmc-getfixedtree-loopbound-vacuity]].

**How to apply:** Don't chase kill score here. Left a sound, strong functional spec: requires is_fresh(lmc) & blocksize>0; assigns length/dist/sublen; ensures is_fresh on the three malloc'd arrays + foralls (length==1, dist==0, sublen==0). Build needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs` (FILE.h stub). Each forall must use a DISTINCT bound-var name (ka/kb/kc) or goto-cc errors "redeclaration with no linkage".
