---
name: kilo-editormovecursor-spec
description: editorMoveCursor verifies NON-vacuously at 39/61 kill; numrows<=2 balances is_fresh-poison budget vs old()-based relational ensures.
metadata: 
  node_type: memory
  type: project
  originSessionId: d606f855-e269-41a5-b287-169f3c4219a1
---

editorMoveCursor in /app/kilo/kilo.c: leaf fn (no callees), verifies and is
genuinely NON-vacuous, kill score 39/61 (0.64). Hard-won; key facts:

- **is_fresh poison is a BUDGET, not all-or-nothing.** Each is_fresh-memory
  touch consumes it: `E.row[i].size` in a requires AND every `__CPROVER_old(E.*)`
  snapshot in an ensures count. Exceed it → preconditions go UNSAT → vacuous
  (kill 0). `numrows<=2` (is_fresh of 2 + 2 size-requires) leaves room for the
  relational ensures; `numrows<=5` did NOT (a single full per-key old()-block
  flipped it vacuous). numrows<=2 still covers both filerow>0 and filerow<numrows
  branches, so no kill-score loss vs larger N.
- **`__CPROVER_forall` in a REQUIRES with an array deref is silently DROPPED** by
  this toolchain (confirmed with a minimal test: `a[0].size` stayed unconstrained).
  Constrain each row size index-by-index instead. Explicit indexed requires IS honored.
- Need `0 <= E.row[i].size` to kill the spurious signed-overflow `filecol - rowlen`
  in the trailing "fix cx" block (rowlen = row->size, havoc'd negative otherwise).
- **Real finding (CBMC caught it):** ARROW_LEFT does `E.cy--` even when cy==0 (if
  rowoff>0 makes filerow>0), so cy can exit at -1. Ensure `-1 <= E.cy`, not `0 <=`.
- The trailing "fix cx" block assigns ONLY cx and coloff, so cy/rowoff at exit are
  exactly the switch result → write exact per-key cy/rowoff ensures (UP/DOWN/LEFT/
  default; no row deref needed). That lifted 31→39.
- **ARROW_RIGHT exact cy/rowoff NOT addable:** needs `E.row[old filerow].size` in
  the ensures; that deref (even with is_fresh bumped to 3) blows the budget → vacuous.
- Remaining 22 survivors need exact cx/coloff, which the re-clamping fix-block plus
  the tiny poison budget make infeasible. 0.64 is a strong result for this fn.
- Vacuity check that works here: add `__CPROVER_ensures(E.rowoff == E.rowoff + 12345)`;
  if it still verifies, spec is vacuous. (The +12345 detector is confounded if a
  real overflow/bug also makes a postcondition fail — block real bugs first.)

Related: [[kilo-cbmc-contracts-vacuous]]
