---
name: cbmc-addsorted-append-depth-vacuity
description: AddSorted verifies soundly but kill 0/18; ZOPFLI_APPEND_DATA depth wall; requires-forall over realloc target crashes goto-instrument
metadata: 
  node_type: memory
  type: project
  originSessionId: 43cdefe6-d987-4090-b698-4ff5b6703365
---

`AddSorted` in `Syzygy_Zopfli/c_code/zopfli.c` verifies soundly but kill 0/18 — same ZOPFLI_APPEND_DATA depth-200 wall as [[cbmc-tracebackwards-depth-vacuity]]. Pin `*outsize==3` (non-power-of-two) so the macro's malloc/realloc branch isn't taken at runtime; is_fresh(out)/is_fresh(*out, 4 elts)/is_fresh(outsize). Strong functional ensures: size+1 and sorted-result forall.

**Why kill 0:** macro malloc/realloc/memset goto-machinery + 4 is_fresh exhaust --depth 200 before exit; postconditions vacuous. Unbeatable (harness fixes --depth).

**How to apply / two gotchas:**
- A `__CPROVER_forall` in a *requires* clause that dereferences a realloc target (`(*out)[k]`) crashes goto-instrument: `instrument_spec_assigns.cpp:597 create_car_expr ... no definite size for lvalue target: malloc::should_malloc_fail`. The *ensures* forall over the same `*out` is fine. Express the sorted-INPUT precondition as scalar comparisons `(*out)[0]<=(*out)[1] && (*out)[1]<=(*out)[2]` instead (works because size pinned to 3). This adds ~6 instrumentation_failed mutants but kill stays 0.
- **is_fresh ordering matters for the malloc crash:** put `is_fresh(*out, ...)` immediately after `is_fresh(out, ...)`, BEFORE the `*outsize==3` / outsize clauses. Original ordering (is_fresh(*out) after reading *outsize) also triggered the should_malloc_fail crash.
- Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.
