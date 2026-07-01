---
name: cbmc-calculateblocksymbolsizesmall-depth-vacuity
description: CalculateBlockSymbolSizeSmall verifies soundly but kill 0; is_fresh on 8-pointer ZopfliLZ77Store + 4 arrays exhausts depth 200 before the loop
metadata: 
  node_type: memory
  type: project
  originSessionId: 0f8befcb-2f56-411c-8a6e-1f7dec8bf328
---

`CalculateBlockSymbolSizeSmall` in `/app/Syzygy_Zopfli/c_code/zopfli.c` (single loop over [lstart,lend) summing ll/d code lengths) **verifies SUCCESSFUL but mutation kill = 0/16** — vacuous, same depth-200 wall as [[cbmc-depth-200-object-limit]], [[cbmc-addlz77data-depth-vacuity]], [[cbmc-adddynamictree-depth-vacuity]].

**Cause:** the contract needs `is_fresh(lz77, sizeof(*lz77))` where `ZopfliLZ77Store` has **8 pointer fields** (~400 depth steps alone), plus `is_fresh` on `lz77->litlens`, `lz77->dists`, `ll_lengths`, `d_lengths`, plus two `__CPROVER_forall` preconditions, plus the loop unwound 5×. Confirmed vacuous: adding `__CPROVER_ensures(0 == 1)` still passes (postcondition never reached). All 16 surviving mutants are on the loop guard and the two asserts (lines 312/314/315/316) — never reached.

**Build note:** needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs` (the `<x86_64-linux-gnu/bits/types/FILE.h>` include resolves from the stubs dir). Distinct forall var names required (`li`, `di`) — reusing `i` gives "redeclaration ... with no linkage / CONVERSION ERROR".

**Spec kept (strongest correct):** is_fresh on lz77 + litlens/dists (sized by lz77->size) + ll_lengths(288)/d_lengths(32); `lstart<=lend<=size`; forall litlens<259; forall dists!=0 ==> litlens in[3,258] && dists<=32768 (needed so ZopfliGetLengthSymbol returns >=257 for ExtraBits' domain and ZopfliGetDistSymbol's dist<=32768); `assigns()`; ensures return>=ll_lengths[256] and lstart==lend ==> return==ll_lengths[256]. Documents intent, exits 0; no spec rewrite raises kill given the depth wall. The sibling `CalculateBlockSymbolSizeGivenCounts` (calls this + has its own loops over lz77) will hit the same wall.
