---
name: cbmc-splitcost-vacuity
description: SplitCost verifies but 0/1; depth-200 vacuity (9 is_fresh + 4 foralls) makes the body unreachable
metadata: 
  node_type: memory
  type: project
  originSessionId: 32e50805-ec22-43dd-8e4a-7779c668727a
---

`SplitCost` in zopfli.c verifies but kills 0/1. Sole survivor is the `+ -> -`
ARITHMETIC mutant on `EstimateCost(...,start,i) + EstimateCost(...,i,end)`.

Proof of vacuity: the existing `__CPROVER_ensures(return_value >= 0)` would
already reject the subtraction mutant (a non-negative sum becomes a possibly
negative difference) if the body were reachable. It survives ⇒ the postcondition
is never evaluated. Confirmed: a false `ensures(return_value == -999999.0)` still
"verifies successfully".

Cause: the spec must establish [[cbmc-getcostmodelmincost-fnptr-param]]-adjacent
EstimateCost's full precondition chain — context + lz77 + 7 array is_fresh
objects (litlens/dists/pos/ll_symbol/d_symbol/ll_counts/d_counts) = 9 is_fresh +
4 __CPROVER_forall. This setup exceeds --depth 200 so the body (two EstimateCost
calls) is unreachable. Same family as [[cbmc-findlargestsplittableblock-vacuity]]
and the EstimateCost forwarder to [[cbmc-calculateblocksizeautotype-vacuity]].

Strong sound spec left in place (start<=i<=end<=size for both call ranges, full
is_fresh chain, foralls over [start,end)). Context fields accessed via repeated
`((SplitCostContext *)context)->...` casts since the param is `void *context`.
