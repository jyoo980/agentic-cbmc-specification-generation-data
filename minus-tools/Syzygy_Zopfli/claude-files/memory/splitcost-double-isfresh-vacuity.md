---
name: splitcost-double-isfresh-vacuity
description: SplitCost 0/1 — single +/- mutant unkillable; double is_fresh callee-precond + depth-200 vacuity
metadata: 
  node_type: memory
  type: project
  originSessionId: 3e0521f2-75af-4686-bb60-80c51e377b53
---

`SplitCost(i, context)` in zopfli.c calls `EstimateCost(c->lz77, c->start, i) + EstimateCost(c->lz77, i, c->end)` — only mutant is `+`→`-`.

Verifies @depth200 with a full sound contract (is_fresh on context + lz77 + 7 arrays + 3 foralls mirroring EstimateCost's precond over [start,end), assigns(), exact-value ensures: ret>=0, both-empty==0, both-nonempty>=6, one-empty>=3). But **0/1 kills** — inherent, don't chase.

Two compounding blockers:
1. **Deep vacuity**: postcondition unreachable until ~depth 800-1000 (false-ensures passes @200, @400, @600; fails @1000). The prologue is dominated by discharging EstimateCost's heavy precondition (8 is_fresh + 3 foralls) at BOTH call sites.
2. **Double is_fresh**: SplitCost calls EstimateCost twice with the SAME `c->lz77`. Under `--replace-call-with-contract EstimateCost`, the callee precondition `is_fresh(lz77,...)` is asserted at each call; is_fresh can only hold once per object, so the 2nd call's precond FAILS. Confirmed: @depth1000 BOTH original and mutant FAIL `[EstimateCost.precondition] Check requires clause` — so the spec can never verify-and-kill simultaneously.

Can't substitute a weak (r_ok) stub for EstimateCost: official run_cbmc replaces the in-file real contract, not a stub. No lever recovers kills. Left the sound-shaped contract that passes @200 vacuously. Related: [[flsb-depth200-vacuity]], [[ec-no-mutants]], [[findminimum-fptr-param-crash]] (SplitCost is FindMinimum's fptr target).
