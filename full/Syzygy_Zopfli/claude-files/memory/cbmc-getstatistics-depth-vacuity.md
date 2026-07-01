---
name: cbmc-getstatistics-depth-vacuity
description: GetStatistics verifies soundly but kill 0/6; SymbolStats is_fresh depth wall
metadata: 
  node_type: memory
  type: project
  originSessionId: e5c4ec50-7a6e-414d-b08f-fc756807ade4
---

`GetStatistics` (zopfli.c ~5175) verifies but kill 0/6 — depth-200 vacuous
(probing with `__CPROVER_ensures(1==0)` still "verifies successfully").

Only 6 mutants exist, all RELATIONAL: 1 on the branch `store->dists[i]==0`
(==→!=) and 5 on the loop bound `i < store->size` (<→<=,>,>=,==,!=).

Even pinning `store->size==2` with concrete entries (entry0 literal litlen 100;
entry1 length litlen 10→symbol 264, dist 5→symbol 4), requiring `stats` cleared,
and writing exact bucket postconditions (litlens[100]==1, litlens[264]==1,
dists[4]==1, litlens[256]==1, plus forall "no other bucket touched") — all
survive. The 2-iteration loop is tiny, so the wall is object footprint, not
unwind: `is_fresh(stats, sizeof(SymbolStats))` is a ~5KB struct (litlens[288],
dists[32], ll_symbols[288], d_symbols[32]) and the replaced
`CalculateStatistics` contract re-asserts `is_fresh(stats)` — exhausts depth 200
before exit. Same SymbolStats wall as [[cbmc-calculatestatistics-n1-callee-wall]]
and [[cbmc-addweighedstatfreqs-depth-vacuity]]. Needs `-I /app/Syzygy_Zopfli/stubs`
for FILE.h; distinct forall varnames required. Left the strong concrete-pin
contract documented. The `< → !=` loop-bound mutant is genuinely equivalent
(count-up from 0) and unkillable regardless.
