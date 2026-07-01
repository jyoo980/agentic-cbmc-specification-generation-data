---
name: kilo-editorinsertnewline-spec
description: editorInsertNewline verifies but vacuous (kill 0/19) — same is_fresh-in-replaced-requires poison via editorInsertRow
metadata: 
  node_type: memory
  type: project
  originSessionId: ec3186bd-bf52-460e-9c4e-8e0960cb6bd8
---

editorInsertNewline in /app/kilo/kilo.c verifies but vacuously (kill 0/19).
Modeled on [[kilo-editorInsertChar-spec]]: requires rowoff+cy<numrows (cursor on
existing row, so 'row' != NULL and one of the two split branches is taken),
is_fresh(E.row, sizeof(erow)*(numrows+1)) (editorInsertRow may grow by one), plus
fresh cursor-row chars/render and hl==NULL for editorUpdateRow's split-branch
precondition. Ensures numrows+1, dirty+1, E.cx==0, E.coloff==0, and the
down-a-row cursor move (scroll rowoff at bottom edge else cy+1).

Vacuous because it calls editorInsertRow whose requires contains
is_fresh(E.row,...); when CBMC replaces that call with its contract the is_fresh
becomes a checked assertion needing malloc declared → UNSAT → vacuous. Confirmed
via +12345 false-ensures detector (still "verifies"). Unavoidable toolchain
poison, not a weak spec. Same root cause as [[kilo-editorRowInsertChar-spec]].
