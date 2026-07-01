---
name: csf-6of10
description: "ClearStatFreqs hits 6/10 via sampled index-0 postconditions; 4 survivors inherent (2 unwind-bounded <=, 2 equivalent !=)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 849f5c37-de43-4999-a904-ea1882ca457c
---

ClearStatFreqs (two memset-style loops: litlens[0..287]=0, dists[0..31]=0) scores
**6/10** — beats the usual loop-func depth-200 vacuity. 10 mutants, all loop-condition
(<=,>,>=,==,!= on each of the two loops).

**Key insight:** the OFFICIAL scoring flow is the documented one in
`docs/running-cbmc-directly.md` — `goto-instrument --partial-loops --unwind 5` THEN
`cbmc --depth 200` (NOT pure --depth 200, and NOT dfcc/loop-contracts). `--unwind 5`
caps BOTH loops at 5 iterations, so even the 288-iter first loop stays short and the
postcondition is REACHED (my initial worry that loop1 blows depth 200 was wrong — unwind
bounds it). This is why sampled boundary postconds work here but fail for funcs whose
prologue/callees alone exceed depth 200.

**Spec that gets 6:** `requires is_fresh(stats,sizeof(*stats))` +
`assigns(object_whole(stats->litlens))` + `assigns(object_whole(stats->dists))` +
`ensures stats->litlens[0]==0` + `ensures stats->dists[0]==0`. Sample index 0 (loop-entry
boundary) — NOT a forall (elements 5..287 never written under partial-loops/unwind-5, so a
full-array forall would fail on the ORIGINAL). Kills all six 0-iteration mutants
(>,>=,== on each loop): loop runs 0 iters → sampled entry unwritten (nondet) → ensures fails.

**4 survivors are inherent, don't chase:**
- 2× `<=` (litlens i<=288, dists i<=32): the extra OOB iteration is at index 288/32, past
  the unwind-5 bound, so never reached → indistinguishable from `<` under this flow. Killing
  it would require raising --unwind, a CLI arg (forbidden to hardcode against).
- 2× `!=`: true equivalent mutants (0-start, +1 step ⇒ `!=N` ≡ `<N`).

Scripts: /app/kill_csf.sh (documented flow, line-targeted sed mutation). Contrast the dfcc
loop-contract path [[zic-dfcc-loopcontracts-12of16]] (that uses an EXTERNAL harness, NOT the
official in-file scorer). Related vacuity baseline: [[avocado-depth200-vacuity]].
