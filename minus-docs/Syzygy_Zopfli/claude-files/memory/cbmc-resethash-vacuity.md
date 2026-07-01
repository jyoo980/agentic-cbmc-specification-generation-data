---
name: cbmc-resethash-vacuity
description: "ZopfliResetHash in zopfli.c verifies vacuously (0/25) — ~230K body loop iterations exceed CBMC's depth bound so postconditions are never reached"
metadata: 
  node_type: memory
  type: project
  originSessionId: 2907f7fc-2840-4867-a5d2-9f8179c0589e
---

`ZopfliResetHash` (zopfli.c ~line 2922) verifies but kills 0/25 — vacuous.
Body clears two 65536-entry head tables plus three window_size (32768) per-window
arrays = ~230K loop iterations, far beyond CBMC's depth bound, so the function's
exit (and all `__CPROVER_ensures`) is never reached. Tell-tale sign: ALL mutants
survive, even loop-body-never-executes ones like `i > 65536`.

Left in place a strong sound spec: `requires(window_size == ZOPFLI_WINDOW_SIZE)`,
is_fresh for h + head/head2 (65536) + prev/hashval/prev2/hashval2/same (window_size),
object_whole assigns, and `ensures` forall asserting head[i]==-1, prev[i]==i,
hashval[i]==-1, same[i]==0 (+ "2" variants), val==val2==0.

Same root cause family as [[cbmc-depth200-isfresh-vacuity]]; loop contracts can't
rescue it per [[cbmc-loop-contracts-ignored]]. Not fixable via spec strengthening
without editing the C code.
