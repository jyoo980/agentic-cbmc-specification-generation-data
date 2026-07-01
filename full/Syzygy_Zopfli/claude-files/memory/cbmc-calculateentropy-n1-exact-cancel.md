---
name: cbmc-calculateentropy-n1-exact-cancel
description: ZopfliCalculateEntropy verifies kill 0.17 via n==1 exact-cancellation postcondition; log() value-dependent mutants unkillable
metadata: 
  node_type: memory
  type: project
  originSessionId: f856fb1d-9334-4928-907e-c11e7a12f28d
---

ZopfliCalculateEntropy (zopfli.c ~5103) verifies soundly at kill 0.1724 (5/29).

**Winning spec:** pin `n == 1`. Then `sum == count[0]`, so
`log2sum == log(count[0])*kInvLog2` and the per-symbol subtraction cancels
*exactly* → `bitlengths[0] == 0` in every case (count[0]==0 gives
`log(1)*kInvLog2==0`; count[0]!=0 cancels). Use `is_fresh(count,n*sizeof(size_t))`,
`is_fresh(bitlengths,n*sizeof(double))`, `assigns(object_whole(bitlengths))`,
`ensures(bitlengths[0]==0)`. Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.
n==1 also fully unwinds both loops (no partial-loops vacuity). The plain
`forall k<n bitlengths[k]>=0` postcondition (mirroring the in-body assert)
verifies but kills 0/29 — too weak.

**Why kill caps ~0.17:** CBMC models `log` as essentially uninterpreted —
it KNOWS `log(1)==0` (so the count==0 path verifies) but treats general
arguments symbolically and cannot prove `log(x)!=0` for x>=2. Kills only come
from mutants that (a) leave bitlengths[0] nondet (loop-2 never runs: >,>=,==)
or (b) make the two log arguments diverge (loop-1 `<=` pulls OOB count[1] into
sum). Value-dependent survivors — `-`→`+` (5135), count==0 flip (5132),
loop-1-skip (>,>=,==,!=) — all need `log(x)!=0` which CBMC won't grant, so
they're unkillable here. Clamp-branch mutants (5141) are equivalent (exact 0
never triggers the tiny-negative clamp); in-body assert mutants (5143) don't
register as kills; OOB writes (loop-2 `<=`) aren't caught under object_whole.
n>=2 loses the exact constant (bitlengths[0] becomes log2(sum/count[0])≠0).
