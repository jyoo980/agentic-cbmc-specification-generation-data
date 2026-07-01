---
name: gbl-fptr-param-crash
description: "GetBestLengths is unverifiable (fptr-param rc134 crash); confirmed empirically, 118 mutants all uncheckable"
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dfd20d9-1302-411e-9835-ce18fb0e4942
---

GetBestLengths in zopfli.c (~line 3929) takes `CostModelFun *costmodel`, so it
hits the same structural CBMC 6.9.0 crash as [[gcmmc-fptr-param-crash]]. Confirmed
empirically 2026-06-27: the scoring pipeline aborts at the **cbmc** step with
`Invariant check failed ... __CPROVER__start::costmodel$object was not found`
(Function Pointer Removal pass), rc134. goto-cc / --add-library / --partial-loops
--unwind 5 / --enforce-contract all succeed; only cbmc aborts.

avocado generates **118 mutants** but they are all uncheckable — the unmutated
original already crashes, so every mutant scores identically. Kill score is 0 and
unimprovable without `--dfcc`/`__CPROVER_obeys_contract` (not in the fixed harness).

**How to apply:** Don't chase kills. Left the strongest SOUND spec: `requires(costmodel
!= NULL)`, `requires(instart <= inend)`, `requires(h != NULL)`, `ensures(instart ==
inend ==> return == 0)`. NO `assigns()` clause — unlike GetCostModelMinCost (pure
reads), GetBestLengths writes length_array/costs/*h, so an empty assigns would be
unsound; omitting it makes no false framing claim. Verify script: /app/_verify_gbl.sh.
Same applies to GetBestLengths2.
