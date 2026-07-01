---
name: cbmc-zopfliupdatehash-depth-vacuity
description: ZopfliUpdateHash verifies soundly but kill 0/40; 9 is_fresh + two 65536-wide foralls + replaced UpdateHashValue exhaust depth 200
metadata: 
  node_type: memory
  type: project
  originSessionId: d57e592e-8217-40c6-9c99-2ee0244a99d8
---

`ZopfliUpdateHash(const unsigned char *array, size_t pos, size_t end, ZopfliHash *h)` in `/app/Syzygy_Zopfli/c_code/zopfli.c` (~line 3520) **verifies** but **kill 0/40** (all mutants survive). Needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs`.

Contract written (sound, strongest functional form):
- `is_fresh` on h plus head/head2 (65536 ints each), prev/prev2/hashval/hashval2/same (ZOPFLI_WINDOW_SIZE each), and `array` (size `end`). 9 is_fresh objects.
- `requires h->val in [0, HASH_MASK]` (feeds UpdateHashValue's precondition).
- Two `forall k<65536` preconditions: `head[k]==-1 || (0<=head[k]<ZOPFLI_WINDOW_SIZE)` (same for head2). **Required for memory safety** of the nested `h->hashval[h->head[h->val]]` / `h->hashval2[h->head2[h->val2]]` reads — head/head2 are nondet ints, so without this each dereference is OOB.
- `requires pos < end` (makes array[pos], array[pos+2] warmup byte, and the run-length scan array[pos+amount+1] in-bounds).
- whole-object assigns; exact ensures: `h->val == ((old(val)<<HASH_SHIFT)^(pos+MIN_MATCH<=end?array[pos+2]:0))&HASH_MASK`, val range, `hashval[hpos]==val`, `head[val]==hpos`, `val2 == ((same[hpos]-MIN_MATCH)&255)^val`, `hashval2[hpos]==val2`, `head2[val2]==hpos`, prev/prev2 `< ZOPFLI_WINDOW_SIZE`.

Vacuous via the [[cbmc-depth-200-object-limit]] wall: 9 is_fresh objects + two 65536-iteration foralls + replaced `UpdateHashValue` contract exhaust depth 200 before the body's writes are observed. Survival of mutant 36 (warmup guard `pos + MIN_MATCH` -> `pos - MIN_MATCH`, which the exact val postcondition encodes) confirms the ensures aren't effectively checked. Object burden is irreducible: foralls are needed for the nested-read OOB, and h->val can't be pinned (computed by replaced UpdateHashValue from nondet). Same class as [[cbmc-zopflifindlongestmatch-depth-vacuity]], [[cbmc-zopfliresethash-loop-depth-vacuity]]. Companion to the no-mutable-operators callee [[cbmc-updatehashvalue-no-mutable-operators]].
