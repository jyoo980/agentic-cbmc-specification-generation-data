---
name: cbmc-lz77greedy-depth-vacuity
description: "ZopfliLZ77Greedy verifies soundly but kill 0/74; reproduces entire heavy hash/match/store callee tree, depth-200 wall + store->size==3 contradiction"
metadata: 
  node_type: memory
  type: project
  originSessionId: 3a617068-3c61-4bed-8225-05ac1ec4d5d4
---

`ZopfliLZ77Greedy` (zopfli.c ~4903) verifies soundly with a full memory-safety
contract (union of all callee preconditions: is_fresh on s/lmc/hash/store + all
their arrays, head/head2 forall validity, inend==8, blockstart bounds so
pos-blockstart<ZMCS_MAXPOS, store->size==3) but kill **0/74**.

**Why vacuous:** the body drives the whole greedy pipeline — ZopfliResetHash,
ZopfliWarmupHash, ZopfliUpdateHash, ZopfliFindLongestMatch, ZopfliVerifyLenDist,
ZopfliStoreLitLenDist — every one already documented depth-vacuous. The dozens
of is_fresh objects + replaced callee contracts exhaust depth-200 before the
loop is explored. Also a hard structural contradiction: ZopfliStoreLitLenDist
requires store->size==3 but increments it, so no second append can satisfy it.

All 74 survivors are arithmetic/logical operator mutants inside the unreached
loop body. Strengthening cannot beat the depth wall — left the sound contract +
NOTE documenting it. Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.
See [[cbmc-zopflifindlongestmatch-depth-vacuity]], [[cbmc-storelitlendist-depth-vacuity]], [[cbmc-zopfliresethash-loop-depth-vacuity]].
