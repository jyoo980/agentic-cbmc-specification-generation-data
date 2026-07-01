---
name: cbmc-trygetfromlmc-vacuity
description: TryGetFromLongestMatchCache in zopfli.c verifies vacuously at depth 200 (0/41 kills) — same is_fresh wall
metadata: 
  node_type: memory
  type: project
  originSessionId: 108bf24b-d2c7-46ae-abb8-50407514d32e
---

`TryGetFromLongestMatchCache(s, pos, limit, sublen, distance, length)` (zopfli.c) is the same depth-200 is_fresh vacuity wall as [[cbmc-depth200-isfresh-vacuity]] — the heaviest cache case: ~9 is_fresh objects (`s` struct + nested `s->lmc` struct + `s->lmc->length`/`dist`/`sublen` arrays + `limit`/`distance`/`length` out-scalars + `sublen` array) plus two contract-replaced callees (`ZopfliMaxCachedSublen`, `ZopfliCacheToSublen`).

Sound strong contract written (mirrors [[cbmc-sublentocache]]'s sibling `StoreInLongestMatchCache`): is_fresh all 9, `pos>=s->blockstart`, lmcpos overflow guard `pos-blockstart < max_malloc/(ZOPFLI_CACHE_LENGTH*3)`, length/dist arrays sized `(lmcpos+1)`, sublen sized `(ZOPFLI_MAX_MATCH+1)`, lmc->sublen sized `ZOPFLI_CACHE_LENGTH*(lmcpos+1)*3` (matches the callee preconds), `ZOPFLI_MIN_MATCH<=*limit<=ZOPFLI_MAX_MATCH`, assigns(*limit,*distance,*length,object_whole(sublen)), ensures `ret in {0,1}` and `ret==1 ==> *length<=*limit && *length<=ZOPFLI_MAX_MATCH`. sublen forced non-null (is_fresh) so the `!sublen`/`else` paths are dead — same choice as StoreInLongestMatchCache; doesn't matter under vacuity.

Verifies at depth 200 → **0/41 kills**; all 41 survivors are the `cache_available`/`limit_ok_for_cache` branch-guard relational+logical ops (lines 2448-2452) — never reached. Confirmed vacuous via the documented probe: added `__CPROVER_ensures(1==0)` → postcondition SUCCEEDS at depth 200 (VERIFICATION SUCCESSFUL). Net 0/41 tooling-forced; left the strong sound spec in place.

Bonus gotcha avoided: the in-body `assert(sublen[*length] == s->lmc->dist[lmcpos])` (line ~2452 region) would FAIL non-vacuously if the body were reached — `ZopfliCacheToSublen`'s contract only pins `sublen[0..3]`, so `sublen[*length]` (for *length>3) is havoc'd and the cache-consistency invariant isn't locally provable. Vacuity hides this; reaching the body would flip it to FAILURE, not kills.
