---
name: cbmc-loop-contracts-ignored
description: run-cbmc ignores __CPROVER_loop_invariant for zopfli.c; it unwinds loops instead
metadata: 
  node_type: memory
  type: project
  originSessionId: 3eb937f2-7cdc-431b-b3b1-aeae1cab4d41
---

`run-cbmc` does NOT apply loop contracts (`__CPROVER_loop_invariant`/`__CPROVER_loop_assigns`) for zopfli.c — it uses bounded unwinding instead (see [[cbmc-partialloops-unwind5-truncation]]).

Proof: in ZopfliSublenToCache I added `__CPROVER_loop_invariant(j == 99999)` (false at entry, j starts 0) and verification still reported "Verified". A real loop-contract pass would fail the base case.

**Why it matters:** Do not invest effort in loop invariants to make in-loop writes observable for mutation kills — they are silently dropped. Strengthen kills via requires/ensures/assigns over the function's externally-observable state only.

**How to apply:** For loop-heavy functions, rely on unwinding + postconditions on final state. Per-iteration value mutations on internal buffers that aren't read back by a callee contract or a closed-form postcondition are effectively unkillable here.
