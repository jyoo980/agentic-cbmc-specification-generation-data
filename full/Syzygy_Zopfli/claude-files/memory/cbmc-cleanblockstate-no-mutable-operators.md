---
name: cbmc-cleanblockstate-no-mutable-operators
description: "ZopfliCleanBlockState verifies with frees-contract; body is pure free dispatch so mutation reports \"no mutable operators\""
metadata: 
  node_type: memory
  type: project
  originSessionId: 31ad6bbf-0a65-44d3-9f2c-c5cb22e76e5b
---

`ZopfliCleanBlockState` (zopfli.c) verifies cleanly with a frees/was_freed contract:
requires is_fresh(s, s->lmc, and lmc->length/dist/sublen), assigns(), frees all four,
ensures was_freed(old(s->lmc)). Needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs`
(FILE.h stub).

Mutation: "no mutable operators" — body is only `if (s->lmc){ ZopfliCleanCache(s->lmc); free(s->lmc); }`,
no arithmetic/comparison operators to mutate. Spec is as strong as possible. Like [[cbmc-getdynamiclengths-no-mutable-operators]].

NOTE: in-body `free(s->lmc)` did NOT crash goto-instrument here, unlike [[cbmc-inbody-free-deallocate-crash]]
(that crash was specific to free inside a __CPROVER_deallocate-using contract).
