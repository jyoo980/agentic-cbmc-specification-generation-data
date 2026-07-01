---
name: cbmc-getcostfixed-distbranch-vacuity
description: "GetCostFixed verifies kill 0.46; dist==0 branch fully killed, dist!=0 branch depth-vacuous"
metadata: 
  node_type: memory
  type: project
  originSessionId: 7768c38c-c85e-41c5-9fd7-ef9e7bf47a80
---

`GetCostFixed` (zopfli.c ~2286) verifies SOUNDLY at kill 0.4615 (6/13) with
`requires(litlen<=258)` + `assigns()` + branch-split exact postcondition.

The `dist==0` branch (literal cost 8 if litlen<=143 else 9) is reachable and
fully checked — all 6 of its mutants are killed by exact pins
`(dist==0 && litlen<=143)==>ret==8`, `(dist==0 && litlen>=144)==>ret==9`.

The `dist!=0` branch is DEPTH-VACUOUS: it inlines two 259-entry static tables
(ZopfliGetLengthSymbol + ZopfliGetLengthExtraBits) plus ZopfliGetDistExtraBits,
exhausting depth 200 before the return/postcondition check. **Confirmed by
falsification**: adding `+100000` to the dist!=0 ensures RHS still "Verified".
So its 7 mutants (5 on `lsym<=279`, 2 on `cost+dbits+lbits`) are unkillable.

Tried and did NOT help: (a) helper-call-in-ensures
`ret == (ZopfliGetLengthSymbol(litlen)<=279?7:8)+5+...` and (b) self-contained
litlen-ladder pinning base(litlen<=114?7:8) and lbits via litlen regions — both
give identical 0.4615 because the branch is simply unreached, not a pinning
problem. Kept the self-contained ladder version (faithful + correct if depth
were deeper). Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.
Same depth-200 wall family as [[cbmc-depth-200-object-limit]].
