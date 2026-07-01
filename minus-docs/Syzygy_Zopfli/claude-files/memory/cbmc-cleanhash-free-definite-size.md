---
name: cbmc-cleanhash-free-definite-size
description: ZopfliCleanHash (7 in-body free calls) verifies cleanly when each freed pointer has a definite size via is_fresh
metadata: 
  node_type: memory
  type: project
  originSessionId: 3283f71a-9472-4a08-b508-4ac2df78fc5a
---

ZopfliCleanHash in zopfli.c is a pure deallocator: 7 `free()` calls on ZopfliHash pointer fields (head/prev/hashval/head2/prev2/hashval2/same). It VERIFIES cleanly (no crash). Mutation testing reports "no mutable operators" — 0 possible kills, not a spec weakness.

Spec: require `is_fresh(h)` + `is_fresh` on all 7 fields with ZopfliAllocHash's sizes (two head tables 65536, per-window arrays ZOPFLI_WINDOW_SIZE), empty `__CPROVER_assigns()`.

**Why this matters:** corrects/bounds [[cbmc-lz77optimalrun-free-body-crash]]. In-body `free()` does NOT inherently crash goto-instrument. The crash there came from freeing a pointer (`*path`) with no DEFINITE size. When every freed pointer gets a definite size from `is_fresh(ptr, N*sizeof(...))`, goto-instrument's __CPROVER_deallocate resolves the size and the frees verify.

**How to apply:** for free-only functions, give each freed pointer a sized `is_fresh` requires; expect "no mutable operators" rather than kills.
