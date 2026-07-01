---
name: cbmc-addweighedstatfreqs-loop-depth-vacuity
description: AddWeighedStatFreqs in zopfli.c verifies but 0/12; the 288-iter litlens FP loop exceeds the depth bound before postconditions are reached
metadata: 
  node_type: memory
  type: project
  originSessionId: d73d78e2-716b-43b6-b919-c257c2e561f9
---

`AddWeighedStatFreqs` (zopfli.c) verifies non-vacuously-looking but kills 0/12.
Distinct from the is_fresh-count cases: vacuity is driven by the **288-iteration
first loop** (`for i < ZOPFLI_NUM_LL` with `(size_t)(stats1->litlens[i]*w1 +
stats2->litlens[i]*w2)` FP arithmetic). Execution is truncated mid-first-loop by
the depth bound, so neither the dists postcondition nor litlens[256] check is
ever reached. Proven: a false `__CPROVER_ensures(result->dists[0] == 999999)`
still "verified successfully".

Loop contracts don't help: per [[cbmc-loop-contracts-ignored]] invariants are
silently dropped, and adding `__CPROVER_loop_invariant`/`__CPROVER_assigns` here
made the function unparseable ("Function missing from file") — the
[[cbmc-treesitter-loopcontract-misparse]] failure mode. No non-vacuous middle
ground. Strong sound spec (3 is_fresh SymbolStats, per-index forall blend
postconditions excluding index 256, litlens[256]==1) left in place.

Note `result` aliases `stats1` in the real call
(`AddWeighedStatFreqs(&stats,1.0,&laststats,0.5,&stats)`); per-index read-then-
write at the same index keeps it correct, but is_fresh forces distinctness so
that case isn't covered.
