---
name: zibs-precondition-kill
description: "ZopfliInitBlockState 1/1 kill — killed the blockend-blockstart->+ mutant via callee precondition + asymmetric requires bound, dfcc flow"
metadata: 
  node_type: memory
  type: project
  originSessionId: 5d36d41a-e511-4242-9865-740ecd10a3dc
---

`ZopfliInitBlockState` in /app/Syzygy_Zopfli/c_code/zopfli.c verifies and scores **1/1** (its only avocado mutant: `ZopfliInitCache(blockend - blockstart, ...)` -> `blockend + blockstart`).

**Kill mechanism — callee precondition via contract replacement + ASYMMETRIC requires bound.**
The mutated value only flows into `ZopfliInitCache`'s `blocksize` arg. With `--replace-call-with-contract ZopfliInitCache`, CBMC asserts the callee's `requires(blocksize <= __CPROVER_max_malloc_size/(ZOPFLI_CACHE_LENGTH*3))` AT the call site. Trick: bound `blockend` (not the difference):
- `requires(blockstart < blockend)` and `requires(blockend <= __CPROVER_max_malloc_size/(ZOPFLI_CACHE_LENGTH*3))`.
- Original arg `blockend-blockstart <= blockend <= BOUND` -> precondition holds.
- Mutant arg `blockend+blockstart` can exceed BOUND (both near BOUND) -> precondition FAILS -> killed.
This bound is symbolic (max_malloc_size), not a hardcoded CLI value, so it's legal.

**Why dfcc (NOT plain --enforce/--replace):** the function does `s->lmc = malloc(...)` then calls the (replaced) callee whose contract `assigns object_whole(lmc)`. Plain goto-instrument does NOT track the locally-malloc'd lmc as assignable -> `assigns ... is assignable: FAILURE`. dfcc DOES (same lesson as [[zic-dfcc-loopcontracts-12of16]]). Need separate harness `__zibs_harness` (mallocs s, calls fn) + link `/app/stubs/cprover_alloc.c` (else malloc returns NULL -> callee `is_fresh(lmc)` precondition fails; and dfcc needs free body).

**No --depth needed / beats vacuity:** once ZopfliInitCache is contract-replaced there are NO loops in ZopfliInitBlockState, so verify UNBOUNDED. Under the framework's `--depth 200` the whole callee-precondition check sits past the frontier and goes vacuous (both orig+mutant SUCCESS = survive); dfcc unbounded is what exposes the kill. Things that did NOT work: OBJECT_SIZE postcondition and `lmc->length[diff-1]==1` postcondition both vacuous@depth200 (is_fresh in a replaced ensures doesn't pin observable array size/content here).

Scripts: /app/_verify_zibs.sh, /app/kill_zibs.sh, /app/_zibs_harness.c. Contrast [[avocado-depth200-vacuity]].
