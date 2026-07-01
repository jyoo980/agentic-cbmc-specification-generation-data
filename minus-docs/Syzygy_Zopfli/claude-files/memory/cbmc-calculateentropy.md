---
name: cbmc-calculateentropy
description: ZopfliCalculateEntropy verifies non-vacuously 5/29; survivors are CBMC log()-model floating-point artifacts
metadata: 
  node_type: memory
  type: project
  originSessionId: d4693ffc-6783-4130-87b4-a073588fc76f
---

`ZopfliCalculateEntropy` in zopfli.c verifies **non-vacuously, 5/29 kills** (kill score 0.17). Spec: `n` in `[1, ZOPFLI_NUM_LL]`, is_fresh on count[n] and bitlengths[n], `assigns(object_whole(bitlengths))`, ensures `forall k<n: bitlengths[k] >= 0 && bitlengths[k] <= 64`.

The `>= 0` mirrors the body's assert; `<= 64` is a sound bound (log2sum = log(unsigned sum)*kInvLog2 <= ~32). The upper bound verifies but adds 0 kills.

**Survivors are all CBMC `log()`-model limitations** (unlike the depth-200 vacuity in most zopfli funcs — this one is NOT vacuous):
- Clamp/assert mutants (the `< 0 && > -1e-5` window, `assert >= 0`): only observable if a float lands in `(-1e-5, 0)`; CBMC's log model can't witness that window → effectively equivalent.
- Loop relational mutants: first loop only affects bitlengths through `log(sum)` (float value unobservable); `i != n` is equivalent to `i < n`. Second-loop `i <= n` is a true OOB write but CBMC didn't flag it.
- `count[i]==0` branch-swap (→ `log(0)` = +inf) and `-`→`+` arithmetic: CBMC's log(0)/magnitude not witnessed as postcondition violation even with the `<=64` bound.

`__CPROVER_isfinite()` CANNOT be used inside a `__CPROVER_forall` quantifier ("quantifier must not contain function calls") — use a numeric bound instead. Related: [[cbmc-addweighedstatfreqs-loop-depth-vacuity]] (the 288-iter FP loop class, but that one is vacuous).
