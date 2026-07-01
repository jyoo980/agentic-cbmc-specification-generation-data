---
name: zlz77ghat-depth200-vacuity
description: ZopfliLZ77GetHistogramAt scores 0/41 kills — 8 is_fresh prologue pushes loop body past hardcoded --depth 200
metadata: 
  node_type: memory
  type: project
  originSessionId: f870ffcd-242f-412f-b0bb-2a8c1773beb7
---

`ZopfliLZ77GetHistogramAt` (Syzygy_Zopfli/c_code/zopfli.c) verifies SUCCESSFUL with a strong
memory-safety contract (is_fresh on lz77 + ll_counts/d_counts outputs + cumulative ll_counts/d_counts
+ ll_symbol/d_symbol/dists arrays, plus two foralls bounding symbol values < ZOPFLI_NUM_LL/_D),
but kill score is **0/41** — another [[avocado-depth200-vacuity]] case.

**Why:** all 41 mutants sit inside the 4 loops (copy + decrement, over ZOPFLI_NUM_LL=288 / ZOPFLI_NUM_D=32).
The contract needs ~8 is_fresh objects for the original's memory safety. That prologue makes even the
FIRST loop-body check (line ~1610, cumulative read) land at depth ~250-300, past the hardcoded `--depth 200`,
so every in-body check is vacuous. Verified: the `llpos = 288*(lpos/288)` → `lpos*288` mutant FAILS (real OOB)
at unlimited depth and at --depth 300, but SUCCEEDS (vacuous) at --depth 200.

**How to apply:** don't chase — proven impossible at depth 200. Re-measured 2026-06-27 with a per-line
depth-attribution harness (threshold = smallest --depth at which the llpos `/`→`*` mutant FAILS):
- Full valid spec: threshold **295**.
- The cost is the FIELD is_fresh allocations (ll_counts/d_counts outputs + cumulative + 3 big arrays),
  each ~20 steps. They are gated by `is_fresh(lz77)`: drop `is_fresh(lz77)` and every `lz77->X` is_fresh
  silently no-ops (threshold collapses to 150), so they can't be cheapened without un-freshing lz77.
- The two foralls HELP (they prune the path): removing them RAISES threshold to 350. Keep them.
- Absolute floor — bare minimum to even reach loop1's OOB read (is_fresh lz77 + lpos<size + ll_counts
  output fresh + lz77->ll_counts cumulative fresh, an INVALID spec that ignores loops 2-4): threshold **215**,
  still > 200. So no valid spec can kill even the earliest mutant at depth 200.
- Lean valid spec (lpos==size-1 → loops 2/4 empty, drop 3 big arrays + foralls): threshold 260, still > 200
  AND weakens the precondition. Not worth it.
An ensures can't help either: body is cut before reaching it.
The counting semantics (ll_counts[j] = cumulative - #{i: ll_symbol[i]==j}) are also not expressible as a
CBMC ensures. Kill harness: Syzygy_Zopfli/c_code/kill_zlghat.py.
