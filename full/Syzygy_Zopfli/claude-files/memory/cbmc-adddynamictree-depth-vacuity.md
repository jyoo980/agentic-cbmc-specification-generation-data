---
name: cbmc-adddynamictree-depth-vacuity
description: AddDynamicTree verifies soundly but kill 0 — depth-200 exhausted by EncodeTree is_fresh checks in search loop
metadata: 
  node_type: memory
  type: project
  originSessionId: 00dab102-0a55-4406-b802-126c06639bf0
---

`AddDynamicTree` (Syzygy_Zopfli/c_code/zopfli.c) verifies with a sound contract
(is_fresh ll_lengths[ZOPFLI_NUM_LL]/d_lengths[ZOPFLI_NUM_D], bp/outsize/out fresh
with *bp<=7, *outsize==3, *out fresh 8B; assigns *bp,*outsize,object_whole(*out)),
but mutation kill score is 0/12.

**Why kill 0:** all 12 mutants live in the 8-iteration search loop that picks
`best`. `best` is only observed through the post-loop EncodeTree call that emits
the chosen tree. EncodeTree is replaced by its contract, whose `is_fresh`
preconditions over the 288- and 32-element length arrays get re-checked at every
unwound iteration, exhausting `--depth 200` before control reaches the post-loop
call. So the output-producing call is never explored → mutants unobservable, any
function-exit ensures is vacuous. This also masks that the final call passes a
non-null `out` while EncodeTree's contract requires `out == NULL` (size-only) —
that precondition mismatch is simply never reached.

Same depth-bounded-vacuity class as [[cbmc-encodetree-loop-depth-vacuity]] and
the AddBits note. Unwind/depth are fixed by the harness; can't be raised. Run
with `-I /app/Syzygy_Zopfli/stubs` (source #includes a stubbed
x86_64-linux-gnu/bits/types/FILE.h).
