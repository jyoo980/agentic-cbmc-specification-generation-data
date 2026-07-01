---
name: cbmc-tracebackwards-append-depth-vacuity
description: "TraceBackwards in zopfli.c verifies but 0/23 kills — ZOPFLI_APPEND_DATA's in-body malloc/realloc/memset exhausts --depth 200, body unreachable"
metadata: 
  node_type: memory
  type: project
  originSessionId: 857ef79b-58d3-422c-be93-55a219d80b0b
---

`TraceBackwards` (static, zopfli.c ~line 3271) verifies with `run-cbmc` but scores 0/23 mutation kills. Vacuous: the body is unreachable. Confirmed with a deliberately-false `__CPROVER_ensures(*pathsize == 999999)` which still "Verified" — even after constraining `size == 2` (tiny). So it is NOT the unwind bound; it is the `--depth 200` cap.

Cause: the body grows `path` with `ZOPFLI_APPEND_DATA` (zopfli.h:209), a macro that expands to `malloc`/`realloc`/`memset` inline. CBMC's realloc/memset models do byte-level copy loops; a couple of appends exhaust `--depth 200` before the path reaches the postcondition assertion, so the all-23 mutants (all in the trace loop + mirror loop, including an OOB-inducing `*pathsize / 2 -> *pathsize * 2`) survive. Can't edit the C (macro), can't change `--depth` (hardcoding rule) → 0/23 unavoidable. Same wall as [[cbmc-depth200-isfresh-vacuity]] / [[cbmc-followpath-vacuity]] but driven by the allocation model, not is_fresh count.

IMPORTANT correction to [[cbmc-malloc-body-enforce-crash]]: that memory claims ANY zopfli.c function whose body calls malloc/realloc/free directly crashes goto-instrument under `--enforce-contract`. FALSE here — TraceBackwards calls malloc/realloc/memset via ZOPFLI_APPEND_DATA and goto-instrument does NOT crash; it compiles, reaches cbmc, and verifies (vacuously). The crash memory's examples were all free-only or bare-malloc bodies; the append macro's malloc form is accepted. So: in-body malloc ⇒ either enforce-crash OR depth-vacuity; test, don't assume crash.

Strong sound spec left in place: requires size>=1 and size bounded by max_malloc_size; is_fresh(length_array, (size+1) elems); a `__CPROVER_forall { 1<=k<=size ==> length_array[k] in [1,258] && length_array[k]<=k }` covering the three in-body asserts (`<=index`, `<=ZOPFLI_MAX_MATCH`, `!=0`); is_fresh(path)/is_fresh(pathsize) with `*pathsize==0` (matches caller LZ77OptimalRun which does `*path=0;*pathsize=0` before the call); assigns(*path,*pathsize); ensures `1 <= *pathsize <= size`.
