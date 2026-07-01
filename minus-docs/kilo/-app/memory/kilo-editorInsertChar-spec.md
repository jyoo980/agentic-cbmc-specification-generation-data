---
name: kilo-editorinsertchar-spec
description: editorInsertChar verifies but is vacuous (kill 0/14) — same is_fresh-in-replaced-requires poison via editorRowInsertChar callee
metadata: 
  node_type: memory
  type: project
  originSessionId: 9c22e399-fc44-48d9-ab47-ddac9c465f38
---

`editorInsertChar` (kilo.c ~line 873) spec: requires bound numrows/dirty(<INT32_MAX-2, two bumps)/rowoff/cy/coloff/cx/screencols; key `(long)rowoff+(long)cy < numrows` so the cursor sits on an existing row and the unbounded `while(E.numrows<=filerow) editorInsertRow(...)` loop is NOT taken (row non-NULL path only); `(long)coloff+(long)cx < INT32_MAX-2` (passed as `at` to editorRowInsertChar); is_fresh(E.row,...); for the row at idx=rowoff+cy: 0<=size<INT32_MAX, is_fresh(chars,size+1), is_fresh(render,1), hl==NULL. assigns E.cx,E.coloff,E.dirty,object_whole(E.row)+object_whole of that row's chars/render. ensures dirty==old+2, numrows unchanged, row!=NULL, chars!=NULL, chars[old(coloff)+old(cx)]==(char)c, and the cursor branch (cx==screencols-1 ⇒ coloff++ else cx++). Verifies.

**Vacuous, kill 0/14 — unavoidable, NOT a weak spec.** Confirmed: replaced `dirty==old+2` with `dirty==old+12345` (clearly false) and it STILL "verified successfully". Same root cause as [[kilo-editorrowinsertchar-spec]] and [[kilo-cbmc-contracts-vacuous]]: callee editorRowInsertChar carries `is_fresh` in its requires; CBMC contract-replaces it, discharging is_fresh-in-replaced-requires needs `malloc` declared (the malloc/realloc stubs shadow it) → UNSAT → poisons the whole proof. Any function whose only non-trivial callee is editorRowInsertChar/editorUpdateRow inherits this. Kept the strong faithful spec.
