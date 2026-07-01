---
name: cbmc-sublentocache
description: ZopfliSublenToCache in zopfli.c verifies non-vacuously at 10/38; cache value-byte mutations unobservable
metadata: 
  node_type: memory
  type: project
  originSessionId: 3eb937f2-7cdc-431b-b3b1-aeae1cab4d41
---

`ZopfliSublenToCache` (zopfli.c) verifies non-vacuously, mutation kill score **0.2632 (10/38)**.

Contract: is_fresh(lmc), `length <= ZOPFLI_MAX_MATCH`, is_fresh(sublen, (length+1)*sizeof(short)), pos bound + is_fresh(lmc->sublen, ...) mirroring [[cbmc-calculatetreesize-minselect-unkillable]]'s sibling ZopfliMaxCachedSublen; assigns object_whole(lmc->sublen); ensures `length>=3 ==> 3 <= ZopfliMaxCachedSublen(lmc,pos,length) <= length`.

**Why 28 survive:** the function is a run-length encoder writing cache[j*3 .. j*3+2] per run. Its only externally-observable output is `ZopfliMaxCachedSublen`'s return, whose contract reads just cache[1], cache[2], and cache[(CACHE_LENGTH-1)*3]. Mutations to the per-entry length/dist bytes and run-boundary tests don't change those 3 cells, so they're unkillable by any closed-form postcondition. Loop invariants that tie cache cells to sublen don't help — see [[cbmc-loop-contracts-ignored]] (harness drops them). 10 kills come from the two ensures + the body asserts.

**How to apply:** This is a converged result; don't reopen expecting > 10/38 without a round-trip decode (ZopfliCacheToSublen) postcondition, which is impractical in an ensures.
