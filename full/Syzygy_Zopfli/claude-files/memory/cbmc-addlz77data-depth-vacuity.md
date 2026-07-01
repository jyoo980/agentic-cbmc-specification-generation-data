---
name: cbmc-addlz77data-depth-vacuity
description: AddLZ77Data verifies soundly but kill 0; depth-200 exhausted by AddHuffmanBits/AddBits is_fresh before loop writes
metadata: 
  node_type: memory
  type: project
  originSessionId: fca500ab-0485-456b-8584-ad26c5233e0e
---

`AddLZ77Data` in `Syzygy_Zopfli/c_code/zopfli.c` verifies soundly under the fixed
harness budget but scores kill 0 (all 40 mutants survive) — same depth-200 wall
as [[cbmc-adddynamictree-depth-vacuity]], [[cbmc-encodetree-loop-depth-vacuity]]
and the AddBits/AddHuffmanBits notes.

**Why:** Every visible effect (advance `*bp`, grow `*outsize`, append `*out`)
flows only through the replaced AddHuffmanBits/AddBits contracts. Establishing
their nested `is_fresh(out) && is_fresh(*out, 8)` + `*outsize==3`/`*bp<=7`
preconditions at each unwound call exhausts the 200-step depth before the loop
body's writes or the post-loop `assert(expected_data_size==0 || testlength==...)`
are reached. Proof of vacuity: that body assert is checked against a fully
nondeterministic `expected_data_size` yet verification still succeeds — it is
never reached.

**How to apply:** Don't chase the kill score on output-emitting zopfli functions
that loop over AddHuffmanBits/AddBits — kill 0 is the depth-bounded ceiling, not
a spec weakness. Write the strongest sound contract and document the vacuity.

**Build note:** this file needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs`
(stubs provide `x86_64-linux-gnu/bits/types/FILE.h`); without them goto-cc fails
preprocessing. Also: multiple `__CPROVER_forall` clauses in one contract must each
use a *distinct* quantifier variable name, and that name must differ from any
body local (e.g. `i`) — else goto-cc errors "redeclaration with no linkage".
