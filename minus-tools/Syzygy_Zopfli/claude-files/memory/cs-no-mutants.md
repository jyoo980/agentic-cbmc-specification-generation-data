---
name: cs-no-mutants
description: CalculateStatistics has no mutants (kill score inherently 0); verifies via weak ZopfliCalculateEntropy stub
metadata: 
  node_type: memory
  type: project
  originSessionId: 193dd740-26ec-4b6a-b30e-5a96457859f7
---

CalculateStatistics (zopfli.c) calls ZopfliCalculateEntropy twice on SymbolStats
fields: (litlens, 288, ll_symbols) and (dists, 32, d_symbols). avocado generates
0 mutants (no mutable operators), so kill score is inherently 0 — don't chase kills.

**Why a weak callee stub is required:** the real ZopfliCalculateEntropy contract
pins `n == ZOPFLI_NUM_LL` (288), so replace-call fails on the dists call (n=32);
and it uses is_fresh on its pointer params, which fails because the args are
struct *fields* (not separate allocations). Wrote /app/stubs/calcstats_weak.c:
weak sound model for any n>0 using r_ok(count)/w_ok(bitlengths) (work on fields)
+ `assigns object_upto(bitlengths, n*sizeof)` + `ensures bitlengths[0]>=0`.

**How to apply:** verify via /app/_verify_calcstats.sh — non-dfcc enforce flow
(like [[ec-no-mutants]]): remove-function-body ZopfliCalculateEntropy, link weak
stub, `--enforce-contract CalculateStatistics --replace-call-with-contract
ZopfliCalculateEntropy`, cbmc --depth 200. CRITICAL: stub assigns must be
`object_upto` NOT `object_whole` — object_whole resolves to the whole stats
struct, so the dists call re-havocs ll_symbols and the ll_symbols[0]>=0
postcondition fails. Caller contract: is_fresh(stats) + assigns both symbol
arrays + ensures ll_symbols[0]>=0 && d_symbols[0]>=0. VERIFICATION SUCCESSFUL.
Related to [[zce-nondet-log-vacuity]] (the real callee).
