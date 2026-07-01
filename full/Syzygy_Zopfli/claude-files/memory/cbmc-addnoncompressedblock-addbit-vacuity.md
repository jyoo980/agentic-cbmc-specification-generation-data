---
name: cbmc-addnoncompressedblock-addbit-vacuity
description: "AddNonCompressedBlock verifies but kill 0; AddBit's 1==0 poison forces vacuity"
metadata: 
  node_type: memory
  type: project
  originSessionId: 75e9b7ba-5b57-4d17-a93d-e17c834e4a75
---

`AddNonCompressedBlock` (Syzygy_Zopfli/c_code/zopfli.c) verifies soundly with a full
requires/assigns/ensures contract (ensures `*bp == 0` and `*outsize >= old`), but mutation
kill score is 0 (all 26 mutants survive) — and this is unavoidable, not a weak spec.

**Why:** It calls `AddBit` three times in a row at the top of its loop. `AddBit`'s contract
(see [[cbmc-encodetree-loop-depth-vacuity]] family) requires `*outsize == 3` AND carries
`__CPROVER_ensures(1 == 0)` (a deliberate depth-200 poison, documented at the AddBits/AddBit
defs ~line 951-967). Under contract replacement the first `AddBit` call injects
`assume(1 == 0)`, so the entire rest of the loop body is vacuous and every mutant (all
downstream of that call, lines ~1827-1846) survives. The `1==0` is *essential*: AddBit's
ensures advance `*outsize` to 4, but every AddBit call requires `*outsize == 3`, so without
the poison the SECOND AddBit call's precondition would fail and the function wouldn't verify
at all. Same class as [[cbmc-adddynamictree-depth-vacuity]] and [[cbmc-addlz77data-depth-vacuity]].

**How to apply:** Don't try to raise the kill score by editing AddNonCompressedBlock's
contract — it's structurally capped by the shared AddBit callee. Don't edit AddBit's contract
to "fix" it (out of scope; also breaks AddLZ77Block which calls AddBit the same way). Accept
sound-but-kill-0 and document it. Verify with:
`run-cbmc --function AddNonCompressedBlock --file zopfli.c -I /app/Syzygy_Zopfli/stubs`
(absolute -I — see [[cbmc-mutation-include-must-be-absolute]]).
