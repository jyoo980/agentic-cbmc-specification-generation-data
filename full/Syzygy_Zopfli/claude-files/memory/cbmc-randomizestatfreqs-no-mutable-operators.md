---
name: cbmc-randomizestatfreqs-no-mutable-operators
description: "RandomizeStatFreqs verifies soundly; pure dispatch, no mutable operators"
metadata: 
  node_type: memory
  type: project
  originSessionId: 8598cb49-235c-4be6-a6a6-a266d75d73d7
---

`RandomizeStatFreqs(RanState*, SymbolStats*)` in Syzygy_Zopfli/c_code/zopfli.c verifies SOUNDLY (not vacuously) with: is_fresh(state)+is_fresh(stats) requires; assigns state->m_z/m_w + object_whole(stats->litlens) + object_whole(stats->dists); ensures stats->litlens[256]==1 (the unconditional end-symbol write). Mutation reports "no mutable operators" — body is pure dispatch (two [[cbmc-randomizefreqs-depth-vacuity]] RandomizeFreqs calls + one constant assignment).

**Why:** Passing interior pointers stats->litlens / stats->dists (sub-arrays of a fresh SymbolStats) to the callee's is_fresh(freqs,...) precondition did NOT trigger a disjointness failure — it verified. Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.

**How to apply:** For pure-dispatch wrappers, the right move is a constant-write postcondition (litlens[256]==1) + correct assigns; expect "no mutable operators", which is a clean result like [[cbmc-initstats-verifies]] / [[cbmc-copystats-verifies]].
