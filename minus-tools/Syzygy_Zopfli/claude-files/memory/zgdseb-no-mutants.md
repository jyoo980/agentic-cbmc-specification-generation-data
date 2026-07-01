---
name: zgdseb-no-mutants
description: ZopfliGetDistSymbolExtraBits has no mutants (pure table lookup); kill score inherently 0
metadata: 
  node_type: memory
  type: project
  originSessionId: 70567f5d-1a14-4b19-9b6c-c4e410f4c26f
---

`ZopfliGetDistSymbolExtraBits(int s)` in /app/Syzygy_Zopfli/c_code/zopfli.c is a
pure `return table[s]` lookup (static const int table[30]). `get-mutants`
reports "No mutant(s) generated ... (no mutable operators)".

**Why:** No arithmetic/relational operators in the body means avocado has nothing
to mutate, so kill score is inherently 0 — there is nothing to chase, unlike the
equivalent-mutant ceilings in [[zgds-equivalent-mutant]] / [[zgdebv-equivalent-mutants]].

**How to apply:** Don't try to raise the kill score. Just write a strong
exact-value contract and verify. The verifying spec: `requires s>=0 && s<=29`,
`assigns()`, `ensures s<=3 ==> rv==0`, `ensures s>=4 ==> rv==(s-2)/2`, plus global
range `rv in [0,13]`. Verifies SUCCESSFUL via `./verify.sh ZopfliGetDistSymbolExtraBits`
(no callees). Sibling `ZopfliGetLengthSymbolExtraBits` has the same table-lookup shape.
