---
name: cbmc-findlargestsplittableblock-vacuity
description: FindLargestSplittableBlock in zopfli.c verifies but 0/17; 4 is_fresh objects + 2 foralls exceed --depth 200 before the body
metadata: 
  node_type: memory
  type: project
  originSessionId: b4f260a1-04bc-40b0-ac24-c00d8e9fd0ea
---

`FindLargestSplittableBlock` (zopfli.c) verifies but kills 0/17. It is a small
leaf function (no callees, loop of npoints+1 iters), yet still vacuous: its 4
is_fresh objects (done, splitpoints, lstart, lend) plus two `__CPROVER_forall`
preconditions (splitpoints[k]<lz77size bound; sortedness) consume >200 CBMC
steps during precondition setup, so the body/postconditions are never reached.

Proven directly (not just inferred): a false ensures `return_value == 12345` is
reported SUCCESSFUL at `--depth 200` but FAILED at `--depth 5000` via raw
goto-cc/goto-instrument/cbmc. So the body IS reachable, just past the fixed
depth bound — which the harness hard-codes and the rules forbid changing.
Shrinking bounds (npoints<=1, lz77size<=3) does NOT remove the vacuity.

This is another instance of [[cbmc-depth200-isfresh-vacuity]], notable because
the count is only 4 pointers (the quantified preconditions add the rest of the
depth). Strong sound spec left in place: done[*lstart]==0, *lstart/*lend <
lz77size, *lend > *lstart when return==1. Gotcha fixed along the way: the
sortedness forall needs a `j < npoints &&` conjunct before `j+1 < npoints`,
else j==SIZE_MAX wraps j+1 to 0<npoints and reads splitpoints OOB.
