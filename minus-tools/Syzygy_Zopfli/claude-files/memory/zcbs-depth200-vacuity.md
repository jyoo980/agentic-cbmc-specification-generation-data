---
name: zcbs-depth200-vacuity
description: "ZopfliCalculateBlockSize 0/6 kills — inherent depth-200 vacuity, r_ok trick does NOT rescue it (unlike the leaf byte-range fn)"
metadata: 
  node_type: memory
  type: project
  originSessionId: d09b3f84-4c81-4b58-8fe3-c8ea23ec5d35
---

ZopfliCalculateBlockSize (zopfli.c) verifies at --depth 200 with its union-of-callees
contract but kills 0/6 mutants. All 6 mutants live on the btype==0 (stored-block) path
(the two btype branch-decision mutants `btype!=1`/`btype!=0`, plus the rem/blocks/return
arithmetic). They are vacuous@200, killed only @600 (postcondition.2, the exact-value
btype==0 ensures, fails there).

**Why r_ok does NOT help here** (unlike [[zlz77gbr-rok-beats-vacuity]]): I tried splitting
the requires so the btype==0 path uses cheap `r_ok` (lz77/litlens/dists/pos) and the
btype!=0 path keeps `is_fresh` (callees demand it), plus the size*sizeof overflow guard.
Result: still 0/6 at depth 200, mutant still killed only @600. Probing with
`__CPROVER_assert(0)` at the btype==0 branch entry: unreachable@200 even with ALL the heavy
btype!=0 is_fresh/forall requires stripped out — the branch entry only becomes reachable at
depth ~210-250. The postcondition is ~100 steps further still. The byte-range CONTRACT
REPLACEMENT (--replace-call-with-contract ZopfliLZ77GetByteRange: requires-check +
assigns-havoc + ensures-assume) plus the minimal prologue needed to satisfy that contract's
own preconditions already pushes the btype==0 branch past depth 200. The leaf
ZopfliLZ77GetByteRange got 5/5 from r_ok precisely because it has NO callee-contract
machinery in its path; this dispatcher does, so the trick can't recover it.

Also: the r_ok split introduced btype!=0 callee precondition FAILUREs at depth 600 (the
guarded is_fresh didn't satisfy CalculateBlockSymbolSize/GetDynamicLengths preconditions)
— a net regression for no kill-score gain. Reverted to the original is_fresh dispatcher.

**Conclusion: inherent depth-200 vacuity, don't chase.** Same family as
[[cbss-depth200-vacuity]], [[avocado-depth200-vacuity]], [[calculatetreesize-depth200-vacuity]].
Kill script: /app/kill_zcbs.sh
