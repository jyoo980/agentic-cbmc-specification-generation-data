---
name: getdynamiclengths-no-mutants
description: GetDynamicLengths verifies but avocado generates 0 mutants (no mutable operators); kill score inherently 0
metadata: 
  node_type: memory
  type: project
  originSessionId: ee7059a1-2988-455f-9b57-c851c4519397
---

`GetDynamicLengths` in /app/Syzygy_Zopfli/c_code/zopfli.c verifies with a full
memory-safety + forwarded-precondition contract (union of ZopfliLZ77GetHistogram,
ZopfliCalculateBitLengths, PatchDistanceCodesForBuggyDecoders, and
TryOptimizeHuffmanForRle requires; ensures return_value >= 26). All 4 callees are
replaced with their contracts.

`get-mutants --function GetDynamicLengths` returns "No mutant(s) generated
(no mutable operators)" — the body is just a sequence of callee calls plus one
constant assignment (`ll_counts[256] = 1`), so there is nothing to mutate. Kill
score is inherently 0; do not chase it. Same situation as [[zgdseb-no-mutants]].

**Why:** strength is normally proxied by kill score, but a function with no
mutable operators has no mutants by construction.
**How to apply:** consider it done once it verifies; don't try to fabricate
killable structure.
