---
name: cbmc-cleanlz77store-no-mutable-operators
description: "ZopfliCleanLZ77Store verifies with 7-free frees/was_freed contract; pure free dispatch, no mutable operators; did NOT crash goto-instrument"
metadata: 
  node_type: memory
  type: project
  originSessionId: 4d16e98f-b05b-4a81-8b80-5e1c5fbead53
---

`ZopfliCleanLZ77Store` (zopfli.c, ~line 3755) verifies cleanly: is_fresh(store) + is_fresh
on all 7 pointer fields (litlens, dists, pos, ll_symbol, d_symbol, ll_counts, d_counts),
`__CPROVER_assigns()`, `__CPROVER_frees(...all 7...)`, and `was_freed(old(...))` for each.
Needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs`.

Mutation: "no mutable operators" — body is 7 straight `free(store->field)` calls, no
arithmetic/comparison. Strongest possible spec. Like [[cbmc-cleanblockstate-no-mutable-operators]].

NOTE: 7 direct in-body `free()` calls did NOT crash goto-instrument, unlike
[[cbmc-inbody-free-deallocate-crash]] (ZopfliCleanCache). So the deallocate crash is NOT a
reliable consequence of in-body free + frees-contract; structurally identical functions can
discharge fine. Try the contract before assuming undischargeable.
