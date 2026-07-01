---
name: cbmc-getcostfixed-lsym-contract-havoc
description: GetCostFixed in zopfli.c verifies non-vacuously 8/13; 5 survivors are lsym<=279 relational mutants unobservable because callee contract havocs lsym across the 279 boundary
metadata: 
  node_type: memory
  type: project
  originSessionId: 804c32d5-76c5-485d-baf6-0c6b6ad72eda
---

`GetCostFixed` (zopfli.c, cost model for fixed Huffman) verifies NON-vacuously with kill score 0.6154 (8/13). Sound spec: `requires litlen<=258 && dist<=32768`, `assigns()`, literal branch `dist==0` returns exactly 8 (litlen<=143) or 9 (else), match branch `dist!=0` returns `[12,31]`.

The 5 survivors are all RELATIONAL mutants of `if (lsym <= 279)` (line ~1860), which selects 7- vs 8-bit length-symbol cost. **Unkillable under contract replacement**: CBMC replaces callee `ZopfliGetLengthSymbol` with its contract, which only pins `lsym ∈ [257,285]` for a match length and does NOT tie a specific lsym value to litlen. So lsym is free across the 279 boundary regardless of input → both 7-bit and 8-bit paths reachable in original AND mutant → the ±1 cost difference is unobservable.

A tighter `(dist!=0 && litlen<=114) ==> return<=29` clause was tried and FAILED to verify (postcondition.4) for exactly this reason — the contract lets `lbits` reach 5 and `lsym` exceed 279 even for litlen<=114, so the body can return 30. Same family as the min-select/branch-equiv unobservable-mutant notes. Strong sound spec left in place.
