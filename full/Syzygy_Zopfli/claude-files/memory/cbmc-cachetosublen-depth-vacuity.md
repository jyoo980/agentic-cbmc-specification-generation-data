---
name: cbmc-cachetosublen-depth-vacuity
description: ZopfliCacheToSublen verifies vacuously (kill 0/21); nested loops + 3 is_fresh + MaxCachedSublen call exhaust depth 200
metadata: 
  node_type: memory
  type: project
  originSessionId: 99456d3b-da16-4a38-bea9-c811b7ed21c2
---

`ZopfliCacheToSublen` (zopfli.c) verifies but mutation kill = 0/21. Confirmed vacuous: a deliberately false `ensures(sublen[0] == ... + 999)` still reports VERIFICATION SUCCESSFUL.

**Cause:** the documented depth-200 nested-loop wall ([[cbmc-depth-200-object-limit]]). Body has nested loops (outer `j < ZOPFLI_CACHE_LENGTH`=8, inner `i = prevlength; i <= length` up to 258), each unwound 5× structurally, PLUS `is_fresh` on lmc, lmc->sublen (96B) and sublen (259-ushort/518B), PLUS a call to ZopfliMaxCachedSublen (which re-does is_fresh). Function exit is unreachable under `--depth 200`, so the postcondition is never checked and all mutants survive.

Strongest *correct* general spec written anyway (verifies, exits 0): requires pos<ZMCS_MAXPOS, length<=ZOPFLI_MAX_MATCH, is_fresh(lmc/lmc->sublen/sublen with sublen sized ZOPFLI_MAX_MATCH+1 since writes reach cache[j*3]+3<=258); assigns object_upto(sublen, full range); functional ensures that for length>=3, sublen[0] == reconstructed dist (cache[1]+256*cache[2]). All sound, just unreachable. Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h. Don't chase kill score.
