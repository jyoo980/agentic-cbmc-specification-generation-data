---
name: copystats-no-mutants
description: CopyStats is a 4-memcpy SymbolStats copier; avocado generates 0 mutants (kill score inherently 0); verifies with exact element-equality postcondition
metadata: 
  node_type: memory
  type: project
  originSessionId: 2fe9475e-8b35-4c4c-be55-0d07dfc660d9
---

CopyStats (Syzygy_Zopfli/c_code/zopfli.c, ~line 5011) byte-copies 4 SymbolStats
tables source→dest: litlens[288], dists[32] (size_t) and ll_symbols[288],
d_symbols[32] (double) via memcpy.

get-mutants reports "No mutant(s) generated (no mutable operators)" —
kill score inherently 0, nothing to chase. Same family as [[zilz77s-no-mutants]],
[[zclz77s-no-mutants]], [[cs-no-mutants]].

**How to apply:** verify SUCCESSFUL @ --depth 200 with the strongest exact-copy
spec — `is_fresh(source,sizeof(*source))` + `is_fresh(dest,sizeof(*dest))` (also
makes the memcpys non-overlapping/in-bounds) + `assigns(object_whole(dest))` +
four `ensures forall { int kN; ... ==> dest->X[kN] == source->X[kN] }` (one per
table, DISTINCT quantifier vars k1..k4 — reusing `k` is a CONVERSION ERROR
"redeclaration of ...::k"). The `==` on the double tables verifies fine here (no
NaN issue under is_fresh). Contract goes AFTER the `(...)` declarator, BEFORE `{`.
Verified via /app/_verify_copystats.sh (goto-cc + --enforce-contract).
