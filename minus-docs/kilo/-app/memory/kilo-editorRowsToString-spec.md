---
name: kilo-editorrowstostring-spec
description: editorRowsToString verifies but vacuous (1/11 kill) due to --depth 200 cutting paths through its two loops before the postcondition; non-vacuous at depth ~400. numrows==0 excluded (malloc frame quirk).
metadata: 
  node_type: memory
  type: project
  originSessionId: 2969d6a7-8d15-4fb2-b23b-87befa2a30db
---

`editorRowsToString` (mallocs a buffer, two loops over `E.numrows`) verifies under
avocado but is **vacuous → kill 1/11**. Root cause is NOT the usual is_fresh-UNSAT
poison — the preconditions are satisfiable (proven with an isolated probe).

**The cause is avocado's fixed `--depth 200`.** Every path through the two row
loops (each unwound 5×) plus `__CPROVER_initialize` of the big `editorConfig E`
exceeds 200 instructions before reaching the postcondition, so CBMC cuts the path
and the ensures pass vacuously (note the "Depth-bounded analysis may yield unsound"
warning). Threshold measured empirically: vacuous at depth ≤300, **non-vacuous at
depth ≥400** (false `*buflen==999999` ensures FAILS, real `*buflen==size+1` ensures
catches mutants). Loop-free kilo funcs (e.g. [[kilo-editorMoveCursor-spec]]) fit in
200; this two-loop body does not. Unavoidable via the contract — can't change
`--depth`, can't shorten the body. Detect vacuity with the `*buflen==999999`
false-ensures trick; confirm depth cause by re-running cbmc directly at `--depth 400`.

**numrows==0 is excluded** from the precondition: with the malloc stub, CBMC's
assigns-frame only recognizes the freshly-malloc'd return buffer as assignable once
it's been written inside the loop, so the final `*p='\0'` fails the assigns check
when the loop never runs (numrows==0). numrows∈[1,2] verifies; numrows==0 fails
`editorRowsToString.assigns.24` (line 757).

Also relevant: the **memcpy stub is a no-op** (`/app/stubs/memcpy.c` ignores src/n),
so `E.row[j].chars` is never actually dereferenced — the `is_fresh(chars)` requires
are kept only for spec soundness, not needed by the toolchain. The **malloc stub**
(`/app/stubs/malloc.c`) is always linked for malloc-callers and uses
`__CPROVER_allocate`; enforce-contract does NOT special-case it as a fresh allocator
(only bodiless `malloc` gets that), which is why buffer-write framing is fragile.
