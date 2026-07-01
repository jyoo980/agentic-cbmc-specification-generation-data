---
name: cbmc-splitcost-double-isfresh-vacuity
description: "SplitCost verifies soundly but kill 0/1; body calls EstimateCost twice on same lz77, second is_fresh unsat -> exit unreachable"
metadata: 
  node_type: memory
  type: project
  originSessionId: 3804c4bf-ede3-45df-bf5f-49e3430c2b28
---

`SplitCost` (zopfli.c ~2195) body is
`EstimateCost(c->lz77, c->start, i) + EstimateCost(c->lz77, i, c->end)`.
EstimateCost's replaced contract requires `is_fresh(lz77)` plus `is_fresh` on
`lz77->pos/dists/litlens`. Calling it TWICE on the same `lz77` means the second
call asserts `is_fresh` on objects already claimed by the first call -> that
assertion is unsatisfiable -> the path after the second call is infeasible ->
SplitCost's exit is unreachable. Verifies soundly with a strong contract
(is_fresh(context)+is_fresh(lz77)+start<=i<=end<=2, end<=size, the two
pos/litlens conditionals, assigns(), ensures return>=0) but `ensures(1==0)`
ALSO passes -> vacuous. Single mutant (+ -> -) unkillable because return never
reached. Needs `-I /app/Syzygy_Zopfli/stubs`.

This is the [[cbmc-is-fresh-gotchas]] double-is_fresh wall: a function that
calls the same is_fresh-requiring callee twice on one object is undischargeable
to a non-vacuous value spec. Distinct from [[cbmc-findminimum-funptr-boundarypm-crash]]
(SplitCost reached via funptr, not verified directly).

**How to apply:** kill 0/1 is the ceiling; left the strong (vacuous-on-value)
contract in place. Cannot fix without changing EstimateCost's contract or the
body (both forbidden).
