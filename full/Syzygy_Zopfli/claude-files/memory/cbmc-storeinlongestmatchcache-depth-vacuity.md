---
name: cbmc-storeinlongestmatchcache-depth-vacuity
description: StoreInLongestMatchCache verifies soundly but kill 0/25; 6 is_fresh objects + replaced ZopfliSublenToCache contract exhaust depth 200 before function exit
metadata: 
  node_type: memory
  type: project
  originSessionId: 1cacbe00-9cfd-4231-8689-3cd9e4a05202
---

`StoreInLongestMatchCache` (zopfli.c ~line 2856) verifies but kill 0/25. All 25 survivors are LOGICAL `&&/||` operator mutants on the two boolean conditions (lines 2909 `cache_available` and 2912 the `if`); killing them needs the function-exit value postcondition, which is depth-truncated.

**Vacuity confirmed:** added unguarded false `__CPROVER_ensures(distance != distance)` and it still "verified successfully" → function exit unreachable under `--depth 200`. Same wall as [[cbmc-depth-200-object-limit]].

**Why:** the contract sets up 6 pointer-heavy fresh objects — `s` (ZopfliBlockState: 2 ptrs + 2 size_t), `s->lmc` (3 ptrs), `s->lmc->length`, `s->lmc->dist`, `s->lmc->sublen` (96B), and the `sublen` param (symbolic `(length+2)*2` bytes) — plus a replaced `ZopfliSublenToCache` contract. That exhausts depth before the writes/exit.

**Strongest correct spec written anyway** (verifies, exits 0): is_fresh on s/lmc/length/dist/sublen-cache/sublen-param; `pos>=blockstart`, `lmcpos=pos-blockstart < ZMCS_MAXPOS(4)`, `length<=ZOPFLI_MAX_MATCH`. Sentinel precondition `length[lmcpos]==0 || dist[lmcpos]!=0 || length[lmcpos]==1` discharges the in-body asserts (matches ZopfliInitCache). assigns length[lmcpos]/dist[lmcpos]/sublen block. Functional ensures: when `limit==MAX_MATCH && sublen && old length!=0 && old dist==0`, length/dist set to `length<3?0:length` / `length<3?0:distance`, else preserved (via `__CPROVER_old`).

Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h. Don't chase the kill score — depth wall.
