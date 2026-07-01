---
name: cbmc-getcostmodelmincost-fnptr-param
description: "GetCostModelMinCost in zopfli.c is unverifiable — --enforce-contract on a function with a function-pointer parameter crashes CBMC's entry harness"
metadata: 
  node_type: memory
  type: project
  originSessionId: 88e56ed0-982d-4e2c-862a-267f7bd54465
---

`GetCostModelMinCost(CostModelFun *costmodel, void *costcontext)` in
`/app/Syzygy_Zopfli/c_code/zopfli.c` (~line 3306) CANNOT be verified by the
avocado pipeline, regardless of spec (crashes even with NO contract).

**Root cause:** the final `cbmc checking-...contracts.goto --function GetCostModelMinCost`
step aborts with an invariant violation:
`identifier __CPROVER__start::costmodel$object was not found` (namespace.h:49 lookup).
CBMC's entry-harness factory cannot nondet-initialize a *function-pointer*
parameter on the `--enforce-contract` wrapper — it emits a reference to a
synthetic `costmodel$object` symbol it never adds to the symbol table.

**Diagnosis (all reproduced):**
- Original entry (plain `goto-cc --function` → `cbmc --function`, NO enforce/unwind)
  resolves the function pointer fine and runs — proves the function & spec are well-formed.
- Crash is triggered specifically by `goto-instrument --enforce-contract` producing
  the wrapper that `cbmc --function` then tries to build a `_start` for.
- Independent of contract: stripping the contract entirely still crashes.
- `goto-instrument --enforce-contract` itself aborts (exit 134) if run WITHOUT the
  prior `--partial-loops --unwind` step; with unwind first it writes the goto OK,
  then the cbmc step crashes.

This is a CBMC 6.9.0 toolchain limitation, not a spec problem. Same blocker will
hit the other CostModelFun-param functions (GetBestLengths line ~3360,
LZ77OptimalRun line ~3479) if verified by entering at them.

**CONFIRMED for GetBestLengths (2026-06-26):** wrote a full strong spec (window/
cache/hash requires mirrored from verified FollowPath + `is_fresh(costcontext,
sizeof(SymbolStats))` + DP-array `is_fresh(costs/length_array, (inend-instart+1)*...)`
+ object_whole assigns + `ensures __CPROVER_return_value >= 0`). CBMC aborts with the
identical `__CPROVER__start::costmodel$object was not found` invariant violation.
Spec is sound and left in place; unverifiable purely due to the fn-ptr param.

**Left in place** (sound, minimal, would be correct if CBMC supported it):
`__CPROVER_requires(__CPROVER_is_fresh(costcontext, sizeof(SymbolStats)))` (covers
the GetCostStat branch's context deref; GetCostFixed ignores context) +
`__CPROVER_assigns()` (neither cost model writes). No sound numeric postcondition
exists: costcontext is is_fresh nondeterministic so GetCostStat can return arbitrary
doubles. Note: `__CPROVER_requires(costmodel == GetCostStat || ...)` does NOT help —
crashes identically (the `$object` failure is in `_start`, before requires applies).

Related: [[cbmc-aarch64-fileh-stub]] (needs `-I /tmp/cbmc-inc`).
