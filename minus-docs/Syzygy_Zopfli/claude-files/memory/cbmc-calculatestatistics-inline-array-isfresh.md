---
name: cbmc-calculatestatistics-inline-array-isfresh
description: "CalculateStatistics can't verify — callee is_fresh on inline struct-member arrays (sub-objects) is unsatisfiable"
metadata: 
  node_type: memory
  type: project
  originSessionId: e23f221f-14b7-4872-9d5c-8c8346500d32
---

`CalculateStatistics` (zopfli.c ~line 4369) does not verify. It calls `ZopfliCalculateEntropy(stats->litlens, NUM_LL, stats->ll_symbols)` and `(stats->dists, NUM_D, stats->d_symbols)`. Failures are exactly `ZopfliCalculateEntropy.precondition.3` (line 4381) and `.6` (line 4382) — both the `__CPROVER_is_fresh(bitlengths, ...)` requires of the callee, checked under contract replacement.

**Root cause (new class):** `SymbolStats` embeds its arrays *inline* (`size_t litlens[288]; double ll_symbols[288]; ...`), so `stats->ll_symbols` / `stats->d_symbols` are **sub-object pointers** into the single `stats` object. `__CPROVER_is_fresh` on a mid-struct sub-object FAILS as a replacement-side assertion (requires a distinct freshly-malloc'd region). The read-only `count` sub-arrays pass (litlens at offset 0), but the writable `bitlengths` outputs fail.

Distinct from the usual `lz77->litlens` pattern, which works because *those* members are separately-allocated pointers, not inline arrays.

**Why unfixable:** the callee genuinely needs `is_fresh` for its OWN standalone verification (it writes `object_whole(bitlengths)`; only is_fresh allocates that object on enforce). Swapping to `w_ok`/`r_ok` would admit sub-objects on replace but break the callee's enforcement (no allocation). Contradictory; can't weaken a working callee ([[cbmc-calculateentropy]] verifies 5/29) to chase the caller.

Strong sound spec left in place: requires `is_fresh(stats, sizeof(SymbolStats))`, assigns `object_whole(stats)`, ensures `forall k<NUM_LL: ll_symbols[k] in [0,64]` and `forall j<NUM_D: d_symbols[j] in [0,64]` (mirrors callee postcondition). Related: [[cbmc-calculateentropy]].
