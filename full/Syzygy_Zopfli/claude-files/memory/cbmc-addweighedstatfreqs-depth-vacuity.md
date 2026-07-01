---
name: cbmc-addweighedstatfreqs-depth-vacuity
description: AddWeighedStatFreqs verifies soundly but kill 0/12; 288-iter litlens loop exhausts depth 200 before exit
metadata: 
  node_type: memory
  type: project
  originSessionId: 133dba6d-affa-44af-80f2-b4cbe03fa57e
---

`AddWeighedStatFreqs` (Syzygy_Zopfli/c_code/zopfli.c ~5205) verifies with a full
functional contract: is_fresh on the 3 disjoint `SymbolStats` operands,
`assigns(result->litlens, result->dists)`, `ensures result->litlens[256]==1`,
and two `__CPROVER_forall` postconditions giving each litlen/dist as the exact
weighted sum `(size_t)(stats1[i]*w1 + stats2[i]*w2)`. Verifies but **kill 0/12**.

**Why:** same depth-200 wall as [[cbmc-encodetree-loop-depth-vacuity]] and
[[cbmc-zopfliresethash-loop-depth-vacuity]]. The first loop runs ZOPFLI_NUM_LL=288
iterations, exhausting `--depth 200` before the function exit where ensures is
evaluated → post-state unreachable → all postconditions vacuous, all 12 mutants
(loop-bound RELATIONAL + arithmetic `+ -> -`) survive.

**How to apply:** unavoidable. Pointwise/low-index ensures don't help (still only
checked at unreachable exit); OOB loop-bound mutants diverge only at i=288, past
depth 200. Build needs `-I /app/Syzygy_Zopfli/stubs -I /app/Syzygy_Zopfli/c_code`
for FILE.h. Note: CPROVER contract clauses go AFTER the param list / before `{`,
not before the function; forall vars must not shadow the body's loop var `i`.
