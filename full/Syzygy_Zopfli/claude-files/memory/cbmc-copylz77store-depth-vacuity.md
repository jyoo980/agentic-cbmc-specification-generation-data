---
name: cbmc-copylz77store-depth-vacuity
description: ZopfliCopyLZ77Store verifies soundly but kill 0/18; 17 is_fresh objects exhaust depth 200 before any loop
metadata: 
  node_type: memory
  type: project
  originSessionId: 37832ab8-8519-482c-a0cf-baa7bc32aa71
---

ZopfliCopyLZ77Store (zopfli.c) verifies SOUNDLY but kill 0/18.

**Why:** A malloc-only body (the only `free` is inside callee ZopfliCleanLZ77Store, which has a frees/was_freed contract and is replaced — so no in-body free crash, like [[cbmc-zopflialloc-malloc-only-verifies]]). But the precondition needs ~17 `__CPROVER_is_fresh` objects: source + dest structs, 7 source payload/histogram arrays, 7 dest array pointers (each required fresh so ZopfliCleanLZ77Store can free them). That exhausts the depth-200 wall ([[cbmc-depth-200-object-limit]]) before any of the three copy loops run. Confirmed vacuous via `__CPROVER_ensures(1==0)` probe — it still "verifies", so the function exit is unreachable.

**How to apply:** Strong sound contract is in place: is_fresh source arrays sized by source->size, dest arrays fresh for the clean, full functional postconditions (size, data, forall litlens/dists/pos copied). Survivors are the two histogram loops (5448/5452, llsize=288*ceil & dsize=32*ceil, always ≥288/32 for size≥1 → exceed unwind 5) and the main copy loop (5440) plus the `||` alloc-failure checks. Pinning source->size==3 won't help — too many is_fresh objects (heavier than [[cbmc-storelitlendist-depth-vacuity]] at 8 which already hit the wall). Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h. Distinct forall varnames required (k/m/n).
