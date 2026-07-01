---
name: cbmc-disablerawmode
description: disableRawMode verifies with assigns(E.rawmode)+ensures(E.rawmode==0); 0 mutants (maxed); needs stubs/rawmode.c for tcsetattr
metadata: 
  node_type: memory
  type: project
  originSessionId: f306c023-3c86-4e7c-b18e-5dbe3c8f64c6
---

`disableRawMode(int fd)` in kilo.c verifies and is maximally strong: it has
**no mutants** (`get-mutants` → "no mutable operators", 0/0).

Contract: `__CPROVER_assigns(E.rawmode)` + `__CPROVER_ensures(E.rawmode == 0)`.
The postcondition holds unconditionally — the guarded branch clears E.rawmode,
the skipped branch means it was already 0. tcsetattr is modeled as a pure read
in [[cbmc-enablerawmode]]'s `stubs/rawmode.c` (returns rm_tcset, assigns
nothing), so E.rawmode is the only written object.

Build: `goto-cc kilo.c rawmode.c --function disableRawMode` → `goto-instrument
--partial-loops --unwind 5` → `--enforce-contract disableRawMode` → `cbmc
--depth 200` ⇒ VERIFICATION SUCCESSFUL.
