---
name: cbmc-calculatestatistics-n1-callee-wall
description: CalculateStatistics undischargeable; callee ZopfliCalculateEntropy pinned n==1 but called with 288/32
metadata: 
  node_type: memory
  type: project
  originSessionId: 54144ddf-fd98-4e52-8e2f-00959fa08bf7
---

`CalculateStatistics` (zopfli.c ~line 5148) is undischargeable due to a callee-precondition wall.

It calls `ZopfliCalculateEntropy(stats->litlens, ZOPFLI_NUM_LL, stats->ll_symbols)` (n=288)
and `ZopfliCalculateEntropy(stats->dists, ZOPFLI_NUM_D, stats->d_symbols)` (n=32).

`ZopfliCalculateEntropy` is pinned `requires(n == 1)` (see [[cbmc-calculateentropy-n1-exact-cancel]])
so its two loops fully unwind for a sound proof of `bitlengths[0]==0`. In contract-replacement
mode CBMC asserts that requires at each call site → `288==1`/`32==1` are statically false →
`[ZopfliCalculateEntropy.precondition.1] ... FAILURE` at both calls. No caller-side spec can fix this.

**Why:** Relaxing the callee to symbolic n makes ITS proof vacuous (--partial-loops truncates the
288-iter loops, `bitlengths[0]==0` no longer holds), so the callee must stay pinned. Verification
of the caller reports 4 failures: precondition.1 (n==1) and precondition.3 (is_fresh bitlengths)
at each of the two calls. is_fresh(count) (.2) passed; the n==1 ones are the irreducible wall.

**How to apply:** Left documented with a strong-intent contract (is_fresh(stats), assigns
ll_symbols/d_symbols whole, ensures both [0]==0) that does not verify — acceptable per task rules.
Needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs` (FILE.h stub) to even compile.
Don't waste retries trying to satisfy n==1 from the caller.
