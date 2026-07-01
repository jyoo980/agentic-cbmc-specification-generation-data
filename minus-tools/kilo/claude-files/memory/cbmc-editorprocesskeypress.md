---
name: cbmc-editorprocesskeypress
description: editorProcessKeypress verifies 9/10; taller-file scrollable harness kills the masked &&->|| clamp mutants; canonical pipeline crashes so use the local inline scripts
metadata: 
  node_type: memory
  type: project
  originSessionId: 113fdb03-0db1-462e-8eff-1acaa6f7a43e
---

`editorProcessKeypress` (/app/kilo/kilo.c ~line 2122) verifies **9/10** (up from 7/10).

**Canonical pipeline cannot run it** (same as [[cbmc-editorfind]]): replacing the
*variadic* editorSetStatusMessage contract aborts goto-instrument 6.9.0, and the
function never calls malloc so other callees' is_fresh clauses fail to
instantiate. Local workaround scripts (KEEP, do not delete):
- `verify_editorProcessKeypress_inline.sh kilo.c 1200` — `--add-library` to
  declare malloc; leaves editorSetStatusMessage AND editorMoveCursor inlined
  (sound); replaces the other callees with contracts. depth 1200 (inlining
  lengthens the path; spec hard-codes no depth).
- `run_mutants_editorProcessKeypress.py 1200` — scorer, same pipeline.
editorMoveCursor MUST stay inlined: replacing its is_fresh+range contract makes
the paging path infeasible (field read through fresh E.row not solver-controllable)
=> postcondition goes vacuous.

**Harness scripts a PAGE_UP/PAGE_DOWN keypress** via editorReadKey's rk_* globals
(ESC [ 5 ~ or ESC [ 6 ~). Only the paging block carries killable mutants.

**Key insight that lifted 7->9 (killing the two `&&`->`||` clamp mutants 8,9):**
The paging block clamps cy then drives editorMoveCursor screenrows times. With the
OLD setup (numrows==screenrows==2, rowoff pinned 0) the cursor SATURATES at the
file edge regardless of the clamp value, so the clamp mutants are masked
(equivalent under that config). Fix: make the file one row TALLER than the screen
(numrows=3, screenrows=2) and leave rowoff FREE in [0,2] so there's real scroll
room. The clamp's effect then shows up in the final (rowoff,cy). For the CORRECT
code the outcome is still constant per key over the admitted starts:
PAGE_UP -> (rowoff=0,cy=0); PAGE_DOWN -> (rowoff=2,cy=1). Mutants 8 (PAGE_UP w/
cy_start==0, rowoff=2) and 9 (PAGE_DOWN w/ cy_start!=0, rowoff=0) now diverge.
Needs is_fresh(E.row, sizeof(erow)*3) + concrete E.row[0..2].size==5 (range on a
fresh field => infeasible path).

**Surviving mutant 10** `if (E.dirty && quit_times)` -> `||` (CTRL_Q branch) is
out of scope / not observably killable: CTRL_Q is never scripted, and the only
difference is the static quit_times (havoc'd by enforce harness) and a
return-vs-exit (exit stub does assume(0), pruning that path) — no nameable
observable. 9/10 is the practical ceiling.
