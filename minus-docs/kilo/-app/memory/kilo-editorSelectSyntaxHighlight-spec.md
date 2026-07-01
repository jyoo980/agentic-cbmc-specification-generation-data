---
name: kilo-editorselectsyntaxhighlight-spec
description: "editorSelectSyntaxHighlight verifies non-vacuously at 1/10 kill; can't reconstruct HLDB's char** filematch under global havoc"
metadata: 
  node_type: memory
  type: project
  originSessionId: f3f2cd43-b67b-4885-b947-2aaf4cbf719d
---

`editorSelectSyntaxHighlight(char *filename)` in /app/kilo/kilo.c reads the global
highlight DB `HLDB[0].filematch` (a `char**` NULL-terminated array of pattern
strings) and calls strlen/strstr on `filename` and each pattern.

Key fact: `goto-instrument --enforce-contract` prints "Adding nondeterministic
initialization of static/global variables" — so HLDB (and C_HL_extensions) are
HAVOC'd on entry. The contract must re-establish HLDB's structure in `requires`.

**Toolchain wall (empirically bisected with a `__CPROVER_ensures(0==1)` vacuity
probe — "verified successfully" ⇒ preconditions UNSAT):** you cannot build a
valid `char**` array via is_fresh. `is_fresh(HLDB[0].filematch, N)` allocates a
byte-typed buffer whose slots read back as INVALID pointers. Then:
- `filematch[0] == filename` (alias to valid ptr) → UNSAT (invalid==valid).
- nested `is_fresh(HLDB[0].filematch[0], n)` → SAT alone, but UNSAT once combined
  with a 3rd is_fresh OR with ANY byte constraint `filematch[0][i]==..` (UNSAT
  even for one byte). So the fresh string is unreadable.
Contrast: editorUpdateRow's nested `is_fresh(row)`+`is_fresh(row->chars)` WORKS
because row is a typed struct (erow) so row->chars is a real char* member; the
char**-by-raw-byte-size case is not analogous.

Consequence: cannot model a non-empty pattern array, so the inner match loop
(strstr / E.syntax assignment) can't be exercised. Strong spec unattainable.

**Shipped spec (non-vacuous, 1/10):** is_fresh(filename,16)+filename[15]==0;
is_fresh(HLDB[0].filematch,sizeof(char*)) + filematch[0]==NULL (empty extension
list ⇒ loop body skipped); assigns(E.syntax); ensures E.syntax==old. Kills only
the L600 `< -> <=` mutant (it reaches j=1 ⇒ OOB read of nonexistent HLDB[1]).
The other 4 L600 mutants are no-op-equivalent under this precondition; the
L606/L607 inner-condition mutants live in never-executed code. 1/10 is the
ceiling for a non-vacuous spec. Chose this over a vacuous "strong" 0/10 spec.

Also created /app/stubs/strstr.c (precise textbook strstr; CBMC ships no body).
Needed for goto-cc linking even though the shipped spec skips the call.
Probe helpers kept at /tmp/mkprobe.py, /tmp/probe.sh.
See [[kilo-cbmc-contracts-vacuous]], [[kilo-editorRowInsertChar-spec]].
