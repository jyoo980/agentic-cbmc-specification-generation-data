---
name: cbsss-depth200-vacuity
description: "CalculateBlockSymbolSizeSmall scores 0 kills under hardcoded --depth 200; loop body sits behind the contract prologue, inherent not weak spec"
metadata: 
  node_type: memory
  type: project
  originSessionId: 1c06f194-9788-4093-9c39-5e79760cfd8d
---

`CalculateBlockSymbolSizeSmall` (zopfli.c) verifies with a strong, complete,
sound contract (is_fresh for ll_lengths[288], d_lengths[32], lz77, lz77->litlens,
lz77->dists; forall over [lstart,lend) pinning litlens<=258 and dist!=0 ==>
litlens>=3 && dist<=32768; lz77->size<=3; ensures return>=ll_lengths[256]; plus an
in-loop `__CPROVER_assert` bound invariant). But it scores **0/16 kills** — an
instance of [[avocado-depth200-vacuity]].

**Why:** the avocado pipeline replaces the 4 callees with contracts and runs
`cbmc --depth 200`. Measured: the loop BODY is first reachable only between depth
300–400 (assert(0) marker test). The static-init floor is ~125; each `is_fresh`
adds ~40 steps, so the 5 memory preconditions push the body past 200. Every
in-loop assertion (the i<size and litlens<259 asserts the mutants target, plus the
branch) is therefore never symbolically reached and passes VACUOUSLY, so all 16
mutants survive. Even the minimal sound set needed to reach the litlens assert
(2 is_fresh, no forall) does NOT reach it at depth 200.

**How to apply:** don't chase these kills. Reducing is_fresh count would only expose
the shallowest assert by DROPPING necessary memory preconditions — a weaker/incomplete
spec, not a stronger one. Keep the complete sound contract; the value delivered is
that it verifies. Kill harness: /app/kill_cbsss.py (mirrors run_cbmc.py with the 4
callees replaced). Same call as [[adddynamictree-out-zero-regime]] family.

**Reconfirmed 2026-06-27:** baseline still VERIFICATION SUCCESSFUL at depth 200;
kill_cbsss.py → 0/16 (all 16 mutants are on the loop guard line 316, in-body asserts
lines 322/323, and the branch line 324). assert(0) probe at top of loop body: SUCCESSFUL
(vacuous) at depth 200 AND 300, only FAILED (body reached) at depth 400. Stripped requires
to just the 5 mandatory is_fresh (dropped forall + size<=3 + ensures — can't drop is_fresh
or the body's derefs go memory-unsafe): probe still SUCCESSFUL at 200 AND 250, FAILED only
at 300. So the prologue floor (~300 steps, whole-file global nondet init + enforce-contract
harness + 5 is_fresh allocs) cannot be brought under 200 by any sound spec rewrite. 0/16 is
firm; left the strongest complete contract byte-identical.
