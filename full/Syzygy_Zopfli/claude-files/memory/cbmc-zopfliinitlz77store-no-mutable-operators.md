---
name: cbmc-zopfliinitlz77store-no-mutable-operators
description: "ZopfliInitLZ77Store verifies with full field postcondition; pure initializer, no mutable operators"
metadata: 
  node_type: memory
  type: project
  originSessionId: 280fc202-3b2d-43c9-8fd8-282dc0e41943
---

`ZopfliInitLZ77Store(data, store)` in zopfli.c verifies successfully with a full
functional contract: `is_fresh(store, sizeof(*store))` require, `assigns(*store)`,
and 9 `ensures` pinning every field (size/litlens/dists/pos/ll_symbol/d_symbol/
ll_counts/d_counts == 0, data == data).

Mutation reports "no mutable operators" — body is pure constant/pointer
assignments with no arithmetic/comparison operators to mutate, so kill is N/A and
the spec is already maximal. Same shape as [[cbmc-cleanlz77store-no-mutable-operators]]
and [[cbmc-cleanblockstate-no-mutable-operators]].

**Build:** needs both `-I <repo>/Syzygy_Zopfli/stubs` (provides
x86_64-linux-gnu/bits/types/FILE.h stub) and `-I c_code` (absolute paths).
