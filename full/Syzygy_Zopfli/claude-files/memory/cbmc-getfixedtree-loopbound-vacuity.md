---
name: cbmc-getfixedtree-loopbound-vacuity
description: GetFixedTree verifies with full functional postcondition but kill 0/25; all mutants are loop-bound operators never reached under unwind cap
metadata: 
  node_type: memory
  type: project
  originSessionId: f2c155c8-7038-4799-aae9-93f34091a0bd
---

`GetFixedTree` in `/app/Syzygy_Zopfli/c_code/zopfli.c` (~line 1845) writes 288
`ll_lengths` and 32 `d_lengths` entries via five constant-bound loops
(144/256/280/288/32). I gave it a complete functional spec: `is_fresh` requires
on both arrays (288/32 * sizeof(unsigned)), `object_whole` assigns, and five
`__CPROVER_forall` ensures pinning every entry to its exact value (8/9/7/8/5).

Baseline VERIFIES, but mutation kill = 0/25. All 25 mutants are RELATIONAL
operator mutations on the five loop conditions (`< -> <=`, `>`, `>=`, `==`,
`!=`). These are boundary mutations that only diverge when the loop reaches its
large constant bound — but the harness unwind cap is far below 144/256/etc, so
none are reachable/distinguishable. Same root cause as [[cbmc-harness-ignores-loop-contracts]]
and [[cbmc-encodetree-loop-depth-vacuity]]: spec is already maximally strong;
the gap is the unwind cap, not the contract. Cannot raise kill score.

Build notes: needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h. Each
`__CPROVER_forall` block MUST use a distinct iterator var name (k1..k5) or
goto-cc errors "redeclaration with no linkage / CONVERSION ERROR".
