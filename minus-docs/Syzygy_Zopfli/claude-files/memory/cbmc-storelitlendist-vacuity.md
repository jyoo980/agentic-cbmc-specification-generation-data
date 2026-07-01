---
name: cbmc-storelitlendist-vacuity
description: "ZopfliStoreLitLenDist verifies vacuously 0/31; 8 is_fresh objects exceed depth 200, but dropping any array is_fresh crashes goto-instrument"
metadata: 
  node_type: memory
  type: project
  originSessionId: a71f21cb-a0cb-40c8-bc28-9c24fc5e6414
---

`ZopfliStoreLitLenDist` (zopfli.c) is in the multi-pointer `is_fresh` vacuity
class — see [[cbmc-depth200-isfresh-vacuity]], [[cbmc-trygetfromlmc-vacuity]],
[[cbmc-findlongestmatch-vacuity]].

The store struct has 7 array fields (litlens, dists, pos, ll_symbol, d_symbol,
ll_counts, d_counts), all dereferenced in-body via `ZOPFLI_APPEND_DATA`
(malloc/realloc/memset). So a sound harness needs 8 `is_fresh` objects (store +
7 arrays). That harness + body exceeds `--depth 200`, so verification is vacuous
(a deliberately false postcondition still "Verified"; mutation kill 0/31).

Tried to dodge the 288/32-element histogram-fill loops (which alone blow depth,
cf. [[cbmc-resethash-vacuity]]) by requiring `store->size == 3` — not a multiple
of ZOPFLI_NUM_LL/ZOPFLI_NUM_D (skips both fill loops) and not a power of two
(no realloc; every append just writes index 3). Still vacuous: the 8 `is_fresh`
harness itself is the depth hog.

Diagnostic: dropping the 7 array `is_fresh` (keeping only `is_fresh(store)`)
does NOT help — it crashes `goto-instrument` with "Invariant check failed"
(unconstrained pointers reach realloc/memset). So there is no middle ground:
all 8 is_fresh → vacuous; fewer → goto-instrument crash. Unverifiable
non-vacuously at default depth.

Strong sound spec left in place (size==3 path, cleared-histogram precondition,
exact stored values + single-increment `<= 1` count postconditions).
