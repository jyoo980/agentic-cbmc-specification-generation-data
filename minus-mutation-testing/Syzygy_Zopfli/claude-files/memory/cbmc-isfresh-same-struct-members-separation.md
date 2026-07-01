---
name: cbmc-isfresh-same-struct-members-separation
description: A caller cannot satisfy a callee contract that demands is_fresh on two pointers when it can only pass two members of the same struct
metadata: 
  node_type: memory
  type: reference
  originSessionId: 954c3cb2-bf61-4cab-8510-e1e478d17a8a
---

Under `--replace-call-with-contract`, `__CPROVER_is_fresh(p, size)` in a callee's
requires clause asserts **separation** at OBJECT granularity: each is_fresh
pointer must reference a distinct allocation. If a caller passes two members of
the SAME struct allocation to two is_fresh parameters, the first is_fresh passes
(nothing to be separate from) but the second FAILS separation — even though the
byte ranges are disjoint.

Concrete case: `CalculateStatistics` (Syzygy_Zopfli/c_code/zopfli.c) calls
`ZopfliCalculateEntropy(stats->litlens, ..., stats->ll_symbols)` and
`(stats->dists, ..., stats->d_symbols)`. The callee required
`is_fresh(count)` AND `is_fresh(bitlengths)`. Since litlens/ll_symbols (and
dists/d_symbols) are inline members of one `SymbolStats`, they share one object,
so precondition checks .3 and .6 failed.

RESOLUTION (2026-06-25, this is a SPEC fix, not a C-code change, so it is
allowed): weaken the callee's requirement on the SECOND pointer from
`__CPROVER_is_fresh(bitlengths, n*sizeof(*bitlengths))` to
`__CPROVER_w_ok(bitlengths, n*sizeof(*bitlengths))`. `w_ok` (resp. `r_ok`/`rw_ok`)
checks validity/writability WITHOUT imposing object separation, so the caller
passing two members of one struct discharges it. Keep `is_fresh` on the first
pointer (one fresh object still materialises the allocation). Verify the callee
still passes (it did — its body's per-element clamp makes the postcondition hold
even under aliasing), then the caller. Both `ZopfliCalculateEntropy` and
`CalculateStatistics` verify at exit 0 after this. Caveat: the caller still
carries `is_fresh(stats)` so its proof is vacuous-pass under
[[cbmc-isfresh-malloc-vacuous-pass]] (negated postcondition still passes), but
the call-site precondition discharge is genuine (the original is_fresh failure
was a real, reachable `2 of 24711 failed`). Related:
[[cbmc-isfresh-malloc-vacuous-pass]].
