---
name: cbmc-zopflicleanhash-no-mutable-operators
description: "ZopfliCleanHash verifies with 7-free frees/was_freed contract; pure free dispatch, no mutable operators"
metadata: 
  node_type: memory
  type: project
  originSessionId: f8635c9c-aac1-4712-9feb-53308d0657f5
---

`ZopfliCleanHash(ZopfliHash *h)` in Syzygy_Zopfli/c_code/zopfli.c frees 7 fields
(head, prev, hashval, head2, prev2, hashval2, same). Verifies with the same pattern
as [[cbmc-cleanlz77store-no-mutable-operators]] / [[cbmc-cleanblockstate-no-mutable-operators]]:
is_fresh(h) + is_fresh on each freed field, `__CPROVER_assigns()`, `__CPROVER_frees(...)`,
and a `was_freed(old(field))` ensures per field. The 7 in-body frees did NOT crash
goto-instrument (unlike the in-body-free-deallocate crash cases).

Mutation result: "no mutable operators" (pure free dispatch) — kill N/A, best achievable.

**Build note:** needs `-I /app/Syzygy_Zopfli/stubs` for the `<x86_64-linux-gnu/bits/types/FILE.h>` include.
