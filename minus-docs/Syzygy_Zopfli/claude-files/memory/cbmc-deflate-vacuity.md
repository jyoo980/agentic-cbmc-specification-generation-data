---
name: cbmc-deflate-vacuity
description: "ZopfliDeflate verifies but 0/18; depth-200 vacuity, the outermost DEFLATE driver whose per-master-block ZopfliDeflatePart call re-checks the full is_fresh precondition chain"
metadata: 
  node_type: memory
  type: project
  originSessionId: a1063668-46d0-4745-957f-e2674f10ff94
---

ZopfliDeflate in zopfli.c VERIFIES but kills 0/18 mutants. Outermost DEFLATE
driver: a do-while loop that chops `in` into ZOPFLI_MASTER_BLOCK_SIZE blocks and
calls contract-replaced [[cbmc-deflatepart-vacuity]] ZopfliDeflatePart once per
block. All 18 surviving mutants are in the loop body (lines ~5692-5703:
masterfinal/final2/size selectors, the `i + size` part-call args, the verbose
`*outsize - offset`).

**Why:** depth-200 vacuity — ZopfliDeflatePart's own precondition chain
(options/in/bp/outsize/out/*out is_fresh + output-buffer invariant) is re-checked
on the FIRST loop iteration and consumes well past 200 CBMC steps, so the body is
never observably reached. Same family as [[cbmc-depth200-isfresh-vacuity]],
[[cbmc-blocksplit-vacuity]], [[cbmc-lz77optimal-vacuity]], [[cbmc-lz77greedy-vacuity]].
Confirmed: a false `__CPROVER_ensures(*outsize == 424242)` still "Verifies".

**How to apply:** strongest SOUND spec is left in place — mirrors ZopfliDeflatePart's
interface (is_fresh options/in(insize)/bp(*bp<=7)/outsize(*outsize in [1,max/2])/
out/*out(+1), insize>=1, insize < max_malloc/(ZOPFLI_CACHE_LENGTH*3), numiterations>=1;
assigns *bp,*outsize,*out,object_whole(*out); ensures *outsize >= old). Do not chase
the kill score; can't raise --depth (fixed CLI arg).
