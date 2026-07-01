---
name: cbmc-zopfliblocksplitlz77-callee-precondition-wall
description: ZopfliBlockSplitLZ77 verifies via tiny-file early return but kill 0/53; body undischargeable
metadata: 
  node_type: memory
  type: project
  originSessionId: 30c74fc0-5164-4169-9173-1803c4531677
---

`ZopfliBlockSplitLZ77` (Syzygy_Zopfli/c_code/zopfli.c) verifies soundly with a tiny-file
early-return pin (`lz77->size < 10`, `__CPROVER_assigns()`) but kill 0/53. In-body
malloc/free did NOT crash goto-instrument here. Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.

**Why kill 0 is a hard wall:** the entire effectful malloc+main-loop body is unreachable on
any sound path because each loop callee's pinned precondition contradicts the args
ZopfliBlockSplitLZ77 actually passes:
- EstimateCost requires `lend <= 2`, called with `lend == lz77->size`
- FindLargestSplittableBlock requires `lz77size == 8 && npoints == 3`
- AddSorted requires `*outsize == 3`
- FindMinimum ensures only `<= end`, too weak for in-body `assert(llpos < lend)` (and FindMinimum is itself funptr/BoundaryPM-undischargeable, see [[cbmc-findminimum-funptr-boundarypm-crash]])

Even forcing entry (pin size>=10, maxblocks==1, options->verbose==0 → loop breaks at the
maxblocks guard, skips PrintBlockSplitPoints) reaches lines 5595/5601/5610 but kills nothing:
only caller-visible effect is via AddSorted (unreachable on that path) and `done` is a freed
local, so no mutant is contract-observable. Maxblocks-guard &&/|| mutants are equivalent on
the break path (||-true whenever &&-true).

Related vacuous/undischargeable callees: [[cbmc-addsorted-append-depth-vacuity]],
[[cbmc-findlargestsplittableblock-global-init-vacuity]], [[cbmc-estimatecost-no-mutable-operators]],
[[cbmc-printblocksplitpoints-depth-earlybreak-vacuity]].
