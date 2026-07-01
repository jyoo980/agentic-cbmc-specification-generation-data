---
name: cbmc-editormovecursor
description: editorMoveCursor verifies 34/61; depth-200 global-init ceiling caps deeper-path kills; exact cx/coloff ensures backfire
metadata: 
  node_type: memory
  type: project
  originSessionId: 843b5dd0-ef59-4c3e-b762-19f18ce4be53
---

editorMoveCursor (kilo.c, no callees) verifies and scores **37/61** at canonical
`--partial-loops --unwind 5` + `--depth 200`. Up from 0 (no contract existed),
then 34, now 37.

**ENSURES ORDER MATTERS at --depth 200 (this got 34→37).** Whichever heavy
postcondition is *last* gets truncated on the longest body paths before its
assertion is reached, so it loses its unique kills. With order [cx>=0,
coloff+cx<=rowlen, filerow(OMC_NFR), rowoff] the *last* (rowoff) truncates and
the 4 ARROW_RIGHT/DOWN `screenrows±1` mutants (L2002×2, L2018×2) survive at 200
(they die at --depth 400). **Swapping to [..., rowoff, filerow]** puts rowoff
3rd (fits) so those 4 die; the cost is filerow now last → loses exactly 1 kill
(L1979 `filerow < 0`, only catchable by filerow on the long ARROW_LEFT inner
path). Net +3 = 37. rowoff-last loses 4, filerow-last loses 1 → keep filerow
last. Do NOT try to recover that 1 by adding a dedicated cheap ARROW_LEFT
filerow ensures or by removing coloff+cx<=rowlen: both push CBMC over the 120s
per-mutant timeout → ~50 mutants go OTHER (TIMEOUT), score collapses. The
coloff+cx<=rowlen ensures (#2, only LIVE array deref) is load-bearing; removing
it → 50 OTHER. 37 is the practical depth-200 ceiling.

Model: `E.numrows==2`, `is_fresh(E.row, sizeof(erow)*2)`, all of
cx/coloff/screencols/screenrows/row[i].size bounded to [0,1000] (or [1,1000]),
and rowoff,cy each in [0,numrows] with `rowoff+cy<=numrows` (memory safety for
`E.row[filerow]` and `E.row[filerow-1]`). `assigns(E.cx,E.cy,E.rowoff,E.coloff)`.
**Overflow pitfall:** must bound rowoff AND cy individually *before* the
`rowoff+cy<=numrows` requires — CBMC checks overflow on the `rowoff+cy`
expression inside the requires itself.

Winning ensures (cheap + strong): `cx>=0`; `coloff+cx <= rowlen_final`; **exact
final `rowoff+cy` (filerow)** and **exact final `rowoff`** via `__CPROVER_old`
history macros keyed on `key` (clamp never touches rowoff/cy, so the switch
fully determines them). These 4 give 34.

**Key lesson — expensive ensures backfire:** adding the *full* exact `E.cx` /
`E.coloff` functional postcondition (folding in the end-of-line clamp) DROPPED
the score 34→23. The giant nested-ternary ensures evaluation consumes the shared
`--depth 200` budget, truncating body paths *before* they reach the assertions,
so fewer mutants are observed. Cheaper, well-chosen ensures beat exhaustive ones.

**Ceiling is structural, not spec-strength:** ~6-7 survivors are provably
equivalent given the model (filerow in [0,2]: `>0`≡`!=0`, `>=numrows`≡`==numrows`,
`<numrows`≡`!=numrows`; clamp `filecol>rowlen` vs `>=` and `cx<0` vs `<=0` are
no-ops at the boundary). The other ~20 (ARROW_RIGHT/LEFT-inner cx/coloff split,
cy==screenrows-1, clamp arithmetic) are **killable only at --depth >=300**
(directly confirmed: L2018 ARROW_DOWN cy-mutant SUCCESS@200, FAILED@300). The
syntax-DB global init (C_HL_keywords etc.) eats most of the 200-step budget;
shrinking to numrows=1 does NOT free enough (still survives @200) and makes more
mutants equivalent — not worth it. Same depth-200 cap pattern as siblings
[[cbmc-editorreadkey]], [[cbmc-editorinsertchar]], [[cbmc-editorinsertnewline]].

Scorer: `/app/score_emc.py` (patches each mutant line onto kilo.c, runs the
canonical pipeline, no callee replacement needed). Keep it (don't delete).
