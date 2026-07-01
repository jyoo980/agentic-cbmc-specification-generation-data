---
name: cbmc-addsorted-malloc-body-crash
description: "AddSorted in zopfli.c is unverifiable; top-level ZOPFLI_APPEND_DATA malloc crashes goto-instrument's create_car_expr (should_malloc_fail)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 780fa6a2-b7b8-425d-bfe4-73d0a21c5e93
---

`AddSorted` (zopfli.c ~line 4566) is UNVERIFIABLE under `--enforce-contract`.
ZOPFLI_APPEND_DATA expands to an in-body malloc/realloc/memset at the TOP LEVEL of
the function (before the insertion-sort loops). goto-instrument aborts in
`instrument_spec_assigns.cpp:597 create_car_expr` with "no definite size for lvalue
target: malloc::1::1::should_malloc_fail" while walking the inlined malloc model.

**Why:** This is the malloc/realloc-body crash class (cf. [[cbmc-malloc-body-enforce-crash]],
[[cbmc-lz77optimalrun-free-body-crash]]). The crash fires regardless of the assigns
clause — tested 4 variants (is_fresh buffer+object_whole; assigns(*out,*outsize);
*outsize==0+assigns(*out,*outsize); assigns(*outsize) only) — all crash identically.

**How to apply:** Contrast with [[cbmc-tracebackwards-append-depth-vacuity]]: the SAME
macro VERIFIES (vacuously) in TraceBackwards because the append sits inside a `for(;;)`
loop body, so loop-contract instrumentation handles the malloc separately. AddSorted's
alloc precedes the loops and cannot be moved. Left the strongest sound spec in place
(is_fresh out + *out buffer, *outsize<8, assigns *out/*outsize/object_whole(*out),
ensures *outsize == old+1).
