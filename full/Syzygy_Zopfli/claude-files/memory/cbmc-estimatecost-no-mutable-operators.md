---
name: cbmc-estimatecost-no-mutable-operators
description: EstimateCost verifies as pure dispatcher; no mutable operators
metadata: 
  node_type: memory
  type: project
  originSessionId: 5f5f2a44-28d5-4861-8948-952beed1f993
---

`EstimateCost` (Syzygy_Zopfli/c_code/zopfli.c, ~line 2166) verifies successfully but mutation reports "no mutable operators" — its body is a single pure forward call `return ZopfliCalculateBlockSizeAutoType(lz77, lstart, lend);`.

**Contract:** mirror [[cbmc-calculateblocksizeautotype-depth-vacuity]]'s replaced preconditions verbatim (is_fresh lz77 + pos/dists/litlens sized 2, lstart<=lend<=size, lend<=2, position/litlen <=100000 bounds), plus `__CPROVER_assigns()` and `__CPROVER_ensures(__CPROVER_return_value >= 0)`.

**Build:** needs `-I /app/Syzygy_Zopfli/stubs` for the FILE.h include.

Same pattern as [[cbmc-getdynamiclengths-no-mutable-operators]] — pure call dispatch, nothing to mutate.
