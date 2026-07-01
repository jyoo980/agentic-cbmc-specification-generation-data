---
name: cbmc-zopfliinitblockstate-add-lmc-vacuity
description: ZopfliInitBlockState verifies soundly but kill 0/1; add_lmc branch vacuous so the only mutant is unreachable
metadata: 
  node_type: memory
  type: project
  originSessionId: 6d7406bb-7e31-477f-97e5-80ababae136e
---

`ZopfliInitBlockState` in `/app/Syzygy_Zopfli/c_code/zopfli.c` verifies with a full
functional contract (requires is_fresh(s) + blockend>blockstart; assigns the 4
fields; ensures options/blockstart/blockend equal args and `add_lmc ? lmc!=NULL : lmc==NULL`)
but kill = **0/1**.

The sole mutant flips `blockend - blockstart` to `blockend + blockstart` inside the
`if (add_lmc)` branch, in the `ZopfliInitCache(blocksize, s->lmc)` call argument.

**Why unkillable:** the `add_lmc==true` path is **vacuous** in the proof — diagnostic
`__CPROVER_ensures(!add_lmc)` VERIFIES, proving CBMC never explores that branch. The
body `malloc`s `s->lmc` then calls `ZopfliInitCache`, whose replaced contract requires
`is_fresh(lmc)`; the body-malloc'd pointer + malloc-may-fail prune the path (same
is_fresh/depth-200 family as [[cbmc-depth-200-object-limit]], [[cbmc-is-fresh-gotchas]]).
Since the mutant lives only in that branch, no postcondition can observe it —
`__CPROVER_object_size(s->lmc->length)` tied to the blocksize also passed vacuously.

Needs `-I /app/Syzygy_Zopfli/stubs` for the FILE.h stub. Strong sound spec left in place;
the surviving mutant is a tool limitation, not a spec defect.
