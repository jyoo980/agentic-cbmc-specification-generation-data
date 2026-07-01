---
name: irs-no-mutants
description: "InitRanState is a constant RNG-seed initializer; avocado generates 0 mutants, kill score inherently 0"
metadata: 
  node_type: memory
  type: project
  originSessionId: c2de0ef5-6d21-4880-93e7-952079290fbe
---

`InitRanState` (zopfli.c) sets `state->m_w = 1; state->m_z = 2;` — pure constant
initializer of the Marsaglia MWC RNG, no operators. `get-mutants` returns
"No mutant(s) generated (no mutable operators)", so kill score is inherently 0.

Verifies @depth200 (official --unwind 5 + --depth 200 flow) with the strongest
spec for a constant init: `is_fresh(state)` + `assigns(object_whole(state))` +
exact-value ensures `m_w==1` and `m_z==2`. Same pattern as [[uhv-no-mutants]],
[[zilz77s-no-mutants]]. Don't chase kills.
