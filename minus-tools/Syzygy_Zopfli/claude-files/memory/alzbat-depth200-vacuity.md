---
name: alzbat-depth200-vacuity
description: AddLZ77BlockAutoType verifies (empty-block regime) but 0/29 kills — depth-200 vacuity (threshold ~1600) + incompatible-btype branches
metadata: 
  node_type: memory
  type: project
  originSessionId: e44301db-9b62-4538-abd7-bcae31646267
---

AddLZ77BlockAutoType (zopfli.c) is the cost-driven dispatcher above
[[addlz77block-depth200-vacuity]]. It prices btype 0/1/2 with
ZopfliCalculateBlockSize, emits the empty block directly (lstart==lend), or
recompresses with a fixed tree and forwards the winner to AddLZ77Block.

**Verifies with a sound spec but kill score is 0/29 @depth200 — inherent, not weak.**
Only the **empty-block regime (lstart==lend)** is verifiable:
- size<=3 (ZopfliCalculateBlockSize's contract) is always <1000, so
  `expensivefixed` is unconditionally true → the non-empty path ALWAYS drives
  ZopfliInitBlockState/ZopfliLZ77OptimalFixed/ZopfliCleanBlockState, whose
  replace-contract obligations on the locally-built block-state/fixedstore don't
  compose (e.g. ZopfliCleanLZ77Store demands is_fresh of all-NULL fixedstore arrays).
- The three branches call AddLZ77Block with btype 0/1/2 — mutually incompatible
  contracts (only btype==0 verifiable). Verifying the original needs inputs that
  pin ONE reachable branch, which makes every branch-decision mutant (lines
  `uncompressedcost < fixedcost && < dyncost`, `fixedcost < dyncost`) EQUIVALENT
  in-regime: they diverge only at cost equalities the single-branch precondition
  excludes, and the diverted branch is unverifiable anyway.

Spec: empty regime, is_fresh(lz77)+litlens/dists/pos + dynamic-path histogram
is_fresh (ll_symbol/d_symbol/ll_counts/d_counts) + foralls; *outsize==1 fresh
1-byte out, *bp<=7. Replace ZopfliCalculateBlockSize, ZopfliLZ77GetByteRange,
ZopfliInitBlockState, ZopfliLZ77OptimalFixed, ZopfliCleanBlockState, AddLZ77Block,
ZopfliCleanLZ77Store. **AddBits left INLINED** — its tight no-append contract
(*bp+length<=8) can't be chained across the 1+2+7 header bits (3rd call needs
*bp==1 going in, forcing *bp==7 into the 2nd call, which violates *bp+2<=8); so
the header writes go through reallocating ZOPFLI_APPEND_DATA (needs alloc stub).
ZopfliInitLZ77Store also inlined (sets fields on the local fixedstore).
Verifies: 0 of 31327 failed @depth200.

28 mutants are in the unreachable non-empty body → vacuous. The 29th
(`lstart==lend -> lstart!=lend`) diverts the empty input into recompression: at
depth ~1600 it FAILS (OOB `lz77->pos[lstart]` read at the size boundary, AND
ZopfliInitBlockState's `blockstart<blockend` precondition with instart==inend).
SUCCESS@200/300/500/800/1000/1300, FAILED@1600 — classic depth-200 vacuity.

Same family as [[addlz77block-depth200-vacuity]], [[avocado-depth200-vacuity]],
[[zcbs-depth200-vacuity]]. Don't chase. Scripts: /app/_verify_alzbat.sh,
/app/kill_alzbat.sh (takes optional depth arg).
