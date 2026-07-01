---
name: cbmc-zopflialloc-malloc-only-verifies
description: ZopfliAllocHash malloc-only body verifies with full alloc postcondition; no mutable operators
metadata: 
  node_type: memory
  type: project
  originSessionId: 06a4a29f-8f3e-44b2-955e-ca59c77c3ec0
---

`ZopfliAllocHash` (zopfli.c) verifies successfully with a strong contract:
`requires(window_size in [1,ZOPFLI_WINDOW_SIZE])`, `requires(is_fresh(h,sizeof *h))`,
`assigns` on all 7 pointer fields, and per-field `ensures(field==NULL || is_fresh(field, size))`.

**Why this matters:** contrasts with [[cbmc-inbody-malloc-enforcement-crash]] and
[[cbmc-inbody-free-deallocate-crash]]. A malloc-**only** body (no `free`) does NOT crash
goto-instrument, as long as `assigns` covers the malloc'd targets. The crash is specific to
`free()`/`should_malloc_fail` interplay, not plain `malloc`.

**How to apply:** for pure allocator functions, give each allocated field a
`field==NULL || is_fresh(field, exact_size)` postcondition. Mutation reports "no mutable
operators" because the only operators sit inside `sizeof(...)*N` size args / constants, which
the mutation framework skips — this is inherent, not a spec weakness. Needs
`-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs` (stubs supplies FILE.h).
