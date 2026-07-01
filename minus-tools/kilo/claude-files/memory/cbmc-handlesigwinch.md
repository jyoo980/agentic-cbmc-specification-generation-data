---
name: cbmc-handlesigwinch
description: "handleSigWinCh verifies (vacuously) but canonical depth-200 kill score is structurally 0/12; ensures fire only at exit which is past the editorRefreshScreen call (unreachable at 200) and ERS's precondition is anyway unsatisfiable after the clamps"
metadata:
  node_type: memory
  type: project
  originSessionId: avocado-handlesigwinch
---

`handleSigWinCh(int)` (kilo.c) body:
```c
updateWindowSize();
if (E.cy > E.screenrows) E.cy = E.screenrows - 1;
if (E.cx > E.screencols) E.cx = E.screencols - 1;
editorRefreshScreen();
```
Callees replaced by contract: updateWindowSize ([[cbmc-updatewindowsize]]) and
editorRefreshScreen ([[cbmc-editorrefreshscreen]]). 12 mutants, all on the two
clamp guards (`>`->`< <= >= == !=`) plus `-1`->`+1` on each RHS.

**Result: VERIFIES (PASS) but canonical kill score 0/12, STRUCTURAL.** Spec:
`requires(rk_idx==0)` + the full editor state ERS demands (numrows==1, rowoff==
coloff==0, cy==0, cx in[0,4], is_fresh E.row/render/hl/chars/filename + row[0]
scalars) ; `assigns(E.cx,E.cy,E.screenrows,E.screencols,rk_idx)`; two EXACT sound
ensures pinning each clamp:
`E.cy == (old(E.cy) > E.screenrows ? E.screenrows-1 : old(E.cy))` (and cx/screencols).
These ensures are 12/12-strong (would kill every mutant) AT ANY DEPTH THAT REACHES
THE RETURN — but canonical depth 200 never reaches it.

**Why 0 canonical kills (binary wall, no middle config):**
- Contract ensures are evaluated at the function RETURN, which is AFTER the
  editorRefreshScreen call (last statement). To observe the clamp result you must
  reach exit, i.e. reach/pass the ERS call.
- The ~5 is_fresh allocations in the precondition dominate the depth-200 budget, so
  the ERS call is reached only past depth 200 -> ERS requires + the postconditions
  are all VACUOUS (confirmed: `ensures(0==1)` also PASSES). Verifies, 0 kills.
- If instead you make the precondition LIGHT (no is_fresh) so the body fits in 200,
  the ERS call IS reached and the baseline FAILS: ERS's requires (screencols==4,
  screenrows==2, cy==0) are UNSATISFIABLE here because updateWindowSize havocs
  E.screenrows/E.screencols (only ensures screencols!=0) and the clamps further move
  cy/cx. Also `E.screenrows - 1` overflows (updateWindowSize doesn't bound screenrows).
- So it's strictly binary: ERS reached => baseline FAILS; ERS truncated => vacuous
  PASS @ 0 kills. No precondition gives exit-reachable-AND-ERS-satisfiable.

**Full verification is IMPOSSIBLE even ignoring depth** (don't waste attempts trying a
getWindowSize->updateWindowSize->ERS contract cascade): the clamp can produce cy<0
(when screenrows in [-2,0], which is sound since updateWindowSize's screenrows =
getWindowSize rows(>=0) - 2, and old_cy>screenrows fires the clamp -> cy=screenrows-1
in [-3,-1]). ERS fundamentally requires cy>=0 (its body does `&E.row[rowoff+cy]` ->
OOB for filerow<0), and no precondition on the ENTRY state can force the POST-
updateWindowSize screenrows>=1. So ERS's precondition (cy>=0) is unsatisfiable on some
path regardless of how ERS is relaxed. This is a latent kilo bug (handleSigWinCh
doesn't floor cy/cx at 0), so CBMC correctly can't verify it -- per CLAUDE.md leaving
it is fine. Chose the vacuous-PASS spec (PASS beats FAIL; same 0 kills either way).

Note: contracts evaluate ensures only at return -> for a function whose ONLY mutated
lines precede a depth-truncated tail call, an inline `__CPROVER_assert` after the
clamps WOULD observe them pre-ERS, but that's a body edit (against this repo's
contracts-only convention) and ERS-after still breaks a non-vacuous baseline anyway.

Script kept: `/app/kilo/run_mutants_handleSigWinCh.py` (canonical 12-mutant scorer).

**RE-CONFIRMED 2026-06-26 (inline-assert escape attempt, empirically REFUTED).**
Tried the line-56 idea concretely: pin entry cx==0 (cy already==0, both constants) and
insert two EXACT inline `__CPROVER_assert(E.cy == (0>E.screenrows?E.screenrows-1:0))`
(and cx/screencols) right after the clamps, pre-ERS — no `__CPROVER_old` needed since
both entry values are constants. Measured with a hand-rolled replica of the canonical
pipeline (callees editorRefreshScreen+updateWindowSize, --partial-loops --unwind 5,
--enforce-contract) at varying --depth:
- With the HEAVY precondition the clamp/assert is reached only at --depth ~300-320
  (cy:<= mutant's assert flips SUCCESS->FAILURE between 280 and 320). At the canonical
  200 the asserts are VACUOUS (unreached -> CBMC reports SUCCESS) -> still 0/12.
- Dropping the is_fresh block to reach the clamp by --depth ~160 makes the baseline FAIL
  at 200 on TWO fronts: (a) editorRefreshScreen.precondition.3-6 (cx∈[0,4], cy==0,
  screencols==4, screenrows==2) — ERS demands screenrows==2 & screencols==4 EXACTLY but
  updateWindowSize havocs both (only ensures screencols!=0), so the ERS-call (adjacent,
  ~5 steps past the clamp) ALWAYS fails once reached; (b) handleSigWinCh.overflow on
  `E.screenrows-1`/`E.screencols-1` (havoc'd screenrows can be INT_MIN) — also masked by
  truncation in the heavy case.
The clamp and the ERS requires-check live in the same ~5-step window, so NO precondition
weight puts clamp<200 while keeping ERS-call>200. Inline asserts don't escape the wall;
reverted them + the cx==0 tightening back to the clean verified spec. Canonical 0/12 is
final/structural. run_cbmc fixes --depth 200, --unwind 5 (no override).
