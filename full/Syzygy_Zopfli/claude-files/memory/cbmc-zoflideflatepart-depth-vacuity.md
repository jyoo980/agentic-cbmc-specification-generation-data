---
name: cbmc-zoflideflatepart-depth-vacuity
description: ZopfliDeflatePart verifies soundly but kill 0/44; top-level deflate driver hits same depth/store-size-pin wall as sibling drivers
metadata: 
  node_type: memory
  type: project
  originSessionId: c7fef141-bfc2-4d90-a865-e92dcbfec494
---

`ZopfliDeflatePart` (zopfli.c) verifies SOUNDLY but kill 0/44 with a memory-safety
contract: requires fresh options/in/bp/out/*out/outsize, instart<=inend, btype in
{0,1,2}, final 0/1, *bp<=7, *outsize==3, is_fresh(*out,8); assigns {*bp, *outsize,
object_whole(*out)}; ensures *outsize >= old(*outsize). Needs `-I /app/Syzygy_Zopfli/stubs`
for FILE.h.

**Why kill 0 (hard wall, not a spec defect):** all 44 survivors are loop-index
arithmetic (`splitpoints[i-1]` etc.) and `&&`/`||` operators in the btype==2
block-splitting region (lines ~6283–6333). They are unreachable because the replaced
callee contracts (ZopfliLZ77Optimal/Fixed, AddLZ77Block, ZopfliAppendLZ77Store) pin
store->size to a fixed value while the `ZopfliInitLZ77Store` immediately above each
call produces store->size==0, so those call sites sit on infeasible paths. Even the
cheap btype==0 branch routes through AddNonCompressedBlock, and the depth-200 object
budget is exhausted by accumulated is_fresh ensures before any mutated operator is
evaluated. Same wall as [[cbmc-zopfliblocksplit-driver-vacuity]],
[[cbmc-zopfliblocksplitlz77-callee-precondition-wall]], [[cbmc-lz77greedy-depth-vacuity]].

In-body malloc/free of local splitpoints did NOT crash goto-instrument here (mirrors
ZopfliBlockSplit, which also frees locals).
