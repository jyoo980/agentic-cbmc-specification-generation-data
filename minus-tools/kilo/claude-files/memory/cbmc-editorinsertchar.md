---
name: cbmc-editorinsertchar
description: "editorInsertChar verifies canonically but scores 0/14 — body truncated <=depth 410, CBMC crashes >=430 on editorInsertRow contract replacement"
metadata: 
  node_type: memory
  type: project
  originSessionId: 216cb362-fe86-49b4-91b3-c98c5ab062e3
---

`editorInsertChar(int c)` in kilo.c: spec VERIFIES under the canonical pipeline
(replace-call-with-contract editorInsertRow + editorRowInsertChar, --partial-loops
--unwind 5, --enforce-contract, --depth 200) but canonical kill score is
structurally **0/14**.

Two callees, both contracted/replaced: editorInsertRow (row-append loop path) and
editorRowInsertChar (in-bounds insert path).

Modeling (mirrors [[cbmc-editordelchar]] in-row-delete shape):
- Only the in-bounds path (filerow < numrows ⇒ row non-NULL ⇒ skip `if(!row)` block)
  is verifiable. The row-append branch is UNVERIFIABLE: its loop calls editorInsertRow,
  whose replaced contract havocs (reassigns+frees) E.row, after which the following
  editorRowInsertChar's is_fresh(&E.row[filerow]) can't be established.
- numrows==1, is_fresh(E.row,sizeof(erow)), row==&E.row[0]==E.row base of fresh object.
- To make filerow/filecol +→- mutants non-equivalent: cy>=1 with rowoff∈[-1e5,-1] and
  rowoff+cy==0 (mutant ⇒ filerow=-2cy ⇒ is_fresh(row) fails); cx>=1 with coloff=-cx and
  coloff+cx==0 (mutant ⇒ at=-2cx ⇒ editorRowInsertChar's at==0 fails). MUST bound rowoff
  AND coloff directly — the bare equality `rowoff+cy==0` admits wraparound solutions and
  CBMC flags a signed-overflow on the body's `E.rowoff+E.cy`.
- screencols symbolic so both arms of `cx==screencols-1` reachable → dual cursor-ensures
  kill the ==→!= and screencols-1→+1 mutants.

The 10 ternary (`filerow>=numrows`) + loop-guard (`numrows<=filerow`) mutants are
EQUIVALENT on the only verifiable path: the loop never iterates and `row` is reassigned
to &E.row[filerow] regardless, so the ternary's value is unobservable.

The other 4 (filerow, filecol, 2×cxsc) the spec WOULD kill — but the body is never
reached: at canonical --depth 200 the is_fresh harness + two-callee contract-replacement
prelude truncate before editorRowInsertChar's call (so its precondition assertions and
my ensures are vacuous → 0 kills, "1 iterations"). Verifies vacuously up to depth ~410;
at depth >=430 cbmc 6.9.0 ABORTS with an invariant-check failure inside editorInsertRow's
contract-replacement (assigns __car_lb, line 792) triggered by the `editorInsertRow(...,"",0)`
string-literal arg on the dead loop path. So NO depth meaningfully checks the body — same
structural 0 as [[cbmc-editorfind]], editorRefreshScreen, [[cbmc-editoropen-canonical-zero-kills]].

Re-confirmed 2026-06-26 (canonical depth IS 200 — all 35 recorded runs use --depth 200).
Tried CONCRETE cursor values (E.cy==1,E.rowoff==-1,E.cx==1,E.coloff==-1) instead of the
bounded ranges to try to constant-fold filerow=0 and statically prune the dead
`if(!row)` editorInsertRow branch. Result: still 0/14 at depth 200 (prelude ~430 steps);
the concrete version DOES avoid the editorInsertRow abort (dead branch pruned → no crash),
so it reaches the body cleanly at higher depth — but that just exposes the real wall:
the UNMUTATED spec is vacuous-SUCCESS only through depth ~420, then at depth >=450 the
body is reached and FAILS — editorRowInsertChar's REPLACED-contract postcondition
`row->chars[0]==(char)c` (line 1020) dereferences row->chars after that contract's
`frees(row->chars)`+object_whole havoc leaves it possibly-NULL/invalid (pointer_dereference
.103/.104/.105). So there is NO depth where the spec is BOTH non-vacuous AND successful:
<=420 vacuous (0 kills), >=450 fails on the callee-contract free/havoc artifact. The
canonical 0/14 is therefore doubly structural — reverted to the bounded (more-general,
documented) requires. Don't retry concrete values; they don't help the canonical score.

Artifacts: verify_editorInsertChar.sh, run_mutants_editorInsertChar.sh (both replace both callees).
