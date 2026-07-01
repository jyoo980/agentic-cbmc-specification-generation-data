---
name: cbmc-initstats-no-mutable-operators
description: "InitStats in zopfli.c verifies non-vacuously with \"no mutable operators\" (memset-only body)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 8c111275-ca50-43a5-8df3-036fc82a171b
---

`InitStats(SymbolStats *stats)` in `Syzygy_Zopfli/c_code/zopfli.c` (~line 4616) verifies NON-vacuously and mutation testing reports "no mutable operators" — strongest possible outcome. The body is four memset(...,0,...) calls (litlens/dists = size_t arrays, ll_symbols/d_symbols = double arrays), nothing to mutate. Spec mirrors [[cbmc-copystats-no-mutable-operators]]: `is_fresh(stats)`, assigns the four arrays, four __CPROVER_forall ensures that every element == 0 (doubles compare == 0 fine since memset zero bytes = IEEE 0.0).
