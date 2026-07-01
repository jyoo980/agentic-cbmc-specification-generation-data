---
name: cbmc-depth200-postcondition-eval-cost
description: "depth-200 can't evaluate multi-cell postconditions even in straight-line funcs; per-deref cost overruns the bound"
metadata: 
  node_type: memory
  type: project
  originSessionId: c5d82fd9-ef8f-477b-8306-d35f87fdcb71
---

In `/app/2048-clone/2048.c`, `gameEnded` could NOT reach a positive mutation kill
score under run-cbmc's fixed `--depth 200`, even with a correct strong
spec. Two compounding costs, both measured with the goto pipeline + binary search:

1. Traversing the body: gameEnded calls rotateBoard 4× (each contract-replaced =
   havoc object_whole(board) + 16 ROTATED assumes w/ __CPROVER_old). Just reaching
   the postcondition on the full path costs ~230 steps.
2. **Evaluating the postcondition expression itself costs depth.** The predicate
   `return == (NO_EMPTY && !ANY_H_PAIR && !ANY_V_PAIR)` over a 4x4 board is ~80
   `board[i][j]` dereferences; each carries pointer/bounds-check steps. Measured
   thresholds with a TRIVIAL `return false` body: NO_EMPTY ~200, +!ANY_H_PAIR ~225,
   +!ANY_V_PAIR (full) **~350**. So the full predicate is vacuous at 200 with NO
   body at all.

Diagnosis trick: add `__CPROVER_ensures(1 == 0)` as the FIRST ensures. If CBMC
still says SUCCESSFUL at depth 200, the postcondition point is unreachable
(vacuous). A cheap clause (1==0) can be caught via a SHALLOW early-return path
while the real predicate at the same program point is still starved — postconditions
are checked in order and depth is consumed cumulatively across them.

Consequence: when both the path AND the predicate are expensive, NO sound spec
kills mutants at depth 200 — every mutant-distinguishing observation needs either
the deep path or a full predicate eval. Don't weaken to a cheap-but-vacuous spec;
keep the strong correct contract (confirmed sound + kills mutants at depth ~600)
and document the depth limitation in a comment. Extends [[cbmc-depth-200-vacuous-loops]]
and [[cbmc-depth200-starves-later-ensures]] (this is the straight-line, no-loop case).
