---
name: kilo-editorprocesskeypress-spec
description: editorProcessKeypress NOT verifiable — same variadic editorSetStatusMessage replacement crash as editorFind/editorSave (Ctrl-Q branch passes vararg quit_times).
metadata: 
  node_type: memory
  type: project
  originSessionId: 25dff6bd-31e8-4c84-bf52-301aa2a3716b
---

editorProcessKeypress in /app/kilo/kilo.c: NOT verifiable by the
avocado/goto-instrument pipeline. The Ctrl-Q branch calls the *variadic*
editorSetStatusMessage with an actual vararg (quit_times), and replacing that
call with its (non-empty assigns) contract crashes goto-instrument:
`util/std_expr.cpp:423 instantiate: variables.size() == values.size()`.
Identical crash to [[kilo-editorFind-variadic-block]] and [[kilo-editorSave-spec]].
The `malloc is not declared` line in the output is the usual is_fresh red herring;
the real abort is the variadic instantiate invariant.

Strong contract retained anyway: preconditions are the intersection of all
dispatched callees' contracts — `0 < E.numrows <= 2` (editorMoveCursor cap +
editorDelChar lower bound), `rowoff+cy < numrows`, in-range allocated cursor row
(is_fresh chars/render, hl==NULL) for editorInsert*/editorDelChar. assigns covers
E.cx/cy/coloff/rowoff/numrows/dirty/row/statusmsg(_time) + object_whole(E.row).
A NOTE comment in the file explains the toolchain limitation.

Related: [[kilo-editorMoveCursor-spec]], [[kilo-editorFind-variadic-block]], [[kilo-editorSave-spec]]
