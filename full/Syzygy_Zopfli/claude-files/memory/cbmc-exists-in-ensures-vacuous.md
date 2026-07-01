---
name: cbmc-exists-in-ensures-vacuous
description: __CPROVER_exists in an ensures clause passes vacuously and kills no mutants
metadata: 
  node_type: memory
  type: reference
  originSessionId: ddf9c9ba-b719-4be9-aef5-ef2ad3302b53
---

CBMC does not soundly *check* a `__CPROVER_exists{...}` postcondition: it passes
the function trivially and catches no mutant that would violate the existential.
(Bounded `__CPROVER_forall` in requires/assumptions is fine — this is specifically
exists used as an assertion/ensures.)

Seen in PatchDistanceCodesForBuggyDecoders (Syzygy_Zopfli/c_code/zopfli.c): the
true invariant "≥2 distinct non-zero codes in d_lengths[0..29]" was written as a
nested exists ensures. It verified but failed to kill the `i<30 -> i<=30` mutant
(which counts the unused index 30 and can leave only 1 non-zero in 0..29).

**How to apply:** Don't rely on exists-in-ensures for kill score. Express the
real strength with quantifier-free clauses. For this function, the working
killers were `__CPROVER_assigns(d_lengths[0], d_lengths[1])` plus preservation
postconditions `__CPROVER_old(d_lengths[k]) != 0 ==> d_lengths[k] == __CPROVER_old(d_lengths[k])`
for k=0,1 (an already-non-zero code is never overwritten). Reached kill 0.667;
remaining survivors were equivalent mutants (`>=2`→`>2`/`==2`, `<`→`!=`).
Build note: needs `-I /app/Syzygy_Zopfli/stubs` for the FILE.h stub include.
See [[cbmc-is-fresh-gotchas]].
