---
name: zah-no-mutants
description: "ZopfliAllocHash 7-malloc allocator; avocado generates 0 mutants, kill score inherently 0; verifies w/ is_fresh in/out contract"
metadata: 
  node_type: memory
  type: project
  originSessionId: ae0792d1-6741-4d0e-9bad-67bcce06a24f
---

ZopfliAllocHash (zopfli.c) is a pure allocator: 7 mallocs into a ZopfliHash (head/head2 fixed 65536*int; prev/hashval/same/prev2/hashval2 sized window_size). No loops, no mutable operators, so `get-mutants` returns "No mutant(s) generated" — kill score is inherently 0, don't chase.

Verifies @depth 200 (no callees to replace) with the [[zic-dfcc-loopcontracts-12of16]]/ZopfliInitCache allocator idiom: requires window_size>0 + window_size<=max_malloc_size/sizeof(int) + is_fresh(h); assigns(object_whole(h)); ensures is_fresh on each of the 7 output pointers at exact sizes. Same family as [[zilz77s-no-mutants]], [[zcc-no-mutants]].

**Why:** strong spec already maxed (0 mutants means kill score can't exceed 0). **How to apply:** leave the sound is_fresh contract, don't add machinery hunting nonexistent mutants.
