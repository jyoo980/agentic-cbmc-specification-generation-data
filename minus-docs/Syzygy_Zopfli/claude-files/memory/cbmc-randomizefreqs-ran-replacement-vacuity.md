---
name: cbmc-randomizefreqs-ran-replacement-vacuity
description: RandomizeFreqs in zopfli.c verifies but 0/8; multiple Ran() contract-replacement calls on the same state make the exit unreachable (NOT loop-length driven)
metadata: 
  node_type: memory
  type: project
  originSessionId: f1c17eb0-efbc-4450-8136-d323600b84d2
---

`RandomizeFreqs(state, freqs, n)` in `/app/Syzygy_Zopfli/c_code/zopfli.c` (~line 4470)
verifies but kills 0/8. Spec left in place: `is_fresh(state)`, `n>0 && n<=ZOPFLI_NUM_LL`,
`is_fresh(freqs, n*sizeof(*freqs))`, `assigns(state->m_w, state->m_z, object_whole(freqs))`.
No sound functional ensures is possible (body is RNG-driven: `freqs[i]=freqs[Ran()%n]` guarded by `(Ran()>>4)%3==0`).

**Vacuity cause = Ran contract replacement, NOT loop length.** The loop body calls
`Ran(state)` multiple times (once per iteration for the guard, again for the copy index),
and [[cbmc-warmuphash-replacement-vacuity]]-style: `Ran`'s contract requires
`__CPROVER_is_fresh(state)`. Under contract *replacement* the repeated `is_fresh(state)`
re-assertion on the same object makes the post-state unreachable. Proven: a false
`ensures(freqs[0]==123456789)` still "Verifies" even when the bound is tightened to
`n<=2` (so a tiny loop is irrelevant) — same 3 loop-emptying mutants die in both cases
(those make the loop a no-op → harness becomes shallow/non-vacuous → false ensures then fails for them).

Same family as [[cbmc-warmuphash-replacement-vacuity]] (callee contract replaced on
multiple body calls → exit unreachable). Strong sound spec left in place.
