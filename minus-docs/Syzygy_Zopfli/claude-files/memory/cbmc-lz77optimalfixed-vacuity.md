---
name: cbmc-lz77optimalfixed-vacuity
description: ZopfliLZ77OptimalFixed verifies but 0/3; ~15 is_fresh objects exceed depth 200 so the in-body mallocs are never reached; strong sound spec left in place
metadata: 
  node_type: memory
  type: project
  originSessionId: 48eab1b5-04c5-402c-ab91-3bbde78426da
---

`ZopfliLZ77OptimalFixed` in zopfli.c (mallocs length_array+costs, ZopfliAllocHash, LZ77OptimalRun with costcontext=0/GetCostFixed, then free×3 + ZopfliCleanHash) **verifies but kills 0/3**.

Notably it does NOT crash goto-instrument despite the in-body `free(path)` (path allocated inside LZ77OptimalRun, no definite size) — unlike [[cbmc-lz77optimalrun-free-body-crash]]. Here the frees compile fine and verification reports "Verified".

Root cause is the usual depth-200 vacuity ([[cbmc-depth200-isfresh-vacuity]]): the preconditions establish ~15 is_fresh objects (s, s->lmc + length/dist/sublen, in, store + 7 sub-arrays) needed to satisfy LZ77OptimalRun's contract. Building that is_fresh+malloc harness exhausts `--depth 200` before the body is reached, so the call is vacuously unreachable. All 3 surviving mutants are body-internal arithmetic on the local malloc sizes (`blocksize = inend-instart` → `inend+instart`; `malloc(... * (blocksize+1))` → `(blocksize-1)`). If the body were reachable, the two shrink-malloc mutants would be killed by LZ77OptimalRun's `is_fresh(length_array, (blocksize+1)*sizeof)` precondition check; they survive only because of vacuity.

Also note the call passes `0` (NULL) as costcontext to LZ77OptimalRun, whose contract requires `is_fresh(costcontext, sizeof(SymbolStats))` — that precondition would fail if the body were reachable, but vacuity hides it too. Sibling of [[cbmc-followpath-vacuity]] / [[cbmc-tracebackwards-append-depth-vacuity]]. Strong sound spec (union of callee preconditions; ensures blockstart==instart, blockend==inend) left in place.
