---
name: cbmc-initblockstate-superset-mutant
description: ZopfliInitBlockState in zopfli.c verifies but 0/1 kill — sole survivor is an enlarging-superset mutant on the InitCache blocksize arg
metadata: 
  node_type: memory
  type: project
  originSessionId: 5b2a952e-14da-4ec7-8d53-fdf1ba513fb7
---

`ZopfliInitBlockState` (zopfli.c) verifies non-vacuously with a full functional contract
(is_fresh(s); add_lmc⇒blockend-blockstart in [1, max_malloc/(CACHE_LENGTH*3)]; assigns(*s);
ensures exact options/blockstart/blockend; add_lmc⇒lmc fresh + length[qa]==1, dist[qb]==0
for qa,qb < blockend-blockstart; else lmc==NULL).

Kill score 0/1. Sole survivor: line `ZopfliInitCache(blockend - blockstart, s->lmc)`
mutated `-` → `+`. The callee `ZopfliInitCache` is **inlined** (not contract-replaced) in
this harness, so the mutant initializes a strict SUPERSET of cache indices
[0, blockend+blockstart) vs original [0, blockend-blockstart). Functional postconditions
over indices < blockend-blockstart are satisfied by both — the mutant does strictly more
init and cannot violate them. Killing it would need the callee precondition checked at the
call site (contract replacement), which this harness does not do. Equivalent-class mutant,
not a spec weakness. Related: [[cbmc-calculateblocksymbolsize-branch-equiv]],
[[cbmc-calculatetreesize-minselect-unkillable]].
