---
name: cbmc-abfree
description: abFree verifies with frees(ab->b); no mutants exist so kill score is 0/0 (max)
metadata: 
  node_type: memory
  type: project
  originSessionId: 13fad36f-5e61-4e6b-8927-84a9eaf31918
---

`abFree` in /app/kilo/kilo.c has **no mutants** (`get-mutants` → "No mutant(s) generated ... no mutable operators"), so its kill score is 0/0 — nothing to strengthen.

Verifying spec (body is just `free(ab->b)`):
- requires `is_fresh(ab, sizeof(*ab))`, `ab->len > 0 && ab->len <= 8`, `is_fresh(ab->b, ab->len)`
- `assigns(__CPROVER_object_whole(ab->b))` **and** `frees(ab->b)` — both needed; with empty `assigns()` the `free` triggers `abFree.assigns.1 ... is assignable: FAILURE`. Same object_whole+frees pattern as [[cbmc-editordelchar]]/editorFreeRow.

Verifies SUCCESSFUL under canonical run_cbmc (--partial-loops --unwind 5, --depth 200), no callee contracts to replace. Related: [[cbmc-abappend]].
