---
name: zch-no-mutants
description: "ZopfliCleanHash — 7-free func, avocado generates 0 mutants, kill score inherently 0"
metadata: 
  node_type: memory
  type: project
  originSessionId: d7dc9f82-223a-4636-9ed8-0e7e47ae09f3
---

`ZopfliCleanHash` (zopfli.c:4133) frees 7 hash pointers (head, prev, hashval, head2, prev2, hashval2, same). avocado generates 0 mutants ("no mutable operators"), so kill score is inherently 0 — nothing to chase.

**Why:** Same family as [[zcc-no-mutants]], [[zclz77s-no-mutants]] — free-only functions have no mutable operators.

**How to apply:** Verifies @depth200 with is_fresh(h) + is_fresh on each of the 7 ptrs (requires), assigns(object_whole(...)) of all 7, frees() of all 7, and was_freed(old(...)) ensures per ptr. Verify script: /app/_verify_zch.sh.
