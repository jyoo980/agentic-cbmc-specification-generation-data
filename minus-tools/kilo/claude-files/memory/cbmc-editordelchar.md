---
name: cbmc-editordelchar
description: "editorDelChar verifies 7/25 — merge branch is unverifiable (callee contract conflict), deep else-path mutants exceed --depth 200"
metadata: 
  node_type: memory
  type: project
  originSessionId: 48bd653e-a934-46e3-9742-cd1e7472f686
---

`editorDelChar` (kilo.c) verifies under the canonical pipeline (replace all 4
contracted callees: editorRowAppendString, editorDelRow, editorRowDelChar,
editorUpdateRow; --partial-loops --unwind 5 --depth 200) and kills **7/25**
mutants. Harnesses: `verify_editorDelChar.sh`, `run_mutants_editorDelChar.sh`.

**Spec models the in-row delete path** (filecol != 0): single fresh row, filerow
pinned to 0 (E.rowoff==E.cy==0), filecol in {2,3} so editorRowDelChar's at=filecol-1
>= row->size(1) → its contract takes the EARLY-RETURN arm (leaves size/render/hl
intact) so editorUpdateRow's size==1/render==NULL/is_fresh(hl,8) preconds still hold
at the tail call. filecol==1 would delete (size→0, render/hl havoced) and break that
tail call. Pin cursor postconds (E.cx/E.coloff) with __CPROVER_old.

**Why only 7/25 (the rest are structural, NOT fixable by a stronger spec):**
- **Merge branch (filecol==0) is UNVERIFIABLE here** → its 10 mutants (lines for
  filerow-1.size, &E.row[filerow-1] append, E.cy==0, E.cx>=screencols ×5, shift ×2)
  are dead code under the filecol!=0 precondition. Root cause: a real callee-contract
  conflict — editorRowAppendString(&E.row[filerow-1]) requires that row's render==NULL,
  but editorDelRow (called next, frees every row) requires is_fresh(render) on the SAME
  row; editorRowAppendString havocs render in between, so no state satisfies both.
- **5 equivalent/deep-only**: filerow=rowoff-cy (0-0 equiv), filerow>numrows / ==numrows
  (need filerow>=numrows boundary, impossible with valid fresh row at filerow=0),
  filecol==0&&filerow!=0 (needs filecol==0, the dead branch), !row&&(...) (needs row==NULL,
  whose divergence only fails deep). None killable at --depth 200 with a sound spec.
- **3 deep else-path mutants reachable only above --depth 200**: editorRowDelChar(row,
  filecol+1) kill lands at depth ~207 (its at<=2 precond); the two `E.cx==0 && E.coloff`
  cursor mutants need the full body incl editorUpdateRow replacement → depth ~550. The
  spec IS strong enough (all 3 die at --depth 1000); canonical --depth 200 just truncates.
  Could only be "won" by removing genuine editorUpdateRow preconditions (is_fresh chars/hl)
  to game the depth limit — declined, as that weakens the actual spec. Same depth-truncation
  limit as [[cbmc-editoropen-canonical-zero-kills]] / [[cbmc-editorrowappendstring]].
