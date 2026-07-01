---
name: cbmc-initstats-verifies
description: InitStats verifies soundly with full 4-array forall-zero postcondition; no mutable operators
metadata: 
  node_type: memory
  type: project
  originSessionId: 8b3446a0-0607-49cb-9618-d07dac000950
---

`InitStats(SymbolStats *stats)` in zopfli.c verifies SOUNDLY with `is_fresh(stats)`
+ `assigns` all 4 arrays + four forall-zero ensures (litlens/dists/ll_symbols/d_symbols).
Pure 4x memset body → mutation reports "no mutable operators" (like [[cbmc-copystats-verifies]]).
Modeled on [[cbmc-clearstatfreqs-unwind-survivors]] zero-postcondition.
Needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs` for FILE.h.
Distinct forall varnames (a/b/c/d) required.
