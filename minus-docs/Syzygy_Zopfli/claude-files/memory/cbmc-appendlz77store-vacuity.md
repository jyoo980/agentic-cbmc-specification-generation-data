---
name: cbmc-appendlz77store-vacuity
description: ZopfliAppendLZ77Store in zopfli.c verifies vacuously (0/5) — 12 is_fresh objects forced by the ZopfliStoreLitLenDist callee contract exceed depth 200
metadata: 
  node_type: memory
  type: project
  originSessionId: d1b781f3-b950-4c54-aca5-855a706e85f5
---

`ZopfliAppendLZ77Store` (zopfli.c) loops over the source store's `size` entries,
appending each into `target` via [[cbmc-storelitlendist-vacuity]] (`ZopfliStoreLitLenDist`).
That callee's contract requires `target->size == 3` + cleared histograms on entry
and yields `target->size == 4`, so its precondition can hold for at most ONE
iteration — the spec restricts the source to `store->size <= 1` to stay sound
under contract replacement.

Verifies but **0/5**; all 5 survivors are the loop-condition `<` mutant on
`for (i = 0; i < store->size; i++)`. Vacuous: a false `target->size == 999`
postcondition still "verified successfully". Cause is the same depth-200 pattern —
the harness forces ~12 `is_fresh` objects (store struct + 3 source arrays + target
struct + 7 target arrays, all required to discharge the callee contract), exceeding
`--depth 200` so the body is never reached. Can't shrink the harness: dropping any
target array makes the callee's `is_fresh` precondition unprovable. Strong sound
spec (per-entry length<259/dist<=32768, single-append size/litlen/dist/pos ensures)
left in place.
