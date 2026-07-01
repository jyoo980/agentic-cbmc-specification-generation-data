---
name: cbmc-updatehashvalue-no-mutable-operators
description: UpdateHashValue verifies with exact rolling-hash postcondition; mutation reports no mutable operators
metadata: 
  node_type: memory
  type: project
  originSessionId: 4cb7a8c4-c2ff-48e2-8af8-35adb8818f8d
---

`UpdateHashValue(ZopfliHash *h, unsigned char c)` in `/app/Syzygy_Zopfli/c_code/zopfli.c` (~line 3375) verifies successfully (needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h).

Contract: `is_fresh(h)`, `requires h->val >= 0 && h->val <= HASH_MASK` (HASH_MASK=32767) to make the signed `h->val << HASH_SHIFT` (shift 5) well-defined and non-overflowing, `assigns(h->val)`, exact `ensures h->val == ((__CPROVER_old(h->val) << HASH_SHIFT) ^ c) & HASH_MASK`, plus range preservation.

Despite the body containing `<<`, `^`, `&`, mutation testing reports **"no mutable operators"** — so kill score is N/A; the exact postcondition is already the strongest possible spec. Same outcome class as [[cbmc-getdynamiclengths-no-mutable-operators]] and the various initializer "no mutable operators" notes.
