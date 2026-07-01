---
name: zbslz-depth200-vacuity
description: "ZopfliBlockSplitLZ77 verifies in early-return regime but 0/53 kills @depth200; loop body unreachable in any sound setup, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: e6afcd58-8a16-494d-b7bc-c46b8705da06
---

ZopfliBlockSplitLZ77 (zopfli.c) verifies @depth200 with a minimal early-return
contract: `is_fresh(lz77) + lz77->size < 10 + assigns()`. Replace-call all 5
callees: FindMinimum EstimateCost AddSorted FindLargestSplittableBlock
PrintBlockSplitPoints (`/app/_verify_zbslz.sh`, stub `/app/stubs/cprover_alloc.c`).

0/53 kills @depth200, and this is inherent — do NOT chase:
- The loop body is UNREACHABLE in any SOUND callee-contract setup. Reaching it
  needs lz77->size >= 10 (skip the early `return`), but EstimateCost's contract
  requires size<=3 and FindLargestSplittableBlock's requires size<=6. Those
  callees can't be inlined instead (FindMinimum's fptr body recurses through
  BoundaryPM → goto-instrument abort; EstimateCost forwards to the full
  ZopfliCalculateBlockSizeAutoType tree). So only the early-return regime
  (size<10, function does nothing) is verifiable.
- Of 53 mutants: 48 are in-loop → never executed when the unmutated guard
  returns early → equivalent in regime, survive at ANY depth.
- 5 are the entry guard `lz77->size < 10`: 3 (`>10`,`>=10`,`==10`) are KILLABLE
  (flip makes the mutant enter the loop, which fails a callee precondition) but
  only @depth>=300; `<=10` and `!=10` are equivalent in the size<10 regime.
- Threshold ~290 is a FIXED harness floor (enforce-contract + 5 replace-call
  instrumentation), NOT loop-iteration cost: pinning lz77->size==0 (zero loop
  iters) gives the SAME ~290 threshold. r_ok lever also fails (same threshold).
  So no lever recovers the 3 entry-guard kills under the fixed --depth 200.

Side fix: had to drop `__CPROVER_old` from FindMinimum's ensures (write
`return >= start && return < end` directly). CBMC can't track history of the `+`
argument `lstart + 1` ZopfliBlockSplitLZ77 passes → goto-instrument aborts with
"Tracking history of + expressions is not supported". Sound: replace-mode keeps
params at incoming values; FindMinimum is its only caller and is itself
unverifiable ([[lor-fptr-param-crash]]-style fptr crash, see its NOTE).
Related: [[avocado-depth200-vacuity]], [[flsb-depth200-vacuity]], [[ec-no-mutants]].
