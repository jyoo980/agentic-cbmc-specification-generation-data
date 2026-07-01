---
name: zcbsat-depth200-vacuity
description: "ZopfliCalculateBlockSizeAutoType verifies @200 but 0/21 kills; histogram is_fresh prologue from mandatory btype==2 replace-call pushes empty-window postcondition past depth 200, killed @1200"
metadata: 
  node_type: memory
  type: project
  originSessionId: 65e5d3ef-e0ab-491e-9450-a38b16365375
---

ZopfliCalculateBlockSizeAutoType (zopfli.c ~line 2035) — the AutoType dispatcher
that calls ZopfliCalculateBlockSize with btype 0/1/2 and returns the minimum.

Verifies @depth 200 with a full union-of-callees precondition contract (is_fresh
of lz77 + litlens/dists/pos/ll_symbol/d_symbol/ll_counts(288)/d_counts(32) +
3 foralls, size<=3) and exact postcondition `lstart==lend ==> return==0` plus
`lstart!=lend ==> return>=3`. Replace-call ZopfliCalculateBlockSize.

**0/21 kills @depth 200** — depth-200 vacuity, same family as [[zcbs-depth200-vacuity]]
(ZopfliCalculateBlockSize 0/6) and [[alzbat-depth200-vacuity]] (AddLZ77BlockAutoType 0/29).

**Why:** the empty-window postcondition (unc=0 < fixed>=3, dyn>=29 ⇒ return 0) is
genuinely strong — it kills the >/>=/==-on-both-operands mutants of the selection
test. Confirmed: mutant `unc<fixed && unc>dyn` gives VERIFICATION FAILED @depth 1200.
But the dominant prologue cost is the 288-wide ll_counts + 32-wide d_counts is_fresh,
which are MANDATORY: the btype==2 replace-call asserts ZopfliCalculateBlockSize's
histogram is_fresh preconditions, so they can't be dropped or guarded. That prologue
pushes the postcondition assertion past the depth-200 frontier → all 21 mutants
vacuously survive @200.

**How to apply:** don't chase. Line-2045 min/max mutants and lstart!=lend-regime
mutants are additionally unkillable under contract replacement anyway (fixed/dyn are
nondeterministic within bounds, return not pinnable). r_ok lever expected to fail
(documented failing for sibling ZopfliCalculateBlockSize). Kill script:
/app/kill_zcbsat.sh (takes depth arg, default 200).
