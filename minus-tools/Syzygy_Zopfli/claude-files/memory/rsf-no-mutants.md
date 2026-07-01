---
name: rsf-no-mutants
description: RandomizeStatFreqs has no mutants (kill score inherently 0); verifies @200 with the strong RandomizeFreqs contract via replace-call — is_fresh on struct fields discharges
metadata: 
  node_type: memory
  type: project
  originSessionId: f7f64144-2147-4480-807b-ab681b801595
---

RandomizeStatFreqs (Syzygy_Zopfli/c_code/zopfli.c, ~line 4977) calls
RandomizeFreqs twice on SymbolStats fields — (state, stats->litlens, 288) and
(state, stats->dists, 32) — then sets stats->litlens[256]=1.

get-mutants reports "No mutant(s) generated (no mutable operators)" —
kill score inherently 0, don't chase. Same family as [[cs-no-mutants]],
[[copystats-no-mutants]], [[initstats-no-mutants]].

**Verifies PASS @depth200 under the OFFICIAL flow** (replace-call-with-contract
RandomizeFreqs + enforce RandomizeStatFreqs) with the strongest sound spec:
is_fresh(state) + is_fresh(stats) + assigns(object_whole litlens, dists, *state)
+ ensures(stats->litlens[256]==1). The only deterministic output is the end
symbol; the two RandomizeFreqs calls write data-dependent random values, so
litlens[256]==1 is the only pinnable postcondition.

**KEY contrast with [[cs-no-mutants]] (CalculateStatistics):** there the *callee*
is_fresh precondition fails under replace-call because (a) the real
ZopfliCalculateEntropy contract pins n==288 (breaks the n=32 call) AND (b)
is_fresh on struct fields. Here RandomizeFreqs's contract is only requires(n>0)
(no n-pin), so replace-call substitutes the concrete 288/32 and CBMC discharges
is_fresh(stats->litlens, 288*8) / is_fresh(stats->dists, 32*8) directly from
is_fresh(stats) — NO weak stub needed, unlike calcstats_weak.c. The takeaway:
is_fresh on a struct sub-field IS dischargeable under replace-call when the size
arg is concrete and the parent struct is is_fresh; it only fails when the callee
contract additionally pins an incompatible constant.

Verified via /app/_run_official.py <FUNC> (calls tools.run_cbmc.run_cbmc, the
exact harness ground-truth flow). Run it from /app, NOT c_code (the leftover
c_code/bisect.py shadows stdlib bisect and crashes the import).
