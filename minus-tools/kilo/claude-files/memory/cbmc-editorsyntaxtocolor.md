---
name: cbmc-editorsyntaxtocolor
description: "editorSyntaxToColor verifies, no mutants exist (0/0 max); pure switch fully specified by per-case ensures"
metadata: 
  node_type: memory
  type: project
  originSessionId: e612af06-4630-4426-ad34-c26edeb50ee1
---

`editorSyntaxToColor(int hl)` in /app/kilo/kilo.c is a pure switch mapping HL_* token types to terminal color ints (36/33/32/35/31/34, default 37). `get-mutants` reports "No mutant(s) generated (no mutable operators)" → kill score is 0/0 (max). Like [[cbmc-abfree]], strength is moot.

Spec: `__CPROVER_assigns()` (empty — pure) + one `__CPROVER_ensures(hl == HL_X ==> __CPROVER_return_value == N)` per case, plus a catch-all ensures (all-not-equal ==> 37). Verifies SUCCESSFUL via canonical pipeline (goto-cc / goto-instrument --partial-loops --unwind 5 / --enforce-contract / cbmc --depth 200). No callees, no stubs needed.
