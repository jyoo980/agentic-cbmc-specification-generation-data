---
name: cbmc-editorsave
description: editorSave verifies but canonical kill score is structurally 0/4 — all 4 mutants flip unmodeled-syscall branches with identical observable output sets
metadata: 
  node_type: memory
  type: project
  originSessionId: c50cd69f-40f0-46da-bcaa-f66a0e580b99
---

`editorSave` (kilo.c) VERIFIES canonically (replace editorRowsToString contract,
enforce editorSave, --partial-loops --unwind 5 --depth 200; do NOT replace
editorSetStatusMessage — variadic, goto-instrument aborts, see [[cbmc-editorfind]]).
Spec: requires single-row state for editorRowsToString's replaced contract +
`is_fresh(E.filename,1)` + `E.dirty==1`; assigns `E.dirty, object_whole(E.statusmsg),
E.statusmsg_time`; ensures `ret∈{0,1}` and the biconditional `(ret==0)==(E.dirty==0)`.

**Canonical kill score is structurally 0/4** (`run_mutants_editorSave.py`). The 4
mutants flip `fd==-1`, `ftruncate(...)==-1`, `write(...)!=len` branches and the
error-path `fd!=-1` close guard. CBMC has NO BODY for open/ftruncate/write/close
("no body for function" warnings) → each returns fresh nondet with no side effects,
no arg checks. So original and every mutant produce the IDENTICAL observable output
set {(ret=0,dirty=0),(ret=1,dirty=1)}; the mutation only swaps which nondet value
drives which path — unobservable. No memory fault (buf is a valid editorRowsToString
contract object freed exactly once per path; close(-1) is a no-op model; statusmsg
content is havoc'd since vsnprintf has no body, so the success-vs-error message text
is unobservable too). Killing them would need a DETERMINISTIC syscall stub, which the
CLAUDE.md stub rule forbids (stubs must be non-deterministic, and nondet preserves
the symmetry). The biconditional spec is the strongest honest one. Same 0-canonical
pattern as [[cbmc-abappend]], [[cbmc-editoropen-canonical-zero-kills]].
