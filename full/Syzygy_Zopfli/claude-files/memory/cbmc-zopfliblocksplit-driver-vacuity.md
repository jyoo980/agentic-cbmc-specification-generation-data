---
name: cbmc-zopfliblocksplit-driver-vacuity
description: "ZopfliBlockSplit verifies soundly but kill 0/14; no early return, Greedy precondition contradicts produced state"
metadata: 
  node_type: memory
  type: project
  originSessionId: 5ac54c5e-8351-4c72-a5cb-99014b5769ad
---

`ZopfliBlockSplit` (zopfli.c ~5792) verifies soundly with a memory-safety contract
(is_fresh options/in/splitpoints/npoints, `inend > instart`, assigns `*splitpoints,*npoints`,
ensures `*npoints == 0`) but **kill 0/14** — all mutants in the post-Greedy LZ77-position
conversion loop/conditions (lines ~5859–5873).

**Why vacuous, and why it's a hard wall:** Unlike [[cbmc-zopfliblocksplitlz77-callee-precondition-wall]],
this driver has NO early return. It unconditionally calls `ZopfliLZ77Greedy`, whose replaced
contract demands fresh `s->lmc` + `store->size==3` + `inend==8`. But the code immediately above
produces `s->lmc==NULL` (`ZopfliInitBlockState` called with add_lmc==0) and `store->size==0`
(`ZopfliInitLZ77Store`). So the Greedy call site is only reachable on an infeasible path;
ZopfliAllocHash's 7 is_fresh ensures + InitLZ77Store/InitBlockState exhaust the depth-200 budget
before the contradiction is flagged, so it passes vacuously and every downstream mutant survives.
Cannot be fixed without editing the C code. See [[cbmc-lz77greedy-depth-vacuity]].

**Build:** needs `-I .` AND `-I /app/Syzygy_Zopfli/stubs` (absolute) for FILE.h stub.
