---
name: cbmc-getcostmodelmincost-funptr-param
description: "GetCostModelMinCost undischargeable; nondet function-pointer param crashes legacy enforce-contract, obeys_contract needs --dfcc"
metadata: 
  node_type: memory
  type: project
  originSessionId: ce15292c-eeed-4294-9214-4dbdbd062fbf
---

`GetCostModelMinCost(CostModelFun *costmodel, void *costcontext)` in
`/app/Syzygy_Zopfli/c_code/zopfli.c` (~line 3933) is **undischargeable** under the
avocado harness. `CostModelFun` is `typedef double f(unsigned,unsigned,void*)`
(zopfli.h:89); the body calls through the `costmodel` pointer.

Two independent blockers:
1. The nondet function-pointer parameter is havoced by `--enforce-contract`. When
   symex reaches the call through it, CBMC aborts with an **invariant violation**:
   `__CPROVER__start::costmodel$object was not found`. Happens with ANY contract
   (empty, `requires(costmodel==&GetCostFixed)`, etc.) — pinning in `requires`
   does not remove the synthetic `costmodel$object` dispatch target.
2. The documented fix `__CPROVER_obeys_contract(costmodel, Contract)` (see
   docs/contracts-function-pointer-predicates.md) is rejected:
   `__CPROVER_obeys_contract is not supported in this version`. It requires
   `goto-instrument --dfcc`, but `run_cbmc_and_mutation_testing.py`
   (`_get_goto_instrument_contract_command`) emits only
   `goto-instrument --replace-call-with-contract ... --enforce-contract ...` —
   no `--dfcc`, no `--restrict-function-pointer`. So DFCC-only features (obeys_contract,
   function-pointer predicates) are unavailable harness-wide.

Implication: every CostModelFun-taking function called via the pointer
(GetCostModelMinCost, GetBestLengths/squeeze forward pass at ~3990, ~4107) hits
blocker #1 the same way. The left-in spec uses obeys_contract as the correct
intended contract for a future --dfcc-enabled run, but it cannot verify here.
Related: [[cbmc-zopflilz77gethistogram-depth-vacuity]] (needs same -I stubs for FILE.h).
