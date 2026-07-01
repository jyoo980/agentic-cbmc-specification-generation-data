---
name: cbmc-editorinsertnewline
description: editorInsertNewline verifies via the immediate-return path only; canonical depth-200 kill cap is 5/19 (every editorInsertRow-reaching path is unverifiable)
metadata: 
  node_type: memory
  type: project
  originSessionId: f0727a4a-87e2-4489-b793-98af29457144
---

editorInsertNewline (kilo.c) VERIFIES under canonical run_cbmc (callees replaced:
editorInsertRow + editorUpdateRow; --partial-loops --unwind 5 --depth 200) and
scores **5/19** mutants killed — the structural maximum.

**Why only the immediate-return path is verifiable:** every other branch reaches
`editorInsertRow`, whose replaced contract requires `1<=len<=2` AND `is_fresh(s,len+1)`.
But editorInsertNewline only ever calls it with `len==0` (the two `editorInsertRow(...,"",0)`
inserts) or with `s==row->chars+filecol` (a non-base offset into an existing object) —
neither can satisfy that contract → those paths fail. The split branch's later
`editorUpdateRow(&E.row[filerow])` is also dead because editorInsertRow's contract
havocs E.row first (`is_fresh(row)` then unprovable). So the ONLY verifiable scenario is
`filerow > E.numrows` ⇒ row==NULL ∧ filerow!=E.numrows ⇒ immediate `return`.

**Spec** (pins to force that path): `E.numrows==2`, `E.rowoff==0`, `3<=E.cy<1000000`
(⇒ filerow==E.cy>2, return; and the `E.rowoff-E.cy` subtraction mutant goes negative),
`is_fresh(E.row, sizeof(erow)*E.numrows)` (so a flipped row-NULL guard makes
`&E.row[filerow]` an out-of-bounds deref at `row->size` = a kill), other cursor coords
bounded for no-overflow, and **empty `__CPROVER_assigns()`** (return path writes nothing;
any mutant reaching editorInsertRow violates this frame = a kill).

**Kills (5):** filerow_sub(OOB), row-guard `<`/`<=`/`==` (force `&E.row[E.cy]` OOB),
append-test `==`→`!=` (reaches editorInsertRow, precond/frame fail).
**Survivors (14, all structural):** filecol_sub (unobservable before the return);
row-guard `>`/`!=` (equivalent to `>=` when filerow>numrows); the 5 filecol-clamp, the
fczero, the 3 split, and the 2 fixcursor mutants all lie on editorInsertRow-reaching
(dead) paths and so are unreachable on the verified path.

Same family as [[cbmc-editorinsertchar]] / [[cbmc-editordelchar]]: editorInsertRow's
strict contract (E.numrows==2, 1<=len<=2, is_fresh source) + its E.row havoc caps every
caller. Scripts: verify_editorInsertNewline.sh, run_mutants_editorInsertNewline.sh.
