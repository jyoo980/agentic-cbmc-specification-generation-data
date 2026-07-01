---
name: cbmc-zopfliwarmuphash-isfresh-vacuity
description: ZopfliWarmupHash verifies soundly but kill 0/8; replaced UpdateHashValue is_fresh(h) re-assertion makes post-call path infeasible
metadata: 
  node_type: memory
  type: project
  originSessionId: 260a9218-0f89-4860-8c9d-919e85ad20a5
---

ZopfliWarmupHash (zopfli.c:3426) verifies but kill 0/8. Tiny body: two
UpdateHashValue calls, second guarded by `pos + 1 < end`. Wrote the full
functional postcondition (branch-aware rolling-hash over array[pos] / array[pos+1],
both masked to HASH_MASK) plus is_fresh(h), is_fresh(array, end), pos<end, val in
[0,HASH_MASK].

Confirmed VACUOUS: `__CPROVER_ensures(1==0)` still "verifies successfully", so the
body is unreachable. Even mutant 7 (OOB `array[pos-1<end]` read at pos=end-1)
survives — definitive sign the body isn't explored. Mutant 8 (`pos - 0`) is
semantically equivalent to `pos + 0`, unkillable anyway.

Root cause is NOT the symbolic array size: pinning `end == 4` (concrete-extent
is_fresh) left ensures(1==0) still passing. The wall is the REPLACED UpdateHashValue
contract whose requires re-asserts is_fresh(h, sizeof(ZopfliHash)) on an already-fresh
h — in replacement/assertion mode that path goes infeasible (assume-false-like), so
everything after the first call (incl. function exit / the ensures) is vacuous.

Same family as [[cbmc-zopfliupdatehash-depth-vacuity]] and [[cbmc-is-fresh-gotchas]].
Kill cannot be increased; left the strongest functional spec. Needs `-I
/app/Syzygy_Zopfli/stubs` for FILE.h.
