---
name: cbmc-lz77optimal-depth-vacuity
description: "ZopfliLZ77Optimal verifies soundly but kill 0/28; depth-200 wall + Greedy's pinned store->size==3 contradicts in-body ZopfliInitLZ77Store size-0 currentstore"
metadata: 
  node_type: memory
  type: project
  originSessionId: b4a82579-312b-4f28-b2b4-922778462aae
---

`ZopfliLZ77Optimal` (Syzygy_Zopfli/c_code/zopfli.c ~5523) verifies (exit 0) with a
full memory-safety contract mirroring its `ZopfliLZ77OptimalFixed` sibling
([[cbmc-lz77optimalfixed-depth-vacuity]]) but **kill 0/28**.

Pointer params are only s, in, store (everything else — currentstore, hash, stats,
ran_state, length_array/path/costs — is local). Contract: is_fresh(s, s->options,
s->lmc + length/dist/sublen at ZMCS_MAXPOS), inend==8, s->blockstart<=instart<inend,
inend-s->blockstart<=ZMCS_MAXPOS, is_fresh(in,inend), and store as
ZopfliCopyLZ77Store's dest (each incoming array a fresh single-element object).
assigns: s->blockstart/blockend + object_whole(lmc length/dist/sublen, store).
In-body malloc/free did NOT crash goto-instrument here (same as the Fixed sibling).

**Why kill 0:** Two compounding walls. (1) Depth-200 budget exhausted by the heavy
is_fresh set (LMC length/dist/sublen) + replaced callee contracts before the loop is
explored. (2) Structural contradiction: the loop's first callee `ZopfliLZ77Greedy`
pins `store->size==3` with fresh arrays, but the body `ZopfliInitLZ77Store`s the local
`currentstore` to size 0 / NULL arrays — so every post-init mutant is unreachable
(same shape as [[cbmc-zopfliblocksplit-driver-vacuity]]). Surviving mutants: blocksize
`-`→`+` and malloc `+`→`-` sizing (buffers only read inside LZ77OptimalRun, contract
replaced, guarded by its 65536-elem hash is_fresh wall), plus the verbose-branch and
`i>5 && cost==lastcost` logical operators inside the unreachable loop body.

**How to apply:** This is the established ceiling for the whole optimal/greedy LZ77
tree — don't chase it. Build: `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs`.
