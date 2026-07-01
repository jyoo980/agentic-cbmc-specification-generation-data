---
name: cbmc-constant-bound-forall-discharge
description: Discharging a quantified precondition through replace-call-with-contract needs a CONSTANT-bound forall, not a symbolic k<n
metadata:
  node_type: memory
  type: project
  originSessionId: full
---

In `/app/Syzygy_Zopfli/c_code/zopfli.c`, `ZopfliCalculateBitLengths` is a thin
wrapper: `error = ZopfliLengthLimitedCodeLengths(...); assert(!error);`. The
harness verifies it by **replacing** the callee with its contract, so the callee
ensures must guarantee `return == 0` for the assert to hold. The callee returns 1
in two cases: (a) `(1<<maxbits) < numsymbols`, (b) some `frequencies[k] >= 2^(WORDBITS-9)`.
Sound success ensures: `(A && forall k. freq[k] < BOUND) ==> return==0`, with a
matching caller `__CPROVER_requires` forall.

**Key finding:** CBMC will NOT discharge a `forall` GOAL from an identical `forall`
ASSUMPTION when the bound is symbolic (`k < (size_t)n`) — it doesn't instantiate
the assumed quantifier at the negated-goal skolem witness. Result: `assertion !error`
FAILS even though caller requires and callee ensures-antecedent are textually
identical. The scalar conjunct `((size_t)1 << __CPROVER_old(maxbits)) >= n`
discharges fine; only the quantifier is the blocker. (`__CPROVER_old(maxbits)` in
a replaced ensures correctly captures the call-site value.)

**Fix that works:** make the quantifier bound a COMPILE-TIME CONSTANT so CBMC
expands it into a finite conjunction it can discharge. Pin the pointer objects to
the max size (`__CPROVER_is_fresh(p, ZLLCL_MAX_N * sizeof(...))`, sound
over-allocation since `n <= ZLLCL_MAX_N`) and bound BOTH the caller requires forall
and the callee ensures-antecedent forall by `k < (size_t)ZLLCL_MAX_N`. Pin the
size on BOTH caller and callee `is_fresh` so the constant-bound `freq[k]` deref
(k up to MAX) stays in-bounds. With this, `ZopfliCalculateBitLengths` verifies.

Mutation testing reports "no mutable operators" for this function (body is just a
call + assert), so verification success is the only meaningful outcome — kill score
N/A. The callee `ZopfliLengthLimitedCodeLengths` remains UNVERIFIED (pre-existing
goto-instrument crash, see [[cbmc-recursive-isfresh-helper-blocks-callers]]); my
sound ensures is assumed at the caller and doesn't change that.
