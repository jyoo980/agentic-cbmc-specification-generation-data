---
name: zdp-depth200-vacuity
description: "ZopfliDeflatePart verifies in btype==0 regime but 0/44 kills @depth200; classic driver vacuity, btype!=0 divert-kill killable @600"
metadata: 
  node_type: memory
  type: project
  originSessionId: eaa696fd-3804-4c50-937e-75608f5b974d
---

ZopfliDeflatePart (top-level DEFLATE driver, zopfli.c ~line 5930) verifies @depth200 with a sound contract but scores **0/44** kills — confirmed depth-200 vacuity, like every sibling driver ([[alzbat-depth200-vacuity]], [[zcbsat-depth200-vacuity]], [[addlz77block-depth200-vacuity]], [[zbs-depth200-vacuity]]).

**Why only btype==0 is verifiable:** both the btype==1 and btype==2 paths build a local `ZopfliLZ77Store`, fill it via a replaced squeeze callee (ZopfliLZ77OptimalFixed / ZopfliLZ77Optimal) whose contract `assigns(object_whole(store))` — havocking the 8 array pointers to nondet — then call `ZopfliCleanLZ77Store`, whose replace-contract precondition demands `is_fresh` of each of those (now nondet) pointers → undischargeable. Same wall as the family. So pin `requires(btype==0)`; body is just AddNonCompressedBlock, forward its precondition (AddBit-ready frame: `*bp` in 1..7, one-byte `*out`, `*outsize==1`, `is_fresh(in,inend)`, `inend>=1`) and re-export `ensures(*bp==0)`.

**All 44 mutants are in the btype==1/btype==2 inner statements** (lines ~5946–6029: the two dispatch guards, both block-splitting loops, ternaries, totalcost compare, final emit loop). In the btype==0 regime that whole region is dead code → mutants equivalent → survive.

**The one apparently-killable mutant is the `if (btype==0)`→`btype!=0` dispatch guard:** it diverts the call into the btype==2 pipeline where ZopfliBlockSplit's replaced precondition `inend<10` is violated by a large inend (I left inend unbounded above precisely for this). Verified KILLED @depth600 (3 fails) and @900 (10 fails) — but VACUOUS @200 (reaching any btype==2 callee-precondition assertion costs >200 steps through the is_fresh-heavy prologue). Original verifies @200 AND @600.

**Don't chase kills** — the depth is hardcoded to 200 by the official flow; the spec is provably strong (divert-kill fires @600). Kill script: `/app/kill_zdp.py`.
