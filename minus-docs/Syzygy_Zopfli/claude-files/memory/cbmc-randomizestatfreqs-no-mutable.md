---
name: cbmc-randomizestatfreqs-no-mutable
description: "RandomizeStatFreqs verifies with \"no mutable operators\"; is_fresh OK on inline arrays passed to callee here"
metadata: 
  node_type: memory
  type: project
  originSessionId: 937e049d-1278-404a-a5b5-db9c114bc776
---

`RandomizeStatFreqs` (zopfli.c) verifies NON-vacuously and mutation testing reports **"no mutable operators"** — strongest possible outcome (CopyStats/InitStats class). Body = two contract-replaced `RandomizeFreqs` calls + constant `litlens[256]=1`; nothing mutable. Spec: `is_fresh(state)`, `is_fresh(stats)`, assigns state seeds + `object_whole(stats->litlens/dists)`, ensures `litlens[256]==1` (proven → body reached, non-vacuous).

**Why notable:** unlike [[cbmc-calculatestatistics-inline-array-isfresh]] (where passing inline struct-member arrays to a callee requiring is_fresh on them FAILED the precondition), here CBMC ACCEPTS `is_fresh` on `stats->litlens`/`stats->dists` as sub-objects of the fresh `stats` parent. Difference: callee `RandomizeFreqs` requires `is_fresh(freqs)` and it discharged fine. Related: [[cbmc-randomizefreqs-ran-replacement-vacuity]] (the callee itself is 0/8 vacuous).
