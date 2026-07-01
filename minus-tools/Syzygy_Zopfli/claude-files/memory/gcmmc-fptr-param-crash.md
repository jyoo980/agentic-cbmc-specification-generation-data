---
name: gcmmc-fptr-param-crash
description: GetCostModelMinCost (and any function-pointer-param func) is unverifiable — CBMC aborts in the enforce-contract harness
metadata: 
  node_type: memory
  type: project
  originSessionId: f34236ba-ebd1-4bd4-ba20-f9922a55d006
---

GetCostModelMinCost(CostModelFun *costmodel, void *costcontext) in zopfli.c cannot
be verified by the avocado scoring harness. The official pipeline
(`tools/run_cbmc.py`: goto-cc → `goto-instrument --partial-loops --unwind 5` →
`goto-instrument --enforce-contract` → `cbmc --depth 200`, NO dfcc, NO loop
contracts) aborts at the **cbmc** step with rc 134:

`Invariant check failed ... identifier __CPROVER__start::costmodel$object was not found`
(in "Removal of function pointers and virtual functions").

**Why:** Structural CBMC 6.9.0 bug — the legacy `--enforce-contract` generated
`__CPROVER__start` wrapper references a `<param>$object` symbol for any
function-pointer *parameter* that the function-pointer-removal pass can't find.
Reproduced with a 6-line minimal example (`caller(cb_t *f, int v)`); crashes
regardless of body/contract. goto-cc, the unwind step, and enforce-contract all
succeed (rc 0); only cbmc aborts. No `requires(f==g)` / `--add-library` /
`requires(f!=0)` avoids it. Fix would need `--dfcc` (+`__CPROVER_obeys_contract`)
or a pre-enforce `--remove-function-pointers` step — neither is in the fixed
scoring pipeline. `run_cbmc` treats `costmodel` as a nondet external callee (no
`--replace-call-with-contract`).

**How to apply:** Don't chase a kill score here — original crashes, so all 20
mutants score identically (uncheckable). Same applies to GetBestLengths /
GetBestLengths2 (also take `CostModelFun *`). Left a sound minimal contract
(`requires(costmodel != NULL)` + `assigns()`) + an explanatory NOTE comment in the
source; this is the strongest sound spec without dfcc/obeys_contract.
