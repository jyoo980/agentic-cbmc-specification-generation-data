---
name: cbmc-singletest-no-mutable
description: "single_test verifies NON-vacuously with \"no mutable operators\"; test driver, only a minimal is_fresh(in,1) requirement needed"
metadata: 
  node_type: memory
  type: project
  originSessionId: a97833d3-62b6-4761-a7f0-103dc08ea129
---

`single_test` in zopfli.c VERIFIES with "no mutable operators" (strongest
outcome, kill score vacuously maximal). It is a test driver: sets local
ZopfliOptions fields, computes `insize = strlen(in)`, calls ZopfliDeflate.
Body has only assignments + strlen + one call — nothing the mutator targets.

Spec is a single `__CPROVER_requires(__CPROVER_is_fresh(in, 1))` so strlen has a
valid base pointer. Notably the callee ZopfliDeflate's strong contract requires
`*outsize >= 1` and fresh `*out`, yet single_test passes the hardcoded
`outsize = 0`, `out = 0` — this did NOT cause a precondition failure (ZopfliDeflate
is not contract-replaced into an asserting call here, or its check is unreached).
Verified identically with and without the contract. See [[cbmc-copystats-no-mutable-operators]],
[[cbmc-deflate-vacuity]].
