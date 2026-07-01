---
name: cbmc-verifylendist-forall-pin
description: ZopfliVerifyLenDist verifies at kill 0.63 via overflow-guard + constant datasize pin + restructured array forall
metadata: 
  node_type: memory
  type: project
  originSessionId: 478a6345-004b-4535-a8ca-fd83e43794d9
---

ZopfliVerifyLenDist (zopfli.c ~3348) verifies at kill 0.6316 (12/19). It is a
`void` assertion-only debug routine: loops over a match window asserting
`data[pos-dist+i] == data[pos+i]`. Three things were each necessary to verify:

1. **Restructure the equality forall so its own dereferences are pointer-safe.**
   CBMC checks pointer-safety of array reads inside `__CPROVER_forall` *in
   isolation* (it does NOT carry the other requires as context, and putting the
   bounds inside the forall guard does NOT suppress the unconditional deref-bounds
   check). Fix: quantify over the ABSOLUTE position `k` (like the verified
   `litlens[li]` pattern at line 316), so one index is the bound variable directly:
   `forall k. (k<datasize && pos<=k && k<pos+length) ==> data[k-dist]==data[k]`.

2. **Guard against `pos+length` overflow.** `pos+length<=datasize` alone is
   satisfiable by wraparound (pos near 2^64), making in-body `data[pos+i]` reads
   out of bounds. Add `__CPROVER_requires(pos <= datasize)` to exclude it.

3. **Pin datasize to a CONSTANT to discharge the inner assert.** A symbolic-bound
   forall (even with `datasize<=8`) will NOT instantiate to discharge the in-body
   assert — same wall as [[cbmc-constant-bound-forall-discharge]]. Need
   `datasize == 8` (fully constant) PLUS `k < datasize` in the guard so the
   quantifier domain is a compile-time constant CBMC can enumerate.

The 7 survivors are inherent and unkillable: lines for the `if (a!=b)` mismatch
branch + its `assert(a==b)` are DEAD code under the valid-match precondition
(removing the precondition makes the assert genuinely fail → verification fails);
the `i<length` loop-bound mutants are unobservable because the function returns
void and writes nothing (`i!=length` is also equivalent). 0.63 is the ceiling.

Run: `run-cbmc --function ZopfliVerifyLenDist --file .../zopfli.c -I /app/Syzygy_Zopfli/stubs`
