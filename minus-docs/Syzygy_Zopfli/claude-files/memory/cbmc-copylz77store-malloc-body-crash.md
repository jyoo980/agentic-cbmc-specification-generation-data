---
name: cbmc-copylz77store-malloc-body-crash
description: ZopfliCopyLZ77Store unverifiable; 7 top-level in-body malloc() calls crash goto-instrument create_car_expr (should_malloc_fail) under --enforce-contract
metadata: 
  node_type: memory
  type: project
  originSessionId: 81bb70b3-a8f6-4ce7-a2ab-9af41da1a40e
---

ZopfliCopyLZ77Store in /app/Syzygy_Zopfli/c_code/zopfli.c is unverifiable.
It makes 7 plain top-level `malloc()` calls in its body (litlens, dists, pos,
ll_symbol, d_symbol, ll_counts, d_counts). Under `--enforce-contract`
goto-instrument aborts during "Enforcing contracts" with:
`instrument_spec_assigns.cpp create_car_expr: no definite size for lvalue
target: symbol ... should_malloc_fail` (a bool). Same family as
[[cbmc-addsorted-malloc-body-crash]] and [[cbmc-malloc-body-enforce-crash]];
the crash is in computing the assigns CAR set for the malloc-fail flag, so it is
independent of the spec written.

**Why:** in-body malloc (not behind a callee contract) trips the assigns-clause
instrumentation regardless of spec content.

**How to apply:** don't keep retrying spec variants for such functions. A strong
sound spec (is_fresh source + all 7 backing arrays sized to source->size /
rounded-up llsize/dsize via CeilDiv, is_freeable dest arrays for the in-body
ZopfliCleanLZ77Store, object_whole(dest) assigns + frees, forall-copy
postconditions on every array) was left in place.
