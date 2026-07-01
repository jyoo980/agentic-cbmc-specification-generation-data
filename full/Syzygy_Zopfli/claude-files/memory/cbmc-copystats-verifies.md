---
name: cbmc-copystats-verifies
description: CopyStats verifies with full 4-array forall postcondition; no mutable operators; forall varname + __CPROVER_old gotchas
metadata: 
  node_type: memory
  type: project
  originSessionId: 1b94ca2e-e593-4e55-ae55-7605f0b6df1e
---

`CopyStats` (zopfli.c, SymbolStats *source/*dest) verifies SOUNDLY with a full
functional postcondition: requires both is_fresh(sizeof(SymbolStats)); assigns
all four dest arrays (litlens, dists, ll_symbols, d_symbols); four
`__CPROVER_forall` ensures each asserting `dest[i] == source[i]` over its array
extent. Mutation testing reports "no mutable operators" (body is four memcpy
calls, no mutable arithmetic/comparison) — like [[cbmc-updatehashvalue-no-mutable-operators]]
and [[cbmc-zopfliinitlz77store-no-mutable-operators]]. So no kill score; spec is
maximally strong for this body.

Two build gotchas:
- Each `__CPROVER_forall` clause needs a DISTINCT bound-variable name; reusing
  `li`/`di` across clauses → "redeclaration of ... with no linkage" CONVERSION ERROR.
- Do NOT wrap a quantified-index expression in `__CPROVER_old` — `__CPROVER_old(source->litlens[a])`
  gave spurious `array_bounds dynamic object upper bound` FAILUREs. Since source is
  unmodified, reference `source->litlens[a]` directly in the postcondition.

Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.
