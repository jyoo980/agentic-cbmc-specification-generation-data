---
name: awsf-depth200-vacuity
description: "AddWeighedStatFreqs verifies @depth200 w/ exact-value spec but 0/12 kills; two FP-heavy unwound loops push every postcondition (incl explicit litlens[256]=1) past depth ~500, r_ok lever fails, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: ed9b4ae6-e74a-466f-84d2-afdb0ba5387a
---

`AddWeighedStatFreqs` in `Syzygy_Zopfli/c_code/zopfli.c` (~line 4889): two simple
loops, `result->litlens[i] = (size_t)(s1->litlens[i]*w1 + s2->litlens[i]*w2)` over
288, then dists over 32, then `result->litlens[256] = 1`. NO callees.

**Verifies SUCCESSFUL @depth200** with the maximally-strong spec: is_fresh on the 3
SymbolStats structs, `assigns(object_whole(result))`, exact-value sampled postconditions
`result->litlens[0]==(size_t)(s1->litlens[0]*w1+s2->litlens[0]*w2)`, same for dists[0],
and `result->litlens[256]==1`. Kill score **0/12** under real harness (`--partial-loops
--unwind 5`, `--enforce-contract`, `--depth 200`).

**12 mutants:** 5 loop-bound on each `for` guard (`<= > >= == !=`) + 2 arithmetic
(`+`→`-` on litlens body and on dists body). Exact-value postcond is the strongest
possible: it kills `> >= ==` (0-iter → witness elem unwritten), `+`→`-` (value diverges),
and `litlens[256]` mutations. `<=` (OOB, needs full 288-iter loop, never reached under
unwind 5) and `!=` (0-start +1-step loop ≡ `<`, equivalent mutant) are inherently
unkillable — so the real ceiling even at high depth is ~8/12, not 12.

**Why 0 @200 — FP-loop depth ceiling, NOT prologue, NOT weak spec:**
- False `ensures(result->litlens[0]==123456789)` PASSES @200 → postcond unreachable.
- False `ensures(result->litlens[256]==99)` PASSES @200/300/400 (the EXPLICIT post-loop
  write is itself unreachable), times out @600. Frontier is ~500-600.
- The `+`→`-` litlens mutant still SURVIVES @depth500 (postcond checked at function exit,
  which is reached only AFTER both loops fully unwound = 5+5 floating-point mul/add/cast
  iterations — the dominant symbolic-execution cost here).
- **r_ok lever FAILS** (unlike [[zlz77gbr-rok-beats-vacuity]]): swapping the 3 is_fresh →
  r_ok/w_ok still leaves `litlens[256]==99` passing @200. The cost is the unwound FP loop
  bodies before exit, NOT the prologue, so no prologue shrink moves the frontier under 200.
- Loop contracts are inert here (no `--apply-loop-contracts`), see [[avocado-depth200-vacuity]].

Kept the strongest exact-value spec (byte-identical, re-verified SUCCESSFUL @200). Same
class as [[avocado-depth200-vacuity]] (ExtractBitLengths/InitLists/AddLZ77Data etc.). Don't
chase. Kill harness: `/tmp/killscore.py` (parses get-mutants diffs, applies each,
runs the real pipeline).
