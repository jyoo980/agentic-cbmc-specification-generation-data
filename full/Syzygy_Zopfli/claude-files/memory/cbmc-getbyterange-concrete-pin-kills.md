---
name: cbmc-getbyterange-concrete-pin-kills
description: "ZopfliLZ77GetByteRange — concrete-pinned arrays beat the usual ZopfliLZ77Store kill-0; got 2/5 via memory-safety, value postcondition still depth-truncated"
metadata: 
  node_type: memory
  type: project
  originSessionId: 578056c5-afde-442f-815d-a1214793aad7
---

`ZopfliLZ77GetByteRange` (zopfli.c, leaf, no loops) reads pos[lstart], pos[lend-1],
dists[lend-1], litlens[lend-1]. Strongest correct spec: is_fresh(lz77) +
is_fresh on pos/dists/litlens, lstart<=lend<=2, full value postcondition
restating the body (return == pos[lend-1] + (dists[lend-1]==0?1:litlens[lend-1]) - pos[lstart]).

**Result: verifies, kill 2/5 (0.40)** — better than the usual [[cbmc-depth-200-object-limit]]
kill-0 for the ZopfliLZ77Store family.

**Why pinning to a CONCRETE extent matters:** sizing the fresh arrays `2 * sizeof(...)`
(with lend<=2) instead of the symbolic `lz77->size` lets CBMC detect the out-of-bounds
reads that two mutants produce — `l = lend+1` (ARITH - -> +) and the flipped `if` that
makes `l = lstart-1` underflow. A symbolic huge size masks these (the bad index can still
be < size), giving kill 0. The concrete pin killed those 2 via memory-safety checks.

**Why the other 3 survive (value-mutants on the return expr):** the long (lstart!=lend)
path's value postcondition is **depth-truncated** at the harness's `--depth 200`.
Confirmed by direct cbmc runs: long-path postcondition first reachable at depth ~212
(SUCCESS at 210, the false probe `lstart!=lend ==> rv!=rv` FAILS at >=215; real
postconditions .2/.3 hold at depth 100000 — spec IS correct). The ~205 floor is
`is_fresh` on the 8-pointer struct + global static init; dropping one array is_fresh
only saved ~5 steps, so nothing reaches under 200. The 3 survivors (==/!= ternary,
+/- and -/+ arithmetic) need that value postcondition → unkillable here.

**How to apply:** For tiny leaf functions over a pointer-heavy struct, PIN array
operands to a small concrete length (>= max reachable index) instead of symbolic size —
it buys memory-safety mutant kills even when the value postcondition stays behind the
depth wall. Vacuity-probe a SPECIFIC path with a guarded false ensures
(`guard ==> rv != rv`); an unguarded one only proves the cheapest (short) path is reached.
