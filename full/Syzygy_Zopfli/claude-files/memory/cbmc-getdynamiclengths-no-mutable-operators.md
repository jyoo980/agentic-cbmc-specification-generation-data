---
name: cbmc-getdynamiclengths-no-mutable-operators
description: "GetDynamicLengths verifies (vacuously) but mutation testing reports \"no mutable operators\""
metadata: 
  node_type: memory
  type: project
  originSessionId: 3a909a0a-0dd5-4b68-bf50-21677d78a000
---

`GetDynamicLengths` in `/app/Syzygy_Zopfli/c_code/zopfli.c` verifies with a sound
contract (lstart==0 && lend>=ZOPFLI_NUM_LL*3, is_fresh lz77 + its 5 internal
arrays + ll_lengths/d_lengths, two symbol-bound foralls; assigns the two length
arrays; ensures return >= 14+12+CBSGC_RESULT_MIN, mirroring its only callee
TryOptimizeHuffmanForRle).

Two notable facts:
- The proof is **vacuous** (depth-200 wall): `__CPROVER_ensures(1==0)` also
  "verifies". The first call ZopfliLZ77GetHistogram's heavy is_fresh setup +
  two symbolic foralls exhausts depth 200 before the return. Same wall as
  [[cbmc-zopflilz77gethistogram-depth-vacuity]] and [[cbmc-tryoptimizehuffmanforrle-depth-vacuity]].
- Mutation testing reports **"no mutable operators"** — the body is pure call
  dispatch (histogram → 2x bitlengths → patch → return TryOptimize), so there is
  no kill score to optimize regardless of vacuity. Verification success is the
  only achievable signal here.

Build: needs `-I /app/Syzygy_Zopfli/stubs` (absolute) for FILE.h, per
[[cbmc-mutation-include-must-be-absolute]].
