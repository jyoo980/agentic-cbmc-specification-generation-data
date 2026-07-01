---
name: cbmc-blocksplit-vacuity
description: ZopfliBlockSplit in zopfli.c verifies but 0/14; depth-200 setup + unsatisfiable callee preconditions make body unreachable
metadata: 
  node_type: memory
  type: project
  originSessionId: b1372bdd-d9b0-43b1-86b5-9c399e9ddfce
---

ZopfliBlockSplit (zopfli.c) VERIFIES but kills 0/14 mutants — vacuous at the
harness's --depth 200. Confirmed: a false ensures `*npoints == 999999` still
"Verifies", so the body's split-conversion loop / final assert is never reached.

Two independent causes (both documented in the in-file comment):
1. Setup chain (ZopfliInitLZ77Store, ZopfliInitBlockState, ZopfliAllocHash) plus
   this function's own is_fresh preconditions exhaust --depth 200 before the body.
2. Even unbounded, callee preconditions under contract replacement are
   unsatisfiable: ZopfliInitLZ77Store gives store->size==0 / NULL arrays but
   contract-replaced [[cbmc-lz77greedy-vacuity]] ZopfliLZ77Greedy requires
   store->size==3 + fresh 4-elem arrays; ZopfliInitBlockState(add_lmc=0) gives
   s->lmc==NULL but ZopfliLZ77Greedy requires s->lmc fresh; ZopfliBlockSplitLZ77
   ([[cbmc-blocksplitlz77-vacuity]]) is handed NULL *splitpoints vs its fresh req.

Same family as [[cbmc-depth200-isfresh-vacuity]] and [[cbmc-addlz77blockautotype-vacuity]].
Strongest SOUND spec for its own interface left in place: is_fresh(options/in/
splitpoints/npoints), instart<=inend, inend>=1, inend<max_malloc/(CACHE_LENGTH*3),
assigns(*npoints,*splitpoints).
