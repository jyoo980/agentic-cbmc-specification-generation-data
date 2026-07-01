---
name: cbmc-clearstatfreqs-unwind-survivors
description: ClearStatFreqs verifies soundly at kill 0.6 with full forall-zero postcondition; survivors are unwind-cap loop-bound mutants
metadata: 
  node_type: memory
  type: project
  originSessionId: f311232f-bf54-4e33-a149-5d7fca73623c
---

ClearStatFreqs (zopfli.c:5195) zeros embedded fixed arrays litlens[288]/dists[32].
Contract: requires is_fresh(stats, sizeof(SymbolStats)); assigns stats->litlens,
stats->dists; two forall ensures (all litlens==0, all dists==0).

Verifies SOUNDLY (not vacuously like sibling [[cbmc-addweighedstatfreqs-depth-vacuity]])
at kill 0.6 (6/10). The forall postconditions genuinely check and kill value mutants.

The 4 survivors are ALL loop-bound relational mutants on the counter conditions
(`< -> <=` and `< -> !=` at lines 5208/5210). Under the harness partial-loops/unwind-5
cap, the loop runs only 5 iterations, never reaching iteration 288/32 where these
mutants would diverge (OOB write or non-termination), so they are unkillable artifacts
of the unwind cap — same pattern as [[cbmc-zopflisublentocache-unwind-wall]]. 0.6 is the
ceiling. Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.
