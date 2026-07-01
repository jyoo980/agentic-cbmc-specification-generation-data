---
name: ec-no-mutants
description: "EstimateCost is a pure pass-through wrapper; avocado generates 0 mutants, kill score inherently 0"
metadata: 
  node_type: memory
  type: project
  originSessionId: daed00b0-6495-4d26-ad38-a5974a1b2498
---

`EstimateCost` (zopfli.c ~line 2095) forwards verbatim to `ZopfliCalculateBlockSizeAutoType` — no mutable operators, so `get-mutants` prints "No mutant(s) generated"; kill score is inherently 0, don't chase.

Verifies @depth 200 with the callee's full contract mirrored verbatim (union-of-btypes is_fresh preconditions + forall symbol bounds + `lstart==lend ==> ret==0` / `lstart!=lend ==> ret>=3` ensures), enforce-contract EstimateCost + replace-call-with-contract ZopfliCalculateBlockSizeAutoType. Script: /app/_verify_ec.sh.

Same pattern as [[getdynamiclengths-no-mutants]], [[zcbsat-depth200-vacuity]] (the callee).
