---
name: cbmc-singletest-no-mutable-operators
description: "single_test verifies with empty-assigns; pure ZopfliDeflate dispatcher, no mutable operators"
metadata: 
  node_type: memory
  type: project
  originSessionId: b13c11ee-8bd7-4d77-9c27-93194d4481ae
---

`single_test` in zopfli.c verifies SOUNDLY with a tight contract: requires
`btype in {0,1,2}`, `is_fresh(in, 9)`, `in[8]=='\0'` (so strlen(in) is well
defined), and an EMPTY `__CPROVER_assigns()`. All its writes are stack locals
(options, out=0, outsize=0, bp=0), so nothing caller-visible is assigned.

Mutation: "no mutable operators" — body is constant field assignments plus one
forwarded ZopfliDeflate call. Nothing to kill; verification is the only signal.

Notably it verifies despite the [[cbmc-zoflideflate-depth-vacuity]] callee
contract pinning `*outsize==3` / `is_fresh(*out,8)` while single_test passes
out=0/outsize=0 — CBMC accepted it (vacuous/depth-walled callee replacement, not
a precondition failure). Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.
