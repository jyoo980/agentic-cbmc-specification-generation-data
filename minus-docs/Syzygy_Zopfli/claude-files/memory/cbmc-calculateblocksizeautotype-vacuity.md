---
name: cbmc-calculateblocksizeautotype-vacuity
description: "ZopfliCalculateBlockSizeAutoType in zopfli.c verifies but 0/21; depth-200 vacuity from 8 is_fresh objects, and min-select mutants are unobservable under contract replacement anyway"
metadata: 
  node_type: memory
  type: project
  originSessionId: 0b728a14-95a2-4c1d-9141-5d28e08982ca
---

`ZopfliCalculateBlockSizeAutoType` (zopfli.c ~line 1662) verifies but 0/21 kills.
It forwards lz77/lstart/lend to three `ZopfliCalculateBlockSize` calls (btype 0/1/2)
and returns the minimum. Spec mirrors that callee's preconditions (8 is_fresh
objects + DEFLATE forall bounds); sound postcondition is `return_value >= 0`
(only uncompressedcost/btype 0 has a >=0 floor).

Vacuous: a false `__CPROVER_ensures(return_value >= 1000000000)` still "Verifies"
→ body unreachable. The 8 is_fresh objects re-checked across 3 contract-replaced
calls exceed --depth 200 (same family as [[cbmc-appendlz77store-vacuity]],
[[cbmc-storelitlendist-vacuity]]).

Even if non-vacuous, the 21 survivors (min-select `&&`->`||`, and the `size>1000`
branch-selector relational mutants at line ~1700) are unobservable: under contract
replacement each call returns a nondeterministic double constrained only by its
lower bound, so any selection logic still yields some value >= 0 — same
min-select-under-replacement limit as [[cbmc-calculatetreesize-minselect-unkillable]].
Strong sound spec left in place.
