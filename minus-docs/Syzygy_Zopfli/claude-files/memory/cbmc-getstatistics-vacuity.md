---
name: cbmc-getstatistics-vacuity
description: GetStatistics in zopfli.c verifies but 0/6; depth-200 vacuity + CalculateStatistics object_whole havoc make histogram unobservable
metadata: 
  node_type: memory
  type: project
  originSessionId: c5529272-8bbd-41f8-af15-2f259b3a2dc2
---

GetStatistics (zopfli.c ~line 4386) verifies but scores 0/6. All 6 survivors are
loop/branch relational mutants on the histogram loop (`i < store->size`, `dists[i] == 0`).

Two compounding reasons the histogram is unobservable:
1. Depth-200 vacuity — 4 is_fresh objects (store struct, store->litlens,
   store->dists, stats) + the histogram loop push the body past `--depth 200`.
   Proven: a false `__CPROVER_ensures(stats->litlens[256] == 999999)` still
   "verified successfully".
2. The callee CalculateStatistics has `assigns(__CPROVER_object_whole(stats))`
   and only ensures ll_symbols/d_symbols in [0,64] — so even if reachable, it
   havocs the litlens/dists counts and the entropy outputs are input-independent.
   No postcondition can observe the loop's histogram writes through it.

Same family as [[cbmc-trygetfromlmc-vacuity]], [[cbmc-calculatestatistics-inline-array-isfresh]].
Strong sound spec left in place (is_fresh store/litlens/dists/stats, forall guards
litlens<288 for literals, litlens<=258 & 1<=dists<=32768 for matches).
