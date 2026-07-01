---
name: cbmc-randomizefreqs-depth-vacuity
description: RandomizeFreqs verifies soundly but kill 0/8; symbolic-size is_fresh + 2 Ran contracts exhaust depth 200 before loop body
metadata: 
  node_type: memory
  type: project
  originSessionId: 05169caf-9855-4c89-85cb-add33dcf6611
---

`RandomizeFreqs(RanState *state, size_t *freqs, int n)` in /app/Syzygy_Zopfli/c_code/zopfli.c (~line 5275) verifies SOUNDLY but kill 0/8 with: requires(n>0), is_fresh(state, sizeof(RanState)), is_fresh(freqs, (size_t)n*sizeof(size_t)), assigns(state->m_z, state->m_w), assigns(__CPROVER_object_whole(freqs)). Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.

**Why kill 0:** body is `for(i<n){ if((Ran(state)>>4)%3==0) freqs[i]=freqs[Ran(state)%n]; }`.
- Survivors 3,4,5,6 (i<n → i>n/>=/==/!=): all make loop not execute (n>0), freqs unchanged, allowed by assigns. The write is conditional+randomized so no postcondition can require a change.
- Survivors 1,8 (condition operator mutants): value/randomization-dependent, inexpressible.
- Survivors 2,7 (i<=n extra-iter OOB at freqs[n]; %n→*n OOB index in iter 0): genuine memory-safety bugs but unreached — symbolic-size is_fresh(freqs, n*8) + two replaced [[cbmc-is-fresh-gotchas|Ran]] contracts exhaust depth 200 before iteration-0 writes.

Only way to reach mutants 2/7 would be pinning n to a tiny concrete value, but real callers (RandomizeStatFreqs) pass ZOPFLI_NUM_LL=288 / ZOPFLI_NUM_D=32, so that's unsound. Kept the sound general contract. Same depth wall as [[cbmc-depth-200-object-limit]].
