---
name: cbmc-quantifier-isfresh-constant-bound
description: "CBMC quirk — quantifiers reading an is_fresh array in ensures must use constant bounds, not symbolic ones"
metadata: 
  node_type: memory
  type: project
  originSessionId: 331ee152-8c48-4dd9-b2e0-5ce2c9c1835c
---

When a `__CPROVER_ensures` clause uses `__CPROVER_forall`/`__CPROVER_exists` to read
the contents of an `__CPROVER_is_fresh` pointer/array parameter, the quantifier bound
MUST be a compile-time constant range (e.g. `0 <= _i && _i < SIZE`). A SYMBOLIC bound
(e.g. `_i < x` where `x` is a parameter) makes CBMC read a HAVOCED copy of the array in
the postcondition instead of the body's actual object, so logically-valid ensures clauses
spuriously FAIL.

**Why:** confirmed empirically (verifying `findTarget` in 2048-clone/2048.c). Scalar reads
like `a[0]` in ensures work fine with symbolic context; only quantifiers with symbolic
bounds break. Not 1D-vs-2D and not forall-vs-exists — purely the symbolic vs constant bound.

**How to apply:** range the quantifier over the constant array size and move the real
(symbolic `stop`/`x`) bounds inside the body as a guard:
`__CPROVER_forall { int _z; (0 <= _z && _z < SIZE) ==> ((stop <= _z && _z < x) ==> array[_z] == 0) }`.
This is also why the verified board functions (findPairDown, countEmpty) cast
`((const uint8_t *)board)[i]` and quantify over the constant `0..SIZE*SIZE`.
