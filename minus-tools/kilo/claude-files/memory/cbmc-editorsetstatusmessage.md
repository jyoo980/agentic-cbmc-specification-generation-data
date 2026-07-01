---
name: cbmc-editorsetstatusmessage
description: "editorSetStatusMessage verifies; variadic, no mutants (0/0 max), vsnprintf/time have no CBMC body"
metadata: 
  node_type: memory
  type: project
  originSessionId: 589f6fd3-98a0-40ba-9454-08b12d50073f
---

`editorSetStatusMessage(const char *fmt, ...)` in kilo.c is variadic (va_start/vsnprintf/va_end + E.statusmsg_time = time(NULL)).

get-mutants reports "No mutant(s) generated ... (no mutable operators)" → kill score 0/0 (already max), like [[cbmc-editorsyntaxtocolor]] and is_separator.

Verifies with just `__CPROVER_requires(__CPROVER_is_fresh(fmt,1))` + `__CPROVER_assigns(__CPROVER_object_whole(E.statusmsg), E.statusmsg_time)`. No ensures on buffer contents is provable: CBMC has "no body for function 'vsnprintf'" and 'time', so they don't model the write. assigns.1/.2/.3 all SUCCESS confirms enforcement; VERIFICATION SUCCESSFUL.
