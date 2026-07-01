---
name: zcbs-no-mutants
description: "ZopfliCleanBlockState — conditional-free func, avocado generates 0 mutants, kill score inherently 0"
metadata: 
  node_type: memory
  type: project
  originSessionId: 7676366a-7359-422f-8772-0ee4fea601cd
---

`ZopfliCleanBlockState` in /app/Syzygy_Zopfli/c_code/zopfli.c verifies (VERIFICATION SUCCESSFUL).

get-mutants reports "No mutant(s) generated (no mutable operators)" — the body is just `if (s->lmc) { ZopfliCleanCache(s->lmc); free(s->lmc); }`, no mutable operators. Kill score is inherently 0; don't chase it. Same situation as [[zcc-no-mutants]] and [[getdynamiclengths-no-mutants]].

**Spec:** requires is_fresh of s, s->lmc, and all three inner cache ptrs (length/dist/sublen); frees() all four ptrs; assigns object_whole of all four; ensures was_freed(old(...)) of all four. Verify with `--replace-call-with-contract ZopfliCleanCache` plus `--enforce-contract ZopfliCleanBlockState`. Script: /app/_verify_zcbs.sh
