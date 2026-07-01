---
name: cbmc-addlz77blockautotype-vacuity
description: AddLZ77BlockAutoType in zopfli.c verifies vacuously 0/29; ~14 is_fresh objects exceed depth 200
metadata: 
  node_type: memory
  type: project
  originSessionId: 42517c05-d33d-4bf6-a165-7fbabf0af510
---

`AddLZ77BlockAutoType` (zopfli.c) verifies but kills 0/29 — depth-bound vacuity, same as [[cbmc-followpath-vacuity]] and [[cbmc-lz77optimalfixed-vacuity]]. The precondition needs ~14 `is_fresh` objects (the `lz77` store + its 8 backing arrays litlens/dists/pos/data/ll_symbol/d_symbol/ll_counts/d_counts, plus `options`, `bp`, `out`, `*out`, `outsize`), built as the union of the callee contracts (`ZopfliCalculateBlockSize` ×3 reads every array; `AddLZ77Block`/`AddBits` need the bitstream). The harness setup exceeds `--depth 200` before any body statement, so all surviving mutants are in-body (lines ~4002/4018/4027).

No non-vacuous middle ground: the three `ZopfliCalculateBlockSize` calls genuinely read all arrays, so no `is_fresh` can be dropped. Even if the body were reachable it would FAIL not survive — the `btype==2` else-branch violates `AddLZ77Block`'s `btype==0||btype==1` precondition, and `fixedstore` is `ZopfliInitLZ77Store`'d to size==0 but `ZopfliLZ77OptimalFixed`'s contract requires store->size==3. Strong sound spec left in place. **Why:** matches established codebase pattern; per CLAUDE.md a few vacuous verifications are acceptable. **How to apply:** don't try to chase kills here by weakening freshness.
