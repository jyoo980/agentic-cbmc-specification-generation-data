---
name: rok-vs-isfresh-depth-vacuity
description: __CPROVER_r_ok in requires can exhaust --depth 200 and make whole spec vacuous; use __CPROVER_is_fresh instead
metadata: 
  node_type: memory
  type: feedback
  originSessionId: a46b3d8f-f616-4923-baff-f891687c4493
---

For array/pointer params, prefer `__CPROVER_is_fresh(p, n)` over `__CPROVER_r_ok(p, n)` in the requires clause.

**Why:** `r_ok` asserts validity of a *wild nondeterministic* pointer — its symbolic eval is so expensive it alone exhausted `--depth 200` (the fixed depth run-cbmc uses) *before* the postcondition assertion was ever reached. Result: every spec (even a trivially-false `return_value == 99`) "verified successfully" with 0/N kill — fully vacuous. `is_fresh` instead acts like `p = malloc(n)`, giving a fresh valid object cheaply, leaving budget to reach and check the ensures. Swapping `r_ok`→`is_fresh` on findTarget (2048-clone/2048.c) flipped 0/6 → 6/6 kill with the identical postconditions.

**How to apply:** Diagnose vacuity by temporarily adding a deliberately-false ensures (e.g. `__CPROVER_return_value == 99`); if it still "verifies", the assertions aren't being reached. If a heavy `r_ok` requires is the cause, switch to `is_fresh`. `is_fresh` works inside a `&&` conjunction with scalar conditions (one predicate per pointer). It's already the established pattern for `board` params in 2048.c. Relates to [[cbmc-depth-200-vacuous-loops]] and [[cbmc-depth200-postcondition-eval-cost]] — same fixed-depth budget, different hog (requires-side, not ensures-side).
