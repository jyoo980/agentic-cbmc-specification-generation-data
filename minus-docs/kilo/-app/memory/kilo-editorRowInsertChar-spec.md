---
name: kilo-editorrowinsertchar-spec
description: "editorRowInsertChar verifies but is vacuous (kill 0/18) due to malloc-not-declared breaking is_fresh in editorUpdateRow's replaced requires"
metadata: 
  node_type: memory
  type: project
  originSessionId: 253a399c-f321-4912-8070-0db9a72904ef
---

`editorRowInsertChar` (kilo.c ~line 750) spec: is_fresh(row, sizeof(erow)) + 0<=size<INT32_MAX + 0<=at<INT32_MAX-2 + is_fresh(row->chars,size+1) + is_fresh(row->render,1) + row->hl==NULL + dirty bound; assigns size/chars/render/rsize/hl/object_whole(chars)/object_whole(render)/E.dirty; ensures dirty+1, chars/render!=NULL, OBJECT_SIZE(chars)>=size+1, branch1 (old size<at ⇒ size==at+1), branch2 (at<=old size ⇒ size==old+1), chars[at]==(char)c, chars[size]=='\0'. Verifies.

**Vacuous, kill 0/18 — unavoidable, NOT a weak spec.** Confirmed by replacing an ensures with `E.dirty==old+12345` (clearly false) and it STILL "verifies successfully". Root cause (in stderr): `function 'malloc' is not declared` in `__CPROVER_{enforce,replace}_requires_is_fresh`. The body needs is_fresh(row->chars) for memory safety, and the callee editorUpdateRow's contract carries is_fresh in its requires; CBMC contract-replaces editorUpdateRow, and discharging is_fresh-in-replaced-requires needs malloc declared (the malloc/realloc stubs shadow it) → instrumentation produces no body → the requires goes UNSAT/vacuous, poisoning the whole proof.

Diagnostics tried: render==NULL instead of is_fresh(render) → still vacuous (so render isn't the trigger; is_fresh(chars)+editorUpdateRow replacement is). Removing is_fresh(render) entirely → forces editorUpdateRow to INLINE, then free(arbitrary render) gives a REAL assigns failure at object_whole(row->render) — so the only non-vacuous configs are unsound. Same toolchain class as [[kilo-cbmc-contracts-vacuous]] and the is_fresh-in-replaced-requires issue in [[kilo-editorRefreshScreen-spec]]. Kept the faithful is_fresh(render,1) version (matches editorUpdateRow's real requires; real rows have an allocated render).
