---
name: cbmc-findminimum-funptr-boundarypm-crash
description: "FindMinimum undischargeable; funptr param inlines SplitCost call tree into recursive BoundaryPM, goto-instrument numeric exception"
metadata: 
  node_type: memory
  type: project
  originSessionId: 9230a2e6-26a5-4756-9c5b-b4ed2a2cabc8
---

`FindMinimum` (zopfli.c ~5458) takes a `FindMinimumFun f` function-pointer
parameter. CBMC's function-pointer removal resolves `f` to the only
signature-compatible target, `SplitCost`, and inlines its whole call tree
(`SplitCost` has no contract → inlined → `EstimateCost` → `ZopfliCalculateBlockSizeAutoType`
→ ... → `ZopfliLengthLimitedCodeLengths` → recursive `BoundaryPM`). Callee
contracts are NOT replaced across the funptr edge — the harness's replace-set is
the direct call graph, which has no funptr edges — so even though EstimateCost/
ZopfliCalculateBitLengths/BoundaryPM all have contracts, the chain is fully
inlined and goto-instrument aborts: "Recursive call to 'BoundaryPM' during
inlining / Numeric exception : 0" at "Enforcing contracts" (~50s).

Same funptr wall as [[cbmc-getcostmodelmincost-funptr-param]] and
[[cbmc-getbestlengths-funptr-crash]]; the crash here surfaces via the
[[cbmc-recursive-isfresh-helper-blocks-callers]] BoundaryPM recursion wall.

**Why:** funptr params are unhandled by enforce-contract; obeys_contract needs
--dfcc which the harness omits.

**How to apply:** undischargeable. Adding a contract to SplitCost does NOT help
(not replaced across funptr edge — tested, same crash). Left the intended strong
contract (requires start<=end + is_fresh(smallest); assigns *smallest; ensures
return in [start,end]) documented in a comment. Needs stub -I /app/Syzygy_Zopfli/stubs
for FILE.h to even reach goto-instrument.
