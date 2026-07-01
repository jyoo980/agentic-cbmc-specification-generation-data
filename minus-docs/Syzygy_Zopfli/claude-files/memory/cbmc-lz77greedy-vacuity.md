---
name: cbmc-lz77greedy-vacuity
description: ZopfliLZ77Greedy in zopfli.c verifies but 0/74 kills; ~18 is_fresh objects exceed depth 200 so body unreachable
metadata: 
  node_type: memory
  type: project
  originSessionId: 3d846c07-997f-4eac-aab6-2f74d88d4d90
---

`ZopfliLZ77Greedy` (zopfli.c) verifies non-vacuously-looking but yields 0/74 mutation kills. Same depth-vacuity class as its callees [[cbmc-findlongestmatch-vacuity]] (0/157) and the structurally identical sibling [[cbmc-followpath-vacuity]] (0/52): the harness instantiates ~18 `__CPROVER_is_fresh` objects (`s`, `s->lmc`+length/dist/sublen, `h`+head/prev/hashval/head2/prev2/hashval2/same, `store`+litlens/dists/pos/ll_symbol/d_symbol/ll_counts/d_counts, `in`), far exceeding `--depth 200`, so the loop body and all `ZopfliStoreLitLenDist`/`ZopfliVerifyLenDist` appends are unreachable and every mutant survives.

**How to apply:** Don't try to raise the kill score. Reuse FollowPath's contract verbatim (it shares the exact callee set: ResetHash/WarmupHash/UpdateHash/FindLongestMatch/StoreLitLenDist/VerifyLenDist) minus the path/pathsize params — store size==3 + cleared histograms, s->lmc slots length==1/dist==0, both hash arrays fresh. Strong sound spec left in place.
