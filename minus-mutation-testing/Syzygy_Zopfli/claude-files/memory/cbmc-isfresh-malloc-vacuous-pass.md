---
name: cbmc-isfresh-malloc-vacuous-pass
description: "run-cbmc passes is_fresh-requires contracts VACUOUSLY (body unreachable) because malloc isn't linked on the first pipeline attempt"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 5ed42afc-6d77-4b4f-a620-20188c333a48
---

In the Syzygy_Zopfli CBMC harness (`run-cbmc` / `tools/run_cbmc.py`), any
function whose contract uses `__CPROVER_is_fresh` in a `requires` clause is
"verified successfully" **vacuously**: the enforced body is unreachable.

**Why:** enforcing an `is_fresh` precondition needs `malloc` declared to allocate
the assumed fresh object. The first pipeline attempt runs WITHOUT
`goto-instrument --add-library`, so malloc is "not declared", the `is_fresh`
assumption collapses to `assume(false)`, and the whole body becomes unreachable →
every property (including a deliberately false postcondition) reports SUCCESS.
The `--add-library` step only runs on the macro-expansion retry, which fires only
on a "no body for callee" message — a vacuous first-attempt success returns before
that retry.

**How to detect vacuity:** negate the postcondition (e.g. add
`__CPROVER_ensures(1 == 0)`); if it still "verifies successfully", the run is
vacuous. A no-`is_fresh` function like `AbsDiff` DOES catch a false postcondition,
proving the framework itself works — the vacuity is specific to `is_fresh`+malloc.

**Deeper (confirmed 2026-06-25 on SplitCost):** linking malloc is NOT enough.
Even running `goto-instrument --add-library` AFTER `--enforce-contract` (so the
injected malloc has a body) STILL passes a negated postcondition vacuously — for
both single-level `EstimateCost` and nested-`is_fresh` `SplitCost`. Root cause is
broader: the callee's `__CPROVER_replace_requires_is_fresh` has "no body for
function", so the replaced-contract assumption also collapses. So genuine
non-vacuous checking is blocked regardless of malloc; don't keep chasing it.

**Consequence:** this vacuity is universal here — even already-"verified"
functions (EstimateCost, FindLargestSplittableBlock, AddSorted, ...) pass this
way. So a strong, satisfiable, sound `is_fresh` contract passing `run-cbmc`
(exit 0) is the realistic bar; genuine non-vacuous checking is blocked by tool
infra (malloc declaration, plus the [[cbmc-function-pointer-param-crash]] for
callees like FindMinimum). Manually adding `--add-library` to chase a real run
hits further goto-instrument invariant crashes ("no definite size for lvalue
target") on is_fresh replacement of function-pointer-param callees.
