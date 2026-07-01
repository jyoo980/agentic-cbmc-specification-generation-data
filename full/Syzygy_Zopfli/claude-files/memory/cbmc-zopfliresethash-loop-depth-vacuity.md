---
name: cbmc-zopfliresethash-loop-depth-vacuity
description: "ZopfliResetHash verifies soundly but kill 0/25; six init loops + 7 is_fresh exhaust depth-200/unwind-5 before exit, all postconditions vacuous"
metadata: 
  node_type: memory
  type: project
  originSessionId: 3f0fbdda-91e4-4a79-9247-735f6dc56d59
---

`ZopfliResetHash(size_t window_size, ZopfliHash *h)` in
`/app/Syzygy_Zopfli/c_code/zopfli.c` (~line 3497) verifies successfully but with
**kill 0/25**. Same vacuity wall as [[cbmc-zopfliinitcache-loop-vacuity]].

**Why:** The body is six back-to-back init loops — two over `65536`
(`head`/`head2 = -1`) and four over `window_size` (`prev[i]=i`, `hashval=-1`,
`same=0`, `prev2[i]=i`, `hashval2=-1`). The harness runs
`--partial-loops --unwind 5 --depth 200`. The leading 65536-element head loops
plus the `is_fresh` setup for all 7 hash arrays (head/head2 are 65536-int
objects) exhaust the depth-200 budget before the function exit is reached, so
every `__CPROVER_ensures` — including `h->val == 0` which is assigned *before*
any loop — passes vacuously. All 25 surviving mutants are RELATIONAL operators on
the loop guards.

**How to apply:** Don't chase kills here. Contract used: `1 <= window_size <=
ZOPFLI_WINDOW_SIZE`, `is_fresh(h,...)` + `is_fresh` for head/head2 (65536*int)
and prev/prev2/hashval/hashval2/same (window_size each), `assigns` with
`object_whole` for each array + val/val2, and full functional `forall`
postconditions (intent only). Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.
Pinning `window_size` smaller does NOT help — the 65536 head loops dominate the
early depth budget regardless of window_size.
