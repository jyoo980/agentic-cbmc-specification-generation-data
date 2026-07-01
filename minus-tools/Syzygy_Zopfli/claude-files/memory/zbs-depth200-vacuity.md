---
name: zbs-depth200-vacuity
description: "ZopfliBlockSplit verifies @200 in tiny-input regime but 0/13 kills; greedy NULL-array precondition fails BEFORE body is reachable, so mutants unkillable in scoring flow, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: f1b476d7-46a6-4f50-ae9f-e2c754b64b4c
---

ZopfliBlockSplit (zopfli.c ~5501) is a driver: ZopfliInitLZ77Store →
ZopfliInitBlockState → ZopfliAllocHash → ZopfliLZ77Greedy → ZopfliBlockSplitLZ77
→ conversion loop (LZ77 split positions → byte offsets) → final
`assert(*npoints == nlz77points)`. Verifies @depth200 with a SOUND tiny-input
contract: `is_fresh(options/in/splitpoints/npoints) + instart<=inend + inend<10
+ assigns(*npoints,*splitpoints) + ensures *npoints==0 && *splitpoints==NULL`.
Replace-call all 8 callees with their real contracts (`/app/_verify_zbs.sh`,
stub `/app/stubs/cprover_alloc.c`).

Why inend<10 is sound & exact: greedy emits <=1 command per advanced byte so
store.size<=inend<10, which discharges ZopfliBlockSplitLZ77's real precondition
(`requires size<10`, `assigns()` = early-return, does nothing). So nlz77points
stays 0, the `if(nlz77points>0)` conversion loop is skipped, and *npoints/*splitpoints
keep their line-5518/5519 zero-init. NOT a CBMC-arg hardcode — 10 is the code's
own `if (lz77->size < 10)` threshold at line 5426.

0/13 kills @depth200 and INHERENTLY UNKILLABLE in the scoring flow — do NOT chase:
- This is a CLEANER vacuity than the sibling [[zbslz-depth200-vacuity]]. The
  official flow replace-calls greedy with its REAL contract, whose
  `is_fresh(store->litlens ...)` precondition is on the all-NULL arrays
  ZopfliInitLZ77Store leaves. That assertion becomes reachable at depth ~350,
  where the ORIGINAL itself FAILS (measured: SUCCESS@200/280/300, FAILED@350).
  The conversion body only becomes reachable ~600. So there is NO depth where
  (original verifies AND body reachable): below ~350 body unreachable → 0 kills;
  at/above ~350 original fails on the greedy NULL precondition, not the mutant.
- Proven maximally strong via weak-stub harness (`/app/_verify_zbs_strength.sh`,
  stubs `/app/stubs/blocksplit_callees.c` — drop greedy's NULL-array preconds,
  keep `ensures store->size<=inend`). @depth600 body reachable: 4/13 killed —
  the assert mutant (#0, `==`→`!=`) and the 3 guard mutants (`<=0`,`>=0`,`==0`,
  #10-12) that flip `nlz77points>0` so the loop is entered and derefs the NULL
  lz77splitpoints. The other 9 are EQUIVALENT mutants: #9 (`<0`) is a size_t
  tautology; #1-8 sit inside the `if(nlz77points>0)` block, genuinely
  unreachable in the only sound (nlz77points==0) regime. So 4/13 is the true
  ceiling, and even that needs depth >> the failing-original threshold.
Related: [[zbslz-depth200-vacuity]], [[avocado-depth200-vacuity]], [[zlz77greedy-depth200-vacuity]].
