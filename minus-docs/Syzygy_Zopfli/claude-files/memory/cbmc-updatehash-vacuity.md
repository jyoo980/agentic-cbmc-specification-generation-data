---
name: cbmc-updatehash-vacuity
description: ZopfliUpdateHash verifies but 0/40; 8 is_fresh objects + up-to-65535-iter loop exceed depth bound → vacuous
metadata: 
  node_type: memory
  type: project
  originSessionId: b7617324-61d4-484a-8982-ee403a531bdf
---

`ZopfliUpdateHash` in zopfli.c verifies (exit 0) but mutation testing kills 0/40.

**Why:** Same depth-bound vacuity as the other multi-pointer hash/cache functions
([[cbmc-resethash-vacuity]], [[cbmc-storelitlendist-vacuity]],
[[cbmc-findlongestmatch-vacuity]], [[cbmc-trygetfromlmc-vacuity]],
[[cbmc-depth200-isfresh-vacuity]]). The harness has 8 `is_fresh` objects
(`h` + head/prev/hashval/head2/prev2/hashval2/same + `array`) plus the
run-length scan `while` loop that can iterate up to 65535 times. Postconditions
are never reached under `--depth`, so even mutant 36 (`pos + ZOPFLI_MIN_MATCH`
→ `pos - ZOPFLI_MIN_MATCH`, which flips the exact `h->val == ...` ensures) survives.

**How to apply:** Don't chase kills here. A strong sound spec is in place:
requires head/head2 entries are -1 or in-window (keeps `h->hashval[h->head[h->val]]`
safe), array fresh up to `end`, h->val in [0,HASH_MASK]; ensures the full
val/val2/head/head2/hashval/hashval2 update relations + prev<WINDOW bounds.
Leave it as-is.
