---
name: cbmc-trygetfromlongestmatchcache-depth-vacuity
description: verifies soundly but kill 0/41; 8 is_fresh objects + 2 replaced LMC callee contracts exhaust depth 200 before cache-check branches
metadata: 
  node_type: memory
  type: project
  originSessionId: 1593bccd-bda8-4e40-9739-9e23b2232d27
---

`TryGetFromLongestMatchCache` (Syzygy_Zopfli/c_code/zopfli.c) verifies soundly
with assigns + memory-safety + (return∈{0,1}, *length<=*limit) contract, but
mutation kill score is **0/41**.

**Why:** classic depth-200 wall ([[cbmc-depth-200-object-limit]]). The contract
needs 8 is_fresh objects — s, s->lmc, lmc->length, lmc->dist, lmc->sublen, plus
the limit/distance/length out-pointers — and the body calls two already-heavy
replaced callees, `ZopfliMaxCachedSublen` ([[cbmc-zopflimaxcachedsublen]] family)
and `ZopfliCacheToSublen` ([[cbmc-cachetosublen-depth-vacuity]]). The is_fresh
setup + callee contract replacements exhaust depth 200 before the cache-check
branches (lines ~3022-3030) are reached. All 41 survivors are LOGICAL `&&`/`||`
mutants on those conditions — never reached within depth.

**How to apply:** same unkillable cluster as [[cbmc-storeinlongestmatchcache-depth-vacuity]],
[[cbmc-cachetosublen-depth-vacuity]]. Build needs `-I /app/Syzygy_Zopfli/stubs`
for FILE.h. No postcondition can distinguish mutants because the differing code
isn't reached; don't burn attempts trying to strengthen. Spec is sound, just
vacuous.
