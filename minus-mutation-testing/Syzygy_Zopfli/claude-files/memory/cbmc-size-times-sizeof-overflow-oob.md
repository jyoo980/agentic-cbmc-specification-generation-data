---
name: cbmc-size-times-sizeof-overflow-oob
description: "is_fresh(p, size*sizeof) + a forall dereferencing p[i] for i<size gives spurious OOB unless size*sizeof is bounded against overflow"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 8e9fd423-8b00-4952-996a-59f3a4c0d360
---

When a contract has `is_fresh(p, n * sizeof(*p))` (n a symbolic `size_t`) AND a `__CPROVER_forall { size_t i; i < n ==> ... p[i] ... }`, CBMC reports a spurious `pointer outside object bounds` on `p[i]` — even though `i < n`.

**Why:** CBMC lets `n` be huge, so `n * sizeof(*p)` overflows `size_t` and wraps to a *small* value. is_fresh then allocates that small (wrapped) region — the `malloc` max-allocation assumption checks the wrapped value and passes — but the region is now smaller than `n` elements, so `p[i]` for large `i < n` is genuinely OOB in the model.

**How to apply:** add a no-overflow precondition: `__CPROVER_requires(n <= (~(size_t)0) / sizeof(*p))`. Then `n*sizeof` can't wrap and `i < n` keeps `p[i]` in range. Confirmed fix for `GetStatistics` in Syzygy_Zopfli/c_code/zopfli.c (store->size).

Related: prefer pure-integer antecedents in foralls with the dereference in the consequent (`i < n ==> p[i]...`), per the proven contracts in that file — but that alone does NOT fix the overflow case; the size bound is what's needed.

Also: `__CPROVER_assigns` must come BEFORE `__CPROVER_loop_invariant`/`__CPROVER_decreases` in a loop contract, and each `__CPROVER_forall` needs a distinct quantifier variable name (no reuse across clauses).
