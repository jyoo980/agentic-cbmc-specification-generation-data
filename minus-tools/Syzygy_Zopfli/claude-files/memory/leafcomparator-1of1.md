---
name: leafcomparator-1of1
description: "LeafComparator 1/1 kill — leaf qsort comparator, exact-value postcond kills the +/- mutant"
metadata: 
  node_type: memory
  type: project
  originSessionId: 3071303b-f9eb-46a1-bf8b-2582e8cc6033
---

LeafComparator in /app/Syzygy_Zopfli/c_code/zopfli.c (`return ((const Node*)a)->weight - ((const Node*)b)->weight`, weight is size_t, return is int). Leaf, no loops, like [[gls-6of6]].

Single avocado mutant: `-` → `+`. Killed 1/1 @depth200 with leaf flow (goto-cc --function, goto-instrument --partial-loops --unwind 5, --enforce-contract, cbmc --depth 200).

Spec: `is_fresh(a,sizeof(Node))` + `is_fresh(b,sizeof(Node))` + exact postcond `return_value == (int)(((const Node*)a)->weight - ((const Node*)b)->weight)`. The `(int)` cast is needed since weight is unsigned size_t and return is int. No bounds needed — `-` vs `+` differ for any nonzero weight, CBMC finds the counterexample.

Scripts: /app/_verify_leafcomparator.sh, /app/kill_leafcomparator.sh.
