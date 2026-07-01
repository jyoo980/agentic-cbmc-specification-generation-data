---
name: gls-6of6
description: "GetLengthScore — exact-value ternary contract kills all 6 mutants, no equivalents"
metadata: 
  node_type: memory
  type: project
  originSessionId: d666ad12-a421-4295-be7a-b451ab80076d
---

GetLengthScore (zopfli.c) is a leaf ternary `distance > 1024 ? length - 1 : length`. Verifies @depth200 and scores 6/6 kills with an exact-value postcondition — like [[zmin-equiv-mutant]] / [[absdiff-equiv-mutant]] but with NO equivalent mutants (all 6 comparison/penalty mutants distinguishable: pick distance=500 for <,<=,!=,==; distance=1024 for >=; distance=2000 for length+1).

**Why:** trivial leaf, no loop → no depth-200 vacuity; exact-value `__CPROVER_ensures(return == (distance > 1024 ? length-1 : length))` pins threshold, direction, and penalty.

**How to apply:** added `__CPROVER_requires(length∈[0,258], distance∈[0,32768])` (caller bounds, unsigned-short args) to keep length±1 in int range. Verify/kill scripts: /app/_verify_gls.sh, /app/kill_gls.sh (mirror zmin pattern: goto-cc → --partial-loops --unwind 5 → --enforce-contract → cbmc --depth 200).
