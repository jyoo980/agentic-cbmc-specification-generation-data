---
name: cbmc-findlargestsplittableblock-global-init-vacuity
description: "FindLargestSplittableBlock verifies soundly but kill 0/17; global static init alone exhausts depth 200 (even npoints=0 can't reach exit); forall-in-requires silently dropped"
metadata: 
  node_type: memory
  type: project
  originSessionId: e1dfffd0-0e18-4512-9645-c7c1ffbf5511
---

`FindLargestSplittableBlock` in `Syzygy_Zopfli/c_code/zopfli.c` (tiny leaf, no
callees): verifies soundly via `run-cbmc ... -I /app/Syzygy_Zopfli/stubs`
but **kill 0/17** — depth-200 wall, same family as [[cbmc-getbyterange-concrete-pin-kills]]
and the many other depth-vacuous functions in this file.

**Novel proof the wall is global static init, not the loop:** probed exit
reachability with a `__CPROVER_ensures(__CPROVER_return_value == 99)` clause —
it passes (vacuous) at depth 200 but FAILS at depth 1000+ (exit really reached).
Shrinking the pinned `npoints` from 3 down to **0** (single loop iteration, no
splitpoint reads) STILL leaves the exit unreachable at depth 200. So the
`--depth 200` budget is exhausted by the file's "Adding nondeterministic
initialization of static/global variables" (huge const tables) BEFORE the
function body completes — independent of object sizes or loop count. No contract
change can recover kills here. In-loop memory checks (line 5337/5338 splitpoints
reads) are reached for the first iteration only, which is why even the OOB
mutant `splitpoints[i+1]` (first OOB at i=2) survives.

**Gotcha — `__CPROVER_forall` in `__CPROVER_requires` was silently dropped:**
a sorted precondition written as
`__CPROVER_forall { size_t k; (k+1 < npoints) ==> splitpoints[k] <= splitpoints[k+1] }`
did NOT constrain the array — a depth-2000 trace showed splitpoints = {4,0,7}
(unsorted), so `end - start` underflowed (size_t) and selected a backwards block,
failing `*lend > *lstart`. Replacing the forall with explicit per-element
conjuncts (`splitpoints[0] <= splitpoints[1]`, etc., npoints pinned to 3) made
the contract SOUND (verifies clean at depth 2000). Lesson: with a constant-pinned
small array, prefer explicit conjuncts over `__CPROVER_forall` in requires.

Final contract left in place: pin lz77size==8, npoints==3, is_fresh on all
pointers, explicit per-element bounds + ascending-sortedness, assigns(*lstart,*lend),
and ensures (return∈{0,1}; found ⟹ *lend>*lstart, *lstart<lz77size,
*lend<=lz77size-1, done[*lstart]==0). Sound and strong; just undischargeable
non-vacuously at the harness's fixed depth 200.
