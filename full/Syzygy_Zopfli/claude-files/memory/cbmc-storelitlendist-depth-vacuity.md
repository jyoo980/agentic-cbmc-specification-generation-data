---
name: cbmc-storelitlendist-depth-vacuity
description: ZopfliStoreLitLenDist verifies soundly but kill 0/31; 8 is_fresh on store+arrays exhaust depth 200 before body
metadata: 
  node_type: memory
  type: project
  originSessionId: f6f8a77b-77c4-4714-b7ce-c856c4f3c35c
---

`ZopfliStoreLitLenDist` (zopfli.c:~3397) verifies soundly but **kill 0/31** — the
depth-200 vacuity wall, same as [[cbmc-calculateblocksymbolsizesmall-depth-vacuity]]
and [[cbmc-getbyterange-concrete-pin-kills]].

**Spec written (sound, strong on paper):** pin `store->size == 3` (smallest value
that is neither a power of two nor a multiple of ZOPFLI_NUM_LL=288 / ZOPFLI_NUM_D=32).
That skips BOTH cumulative-histogram loops (`origsize % NUM == 0` false) and makes
every ZOPFLI_APPEND_DATA write index 3 in place (3 & 2 != 0, no realloc), with
llstart=dstart=0. Requires: is_fresh(store) + is_fresh on all 7 arrays (litlens/
dists/pos/ll_symbol/d_symbol sized 4; ll_counts 288; d_counts 32), length<259,
dist<=32768. Ensures: size==old+1, litlens[3]==length, dists[3]==dist, pos[3]==pos,
dist==0 ==> ll_symbol[3]==length && d_symbol[3]==0.

**Why kill 0:** confirmed vacuous — a temporary `__CPROVER_ensures(1==0)` PASSES, so
the function exit is unreachable within `--depth 200`. The 8 is_fresh allocations
themselves exhaust the budget. Probed: even shrinking to `dist==0` + `length<2` +
2-element ll_counts + 1-element d_counts (eliminating the else branch's two heavy
ZopfliGetLengthSymbol/ZopfliGetDistSymbol contract replacements) still leaves
1==0 passing. So it's the is_fresh COUNT/machinery, not object sizes — size pinning
can't rescue it. All 31 survivors (histogram-loop region lines, the assert, the
dist branch, and the increment-index arithmetic) are simply never reached.

**Build:** needs `-I /app/Syzygy_Zopfli/stubs` for x86_64-linux-gnu/bits/types/FILE.h.
Kept the spec as-is; kill 0 is the ceiling.
