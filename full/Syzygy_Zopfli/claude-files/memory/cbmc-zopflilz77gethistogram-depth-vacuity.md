---
name: cbmc-zopflilz77gethistogram-depth-vacuity
description: ZopfliLZ77GetHistogram verifies soundly but kill 0 under depth-200
metadata: 
  node_type: memory
  type: project
  originSessionId: 00d866f4-cb50-41d2-a096-e72fc23030ad
---

`ZopfliLZ77GetHistogram` in /app/Syzygy_Zopfli/c_code/zopfli.c verifies (exit 0)
but mutation kill = 0/29 (all survive). Same depth-200 wall as its callee
[[cbmc-gethistogramat-depth-vacuity]]: the contract needs 8 symbolic-size
is_fresh allocations (store + ll_counts/d_counts cumulative + ll_symbol/d_symbol/
dists + 2 output histograms) plus 2 symbolic-range forall preconditions to cover
both the small-block walk and the GetHistogramAt-subtraction branch. These
exhaust depth 200 during contract setup before any branch body runs, so the
proof is vacuous.

**Why:** branch 2 calls ZopfliLZ77GetHistogramAt(lz77, lend-1/lstart-1), so the
caller must reproduce that callee's heavy is_fresh preconditions — at least as
expensive as the (already vacuous) callee.

**How to apply:** kept the full sound contract with the NB vacuity note, matching
every sibling in this file. Build needs BOTH `-I /app/Syzygy_Zopfli/c_code` and
`-I /app/Syzygy_Zopfli/stubs` (line 7 includes x86_64-linux-gnu/bits/types/FILE.h
which only exists under stubs/). Narrowing to lstart==lend would escape the wall
(3 is_fresh) and kill the memset mutants, but is a degenerate spec — not worth
abandoning the functional characterization.
