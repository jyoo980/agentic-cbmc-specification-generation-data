---
name: kilo-editorsave-spec
description: "editorSave NOT verifiable — same variadic editorSetStatusMessage replacement crash as editorFind; confirmed by isolation it's the variadic call, not is_fresh."
metadata: 
  node_type: memory
  type: project
  originSessionId: d094f838-b7e7-419d-b554-5dd658f1163c
---

editorSave in /app/kilo/kilo.c is NOT dischargeable by avocado/goto-instrument, same toolchain limit as [[kilo-editorFind-variadic-block]]. It calls the variadic editorSetStatusMessage twice with actual varargs ("%d bytes written on disk", len) and ("Can't save! I/O error: %s", strerror(errno)). The pipeline --replace-call-with-contract substitutes every in-file callee; replacing a variadic call passed varargs aborts goto-instrument: `util/std_expr.cpp:423 instantiate: variables.size() == values.size()`.

**Why confirmed (not is_fresh poison):** isolated empirically — gave editorSetStatusMessage a bare `__CPROVER_assigns(E.statusmsg,E.statusmsg_time)` contract with NO is_fresh requires; crash persisted. So trigger is the variadic replacement itself. (The "malloc not declared / __CPROVER_replace_ensures_is_fresh" line is from editorRowsToString's replaced contract — a red herring, prints before the abort.)

**How to apply:** retained a strong, correct contract on editorSave (requires mirror editorRowsToString's: is_fresh filename, 1<=numrows<=2, fresh bounded rows/chars; assigns E.dirty/statusmsg/statusmsg_time; ensures return in {0,1} and return==0 ⇒ E.dirty==0) plus an explanatory comment, exactly like editorFind. Not checkable here — don't keep retrying. Any kilo function calling editorSetStatusMessage with varargs will hit this.
