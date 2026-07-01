---
name: initstats-no-mutants
description: "InitStats zeroes 4 SymbolStats tables; avocado 0 mutants, kill score inherently 0"
metadata: 
  node_type: memory
  type: project
  originSessionId: c01f41fc-7938-4f08-8d13-afcede6d9902
---

`InitStats` in /app/Syzygy_Zopfli/c_code/zopfli.c is a pure struct zero-initializer
(4 memset-to-0 over litlens[288]/dists[32]/ll_symbols[288]/d_symbols[32]).

avocado generates 0 mutants (no mutable operators) → kill score inherently 0, don't chase.

Verifies @depth200 with the [[copystats-no-mutants]] idiom: `is_fresh(stats)` +
`assigns(object_whole(stats))` + 4 exact-value forall ensures (each table element == 0,
distinct quantifier vars k1..k4). == on double tables (ll_symbols/d_symbols) is fine.
