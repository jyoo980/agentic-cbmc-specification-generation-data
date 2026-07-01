---
name: cbmc-recursive-isfresh-helper-blocks-callers
description: A callee contract using a recursive is_fresh helper (valid_chain_nodes) crashes goto-instrument when that callee is replaced into a caller
metadata:
  node_type: memory
  type: project
  originSessionId: full
---

In `/app/Syzygy_Zopfli/c_code/zopfli.c`, `BoundaryPM`'s contract has
`__CPROVER_requires(valid_chain_nodes(lists, 15))`, where `valid_chain_nodes` is
a **recursive** helper that calls `__CPROVER_is_fresh`. This verifies fine when
`BoundaryPM` is *enforced* standalone (the requires is an assumed function call;
recursion is harmlessly unwound). But the avocado harness verifies a caller by
running `goto-instrument --replace-call-with-contract <each in-file callee>`.
When `BoundaryPM` is *replaced* into a caller, goto-instrument inlines the
contract expression and the recursive `valid_chain_nodes` throws
`Numeric exception : 0` — killing the pipeline before cbmc ever runs. The
recursion-inlining retry in `run_cbmc_and_mutation_testing.py` only fires on
`Recursive call to '<function_under_verification>'`; here the recursion is in
`valid_chain_nodes`, so no retry triggers.

**Consequence:** `ZopfliLengthLimitedCodeLengths` (the only in-file caller of
`BoundaryPM`) cannot be instrumented at all — it fails at goto-instrument, never
reaching cbmc, so it cannot even reach a (vacuous) exit-0. A `qsort` stub
(`/app/stubs/qsort.c`, the auto-indexed `_STUBS_DIR = parents[2]/"stubs"` = `/app/stubs`)
clears the separate "no body for qsort" message but does NOT fix the crash.

**Second, independent blocker:** even if the crash were fixed, `BoundaryPM` /
`BoundaryPMFinal` require `is_fresh(lists, 16 * sizeof(*lists))`, but the caller
allocates `lists = malloc(maxbits * sizeof(*lists))` with
`maxbits <= numsymbols-1 <= 7`. So the 16-entry precondition is unsatisfiable at
the call site and would FAIL even without the crash. The callee contracts were
written for standalone verification with an over-approximated lists size that the
real caller never allocates.

**How to apply:** A caller of `BoundaryPM` is unverifiable here without editing
`BoundaryPM`/`BoundaryPMFinal` contracts (recursive helper + 16-entry lists),
which would require re-verifying those already-passing callees and risks breaking
them — not worth it, and the depth wall ([[cbmc-depth-200-object-limit]]) makes
the result vacuous anyway. Keep the strongest *correct* caller contract for
documentation and stop. See also [[cbmc-is-fresh-gotchas]].
