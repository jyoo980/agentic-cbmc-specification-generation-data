---
name: cbmc-addlz77blockautotype-depth-vacuity
description: "AddLZ77BlockAutoType verifies soundly but kill 0/29; 3 unconditional ZopfliCalculateBlockSize calls exhaust depth 200, and contract is structurally undischargeable"
metadata: 
  node_type: memory
  type: project
  originSessionId: aa21e8de-f7c1-4156-b8f7-5b23e7bb3cff
---

`AddLZ77BlockAutoType` (zopfli.c) verifies soundly with a strong memory-safety
contract (is_fresh options/lz77/bp/out/outsize, *bp<=7, *outsize==3, lend<=2 +
fresh pos/dists/litlens[2]; assigns *bp,*outsize,object_whole(*out); ensures
*bp<=7 && *outsize>=old) but mutation **kill 0/29** — all survivors are body
mutants (expensivefixed `<`/`||`, inend `+`, the cost `<`/`&&` comparisons).

**Why depth-vacuous:** the body opens with THREE unconditional
`ZopfliCalculateBlockSize(lz77,lstart,lend, 0/1/2)` calls. Each replaced contract
carries heavy `__CPROVER_is_fresh` setup for the store + its arrays; that setup
alone exhausts the harness's `--depth 200` before any body branch is reached, so
every body assertion/mutant passes vacuously. Same depth wall as the sibling
[[cbmc-calculateblocksize-dispatcher-depth-vacuity]] and [[cbmc-addlz77block-addbit-vacuity]].

**Also structurally undischargeable at unbounded depth** (so kill can't be
recovered even without the depth cap): the btype==0 call of ZopfliCalculateBlockSize
requires `lend<=2` while the btype!=0 calls require `lend>=ZOPFLI_NUM_LL*3`(=864) —
contradictory on one `lend`. And the locally `ZopfliInitLZ77Store`-initialised
fixedstore (size==0, null arrays) cannot meet `ZopfliLZ77OptimalFixed`'s replaced
precondition (size==3, fresh arrays). Pinned preconditions to the first-reached
(btype==0) call; the rest are unreachable at depth 200. Verdict: accept kill 0,
contract retained as documentation of intent. Don't re-attempt.
