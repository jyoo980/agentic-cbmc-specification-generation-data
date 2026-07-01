---
name: kilo-editorreadkey-spec
description: "Verifying editorReadKey in /app/kilo/kilo.c: needed a new read stub; spec is a return-value range bound; kill score is inherently 0 (nondeterministic-read input)"
metadata: 
  node_type: memory
  type: project
  originSessionId: c5459148-44ea-4c3e-97ec-c81c16e70b1f
---

`editorReadKey(int fd)` in `/app/kilo/kilo.c` verifies successfully (exit 0).

**Two gotchas:**

1. **Needed a new `read` stub.** `editorReadKey`'s external callees are `exit`
   (stubbed) and `read` (was NOT stubbed). With no stub, CBMC pulls in its
   built-in `<builtin-library-read>` model, whose internal `read::1::1::error`
   bool symbol crashes `goto-instrument --enforce-contract` assigns-frame
   instrumentation ("no definite size for lvalue target") — exactly the failure
   class `exit.c` documents. Fix: wrote `/app/stubs/read.c`, a nondet model:
   `ssize_t result; assume(-1 <= result <= count); if (result>0)
   __CPROVER_havoc_slice(buf, result); return result;`. run-cbmc
   auto-resolves it via the call graph's `external` list — no flag needed.

2. **char is UNSIGNED on aarch64.** First spec used `>= -128 && <= 127` and the
   postcondition FAILED because `return c` (c is `char`) yields 0..255. Use the
   signedness-independent idiom instead:
   `(char)__CPROVER_return_value == __CPROVER_return_value` to mean "fits in a
   char". Final ensures: that OR `(rv >= ARROW_LEFT && rv <= PAGE_DOWN)`
   (special keys 1000..1008).

**Kill score is 0/21 and that is EXPECTED here, NOT vacuity.** This function has
NO `requires` clause, so it cannot be UNSAT-vacuous (unlike the is_fresh funcs in
[[kilo-cbmc-contracts-vacuous]]). The proof is genuine. But the function's only
output is the return value, and its only input is the nondeterministic byte
stream from `read()`. For almost any target return value there exists a byte
sequence producing it, so the output *range* is invariant under body mutations —
a range postcondition cannot kill mutants that merely permute the byte→key
mapping. No stronger postcondition is expressible without observing the internal
read bytes (which a contract cannot reference). The range bound is the strongest
intended spec achievable. Don't chase the 0 here.
