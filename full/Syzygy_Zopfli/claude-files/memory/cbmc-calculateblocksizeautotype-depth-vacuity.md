---
name: cbmc-calculateblocksizeautotype-depth-vacuity
description: ZopfliCalculateBlockSizeAutoType verifies soundly but kill 0/21; same depth-wall + contradictory-lend structure as AddLZ77BlockAutoType
metadata: 
  node_type: memory
  type: project
  originSessionId: cb2da6b3-1ee7-4429-8158-f2599b0c5226
---

`ZopfliCalculateBlockSizeAutoType` (zopfli.c ~2106) verifies soundly with an
`is_fresh(lz77)` + `lend<=2` + pos/dists/litlens(2-elem) precondition, empty
`__CPROVER_assigns()`, and `return_value >= 0`, but mutation **kill 0/21**.

Cost-computing twin of [[cbmc-addlz77blockautotype-depth-vacuity]]. Body makes up
to 3 `ZopfliCalculateBlockSize` calls (btype 0, conditionally 1 when size<=1000,
and 2). Two compounding reasons it's vacuous:
- Depth wall: the first (btype==0) call's heavy is_fresh store setup exhausts
  CBMC's harness-fixed `--depth 200` before the return min-expression/ensures is
  reached (same wall as the [[cbmc-calculateblocksize-dispatcher-depth-vacuity]]).
- Structurally undischargeable at unbounded depth: btype==0 needs `lend<=2`,
  btype 1/2 need `lstart==0 && lend>=ZOPFLI_NUM_LL*3 (864)` on the SAME lstart/lend.

All 21 survivors are body relational/logical mutants (the `size>1000` ternary and
the `< && <` three-way min at lines ~2149/2151). Needs `-I /app/Syzygy_Zopfli/stubs`
for FILE.h. Contract retained as strong memory-safety spec documenting intent.
