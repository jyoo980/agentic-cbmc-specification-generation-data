---
name: cbmc-editorrefreshscreen
description: "editorRefreshScreen verifies only by making abAppend/abFree threadable (is_fresh in ensures), which regresses abAppend self-verify; canonical depth-200 kill score is structurally 0"
metadata: 
  node_type: memory
  type: project
  originSessionId: 38bedb4c-22a5-4466-bf2c-17e25352d2ce
---

editorRefreshScreen (kilo.c) baseline does NOT verify: it builds a local `struct abuf ab = ABUF_INIT` ({NULL,0}) through 26 sequential abAppend calls. The original abAppend contract (`ab->len>0 && ab->len<=8`, `is_fresh(ab->b,ab->len)`, `is_fresh(s,len)`) is unsatisfiable at the first call (ab->len==0) — there is no ERS-level precondition that helps because `ab` is a local. abAppend is called ONLY by ERS; abFree ONLY by ERS.

**Fix that makes ERS verify (committed):** rewrite abAppend + abFree to be *threadable*:
- requires: `ab->len>=0`, `len>=0`, `__CPROVER_r_ok(s,len)` (NOT is_fresh(s,len) — is_fresh on string literals flakily fails at repeated call sites), `ab->len>0 || ab->b==NULL`, `ab->len==0 || is_fresh(ab->b,ab->len)`.
- ensures (the threading): re-establish `ab->len==0 || is_fresh(ab->b,ab->len)` and `ab->len>0 || ab->b==NULL`, plus `len==old || old+len`. The assumed is_fresh-in-ensures lets the NEXT call's asserted is_fresh-in-requires pass — CBMC's is_fresh DOES thread assumed→asserted across replaced calls (verified empirically).

**Unavoidable cost:** is_fresh in abAppend's *ensures* + the realloc body crashes abAppend's own goto-instrument (`instrument_spec_assigns.cpp:597 create_car_expr: no definite size for ... should_malloc_fail`). So abAppend regresses VERIFIED→GOTO-INSTRUMENT_FAILED. No threadable abAppend contract can self-verify (the realloc body needs ab->b dynamic; threading a dynamic object needs is_fresh-ensures; the two are incompatible in this CBMC). abFree still self-verifies. Prior session hit the same wall and never committed the swap (kept OLD_AB self-verifying, ERS unverified, measured kills via /tmp/ers_contract.txt + /app/score_ers.py).

**ERS contract (strong, enables row loop):** numrows==1, screenrows==2, screencols==4, rowoff==coloff==0, cy==0, cx in [0,4]; is_fresh(E.row,sizeof(erow)), row[0].rsize==size==4, is_fresh(render,5)+render[4]=='\0', is_fresh(hl,4), is_fresh(chars,4), is_fresh(filename,1)+filename[0]=='\0'; `__CPROVER_assigns()` (ERS writes NO global — output goes via write()).

**Kill score = 0 at canonical depth 200, structural:** (1) no observable postcondition (output via write(), no global mutation, assigns() empty) → behavioral mutants invisible; (2) memory-safety/precondition mutants live in code unreachable at depth 200 — the row-rendering loop is reached only ~depth 600 (probed: `abAppend(&ab,c+j,1)`→`c-j` OOB-before-render kills at 600, not 200/300/400). (3) is_fresh objects have UNBOUNDED extent under enforcement, so OOB-*after* (e.g. E.row[1] via `filerow>=numrows`→`>`) never kills even at depth 500 — only OOB-*before* is caught. Cannot raise depth (rule: no CLI-arg hardcoding). Consistent with [[cbmc-abappend]] (0/3), [[cbmc-editoropen-canonical-zero-kills]] (0/9).

Helper scripts kept in /app/kilo: `_run_ers.py`, `_run_fn.py <fn>`, `_score.py` (canonical 95-mutant scorer), `_depthprobe.py`.
