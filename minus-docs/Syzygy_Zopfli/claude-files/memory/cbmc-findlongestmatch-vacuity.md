---
name: cbmc-findlongestmatch-vacuity
description: ZopfliFindLongestMatch verifies vacuously 0/157 — ~17 is_fresh objects exceed CBMC depth bound
metadata: 
  node_type: memory
  type: project
  originSessionId: 3686c9c7-d2e0-4eba-bfe9-8ce5b32b1fc0
---

`ZopfliFindLongestMatch` in zopfli.c verifies (exit 0) but **vacuously, 0/157 kills**
(156 survived, 1 compile-fail). Same root cause as [[cbmc-trygetfromlmc-vacuity]] and
[[cbmc-depth200-isfresh-vacuity]]: the sound spec needs ~17 `__CPROVER_is_fresh`
objects (s, s->lmc + its length/dist/sublen arrays, h + its head/prev/hashval/
head2/prev2/hashval2/same arrays, array, distance, length, sublen), whose malloc
harness exceeds the depth bound → precondition conjunction UNSAT → all mutants survive.

The spec is strong & sound: hash-chain well-formedness (head[val]==hpos, forall
prev[i]<WINDOW_SIZE, forall hashval[i]==val for both hashes), cache slot ==1/dist==0
to satisfy StoreInLongestMatchCache, array fresh of `size`, limit in [3,258],
ensures `*length<=258 && pos+*length<=size`. Left in place.

Kill score cannot be raised: can't reduce pointer-param count (no C edits allowed),
loop contracts silently ignored ([[cbmc-loop-contracts-ignored]]). Gotcha: each
`__CPROVER_forall` block needs a UNIQUE quantifier var name across the whole
contract (reused `i` → "redeclaration with no linkage" CONVERSION ERROR).
