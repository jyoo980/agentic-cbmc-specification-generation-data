---
name: kilo-initeditor-spec
description: initEditor verifies under avocado; spec is strong/non-vacuous (proven w/o --depth) but vacuous UNDER --depth 200; kill score N/A (no mutable operators).
metadata: 
  node_type: memory
  type: project
  originSessionId: b55ac70b-179f-4695-89af-4f33e67ad686
---

`initEditor` (kilo.c) sets 9 E fields to 0/NULL then calls [[kilo-updateWindowSize-spec]]
(replaced by contract → havocs screenrows/screencols) and `signal` (external, no body → nondet).

Spec: `__CPROVER_assigns(E.cx,E.cy,E.rowoff,E.coloff,E.numrows,E.row,E.dirty,E.filename,
E.syntax,E.screenrows,E.screencols)` + 8 relational `ensures` pinning each directly-set field
(cx/cy/rowoff/coloff==0, numrows==0, row/filename/syntax==NULL, dirty==0). screenrows/cols
can't be pinned (updateWindowSize contract has no ensures on them).

**Verifies under `run-cbmc`.** Mutation N/A — "no mutable operators" (body is all
constant stores; updateWindowSize is replaced so its operators aren't in scope).

**Vacuity nuance (like [[kilo-editorRowsToString-spec]]):** the false-ensures probe
(`E.cx==12345`) PASSES under avocado's `--depth 200` → looks vacuous. But run cbmc with NO
depth bound and the probe FAILS (2 of 12078) → spec is genuinely NON-vacuous & strong. The
depth-200 cutoff truncates the path before the postcondition because the updateWindowSize
contract-replacement + is_fresh memory-map setup exceeds 200 steps. NOT the is_fresh-UNSAT
poison (preconditions are SAT). Can't fix — rules forbid hardcoding `--depth`. Accept as-is.
