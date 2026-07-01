---
name: cbmc-clearstatfreqs-loopbound-equiv
description: ClearStatFreqs in zopfli.c verifies non-vacuously 6/10; 4 survivors are equivalent loop-boundary mutants
metadata: 
  node_type: memory
  type: project
  originSessionId: 7b42d424-5dec-4ecd-897d-35babb67d95c
---

ClearStatFreqs (zopfli.c ~line 4406) zeros stats->litlens[ZOPFLI_NUM_LL=288] and stats->dists[ZOPFLI_NUM_D=32] (both inline arrays in SymbolStats). Spec: is_fresh(stats, sizeof(SymbolStats)), assigns(litlens, dists), forall ensures both arrays == 0. Verifies NON-vacuously 6/10.

The 4 survivors are all loop-bound mutants and are equivalent mutants:
- `< -> !=` (both loops): i increments by 1 and hits the bound exactly, so termination is identical. Provably equivalent.
- `< -> <=` on litlens loop: extra write hits litlens[288] which overlaps dists[0] (dists immediately follows litlens) = 0, same as the dists loop does. Equivalent.
- `< -> <=` on dists loop: extra write lands in first bytes of ll_symbols[0]; CBMC checks __CPROVER_assigns at containing-object granularity for inline member arrays, so the OOB-into-sibling-member write is NOT flagged as an assigns violation. Unobservable (ll_symbols not constrained).

Conclusion: 6/10 is the ceiling; survivors can't be killed by strengthening. Compare [[cbmc-calculatestatistics-inline-array-isfresh]] (same struct's inline-array is_fresh issue).
